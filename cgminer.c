/*
 * Copyright 2011-2017 Con Kolivas
 * Copyright 2011-2015 Andrew Smith
 * Copyright 2011-2012 Luke Dashjr
 * Copyright 2010 Jeff Garzik
 *
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <libgen.h>

#include "cgminer.h"
#include "opt/opt.h"
#include "sha2.h"
#include "bench_block.h"

#ifdef USE_LIBSYSTEMD
#include <systemd/sd-daemon.h>
#endif

#if defined(unix) || defined(__APPLE__)
	#include <errno.h>
	#include <fcntl.h>
	#include <sys/wait.h>
#endif

#if defined(USE_AVALON2)
#include "driver-avalon-2.h"
#define USE_FPGA
#endif

#if defined(USE_AVALON4)
#include "driver-avalon-4.h"
#endif

#if defined(USE_AVALON7)
#include "driver-avalon-7.h"
#include "libssplus.h"
#endif

#if defined(USE_AVALON8)
#include "driver-avalon-8.h"
#endif

#if defined(USE_AVALON9)
#include "driver-avalon-9.h"
#endif

#if defined(USE_ANTMINER_S9)
#include "driver-bitmain-c5.h"
#endif

#if defined(USE_ANTMINER_T9)
#include "driver-bitmain-c5.h"
#endif

#if defined(USE_ANTMINER_L3)
#include "driver-bitmain-l3.h"
#include "scrypt.h"
#endif

#if defined(USE_S19XPH_DRIVER)
#include "driver-s19xph.h"
#endif

#if defined(USE_S21_DRIVER)
#include "driver-s21.h"
#endif

struct strategies strategies[]=
{
	{ "Failover" },
	{ "Round Robin" },
	{ "Rotate" },
	{ "Load Balance" },
	{ "Balance" },
};

static char packagename[256];

bool opt_work_update;
bool opt_clean_jobs=false;
bool opt_protocol;
enum benchwork
{
	BENCHWORK_VERSION=0,
	BENCHWORK_MERKLEROOT,
	BENCHWORK_PREVHASH,
	BENCHWORK_DIFFBITS,
	BENCHWORK_NONCETIME,
	BENCHWORK_COUNT
};

static bool opt_benchmark;
bool have_longpoll;
bool want_per_device_stats;
bool opt_quiet;
bool opt_realquiet;
bool opt_loginput;
bool opt_compact;
bool opt_decode;
int opt_cutofftemp=95;
int opt_log_interval=5;
int opt_pool_fallback=120;
static const int max_queue=1;
int max_scantime=60;
const int max_expiry=600;
unsigned long global_quota_gcd=1;
time_t last_getwork;
json_t *config_json_data=NULL;

#if defined(USE_USBUTILS)
int nDevs;
#endif
bool opt_restart=true;
bool opt_nogpu;

struct list_head scan_devices;
int total_devices;
int zombie_devs;
static int most_devices;
struct cgpu_info **devices;
int mining_threads;
int num_processors;
static bool opt_widescreen;
static bool alt_status;
static bool switch_status;
static bool opt_submit_stale=true;
static int opt_shares;
static bool opt_fix_protocol;
bool opt_lowmem;
bool opt_autofan;
bool opt_autoengine;
bool opt_noadl;
char *opt_api_allow=NULL;
char *opt_api_groups;
char *opt_api_description=PACKAGE_STRING;
int opt_api_port=4028;
char *opt_api_host=API_LISTEN_ADDR;
bool opt_api_listen;
bool opt_api_mcast;
char *opt_api_mcast_addr=API_MCAST_ADDR;
char *opt_api_mcast_code=API_MCAST_CODE;
char *opt_api_mcast_des="";
int opt_api_mcast_port=4028;
bool opt_api_network;
bool opt_delaynet;
bool opt_disable_pool;
static bool no_work;
bool opt_worktime;
#ifdef USE_AVALON2
static char *opt_set_avalon2_freq;
static char *opt_set_avalon2_fan;
static char *opt_set_avalon2_voltage;
#endif
#ifdef USE_AVALON4
static char *opt_set_avalon4_fan;
static char *opt_set_avalon4_voltage;
static char *opt_set_avalon4_freq;
#endif
#ifdef USE_AVALON7
static char *opt_set_avalon7_fan;
static char *opt_set_avalon7_voltage;
static char *opt_set_avalon7_voltage_level;
static char *opt_set_avalon7_voltage_offset;
static char *opt_set_avalon7_freq;
#endif
#ifdef USE_AVALON8
static char *opt_set_avalon8_fan;
static char *opt_set_avalon8_voltage_level;
static char *opt_set_avalon8_voltage_level_offset;
static char *opt_set_avalon8_freq;
static char *opt_set_avalon8_asic_otp;
#endif
#ifdef USE_AVALON9
static char *opt_set_avalon9_fan;
static char *opt_set_avalon9_voltage_level;
static char *opt_set_avalon9_voltage_level_offset;
static char *opt_set_avalon9_freq;
static char *opt_set_avalon9_asic_otp;
static char *opt_set_avalon9_adjust_volt_info;
static char *opt_set_avalon9_adjust_freq_info;
#endif
static char *opt_set_null;
#ifdef USE_USBUTILS
char *opt_usb_select=NULL;
int opt_usbdump=-1;
bool opt_usb_list_all;
cgsem_t usb_resource_sem;
static pthread_t usb_poll_thread;
static bool usb_polling;
static bool polling_usb;
static bool usb_reinit;
#endif

char *opt_kernel_path;
char *cgminer_path;
bool opt_gen_stratum_work;

#define QUIET	(opt_quiet || opt_realquiet)

struct thr_info *control_thr;
struct thr_info **mining_thr;
static int gwsched_thr_id;
static int watchpool_thr_id;
static int watchdog_thr_id;
int gpur_thr_id;
static int api_thr_id;
#ifdef USE_USBUTILS
static int usbres_thr_id;
static int hotplug_thr_id;
#endif
static int total_control_threads;
bool hotplug_mode;
static int new_devices;
static int new_threads;
int hotplug_time=5;

#if LOCK_TRACKING
pthread_mutex_t lockstat_lock;
#endif

pthread_mutex_t hash_lock;
static pthread_mutex_t *stgd_lock;
pthread_mutex_t console_lock;
cglock_t ch_lock;
static pthread_rwlock_t blk_lock;
static pthread_mutex_t sshare_lock;

pthread_rwlock_t netacc_lock;
pthread_rwlock_t mining_thr_lock;
pthread_rwlock_t devices_lock;

static pthread_mutex_t lp_lock;
static pthread_cond_t lp_cond;

pthread_mutex_t restart_lock;
pthread_cond_t restart_cond;

pthread_cond_t gws_cond;

double g_hr_rolling1m, g_hr_rolling5m, g_hr_rolling15m;
double total_ghashes_done;
static struct timeval total_tv_start, total_tv_end;
static struct timeval restart_tv_start, update_tv_start;

cglock_t control_lock;
pthread_mutex_t stats_lock;

int hw_errors;
int64_t total_accepted, total_rejected, total_diff1;
int64_t total_getworks, total_stale, total_discarded;
int64_t total_diff_accepted, total_diff_rejected, total_diff_stale;
static int staged_rollable;
unsigned int new_blocks;
static unsigned int work_block;
unsigned int found_blocks;

unsigned int local_work;
unsigned int total_go, total_ro;

struct pool **pools;
static struct pool *currentpool=NULL;

int total_pools, enabled_pools;
enum pool_strategy pool_strategy=POOL_FAILOVER;
int opt_rotate_period;
static int total_urls, total_users, total_passes, total_userpasses, total_extranonce;

/* Protected by ch_lock */
char current_hash[68];
static char prev_block[12];
static char current_block[32];

static char datestamp[40];
static char blocktime[32];
struct timeval block_timeval;
static char best_share[8]="0";
static char block_diff[8];
int64_t current_diff=0xFFFFFFFFFFFFFFFFULL;
int64_t best_diff=0;

struct block
{
	char hash[68];
	UT_hash_handle hh;
	int block_no;
};

static struct block *blocks=NULL;


int swork_id;

/* For creating a hash database of stratum shares submitted that have not had
 * a response yet */
struct stratum_share
{
	UT_hash_handle hh;
	bool block;
	int id;
	struct work *work;
	struct timeval sshare_sent;
	time_t sshare_time;
};

static struct stratum_share *stratum_shares=NULL;

char *opt_socks_proxy=NULL;
int opt_suggest_diff;
int opt_force_clean_jobs=20;
static const char def_conf[]="cgminer.conf";
static char *default_config;
static bool config_loaded;
static int include_count;
#define JSON_INCLUDE_CONF "include"
#define JSON_LOAD_ERROR "JSON decode of file '%s' failed\n %s"
#define JSON_LOAD_ERROR_LEN strlen(JSON_LOAD_ERROR)
#define JSON_MAX_DEPTH 10
#define JSON_MAX_DEPTH_ERR "Too many levels of JSON includes (limit 10) or a loop"
#define JSON_WEB_ERROR "WEB config err"

struct sigaction termhandler, inthandler, abrthandler;

struct thread_q *getq;

static uint32_t total_work;
struct work *staged_work=NULL;

struct schedtime
{
	bool enable;
	struct tm tm;
};

struct schedtime schedstart;
struct schedtime schedstop;
bool sched_paused;

static bool time_before(struct tm *tm1, struct tm *tm2)
{
	if(tm1->tm_hour<tm2->tm_hour)
		return true;
	if(tm1->tm_hour==tm2->tm_hour && tm1->tm_min<tm2->tm_min)
		return true;
	return false;
}

static bool should_run(void)
{
	struct timeval tv;
	struct tm *tm;

	if(!schedstart.enable && !schedstop.enable)
		return true;

	cgtime(&tv);
	const time_t tmp_time=tv.tv_sec;
	tm=localtime(&tmp_time);
	if(schedstart.enable) {
		if(!schedstop.enable) {
			if(time_before(tm, &schedstart.tm))
				return false;

			/* This is a once off event with no stop time set */
			schedstart.enable=false;
			return true;
		}
		if(time_before(&schedstart.tm, &schedstop.tm)) {
			if(time_before(tm, &schedstop.tm) && !time_before(tm, &schedstart.tm))
				return true;
			return false;
		} /* Times are reversed */
		if(time_before(tm, &schedstart.tm)) {
			if(time_before(tm, &schedstop.tm))
				return true;
			return false;
		}
		return true;
	}
	/* only schedstop.enable==true */
	if(!time_before(tm, &schedstop.tm))
		return false;
	return true;
}

void get_datestamp(char *f, size_t fsiz, struct timeval *tv)
{
	struct tm *tm;

	const time_t tmp_time=tv->tv_sec;
	int ms=(int)(tv->tv_usec/1000);
	tm=localtime(&tmp_time);
	snprintf(f, fsiz, "[%d-%02d-%02d %02d:%02d:%02d.%03d]",
		tm->tm_year + 1900,
		tm->tm_mon + 1,
		tm->tm_mday,
		tm->tm_hour,
		tm->tm_min,
		tm->tm_sec, ms);
}

static void get_timestamp(char *f, size_t fsiz, struct timeval *tv)
{
	struct tm *tm;

	const time_t tmp_time=tv->tv_sec;
	int ms=(int)(tv->tv_usec/1000);
	tm=localtime(&tmp_time);
	snprintf(f, fsiz, "[%02d:%02d:%02d.%03d]",
		tm->tm_hour,
		tm->tm_min,
		tm->tm_sec, ms);
}

static char exit_buf[512];

static void applog_and_exit(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(exit_buf, sizeof(exit_buf), fmt, ap);
	va_end(ap);
	_applog(LOG_ERR, exit_buf);
	exit(1);
}

static pthread_mutex_t sharelog_lock;
static FILE *sharelog_file=NULL;

static struct thr_info *__get_thread(int thr_id)
{
	return mining_thr[thr_id];
}

struct thr_info *get_thread(int thr_id)
{
	struct thr_info *thr;

	rd_lock(&mining_thr_lock);
	thr=__get_thread(thr_id);
	rd_unlock(&mining_thr_lock);

	return thr;
}

static struct cgpu_info *get_thr_cgpu(int thr_id)
{
	struct thr_info *thr=get_thread(thr_id);

	return thr->cgpu;
}

struct cgpu_info *get_devices(int id)
{
	struct cgpu_info *cgpu;

	rd_lock(&devices_lock);
	cgpu=devices[id];
	rd_unlock(&devices_lock);

	return cgpu;
}

static void sharelog(const char *disposition, const struct work*work)
{
	char *target, *hash, *data;
	struct cgpu_info *cgpu;
	unsigned long int t;
	struct pool *pool;
	int thr_id, rv;
	char s[1024];
	size_t ret;

	if(!sharelog_file)
		return;

	thr_id=work->thr_id;
	cgpu=get_thr_cgpu(thr_id);
	pool=work->pool;
	t=(unsigned long int)(work->tv_work_found.tv_sec);
	target=bin2hex(work->target, sizeof(work->target));
	hash=bin2hex(work->hash, sizeof(work->hash));
	data=bin2hex(work->data, sizeof(work->data));

	// timestamp,disposition,target,pool,dev,thr,sharehash,sharedata
	rv=snprintf(s, sizeof(s), "%lu,%s,%s,%s,%s%u,%u,%s,%s\n", t, disposition, target, pool->rpc_url, cgpu->drv->name, cgpu->device_id, thr_id, hash, data);
	free(target);
	free(hash);
	free(data);
	if(rv >= (int)(sizeof(s)))
		s[sizeof(s) - 1]='\0';
	else if(rv<0) {
		applog(LOG_ERR, "sharelog printf error");
		return;
	}

	mutex_lock(&sharelog_lock);
	ret=fwrite(s, rv, 1, sharelog_file);
	fflush(sharelog_file);
	mutex_unlock(&sharelog_lock);

	if(ret != 1)
		applog(LOG_ERR, "sharelog fwrite error");
}

static char *gbt_req="{\"id\": 0, \"method\": \"getblocktemplate\", \"params\": [{\"capabilities\": [\"coinbasetxn\", \"workid\", \"coinbase/append\"]}]}\n";
static const char *gbt_solo_understood_rules[2]={"segwit", NULL};

static bool gbt_check_required_rule(const char *rule, const char **understood_rules)
{
	const char *understood_rule;

	if(!understood_rules || !rule)
		return false;
	while((understood_rule=*understood_rules++)) {
		if(strcmp(understood_rule, rule)==0)
			return true;
	}
	return false;
}

static bool gbt_check_rules(json_t* rules_arr, const char **understood_rules)
{
	int i, rule_count;
	const char *rule;

	if(!rules_arr)
		return true;
	rule_count=json_array_size(rules_arr);
	for(i=0; i<rule_count; i++) {
		rule=json_string_value(json_array_get(rules_arr, i));
		if(rule && *rule++=='!' && !gbt_check_required_rule(rule, understood_rules))
			return false;
	}
	return true;
}

/* Adjust all the pools' quota to the greatest common denominator after a pool
 * has been added or the quotas changed. */
void adjust_quota_gcd(void)
{
	unsigned long gcd, lowest_quota=~0UL, quota;
	struct pool *pool;
	int i;

	for(i=0; i<total_pools; i++) {
		pool=pools[i];
		quota=pool->quota;
		if(!quota)
			continue;
		if(quota<lowest_quota)
			lowest_quota=quota;
	}

	if((lowest_quota<~0UL)) {
		gcd=lowest_quota;
		for(i=0; i<total_pools; i++) {
			pool=pools[i];
			quota=pool->quota;
			if(!quota)
				continue;
			while(quota % gcd)
				gcd--;
		}
	} else
		gcd=1;

	for(i=0; i<total_pools; i++) {
		pool=pools[i];
		pool->quota_used *= global_quota_gcd;
		pool->quota_used /= gcd;
		pool->quota_gcd=pool->quota/gcd;
	}

	global_quota_gcd=gcd;
	applog(LOG_DEBUG, "Global quota greatest common denominator set to %lu", gcd);
}

/* Return value is ignored if not called from input_pool */
struct pool *add_pool(void)
{
	struct pool *pool;
	pool=cgcalloc(sizeof(struct pool), 1);
	pool->pool_no=pool->prio=total_pools;
#if defined(ENABLE_ASIC_BOOST)
	pool->supports_version_rolling=false;
#endif
	pools=cgrealloc(pools, sizeof(struct pool *) * (total_pools + 2));
	pools[total_pools++]=pool;
	mutex_init(&pool->pool_lock);
	if((pthread_cond_init(&pool->cr_cond, NULL)))
	{
		quit(1, "Failed to pthread_cond_init in add_pool");
	}
	cglock_init(&pool->data_lock);
	mutex_init(&pool->stratum_lock);
	INIT_LIST_HEAD(&pool->curlring);

	/* Make sure the pool doesn't think we've been idle since time 0 */
	pool->tv_idle.tv_sec=~0UL;

	pool->rpc_req=gbt_req;
	pool->rpc_proxy=NULL;
	pool->quota=1;
	adjust_quota_gcd();
	pool->extranonce_subscribe=false;

	return pool;
}

/* Pool variant of test and set */
static bool pool_tset(struct pool *pool, bool *var)
{
	bool ret;

	mutex_lock(&pool->pool_lock);
	ret=*var;
	*var=true;
	mutex_unlock(&pool->pool_lock);

	return ret;
}

bool pool_tclear(struct pool *pool, bool *var)
{
	bool ret;

	mutex_lock(&pool->pool_lock);
	ret=*var;
	*var=false;
	mutex_unlock(&pool->pool_lock);

	return ret;
}

struct pool *current_pool(void)
{
	struct pool *pool;

	cg_rlock(&control_lock);
	pool=currentpool;
	cg_runlock(&control_lock);

	return pool;
}

char *set_int_range(const char *arg, int *i, int min, int max)
{
	int tval;
	char *err=opt_set_intval(arg, &tval);
	if(err)
	{
		return err;
	}
	if(tval<min)
	{
		tval=min;
	}
	if(tval>max)
	{
		tval=max;
	}
	*i=tval;
	return NULL;
}

static char *set_int_0_to_65535(const char *arg, int *i)
{
	return set_int_range(arg, i, 0, 65535);
}

static char *set_int_1_to_65535(const char *arg, int *i)
{
	return set_int_range(arg, i, 1, 65535);
}

static char *set_int_0_to_9999(const char *arg, int *i)
{
	return set_int_range(arg, i, 0, 9999);
}

#if defined(USE_AVALON8) || defined(USE_AVALON9)
static char *set_int_0_to_1(const char *arg, int *i)
{
	return set_int_range(arg, i, 0, 1);
}

static char *set_int_0_to_2(const char *arg, int *i)
{
	return set_int_range(arg, i, 0, 2);
}

static char *set_int_0_to_3(const char *arg, int *i)
{
	return set_int_range(arg, i, 0, 3);
}
#endif
#if defined(USE_AVALON7)
static char *set_int_0_to_5(const char *arg, int *i)
{
	return set_int_range(arg, i, 0, 5);
}
#endif
static char *set_int_0_to_10(const char *arg, int *i)
{
	return set_int_range(arg, i, 0, 10);
}

static char *set_int_0_to_100(const char *arg, int *i)
{
	return set_int_range(arg, i, 0, 100);
}

static char *set_int_0_to_255(const char *arg, int *i)
{
	return set_int_range(arg, i, 0, 255);
}

static char *set_int_0_to_7680(const char *arg, int *i)
{
	return set_int_range(arg, i, 0, 7680);
}

#if defined(USE_AVALON4)
static char *set_int_1_to_255(const char *arg, int *i)
{
	return set_int_range(arg, i, 1, 255);
}

static char *set_int_1_to_60(const char *arg, int *i)
{
	return set_int_range(arg, i, 1, 60);
}

static char *set_int_22_to_75(const char *arg, int *i)
{
	return set_int_range(arg, i, 22, 75);
}

static char *set_int_42_to_85(const char *arg, int *i)
{
	return set_int_range(arg, i, 42, 85);
}
#endif

static char *set_int_0_to_200(const char *arg, int *i)
{
	return set_int_range(arg, i, 0, 200);
}

static char *set_int_24_to_32(const char *arg, int *i)
{
	return set_int_range(arg, i, 24, 32);
}

static char *set_int_0_to_4(const char *arg, int *i)
{
	return set_int_range(arg, i, 0, 4);
}

#ifdef USE_FPGA_SERIAL
static char *opt_add_serial;
static char *add_serial(char *arg)
{
	string_elist_add(arg, &scan_devices);
	return NULL;
}
#endif

void get_intrange(char *arg, int *val1, int *val2)
{
	if(sscanf(arg, "%d-%d", val1, val2)==1)
		*val2=*val1;
}

static char *set_balance(enum pool_strategy *strategy)
{
	*strategy=POOL_BALANCE;
	return NULL;
}

static char *set_loadbalance(enum pool_strategy *strategy)
{
	*strategy=POOL_LOADBALANCE;
	return NULL;
}

static char *set_rotate(const char *arg, char *i)
{
	pool_strategy=POOL_ROTATE;
	return set_int_range(arg, &opt_rotate_period, 0, 9999);
}

static char *set_rr(enum pool_strategy *strategy)
{
	*strategy=POOL_ROUNDROBIN;
	return NULL;
}

/* Detect that url is for a stratum protocol either via the presence of
 * stratum+tcp or by detecting a stratum server response */
bool detect_stratum(struct pool *pool, char *url)
{
	bool ret=false;

	if(!extract_sockaddr(url, &pool->sockaddr_url, &pool->stratum_port))
		goto out;

	if(!strncasecmp(url, "stratum+tcp://", 14)) {
		pool->rpc_url=strdup(url);
		pool->has_stratum=true;
		pool->stratum_url=pool->sockaddr_url;
		ret=true;
	}
out:
	if(!ret) {
		free(pool->sockaddr_url);
		free(pool->stratum_port);
		pool->stratum_port=pool->sockaddr_url=NULL;
	}
	return ret;
}

static struct pool *add_url(void)
{
	total_urls++;
	if(total_urls>total_pools)
		add_pool();
	return pools[total_urls - 1];
}

static char *setup_url(struct pool *pool, char *arg)
{
	arg=get_proxy(arg, pool);
	if(detect_stratum(pool, arg))
	{
		goto out;
	}
	char httpinput[256];
	opt_set_charp(arg, &pool->rpc_url);
	if(strncmp(arg, "http://", 7) && strncmp(arg, "https://", 8))
	{
		strcpy(httpinput, "stratum+tcp://");
		strncat(httpinput, arg, 241);
		detect_stratum(pool, httpinput);
	}
out:
	return pool->rpc_url;
}

static char *set_url(char *arg)
{
	struct pool *pool=add_url();

	setup_url(pool, arg);
	if(strstr(pool->rpc_url, "#xnsub"))
	{
		pool->extranonce_subscribe=true;
		applog(LOG_DEBUG, "Pool %d extranonce subscribing enabled.", pool->pool_no);
	}
	return NULL;
}

static char *set_quota(char *arg)
{
	char *semicolon=strchr(arg, ';'), *url;
	int len, qlen, quota;
	struct pool *pool;

	if(!semicolon)
		return "No semicolon separated quota;URL pair found";
	len=strlen(arg);
	*semicolon='\0';
	qlen=strlen(arg);
	if(!qlen)
		return "No parameter for quota found";
	len -= qlen + 1;
	if(len<1)
		return "No parameter for URL found";
	quota=atoi(arg);
	if(quota<0)
		return "Invalid negative parameter for quota set";
	url=arg + qlen + 1;
	pool=add_url();
	setup_url(pool, url);
	pool->quota=quota;
	applog(LOG_INFO, "Setting pool %d to quota %d", pool->pool_no, pool->quota);
	adjust_quota_gcd();

	return NULL;
}

static char *set_user(const char *arg)
{
	struct pool *pool;

	if(total_userpasses)
		return "Use only user + pass or userpass, but not both";
	total_users++;
	if(total_users>total_pools)
		add_pool();

	pool=pools[total_users - 1];
	opt_set_charp(arg, &pool->rpc_user);

	return NULL;
}

static char *set_pass(const char *arg)
{
	struct pool *pool;

	if(total_userpasses)
		return "Use only user + pass or userpass, but not both";
	total_passes++;
	if(total_passes>total_pools)
		add_pool();

	pool=pools[total_passes - 1];
	opt_set_charp(arg, &pool->rpc_pass);

	return NULL;
}

static char *set_userpass(const char *arg)
{
	struct pool *pool;
	char *updup;

	if(total_users || total_passes)
		return "Use only user + pass or userpass, but not both";
	total_userpasses++;
	if(total_userpasses>total_pools)
		add_pool();

	pool=pools[total_userpasses - 1];
	updup=strdup(arg);
	opt_set_charp(arg, &pool->rpc_userpass);
	pool->rpc_user=strtok(updup, ":");
	if(!pool->rpc_user)
		return "Failed to find : delimited user info";
	pool->rpc_pass=strtok(NULL, ":");
	if(!pool->rpc_pass)
		pool->rpc_pass=strdup("");

	return NULL;
}

static char *set_extranonce_subscribe(char *arg)
 {
    struct pool *pool;
    total_extranonce++;
	if(total_extranonce>total_pools)
	{
		add_pool();
	}
	pool=pools[total_extranonce - 1];
    applog(LOG_DEBUG, "Enable extranonce subscribe on %d", pool->pool_no);
    opt_set_bool(&pool->extranonce_subscribe);
    return NULL;
 }

static char *enable_debug(bool *flag)
{
	*flag=true;
	/* Turn on verbose output, too. */
	opt_log_verbose=true;
	return NULL;
}

static char *opt_set_sched_start;
static char *opt_set_sched_stop;

static char *set_schedtime(const char *arg, struct schedtime *st)
{
	if(sscanf(arg, "%d:%d", &st->tm.tm_hour, &st->tm.tm_min) != 2)
		return "Invalid time set, should be HH:MM";
	if(st->tm.tm_hour>23 || st->tm.tm_min>59 || st->tm.tm_hour<0 || st->tm.tm_min<0)
		return "Invalid time set.";
	st->enable=true;
	return NULL;
}

static char *set_sched_start(const char *arg)
{
	return set_schedtime(arg, &schedstart);
}

static char *set_sched_stop(const char *arg)
{
	return set_schedtime(arg, &schedstop);
}

static void load_temp_cutoffs()
{
	int i;
	{
		rd_lock(&devices_lock);
		for(i=0; i<total_devices; ++i)
		{
			devices[i]->cutofftemp=opt_cutofftemp;
		}
		rd_unlock(&devices_lock);
	}
}

static char *set_float_125_to_500(const char *arg, float *i)
{
	char *err=opt_set_floatval(arg, i);

	if(err)
		return err;

	if(*i<125 || *i>500)
		return "Value out of range";

	return NULL;
}

static char *set_float_100_to_250(const char *arg, float *i)
{
	char *err=opt_set_floatval(arg, i);

	if(err)
		return err;

	if(*i<100 || *i>250)
		return "Value out of range";

	return NULL;
}

static char *set_null(const char *arg)
{
	return NULL;
}

/* These options are available from config file or commandline */
static struct opt_table opt_config_table[]=
{
	OPT_WITH_ARG("--api-allow",
			 opt_set_charp, NULL, &opt_api_allow,
			 "Allow API access only to the given list of [G:]IP[/Prefix] addresses[/subnets]"),
	OPT_WITH_ARG("--api-description",
			 opt_set_charp, NULL, &opt_api_description,
			 "Description placed in the API status header, default: cgminer version"),
	OPT_WITH_ARG("--api-groups",
			 opt_set_charp, NULL, &opt_api_groups,
			 "API one letter groups G:cmd:cmd[,P:cmd:*...] defining the cmds a groups can use"),
	OPT_WITHOUT_ARG("--api-listen",
			opt_set_bool, &opt_api_listen,
			"Enable API, default: disabled"),
	OPT_WITHOUT_ARG("--api-mcast",
			opt_set_bool, &opt_api_mcast,
			"Enable API Multicast listener, default: disabled"),
	OPT_WITH_ARG("--api-mcast-addr",
			 opt_set_charp, NULL, &opt_api_mcast_addr,
			 "API Multicast listen address"),
	OPT_WITH_ARG("--api-mcast-code",
			 opt_set_charp, NULL, &opt_api_mcast_code,
			 "Code expected in the API Multicast message, don't use '-'"),
	OPT_WITH_ARG("--api-mcast-des",
			 opt_set_charp, NULL, &opt_api_mcast_des,
			 "Description appended to the API Multicast reply, default: ''"),
	OPT_WITH_ARG("--api-mcast-port",
			 set_int_1_to_65535, opt_show_intval, &opt_api_mcast_port,
			 "API Multicast listen port"),
	OPT_WITHOUT_ARG("--api-network",
			opt_set_bool, &opt_api_network,
			"Allow API (if enabled) to listen on/for any address, default: only 127.0.0.1"),
	OPT_WITH_ARG("--api-port",
			 set_int_1_to_65535, opt_show_intval, &opt_api_port,
			 "Port number of miner API"),
	OPT_WITH_ARG("--api-host",
			 opt_set_charp, NULL, &opt_api_host,
			 "Specify API listen address, default: 0.0.0.0"),
#ifdef USE_AVALON2
	OPT_WITH_CBARG("--avalon2-freq",
			 set_avalon2_freq, NULL, &opt_set_avalon2_freq,
			 "Set frequency range for Avalon2, single value or range, step: 25"),
	OPT_WITH_CBARG("--avalon2-voltage",
			 set_avalon2_voltage, NULL, &opt_set_avalon2_voltage,
			 "Set Avalon2 core voltage, in millivolts, step: 125"),
	OPT_WITH_CBARG("--avalon2-fan",
			 set_avalon2_fan, NULL, &opt_set_avalon2_fan,
			 "Set Avalon2 target fan speed"),
	OPT_WITH_ARG("--avalon2-cutoff",
			 set_int_0_to_100, opt_show_intval, &opt_avalon2_overheat,
			 "Set Avalon2 overheat cut off temperature"),
	OPT_WITHOUT_ARG("--avalon2-fixed-speed",
			 set_avalon2_fixed_speed, &opt_avalon2_fan_fixed,
			 "Set Avalon2 fan to fixed speed"),
	OPT_WITH_ARG("--avalon2-polling-delay",
			 set_int_1_to_65535, opt_show_intval, &opt_avalon2_polling_delay,
			 "Set Avalon2 polling delay value (ms)"),
#endif
#ifdef USE_AVALON4
	OPT_WITHOUT_ARG("--avalon4-automatic-voltage",
			 opt_set_bool, &opt_avalon4_autov,
			 "Automatic adjust voltage base on module DH"),
	OPT_WITH_CBARG("--avalon4-voltage",
			 set_avalon4_voltage, NULL, &opt_set_avalon4_voltage,
			 "Set Avalon4 core voltage, in millivolts, step: 125"),
	OPT_WITH_CBARG("--avalon4-freq",
			 set_avalon4_freq, NULL, &opt_set_avalon4_freq,
			 "Set frequency for Avalon4, 1 to 3 values, example: 445:385:370"),
	OPT_WITH_CBARG("--avalon4-fan",
			 set_avalon4_fan, NULL, &opt_set_avalon4_fan,
			 "Set Avalon4 target fan speed range"),
	OPT_WITH_ARG("--avalon4-temp",
			 set_int_22_to_75, opt_show_intval, &opt_avalon4_temp_target,
			 "Set Avalon4 target temperature"),
	OPT_WITH_ARG("--avalon4-cutoff",
			 set_int_42_to_85, opt_show_intval, &opt_avalon4_overheat,
			 "Set Avalon4 overheat cut off temperature"),
	OPT_WITH_ARG("--avalon4-polling-delay",
			 set_int_1_to_65535, opt_show_intval, &opt_avalon4_polling_delay,
			 "Set Avalon4 polling delay value (ms)"),
	OPT_WITH_ARG("--avalon4-ntime-offset",
			 opt_set_intval, opt_show_intval, &opt_avalon4_ntime_offset,
			 "Set Avalon4 MM ntime rolling max offset"),
	OPT_WITH_ARG("--avalon4-aucspeed",
			 opt_set_intval, opt_show_intval, &opt_avalon4_aucspeed,
			 "Set Avalon4 AUC IIC bus speed"),
	OPT_WITH_ARG("--avalon4-aucxdelay",
			 opt_set_intval, opt_show_intval, &opt_avalon4_aucxdelay,
			 "Set Avalon4 AUC IIC xfer read delay, 4800 ~= 1ms"),
	OPT_WITH_ARG("--avalon4-miningmode",
			 opt_set_intval, opt_show_intval, &opt_avalon4_miningmode,
			 "Set Avalon4 mining mode(0:custom, 1:eco, 2:normal, 3:turbo"),
	OPT_WITHOUT_ARG("--avalon4-freezesafe",
			 opt_set_bool, &opt_avalon4_freezesafe,
			 "Make Avalon4 running as a radiator when stratum server failed"),
	OPT_WITH_ARG("--avalon4-ntcb",
			 opt_set_intval, opt_show_intval, &opt_avalon4_ntcb,
			 "Set Avalon4 MM NTC B value"),
	OPT_WITH_ARG("--avalon4-freq-min",
			 opt_set_intval, opt_show_intval, &opt_avalon4_freq_min,
			 "Set minimum frequency for Avalon4"),
	OPT_WITH_ARG("--avalon4-freq-max",
			 opt_set_intval, opt_show_intval, &opt_avalon4_freq_max,
			 "Set maximum frequency for Avalon4"),
	OPT_WITHOUT_ARG("--avalon4-noncecheck-off",
			 opt_set_invbool, &opt_avalon4_noncecheck,
			 "Disable A3218 inside nonce check function"),
	OPT_WITH_ARG("--avalon4-smart-speed",
			 opt_set_intval, opt_show_intval, &opt_avalon4_smart_speed,
			 "Set smart speed, range 0-3. 0 means Disable"),
	OPT_WITH_ARG("--avalon4-speed-bingo",
			 set_int_1_to_255, opt_show_intval, &opt_avalon4_speed_bingo,
			 "Set A3218 speed bingo for smart speed mode 1"),
	OPT_WITH_ARG("--avalon4-speed-error",
			 set_int_1_to_255, opt_show_intval, &opt_avalon4_speed_error,
			 "Set A3218 speed error for smart speed mode 1"),
	OPT_WITH_ARG("--avalon4-least-pll",
			 set_int_0_to_7680, opt_show_intval, &opt_avalon4_least_pll_check,
			 "Set least pll check threshold for smart speed mode 2"),
	OPT_WITH_ARG("--avalon4-most-pll",
			 set_int_0_to_7680, opt_show_intval, &opt_avalon4_most_pll_check,
			 "Set most pll check threshold for smart speed mode 2"),
	OPT_WITHOUT_ARG("--avalon4-iic-detect",
			 opt_set_bool, &opt_avalon4_iic_detect,
			 "Enable miner detect through iic controller"),
	OPT_WITH_ARG("--avalon4-freqadj-time",
			 set_int_1_to_60, opt_show_intval, &opt_avalon4_freqadj_time,
			 "Set Avalon4 check interval when run in AVA4_FREQ_TEMPADJ_MODE"),
	OPT_WITH_ARG("--avalon4-delta-temp",
			 opt_set_intval, opt_show_intval, &opt_avalon4_delta_temp,
			 "Set Avalon4 delta temperature when reset freq in AVA4_FREQ_TEMPADJ_MODE"),
	OPT_WITH_ARG("--avalon4-delta-freq",
			 opt_set_intval, opt_show_intval, &opt_avalon4_delta_freq,
			 "Set Avalon4 delta freq when adjust freq in AVA4_FREQ_TEMPADJ_MODE"),
	OPT_WITH_ARG("--avalon4-freqadj-temp",
			 opt_set_intval, opt_show_intval, &opt_avalon4_freqadj_temp,
			 "Set Avalon4 check temperature when run into AVA4_FREQ_TEMPADJ_MODE"),
#endif
#ifdef USE_AVALON7
	OPT_WITH_CBARG("--avalon7-voltage",
			 set_avalon7_voltage, NULL, &opt_set_avalon7_voltage,
			 "Set Avalon7 default core voltage, in millivolts, step: 78"),
	OPT_WITH_CBARG("--avalon7-voltage-level",
			 set_avalon7_voltage_level, NULL, &opt_set_avalon7_voltage_level,
			 "Set Avalon7 default level of core voltage, range:[0, 15], step: 1"),
	OPT_WITH_CBARG("--avalon7-voltage-offset",
			 set_avalon7_voltage_offset, NULL, &opt_set_avalon7_voltage_offset,
			 "Set Avalon7 default offset of core voltage, range:[-2, 1], step: 1"),
	OPT_WITH_CBARG("--avalon7-freq",
			 set_avalon7_freq, NULL, &opt_set_avalon7_freq,
			 "Set Avalon7 default frequency, range:[24, 1404], step: 12, example: 500"),
	OPT_WITH_ARG("--avalon7-freq-sel",
			 set_int_0_to_5, opt_show_intval, &opt_avalon7_freq_sel,
			 "Set Avalon7 default frequency select, range:[0, 5], step: 1, example: 3"),
	OPT_WITH_CBARG("--avalon7-fan",
			 set_avalon7_fan, NULL, &opt_set_avalon7_fan,
			 "Set Avalon7 target fan speed, range:[0, 100], step: 1, example: 0-100"),
	OPT_WITH_ARG("--avalon7-temp",
			 set_int_0_to_100, opt_show_intval, &opt_avalon7_temp_target,
			 "Set Avalon7 target temperature, range:[0, 100]"),
	OPT_WITH_ARG("--avalon7-polling-delay",
			 set_int_1_to_65535, opt_show_intval, &opt_avalon7_polling_delay,
			 "Set Avalon7 polling delay value (ms)"),
	OPT_WITH_ARG("--avalon7-aucspeed",
			 opt_set_intval, opt_show_intval, &opt_avalon7_aucspeed,
			 "Set AUC3 IIC bus speed"),
	OPT_WITH_ARG("--avalon7-aucxdelay",
			 opt_set_intval, opt_show_intval, &opt_avalon7_aucxdelay,
			 "Set AUC3 IIC xfer read delay, 4800 ~= 1ms"),
	OPT_WITH_ARG("--avalon7-smart-speed",
			 opt_set_intval, opt_show_intval, &opt_avalon7_smart_speed,
			 "Set Avalon7 smart speed, range 0-1. 0 means Disable"),
	OPT_WITH_ARG("--avalon7-th-pass",
			 set_int_0_to_65535, opt_show_intval, &opt_avalon7_th_pass,
			 "Set A3212 th pass value"),
	OPT_WITH_ARG("--avalon7-th-fail",
			 set_int_0_to_65535, opt_show_intval, &opt_avalon7_th_fail,
			 "Set A3212 th fail value"),
	OPT_WITH_ARG("--avalon7-th-init",
			 set_int_0_to_65535, opt_show_intval, &opt_avalon7_th_init,
			 "Set A3212 th init value"),
	OPT_WITH_ARG("--avalon7-th-ms",
			 set_int_0_to_65535, opt_show_intval, &opt_avalon7_th_ms,
			 "Set A3212 th ms value"),
	OPT_WITH_ARG("--avalon7-th-timeout",
			 opt_set_uintval, opt_show_uintval, &opt_avalon7_th_timeout,
			 "Set A3212 th timeout value"),
	OPT_WITHOUT_ARG("--avalon7-iic-detect",
			 opt_set_bool, &opt_avalon7_iic_detect,
			 "Enable Avalon7 detect through iic controller"),
	OPT_WITH_ARG("--avalon7-nonce-mask",
			 set_int_24_to_32, opt_show_intval, &opt_avalon7_nonce_mask,
			 "Set A3212 nonce mask, range 24-32."),
	OPT_WITHOUT_ARG("--no-avalon7-asic-debug",
			 opt_set_invbool, &opt_avalon7_asic_debug,
			 "Disable A3212 debug."),
	OPT_WITHOUT_ARG("--avalon7-ssplus-enable",
			 opt_set_bool, &opt_avalon7_ssplus_enable,
			 "Enable avalon7 smart speed plus."),
#endif
#ifdef USE_AVALON8
	OPT_WITH_CBARG("--avalon8-voltage-level",
			 set_avalon8_voltage_level, NULL, &opt_set_avalon8_voltage_level,
			 "Set Avalon8 default level of core voltage, range:[0, 15], step: 1"),
	OPT_WITH_CBARG("--avalon8-voltage-level-offset",
			 set_avalon8_voltage_level_offset, NULL, &opt_set_avalon8_voltage_level_offset,
			 "Set Avalon8 default offset of core voltage level, range:[-2, 1], step: 1"),
	OPT_WITH_CBARG("--avalon8-freq",
			 set_avalon8_freq, NULL, &opt_set_avalon8_freq,
			 "Set Avalon8 default frequency, range:[25, 1200], step: 25, example: 800"),
	OPT_WITH_ARG("--avalon8-freq-sel",
			 set_int_0_to_4, opt_show_intval, &opt_avalon8_freq_sel,
			 "Set Avalon8 default frequency select, range:[0, 4], step: 1, example: 3"),
	OPT_WITH_CBARG("--avalon8-fan",
			 set_avalon8_fan, NULL, &opt_set_avalon8_fan,
			 "Set Avalon8 target fan speed, range:[0, 100], step: 1, example: 0-100"),
	OPT_WITH_ARG("--avalon8-temp",
			 set_int_0_to_100, opt_show_intval, &opt_avalon8_temp_target,
			 "Set Avalon8 target temperature, range:[0, 100]"),
	OPT_WITH_ARG("--avalon8-polling-delay",
			 set_int_1_to_65535, opt_show_intval, &opt_avalon8_polling_delay,
			 "Set Avalon8 polling delay value (ms)"),
	OPT_WITH_ARG("--avalon8-aucspeed",
			 opt_set_intval, opt_show_intval, &opt_avalon8_aucspeed,
			 "Set AUC3 IIC bus speed"),
	OPT_WITH_ARG("--avalon8-aucxdelay",
			 opt_set_intval, opt_show_intval, &opt_avalon8_aucxdelay,
			 "Set AUC3 IIC xfer read delay, 4800 ~= 1ms"),
	OPT_WITH_ARG("--avalon8-smart-speed",
			 opt_set_intval, opt_show_intval, &opt_avalon8_smart_speed,
			 "Set Avalon8 smart speed, range 0-1. 0 means Disable"),
	OPT_WITH_ARG("--avalon8-th-pass",
			 set_int_0_to_65535, opt_show_intval, &opt_avalon8_th_pass,
			 "Set A3210 th pass value"),
	OPT_WITH_ARG("--avalon8-th-fail",
			 set_int_0_to_65535, opt_show_intval, &opt_avalon8_th_fail,
			 "Set A3210 th fail value"),
	OPT_WITH_ARG("--avalon8-th-init",
			 set_int_0_to_65535, opt_show_intval, &opt_avalon8_th_init,
			 "Set A3210 th init value"),
	OPT_WITH_ARG("--avalon8-th-ms",
			 set_int_0_to_65535, opt_show_intval, &opt_avalon8_th_ms,
			 "Set A3210 th ms value"),
	OPT_WITH_ARG("--avalon8-th-timeout",
			 opt_set_uintval, opt_show_uintval, &opt_avalon8_th_timeout,
			 "Set A3210 th timeout value"),
	OPT_WITH_ARG("--avalon8-th-add",
			 set_int_0_to_1, opt_show_intval, &opt_avalon8_th_add,
			 "Set A3210 th add value"),
	OPT_WITHOUT_ARG("--avalon8-iic-detect",
			 opt_set_bool, &opt_avalon8_iic_detect,
			 "Enable Avalon8 detect through iic controller"),
	OPT_WITH_ARG("--avalon8-nonce-mask",
			 set_int_24_to_32, opt_show_intval, &opt_avalon8_nonce_mask,
			 "Set A3210 nonce mask, range 24-32."),
	OPT_WITH_ARG("--avalon8-nonce-check",
			 set_int_0_to_1, opt_show_intval, &opt_avalon8_nonce_check,
			 "Set A3210 nonce check, range 0-1."),
	OPT_WITH_ARG("--avalon8-roll-enable",
			 set_int_0_to_1, opt_show_intval, &opt_avalon8_roll_enable,
			 "Set A3210 roll enable, range 0-1."),
	OPT_WITH_ARG("--avalon8-mux-l2h",
			 set_int_0_to_2, opt_show_intval, &opt_avalon8_mux_l2h,
			 "Set Avalon8 mux l2h, range 0-2."),
	OPT_WITH_ARG("--avalon8-mux-h2l",
			 set_int_0_to_1, opt_show_intval, &opt_avalon8_mux_h2l,
			 "Set Avalon8 mux h2l, range 0-1."),
	OPT_WITH_ARG("--avalon8-h2ltime0-spd",
			 set_int_0_to_255, opt_show_intval, &opt_avalon8_h2ltime0_spd,
			 "Set Avalon8 h2ltime0 spd, range 0-255."),
	OPT_WITH_ARG("--avalon8-spdlow",
			 set_int_0_to_3, opt_show_intval, &opt_avalon8_spdlow,
			 "Set Avalon8 spdlow, range 0-3."),
	OPT_WITH_ARG("--avalon8-spdhigh",
			 set_int_0_to_3, opt_show_intval, &opt_avalon8_spdhigh,
			 "Set Avalon8 spdhigh, range 0-3."),
	OPT_WITH_CBARG("--avalon8-cinfo-asic",
			 set_avalon8_asic_otp, NULL, &opt_set_avalon8_asic_otp,
			 "Set Avalon8 cinfo asic index, range:[0, 25], step: 1"),
	OPT_WITH_ARG("--avalon8-pid-p",
			 set_int_0_to_9999, opt_show_intval, &opt_avalon8_pid_p,
			 "Set Avalon8 pid-p, range 0-9999."),
	OPT_WITH_ARG("--avalon8-pid-i",
			 set_int_0_to_9999, opt_show_intval, &opt_avalon8_pid_i,
			 "Set Avalon8 pid-i, range 0-9999."),
	OPT_WITH_ARG("--avalon8-pid-d",
			 set_int_0_to_9999, opt_show_intval, &opt_avalon8_pid_d,
			 "Set Avalon8 pid-d, range 0-9999."),
#endif
#ifdef USE_AVALON9
	OPT_WITH_CBARG("--avalon9-voltage-level",
			 set_avalon9_voltage_level, NULL, &opt_set_avalon9_voltage_level,
			 "Set Avalon9 default level of core voltage, range:[0, 35], step: 1"),
	OPT_WITH_CBARG("--avalon9-voltage-level-offset",
			 set_avalon9_voltage_level_offset, NULL, &opt_set_avalon9_voltage_level_offset,
			 "Set Avalon9 default offset of core voltage level, range:[-2, 1], step: 1"),
	OPT_WITH_CBARG("--avalon9-freq",
			 set_avalon9_freq, NULL, &opt_set_avalon9_freq,
			 "Set Avalon9 default frequency, range:[25, 1200], step: 25, example: 800"),
	OPT_WITH_ARG("--avalon9-freq-sel",
			 set_int_0_to_4, opt_show_intval, &opt_avalon9_freq_sel,
			 "Set Avalon9 default frequency select, range:[0, 4], step: 1, example: 3"),
	OPT_WITH_CBARG("--avalon9-fan",
			 set_avalon9_fan, NULL, &opt_set_avalon9_fan,
			 "Set Avalon9 target fan speed, range:[0, 100], step: 1, example: 0-100"),
	OPT_WITH_ARG("--avalon9-temp",
			 set_int_0_to_100, opt_show_intval, &opt_avalon9_temp_target,
			 "Set Avalon9 target temperature, range:[0, 100]"),
	OPT_WITH_ARG("--avalon9-polling-delay",
			 set_int_1_to_65535, opt_show_intval, &opt_avalon9_polling_delay,
			 "Set Avalon9 polling delay value (ms)"),
	OPT_WITH_ARG("--avalon9-aucspeed",
			 opt_set_intval, opt_show_intval, &opt_avalon9_aucspeed,
			 "Set AUC3 IIC bus speed"),
	OPT_WITH_ARG("--avalon9-aucxdelay",
			 opt_set_intval, opt_show_intval, &opt_avalon9_aucxdelay,
			 "Set AUC3 IIC xfer read delay, 4800 ~= 1ms"),
	OPT_WITH_ARG("--avalon9-smart-speed",
			 opt_set_intval, opt_show_intval, &opt_avalon9_smart_speed,
			 "Set Avalon9 smart speed, range 0-1. 0 means Disable"),
	OPT_WITH_ARG("--avalon9-th-pass",
			 set_int_0_to_65535, opt_show_intval, &opt_avalon9_th_pass,
			 "Set A3210M th pass value"),
	OPT_WITH_ARG("--avalon9-th-fail",
			 set_int_0_to_65535, opt_show_intval, &opt_avalon9_th_fail,
			 "Set A3210M th fail value"),
	OPT_WITH_ARG("--avalon9-th-init",
			 set_int_0_to_65535, opt_show_intval, &opt_avalon9_th_init,
			 "Set A3210M th init value"),
	OPT_WITH_ARG("--avalon9-th-ms",
			 set_int_0_to_65535, opt_show_intval, &opt_avalon9_th_ms,
			 "Set A3210M th ms value"),
	OPT_WITH_ARG("--avalon9-th-timeout",
			 opt_set_uintval, opt_show_uintval, &opt_avalon9_th_timeout,
			 "Set A3210M th timeout value"),
	OPT_WITH_ARG("--avalon9-th-add",
			 set_int_0_to_1, opt_show_intval, &opt_avalon9_th_add,
			 "Set A3210M th add value"),
	OPT_WITHOUT_ARG("--avalon9-iic-detect",
			 opt_set_bool, &opt_avalon9_iic_detect,
			 "Enable Avalon9 detect through iic controller"),
	OPT_WITH_ARG("--avalon9-nonce-mask",
			 set_int_24_to_32, opt_show_intval, &opt_avalon9_nonce_mask,
			 "Set A3210M nonce mask, range 24-32."),
	OPT_WITH_ARG("--avalon9-nonce-check",
			 set_int_0_to_1, opt_show_intval, &opt_avalon9_nonce_check,
			 "Set A3210M nonce check, range 0-1."),
	OPT_WITH_ARG("--avalon9-roll-enable",
			 set_int_0_to_1, opt_show_intval, &opt_avalon9_roll_enable,
			 "Set A3210M roll enable, range 0-1."),
	OPT_WITH_ARG("--avalon9-mux-l2h",
			 set_int_0_to_2, opt_show_intval, &opt_avalon9_mux_l2h,
			 "Set Avalon9 mux l2h, range 0-2."),
	OPT_WITH_ARG("--avalon9-mux-h2l",
			 set_int_0_to_1, opt_show_intval, &opt_avalon9_mux_h2l,
			 "Set Avalon9 mux h2l, range 0-1."),
	OPT_WITH_ARG("--avalon9-h2ltime0-spd",
			 set_int_0_to_255, opt_show_intval, &opt_avalon9_h2ltime0_spd,
			 "Set Avalon9 h2ltime0 spd, range 0-255."),
	OPT_WITH_ARG("--avalon9-spdlow",
			 set_int_0_to_3, opt_show_intval, &opt_avalon9_spdlow,
			 "Set Avalon9 spdlow, range 0-3."),
	OPT_WITH_ARG("--avalon9-spdhigh",
			 set_int_0_to_3, opt_show_intval, &opt_avalon9_spdhigh,
			 "Set Avalon9 spdhigh, range 0-3."),
	OPT_WITH_ARG("--avalon9-tbase",
			 set_int_0_to_255, opt_show_intval, &opt_avalon9_tbase,
			 "Set Avalon9 tbase and use (0-8) bits, range 0-255."),
	OPT_WITH_CBARG("--avalon9-cinfo-asic",
			 set_avalon9_asic_otp, NULL, &opt_set_avalon9_asic_otp,
			 "Set Avalon9 cinfo asic index, range:[0, 25], step: 1"),
	OPT_WITH_ARG("--avalon9-pid-p",
			 set_int_0_to_9999, opt_show_intval, &opt_avalon9_pid_p,
			 "Set Avalon9 pid-p, range 0-9999."),
	OPT_WITH_ARG("--avalon9-pid-i",
			 set_int_0_to_9999, opt_show_intval, &opt_avalon9_pid_i,
			 "Set Avalon9 pid-i, range 0-9999."),
	OPT_WITH_ARG("--avalon9-pid-d",
			 set_int_0_to_9999, opt_show_intval, &opt_avalon9_pid_d,
			 "Set Avalon9 pid-d, range 0-9999."),
	OPT_WITH_ARG("--avalon9-adjust-volt-freq",
			 set_int_0_to_1, opt_show_intval, &opt_avalon9_adjust_volt_freq,
			 "Set Avalon9 adjust voltage and frequency, range 0-1, 0: disable, 1: enable"),
	OPT_WITH_CBARG("--avalon9-adjust-volt-info",
			 set_avalon9_adjust_volt_info, NULL, &opt_set_avalon9_adjust_volt_info,
			 "Set Avalon9 adjust volt info, eg. up_init/factor/threshold, down_init/factor/threshold, adjust_time"),
	OPT_WITH_CBARG("--avalon9-adjust-freq-info",
			 set_avalon9_adjust_freq_info, NULL, &opt_set_avalon9_adjust_freq_info,
			 "Set Avalon9 adjust freq info, eg. up_init/factor/threshold, down_init/factor/threshold, adjust_time"),
#endif
	OPT_WITHOUT_ARG("--balance",
			 set_balance, &pool_strategy,
			 "Change multipool strategy from failover to even share balance"),
	OPT_WITHOUT_ARG("--benchmark",
			opt_set_bool, &opt_benchmark,
			"Run cgminer in benchmark mode - produces no shares"),
#if defined(USE_ANTMINER_L3)
	OPT_WITHOUT_ARG("--enable-autotuner",
	opt_set_bool, &opt_enable_autotuner,
	"Allow autotuning procedure"),
	OPT_WITH_ARG("--core-temp",
	opt_set_intval, opt_show_intval, &opt_core_temp,
	"Set core temp"),
	OPT_WITH_ARG("--target-temperature",
	set_int_0_to_100, opt_show_intval, &opt_target_temperature,
	"Set target temperature [0~100]C for automatic fan controller"),
	OPT_WITH_ARG("--target-frequency",
	opt_set_intval, opt_show_intval, &opt_target_freq,
	"Set target frequency [250~650]MHz for autotuning procedure"),
	OPT_WITH_ARG("--frequency1",
	opt_set_intval, opt_show_intval, &opt_frequency[0],
	"Set frequency [250~650]MHz on chain 1"),
	OPT_WITH_ARG("--frequency2",
	opt_set_intval, opt_show_intval, &opt_frequency[1],
	"Set frequency [250~650]MHz on chain 2"),
	OPT_WITH_ARG("--frequency3",
	opt_set_intval, opt_show_intval, &opt_frequency[2],
	"Set frequency [250~650]MHz on chain 3"),
	OPT_WITH_ARG("--frequency4",
	opt_set_intval, opt_show_intval, &opt_frequency[3],
	"Set frequency [250~650]MHz on chain 4"),
	OPT_WITH_ARG("--voltage1",
	opt_set_intval, opt_show_intval, &opt_voltage[0],
	"Set voltage [0~255] on chain 1"),
	OPT_WITH_ARG("--voltage2",
	opt_set_intval, opt_show_intval, &opt_voltage[1],
	"Set voltage [0~255] on chain 2"),
	OPT_WITH_ARG("--voltage3",
	opt_set_intval, opt_show_intval, &opt_voltage[2],
	"Set voltage [0~255] on chain 3"),
	OPT_WITH_ARG("--voltage4",
	opt_set_intval, opt_show_intval, &opt_voltage[3],
	"Set voltage [0~255] on chain 4"),
#endif
#if defined(USE_S19XPH_DRIVER)
	OPT_WITH_ARG("--target-temperature",
	set_int_0_to_100, opt_show_intval, &opt_target_temperature,
	"Set target temperature [0~100]C for automatic temperature control"),
	OPT_WITH_ARG("--frequency",
	opt_set_intval, opt_show_intval, &opt_frequency,
	"Set ASIC frequency [50~650]MHz"),
	OPT_WITH_ARG("--psu_voltage",
	opt_set_floatval, opt_show_floatval, &opt_psu_voltage,
	"Set PSU voltage [15.0~24.0]V"),
#endif
#if defined(USE_S21_DRIVER)
	OPT_WITH_ARG("--autotuner-mode",
	opt_set_intval, opt_show_intval, &opt_autotuner_mode,
	"Set atotuner mode [0~3] where:\n\t0) disabled\n\t1) only fans\n\t2) target hashrate\n\t3) target power consumption"),
	OPT_WITH_ARG("--target-temperature",
	opt_set_floatval, opt_show_floatval, &opt_target_temperature,
	"Set target temperature [0~80]C for autotuning procedure"),
	OPT_WITH_ARG("--target-hashrate",
	opt_set_intval, opt_show_intval, &opt_target_hashrate,
	"Set target hashrate [0~999999]Gh/sec for autotuning procedure"),
	OPT_WITH_ARG("--target-power-consumption",
	opt_set_intval, opt_show_intval, &opt_target_power_consumption,
	"Set target power consumption [0~9999]Wh/h for autotuning procedure"),
	OPT_WITH_ARG("--fan-speed-percentage",
	opt_set_intval, opt_show_intval, &opt_fan_speed_percentage,
	"Set fan speed [0~100]% for manual fan control"),
	OPT_WITH_ARG("--frequency",
	opt_set_intval, opt_show_intval, &opt_frequency,
	"Set ASIC frequency [0~1000]MHz"),
	OPT_WITH_ARG("--psu-voltage",
	opt_set_floatval, opt_show_floatval, &opt_psu_voltage,
	"Set PSU voltage [11.0~15.4]V"),
#endif // USE_S21_DRIVER
	OPT_WITHOUT_ARG("--debug|-D",
			 enable_debug, &opt_debug,
			 "Enable debug output"),
	OPT_WITHOUT_ARG("--disable-rejecting",
			opt_set_bool, &opt_disable_pool,
			"Automatically disable pools that continually reject shares"),
	OPT_WITH_ARG("--expiry|-E",
			 set_null, NULL, &opt_set_null,
			 opt_hidden),
    OPT_WITHOUT_ARG("--extranonce-subscribe",
                set_extranonce_subscribe, NULL,
                "Enable 'extranonce' stratum subscribe"),
	OPT_WITHOUT_ARG("--failover-only",
			set_null, &opt_set_null,
			opt_hidden),
	OPT_WITH_ARG("--fallback-time",
			 opt_set_intval, opt_show_intval, &opt_pool_fallback,
			 "Set time in seconds to fall back to a higher priority pool after period of instability"),
	OPT_WITHOUT_ARG("--fix-protocol",
			opt_set_bool, &opt_fix_protocol,
			"Do not redirect to stratum protocol from GBT"),
	OPT_WITH_ARG("--hotplug",
			 set_int_0_to_9999, NULL, &hotplug_time,
#ifdef USE_USBUTILS
			 "Seconds between hotplug checks (0 means never check)"
#else
			 opt_hidden
#endif
			),
	OPT_WITHOUT_ARG("--load-balance",
			 set_loadbalance, &pool_strategy,
			 "Change multipool strategy from failover to quota based balance"),
	OPT_WITH_ARG("--log-interval|-l",
			 set_int_0_to_9999, opt_show_intval, &opt_log_interval,
			 "Interval in seconds between log output"),
	OPT_WITH_ARG("--log-level",
			 opt_set_uintval, NULL, &opt_log_level,
			 "Log detalization level [0~7] (default: 5)"),
	OPT_WITHOUT_ARG("--lowmem",
			opt_set_bool, &opt_lowmem,
			"Minimise caching of shares for low memory applications"),
	OPT_WITHOUT_ARG("--net-delay",
			opt_set_bool, &opt_delaynet,
			"Impose small delays in networking to not overload slow routers"),
	OPT_WITHOUT_ARG("--no-pool-disable",
			opt_set_invbool, &opt_disable_pool,
			opt_hidden),
	OPT_WITHOUT_ARG("--no-submit-stale",
			opt_set_invbool, &opt_submit_stale,
				"Don't submit shares if they are detected as stale"),
	OPT_WITH_ARG("--pass|-p",
			 set_pass, NULL, &opt_set_null,
			 "Password for bitcoin JSON-RPC server"),
	OPT_WITHOUT_ARG("--per-device-stats",
			opt_set_bool, &want_per_device_stats,
			"Force verbose mode and output per-device statistics"),
	OPT_WITH_ARG("--pools",
			opt_set_bool, NULL, &opt_set_null, opt_hidden),
	OPT_WITHOUT_ARG("--protocol-dump|-P",
			opt_set_bool, &opt_protocol,
			"Verbose dump of protocol-level activities"),
	OPT_WITH_ARG("--queue|-Q",
			 set_null, NULL, &opt_set_null,
			 opt_hidden),
	OPT_WITHOUT_ARG("--quiet|-q",
			opt_set_bool, &opt_quiet,
			"Disable logging output, display status and errors"),
	OPT_WITH_ARG("--quota|-U",
			 set_quota, NULL, &opt_set_null,
			 "quota;URL combination for server with load-balance strategy quotas"),
	OPT_WITHOUT_ARG("--real-quiet",
			opt_set_bool, &opt_realquiet,
			"Disable all output"),
	OPT_WITH_ARG("--retries",
			 set_null, NULL, &opt_set_null,
			 opt_hidden),
	OPT_WITH_ARG("--retry-pause",
			 set_null, NULL, &opt_set_null,
			 opt_hidden),
	OPT_WITH_ARG("--rotate",
			 set_rotate, NULL, &opt_set_null,
			 "Change multipool strategy from failover to regularly rotate at N minutes"),
	OPT_WITHOUT_ARG("--round-robin",
			 set_rr, &pool_strategy,
			 "Change multipool strategy from failover to round robin on failure"),
#ifdef USE_FPGA_SERIAL
	OPT_WITH_CBARG("--scan-serial|-S",
			 add_serial, NULL, &opt_add_serial,
			 "Serial port to probe for Serial FPGA Mining device"),
#endif
	OPT_WITH_ARG("--scan-time|-s",
			 set_null, NULL, &opt_set_null,
			 opt_hidden),
	OPT_WITH_CBARG("--sched-start",
			 set_sched_start, NULL, &opt_set_sched_start,
			 "Set a time of day in HH:MM to start mining (a once off without a stop time)"),
	OPT_WITH_CBARG("--sched-stop",
			 set_sched_stop, NULL, &opt_set_sched_stop,
			 "Set a time of day in HH:MM to stop mining (will quit without a start time)"),
	OPT_WITH_ARG("--shares",
			 opt_set_intval, NULL, &opt_shares,
			 "Quit after mining N shares (default: unlimited)"),
	OPT_WITH_ARG("--socks-proxy",
			 opt_set_charp, NULL, &opt_socks_proxy,
			 "Set socks4 proxy (host:port)"),
	OPT_WITH_ARG("--suggest-diff",
			 opt_set_intval, NULL, &opt_suggest_diff,
			 "Suggest miner difficulty for pool to user (default: none)"),
#ifdef HAVE_SYSLOG_H
	OPT_WITHOUT_ARG("--syslog",
			opt_set_bool, &opt_use_syslog,
			"Use system log for output messages (default: standard error)"),
#endif
	OPT_WITH_ARG("--url|-o",
			 set_url, NULL, &opt_set_null,
			 "URL for bitcoin JSON-RPC server"),
#ifdef USE_USBUTILS
	OPT_WITH_ARG("--usb",
			 opt_set_charp, NULL, &opt_usb_select,
			 "USB device selection"),
	OPT_WITH_ARG("--usb-dump",
			 set_int_0_to_10, opt_show_intval, &opt_usbdump,
			 opt_hidden),
	OPT_WITHOUT_ARG("--usb-list-all",
			opt_set_bool, &opt_usb_list_all,
			opt_hidden),
#endif
	OPT_WITH_ARG("--user|-u",
			 set_user, NULL, &opt_set_null,
			 "Username for bitcoin JSON-RPC server"),
	OPT_WITH_ARG("--userpass|-O",
			 set_userpass, NULL, &opt_set_null,
			 "Username:Password pair for bitcoin JSON-RPC server"),
	OPT_WITHOUT_ARG("--verbose",
			opt_set_bool, &opt_log_verbose,
			"Log verbose output to stderr as well as status output"),
	OPT_WITHOUT_ARG("--widescreen",
			opt_set_bool, &opt_widescreen,
			"Use extra wide display without toggling"),
	OPT_WITHOUT_ARG("--worktime",
			opt_set_bool, &opt_worktime,
			"Display extra work time debug information"),
	OPT_WITH_ARG("--force-clean-jobs",
			 opt_set_intval, NULL, &opt_force_clean_jobs,
			 "Force clean jobs to miners (default: 20)"),
	OPT_ENDTABLE
};

static int fileconf_load;
char *cnfbuf=NULL;

char *parse_config(json_t *config, bool fileconf)
{
	static char err_buf[200];
	struct opt_table *opt;
	const char *str;
	json_t *val;

	if(fileconf && !fileconf_load)
	{
		fileconf_load=1;
	}

	for(opt=opt_config_table; opt->type != OPT_END; opt++)
	{
		char *p, *saved, *name;

		if(!opt->names || !strlen(opt->names))
		{
			continue;
		}

		/* Pull apart the option name(s). */
		name=strdup(opt->names);
		for(p=strtok_r(name, "|", &saved); p != NULL; p=strtok_r(NULL, "|", &saved))
		{
			char *err=NULL;

			if(strlen(p)<3)
				continue;

			/* Ignore short options. */
			if(p[1] != '-')
				continue;

			val=json_object_get(config, p+2);
			if(!val)
			{
				continue;
			}
			if((opt->type & (OPT_HASARG | OPT_PROCESSARG)) && json_is_string(val))
			{
				str=json_string_value(val);
				err=opt->cb_arg(str, opt->u.arg);
				if(opt->type==OPT_PROCESSARG)
				{
					opt_set_charp(str, opt->u.arg);
				}
			}
			else if((opt->type & (OPT_HASARG | OPT_PROCESSARG)) && json_is_array(val))
			{
				json_t *arr_val;
				size_t index;
				json_array_foreach(val, index, arr_val)
				{
					if(json_is_string(arr_val))
					{
						str=json_string_value(arr_val);
						err=opt->cb_arg(str, opt->u.arg);
						if(opt->type==OPT_PROCESSARG)
							opt_set_charp(str, opt->u.arg);
					}
					else if(json_is_object(arr_val))
					{
						err=parse_config(arr_val, false);
					}
					if(err)
					{
						break;
					}
				}
			}
			else if((opt->type & OPT_NOARG) && json_is_true(val))
			{
				err=opt->cb(opt->u.arg);
			}
			else
			{
				err="Invalid value";
			}

			if(err)
			{
				/* Allow invalid values to be in configuration
				 * file, just skipping over them provided the
				 * JSON is still valid after that. */
				if(fileconf) {
					applog(LOG_ERR, "Invalid config option %s: %s", p, err);
					fileconf_load=-1;
				} else {
					snprintf(err_buf, sizeof(err_buf), "Parsing JSON option %s: %s",
						p, err);
					return err_buf;
				}
			}
		}
		free(name);
	}

	val=json_object_get(config, JSON_INCLUDE_CONF);
	if(json_is_string(val))
	{
		return load_config(json_string_value(val));
	}
	return NULL;
}

char *load_config(const char *arg)
{
	json_error_t err;
	char *json_error;
	size_t siz;

	if(!cnfbuf)
		cnfbuf=strdup(arg);

	if(++include_count>JSON_MAX_DEPTH)
		return JSON_MAX_DEPTH_ERR;

	config_json_data=json_load_file(arg, 0, &err);
	if(!json_is_object(config_json_data))
	{
		siz=JSON_LOAD_ERROR_LEN + strlen(arg) + strlen(err.text);
		json_error=cgmalloc(siz);
		snprintf(json_error, siz, JSON_LOAD_ERROR, arg, err.text);
		config_json_data=NULL;
		return json_error;
	}

	config_loaded=true;

	/* Parse the config now, so we can override it.  That can keep pointers
	 * so don't free config object. */
	return parse_config(config_json_data, true);
}

static char *set_default_config(const char *arg)
{
	opt_set_charp(arg, &default_config);
	return NULL;
}

void default_save_file(char *filename)
{
	if(default_config && *default_config)
	{
		strcpy(filename, default_config);
		return;
	}

#if defined(unix) || defined(__APPLE__)
	if(getenv("HOME") && *getenv("HOME"))
	{
		strcpy(filename, getenv("HOME"));
		strcat(filename, "/");
	}
	else
	{
		strcpy(filename, "");
	}
	strcat(filename, ".cgminer/");
	mkdir(filename, 0777);
#else
	strcpy(filename, "");
#endif
	strcat(filename, def_conf);
}

static void load_default_config(void)
{
	cnfbuf=cgmalloc(PATH_MAX);
	default_save_file(cnfbuf);
	if(!access(cnfbuf, R_OK))
	{
		load_config(cnfbuf);
	}
	else
	{
		free(cnfbuf);
		cnfbuf=NULL;
	}
}

extern const char *opt_argv0;

static char *opt_verusage_and_exit(const char *extra)
{
	printf("%s\nBuilt with "
#if defined(USE_S19XPH_DRIVER)
		"antminer s19xp hydro "
#endif
#if defined(USE_ANTMINER_L3)
		"antminer l3 "
#endif
#ifdef USE_AVALON2
		"avalon2 "
#endif
#ifdef USE_AVALON4
		"avalon4 "
#endif
#ifdef USE_AVALON7
		"avalon7 "
#endif
#ifdef USE_AVALON8
		"avalon8 "
#endif
#ifdef USE_AVALON9
		"avalon9 "
#endif
		"mining support.\n"
		, packagename);
	printf("%s", opt_usage(opt_argv0, extra));
	fflush(stdout);
	exit(0);
}

#if defined(USE_USBUTILS)
char *display_devs(int *ndevs)
{
	*ndevs=0;
	usb_all(0);
	exit(*ndevs);
}
#endif

/* These options are available from commandline only */
static struct opt_table opt_cmdline_table[]={
	OPT_WITH_ARG("--config|-c",
			 load_config, NULL, &opt_set_null,
			 "Load a JSON-format configuration file\n"
			 "See example.conf for an example configuration."),
	OPT_WITH_ARG("--default-config",
			 set_default_config, NULL, &opt_set_null,
			 "Specify the filename of the default config file\n"
			 "Loaded at start and used when saving without a name."),
	OPT_WITHOUT_ARG("--help|-h",
			opt_verusage_and_exit, NULL,
			"Print this message"),
#if defined(USE_USBUTILS)
	OPT_WITHOUT_ARG("--ndevs|-n",
			display_devs, &nDevs,
			"Display all USB devices and exit"),
#endif
	OPT_WITHOUT_ARG("--version|-V",
			opt_version_and_exit, packagename,
			"Display version and exit"),
	OPT_ENDTABLE
};

static void calc_midstate(struct work *work)
{
#if defined(ENABLE_ASIC_BOOST)

	uint8_t data[64];
	sha256_ctx ctx;
	flip64(data, work->data);
	sha256_init(&ctx);
	sha256_update(&ctx, data, 64);
	cg_memcpy(work->midstate[0], ctx.h, 32);

#else

	uint8_t data[64];
	sha256_ctx ctx;
	flip64(data, work->data);
	sha256_init(&ctx);
	sha256_update(&ctx, data, 64);
	cg_memcpy(work->midstate, ctx.h, 32);

#endif
}

/* Returns the current value of total_work and increments it */
static int total_work_inc(void)
{
	int ret;
	cg_wlock(&control_lock);
	ret=total_work++;
	cg_wunlock(&control_lock);
	return ret;
}

static struct work *make_work(void)
{
	struct work *work=cgcalloc(1, sizeof(struct work));
	work->id=total_work_inc();
	return work;
}

/* This is the central place all work that is about to be retired should be
 * cleaned to remove any dynamically allocated arrays within the struct */
void clean_work(struct work *work)
{
	free(work->job_id);
	free(work->ntime);
	free(work->coinbase);
	free(work->nonce1);
	memset(work, 0, sizeof(struct work));
}

/* All dynamically allocated work structs should be freed here to not leak any
 * ram from arrays allocated within the work struct. Null the actual pointer
 * used to call free_work. */
void free_work(struct work **workptr)
{
	struct work *work=*workptr;
	if(!work)
	{
		return;
	}
	clean_work(work);
	free(work);
	*workptr=NULL;
}

char *workpadding="000000800000000000000000000000000000000000000000000000000000000000000000000000000000000080020000";

/* truediffone==0x00000000FFFF0000000000000000000000000000000000000000000000000000
 * Generate a 256 bit binary LE target by cutting up diff into 64 bit sized
 * portions or vice versa. */
static const double truediffone=26959535291011309493156476344723991336010898738574164086137773096960.0;
static const double bits192=6277101735386680763835789423207666416102355444464034512896.0;
static const double bits128=340282366920938463463374607431768211456.0;
static const double bits64=18446744073709551616.0;

/* Converts a little endian 256 bit value to a double */
static double le256todouble(const void *target)
{
	uint64_t *data64;
	double dcut64;

	data64=(uint64_t *)(target + 24);
	dcut64=le64toh(*data64)*bits192;

	data64=(uint64_t *)(target + 16);
	dcut64+=le64toh(*data64)*bits128;

	data64=(uint64_t *)(target + 8);
	dcut64+=le64toh(*data64)*bits64;

	data64=(uint64_t *)(target);
	dcut64+=le64toh(*data64);

	return dcut64;
}

static int64_t diff_from_target(void *target)
{
	double d64, dcut64;
#if defined(SCRYPT_H)
	d64=(double)65536;
#else
	d64=truediffone;
#endif
	dcut64=le256todouble(target);
	if(dcut64<1)
	{
		dcut64=1;
	}
	d64/=dcut64;
	return(d64);
}

/* Convert a uint64_t value into a truncated string for displaying with its
 * associated suitable for Mega, Giga etc. Buf array needs to be long enough */
static void suffix_string(uint64_t val, char *buf, size_t bufsiz, int sigdigits)
{
	const double  dkilo=1000.0;
	const uint64_t kilo=1000ull;
	const uint64_t mega=1000000ull;
	const uint64_t giga=1000000000ull;
	const uint64_t tera=1000000000000ull;
	const uint64_t peta=1000000000000000ull;
	const uint64_t exa =1000000000000000000ull;
	char suffix[2]="";
	bool decimal=true;
	double dval;

	if(val >= exa) {
		val /= peta;
		dval=(double)val/dkilo;
		strcpy(suffix, "E");
	} else if(val >= peta) {
		val /= tera;
		dval=(double)val/dkilo;
		strcpy(suffix, "P");
	} else if(val >= tera) {
		val /= giga;
		dval=(double)val/dkilo;
		strcpy(suffix, "T");
	} else if(val >= giga) {
		val /= mega;
		dval=(double)val/dkilo;
		strcpy(suffix, "G");
	} else if(val >= mega) {
		val /= kilo;
		dval=(double)val/dkilo;
		strcpy(suffix, "M");
	}
	else if(val >= kilo)
	{
		dval=(double)val/dkilo;
		strcpy(suffix, "K");
	}
	else
	{
		dval=val;
		decimal=false;
	}
	if(!sigdigits)
	{
		if(decimal)
			snprintf(buf, bufsiz, "%.3g%s", dval, suffix);
		else
			snprintf(buf, bufsiz, "%d%s", (unsigned int)dval, suffix);
	}
	else
	{
		/* Always show sigdigits + 1, padded on right with zeroes
		 * followed by suffix */
		int ndigits=sigdigits - 1 - (dval>0.0 ? floor(log10(dval)) : 0);

		snprintf(buf, bufsiz, "%*.*f%s", sigdigits + 1, ndigits, dval, suffix);
	}
}

/*
 * Calculate the work->work_difficulty based on the work->target
 */
static void calc_diff(struct work *work, int64_t known_diff)
{
	struct cgminer_pool_stats *pool_stats=&(work->pool->cgminer_pool_stats);
	int64_t difficulty;
	uint64_t uintdiff;

	if(known_diff)
		work->work_difficulty=known_diff;
	else
		work->work_difficulty=diff_from_target(work->target);

	difficulty=work->work_difficulty;

	pool_stats->last_diff=difficulty;
	uintdiff=difficulty;
	suffix_string(uintdiff, work->pool->diff, sizeof(work->pool->diff), 0);

	if(difficulty==pool_stats->min_diff)
	{
		pool_stats->min_diff_count++;
	}
	else if(difficulty<pool_stats->min_diff || pool_stats->min_diff==0)
	{
		pool_stats->min_diff=difficulty;
		pool_stats->min_diff_count=1;
	}

	if(difficulty==pool_stats->max_diff)
	{
		pool_stats->max_diff_count++;
	}
	else if(difficulty>pool_stats->max_diff)
	{
		pool_stats->max_diff=difficulty;
		pool_stats->max_diff_count=1;
	}
}

#define work_decode(pool, work, val) (false)
#define gen_gbt_work(pool, work) {}

int dev_from_id(int thr_id)
{
	struct cgpu_info *cgpu=get_thr_cgpu(thr_id);

	return cgpu->device_id;
}

/* Create an exponentially decaying average over the opt_log_interval */
void decay_time(double *f, double fadd, double fsecs, double interval)
{
	double ftotal, fprop;
	if(fsecs <= 0)
	{
		return;
	}
	fprop=1.0 - 1/(exp(fsecs/interval));
	ftotal=1.0 + fprop;
	*f+=(fadd/fsecs * fprop);
	*f /= ftotal;
}

static int total_staged(void)
{
	return HASH_COUNT(staged_work);
}

double total_secs=1.0;
static char statusline[256];
/* logstart is where the log window should start */
static int devcursor, logstart, logcursor;

double cgpu_runtime(struct cgpu_info *cgpu)
{
	struct timeval now;
	double dev_runtime;
	if(cgpu->dev_start_tv.tv_sec==0)
	{
		dev_runtime=total_secs;
	}
	else
	{
		cgtime(&now);
		dev_runtime=tdiff(&now, &(cgpu->dev_start_tv));
	}

	if(dev_runtime<1.0)
		dev_runtime=1.0;
	return dev_runtime;
}

double tsince_restart(void)
{
	struct timeval now;

	cgtime(&now);
	return tdiff(&now, &restart_tv_start);
}

double tsince_update(void)
{
	struct timeval now;

	cgtime(&now);
	return tdiff(&now, &update_tv_start);
}

static void get_statline(char *buf, size_t bufsiz, struct cgpu_info *cgpu)
{
	char displayed_hashes[16];
	double dev_runtime, wu;
	uint64_t dh64;
	size_t space_left=bufsiz, len=0;

	dev_runtime=cgpu_runtime(cgpu);

	wu=cgpu->diff1/dev_runtime * 60.0;

	dh64=(double)cgpu->total_mhashes/dev_runtime * 1000000ull;
	suffix_string(dh64, displayed_hashes, sizeof(displayed_hashes), 4);

	snprintf(buf+len, space_left, "%s %d ", cgpu->drv->name, cgpu->device_id);
	len=strlen(buf);
	space_left=bufsiz-len;

	cgpu->drv->get_statline_before(buf+len, space_left, cgpu);
	len=strlen(buf);
	space_left=bufsiz-len;

	snprintf(buf+len, space_left, "(avg):%sh/s | A:%lli R:%lli HWE:%i WU:%.1lf/m",
		displayed_hashes,
		cgpu->diff_accepted,
		cgpu->diff_rejected,
		cgpu->hw_errors,
		wu);
	len=strlen(buf);
	space_left=bufsiz-len;

	cgpu->drv->get_statline(buf+len, space_left, cgpu);
}

static bool shared_strategy(void)
{
	return(pool_strategy==POOL_LOADBALANCE || pool_strategy==POOL_BALANCE);
}

static void enable_pool(struct pool *pool)
{
	if(pool->enabled != POOL_ENABLED)
	{
		enabled_pools++;
		pool->enabled=POOL_ENABLED;
	}
}

static void reject_pool(struct pool *pool)
{
	if(pool->enabled==POOL_ENABLED)
	{
		enabled_pools--;
	}
	pool->enabled=POOL_REJECTING;
}

static bool stale_work(struct work *work, bool share)
{
	struct timeval now;
	time_t work_expiry;
	struct pool *pool;

	if(opt_benchmark)
		return false;

	if(work->work_block != work_block) {
		applog(LOG_DEBUG, "Work stale due to block mismatch");
		return true;
	}

	/* Technically the rolltime should be correct but some pools
	 * advertise a broken expire= that is lower than a meaningful
	 * scantime */
	if(work->rolltime>max_scantime)
		work_expiry=work->rolltime;
	else
		work_expiry=max_expiry;

	pool=work->pool;

	if(!share && pool->has_stratum)
	{
		bool same_job;

		if(!pool->stratum_active || !pool->stratum_notify)
		{
			applog(LOG_DEBUG, "Work stale due to stratum inactive");
			return true;
		}

		same_job=true;

		cg_rlock(&pool->data_lock);
		if(strcmp(work->job_id, pool->swork.job_id))
		{
			same_job=false;
		}
		cg_runlock(&pool->data_lock);

		if(!same_job)
		{
			applog(LOG_DEBUG, "Work stale due to stratum job_id mismatch");
			return true;
		}
	}

	if((work_expiry<5))
		work_expiry=5;

	cgtime(&now);
	if((now.tv_sec - work->tv_staged.tv_sec) >= work_expiry) {
		applog(LOG_DEBUG, "Work stale due to expiry");
		return true;
	}

	return false;
}

static void discard_stale(void)
{
	struct work *work, *tmp;
	int stale=0;

	mutex_lock(stgd_lock);
	HASH_ITER(hh, staged_work, work, tmp)
	{
		if(stale_work(work, false))
		{
			HASH_DEL(staged_work, work);
			discard_work(&work);
			stale++;
		}
	}
	pthread_cond_signal(&gws_cond);
	mutex_unlock(stgd_lock);

	if(stale)
		applog(LOG_DEBUG, "Discarded %d stales that didn't match current hash", stale);
}

static void *restart_thread(void *arg)
{
	struct cgpu_info *cgpu;
	int i, mt;

	pthread_detach(pthread_self());

	/* Discard staged work that is now stale */
	discard_stale();

	rd_lock(&mining_thr_lock);
	mt=mining_threads;
	rd_unlock(&mining_thr_lock);

	for(i=0; i<mt; i++)
	{
		cgpu=mining_thr[i]->cgpu;
		if((!cgpu))
			continue;
		if(cgpu->deven != DEV_ENABLED)
			continue;
		mining_thr[i]->work_restart=true;
		flush_queue(cgpu);
		cgpu->drv->flush_work(cgpu);
	}

	mutex_lock(&restart_lock);
	pthread_cond_broadcast(&restart_cond);
	mutex_unlock(&restart_lock);

#ifdef USE_USBUTILS
	/* Cancels any cancellable usb transfers. Flagged as such it means they
	 * are usualy waiting on a read result and it's safe to abort the read
	 * early. */
	cancel_usb_transfers();
#endif
	return NULL;
}

/* In order to prevent a deadlock via the various drv->flush_work
 * implementations we send the restart messages via a separate thread. */
static void restart_threads(void)
{
	pthread_t rthread;

	cgtime(&restart_tv_start);
	if((pthread_create(&rthread, NULL, restart_thread, NULL)))
	{
		quithere(1, "Failed to create restart thread errno=%d", errno);
	}
}

/* Theoretically threads could race when modifying accepted and
 * rejected values but the chance of two submits completing at the
 * same time is zero so there is no point adding extra locking */
static void share_result(json_t *val, json_t *res, json_t *err, const struct work *work,
		 char *hashshow, bool resubmit, char *worktime)
{
	struct pool *pool=work->pool;
	struct cgpu_info *cgpu;

	cgpu=get_thr_cgpu(work->thr_id);

	if(json_is_true(res) || (work->gbt && json_is_null(res))) {
		mutex_lock(&stats_lock);
		cgpu->accepted++;
		total_accepted++;
		pool->accepted++;
		cgpu->diff_accepted+=work->work_difficulty;
		total_diff_accepted+=work->work_difficulty;
		pool->diff_accepted+=work->work_difficulty;
		mutex_unlock(&stats_lock);

		pool->seq_rejects=0;
		cgpu->last_share_pool=pool->pool_no;
		cgpu->last_share_pool_time=time(NULL);
		cgpu->last_share_diff=work->work_difficulty;
		pool->last_share_time=cgpu->last_share_pool_time;
		pool->last_share_diff=work->work_difficulty;
		applog(LOG_DEBUG, "PROOF OF WORK RESULT: true (yay!!!)");
		if(!QUIET) {
			if(total_pools>1)
				applog(LOG_DEBUG, "Accepted %s %s %d pool %d %s%s",
					   hashshow, cgpu->drv->name, cgpu->device_id, work->pool->pool_no, resubmit ? "(resubmit)" : "", worktime);
			else
				applog(LOG_DEBUG, "Accepted %s %s %d %s%s",
					   hashshow, cgpu->drv->name, cgpu->device_id, resubmit ? "(resubmit)" : "", worktime);
		}
		sharelog("accept", work);
		if(opt_shares && total_diff_accepted >= opt_shares)
		{
			applog(LOG_WARNING, "Successfully mined %d accepted shares as requested and exiting.", opt_shares);
			kill_work();
			return;
		}

		/* Detect if a pool that has been temporarily disabled for
		 * continually rejecting shares has started accepting shares.
		 * This will only happen with the work returned from a
		 * longpoll */
		if((pool->enabled==POOL_REJECTING)) {
			applog(LOG_WARNING, "Rejecting pool %d now accepting shares, re-enabling!", pool->pool_no);
			enable_pool(pool);
			switch_pools(NULL);
		}
		/* If we know we found the block we know better than anyone
		 * that new work is needed. */
		if((work->block))
			restart_threads();
	} else {
		mutex_lock(&stats_lock);
		cgpu->rejected++;
		total_rejected++;
		pool->rejected++;
		cgpu->diff_rejected+=work->work_difficulty;
		total_diff_rejected+=work->work_difficulty;
		pool->diff_rejected+=work->work_difficulty;
		pool->seq_rejects++;
		mutex_unlock(&stats_lock);

		applog(LOG_DEBUG, "PROOF OF WORK RESULT: false (booooo)");
		if(!QUIET) {
			char where[20];
			char disposition[36]="reject";
			char reason[32];

			strcpy(reason, "");
			if(total_pools>1)
				snprintf(where, sizeof(where), "pool %d", work->pool->pool_no);
			else
				strcpy(where, "");

			if(!work->gbt)
				res=json_object_get(val, "reject-reason");
			if(res) {
				const char *reasontmp=json_string_value(res);

				size_t reasonLen=strlen(reasontmp);
				if(reasonLen>28)
					reasonLen=28;
				reason[0]=' '; reason[1]='(';
				cg_memcpy(2 + reason, reasontmp, reasonLen);
				reason[reasonLen + 2]=')'; reason[reasonLen + 3]='\0';
				cg_memcpy(disposition + 7, reasontmp, reasonLen);
				disposition[6]=':'; disposition[reasonLen + 7]='\0';
			}
			else if(work->stratum)
			{
				if(json_is_array(err))
				{
					json_t *reason_val=json_array_get(err, 1);
					char *reason_str;

					if(json_is_string(reason_val))
					{
						reason_str=(char *)json_string_value(reason_val);
						snprintf(reason, 31, " (%s)", reason_str);
					}
				}
				else if(json_is_string(err))
				{
					const char *s=json_string_value(err);
					snprintf(reason, 31, " (%s)", s);
				}
			}
			applog(LOG_NOTICE, "Rejected %s %s %d %s%s %s%s", hashshow, cgpu->drv->name, cgpu->device_id, where, reason, resubmit ? "(resubmit)" : "", worktime);
			sharelog(disposition, work);
		}

		/* Once we have more than a nominal amount of sequential rejects,
		 * at least 10 and more than 3 mins at the current utility,
		 * disable the pool because some pool error is likely to have
		 * ensued. Do not do this if we know the share just happened to
		 * be stale due to networking delays.
		 */
		if(pool->seq_rejects>10 && !work->stale && opt_disable_pool && enabled_pools>1)
		{
			double utility=total_accepted/total_secs * 60;
			if(pool->seq_rejects>utility * 3)
			{
				applog(LOG_WARNING, "Pool %d rejected %d sequential shares, disabling!", pool->pool_no, pool->seq_rejects);
				reject_pool(pool);
				if(pool==current_pool())
				{
					switch_pools(NULL);
				}
				pool->seq_rejects=0;
			}
		}
	}
}

static void show_hash(struct work *work, char *hashshow)
{
	uint8_t rhash[32];
	char diffdisp[16];
	unsigned long h32;
	uint64_t uintdiff;
	int ofs;
	swab256(rhash, work->hash);
	for(ofs=0; ofs <= 28; ofs ++)
	{
		if(rhash[ofs])
		{
			break;
		}
	}
	h32=be32toh(*(rhash + ofs));
	uintdiff=round(work->work_difficulty);
	suffix_string(work->share_diff, diffdisp, sizeof (diffdisp), 0);
	snprintf(hashshow, 64, "%08lx Diff %s/%"PRIu64"%s", h32, diffdisp, uintdiff, work->block? " BLOCK!" : "");
}

/* Specifies whether we can use this pool for work or not. */
static bool pool_unusable(struct pool *pool)
{
	if(pool->idle)
		return true;
	if(pool->enabled != POOL_ENABLED)
		return true;
	if(pool->has_stratum && (!pool->stratum_active || !pool->stratum_notify))
		return true;
	return false;
}

/* In balanced mode, the amount of diff1 solutions per pool is monitored as a
 * rolling average per 10 minutes and if pools start getting more, it biases
 * away from them to distribute work evenly. The share count is reset to the
 * rolling average every 10 minutes to not send all work to one pool after it
 * has been disabled/out for an extended period. */
static struct pool *select_balanced(struct pool *cp)
{
	int i, lowest=cp->shares;
	struct pool *ret=cp;

	for(i=0; i<total_pools; i++) {
		struct pool *pool=pools[i];

		if(pool_unusable(pool))
			continue;
		if(pool->shares<lowest) {
			lowest=pool->shares;
			ret=pool;
		}
	}

	ret->shares++;
	return ret;
}

static struct pool *priority_pool(int choice);

/* Select any active pool in a rotating fashion when loadbalance is chosen if
 * it has any quota left. */
static inline struct pool *select_pool(void)
{
	static int rotating_pool=0;
	struct pool *pool, *cp;
	bool avail=false;
	int tested, i;

	cp=current_pool();

	if(pool_strategy==POOL_BALANCE) {
		pool=select_balanced(cp);
		goto out;
	}

	if(pool_strategy != POOL_LOADBALANCE) {
		pool=cp;
		goto out;
	} else
		pool=NULL;

	for(i=0; i<total_pools; i++) {
		struct pool *tp=pools[i];

		if(tp->quota_used<tp->quota_gcd) {
			avail=true;
			break;
		}
	}

	/* There are no pools with quota, so reset them. */
	if(!avail) {
		for(i=0; i<total_pools; i++)
			pools[i]->quota_used=0;
		if(++rotating_pool >= total_pools)
			rotating_pool=0;
	}

	/* Try to find the first pool in the rotation that is usable */
	tested=0;
	while(!pool && tested++<total_pools) {
		pool=pools[rotating_pool];
		if(pool->quota_used++<pool->quota_gcd) {
			if(!pool_unusable(pool))
				break;
		}
		pool=NULL;
		if(++rotating_pool >= total_pools)
			rotating_pool=0;
	}

	/* If there are no alive pools with quota, choose according to
	 * priority. */
	if(!pool) {
		for(i=0; i<total_pools; i++) {
			struct pool *tp=priority_pool(i);

			if(!pool_unusable(tp)) {
				pool=tp;
				break;
			}
		}
	}

	/* If still nothing is usable, use the current pool */
	if(!pool)
		pool=cp;
out:
	applog(LOG_DEBUG, "Selecting pool %d for work", pool->pool_no);
	return pool;
}

static uint8_t bench_hidiff_bins[16][160];
static uint8_t bench_lodiff_bins[16][160];
static uint8_t bench_target[32];

/* Iterate over the lo and hi diff benchmark work items such that we find one
 * diff 32+ share every 32 work items. */
static void get_benchmark_work(struct work *work)
{
	work->work_difficulty=32;
	cg_memcpy(work->target, bench_target, 32);
	work->drv_rolllimit=0;
	work->mandatory=true;
	work->pool=pools[0];
	cgtime(&work->tv_getwork);
	copy_time(&work->tv_getwork_reply, &work->tv_getwork);
	work->getwork_mode=GETWORK_MODE_BENCHMARK;
}

static void kill_timeout(struct thr_info *thr)
{
	cg_completion_timeout(&thr_info_cancel, thr, 1000);
}

static void kill_mining(void)
{
	struct thr_info *thr;
	int i;

	applog(LOG_DEBUG, "Killing off mining threads");
	/* Kill the mining threads*/
	for(i=0; i<mining_threads; i++)
	{
		pthread_t *pth=NULL;

		thr=get_thread(i);
		if(thr && thr->pth)
		{
			pth=&thr->pth;
		}
		thr_info_cancel(thr);
		if(pth && *pth)
		{
			pthread_join(*pth, NULL);
		}
	}
}

static void __kill_work(void)
{
	struct thr_info *thr;
	int i;

	if(!successful_connect)
		return;

	applog(LOG_INFO, "Received kill message");

#ifdef USE_USBUTILS
	/* Best to get rid of it first so it doesn't
	 * try to create any new devices */
	applog(LOG_DEBUG, "Killing off HotPlug thread");
	thr=&control_thr[hotplug_thr_id];
	kill_timeout(thr);
#endif

	applog(LOG_DEBUG, "Killing off watchpool thread");
	/* Kill the watchpool thread */
	thr=&control_thr[watchpool_thr_id];
	kill_timeout(thr);

	applog(LOG_DEBUG, "Killing off watchdog thread");
	/* Kill the watchdog thread */
	thr=&control_thr[watchdog_thr_id];
	kill_timeout(thr);

	applog(LOG_DEBUG, "Shutting down mining threads");
	for(i=0; i<mining_threads; i++) {
		struct cgpu_info *cgpu;

		thr=get_thread(i);
		if(!thr)
			continue;
		cgpu=thr->cgpu;
		if(!cgpu)
			continue;

		cgpu->shutdown=true;
	}

	sleep(1);

	cg_completion_timeout(&kill_mining, NULL, 3000);

	/* Stop the others */
	applog(LOG_DEBUG, "Killing off API thread");
	thr=&control_thr[api_thr_id];
	kill_timeout(thr);

#ifdef USE_USBUTILS
	/* Release USB resources in case it's a restart
	 * and not a QUIT */
	applog(LOG_DEBUG, "Releasing all USB devices");
	cg_completion_timeout(&usb_cleanup, NULL, 1000);

	applog(LOG_DEBUG, "Killing off usbres thread");
	thr=&control_thr[usbres_thr_id];
	kill_timeout(thr);
#endif

}

/* This should be the common exit path */
void kill_work(void)
{
	cg_completion_timeout(&__kill_work, NULL, 5000);
	quit(0, "Shutdown signal received.");
}

static
#ifdef WIN32
const
#endif
char **initial_args;

static void clean_up(bool restarting);

void app_restart(void)
{
	applog(LOG_WARNING, "Attempting to restart %s", packagename);
#ifdef USE_LIBSYSTEMD
	sd_notify(false, "RELOADING=1\n"
		"STATUS=Restarting...");
#endif

	cg_completion_timeout(&__kill_work, NULL, 5000);
	clean_up(true);

	execv(initial_args[0], (EXECV_2ND_ARG_TYPE)initial_args);
	applog(LOG_WARNING, "Failed to restart application");
}

static void sighandler(int sig)
{
	/* Restore signal handlers so we can still quit if kill_work fails */
	sigaction(SIGTERM, &termhandler, NULL);
	sigaction(SIGINT, &inthandler, NULL);
	sigaction(SIGABRT, &abrthandler, NULL);
	kill_work();
}

static void _stage_work(struct work *work);

#define stage_work(WORK) do { \
	_stage_work(WORK); \
	WORK=NULL; \
} while(0)

/* Adjust an existing char ntime field with a relative noffset */
static void modify_ntime(char *ntime, int noffset)
{
	uint8_t bin[4];
	uint32_t h32, *be32=(uint32_t *)bin;
	hex2bin(bin, ntime, 4);
	h32=be32toh(*be32) + noffset;
	*be32=htobe32(h32);
	__bin2hex(ntime, bin, 4);
}

void roll_work(struct work *work)
{
	uint32_t *work_ntime;
	uint32_t ntime;

	work_ntime=(uint32_t *)(work->data + 68);
	ntime=be32toh(*work_ntime);
	ntime++;
	*work_ntime=htobe32(ntime);
	local_work++;
	work->rolls++;
	work->nonce=0;
	applog(LOG_DEBUG, "Successfully rolled work");
	/* Change the ntime field if this is stratum work */
	if(work->ntime)
		modify_ntime(work->ntime, 1);

	/* This is now a different work item so it needs a different ID for the
	 * hashtable */
	work->id=total_work_inc();
}

void roll_work_ntime(struct work *work, int noffset)
{
	uint32_t *work_ntime;
	uint32_t ntime;

	work_ntime=(uint32_t *)(work->data + 68);
	ntime=be32toh(*work_ntime);
	ntime+=noffset;
	*work_ntime=htobe32(ntime);
	local_work++;
	work->rolls+=noffset;
	work->nonce=0;
	applog(LOG_DEBUG, "Successfully rolled work");

	/* Change the ntime field if this is stratum work */
	if(work->ntime)
		modify_ntime(work->ntime, noffset);

	/* This is now a different work item so it needs a different ID for the
	 * hashtable */
	work->id=total_work_inc();
}

static void *submit_work_thread(void *userdata)
{
	pthread_detach(pthread_self());
	return NULL;
}

/* Return an adjusted ntime if we're submitting work that a device has
 * internally offset the ntime. */
static char *offset_ntime(const char *ntime, int noffset)
{
	uint8_t bin[4];
	uint32_t h32, *be32=(uint32_t *)bin;

	hex2bin(bin, ntime, 4);
	h32=be32toh(*be32) + noffset;
	*be32=htobe32(h32);

	return bin2hex(bin, 4);
}

/* Duplicates any dynamically allocated arrays within the work struct to
 * prevent a copied work struct from freeing ram belonging to another struct */
static void _copy_work(struct work *work, const struct work *base_work, int noffset)
{
	uint32_t id=work->id;
	clean_work(work);
	cg_memcpy(work, base_work, sizeof(struct work));
	/* Keep the unique new id assigned during make_work to prevent copied
	 * work from having the same id. */
	work->id=id;
	if(base_work->job_id)
	{
		work->job_id=strdup(base_work->job_id);
	}
	if(base_work->nonce1)
	{
		work->nonce1=strdup(base_work->nonce1);
	}
	if(base_work->ntime)
	{
		/* If we are passed an noffset the binary work->data ntime and
		 * the work->ntime hex string need to be adjusted. */
		if(noffset)
		{
			uint32_t *work_ntime=(uint32_t *)(work->data + 68);
			uint32_t ntime=be32toh(*work_ntime);
			ntime+=noffset;
			*work_ntime=htobe32(ntime);
			work->ntime=offset_ntime(base_work->ntime, noffset);
		}
		else
		{
			work->ntime=strdup(base_work->ntime);
		}
	}
	else if(noffset)
	{
		uint32_t *work_ntime=(uint32_t *)(work->data + 68);
		uint32_t ntime=be32toh(*work_ntime);
		ntime+=noffset;
		*work_ntime=htobe32(ntime);
	}
	if(base_work->coinbase)
	{
		work->coinbase=strdup(base_work->coinbase);
	}
#if defined(ENABLE_ASIC_BOOST)
	work->version=base_work->version;
#endif
}

/* Generates a copy of an existing work struct, creating fresh heap allocations
 * for all dynamically allocated arrays within the struct. noffset is used for
 * when a driver has internally rolled the ntime, noffset is a relative value.
 * The macro copy_work() calls this function with an noffset of 0. */
struct work *copy_work_noffset(struct work *base_work, int noffset)
{
	struct work *work=make_work();
	_copy_work(work, base_work, noffset);
	return work;
}

void pool_died(struct pool *pool)
{
	if(!pool_tset(pool, &pool->idle)) {
		cgtime(&pool->tv_idle);
		if(pool==current_pool()) {
			applog(LOG_WARNING, "Pool %d %s not responding!", pool->pool_no, pool->rpc_url);
			switch_pools(NULL);
		} else
			applog(LOG_INFO, "Pool %d %s failed to return work", pool->pool_no, pool->rpc_url);
	}
}

int64_t share_diff(const struct work *work)
{
	int64_t ret;
	double d64, s64;
	bool new_best=false;
	d64=truediffone;
#if defined(SCRYPT_H)
	d64 *= (double)65536;
#endif
	s64=le256todouble(work->hash);
	if(s64<1)
	{
		s64=1;
	}
	ret=d64/s64;
	cg_wlock(&control_lock);
	if(ret>best_diff)
	{
		new_best=true;
		best_diff=ret;
		suffix_string(best_diff, best_share, sizeof(best_share), 0);
	}
	if(ret>work->pool->best_diff)
	{
		work->pool->best_diff=ret;
	}
	cg_wunlock(&control_lock);
	if(new_best)
	{
		applog(LOG_INFO, "New best share: %s", best_share);
	}
	return ret;
}

static void regen_hash(struct work *work)
{
	uint8_t swap[80];
	uint8_t hash1[32];
	flip80(swap, work->data);
	sha256(swap, 80, hash1);
	sha256(hash1, 32, (uint8_t *)(work->hash));
}

/* Find the pool that currently has the highest priority */
static struct pool *priority_pool(int choice)
{
	struct pool *ret=NULL;
	int i;
	for(i=0; i<total_pools; i++)
	{
		struct pool *pool=pools[i];
		if(pool->prio==choice)
		{
			ret=pool;
			break;
		}
	}

	if((!ret))
	{
		applog(LOG_ERR, "WTF No pool %d found!", choice);
		return pools[choice];
	}
	return ret;
}

void switch_pools(struct pool *selected)
{
	struct pool *pool, *last_pool;
	int i, pool_no, next_pool;

	cg_wlock(&control_lock);
	last_pool=currentpool;
	pool_no=currentpool->pool_no;

	/* Switch selected to pool number 0 and move the rest down */
	if(selected) {
		if(selected->prio != 0) {
			for(i=0; i<total_pools; i++) {
				pool=pools[i];
				if(pool->prio<selected->prio)
					pool->prio++;
			}
			selected->prio=0;
		}
	}

	switch (pool_strategy) {
		/* All of these set to the master pool */
		case POOL_BALANCE:
		case POOL_FAILOVER:
		case POOL_LOADBALANCE:
			for(i=0; i<total_pools; i++) {
				pool=priority_pool(i);
				if(pool_unusable(pool))
					continue;
				pool_no=pool->pool_no;
				break;
			}
			break;
		/* Both of these simply increment and cycle */
		case POOL_ROUNDROBIN:
		case POOL_ROTATE:
			if(selected && !selected->idle) {
				pool_no=selected->pool_no;
				break;
			}
			next_pool=pool_no;
			/* Select the next alive pool */
			for(i=1; i<total_pools; i++) {
				next_pool++;
				if(next_pool >= total_pools)
					next_pool=0;
				pool=pools[next_pool];
				if(pool_unusable(pool))
					continue;
				pool_no=next_pool;
				break;
			}
			break;
		default:
			break;
	}

	currentpool=pools[pool_no];
	pool=currentpool;
	cg_wunlock(&control_lock);

	if(pool != last_pool && pool_strategy != POOL_LOADBALANCE && pool_strategy != POOL_BALANCE) {
		applog(LOG_WARNING, "Switching to pool %d %s", pool->pool_no, pool->rpc_url);
		clear_pool_work(last_pool);
	}

	mutex_lock(&lp_lock);
	pthread_cond_broadcast(&lp_cond);
	mutex_unlock(&lp_lock);
}

void discard_work(struct work **workptr)
{
	struct work *work=*workptr;
	if(!work)
	{
		applog(LOG_ERR, "discard_work() called with nullptr");
		return;
	}
	if(!work->clone && !work->rolls && !work->mined)
	{
		if(work->pool)
		{
			work->pool->discarded_work++;
			work->pool->quota_used--;
			work->pool->works--;
		}
		total_discarded++;
		applog(LOG_DEBUG, "Discarded work");
	}
	else
	{
		applog(LOG_DEBUG, "Discarded cloned or rolled work");
	}
	free_work(workptr);
}

static void wake_gws(void)
{
	mutex_lock(stgd_lock);
	pthread_cond_signal(&gws_cond);
	mutex_unlock(stgd_lock);
}

/* A generic wait function for threads that poll that will wait a specified
 * time tdiff waiting on the pthread conditional that is broadcast when a
 * work restart is required. Returns the value of pthread_cond_timedwait
 * which is zero if the condition was met or ETIMEDOUT if not.
 */
int restart_wait(struct thr_info *thr, unsigned int mstime)
{
	struct timeval now, then, tdiff;
	struct timespec abstime;
	int rc;

	tdiff.tv_sec=mstime/1000;
	tdiff.tv_usec=mstime * 1000 - (tdiff.tv_sec * 1000000);
	cgtime(&now);
	timeradd(&now, &tdiff, &then);
	abstime.tv_sec=then.tv_sec;
	abstime.tv_nsec=then.tv_usec * 1000;

	mutex_lock(&restart_lock);
	if(thr->work_restart)
		rc=0;
	else
		rc=pthread_cond_timedwait(&restart_cond, &restart_lock, &abstime);
	mutex_unlock(&restart_lock);

	return rc;
}

static void signal_work_update(void)
{
	int i;

	applog(LOG_INFO, "Work update message received");

	cgtime(&update_tv_start);
	rd_lock(&mining_thr_lock);
	for(i=0; i<mining_threads; i++)
	{
		mining_thr[i]->work_update=true;
	}
	rd_unlock(&mining_thr_lock);
}

static void signal_clean_jobs(void)
{
	int i;

	applog(LOG_NOTICE, "Job clean message received");

	rd_lock(&mining_thr_lock);
	for(i=0; i<mining_threads; i++)
	{
		mining_thr[i]->clean_jobs=true;
	}
	rd_unlock(&mining_thr_lock);
}

static void set_curblock(const char *hexstr, const uint8_t *bedata)
{
	int ofs;

	cg_wlock(&ch_lock);
	cgtime(&block_timeval);
	strcpy(current_hash, hexstr);
	cg_memcpy(current_block, bedata, 32);
	get_timestamp(blocktime, sizeof(blocktime), &block_timeval);
	cg_wunlock(&ch_lock);

	for(ofs=0; ofs <= 56; ofs++)
	{
		if(memcmp(&current_hash[ofs], "0", 1))
			break;
	}
	strncpy(prev_block, &current_hash[ofs], 8);
	prev_block[8]='\0';

	applog(LOG_INFO, "New block: %s... diff %s", current_hash, block_diff);
}

static int block_sort(struct block *blocka, struct block *blockb)
{
	return blocka->block_no - blockb->block_no;
}

/* Decode the current block difficulty which is in packed form */
static void set_blockdiff(const struct work *work)
{
	uint8_t pow=work->data[72];
	int powdiff=(8 * (0x1d - 3)) - (8 * (pow - 3));
	if(powdiff<8)
		powdiff=8;
	uint32_t diff32=be32toh(*((uint32_t *)(work->data + 72))) & 0x00FFFFFF;
	double numerator=0xFFFFULL << powdiff;
	double ddiff=numerator/(double)diff32;

	if((current_diff != ddiff)) {
		suffix_string(ddiff, block_diff, sizeof(block_diff), 0);
		current_diff=ddiff;
		applog(LOG_NOTICE, "Network diff set to %s", block_diff);
	}
}

/* Search to see if this string is from a block that has been seen before */
static bool block_exists(const char *hexstr, const uint8_t *bedata, const struct work *work)
{
	int deleted_block=0;
	struct block *s;
	bool ret=true;

	wr_lock(&blk_lock);
	HASH_FIND_STR(blocks, hexstr, s);
	if(!s)
	{
		s=cgcalloc(sizeof(struct block), 1);
		if((!s))
		{
			quit (1, "block_exists OOM");
		}
		strcpy(s->hash, hexstr);
		s->block_no=new_blocks++;

		ret=false;
		/* Only keep the last hour's worth of blocks in memory since
		 * work from blocks before this is virtually impossible and we
		 * want to prevent memory usage from continually rising */
		if(HASH_COUNT(blocks)>6)
		{
			struct block *oldblock;
			HASH_SORT(blocks, block_sort);
			oldblock=blocks;
			deleted_block=oldblock->block_no;
			HASH_DEL(blocks, oldblock);
			free(oldblock);
		}
		HASH_ADD_STR(blocks, hash, s);
		set_blockdiff(work);
		if(deleted_block)
			applog(LOG_DEBUG, "Deleted block %d from database", deleted_block);
	}
	wr_unlock(&blk_lock);

	if(!ret)
		set_curblock(hexstr, bedata);
	if(deleted_block)
		applog(LOG_DEBUG, "Deleted block %d from database", deleted_block);

	return ret;
}

static bool test_work_current(struct work *work)
{
	struct pool *pool=work->pool;
	uint8_t bedata[32];
	char hexstr[68];
	bool ret=true;
	uint8_t *bin_height=&pool->coinbase[43];
	uint8_t cb_height_sz=bin_height[-1];
	uint32_t height=0;

	if(work->mandatory)
		return ret;

	swap256(bedata, work->data + 4);
	__bin2hex(hexstr, bedata, 32);

	/* Calculate block height */
	if(cb_height_sz <= 4) {
		memcpy(&height, bin_height, cb_height_sz);
		height=le32toh(height);
		height--;
	}

	cg_wlock(&pool->data_lock);
	if(pool->swork.clean) {
		pool->swork.clean=false;
		work->longpoll=true;
		opt_clean_jobs=true;
	}
	if(pool->current_height != height) {
		pool->current_height=height;
	}
	cg_wunlock(&pool->data_lock);

	/* Search to see if this block exists yet and if not, consider it a
	 * new block and set the current block details to this one */
	if(!block_exists(hexstr, bedata, work)) {
		/* Copy the information to this pool's prev_block since it
		 * knows the new block exists. */
		cg_memcpy(pool->prev_block, bedata, 32);
		if((new_blocks==1)) {
			ret=false;
			goto out;
		}

		work->work_block=++work_block;

		if(work->longpoll) {
			if(work->stratum) {
				applog(LOG_NOTICE, "Stratum from pool %d detected new block at height %d",
					   pool->pool_no, height);
			} else {
				applog(LOG_NOTICE, "%sLONGPOLL from pool %d detected new block at height %d",
					   work->gbt ? "GBT " : "", pool->pool_no, height);
			}
		} else if(have_longpoll && !pool->gbt_solo)
			applog(LOG_NOTICE, "New block detected on network before pool notification from pool %d at height %d",
				   pool->pool_no, height);
		else if(!pool->gbt_solo)
			applog(LOG_NOTICE, "New block detected on network from pool %d at height %d",
				   pool->pool_no, height);
		restart_threads();
	} else {
		if(memcmp(pool->prev_block, bedata, 32)) {
			/* Work doesn't match what this pool has stored as
			 * prev_block. Let's see if the work is from an old
			 * block or the pool is just learning about a new
			 * block. */
			if(memcmp(bedata, current_block, 32)) {
				/* Doesn't match current block. It's stale */
				applog(LOG_DEBUG, "Stale data from pool %d at height %d", pool->pool_no, height);
				ret=false;
			} else {
				/* Work is from new block and pool is up now
				 * current. */
				applog(LOG_INFO, "Pool %d now up to date at height %d", pool->pool_no, height);
				cg_memcpy(pool->prev_block, bedata, 32);
			}
		}
#if 0
		/* This isn't ideal, this pool is still on an old block but
		 * accepting shares from it. To maintain fair work distribution
		 * we work on it anyway. */
		if(memcmp(bedata, current_block, 32))
			applog(LOG_DEBUG, "Pool %d still on old block", pool->pool_no);
#endif
		if(work->longpoll)
		{
			work->work_block=++work_block;
			if(shared_strategy() || work->pool==current_pool())
			{
				if(work->stratum)
				{
					applog(LOG_NOTICE, "Stratum from pool %d requested work restart", pool->pool_no);
				}
				else
				{
					applog(LOG_NOTICE, "%sLONGPOLL from pool %d requested work restart", work->gbt ? "GBT " : "", pool->pool_no);
				}
				restart_threads();
			}
		}
	}
out:
	work->longpoll=false;

	return ret;
}

static int tv_sort(struct work *worka, struct work *workb)
{
	return worka->tv_staged.tv_sec - workb->tv_staged.tv_sec;
}

static bool work_rollable(struct work *work)
{
	return(!work->clone && work->rolltime);
}

static bool hash_push(struct work *work)
{
	bool rc=true;

	mutex_lock(stgd_lock);
	if(work_rollable(work))
		staged_rollable++;
	if((!getq->frozen)) {
		HASH_ADD_INT(staged_work, id, work);
		HASH_SORT(staged_work, tv_sort);
	} else
		rc=false;
	pthread_cond_broadcast(&getq->cond);
	mutex_unlock(stgd_lock);

	return rc;
}

static void _stage_work(struct work *work)
{
	applog(LOG_DEBUG, "Pushing work from pool %d to hash queue", work->pool->pool_no);
	work->work_block=work_block;
	test_work_current(work);
	work->pool->works++;
	hash_push(work);
}

/* We can't remove the memory used for this struct pool because there may
 * still be work referencing it. We just remove it from the pools list */
void remove_pool(struct pool *pool)
{
	int i, last_pool=total_pools - 1;
	struct pool *other;

	/* Boost priority of any lower prio than this one */
	for(i=0; i<total_pools; i++) {
		other=pools[i];
		if(other->prio>pool->prio)
			other->prio--;
	}

	if(pool->pool_no<last_pool) {
		/* Swap the last pool for this one */
		(pools[last_pool])->pool_no=pool->pool_no;
		pools[pool->pool_no]=pools[last_pool];
	}
	/* Give it an invalid number */
	pool->pool_no=total_pools;
	pool->removed=true;
	total_pools--;
}

/* add a mutex if this needs to be thread safe in the future */
static struct JE {
	char *buf;
	struct JE *next;
} *jedata=NULL;

static void json_escape_free()
{
	struct JE *jeptr=jedata;
	struct JE *jenext;

	jedata=NULL;

	while(jeptr) {
		jenext=jeptr->next;
		free(jeptr->buf);
		free(jeptr);
		jeptr=jenext;
	}
}

static char *json_escape(char *str)
{
	struct JE *jeptr;
	char *buf, *ptr;

	/* 2x is the max, may as well just allocate that */
	ptr=buf=cgmalloc(strlen(str) * 2 + 1);
	jeptr=cgmalloc(sizeof(*jeptr));
	jeptr->buf=buf;
	jeptr->next=jedata;
	jedata=jeptr;

	while(*str) {
		if(*str=='\\' || *str=='"')
			*(ptr++)='\\';

		*(ptr++)=*(str++);
	}

	*ptr='\0';

	return buf;
}

void write_config(FILE *fcfg)
{
	struct opt_table *opt;
	int i;

	/* Write pool values */
	fputs("{\n\"pools\" : [", fcfg);
	for(i=0; i<total_pools; i++) {
		struct pool *pool=priority_pool(i);

		if(pool->quota != 1) {
			fprintf(fcfg, "%s\n\t{\n\t\t\"quota\" : \"%s%s%s%d;%s\",", i>0 ? "," : "",
				pool->rpc_proxy ? json_escape((char *)proxytype(pool->rpc_proxytype)) : "",
				pool->rpc_proxy ? json_escape(pool->rpc_proxy) : "",
				pool->rpc_proxy ? "|" : "",
				pool->quota,
				json_escape(pool->rpc_url));
		} else {
			fprintf(fcfg, "%s\n\t{\n\t\t\"url\" : \"%s%s%s%s\",", i>0 ? "," : "",
				pool->rpc_proxy ? json_escape((char *)proxytype(pool->rpc_proxytype)) : "",
				pool->rpc_proxy ? json_escape(pool->rpc_proxy) : "",
				pool->rpc_proxy ? "|" : "",
				json_escape(pool->rpc_url));
		}
		if(pool->extranonce_subscribe)
                    fputs("\n\t\t\"extranonce-subscribe\" : true,", fcfg);
		fprintf(fcfg, "\n\t\t\"user\" : \"%s\",", json_escape(pool->rpc_user));
		fprintf(fcfg, "\n\t\t\"pass\" : \"%s\"\n\t}", json_escape(pool->rpc_pass));
		}
	fputs("\n]\n", fcfg);

	/* Simple bool,int and char options */
	for(opt=opt_config_table; opt->type != OPT_END; opt++) {
		char *p, *name=strdup(opt->names);

		for(p=strtok(name, "|"); p; p=strtok(NULL, "|")) {
			if(p[1] != '-')
				continue;

			if(opt->desc==opt_hidden)
				continue;

			if(opt->type & OPT_NOARG &&
			   ((void *)opt->cb==(void *)opt_set_bool || (void *)opt->cb==(void *)opt_set_invbool) &&
			   (*(bool *)opt->u.arg==((void *)opt->cb==(void *)opt_set_bool)))
			{
				fprintf(fcfg, ",\n\"%s\" : true", p+2);
				continue;
			}

			if(opt->type & OPT_HASARG &&
				((void *)opt->cb_arg==(void *)opt_set_intval ||
				 (void *)opt->cb_arg==(void *)set_int_0_to_9999 ||
				 (void *)opt->cb_arg==(void *)set_int_0_to_65535 ||
				 (void *)opt->cb_arg==(void *)set_int_1_to_65535 ||
				 (void *)opt->cb_arg==(void *)set_int_0_to_10 ||
				 (void *)opt->cb_arg==(void *)set_int_24_to_32 ||
				 (void *)opt->cb_arg==(void *)set_int_0_to_100 ||
				 (void *)opt->cb_arg==(void *)set_int_0_to_255 ||
				 (void *)opt->cb_arg==(void *)set_int_0_to_7680 ||
				 (void *)opt->cb_arg==(void *)set_int_0_to_200 ||
				 (void *)opt->cb_arg==(void *)set_int_0_to_4))
			{
				fprintf(fcfg, ",\n\"%s\" : \"%d\"", p+2, *(int *)opt->u.arg);
				continue;
			}

			if(opt->type & OPT_HASARG &&
				(((void *)opt->cb_arg==(void *)set_float_125_to_500) ||
				 (void *)opt->cb_arg==(void *)set_float_100_to_250))
			{
				fprintf(fcfg, ",\n\"%s\" : \"%.1f\"", p+2, *(float *)opt->u.arg);
				continue;
			}

			if(opt->type & (OPT_HASARG | OPT_PROCESSARG) &&
				(opt->u.arg != &opt_set_null))
			{
				char *carg=*(char **)opt->u.arg;
				if(carg)
				{
					fprintf(fcfg, ",\n\"%s\" : \"%s\"", p+2, json_escape(carg));
				}
				continue;
			}
		}
		free(name);
	}

	/* Special case options */
	if(pool_strategy==POOL_BALANCE)
		fputs(",\n\"balance\" : true", fcfg);
	if(pool_strategy==POOL_LOADBALANCE)
		fputs(",\n\"load-balance\" : true", fcfg);
	if(pool_strategy==POOL_ROUNDROBIN)
		fputs(",\n\"round-robin\" : true", fcfg);
	if(pool_strategy==POOL_ROTATE)
		fprintf(fcfg, ",\n\"rotate\" : \"%d\"", opt_rotate_period);
	fputs("\n}\n", fcfg);

	json_escape_free();
}

void zero_bestshare(void)
{
	int i;

	best_diff=0;
	memset(best_share, 0, 8);
	suffix_string(best_diff, best_share, sizeof(best_share), 0);

	for(i=0; i<total_pools; i++) {
		struct pool *pool=pools[i];
		pool->best_diff=0;
	}
}

static struct timeval tv_hashmeter;
static time_t hashdisplay_t;

void zero_stats(void)
{
	int i;

	cgtime(&total_tv_start);
	copy_time(&tv_hashmeter, &total_tv_start);
	g_hr_rolling1m=0;
	g_hr_rolling5m=0;
	g_hr_rolling15m=0;
	total_ghashes_done=0;
	total_getworks=0;
	total_accepted=0;
	total_rejected=0;
	hw_errors=0;
	total_stale=0;
	total_discarded=0;
	local_work=0;
	total_go=0;
	total_ro=0;
	total_secs=1.0;
	total_diff1=0;
	found_blocks=0;
	total_diff_accepted=0;
	total_diff_rejected=0;
	total_diff_stale=0;

	for(i=0; i<total_pools; i++) {
		struct pool *pool=pools[i];

		pool->getwork_requested=0;
		pool->accepted=0;
		pool->rejected=0;
		pool->stale_shares=0;
		pool->discarded_work=0;
		pool->getfail_occasions=0;
		pool->remotefail_occasions=0;
		pool->last_share_time=0;
		pool->diff1=0;
		pool->diff_accepted=0;
		pool->diff_rejected=0;
		pool->diff_stale=0;
		pool->last_share_diff=0;
	}

	zero_bestshare();

	for(i=0; i<total_devices; ++i)
	{
		struct cgpu_info *cgpu=get_devices(i);
		copy_time(&cgpu->dev_start_tv, &total_tv_start);
		mutex_lock(&hash_lock);
		cgpu->total_mhashes=0;
		cgpu->accepted=0;
		cgpu->rejected=0;
		cgpu->hw_errors=0;
		cgpu->utility=0.0;
		cgpu->last_share_pool_time=0;
		cgpu->diff1=0;
		cgpu->diff_accepted=0;
		cgpu->diff_rejected=0;
		cgpu->last_share_diff=0;
		mutex_unlock(&hash_lock);

		/* Don't take any locks in the driver zero stats function, as
		 * it's called async from everything else and we don't want to
		 * deadlock. */
		cgpu->drv->zero_stats(cgpu);
	}
}

static void set_highprio(void)
{
#ifndef WIN32
	int ret=nice(-10);

	if(!ret)
		applog(LOG_DEBUG, "Unable to set thread to high priority");
#else
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
#endif
}

static void set_lowprio(void)
{
#ifndef WIN32
	int ret=nice(10);

	if(!ret)
		applog(LOG_INFO, "Unable to set thread to low priority");
#else
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
#endif
}

static void *api_thread(void *userdata)
{
	struct thr_info *mythr=userdata;

	pthread_detach(pthread_self());
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	RenameThread("API");

	set_lowprio();
	api(api_thr_id);

	mythr->pth=0UL;

	return NULL;
}

/* Sole work devices are serialised wrt calling get_work so they report in on
 * each pass through their scanhash function as well as in get_work whereas
 * queued work devices work asynchronously so get them to report in and out
 * only across get_work. */
static void thread_reportin(struct thr_info *thr)
{
	thr->getwork=false;
	cgtime(&thr->last);
	thr->cgpu->status=LIFE_WELL;
	thr->cgpu->device_last_well=time(NULL);
}

/* Tell the watchdog thread this thread is waiting on get work and should not
 * be restarted */
static void thread_reportout(struct thr_info *thr)
{
	thr->getwork=true;
	cgtime(&thr->last);
	thr->cgpu->status=LIFE_WELL;
	thr->cgpu->device_last_well=time(NULL);
}

static void hashmeter(int thr_id, uint64_t hashes_done)
{
	bool showlog=false;
	double tv_tdiff;
	double mega_hashes_done=hashes_done/1000000;
	double giga_hashes_done=mega_hashes_done/1000;
	time_t now_t;
	int diff_t;

	cgtime(&total_tv_end);
	tv_tdiff=tdiff(&total_tv_end, &tv_hashmeter);
	now_t=total_tv_end.tv_sec;
	diff_t=now_t - hashdisplay_t;
	if(diff_t >= opt_log_interval)
	{
		alt_status ^= switch_status;
		hashdisplay_t=now_t;
		showlog=true;
	}
	else if(thr_id<0)
	{
		/* hashmeter is called by non-mining threads in case nothing
		 * has reported in to allow hashrate to converge to zero , but
		 * we only update if it has been more than opt_log_interval */
		return;
	}
	copy_time(&tv_hashmeter, &total_tv_end);

	if(thr_id >= 0)
	{
		struct thr_info *thr=get_thread(thr_id);
		struct cgpu_info *cgpu=thr->cgpu;
		double device_tdiff;

		/* Update the last time this thread reported in */
		copy_time(&thr->last, &total_tv_end);
		cgpu->device_last_well=now_t;
		device_tdiff=tdiff(&total_tv_end, &cgpu->last_message_tv);
		copy_time(&cgpu->last_message_tv, &total_tv_end);

		mutex_lock(&hash_lock);
		cgpu->total_mhashes+=mega_hashes_done;
		decay_time(&cgpu->rolling1, mega_hashes_done, device_tdiff, 60.0);
		decay_time(&cgpu->rolling5, mega_hashes_done, device_tdiff, 300.0);
		decay_time(&cgpu->rolling15, mega_hashes_done, device_tdiff, 900.0);
		mutex_unlock(&hash_lock);

		if(want_per_device_stats && showlog)
		{
			char logline[256];
			get_statline(logline, sizeof(logline), cgpu);
			printf("%s          \r", logline);
			fflush(stdout);
			//applog(LOG_INFO, "%s", logline);
		}
	}
	else
	{
		/* No device has reported in, we have been called from the
		 * watchdog thread so decay all the hashrates */
		mutex_lock(&hash_lock);
		for(thr_id=0; thr_id<mining_threads; thr_id++)
		{
			struct thr_info *thr=get_thread(thr_id);
			struct cgpu_info *cgpu=thr->cgpu;
			double device_tdiff =tdiff(&total_tv_end, &cgpu->last_message_tv);

			copy_time(&cgpu->last_message_tv, &total_tv_end);
			decay_time(&cgpu->rolling1, 0, device_tdiff, 60.0);
			decay_time(&cgpu->rolling5, 0, device_tdiff, 300.0);
			decay_time(&cgpu->rolling15, 0, device_tdiff, 900.0);
		}
		mutex_unlock(&hash_lock);
	}

	uint64_t hrval_u64;
	char displayed_ghs_r1m[16];
	char displayed_ghs_r5m[16];
	char displayed_ghs_r15m[16];
	char displayed_ghs_av[16];

	mutex_lock(&hash_lock);
	total_ghashes_done+=giga_hashes_done;
	decay_time(&g_hr_rolling1m, giga_hashes_done, tv_tdiff, 60.0);
	decay_time(&g_hr_rolling5m, giga_hashes_done, tv_tdiff, 300.0);
	decay_time(&g_hr_rolling15m, giga_hashes_done, tv_tdiff, 900.0);
	total_secs=tdiff(&total_tv_end, &total_tv_start);

	if(showlog)
	{
		hrval_u64=g_hr_rolling1m;
		suffix_string(hrval_u64*1000000000, displayed_ghs_r1m, sizeof(displayed_ghs_av), 4);

		hrval_u64=g_hr_rolling5m;
		suffix_string(hrval_u64*1000000000, displayed_ghs_r5m, sizeof(displayed_ghs_av), 4);

		hrval_u64=g_hr_rolling15m;
		suffix_string(hrval_u64*1000000000, displayed_ghs_r15m, sizeof(displayed_ghs_av), 4);

		hrval_u64=total_ghashes_done/total_secs;
		suffix_string(hrval_u64*1000000000, displayed_ghs_av, sizeof(displayed_ghs_av), 4);

		snprintf(statusline, sizeof(statusline),
			"(1m):%sh/s (5m):%sh/s (15m):%sh/s (avg):%sh/s",
			displayed_ghs_r1m, displayed_ghs_r5m, displayed_ghs_r15m, displayed_ghs_av);
	}
	mutex_unlock(&hash_lock);

#ifdef USE_LIBSYSTEMD
	sd_notifyf(false, "STATUS=%s", statusline);
#endif
	if(showlog)
	{
		printf("%s          \r", statusline);
		fflush(stdout);
		//applog(LOG_INFO, "%s", statusline);
	}
}

static void stratum_share_result(json_t *val, json_t *res_val, json_t *err_val, struct stratum_share *sshare)
{
	struct work *work=sshare->work;
	struct timeval tv_now;
	char hashshow[64];
	gettimeofday(&tv_now, NULL);
	timersub(&tv_now, &sshare->sshare_sent, &work->pool->cgminer_pool_stats.last_share_result_lag);
	if(work->pool->cgminer_pool_stats.last_share_result_lag.tv_sec>0)
	{
		applog(LOG_NOTICE, "Pool %d stratum share result lag time %lu", work->pool->pool_no, work->pool->cgminer_pool_stats.last_share_result_lag.tv_sec);
	}
	show_hash(work, hashshow);
	share_result(val, res_val, err_val, work, hashshow, false, "");
}

/* Parses stratum json responses and tries to find the id that the request
 * matched to and treat it accordingly. */
static bool parse_stratum_response(struct pool *pool, char *s)
{
	json_t *val=NULL, *err_val, *res_val, *id_val;
	struct stratum_share *sshare;
	json_error_t err;
	bool ret=false;
	int id;

	val=json_loads(s, 0, &err);
	if(!val)
	{
		applog(LOG_INFO, "JSON decode failed(%d): %s", err.line, err.text);
		goto out;
	}

	res_val=json_object_get(val, "result");
	err_val=json_object_get(val, "error");
	id_val=json_object_get(val, "id");

	if(json_is_null(id_val) || !id_val) {
		char *ss;

		if(err_val)
			ss=json_dumps(err_val, JSON_INDENT(3));
		else
			ss=strdup("(unknown reason)");

		applog(LOG_INFO, "JSON-RPC non method decode failed: %s", ss);

		free(ss);

		goto out;
	}

	id=json_integer_value(id_val);

	mutex_lock(&sshare_lock);
	HASH_FIND_INT(stratum_shares, &id, sshare);
	if(sshare) {
		HASH_DEL(stratum_shares, sshare);
		pool->sshares--;
	}
	mutex_unlock(&sshare_lock);

	int64_t pool_diff;
	if(!sshare)
	{

		if(!res_val)
			goto out;
		/* Since the share is untracked, we can only guess at what the
		 * work difficulty is based on the current pool diff. */
		cg_rlock(&pool->data_lock);
		pool_diff=pool->stratum_diff;
		cg_runlock(&pool->data_lock);

		if(json_is_true(res_val)) {
			applog(LOG_NOTICE, "Accepted untracked stratum share from pool %d", pool->pool_no);

			/* We don't know what device this came from so we can't
			 * attribute the work to the relevant cgpu */
			mutex_lock(&stats_lock);
			total_accepted++;
			pool->accepted++;
			total_diff_accepted+=pool_diff;
			pool->diff_accepted+=pool_diff;
			mutex_unlock(&stats_lock);
		} else {
			applog(LOG_NOTICE, "Rejected untracked stratum share from pool %d", pool->pool_no);

			mutex_lock(&stats_lock);
			total_rejected++;
			pool->rejected++;
			total_diff_rejected+=pool_diff;
			pool->diff_rejected+=pool_diff;
			mutex_unlock(&stats_lock);
		}
		goto out;
	}
	stratum_share_result(val, res_val, err_val, sshare);
	free_work(&sshare->work);
	free(sshare);

	ret=true;
out:
	if(val)
		json_decref(val);

	return ret;
}

void clear_stratum_shares(struct pool *pool)
{
	struct stratum_share *sshare, *tmpshare;
	double diff_cleared=0;
	int cleared=0;

	mutex_lock(&sshare_lock);
	HASH_ITER(hh, stratum_shares, sshare, tmpshare) {
		if(sshare->work->pool==pool) {
			HASH_DEL(stratum_shares, sshare);
			diff_cleared+=sshare->work->work_difficulty;
			free_work(&sshare->work);
			pool->sshares--;
			free(sshare);
			cleared++;
		}
	}
	mutex_unlock(&sshare_lock);

	if(cleared) {
		applog(LOG_WARNING, "Lost %d shares due to stratum disconnect on pool %d", cleared, pool->pool_no);
		pool->stale_shares+=cleared;
		total_stale+=cleared;
		pool->diff_stale+=diff_cleared;
		total_diff_stale+=diff_cleared;
	}
}

void clear_pool_work(struct pool *pool)
{
	struct work *work, *tmp;
	int cleared=0;

	mutex_lock(stgd_lock);
	HASH_ITER(hh, staged_work, work, tmp) {
		if(work->pool==pool) {
			HASH_DEL(staged_work, work);
			free_work(&work);
			cleared++;
		}
	}
	mutex_unlock(stgd_lock);

	if(cleared)
		applog(LOG_INFO, "Cleared %d work items due to stratum disconnect on pool %d", cleared, pool->pool_no);
}

static int cp_prio(void)
{
	int prio;

	cg_rlock(&control_lock);
	prio=currentpool->prio;
	cg_runlock(&control_lock);

	return prio;
}

/* We only need to maintain a secondary pool connection when we need the
 * capacity to get work from the backup pools while still on the primary */
static bool cnx_needed(struct pool *pool)
{
	struct pool *cp;

	if(pool->enabled != POOL_ENABLED)
		return false;

	/* Balance strategies need all pools online */
	if(pool_strategy==POOL_BALANCE)
		return true;
	if(pool_strategy==POOL_LOADBALANCE)
		return true;

	/* Idle stratum pool needs something to kick it alive again */
	if(pool->has_stratum && pool->idle)
		return true;

	cp=current_pool();
	if(cp==pool)
		return true;
	/* If we're waiting for a response from shares submitted, keep the
	 * connection open. */
	if(pool->sshares)
		return true;
	/* If the pool has only just come to life and is higher priority than
	 * the current pool keep the connection open so we can fail back to
	 * it. */
	if(pool_strategy==POOL_FAILOVER && pool->prio<cp_prio())
		return true;
	/* We've run out of work, bring anything back to life. */
	if(no_work)
		return true;
	return false;
}

static void wait_lpcurrent(struct pool *pool);
static void pool_resus(struct pool *pool);

/* Generates stratum based work based on the most recent notify information
 * from the pool. This will keep generating work while a pool is down so we use
 * other means to detect when the pool has died in stratum_thread */
static void gen_stratum_work(struct pool *pool, struct work *work)
{
	uint8_t merkle_root[32], merkle_sha[64];
	uint32_t *data32, *swap32;
	uint64_t nonce2le;
	int i;

	cg_wlock(&pool->data_lock);

	/* Update coinbase. Always use an LE encoded nonce2 to fill in values
	 * from left to right and prevent overflow errors with small n2sizes */
	nonce2le=htole64(pool->nonce2);
	cg_memcpy(pool->coinbase + pool->nonce2_offset, &nonce2le, pool->n2size);
	work->nonce2=pool->nonce2++;
	work->nonce2_len=pool->n2size;

	/* Downgrade to a read lock to read off the pool variables */
	cg_dwlock(&pool->data_lock);

	/* Generate merkle root */
	gen_hash(pool->coinbase, merkle_root, pool->coinbase_len);
	cg_memcpy(merkle_sha, merkle_root, 32);
	for(i=0; i<pool->merkles; i++)
	{
		cg_memcpy(merkle_sha + 32, pool->swork.merkle_bin[i], 32);
		gen_hash(merkle_sha, merkle_root, 64);
		cg_memcpy(merkle_sha, merkle_root, 32);
	}
	data32=(uint32_t *)merkle_sha;
	swap32=(uint32_t *)merkle_root;
	flip32(swap32, data32);

	/* Copy the data template from header_bin */
	cg_memcpy(work->data, pool->header_bin, 112);
	cg_memcpy(work->data + 36, merkle_root, 32);

	/* Store the stratum work diff to check it still matches the pool's
	 * stratum diff when submitting shares */
	work->stratum_diff=pool->stratum_diff;

	/* Copy parameters required for share submission */
	work->job_id=strdup(pool->swork.job_id);
	work->nonce1=strdup(pool->nonce1);
	work->ntime=strdup(pool->ntime);
	cg_runlock(&pool->data_lock);

	calc_midstate(work);
	set_target(work->target, work->stratum_diff);

	local_work++;
	work->pool=pool;
	work->stratum=true;
	work->nonce=0;
	work->longpoll=false;
	work->getwork_mode=GETWORK_MODE_STRATUM;
	work->work_block=work_block;
	/* Nominally allow a driver to ntime roll 60 seconds */
	work->drv_rolllimit=60;
	calc_diff(work, work->stratum_diff);

	cgtime(&work->tv_staged);
}

void stratum_resumed(struct pool *pool)
{
	if(pool_tclear(pool, &pool->idle)) {
		applog(LOG_INFO, "Stratum connection to pool %d resumed", pool->pool_no);
		pool_resus(pool);
	}
}

static bool supports_resume(struct pool *pool)
{
	bool ret;

	cg_rlock(&pool->data_lock);
	ret=(pool->sessionid != NULL);
	cg_runlock(&pool->data_lock);

	return ret;
}

/* One stratum receive thread per pool that has stratum waits on the socket
 * checking for new messages and for the integrity of the socket connection. We
 * reset the connection based on the integrity of the receive side only as the
 * send side will eventually expire data it fails to send. */
static void *stratum_rthread(void *userdata)
{
	struct pool *pool=(struct pool *)userdata;
	char threadname[16];

	pthread_detach(pthread_self());

	snprintf(threadname, sizeof(threadname), "%d/RStratum", pool->pool_no);
	RenameThread(threadname);

	while(42) {
		struct timeval timeout;
		int sel_ret;
		fd_set rd;
		char *s;

		if((pool->removed)) {
			suspend_stratum(pool);
			break;
		}

		/* Check to see whether we need to maintain this connection
		 * indefinitely or just bring it up when we switch to this
		 * pool */
		if(!sock_full(pool) && !cnx_needed(pool)) {
			suspend_stratum(pool);
			clear_stratum_shares(pool);
			clear_pool_work(pool);

			wait_lpcurrent(pool);
			while(!restart_stratum(pool)) {
				pool_died(pool);
				if(pool->removed)
					goto out;
				cgsleep_ms(5000);
			}
		}

		FD_ZERO(&rd);
		FD_SET(pool->sock, &rd);
		timeout.tv_sec=90;
		timeout.tv_usec=0;

		/* The protocol specifies that notify messages should be sent
		 * every minute so if we fail to receive any for 90 seconds we
		 * assume the connection has been dropped and treat this pool
		 * as dead */
		if(!sock_full(pool) && (sel_ret=select(pool->sock + 1, &rd, NULL, NULL, &timeout))<1) {
			applog(LOG_DEBUG, "Stratum select failed on pool %d with value %d", pool->pool_no, sel_ret);
			s=NULL;
		} else
			s=recv_line(pool);
		if(!s) {
			applog(LOG_NOTICE, "Stratum connection to pool %d interrupted", pool->pool_no);
			pool->getfail_occasions++;
			total_go++;

			/* If the socket to our stratum pool disconnects, all
			 * tracked submitted shares are lost and we will leak
			 * the memory if we don't discard their records. */
			if(!supports_resume(pool) || opt_lowmem)
				clear_stratum_shares(pool);
			clear_pool_work(pool);
			if(pool==current_pool())
				restart_threads();

			while(!restart_stratum(pool)) {
				pool_died(pool);
				if(pool->removed)
					goto out;
				cgsleep_ms(5000);
			}
			continue;
		}

		/* Check this pool hasn't died while being a backup pool and
		 * has not had its idle flag cleared */
		stratum_resumed(pool);

		struct work *work;
		if(!parse_method(pool, s) && !parse_stratum_response(pool, s))
		{
			applog(LOG_INFO, "Unknown stratum msg: %s", s);
		}
		else if(pool->swork.clean)
		{
			work=make_work();

			/* Generate a single work item to update the current
			 * block database */
			gen_stratum_work(pool, work);

			/* Return value doesn't matter. We're just informing
			 * that we may need to restart. */
			test_work_current(work);
			free_work(&work);
		}
		free(s);
	}

out:
	return NULL;
}

/* Each pool has one stratum send thread for sending shares to avoid many
 * threads being created for submission since all sends need to be serialised
 * anyway. */
static void *stratum_sthread(void *userdata)
{
	struct pool *pool=(struct pool *)userdata;
	struct stratum_share *sshare;
	struct work *work;
	char threadname[16];
	char noncehex[12], nonce2hex[20], s[1024];
	uint32_t *hash32, nonce;
	bool submitted;
	uint64_t last_nonce2=0;
	uint32_t last_nonce=0;
#if defined(ENABLE_ASIC_BOOST)
	uint32_t last_version=0;
#endif

	pthread_detach(pthread_self());

	snprintf(threadname, sizeof(threadname), "%d/SStratum", pool->pool_no);
	RenameThread(threadname);

	pool->stratum_q=tq_new();
	if(!pool->stratum_q)
	{
		quit(1, "Failed to create stratum_q in stratum_sthread");
	}

	while(42)
	{
		if(pool->removed)
		{
			applog(LOG_NOTICE, "Pool %d has been removed.", pool->pool_no);
			break;
		}

		work=tq_pop(pool->stratum_q);
		if(!work)
		{
			applog(LOG_ERR, "Stratum q returned empty work");
			break;
		}

		if(work->nonce2_len>8)
		{
			applog(LOG_ERR, "Pool %d asking for inappropriately long nonce2 length %d", pool->pool_no, (int)work->nonce2_len);
			applog(LOG_ERR, "Not attempting to submit shares");
			free_work(&work);
			continue;
		}
		nonce=*(uint32_t *)&work->data[76];
		/* Filter out duplicate shares */
		if(last_nonce==nonce && last_nonce2==work->nonce2)
		{
#if defined(ENABLE_ASIC_BOOST)
			if(last_version==work->version)
#endif
			{
				applog(LOG_INFO, "Filtering duplicate share to pool %d", pool->pool_no);
				free_work(&work);
				continue;
			}
		}
		last_nonce=nonce;
		last_nonce2=work->nonce2;
#if defined(ENABLE_ASIC_BOOST)
		last_version=work->version;
#endif
		__bin2hex(noncehex, (const uint8_t *)&nonce, 4);
		__bin2hex(nonce2hex, (const uint8_t *)&work->nonce2, work->nonce2_len);

		sshare=cgcalloc(sizeof(struct stratum_share), 1);
		hash32=(uint32_t *)work->hash;
		submitted=false;

		sshare->sshare_time=time(NULL);
		/* This work item is freed in parse_stratum_response */
		sshare->work=work;
		memset(s, 0, 1024);

		mutex_lock(&sshare_lock);
		/* Give the stratum share a unique id */
		sshare->id=swork_id++;
		mutex_unlock(&sshare_lock);
#if defined(ENABLE_ASIC_BOOST)
		if(pool->supports_version_rolling)
		{
			applog(LOG_INFO, "Submitting share with work version 0x%08X\nstratum_diff %lli\nshare_diff %lli\nwork_difficulty %lli",
				   work->version,
				   work->stratum_diff,
				   work->share_diff,
				   work->work_difficulty);
			snprintf(s, sizeof(s), "{\"params\": [\"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%08x\"], \"id\": %d, \"method\": \"mining.submit\"}",
				pool->rpc_user,
				work->job_id,
				nonce2hex,
				work->ntime,
				noncehex,
				work->version,
				sshare->id);
		}
		else
#endif
		{
			applog(LOG_INFO, "Submitting share %08lx to pool %d", (long unsigned int)htole32(hash32[6]), pool->pool_no);
			snprintf(s, sizeof(s), "{\"params\": [\"%s\", \"%s\", \"%s\", \"%s\", \"%s\"], \"id\": %d, \"method\": \"mining.submit\"}",
				pool->rpc_user,
				work->job_id,
				nonce2hex,
				work->ntime,
				noncehex,
				sshare->id);
		}

		/* Try resubmitting for up to 2 minutes if we fail to submit
		 * once and the stratum pool nonce1 still matches suggesting
		 * we may be able to resume. */
		while(time(NULL)<sshare->sshare_time + 120) {
			bool sessionid_match;

			if((stratum_send(pool, s, strlen(s)))) {
				mutex_lock(&sshare_lock);
				HASH_ADD_INT(stratum_shares, id, sshare);
				pool->sshares++;
				mutex_unlock(&sshare_lock);

				if(pool_tclear(pool, &pool->submit_fail))
						applog(LOG_WARNING, "Pool %d communication resumed, submitting work", pool->pool_no);
				applog(LOG_DEBUG, "Successfully submitted, adding to stratum_shares db");
				submitted=true;
				break;
			}
			if(!pool_tset(pool, &pool->submit_fail) && cnx_needed(pool)) {
				applog(LOG_WARNING, "Pool %d stratum share submission failure", pool->pool_no);
				total_ro++;
				pool->remotefail_occasions++;
			}

			if(opt_lowmem) {
				applog(LOG_DEBUG, "Lowmem option prevents resubmitting stratum share");
				break;
			}

			cg_rlock(&pool->data_lock);
			sessionid_match=(pool->nonce1 && !strcmp(work->nonce1, pool->nonce1));
			cg_runlock(&pool->data_lock);

			if(!sessionid_match) {
				applog(LOG_DEBUG, "No matching session id for resubmitting stratum share");
				break;
			}
			/* Retry every 5 seconds */
			sleep(5);
		}

		if((!submitted))
		{
			applog(LOG_DEBUG, "Failed to submit stratum share, discarding");
			free_work(&work);
			free(sshare);
			pool->stale_shares++;
			total_stale++;
		}
		else
		{
			int ssdiff;
			gettimeofday(&sshare->sshare_sent, NULL);
			ssdiff=sshare->sshare_sent.tv_sec - sshare->sshare_time;
			if(ssdiff>0)
			{
				applog(LOG_INFO, "Pool %d stratum share submission lag time %d seconds", pool->pool_no, ssdiff);
			}
		}
	}

	/* Freeze the work queue but don't free up its memory in case there is
	 * work still trying to be submitted to the removed pool. */
	tq_freeze(pool->stratum_q);

	return NULL;
}

static void init_stratum_threads(struct pool *pool)
{
	have_longpoll=true;

	if((pthread_create(&pool->stratum_sthread, NULL, stratum_sthread, (void *)pool)))
		quit(1, "Failed to create stratum sthread");
	if((pthread_create(&pool->stratum_rthread, NULL, stratum_rthread, (void *)pool)))
		quit(1, "Failed to create stratum rthread");
}

static bool stratum_works(struct pool *pool)
{
	//applog(LOG_INFO, "Testing pool %d stratum %s", pool->pool_no, pool->stratum_url);
	if(!extract_sockaddr(pool->stratum_url, &pool->sockaddr_url, &pool->stratum_port))
		return false;

	if(!initiate_stratum(pool))
		return false;

	return true;
}

static bool pool_active(struct pool *pool, bool pinging)
{
	struct timeval tv_getwork, tv_getwork_reply;
	json_t *val=NULL;
	bool ret=false;

	/* This is the central point we activate stratum when we can */
retry_stratum:
	if(pool->has_stratum)
	{
		/* We create the stratum thread for each pool just after
		 * successful authorisation. Once the init flag has been set
		 * we never unset it and the stratum thread is responsible for
		 * setting/unsetting the active flag */
		bool init=pool_tset(pool, &pool->stratum_init);

		if(!init)
		{
			bool ret=initiate_stratum(pool) && (!pool->extranonce_subscribe || subscribe_extranonce(pool)) && auth_stratum(pool);

			if(ret)
			{
				init_stratum_threads(pool);
			}
			else
			{
				pool_tclear(pool, &pool->stratum_init);
			}
			return ret;
		}
		return pool->stratum_active;
	}

	/* Probe for GBT support on first pass */
	if(!pool->probed)
	{
		applog(LOG_DEBUG, "Probing for GBT support");
		json_t *rules_arr;
		applog(LOG_DEBUG, "Probing for GBT solo support");
		rules_arr=json_object_get(val, "rules");
		if(!gbt_check_rules(rules_arr, gbt_solo_understood_rules))
		{
			applog(LOG_DEBUG, "Not all rules understood for GBT solo");
			json_decref(val);
			val=NULL;
		}
		/* Reset this so we can probe fully just after this. It will be
		 * set to true that time.*/
		pool->probed=false;

		if(pool->gbt_solo)
			applog(LOG_DEBUG, "GBT coinbase without append found, switching to GBT solo protocol");
		else
			applog(LOG_DEBUG, "No GBT coinbase + append support found, pool unusable if it has no stratum");
	}

	cgtime(&tv_getwork);
	cgtime(&tv_getwork_reply);

	/* Detect if a http pool has an X-Stratum header at startup,
	 * and if so, switch to that in preference to gbt if it works */
	if(pool->stratum_url && !opt_fix_protocol && stratum_works(pool))
	{
		applog(LOG_NOTICE, "Switching pool %d %s to %s", pool->pool_no, pool->rpc_url, pool->stratum_url);
		if(!pool->rpc_url)
			pool->rpc_url=strdup(pool->stratum_url);
		pool->has_stratum=true;
		goto retry_stratum;
	}

	if(!pool->has_stratum && !pool->gbt_solo)
	{
		applog(LOG_WARNING, "No Stratum or Solo support in pool %d %s unable to use", pool->pool_no, pool->rpc_url);
		return false;
	}
	applog(LOG_DEBUG, "FAILED to retrieve work from pool %u %s", pool->pool_no, pool->rpc_url);
	if(!pinging && !pool->idle)
	{
		applog(LOG_WARNING, "Pool %u slow/down or URL or credentials invalid", pool->pool_no);
	}
	return ret;
}

static void pool_resus(struct pool *pool)
{
	pool->seq_getfails=0;
	if(pool_strategy==POOL_FAILOVER && pool->prio<cp_prio())
		applog(LOG_WARNING, "Pool %d %s alive, testing stability", pool->pool_no, pool->rpc_url);
	else
		applog(LOG_INFO, "Pool %d %s alive", pool->pool_no, pool->rpc_url);
}

static bool work_filled;
static bool work_emptied;

/* If this is called non_blocking, it will return NULL for work so that must
 * be handled. */
static struct work *hash_pop(bool blocking)
{
	struct work *work=NULL, *tmp;
	int hc, rc;
	mutex_lock(stgd_lock);
	if(!HASH_COUNT(staged_work))
	{
		work_emptied=true;
		if(!blocking)
		{
			goto out_unlock;
		}
		do
		{
			struct timespec then;
			struct timeval now;
			cgtime(&now);
			then.tv_sec=now.tv_sec + 10;
			then.tv_nsec=now.tv_usec * 1000;
			pthread_cond_signal(&gws_cond);
			rc=pthread_cond_timedwait(&getq->cond, stgd_lock, &then);
			/* Check again for !no_work as multiple threads may be
				* waiting on this condition and another may set the
				* bool separately. */
			if(rc && !no_work)
			{
				no_work=true;
				applog(LOG_WARNING, "Waiting for work to be available from pools.");
			}
		} while(!HASH_COUNT(staged_work));
	}

	if(no_work)
	{
		no_work=false;
		applog(LOG_WARNING, "Work available from pools, resuming.");
	}

	hc=HASH_COUNT(staged_work);
	/* Find clone work if possible, to allow masters to be reused */
	if(hc>staged_rollable) {
		HASH_ITER(hh, staged_work, work, tmp) {
			if(!work_rollable(work))
				break;
		}
	} else
		work=staged_work;
	HASH_DEL(staged_work, work);
	if(work_rollable(work))
		staged_rollable--;

	/* Signal the getwork scheduler to look for more work */
	pthread_cond_signal(&gws_cond);

	/* Signal hash_pop again in case there are mutliple hash_pop waiters */
	pthread_cond_signal(&getq->cond);

	/* Keep track of last getwork grabbed */
	last_getwork=time(NULL);
out_unlock:
	mutex_unlock(stgd_lock);

	return work;
}

void gen_hash(uint8_t *data, uint8_t *hash, int len)
{
	uint8_t hash1[32];
	sha256(data, len, hash1);
	sha256(hash1, 32, hash);
}

void set_target(uint8_t *dest_target, int64_t diff)
{
	uint8_t target[32];
	uint64_t *data64, h64;
	double d64, dcut64;

	if(diff<1)
	{
		diff=1;
		/* This shouldn't happen but best we check to prevent a crash */
		applog(LOG_ERR, "Incorrect diff passed to set_target");
	}

	d64=truediffone;
#if defined(SCRYPT_H)
	d64 *= (double)65536;
#endif
	d64 /= diff;

	dcut64=d64/bits192;
	h64=dcut64;
	data64=(uint64_t *)(target + 24);
	*data64=htole64(h64);
	dcut64=h64;
	dcut64 *= bits192;
	d64 -= dcut64;

	dcut64=d64/bits128;
	h64=dcut64;
	data64=(uint64_t *)(target + 16);
	*data64=htole64(h64);
	dcut64=h64;
	dcut64 *= bits128;
	d64 -= dcut64;

	dcut64=d64/bits64;
	h64=dcut64;
	data64=(uint64_t *)(target + 8);
	*data64=htole64(h64);
	dcut64=h64;
	dcut64 *= bits64;
	d64 -= dcut64;

	h64=d64;
	data64=(uint64_t *)(target);
	*data64=htole64(h64);

	if(opt_debug)
	{
		char *htarget=bin2hex(target, 32);
		applog(LOG_DEBUG, "Generated target %s", htarget);
		free(htarget);
	}
	cg_memcpy(dest_target, target, 32);
}

#if defined(USE_AVALON2) || defined(USE_AVALON4) || defined(USE_AVALON7) || defined(USE_AVALON8) || defined(USE_AVALON9)
bool submit_nonce2_nonce(struct thr_info *thr, struct pool *pool, struct pool *real_pool, uint32_t nonce2, uint32_t nonce,  uint32_t ntime)
{
	const int thr_id=thr->id;
	struct cgpu_info *cgpu=thr->cgpu;
	struct device_drv *drv=cgpu->drv;
	struct work *work=make_work();
	bool ret;

	cg_wlock(&pool->data_lock);
	pool->nonce2=nonce2;
	cg_wunlock(&pool->data_lock);

	gen_stratum_work(pool, work);
	roll_work_ntime(work, ntime);

	work->pool=real_pool;
	/* Inherit the sdiff from the original stratum */
	work->stratum_diff=pool->stratum_diff;

	work->thr_id=thr_id;
	work->work_block=work_block;
	work->pool->works++;

	work->mined=true;

	ret=submit_nonce(thr, work);
	free_work(&work);
	return ret;
}
#endif

static void update_work_stats(struct thr_info *thr, struct work *work)
{
	int64_t test_diff=current_diff;

	work->share_diff=share_diff(work);

#if defined(SCRYPT_H)
	test_diff *= (double)65536;
#endif

	if(work->share_diff >= test_diff)
	{
		work->block=true;
		work->pool->solved++;
		found_blocks++;
		work->mandatory=true;
		applog(LOG_NOTICE, "Found block for pool %d!", work->pool->pool_no);
	}

	mutex_lock(&stats_lock);
//	total_diff1+=work->device_diff;
//	thr->cgpu->diff1+=work->device_diff;
//	work->pool->diff1+=work->device_diff;
	thr->cgpu->last_device_valid_work=time(NULL);
	mutex_unlock(&stats_lock);
}

/* Submit a copy of the tested, statistic recorded work item asynchronously */
static void submit_work_async(struct work *work)
{
	struct pool *pool=work->pool;
	pthread_t submit_thread;
	cgtime(&work->tv_work_found);
	if(opt_benchmark)
	{
		struct cgpu_info *cgpu=get_thr_cgpu(work->thr_id);
		mutex_lock(&stats_lock);
		cgpu->accepted++;
		total_accepted++;
		pool->accepted++;
		cgpu->diff_accepted+=work->work_difficulty;
		total_diff_accepted+=work->work_difficulty;
		pool->diff_accepted+=work->work_difficulty;
		mutex_unlock(&stats_lock);
		//applog(LOG_NOTICE, "Accepted %s %i benchmark share nonce %08x", cgpu->drv->name, cgpu->device_id, *(uint32_t *)(work->data + 64 + 12));
		return;
	}
	if(stale_work(work, true))
	{
		if(opt_submit_stale)
		{
			applog(LOG_NOTICE, "Pool %d stale share detected, submitting as user requested", pool->pool_no);
		}
		else if(pool->submit_old)
		{
			applog(LOG_NOTICE, "Pool %d stale share detected, submitting as pool requested", pool->pool_no);
		}
		else
		{
			applog(LOG_NOTICE, "Pool %d stale share detected, discarding", pool->pool_no);
			sharelog("discard", work);

			mutex_lock(&stats_lock);
			total_stale++;
			pool->stale_shares++;
			total_diff_stale+=work->work_difficulty;
			pool->diff_stale+=work->work_difficulty;
			mutex_unlock(&stats_lock);

			free_work(&work);
			return;
		}
		work->stale=true;
	}

	if(work->stratum)
	{
		applog(LOG_DEBUG, "Pushing pool %d work to stratum queue", pool->pool_no);
		if(!pool->stratum_q || !tq_push(pool->stratum_q, work))
		{
			applog(LOG_DEBUG, "Discarding work from removed pool");
			free_work(&work);
		}
	}
	else
	{
		applog(LOG_DEBUG, "Pushing submit work to work thread");
		if(pthread_create(&submit_thread, NULL, submit_work_thread, (void *)work))
		{
			quit(1, "Failed to create submit_work_thread");
		}
	}
}

void inc_hw_errors(struct thr_info *thr)
{
	mutex_lock(&stats_lock);
	hw_errors++;
	thr->cgpu->hw_errors++;
	mutex_unlock(&stats_lock);
	thr->cgpu->drv->hw_error(thr);
//	applog(LOG_ERR, "%s %d: HW error (invalid nonce)", thr->cgpu->drv->name, thr->cgpu->device_id);
}

#if defined(USE_ANTMINER_L3)
bool submit_nonce_1(struct thr_info *thr, struct work *work, uint32_t nonce, uint8_t *nofull)
{
	bool retcode=true;
	if(test_nonce(work, nonce))
	{
		update_work_stats(thr, work);
		if(!fulltest(work->hash, work->target))
		{
			applog(LOG_DEBUG, "%s: Share above target", __FUNCTION__);
			if(nofull)
			{
				*nofull=1;
			}
			retcode=false;
		}
	}
	else
	{
		retcode=false;
	}
	return retcode;
}

void submit_nonce_2(struct work *work)
{
	struct work *work_out;
	work_out=copy_work(work);
	submit_work_async(work_out);
}
#endif //USE_ANTMINER_L3

/* The time difference in seconds between when this device last got work via
 * get_work() and generated a valid share. */
int share_work_tdiff(struct cgpu_info *cgpu)
{
	return last_getwork - cgpu->last_device_valid_work;
}

struct work *get_work(struct thr_info *thr)
{
	struct cgpu_info *cgpu=thr->cgpu;
	struct work *work=NULL;
	time_t dt;
	thread_reportout(thr);
	dt=time(NULL);
	while(!work)
	{
		work=hash_pop(true);
		if(stale_work(work, false))
		{
			discard_work(&work);
			wake_gws();
		}
	}
	dt=time(NULL)-dt;
	/* Since this is a blocking function, we need to add grace time to
	 * the device's last valid work to not make outages appear to be
	 * device failures. */
	if(dt>0)
	{
		cgpu->last_device_valid_work+=dt;
	}
	work->thr_id=thr->id;
	thread_reportin(thr);
	work->mined=true;
	return work;
}

// Fills in the work nonce and builds the output data in work->hash
static void rebuild_nonce(struct work *work)
{
#if defined(ENABLE_ASIC_BOOST)
	uint32_t *work_data_version=(uint32_t *)&work->data[0];
	*work_data_version=bswap_32(work->version) | *(uint32_t *)work->pool->header_bin;
#endif

	uint32_t *work_data_nonce=(uint32_t *)&work->data[76];
	*work_data_nonce=work->nonce;

#if defined(SCRYPT_H)
	scrypt_regen_hash(work);
#else
	regen_hash(work);
#endif
}

// For testing a nonce against an arbitrary diff
bool test_nonce_diff(struct work *work, int64_t diff)
{
	uint64_t diff64, *hash64=(uint64_t *)(work->hash + 24);
#if defined(SCRYPT_H)
	diff64=0x0000ffff00000000ULL;
#else
	diff64=0x00000000ffff0000ULL;
#endif
	diff64 /= diff;
	return(*hash64 <= diff64);
}

// For submiting a work_item that was rebuilt with new nonce and then tested against target_diff
void submit_tested_work(struct thr_info *thr, struct work *work)
{
	struct work *work_out;
	update_work_stats(thr, work);
	work_out=copy_work(work);
	submit_work_async(work_out);
}

// Returns true if nonce for work was a valid share
bool submit_nonce(struct thr_info *thr, struct work *work)
{
	bool target_diff_was_reached;
	rebuild_nonce(work);
//	target_diff_was_reached=test_nonce_diff(work, work->work_difficulty);
	target_diff_was_reached=fulltest(work->hash, work->target);
	if(target_diff_was_reached)
	{
		submit_tested_work(thr, work);
	}
	else
	{
		inc_hw_errors(thr);
	}
	return(target_diff_was_reached);
}

static inline bool abandon_work(struct work *work, struct timeval *wdiff, uint64_t hashes)
{
	if(wdiff->tv_sec>max_scantime || hashes >= 0xfffffffe || stale_work(work, false))
	{
		return true;
	}
	return false;
}

static void mt_disable(struct thr_info *thr, struct device_drv *drv)
{
	applog(LOG_WARNING, "Thread %d being disabled", thr->id);
	applog(LOG_DEBUG, "Waiting on sem in miner thread");
	cgsem_wait(&thr->sem);
	applog(LOG_WARNING, "Thread %d being re-enabled", thr->id);
	drv->thread_enable(thr);
}

void dev_error(struct cgpu_info *dev, enum dev_reason reason)
{
	dev->device_last_not_well=time(NULL);
	dev->device_not_well_reason=reason;
	switch (reason)
	{
		case REASON_THREAD_FAIL_INIT:
			dev->thread_fail_init_count++;
			break;
		case REASON_THREAD_ZERO_HASH:
			dev->thread_zero_hash_count++;
			break;
		case REASON_THREAD_FAIL_QUEUE:
			dev->thread_fail_queue_count++;
			break;
		case REASON_DEV_SICK_IDLE_60:
			dev->dev_sick_idle_60_count++;
			break;
		case REASON_DEV_DEAD_IDLE_600:
			dev->dev_dead_idle_600_count++;
			break;
		case REASON_DEV_NOSTART:
			dev->dev_nostart_count++;
			break;
		case REASON_DEV_OVER_HEAT:
			dev->dev_over_heat_count++;
			break;
		case REASON_DEV_THERMAL_CUTOFF:
			dev->dev_thermal_cutoff_count++;
			break;
		case REASON_DEV_COMMS_ERROR:
			dev->dev_comms_error_count++;
			break;
		case REASON_DEV_THROTTLE:
			dev->dev_throttle_count++;
			break;
	}
}

/* The main hashing loop for devices that are slow enough to work on one work
 * item at a time, without a queue, aborting work before the entire nonce
 * range has been hashed if needed. */
static void hash_sole_work(struct thr_info *thr)
{
	const int thr_id=thr->id;
	struct cgpu_info *cgpu=thr->cgpu;
	struct device_drv *drv=cgpu->drv;
	struct timeval getwork_start, tv_start, *tv_end, tv_workstart, tv_lastupdate;
	/* Try to cycle approximately 5 times before each log update */
	const long cycle=opt_log_interval/5 ? opt_log_interval/5 : 1;
	const bool primary=(!thr->device_thread) || thr->primary_thread;
	struct timeval diff, sdiff, wdiff={0, 0};
	uint32_t max_nonce=drv->can_limit_work(thr);
	int64_t hashes_done=0;

	tv_end=&getwork_start;
	cgtime(&getwork_start);
	sdiff.tv_sec=sdiff.tv_usec=0;
	cgtime(&tv_lastupdate);

	while((!cgpu->shutdown))
	{
		struct work *work=get_work(thr);
		int64_t hashes;

		thr->work_restart=false;
		cgpu->new_work=true;

		cgtime(&tv_workstart);
		work->nonce=0;
		cgpu->max_hashes=0;
		if(!drv->prepare_work(thr, work))
		{
			applog(LOG_ERR, "work prepare failed, exiting mining thread %d", thr_id);
			break;
		}
#if defined(SCRYPT_H)
		double wu;
		wu=total_diff1/total_secs * 60;
		if(wu>30 && drv->working_diff<drv->max_diff && drv->working_diff<work->work_difficulty)
		{
			drv->working_diff++;
			applog(LOG_DEBUG, "Driver %s working diff changed to %.0f", drv->dname, drv->working_diff);
		}
		else if(drv->working_diff>work->work_difficulty)
		{
			drv->working_diff=work->work_difficulty;
		}
		set_target(work->device_target, work->work_difficulty);
#endif

		do
		{
			cgtime(&tv_start);
			subtime(&tv_start, &getwork_start);

			cgtime(&(work->tv_work_start));

			/* Only allow the mining thread to be cancelled when
			 * it is not in the driver code. */
			pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

			thread_reportin(thr);
			hashes=drv->scanhash(thr, work, work->nonce + max_nonce);
			thread_reportout(thr);

			pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
			pthread_testcancel();

			/* tv_end is==&getwork_start */
			cgtime(&getwork_start);

			if((hashes==-1)) {
				applog(LOG_ERR, "%s %d failure, disabling!", drv->name, cgpu->device_id);
				cgpu->deven=DEV_DISABLED;
				dev_error(cgpu, REASON_THREAD_ZERO_HASH);
				cgpu->shutdown=true;
				break;
			}

			hashes_done+=hashes;
			if(hashes>cgpu->max_hashes)
				cgpu->max_hashes=hashes;

			timersub(tv_end, &tv_start, &diff);
			sdiff.tv_sec+=diff.tv_sec;
			sdiff.tv_usec+=diff.tv_usec;
			if(sdiff.tv_usec>1000000) {
				++sdiff.tv_sec;
				sdiff.tv_usec -= 1000000;
			}

			timersub(tv_end, &tv_workstart, &wdiff);

			if(((long)sdiff.tv_sec<cycle)) {
				int mult;

				if((max_nonce==0xffffffff))
					continue;

				mult=1000000/((sdiff.tv_usec + 0x400)/0x400) + 0x10;
				mult *= cycle;
				if(max_nonce>(0xffffffff * 0x400)/mult)
					max_nonce=0xffffffff;
				else
					max_nonce=(max_nonce * mult)/0x400;
			} else if((sdiff.tv_sec>cycle))
				max_nonce=max_nonce * cycle/sdiff.tv_sec;
			else if((sdiff.tv_usec>100000))
				max_nonce=max_nonce * 0x400/(((cycle * 1000000) + sdiff.tv_usec)/(cycle * 1000000/0x400));

			timersub(tv_end, &tv_lastupdate, &diff);
			/* Update the hashmeter at most 5 times per second */
			if((hashes_done && (diff.tv_sec>0 || diff.tv_usec>200000)) ||
				diff.tv_sec >= opt_log_interval) {
				hashmeter(thr_id, hashes_done);
				hashes_done=0;
				copy_time(&tv_lastupdate, tv_end);
			}

			if(thr->work_restart)
			{
				/* Apart from device_thread 0, we stagger the
				 * starting of every next thread to try and get
				 * all devices busy before worrying about
				 * getting work for their extra threads */
				if(!primary)
				{
					struct timespec rgtp;
					rgtp.tv_sec=0;
					rgtp.tv_nsec=250 * thr->device_thread * 1000000;
					nanosleep(&rgtp, NULL);
				}
				break;
			}

			if((thr->pause || cgpu->deven != DEV_ENABLED))
			{
				mt_disable(thr, drv);
			}

			sdiff.tv_sec=sdiff.tv_usec=0;
		} while(!abandon_work(work, &wdiff, cgpu->max_hashes));
		free_work(&work);
	}
	cgpu->deven=DEV_DISABLED;
}

/* Put a new unqueued work item in cgpu->unqueued_work under cgpu->qlock till
 * the driver tells us it's full so that it may extract the work item using
 * the get_queued() function which adds it to the hashtable on
 * cgpu->queued_work. */
static void fill_queue(struct thr_info *mythr, struct cgpu_info *cgpu, struct device_drv *drv)
{
	bool need_work;
	do
	{
		/* Do this lockless just to know if we need more unqueued work. */
		need_work=(!cgpu->unqueued_work);
		/* get_work is a blocking function so do it outside of lock
		 * to prevent deadlocks with other locks. */
		if(need_work)
		{
			struct work *work=get_work(mythr);
			/* Check we haven't grabbed work somehow between
			 * checking and picking up the lock. */
			wr_lock(&cgpu->qlock);
			if(!cgpu->unqueued_work)
			{
				cgpu->unqueued_work=work;
			}
			else
			{
				need_work=false;
			}
			wr_unlock(&cgpu->qlock);
			if(!need_work)
			{
				discard_work(&work);
			}
		}
		/* The queue_full function should be used by the driver to
		 * actually place work items on the physical device if it
		 * does have a queue. */
	} while(!drv->queue_full(cgpu));
}

/* Add a work item to a cgpu's queued hashlist */
void __add_queued(struct cgpu_info *cgpu, struct work *work)
{
	cgpu->queued_count++;
	HASH_ADD_INT(cgpu->queued_work, id, work);
}

struct work *__get_queued(struct cgpu_info *cgpu)
{
	struct work *work=NULL;

	if(cgpu->unqueued_work) {
		work=cgpu->unqueued_work;
		if((stale_work(work, false))) {
			discard_work(&work);
		} else
			__add_queued(cgpu, work);
		cgpu->unqueued_work=NULL;
		wake_gws();
	}

	return work;
}

/* This function is for retrieving one work item from the unqueued pointer and
 * adding it to the hashtable of queued work. Code using this function must be
 * able to handle NULL as a return which implies there is no work available. */
struct work *get_queued(struct cgpu_info *cgpu)
{
	struct work *work;

	wr_lock(&cgpu->qlock);
	work=__get_queued(cgpu);
	wr_unlock(&cgpu->qlock);

	return work;
}

void add_queued(struct cgpu_info *cgpu, struct work *work)
{
	wr_lock(&cgpu->qlock);
	__add_queued(cgpu, work);
	wr_unlock(&cgpu->qlock);
}

/* This function is for finding an already queued work item in the
 * given que hashtable. Code using this function must be able
 * to handle NULL as a return which implies there is no matching work.
 * The calling function must lock access to the que if it is required. */
struct work *__find_work_byid(struct work *que, uint32_t id)
{
	struct work *work, *tmp, *ret=NULL;

	HASH_ITER(hh, que, work, tmp)
	{
		if(work->id==id)
		{
			ret=work;
			break;
		}
	}

	return ret;
}

void __work_completed(struct cgpu_info *cgpu, struct work *work)
{
	cgpu->queued_count--;
	HASH_DEL(cgpu->queued_work, work);
}

/* This iterates over a queued hashlist finding work started more than secs
 * seconds ago and discards the work as completed. The driver must set the
 * work->tv_work_start value appropriately. Returns the number of items aged. */
int age_queued_work(struct cgpu_info *cgpu, double secs)
{
	struct work *work, *tmp;
	struct timeval tv_now;
	int aged=0;

	cgtime(&tv_now);

	wr_lock(&cgpu->qlock);
	HASH_ITER(hh, cgpu->queued_work, work, tmp) {
		if(tdiff(&tv_now, &work->tv_work_start)>secs) {
			__work_completed(cgpu, work);
			free_work(&work);
			aged++;
		}
	}
	wr_unlock(&cgpu->qlock);

	return aged;
}

/* This function should be used by queued device drivers when they're sure
 * the work struct is no longer in use. */
void work_completed(struct cgpu_info *cgpu, struct work *work)
{
	wr_lock(&cgpu->qlock);
	__work_completed(cgpu, work);
	wr_unlock(&cgpu->qlock);

	free_work(&work);
}

void flush_queue(struct cgpu_info *cgpu)
{
	struct work *work=NULL;

	if((!cgpu))
		return;

	/* Use only a trylock in case we get into a deadlock with a queueing
	 * function holding the read lock when we're called. */
	if(wr_trylock(&cgpu->qlock))
		return;
	work=cgpu->unqueued_work;
	cgpu->unqueued_work=NULL;
	wr_unlock(&cgpu->qlock);

	if(work) {
		free_work(&work);
		applog(LOG_DEBUG, "Discarded queued work item");
	}
}

/* This version of hash work is for devices that are fast enough to always
 * perform a full nonce range and need a queue to maintain the device busy.
 * Work creation and destruction is not done from within this function
 * directly. */
void hash_queued_work(struct thr_info *thr)
{
	struct timeval tv_start={0, 0}, tv_end;
	struct cgpu_info *cgpu=thr->cgpu;
	struct device_drv *drv=cgpu->drv;
	int64_t hashes_done=0;

	while((!cgpu->shutdown))
	{
		struct timeval diff;
		int64_t hashes;

		fill_queue(thr, cgpu, drv);

		hashes=drv->scanwork(thr);

		/* Reset the bool here in case the driver looks for it
		 * synchronously in the scanwork loop. */
		thr->work_restart=false;

		if(hashes<0)
		{
			applog(LOG_ERR, "%s %d failure, disabling!", drv->name, cgpu->device_id);
			cgpu->deven=DEV_DISABLED;
			dev_error(cgpu, REASON_THREAD_ZERO_HASH);
			break;
		}

		hashes_done+=hashes;
		cgtime(&tv_end);
		timersub(&tv_end, &tv_start, &diff);
		/* Update the hashmeter at most 5 times per second */
		if((hashes_done && (diff.tv_sec>0 || diff.tv_usec>200000)) ||
			diff.tv_sec >= opt_log_interval)
		{
			hashmeter(thr->id, hashes_done);
			hashes_done=0;
			copy_time(&tv_start, &tv_end);
		}

		if((thr->pause || cgpu->deven != DEV_ENABLED))
		{
			mt_disable(thr, drv);
		}

		if(thr->work_update)
		{
			thr->work_update=false;
			drv->update_work(cgpu);
		}
	}
	cgpu->deven=DEV_DISABLED;
}

/* This version of hash_work is for devices drivers that want to do their own
 * work management entirely, usually by using get_work(). Note that get_work
 * is a blocking function and will wait indefinitely if no work is available
 * so this must be taken into consideration in the driver. */
void hash_driver_work(struct thr_info *thr)
{
	struct timeval tv_start={0, 0}, tv_end;
	struct cgpu_info *cgpu=thr->cgpu;
	struct device_drv *drv=cgpu->drv;
	int64_t hashes_done=0;
	int64_t hashes;

	while(!cgpu->shutdown)
	{
		struct timeval diff;

		hashes=drv->scanwork(thr);

		/* Reset the bool here in case the driver looks for it
		 * synchronously in the scanwork loop. */
		thr->work_restart=false;

		if(hashes<0)
		{
			applog(LOG_ERR, "%s %d failure, disabling!", drv->name, cgpu->device_id);
			cgpu->deven=DEV_DISABLED;
			dev_error(cgpu, REASON_THREAD_ZERO_HASH);
			break;
		}

		hashes_done+=hashes;
		cgtime(&tv_end);
		timersub(&tv_end, &tv_start, &diff);
		/* Update the hashmeter at most 5 times per second */
		if((hashes_done && (diff.tv_sec>0 || diff.tv_usec>200000)) || diff.tv_sec >= opt_log_interval)
		{
			hashmeter(thr->id, hashes_done);
			hashes_done=0;
			copy_time(&tv_start, &tv_end);
		}

		if(thr->pause || cgpu->deven != DEV_ENABLED)
		{
			mt_disable(thr, drv);
		}

		if(thr->work_update)
		{
			thr->work_update=false;
			drv->update_work(cgpu);
		}
	}
	cgpu->deven=DEV_DISABLED;
}

void *miner_thread(void *userdata)
{
	struct thr_info *mythr=userdata;
	const int thr_id=mythr->id;
	struct cgpu_info *cgpu=mythr->cgpu;
	struct device_drv *drv=cgpu->drv;
	char threadname[16];

		snprintf(threadname, sizeof(threadname), "%d/Miner", thr_id);
	RenameThread(threadname);

	thread_reportout(mythr);
	if(!drv->thread_init(mythr)) {
		dev_error(cgpu, REASON_THREAD_FAIL_INIT);
		goto out;
	}

	applog(LOG_DEBUG, "Waiting on sem in miner thread");
	cgsem_wait(&mythr->sem);

	cgpu->last_device_valid_work=time(NULL);
	drv->hash_work(mythr);
	drv->thread_shutdown(mythr);
out:
	return NULL;
}

enum {
	STAT_SLEEP_INTERVAL		= 1,
	STAT_CTR_INTERVAL		= 10000000,
	FAILURE_INTERVAL		= 30,
};

/* This will make the longpoll thread wait till it's the current pool, or it
 * has been flagged as rejecting, before attempting to open any connections.
 */
static void wait_lpcurrent(struct pool *pool)
{
	while(!cnx_needed(pool) && (pool->enabled==POOL_DISABLED ||
		   (pool != current_pool() && pool_strategy != POOL_LOADBALANCE && pool_strategy != POOL_BALANCE)))
	{
		mutex_lock(&lp_lock);
		pthread_cond_wait(&lp_cond, &lp_lock);
		mutex_unlock(&lp_lock);
	}
}

void reinit_device(struct cgpu_info *cgpu)
{
	if(cgpu->deven==DEV_DISABLED)
	{
		return;
	}
#ifdef USE_USBUTILS
	/* Attempt a usb device reset if the device has gone sick */
	if(cgpu->usbdev && cgpu->usbdev->handle)
		libusb_reset_device(cgpu->usbdev->handle);
#endif
	cgpu->drv->reinit_device(cgpu);
}

static struct timeval rotate_tv;

/* We reap curls if they are unused for over a minute */
static void reap_curl(struct pool *pool)
{
	struct curl_ent *ent, *iter;
	struct timeval now;
	int reaped=0;

	cgtime(&now);

	mutex_lock(&pool->pool_lock);
	list_for_each_entry_safe(ent, iter, &pool->curlring, node)
	{
		if(pool->curls<2)
		{
			break;
		}
		if(now.tv_sec - ent->tv.tv_sec>300)
		{
			reaped++;
			pool->curls--;
			list_del(&ent->node);
			free(ent);
		}
	}
	mutex_unlock(&pool->pool_lock);

	if(reaped)
	{
		applog(LOG_DEBUG, "Reaped %d curl%s from pool %d", reaped, reaped>1 ? "s" : "", pool->pool_no);
	}
}

/* Prune old shares we haven't had a response about for over 2 minutes in case
 * the pool never plans to respond and we're just leaking memory. If we get a
 * response beyond that time they will be seen as untracked shares. */
static void prune_stratum_shares(struct pool *pool)
{
	struct stratum_share *sshare, *tmpshare;
	time_t current_time=time(NULL);
	int cleared=0;

	mutex_lock(&sshare_lock);
	HASH_ITER(hh, stratum_shares, sshare, tmpshare) {
		if(sshare->work->pool==pool && current_time>sshare->sshare_time + 120) {
			HASH_DEL(stratum_shares, sshare);
			free_work(&sshare->work);
			free(sshare);
			cleared++;
		}
	}
	mutex_unlock(&sshare_lock);

	if(cleared) {
		applog(LOG_WARNING, "Lost %d shares due to no stratum share response from pool %d",
			   cleared, pool->pool_no);
		pool->stale_shares+=cleared;
		total_stale+=cleared;
	}
}

static void *watchpool_thread(void *userdata)
{
	int intervals=0;
	cgtimer_t cgt;

	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	RenameThread("Watchpool");

	set_lowprio();
	cgtimer_time(&cgt);

	while(42) {
		struct timeval now;
		int i;

		if(++intervals>120)
			intervals=0;
		cgtime(&now);

		for(i=0; i<total_pools; i++) {
			struct pool *pool=pools[i];

			if(!opt_benchmark)
			{
				reap_curl(pool);
				prune_stratum_shares(pool);
			}

			/* Get a rolling utility per pool over 10 mins */
			if(intervals>119) {
				double shares=pool->diff1 - pool->last_shares;

				pool->last_shares=pool->diff1;
				pool->utility=(pool->utility + shares * 0.63)/1.63;
				pool->shares=pool->utility;
			}

			if(pool->enabled==POOL_DISABLED)
				continue;

			/* Don't start testing a pool if its test thread
			 * from startup is still doing its first attempt. */
			if((pool->testing))
				continue;

			if(pool_active(pool, true)) {
				if(pool_tclear(pool, &pool->idle))
					pool_resus(pool);
			} else
				cgtime(&pool->tv_idle);

			/* Only switch pools if the failback pool has been
			 * alive for more than 5 minutes to prevent
			 * intermittently failing pools from being used. */
			if(!pool->idle && pool_strategy==POOL_FAILOVER && pool->prio<cp_prio() &&
				now.tv_sec - pool->tv_idle.tv_sec>opt_pool_fallback) {
				applog(LOG_WARNING, "Pool %d %s stable for >%d seconds",
					   pool->pool_no, pool->rpc_url, opt_pool_fallback);
				switch_pools(NULL);
			}
		}

		if(current_pool()->idle)
			switch_pools(NULL);

		if(pool_strategy==POOL_ROTATE && now.tv_sec - rotate_tv.tv_sec>60 * opt_rotate_period) {
			cgtime(&rotate_tv);
			switch_pools(NULL);
		}

		cgsleep_ms_r(&cgt, 5000);
		cgtimer_time(&cgt);
	}
	return NULL;
}

/* Makes sure the hashmeter keeps going even if mining threads stall, updates
 * the screen at regular intervals, and restarts threads if they appear to have
 * died. */
#define WATCHDOG_INTERVAL		2
#define WATCHDOG_SICK_TIME		120
#define WATCHDOG_DEAD_TIME		600
#define WATCHDOG_SICK_COUNT		(WATCHDOG_SICK_TIME/WATCHDOG_INTERVAL)
#define WATCHDOG_DEAD_COUNT		(WATCHDOG_DEAD_TIME/WATCHDOG_INTERVAL)

static void *watchdog_thread(void *userdata)
{
	const unsigned int interval=WATCHDOG_INTERVAL;
	struct timeval zero_tv;

#ifdef USE_LIBSYSTEMD
	uint64_t notify_usec;
	struct timeval notify_interval, notify_tv;

	if(sd_watchdog_enabled(false, &notify_usec)) {
		notify_usec=notify_usec/2;
		us_to_timeval(&notify_interval, notify_usec);
		cgtime(&notify_tv);
		addtime(&notify_interval, &notify_tv);

		applog(LOG_DEBUG, "Watchdog notify interval: %.3gs",
				notify_usec/1000000.0);
	}
#endif

	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	RenameThread("Watchdog");

	set_lowprio();
	memset(&zero_tv, 0, sizeof(struct timeval));
	cgtime(&rotate_tv);

	while(1)
	{
		int i;
		struct timeval now;

		sleep(interval);

		discard_stale();

		hashmeter(-1, 0);
		cgtime(&now);
#if USE_LIBSYSTEMD
		if(notify_usec && !time_more(&notify_tv, &now))
		{
			sd_notify(false, "WATCHDOG=1");
			copy_time(&notify_tv, &now);
			addtime(&notify_interval, &notify_tv);
			applog(LOG_DEBUG, "Notified watchdog");
		}
#endif
		if(!sched_paused && !should_run())
		{
			applog(LOG_WARNING, "Pausing execution as per stop time %02d:%02d scheduled",
				   schedstop.tm.tm_hour, schedstop.tm.tm_min);
			if(!schedstart.enable) {
				quit(0, "Terminating execution as planned");
				break;
			}

			applog(LOG_WARNING, "Will restart execution as scheduled at %02d:%02d",
				   schedstart.tm.tm_hour, schedstart.tm.tm_min);
			sched_paused=true;

			rd_lock(&mining_thr_lock);
			for(i=0; i<mining_threads; i++)
				mining_thr[i]->pause=true;
			rd_unlock(&mining_thr_lock);
		} else if(sched_paused && should_run()) {
			applog(LOG_WARNING, "Restarting execution as per start time %02d:%02d scheduled",
				schedstart.tm.tm_hour, schedstart.tm.tm_min);
			if(schedstop.enable)
				applog(LOG_WARNING, "Will pause execution as scheduled at %02d:%02d",
					schedstop.tm.tm_hour, schedstop.tm.tm_min);
			sched_paused=false;

			for(i=0; i<mining_threads; i++) {
				struct thr_info *thr;

				thr=get_thread(i);

				/* Don't touch disabled devices */
				if(thr->cgpu->deven==DEV_DISABLED)
					continue;
				thr->pause=false;
				applog(LOG_DEBUG, "Pushing sem post to thread %d", thr->id);
				cgsem_post(&thr->sem);
			}
		}

		for(i=0; i<total_devices; ++i)
		{
			struct cgpu_info *cgpu=get_devices(i);
			struct thr_info *thr=cgpu->thr[0];
			enum dev_enable *denable;
			char dev_str[8];
			if(!thr)
			{
				continue;
			}
			cgpu->drv->get_stats(cgpu);
			denable=&cgpu->deven;
			snprintf(dev_str, sizeof(dev_str), "%s %d", cgpu->drv->name, cgpu->device_id);
			/* Thread is waiting on getwork or disabled */
			if(thr->getwork || *denable==DEV_DISABLED || thr->pause)
				continue;

			if(cgpu->status != LIFE_WELL && (now.tv_sec - thr->last.tv_sec<WATCHDOG_SICK_TIME)) {
				if(cgpu->status != LIFE_INIT)
				applog(LOG_ERR, "%s: Recovered, declaring WELL!", dev_str);
				cgpu->status=LIFE_WELL;
				cgpu->device_last_well=time(NULL);
			}
			else if(cgpu->status==LIFE_WELL && (now.tv_sec - thr->last.tv_sec>WATCHDOG_SICK_TIME))
			{
				cgpu->status=LIFE_SICK;
				applog(LOG_ERR, "%s: Idle for more than 60 seconds, declaring SICK!", dev_str);
				cgtime(&thr->sick);

				dev_error(cgpu, REASON_DEV_SICK_IDLE_60);
				if(opt_restart) {
					applog(LOG_ERR, "%s: Attempting to restart", dev_str);
					reinit_device(cgpu);
				}
			} else if(cgpu->status==LIFE_SICK && (now.tv_sec - thr->last.tv_sec>WATCHDOG_DEAD_TIME)) {
				cgpu->status=LIFE_DEAD;
				applog(LOG_ERR, "%s: Not responded for more than 10 minutes, declaring DEAD!", dev_str);
				cgtime(&thr->sick);

				dev_error(cgpu, REASON_DEV_DEAD_IDLE_600);
			} else if(now.tv_sec - thr->sick.tv_sec>60 &&
				   (cgpu->status==LIFE_SICK || cgpu->status==LIFE_DEAD)) {
				/* Attempt to restart a GPU that's sick or dead once every minute */
				cgtime(&thr->sick);
				if(opt_restart)
					reinit_device(cgpu);
			}
		}
	}

	return NULL;
}

static void log_print_status(struct cgpu_info *cgpu)
{
	char logline[255];

	get_statline(logline, sizeof(logline), cgpu);
	applog(LOG_WARNING, "%s", logline);
}

/* Various noop functions for drivers that don't support or need their
 * variants. */
static void noop_reinit_device()
{
}

static bool noop_get_stats()
{
	return true;
}

static bool noop_thread_prepare()
{
	return true;
}

static uint64_t noop_can_limit_work()
{
	return 0xffffffff;
}

static bool noop_thread_init()
{
	return true;
}

static bool noop_prepare_work()
{
	return true;
}

static void noop_hw_error()
{
}

static void noop_thread_shutdown()
{
}

static void noop_thread_enable()
{
}

static void noop_drv_detect(_GL_UNUSED bool hotplug)
{
}

static struct api_data *noop_get_api_stats()
{
	return NULL;
}

static void noop_hash_work()
{
}

static void noop_get_statline()
{
}

static void noop_get_statline_before()
{
}

void print_summary()
{
	struct timeval diff;
	int hours, mins, secs, i;
	double utility, displayed_hashes, work_util;

	timersub(&total_tv_end, &total_tv_start, &diff);
	hours=diff.tv_sec/3600;
	mins=(diff.tv_sec % 3600)/60;
	secs=diff.tv_sec % 60;

	utility=total_accepted/total_secs * 60;
	work_util=total_diff1/total_secs * 60;

	applog(LOG_WARNING, "\nSummary of runtime statistics:\n");
	applog(LOG_WARNING, "Started at %s", datestamp);
	if(total_pools==1)
		applog(LOG_WARNING, "Pool: %s", pools[0]->rpc_url);
	applog(LOG_WARNING, "Runtime: %d hrs : %d mins : %d secs", hours, mins, secs);
	displayed_hashes=total_ghashes_done/total_secs;

	applog(LOG_WARNING, "Average hashrate: %.1f Mhash/s", displayed_hashes);
	applog(LOG_WARNING, "Solved blocks: %d", found_blocks);
	applog(LOG_WARNING, "Best share difficulty: %s", best_share);
	applog(LOG_WARNING, "Share submissions: %"PRId64, total_accepted + total_rejected);
	applog(LOG_WARNING, "Accepted shares: %"PRId64, total_accepted);
	applog(LOG_WARNING, "Rejected shares: %"PRId64, total_rejected);
	applog(LOG_WARNING, "Accepted difficulty shares: %lli", total_diff_accepted);
	applog(LOG_WARNING, "Rejected difficulty shares: %lli", total_diff_rejected);
	if(total_accepted || total_rejected)
		applog(LOG_WARNING, "Reject ratio: %.1f%%", (double)(total_rejected * 100)/(double)(total_accepted + total_rejected));
	applog(LOG_WARNING, "Hardware errors: %d", hw_errors);
	applog(LOG_WARNING, "Utility (accepted shares/min): %.2f/min", utility);
	applog(LOG_WARNING, "Work Utility (diff1 shares solved/min): %.2f/min\n", work_util);

	applog(LOG_WARNING, "Stale submissions discarded due to new blocks: %"PRId64, total_stale);
	applog(LOG_WARNING, "Unable to get work from server occasions: %d", total_go);
	applog(LOG_WARNING, "Work items generated locally: %d", local_work);
	applog(LOG_WARNING, "Submitting work remotely delay occasions: %d", total_ro);
	applog(LOG_WARNING, "New blocks detected on network: %d\n", new_blocks);

	if(total_pools>1) {
		for(i=0; i<total_pools; i++) {
			struct pool *pool=pools[i];

			applog(LOG_WARNING, "Pool: %s", pool->rpc_url);
			if(pool->solved)
				applog(LOG_WARNING, "SOLVED %d BLOCK%s!", pool->solved, pool->solved>1 ? "S" : "");
			applog(LOG_WARNING, " Share submissions: %"PRId64, pool->accepted + pool->rejected);
			applog(LOG_WARNING, " Accepted shares: %"PRId64, pool->accepted);
			applog(LOG_WARNING, " Rejected shares: %"PRId64, pool->rejected);
			applog(LOG_WARNING, " Accepted difficulty shares: %lli", pool->diff_accepted);
			applog(LOG_WARNING, " Rejected difficulty shares: %lli", pool->diff_rejected);
			if(pool->accepted || pool->rejected)
				applog(LOG_WARNING, " Reject ratio: %.1f%%", (double)(pool->rejected * 100)/(double)(pool->accepted + pool->rejected));

			applog(LOG_WARNING, " Items worked on: %d", pool->works);
			applog(LOG_WARNING, " Stale submissions discarded due to new blocks: %d", pool->stale_shares);
			applog(LOG_WARNING, " Unable to get work from server occasions: %d", pool->getfail_occasions);
			applog(LOG_WARNING, " Submitting work remotely delay occasions: %d\n", pool->remotefail_occasions);
		}
	}

	applog(LOG_WARNING, "Summary of per device statistics:\n");
	for(i=0; i<total_devices; ++i)
	{
		struct cgpu_info *cgpu=get_devices(i);
		cgpu->drv->get_statline_before=&noop_get_statline_before;
		cgpu->drv->get_statline=&noop_get_statline;
		log_print_status(cgpu);
	}

	if(opt_shares)
	{
		applog(LOG_WARNING, "Mined %lli accepted shares of %i requested\n", total_diff_accepted, opt_shares);
		if(opt_shares>total_diff_accepted)
		{
			applog(LOG_WARNING, "WARNING - Mined only %lli shares of %i requested.", total_diff_accepted, opt_shares);
		}
	}
	applog(LOG_WARNING, " ");

	fflush(stderr);
	fflush(stdout);
}

static void clean_up(bool restarting)
{
#ifdef USE_USBUTILS
	usb_polling=false;
	pthread_join(usb_poll_thread, NULL);
	libusb_exit(NULL);
#endif
	cgtime(&total_tv_end);
	if(!restarting && !opt_realquiet && successful_connect)
	{
		print_summary();
	}
}

/* Should all else fail and we're unable to clean up threads due to locking
 * issues etc, just silently exit. */
static void *killall_thread(void *arg)
{
	pthread_detach(pthread_self());
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	sleep(5);
	exit(1);
	return NULL;
}

void __quit(int status, bool clean)
{
	pthread_t killall_t;

#ifdef USE_LIBSYSTEMD
	sd_notify(false, "STOPPING=1\nSTATUS=Shutting down...");
#endif

	if((pthread_create(&killall_t, NULL, killall_thread, NULL)))
	{
		exit(1);
	}

	if(clean)
	{
		clean_up(false);
	}
	pthread_cancel(killall_t);
	exit(status);
}

static bool pools_active=false;

static void *test_pool_thread(void *arg)
{
	struct pool *pool=(struct pool *)arg;

	if(!pool->blocking)
		pthread_detach(pthread_self());
retry:
	if(pool->removed)
	{
		goto out;
	}
	if(pool_active(pool, false))
	{
		pool_tclear(pool, &pool->idle);
		bool first_pool=false;
		cg_wlock(&control_lock);
		if(!pools_active)
		{
			currentpool=pool;
			if(pool->pool_no != 0)
				first_pool=true;
			pools_active=true;
		}
		cg_wunlock(&control_lock);

		if(first_pool)
		{
			applog(LOG_NOTICE, "Switching to pool %d %s - first alive pool", pool->pool_no, pool->rpc_url);
		}

		pool_resus(pool);
		switch_pools(NULL);
	}
	else
	{
		pool_died(pool);
		if(!pool->blocking)
		{
			sleep(5);
			goto retry;
		}
	}

	pool->testing=false;
out:
	return NULL;
}

/* Always returns true that the pool details were added unless we are not
 * live, implying this is the only pool being added, so if no pools are
 * active it returns false. */
bool add_pool_details(struct pool *pool, bool live, char *url, char *user, char *pass)
{
	size_t siz;

	pool->rpc_url=url;
	pool->rpc_user=user;
	pool->rpc_pass=pass;
	siz=strlen(pool->rpc_user) + strlen(pool->rpc_pass) + 2;
	pool->rpc_userpass=cgmalloc(siz);
	snprintf(pool->rpc_userpass, siz, "%s:%s", pool->rpc_user, pool->rpc_pass);

	pool->testing=true;
	pool->idle=true;
	pool->blocking=!live;
	enable_pool(pool);

	pthread_create(&pool->test_thread, NULL, test_pool_thread, (void *)pool);
	if(!live) {
		pthread_join(pool->test_thread, NULL);
		return pools_active;
	}
	return true;
}

static int cgminer_id_count=0;

static void generic_zero_stats(struct cgpu_info *cgpu)
{
	cgpu->diff_accepted =
	cgpu->diff_rejected =
	cgpu->hw_errors=0;
}

/* Fill missing driver drv functions with noops */
void fill_device_drv(struct device_drv *drv)
{
	if(!drv->drv_detect)
		drv->drv_detect=&noop_drv_detect;
	if(!drv->reinit_device)
		drv->reinit_device=&noop_reinit_device;
	if(!drv->get_statline_before)
		drv->get_statline_before=&noop_get_statline_before;
	if(!drv->get_statline)
		drv->get_statline=&noop_get_statline;
	if(!drv->get_stats)
		drv->get_stats=&noop_get_stats;
	if(!drv->get_api_stats)
		drv->get_api_stats=&noop_get_api_stats;
	if(!drv->thread_prepare)
		drv->thread_prepare=&noop_thread_prepare;
	if(!drv->can_limit_work)
		drv->can_limit_work=&noop_can_limit_work;
	if(!drv->thread_init)
		drv->thread_init=&noop_thread_init;
	if(!drv->prepare_work)
		drv->prepare_work=&noop_prepare_work;
	if(!drv->hw_error)
		drv->hw_error=&noop_hw_error;
	if(!drv->thread_shutdown)
		drv->thread_shutdown=&noop_thread_shutdown;
	if(!drv->thread_enable)
		drv->thread_enable=&noop_thread_enable;
	if(!drv->hash_work)
		drv->hash_work=&hash_sole_work;
	if(!drv->flush_work)
		drv->flush_work=&noop_reinit_device;
	if(!drv->update_work)
		drv->update_work=&noop_reinit_device;
	if(!drv->queue_full)
		drv->queue_full=&noop_get_stats;
	if(!drv->zero_stats)
		drv->zero_stats=&generic_zero_stats;
	/* If drivers support internal diff they should set a max_diff or
	 * we will assume they don't and set max to 1. */
	if(!drv->max_diff)
		drv->max_diff=1;
	if(!drv->genwork)
		opt_gen_stratum_work=true;
}

void null_device_drv(struct device_drv *drv)
{
	drv->drv_detect=&noop_drv_detect;
	drv->reinit_device=&noop_reinit_device;
	drv->get_statline_before=&noop_get_statline_before;
	drv->get_statline=&noop_get_statline;
	drv->get_api_stats=&noop_get_api_stats;
	drv->get_stats=&noop_get_stats;
	drv->identify_device=&noop_reinit_device;
	drv->set_device=NULL;

	drv->thread_prepare=&noop_thread_prepare;
	drv->can_limit_work=&noop_can_limit_work;
	drv->thread_init=&noop_thread_init;
	drv->prepare_work=&noop_prepare_work;

	/* This should make the miner thread just exit */
	drv->hash_work=&noop_hash_work;

	drv->hw_error=&noop_hw_error;
	drv->thread_shutdown=&noop_thread_shutdown;
	drv->thread_enable=&noop_thread_enable;
	drv->zero_stats=&generic_zero_stats;

	drv->queue_full=&noop_get_stats;
	drv->flush_work=&noop_reinit_device;
	drv->update_work=&noop_reinit_device;

	drv->max_diff=1;
	drv->min_diff=1;
}

void enable_device(struct cgpu_info *cgpu)
{
	cgpu->deven=DEV_ENABLED;

	wr_lock(&devices_lock);
	devices[cgpu->cgminer_id=cgminer_id_count++]=cgpu;
	wr_unlock(&devices_lock);

	if(hotplug_mode)
		new_threads+=cgpu->threads;
	else
		mining_threads+=cgpu->threads;

	rwlock_init(&cgpu->qlock);
	cgpu->queued_work=NULL;
}

struct _cgpu_devid_counter
{
	char name[4];
	int lastid;
	UT_hash_handle hh;
};

static void adjust_mostdevs(void)
{
	if(total_devices - zombie_devs>most_devices)
	{
		most_devices=total_devices - zombie_devs;
	}
}

bool add_cgpu(struct cgpu_info *cgpu)
{
	static struct _cgpu_devid_counter *devids=NULL;
	struct _cgpu_devid_counter *d;

	HASH_FIND_STR(devids, cgpu->drv->name, d);
	if(d)
	{
		cgpu->device_id=++d->lastid;
	}
	else
	{
		d=cgmalloc(sizeof(*d));
		cg_memcpy(d->name, cgpu->drv->name, sizeof(d->name));
		cgpu->device_id=d->lastid=0;
		HASH_ADD_STR(devids, name, d);
	}

	wr_lock(&devices_lock);
	devices=cgrealloc(devices, sizeof(struct cgpu_info *) * (total_devices + new_devices + 2));
	wr_unlock(&devices_lock);

	mutex_lock(&stats_lock);
	cgpu->last_device_valid_work=time(NULL);
	mutex_unlock(&stats_lock);

	if(hotplug_mode)
	{
		devices[total_devices + new_devices++]=cgpu;
	}
	else
	{
		devices[total_devices++]=cgpu;
	}
	adjust_mostdevs();
#ifdef USE_USBUTILS
	if(cgpu->usbdev && !cgpu->unique_id && cgpu->usbdev->serial_string &&
		strlen(cgpu->usbdev->serial_string)>4)
		cgpu->unique_id=str_text(cgpu->usbdev->serial_string);
#endif
	applog(LOG_INFO, "Total devices %i", total_devices);
	return true;
}

struct device_drv *copy_drv(struct device_drv *drv)
{
	struct device_drv *copy;

	copy=cgmalloc(sizeof(*copy));
	cg_memcpy(copy, drv, sizeof(*copy));
	copy->copy=true;
	return copy;
}

#ifdef USE_USBUTILS
static void hotplug_process(void)
{
	struct thr_info *thr;
	int i, j;

	for(i=0; i<new_devices; i++)
	{
		struct cgpu_info *cgpu;
		int dev_no=total_devices + i;
		cgpu=devices[dev_no];
		enable_device(cgpu);
		cgpu->cgminer_stats.getwork_wait_min.tv_sec=MIN_SEC_UNSET;
		cgpu->rolling=cgpu->total_mhashes=0;
	}

	wr_lock(&mining_thr_lock);
	mining_thr=cgrealloc(mining_thr, sizeof(thr) * (mining_threads + new_threads + 1));
	for(i=0; i<new_threads; i++)
		mining_thr[mining_threads + i]=cgcalloc(1, sizeof(*thr));

	// Start threads
	for(i=0; i<new_devices; ++i)
	{
		struct cgpu_info *cgpu=devices[total_devices];
		cgpu->thr=cgmalloc(sizeof(*cgpu->thr) * (cgpu->threads+1));
		cgpu->thr[cgpu->threads]=NULL;
		cgpu->status=LIFE_INIT;
		cgtime(&(cgpu->dev_start_tv));

		for(j=0; j<cgpu->threads; ++j)
		{
			thr=__get_thread(mining_threads);
			thr->id=mining_threads;
			thr->cgpu=cgpu;
			thr->device_thread=j;
			if(!cgpu->drv->thread_prepare(thr))
			{
				null_device_drv(cgpu->drv);
				cgpu->deven=DEV_DISABLED;
				continue;
			}

			if((thr_info_create(thr, NULL, miner_thread, thr)))
				quit(1, "hotplug thread %d create failed", thr->id);

			cgpu->thr[j]=thr;

			/* Enable threads for devices set not to mine but disable
			 * their queue in case we wish to enable them later */
			if(cgpu->deven != DEV_DISABLED) {
				applog(LOG_DEBUG, "Pushing sem post to thread %d", thr->id);
				cgsem_post(&thr->sem);
			}

			mining_threads++;
		}
		total_devices++;
		applog(LOG_WARNING, "Hotplug: %s added %s %i", cgpu->drv->dname, cgpu->drv->name, cgpu->device_id);
	}
	wr_unlock(&mining_thr_lock);

	adjust_mostdevs();
}

#define DRIVER_DRV_DETECT_HOTPLUG(X) X##_drv.drv_detect(true);

static void reinit_usb(void)
{
	int err;

	usb_reinit=true;
	/* Wait till libusb_poll_thread is no longer polling */
	while(polling_usb)
		cgsleep_ms(100);

	applog(LOG_DEBUG, "Reinitialising libusb");
	libusb_exit(NULL);
	err=libusb_init(NULL);
	if(err)
		quit(1, "Reinit of libusb failed err %d:%s", err, libusb_error_name(err));
	usb_reinit=false;
}

static void *hotplug_thread(void *userdata)
{
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	RenameThread("Hotplug");

	hotplug_mode=true;

	cgsleep_ms(5000);

	while(0x2a)
	{
		// Version 0.1 just add the devices on - worry about using nodev later

		if(hotplug_time==0)
		{
			cgsleep_ms(5000);
		}
		else
		{
			new_devices=0;
			new_threads=0;

			/* Use the DRIVER_PARSE_COMMANDS macro to detect all
			 * devices */
			DRIVER_PARSE_COMMANDS(DRIVER_DRV_DETECT_HOTPLUG)

			if(new_devices)
				hotplug_process();

			/* If we have no active devices, libusb may need to
			 * be re-initialised to work properly */
			if(total_devices==zombie_devs)
			{
				reinit_usb();
			}
			// hotplug_time >0 && <=9999
			cgsleep_ms(hotplug_time * 1000);
		}
	}

	return NULL;
}
#endif

static void probe_pools(void)
{
	int i;

	for(i=0; i<total_pools; i++) {
		struct pool *pool=pools[i];

		pool->testing=true;
		pthread_create(&pool->test_thread, NULL, test_pool_thread, (void *)pool);
	}
}

#define DRIVER_FILL_DEVICE_DRV(X) fill_device_drv(&X##_drv);
#define DRIVER_DRV_DETECT_ALL(X) X##_drv.drv_detect(false);

#ifdef USE_USBUTILS
static void *libusb_poll_thread(void *arg)
{
	struct timeval tv_end;

	RenameThread("USBPoll");

	while((usb_polling)) {
		tv_end.tv_sec=0;
		tv_end.tv_usec=100000;
		while(usb_reinit) {
			polling_usb=false;
			cgsleep_ms(100);
		}
		polling_usb=true;
		libusb_handle_events_timeout_completed(NULL, &tv_end, NULL);
	}

	/* Cancel any cancellable usb transfers */
	cancel_usb_transfers();

	/* Keep event handling going until there are no async transfers in
	 * flight. */
	while(async_usb_transfers()) {
		tv_end.tv_sec=0;
		tv_end.tv_usec=0;
		libusb_handle_events_timeout_completed(NULL, &tv_end, NULL);
	};

	return NULL;
}

static void initialise_usb(void) {
	int err=libusb_init(NULL);

	if(err) {
		fprintf(stderr, "libusb_init() failed err %d", err);
		fflush(stderr);
		quit(1, "libusb_init() failed");
	}
	initialise_usblocks();
	usb_polling=true;
	pthread_create(&usb_poll_thread, NULL, libusb_poll_thread, NULL);
}
#else
#define initialise_usb() {}
#endif

int main(int argc, char *argv[])
{
	struct sigaction handler;
	struct work *work=NULL;
	struct thr_info *thr;
	struct block *block;
	int i, j, slept=0;
	unsigned int k;
	char *s;

	/* This dangerous functions tramples random dynamically allocated
	 * variables so do it before anything at all */

#ifdef USE_LIBSYSTEMD
	sd_notify(false, "STATUS=Starting up...");
#endif

# ifdef __linux
	/* If we're on a small lowspec platform with only one CPU, we should
	 * yield after dropping a lock to allow a thread waiting for it to be
	 * able to get CPU time to grab the lock. */
	if(sysconf(_SC_NPROCESSORS_ONLN)==1)
		selective_yield=&sched_yield;
#endif

#if LOCK_TRACKING
	// Must be first
	if((pthread_mutex_init(&lockstat_lock, NULL)))
		quithere(1, "Failed to pthread_mutex_init lockstat_lock errno=%d", errno);
#endif

	initial_args=cgmalloc(sizeof(char *) * (argc + 1));
	for  (i=0; i<argc; i++)
		initial_args[i]=strdup(argv[i]);
	initial_args[argc]=NULL;

	mutex_init(&hash_lock);
	mutex_init(&console_lock);
	cglock_init(&control_lock);
	mutex_init(&stats_lock);
	mutex_init(&sharelog_lock);
	cglock_init(&ch_lock);
	mutex_init(&sshare_lock);
	rwlock_init(&blk_lock);
	rwlock_init(&netacc_lock);
	rwlock_init(&mining_thr_lock);
	rwlock_init(&devices_lock);

	mutex_init(&lp_lock);
	if((pthread_cond_init(&lp_cond, NULL)))
		early_quit(1, "Failed to pthread_cond_init lp_cond");

	mutex_init(&restart_lock);
	if((pthread_cond_init(&restart_cond, NULL)))
		early_quit(1, "Failed to pthread_cond_init restart_cond");

	if((pthread_cond_init(&gws_cond, NULL)))
		early_quit(1, "Failed to pthread_cond_init gws_cond");

	/* Create a unique get work queue */
	getq=tq_new();
	if(!getq)
		early_quit(1, "Failed to create getq");
	/* We use the getq mutex as the staged lock */
	stgd_lock=&getq->mutex;

	initialise_usb();

	snprintf(packagename, sizeof(packagename), "%s %s", PACKAGE_NAME, VERSION);

	handler.sa_handler=&sighandler;
	handler.sa_flags=0;
	sigemptyset(&handler.sa_mask);
	sigaction(SIGTERM, &handler, &termhandler);
	sigaction(SIGINT, &handler, &inthandler);
	sigaction(SIGABRT, &handler, &abrthandler);
	signal(SIGPIPE, SIG_IGN);
	opt_kernel_path=malloc(PATH_MAX);
	strcpy(opt_kernel_path, CGMINER_PREFIX);
	cgminer_path=malloc(PATH_MAX);
	s=strdup(argv[0]);
	if(s)
	{
		if(strlen(dirname(s))<PATH_MAX)
		{
			strcpy(cgminer_path, dirname(s));
		}
		free(s);
	}
	strcat(cgminer_path, "/");

	devcursor=8;
	logstart=devcursor + 1;
	logcursor=logstart + 1;

	block=cgcalloc(sizeof(struct block), 1);
	for(i=0; i<36; i++)
	{
		strcat(block->hash, "0");
	}
	HASH_ADD_STR(blocks, hash, block);
	strcpy(current_hash, block->hash);

	INIT_LIST_HEAD(&scan_devices);

	/* parse command line */
	opt_register_table(opt_config_table, "Options for both config file and command line");
	opt_register_table(opt_cmdline_table, "Options for command line only");

	opt_parse(&argc, argv, applog_and_exit);
	if(argc != 1)
	{
		early_quit(1, "Unexpected extra commandline arguments");
	}

	if(!config_loaded)
		load_default_config();

	if(opt_benchmark)
	{
		struct pool *pool;
		pool=add_pool();
		pool->rpc_url=cgmalloc(255);
		strcpy(pool->rpc_url, "Benchmark");
		pool->rpc_user=pool->rpc_url;
		pool->rpc_pass=pool->rpc_url;
		pool->rpc_userpass=pool->rpc_url;
		pool->sockaddr_url=pool->rpc_url;
		strncpy(pool->diff, "?", sizeof(pool->diff)-1);
		pool->diff[sizeof(pool->diff)-1]='\0';
		enable_pool(pool);
		pool->idle=false;
		successful_connect=true;

		for(i=0; i<16; i++) {
			hex2bin(&bench_hidiff_bins[i][0], &bench_hidiffs[i][0], 160);
			hex2bin(&bench_lodiff_bins[i][0], &bench_lodiffs[i][0], 160);
		}
		set_target(bench_target, 32);
	}

	applog(LOG_WARNING, "Started %s", packagename);
	if(cnfbuf) {
		applog(LOG_NOTICE, "Loaded configuration file %s", cnfbuf);
		switch (fileconf_load) {
			case 0:
				applog(LOG_WARNING, "Fatal JSON error in configuration file.");
				applog(LOG_WARNING, "Configuration file could not be used.");
				break;
			case -1:
				applog(LOG_WARNING, "Error in configuration file, partially loaded.");
				break;
			default:
				break;
		}
		free(cnfbuf);
		cnfbuf=NULL;
	}

	strcat(opt_kernel_path, "/");

	if(want_per_device_stats)
	{
		opt_log_verbose=true;
	}
#if defined(SCRYPT_H)
	/* Use a shorter scantime for scrypt */
	max_scantime=30;
#else
	max_scantime=60;
#endif
	total_control_threads=8;
	control_thr=cgcalloc(total_control_threads, sizeof(*thr));

	gwsched_thr_id=0;

#ifdef USE_AVALON7
	if(opt_avalon7_ssplus_enable) {
		ssp_sorter_init(HT_SIZE, HT_PRB_LMT, HT_PRB_C1, HT_PRB_C2);
		ssp_hasher_init();
	}
#endif
#ifdef USE_USBUTILS
	usb_initialise();

	// before device detection
	cgsem_init(&usb_resource_sem);
	usbres_thr_id=1;
	thr=&control_thr[usbres_thr_id];
	if(thr_info_create(thr, NULL, usb_resource_thread, thr))
		early_quit(1, "usb resource thread create failed");
	pthread_detach(thr->pth);
#endif

	/* Use the DRIVER_PARSE_COMMANDS macro to fill all the device_drvs */
	DRIVER_PARSE_COMMANDS(DRIVER_FILL_DEVICE_DRV)

	/* Use the DRIVER_PARSE_COMMANDS macro to detect all devices */
	DRIVER_PARSE_COMMANDS(DRIVER_DRV_DETECT_ALL)

	applog(LOG_NOTICE, "Devices detected:");
	for(i=0; i<total_devices; ++i)
	{
		struct cgpu_info *cgpu=devices[i];
		if(cgpu->name)
		{
			applog(LOG_NOTICE, " %2d. %s %d: %s (driver: %s)", i, cgpu->drv->name, cgpu->device_id, cgpu->name, cgpu->drv->dname);
		}
		else
		{
			applog(LOG_NOTICE, " %2d. %s %d (driver: %s)", i, cgpu->drv->name, cgpu->device_id, cgpu->drv->dname);
		}
	}
	applog(LOG_NOTICE, "%d devices listed", total_devices);

	mining_threads=0;
	for(i=0; i<total_devices; ++i)
	{
		enable_device(devices[i]);
	}

	if(!opt_decode)
	{
#ifdef USE_USBUTILS
		if(!total_devices)
		{
			applog(LOG_WARNING, "No devices detected!");
			applog(LOG_WARNING, "Waiting for USB hotplug devices or press q to quit");
		}
#else
		if(!total_devices)
		{
			early_quit(1, "All devices disabled, cannot mine!");
		}
#endif
	}

	most_devices=total_devices;

	load_temp_cutoffs();

	if(!opt_compact)
	{
		logstart+=most_devices;
		logcursor=logstart + 1;
	}

	if(!total_pools)
	{
		applog(LOG_WARNING, "Need to specify at least one pool server.");
		early_quit(1, "Pool setup failed");
	}

	for(i=0; i<total_pools; i++)
	{
		struct pool *pool=pools[i];
		size_t siz;

		pool->cgminer_pool_stats.getwork_wait_min.tv_sec=MIN_SEC_UNSET;

		if(!pool->rpc_userpass) {
			if(!pool->rpc_pass)
				pool->rpc_pass=strdup("");
			if(!pool->rpc_user)
				early_quit(1, "No login credentials supplied for pool %u %s", i, pool->rpc_url);
			siz=strlen(pool->rpc_user) + strlen(pool->rpc_pass) + 2;
			pool->rpc_userpass=cgmalloc(siz);
			snprintf(pool->rpc_userpass, siz, "%s:%s", pool->rpc_user, pool->rpc_pass);
		}
	}
	/* Set the currentpool to pool 0 */
	currentpool=pools[0];

#ifdef HAVE_SYSLOG_H
	if(opt_use_syslog)
	{
		setlogmask(LOG_UPTO(LOG_DEBUG));
		openlog(PACKAGE_NAME, LOG_PID, LOG_USER);
	}
#endif

	mining_thr=cgcalloc(mining_threads, sizeof(void *));
	for(i=0; i<mining_threads; i++)
	{
		mining_thr[i]=cgcalloc(1, sizeof(*thr));
	}

	// Start threads
	k=0;
	for(i=0; i<total_devices; ++i)
	{
		struct cgpu_info *cgpu=devices[i];
		cgpu->thr=cgmalloc(sizeof(*cgpu->thr) * (cgpu->threads+1));
		cgpu->thr[cgpu->threads]=NULL;
		cgpu->status=LIFE_INIT;

		for(j=0; j<cgpu->threads; ++j, ++k)
		{
			thr=get_thread(k);
			thr->id=k;
			thr->cgpu=cgpu;
			thr->device_thread=j;

			if(!cgpu->drv->thread_prepare(thr))
				continue;

			if((thr_info_create(thr, NULL, miner_thread, thr)))
				early_quit(1, "thread %d create failed", thr->id);

			cgpu->thr[j]=thr;

			/* Enable threads for devices set not to mine but disable
			 * their queue in case we wish to enable them later */
			if(cgpu->deven != DEV_DISABLED) {
				applog(LOG_DEBUG, "Pushing sem post to thread %d", thr->id);
				cgsem_post(&thr->sem);
			}
		}
	}

	if(opt_benchmark)
		goto begin_bench;

	for(i=0; i<total_pools; i++)
	{
		struct pool *pool =pools[i];
		enable_pool(pool);
		pool->idle=true;
	}

	while(!pools_active)
	{
		slept=0;
		/* Look for at least one active pool before starting */
		applog(LOG_NOTICE, "Probing for an alive pool");
		probe_pools();

		if(pools_active)
		{
			break;
		}
		else
		{
			applog(LOG_ERR, "There are no servers that could be used to get work from.");
			applog(LOG_ERR, "Most likely you have input the wrong URL, forgotten to add a port, or have not set up workers.");
			applog(LOG_ERR, "Please check the details from the list below:");
			for(i=0; i<total_pools; i++)
			{
				struct pool *pool=pools[i];
				applog(LOG_WARNING, "Pool: %d  URL: %s User: %s Password: %s", i, pool->rpc_url, pool->rpc_user, pool->rpc_pass);
			}
		}

		do {
			sleep(1);
			slept++;
		} while(!pools_active && slept<30);
	}

begin_bench:
	total_ghashes_done=0;
	for(i=0; i<total_devices; i++)
	{
		struct cgpu_info *cgpu=devices[i];
		cgpu->total_mhashes=0;
	}

	cgtime(&total_tv_start);
	cgtime(&total_tv_end);
	cgtime(&tv_hashmeter);
	get_datestamp(datestamp, sizeof(datestamp), &total_tv_start);

	watchpool_thr_id=2;
	thr=&control_thr[watchpool_thr_id];
	/* start watchpool thread */
	if(thr_info_create(thr, NULL, watchpool_thread, NULL))
		early_quit(1, "watchpool thread create failed");
	pthread_detach(thr->pth);

	watchdog_thr_id=3;
	thr=&control_thr[watchdog_thr_id];
	/* start watchdog thread */
	if(thr_info_create(thr, NULL, watchdog_thread, NULL))
		early_quit(1, "watchdog thread create failed");
	pthread_detach(thr->pth);

	/* Create API socket thread */
	api_thr_id=5;
	thr=&control_thr[api_thr_id];
	if(thr_info_create(thr, NULL, api_thread, thr))
		early_quit(1, "API thread create failed");

#ifdef USE_USBUTILS
	hotplug_thr_id=6;
	thr=&control_thr[hotplug_thr_id];
	if(thr_info_create(thr, NULL, hotplug_thread, thr))
		early_quit(1, "hotplug thread create failed");
	pthread_detach(thr->pth);
#endif

	/* Just to be sure */
	if(total_control_threads != 8)
		early_quit(1, "incorrect total_control_threads (%d) should be 8", total_control_threads);

	set_highprio();

#ifdef USE_LIBSYSTEMD
	sd_notify(false, "READY=1\n"
		"STATUS=Started");
#endif

	/* Once everything is set up, main() becomes the getwork scheduler */
	while(42)
	{
		int ts, max_staged=max_queue;
		struct pool *pool;

		if(opt_work_update)
			signal_work_update();

		opt_work_update=false;

		if(opt_clean_jobs)
		{
			opt_clean_jobs=false;
			signal_clean_jobs();
		}

		mutex_lock(stgd_lock);
		ts=total_staged();
		/* Wait until hash_pop tells us we need to create more work */
		if(ts>max_staged)
		{
			work_filled=true;
			pthread_cond_wait(&gws_cond, stgd_lock);
			ts=total_staged();
		}
		mutex_unlock(stgd_lock);

		if(ts>max_staged)
		{
			/* Keeps slowly generating work even if it's not being
			 * used to keep last_getwork incrementing and to see
			 * if pools are still alive. */
			work_filled=true;
			work=hash_pop(false);
			if(work)
			{
				discard_work(&work);
			}
			continue;
		}

		if(work)
		{
			discard_work(&work);
		}
		work=make_work();

		while(42)
		{
			pool=select_pool();
			if(!pool_unusable(pool))
			{
				break;
			}
			switch_pools(NULL);
			pool=select_pool();
			if(pool_unusable(pool))
			{
				cgsleep_ms(5);
			}
		};
		if(pool->has_stratum)
		{
			if(opt_gen_stratum_work)
			{
				gen_stratum_work(pool, work);
				applog(LOG_DEBUG, "Generated stratum work");
				stage_work(work);
			}
			continue;
		}
		if(opt_benchmark)
		{
			get_benchmark_work(work);
			applog(LOG_DEBUG, "Generated benchmark work");
			stage_work(work);
			continue;
		}
	}

	return 0;
}
