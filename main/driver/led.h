/*
 * led.h
 *
 *  Created on: 05.02.2020
 *      Author: Flo
 */

#ifndef MAIN_DRIVER_LED_H_
#define MAIN_DRIVER_LED_H_

#include <stdint.h>

#define NUM_LEDS 50

typedef struct led_state {
    uint32_t leds[NUM_LEDS];
} led_state;

int led_setup(void);
void led_write(led_state new_state);

#endif /* MAIN_DRIVER_LED_H_ */
