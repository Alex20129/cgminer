#ifndef __UTIL_H__
#define __UTIL_H__

#include <semaphore.h>
#include <stdbool.h>

#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SOCKETTYPE long
#define SOCKETFAIL(a) (a<0)
#define INVSOCK -1
#define INVINETADDR -1
#define CLOSESOCKET close
#define INET_PTON inet_pton

#define SOCKERRMSG strerror(errno)
static inline bool sock_blocks(void)
{
    return(errno == EAGAIN || errno == EWOULDBLOCK);
}
static inline bool sock_timeout(void)
{
    return(errno == ETIMEDOUT);
}
static inline bool interrupted(void)
{
    return(errno == EINTR);
}

#ifdef HAVE_LIBCURL
#include <curl/curl.h>
typedef curl_proxytype proxytypes_t;
#else
typedef int proxytypes_t;
#endif /* HAVE_LIBCURL */

#if defined(ENABLE_ASIC_BOOST)
#define STRATUM_VERSION_ROLLING_STRING "version-rolling"
#define VERSION_BITS_NUM 2
#define MIDSTATE_NUM 4
#define VERSION_BITS_S9 0x00c00000

#endif /* ENABLE_ASIC_BOOST */

/* cgminer locks, a write biased variant of rwlocks */
struct cglock
{
	pthread_mutex_t mutex;
	pthread_rwlock_t rwlock;
};

typedef struct cglock cglock_t;

/* cgminer specific unnamed semaphore implementations to cope with osx not
 * implementing them. */
#ifdef __APPLE__
struct cgsem {
	int pipefd[2];
};

typedef struct cgsem cgsem_t;
#else
typedef sem_t cgsem_t;
#endif
typedef struct timespec cgtimer_t;

struct thread_q
{
	struct list_head	q;
	bool frozen;
	pthread_mutex_t		mutex;
	pthread_cond_t		cond;
};

struct thr_info
{
	int		id;
	int		device_thread;
	bool	primary_thread;

	pthread_t	pth;
	cgsem_t		sem;
	struct thread_q	*q;
	struct cgpu_info *cgpu;
	void *cgpu_data;
	struct timeval last;
	struct timeval sick;

	bool	pause;
	bool	getwork;

	bool	work_restart;
	bool	work_update;
	bool	clean_jobs;
};

#if defined(USE_ANTMINER_L3)
extern void rev(uint8_t *s, size_t l);
#endif

extern int no_yield(void);
extern int (*selective_yield)(void);
void *_cgmalloc(size_t size, const char *file, const char *func, const int line);
void *_cgcalloc(const size_t memb, size_t size, const char *file, const char *func, const int line);
void *_cgrealloc(void *ptr, size_t size, const char *file, const char *func, const int line);
#define cgmalloc(_size) _cgmalloc(_size, __FILE__, __func__, __LINE__)
#define cgcalloc(_memb, _size) _cgcalloc(_memb, _size, __FILE__, __func__, __LINE__)
#define cgrealloc(_ptr, _size) _cgrealloc(_ptr, _size, __FILE__, __func__, __LINE__)
struct thr_info;
struct pool;
struct cgpu_info;

void b58tobin(uint8_t *b58bin, const char *b58);
void address_to_pubkeyhash(uint8_t *pkh, const char *addr);
int ser_number(uint8_t *s, int32_t val);
bool fulltest(const uint8_t *hash, const uint8_t *target);
struct thread_q *tq_new(void);
void tq_free(struct thread_q *tq);
void tq_freeze(struct thread_q *tq);
void tq_thaw(struct thread_q *tq);
bool tq_push(struct thread_q *tq, void *data);
void *tq_pop(struct thread_q *tq);
int thr_info_create(struct thr_info *thr, pthread_attr_t *attr, void *(*start) (void *), void *arg);
void thr_info_cancel(struct thr_info *thr);
void cgtime(struct timeval *tv);
void subtime(struct timeval *a, struct timeval *b);
void addtime(struct timeval *a, struct timeval *b);
bool time_more(struct timeval *a, struct timeval *b);
bool time_less(struct timeval *a, struct timeval *b);
void copy_time(struct timeval *dest, const struct timeval *src);
void timespec_to_val(struct timeval *val, const struct timespec *spec);
void timeval_to_spec(struct timespec *spec, const struct timeval *val);
void us_to_timeval(struct timeval *val, int64_t us);
void us_to_timespec(struct timespec *spec, int64_t us);
void ms_to_timespec(struct timespec *spec, int64_t ms);
void timeraddspec(struct timespec *a, const struct timespec *b);
char *Strsep(char **stringp, const char *delim);
void cgsleep_ms(int ms);
void cgsleep_us(int64_t us);
void cgtimer_time(cgtimer_t *ts_start);
#define cgsleep_prepare_r(ts_start) cgtimer_time(ts_start)
void cgsleep_ms_r(cgtimer_t *ts_start, int ms);
void cgsleep_us_r(cgtimer_t *ts_start, int64_t us);
int cgtimer_to_ms(cgtimer_t *cgt);
void cgtimer_sub(cgtimer_t *a, cgtimer_t *b, cgtimer_t *res);
double us_tdiff(struct timeval *end, struct timeval *start);
int ms_tdiff(struct timeval *end, struct timeval *start);
double tdiff(struct timeval *end, struct timeval *start);
bool stratum_send(struct pool *pool, char *s, ssize_t len);
bool sock_full(struct pool *pool);
char *recv_line(struct pool *pool);
bool parse_method(struct pool *pool, char *s);
bool subscribe_extranonce(struct pool *pool);
bool extract_sockaddr(char *url, char **sockaddr_url, char **sockaddr_port);
bool auth_stratum(struct pool *pool);
bool initiate_stratum(struct pool *pool);
bool restart_stratum(struct pool *pool);
void suspend_stratum(struct pool *pool);
void *realloc_strcat(char *ptr, char *s);
void *str_text(char *ptr);
void RenameThread(const char *name);
void cgsem_init(cgsem_t *cgsem);
void cgsem_post(cgsem_t *cgsem);
void cgsem_wait(cgsem_t *cgsem);
int cgsem_mswait(cgsem_t *cgsem, int ms);
void cgsem_reset(cgsem_t *cgsem);
void cgsem_destroy(cgsem_t *cgsem);
bool cg_completion_timeout(void (*fn)(void *fnarg), void *fnarg, int timeout);
void _cg_memcpy(void *dest, const void *src, unsigned int n, const char *file, const char *func, const int line);

#define cg_memcpy(dest, src, n) _cg_memcpy(dest, src, n, __FILE__, __func__, __LINE__)

#endif /* __UTIL_H__ */
