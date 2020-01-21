#include <nvs_flash.h>
#include "rest.h"
#include "LED.h"

#define RED   0xFFFFFF
#define GREEN 0x00FF00
#define BLUE  0x0000FF
#define NUM_LEDS 3

void app_main() {
    static httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(nvs_flash_init());
    init_camera();
    init_wifi(&server);
    printf("init led");
    init_leds();
//    printf("starting led");
//    //reset_leds();
//    struct led_state new_state;
//    new_state.leds[0] = RED;
//    new_state.leds[1] = GREEN;
//    new_state.leds[2] = BLUE;
//
//    write_leds(new_state);
}

