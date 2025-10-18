#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdlib.h>

#include "apw17.h"
#include "i2c.h"

static const char gpio_chip_dev_full_path[]="/dev/gpiochip1";

apw17_psu_t *g_apw17_psu=NULL;

void print_packet(apw17_proto_packet_t *packet)
{
	uint16_t i, p_size=packet->length+PSU_PACKET_PREAMBLE_LENGTH;
	if(p_size>sizeof(apw17_proto_packet_t))
	{
		p_size=sizeof(apw17_proto_packet_t);
	}
	for(i=0; i<p_size-1; i++)
	{
		fprintf(stdout, "%02X ", ((uint8_t *)packet)[i]);
	}
	fprintf(stdout, "%02X\n", ((uint8_t *)packet)[i]);
}

int apw17_init(pthread_mutex_t *i2c_mutex)
{
	if(g_apw17_psu)
	{
		return(0);
	}
	g_apw17_psu=malloc(sizeof(apw17_psu_t));
	if(!g_apw17_psu)
	{
		fprintf(stderr, "g_apw17_psu allocation error\n");
		return(-1);
	}
	g_apw17_psu->gpio_ok=0;
	g_apw17_psu->pending_packet=calloc(1, sizeof(apw17_proto_packet_t));
	if(!g_apw17_psu->pending_packet)
	{
		fprintf(stderr, "psu->pending_packet allocation error\n");
		return(-2);
	}
	g_apw17_psu->last_response_packet=calloc(1, sizeof(apw17_proto_packet_t));
	if(!g_apw17_psu->last_response_packet)
	{
		fprintf(stderr, "psu->last_response_packet allocation error\n");
		return(-3);
	}
	g_apw17_psu->voltage_setting=-1.0F;

	g_apw17_psu->i2c_mutex=i2c_mutex;
	if(!g_apw17_psu->i2c_mutex)
	{
		fprintf(stderr, "psu->i2c_mutex is nullptr\n");
		return(-5);
	}

	int i2c_open_retcode=i2c_open(APW17_I2C_SDA_PIN, APW17_I2C_SCL_PIN);
	if(i2c_open_retcode<0)
	{
		fprintf(stderr, "i2c_open() error %i\n", i2c_open_retcode);
		return(i2c_open_retcode);
	}

	if(!g_apw17_psu->gpio_chip)
	{
		g_apw17_psu->gpio_chip=gpiod_chip_open(gpio_chip_dev_full_path);
	}
	if(!g_apw17_psu->gpio_chip)
	{
		fprintf(stderr, "gpiod_chip_open() error\n");
		return(-6);
	}
	if(!g_apw17_psu->power_on_line)
	{
		g_apw17_psu->power_on_line=gpiod_chip_get_line(g_apw17_psu->gpio_chip, APW17_ENABLE_GPIO_PIN);
	}
	if(!g_apw17_psu->power_on_line)
	{
		fprintf(stderr, "gpiod_chip_get_line(%i) error\n", APW17_ENABLE_GPIO_PIN);
		return(-7);
	}
	if(gpiod_line_request_output(g_apw17_psu->power_on_line, "APW17", 1)<0)
	{
		fprintf(stderr, "gpiod_line_request_output() error\n");
		return(-8);
	}

	g_apw17_psu->gpio_ok=1;

	apw17_power_on();

//	apw17_identification();
//	usleep(WAIT_PERIOD);

	apw17_set_voltage(APW17_INIT_VOLTAGE);
	usleep(WAIT_PERIOD);

//	apw17_something_01();
//	usleep(WAIT_PERIOD);

//	apw17_jump_from_loader_to_app();
//	usleep(WAIT_PERIOD);

//	apw17_something_0a();
//	usleep(WAIT_PERIOD);

//	apw17_something_81();
//	usleep(WAIT_PERIOD);

	return(0);
}

void apw17_power_on()
{
	if(gpiod_line_set_value(g_apw17_psu->power_on_line, 0)<0)
	{
		fprintf(stderr, "gpiod_line_set_value() error\n");
	}
}

void apw17_power_off()
{
	if(gpiod_line_set_value(g_apw17_psu->power_on_line, 1)<0)
	{
		fprintf(stderr, "gpiod_line_set_value() error\n");
	}
}

void set_check_summ(apw17_proto_packet_t *packet)
{
	uint8_t *data_ptr=(uint8_t *)packet;
	uint16_t cs=0;
	uint32_t i;
	if(packet->length<4)
	{
		packet->length=4;
	}
	for(i=2; i<packet->length; i++)
	{
		cs+=data_ptr[i];
	}
	*(uint16_t *)(data_ptr+i)=cs;
	return;
}

void send_packet()
{
	size_t byte, bytes_total=g_apw17_psu->pending_packet->length;

//	fprintf(stdout, ">>>> ");
//	print_packet(g_apw17_psu->pending_packet);

	bytes_total+=PSU_PACKET_PREAMBLE_LENGTH;
	if(bytes_total>sizeof(apw17_proto_packet_t))
	{
		bytes_total=sizeof(apw17_proto_packet_t);
	}
	for(byte=0; byte<bytes_total; byte++)
	{
		i2c_start(I2C_WRITE_MODE);
		i2c_write_single_byte(APW17_I2C_DEV_ADDR);
		i2c_write_single_byte(APW17_I2C_REG_ADDR);
		i2c_write_single_byte(*(byte+(uint8_t *)g_apw17_psu->pending_packet));
		i2c_stop();
	}
	return;
}

int receive_packet()
{
	int bytes_left, bb;
	uint8_t *storage_ptr=(uint8_t *)g_apw17_psu->last_response_packet;
	g_apw17_psu->last_response_packet->preamble=
	g_apw17_psu->last_response_packet->length=
	g_apw17_psu->last_response_packet->command=0;
	i2c_start(I2C_READ_MODE);
	i2c_write_single_byte(APW17_I2C_DEV_ADDR);
	*storage_ptr=i2c_read_single_byte(0);
	i2c_stop();
	storage_ptr++;
	i2c_start(I2C_READ_MODE);
	i2c_write_single_byte(APW17_I2C_DEV_ADDR);
	*storage_ptr=i2c_read_single_byte(0);
	i2c_stop();
	storage_ptr++;

	if(g_apw17_psu->last_response_packet->preamble!=PSU_PACKET_PREAMBLE)
	{
		fprintf(stderr, "apw17_psu invalid preamble %04X\n", g_apw17_psu->last_response_packet->preamble);
		return(-1);
	}

	i2c_start(I2C_READ_MODE);
	i2c_write_single_byte(APW17_I2C_DEV_ADDR);
	bytes_left=i2c_read_single_byte(0);
	i2c_stop();
	storage_ptr++;
	if(bytes_left>3)
	{
		g_apw17_psu->last_response_packet->length=bytes_left;
		bytes_left--;
		for(bb=0; bb<bytes_left; bb++)
		{
			i2c_start(I2C_READ_MODE);
			i2c_write_single_byte(APW17_I2C_DEV_ADDR);
			*storage_ptr=i2c_read_single_byte(0);
			i2c_stop();
			storage_ptr++;
		}
	}
	else
	{
		fprintf(stderr, "apw17_psu incorrect packet size %i\n", bytes_left);
		return(-2);
	}

//	fprintf(stdout, "<<<< ");
//	print_packet(g_apw17_psu->last_response_packet);

	return(0);
}

// 01: something
//   | preamble | len | cmd |  data |    cs
// W |    55 AA |  04 |  01 |       | 05 00
// R |    55 AA |  06 |  01 | 08 1B | 2A 00
void apw17_something_01()
{
	pthread_mutex_lock(g_apw17_psu->i2c_mutex);

	g_apw17_psu->pending_packet->preamble=PSU_PACKET_PREAMBLE;
	g_apw17_psu->pending_packet->length=4;
	g_apw17_psu->pending_packet->command=APW17_SOMETHING_01;
	g_apw17_psu->pending_packet->data[0]=
	g_apw17_psu->pending_packet->data[1]=
	g_apw17_psu->pending_packet->data[2]=
	g_apw17_psu->pending_packet->data[3]=0;
	set_check_summ(g_apw17_psu->pending_packet);

	send_packet();
	usleep(WAIT_PERIOD);
	receive_packet();
	usleep(WAIT_PERIOD);

	pthread_mutex_unlock(g_apw17_psu->i2c_mutex);
	return;
}

// 02: identification
//   | preamble | len | cmd |  data |    cs
// W |    55 AA |  04 |  02 |       | 06 00
// R |    55 AA |  06 |  02 | C1 00 | C9 00
void apw17_identification()
{
	pthread_mutex_lock(g_apw17_psu->i2c_mutex);

	g_apw17_psu->pending_packet->preamble=PSU_PACKET_PREAMBLE;
	g_apw17_psu->pending_packet->length=4;
	g_apw17_psu->pending_packet->command=APW17_IDENTIFICATION;
	g_apw17_psu->pending_packet->data[0]=
	g_apw17_psu->pending_packet->data[1]=
	g_apw17_psu->pending_packet->data[2]=
	g_apw17_psu->pending_packet->data[3]=0;
	set_check_summ(g_apw17_psu->pending_packet);

	send_packet();
	usleep(WAIT_PERIOD);
	receive_packet();
	usleep(WAIT_PERIOD);

	pthread_mutex_unlock(g_apw17_psu->i2c_mutex);
	return;
}

// 04: measure voltage
//   | preamble | len | cmd |  data |    cs
// W |    55 AA |  04 |  04 |       | 08 00
// R |    55 AA |  06 |  04 | AC 0E | C4 00
float apw17_measure_voltage()
{
	pthread_mutex_lock(g_apw17_psu->i2c_mutex);

	g_apw17_psu->pending_packet->preamble=PSU_PACKET_PREAMBLE;
	g_apw17_psu->pending_packet->length=4;
	g_apw17_psu->pending_packet->command=APW17_MEASURE_VOLTAGE;
	g_apw17_psu->pending_packet->data[0]=
	g_apw17_psu->pending_packet->data[1]=
	g_apw17_psu->pending_packet->data[2]=
	g_apw17_psu->pending_packet->data[3]=0;
	set_check_summ(g_apw17_psu->pending_packet);

	send_packet();
	usleep(WAIT_PERIOD);
	receive_packet();
	usleep(WAIT_PERIOD);

	float mv=0.0;
	if(g_apw17_psu->last_response_packet->command==APW17_MEASURE_VOLTAGE)
	{
		mv=g_apw17_psu->last_response_packet->data[0];
		mv/=256.0;
		mv+=g_apw17_psu->last_response_packet->data[1];
	}

	pthread_mutex_unlock(g_apw17_psu->i2c_mutex);
	return(mv);
}

// 06: jump to app
//   | preamble | len | cmd |  data |    cs
// W |    55 AA |  06 |  06 | 00 20 | 2C 00
// R |    55 AA |  25 |  06 | 00 00 FF FF FF FF FF FF FF FF .. .. FF FF | 00 00
void apw17_jump_from_loader_to_app()
{
	pthread_mutex_lock(g_apw17_psu->i2c_mutex);

	g_apw17_psu->pending_packet->preamble=PSU_PACKET_PREAMBLE;
	g_apw17_psu->pending_packet->length=6;
	g_apw17_psu->pending_packet->command=APW17_JUMP_FROM_LOADER_TO_APP;
	g_apw17_psu->pending_packet->data[0]=0x00;
	g_apw17_psu->pending_packet->data[1]=0x20;
	set_check_summ(g_apw17_psu->pending_packet);

	send_packet();
	usleep(WAIT_PERIOD);
	receive_packet();
	usleep(WAIT_PERIOD);

	pthread_mutex_unlock(g_apw17_psu->i2c_mutex);
	return;
}

// 08: measure power
//   | preamble | len | cmd |  data |    cs
// W |    55 AA |  04 |  08 |       | 0C 00
// R |    55 AA |  06 |  08 | 98 0E | B4 00
uint16_t apw17_measure_power()
{
	pthread_mutex_lock(g_apw17_psu->i2c_mutex);

	g_apw17_psu->pending_packet->preamble=PSU_PACKET_PREAMBLE;
	g_apw17_psu->pending_packet->length=4;
	g_apw17_psu->pending_packet->command=APW17_MEASURE_POWER;
	g_apw17_psu->pending_packet->data[0]=
	g_apw17_psu->pending_packet->data[1]=
	g_apw17_psu->pending_packet->data[2]=
	g_apw17_psu->pending_packet->data[3]=0;
	set_check_summ(g_apw17_psu->pending_packet);

	send_packet();
	usleep(WAIT_PERIOD);
	receive_packet();
	usleep(WAIT_PERIOD);

	uint16_t mp=0;
	if(g_apw17_psu->last_response_packet->command==APW17_MEASURE_POWER)
	{
		mp=*(uint16_t *)g_apw17_psu->last_response_packet->data;
	}

	pthread_mutex_unlock(g_apw17_psu->i2c_mutex);
	return(mp);
}

// 0A: something
//   | preamble | len | cmd |                    data |    cs
// W |    55 AA |  04 |  0A |                         | 0E 00
// R |    55 AA |  0C |  0A | 00 00 00 00 00 00 00 00 | 16 00
void apw17_something_0a()
{
	pthread_mutex_lock(g_apw17_psu->i2c_mutex);

	g_apw17_psu->pending_packet->preamble=PSU_PACKET_PREAMBLE;
	g_apw17_psu->pending_packet->length=4;
	g_apw17_psu->pending_packet->command=APW17_SOMETHING_0A;
	g_apw17_psu->pending_packet->data[0]=
	g_apw17_psu->pending_packet->data[1]=
	g_apw17_psu->pending_packet->data[2]=
	g_apw17_psu->pending_packet->data[3]=0;
	set_check_summ(g_apw17_psu->pending_packet);

	send_packet();
	usleep(WAIT_PERIOD);
	receive_packet();
	usleep(WAIT_PERIOD);

	pthread_mutex_unlock(g_apw17_psu->i2c_mutex);
	return;
}

// 81: something
//   | preamble | len | cmd |  data |    cs
// W |    55 AA |  06 |  81 | 00 00 | 87 00
// R |    55 AA |  06 |  81 | 00 00 | 87 00
void apw17_something_81()
{
	pthread_mutex_lock(g_apw17_psu->i2c_mutex);

	g_apw17_psu->pending_packet->preamble=PSU_PACKET_PREAMBLE;
	g_apw17_psu->pending_packet->length=6;
	g_apw17_psu->pending_packet->command=APW17_SOMETHING_81;
	g_apw17_psu->pending_packet->data[0]=
	g_apw17_psu->pending_packet->data[1]=
	g_apw17_psu->pending_packet->data[2]=
	g_apw17_psu->pending_packet->data[3]=0;
	set_check_summ(g_apw17_psu->pending_packet);

	send_packet();
	usleep(WAIT_PERIOD);
	receive_packet();
	usleep(WAIT_PERIOD);

	pthread_mutex_unlock(g_apw17_psu->i2c_mutex);
	return;
}

// 83: set voltage
//   | preamble | len | cmd |  data |    cs
// W |    55 AA |  06 |  83 | 6F 00 | F8 00
// R |    55 AA |  06 |  83 | 6F 00 | F8 00
void apw17_set_voltage(float voltage)
{
	float new_voltage_pic_val;
	if(voltage<APW17_MIN_VOLTAGE)
	{
		voltage=APW17_MIN_VOLTAGE;
	}
	else if(voltage>APW17_MAX_VOLTAGE)
	{
		voltage=APW17_MAX_VOLTAGE;
	}

	voltage*=VOLTAGE_STEP_DIV;
	voltage=(int32_t)voltage;
	voltage/=VOLTAGE_STEP_DIV;

	if(voltage==g_apw17_psu->voltage_setting)
	{
		return;
	}
	g_apw17_psu->voltage_setting=voltage;

	new_voltage_pic_val=APW17_MAX_VOLTAGE;
	new_voltage_pic_val-=voltage;
	new_voltage_pic_val*=APW17_VK;

	if(new_voltage_pic_val>255.0)
	{
		new_voltage_pic_val=255.0;
	}
	else if(new_voltage_pic_val<0.0)
	{
		new_voltage_pic_val=0.0;
	}

	pthread_mutex_lock(g_apw17_psu->i2c_mutex);
	g_apw17_psu->pending_packet->preamble=PSU_PACKET_PREAMBLE;
	g_apw17_psu->pending_packet->length=6;
	g_apw17_psu->pending_packet->command=APW17_SET_VOLTAGE;
	g_apw17_psu->pending_packet->data[0]=new_voltage_pic_val;
	set_check_summ(g_apw17_psu->pending_packet);

	send_packet();
	usleep(WAIT_PERIOD);
	receive_packet();
	usleep(WAIT_PERIOD);

	pthread_mutex_unlock(g_apw17_psu->i2c_mutex);
	return;
}

float apw17_get_voltage_setting()
{
	return(g_apw17_psu->voltage_setting);
}
