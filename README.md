# easy NRF52
Tools for simplified handling of the Nordic Bluetooth Low Energy SDK for nrf52 chipset (prior to NRFConnect) on Debian based Linux PC.

This is a tool set where I have put into my many years of experience struggling with the Nordic SDK and thereby hoping it will enable shortening of the learning curve for others. Then again the SDK has been replaced by NRFConnect but easy_nrf52 provides a more down to earth straight on approach as long as you are using the nrf52. It also facilitates building of controlled release versions of nrf52 product firmware.

The basic idea is to provide easy methods for handling all the aspects of making a product based on nrf52. A makefile takes care of the building and flashing process. A source code library is provided to remove the need to duplicate the massive amount code as done in the Nordic examples. This takes some shortcuts but should be sufficient in most cases.

The build process of easy_nrf52 will automatically extract the SDK config file and source files list from the Nordic examples and use these as the template for the configuration. When needed this can be overridden in part or completely by the project configuration.

A main idea is to use the Nordic UART service as the channel for data exchange between units and also to a PC.

A number of examples of typical applications are provided.

Automatic handling of secure bootloader with correct signing is available and the dfu can be performed directly from a PC with Bluetooth support.

Debugging can either be done via Visual Studio Code or from the command line via gdb.

Flashing methods include Segger and STLink.

The standard Nordic dev kit boards are supported as well as some others. It is also very easy to add other boards.

**Please note** The documentation here is somewhat meager for now. For better understanding check out the examples and the makefile source.

The whole concept of easy_nrf52 is very equal to that of makeEspArduino (https://github.com/plerup/makeEspArduino) so if you have been using that you would feel somewhat at home.


#### Installing

Perform the installation of easy_nrf52 by starting the *install* script. This will download and install the Nordic SDKs, GCC compiler and some other tools.

The script will prompt for which SDK(s) to use. Only version 15 up to 17 are supported.

It will also prompt for a location where to put all the expanded downloads.

As some possible additional system installations will be required (via apt) sudo prompt will be shown.

Once the installation has completed it can be tested by issuing the command:

    enrfmake

This will build the example template included in easy_nrf52.

If you have a Nordic nrf52840 DK kit (pca10056) you can then issue the following command to get the softdevice, bootloader and template application into this by entering:

    enrfmake flash_all

If you don't know the mac address of your board you can now find it via this command:

    enrfscan "enrf template"

After that you can use this command to communicate with the application:

    enrfuart -b mac_addr

where mac_addr should be replaced with the address that was shown in the previous command. You can then enter the commands supported by the template application, e.g. *led* which will toggle the onboard led.

By entering:

    enrfmake help

a list of common build commands and their possible configuration parameters will be shown.

If you have another board this must be specified via the *BOARD* variable, e.g.:

    enrfmake BOARD=pca10040


#### Building projects

Arrange a project by adding your source files in a dedicated directory. You can configure what files to use either in a top make file (named Makefile as usual) or in specific configuration file named *config.mk*. Check for instance the *ble_tool* example. The default main source file that will be search for is *main.c* but this can be changed via the variable *PROJ_MAIN*

The build can then be performed by issuing *enrfmake* in the project directory or if you have a top make file by standard *make* command. In the latter case you can add the following line to this file for automatic include of the easy_nrf52 makefile.

    include $(shell enrfpath)

A typical *config.mk* may look something like this:

    MY_SRC = $(HOME)/dev/my_proj
    PROJ_MAIN = $(MY_SRC)/main.c
    SRC_FILES += \
       $(MY_SRC)/file1.c \
       $(MY_SRC)/file2.c
    INC_FOLDERS += $(MY_SRC)

If you need additional source files from the Nordic SDK just add them here. Possible extra or replaced SDK configuration variables can be added to a file named *app_sdk_config.h*

It is possible to use C++ files in the build but this requires that the main source files is also in C++


##### Some examples

The *src/examples* directory contains some typical examples on how to use easy_nrf52. It is recommended to study these for better understanding of the concepts of easy_nrf52.

##### Build time and version information

The easy_nrf52 build process will also automatically produce and compile a source file which contains the actual build date and git versions of the project source files and easy_nrf52, whenever a link operation is performed. This is accessible via the global strings:

    extern char *_build_time, *_build_version;

#### Flash operations

The easy_nrf52 makefile can be used to perform all types flashing operations of a nrf52 device. Direct cable based flashing from a PC can be made via a Segger equipped programmer like the Nordic development boards pca10040 or pca10056. It can also be done via an STLink device connected to the SWIO interface of the nrf52.

The following command can be used to do an initial complete reflash of the nrf52 unit:

    enrfmake flash_all

This will first erase the flash and UICR and then flash softdevice, bootloader and then the actual application.

During subsequent development work the command:

    enrfmake flash

can be used to just reflash the application.

Please note that when erasing the flash of a Nordic nrf52840 dongle (pca10059), the makefile will make sure that the REGOUT0 register is always kept at 3.3V to avoid bricking the device.

It is also possible to do a wireless update of the application via the DFU function on a PC with bluetooth capability, This requires that the unit has the standard Nordic bootloader which has the public key of the private key that is used to sign the application during the build. The device mac address must be specified. Example:

    enrfmake DFU_ADDR=12:34:56:78:9A:BC dfu

To save the complete flash and UICR content to a file us the comand:

    enrfmake dump_flash

The default file name is *nrf_flash.hex* but it can be changed via the variable *FLASH_FILE*

The produced file can later be resored via the command:

    enrfmake flash_file

#### Monitor

Builds made by easy_nrf52 will enable uart logging by default. This can be disabled by defining UART_LOG=0. In this case RTT logging will be enabled instead, same as in the Nordic examples.

To automatically start a terminal for monitoring the uart output use the command:

    enrfmake monitor

or to build (when needed) and then start the monitor:

    enrfmake run

It is also possible to use functionality for serial input and output via either the uart or the usb interface on the nrf52. This is enabled via *ENRF_SERIAL=uart* or *ENRF_SERIAL=usb* . Check the the *ble_tool* example for more info on this.

#### Bootloader

easy_nrf52 will automically build and flash the standard Nordic bootloaders. Which one to use is controlled by the following variables:

**BL_TYPE** Type of bootloader, default is secure

**BL_COM**  Bootloader communication interface, default is ble

By default a development security key is used for the secure bootloader. To generate your own unique one use:

    enrfmake gen_priv_key


#### Misc build features

##### Using ccache

If you want to speed up your builds with easy_nrf52 you can install ccache on your system, https://ccache.dev/

Once installed easy_nrf52 will automatically use it during the build by preceding all C and C++ compilation commands with ccache. In case you have ccache installed but don't want to use it for the build, you can set the variable *USE_CCACHE=0*


##### Parallel builds

The make build operation is performed using parallel build compilation threads. By default all the cores of the machine is used. You can however limit the number of compilation threads started by setting the *BUILD_THREADS* variable to a desired alternate number.

##### Automatic rebuild

A record of the command line parameters and git versions used in the last build is stored in the build directory. If any of these are changed during the next build, e.g. changing a variable definition, a complete rebuild is made in order to ensure that all possible changes are applied. If you don't want this function just define the variable *IGNORE_STATE=1*.


#### Using Visual Studio Code

Visual Studio Code is a great editor which can be used together with easy_nrf52. The makefile contains a rule named "vscode". When invoked it will generate config files for IntellliSense, build and debug functions. The information is based on the parameters of the c/c++ compilation command.

The project will have "tasks" configuration which enables building with easy_nrf52 from within the editor. This is convenient for stepping through compilation errors for instance.

For performing debugging the VS Code Addin Cortex-Debug must be installed, https://marketplace.visualstudio.com/items?itemName=marus25.cortex-debug

The configuration files will have settings with the name of the main sketch.

The workspace directory for the settings files will be ".vscode" and this can either be automatically detected by easy_nrf52 or be specified via the variable *VS_CODE_DIR*. Automatic here means checking the parent directories of the sketch for a config directory and if doesn't exist then the sketch directory itself will be used and created if not found. If an existing project file (*.code-workspace) is found in that directory it will be used as input for the launch of VS Code.

After generating the configuration files easy_nrf52 will launch Visual Studio (if available in the path)


#### Compiler preprocessor

Sometimes it can be useful to see the actual full source file content once all include files and macros have been expanded. The rule *preproc* is available for this purpose. The path of the source file to be analyzed is specified via the variable *SRC_FILE*. Example:

    enrfmake preproc SRC_FILE=my_file.c


#### Other tools

The installation makes some other tools available as well on a PC with Bluetooth capability.

**enrfscan** This command scans and lists all advertising Bluetooth low energy devices. Filtering can be made my supplying a match string. Advertising data will be shown together with address and rssi value.

**enrfuart** This implements a Nordic uart service client and can be used to connect to units with this service enabled e.g. the easy_nrf52 examples