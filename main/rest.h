/*
 * rest.h
 *
 *  Created on: 30.11.2019
 *      Author: Gerry
 */

#ifndef MAIN_REST_H_
#define MAIN_REST_H_


#include <esp_http_server.h>
#include <esp_camera.h>
#include <esp_event_loop.h>


//typedef struct {
//  httpd_req_t *req;
//  size_t len;
//} jpg_chunking_t;

static const char *TAG = "example:http_jpg";

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

// Initializes the camera driver
void init_camera();

// Initializes the wifi connection
void init_wifi(void *arg);

// Handles WiFi status changes and manages webserver execution
static esp_err_t event_handler(void *ctx, system_event_t *event);

// Starts the HTTP daemon server
static httpd_handle_t start_webserver(void);

// Stops the HTTP daemon server
static void stop_webserver(httpd_handle_t server);

// Handles HTTP GET: "Image" request
static esp_err_t jpg_httpd_handler(httpd_req_t *req);

// Handles HTTP GET: "Lamp ON" request
static esp_err_t lamp_on_httpd_handler(httpd_req_t *req);

// Handles HTTP GET: "Lamp OFF" request
static esp_err_t lamp_off_httpd_handler(httpd_req_t *req);

// Turn LED On or Off
static esp_err_t enable_led(int enable);

static httpd_uri_t uri_handler_jpg = {
    .uri = "/jpg",
    .method = HTTP_GET,
    .handler = jpg_httpd_handler
};

#endif /* MAIN_REST_H_ */
