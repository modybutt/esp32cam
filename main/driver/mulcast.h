/*
 * multicast.h
 *
 *  Created on: 05.02.2020
 *      Author: Gerry
 */

#ifndef MAIN_DRIVER_MULCAST_H_
#define MAIN_DRIVER_MULCAST_H_

#include "libs/mulmsg.h"

#define CONFIG_DEVICE_ID 	       2
#define CONFIG_MULTICAST_ADDR      "230.0.0.0"
#define CONFIG_MULTICAST_PORT 	   4446
#define CONFIG_MULTICAST_HANDSHAKE
#define CONFIG_MULTICAST_DEBUG

int mulcast_setup(void);
void mulcast_start(void);

#endif /* MAIN_DRIVER_MULCAST_H_ */
