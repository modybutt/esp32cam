/*
 * rest.h
 *
 *  Created on: 30.11.2019
 *      Author: Gerry
 */

#ifndef MAIN_REST_H_
#define MAIN_REST_H_

#include <esp_http_server.h>

// Initializes the flash driver
void init_flash();
// Initializes the camera driver
void init_camera();
// Initializes the wifi driver
void init_wifi(httpd_handle_t* arg);

#endif /* MAIN_REST_H_ */
