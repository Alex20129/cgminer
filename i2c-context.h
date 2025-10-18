#ifndef I2C_CONTEXT_H
#define I2C_CONTEXT_H

#include <stdbool.h>
#include <stdint.h>

/* common i2c context */
struct i2c_ctx
{
	/* destructor */
	void (*exit)(struct i2c_ctx *ctx);
	/* write one byte to given register */
	bool (*write)(struct i2c_ctx *ctx, u_int8_t reg, u_int8_t val);
	/* read one byte from given register */
	bool (*read)(struct i2c_ctx *ctx, u_int8_t reg, u_int8_t *val);
	/* write multiple bytes to addr */
	bool (*write_raw)(struct i2c_ctx *ctx, u_int8_t *buf, u_int32_t len);
	/* read multiple bytes from addr */
	bool (*read_raw)(struct i2c_ctx *ctx, u_int8_t *buf, u_int32_t len);

	/* common data */
	u_int8_t addr;
	int file;
};

/* the default I2C bus on RPi */
#define I2C_BUS		"/dev/i2c-1"

struct i2c_ctx *i2c_slave_open(char *i2c_bus, u_int8_t slave_addr);

#endif /* I2C_CONTEXT_H */
