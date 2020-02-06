/*
 * rest.c
 *
 *  Created on: 05.02.2020
 *      Author: Gerry
 */

#include "rest.h"
#include "led.h"
#include "camera.h"
#include "mulcast.h"
#include <esp_log.h>
#include <esp_http_server.h>
#include <tcpip_adapter.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp_err.h>

// Handles WiFi status changes and manages webserver execution
static esp_err_t event_handler(void* ctx, system_event_t* event);
// Starts the HTTP daemon server
static httpd_handle_t start_webserver(void);
// Stops the HTTP daemon server
static void stop_webserver(httpd_handle_t server);
// Handles HTTP GET: "Image Snapshot"
static esp_err_t httpd_handler_jpg(httpd_req_t* req);
// Handles HTTP POST: "Start LED"
static esp_err_t httpd_handler_start_led(httpd_req_t* req);

static const char* LOGGER_NAME = "REST";	// logger name tag

// HTTP GET service definition: "Image Snapshot"
static httpd_uri_t uri_handler_jpg = {
	.uri = "/jpg",
	.method = HTTP_GET,
	.handler = httpd_handler_jpg
};

// HTTP POST service definition: "Start LED"
static httpd_uri_t uri_handler_start_leds = {
	.uri = "/start_led",
	.method = HTTP_POST,
	.handler = httpd_handler_start_led
};

int rest_setup(void) {
    ESP_LOGI(LOGGER_NAME, "Initializing...");
    tcpip_adapter_init();


    static httpd_handle_t server = NULL;	// The HTTP-Server (REST) reference
    int err = esp_event_loop_init(event_handler, &server);

    ESP_ERROR_CHECK(err);
    if (err == ESP_OK) {
		wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
		err = esp_wifi_init(&cfg) | esp_wifi_set_storage(WIFI_STORAGE_RAM);

		if (err == ESP_OK) {
			wifi_config_t wifi_config = {
				.sta = {
					.ssid = CONFIG_WIFI_SSID,
					.password = CONFIG_WIFI_PASSWORD,
				},
			};

			ESP_LOGI(LOGGER_NAME, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
			err = esp_wifi_set_mode(WIFI_MODE_STA) | esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
		}
    }

    return err;
}

void rest_start(void) {
    ESP_ERROR_CHECK(esp_wifi_start());
}

// Handles WiFi status changes and manages webserver execution
static esp_err_t event_handler(void* ctx, system_event_t* event) {
    httpd_handle_t* webserver = (httpd_handle_t*) ctx;

    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START: {
            ESP_LOGI(LOGGER_NAME, "SYSTEM_EVENT_STA_START");
            ESP_ERROR_CHECK(esp_wifi_connect());
            break;
        }
        case SYSTEM_EVENT_STA_GOT_IP: {
            ESP_LOGI(LOGGER_NAME, "SYSTEM_EVENT_STA_GOT_IP");
            ESP_LOGI(LOGGER_NAME, "Got IP: '%s'", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));

            /* Start the web server */
            if (*webserver == NULL) {
            	*webserver = start_webserver();
                mulcast_enable(1);
            }

            break;
        }
        case SYSTEM_EVENT_STA_DISCONNECTED: {
            ESP_LOGI(LOGGER_NAME, "SYSTEM_EVENT_STA_DISCONNECTED");

            /* Stop the web server */
            if (*webserver != NULL) {
            	mulcast_enable(0);
                stop_webserver(*webserver);
                *webserver = NULL;
            }

            ESP_ERROR_CHECK(esp_wifi_connect());
            break;
        }
        default: {
            break;
        }
    }

    return ESP_OK;
}

// Starts the HTTP daemon server
static httpd_handle_t start_webserver(void) {
    httpd_handle_t webserver = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    ESP_LOGI(LOGGER_NAME, "Starting server on port: '%d'", config.server_port);

    if (httpd_start(&webserver, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(LOGGER_NAME, "Registering URI handlers");
        httpd_register_uri_handler(webserver, &uri_handler_jpg);
        httpd_register_uri_handler(webserver, &uri_handler_start_leds);
        return webserver;
    }

    ESP_LOGI(LOGGER_NAME, "Error starting server!");
    return NULL;
}

// Stops the HTTP daemon server
static void stop_webserver(httpd_handle_t webserver) {
    httpd_stop(webserver);
}

// Handles HTTP GET: "Image Snapshot" request
static esp_err_t httpd_handler_jpg(httpd_req_t* req) {
    camera_fb_t* fb = NULL;
    esp_err_t res = ESP_OK;
    size_t fb_len = 0;
    int64_t fr_start = esp_timer_get_time();

    fb = esp_camera_fb_get();

    if (!fb) {
        ESP_LOGE(LOGGER_NAME, "Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    res = httpd_resp_set_type(req, "image/jpeg");

    if (res == ESP_OK) {
        res = httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    }

    if (res == ESP_OK) {
        fb_len = fb->len;
        res = httpd_resp_send(req, (const char *) fb->buf, fb->len);
    }

    esp_camera_fb_return(fb);

    int64_t fr_end = esp_timer_get_time();
    ESP_LOGI(LOGGER_NAME, "JPG: %uKB %ums", (uint32_t) (fb_len / 1024), (uint32_t) ((fr_end - fr_start) / 1000));
    return res;
}

// Handles HTTP POST: "Start LED"
static esp_err_t httpd_handler_start_led(httpd_req_t* req){
    char buf[100];
    int ret = 0;
    ret = req->content_len;
    int remaining = req->content_len;
    struct led_state new_state;

    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
        }

        remaining -= ret;
    }

    if (ret == 8) {
        int color;
        sscanf(buf, "%x", &color);

        for (int led = 0; led < NUM_LEDS; led++) {
            new_state.leds[led] = color;
        }
    }

    led_write(new_state);
    return httpd_resp_send(req, NULL, 0);	// 200 OK
}
