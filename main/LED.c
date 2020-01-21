#include "LED.h"
#include "driver/rmt.h"

// Configure these based on your project needs ********
#define LED_RMT_TX_CHANNEL RMT_CHANNEL_0
#define LED_RMT_TX_GPIO 14
// ****************************************************

#define BITS_PER_LED_CMD 24
#define LED_BUFFER_ITEMS ((NUM_LEDS * BITS_PER_LED_CMD))

// These values are determined by measuring pulse timing with logic analyzer and adjusting to match datasheet.
#define T0H 8 //4 //12  // 0 bit high time 14
#define T1H 17 //8 //36  // 1 bit high time 52
#define T1L 8//4 //12  // low time for either bit 52
#define T0L 17 //8 //36

#define GREEN 0xFF0000
#define RED   0x00FF00
#define BLUE  0x0000FF
#define WHITE 0xFFFFFF
#define BLACK 0x000000
#define NUM_LEDS 50
// This is the buffer which the hw peripheral will access while pulsing the output pin
rmt_item32_t led_data_buffer[LED_BUFFER_ITEMS];

void setup_rmt_data_buffer(struct led_state new_state);

void init_leds(void) {
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

//    struct led_state new_state;
//    for (int led = 0; led < NUM_LEDS; led++) {
//        new_state.leds[led] = WHITE;
//    }


    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, 0));
    //int i = 0;
    //write_leds(new_state);

}

void write_leds(struct led_state new_state) {
    setup_rmt_data_buffer(new_state);
    ESP_ERROR_CHECK(rmt_write_items(LED_RMT_TX_CHANNEL, led_data_buffer, LED_BUFFER_ITEMS, false));
    ESP_ERROR_CHECK(rmt_wait_tx_done(LED_RMT_TX_CHANNEL, portMAX_DELAY));
}

void turn_leds_off() {

    struct led_state new_state;
    for (int led = 0; led < NUM_LEDS; led++) {
        new_state.leds[led] = BLACK;
    }
        write_leds(new_state);
}

void setup_rmt_data_buffer(struct led_state new_state) {
    for (uint32_t led = 0; led < NUM_LEDS; led++) {
        uint32_t bits_to_send = new_state.leds[led];
        uint32_t mask = 1 << (BITS_PER_LED_CMD - 1);
        for (uint32_t bit = 0; bit < BITS_PER_LED_CMD; bit++) {
            uint32_t bit_is_set = bits_to_send & mask;
            led_data_buffer[led * BITS_PER_LED_CMD + bit] = bit_is_set ? (rmt_item32_t) {{{T1H, 1, T1L, 0}}}
                                                                       : (rmt_item32_t) {{{T0H, 1, T0L, 0}}};
            mask >>= 1;
        }
    }
}




