#include <stdio.h>
#include <pigpio.h>
#include <string.h>
#include "stripleds.h"

#include "ws2811.h"

#define TARGET_FREQ WS2811_TARGET_FREQ
#define DMA 10
#define STRIP_TYPE WS2812_STRIP
#define DEFAULT_PIN_CH0 18
#define DEFAULT_PIN_CH1 12
#define DEFAULT_LEDS 64
#define DEFAULT_LEDS_OFF 0
#define NUM_CHANNELS 2

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
            .gpionum = DEFAULT_PIN_CH0,     // Set to desired pin number
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

extern int sled_channelsetup(uint32_t numleds, uint32_t pin, uint8_t fmt, uint8_t ch) {
    if (verbosity >= 3) printf("Strip LED Port: channel setup: numleds =%d, pin=%d, format=%#x, channel=%d\n", numleds, pin, fmt, ch);

    if (fmt == SLED_DEFAULT_VALUE) fmt = 0;
    if (ch == SLED_DEFAULT_VALUE) ch = 0;
    if (pin == 0) pin = ch == 0 ? DEFAULT_PIN_CH0 : DEFAULT_PIN_CH1;

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

    if (verbosity >= 1) printf("Strip LED Port: configured channel %d.\n", ch);
    return 0;
}

extern int sled_begin() {
    // Bound check, not already running
    if (ready[0] || ready[1]) {
        if (verbosity >= 0) {
            fprintf(stderr, "Strip LED already running.\n");
        }
        return 0;
    }
    // Bound check: Nothing to configure
    if (!ledstring.channel[0].count && !ledstring.channel[1].count) {
        if (verbosity >= 0) {
            fprintf(stderr, "Strip LED Port: no channels configured.\n");
        }
        return PI_INIT_FAILED;
    }

    // Initialize the ws2812 library
    ws2811_return_t ret;
    if ((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS)
    {
        if (verbosity >= 0) {
            fprintf(stderr, "Strip LED Port: init failed: ");
            for (int ch = 0; ch < NUM_CHANNELS; ch++) {
                // Write out all failed channels
                fprintf(stderr, "chan=%d, gpio =%d, leds=%d",
                    ch,
                    ledstring.channel[ch].gpionum,
                    ledstring.channel[ch].count
                );
                if (ch < NUM_CHANNELS-1) fprintf(stderr, ", ");
                else fprintf(stderr, ".");
            }
        }
        return PI_INIT_FAILED;
    }

    // Check channels started ok
    int ok = 0;
    for (int c = 0; c < NUM_CHANNELS; c++) {
        if (ledstring.channel[c].count > 0 && ledstring.channel[c].leds == NULL) {
            // Buffer not allocated.
            if (verbosity >= 0) {
                fprintf(stderr, "Strip LED Port: channel=%d on pin=%d failed to start.\n",
                    c,
                    ledstring.channel[c].gpionum
                );
            }
            return PI_INIT_FAILED;
        } else if (ledstring.channel[c].count > 0) {
            ready[c] = 1; ok = 1;
            if (verbosity >= 1) printf("Strip LED Port: clearing buffer for channel %d.\n", c);
            memset(ledstring.channel[c].leds, 0x00, ledstring.channel[c].count*sizeof(ws2811_led_t));
            if (verbosity >= 1) printf("Strip LED Port: channel %d ready.\n", c);
        }
    }

    // Only return failure if no channels successfully started
    return ok ? 0 : PI_INIT_FAILED;
}

extern int sled_end() {
    if (verbosity >= 3) printf("Strip LED Port: end all channels.\n");

    ready[0] = 0; ready[1] = 0;
    ws2811_fini(&ledstring);

    return 0;
}

extern int sled_render() {
    if (verbosity >= 3) printf("Strip LED Port: render\n");

    if ((ledstring.channel[0].count > 0 && !ready[0]) || (ledstring.channel[1].count > 0 && !ready[1])) {
        if (verbosity >= 2) fprintf(stderr, "Strip LED Port: channels not ready.");
        return PI_NOT_INITIALISED;
    }

    // Send signal to the strip
    ws2811_return_t ret;
    if ((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS)
    {
        fprintf(stderr, "Strip LED Port: render failed: %s\n", ws2811_get_return_t_str(ret));
    }

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

    ledstring.channel[ch].leds[led] = (ws2811_led_t)val;
    return 0;
}