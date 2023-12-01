# alert2040
This is a sample project based on the RP2040 microcontroller. It is meant to interact with an
[alertapp](https://github.com/crazybolillo/alertapp) server, said server can run locally on your computer.

It shows the basics of using [Raspberry's Pico SDK](https://github.com/raspberrypi/pico-sdk) and
[FreeRTOS](https://www.freertos.org/index.html) to run a multicore application that performs arbitrary processing and
communicates over HTTP with remote servers.

# Building and Flashing
This is a CMake project, so the build process is the same as for any other CMake project. 
In order to flash it you can use [picoprobe](https://github.com/raspberrypi/picoprobe) together with 
[OpenOCD](https://openocd.org/) to flash the generated `.elf` file.

To run OpenOCD use:

```shell
openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg
```

You can then connect with `telnet` and beside other things, flash the program, verify and reset the target (to run
the newly flashed program):

```shell
telnet localhost 4444
program <path/to/elf> verify reset
```
