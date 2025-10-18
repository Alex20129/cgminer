#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/select.h>
#include <dirent.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

#include "driver-bitmain-l3.h"
#include "pic.h"
#include "../crc.h"
#include "../cgminer.h"

struct thr_info *check_miner_status_thr=NULL;
struct thr_info *check_fan_thr=NULL;
struct thr_info *read_temp_thr=NULL;
struct thr_info *read_hash_rate_thr=NULL;
struct thr_info *pic_heart_beat_thr=NULL;
struct thr_info *autotuner_thr=NULL;

const int plug[CHAINS_MAX]={51, 48, 47, 44};
const int tty[CHAINS_MAX]={1, 2, 4, 5};
const int g_gpio_data[CHAINS_MAX]={5, 4, 27, 22};

const uint8_t temp_sensor_chip_addr=0x0C;
uint8_t g_check_temp_sensor_values=3;

int opt_target_temp=65;
int opt_target_freq=450;
int opt_freq[CHAINS_MAX]={DEFAULT_FREQUENCY, DEFAULT_FREQUENCY, DEFAULT_FREQUENCY, DEFAULT_FREQUENCY};
int opt_volt[CHAINS_MAX]={DEFAULT_VOLTAGE, DEFAULT_VOLTAGE, DEFAULT_VOLTAGE, DEFAULT_VOLTAGE};
int opt_core_temp=2;
bool opt_enable_autotuner=false;

pthread_mutex_t i2c_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t work_queue_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t nonce_fifo_mutex=PTHREAD_MUTEX_INITIALIZER;

bool update_asic_num=false;

device_t	*device;
board_parameters_t	*boards;
autotuner_t			*autotuner;

struct nonce_buf	g_nonce_fifo;
struct timeval		g_tv_send_job={0, 0};
struct work			*g_work_queue[WORK_QUEUE_SIZE];

speed_t tiospeed_t(int baud)
{
	switch(baud)
	{
		case 9600:
			return B9600;
		case 19200:
			return B19200;
		case 38400:
			return B38400;
		case 57600:
			return B57600;
		case 115200:
			return B115200;
		case 230400:
			return B230400;
		case 460800:
			return B460800;
		case 576000:
			return B576000;
		case 1152000:
			return B1152000;
		case 1500000:
			return B1500000;
		case 2000000:
			return B2000000;
		case 2500000:
			return B2500000;
		case 3000000:
			return B3000000;
		default:
			return B0;
	}
}

const pll_freq_t freq_pll_1485[] =
{
	{266, 0x00400231}, // 266.67 MHz 115 W
	{270, 0x00410231}, // 270.83 MHz 117 W
	{275, 0x00420231}, // 275.00 MHz 119 W
	{279, 0x00430231}, // 279.17 MHz 120 W
	{283, 0x00440231}, // 283.33 MHz 122 W
	{287, 0x00450231}, // 287.50 MHz 124 W
	{291, 0x00460231}, // 291.67 MHz 125 W
	{295, 0x00470231}, // 295.83 MHz 127 W
	{300, 0x00480231}, // 300.00 MHz 129 W
	{304, 0x00490231}, // 304.17 MHz 131 W

	{308, 0x004A0231}, // 308.33 MHz 132 W
	{312, 0x004B0231}, // 312.50 MHz 134 W
	{316, 0x004C0231}, // 316.67 MHz 136 W
	{320, 0x004D0231}, // 320.83 MHz 137 W
	{325, 0x004E0231}, // 325.00 MHz 139 W
	{329, 0x004F0231}, // 329.17 MHz 141 W
	{333, 0x00500231}, // 333.33 MHz 142 W
	{337, 0x00510231}, // 337.50 MHz 144 W
	{341, 0x00520231}, // 341.67 MHz 146 W
	{345, 0x00530231}, // 345.83 MHz 148 W

	{350, 0x00540231}, // 350.00 MHz 149 W
	{354, 0x00550231}, // 354.17 MHz 151 W
	{358, 0x00560231}, // 358.33 MHz 153 W
	{362, 0x00570231}, // 362.50 MHz 154 W
	{366, 0x00580231}, // 366.67 MHz 156 W
	{370, 0x00590231}, // 370.83 MHz 158 W
	{375, 0x005A0231}, // 375.00 MHz 159 W
	{379, 0x005B0231}, // 379.17 MHz 161 W
	{383, 0x005C0231}, // 383.33 MHz 163 W
	{387, 0x005D0231}, // 387.50 MHz 164 W

	{391, 0x005E0231}, // 391.67 MHz 166 W
	{395, 0x005F0231}, // 395.83 MHz 168 W
	{400, 0x00600231}, // 400.00 MHz 170 W
	{404, 0x00610231}, // 404.17 MHz 171 W
	{408, 0x00620231}, // 408.33 MHz 173 W
	{412, 0x00630231}, // 412.50 MHz 175 W
	{416, 0x00640231}, // 416.67 MHz 176 W
	{420, 0x00650231}, // 420.83 MHz 178 W
	{424, 0x00660231}, // 425.00 MHz 180 W
	{429, 0x00670231}, // 429.17 MHz 181 W
	{433, 0x00680231}, // 433.33 MHz 183 W
	{437, 0x00690231}, // 437.50 MHz 185 W
	{441, 0x006A0231}, // 441.67 MHz 187 W
	{445, 0x006B0231}, // 445.83 MHz 188 W
	{449, 0x006C0231}, // 450.00 MHz 190 W
	{454, 0x006D0231}, // 454.17 MHz 192 W
	{458, 0x006E0231}, // 458.33 MHz 193 W
	{462, 0x006F0231}, // 462.50 MHz 195 W
	{466, 0x00700231}, // 466.67 MHz 197 W
	{470, 0x00710231}, // 470.83 MHz 198 W
	{474, 0x00720231}, // 475.00 MHz 200 W
	{479, 0x00730231}, // 479.17 MHz 202 W
	{483, 0x00740231}, // 483.33 MHz 203 W
	{487, 0x00750231}, // 487.50 MHz 205 W
	{491, 0x00760231}, // 491.67 MHz 207 W
	{495, 0x00770231}, // 495.83 MHz 209 W
	{499, 0x00780231}, // 500.00 MHz 210 W
	{504, 0x00790231}, // 504.17 MHz 212 W
	{508, 0x007A0231}, // 508.33 MHz 214 W
	{512, 0x007B0231}, // 512.50 MHz 215 W
	{516, 0x007C0231}, // 516.67 MHz 217 W
	{520, 0x007D0231}, // 520.83 MHz 219 W
	{525, 0x007E0231}, // 525.00 MHz 220 W
	{529, 0x007F0231}, // 529.17 MHz 222 W
	{533, 0x00800231}, // 533.33 MHz 224 W
	{537, 0x00810231}, // 537.50 MHz 226 W
	{541, 0x00820231}, // 541.67 MHz 227 W
	{545, 0x00830231}, // 545.83 MHz 229 W
	{550, 0x00840231}, // 550.00 MHz 231 W
	{554, 0x00850231}, // 554.17 MHz 232 W
	{558, 0x00860231}, // 558.33 MHz 234 W
	{562, 0x00870231}, // 562.50 MHz 236 W
	{566, 0x00880231}, // 566.67 MHz 237 W
	{570, 0x00890231}, // 570.83 MHz 239 W
	{575, 0x008A0231}, // 575.00 MHz 241 W
	{579, 0x008B0231}, // 579.17 MHz 242 W
	{583, 0x008C0231}, // 583.33 MHz 244 W
	{587, 0x008D0231}, // 587.50 MHz 246 W
	{591, 0x008E0231}, // 591.67 MHz 248 W
	{595, 0x008F0231}, // 595.83 MHz 249 W
	{600, 0x00900231}, // 600.00 MHz 251 W
	{604, 0x00910231}, // 604.17 MHz 253 W
	{608, 0x00920231}, // 608.33 MHz 254 W
	{612, 0x00930231}, // 612.50 MHz 256 W
	{616, 0x00940231}, // 616.67 MHz 258 W
	{620, 0x00950231}, // 620.83 MHz 259 W
	{625, 0x00960231}, // 625.00 MHz 261 W
	{629, 0x00970231}, // 629.17 MHz 263 W
	{633, 0x00980231}, // 633.33 MHz 265 W
	{637, 0x00990231}, // 637.50 MHz 266 W
	{641, 0x009A0231}, // 641.67 MHz 268 W
	{645, 0x009B0231}, // 645.83 MHz 270 W
	{650, 0x009C0231} // 650.00 MHz 271 W
};

static void reset_asics_one_chain(uint8_t chain_id)
{
	char rstBuf[48];
	sprintf(rstBuf, "echo 0 > " GPIO_DEVICE_TEMPLATE, g_gpio_data[chain_id]);
	system(rstBuf);
	usleep(500000);
	sprintf(rstBuf, "echo 1 > " GPIO_DEVICE_TEMPLATE, g_gpio_data[chain_id]);
	system(rstBuf);
}

void clear_nonce_buf()
{
// really need to do that?
//	uint8_t chain;
//	for(chain=0; chain<CHAINS_MAX; chain++)
//	{
//		tcflush(boards[chain].dev_fd, TCIOFLUSH);
//	}
	g_nonce_fifo.p_wr=0;
	g_nonce_fifo.p_rd=0;
	g_nonce_fifo.nonce_num=0;
}

static int64_t bitmain_L3_scanhash(struct thr_info *thr)
{
    struct timeval current;
	uint32_t nonce;
	uint8_t nonce_bin[4], work_id, asic_id, chain_id, nofull, submit_nonce_ok;
	int64_t hashes=0;

    struct work *work=NULL;
    cgtime(&current);

	pthread_mutex_lock(&nonce_fifo_mutex);
	while(g_nonce_fifo.nonce_num)
    {
		/*
		crc_check=crc5((uint8_t *)&(g_nonce_fifo.nonce_buffer[g_nonce_fifo.p_rd]), 7*8-5);
		if(crc_check!=(g_nonce_fifo.nonce_buffer[g_nonce_fifo.p_rd].crc5 & 0x1f))
        {
				applog(LOG_ERR, "%s: crc5 error, should be %02x, but check as %02x", __FUNCTION__, g_nonce_fifo.nonce_buffer[g_nonce_fifo.p_rd].crc5 & 0x1f,crc_check);
				applog(LOG_ERR, "%s: get nonce %02x%02x%02x%02x wc %02x diff %02x crc5 %02x chainid %u", __FUNCTION__, g_nonce_fifo.nonce_buffer[g_nonce_fifo.p_rd].nonce[0], \
					   g_nonce_fifo.nonce_buffer[g_nonce_fifo.p_rd].nonce[1], g_nonce_fifo.nonce_buffer[g_nonce_fifo.p_rd].nonce[2], \
					   g_nonce_fifo.nonce_buffer[g_nonce_fifo.p_rd].nonce[3], g_nonce_fifo.nonce_buffer[g_nonce_fifo.p_rd].diff, \
					   g_nonce_fifo.nonce_buffer[g_nonce_fifo.p_rd].wc, g_nonce_fifo.nonce_buffer[g_nonce_fifo.p_rd].crc5,   \
					   g_nonce_fifo.nonce_buffer[g_nonce_fifo.p_rd].chainid);
			//if signature enabled, check SIG_INFO register
        }
		*/
		memcpy(nonce_bin, g_nonce_fifo.nonce_data[g_nonce_fifo.p_rd].nonce, 4);
		work_id=g_nonce_fifo.nonce_data[g_nonce_fifo.p_rd].wc & 0x7F;
		chain_id=g_nonce_fifo.nonce_data[g_nonce_fifo.p_rd].chainid;

		memcpy(&nonce, nonce_bin, 4);
        nonce=htobe32(nonce);
        pthread_mutex_lock(&work_queue_mutex);
		work=g_work_queue[work_id];
        if(work)
        {
			asic_id=(((nonce >> 20) & 0xFF) / device->addrInterval);
			nofull=0;
			submit_nonce_ok=submit_nonce_1(thr, work, nonce, &nofull);
			if(submit_nonce_ok)
			{
				submit_nonce_2(work);
			}
			pthread_mutex_unlock(&work_queue_mutex);
			if(submit_nonce_ok || nofull)
			{
				hashes+= 0x01UL << DEVICE_DIFF;
				if(chain_id<CHAINS_MAX)
				{
					if(asic_id<ASICS_ON_CHAIN_MAX)
					{
						device->valid_nonce_count++;
						boards[chain_id].nonce_count++;
						boards[chain_id].asic_nonce_count[asic_id]++;
					}
				}
            }
			else
			{
				inc_hw_errors(thr);
				if(chain_id<CHAINS_MAX)
				{
					if(asic_id<ASICS_ON_CHAIN_MAX)
					{
						device->hwe_count++;
						boards[chain_id].hwe_count++;
						boards[chain_id].asic_hwe_count[asic_id]++;
					}
				}
			}
        }
        else
        {
			pthread_mutex_unlock(&work_queue_mutex);
			applog(LOG_ERR, "%s: work_id=%u not found", __FUNCTION__, work_id);
		}

		if(++g_nonce_fifo.p_rd >= MAX_NONCE_NUMBER_IN_FIFO)
		{
			g_nonce_fifo.p_rd=0;
		}
		g_nonce_fifo.nonce_num--;
	}
	pthread_mutex_unlock(&nonce_fifo_mutex);

	hashes*=0x0000FFFFULL;

	return hashes;
}

void set_led(bool stop)
{
	static bool blink=true;
	char cmd[48];
	blink=!blink;
    if(stop)
    {
		sprintf(cmd, "echo %i > " GPIO_DEVICE_TEMPLATE, 0, GREEN_LED_PIN);
        system(cmd);
		sprintf(cmd, "echo %i > " GPIO_DEVICE_TEMPLATE, blink, RED_LED_PIN);
        system(cmd);
    }
    else
    {
		sprintf(cmd, "echo %i > " GPIO_DEVICE_TEMPLATE, 0, RED_LED_PIN);
        system(cmd);
		sprintf(cmd, "echo %i > " GPIO_DEVICE_TEMPLATE, blink, GREEN_LED_PIN);
        system(cmd);
    }
}

void set_beep(bool flag)
{
	char cmd[48];
	sprintf(cmd, "echo %i > " GPIO_DEVICE_TEMPLATE, flag?1:0, BEEPER_PIN);
    system(cmd);
}

static unsigned int getNum(const char *buffer)
{
	char *pos=strstr(buffer, ":");

	while(*(++pos)==' ');
	char *startPos=pos;

	while(*(++pos)!=' ');
    *pos='\0';

    return(atoi(startPos));

}

void *check_fan_func()
{
	uint32_t fan0SpeedHist=0;
	uint32_t fan1SpeedHist=0;
	uint32_t fan0SpeedCur;
	uint32_t fan1SpeedCur;
	uint32_t fan0Speed;
	uint32_t fan1Speed;
	uint32_t fan0_exist=0;
	uint32_t fan1_exist=0;
	char buffer[256], *pos;
    FILE* fanpfd=fopen(PROCFILENAME, "r");
	while(1)
    {
        fseek(fanpfd, 0, SEEK_SET);
		while(fgets(buffer, 256, fanpfd))
        {
			if((pos=strstr(buffer, FAN0)) && (strstr(buffer, "gpiolib")!=0 ) )
            {
                fan0SpeedCur=getNum(buffer);
				if(fan0SpeedHist>fan0SpeedCur)
                {
					fan0Speed=(0xffffffff-fan0SpeedHist+fan0SpeedCur);
                }
                else
                {
					fan0Speed=(fan0SpeedCur-fan0SpeedHist);
                }
				fan0Speed=fan0Speed * 60 / 2;
				if(fan0Speed)
                {
                    fan0_exist=1;
					if(fan0Speed>6600)
					{
						fan0Speed=6600;
					}
                }
                else
                {
                    fan0_exist=0;
                }
				device->fan_speed_value[0]=fan0Speed;
                fan0SpeedHist=fan0SpeedCur;
            }
			else if((pos=strstr(buffer, FAN1)) && (strstr(buffer, "gpiolib")!=0 ))
            {
                fan1SpeedCur=getNum(buffer);
				if(fan1SpeedHist>fan1SpeedCur)
                {
					fan1Speed=(0xffffffff-fan1SpeedHist+fan1SpeedCur);
                }
                else
                {
					fan1Speed=( fan1SpeedCur-fan1SpeedHist );
                }
                fan1SpeedHist=fan1SpeedCur;
				fan1Speed=fan1Speed* 60 / 2;
				if(fan1Speed )
                {
                    fan1_exist=1;
					if(fan1Speed>6600)
					{
						fan1Speed=6600;
					}
                }
                else
                {
                    fan1_exist=0;
                }
				device->fan_speed_value[1]=fan1Speed;
            }
        }
		device->fan_num=fan1_exist+fan0_exist;
		sleep(FAN_CHECK_INTERVAL);
    }
}

void check_asic_reg_all_chains(unsigned char chip_addr, unsigned char reg_addr, unsigned char mode)
{
	uint8_t rdreg_buf[5]={0}, chain;
    rdreg_buf[0]=CMD_TYPE | GET_STATUS;
    if(mode)
	{
		rdreg_buf[0] |= CMD_ALL;
	}
    rdreg_buf[1]=CMD_LENTH;
    rdreg_buf[2]=chip_addr;
    rdreg_buf[3]=reg_addr;
	rdreg_buf[4]=0;
	rdreg_buf[4]=crc5(rdreg_buf, 32);
	for(chain=0; chain<CHAINS_MAX; chain++)
    {
		if(boards[chain].exist)
        {
			pthread_mutex_lock(&boards[chain].tty_rw_mutex);
			write(boards[chain].tty_fd, rdreg_buf, CMD_LENTH+1);
			pthread_mutex_unlock(&boards[chain].tty_rw_mutex);
			cgsleep_ms(10);
        }
    }
}

void *read_hash_rate_func()
{
    while(42)
    {
		check_asic_reg_all_chains(0, HASHRATE, 1);
        cgsleep_ms(1000);
    }
}

unsigned int find_nearest_pll_index(unsigned int frequency)
{
	unsigned int i;
	if(frequency>1)
	{
		frequency--;
	}
	for(i=0; i<sizeof(freq_pll_1485)/sizeof(freq_pll_1485[0]); i++)
	{
		if(freq_pll_1485[i].freq>frequency)
		{
			return i;
		}
	}
	return find_nearest_pll_index(DEFAULT_FREQUENCY);
}

void init_pwm(int period_ns)
{
	char clbuf[64];
	sprintf(clbuf, "echo %i > /sys/class/pwm/pwm1/period_ns", period_ns);
	system(clbuf);
	sprintf(clbuf, "echo 1 > /sys/class/pwm/pwm1/run");
	system(clbuf);
}

void apply_pwm_setting(uint32_t pwm_value)
{
	char clbuf[64];
	uint32_t duty_ns;
	if(pwm_value<FAN_PWM_MIN_VALUE)
	{
		pwm_value=FAN_PWM_MIN_VALUE;
	}
	else if(pwm_value>FAN_PWM_MAX_VALUE)
	{
		pwm_value=FAN_PWM_MAX_VALUE;
	}
	if(autotuner->fan_pwm_value==pwm_value)
	{
		return;
	}
	autotuner->fan_pwm_value=pwm_value;
	duty_ns=FAN_PWM_PERIOD_NS*pwm_value/FAN_PWM_MAX_VALUE;
	sprintf(clbuf, "echo %u > /sys/class/pwm/pwm1/duty_ns", duty_ns);
	system(clbuf);
}

void adjust_pwm_according_to_temperature()
{
	uint8_t chain_id, hottest_board_id=0;
	float fan_control_p, fan_control_d, fan_pwm_new_value, hottest_board_temperature=0;
	for(chain_id=0; chain_id<CHAINS_MAX; chain_id++)
	{
		if(hottest_board_temperature<boards[chain_id].temperature.ext_sensor_value)
		{
			hottest_board_temperature=boards[chain_id].temperature.ext_sensor_value;
			hottest_board_id=chain_id;
		}
	}
	fan_pwm_new_value=autotuner->fan_pwm_value;
	if(autotuner->fan_pid_control_enabled)
	{
		if(device->number_of_chains)
		{
			fan_control_p=hottest_board_temperature-autotuner->target_temperature;
			autotuner->fan_control_i+=fan_control_p;
			fan_control_d=fan_control_p-autotuner->fan_control_p_prev;
			autotuner->fan_control_p_prev=fan_control_p;

			fan_pwm_new_value=fan_control_p*FAN_CONTROL_PK;
			fan_pwm_new_value+=autotuner->fan_control_i*FAN_CONTROL_IK;
			fan_pwm_new_value+=fan_control_d*FAN_CONTROL_DK;
		}
	}
	else // PID loop disabled from start untill some moment
	{
		// if temperature is stable, we can drop the rpm down slowly
		if((int)boards[hottest_board_id].temperature.ext_sensor_value==(int)boards[hottest_board_id].temperature_prev.ext_sensor_value)
		{
			fan_pwm_new_value-=1.0f;
			autotuner->fan_control_i=fan_pwm_new_value/FAN_CONTROL_IK;
		}
		// pretty close to the target_temp
		if(hottest_board_temperature>=autotuner->target_temperature)
		{
			// so we can start the PID loop normally
			autotuner->fan_pid_control_enabled=1;
		}
		// maybe, it's too cold outside and device cannot reach the target_temp
		// we should start the PID loop anyway
		if(autotuner->fan_pwm_value==FAN_PWM_MIN_VALUE)
		{
			autotuner->fan_pid_control_enabled=1;
		}
	}
	apply_pwm_setting(fan_pwm_new_value);
}

void *fill_work(void *arg)
{
    pthread_detach(pthread_self());
	board_parameters_t *brd=(board_parameters_t *)arg;
    struct timeval send_start, last_send, send_elapsed;
    struct work_ltc workdata;
	struct work *work=NULL;
	uint8_t workbuf[80], work_id;
	while(brd->powered_on)
    {
		usleep(10000);
		gettimeofday(&send_start, NULL);
        timersub(&send_start, &last_send, &send_elapsed);
		if(brd->new_block || send_elapsed.tv_sec >= brd->work_update_interval)
        {
			work=get_work(device->mining_control_thr);
			gettimeofday(&last_send, NULL);
			work_id=work->id & 0x7F;
			brd->new_block=false;
			memset(&workdata, 0, sizeof(workdata));
			memcpy(workbuf, work->data, SCRYPTDATA_SIZE+4);
			rev(workbuf, (size_t)SCRYPTDATA_SIZE+4);
			memcpy(workdata.s_data, workbuf+4, SCRYPTDATA_SIZE);
            workdata.type=0x01<<5;
			workdata.wc_base=work_id;
			workdata.length=SCRYPTDATA_SIZE+4;
			workdata.crc16=crc16_itu(0xFFFF, (uint8_t *) &workdata, workdata.length);
			workdata.crc16=bswap_16(workdata.crc16);
			memcpy((uint8_t *)&workdata+workdata.length, &workdata.crc16, 2);

            pthread_mutex_lock(&work_queue_mutex);
			if(g_work_queue[work_id])
            {
				free_work(&g_work_queue[work_id]);
            }
			g_work_queue[work_id]=copy_work(work);
            pthread_mutex_unlock(&work_queue_mutex);

			free_work(&work);

			pthread_mutex_lock(&brd->tty_rw_mutex);
			write(brd->tty_fd, &workdata, sizeof(workdata));
			pthread_mutex_unlock(&brd->tty_rw_mutex);

			gettimeofday(&g_tv_send_job, NULL);
        }
    }
	return NULL;
}

static inline uint32_t get_nonce_num_in_fifo(int const fd)
{
	uint32_t rx_len;
	if(ioctl(fd, FIONREAD, &rx_len)==0)
	{
		rx_len/=7;
		return rx_len;
	}
	return 0;
}

void check_chains()
{
	int fd, ret;
	uint8_t chain_id;
	char dev_fname[32], command[2];
	for(chain_id=0; chain_id<CHAINS_MAX; chain_id++)
    {
		boards[chain_id].chain_id=chain_id;
		sprintf(dev_fname, GPIO_DEVICE_TEMPLATE, plug[chain_id]);
		fd=open(dev_fname, O_RDONLY);
		if(fd<0)
        {
			applog(LOG_ERR, "%s:open %s failed", __FUNCTION__, dev_fname);
			continue;
        }
		if(lseek(fd, 0, SEEK_SET)<0)
		{
			perror(dev_fname);
		}
        ret=read(fd, command, 2);
		if(ret>0 && command[0]=='1')
        {
			boards[chain_id].exist=1;
			device->number_of_chains++;
			applog(LOG_NOTICE, "Chain[%u] detected at %s", chain_id, dev_fname);
        }
        else
        {
			boards[chain_id].exist=0;
        }
		close(fd);
    }
	applog(LOG_NOTICE, "detect total chain num %u", device->number_of_chains);
}

void tty_init_one_chain(uint8_t chain_id, int baud_rate)
{
	char dev_fname[64];
    struct termios options;
	speed_t speed;
	sprintf(dev_fname, TTY_DEVICE_TEMPLATE, tty[chain_id]);
	boards[chain_id].tty_fd=open(dev_fname, O_RDWR|O_NOCTTY);
	if(boards[chain_id].tty_fd<0)
	{
		applog(LOG_ERR, "%s: Chain[%u] open %s failed", __FUNCTION__, chain_id, dev_fname);
		boards[chain_id].tty_fd=0;
		return;
	}
	tcgetattr(boards[chain_id].tty_fd, &options);
	speed=tiospeed_t(baud_rate);
	if(speed==B0)
	{
		applog(LOG_WARNING, "Unrecognized baud rate %i. Default baud rate %i will be used.", baud_rate, DEFAULT_BAUD_RATE);
		speed=tiospeed_t(DEFAULT_BAUD_RATE);
	}
	cfsetispeed(&options, speed);
	cfsetospeed(&options, speed);
	options.c_cflag &= ~(CSIZE | PARENB);
	options.c_cflag |= CS8;
	options.c_cflag |= CREAD;
	options.c_cflag |= CLOCAL;
	options.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	options.c_oflag &= ~OPOST;
	options.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	options.c_cc[VTIME]=0;
	options.c_cc[VMIN]=7;
	tcsetattr(boards[chain_id].tty_fd, TCSANOW, &options);
	tcflush(boards[chain_id].tty_fd, TCIOFLUSH);
}

void set_config(uint8_t chain, uint8_t all, uint8_t asic_addr, uint8_t reg_addr, union REG_DATA reg_data)
{
    unsigned char cmd_buf[9]={0};

    cmd_buf[0]=CMD_TYPE | SET_CONFIG;
	if(all)
	{
		cmd_buf[0] |= CMD_ALL;
	}
    cmd_buf[1]=CONFIG_LENTH;
    cmd_buf[2]=asic_addr;
    cmd_buf[3]=reg_addr;
	if(reg_addr==MISC_CONTROL)
    {
		memcpy(&cmd_buf[4], (unsigned char *)&(reg_data.misc_ctrl_data),4);
    }
	else if(reg_addr==GENERAL_IIC)
    {
		memcpy(&cmd_buf[4], (unsigned char *)&(reg_data.general_iic_data),4);
    }
	else if(reg_addr==SECURITY_IIC)
    {
		memcpy(&cmd_buf[4], (unsigned char *)&(reg_data.security_iic_data),4);
    }
	else if(reg_addr==SEC_CTRL_STATUS)
    {
		memcpy(&cmd_buf[4], (unsigned char *)&(reg_data.scs_data),4);
    }
	else if(reg_addr==CORE_CMD_IN)
    {
		memcpy(&cmd_buf[4], (unsigned char *)&(reg_data.core_cmd_data),4);
    }
	else if(reg_addr==TICKET_MASK)
    {
		memcpy(&cmd_buf[4], (unsigned char *)&(reg_data.tm_data),4);
    }
	else if(reg_addr==HCN)
    {
		memcpy(&cmd_buf[4], (unsigned char *)&(reg_data.hcn_data),4);
    }
	else if(reg_addr==SNO)
    {
		memcpy(&cmd_buf[4], (unsigned char *)&(reg_data.sno_data),4);
    }
	else if(reg_addr==PLL_PARAMETER)
    {
		memcpy(&cmd_buf[4], (unsigned char *)&(reg_data.pll_data),4);
    }
	cmd_buf[8]=crc5(cmd_buf, 8*8);
	pthread_mutex_lock(&boards[chain].tty_rw_mutex);
	write(boards[chain].tty_fd, cmd_buf, CONFIG_LENTH+1);
	pthread_mutex_unlock(&boards[chain].tty_rw_mutex);
}

void chain_inactive(uint8_t chain_id)
{
    unsigned char cmd_buf[5]={0};
    cmd_buf[0]=CMD_ALL | CMD_TYPE | CHAIN_INACTIVE;
    cmd_buf[1]=CMD_LENTH ;
	cmd_buf[4]=crc5(cmd_buf, 4*8);
	pthread_mutex_lock(&boards[chain_id].tty_rw_mutex);
	write(boards[chain_id].tty_fd, cmd_buf, CMD_LENTH+1);
	pthread_mutex_unlock(&boards[chain_id].tty_rw_mutex);
}

void set_asic_address_one_chain(uint8_t chain_id)
{
	uint8_t chip_addr, cmd_buf[5];
	unsigned int j;
	if(device->max_asic_num_in_one_chain==0)
    {
		device->addrInterval=0;
        return;
    }
	device->addrInterval=256/device->max_asic_num_in_one_chain;
	chain_inactive(chain_id);
	cgsleep_ms(30);
	pthread_mutex_lock(&boards[chain_id].tty_rw_mutex);
	for(j=0, chip_addr=0; j<256/device->addrInterval; j++)
	{
		cmd_buf[0]=CMD_TYPE | SET_ADDR;
		cmd_buf[1]=CMD_LENTH;
		cmd_buf[2]=chip_addr;
		cmd_buf[4]=crc5(cmd_buf, 4*8);
		write(boards[chain_id].tty_fd, cmd_buf, CMD_LENTH+1);
		usleep(1000);
		chip_addr+= device->addrInterval;
	}
	pthread_mutex_unlock(&boards[chain_id].tty_rw_mutex);
}

void set_core_ctrl_one_chain(uint8_t chain_id, uint8_t cmd_type, uint8_t mode, uint8_t cmd_data)
{
	uint8_t i;
    union REG_DATA regdata;

    regdata.core_cmd_data.all=mode;
    regdata.core_cmd_data.cmd_type=cmd_type;
    regdata.core_cmd_data.cmd_data=cmd_data;

	for(i=0; i<BM1485_CORE_NUM; i++)
	{
		if(i>5)
		{
			regdata.core_cmd_data.core_index=i+2;
		}
		else
		{
			regdata.core_cmd_data.core_index=i;
		}
		set_config(chain_id, 1, 0, CORE_CMD_IN, regdata);
		cgsleep_ms(2);
	}
}

void set_core_temp_ctrl_one_chain(uint8_t chain_id, uint8_t cmd_type, uint8_t mode, uint8_t cmd_data)
{
    union REG_DATA regdata;

    regdata.core_cmd_data.all=mode;
	regdata.core_cmd_data.core_index=opt_core_temp;
    regdata.core_cmd_data.cmd_type=cmd_type;
    regdata.core_cmd_data.cmd_data=cmd_data;

	set_config(chain_id, 0, temp_sensor_chip_addr, CORE_CMD_IN, regdata);
	cgsleep_ms(2);
}

void set_temp_read_all_chains(unsigned char reg_addr)
{
	union REG_DATA regdata;
	uint8_t chain;
    memset(&regdata, 0, sizeof( regdata ));
    regdata.general_iic_data.rw_ctrl=0x0;
    regdata.general_iic_data.regaddrvalid=0x1;
    regdata.general_iic_data.deviceaddr=0x4c;
	regdata.general_iic_data.regaddr=reg_addr;
    regdata.general_iic_data.autoreadtemp=0x00;
	for(chain=0; chain<CHAINS_MAX; ++chain)
    {
		if(boards[chain].exist==1)
        {
			set_config(chain, 0, temp_sensor_chip_addr, GENERAL_IIC, regdata);
			cgsleep_ms(2);
        }
    }
	cgsleep_ms(IIC_SLEEP);
}

void set_misc_ctrl_one_chain(uint8_t chain_id)
{
    union REG_DATA regdata;
    memset(&regdata, 0, sizeof( regdata));
    regdata.misc_ctrl_data.hashratrectrl1=(1);
    regdata.misc_ctrl_data.hashratrectrl2=(4);
    regdata.misc_ctrl_data.ldo18ctrl=(4);
    regdata.misc_ctrl_data.inv_clko=(1);
	regdata.misc_ctrl_data.bt8d=26; // 0x1A for 115200 which is default baud rate
    regdata.misc_ctrl_data.ldo09ctrl=(4);
    regdata.misc_ctrl_data.ldo09_pd=0;

	set_config(chain_id, 1, 0, MISC_CONTROL, regdata);
	cgsleep_ms(2);

    regdata.misc_ctrl_data.tfs=(3);
    regdata.misc_ctrl_data.rfs=(1);

	set_config(chain_id, 0, temp_sensor_chip_addr, MISC_CONTROL, regdata);
	cgsleep_ms(2);
}

void *read_temp_func()
{
//	uint8_t sensor_reg[4]={EXT_TEMP_VALUE_HIGH_BYTE, EXT_TEMP_VALUE_LOW_BYTE, INT_TEMP_VALUE_HIGH_BYTE, INT_TEMP_VALUE_LOW_BYTE};
	uint8_t sensor_reg[4]={EXT_TEMP_VALUE_HIGH_BYTE, EXT_TEMP_VALUE_LOW_BYTE, INT_TEMP_VALUE_HIGH_BYTE};
	uint8_t i;
	while(42)
	{
		for(i=0; i<g_check_temp_sensor_values; i++)
		{
			set_temp_read_all_chains(sensor_reg[i]);
			check_asic_reg_all_chains(temp_sensor_chip_addr, GENERAL_IIC, 0);
			sleep(1);
		}
	}
}

void set_asic_ticket_mask_one_chain(uint8_t chain_id, uint32_t ticket_mask)
{
	uint32_t tm_swapped;
	tm_swapped=bswap_32(ticket_mask);
    union REG_DATA regdata;
	memcpy(regdata.tm_data.reg_data, &tm_swapped, 4);
	set_config(chain_id, 1, 0, TICKET_MASK, regdata);
	cgsleep_ms(2);
}

void i2c_init()
{
	device->i2c_fd=open(I2C_DEVICE, O_RDWR | O_NONBLOCK);
	if(device->i2c_fd<0)
    {
		applog(LOG_ERR, "%s: i2c init error. Cannot open %s", __FUNCTION__, I2C_DEVICE);
    }
	else
	{
		applog(LOG_NOTICE, "%s: i2c init ok", __FUNCTION__);
	}
}

void *pic_heart_beat_func()
{
	uint8_t board_id;
	while(1)
	{
		for(board_id=0; board_id<CHAINS_MAX; board_id++)
		{
			if(boards[board_id].exist)
			{
				pic_send_heart_beat(board_id);
			}
		}
		sleep(PIC_HEART_BEAT_INTERVAL);
	}
}

static void ProcessTempData(const uint8_t *const data, const uint8_t chip_addr, uint8_t chain_id)
{
	uint8_t buffer[4];
	memcpy(buffer, data, 4);

	if(chip_addr!=temp_sensor_chip_addr)
	{
		return;
	}

	int32_t itemp=0;

	if(buffer[2]==EXT_TEMP_VALUE_HIGH_BYTE)
	{
		boards[chain_id].temperature_prev.ext_sensor_value=boards[chain_id].temperature.ext_sensor_value;
		itemp=boards[chain_id].temperature.ext_sensor_value-boards[chain_id].temperature.offset;
		if(itemp!=buffer[3])
		{
			boards[chain_id].temperature.ext_sensor_value=buffer[3];
			boards[chain_id].temperature.ext_sensor_value+=boards[chain_id].temperature.offset;
		}
		//applog(LOG_NOTICE, "Chain[%u] ext_sensor_value=%.2f", chain_id, boards[chain_id].temperature.ext_sensor_value);
	}
	// LOW_BYTE value can be 11110000, so that is correct way to get real number
	// (float)(buffer[3]>>4)/16.0f;
	if(buffer[2]==EXT_TEMP_VALUE_LOW_BYTE)
	{
		boards[chain_id].temperature_prev.ext_sensor_value=boards[chain_id].temperature.ext_sensor_value;
		itemp=boards[chain_id].temperature.ext_sensor_value;
		boards[chain_id].temperature.ext_sensor_value=itemp;
		boards[chain_id].temperature.ext_sensor_value+=(float)(buffer[3]>>4)/16.0f;
		//applog(LOG_DEBUG, "Chain[%u] ext_sensor_low_byte=%u", chain_id, buffer[3]);
	}

	if(itemp)
	{
		if(device->temperature_max<boards[chain_id].temperature.ext_sensor_value)
		{
			device->temperature_max=boards[chain_id].temperature.ext_sensor_value;
		}
	}

	if(g_check_temp_sensor_values>2)
	{
		if(buffer[2]==INT_TEMP_VALUE_HIGH_BYTE)
		{
			itemp=boards[chain_id].temperature.int_sensor_value;
			if(itemp!=buffer[3])
			{
				boards[chain_id].temperature.int_sensor_value=buffer[3];
			}
			//applog(LOG_NOTICE, "Chain[%u] int_sensor_value=%.2f", chain_id, boards[chain_id].temperature.int_sensor_value);
		}
//		if(buffer[2]==INT_TEMP_VALUE_LOW_BYTE)
//		{
//			itemp=boards[chain_id].temperature.int_sensor_value;
//			boards[chain_id].temperature.int_sensor_value=itemp;
//			boards[chain_id].temperature.int_sensor_value+=(float)(buffer[3]>>4)/16.0f;
//			applog(LOG_DEBUG, "Chain[%u] int_sensor_low_byte=%u", chain_id, buffer[3]);
//		}
		if(boards[chain_id].temperature.ext_sensor_value>1.0f && boards[chain_id].temperature.int_sensor_value>1.0f)
		{
			boards[chain_id].temperature.ext_sensor_value-=boards[chain_id].temperature.offset;
			boards[chain_id].temperature.offset=boards[chain_id].temperature.int_sensor_value-boards[chain_id].temperature.ext_sensor_value;
			boards[chain_id].temperature.ext_sensor_value+=boards[chain_id].temperature.offset;
			applog(LOG_INFO, "Chain[%u] temperature offset=%i", chain_id, boards[chain_id].temperature.offset);
//			for(chain_id=0, itemp=0; chain_id<CHAINS_MAX; itemp+=boards[chain_id].temperature.offset?1:0, chain_id++);
//			if(chain_id==itemp)
//			{
//				g_check_temp_sensor_values=2;
//			}
		}
	}
}

void *get_asic_response(void *arg)
{
	pthread_detach(pthread_self());
	board_parameters_t *brd=(board_parameters_t *)arg;
	unsigned char *nonce_bin_rbuf=malloc(NONCE_BIN_RBUF_SIZE*7);
	uint8_t reg_val[4]={0, 0, 0, 0}, chip_addr, reg_addr, chip_id=0, crc_check;
	uint32_t n, nonce_available;
	int32_t how_much_readed;
	while(brd->powered_on)
	{
		usleep(2500);
		nonce_available=get_nonce_num_in_fifo(brd->tty_fd);
		pthread_mutex_lock(&brd->tty_rw_mutex);
		how_much_readed=read(brd->tty_fd, nonce_bin_rbuf, nonce_available<NONCE_BIN_RBUF_SIZE?nonce_available*7:NONCE_BIN_RBUF_SIZE*7);
		pthread_mutex_unlock(&brd->tty_rw_mutex);
		for(n=0; n<how_much_readed; n+=7)
		{
			if((nonce_bin_rbuf[n+6] & 0x80)==NONCE_BIT)   //nonce
			{
				pthread_mutex_lock(&nonce_fifo_mutex);
				memcpy(g_nonce_fifo.nonce_data[g_nonce_fifo.p_wr].nonce, &nonce_bin_rbuf[n+0], 4);
				g_nonce_fifo.nonce_data[g_nonce_fifo.p_wr].diff          =nonce_bin_rbuf[n+4];
				g_nonce_fifo.nonce_data[g_nonce_fifo.p_wr].wc            =nonce_bin_rbuf[n+5];
				g_nonce_fifo.nonce_data[g_nonce_fifo.p_wr].crc5          =nonce_bin_rbuf[n+6];
				g_nonce_fifo.nonce_data[g_nonce_fifo.p_wr].chainid       =brd->chain_id;
				if(++g_nonce_fifo.p_wr>=MAX_NONCE_NUMBER_IN_FIFO)
				{
					g_nonce_fifo.p_wr=0;
				}
				if(g_nonce_fifo.nonce_num<MAX_NONCE_NUMBER_IN_FIFO)
				{
					g_nonce_fifo.nonce_num++;
				}
				else
				{
					clear_nonce_buf();
				}
				pthread_mutex_unlock(&nonce_fifo_mutex);
			}
			else //reg value
			{
				chip_addr		=nonce_bin_rbuf[n+4];
				reg_addr		=nonce_bin_rbuf[n+5];
				crc_check		=nonce_bin_rbuf[n+6] & 0x1F;

				if(brd->glitch_detected)
				{
					goto skip_value;
				}

				uint8_t crc5_true=crc5(&nonce_bin_rbuf[n], 7*8-5);
				if(crc_check!=crc5_true)
				{
					applog(LOG_ERR, "%s: Chain[%u] CRC5 error, should be 0x%02x, received 0x%02x", __FUNCTION__, brd->chain_id, crc5_true, crc_check);
					brd->glitch_detected=1;
					goto skip_value;
				}

				memcpy(reg_val, &nonce_bin_rbuf[n], 4);

	//			if(reg_addr==CORE_RESP_OUT)
	//			{
	//			}

	//			if(reg_addr==PLL_PARAMETER)
	//			{
	//			}

	//			if(reg_addr==MISC_CONTROL)
	//			{
	//			}

				if(reg_addr==GENERAL_IIC)
				{
					ProcessTempData(reg_val, chip_addr, brd->chain_id);
				}
				if(reg_addr==HASHRATE)
				{
					brd->asic_hash_rate[chip_id]=(0x00000000)|(reg_val[3]<<8)|(reg_val[2]<<16)|(reg_val[1]<<24);
					if(++chip_id<ASICS_ON_CHAIN_MAX)
					{
						;
					}
					else
					{
						chip_id=0;
					}
				}
	//			if(reg_addr==EXT_TEMP_SENSOR)
	//			{
	//			}
				if(reg_addr==CHIP_ADDR)
				{
					if(update_asic_num)
					{
						if(++brd->number_of_asics>ASICS_ON_CHAIN_MAX)
						{
							brd->number_of_asics=ASICS_ON_CHAIN_MAX;
						}
					}
					if(brd->number_of_asics>device->max_asic_num_in_one_chain)
					{
						device->max_asic_num_in_one_chain=brd->number_of_asics;
					}
				}
				skip_value:
				;
			}
		}
	}
	free(nonce_bin_rbuf);
	return NULL;
}

void set_frequency_by_index_on_chain(uint8_t chain_id, unsigned int pll_index)
{
	uint32_t vilpll_bswapped=bswap_32(freq_pll_1485[pll_index].vilpll);
	union REG_DATA regdata;
	applog(LOG_NOTICE, "Chain[%u] set frequency=%u", chain_id, freq_pll_1485[pll_index].freq);
	memcpy(regdata.pll_data.reg_data, &vilpll_bswapped, 4);
	boards[chain_id].freq=freq_pll_1485[pll_index].freq;
	set_config(chain_id, 1, 0, PLL_PARAMETER, regdata);
	cgsleep_ms(2);
}

void set_frequency_on_chain(uint8_t chain_id, unsigned int frequency)
{
	unsigned int pll_index=find_nearest_pll_index(frequency);
	set_frequency_by_index_on_chain(chain_id, pll_index);
}

void set_frequency_by_index_on_asic(uint8_t chain_id, uint8_t asic_addr, unsigned int pll_index)
{
	uint32_t vilpll_bswapped=bswap_32(freq_pll_1485[pll_index].vilpll);
	union REG_DATA regdata;
	applog(LOG_NOTICE, "Chain[%u] asic_addr=%u set frequency=%u", chain_id, asic_addr, freq_pll_1485[pll_index].freq);
	memcpy(regdata.pll_data.reg_data, &vilpll_bswapped, 4);
	set_config(chain_id, 0, asic_addr, PLL_PARAMETER, regdata);
	cgsleep_ms(2);
}

//Параметры "static loss" и "switching loss" выбираются на основе измерений.
#define static_loss		0.15
#define switching_loss	0.75e-3
static inline float asic_power_estimate(float freq, float Vdd)
{
	return(freq*switching_loss*BM1485_CORE_NUM+static_loss)*Vdd*Vdd;
}

static inline float volt_pic_val_to_real_val(uint8_t volt_pic_val)
{
	return(10.965-volt_pic_val*0.006);
}

void set_voltage_one_chain(uint8_t chain_id, uint8_t volt_pic_val)
{
	applog(LOG_NOTICE, "set voltage on chain[%u], pic value=%u | voltage=%.2f V", chain_id, volt_pic_val, volt_pic_val_to_real_val(volt_pic_val));
	pic_set_voltage(chain_id, volt_pic_val);
	boards[chain_id].volt=volt_pic_val;
}

void recalculate_update_interval(uint8_t chain_id)
{
	boards[chain_id].work_update_interval=7864200.0/(ASICS_ON_CHAIN_MAX*BM1485_CORE_NUM*boards[chain_id].freq);
	applog(LOG_INFO, "Chain[%u] work_update_interval=%u", chain_id, boards[chain_id].work_update_interval);
}

void power_up_board(uint8_t board_id)
{
	applog(LOG_NOTICE, "Board[%u] power will be powered up now.", board_id);
	pic_enable_voltage(board_id);
	boards[board_id].powered_on=1;
}

void shutdown_board(uint8_t board_id)
{
	applog(LOG_WARNING, "Board[%u] power will be turned off now.", board_id);
	boards[board_id].powered_on=0;
	pic_disable_voltage(board_id);
}

void enable_board(uint8_t chain_id)
{
	int ptcret;

	power_up_board(chain_id);
	sleep(1);

	reset_asics_one_chain(chain_id);
	cgsleep_ms(500);

	tty_init_one_chain(chain_id, DEFAULT_BAUD_RATE);

	ptcret=pthread_create(&boards[chain_id].uart_rx_thr, NULL, get_asic_response, &boards[chain_id]);
	if(ptcret)
	{
		applog(LOG_ERR, "Create RX thread for Chain[%u]:failed", chain_id);
	}
	else
	{
		applog(LOG_INFO, "Create RX thread for Chain[%u]:ok", chain_id);
	}
	ptcret=pthread_create(&boards[chain_id].uart_tx_thr, NULL, fill_work, &boards[chain_id]);
	if(ptcret)
	{
		applog(LOG_ERR, "Create TX thread for Chain[%u] failed", chain_id);
	}
	else
	{
		applog(LOG_INFO, "Create TX thread for Chain[%u]:ok", chain_id);
	}

	set_asic_address_one_chain(chain_id);

	set_voltage_one_chain(chain_id, autotuner->target_volt[chain_id]);
	set_frequency_by_index_on_chain(chain_id, autotuner->chain_pll_index_last_time[chain_id]);
	recalculate_update_interval(chain_id);

	//set baud
	set_misc_ctrl_one_chain(chain_id);

	//can be useful somehow, but there is no any handler for `MISC_CONTROL` response
	//case in `get_asic_response()`
	//check_asic_reg_all_chains(0, MISC_CONTROL, 1);
	//cgsleep_ms(IIC_SLEEP);

	set_core_temp_ctrl_one_chain(chain_id, TEMP_DIODE_SEL, 0, 0);

	//open block
	set_core_ctrl_one_chain(chain_id, CLOCK_EN_CTRL, 0, 1);

	set_asic_ticket_mask_one_chain(chain_id, (1<<DEVICE_DIFF)-1); //000000FF

	boards[chain_id].exist=1;
	device->number_of_chains++;
}

void disable_board(uint8_t chain_id)
{
	boards[chain_id].exist=0;
	device->number_of_chains--;
	shutdown_board(chain_id);
	memset(boards[chain_id].asic_hwe_count, 0, ASICS_ON_CHAIN_MAX*sizeof(boards[chain_id].asic_hwe_count[0]));
	boards[chain_id].hwe_count=0;
	boards[chain_id].glitch_detected=0;
	autotuner->chain_hwe_prev[chain_id]=0;
	sleep(1);
	tcflush(boards[chain_id].tty_fd, TCIOFLUSH);
	close(boards[chain_id].tty_fd);
}

void *check_miner_status_func()
{
	double hr_rolling, chain_hashrate;
	uint8_t chain_id, chip_id;
	struct timeval tv_start={0, 0}, tv_end, tv_send, tv_diff;
	cgtime(&tv_end);
	cgtime(&tv_send);
	copy_time(&tv_start, &tv_end);
	copy_time(&g_tv_send_job, &tv_send);
	while(1)
	{
		cgtime(&tv_end);
		cgtime(&tv_send);
		timersub(&tv_end, &tv_start, &tv_diff);

		if(tv_diff.tv_sec>600)
		{
			for(chain_id=0; chain_id<CHAINS_MAX; chain_id++)
			{
				if(boards[chain_id].exist)
				{
					for(chip_id=0; chip_id<ASICS_ON_CHAIN_MAX; chip_id++)
					{
						if(boards[chain_id].asic_nonce_count[chip_id])
						{
							boards[chain_id].asic_alive[chip_id]=1;
						}
						else
						{
							boards[chain_id].asic_alive[chip_id]=0;
						}
						boards[chain_id].asic_nonce_count[chip_id]=0;
					}
				}
			}
			copy_time(&tv_start, &tv_end);
		}

		timersub(&tv_send, &g_tv_send_job, &tv_diff);
		if(tv_diff.tv_sec>120)
		{
			applog(LOG_ERR, "Latest job done more than 2 mins ago!..");
		}

		if(device->mining_control_thr->clean_jobs)
		{
			device->mining_control_thr->clean_jobs=0;
			for(chain_id=0; chain_id<CHAINS_MAX; chain_id++)
			{
				boards[chain_id].new_block=true;
			}
		}

		for(chain_id=0, hr_rolling=0; chain_id<CHAINS_MAX; chain_id++)
		{
			if(boards[chain_id].exist)
			{
				for(chip_id=0, chain_hashrate=0; chip_id<ASICS_ON_CHAIN_MAX; chip_id++)
				{
					chain_hashrate+=boards[chain_id].asic_hash_rate[chip_id];
				}
				boards[chain_id].hash_rate=chain_hashrate;
				hr_rolling+=(double)chain_hashrate/1000000.0;
			}
			if(boards[chain_id].exist && boards[chain_id].glitch_detected)
			{
				disable_board(chain_id);
			}
			if(boards[chain_id].exist && boards[chain_id].temperature.ext_sensor_value>WORKING_TEMPERATURE_MAX)
			{
				applog(LOG_WARNING, "Board[%u] overheat!", chain_id);
				autotuner->chain_tunable[chain_id]=0;
				disable_board(chain_id);
			}
		}
		g_hr_rolling=hr_rolling;

		sleep(CHECK_SYSTEM_INTERVAL);
	}
}

int8_t autotuner_check_chips_num(uint8_t chain)
{
	uint8_t chip, chips_alive;
	for(chip=0, chips_alive=0; chip<ASICS_ON_CHAIN_MAX; chip++)
	{
		if(boards[chain].asic_alive[chip])
		{
			chips_alive++;
		}
	}
	if(chips_alive==ASICS_ON_CHAIN_MAX)
	{
		applog(LOG_INFO, "%s: Chain[%u] %u chips alive. Ok, i can work with it.", __FUNCTION__, chain, chips_alive);
		return(1);
	}
	applog(LOG_NOTICE, "%s: Chain[%u] %u chips alive. Not enouth to work properly.", __FUNCTION__, chain, chips_alive);
	return(-1);
}

int8_t autotuner_check_hwerrors(uint8_t chain)
{
	int8_t res=-1;
	int new_hwerr=boards[chain].hwe_count-autotuner->chain_hwe_prev[chain];
	autotuner->chain_hwe_prev[chain]=boards[chain].hwe_count;
	if(new_hwerr<1)
	{
		applog(LOG_INFO, "%s: Chain[%u] hw errors %u looks Ok to me.", __FUNCTION__, chain, boards[chain].hwe_count);
		res=1;
	}
	else if(new_hwerr<2)
	{
		applog(LOG_INFO, "%s: Chain[%u] hw errors %u not good, not terrible.", __FUNCTION__, chain, boards[chain].hwe_count);
		res=0;
	}
	else
	{
		applog(LOG_NOTICE, "%s: Chain[%u] hw errors %u that's too much.", __FUNCTION__, chain, boards[chain].hwe_count);
	}
	return(res);
}

int8_t autotuner_check_temperature(uint8_t chain)
{
	int8_t res=-1;
	if((int)boards[chain].temperature.ext_sensor_value<(int)autotuner->target_temperature)
	{
		res=1;
		applog(LOG_INFO, "%s: Chain[%u] temperature=%.1f looks Ok to me.", __FUNCTION__, chain, boards[chain].temperature.ext_sensor_value);
	}
	else if((int)boards[chain].temperature.ext_sensor_value==(int)autotuner->target_temperature)
	{
		res=0;
		applog(LOG_INFO, "%s: Chain[%u] temperature=%.1f looks Ok to me.", __FUNCTION__, chain, boards[chain].temperature.ext_sensor_value);
	}
	else if((int)boards[chain].temperature.ext_sensor_value>(int)autotuner->target_temperature)
	{
		res=-1;
		applog(LOG_NOTICE, "%s: Chain[%u] temperature=%.1f that's too hot.", __FUNCTION__, chain, boards[chain].temperature.ext_sensor_value);
	}
	return(res);
}

int8_t autotuner_check_hashrate(uint8_t chain)
{
	int8_t res=-1;
	uint32_t hr_estimated_max, hr_estimated_min;
	/*
	* (freq[chain_id] * ASIC_NUM * CORE_NUM * 0.95) / 2500
	* freq[chain_id] * ASIC_NUM * CORE_NUM * 380
	*
	* (400 MHz*72*12*0.95)/2500	=131,328 MH/s
	* 400 MHz*72*12*380			=131328000 H/s
	*/
	hr_estimated_max=(boards[chain].freq+30)*ASICS_ON_CHAIN_MAX*BM1485_CORE_NUM*380;
	hr_estimated_min=(boards[chain].freq-30)*ASICS_ON_CHAIN_MAX*BM1485_CORE_NUM*380;
	if(boards[chain].hash_rate>hr_estimated_min && boards[chain].hash_rate<hr_estimated_max)
	{
		res=0;
		applog(LOG_INFO, "%s: Chain[%u] HR=%u looks Ok to me.", __FUNCTION__, chain, boards[chain].hash_rate/1000000);
		hr_estimated_max=(boards[chain].freq+15)*ASICS_ON_CHAIN_MAX*BM1485_CORE_NUM*380;
		hr_estimated_min=(boards[chain].freq-15)*ASICS_ON_CHAIN_MAX*BM1485_CORE_NUM*380;
		if(boards[chain].hash_rate>hr_estimated_min && boards[chain].hash_rate<hr_estimated_max)
		{
			res=1;
		}
	}
	else
	{
		applog(LOG_NOTICE, "%s: Chain[%u] HR=%u looks inadequate (should be: %u ~ %u)", __FUNCTION__, chain, boards[chain].hash_rate/1000000, hr_estimated_min/1000000, hr_estimated_max/1000000);
	}
	return(res);
}

void *autotuner_func()
{
	uint32_t chain_freq_pll_index;
	uint8_t chain_id;
	int8_t chain_rating[CHAINS_MAX]={0, 0, 0, 0};
	struct timeval tv_start={0, 0}, tv_now={0, 0}, tv_elapsed;

	gettimeofday(&tv_start, NULL);
	copy_time(&tv_now, &tv_start);

	while(42)
	{
		adjust_pwm_according_to_temperature();
		/* WIP | WE NEED DO THE FAN CHECK HERE
		 * this is old fan_check from `check_miner_status_func()`

		fan_ret=check_fan_ok();
		if(fan_ret)
		{
			for(chain_id=0; chain_id<MAX_CHAIN_NUM; chain_id++)
			{
				if(dev.chain_exist[chain_id])
				{
					pthread_mutex_lock(&i2c_mutex);
					if(ioctl(dev.i2c_fd, I2C_SLAVE, i2c_slave_addr[chain_id] >> 1)<0)
					{
						applog(LOG_ERR, "%s: Chain[%u] ioctl error: %s", __FUNCTION__, chain_id, strerror(errno));
					}
					pic_disable();
					pthread_mutex_unlock(&i2c_mutex);
				}
			}
		}
		*/

		switch(autotuner->state)
		{
			case AUTOTUNER_PREPARE:
			{
				gettimeofday(&tv_now, NULL);
				timersub(&tv_now, &tv_start, &tv_elapsed);
				if(tv_elapsed.tv_sec>AUTOTUNER_PREPARE_INTERVAL && autotuner->fan_pid_control_enabled)
				{
					copy_time(&tv_start, &tv_now);
					for(chain_id=0; chain_id<CHAINS_MAX; chain_id++)
					{
						// bad chips? shutdown board // WIP | NTFL
						if(autotuner->chain_tunable[chain_id] && autotuner_check_chips_num(chain_id)<0)
						{
							applog(LOG_WARNING, "Board[%u] not enought chips alive!", chain_id);
							boards[chain_id].glitch_detected=1;
							autotuner->chain_tunable[chain_id]=0;
							continue;
						}
						if(autotuner->chain_tunable[chain_id] && boards[chain_id].hash_rate==0)
						{
							applog(LOG_WARNING, "Board[%u] low hash rate!", chain_id);
							boards[chain_id].glitch_detected=1;
							autotuner->chain_tunable[chain_id]=0;
							continue;
						}
						if(autotuner->chain_tunable[chain_id] && !autotuner->chain_tuned[chain_id] && boards[chain_id].powered_on)
						{
							chain_rating[chain_id]=0;
							chain_rating[chain_id]+=autotuner_check_hashrate(chain_id);
							chain_rating[chain_id]+=autotuner_check_hwerrors(chain_id);
							chain_rating[chain_id]+=autotuner_check_temperature(chain_id);
							if(chain_rating[chain_id]==3)
							{
								chain_rating[chain_id]=1;
							}
							else if(chain_rating[chain_id]<0)
							{
								chain_rating[chain_id]=-1;
							}
							else
							{
								chain_rating[chain_id]=0;
							}
						}
						if(autotuner->chain_tunable[chain_id] && autotuner->chain_tuned[chain_id] && boards[chain_id].powered_on)
						{
							if(autotuner_check_temperature(chain_id)<0)
							{
								autotuner->chain_tuned[chain_id]=0;
								chain_rating[chain_id]=-1;
							}
						}
					}
					autotuner->state=AUTOTUNER_WORKING;
				}
				break;
			}
			case AUTOTUNER_WORKING:
			{
				for(chain_id=0; chain_id<CHAINS_MAX; chain_id++)
				{
					if(autotuner->chain_tunable[chain_id] && !autotuner->chain_tuned[chain_id] && boards[chain_id].powered_on)
					{
						chain_freq_pll_index=find_nearest_pll_index(boards[chain_id].freq);
						autotuner->chain_pll_index_last_time[chain_id]=chain_freq_pll_index;
						chain_freq_pll_index+=chain_rating[chain_id];
						if(chain_rating[chain_id]<0)
						{
							autotuner->chain_tuned[chain_id]=1;
						}
						if(chain_rating[chain_id])
						{
							set_frequency_by_index_on_chain(chain_id, chain_freq_pll_index);
							recalculate_update_interval(chain_id);
							if(boards[chain_id].freq<opt_target_freq)
							{
								// ?
							}
							else
							{
								autotuner->chain_tuned[chain_id]=1;
							}
						}
					}
					if(autotuner->chain_tunable[chain_id] && !boards[chain_id].powered_on)
					{
						usleep(250000);
					}
					if(autotuner->chain_tunable[chain_id] && !boards[chain_id].powered_on)
					{
						enable_board(chain_id);
					}
				}
				autotuner->state=AUTOTUNER_PREPARE;
				break;
			}
			case AUTOTUNER_DONE:
			case AUTOTUNER_DISABLED:
			{
				break;
			}
		}
		sleep(3);
	}
	return NULL;
}

void autotuner_init()
{
	uint8_t chain;
	if(opt_enable_autotuner)
	{
		autotuner->state=AUTOTUNER_PREPARE;
		applog(LOG_INFO, "%s: autotuner enabled", __FUNCTION__);
	}
	else
	{
		autotuner->state=AUTOTUNER_DISABLED;
		applog(LOG_INFO, "%s: autotuner disabled", __FUNCTION__);
	}
	if(opt_target_temp>WORKING_TEMPERATURE_MAX)
	{
		opt_target_temp=WORKING_TEMPERATURE_MAX;
	}
	else if(opt_target_temp<WORKING_TEMPERATURE_MIN)
	{
		opt_target_temp=WORKING_TEMPERATURE_MIN;
	}
	autotuner->target_temperature=opt_target_temp;
	applog(LOG_INFO, "%s: target_temperature=%.0f", __FUNCTION__, autotuner->target_temperature);
	init_pwm(FAN_PWM_PERIOD_NS);
	apply_pwm_setting((FAN_PWM_MAX_VALUE+FAN_PWM_MIN_VALUE)/2);
	for(chain=0; chain<CHAINS_MAX; chain++)
	{
		autotuner->chain_pll_index_last_time[chain]=32; // 400MHz
		if(opt_volt[chain]<0)
		{
			opt_volt[chain]=0;
		}
		else if(opt_volt[chain]>255)
		{
			opt_volt[chain]=255;
		}
		autotuner->target_volt[chain]=opt_volt[chain];
		applog(LOG_INFO, "%s: target_volt[%u]=%u", __FUNCTION__, chain, autotuner->target_volt[chain]);
		if(boards[chain].exist)
		{
			set_voltage_one_chain(chain, autotuner->target_volt[chain]);
		}
	}
	if(opt_target_freq<250)
	{
		opt_target_freq=250;
	}
	else if(opt_target_freq>650)
	{
		opt_target_freq=650;
	}
	for(chain=0; chain<CHAINS_MAX; chain++)
	{
		if(opt_freq[chain]<250)
		{
			opt_freq[chain]=250;
		}
		else if(opt_freq[chain]>650)
		{
			opt_freq[chain]=650;
		}
		if(boards[chain].exist)
		{
			set_frequency_on_chain(chain, opt_freq[chain]);
			recalculate_update_interval(chain);
		}
	}
}

static bool bitmain_L3_prepare(struct thr_info *thr)
{
	int check_asic_times=0;
	uint8_t chain_id, chip;
	bool check_asic_fail=false;

	device->mining_control_thr=thr;

	for(chain_id=0; chain_id<WORK_QUEUE_SIZE; chain_id++)
	{
		g_work_queue[chain_id]=NULL;
	}

	check_chains();
	cgsleep_ms(10);

	i2c_init();
	cgsleep_ms(10);

	for(chain_id=0; chain_id<CHAINS_MAX; chain_id++)
	{
		if(boards[chain_id].exist==1)
		{
			pic_reset(chain_id);
		}
	}
	cgsleep_ms(800);

	for(chain_id=0; chain_id<CHAINS_MAX; chain_id++)
	{
		if(boards[chain_id].exist==1)
		{
			pic_jump_from_loader_to_app(chain_id);
		}
	}
	cgsleep_ms(800);

	device->temperature_max=0;

	for(chain_id=0; chain_id<CHAINS_MAX; chain_id++)
	{
		if(boards[chain_id].exist)
		{
			power_up_board(chain_id);
			set_voltage_one_chain(chain_id, 255);
		}
	}
	sleep(1);

	for(chain_id=0; chain_id<CHAINS_MAX; chain_id++)
	{
		reset_asics_one_chain(chain_id);
	}
	cgsleep_ms(500);

	clear_nonce_buf();

	for(chain_id=0; chain_id<CHAINS_MAX; chain_id++)
	{
		int ptcret;
		if(boards[chain_id].exist)
		{
			tty_init_one_chain(chain_id, DEFAULT_BAUD_RATE);
			ptcret=pthread_create(&boards[chain_id].uart_rx_thr, NULL, get_asic_response, (void *)&boards[chain_id]);
			if(ptcret)
			{
				applog(LOG_ERR, "Create RX thread for Chain[%u]:failed", chain_id);
			}
			else
			{
				applog(LOG_INFO, "Create RX thread for Chain[%u]:ok", chain_id);
			}
			ptcret=pthread_create(&boards[chain_id].uart_tx_thr, NULL, fill_work, (void *)&boards[chain_id]);
			if(ptcret)
			{
				applog(LOG_ERR, "Create TX thread for Chain[%u] failed", chain_id);
			}
			else
			{
				applog(LOG_INFO, "Create TX thread for Chain[%u]:ok", chain_id);
			}
		}
	}

	//check ASIC number for every chain
	applog(LOG_NOTICE, "send cmd to get chip address");

check_asic_num:
	update_asic_num=true;
	check_asic_reg_all_chains(0, CHIP_ADDR, 1);
	sleep(2);
	update_asic_num=false;

	for(chain_id=0; chain_id<CHAINS_MAX; chain_id++)
	{
		if(boards[chain_id].exist==1)
		{
			if(boards[chain_id].number_of_asics!=ASICS_ON_CHAIN_MAX)
			{
				check_asic_fail=true;
				check_asic_times++;
			}
			else
			{
				for(chip=0; chip<ASICS_ON_CHAIN_MAX; chip++)
				{
					boards[chain_id].asic_alive[chip]=1;
				}
			}
			applog(LOG_NOTICE, "Board[%u].number_of_asics=%u", chain_id, boards[chain_id].number_of_asics);
		}
	}

	if(check_asic_fail && (check_asic_times<3))
	{
		applog(LOG_ERR, "Need to recheck asic num...");
		for(chain_id=0; chain_id<CHAINS_MAX; chain_id++)
		{
			boards[chain_id].number_of_asics=0;
			for(chip=0; chip<ASICS_ON_CHAIN_MAX; chip++)
			{
				boards[chain_id].asic_alive[chip]=0;
			}
		}
		check_asic_fail=false;
		goto check_asic_num;
	}

	for(chain_id=0; chain_id<CHAINS_MAX; chain_id++)
	{
		if(boards[chain_id].exist && boards[chain_id].number_of_asics!=ASICS_ON_CHAIN_MAX)
		{
			applog(LOG_NOTICE, "%s: Board[%u] looks bad.", __FUNCTION__, chain_id);
			disable_board(chain_id);
			autotuner->chain_tunable[chain_id]=0;
		}
		else
		{
			applog(LOG_NOTICE, "%s: Board[%u] looks ok.", __FUNCTION__, chain_id);
			autotuner->chain_tunable[chain_id]=1;
			boards[chain_id].hash_rate=1;
		}
	}

	for(chain_id=0; chain_id<CHAINS_MAX; chain_id++)
	{
		if(boards[chain_id].exist)
		{
			set_asic_address_one_chain(chain_id);
		}
	}

	//set baud
	for(chain_id=0; chain_id<CHAINS_MAX; chain_id++)
	{
		if(boards[chain_id].exist)
		{
			set_misc_ctrl_one_chain(chain_id);
		}
	}
	//can be useful somehow, but there is no any handler for `MISC_CONTROL` response
	//case in `get_asic_response()`
	//check_asic_reg_all_chains(0, MISC_CONTROL, 1);
	//cgsleep_ms(IIC_SLEEP);

	for(chain_id=0; chain_id<CHAINS_MAX; chain_id++)
	{
		if(boards[chain_id].exist)
		{
			set_core_temp_ctrl_one_chain(chain_id, TEMP_DIODE_SEL, 0, 0);
		}
	}

	read_temp_thr=calloc(1, sizeof(struct thr_info));
	if(thr_info_create(read_temp_thr, NULL, read_temp_func, read_temp_thr))
	{
		applog(LOG_ERR, "%s: create thread for read_temp_func() failed", __FUNCTION__);
		return 0;
	}
	pthread_detach(read_temp_thr->pth);

	pic_heart_beat_thr=calloc(1, sizeof(struct thr_info));
	if(thr_info_create(pic_heart_beat_thr, NULL, pic_heart_beat_func, pic_heart_beat_thr))
	{
		applog(LOG_ERR, "%s: create thread error for pic_heart_beat_func() failed", __FUNCTION__);
		return 0;
	}
	pthread_detach(pic_heart_beat_thr->pth);

	//WIP NTF
	//stupid blow-off procedure for initial sensor offset calibration
	int sensor_offsets_fd=open("/config/sensor_offsets", O_RDONLY), temp_sensor_blowoff_count=0;
	if(sensor_offsets_fd>(-1))
	{
		for(chain_id=0; chain_id<CHAINS_MAX; chain_id++)
		{
			read(sensor_offsets_fd, &boards[chain_id].temperature.offset, 1);
			applog(LOG_NOTICE, "boards[%u] temperature sensor offset=%i", chain_id, boards[chain_id].temperature.offset);
			if(boards[chain_id].temperature.offset==0 && boards[chain_id].exist)
			{
				close(sensor_offsets_fd);
				goto blow_off;
			}
		}
		close(sensor_offsets_fd);
	}
	else
	{
		blow_off:
		apply_pwm_setting((FAN_PWM_MAX_VALUE+FAN_PWM_MIN_VALUE)/2);
		while(++temp_sensor_blowoff_count<90)
		{
			applog(LOG_INFO, "%s: blow off...", __FUNCTION__);
			sleep(1);
		}
		sensor_offsets_fd=open("/config/sensor_offsets", O_WRONLY | O_CREAT, 0644);
		for(chain_id=0; chain_id<CHAINS_MAX; chain_id++)
		{
			write(sensor_offsets_fd, &boards[chain_id].temperature.offset, 1);
		}
		close(sensor_offsets_fd);
	}
	g_check_temp_sensor_values=2;
	//==============================

	// set voltage and frequency
	autotuner_init();

	//open block
	applog(LOG_NOTICE, "send cmd to open block");
	for(chain_id=0; chain_id<CHAINS_MAX; chain_id++)
	{
		if(boards[chain_id].exist)
		{
			set_core_ctrl_one_chain(chain_id, CLOCK_EN_CTRL, 0, 1);
		}
	}

	for(chain_id=0; chain_id<CHAINS_MAX; chain_id++)
	{
		if(boards[chain_id].exist)
		{
			set_asic_ticket_mask_one_chain(chain_id, (1<<DEVICE_DIFF)-1);
		}
	}

	check_fan_thr=calloc(1, sizeof(struct thr_info));
	if(thr_info_create(check_fan_thr, NULL, check_fan_func, check_fan_thr))
	{
		applog(LOG_ERR, "%s: create thread for check_fan_func() failed", __FUNCTION__);
		return 0;
	}
	pthread_detach(check_fan_thr->pth);

	autotuner_thr=calloc(1, sizeof(struct thr_info));
	if(thr_info_create(autotuner_thr, NULL, autotuner_func, autotuner_thr))
	{
		applog(LOG_ERR, "%s: create thread for autotuner_func() failed", __FUNCTION__);
		return 0;
	}
	pthread_detach(autotuner_thr->pth);

	check_miner_status_thr=calloc(1, sizeof(struct thr_info));
	if(thr_info_create(check_miner_status_thr, NULL, check_miner_status_func, check_miner_status_thr))
	{
		applog(LOG_ERR, "%s: create thread for check_miner_status_func() failed", __FUNCTION__);
		return 0;
	}
	pthread_detach(check_miner_status_thr->pth);

	read_hash_rate_thr=calloc(1, sizeof(struct thr_info));
	if(thr_info_create(read_hash_rate_thr, NULL, read_hash_rate_func, read_hash_rate_thr))
	{
		applog(LOG_ERR, "%s: create thread for read_hash_rate_func() failed", __FUNCTION__);
		return 0;
	}
	pthread_detach(read_hash_rate_thr->pth);

	return true;
}

static void bitmain_L3_reinit_device(struct cgpu_info *bitmain)
{
	system("/etc/init.d/cgminer.sh restart > /dev/null 2 > &1 &");
}

static void bitmain_L3_update_work(struct cgpu_info *bitmain)
{
	uint8_t chain;
	for(chain=0; chain<CHAINS_MAX; chain++)
    {
		boards[chain].new_block=true;
    }
}

static void bitmain_L3_detect(bool hotplug)
{
	uint8_t chain_id;
	struct cgpu_info *cgpu=calloc(1, sizeof(struct cgpu_info));
	if(!cgpu)
	{
		quit(1, "Failed to calloc 'cgpu'");
	}

	device=calloc(1, sizeof(device_t));
	if(!device)
	{
		quit(1, "Failed to calloc 'device'");
	}

	boards=calloc(CHAINS_MAX, sizeof(board_parameters_t));
	if(!boards)
	{
		quit(1, "Failed to calloc 'boards'");
	}
	for(chain_id=0; chain_id<CHAINS_MAX; chain_id++)
	{
		pthread_mutex_init(&boards[chain_id].tty_rw_mutex, NULL);
	}

	autotuner=calloc(1, sizeof(autotuner_t));
	if(!autotuner)
	{
		quit(1, "Failed to calloc 'autotuner'");
	}

	cgpu->drv=&antminer_l3_drv;
	cgpu->deven=DEV_ENABLED;
	cgpu->threads=1;
	add_cgpu(cgpu);
}

static void bitmain_L3_shutdown(struct thr_info *thr)
{
	uint8_t chain_id;
	autotuner->fan_pid_control_enabled=0;
	apply_pwm_setting(FAN_PWM_MIN_VALUE);
	for(chain_id=0; chain_id<CHAINS_MAX; ++chain_id)
	{
		if(boards[chain_id].exist)
		{
			disable_board(chain_id);
		}
	}
	thr_info_cancel(check_miner_status_thr);
	thr_info_cancel(read_temp_thr);
	thr_info_cancel(read_hash_rate_thr);
	thr_info_cancel(pic_heart_beat_thr);
}

static struct api_data *bitmain_api_stats(struct cgpu_info *cgpu)
{
	unsigned int chain, asic;
	char field_name_str[32], board_data[768];
	struct api_data *root=NULL;

	root=api_add_uint8(root, "board_num", &(device->number_of_chains), false);

	for(chain=0; chain<CHAINS_MAX; chain++)
	{
		if(boards[chain].exist)
		{
			char chip_hr_str[16];
			sprintf(field_name_str, "board%u_chip_mhs", chain+1);
			sprintf(board_data, "[%.3f", ((float)boards[chain].asic_hash_rate[0])/1000000.0);

			for(asic=1; asic<ASICS_ON_CHAIN_MAX; asic++)
			{
				sprintf(chip_hr_str, " %.3f", ((float)boards[chain].asic_hash_rate[asic])/1000000.0);
				strcat(board_data, chip_hr_str);
			}
			strcat(board_data, "]");
			root=api_add_string(root, field_name_str, board_data, true);
		}
	}

	for(chain=0; chain<CHAINS_MAX; chain++)
	{
		if(boards[chain].exist)
		{
			char chip_hr_str[16];
			sprintf(field_name_str, "board%u_chip_hwe", chain+1);
			sprintf(board_data, "[%u", boards[chain].asic_hwe_count[0]);
			for(asic=1; asic<ASICS_ON_CHAIN_MAX; asic++)
			{
				sprintf(chip_hr_str, " %u", boards[chain].asic_hwe_count[asic]);
				strcat(board_data, chip_hr_str);
			}
			strcat(board_data, "]");
			root=api_add_string(root, field_name_str, board_data, true);
		}
	}

	for(chain=0; chain<CHAINS_MAX; chain++)
	{
		if(boards[chain].exist)
		{
			double chmhs=boards[chain].hash_rate/1000000.0;
			sprintf(field_name_str, "board%u_mhs", chain+1);
			root=api_add_mhs(root, field_name_str, &chmhs, true);
		}
	}

	for(chain=0; chain<CHAINS_MAX; chain++)
	{
		if(boards[chain].exist)
		{
			sprintf(field_name_str, "board%u_frequency", chain+1);
			root=api_add_uint32(root, field_name_str, &(boards[chain].freq), false);
		}
	}

	float chain_voltage, chain_power;
	for(chain=0; chain<CHAINS_MAX; chain++)
	{
		if(boards[chain].exist)
		{
			chain_voltage=volt_pic_val_to_real_val(boards[chain].volt);
			sprintf(field_name_str, "board%u_voltage", chain+1);
			root=api_add_volts(root, field_name_str, &chain_voltage, true);
		}
	}

	for(chain=0; chain<CHAINS_MAX; chain++)
	{
		if(boards[chain].exist)
		{
			chain_voltage=volt_pic_val_to_real_val(boards[chain].volt);
			chain_power=asic_power_estimate(boards[chain].freq, chain_voltage/(float)DOMAIN_SIZE)*ASICS_ON_CHAIN_MAX;
			sprintf(field_name_str, "board%u_power", chain+1);
			root=api_add_volts(root, field_name_str, &chain_power, true);
		}
	}

	for(chain=0; chain<CHAINS_MAX; chain++)
	{
		if(boards[chain].exist)
		{
			sprintf(field_name_str, "board%u_hwe", chain+1);
			root=api_add_uint32(root, field_name_str, &(boards[chain].hwe_count), false);
		}
	}

	root=api_add_uint8(root, "fan_num", &(device->fan_num), false);
	for(chain=0; chain<FANS_MAX; chain++)
    {
		sprintf(field_name_str, "fan%u", chain+1);
		root=api_add_uint16(root, field_name_str, &(device->fan_speed_value[chain]), false);
    }

	for(chain=0; chain<CHAINS_MAX; chain++)
	{
		if(boards[chain].exist)
		{
			sprintf(field_name_str, "board%u_temperature", chain+1);
			root=api_add_temp(root, field_name_str, &(boards[chain].temperature.ext_sensor_value), false);
		}
	}

	root=api_add_temp(root, "temp_max", &device->temperature_max, false);
	uint32_t n_summ=device->valid_nonce_count+device->hwe_count;
	double hwp=n_summ ? (double)(device->hwe_count) / (double)(n_summ):0;
	root=api_add_percent(root, "hwe_percentage", &hwp, true);
	root=api_add_uint32(root, "hwe_total", &device->hwe_count, false);

	for(chain=0; chain<CHAINS_MAX; chain++)
    {
		if(boards[chain].exist)
		{
			sprintf(field_name_str, "board%u_acn", chain+1);
			root=api_add_uint8(root, field_name_str, &(boards[chain].number_of_asics), false);
		}
    }

    return root;
}

struct device_drv antminer_l3_drv=
{
	.drv_id=DRIVER_antminer_l3,
	.dname="Bitmain_L3",
	.name="L3",
	.drv_detect=bitmain_L3_detect,
	.hash_work=&hash_driver_work,
	.scanwork=bitmain_L3_scanhash,
	.update_work=bitmain_L3_update_work,
	.get_api_stats=bitmain_api_stats,
	.reinit_device=bitmain_L3_reinit_device,
	.thread_prepare=bitmain_L3_prepare,
	.thread_shutdown=bitmain_L3_shutdown
};
