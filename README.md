# easy_nrf52
Tools for simplified handling of the Nordic Bluetooth Low Energy SDK for the nRF52 chipset (prior to nRF Connect) on Debian-based Linux PCs.

easy_nrf52 is a toolset built from many years of experience working with and struggling with the Nordic SDK. The goal is to shorten the learning curve and simplify development workflows for nRF52-based products.

Although the original Nordic SDK has now been superseded by nRF Connect SDK, easy_nrf52 provides a more straightforward and practical approach for projects that still use the classic nRF52 SDK. It also simplifies the creation of controlled firmware release builds for production devices.

The core idea behind easy_nrf52 is to simplify all aspects of nRF52 product development. A Makefile-based build system handles compilation and flashing, while a reusable source library reduces the need to duplicate the large amount of boilerplate code found in the Nordic SDK examples. While this approach takes some shortcuts, it is sufficient for most use cases.

The build process automatically extracts SDK configuration files and source file lists from Nordic SDK examples and uses them as templates for project configuration. These defaults can be partially or completely overridden by project-specific settings.

One of the primary design goals is to use the Nordic UART Service (NUS) as the main communication channel between devices and PCs.

Several example applications are included.

Automatic handling of secure bootloaders with correct signing is supported, and DFU updates can be performed directly from a Bluetooth-enabled PC.

Debugging can be performed either through Visual Studio Code or directly from the command line using GDB.

Supported flashing interfaces include Segger and STLink.

Standard Nordic development boards are supported, along with several additional boards. Adding support for custom boards is also straightforward.

> **Note**
> The documentation is still somewhat limited. For a better understanding of the framework, study the examples and Makefile sources.

The overall concept of easy_nrf52 is similar to that of [makeEspArduino](https://github.com/plerup/makeEspArduino). Users familiar with that project should feel somewhat at home.

---

# Features

- Simplified nRF52 SDK build system
- Automatic SDK configuration extraction
- Secure bootloader and DFU support
- BLE UART tooling
- Segger and STLink flashing support
- Visual Studio Code integration
- Automatic build/version metadata generation
- Example applications included

---

# Quick Start

```bash
wget -qO - https://github.com/plerup/easy_nrf52/raw/master/install | bash

enrfmake
enrfmake flash_all
```

Tested on Debian-based Linux systems.

---

# Installation

There are two ways to install easy_nrf52.

The first method is to clone this repository and run the `install` script. This downloads and installs the Nordic SDKs, GCC compiler, and additional required tools.

The second method is to download and execute the installation script directly. In this case, the repository contents are copied automatically and Git is not required.

Example:

```bash
wget -qO - https://github.com/plerup/easy_nrf52/raw/master/install | bash
```

The installer prompts for:

- Which SDK versions to install (SDK 15 through 17 are supported)
- The directory where downloaded files should be extracted

A sudo password prompt may appear if additional packages need to be installed through `apt`.

The installation script supports two optional parameters:

| Parameter | Description |
|---|---|
| `--use_defaults` | Runs the installer without prompts using default values |
| `--global` | Installs everything under `/opt` for system-wide access |

After installation, verify the setup by running:

```bash
enrfmake
```

This builds the included template example.

If you have an nRF52840 DK (`pca10056`), flash the SoftDevice, bootloader, and example application using:

```bash
enrfmake flash_all
```

To locate the MAC address of the board:

```bash
enrfscan "enrf template"
```

You can then communicate with the device using:

```bash
enrfuart -b mac_addr
```

Replace `mac_addr` with the detected Bluetooth address.

Try entering commands supported by the example application, such as:

```text
led
```

which toggles the onboard LED.

To display available build commands and options:

```bash
enrfmake help
```

To specify another board:

```bash
enrfmake BOARD=pca10040
```

All boards defined in the Nordic SDK are supported, along with additional board definitions located under:

```text
src/boards
```

Additional board definitions can also be added:

- In the project directory
- In a directory specified via `BOARDS_DIR`

Board header files must define LEDs and buttons. Optional board-specific Makefile settings can be placed in a corresponding `.mk` file.

---

# Building Projects

Create a project by placing source files in a dedicated directory.

Build configuration can be specified either:

- In a top-level `Makefile`
- In a project configuration file named `config.mk`

See the `ble_tool` example for reference.

By default, the build system searches for a source file named:

```text
main.c
```

This can be overridden using the following variable:

```makefile
PROJ_MAIN
```

Projects can be built by running:

```bash
enrfmake
```

inside the project directory.

If using a top-level Makefile, standard `make` can also be used. Add the following line for automatic inclusion of the easy_nrf52 build system:

```makefile
include $(shell enrfpath)
```

Example `config.mk`:

```makefile
MY_SRC = $(HOME)/dev/my_proj
PROJ_MAIN = $(MY_SRC)/main.c
SRC_FILES += \
   $(MY_SRC)/file1.c \
   $(MY_SRC)/file2.c

INC_FOLDERS += $(MY_SRC)
```

Additional Nordic SDK source files can be included as needed.

Extra or overridden SDK configuration values can be placed in:

```text
app_sdk_config.h
```

C++ source files are supported, but the main source file must also be written in C++.

---

# Example Applications

The `src/examples` directory contains several example applications demonstrating typical easy_nrf52 usage patterns.

Studying these examples is strongly recommended.

---

# Build Time and Version Information

During linking, easy_nrf52 automatically generates a source file containing:

- Build timestamp
- Git revision information for both:
  - The project
  - easy_nrf52 itself

These values are accessible through the global variables:

```c
extern char *_build_time, *_build_version;
```

---

# Flash Operations

easy_nrf52 supports all common nRF52 flashing operations.

Direct cable-based flashing can be performed using:

- Segger programmers
- Nordic development boards
- STLink devices connected through SWD

To perform a complete device flash:

```bash
enrfmake flash_all
```

This operation:

1. Erases flash and UICR
2. Programs the SoftDevice
3. Programs the bootloader
4. Programs the application

During development, the application alone can be reflashed using:

```bash
enrfmake flash
```

When erasing an nRF52840 dongle (`pca10059`), easy_nrf52 preserves the `REGOUT0` setting at 3.3V to avoid bricking the device.

Wireless DFU updates are also supported on Bluetooth-enabled PCs.

Example:

```bash
enrfmake DFU_ADDR=12:34:56:78:9A:BC dfu
```

To save complete flash and UICR contents:

```bash
enrfmake dump_flash
```

Default output file:

```text
nrf_flash.hex
```

Override using:

```makefile
FLASH_FILE
```

Restore a saved image using:

```bash
enrfmake flash_file
```

---

# Monitor

Builds generated by easy_nrf52 enable UART logging by default.

Disable UART logging with:

```makefile
UART_LOG=0
```

This enables RTT logging instead, matching Nordic SDK example behavior.

To launch a logging monitor:

```bash
enrfmake monitor
```

To build and launch automatically:

```bash
enrfmake run
```

Other application serial communication through either UART or USB can be enabled using:

```makefile
ENRF_SERIAL=uart
```

or

```makefile
ENRF_SERIAL=usb
```

See the `ble_tool` example for more information.

The default terminal is a patched version of Python `miniterm`, located at:

```text
tools/miniterm.py
```

This version:

- Avoids USB UART port lockups
- Allows optional output line modification

Alternative terminal applications can be selected using:

```makefile
MONITOR_COM
```

---

# Bootloader

easy_nrf52 automatically builds and flashes standard Nordic bootloaders.

Bootloader behavior is controlled through:

| Variable | Description |
|---|---|
| `BL_TYPE` | Bootloader type (default: `secure`) |
| `BL_COM` | Bootloader communication interface (default: `ble`) |

By default, a development signing key is used.

Generate a custom private key using:

```bash
enrfmake gen_priv_key
```

---

# Miscellaneous Build Features

## Using ccache

To accelerate builds, install [ccache](https://ccache.dev/).

If detected, easy_nrf52 automatically uses it for all C/C++ compilation steps.

Disable ccache usage with:

```makefile
USE_CCACHE=0
```

---

## Parallel Builds

Compilation is performed using parallel build threads.

By default, all CPU cores are used.

Limit the number of build threads using:

```makefile
BUILD_THREADS
```

---

## Automatic Rebuild Detection

easy_nrf52 stores previous build parameters and Git revisions.

If configuration values or revisions change, a full rebuild is triggered automatically.

Disable this feature with:

```makefile
IGNORE_STATE=1
```

---

# Using Visual Studio Code

Visual Studio Code integrates well with easy_nrf52.

The Makefile includes a `vscode` rule that generates:

- IntelliSense configuration
- Build tasks
- Debug configuration

The generated configuration is based on the actual compiler command line.

This enables convenient in-editor building and debugging.

For debugging support, install the Cortex-Debug extension:

https://marketplace.visualstudio.com/items?itemName=marus25.cortex-debug
This is done automatically if Visual Studio Code was already installed when easy_nrf52 was installed.

Generated configuration names are based on the main project source file.

Workspace settings are stored under:

```text
.vscode
```

easy_nrf52 automatically searches parent directories for an existing VS Code configuration directory.

The location can also be specified explicitly using:

```makefile
VS_CODE_DIR
```

If an existing `.code-workspace` file is found, it is used automatically.

After generating the configuration, easy_nrf52 launches Visual Studio Code if available in the system path.

---

# Compiler Preprocessor

To inspect fully expanded source files after preprocessing:

```bash
enrfmake preproc SRC_FILE=my_file.c
```

This expands:

- Include files
- Macros
- Conditional compilation

---

# Additional Tools

Several additional Bluetooth-related tools are installed and usable on a PC with Bluetooth capability.

## enrfscan

Scans and lists advertising BLE devices.

Optional filtering can be applied using a match string.

Displayed information includes:

- Device address
- RSSI
- Advertising data

---

## enrfuart

Implements a Nordic UART Service client for communicating with compatible devices.

Works with the included example applications.

---

If you flash the example application *ble_tool* onto a nrf52840 module, e.g. the Nordic dongle pca10059, you can use this in the same way as the command above. But in this case extended MTU and long range (PHY_CODED) can be used as well.

## eenrfscan

Extended version of `enrfscan` with long-range support.

---

## eeenrfuart

Extended UART client with:

- 256-byte MTU support
- Long-range PHY support

Use:

```bash
--help
```

for complete command descriptions.
