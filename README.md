# pigpio_sled

This repo contains a modified version of the pigpio daemon and utilities for shared access to the gpio ports and functions. It has been modified to include convenient access for ws281x led strips.

The baseline of this library is the classic pigpio library created by joan2937. These extensions come with the same license conditions as the original pigpio software.

pigpio is a C library for the Raspberry which allows control of the General Purpose Input Outputs (GPIO). The original code is contained in the master branch.

The extensions contained in the **strip-led-support** branch of this repo has integrated support for the ws2811/ws2812 strip leds. These have been implemented as natural server extensions and can be accessed as other pigpiod features through the client interfaces such as pigs or the new C extensions in pigpiod_if2.h.

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

## Clone the repo

1) git clone 
## Installation of rpi_ws281x Library

1) Cd to the directory just above the pigpio repo's home directory
    cd /...your directory.../pigpio/..

2) Clone the rpi_ws281x library into the repo's home directory
    clone https://github.com/tenchirocom/rpi_ws281x
    This was forked from (https://github.com/jgarff/rpi_ws281x.git) if you wish to use the original.

3) Check to make sure the pigpio/rpi_ws281x symbolic link in the pigpio directory points to the ws2812 libraries.

4) cd rpi_ws281x and build the strip led libraries first.

5) cd ../pigpio and make the pigpio libraries and executables.

6) Connect the strip leds to pin 18, and run the x_pigpiod_if2_sled test program. You can also use pigs to dynamically configure and set the led strip.

## Strip Types Supported

The following strip types are available in the rpi_ws281x library header file, ws2811.h

// 4 color R, G, B and W ordering
#define SK6812_STRIP_RGBW                        0x18100800
#define SK6812_STRIP_RBGW                        0x18100008
#define SK6812_STRIP_GRBW                        0x18081000
#define SK6812_STRIP_GBRW                        0x18080010
#define SK6812_STRIP_BRGW                        0x18001008
#define SK6812_STRIP_BGRW                        0x18000810
#define SK6812_SHIFT_WMASK                       0xf0000000

// 3 color R, G and B ordering
#define WS2811_STRIP_RGB                         0x00100800
#define WS2811_STRIP_RBG                         0x00100008
#define WS2811_STRIP_GRB                         0x00081000
#define WS2811_STRIP_GBR                         0x00080010
#define WS2811_STRIP_BRG                         0x00001008
#define WS2811_STRIP_BGR                         0x00000810

// predefined fixed LED types
#define WS2812_STRIP                             WS2811_STRIP_GRB
#define SK6812_STRIP                             WS2811_STRIP_GRB
#define SK6812W_STRIP                            SK6812_STRIP_GRBW

struct ws2811_device;


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
