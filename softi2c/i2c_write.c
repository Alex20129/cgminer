/*
 * This program is a free software. You can redistribute it and modify it
 * under the terms of the GNU General Public License.
 * Either version 3 of the GNU GPL, or any later version
 * of the GNU GPL, at your discretion, allowed.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. See
 * https://www.gnu.org/licenses/gpl-3.0.html
 * for the full text of the license.
 */

#include <stdio.h>
#include <unistd.h>
#include "i2c.h"

//#define I2C_SDA_PIN 3
//#define I2C_SCL_PIN 2

#define DeviceAddress	0x10
#define Register		0x01

void write_test()
{
	uint8_t data=0x50;
	i2c_open(3, 2);
	i2c_start(I2C_WRITE_MODE);
	i2c_write_single_byte(DeviceAddress);
	i2c_write_single_byte(Register);
	i2c_write_single_byte(data);
	i2c_stop();
	i2c_close();
}
