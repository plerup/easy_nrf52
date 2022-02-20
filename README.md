# easy NRF52
Tools for simplified handling of the Nordic SDK for nrf52 chipset

THIS INFORMATION IS INCOMPLETE YET, WORK IN PROGRESS

#### Installing


#### Getting help


#### Building projects

##### Some examples

#### Advanced usage


##### Build time and version information


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

A record of the command line parameters and git versions used in the last build is stored in the build directory. If any of these are changed during the next build, e.g. changing a variable definition, a complete rebuild is made in order to ensure that all possible changes are applied. If you don't want this function just define the variable **IGNORE_STATE**.


#### Using Visual Studio Code

Visual Studio Code is a great editor which can be used together with easy_nrf52. The makefile contains a rule named "vscode". When invoked it will generate a config file for the C/C++ addin. This will contain all the required definitions for the IntelliSense function. The information is based on the parameters of the c/c++ compilation command.

It will also generate contents in the "tasks" configuration file which enables building with easy_nrf52 from within the editor. This is convenient for stepping through compilation errors for instance.

The configuration files will have settings with the name of the main sketch.

The workspace directory for the settings files will be ".vscode" and this can either be automatically detected by easy_nrf52 or be specified via the variable **VS_CODE_DIR**. Automatic here means checking the parent directories of the sketch for a config directory and if doesn't exist then the sketch directory itself will be used and created if not found. If an existing project file (*.code-workspace) is found in that directory it will be used as input for the launch of VS Code.

After generating the configuration files easy_nrf52 will launch Visual Studio (if available in the path)


#### Compiler preprocessor

Sometimes it can be useful to see the actual full source file content once all include files and macros have been expanded. The rule **preproc** is available for this purpose. The path of the source file to be analyzed is specified via the variable **SRC_FILE**. Example:

    espmake preproc SRC_FILE=my_file.ino


