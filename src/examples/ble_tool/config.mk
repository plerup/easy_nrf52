
# Make room for more UUIDs
UUID_CNT ?= 3

# Choose board specific defaults
ifeq ($(BOARD),pca10059)
  # Dongle normally has only usb uart
  ENRF_SERIAL ?= usb
	UART_LOG ?= 0
else
  ENRF_SERIAL ?= uart
endif

# Enable buttonless dfu by default
BUTTONLESS_DFU ?= 1