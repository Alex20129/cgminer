#ifndef _APW17_H_
#define _APW17_H_

#include <pthread.h>
#include <stdint.h>
#include <gpiod.h>

#define APW17_I2C_DEV_ADDR			0x10
#define APW17_I2C_REG_ADDR			0x11

#define PSU_PACKET_PREAMBLE			0xAA55
#define PSU_PACKET_PREAMBLE_LENGTH	2

#define VOLTAGE_STEP_DIV			16.0F

//==========================================
#define APW17_SOMETHING_01				0x01
#define APW17_IDENTIFICATION			0x02
#define APW17_MEASURE_VOLTAGE			0x04
#define APW17_JUMP_FROM_LOADER_TO_APP	0x06 // ?
#define APW17_MEASURE_POWER				0x08
#define APW17_SOMETHING_0A				0x0A
#define APW17_SOMETHING_81				0x81 // enable voltage?
#define APW17_SET_VOLTAGE				0x83
//==========================================

#define WAIT_PERIOD			400000 //us
#define APW17_MIN_VOLTAGE	11.0F
#define APW17_MAX_VOLTAGE	15.4F //v
#define APW17_INIT_VOLTAGE	15.0F //v
#define APW17_VK			71.0F

#define APW17_ENABLE_GPIO_PIN 26

#define APW17_I2C_SDA_PIN 66
#define APW17_I2C_SCL_PIN 65

typedef struct __attribute__((__packed__))
{
	uint16_t preamble;
	uint8_t length;
	uint8_t command;
	uint8_t data[60];
} apw17_proto_packet_t;

typedef struct
{
	int gpio_ok;
	pthread_mutex_t *i2c_mutex;
	apw17_proto_packet_t *pending_packet;
	apw17_proto_packet_t *last_response_packet;
	struct gpiod_chip *gpio_chip;
	struct gpiod_line *power_on_line;
	float voltage_setting;
} apw17_psu_t;

void print_packet(apw17_proto_packet_t *packet);
int apw17_init(pthread_mutex_t *i2c_mutex);

void apw17_power_on();
void apw17_power_off();

void apw17_something_01();
void apw17_identification();
float apw17_measure_voltage();
void apw17_jump_from_loader_to_app();
uint16_t apw17_measure_power();
void apw17_something_0a();
void apw17_something_81();
void apw17_set_voltage(float voltage);
float apw17_get_voltage_setting();

#endif	/* _APW17_H_ */
