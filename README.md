# pigpio_sled

This repo contains a modified version of the pigpio daemon and utilities for shared access to the gpio ports and functions. It has been modified to include convenient access for ws281x led strips.

The baseline of this library is the classic pigpio library created by joan2937. These strip led extensions come with the same license conditions as the original pigpio software.

pigpio is a C library for the Raspberry which allows control of the General Purpose Input Outputs (GPIO). The original code is contained in the master branch.

The extensions contained in the **strip-led-support** branch of this repo has integrated support for the ws2811/ws2812 strip leds. These have been implemented as natural server extensions and can be accessed as other pigpiod features through the client interfaces such as pigs or the new C extensions in pigpiod_if2.h.

To access the strip led extensions, you **MUST** checkout this branch. See below.

Led strips are a convenient way to add user feedback to Raspberry Pi projects. However, due to the demanding nature of led strips, i.e. < 1us timing requirements, it is difficult to reliably write drivers on top of the exising pigpiod. Libraries provide PWM and DMA drivers, but typically must be used outside of pigpiod, thus with different control capabilities than other GPIO functions. With this library, it is possible to access attached ws281x led strips through the customary pigpiod interfaces and maintain consistency with features that use other GPIO functions.

## Strip LED Features

* Access the strip leds through the commandline pigs interface:

  - pigs sled_begin
    By default uses PWM0 on pin 18, sets up 64 light channel buffer for ws2812 strip type on channel 0.

  - pigs sled_set 0 #FF0000
    Sets the 0 led to red in the channel buffer.

  - pigs sled_set 1 #00FF00
    Sets the 1 led to green in the channel buffer.

  - pigs sled_render
    Sends the signal to the led strip to render the channel buffer.

* Optionally configure the ws281x library channels

  - pigs sled_channel num_leds gpio_pin strip_type channel
    Configures the channel where num_leds is the number of leds on the strip, gpio_pin is the pin to use (limited to hardware), strip_type (see below), and channel (either 0 or 1). This should be called before sled_begin. Otherwise, call sled_end, then sled_channel, then sled_begin again. Due to hardware and ws281x library quirks, when switching pins and setting up alternate channels, this can be limited by hardware or require a daemon restart.

* Standard C Interface through pigpiod_if2.h. Functions include:

    sled_channel();
    sled_begin();
    sled_end();
    sled_set();
    sled_render();

## Use

### Attach led strips to proper ports. The most reliable pin in our testing has been GPIO 18. Pin 21 has varied results with improper colors. Other pins like 12, 13, and 19 are theoretically possible on some devices.

### Build and install the new libraries. These replace the existing pigpiod and libraries.

### Command line: Use the pigs interface to interactively control ws2812 strips.

### Program use: In C/C++, use #include "pigpiod_if2.h" and compile with -lpigpiod_if2 for program access to led strips.

## Clone the repos && build

1) cd <your working directory>

2) git clone https://github.com/tenchirocom/pigpio_sled.git

3) git clone https://github.com/tenchirocom/rpi_ws281x

4) cd pigpio_sled

5) git checkout strip-led-support

6) cd rpi_ws281x && scons
   You may need to install scons, use the following: sudo apt install scons

7) cd .. && make

## Installing the Daemon service

1) Run the following in the pigpio_sled directory
   sudo make install
   
   This copies the built files into /usr/local/bin and /usr/local/lib. Your original files are still located in /usr/bin and /usr/lib. So becareful, we need to ensure search path and library paths do not pick up the old ones or mismatch the binaries and libaries.

   The Makefile has been updated to automatically perform the next few steps and automatically startup the new daemon. However if there are problems, the following steps are included for completeness.

2) Setup the service
   sudo systemctl edit --full pigpiod

   Edit the service file and replace the ExecStart=/usr/bin/pigpiod with:

   Environment=LD_LIBRARY_PATH=/usr/local/lib
   ExecStart=/usr/local/bin/pigpiod -n localhost -p 8888 -s 1

3) Check for dropin service files overriding the default. Look in the directory for the file:

   /etc/systemd/system/pigpiod.service.d/public.conf

   If it has a single ExecStart=/usr/lib/pigpiod line in it, move it to public.conf.old or remove it. If there is more in the file, then you can edit the ExecStart as above.

4) Make sure that your shell path is not picking up the wrong files:

   which pigs => /usr/local/bin/pigs
   which pigpiod => /usr/local/bin/pigpiod (but you don't run this manually, so less important)
   ldd `which pigpiod` => /usr/local/lib/libpigpiod.so (something like that)

5) Restart the service

   sudo systemctl daemon-reexec
   sudo systemctl restart pigpiod
   sudo status pigpiod

   It should be active. Then you can try to enable the default led strip, if connected with:

   pigs sled_begin

   It should exit normally.

   pigs sled_set 0 0x00FF00
   pigs sled_render

   This should turn the first led green!

   Congratulations!

## Strip Types Supported

The following strip format types are available for use when configuring channels. The format number corresponds to the following #define in the rpi_ws281x library header file, ws2811.h.

        0x00: WS2811_STRIP_GRB | WS282_STRIP | SK6812_STRIP
        0x01: WS2811_STRIP_GBR
        0x02: WS2811_STRIP_RGB
        0x03: WS2811_STRIP_RBG
        0x04: WS2811_STRIP_BRG
        0x05: WS2811_STRIP_BGR
        0x06: SK6812_STRIP_GRBW | SK6812W_STRIP
        0x07: SK6812_STRIP_GBRW
        0x08: SK6812_STRIP_RGBW
        0x09: SK6812_STRIP_RBGW
        0x0A: SK6812_STRIP_BRGW
        0x0B: SK6812_STRIP_BGRW
        0x0C: SK6812_SHIFT_WMASK

Use these values in the sled.conf file as well for strip type.

## Autostart Strip LED Configuration

On startup, the pigpiod reads up to two config files and can initialize the sled service for the particular hardware. To use this feature, the following locations can be used:

  /etc/pigpio/sled.conf
  or
  $CONFIG_FILE/sled.conf

In the second option, you can place the file anywhere you choose and set the environment variable CONFIG_FILE to specify the locations.

Config files are lines that either start with # for commments or include name/value pairs. For example:

# Example sled.conf file

# Pigpio Strip LED Config Files

| # Pigpio Strip LED Config Example File
| # 
| # Place this file in the /etc/pigpio/sled.conf file, or run
| # with the $CONFIG_DIR environment variable set to the
| # directory with the sled.conf file in it.
|
| # Basic config parameters
| autobegin=true
| dmanum=default
| freq=default
| render_wait_type=default
| 
| # Channel 0
| ch0:gpionum=18
| ch0:count=64
| ch0:strip_type=0x00
| ch0:invert=false
| ch0:brightness=255
| ch0:wshift=default
| ch0:rshift=default
| ch0:gshift=default
| ch0:bshift=default
| 
| # Channel 1 (off)
| ch1:gpionum=0
| ch1:count=0
| ch1:strip_type=0x00
| ch1:invert=false
| ch1:brightness=255
| ch1:wshift=default
| ch1:rshift=default
| ch1:gshift=default
| ch1:bshift=default

## Original pigpio Features

* Sampling and time-stamping of GPIO 0-31 between 100,000 and 1,000,000 times per second
* Provision of PWM on any number of the user GPIO simultaneously
* Provision of servo pulses on any number of the user GPIO simultaneously
* Callbacks when any of GPIO 0-31 change state (callbacks receive the time of the event
  accurate to a few microseconds)
* Notifications via pipe when any of GPIO 0-31 change state
* Callbacks at timed intervals
* Reading/writing all of the GPIO in a bank (0-31, 32-53) as a single operation
* Individually setting GPIO modes, reading and writing
* Socket and pipe interfaces for the bulk of the functionality in addition to the
  underlying C library calls
* Construction of arbitrary waveforms to give precise timing of output GPIO
  level changes (accurate to a few microseconds)
* Software serial links, I2C, and SPI using any user GPIO
* Rudimentary permission control through the socket and pipe interfaces so users
  can be prevented from "updating" inappropriate GPIO
* Creating and running scripts on the pigpio daemon

## Interfaces

The library provides a number of control interfaces
* the C function interface,
* the /dev/pigpio pipe interface,
* the socket interface (used by the pigs utility and the Python module).

## Utilities

A number of utility programs are provided:
* the pigpiod daemon,
* the Python module,
* the piscope digital waveform viewer,
* the pigs command line utility,
* the pig2vcd utility which converts notifications into the value change dump (VCD)
  format (useful for viewing digital waveforms with GTKWave).

## Documentation

See http://abyz.me.uk/rpi/pigpio/

## Example programs

See http://abyz.me.uk/rpi/pigpio/examples.html

## GPIO

ALL GPIO are identified by their Broadcom number.  See http://elinux.org.

There are 54 GPIO in total, arranged in two banks.

Bank 1 contains GPIO 0-31.  Bank 2 contains GPIO 32-54.

A user should only manipulate GPIO in bank 1.

There are at least three types of board:
* Type 1
    * 26 pin header (P1)
    * Hardware revision numbers of 2 and 3
    * User GPIO 0-1, 4, 7-11, 14-15, 17-18, 21-25
* Type 2
    * 26 pin header (P1) and an additional 8 pin header (P5)
    * Hardware revision numbers of 4, 5, 6, and 15
    * User GPIO 2-4, 7-11, 14-15, 17-18, 22-25, 27-31
* Type 3
    * 40 pin expansion header (J8)
    * Hardware revision numbers of 16 or greater
    * User GPIO 2-27 (0 and 1 are reserved)

It is safe to read all the GPIO. If you try to write a system GPIO or change
its mode you can crash the Pi or corrupt the data on the SD card.
