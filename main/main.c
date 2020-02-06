/*
 * main.c
 *
 *  Created on: 05.02.2020
 *      Author: Gerry
 */

#include "driver/flash.h"
#include "driver/camera.h"
#include "driver/led.h"
#include "driver/rest.h"
#include "driver/mulcast.h"
#include <esp_log.h>
#include <esp_err.h>

void app_main(void) {
	if ((flash_setup() | camera_setup() | led_setup() | rest_setup() | mulcast_setup()) != ESP_OK) {
		ESP_LOGI("APP", "Failed to setup application!");
	} else {
		ESP_LOGI("APP", "Starting application.");
		rest_start();
		mulcast_start();
		//while(1);
	}
}
