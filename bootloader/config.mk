#
# Configuration file for building a nrf bootloader
#
# The following configuration variables can be defined
#
# Name                Description           Default
# ----------------------------------------------------------
# BL_TYPE             Type of bootloader    secure
# BL_COM              Communication mode    ble
# BL_TEMPLATE_MAKE    Template make file    Nordic example
# BL_SDK_CONFIG       Sdk config file       Nordic example
# BL_PROJ_MAIN        Main source file      Nordic example
# BL_MEM_CONF         Linker mem conf file  Nordic example

# Set defaults if not started from application make
BL_TYPE ?= secure
BL_COM ?= ble
TEMPLATE_BOARD ?= pca10056
PUB_KEY_FILE ?= $(BUILD_ROOT)/pub_key.c
# Find template for the bootloader
PROJ_MAIN = $(or $(BL_PROJ_MAIN),$(SDK_ROOT)/examples/dfu/$(BL_TYPE)_bootloader/main.c)
TEMPLATE_ROOT = $(firstword $(wildcard $(dir $(PROJ_MAIN))$(TEMPLATE_BOARD)_*$(BL_COM)))
ifeq ($(TEMPLATE_ROOT),)
  $(error No bootloader template found for $(BL_TYPE), $(BL_COM))
endif
TEMPLATE_MAKE_LIST = $(or $(BL_TEMPLATE_MAKE),$(TEMPLATE_ROOT)/armgcc/Makefile)
SDK_CONFIG = $(or $(BL_SDK_CONFIG),$(TEMPLATE_ROOT)/config/sdk_config.h)
LINKER_SCRIPT = $(or $(BL_MEM_CONF),$(DEF_BL_MEM_CONF))
DEF_BL_MEM_CONF = $(BUILD_ROOT)/mem_conf.ld
SRC_FILES += $(PUB_KEY_FILE)
NO_ENRF = 1
