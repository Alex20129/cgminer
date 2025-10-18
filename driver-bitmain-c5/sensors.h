#ifndef SENSORS_H
#define SENSORS_H

#include <sys/param.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * Fake I2C driver
 */

extern bool opt_no_scan_i2c_sensors;

struct i2c_dev
{
	int chain, bus, chip_addr;
	int i2c_addr;
};

/*
 * Sensors and temperatures
 */

struct temp
{
	float local, remote;
};

struct sensor;

typedef struct
{
	const char *name;
	u_int8_t manufacturer_id;
	int (*init)(struct sensor *sensor);
	int (*read_temp)(struct sensor *sensor, struct temp *temp);
} sensor_ops_t;

struct sensor
{
	struct i2c_dev dev;
	sensor_ops_t *ops;
};

int probe_sensors(int chain, int bus, struct sensor *sensors, int max_sensors);

#define sensor_init(sens) (sens)->ops->init(sens)
int sensor_read_temp(struct sensor *sensor, struct temp *temp);

#define ZERO_TEMP {.local=0, .remote=0}

static inline void max_temp(struct temp *tmax, struct temp *temp)
{
    tmax->local=MAX(tmax->local, temp->local);
    tmax->remote=MIN(tmax->remote, temp->remote);
}

#endif //SENSORS_H

