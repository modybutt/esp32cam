#ifndef ESP32_CAM_HTTP_JPG_LED_H
#define ESP32_CAM_HTTP_JPG_LED_H

#include <stdint.h>


#ifndef NUM_LEDS
#define NUM_LEDS 50
#endif


struct led_state {
    uint32_t leds[NUM_LEDS];
};

void init_leds(void);

void write_leds(struct led_state new_state);

#endif //ESP32_CAM_HTTP_JPG_LED_H
