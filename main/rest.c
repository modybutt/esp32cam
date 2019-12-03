/*
 * rest.c
 *
 *  Created on: 30.11.2019
 *      Author: Gerry
 */
#include "rest.h"
#include <esp_wifi.h>
#include <esp_log.h>



// Initializes the camera driver
void init_camera() {
	ESP_LOGI(TAG, "Initializing Camera...");
	ESP_ERROR_CHECK(esp_camera_init(&camera_config));
}

// Initializes the wifi connection
void init_wifi(void *arg) {
	ESP_LOGI(TAG, "Initializing WiFi...");
	tcpip_adapter_init();
	ESP_ERROR_CHECK(esp_event_loop_init(event_handler, arg));

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

	wifi_config_t wifi_config = {
		.sta = {
			.ssid = CONFIG_WIFI_SSID,
			.password = CONFIG_WIFI_PASSWORD,
		},
	};

	ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());
}

// Handles WiFi status changes and manages webserver execution
static esp_err_t event_handler(void *ctx, system_event_t *event) {
	httpd_handle_t *server = (httpd_handle_t *)ctx;

	switch (event->event_id) {
		case SYSTEM_EVENT_STA_START: {
			ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
			ESP_ERROR_CHECK(esp_wifi_connect());
			break;
		}
		case SYSTEM_EVENT_STA_GOT_IP: {
			ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
			ESP_LOGI(TAG, "Got IP: '%s'", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));

			/* Start the web server */
			if (*server == NULL) {
				*server = start_webserver();
			}

			break;
		}
		case SYSTEM_EVENT_STA_DISCONNECTED: {
			ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
			ESP_ERROR_CHECK(esp_wifi_connect());

			/* Stop the web server */
			if (*server) {
				stop_webserver(*server);
				*server = NULL;
			}

			break;
		}
		default: {
			break;
		}
	}

	return ESP_OK;
}

// Starts the HTTP daemon server
httpd_handle_t start_webserver(void) {
	httpd_handle_t server = NULL;
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);

	if (httpd_start(&server, &config) == ESP_OK) {
		// Set URI handlers
		ESP_LOGI(TAG, "Registering URI handlers");
		httpd_register_uri_handler(server, &uri_handler_jpg);
		return server;
	}

	ESP_LOGI(TAG, "Error starting server!");
	return NULL;
}

// Stops the HTTP daemon server
void stop_webserver(httpd_handle_t server) {
	httpd_stop(server);
}

// Handles HTTP GET: "Image" request
static esp_err_t jpg_httpd_handler(httpd_req_t *req) {
	camera_fb_t *fb = NULL;
	esp_err_t res = ESP_OK;
	size_t fb_len = 0;
	int64_t fr_start = esp_timer_get_time();

	fb = esp_camera_fb_get();

	if (!fb) {
		ESP_LOGE(TAG, "Camera capture failed");
		httpd_resp_send_500(req);
		return ESP_FAIL;
	}

	res = httpd_resp_set_type(req, "image/jpeg");

	if (res == ESP_OK) {
		res = httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
	}

	if (res == ESP_OK) {
		fb_len = fb->len;
		res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
	}

	esp_camera_fb_return(fb);

	int64_t fr_end = esp_timer_get_time();
	ESP_LOGI(TAG, "JPG: %uKB %ums", (uint32_t)(fb_len / 1024), (uint32_t)((fr_end - fr_start) / 1000));
	return res;
}

//static size_t jpg_encode_stream(void *arg, size_t index, const void *data, size_t len)
//{
//  jpg_chunking_t *j = (jpg_chunking_t *)arg;
//  if (!index)
//  {
//    j->len = 0;
//  }
//  if (httpd_resp_send_chunk(j->req, (const char *)data, len) != ESP_OK)
//  {
//    return 0;
//  }
//  j->len += len;
//  return len;
//}
