#include <stdint.h>

#define SLED_DEFAULT_VALUE 0xFF

extern void sled_startup(const char *default_sledconf_path, const char *config_dir_env);
extern int sled_channelsetup(uint32_t numleds, uint32_t pin, uint8_t fmt, uint8_t ch);
extern int sled_begin();
extern int sled_end();
extern int sled_setled(uint32_t led, uint32_t val, uint8_t ch);
extern int sled_render();

