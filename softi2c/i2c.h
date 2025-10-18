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

#ifndef I2C_H
#define I2C_H

#include <stdint.h>

#define I2C_HIGH_LEVEL_TIME_US	1024
#define I2C_LOW_LEVEL_TIME_US	I2C_HIGH_LEVEL_TIME_US

#define I2C_WRITE_MODE	22
#define I2C_READ_MODE	24

uint32_t i2c_open(const uint32_t sda_pin, const uint32_t scl_pin);
void i2c_close();

void i2c_start(uint8_t mode);
void i2c_stop();

void i2c_write_single_byte(uint8_t data);
uint8_t i2c_read_single_byte(uint8_t ack);

#endif // I2C_H
