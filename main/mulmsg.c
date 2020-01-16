/*
 * mulmsg.c
 *
 *  Created on: 14.01.2020
 *      Author: Gerry
 */

#include "mulmsg.h"
#include <stdlib.h>
#include <esp_log.h>

struct mulmsg {
	unsigned char hi;
	unsigned char lo;
};

mulmsg* mulmsg_create(char* buffer, int len) {
	mulmsg* message = 0;

	if (len == MULMSG_LEN) {
		message = malloc(MULMSG_LEN);

		if (message != 0) {
			message->hi = buffer[0];
			message->lo = buffer[1];
		}
	}

	return message;
}


void mulmsg_destroy(mulmsg* message) {
	if (message != 0) {
		free(message);
	}
}

const char* mulmsg_unwrap(mulmsg* message) {
	if (message != 0) {
		return (const char*) message;
	}

	return 0;
}

unsigned char mulmsg_getSource(mulmsg* message) {
	if (message != 0) {
		return message->hi & BIT_SOURCE;
	}

	return 0;
}

unsigned char mulmsg_getAlive(mulmsg* message) {
	if (message != 0) {
		return message->hi & BIT_ALIVE;
	}

	return 0;
}

unsigned int mulmsg_getDeviceId(mulmsg* message) {
	int deviceId = 0;

	deviceId |= ((message->hi & BIT_ID_HI) << 8);    // bit 11 - 8
	deviceId |= (message->lo & BIT_ID_LO);           // bit 7 - 0

	return deviceId;
}

void mulmsg_setSource(mulmsg* message, unsigned char source) {
	if (message != 0) {
		if (source == 1) {
			message->hi |= BIT_SOURCE;
		} else {
			message->hi &= ~BIT_SOURCE;
		}
	}
}

void mulmsg_setAlive(mulmsg* message, unsigned char alive) {
	if (message != 0) {
		if (alive == 1) {
			message->hi |= BIT_ALIVE;
		} else {
			message->hi &= ~BIT_ALIVE;
		}
	}
}

void mulmsg_setDeviceId(mulmsg* message, unsigned int deviceId) {
	message->hi |= ((deviceId >> 8) & BIT_ID_HI);    // bit 11 - 8
	message->lo |= (deviceId & BIT_ID_LO);
}
