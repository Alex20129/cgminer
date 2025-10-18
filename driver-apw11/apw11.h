#ifndef _APW11_H_
#define _APW11_H_

#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>

#define APW11_I2C_ADDR				0x10

#define PSU_PACKET_PREAMBLE_INPUT	0xAA55
#define PSU_PACKET_PREAMBLE_OUTPUT	0x55AA
#define PSU_PACKET_PREAMBLE_LENGTH	2

//==========================================
#define APW11_SOMETHING_01				0x01 //?
#define APW11_IDENTIFICATION			0x02 //?
#define APW11_JUMP_FROM_LOADER_TO_APP	0x06
#define APW11_SOMETHING_0E				0x0E //?
#define APW11_ENABLE_VOLTAGE			0x15 //?
#define APW11_SET_VOLTAGE				0x83
//==========================================

#define WAIT_PERIOD				550000 //us
#define STEP_DENOMINATOR		8.0F
#define MIN_VOLTAGE				15.0F
#define MAX_VOLTAGE				24.0F

typedef struct __attribute__((__packed__))
{
	uint16_t preamble;
	uint8_t length;
	uint8_t command;
	uint8_t data[60];
} apw11_proto_packet_t;

typedef struct
{
	int i2c_fd;
	pthread_mutex_t *i2c_mutex;
	apw11_proto_packet_t *pending_packet;
	apw11_proto_packet_t *last_response_packet;
	float voltage;
} apw11_psu_t;

void print_packet(apw11_proto_packet_t *packet);
int apw11_init(int i2c_fd, pthread_mutex_t *i2c_mutex);

void apw11_something_01();
void apw11_identification();
void apw11_jump_from_loader_to_app();
void apw11_something_0e();
void apw11_set_voltage_instantly(float new_voltage);
void apw11_set_voltage_gradually(float new_voltage);

float apw11_get_voltage();

#endif	/* _APW11_H_ */
