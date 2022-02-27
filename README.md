# easy NRF52
Tools for simplified handling of the Nordic SDK for nrf52 (prior to NRFConnect) chipset on Linux.

**THIS INFORMATION IS YET INCOMPLETE, WORK IN PROGRESS**

This is a tool set where I have put my many years of experience of struggling with the Nordic SDK and hope it will enable shortening the learning curve for others. Then again the SDk has been replaced by NRFConnect but easy_nrf52 provides a more down to earth straight on approach as long as you are using the nrf52. It also facilitates building of controlled release versions of nrf52 product firmware.

The basic idea is to provide easy methods for handling all the aspects of making a product based on nrf52. A special library is provided to remove the need to duplicate the massive amount code as done in the Nordic examples. This takes some shortcuts but should be sufficient in most cases.

A main idea is to use the Nordic UART service as the channel for data exchange between units and also to a PC.

A number of examples of typical applications are provided.

Automatic handling of secure bootloader with correct signing is available and the dfu can be performed directly from a PC with Bluetooth support

Debugging can either be done via Visual Studio Code or from the command line via gdb

Flashing methods include Segger and STLink

The standard Nordic dev kit boards are supported as well as some others. It is also vey easy to add other boards.


#### Installing

Perform installation of easy_nrf52 by starting the "install" script. This will download and install the Nordic SDKs, GCC compiler and some other tools.

The script will prompt for which SDK(s) to use. Only version 15 up to 17 are supported.

It will also prompt for a location where to put all the expanded downloads.

As some possible additional system installations will be required (via apt) sudo prompt will be shown.

Once the installation has completed it can be tested by issuing the command *enrfmake*. This will build the example template included in easy_nrf52.


#### Getting help


#### Building projects

##### Some examples

#### Advanced usage


##### Build time and version information

The easy_nrf52 build process will also automatically produce and compile a source file which contains the actual build date and git versions of the project source files and easy_nrf52, whenever a link operation is performed. This is accessible via the global strings:
___build_time__ and ___build_version__


##### Including the makefile



#### Flash operations

#### Additional flash I/O operations


#### Monitor


#### Misc build features

##### Using ccache

If you want to speed up your builds with easy_nrf52 you can install ccache on your system, https://ccache.dev/

Once installed easy_nrf52 will automatically use it during the build by preceding all C and C++ compilation commands with ccache.

In case you have ccache installed but don't want to use it for the build, you can set the variable **USE_CCACHE=0**


##### Parallel builds

The actual make operation is performed using parallel build compilation threads. By default all the cores of the machine is used. You can however limit the number of compilation threads started by setting the **BUILD_THREADS** variable to a desired alternate number.

##### Automatic rebuild

A record of the command line parameters and git versions used in the last build is stored in the build directory. If any of these are changed during the next build, e.g. changing a variable definition, a complete rebuild is made in order to ensure that all possible changes are applied. If you don't want this function just define the variable **IGNORE_STATE=1**.


#### Using Visual Studio Code

Visual Studio Code is a great editor which can be used together with easy_nrf52. The makefile contains a rule named "vscode". When invoked it will generate config files for IntellliSense, build and debug functions. The information is based on the parameters of the c/c++ compilation command.

The project will have "tasks" configuration which enables building with easy_nrf52 from within the editor. This is convenient for stepping through compilation errors for instance.

For performing debugging the VS Code Addin Cortex-Debug must be installed https://marketplace.visualstudio.com/items?itemName=marus25.cortex-debug

The configuration files will have settings with the name of the main sketch.

The workspace directory for the settings files will be ".vscode" and this can either be automatically detected by easy_nrf52 or be specified via the variable **VS_CODE_DIR**. Automatic here means checking the parent directories of the sketch for a config directory and if doesn't exist then the sketch directory itself will be used and created if not found. If an existing project file (*.code-workspace) is found in that directory it will be used as input for the launch of VS Code.

After generating the configuration files easy_nrf52 will launch Visual Studio (if available in the path)


#### Compiler preprocessor

Sometimes it can be useful to see the actual full source file content once all include files and macros have been expanded. The rule **preproc** is available for this purpose. The path of the source file to be analyzed is specified via the variable **SRC_FILE**. Example:

    espmake preproc SRC_FILE=my_file.c


