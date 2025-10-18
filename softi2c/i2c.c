/*
 * This program is a free software. You can redistribute it and
 * modify it under the terms of the GNU General Public License.
 * Either version 3 of the GNU GPL, or any later version
 * of the GNU GPL, at your discretion, allowed.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. See
 * https://www.gnu.org/licenses/gpl-3.0.html
 * for the full text of the license.
 */

#include <stdio.h>
#include <gpiod.h>
#include <unistd.h>
#include "i2c.h"

static const char gpio_chip_dev_full_path[]="/dev/gpiochip1";
static const char soft_i2c_name[]="soft-i2c";

static uint8_t starting_shift=0;
static uint8_t set_read_bit=0;

static struct gpiod_chip *gpio_chip=NULL;
static struct gpiod_line *sda_line=NULL, *scl_line=NULL;

uint32_t i2c_open(const uint32_t sda_pin, const uint32_t scl_pin)
{
	if(!gpio_chip)
	{
		gpio_chip=gpiod_chip_open(gpio_chip_dev_full_path);
	}
	if(!gpio_chip)
	{
		fprintf(stderr, "gpiod_chip_open() error\n");
		return(-1);
	}
	if(!sda_line)
	{
		sda_line=gpiod_chip_get_line(gpio_chip, sda_pin);
	}
	if(!sda_line)
	{
		fprintf(stderr, "gpiod_chip_get_line() error\n");
		return(-2);
	}
	if(!scl_line)
	{
		scl_line=gpiod_chip_get_line(gpio_chip, scl_pin);
	}
	if(!scl_line)
	{
		fprintf(stderr, "gpiod_chip_get_line() error\n");
		return(-3);
	}
	if(gpiod_line_request_output(sda_line, soft_i2c_name, 1)<0)
	{
		fprintf(stderr, "gpiod_line_request_output() error\n");
		return(-4);
	}
	if(gpiod_line_request_output(scl_line, soft_i2c_name, 1)<0)
	{
		fprintf(stderr, "gpiod_line_request_output() error\n");
		return(-5);
	}
	return(0);
}

void i2c_close()
{
	if(scl_line)
	{
		gpiod_line_release(scl_line);
		scl_line=NULL;
	}
	if(sda_line)
	{
		gpiod_line_release(sda_line);
		sda_line=NULL;
	}
	if(gpio_chip)
	{
		gpiod_chip_close(gpio_chip);
		gpio_chip=NULL;
	}
}

void i2c_start(uint8_t mode)
{
	starting_shift=1;
	if(mode==I2C_READ_MODE)
	{
		set_read_bit=1;
	}
	usleep(I2C_LOW_LEVEL_TIME_US);
	gpiod_line_set_value(scl_line, 1);
	usleep(I2C_HIGH_LEVEL_TIME_US);
	gpiod_line_set_value(sda_line, 0);
	usleep(I2C_HIGH_LEVEL_TIME_US);
	gpiod_line_set_value(scl_line, 0);
}

void i2c_stop()
{
	usleep(I2C_LOW_LEVEL_TIME_US/2);
	gpiod_line_set_value(sda_line, 0);
	usleep(I2C_LOW_LEVEL_TIME_US/2);
	gpiod_line_set_value(scl_line, 1);
	usleep(I2C_HIGH_LEVEL_TIME_US);
	gpiod_line_set_value(sda_line, 1);
}

void i2c_write_single_byte(uint8_t data)
{
	uint8_t bit;
	if(starting_shift)
	{
		data=data<<1;
		starting_shift=0;
		data+=set_read_bit;
		set_read_bit=0;
	}
	for(bit=0; bit<8; bit++)
	{
		usleep(I2C_LOW_LEVEL_TIME_US/2);
		if(data & 0x80)
		{
			gpiod_line_set_value(sda_line, 1);
		}
		else
		{
			gpiod_line_set_value(sda_line, 0);
		}
		usleep(I2C_LOW_LEVEL_TIME_US/2);
		gpiod_line_set_value(scl_line, 1);
		usleep(I2C_HIGH_LEVEL_TIME_US);
		gpiod_line_set_value(scl_line, 0);
		data=data<<1;
	}
	usleep(I2C_LOW_LEVEL_TIME_US/2);
	gpiod_line_set_value(sda_line, 0);
	usleep(I2C_LOW_LEVEL_TIME_US/2);
	gpiod_line_set_value(scl_line, 1);
	usleep(I2C_HIGH_LEVEL_TIME_US);
	gpiod_line_set_value(scl_line, 0);
}

uint8_t i2c_read_single_byte(uint8_t ack)
{
	uint8_t data=0, bit;
	gpiod_line_release(sda_line);
	gpiod_line_request_input(sda_line, soft_i2c_name);
	for(bit=0; bit<8; bit++)
	{
		usleep(I2C_LOW_LEVEL_TIME_US);
		gpiod_line_set_value(scl_line, 1);
		usleep(I2C_HIGH_LEVEL_TIME_US);
		data=data<<1;
		if(gpiod_line_get_value(sda_line))
		{
			data=data|1;
		}
		gpiod_line_set_value(scl_line, 0);
	}
	usleep(I2C_LOW_LEVEL_TIME_US/2);
	gpiod_line_release(sda_line);
	gpiod_line_request_output(sda_line, soft_i2c_name, !ack);
	usleep(I2C_LOW_LEVEL_TIME_US/2);
	gpiod_line_set_value(scl_line, 1);
	usleep(I2C_HIGH_LEVEL_TIME_US);
	gpiod_line_set_value(scl_line, 0);
	return data;
}
