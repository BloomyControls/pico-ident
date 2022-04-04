# Raspberry Pi Pico-Based System Identification

This project uses a Raspberry Pi Pico to identify and store various information
about a system/fixture, settable and retrievable over UART or USB. Additionally,
writing can be disabled by jumping GPIO pins 14 and 15 together (pins 19 and 20
on the board).

## Information Fields

There are 10 R/W information fields stored on the device. It's also possible to
read the Pico's unique 64-bit identifier as a hex string, but of course this is
read-only. The idea is that the Pico's serial number can always be used to
uniquely identify any device, as no two Picos have the same serial.

It's worth noting that each sector (4096 bytes) of flash has a guaranteed
minimum of 100,000 program-erase cycles according to the manufacturer. Each
write to one of these fields will incur one erase and one program. This
effectively limits us to 100,000 writes to any of these fields. Of course, the
intended use of this device is to be set once and then write-locked for the rest
of its lifespan, so that's likely not a huge concern here.

| Field | Access | Description |
|---|---|---|
| `MFG` | Read-write | Manufacturer |
| `NAME` | Read-write | Name |
| `VER` | Read-write | Version |
| `DATE` | Read-write | Date |
| `PART` | Read-write | Part number |
| `MFGSERIAL` | Read-write | Manufacturer's custom serial number |
| `USER1` | Read-write | General-purpose field 1 |
| `USER2` | Read-write | General-purpose field 2 |
| `USER3` | Read-write | General-purpose field 3 |
| `USER4` | Read-write | General-purpose field 4 |
| `SERIAL` | Read-only | Pico's unique 64-bit serial number |

Note that each of the above fields has a maximum length of 63. Each field is 64
bytes, but is null-terminated. Values longer than 63 bytes will be truncated.

## Commands

Communicating with the Pico over serial is very simple. Each command must be
terminated with a carriage return (`\r`). If a line feed (`\n`) is sent
afterwards, it will be ignored. Use 115200 baud, no flow control, 8 bits per
byte, 1 stop bit.

To set a field, send the field name, an equals sign, and the field's value. For
example, to set the device's manufacturer field:

```
MFG=Bloomy Controls\r
```

To query a value, send the name of the field followed by a question mark. The
Pico will respond with the value delimited by a CRLF (`\r\n`). For example,
to query the manufacturer:

```
MFG?\r
```

There are two additional commands:

| Command | Description |
|---|---|
| `CLEAR` | Clear all writable fields |
| `CHECK?` | Check that the data stored in flash matches the stored checksum, then return either `OK` or `ERR` |

## Build Requirements

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
flash the Pico) will be named `pico-ident.uf2`.

## Installing

To install the firmware onto the pico, hold down the BOOTSEL button on the Pico
while you are plugging in its USB cable. It should show up as a storage device
in your file explorer. Simply drag and drop the UF2 file to it and it should
unmount automatically. It's now flashed and ready to use.
