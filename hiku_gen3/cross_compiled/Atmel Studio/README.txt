Notes for running C++ projects:

- To expose the Atmel C++ toolchain, ensure the following XML parameters are set
  and reload the project (after first unloading the project and editing the project's .cproj):

<ToolchainName>com.Atmel.ARMGCC.CPP</ToolchainName>
<Language>CPP</Language>

- Rename the file implementing main() to .cpp (so that Atmel Studio invokes g++ instead of gcc when necessary)
- Define the following C and C++ symbols/flags (-D):

DEBUG
__<atmel_device_model>__ (e.g. __SAM4S16C__)
<arm_processor_model>=true (e.g. ARM_MATH_CM4=true for an ARM Cortex-M4 processor)
BOARD=<atmel_board_model> (e.g. BOARD=SAM4S_WPIR_RD)

- Define the following C and C++ debugging optimizations:

Optimization Level: -O0 (disables optimization while debugging)
Debugging level: -g3 (maximizes debugging capabilities)

- Define the following C-only symbols/flags (-D):

scanf=iscanf (redefines scanf to use non-floating-point format specifiers and might be optional)
printf=iprintf (redefines printf to use non-floating-point format specifiers and might be optional)

- Copy all include paths from the C compiler configuration to the C++ compiler configuration (so that g++ can find C files)
- Add the following C++ include paths to find OpenCV- and OpenQR-related headers (-I):

C:\OpenCV\ARM EABI\install\include
C:\OpenQR\ARM EABI\install\include

- Add the following C++ library paths to locate linkable libraries (-L):

C:\OpenCV\ARM EABI\install\lib
C:\OpenQR\ARM EABI\install\lib
<path_to_linker_scripts> (e.g. ../src/ASF/sam/utils/linker_scripts/sam4s/sam4s16/gcc for the SAM4S_WPIR_RD demo project since it contains the flash.ld linker script)

- Add the following C++ libraries to link OpenCV- and OpenQR-related static libraries (-l):

libopenqr
libopencv_core

- Check the following linker settings:

General: Generate MAP file ("-Map" flag)
Optimization: Garbage collect unused sections ("-Wl,--gc-sections" flag)

- Add the following custom linker flags:

Miscellaneous: -T<linker_script_name> (e.g. -Tflash.ld for the SAM4S_WPIR_RD demo project)

- (Optional) Add additional linker-related specs if required (depending on the I/O capabilities of the host):

--specs=nosys.specs (disables semihosting, thereby stubbing out system calls)
--specs=rdimon.specs (enables semihosting via the rdimon library)


Notes for refactoring C files to work with C++:

- Designated initializers are not allowed by the ARM EABI C++ toolchain. If they're implemented,
  ensure variables names that designate values are stripped (see rtc_example.cpp, lines 242-251
  that initializes a struct of type "usart_serial_options_t" without variables)
