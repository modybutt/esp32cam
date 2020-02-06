/*
 * mulmsg.h
 *
 *  Created on: 05.02.2020
 *      Author: Gerry
 */

#ifndef MAIN_DRIVER_LIBS_MULMSG_H_
#define MAIN_DRIVER_LIBS_MULMSG_H_

#define MULMSG_LEN   2
#define BIT_SOURCE   0x80
#define BIT_ALIVE    0x40
#define BIT_ID_HI    0x0f   // bitmask
#define BIT_ID_LO    0xff   // bitmask
#define DEVICEID_MAX 0xfff

typedef struct mulmsg mulmsg;

mulmsg* mulmsg_create(char* buffer, int len);
void mulmsg_destroy(mulmsg* message);
const char* mulmsg_unwrap(mulmsg* message);

unsigned char mulmsg_getSource(mulmsg* message);
unsigned char mulmsg_getAlive(mulmsg* message);
unsigned int mulmsg_getDeviceId(mulmsg* message);

void mulmsg_setSource(mulmsg* message, unsigned char source);
void mulmsg_setAlive(mulmsg* message, unsigned char alive);
void mulmsg_setDeviceId(mulmsg* message, unsigned int deviceId);

#endif /* MAIN_DRIVER_LIBS_MULMSG_H_ */
