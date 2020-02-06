/*
 * flash.c
 *
 *  Created on: 06.02.2020
 *      Author: Gerry
 */

#include "flash.h"
#include <esp_log.h>
#include <nvs_flash.h>

static const char* LOGGER_NAME = "FLASH";	// logger name tag

int flash_setup(void) {
	ESP_LOGI(LOGGER_NAME, "Initializing...");
	int err = nvs_flash_init();
	ESP_ERROR_CHECK(err);
	return err;
}
