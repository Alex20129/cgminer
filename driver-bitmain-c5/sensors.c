#include <stdio.h>
#include <math.h>
#include <unistd.h>

#include "sensors.h"
#include "driver-bitmain-c5.h"

#include "../config.h"
#include "../logging.h"

#define CHIP_ID_TO_ADDR(x) (((x) - 1) * 4)
#define I2C_SCAN_LOG_NAME "/tmp/i2c_scan.log"

static FILE *i2c_scan_log;
bool opt_no_scan_i2c_sensors=true;

/*
 * Sensor chip implementation
 */

enum
{
	TMP451_REG_R_LOCAL_T     =0x00,
	TMP451_REG_R_REMOTE_T    =0x01,
	TMP451_REG_R_CONFIG      =0x03,
	TMP451_REG_W_CONFIG      =0x09,
	TMP451_CONFIG_RANGE      =0x04,
	TMP451_REG_RW_OFFSET     =0x11,
	TMP451_REG_R_REMOTE_FRAC =0x10,
	TMP451_REG_R_LOCAL_FRAC  =0x15,
};

void i2c_start_dev(struct i2c_dev *i2cdev)
{
    /* i2cdev->bus is ignored and everything is on bus 0x20 (TEMP_MIDDLE) */
    /* also the parameter name of i2c_dev is `i2cdev`, not `dev` because we
     * need access to the global variable `dev`... */
	set_baud_with_addr(device->baud, 0, i2cdev->chip_addr, i2cdev->chain, 1, 0, (int) TEMP_MIDDLE);
}

static int i2c_tran_ok(struct i2c_dev *dev, unsigned int ret, uint8_t reg)
{
    uint8_t taddr=ret >> 16;
    uint8_t treg=ret >> 8;
    return taddr==dev->i2c_addr && treg==reg;
}

int i2c_read(struct i2c_dev *dev, uint8_t reg, uint8_t *data)
{
    int retry=2;
    while(retry-- > 0) 
    {
		/* read register */
		wait_iic_ok(dev->chip_addr, dev->chain);
		read_temp(dev->i2c_addr, reg, 0, 0, dev->chip_addr, dev->chain);
		usleep(1000);
		/* is the reply ok? */
		unsigned int ret=wait_iic_ok(dev->chip_addr, dev->chain);
		uint8_t ret_data=ret & 0xff;
		if(i2c_tran_ok(dev, ret, reg))
		{
			*data=ret_data;
			return(0);
		}
		/* nope, another round */
		usleep(1000);
    }
    return -1;
}

/*
 * `reg_read` is register addr from which to read data back. Most of the time
 * these two addresses are the same but some have different address to read and
 * to write.
 */
int i2c_write2(struct i2c_dev *dev, uint8_t reg, uint8_t data, uint8_t reg_read)
{
    int retry=2;
	while(retry-- > 0)
	{
	/* write register */
	wait_iic_ok(dev->chip_addr, dev->chain);
	read_temp(dev->i2c_addr, reg, data, 1, dev->chip_addr, dev->chain);
	wait_iic_ok(dev->chip_addr, dev->chain);
	usleep(1000);

	/* read it back */
	wait_iic_ok(dev->chip_addr, dev->chain);
	read_temp(dev->i2c_addr, reg_read, 0, 0, dev->chip_addr, dev->chain);

	/* was reply ok and does it contain matching data? */
	unsigned int ret=wait_iic_ok(dev->chip_addr, dev->chain);
	uint8_t ret_data=ret & 0xff;
	if(i2c_tran_ok(dev, ret, reg_read) && ret_data==data)
	{
            return(0);
	}
	/* nope, another round */
	usleep(1000);
    }
    return -1;
}

int i2c_write(struct i2c_dev *dev, uint8_t reg, uint8_t data)
{
    return i2c_write2(dev, reg, data, reg);
}

static int tmp451_init(struct sensor *sensor)
{
	int ret;

	/* set extended mode */
	ret=i2c_write2(&sensor->dev, TMP451_REG_W_CONFIG, TMP451_CONFIG_RANGE, TMP451_REG_R_CONFIG);
	if(ret<0)
		return ret;

	/* zero offset */
	i2c_write(&sensor->dev, TMP451_REG_RW_OFFSET, 0);
	if(ret<0)
		return ret;

	return(0);
}

static inline void i2c_makedev(struct i2c_dev *dev, int chain, int bus, int chip_addr, int i2c_addr)
{
	dev->chain=chain;
	dev->bus=bus;
	dev->chip_addr=chip_addr;
	dev->i2c_addr=i2c_addr;
}

/* this is for sensor configured in _extended_ mode:
 * temperature 0..255 with offset 0x40 (zero is 64)
 */
static inline float tmp451_make_temp(uint8_t whole, uint8_t fract)
{
    return(int)whole - 0x40 + fract / 256.0;
}

static int tmp451_read_temp(struct sensor *sensor, struct temp *temp)
{
	int ret;
	uint8_t local, remote;

	/* read temperature registers */
	ret=i2c_read(&sensor->dev, TMP451_REG_R_LOCAL_T, &local);
	if(ret<0)
		return ret;

	ret=i2c_read(&sensor->dev, TMP451_REG_R_REMOTE_T, &remote);
	if(ret<0)
		return ret;

	/* put temperatures together */
	temp->local=tmp451_make_temp(local, 0);
	temp->remote=tmp451_make_temp(remote, 0);

	return(0);
}


static int nct218_read_temp(struct sensor *sensor, struct temp *temp)
{
	int ret;
	u_int8_t local;

	/* read local temperature */
	ret=i2c_read(&sensor->dev, 0x00, &local);
	if(ret<0)
	{
		return ret;
	}

	/* put temperatures together */
	temp->local=tmp451_make_temp(local, 0);

	/* fake remote temperature - chip is about 15 degrees hotter than pcb*/
	temp->remote=temp->local + 15;

	return(0);
}

static sensor_ops_t adt7461_chip=
{
	.name="ADT7461",
	.manufacturer_id=0x41,
	.init=tmp451_init,
	.read_temp=tmp451_read_temp
};

static sensor_ops_t tmp451_chip=
{
	.name="TMP451",
	.manufacturer_id=0x55,
	.init=tmp451_init,
	.read_temp=tmp451_read_temp
};

static sensor_ops_t nct218_chip=
{
	.name="NCT218",
	.manufacturer_id=0x1a,
	.init=tmp451_init,
	.read_temp=nct218_read_temp
};

/*
 * Sensor probing and management
 */

/*
 * checks if byte is some sort of 0xff, 0x7f, 0x3f...
 * right-aligned string of zeros
 */
static int is_i2c_garbage_byte(uint8_t b)
{
    return(b & (b + 1))==0;
}

static int probe_sensor_addr(struct sensor *sensor)
{
	int ret;
	uint8_t man_id;

	ret=i2c_read(&sensor->dev, 0xfe, &man_id);
	if(ret<0)
		return ret;

	if(man_id==0x55) {
		/* TMP451 */
		sensor->ops=&tmp451_chip;
		return(0);
	} else if(man_id==0x41) {
		/* ADT7461 */
		sensor->ops=&adt7461_chip;
		return(0);
	} else if(man_id==0x1a) {
		/* NCT218 */
		sensor->ops=&nct218_chip;
		return(0);
	} else {
		/* not found */
		if(!is_i2c_garbage_byte(man_id)) {
			applog(LOG_NOTICE, "there's probably unsupported sensor at chain=%d, i2c_addr=%02x with man_id=%02x",
				sensor->dev.chain, sensor->dev.i2c_addr, man_id);
		}
		return -1;
	}
}

static void dump_i2c_device(FILE *fw, struct i2c_dev *dev)
{
    uint8_t reg=0xfe;
    int i;
    fprintf(fw, "chain %d: found device on chip_addr=%02x, i2c_addr=%02x\n",
            dev->chain, dev->chip_addr, dev->i2c_addr);

    fprintf(fw, "regs from %02x:", reg);
    for(i=0; i<32; i++) 
    {
	uint8_t data;
	int ret;
	ret=i2c_read(dev, reg, &data);
	if(ret<0)
            fprintf(fw, " XX");
	else
            fprintf(fw, " %02x", data);
	reg++;
    }
    fprintf(fw, "\n");
}

static void scan_i2c_sensors(int chain, int bus)
{
    struct i2c_dev dev;
    int chip_id;
    int i2c_addr;
    
    /* open log file */
    if(i2c_scan_log==NULL) 
    {
        i2c_scan_log=fopen(I2C_SCAN_LOG_NAME, "w");
        if(i2c_scan_log==NULL)
        {
                applog(LOG_ERR, "cannot open log file %s", I2C_SCAN_LOG_NAME);
                return;
        }
    }

	/* notify user this will take long */
    applog(LOG_NOTICE, "chain %d: running i2c scan, this may take up to 30 minutes per chain", chain);
    applog(LOG_NOTICE, "(if you don't want to do this, use --no-sensor-scan parameter)");

    /* make device for this sensor */
    for(chip_id=CHAIN_ASIC_NUM; chip_id > 0; chip_id--) 
    {
        fprintf(i2c_scan_log, "chain %d: scanning chip %d\n", chain, chip_id);
		for(i2c_addr=8*2; i2c_addr<124*2; i2c_addr += 2)
        {
            int ret;
            uint8_t man_id;

            /* make device */
            i2c_makedev(&dev, chain, bus, CHIP_ID_TO_ADDR(chip_id), i2c_addr);

			i2c_start_dev(&dev);

            ret=i2c_read(&dev, 0xfe, &man_id);
			if(ret<0)
                continue;
            if(is_i2c_garbage_byte(man_id))
                continue;

            applog(LOG_NOTICE, "chain %d: found device man_id=%02x on chip=%d, i2c_addr=%02x", chain, man_id, chip_id, i2c_addr);
            /* dump it */
            dump_i2c_device(i2c_scan_log, &dev);
        }
        fflush(i2c_scan_log);
    }
}

static int probe_chip_addrs[]=
{
    CHIP_ID_TO_ADDR(62),
};

static int probe_i2c_addrs[]=
{
    0x98,
    0x9a,
    0x9c,
};

int probe_sensors(int chain, int bus, struct sensor *sensors, int max_sensors)
{
    int ret;
    int n=0;
    int i, j;
#if 0
    applog(LOG_NOTICE, "probing sensors: chain=%d max_sensors=%d",
                    chain, max_sensors);
#endif

	for(i=0; i<(sizeof(probe_chip_addrs)/sizeof(probe_chip_addrs[0])); i++)
    {
		for(j=0; j<(sizeof(probe_i2c_addrs)/sizeof(probe_i2c_addrs[0])); j++)
        {
                struct sensor *sensor=&sensors[n];

                /* make device for this sensor */
                i2c_makedev(&sensor->dev, chain, bus, probe_chip_addrs[i], probe_i2c_addrs[j]);

                /* try to start i2c bus for this device */
				i2c_start_dev(&sensor->dev);

                /* try to probe it */
                ret=probe_sensor_addr(sensor);
				if(ret<0)
                {
                    continue;
                }

                applog(LOG_NOTICE, "chain %d: found sensor %s at chip_addr=%02x, i2c_addr=%02x", chain, sensor->ops->name, sensor->dev.chip_addr, sensor->dev.i2c_addr);

                /* ok, this sensor has been probed */
                n++;
                if(n >= max_sensors)
            goto done;
        }
    }
    if(n==0)
    {
    	applog(LOG_WARNING, "chain %d: no sensors found!", chain);
        /* beware the double negative */
        if(!opt_no_scan_i2c_sensors)
        {
            scan_i2c_sensors(chain, bus);
        }
    }
done:
    return n;
}

int sensor_read_temp(struct sensor *sensor, struct temp *temp)
{
    int ret;
	i2c_start_dev(&sensor->dev);
    return sensor->ops->read_temp(sensor, temp);
}
