#include <stdint.h>

extern int sled_init(uint32_t pin, uint32_t numleds, uint8_t fmt, uint8_t ch);
extern int sled_chan(uint32_t pin, uint32_t numleds)
extern int sled_render(uint8_t ch);
extern int sled_setled(uint32_t led, uint32_t val, uint8_t ch);
extern int sled_end(uint8_t ch);
