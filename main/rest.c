/*
 * rest.c
 *
 *  Created on: 30.11.2019
 *      Author: Gerry
 */
#include "rest.h"
#include "settings.h"
#include "mulmsg.h"
#include <nvs_flash.h>
#include <esp_camera.h>
#include <esp_event_loop.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <esp_now.h>
#include <tcpip_adapter.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <freertos/event_groups.h>
#include "LED.h"
#include <sys/param.h>


// Handles WiFi status changes and manages webserver execution
static esp_err_t event_handler(void *ctx, system_event_t *event);

// Starts the HTTP daemon server
static httpd_handle_t start_webserver(void);

// Stops the HTTP daemon server
static void stop_webserver(httpd_handle_t server);

// Handles HTTP GET: "Image" request
static esp_err_t jpg_httpd_handler(httpd_req_t *req);

// Creates an IPV4 multicast socket for receiving and sending messages
static int create_multicast_ipv4_socket();

// Multicast working task handling receiving and sending messages
static void mcast_worker_task(void *pvParameters);

// Registers to multicast group to receive messages
static int register_multicast_ipv4_group(int sock);

// Handles received multicast message
static int handle_mulmsg(int sock, mulmsg *message, const char *address);

// Sends a multicast message via socket
static int multicast_send(int sock, mulmsg *message, const char *address);
//handle the leds
static esp_err_t start_led_httpd_handler(httpd_req_t *req);


// Logger tag name
static const char *TAG = "LMS";
// FreeRTOS event group to signal when we are connected & ready to make a request
static EventGroupHandle_t wifi_event_group;

#ifdef CONFIG_MULTICAST_HANDSHAKE
static int handshakeDone = 0;
#endif

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

// HTTP GET service definition: "Image"
static httpd_uri_t uri_handler_jpg = {
        .uri = "/jpg",
        .method = HTTP_GET,
        .handler = jpg_httpd_handler
};

static httpd_uri_t uri_handler_start_leds = {
        .uri = "/start_led",
        .method = HTTP_POST,
        .handler = start_led_httpd_handler
};

static httpd_uri_t uri_handler_stop_leds = {
        .uri = "/stop_led",
        .method = HTTP_GET,
        .handler = stop_led_httpd_handler
};

// Initializes the flash driver
void init_flash() {
    ESP_LOGI(TAG, "Initializing Flash...");
    ESP_ERROR_CHECK(nvs_flash_init());
}

// Initializes the camera driver
void init_camera() {
    ESP_LOGI(TAG, "Initializing Camera...");
    ESP_ERROR_CHECK(esp_camera_init(&camera_config));
}

// Initializes the wifi driver
void init_wifi(httpd_handle_t *arg) {
    ESP_LOGI(TAG, "Initializing WiFi...");
    tcpip_adapter_init();

    wifi_event_group = xEventGroupCreate();
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

    xTaskCreate(&mcast_worker_task, "mcast_task", 4096, NULL, 5, NULL);
}

// Handles WiFi status changes and manages webserver execution
static esp_err_t event_handler(void *ctx, system_event_t *event) {
    httpd_handle_t *server = (httpd_handle_t *) ctx;

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
                xEventGroupSetBits(wifi_event_group, BIT0);
            }

            break;
        }
        case SYSTEM_EVENT_STA_DISCONNECTED: {
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
            ESP_ERROR_CHECK(esp_wifi_connect());

            /* Stop the web server */
            if (*server) {
                xEventGroupClearBits(wifi_event_group, BIT0);
                stop_webserver(*server);
                *server = NULL;
            }

            esp_wifi_connect();
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
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);

    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &uri_handler_jpg);
        httpd_register_uri_handler(server, &uri_handler_start_leds);
        httpd_register_uri_handler(server, &uri_handler_stop_leds);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

// Stops the HTTP daemon server
static void stop_webserver(httpd_handle_t server) {
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
        res = httpd_resp_send(req, (const char *) fb->buf, fb->len);
    }

    esp_camera_fb_return(fb);

    int64_t fr_end = esp_timer_get_time();
    ESP_LOGI(TAG, "JPG: %uKB %ums", (uint32_t) (fb_len / 1024), (uint32_t) ((fr_end - fr_start) / 1000));
    return res;
}

static esp_err_t start_led_httpd_handler(httpd_req_t *req) {

    char buf[100];

    int ret = 0;
    ret = req->content_len;
    int remaining = req->content_len;
    struct led_state new_state;

    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf,
                                  MIN(remaining, sizeof(buf)))) <= 0) {
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
    write_leds(new_state);
    const char resp[] = "200 OK";
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// Creates an IPV4 multicast socket for receiving and sending messages
static int create_multicast_ipv4_socket() {
    struct sockaddr_in saddr = {0};
    int sock = -1;
    int err = 0;

    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket. Error %d", errno);
        return -1;
    }

	// Bind the socket to any address
	saddr.sin_family = PF_INET;
	saddr.sin_port = htons(CONFIG_MULTICAST_PORT);
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	//inet_aton("192.168.4.1", &saddr.sin_addr.s_addr);

    err = bind(sock, (struct sockaddr *) &saddr, sizeof(struct sockaddr_in));

    if (err < 0) {
        ESP_LOGE(TAG, "Failed to bind socket. Error %d", errno);
        close(sock);
        return -1;
    }

    // Set Time To Live to not forward messages
    uint8_t ttl = 0;
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(uint8_t));

    if (err < 0) {
        ESP_LOGE(TAG, "Failed to set IP_MULTICAST_TTL. Error %d", errno);
        close(sock);
        return -1;
    }

	// Disable device multicast loopback		DEFAULT
//	uint8_t loopback_val = 0;
//	err = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &loopback_val, sizeof(uint8_t));
//
//	if (err < 0) {
//		ESP_LOGE(TAG, "Failed to set IP_MULTICAST_LOOP. Error %d", errno);
//	    close(sock);
//		return -1;
//	}

    // Register to multicast group for receiving messages
    err = register_multicast_ipv4_group(sock);

    if (err < 0) {
        ESP_LOGE(TAG, "Failed to register multicast group. Error %d", errno);
        close(sock);
        return -1;
    }

    return sock;
}

// Registers to multicast group to receive messages
static int register_multicast_ipv4_group(int sock) {
    struct ip_mreq imreq = { 0 };
//    struct in_addr iaddr = { 0 };
//    esp_netif_ip_info_t ip_info = { 0 };
    int err = 0;

//    err = esp_netif_get_ip_info(get_example_netif(), &ip_info);
//
//    if (err != ESP_OK) {
//		ESP_LOGE(TAG, "Failed to get IP address info. Error 0x%x", err);
//		return -1;
//	}
//
//	inet_addr_from_ip4addr(&iaddr, &ip_info.ip);

    //tcpip_adapter_ip_info_t ip_info = { 0 };
    //tcpip_adapter_get_ip_info(ESP_IF_WIFI_STA, &ip_info);
    //ESP_LOGI(TAG, "IP %s", ip4addr_ntoa(&ip_info.ip));
    //inet_addr_from_ip4addr(&iaddr, &ip_info.ip);

    // Configure multicast address to listen to
    err = inet_aton(CONFIG_MULTICAST_ADDR, &imreq.imr_multiaddr.s_addr);

    if (err != 1) {
        ESP_LOGE(TAG, "Configured IPV4 multicast address '%s' is invalid.", CONFIG_MULTICAST_ADDR);
        return -1;
    }

	// Check for valid address range
    if (!IP_MULTICAST(ntohl(imreq.imr_multiaddr.s_addr))) {
        ESP_LOGW(TAG, "Configured IPV4 multicast address '%s' is not a valid multicast address. This will probably not work.", CONFIG_MULTICAST_ADDR);
    } else {
    	ESP_LOGI(TAG, "Configured IPV4 multicast address %s", inet_ntoa(imreq.imr_multiaddr.s_addr));
    }

//    // Configure multicast source interface
//	err = inet_aton(ip4addr_ntoa(&ip_info.ip), &imreq.imr_interface.s_addr);
//
//	if (err != 1) {
//		ESP_LOGE(TAG, "Configured IPV4 multicast source interface '%s' is invalid.", ip4addr_ntoa(&ip_info.ip));
//		return -1;
//	}

    // Assign the IPV4 multicast source interface via its IP
//    err = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, &iaddr, sizeof(struct in_addr));
//
//    if (err < 0) {
//		ESP_LOGE(TAG, "Failed to set IP_MULTICAST_IF. Error %d", errno);
//		return -1;
//	}

    // Assign to the multicast group
    err = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imreq, sizeof(struct ip_mreq));

    if (err < 0) {
        ESP_LOGE(TAG, "Failed to set IP_ADD_MEMBERSHIP. Error %d", errno);
        return -1;
    }

    return err;
}

// Multicast working task handling receiving and sending messages
static void mcast_worker_task(void *pvParameters) {
    while (1) {
        // Wait for the ip address to be set
        ESP_LOGI(TAG, "Waiting for AP connection...");
        xEventGroupWaitBits(wifi_event_group, BIT0, false, true, portMAX_DELAY);
        ESP_LOGI(TAG, "Connected to AP");

        int sock = create_multicast_ipv4_socket();

        if (sock < 0) {
            ESP_LOGE(TAG, "Failed to create IPV4 multicast socket");
            vTaskDelay(5 / portTICK_PERIOD_MS);
            continue;
        }

        // set destination multicast addresses
        struct sockaddr_in sdestv4 = {
                .sin_family = PF_INET,
                .sin_port = htons(CONFIG_MULTICAST_PORT),
        };

        inet_aton(CONFIG_MULTICAST_ADDR, &sdestv4.sin_addr.s_addr);

        // Loop waiting for UDP received, and sending UDP packets if we don't see any.
#ifdef CONFIG_MULTICAST_HANDSHAKE
        handshakeDone = 0;
#endif
        for (int state = 1; state > 0;) {
            struct timeval tv = {
                    .tv_sec = 3/*0*/,
                    .tv_usec = 0,
            };

            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(sock, &rfds);

            int selected = select(sock + 1, &rfds, NULL, NULL, &tv);
            char buffer[MULMSG_LEN];

            if (selected < 0) {
                ESP_LOGE(TAG, "Select failed: errno %d", errno);
                state = -1;
                break;
            } else if (selected > 0) {
                if (FD_ISSET(sock, &rfds)) {
                    // Incoming datagram received
                    char raddr_name[32] = {0};
                    struct sockaddr_in6 raddr; // Large enough for both IPV4 or IPV6
                    socklen_t socklen = sizeof(raddr);

                    int len = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *) &raddr, &socklen);

                    if (len < 0) {
                        ESP_LOGE(TAG, "Multicast recvfrom failed: errno %d", errno);
                        state = -1;
                        break;
                    }

                    // Get the sender's address as a string
                    if (raddr.sin6_family == PF_INET) {
                        inet_ntoa_r(((struct sockaddr_in *) &raddr)->sin_addr.s_addr, raddr_name,
                                    sizeof(raddr_name) - 1);
                    }

                    ESP_LOGI(TAG, "Received %d bytes from %s", len, raddr_name);
#ifdef CONFIG_MULTICAST_DEBUG
                    switch (len) {
                    	case 0: break;
                    	case 1:  ESP_LOGI(TAG, "RCV %02x", buffer[0]); break;
                    	default: ESP_LOGI(TAG, "RCV %02x %02x", buffer[0], buffer[1]); break;
                    }
#endif

                    mulmsg *msg = mulmsg_create(buffer, MULMSG_LEN);
                    state = handle_mulmsg(sock, msg, raddr_name);
                    mulmsg_destroy(msg);
                }
            }
#ifdef CONFIG_MULTICAST_HANDSHAKE
			else if (handshakeDone == 0) {
				// Timeout passed with no incoming data, so send "Are You There?"
				mulmsg* msg = mulmsg_create(buffer, MULMSG_LEN);

				if (msg != 0) {
					mulmsg_setSource(msg, 0);
					mulmsg_setAlive(msg, 0);
					mulmsg_setDeviceId(msg, CONFIG_DEVICE_ID);
					state = multicast_send(sock, msg, CONFIG_MULTICAST_ADDR);
					mulmsg_destroy(msg);
				}
			}
#endif
        }

        ESP_LOGE(TAG, "Shutting down socket and restarting...");
        shutdown(sock, 0);
        close(sock);
    }
}

// Handles received multicast message
static int handle_mulmsg(int sock, mulmsg* message, const char* address) {
	if (sock == 0 || message == 0 || address == 0) {
		ESP_LOGE(TAG, "Failed to handle multicast message!");
		return -1;
	}

	int err = 1;	// >0 -> success

	if (mulmsg_getSource(message) != 0) {
		if (mulmsg_getAlive(message) == 0) {
			// send client 'Here I Am!' on server 'Are You There?'
			mulmsg_setSource(message, 0);
			mulmsg_setAlive(message, 1);
			mulmsg_setDeviceId(message, CONFIG_DEVICE_ID);
			err = multicast_send(sock, message, address);
		} else {
#ifdef CONFIG_MULTICAST_HANDSHAKE
			// send client 'Here I Am!' on server 'Here I Am!'
			mulmsg_setSource(message, 0);
			mulmsg_setAlive(message, 1);
			mulmsg_setDeviceId(message, CONFIG_DEVICE_ID);
			err = multicast_send(sock, message, address);

            if (err > 0) {
                handshakeDone = 1;
            }
#endif
        }
    }

    return err;
}

// Sends a multicast message via socket
static int multicast_send(int sock, mulmsg* message, const char* address) {
	const char* data = mulmsg_unwrap(message);

	if (sock == 0 || data == 0 || address == 0) {
		ESP_LOGE(TAG, "Failed to send multicast message!");
		return -1;
	}

	char addrbuf[32] = {0};
	unsigned int deviceId = mulmsg_getDeviceId(message);

	if (deviceId > DEVICEID_MAX) {
		ESP_LOGE(TAG, "Device ID must be in range of 0 <= deviceId <= %d", DEVICEID_MAX);
		return -1;
	}

	struct addrinfo* res;
	struct addrinfo hints = {
		.ai_flags = AI_PASSIVE,
		.ai_socktype = SOCK_DGRAM,
		.ai_family = PF_INET
	};

	int err = getaddrinfo(address, NULL, &hints, &res);

	if (err < 0) {
		ESP_LOGE(TAG, "Failed getaddrinfo() for IP destination address. error: %d", err);
		return err;
	}

	((struct sockaddr_in*) res->ai_addr)->sin_port = htons(CONFIG_MULTICAST_PORT);
	inet_ntoa_r(((struct sockaddr_in*) res->ai_addr)->sin_addr, addrbuf, sizeof(addrbuf) - 1);

	ESP_LOGI(TAG, "Sending to IPV4 address %s:%d...", addrbuf, CONFIG_MULTICAST_PORT);
#ifdef CONFIG_MULTICAST_DEBUG
	ESP_LOGI(TAG, "SND %02x %02x", data[0], data[1]); 	// hint: MULMSG_LEN confirmed
#endif

	err = sendto(sock, data, MULMSG_LEN, 0, res->ai_addr, res->ai_addrlen);
	freeaddrinfo(res);

	if (err < 0) {
		ESP_LOGE(TAG, "Failed sendto() data. errno: %d", errno);
	}

	return err;
}
