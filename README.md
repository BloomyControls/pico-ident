# Raspberry Pi Pico System Identification

This project uses a Raspberry Pi Pico to identify a system and store various
information about the system, settable and retrievable over UART or USB.

## Requirements

You'll need Ubuntu or Debian to build this (WSL works just fine). Before
attempting to build this project, install required packages using the following
commands:

```
sudo apt update
sudo apt upgrade
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential python3 git
```

## Building

First, you must configure the build system:

```
cmake -B build
```

This step may take quite some time, as it will download the Pi Pico SDK. If you
wish to use USB for serial communications instead of UART, add `-DUSB_SERIAL=1`
to the above command.

After the configuration step is done, you can build the project like so:

```
cmake --build build
```

This will generate all the outputs in the build directory. The UF2 file (used to
flash the pico) will be named `pico-ident.uf2`.

## Installing

To install the firmware onto the pico, hold down the BOOTSEL button on the pico
while you are plugging in its USB cable. It should show up as a storage device
in your file explorer. Simply drag and drop the UF2 file to it and it should
unmount automatically. It's now flashed and ready to use.
