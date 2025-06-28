#====================================================================================
# easy_nrf52.mk
#
# A makefile for simplified build and other operations
# when using the Nordic NRF SDK
#
# This file is part of easy_nrf52
# License: LGPL 2.1
# General and full license information is available at:
#    https://github.com/plerup/easy_nrf52
#
# Copyright (c) 2022-2024 Peter Lerup. All rights reserved.
#
#====================================================================================

START_TIME := $(shell date +%s)
_THIS := $(realpath $(lastword $(MAKEFILE_LIST)))
ENV_ROOT := $(dir $(_THIS))
TOOLS_DIR := $(ENV_ROOT)tools
ENV_NAME := $(notdir $(basename $(_THIS)))
CONFIG_NAME = config.mk
CMD_DEFINES := $(strip $(foreach par,$(shell tr "\0" " " </proc/$$PPID/cmdline),$(if $(findstring =,$(par)),$(par),)))
SUB_MAKE = +make -f $(_THIS) $(CMD_DEFINES)
PYTHON ?= python3
PERL ?= perl

# Utility functions
git_description = $(shell git -C $(1) describe --tags --always --dirty 2>/dev/null || echo Unknown)
calc = $(shell printf "0x%0X" $$(($1)))

# Validate installation
INST_FILE = $(HOME)/.config/$(ENV_NAME)/install
ifeq ($(wildcard $(INST_FILE)),)
  $(error Installation has not been made or has changed. Run ./install)
endif
include $(INST_FILE)
SDK_ROOT = $(lastword $(wildcard $(INSTALL_ROOT)/nRF5_SDK_$(SDK_VERSION)*))
ifeq ($(wildcard $(SDK_ROOT)),)
  $(error No SDK for version "$(SDK_VERSION) installed")
endif
FULL_SDK_VERSION := $(shell $(PERL) -e 'print $$1 if $$ARGV[0] =~ /SDK_([\d\.]+)/' $(SDK_ROOT))
GCC_ROOT ?= $(wildcard $(INSTALL_ROOT)/gcc-arm-none-eabi*)
ifeq ($(wildcard $(GCC_ROOT)),)
  $(error No GCC installation found")
endif

# Include possible local configuration
-include $(CONFIG_NAME)

# Board handling
BOARD ?= pca10056
SDK_BOARD_DIR := $(SDK_ROOT)/components/boards
BOARDS_PATH += $(PWD) $(BOARDS_DIR) $(ENV_ROOT)src/boards $(SDK_BOARD_DIR)
BOARD_H := $(firstword $(foreach path,$(BOARDS_PATH),$(wildcard $(path)/$(BOARD).h)))
ifeq ($(BOARD_H),)
  $(error Invalid board: $(BOARD))
endif
BOARD_DIR := $(dir $(BOARD_H))
# Include possible board specific configuration
-include $(BOARD_DIR)$(BOARD).mk

# Detect chip type unless defined in board config
CHIP ?= $(if $(findstring $(BOARD),pca10040),nrf52832,nrf52840)
CHIP_FAM = $(if $(findstring $(CHIP),nrf52832),NRF52,NRF52840)
# Select softdevice variant based on chip type
SD_NAME ?= $(if $(findstring $(CHIP),nrf52832),s132,s140)
# Softdevice file
SOFTDEVICE := $(firstword $(wildcard $(SDK_ROOT)/components/softdevice/$(SD_NAME)/hex/*.hex))
SD_VERS := $(subst _softdevice.hex,,$(notdir $(SOFTDEVICE)))
# Generic settings taken from board template
TEMPLATE_BOARD ?= $(if $(findstring $(CHIP),nrf52832),pca10040,pca10056)

# Project and environment versions
PROJ_VERSION = $(call git_description, $(dir $(PROJ_MAIN)))
ENV_VERSION =  $(call git_description, $(ENV_ROOT))

# Use demo application unless specified or available in current directory
DEMO_APP = $(ENV_ROOT)src/examples/template/main.c
PROJ_MAIN ?= $(firstword $(realpath $(wildcard main.c*)) $(DEMO_APP))
PROJ_NAME ?= $(notdir $(patsubst %/,%,$(dir $(realpath $(PROJ_MAIN)))))

BUILD_ROOT ?= /tmp/$(ENV_NAME)/$(PROJ_NAME)/$(BOARD)

# Use multiple threads when building executables
ifeq ($(MAKECMDGOALS),)
  BUILD_THREADS ?= $(shell nproc)
  MAKEFLAGS += -j $(BUILD_THREADS)
else
  IGNORE_STATE = 1
endif

# Check if command line or git settings have changed since last build
ifneq ($(IGNORE_STATE),1)
  STATE_LOG := $(BUILD_ROOT)/state.txt
  STATE_INF := $(CMD_DEFINES) $(PROJ_VERSION) $(ENV_VERSION)
  # Ignore some specific variables
  STATE_INF := $(patsubst MONITOR_PORT%,,$(STATE_INF))
  STATE_INF := $(patsubst DFU_ADDR%,,$(STATE_INF))
  STATE_INF := $(patsubst VERBOSE%,,$(STATE_INF))
  PREV_STATE_INF := $(if $(wildcard $(STATE_LOG)),$(shell cat $(STATE_LOG)),$(STATE_INF))
  ifneq ($(PREV_STATE_INF),$(STATE_INF))
    $(info * Build state has changed, doing a full rebuild *)
    DEFAULT_GOAL = rebuild
    BUILD_THREADS = 1
    MAKEFLAGS += -B
  endif
  STATE_SAVE := $(shell mkdir -p $(BUILD_ROOT) ; echo -n '$(STATE_INF)' >$(STATE_LOG))
endif

# Templates for makefile (files and compile switches) and sdk_config. Defaults are the ble uart examples
# Include for both peripheral and central role
TEMPLATE_DIR_PERI ?= $(SDK_ROOT)/examples/ble_peripheral/ble_app_uart/$(TEMPLATE_BOARD)/$(SD_NAME)
TEMPLATE_DIR_CENT ?= $(SDK_ROOT)/examples/ble_central/ble_app_uart_c/$(TEMPLATE_BOARD)/$(SD_NAME)
TEMPLATE_MAKE_PERI ?= $(TEMPLATE_DIR_PERI)/armgcc/Makefile
TEMPLATE_MAKE_CENT ?= $(TEMPLATE_DIR_CENT)/armgcc/Makefile
TEMPLATE_MAKE_LIST ?= $(TEMPLATE_MAKE_PERI) $(TEMPLATE_MAKE_CENT)
DEF_MAKE = $(BUILD_ROOT)/template.mk
DEF_MAKE_CMD = $(TOOLS_DIR)/parse_make.pl
$(DEF_MAKE): $(TEMPLATE_MAKE_LIST) $(MAKEFILE_LIST) $(DEF_MAKE_CMD) | $(BUILD_ROOT)
	$(PERL) $(DEF_MAKE_CMD) $(TEMPLATE_MAKE_LIST) >$(DEF_MAKE)

-include $(DEF_MAKE)

ifndef SDK_CONFIG
  # Generate a merged sdk config file from templates
  SDK_CONFIG = $(BUILD_ROOT)/sdk_config.h
  SDK_TEMPLATES ?= $(TEMPLATE_DIR_PERI)/config/sdk_config.h $(TEMPLATE_DIR_CENT)/config/sdk_config.h
  MERGE_CONF_CMD = $(TOOLS_DIR)/merge_config.pl
  $(SDK_CONFIG): $(MERGE_CONF_CMD) $(SDK_TEMPLATES) $(MAKEFILE_LIST)
		$(PERL) $(MERGE_CONF_CMD) $(SDK_TEMPLATES) >$(SDK_CONFIG)
endif

# Possible serial usb or uart
UART_BAUDRATE ?= 115200
ifeq ($(ENRF_SERIAL),usb)
  CFLAGS += -DAPP_USBD_CDC_ACM_ENABLED=1 -DAPP_USBD_ENABLED=1 -DNRFX_USBD_ENABLED=1 \
	          -DPOWER_ENABLED=1 -DNRFX_SYSTICK_ENABLED=1 -DUSBD_POWER_DETECTION=1 \
            -DNRFX_USBD_CONFIG_IRQ_PRIORITY=3 \
	          -DENRF_SERIAL_USB
  SRC_FILES += \
    $(SDK_ROOT)/components/libraries/usbd/app_usbd.c \
    $(SDK_ROOT)/components/libraries/usbd/class/cdc/acm/app_usbd_cdc_acm.c \
    $(SDK_ROOT)/components/libraries/usbd/app_usbd_core.c \
    $(SDK_ROOT)/components/libraries/usbd/app_usbd_serial_num.c \
    $(SDK_ROOT)/components/libraries/usbd/app_usbd_string_desc.c \
    $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_usbd.c \
    $(SDK_ROOT)/integration/nrfx/legacy/nrf_drv_power.c \
    $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_power.c \
    $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_systick.c
else ifeq ($(ENRF_SERIAL),uart)
  UART_LOG = 0
  CFLAGS += -DENRF_SERIAL_UART -DUART_BAUD_RATE=UART_BAUDRATE_BAUDRATE_Baud$(UART_BAUDRATE)
endif
ifneq ($(UART_LOG),0)
  # Add the uart backend and set used tx pin
  SRC_FILES += $(SDK_ROOT)/components/libraries/log/src/nrf_log_backend_uart.c
  UART_LOG_PIN ?= TX_PIN_NUMBER
  nrf_log_backend_uart.c_CFLAGS = \
    -include $(BOARD_H) \
    -DNRF_LOG_BACKEND_UART_TEMP_BUFFER_SIZE=64 -DNRF_LOG_BACKEND_UART_BAUDRATE=UART_BAUDRATE_BAUDRATE_Baud$(UART_BAUDRATE) \
    -DNRF_LOG_BACKEND_UART_TX_PIN=$(UART_LOG_PIN)
  CFLAGS += -DNRF_LOG_BACKEND_UART_ENABLED=1 -DNRF_LOG_BACKEND_RTT_ENABLED=0
endif

# Buttonless DFU
ifeq ($(BUTTONLESS_DFU),1)
  INC_FOLDERS += \
    $(SDK_ROOT)/components/libraries/bootloader \
  	$(SDK_ROOT)/components/libraries/bootloader/dfu $(SDK_ROOT)/components/libraries/bootloader/ble_dfu

  SRC_FILES += $(SDK_ROOT)/components/libraries/bootloader/dfu/nrf_dfu_svci.c \
    $(SDK_ROOT)/components/ble/ble_services/ble_dfu/ble_dfu.c \
    $(SDK_ROOT)/components/ble/ble_services/ble_dfu/ble_dfu_bonded.c \
    $(SDK_ROOT)/components/ble/ble_services/ble_dfu/ble_dfu_unbonded.c

  CFLAGS += \
    -DBLE_DFU_ENABLED=1 -DNRF_PWR_MGMT_CONFIG_AUTO_SHUTDOWN_RETRY=1 \
    -DNRF_SDH_BLE_SERVICE_CHANGED=1 -DBL_SETTINGS_ACCESS_ONLY -DNRF_DFU_TRANSPORT_BLE=1
endif

# Memory definitions and linker configuration file
FLASH_SIZE ?= $(if $(findstring $(CHIP),nrf52840),0x00100000,0x00080000)
RAM_SIZE ?= $(if $(findstring $(CHIP),nrf52840),0x00040000,0x00010000)
RAM_REDUC ?= 0x4000
ifndef LINKER_SCRIPT
  # Generate a linker script with appropriate ram definitions
  LINKER_SCRIPT = $(BUILD_ROOT)/mem_conf.ld
  MEM_CONF_CMD = $(TOOLS_DIR)/mem_conf.pl
  TEMPLATE_MEM_CONF ?= $(SDK_ROOT)/examples/ble_central/ble_app_uart_c/$(TEMPLATE_BOARD)/$(SD_NAME)/armgcc/ble_app_uart_c_gcc_nrf52.ld
  $(LINKER_SCRIPT): $(MEM_CONF_CMD) $(TEMPLATE_MEM_CONF) $(_THIS)
		perl $(MEM_CONF_CMD) $(RAM_REDUC) $(TEMPLATE_MEM_CONF) >$(LINKER_SCRIPT)
endif

# Adjust source file list and include directories
ifndef NO_ENRF
  SRC_FILES += $(ENV_ROOT)/src/lib/enrf.c
  INC_FOLDERS += $(ENV_ROOT)src/lib
endif
SRC_FILES += $(PROJ_MAIN)
SRC_FILES := $(sort $(filter-out $(EXCLUDE_FILES),$(SRC_FILES)))
INC_FOLDERS += $(dir $(SDK_CONFIG)) $(dir $(PROJ_MAIN))
CFLAGS += -DCUSTOM_BOARD_INC=$(basename $(BOARD_H))
CFLAGS += -DSDK_VERSION=$(SDK_VERSION)
ifdef DEBUG
  CFLAGS += -DDEBUG=1 -DNRF_LOG_DEFAULT_LEVEL=4
  OPT = -O0 -g3
endif

# UUID count
UUID_CNT ?= 2
CFLAGS += -DNRF_SDH_BLE_VS_UUID_COUNT=$(UUID_CNT)

# GCC toolchain commands
GCC_ARM_PREFIX := $(GCC_ROOT)/bin/arm-none-eabi
CC = '$(GCC_ARM_PREFIX)-gcc'
CXX = '$(GCC_ARM_PREFIX)-c++'
LINK ?= $(CC)
OBJCOPY = '$(GCC_ARM_PREFIX)-objcopy'
SIZE = '$(GCC_ARM_PREFIX)-size'
GDB = '$(GCC_ARM_PREFIX)-gdb'

# Use ccache if it is available and not explicitly disabled (USE_CCACHE=0)
USE_CCACHE ?= $(if $(shell which ccache 2>/dev/null),1,0)
ifeq ($(USE_CCACHE),1)
	GCC_PREFIX = ccache
endif

# Build rules
OBJ_DIR = $(BUILD_ROOT)/obj
OBJ_EXT = .o
DEP_EXT = .d
C_INCLUDES := $(foreach dir, $(INC_FOLDERS),-I$(dir))
OBJ_FILES := $(patsubst %,$(OBJ_DIR)/%$(OBJ_EXT),$(notdir $(SRC_FILES)))
VPATH += $(sort $(dir $(SRC_FILES)))
COMP_DEP = $(MAKEFILE_LIST) $(SDK_CONFIG) $(COMP_DEP_EXTRA) | $(OBJ_DIR)

# Compile
C_COM = $(GCC_PREFIX) $(CC) -c $(CFLAGS) $(C_INCLUDES) --std=gnu99 $(C_UNDEF)
CPP_COM = $(GCC_PREFIX) $(CXX) -c $(CFLAGS) $(C_INCLUDES) --std=gnu++11 -fno-rtti
$(OBJ_DIR)/%.c$(OBJ_EXT): %.c $(COMP_DEP)
	echo CC $(<F)
	$(C_COM) -MMD $($(<F)_CFLAGS) $(realpath $<) -o $@

$(OBJ_DIR)/%.cpp$(OBJ_EXT): %.cpp $(COMP_DEP)
	echo CCX $(<F)
	$(CPP_COM) -MMD $($(<F)_CFLAGS) $(realpath $<) -o $@

$(OBJ_DIR)/%.S$(OBJ_EXT): %.S $(COMP_DEP)
	echo ASM $(<F)
	$(CC) -c -MMD -x assembler-with-cpp $(ASMFLAGS) $(C_INCLUDES) $(realpath $<) -o $@

# Link the main executable
OUT_PATH ?= $(BUILD_ROOT)/$(PROJ_NAME)
OUT_ELF = $(OUT_PATH).elf
OUT_HEX = $(OUT_PATH).hex
OUT_BIN = $(OUT_PATH).bin
# Automatically generate build information data for the application
BUILD_INFO = $(OBJ_DIR)/_build_info.c.o
DEB_INFO = $(if $(DEBUG),-D,)
BUILD_TIME = $(shell date +"%F %T")
BUILD_VERSION = "$(BUILD_VERSION_HEADER)$(PROJ_VERSION)$(BUILD_VERSION_TAIL) \($(ENV_VERSION)-$(FULL_SDK_VERSION)$(DEB_INFO)\)"

$(OUT_HEX): $(OBJ_FILES) $(LINKER_SCRIPT)
	echo "Linking: $@ ($(CHIP))"
	echo "  Version: $(BUILD_TIME) $(BUILD_VERSION)"
	echo "char *_build_time=\"$(BUILD_TIME)\", *_build_version=\"$(BUILD_VERSION)\";" \
	  | $(CC) -c $(CFLAGS) $(C_INCLUDES) -xc -o $(BUILD_INFO) -
	$(LINK) $(LDFLAGS) $(OBJ_FILES) $(BUILD_INFO) $(LIB_FILES) -Wl,-Map "-Wl,$(OUT_PATH).map" -o $(OUT_ELF)
	$(OBJCOPY) -O ihex $(OUT_ELF) $(OUT_HEX)
	$(SIZE) -A $(OUT_ELF) | $(PERL) -e 'while (<>) {$$r += $$1 if /^\.(?:data|bss)\s+(\d+)/;$$f += $$1 if /^\.(?:text|data)\s+(\d+)/;}print "\nMemory usage\n";print sprintf("  %-6s %6d bytes (%.0f KB)\n" x 2 ."\n", "Ram:", $$r, $$r/1024, "Flash:", $$f, $$f/1024);'
	@$(PERL) -e 'print "Build complete. Elapsed time: ", time()-$(START_TIME),  " seconds\n\n"'
ifeq ($(DEMO_APP),$(PROJ_MAIN))
	echo "======================================================"
	echo "=== Please note that this is the example template! ==="
	echo "======================================================\n"
endif

# Run compiler preprocessor to get full expanded source for a file
preproc:
ifeq ($(SRC_FILE),)
	$(error SRC_FILE must be defined)
endif
ifneq ($(findstring .cpp,$(notdir $(SRC_FILE))),)
	$(CPP_COM) -E $(SRC_FILE)
else
	$(C_COM) -E $(SRC_FILE)
endif

clean:
	@echo Removing all build files
	rm -rf $(BUILD_ROOT)

# Bootloader management
BL_TYPE ?= secure
BL_COM ?= ble
# Bootloader settings are required when flashing a new application
BOOTLOADER_SETTINGS = $(BUILD_ROOT)/bl_settings.hex
$(BOOTLOADER_SETTINGS) bootloader_settings: $(OUT_HEX)
	echo Generating bootloader settings
	$(NRFUTIL) settings generate $(BOOTLOADER_SETTINGS) \
          --family $(CHIP_FAM) \
          --bootloader-version 1 --bl-settings-version 1 \
          --application=$(OUT_HEX) --application-version 1 >/dev/null

# Build a bootloader
BOOTLOADER_FILE ?= $(BUILD_ROOT)/bootloader_$(BL_TYPE)_$(BL_COM).hex
BOOTLOADER_DIR ?= $(ENV_ROOT)bootloader
$(BOOTLOADER_FILE) bootloader: $(COMP_DEP)
	$(eval export BOARD TEMPLATE_BOARD BL_TYPE BL_COM BL_TEMPLATE_MAKE BL_SDK_CONFIG \
	              BOARDS_DIR BL_PROJ_MAIN BL_MEM_CONF PRIV_KEY_FILE PUB_KEY_FILE)
	echo "Building bootloader ($(BL_TYPE), $(BL_COM))"
	$(SUB_MAKE) -C $(BOOTLOADER_DIR) OUT_HEX=$(BOOTLOADER_FILE) BUTTONLESS_DFU=0 BUILD_ROOT=$(BUILD_ROOT)/bootloader ENRF_SERIAL=""

# Complete (production) hex file
ALL_HEX_FILE ?= $(PROJ_NAME)_$(BOARD)_all_$(PROJ_VERSION).hex
hex_all:
	$(SUB_MAKE)
	$(SUB_MAKE) bootloader
	$(SUB_MAKE) bootloader_settings
	echo "Creating combined flash file: \"$(ALL_HEX_FILE)\"..."
	srec_cat $(SOFTDEVICE) -Intel $(BOOTLOADER_FILE) -Intel $(OUT_HEX) -Intel $(BOOTLOADER_SETTINGS) -Intel -o $(ALL_HEX_FILE) -Intel

# Show all involved include directories, source files and compilation defines
list_files:
	$(PERL) -e 'foreach (@ARGV) {print "$$_\n"}' "===== Include directories =====" $(INC_FOLDERS)  "===== Source files =====" $(SRC_FILES) \
	    "===== Compilation defines =====" $(CFLAGS)

# == Flashing operations ==

UICR_ADDR = 0x10001000
MAC_ADDR  = 0x100000A4
UICR_SIZE = 0x400
ifeq ($(BOARD),pca10059)
  # Make sure that REGOUT0 is always kept at 3.3V (5),for nrf52840 dongle
  REGOUT0_ADDR = 0x10001304
  REGOUT0_VAL = 0xFFFFFFFD
endif

FLASH_FILE ?= nrf_flash.hex
FLASH_FILE_IS_HEX := $(if $(findstring .hex,$(suffix $(FLASH_FILE))),1,)
FLASH_DUMP_FILE = $(BUILD_ROOT)/flash.bin

# Openocd is used for stlink programmer and gdb
OOCD_COM = openocd -f interface/$(OOCD_CFG).cfg $(OOD_PARAMS) -f target/nrf52.cfg
OOCD_LOG = /tmp/$(ENV_NAME)_openocd.log
OOCD_TAIL = >$(OOCD_LOG) 2>&1 || (cat $(OOCD_LOG) && exit 1)
# Support Segger (nrfjprog) and STLink (openocd) programmers
PROG_HW ?= segger
ifeq ($(PROG_HW),segger)
  FLASH_COM ?= nrfjprog -f nrf52 -q
  ifdef SEGGER_SNR
    # Use specific Segger unit
    FLASH_COM += --snr $(SEGGER_SNR)
    RTT_PAR = -select USB=$(SEGGER_SNR)
  endif
  define RTT_COM
  	JLinkGDBServer -singlerun -nogui -if swd -port 50000 -swoport 50001 -telnetport 50002 -device nrf52 $(RTT_PAR) >/dev/null &
  	-JLinkRTTClient </dev/null 2>/dev/null
  	-pkill JLinkGDBServer
  endef
  FLASH_PROG = $(FLASH_COM) --sectorerase --program $1 --reset
  define FLASH_ERASE
  	$(FLASH_COM) --eraseall
		$(if $(REGOUT0_ADDR),$(FLASH_COM) --memwr $(REGOUT0_ADDR) --val $(REGOUT0_VAL),)
  endef
  READ_UICR = $(FLASH_COM) --memrd $(UICR_ADDR) --n $(UICR_SIZE)
  READ_MAC = $(FLASH_COM) --memrd $(MAC_ADDR) --n 8
  RESET_COM = $(FLASH_COM) --reset
  OOCD_CFG = jlink
  OOD_PARAMS = -c "transport select swd"
else ifeq ($(PROG_HW),stlink)
  POST_FLASH = $(if $(REGOUT0_ADDR),mww 0x4001E504 1; mww $(REGOUT0_ADDR) $(REGOUT0_VAL); mww 0x4001E504 0;,)
  OOCD_CFG = stlink
  ifdef STLINK_SNR
    OOCD_COM += -c "hla_serial \"$(STLINK_SNR)\""
  endif

  FLASH_COM ?= $(OOCD_COM)
  FLASH_PROG ?= $(FLASH_COM) -c "program $1 verify; $(POST_FLASH) reset; exit" $(OOCD_TAIL)
  FLASH_ERASE ?= $(FLASH_COM) -c "init; reset halt; targets; nrf5 mass_erase; $(POST_FLASH) exit" $(OOCD_TAIL)
  READ_UICR = $(FLASH_COM) -c "init; mdw $(UICR_ADDR) $(UICR_SIZE)" -c " exit" 2>&1
  READ_MAC = $(FLASH_COM) -c "init; mdw $(MAC_ADDR) 2" -c " exit" 2>&1
  RESET_COM = $(FLASH_COM) -c "init; reset; exit" $(OOCD_TAIL)
else
  $(error Unknown programmer)
endif

define WRITE_FILE_TO_FLASH
	@echo Flashing: $(notdir $1) ...
	$(call FLASH_PROG,$1)
endef

LAST_FLASH = $(BUILD_ROOT)/last_flash

flash:
	$(SUB_MAKE)
ifndef NO_BOOTLOADER
	$(SUB_MAKE) bootloader_settings
	$(call WRITE_FILE_TO_FLASH,$(BOOTLOADER_SETTINGS))
endif
	$(call WRITE_FILE_TO_FLASH,$(OUT_HEX))
	touch $(LAST_FLASH)

$(LAST_FLASH): $(OUT_HEX)
	$(SUB_MAKE) flash

build_flash:
	$(SUB_MAKE)
	$(SUB_MAKE) $(LAST_FLASH)

flash_softdevice: $(SOFTDEVICE)
ifndef NO_SOFTDEVICE
	$(call WRITE_FILE_TO_FLASH,$<)
endif

flash_bootloader:
ifndef NO_BOOTLOADER
	$(SUB_MAKE) bootloader
	$(call WRITE_FILE_TO_FLASH,$(BOOTLOADER_FILE))
endif

flash_all:
	$(SUB_MAKE) erase_flash
	$(SUB_MAKE) flash_softdevice
	$(SUB_MAKE) flash_bootloader
	$(SUB_MAKE) flash

flash_file:
	$(call WRITE_FILE_TO_FLASH,$(FLASH_FILE))

bin_to_hex:
	srec_cat $(FLASH_FILE) -binary -o $(basename $(FLASH_FILE)).hex -Intel

DUMP_ADDR ?= 0
DUMP_SIZE ?= $(FLASH_SIZE)
DUMP_COM = $(OOCD_COM) -c "init; dump_image $(FLASH_DUMP_FILE)
dump_flash: $(BUILD_ROOT)
	echo Saving flash to: \"$(FLASH_FILE)\" \($(DUMP_ADDR) - $(DUMP_SIZE)\)...
	$(DUMP_COM) $(DUMP_ADDR) $(DUMP_SIZE); exit" $(OOCD_TAIL)
ifeq ($(FLASH_FILE_IS_HEX),1)
	srec_cat $(FLASH_DUMP_FILE) -binary -offset $(DUMP_ADDR) -o $(FLASH_FILE) -Intel
else
	mv $(FLASH_DUMP_FILE) $(FLASH_FILE)
endif
ifeq ($(DUMP_ADDR),0)
  ifeq ($(FLASH_FILE_IS_HEX),1)
		echo Adding UICR \($(UICR_ADDR) - $(UICR_SIZE)\)...
		$(DUMP_COM) $(UICR_ADDR) $(UICR_SIZE); exit" $(OOCD_TAIL)
		srec_cat $(FLASH_FILE) -Intel $(FLASH_DUMP_FILE) -binary -offset $(UICR_ADDR) -o $(FLASH_FILE) -Intel
  else
		echo "* UICR not added to binary dump file"
  endif
	-rm $(FLASH_DUMP_FILE)
endif

erase_flash:
	echo Erasing flash and UICR...
	$(FLASH_ERASE)

show_uicr:
	$(READ_UICR) | $(PERL) $(TOOLS_DIR)/show_uicr.pl

show_mac:
	$(READ_MAC) | $(PYTHON) $(TOOLS_DIR)/show_mac.py

reset:
	echo Reset
	$(RESET_COM)

# Build output root directory
$(BUILD_ROOT):
	mkdir -p $(BUILD_ROOT)

$(OBJ_DIR): | $(BUILD_ROOT)
	mkdir -p $@

# Patching possible faulty UICR definitions in bootloader memory config file
$(DEF_BL_MEM_CONF):
	$(PERL) $(TOOLS_DIR)/patch_uicr.pl $(wildcard $(dir $(TEMPLATE_MAKE_LIST))*.ld) >$@

# Visual Studio Code
vscode: $(DEF_MAKE)
	$(eval CMD_DEFINES := $(strip $(foreach par,$(shell tr "\0" " " </proc/$$PPID/cmdline),$(if $(findstring =,$(par)),$(par),))))
	$(PERL) $(TOOLS_DIR)/vscode.pl -n "$(PROJ_NAME)" -m "make -f $(_THIS) $(CMD_DEFINES)" -w "$(VS_CODE_DIR)" -b "$(OOCD_CFG)" -o "$(OUT_ELF)" $(CC) $(CFLAGS) $(C_INCLUDES)

# GDB debugging
start_gdb_server:
	$(OOCD_COM) >$(OOCD_LOG) 2>&1 &
	sleep 1
	cat $(OOCD_LOG)
	echo

stop_gdb_server:
	-pkill openocd

gdb:
	$(SUB_MAKE) start_gdb_server
	-$(GDB) $(OUT_ELF) -ex "target remote localhost:3333" -ex "monitor reset halt" -ex "b main"
	$(SUB_MAKE) stop_gdb_server

# DFU handling, bootloader needs public key source file
PROJ_PRIV_KEY_FILE ?= $(dir $(PROJ_MAIN))privkey.pem
DEV_PRIV_KEY_FILE = $(ENV_ROOT)bootloader/dev_priv_key.pem
PRIV_KEY_FILE ?= $(wildcard $(PROJ_PRIV_KEY_FILE))
ifeq ($(PRIV_KEY_FILE),)
  PRIV_KEY_FILE = $(DEV_PRIV_KEY_FILE)
endif
PUB_KEY_FILE ?= $(BUILD_ROOT)/pub_key.c
$(PUB_KEY_FILE): $(PRIV_KEY_FILE)
ifeq ($(PRIV_KEY_FILE), $(DEV_PRIV_KEY_FILE))
	echo "*** Warning, using development key for secure bootloader! ***"
else
	echo "- Generating public key from: $(PRIV_KEY_FILE)"
endif
	$(NRFUTIL) keys display --key pk --format code $(PRIV_KEY_FILE) --out_file $@

gen_priv_key:
	$(NRFUTIL) keys generate $(PRIV_KEY_FILE)

# Nordic dfu zip
DFU_ZIP ?= $(PROJ_NAME)_$(BOARD)_dfu.zip
dfu_zip $(DFU_ZIP):
	$(SUB_MAKE)
	echo Signing package with key file: $(PRIV_KEY_FILE)
	$(eval SD_ID := $(shell $(NRFUTIL) pkg generate --help | grep $(SD_VERS) | $(PERL) -e "print +(split(/\|/, <>))[2];"))
	$(eval SD_ID := $(if $(SD_ID), $(SD_ID), 0x0100))
	$(NRFUTIL) pkg generate --hw-version 52 --application-version 0xff --sd-req $(SD_ID) \
                       --key-file $(PRIV_KEY_FILE) \
                       --application $(OUT_HEX) $(DFU_ZIP)

# Run dfu via nrfdfu program
DFU_TEMP_DIR = $(BUILD_ROOT)/_dfu
DFU_BLE = $(if $(findstring ble,$(BL_COM)),1,)
DFU_DEST = $(if $(DFU_BLE),$(DFU_ADDR),$(DFU_PORT))
DFU_PAR = $(if $(findstring ble,$(BL_COM)),-b $(DFU_DEST),-t serial -p $(DFU_DEST))
PORT_DEF ?= ACM
DFU_PORT ?= $(shell ls -1tr /dev/tty$(PORT_DEF)* 2>/dev/null | tail -1)
DFU_COM ?= $(TOOLS_DIR)/nrfdfu $(DFU_PAR) -f $(DFU_ZIP)
.PHONY: dfu
dfu:
ifeq ($(DFU_DEST),)
	@echo == Error: DFU destination not specified
	exit 1
endif
	$(SUB_MAKE) dfu_zip
	$(DFU_COM)

# Serial monitor
MONITOR_PORT ?= $(DFU_PORT)
MONITOR_SPEED ?= 115200
MONITOR_COM ?= $(PYTHON) -m serial.tools.miniterm -e $(MONITOR_PORT) $(MONITOR_SPEED)
monitor:
ifneq ($(ENRF_SERIAL),)
	$(MONITOR_COM)
else ifneq ($(UART_LOG),0)
	$(MONITOR_COM)
else
	$(RTT_COM)
endif

run:
	$(SUB_MAKE) build_flash
	sleep 2
	$(SUB_MAKE) monitor

# Other build options
default: $(OUT_HEX)

rebuild:
	$(SUB_MAKE) clean
	$(SUB_MAKE)

DEFAULT_GOAL ?= default
.DEFAULT_GOAL := $(DEFAULT_GOAL)

ifndef VERBOSE
  # Set silent mode as default
  MAKEFLAGS += --silent
endif

# Include all available generated dependencies
-include $(wildcard $(OBJ_DIR)/*$(DEP_EXT))

# Show connected Segger units
list_segger:
	$(FLASH_COM) --com

# Show connected stlink units
list_stlink:
	st-info --probe | $(PERL) -e 'while (<>) { next unless s/\s+serial:\s+//; s/(\w\w)/\\x$$1/g; print; }'

help:
help:
	@echo
	@echo "Makefile for building software for the nrf52 chipset using the Nordic SDK"
	@echo "This file can either be used directly or included from another makefile"
	@echo ""
	@echo "The following targets are available:"
	@echo "  default (or empty)   Build the project application executable"
	@echo "  clean                Remove all intermediate build files"
	@echo "  flash                Build and and flash the project application"
	@echo "  flash_all            Complete reflash of the unit"
	@echo "                         including softdevice, bootloader and application"
	@echo "  dfu                  Build and and flash via dfu"
	@echo "                         Params: DFU_ADDR or DFU_PORT"
	@echo "  dfu_zip              Create standard Nordic dfu zip file"
	@echo "                         File name controlled by DFU_ZIP"
	@echo "                         Default: '$(DFU_ZIP)'"
	@echo "  hex_all              Create combined flash file with all"
	@echo "                         required components for later flashing"
	@echo "  dump_flash           Dump the whole board flash memory to a file"
	@echo "  flash_file           Restore flash memory from a previously dumped file"
	@echo "  erase_flash          Erase the whole flash (use with care!)"
	@echo "  list_files           Show a list of used solurce files and include directories"
	@echo "  vscode               Create config file for Visual Studio Code and launch"
	@echo "  monitor              Start serial monitor on the upload port"
	@echo "                         or if UART_LOG is set to 0, the Segger RTT monitor"
	@echo "  run                  Build flash and start serial monitor"
	@echo "  gdb                  Start gdb command line debugger"
	@echo "  gen_priv_key         Generate a private key file for secure bootloader"
	@echo "  preproc              Run compiler preprocessor on source file"
	@echo "                         specified via SRC_FILE"
	@echo "  list_segger          List serial number of all connected Segger interfaces"
	@echo "  list_stlink          List serial number of all connected stlink interfaces"
	@echo "  show_uicr            Show changed UICR registers"
	@echo "  show_mac             Show device mac address"
	@echo "Some configurable parameters"
	@echo "For more detailed information check the makefile"
	@echo "  PROJ_NAME            Main source file"
	@echo "                         If not specified a search for main.c will be made"
	@echo "                         and if not found the template example will be used"
	@echo "  SRC_FILES            Extend this variable to declare additional source files"
	@echo "  INC_FOLDERS          Extend this to add additional include directories"
	@echo "  BOARD                Name of the target board. Default: '$(BOARD)'"
	@echo "  BUILD_ROOT           Directory for intermediate build files."
	@echo "                         Default '$(BUILD_ROOT)'"
	@echo "  CFLAGS               Extend with possible extra compilation options"
	@echo "  PRIV_KEY_FILE        Private key file for secure bootloader"
	@echo "                         If not specified a deleopment key will be used"
	@echo "                         Dont use this for product applications!"
	@echo "  SDK_VERSION          Used SDK version. Default: '$(SDK_VERSION)'"
	@echo "  MONITOR_PORT         Serial monitor port. Default: '$(MONITOR_PORT)'"
	@echo "  FLASH_FILE           File name for dump and restore flash operations"
	@echo "                         Default: '$(FLASH_FILE)'"
	@echo "  VERBOSE              Set to 1 to get full printout of the build"
	@echo "  BUILD_THREADS        Number of parallel build threads"
	@echo "                         Default: Maximum possible, based on number of CPUs"
	@echo "  USE_CCACHE           Set to 0 to disable ccache when it is available"
	@echo "  SEGGER_SNR           Required when several Segger units are present"
	@echo "  STLINK_SNR           Required when several stlink units are present"
	@echo "  PROG_HW              Flashing and debug hardware interface"
	@echo "                         segger or stlink"
	@echo "  NO_BOOTLOADER        When defined no bootloader will be built or flashed"
	@echo "  NO_SOFTDEVICE        When defined the softdevice will not be flashed"
	@echo
