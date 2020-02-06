/*
 * camera.c
 *
 *  Created on: 05.02.2020
 *      Author: Gerry
 */

#include "camera.h"
#include <esp_log.h>

static const char* LOGGER_NAME = "CAM";	// logger name tag

// Camera config
static camera_config_t camera_config = {
	.pin_pwdn = -1,
	.pin_reset = CONFIG_RESET,
	.pin_xclk = CONFIG_XCLK,
	.pin_sscb_sda = CONFIG_SDA,
	.pin_sscb_scl = CONFIG_SCL,

	.pin_d7 = CONFIG_D7,
	.pin_d6 = CONFIG_D6,
	.pin_d5 = CONFIG_D5,
	.pin_d4 = CONFIG_D4,
	.pin_d3 = CONFIG_D3,
	.pin_d2 = CONFIG_D2,
	.pin_d1 = CONFIG_D1,
	.pin_d0 = CONFIG_D0,
	.pin_vsync = CONFIG_VSYNC,
	.pin_href = CONFIG_HREF,
	.pin_pclk = CONFIG_PCLK,

	//XCLK 20MHz or 10MHz
	.xclk_freq_hz = CONFIG_XCLK_FREQ,
	.ledc_timer = LEDC_TIMER_0,
	.ledc_channel = LEDC_CHANNEL_0,

	.pixel_format = PIXFORMAT_JPEG, //YUV422,GRAYSCALE,RGB565,JPEG
	.frame_size = FRAMESIZE_UXGA,   //QQVGA-UXGA Do not use sizes above QVGA when not JPEG

	.jpeg_quality = 12, //0-63 lower number means higher quality
	.fb_count = 1       //if more than one, i2s runs in continuous mode. Use only with JPEG
};

int camera_setup(void) {
	ESP_LOGI(LOGGER_NAME, "Initializing...");
	int err = esp_camera_init(&camera_config);
	ESP_ERROR_CHECK(err);
	return err;
}
