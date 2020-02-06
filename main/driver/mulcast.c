/*
 * multicast.c
 *
 *  Created on: 05.02.2020
 *      Author: Gerry
 */

#include "mulcast.h"
#include <esp_log.h>
#include <tcpip_adapter.h>
#include <freertos/event_groups.h>
#include <lwip/netdb.h>
#include <esp_err.h>

// Multicast working task handling receiving and sending messages
static void mcast_worker_task(void* args);
// Registers to multicast group to receive messages
static int register_multicast_ipv4_group(int sock);
// Creates an IPV4 multicast socket for receiving and sending messages
static int create_multicast_ipv4_socket(void);
// Handles received multicast message
static int handle_mulmsg(int sock, mulmsg* message, const char* address);
// Sends a multicast message via socket
static int multicast_send(int sock, mulmsg* message, const char* address);

static const char* LOGGER_NAME = "MULCAST";	// logger name tag

// FreeRTOS event group to signal when we are connected & ready to make a request
static EventGroupHandle_t wifi_event_group;

#ifdef CONFIG_MULTICAST_HANDSHAKE
static int handshakeDone = 0;
#endif

int mulcast_setup(void) {
	ESP_LOGI(LOGGER_NAME, "Initializing...");
	wifi_event_group = xEventGroupCreate();
	return (wifi_event_group == NULL ? ESP_FAIL : ESP_OK);
}

void mulcast_start(void) {
	if (xTaskCreate(&mcast_worker_task, "mcast_task", 4096, NULL, 5, NULL) != pdPASS) {
		ESP_LOGW(LOGGER_NAME, "Failed to create multicast task!");
	}
}

void mulcast_enable(unsigned char enabled) {
	ESP_LOGI(LOGGER_NAME, "Enabled: %d", enabled);

	if (enabled == 1) {
		xEventGroupSetBits(wifi_event_group, BIT0);
	} else {
		xEventGroupClearBits(wifi_event_group, BIT0);
	}
}

// Multicast working task handling receiving and sending messages
static void mcast_worker_task(void* args) {
    while (1) {
        // Wait for the ip address to be set
        ESP_LOGI(LOGGER_NAME, "Waiting for AP connection...");
        xEventGroupWaitBits(wifi_event_group, BIT0, false, true, portMAX_DELAY);
        ESP_LOGI(LOGGER_NAME, "Connected to AP");

        int sock = create_multicast_ipv4_socket();

        if (sock < 0) {
            ESP_LOGE(LOGGER_NAME, "Failed to create IPV4 multicast socket");
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
				.tv_sec = 3,
				.tv_usec = 0,
            };

            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(sock, &rfds);

            int selected = select(sock + 1, &rfds, NULL, NULL, &tv);
            char buffer[MULMSG_LEN];

            if (selected < 0) {
                ESP_LOGE(LOGGER_NAME, "Select failed: errno %d", errno);
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
                        ESP_LOGE(LOGGER_NAME, "Multicast recvfrom failed: errno %d", errno);
                        state = -1;
                        break;
                    }

                    // Get the sender's address as a string
                    if (raddr.sin6_family == PF_INET) {
                        inet_ntoa_r(((struct sockaddr_in *) &raddr)->sin_addr.s_addr, raddr_name, sizeof(raddr_name) - 1);
                    }

                    ESP_LOGI(LOGGER_NAME, "Received %d bytes from %s", len, raddr_name);
#ifdef CONFIG_MULTICAST_DEBUG
                    switch (len) {
                    	case 0: break;
                    	case 1:  ESP_LOGI(LOGGER_NAME, "RCV %02x", buffer[0]); break;
                    	default: ESP_LOGI(LOGGER_NAME, "RCV %02x %02x", buffer[0], buffer[1]); break;
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

        ESP_LOGE(LOGGER_NAME, "Shutting down socket and restarting...");
        shutdown(sock, 0);
        close(sock);
    }
}

// Creates an IPV4 multicast socket for receiving and sending messages
static int create_multicast_ipv4_socket(void) {
	struct sockaddr_in saddr = {0};
	int sock = -1;
	int err = 0;

	sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

	if (sock < 0) {
		ESP_LOGE(LOGGER_NAME, "Failed to create socket. Error %d", errno);
		return -1;
	}

	// Bind the socket to any address
	saddr.sin_family = PF_INET;
	saddr.sin_port = htons(CONFIG_MULTICAST_PORT);
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);

	err = bind(sock, (struct sockaddr *) &saddr, sizeof(struct sockaddr_in));

	if (err < 0) {
		ESP_LOGE(LOGGER_NAME, "Failed to bind socket. Error %d", errno);
		close(sock);
		return -1;
	}

	// Set Time To Live to not forward messages
	uint8_t ttl = 0;
	setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(uint8_t));

	if (err < 0) {
		ESP_LOGE(LOGGER_NAME, "Failed to set IP_MULTICAST_TTL. Error %d", errno);
		close(sock);
		return -1;
	}

	// Disable device multicast loopback		(==> DEFAULT)
//    	uint8_t loopback_val = 0;
//    	err = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &loopback_val, sizeof(uint8_t));
//
//    	if (err < 0) {
//    		ESP_LOGE(TAG, "Failed to set IP_MULTICAST_LOOP. Error %d", errno);
//    	    close(sock);
//    		return -1;
//    	}

	// Register to multicast group for receiving messages
	err = register_multicast_ipv4_group(sock);

	if (err < 0) {
		ESP_LOGE(LOGGER_NAME, "Failed to register multicast group. Error %d", errno);
		close(sock);
		return -1;
	}

	return sock;
}

// Registers to multicast group to receive messages
static int register_multicast_ipv4_group(int sock) {
	struct ip_mreq imreq = { 0 };
	int err = 0;

	// Configure multicast address to listen to
	err = inet_aton(CONFIG_MULTICAST_ADDR, &imreq.imr_multiaddr.s_addr);

	if (err != 1) {
		ESP_LOGE(LOGGER_NAME, "Configured IPV4 multicast address '%s' is invalid.", CONFIG_MULTICAST_ADDR);
		return -1;
	}

	// Check for valid address range
	if (!IP_MULTICAST(ntohl(imreq.imr_multiaddr.s_addr))) {
		ESP_LOGW(LOGGER_NAME, "Configured IPV4 multicast address '%s' is not a valid multicast address. This will probably not work.", CONFIG_MULTICAST_ADDR);
	} else {
		ESP_LOGI(LOGGER_NAME, "Configured IPV4 multicast address %s", inet_ntoa(imreq.imr_multiaddr.s_addr));
	}

	// Assign to the multicast group
	err = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imreq, sizeof(struct ip_mreq));

	if (err < 0) {
		ESP_LOGE(LOGGER_NAME, "Failed to set IP_ADD_MEMBERSHIP. Error %d", errno);
		return -1;
	}

	return err;
}

// Handles received multicast message
static int handle_mulmsg(int sock, mulmsg* message, const char* address) {
	if (sock == 0 || message == 0 || address == 0) {
		ESP_LOGE(LOGGER_NAME, "Failed to handle multicast message!");
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
		ESP_LOGE(LOGGER_NAME, "Failed to send multicast message!");
		return -1;
	}

	char addrbuf[32] = {0};
	unsigned int deviceId = mulmsg_getDeviceId(message);

	if (deviceId > DEVICEID_MAX) {
		ESP_LOGE(LOGGER_NAME, "Device ID must be in range of 0 <= deviceId <= %d", DEVICEID_MAX);
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
		ESP_LOGE(LOGGER_NAME, "Failed getaddrinfo() for IP destination address. error: %d", err);
		return err;
	}

	((struct sockaddr_in*) res->ai_addr)->sin_port = htons(CONFIG_MULTICAST_PORT);
	inet_ntoa_r(((struct sockaddr_in*) res->ai_addr)->sin_addr, addrbuf, sizeof(addrbuf) - 1);

	ESP_LOGI(LOGGER_NAME, "Sending to IPV4 address %s:%d...", addrbuf, CONFIG_MULTICAST_PORT);
#ifdef CONFIG_MULTICAST_DEBUG
	ESP_LOGI(LOGGER_NAME, "SND %02x %02x", data[0], data[1]); 	// hint: MULMSG_LEN confirmed
#endif

	err = sendto(sock, data, MULMSG_LEN, 0, res->ai_addr, res->ai_addrlen);
	freeaddrinfo(res);

	if (err < 0) {
		ESP_LOGE(LOGGER_NAME, "Failed sendto() data. errno: %d", errno);
	}

	return err;
}
