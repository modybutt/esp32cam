#ifndef ESP32_CAM_HTTP_JPG_LED_H
#define ESP32_CAM_HTTP_JPG_LED_H

#include <stdint.h>

#ifndef NUM_LEDS
#define NUM_LEDS 50
#endif

// This structure is used for indicating what the colors of each LED should be set to.
// There is a 32bit value for each LED. Only the lower 3 bytes are used and they hold the
// Red (byte 2), Green (byte 1), and Blue (byte 0) values to be set.
struct led_state {
    uint32_t leds[NUM_LEDS];
};

// Setup the hardware peripheral. Only call this once.
void init_leds(void);

// Update the LEDs to the new state. Call as needed.
// This function will block the current task until the RMT peripheral is finished sending
// the entire sequence.
void write_leds(struct led_state new_state);

void turn_leds_off(void);

void reset_leds_rmt_buffer(void);
#endif //ESP32_CAM_HTTP_JPG_LED_H
