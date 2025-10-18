#ifndef SPI_CONTEXT_H
#define SPI_CONTEXT_H

#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#define SPI_DEVICE_TEMPLATE		"/dev/spidev%d.%d"
#define DEFAULT_SPI_BUS			0
#define DEFAULT_SPI_CS_LINE		0
#define DEFAULT_SPI_MODE		SPI_MODE_0
#define DEFAULT_SPI_BITS_PER_WORD	8
#define DEFAULT_SPI_SPEED		1500000
#define DEFAULT_SPI_DELAY_USECS		0

struct spi_config
{
	int bus;
	int cs_line;
	u_int8_t mode;
	u_int32_t speed;
	u_int8_t bits;
	u_int16_t delay;
};

static const struct spi_config default_spi_config=
{
	.bus		= DEFAULT_SPI_BUS,
	.cs_line	= DEFAULT_SPI_CS_LINE,
	.mode		= DEFAULT_SPI_MODE,
	.speed		= DEFAULT_SPI_SPEED,
	.bits		= DEFAULT_SPI_BITS_PER_WORD,
	.delay		= DEFAULT_SPI_DELAY_USECS,
};

struct spi_ctx
{
	int fd;
	struct spi_config config;
};

/* create SPI context with given configuration, returns NULL on failure */
struct spi_ctx *spi_init(struct spi_config *config);
/* close descriptor and free resources */
void spi_exit(struct spi_ctx *ctx);
/* process RX/TX transfer, ensure buffers are long enough */
bool spi_transfer(struct spi_ctx *ctx, u_int8_t *txbuf, u_int8_t *rxbuf, int len);

#endif /* SPI_CONTEXT_H */
