#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <pigpio.h>
#include <limits.h>
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

// Helper functions
static uint32_t get_fmt_type(uint8_t fmt);
static void set_default(int key, int value);
static char *trim(char *s);
static int keymap_size();

typedef struct {
    const char *name;
    int numeric_key;
    uint8_t ch;
    int min;
    int max;
} KeyMap;
static KeyMap key_map[];
static int keymap_size();

static int verbosity = 0;
static int ready[2] = { 0, 0 };
static int autobegin = 0;

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

void sled_startup(const char *default_sledconf_path, const char *config_dir_env) {

    char *user_sledconf_path = getenv(config_dir_env);
    FILE *fp;
    char line[512];
    const char *paths[2];
    
    paths[0] = default_sledconf_path;
    paths[1] = user_sledconf_path ? user_sledconf_path : NULL;

    for(int p = 0; p < 2; p++) {
        if(paths[p] == NULL) continue;

        fp = fopen(paths[p], "r");
        if(!fp) continue;

        if (verbosity >= 0) printf("Load Strip LED config file: %s...", paths[p]);

        while(fgets(line, sizeof(line), fp)) {
            char *s = trim(line);
            if(*s == '#' || *s == '\0') continue;  // comment or empty line

            char *eq = strchr(s, '=');
            if(!eq) continue;
            *eq = '\0';
            char *key_str = trim(s);
            char *value_str = trim(eq + 1);

            // If default, just skip it.
            if (strcmp(value_str, "default") == 0) continue;
            if (strcmp(value_str, "Default") == 0) continue;
            if (strcmp(value_str, "DEFAULT") == 0) continue;

            // Value is only the first word
            char *space = strpbrk(value_str, " \t#");
            if(space) *space = '\0';

            // Convert boolean strings to int
            int value;
            char *end;
            if ((strcmp(value_str, "true") == 0)
                ||(strcmp(value_str, "True") == 0)
                ||(strcmp(value_str, "TRUE") == 0)
            ) value = 1;
            else if((strcmp(value_str, "false") == 0)
                ||(strcmp(value_str, "False") == 0)
                ||(strcmp(value_str, "FALSE") == 0)
            ) value = 0;
            else value = strtol(value_str, &end, 0);

            // Find numeric key
            int map_index = -1;
            for(int i=0;i<keymap_size();i++) {
                if(strcmp(key_str, key_map[i].name) == 0) {
                    map_index = i;
                    break;
                }
            }

            if(map_index != -1) {
                if (verbosity >= 3) printf("Strip LED Port: key found in config file: %s, value %d\n", key_str, value);
                // Check value in range
                if ((value >= key_map[map_index].min) && (value <= key_map[map_index].max)) {
                    set_default(map_index, value);
                }
            } else {
                if (verbosity >= 1) fprintf(stderr, "Strip LED Port: unknown key in config file: %s\n", key_str);
            }
        }
        fclose(fp);
        if (verbosity >= 0) printf("Done.\n");
    }

    // Auto start the sled channels so they are ready for use.
    if (autobegin) sled_begin();
}

extern int sled_channelsetup(uint32_t numleds, uint32_t pin, uint8_t fmt, uint8_t ch) {

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

    if (verbosity >= 0) printf("Strip LED Port: channel setup: numleds =%d, pin=%d, format=%#x, channel=%d\n", numleds, pin, fmt, ch);

    ready[ch] = 0;
    ledstring.channel[ch].gpionum = pin;
    ledstring.channel[ch].count = numleds;
    ledstring.channel[ch].strip_type = get_fmt_type(ch);

    if (verbosity >= 0) printf("Strip LED Port: configured channel %d.\n", ch);
    return 0;
}

extern int sled_begin() {
    // Bound check, not already running
    if (ready[0] || ready[1]) {
        if (verbosity >= 0) {
            fprintf(stderr, "Strip LED Port: attempt to start when already running.\n");
        }
        return 0;
    }
    // Bound check: Nothing to configure
    if (!ledstring.channel[0].count && !ledstring.channel[1].count) {
        if (verbosity >= 0) {
            fprintf(stderr, "Strip LED Port: attempt to start with no channels configured.\n");
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
            if (verbosity >= 0) printf("Strip LED Port: channel %d ready on pin %d.\n", c, ledstring.channel[c].gpionum);
        }
    }

    // Only return failure if no channels successfully started
    return ok ? 0 : PI_INIT_FAILED;
}

extern int sled_end() {

    if (ready[0] || ready[1]) {
        ready[0] = 0; ready[1] = 0;
        ws2811_fini(&ledstring);
        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
            ledstring.channel[ch].leds = NULL;
        }
        if (verbosity >= 0) printf("Strip LED Port: deactivated all channels.\n");
    }

    return 0;
}

extern int sled_render() {
    if (verbosity >= 2) printf("Strip LED Port: render\n");

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
        if (verbosity >= 2) fprintf(stderr, "Strip LED Port: unable to set led, not ready.");
        return PI_NOT_INITIALISED;
    }
    // Bound check: led in range
    if (led >= ledstring.channel[ch].count) {
        if (verbosity >= 2) fprintf(stderr, "Strip LED Port: unable to set led, not in range.");
        return PI_BAD_PARAM;
    }
    // Bound check: leds memory properly allocated
    if (ledstring.channel[ch].leds == NULL) {
        if (verbosity >= 2) fprintf(stderr, "Strip LED Port: unable to set led, port not initialized.");
        return PI_NOT_INITIALISED;
    }

    ledstring.channel[ch].leds[led] = (ws2811_led_t)val;
    return 0;
}

// Static helper funciton definitions

static uint32_t get_fmt_type(uint8_t fmt) {
    switch (fmt & 0x0F) {
        case 0x00: return(WS2811_STRIP_GRB);
        case 0x01: return(WS2811_STRIP_GBR);
        case 0x02: return(WS2811_STRIP_RGB);
        case 0x03: return(WS2811_STRIP_RBG);
        case 0x04: return(WS2811_STRIP_BRG);
        case 0x05: return(WS2811_STRIP_BGR);
        case 0x06: return(SK6812_STRIP_GRBW);
        case 0x07: return(SK6812_STRIP_GBRW);
        case 0x08: return(SK6812_STRIP_RGBW);
        case 0x09: return(SK6812_STRIP_RBGW);
        case 0x0A: return(SK6812_STRIP_BRGW);
        case 0x0B: return(SK6812_STRIP_BGRW);
        case 0x0C: return(SK6812_SHIFT_WMASK);
        default:
            fprintf(stderr, "Strip LED Port: unknown type, using 2811 GRB format.\n");
            return(WS2811_STRIP_GRB);
    }
}

// Map string keys to numeric keys
#define CONF_AUTOBEGIN      0
#define CONF_VERBOSITY      1
#define CONF_DMANUM         2
#define CONF_FREQUENCY      3
#define CONF_RENDERWAIT     4
#define CONF_CH_GPIONUM     10
#define CONF_CH_COUNT       11
#define CONF_CH_STRIP       12
#define CONF_CH_INVERT      13
#define CONF_CH_BRIGHT      14
#define CONF_CH_WSHIFT      15
#define CONF_CH_RSHIFT      16
#define CONF_CH_GSHIFT      17
#define CONF_CH_BSHIFT      18

static KeyMap key_map[] = {
    {"autobegin",           CONF_AUTOBEGIN, -1,     0,      1  },
    {"verbosity",           CONF_VERBOSITY, -1,    -1,      5  },
    {"dmanum",              CONF_DMANUM,    -1,     0,      32 },
    {"freq",                CONF_FREQUENCY, -1,     400000, 800000 },
    {"render_wait_time",    CONF_RENDERWAIT,-1,     0,      1  },
    {"ch0:gpionum",         CONF_CH_GPIONUM, 0,     0,      31 },
    {"ch0:count",           CONF_CH_COUNT,   0,     0,      INT_MAX },
    {"ch0:strip_type",      CONF_CH_STRIP,   0,     0,      0xF },
    {"ch0:invert",          CONF_CH_INVERT,  0,     0,      1   },
    {"ch0:brightness",      CONF_CH_BRIGHT,  0,     0,      255 },
    {"ch0:wshift",          CONF_CH_WSHIFT,  0,     0,      255 },
    {"ch0:rshift",          CONF_CH_RSHIFT,  0,     0,      255 },
    {"ch0:gshift",          CONF_CH_GSHIFT,  0,     0,      255 },
    {"ch0:bshift",          CONF_CH_BSHIFT,  0,     0,      255 },
    {"ch1:gpionum",         CONF_CH_GPIONUM, 1,     0,      31 },
    {"ch1:count",           CONF_CH_COUNT,   1,     0,      INT_MAX },
    {"ch1:strip_type",      CONF_CH_STRIP,   1,     0,      0xF },
    {"ch1:invert",          CONF_CH_INVERT,  1,     0,      1   },
    {"ch1:brightness",      CONF_CH_BRIGHT,  1,     0,      255 },
    {"ch1:wshift",          CONF_CH_WSHIFT,  1,     0,      255 },
    {"ch1:rshift",          CONF_CH_RSHIFT,  1,     0,      255 },
    {"ch1:gshift",          CONF_CH_GSHIFT,  1,     0,      255 },
    {"ch1:bshift",          CONF_CH_BSHIFT,  1,     0,      255 },
};

static int keymap_size()  {
    return (sizeof(key_map)/sizeof(key_map[0]));
}

// Trim leading and trailing whitespace
static char *trim(char *s) {
    char *end;
    while(isspace((unsigned char)*s)) s++;
    if(*s == 0) return s;
    end = s + strlen(s) - 1;
    while(end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}

// Example static callback
static void set_default(int map_index, int value) {
    // Replace with actual handling
    if (verbosity >= 3) printf("Key index %d = %d\n", map_index, value);
    int ch = key_map[map_index].ch;
    switch (key_map[map_index].numeric_key) {
        case CONF_AUTOBEGIN:
            autobegin = value;
            break;
        case CONF_VERBOSITY:
            verbosity = value;
            break;
        case CONF_DMANUM:
            ledstring.dmanum = value;
            break;
        case CONF_FREQUENCY:
            ledstring.freq = value;
            break;
        case CONF_RENDERWAIT:
            ledstring.render_wait_time = value;
            break;
        case CONF_CH_GPIONUM:
            ledstring.channel[ch].gpionum = value;
            break;
        case CONF_CH_COUNT:
            ledstring.channel[ch].count = value;
            break;
        case CONF_CH_STRIP:
            ledstring.channel[ch].strip_type = get_fmt_type(value);
            break;
        case CONF_CH_INVERT:
            ledstring.channel[ch].invert = value;
            break;
        case CONF_CH_BRIGHT:
            ledstring.channel[ch].brightness = value;
            break;
        case CONF_CH_WSHIFT:
            ledstring.channel[ch].wshift = value;
            break;
        case CONF_CH_RSHIFT:
            ledstring.channel[ch].rshift = value;
            break;
        case CONF_CH_GSHIFT:
            ledstring.channel[ch].gshift = value;
            break;
        case CONF_CH_BSHIFT:
            ledstring.channel[ch].bshift = value;
            break;
        default:
            if (verbosity >= 2)
                fprintf(stderr, "Strip LED Port: unrecognized config option: %s\n",
                    key_map[map_index].name
                );
            break;
    } /* end switch */
}