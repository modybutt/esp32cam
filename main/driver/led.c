/*
 * led.c
 *
 *  Created on: 05.02.2020
 *      Author: Flo
 */

#include "led.h"
#include <esp_log.h>
#include <driver/rmt.h>

#define LED_RMT_TX_CHANNEL RMT_CHANNEL_0
#define LED_RMT_TX_GPIO 14

#define BITS_PER_LED_CMD 24
#define LED_BUFFER_ITEMS ((NUM_LEDS * BITS_PER_LED_CMD))

#define T0H 8
#define T1H 17
#define T1L 8
#define T0L 17

#define GREEN 0xFF0000
#define RED   0x00FF00
#define BLUE  0x0000FF
#define WHITE 0xFFFFFF
#define BLACK 0x000000

static const char* LOGGER_NAME = "LED";	// logger name tag

void setup_rmt_data_buffer(struct led_state new_state);

rmt_item32_t led_data_buffer[LED_BUFFER_ITEMS];

int led_setup(void) {
	ESP_LOGI(LOGGER_NAME, "Initializing...");

    rmt_config_t config;
    config.rmt_mode = RMT_MODE_TX;
    config.channel = LED_RMT_TX_CHANNEL;
    config.gpio_num = LED_RMT_TX_GPIO;
    config.mem_block_num = 2;
    config.tx_config.loop_en = false;
    config.tx_config.carrier_freq_hz = 100;
    config.tx_config.carrier_duty_percent = 50;
    config.tx_config.carrier_en = false;
    config.tx_config.idle_output_en = true;
    config.tx_config.idle_level = 0;
    config.clk_div = 4;

    int err = rmt_config(&config);
    ESP_ERROR_CHECK(err);

    if (err == ESP_OK) {
		err = rmt_driver_install(config.channel, 0, 0);
		ESP_ERROR_CHECK(err);
    }

    return err;
}

void led_write(struct led_state new_state) {
    setup_rmt_data_buffer(new_state);
    ESP_ERROR_CHECK(rmt_write_items(LED_RMT_TX_CHANNEL, led_data_buffer, LED_BUFFER_ITEMS, false));
    ESP_ERROR_CHECK(rmt_wait_tx_done(LED_RMT_TX_CHANNEL, portMAX_DELAY));
}


void setup_rmt_data_buffer(struct led_state new_state) {
    for (uint32_t led = 0; led < NUM_LEDS; led++) {
        uint32_t bits_to_send = new_state.leds[led];
        uint32_t mask = 1 << (BITS_PER_LED_CMD - 1);

        for (uint32_t bit = 0; bit < BITS_PER_LED_CMD; bit++) {
            uint32_t bit_is_set = bits_to_send & mask;
            led_data_buffer[led * BITS_PER_LED_CMD + bit] = bit_is_set ? (rmt_item32_t) {{{T1H, 1, T1L, 0}}} : (rmt_item32_t) {{{T0H, 1, T0L, 0}}};
            mask >>= 1;
        }
    }
}
