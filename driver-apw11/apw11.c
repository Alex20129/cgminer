#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <string.h>
#include "apw11.h"

apw11_psu_t *g_apw11_psu=NULL;

void print_packet(apw11_proto_packet_t *packet)
{
	u_int16_t i, p_size=packet->length+PSU_PACKET_PREAMBLE_LENGTH;
	if(p_size>sizeof(apw11_proto_packet_t))
	{
		p_size=sizeof(apw11_proto_packet_t);
	}
	for(i=0; i<p_size-1; i++)
	{
		fprintf(stdout, "%02X ", ((u_int8_t *)packet)[i]);
	}
	fprintf(stdout, "%02X\n", ((u_int8_t *)packet)[i]);
}

static inline int apw11_select_slave_device()
{
	int retcode=ioctl(g_apw11_psu->i2c_fd, I2C_SLAVE, APW11_I2C_ADDR);
	if(retcode<0)
	{
		fprintf(stderr, "cannot select %02X\n", APW11_I2C_ADDR);
	}
	return(retcode);
}

int apw11_init(int i2c_fd, pthread_mutex_t *i2c_mutex)
{
	if(g_apw11_psu)
	{
		return(0);
	}
	g_apw11_psu=malloc(sizeof(apw11_psu_t));
	if(!g_apw11_psu)
	{
		fprintf(stderr, "g_apw11_psu allocation error\n");
		return(-1);
	}
	g_apw11_psu->i2c_fd=i2c_fd;
	if(i2c_mutex)
	{
		g_apw11_psu->i2c_mutex=i2c_mutex;
	}
	else
	{
		fprintf(stderr, "error i2c_mutex is nullptr\n");
		return(-1);
	}
	g_apw11_psu->pending_packet=calloc(1, sizeof(apw11_proto_packet_t));
	if(!g_apw11_psu->pending_packet)
	{
		fprintf(stderr, "psu->pending_packet allocation error\n");
		return(-1);
	}
	g_apw11_psu->last_response_packet=calloc(1, sizeof(apw11_proto_packet_t));
	if(!g_apw11_psu->last_response_packet)
	{
		fprintf(stderr, "psu->last_response_packet allocation error\n");
		return(-1);
	}
	g_apw11_psu->voltage=MIN_VOLTAGE;
	g_apw11_psu->voltage*=STEP_DENOMINATOR;
	g_apw11_psu->voltage=(int32_t)g_apw11_psu->voltage;
	g_apw11_psu->voltage/=STEP_DENOMINATOR;
	return(0);
}

void set_check_summ(apw11_proto_packet_t *packet)
{
	u_int8_t *data_ptr=(u_int8_t *)packet;
	u_int32_t i, cs_b1=0, cs_b2=0;
	if(packet->length%2)
	{
		packet->length++;
	}
	for(i=2; i<packet->length; i+=2)
	{
		cs_b1+=data_ptr[i];
		cs_b2+=data_ptr[i+1];
	}
	data_ptr[i]=cs_b1&0xFF;
	data_ptr[i+1]=cs_b2&0xFF;
	return;
}

int send_packet()
{
	int retcode=0;
	size_t bytes_total=g_apw11_psu->pending_packet->length;
	bytes_total+=PSU_PACKET_PREAMBLE_LENGTH;
	if(bytes_total>sizeof(apw11_proto_packet_t))
	{
		bytes_total=sizeof(apw11_proto_packet_t);
	}
	if(write(g_apw11_psu->i2c_fd, g_apw11_psu->pending_packet, bytes_total) != bytes_total)
	{
		retcode=-1;
	}
	if(retcode<0)
	{
		fprintf(stderr, "apw11 write error\n");
	}
	return(retcode);
}

int receive_packet()
{
	int retcode=0;
	u_int8_t *storage_ptr=(u_int8_t *)g_apw11_psu->last_response_packet;
	size_t bytes_total;

	g_apw11_psu->last_response_packet->preamble=
	g_apw11_psu->last_response_packet->length=
	g_apw11_psu->last_response_packet->command=0;

	read(g_apw11_psu->i2c_fd, storage_ptr, PSU_PACKET_PREAMBLE_LENGTH);
	if(g_apw11_psu->last_response_packet->preamble!=PSU_PACKET_PREAMBLE_OUTPUT)
	{
		fprintf(stderr, "apw11_psu invalid preamble\n");
		retcode=-1;
	}

	if(!retcode)
	{
		read(g_apw11_psu->i2c_fd, &g_apw11_psu->last_response_packet->length, 1);
		if(g_apw11_psu->last_response_packet->length>1)
		{
			bytes_total=g_apw11_psu->last_response_packet->length-1;
			if(read(g_apw11_psu->i2c_fd, storage_ptr, bytes_total)!=bytes_total)
			{
				retcode=-1;
			}
		}
		else
		{
			fprintf(stderr, "Looks like no data can be received. =/\n");
			retcode=-1;
		}
	}

	print_packet(g_apw11_psu->last_response_packet);

	return(retcode);
}

// 01: something
//   | preamble | len | cmd |  data |    cs
// W |    55 AA |  04 |  01 |       | 04 02
// R |    55 AA |  06 |  01 | 05 00 | 0B 01
void apw11_something_01()
{
	pthread_mutex_lock(g_apw11_psu->i2c_mutex);

	g_apw11_psu->pending_packet->preamble=PSU_PACKET_PREAMBLE_INPUT;
	g_apw11_psu->pending_packet->length=4;
	g_apw11_psu->pending_packet->command=APW11_SOMETHING_01;
	g_apw11_psu->pending_packet->data[0]=
	g_apw11_psu->pending_packet->data[1]=
	g_apw11_psu->pending_packet->data[2]=
	g_apw11_psu->pending_packet->data[3]=0;
	set_check_summ(g_apw11_psu->pending_packet);

	if(apw11_select_slave_device()<0)
	{
		pthread_mutex_unlock(g_apw11_psu->i2c_mutex);
		return;
	}
	if(send_packet()<0)
	{
		pthread_mutex_unlock(g_apw11_psu->i2c_mutex);
		return;
	}
	usleep(WAIT_PERIOD);
	receive_packet();

	pthread_mutex_unlock(g_apw11_psu->i2c_mutex);
	return;
}

// 02: identification
//   | preamble | len | cmd |  data |    cs
// W |    55 AA |  04 |  02 |       | 04 02
// R |    55 AA |  06 |  02 | 64 00 | 6A 02
void apw11_identification()
{
	pthread_mutex_lock(g_apw11_psu->i2c_mutex);

	g_apw11_psu->pending_packet->preamble=PSU_PACKET_PREAMBLE_INPUT;
	g_apw11_psu->pending_packet->length=4;
	g_apw11_psu->pending_packet->command=APW11_IDENTIFICATION;
	g_apw11_psu->pending_packet->data[0]=
	g_apw11_psu->pending_packet->data[1]=
	g_apw11_psu->pending_packet->data[2]=
	g_apw11_psu->pending_packet->data[3]=0;
	set_check_summ(g_apw11_psu->pending_packet);

	if(apw11_select_slave_device()<0)
	{
		pthread_mutex_unlock(g_apw11_psu->i2c_mutex);
		return;
	}
	if(send_packet()<0)
	{
		pthread_mutex_unlock(g_apw11_psu->i2c_mutex);
		return;
	}
	usleep(WAIT_PERIOD);
	receive_packet();

	pthread_mutex_unlock(g_apw11_psu->i2c_mutex);
	return;
}

// 06: jump to app
//   | preamble | len | cmd |        data |    cs
// W |    55 AA |  08 |  06 | 00 00 20 00 | 28 06
// R |    55 AA |  26 |  06 | 00 00 FF FF FF FF FF FF FF FF .. .. .. FF FF | 00 00
void apw11_jump_from_loader_to_app()
{
	pthread_mutex_lock(g_apw11_psu->i2c_mutex);

	g_apw11_psu->pending_packet->preamble=PSU_PACKET_PREAMBLE_INPUT;
	g_apw11_psu->pending_packet->length=8;
	g_apw11_psu->pending_packet->command=APW11_JUMP_FROM_LOADER_TO_APP;
	g_apw11_psu->pending_packet->data[0]=0x00;
	g_apw11_psu->pending_packet->data[1]=0x00;
	g_apw11_psu->pending_packet->data[2]=0x20;
	g_apw11_psu->pending_packet->data[3]=0x00;
	set_check_summ(g_apw11_psu->pending_packet);

	if(apw11_select_slave_device()<0)
	{
//		pthread_mutex_unlock(g_apw11_psu->i2c_mutex);
//		return;
	}
	if(send_packet()<0)
	{
//		pthread_mutex_unlock(g_apw11_psu->i2c_mutex);
//		return;
	}
//	usleep(WAIT_PERIOD);
//	receive_packet(psu);

	pthread_mutex_unlock(g_apw11_psu->i2c_mutex);
	return;
}

// 0E: something
//   | preamble | len | cmd |                    data |    cs
// W |    55 AA |  04 |  0E |                         | 04 0E
// R |    55 AA |  0C |  02 | 00 00 00 00 00 05 00 00 | 0C 13
void apw11_something_0e()
{
	pthread_mutex_lock(g_apw11_psu->i2c_mutex);

	g_apw11_psu->pending_packet->preamble=PSU_PACKET_PREAMBLE_INPUT;
	g_apw11_psu->pending_packet->length=4;
	g_apw11_psu->pending_packet->command=APW11_SOMETHING_0E;
	g_apw11_psu->pending_packet->data[0]=
	g_apw11_psu->pending_packet->data[1]=
	g_apw11_psu->pending_packet->data[2]=
	g_apw11_psu->pending_packet->data[3]=0;
	set_check_summ(g_apw11_psu->pending_packet);

	if(apw11_select_slave_device()<0)
	{
		pthread_mutex_unlock(g_apw11_psu->i2c_mutex);
		return;
	}
	if(send_packet()<0)
	{
		pthread_mutex_unlock(g_apw11_psu->i2c_mutex);
		return;
	}
	usleep(WAIT_PERIOD);
	receive_packet();

	pthread_mutex_unlock(g_apw11_psu->i2c_mutex);
	return;
}

// 83: set voltage
//   | preamble | len | cmd |        data |    cs
// W |    55 AA |  08 |  83 | 14 AE 9F 41 | BB 72
// R |    55 AA |  08 |  83 | 14 AE 9F 41 | BB 72
void apw11_set_voltage_instantly(float new_voltage)
{
	if(new_voltage<MIN_VOLTAGE)
	{
		new_voltage=MIN_VOLTAGE;
	}
	else if(new_voltage>MAX_VOLTAGE)
	{
		new_voltage=MAX_VOLTAGE;
	}
	new_voltage*=STEP_DENOMINATOR;
	new_voltage=(int32_t)new_voltage;
	new_voltage/=STEP_DENOMINATOR;
	if(new_voltage==g_apw11_psu->voltage)
	{
		return;
	}
	g_apw11_psu->voltage=new_voltage;
	pthread_mutex_lock(g_apw11_psu->i2c_mutex);
	g_apw11_psu->pending_packet->preamble=PSU_PACKET_PREAMBLE_INPUT;
	g_apw11_psu->pending_packet->length=8;
	g_apw11_psu->pending_packet->command=APW11_SET_VOLTAGE;
	*(float *)g_apw11_psu->pending_packet->data=new_voltage-0.25F;
	set_check_summ(g_apw11_psu->pending_packet);
	if(apw11_select_slave_device()<0)
	{
		pthread_mutex_unlock(g_apw11_psu->i2c_mutex);
		return;
	}
	if(send_packet()<0)
	{
		pthread_mutex_unlock(g_apw11_psu->i2c_mutex);
		return;
	}
//	usleep(WAIT_PERIOD);
//	receive_packet();
	pthread_mutex_unlock(g_apw11_psu->i2c_mutex);
	return;
}

void apw11_set_voltage_gradually(float new_voltage)
{
	float v_step=1.0/STEP_DENOMINATOR;
	if(new_voltage<MIN_VOLTAGE)
	{
		new_voltage=MIN_VOLTAGE;
	}
	else if(new_voltage>MAX_VOLTAGE)
	{
		new_voltage=MAX_VOLTAGE;
	}
	new_voltage*=STEP_DENOMINATOR;
	new_voltage=(int32_t)new_voltage;
	new_voltage/=STEP_DENOMINATOR;
	if(new_voltage==g_apw11_psu->voltage)
	{
		return;
	}
	while(new_voltage>g_apw11_psu->voltage)
	{
		apw11_set_voltage_instantly(g_apw11_psu->voltage+v_step);
		usleep(WAIT_PERIOD);
	}
	while(new_voltage<g_apw11_psu->voltage)
	{
		apw11_set_voltage_instantly(g_apw11_psu->voltage-v_step);
		usleep(WAIT_PERIOD);
	}
	return;
}

float apw11_get_voltage()
{
	return(g_apw11_psu->voltage);
}

