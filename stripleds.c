#include <stdio.h>
#include <pigpio.h>
#include <string.h>
#include "stripleds.h"

#include "ws2811.h"

#define TARGET_FREQ WS2811_TARGET_FREQ
#define DMA 10
#define STRIP_TYPE WS2812_STRIP
#define DEFAULT_PIN 18
#define DEFAULT_LEDS 16 // Turns off both channels
#define DEFAULT_LEDS_OFF 0

static int verbosity = 3;
static int ready[2] = { 0, 0 };

static ws2811_t ledstring = {
    .render_wait_time = 0,
    .device = 0,
    .rpi_hw = 0,
    .freq = TARGET_FREQ,
    .dmanum = DMA,
    .channel =
    {
        [0] =
        {
            .gpionum = DEFAULT_PIN,     // Set to desired pin number
            .invert = 0,
            .count = DEFAULT_LEDS,                 // Set to number of LEDs
            .strip_type = STRIP_TYPE,
            .leds = NULL,
            .brightness = 255,
            .wshift = 0,
            .rshift = 0,
            .gshift = 0,
            .bshift = 0,
            .gamma = NULL,
        },
        [1] =
        {
            .gpionum = 0,
            .invert = 0,
            .count = 0,
            .strip_type = STRIP_TYPE,
            .leds = NULL,
            .brightness = 255,
            .wshift = 0,
            .rshift = 0,
            .gshift = 0,
            .bshift = 0,
            .gamma = NULL,
        },
    },
}; /* end ledstring */

extern int sled_init(uint32_t pin, uint32_t numleds, uint8_t fmt, uint8_t ch) {
    if (verbosity >= 3) printf("Strip LED Port: Init: pin=%d, numpixels =%d, format=%#x, channel=%d\n", pin, numleds, fmt, ch);

    if (ch > 1) {
        fprintf(stderr, "Strip LED Initialization: failed: unsupported channel number given.\n");
        return PI_INIT_FAILED;
    }
    if (pin > 31) {
        fprintf(stderr, "Strip LED Initialization: failed: unsupported pin given.\n");
        return PI_BAD_USER_GPIO;
    }
    ready[ch] = 0;
    ledstring.channel[ch].gpionum = pin;
    ledstring.channel[ch].count = numleds;
    switch (fmt & 0x0000000F) {
        case 0x00: ledstring.channel[ch].strip_type = WS2811_STRIP_GRB; break;
        case 0x01: ledstring.channel[ch].strip_type = WS2811_STRIP_GBR; break;
        case 0x02: ledstring.channel[ch].strip_type = WS2811_STRIP_RGB; break;
        case 0x03: ledstring.channel[ch].strip_type = WS2811_STRIP_RBG; break;
        case 0x04: ledstring.channel[ch].strip_type = WS2811_STRIP_BRG; break;
        case 0x05: ledstring.channel[ch].strip_type = WS2811_STRIP_BGR; break;
        case 0x06: ledstring.channel[ch].strip_type = SK6812_STRIP_GRBW; break;
        case 0x07: ledstring.channel[ch].strip_type = SK6812_STRIP_GBRW; break;
        case 0x08: ledstring.channel[ch].strip_type = SK6812_STRIP_RGBW; break;
        case 0x09: ledstring.channel[ch].strip_type = SK6812_STRIP_RBGW; break;
        case 0x0A: ledstring.channel[ch].strip_type = SK6812_STRIP_BRGW; break;
        case 0x0B: ledstring.channel[ch].strip_type = SK6812_STRIP_BGRW; break;
        case 0x0C: ledstring.channel[ch].strip_type = SK6812_SHIFT_WMASK; break;
        default: fprintf(stderr, "Strip LED Init: unknown type, using 2811 GRB format.\n"); break;
    }
    // Set line low initially
    //gpioSetMode(ledstring.channel[ch].gpionum, PI_OUTPUT);
    //gpioWrite(ledstring.channel[ch].gpionum, PI_LOW);
    // Initialize the ws2812 library
    ws2811_return_t ret;
    if ((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS)
    {
        if (verbosity >= 0) {
            fprintf(stderr, "Strip LED Init: failed, pin=%d\n", pin);
        }
        return PI_INIT_FAILED;
    }
    if (ledstring.channel[ch].leds == NULL) {
        // Some error in initialization.
        if (verbosity >= 0) {
            fprintf(stderr, "Strip LED Init: led buffer allocation failed, pin=%d\n", pin);
        }
        return PI_INIT_FAILED;
    }

    ready[ch] = 1;

    if (verbosity >= 1) printf("Strip LED Init: clearing buffer.\n");
    memset(ledstring.channel[ch].leds, 0x00, numleds*sizeof(ws2811_led_t));

    if (verbosity >= 1) printf("Strip LED Init: ready.\n");
    return 0;
}

extern int sled_end(uint8_t ch) {
    if (verbosity >= 3) printf("Strip LED Port: end all channels.\n");

    ready[0] = 0; ready[1] = 0;
    ws2811_fini(&ledstring);
    return 0;
}

extern int sled_render(uint8_t ch) {
    if (verbosity >= 3) printf("Strip LED Port: render, channel=%d\n", ch);

    if (!ready[ch]) {
        if (verbosity >= 2) fprintf(stderr, "Strip LED Port: not ready.");
        return PI_NOT_INITIALISED;
    }
    // Send signal to the strip
    ws2811_return_t ret;
    if ((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS)
    {
        fprintf(stderr, "Strip LED Port: render failed: %s\n", ws2811_get_return_t_str(ret));
    }
    // Set line low
    //gpioWrite(ledstring.channel[ch].gpionum, PI_LOW);
    return 0;
}

extern int sled_setled(uint32_t led, uint32_t val, uint8_t ch) {
    if (verbosity >= 3) printf("Strip LED Port: set led: led=%d, val=%#x, channel=%d\n", led, val, ch);

    // Bound check: ready
    if (!ready[ch]) {
        if (verbosity >= 2) fprintf(stderr, "Strip LED Port: not ready.");
        return PI_NOT_INITIALISED;
    }
    // Bound check: led in range
    if (led >= ledstring.channel[ch].count) {
        if (verbosity >= 2) fprintf(stderr, "Strip LED Port: led not in range.");
        return PI_BAD_PARAM;
    }
    // Bound check: leds memory properly allocated
    if (ledstring.channel[ch].leds == NULL) {
        if (verbosity >= 2) fprintf(stderr, "Strip LED Port: led port not initialized.");
        return PI_NOT_INITIALISED;
    }

    printf("Set led %d to %#x.\n", led, (ws2811_led_t)val);

    ledstring.channel[ch].leds[led] = (ws2811_led_t)val;
    return 0;
}