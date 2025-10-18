/*
 * Copyright 2011-2015 Andrew Smith
 * Copyright 2011-2015 Con Kolivas
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or(at your option)
 * any later version.  See COPYING for more details.
 */
#define _MEMORY_DEBUG_MASTER 1

#include "config.h"

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>

#include "cgminer.h"
#include "util.h"
#include "klist.h"

#if defined(USE_AVALON2) || defined(USE_AVALON4) || defined(USE_AVALON7) || defined(USE_AVALON8) || defined(USE_AVALON9)
#define HAVE_AN_ASIC 1
#endif

#if defined(USE_S21_DRIVER)
#include "driver-s21/driver-s21.h"
#endif // USE_S21_DRIVER

// BUFSIZ varies on Windows and Linux
#define TMPBUFSIZ	8192

// Number of requests to queue - normally would be small
// However lots of PGA's may mean more
#define QUEUE	100

#if defined(__APPLE__) || defined(__FreeBSD__)
#define IPV6_ADD_MEMBERSHIP IPV6_JOIN_GROUP
#define IPV6_DROP_MEMBERSHIP IPV6_LEAVE_GROUP
#endif

static const char *UNAVAILABLE=" - API will not be available";
static const char *MUNAVAILABLE=" - API multicast listener will not be available";

static const char *BLANK="";
static const char *COMMA=",";
static const char SEPARATOR='|';
#define COMSTR ","
//#define CMDJOIN '+'
#define JOIN_CMD "CMD="

static const char *APIVERSION="3.8";
static const char *DEAD="Dead";
#if defined(HAVE_AN_ASIC)
static const char *SICK="Sick";
static const char *NOSTART="NoStart";
static const char *INIT="Initialising";
#endif
static const char *DISABLED="Disabled";
static const char *ALIVE="Alive";
static const char *REJECTING="Rejecting";
static const char *UNKNOWN="Unknown";

static const char *YES_STR="Y";
static const char *NO_STR="N";
static const char *NULL_STR="(null)";
static const char *TRUE_STR="true";
static const char *FALSE_STR="false";
static const char *SHA256_STR="sha256";

#define _DEVS		"DEVS"
#define _POOLS		"POOLS"
#define _SUMMARY	"SUMMARY"
#define _STATUS		"STATUS"
#define _VERSION	"VERSION"
#define _CONFIG		"CONFIG"

#ifdef HAVE_AN_ASIC
#define _ASC		"ASC"
#endif

#define _ASCS		"ASCS"
#define _NOTIFY		"NOTIFY"
#define _DEVDETAILS	"DEVDETAILS"
#define _BYE		"BYE"
#define _RESTART	"RESTART"
#define _MINESTATS	"STATS"
#define _CHECK		"CHECK"
#define _MINECOIN	"COIN"
#define _DEBUGSET	"DEBUG"
#define _SETCONFIG	"SETCONFIG"
#define _USBSTATS	"USBSTATS"

#define JSON_ARRAY_BEGIN	"["
#define JSON_ARRAY_END		"]"

#define JSON0		"{"
#define JSON1		"\""
#define JSON4		JSON1 ":"
#define JSON5		"}"

#define JSON_START	JSON0
#define JSON_DEVS	JSON1 _DEVS JSON4 JSON_ARRAY_BEGIN
#define JSON_POOLS	JSON1 _POOLS JSON4 JSON_ARRAY_BEGIN
#define JSON_SUMMARY	JSON1 _SUMMARY JSON4
#define JSON_STATUS	JSON1 _STATUS JSON4
#define JSON_VERSION	JSON1 _VERSION JSON4
#define JSON_CONFIG			JSON1 _CONFIG JSON4
#define JSON_MINESTATS		JSON1 _MINESTATS JSON4
#define JSON_BETWEEN_JOIN	","
#define JSON_END			JSON5

#ifdef HAVE_AN_ASIC
#define JSON_ASC	JSON1 _ASC JSON2
#endif

#define JSON_ASCS	JSON1 _ASCS JSON4 JSON_ARRAY_BEGIN
#define JSON_NOTIFY	JSON1 _NOTIFY JSON4 JSON_ARRAY_BEGIN
#define JSON_DEVDETAILS	JSON1 _DEVDETAILS JSON4 JSON_ARRAY_BEGIN
#define JSON_BYE	JSON1 _BYE JSON1
#define JSON_RESTART	JSON1 _RESTART JSON1
#define JSON_CHECK	JSON1 _CHECK JSON4 JSON_ARRAY_BEGIN
#define JSON_MINECOIN	JSON1 _MINECOIN JSON4 JSON_ARRAY_BEGIN
#define JSON_DEBUGSET	JSON1 _DEBUGSET JSON4 JSON_ARRAY_BEGIN
#define JSON_SETCONFIG	JSON1 _SETCONFIG JSON4 JSON_ARRAY_BEGIN
#define JSON_USBSTATS	JSON1 _USBSTATS JSON4 JSON_ARRAY_BEGIN

static const char *JSON_COMMAND="command";
static const char *JSON_PARAMETER="parameter";

#define MSG_POOL 7
#define MSG_NOPOOL 8
#define MSG_DEVS 9
#define MSG_NODEVS 10
#define MSG_SUMM 11
#define MSG_INVCMD 14
#define MSG_MISID 15

#define MSG_VERSION 22
#define MSG_INVJSON 23
#define MSG_MISCMD 24
#define MSG_MISPID 25
#define MSG_INVPID 26
#define MSG_SWITCHP 27
#define MSG_MISVAL 28
#define MSG_NOADL 29
#define MSG_INVINT 31
#define MSG_CONFIG 33

#if defined(USE_S21_DRIVER)
#define MSG_VOLTAGE 34
#define MSG_TUNER_MODE 35
#define MSG_FREQUENCY 36
#define MSG_TEMPERATURE 37
#define MSG_HASHRATE 38
#define MSG_CONSUMPTION 39
#define MSG_FANSPEED 40
#endif // USE_S21_DRIVER

#define MSG_MISFN 42
#define MSG_BADFN 43
#define MSG_SAVED 44
#define MSG_ACCDENY 45
#define MSG_ACCOK 46
#define MSG_ENAPOOL 47
#define MSG_DISPOOL 48
#define MSG_ALRENAP 49
#define MSG_ALRDISP 50
#define MSG_DISLASTP 51
#define MSG_MISPDP 52
#define MSG_INVPDP 53
#define MSG_TOOMANYP 54
#define MSG_ADDPOOL 55

#define MSG_NOTIFY 60

#define MSG_REMLASTP 66
#define MSG_ACTPOOL 67
#define MSG_REMPOOL 68
#define MSG_DEVDETAILS 69
#define MSG_MINESTATS 70
#define MSG_MISCHK 71
#define MSG_CHECK 72
#define MSG_POOLPRIO 73
#define MSG_DUPPID 74
#define MSG_MISBOOL 75
#define MSG_INVBOOL 76
#define MSG_FOO 77
#define MSG_MINECOIN 78
#define MSG_DEBUGSET 79
#define MSG_PGAIDENT 80
#define MSG_PGANOID 81
#define MSG_SETCONFIG 82
#define MSG_UNKCON 83
#define MSG_INVNUM 84
#define MSG_CONPAR 85
#define MSG_CONVAL 86
#define MSG_USBSTA 87
#define MSG_NOUSTA 88

#define MSG_ZERMIS 94
#define MSG_ZERINV 95
#define MSG_ZERSUM 96
#define MSG_ZERNOSUM 97
#define MSG_PGAUSBNODEV 98

#define MSG_NUMASC 104
#ifdef HAVE_AN_ASIC
#define MSG_ASCNON 105
#define MSG_ASCDEV 106
#define MSG_INVASC 107
#define MSG_ASCLRENA 108
#define MSG_ASCLRDIS 109
#define MSG_ASCENA 110
#define MSG_ASCDIS 111
#define MSG_ASCUNW 112
#define MSG_ASCIDENT 113
#define MSG_ASCNOID 114
#endif
#define MSG_ASCUSBNODEV 115

#ifdef HAVE_AN_ASIC
#define MSG_MISASCOPT 116
#define MSG_ASCNOSET 117
#define MSG_ASCHELP 118
#define MSG_ASCSETOK 119
#define MSG_ASCSETERR 120
#endif

#define MSG_INVNEG 121
#define MSG_SETQUOTA 122
#define MSG_LOCKOK 123
#define MSG_LOCKDIS 124

#define MSG_DEPRECATED 126

enum code_parameters
{
	PARAM_PGA,
	PARAM_ASC,
	PARAM_PID,
	PARAM_PGAMAX,
	PARAM_ASCMAX,
	PARAM_PMAX,
	PARAM_POOLMAX,
// Single generic case: have the code resolve it - see below
	PARAM_DMAX,
	PARAM_CMD,
	PARAM_POOL,
	PARAM_STR,
	PARAM_BOTH,
	PARAM_BOOL,
	PARAM_SET,
	PARAM_INT,
	PARAM_NONE
};

struct CODES
{
	const int code;
	const enum code_parameters params;
	const char *description;
} codes[]=
{
 { MSG_POOL,	PARAM_PMAX,	"%d Pool(s)" },
 { MSG_NOPOOL,	PARAM_NONE,	"No pools" },

 { MSG_DEVS,	PARAM_DMAX,
#ifdef HAVE_AN_ASIC
	"%d ASC(s)"
#else
	"Devs"
#endif
 },

 { MSG_NODEVS,	PARAM_NONE,	"No "
#if defined(HAVE_AN_ASIC)
	"ASCs"
#else
	"Devs"
#endif
 },

 { MSG_SUMM,	PARAM_NONE,	"Summary" },
 { MSG_INVCMD,	PARAM_NONE,	"Invalid command" },
 { MSG_MISID,	PARAM_NONE,	"Missing device id parameter" },
 { MSG_NUMASC,	PARAM_NONE,	"ASC count" },
 { MSG_VERSION,	PARAM_NONE,	"CGMiner versions" },
 { MSG_INVJSON,	PARAM_NONE,	"Invalid JSON" },
 { MSG_MISCMD,	PARAM_CMD,	"Missing JSON '%s'" },
 { MSG_MISPID,	PARAM_NONE,	"Missing pool id parameter" },
 { MSG_INVPID,	PARAM_POOLMAX,	"Invalid pool id %d - range is 0 - %d" },
 { MSG_SWITCHP,	PARAM_POOL,	"Switching to pool %d:'%s'" },
 { MSG_CONFIG,	PARAM_NONE,	"CGMiner config" },
#if defined(USE_S21_DRIVER)
 { MSG_VOLTAGE,	PARAM_NONE,	"Voltage" },
 { MSG_TUNER_MODE,	PARAM_NONE,	"Autotuner mode" },
 { MSG_FREQUENCY,	PARAM_NONE,	"Frequency" },
 { MSG_TEMPERATURE,	PARAM_NONE,	"Temperature" },
 { MSG_HASHRATE,	PARAM_NONE,	"Hash rate" },
 { MSG_CONSUMPTION,	PARAM_NONE,	"Power consumption" },
 { MSG_FANSPEED,	PARAM_NONE,	"Fan speed" },
#endif // USE_S21_DRIVER
 { MSG_MISFN,	PARAM_NONE,	"Missing save filename parameter" },
 { MSG_BADFN,	PARAM_STR,	"Can't open or create save file '%s'" },
 { MSG_SAVED,	PARAM_STR,	"Configuration saved to file '%s'" },
 { MSG_ACCDENY,	PARAM_STR,	"Access denied to '%s' command" },
 { MSG_ACCOK,	PARAM_NONE,	"Privileged access OK" },
 { MSG_ENAPOOL,	PARAM_POOL,	"Enabling pool %d:'%s'" },
 { MSG_POOLPRIO,PARAM_NONE,	"Changed pool priorities" },
 { MSG_DUPPID,	PARAM_PID,	"Duplicate pool specified %d" },
 { MSG_DISPOOL,	PARAM_POOL,	"Disabling pool %d:'%s'" },
 { MSG_ALRENAP,	PARAM_POOL,	"Pool %d:'%s' already enabled" },
 { MSG_ALRDISP,	PARAM_POOL,	"Pool %d:'%s' already disabled" },
 { MSG_DISLASTP,PARAM_POOL,	"Cannot disable last active pool %d:'%s'" },
 { MSG_MISPDP,	PARAM_NONE,	"Missing addpool details" },
 { MSG_INVPDP,	PARAM_STR,	"Invalid addpool details '%s'" },
 { MSG_TOOMANYP,PARAM_NONE,	"Reached maximum number of pools (%d)" },
 { MSG_ADDPOOL,	PARAM_POOL,	"Added pool %d: '%s'" },
 { MSG_REMLASTP,PARAM_POOL,	"Cannot remove last pool %d:'%s'" },
 { MSG_ACTPOOL, PARAM_POOL,	"Cannot remove active pool %d:'%s'" },
 { MSG_REMPOOL, PARAM_BOTH,	"Removed pool %d:'%s'" },
 { MSG_NOTIFY,	PARAM_NONE,	"Notify" },
 { MSG_DEVDETAILS,PARAM_NONE,	"Device Details" },
 { MSG_MINESTATS,PARAM_NONE,	"CGMiner stats" },
 { MSG_MISCHK,	PARAM_NONE,	"Missing check cmd" },
 { MSG_CHECK,	PARAM_NONE,	"Check command" },
 { MSG_MISBOOL,	PARAM_NONE,	"Missing parameter: true/false" },
 { MSG_INVBOOL,	PARAM_NONE,	"Invalid parameter should be true or false" },
 { MSG_FOO,	PARAM_BOOL,	"Failover-Only set to %s" },
 { MSG_MINECOIN,PARAM_NONE,	"CGMiner coin" },
 { MSG_DEBUGSET,PARAM_NONE,	"Debug settings" },
 { MSG_SETCONFIG,PARAM_SET,	"Set config '%s' to %d" },
 { MSG_UNKCON,	PARAM_STR,	"Unknown config '%s'" },
 { MSG_DEPRECATED, PARAM_STR,	"Deprecated config option '%s'" },
 { MSG_INVNUM,	PARAM_BOTH,	"Invalid number (%d) for '%s' range is 0-9999" },
 { MSG_INVNEG,	PARAM_BOTH,	"Invalid negative number (%d) for '%s'" },
 { MSG_SETQUOTA,PARAM_SET,	"Set pool '%s' to quota %d'" },
 { MSG_CONPAR,	PARAM_NONE,	"Missing config parameters 'name,N'" },
 { MSG_CONVAL,	PARAM_STR,	"Missing config value N for '%s,N'" },
 { MSG_USBSTA,	PARAM_NONE,	"USB Statistics" },
 { MSG_NOUSTA,	PARAM_NONE,	"No USB Statistics" },
 { MSG_ZERMIS,	PARAM_NONE,	"Missing zero parameters" },
 { MSG_ZERINV,	PARAM_STR,	"Invalid zero parameter '%s'" },
 { MSG_ZERSUM,	PARAM_STR,	"Zeroed %s stats with summary" },
 { MSG_ZERNOSUM, PARAM_STR,	"Zeroed %s stats without summary" },
#ifdef HAVE_AN_ASIC
 { MSG_ASCNON,	PARAM_NONE,	"No ASCs" },
 { MSG_ASCDEV,	PARAM_ASC,	"ASC%d" },
 { MSG_INVASC,	PARAM_ASCMAX,	"Invalid ASC id %d - range is 0 - %d" },
 { MSG_ASCLRENA,PARAM_ASC,	"ASC %d already enabled" },
 { MSG_ASCLRDIS,PARAM_ASC,	"ASC %d already disabled" },
 { MSG_ASCENA,	PARAM_ASC,	"ASC %d sent enable message" },
 { MSG_ASCDIS,	PARAM_ASC,	"ASC %d set disable flag" },
 { MSG_ASCUNW,	PARAM_ASC,	"ASC %d is not flagged WELL, cannot enable" },
 { MSG_ASCIDENT,PARAM_ASC,	"Identify command sent to ASC%d" },
 { MSG_ASCNOID,	PARAM_ASC,	"ASC%d does not support identify" },
 { MSG_MISASCOPT, PARAM_NONE,	"Missing option after ASC number" },
 { MSG_ASCNOSET, PARAM_ASC,	"ASC %d does not support ascset" },
 { MSG_ASCHELP, PARAM_BOTH,	"ASC %d set help: %s" },
 { MSG_ASCSETOK, PARAM_BOTH,	"ASC %d set OK" },
 { MSG_ASCSETERR, PARAM_BOTH,	"ASC %d set failed: %s" },
#endif
 { MSG_LOCKOK,	PARAM_NONE,	"Lock stats created" },
 { MSG_LOCKDIS,	PARAM_NONE,	"Lock stats not enabled" },
 { 0xFF, 0, NULL }
};

static const char *localaddr="127.0.0.1";

static int my_thr_id=0;
static bool bye;

// Used to control quit restart access to shutdown variables
static pthread_mutex_t quit_restart_lock;

static bool do_a_quit;
static bool do_a_restart;

static time_t when=0;	// when the request occurred

struct IPACCESS {
	struct in6_addr ip;
	struct in6_addr mask;
	char group;
};

#define GROUP(g) (toupper(g))
#define PRIVGROUP GROUP('W')
#define NOPRIVGROUP GROUP('R')
#define ISPRIVGROUP(g) (GROUP(g)==PRIVGROUP)
#define GROUPOFFSET(g) (GROUP(g) - GROUP('A'))
#define VALIDGROUP(g) (GROUP(g) >= GROUP('A') && GROUP(g) <= GROUP('Z'))
#define COMMANDS(g) (apigroups[GROUPOFFSET(g)].commands)
#define DEFINEDGROUP(g) (ISPRIVGROUP(g) || COMMANDS(g) != NULL)

struct APIGROUPS {
	// This becomes a string like: "|cmd1|cmd2|cmd3|" so it's quick to search
	char *commands;
} apigroups['Z' - 'A' + 1]; // only A=0 to Z=25 (R: noprivs, W: allprivs)

static struct IPACCESS *ipaccess=NULL;
static int ips=0;

struct io_data {
	size_t siz;
	char *ptr;
	char *cur;
	bool sock;
	bool close;
};

struct io_list {
	struct io_data *io_data;
	struct io_list *prev;
	struct io_list *next;
};

static struct io_list *io_head=NULL;

#define SOCKBUFALLOCSIZ 65536

#define io_new(init) _io_new(init, false)
#define sock_io_new() _io_new(SOCKBUFALLOCSIZ, true)

#define ALLOC_SBITEMS 2
#define LIMIT_SBITEMS 0

typedef struct sbitem {
	char *buf;
	size_t siz;
	size_t tot;
} SBITEM;

// Size to grow tot if exceeded
#define SBEXTEND 4096

#define DATASB(_item) ((SBITEM *)(_item->data))

static K_LIST *strbufs;

static void io_reinit(struct io_data *io_data)
{
    io_data->cur=io_data->ptr;
    *(io_data->ptr)='\0';
    io_data->close=false;
}

static struct io_data *_io_new(size_t initial, bool socket_buf)
{
	struct io_data *io_data;
	struct io_list *io_list;

    io_data=cgmalloc(sizeof(*io_data));
    io_data->ptr=cgmalloc(initial);
    io_data->siz=initial;
    io_data->sock=socket_buf;
	io_reinit(io_data);

    io_list=cgmalloc(sizeof(*io_list));

    io_list->io_data=io_data;

	if(io_head) {
        io_list->next=io_head;
        io_list->prev=io_head->prev;
        io_list->next->prev=io_list;
        io_list->prev->next=io_list;
	} else {
        io_list->prev=io_list;
        io_list->next=io_list;
        io_head=io_list;
	}

	return io_data;
}

static void io_add(struct io_data *io_data, char *buf)
{
	size_t len, dif, tot;
    len=strlen(buf);
    dif=io_data->cur - io_data->ptr;
	// send will always have enough space to add the JSON
	tot=len + 1 + dif + sizeof(JSON_END);
	if(tot > io_data->siz)
	{
        size_t new=io_data->siz + (2 * SOCKBUFALLOCSIZ);

		if(new < tot)
            new=(2 + (size_t)((float)tot / (float)SOCKBUFALLOCSIZ)) * SOCKBUFALLOCSIZ;

        io_data->ptr=cgrealloc(io_data->ptr, new);
        io_data->cur=io_data->ptr + dif;
        io_data->siz=new;
	}
	memcpy(io_data->cur, buf, len + 1);
	io_data->cur += len;
}

static void io_put(struct io_data *io_data, char *buf)
{
	io_reinit(io_data);
	io_add(io_data, buf);
}

static void io_close(struct io_data *io_data)
{
    io_data->close=true;
}

static void io_free()
{
	struct io_list *io_list, *io_next;

	if(io_head) {
        io_list=io_head;
		do {
            io_next=io_list->next;

			free(io_list->io_data->ptr);
			free(io_list->io_data);
			free(io_list);

            io_list=io_next;
		} while(io_list != io_head);

        io_head=NULL;
	}
}

// This is only called when expected to be needed (rarely)
// i.e. strings outside of the codes control (input from the user)
static char *escape_string(char *str, bool isjson)
{
	char *buf, *ptr;
	int count;

    count=0;
    for(ptr=str; *ptr; ptr++) {
		switch (*ptr) {
			case ',':
			case '|':
			case '=':
				if(!isjson)
					count++;
				break;
			case '"':
				if(isjson)
					count++;
				break;
			case '\\':
				count++;
				break;
		}
	}

    if(count==0)
		return str;

    buf=cgmalloc(strlen(str) + count + 1);

    ptr=buf;
	while(*str)
		switch (*str) {
			case ',':
			case '|':
			case '=':
				if(!isjson)
                    *(ptr++)='\\';
                *(ptr++)=*(str++);
				break;
			case '"':
				if(isjson)
                    *(ptr++)='\\';
                *(ptr++)=*(str++);
				break;
			case '\\':
                *(ptr++)='\\';
                *(ptr++)=*(str++);
				break;
			default:
                *(ptr++)=*(str++);
				break;
		}

    *ptr='\0';

	return buf;
}

static struct api_data *api_add_extra(struct api_data *root, struct api_data *extra)
{
	struct api_data *tmp;

	if(root)
	{
		if(extra)
		{
			// extra tail
            tmp=extra->prev;

            // extra prev=root tail
            extra->prev=root->prev;

            // root tail next=extra
            root->prev->next=extra;

            // extra tail next=root
            tmp->next=root;

            // root prev=extra tail
            root->prev=tmp;
		}
	}
	else
	{
		root=extra;
	}
	return root;
}

static struct api_data *api_add_data_full(struct api_data *root, char *name, enum api_data_type type, void *data, bool copy_data)
{
	struct api_data *api_data;
	size_t siz;
    api_data=cgmalloc(sizeof(struct api_data));
    api_data->name=strdup(name);
    api_data->type=type;
	if(root==NULL)
	{
        root=api_data;
        root->prev=root;
        root->next=root;
	}
	else
	{
        api_data->prev=root->prev;
        root->prev=api_data;
        api_data->next=root;
        api_data->prev->next=api_data;
	}

    api_data->data_was_malloc=copy_data;

	// Avoid crashing on bad data
	if(data==NULL)
	{
        api_data->type=type=API_CONST;
		data=(void *)NULL_STR;
        api_data->data_was_malloc=copy_data=false;
	}

	if(!copy_data)
	{
		api_data->data=data;
	}
	else
	{
		switch(type)
		{
			case API_ESCAPE:
			case API_STRING:
			case API_CONST:
				siz=strlen((char *)data)+1;
				api_data->data=cgmalloc(siz);
				strncpy(api_data->data, data, siz);
				break;
			case API_INT_ARRAY:
				siz=((int *)data)[0]+1;
				siz=siz*sizeof(int);
				api_data->data=cgmalloc(siz);
				memcpy(api_data->data, data, siz);
				break;
			case API_INT:
				api_data->data=cgmalloc(sizeof(int));
				*(int *)api_data->data=*(int *)data;
				break;
			case API_INT8:
				/* Most OSs won't really alloc less than 4 */
				api_data->data=cgmalloc(4);
				*(int8_t *)api_data->data=*(int8_t *)data;
			break;
			case API_INT16:
				/* Most OSs won't really alloc less than 4 */
                api_data->data=cgmalloc(4);
                *(int16_t *)api_data->data=*(int16_t *)data;
				break;
			case API_INT32:
				api_data->data=cgmalloc(sizeof(int32_t));
				*(int32_t *)api_data->data=*(int32_t *)data;
				break;
			case API_INT64:
				api_data->data=cgmalloc(sizeof(int64_t));
				*(int64_t *)api_data->data=*(int64_t *)data;
				break;
			case API_UINT_ARRAY:
				siz=((uint *)data)[0]+1;
				siz=siz*sizeof(uint);
				api_data->data=cgmalloc(siz);
				memcpy(api_data->data, data, siz);
				break;
			case API_UINT:
				api_data->data=cgmalloc(sizeof(uint));
				*(uint *)api_data->data=*(uint *)data;
				break;
			case API_UINT8:
				/* Most OSs won't really alloc less than 4 */
				api_data->data=cgmalloc(4);
				*(uint8_t *)api_data->data=*((uint8_t *)data);
				break;
			case API_UINT16:
				/* Most OSs won't really alloc less than 4 */
				api_data->data=cgmalloc(4);
				*(uint16_t *)api_data->data=*((uint16_t *)data);
				break;
			case API_UINT32:
				api_data->data=cgmalloc(sizeof(uint32_t));
				*(uint32_t *)api_data->data=*((uint32_t *)data);
				break;
			case API_UINT64:
				api_data->data=cgmalloc(sizeof(uint64_t));
				*((uint64_t *)api_data->data)=*((uint64_t *)data);
				break;
			case API_DOUBLE:
			case API_ELAPSED:
			case API_MHS:
			case API_MHTOTAL:
			case API_UTILITY:
			case API_HS:
			case API_PERCENT:
                api_data->data=cgmalloc(sizeof(double));
				*((double *)api_data->data)=*((double *)data);
				break;
			case API_BOOL:
                api_data->data=cgmalloc(sizeof(bool));
				*((bool *)api_data->data)=*((bool *)data);
				break;
			case API_TIMEVAL:
                api_data->data=cgmalloc(sizeof(struct timeval));
				memcpy(api_data->data, data, sizeof(struct timeval));
				break;
			case API_TIME:
                api_data->data=cgmalloc(sizeof(time_t));
				*((time_t *)api_data->data)=*((time_t *)data);
				break;
			case API_FLOAT_ARRAY:
				siz=((float *)data)[0]+1;
				siz=siz*sizeof(float);
				api_data->data=cgmalloc(siz);
				memcpy(api_data->data, data, siz);
				break;
			case API_FLOAT:
			case API_VOLTS:
			case API_TEMP:
                api_data->data=cgmalloc(sizeof(float));
				*((float *)api_data->data)=*((float *)data);
				break;
			default:
				applog(LOG_ERR, "API: unknown1 data type %d ignored", type);
                api_data->type=API_STRING;
                api_data->data_was_malloc=false;
                api_data->data=(void *)UNKNOWN;
				break;
		}
	}
	return root;
}

struct api_data *api_add_escape(struct api_data *root, char *name, char *data, bool copy_data)
{
	return api_add_data_full(root, name, API_ESCAPE, (void *)data, copy_data);
}

struct api_data *api_add_string(struct api_data *root, char *name, char *data, bool copy_data)
{
	return api_add_data_full(root, name, API_STRING, (void *)data, copy_data);
}

struct api_data *api_add_const(struct api_data *root, char *name, const char *data, bool copy_data)
{
	return api_add_data_full(root, name, API_CONST, (void *)data, copy_data);
}

struct api_data *api_add_int_array(struct api_data *root, char *name, int *data, bool copy_data)
{
	return api_add_data_full(root, name, API_INT_ARRAY, (void *)data, copy_data);
}

struct api_data *api_add_int(struct api_data *root, char *name, int *data, bool copy_data)
{
	return api_add_data_full(root, name, API_INT, (void *)data, copy_data);
}

struct api_data *api_add_int8(struct api_data *root, char *name, int8_t *data, bool copy_data)
{
	return api_add_data_full(root, name, API_INT8, (void *)data, copy_data);
}

struct api_data *api_add_int16(struct api_data *root, char *name, int16_t *data, bool copy_data)
{
	return api_add_data_full(root, name, API_INT16, (void *)data, copy_data);
}

struct api_data *api_add_int32(struct api_data *root, char *name, int32_t *data, bool copy_data)
{
	return api_add_data_full(root, name, API_INT32, (void *)data, copy_data);
}

struct api_data *api_add_int64(struct api_data *root, char *name, int64_t *data, bool copy_data)
{
	return api_add_data_full(root, name, API_INT64, (void *)data, copy_data);
}

struct api_data *api_add_uint_array(struct api_data *root, char *name, uint *data, bool copy_data)
{
	return api_add_data_full(root, name, API_UINT_ARRAY, (void *)data, copy_data);
}

struct api_data *api_add_uint(struct api_data *root, char *name, uint *data, bool copy_data)
{
	return api_add_data_full(root, name, API_UINT, (void *)data, copy_data);
}

struct api_data *api_add_uint8(struct api_data *root, char *name, uint8_t *data, bool copy_data)
{
	return api_add_data_full(root, name, API_UINT8, (void *)data, copy_data);
}

struct api_data *api_add_uint16(struct api_data *root, char *name, uint16_t *data, bool copy_data)
{
	return api_add_data_full(root, name, API_UINT16, (void *)data, copy_data);
}

struct api_data *api_add_uint32(struct api_data *root, char *name, uint32_t *data, bool copy_data)
{
	return api_add_data_full(root, name, API_UINT32, (void *)data, copy_data);
}

struct api_data *api_add_uint64(struct api_data *root, char *name, uint64_t *data, bool copy_data)
{
	return api_add_data_full(root, name, API_UINT64, (void *)data, copy_data);
}

struct api_data *api_add_float_array(struct api_data *root, char *name, float *data, bool copy_data)
{
	return api_add_data_full(root, name, API_FLOAT_ARRAY, (void *)data, copy_data);
}

struct api_data *api_add_float(struct api_data *root, char *name, float *data, bool copy_data)
{
	return api_add_data_full(root, name, API_FLOAT, (void *)data, copy_data);
}

struct api_data *api_add_double(struct api_data *root, char *name, double *data, bool copy_data)
{
	return api_add_data_full(root, name, API_DOUBLE, (void *)data, copy_data);
}

struct api_data *api_add_elapsed(struct api_data *root, char *name, double *data, bool copy_data)
{
	return api_add_data_full(root, name, API_ELAPSED, (void *)data, copy_data);
}

struct api_data *api_add_bool(struct api_data *root, char *name, bool *data, bool copy_data)
{
	return api_add_data_full(root, name, API_BOOL, (void *)data, copy_data);
}

struct api_data *api_add_timeval(struct api_data *root, char *name, struct timeval *data, bool copy_data)
{
	return api_add_data_full(root, name, API_TIMEVAL, (void *)data, copy_data);
}

struct api_data *api_add_time(struct api_data *root, char *name, time_t *data, bool copy_data)
{
	return api_add_data_full(root, name, API_TIME, (void *)data, copy_data);
}

struct api_data *api_add_mhtotal(struct api_data *root, char *name, double *data, bool copy_data)
{
	return api_add_data_full(root, name, API_MHTOTAL, (void *)data, copy_data);
}

struct api_data *api_add_temp(struct api_data *root, char *name, float *data, bool copy_data)
{
	return api_add_data_full(root, name, API_TEMP, (void *)data, copy_data);
}

struct api_data *api_add_utility(struct api_data *root, char *name, double *data, bool copy_data)
{
	return api_add_data_full(root, name, API_UTILITY, (void *)data, copy_data);
}

struct api_data *api_add_volts(struct api_data *root, char *name, float *data, bool copy_data)
{
	return api_add_data_full(root, name, API_VOLTS, (void *)data, copy_data);
}

struct api_data *api_add_hs(struct api_data *root, char *name, double *data, bool copy_data)
{
	return api_add_data_full(root, name, API_HS, (void *)data, copy_data);
}

struct api_data *api_add_percent(struct api_data *root, char *name, double *data, bool copy_data)
{
	return api_add_data_full(root, name, API_PERCENT, (void *)data, copy_data);
}

static void add_item_buf(K_ITEM *item, const char *str)
{
	size_t old_siz, new_siz, siz, ext;
	char *buf;

    buf=DATASB(item)->buf;
    siz=(size_t)strlen(str);

    old_siz=DATASB(item)->siz;
    new_siz=old_siz + siz + 1; // include '\0'
	if(DATASB(item)->tot < new_siz) {
        ext=(siz + 1) + SBEXTEND - ((siz + 1) % SBEXTEND);
        DATASB(item)->buf=buf=cgrealloc(DATASB(item)->buf, DATASB(item)->tot + ext);
		DATASB(item)->tot += ext;
	}
	memcpy(buf + old_siz, str, siz + 1);
	DATASB(item)->siz += siz;
}

static struct api_data *print_data(struct io_data *io_data, struct api_data *root, bool precom)
{
	char buf[64], *original, *escape;
	struct api_data *tmp;
	K_ITEM *item;
    bool done, first=true;

	K_WLOCK(strbufs);
    item=k_unlink_head(strbufs);
	K_WUNLOCK(strbufs);

    DATASB(item)->siz=0;

	if(precom)
	{
		add_item_buf(item, COMMA);
	}

	add_item_buf(item, JSON_START);

	while(root)
	{
		if(!first)
		{
			add_item_buf(item, COMMA);
		}
		else
		{
			first=false;
		}

		add_item_buf(item, JSON1);
		add_item_buf(item, root->name);
		add_item_buf(item, JSON4);

        done=false;
		switch(root->type)
		{
			case API_ESCAPE:
				original=(char *)(root->data);
				escape=escape_string((char *)(root->data), true);
				add_item_buf(item, JSON1);
				add_item_buf(item, escape);
				add_item_buf(item, JSON1);
				if(escape != original)
				{
					free(escape);
				}
				done=true;
				break;
			case API_STRING:
			case API_CONST:
				add_item_buf(item, JSON1);
				add_item_buf(item, (char *)root->data);
				add_item_buf(item, JSON1);
                done=true;
				break;
			case API_INT_ARRAY:
				snprintf(buf, sizeof(buf), JSON_ARRAY_BEGIN "%u", ((int *)root->data)[1]);
				add_item_buf(item, buf);
				for(int i=2; i<=((int *)root->data)[0]; i++)
				{
					snprintf(buf, sizeof(buf), JSON_BETWEEN_JOIN "%u", ((int *)root->data)[i]);
					add_item_buf(item, buf);
				}
				snprintf(buf, sizeof(buf), JSON_ARRAY_END);
				add_item_buf(item, buf);
				done=true;
				break;
			case API_INT:
				snprintf(buf, sizeof(buf), "%i", *((int *)root->data));
				break;
			case API_INT8:
				snprintf(buf, sizeof(buf), "%"PRIi8, *((int8_t *)root->data));
				break;
			case API_INT16:
				snprintf(buf, sizeof(buf), "%"PRIi16, *((int16_t *)root->data));
				break;
			case API_INT32:
				snprintf(buf, sizeof(buf), "%"PRIi32, *((int32_t *)root->data));
				break;
			case API_INT64:
				snprintf(buf, sizeof(buf), "%"PRIi64, *((int64_t *)root->data));
				break;
			case API_UINT_ARRAY:
				snprintf(buf, sizeof(buf), JSON_ARRAY_BEGIN "%u", ((uint *)root->data)[1]);
				add_item_buf(item, buf);
				for(uint i=2; i<=((uint *)root->data)[0]; i++)
				{
					snprintf(buf, sizeof(buf), JSON_BETWEEN_JOIN "%u", ((uint *)root->data)[i]);
					add_item_buf(item, buf);
				}
				snprintf(buf, sizeof(buf), JSON_ARRAY_END);
				add_item_buf(item, buf);
				done=true;
				break;
			case API_UINT:
				snprintf(buf, sizeof(buf), "%u", *((uint *)root->data));
				break;
			case API_UINT8:
				snprintf(buf, sizeof(buf), "%"PRIu8, *((uint8_t *)root->data));
				break;
			case API_UINT16:
				snprintf(buf, sizeof(buf), "%"PRIu16, *((uint16_t *)root->data));
				break;
			case API_UINT32:
				snprintf(buf, sizeof(buf), "%"PRIu32, *((uint32_t *)root->data));
				break;
			case API_UINT64:
				snprintf(buf, sizeof(buf), "%"PRIu64, *((uint64_t *)root->data));
				break;
			case API_FLOAT_ARRAY:
				snprintf(buf, sizeof(buf), JSON_ARRAY_BEGIN "%0.02f", ((float *)root->data)[1]);
				add_item_buf(item, buf);
				for(int i=2; i<=((float *)root->data)[0]; i++)
				{
					snprintf(buf, sizeof(buf), JSON_BETWEEN_JOIN "%0.02f", ((float *)root->data)[i]);
					add_item_buf(item, buf);
				}
				snprintf(buf, sizeof(buf), JSON_ARRAY_END);
				add_item_buf(item, buf);
				done=true;
				break;
			case API_FLOAT:
				snprintf(buf, sizeof(buf), "%f", *((float *)root->data));
				break;
			case API_DOUBLE:
				snprintf(buf, sizeof(buf), "%lf", *((double *)root->data));
				break;
			case API_ELAPSED:
				snprintf(buf, sizeof(buf), "%.0f", *((double *)root->data));
				break;
			case API_TIME:
				snprintf(buf, sizeof(buf), "%lu", *((time_t *)root->data));
				break;
			case API_UTILITY:
			case API_MHS:
				snprintf(buf, sizeof(buf), "%.2f", *((double *)root->data));
				break;
			case API_MHTOTAL:
				snprintf(buf, sizeof(buf), "%.4f", *((double *)root->data));
				break;
			case API_HS:
				snprintf(buf, sizeof(buf), "%.15f", *((double *)root->data));
				break;
			case API_BOOL:
				snprintf(buf, sizeof(buf), "%s", *((bool *)root->data) ? TRUE_STR : FALSE_STR);
				break;
			case API_TIMEVAL:
				snprintf(buf, sizeof(buf), "%ld.%06ld",
					(long)((struct timeval *)root->data)->tv_sec,
					(long)((struct timeval *)root->data)->tv_usec);
				break;
			case API_VOLTS:
			case API_TEMP:
				snprintf(buf, sizeof(buf), "%0.02f", *((float *)root->data));
				break;
			case API_PERCENT:
				snprintf(buf, sizeof(buf), "%0.04lf", *((double *)root->data) * 100.0F);
				break;
			default:
				applog(LOG_ERR, "API: unknown2 data type %d ignored", root->type);
				add_item_buf(item, JSON1);
				add_item_buf(item, UNKNOWN);
				add_item_buf(item, JSON1);
                done=true;
				break;
		}

		if(!done)
		{
			add_item_buf(item, buf);
		}

		free(root->name);
		if(root->data_was_malloc)
		{
			free(root->data);
		}

		if(root->next==root)
		{
			free(root);
            root=NULL;
		}
		else
		{
            tmp=root;
            root=tmp->next;
            root->prev=tmp->prev;
            root->prev->next=root;
			free(tmp);
		}
	}

	add_item_buf(item, JSON_END);
	io_add(io_data, DATASB(item)->buf);

	K_WLOCK(strbufs);
	k_add_head(strbufs, item);
	K_WUNLOCK(strbufs);

	return root;
}

#define DRIVER_COUNT_DRV(X) \
	if(devices[i]->drv->drv_id==DRIVER_##X) \
	count++;

#ifdef HAVE_AN_ASIC
static int numascs(void)
{
    int count=0;
	int i;

	rd_lock(&devices_lock);
	for(i=0; i<total_devices; i++)
	{
		DRIVER_PARSE_COMMANDS(DRIVER_COUNT_DRV)
	}
	rd_unlock(&devices_lock);
	return count;
}

static int ascdevice(int ascid)
{
    int count=0;
	int i;

	rd_lock(&devices_lock);
	for(i=0; i<total_devices; i++)
	{
		DRIVER_PARSE_COMMANDS(DRIVER_COUNT_DRV)
        if(count==(ascid + 1))
		{
			goto foundit;
		}
	}

	rd_unlock(&devices_lock);
	return -1;

foundit:

	rd_unlock(&devices_lock);
	return i;
}
#endif

// All replies (except BYE and RESTART) start with a message
// thus for JSON, message() inserts JSON_START at the front
// and send_result() adds JSON_END at the end
static void message(struct io_data *io_data, int messageid, int paramid, char *param2)
{
    struct api_data *root=NULL;
	char buf[TMPBUFSIZ];
	int i;
#ifdef HAVE_AN_ASIC
	int asc;
#endif

	io_add(io_data, JSON_START);
	io_add(io_data, JSON_STATUS);

	for(i=0; codes[i].code<0xFF; i++)
	{
		if(codes[i].code==messageid)
		{
            switch(codes[i].params)
            {
				case PARAM_PGA:
				case PARAM_ASC:
				case PARAM_PID:
				case PARAM_INT:
                    sprintf(buf, codes[i].description, paramid);
					break;
				case PARAM_POOL:
					sprintf(buf, codes[i].description, paramid, pools[paramid]->rpc_url);
					break;
#ifdef HAVE_AN_ASIC
				case PARAM_ASCMAX:
                    asc=numascs();
					sprintf(buf, codes[i].description, paramid, asc - 1);
					break;
#endif
				case PARAM_PMAX:
					sprintf(buf, codes[i].description, total_pools);
					break;
				case PARAM_POOLMAX:
					sprintf(buf, codes[i].description, paramid, total_pools - 1);
					break;
				case PARAM_DMAX:
#ifdef HAVE_AN_ASIC
                    asc=numascs();
#endif
                    sprintf(buf, "%s", codes[i].description
#ifdef HAVE_AN_ASIC
						, asc
#endif
						);
					break;
				case PARAM_CMD:
					sprintf(buf, codes[i].description, JSON_COMMAND);
					break;
				case PARAM_STR:
					sprintf(buf, codes[i].description, param2);
					break;
				case PARAM_BOTH:
					sprintf(buf, codes[i].description, paramid, param2);
					break;
				case PARAM_BOOL:
					sprintf(buf, codes[i].description, paramid ? TRUE_STR : FALSE_STR);
					break;
				case PARAM_SET:
					sprintf(buf, codes[i].description, param2, paramid);
					break;
				case PARAM_NONE:
				default:
					strcpy(buf, codes[i].description);
			}

            root=api_add_time(root, "When", &when, false);
            root=api_add_int(root, "Code", &messageid, false);
            root=api_add_escape(root, "Msg", buf, false);
            root=api_add_escape(root, "Description", opt_api_description, false);
			root=print_data(io_data, root, false);
			return;
		}
	}

    root=api_add_time(root, "When", &when, false);
    int id=-1;
    root=api_add_int(root, "Code", &id, false);
	sprintf(buf, "%d", messageid);
    root=api_add_escape(root, "Msg", buf, false);
    root=api_add_escape(root, "Description", opt_api_description, false);
	root=print_data(io_data, root, false);
}

#if LOCK_TRACKING

#define LOCK_FMT_FFL " - called from %s %s():%d"

#define LOCKMSG(fmt, ...)	fprintf(stderr, "APILOCK: " fmt "\n", ##__VA_ARGS__)
#define LOCKMSGMORE(fmt, ...)	fprintf(stderr, "          " fmt "\n", ##__VA_ARGS__)
#define LOCKMSGFFL(fmt, ...) fprintf(stderr, "APILOCK: " fmt LOCK_FMT_FFL "\n", ##__VA_ARGS__, file, func, linenum)
#define LOCKMSGFLUSH() fflush(stderr)

typedef struct lockstat {
	uint64_t lock_id;
	const char *file;
	const char *func;
	int linenum;
	struct timeval tv;
} LOCKSTAT;

typedef struct lockline {
	struct lockline *prev;
	struct lockstat *stat;
	struct lockline *next;
} LOCKLINE;

typedef struct lockinfo {
	void *lock;
	enum cglock_typ typ;
	const char *file;
	const char *func;
	int linenum;
	uint64_t gets;
	uint64_t gots;
	uint64_t tries;
	uint64_t dids;
	uint64_t didnts; // should be tries - dids
	uint64_t unlocks;
	LOCKSTAT lastgot;
	LOCKLINE *lockgets;
	LOCKLINE *locktries;
} LOCKINFO;

typedef struct locklist {
	LOCKINFO *info;
	struct locklist *next;
} LOCKLIST;

static uint64_t lock_id=1;

static LOCKLIST *lockhead;

static void lockmsgnow()
{
	struct timeval now;
	struct tm *tm;
	time_t dt;

	cgtime(&now);

    dt=now.tv_sec;
    tm=localtime(&dt);

	LOCKMSG("%d-%02d-%02d %02d:%02d:%02d",
		tm->tm_year + 1900,
		tm->tm_mon + 1,
		tm->tm_mday,
		tm->tm_hour,
		tm->tm_min,
		tm->tm_sec);
}

static LOCKLIST *newlock(void *lock, enum cglock_typ typ, const char *file, const char *func, const int linenum)
{
	LOCKLIST *list;

    list=cgcalloc(1, sizeof(*list));
    list->info=cgcalloc(1, sizeof(*(list->info)));
    list->next=lockhead;
    lockhead=list;

    list->info->lock=lock;
    list->info->typ=typ;
    list->info->file=file;
    list->info->func=func;
    list->info->linenum=linenum;

	return list;
}

static LOCKINFO *findlock(void *lock, enum cglock_typ typ, const char *file, const char *func, const int linenum)
{
	LOCKLIST *look;

    look=lockhead;
	while(look) {
        if(look->info->lock==lock)
			break;
        look=look->next;
	}

	if(!look)
        look=newlock(lock, typ, file, func, linenum);

	return look->info;
}

static void addgettry(LOCKINFO *info, uint64_t id, const char *file, const char *func, const int linenum, bool get)
{
	LOCKSTAT *stat;
	LOCKLINE *line;

    stat=cgcalloc(1, sizeof(*stat));
    line=cgcalloc(1, sizeof(*line));

	if(get)
		info->gets++;
	else
		info->tries++;

    stat->lock_id=id;
    stat->file=file;
    stat->func=func;
    stat->linenum=linenum;
	cgtime(&stat->tv);

    line->stat=stat;

	if(get) {
        line->next=info->lockgets;
		if(info->lockgets)
            info->lockgets->prev=line;
        info->lockgets=line;
	} else {
        line->next=info->locktries;
		if(info->locktries)
            info->locktries->prev=line;
        info->locktries=line;
	}
}

static void markgotdid(LOCKINFO *info, uint64_t id, const char *file, const char *func, const int linenum, bool got, int ret)
{
	LOCKLINE *line;

	if(got)
		info->gots++;
	else {
        if(ret==0)
			info->dids++;
		else
			info->didnts++;
	}

    if(got || ret==0) {
        info->lastgot.lock_id=id;
        info->lastgot.file=file;
        info->lastgot.func=func;
        info->lastgot.linenum=linenum;
		cgtime(&info->lastgot.tv);
	}

	if(got)
        line=info->lockgets;
	else
        line=info->locktries;
	while(line) {
        if(line->stat->lock_id==id)
			break;
        line=line->next;
	}

	if(!line) {
		lockmsgnow();
		LOCKMSGFFL("ERROR attempt to mark a lock as '%s' that wasn't '%s' id=%"PRIu64,
				got ? "got" : "did/didnt", got ? "get" : "try", id);
	}

	// Unlink it
	if(line->prev)
        line->prev->next=line->next;
	if(line->next)
        line->next->prev=line->prev;

	if(got) {
        if(info->lockgets==line)
            info->lockgets=line->next;
	} else {
        if(info->locktries==line)
            info->locktries=line->next;
	}

	free(line->stat);
	free(line);
}

// Yes this uses locks also ... ;/
static void locklock()
{
	if(pthread_mutex_lock(&lockstat_lock))
		quithere(1, "WTF MUTEX ERROR ON LOCK! errno=%d", errno);
}

static void lockunlock()
{
	if(pthread_mutex_unlock(&lockstat_lock))
		quithere(1, "WTF MUTEX ERROR ON UNLOCK! errno=%d", errno);
}

uint64_t api_getlock(void *lock, const char *file, const char *func, const int linenum)
{
	LOCKINFO *info;
	uint64_t id;

	locklock();

    info=findlock(lock, CGLOCK_UNKNOWN, file, func, linenum);
    id=lock_id++;
	addgettry(info, id, file, func, linenum, true);

	lockunlock();

	return id;
}

void api_gotlock(uint64_t id, void *lock, const char *file, const char *func, const int linenum)
{
	LOCKINFO *info;

	locklock();

    info=findlock(lock, CGLOCK_UNKNOWN, file, func, linenum);
	markgotdid(info, id, file, func, linenum, true, 0);

	lockunlock();
}

uint64_t api_trylock(void *lock, const char *file, const char *func, const int linenum)
{
	LOCKINFO *info;
	uint64_t id;

	locklock();

    info=findlock(lock, CGLOCK_UNKNOWN, file, func, linenum);
    id=lock_id++;
	addgettry(info, id, file, func, linenum, false);

	lockunlock();

	return id;
}

void api_didlock(uint64_t id, int ret, void *lock, const char *file, const char *func, const int linenum)
{
	LOCKINFO *info;

	locklock();

    info=findlock(lock, CGLOCK_UNKNOWN, file, func, linenum);
	markgotdid(info, id, file, func, linenum, false, ret);

	lockunlock();
}

void api_gunlock(void *lock, const char *file, const char *func, const int linenum)
{
	LOCKINFO *info;

	locklock();

    info=findlock(lock, CGLOCK_UNKNOWN, file, func, linenum);
	info->unlocks++;

	lockunlock();
}

void api_initlock(void *lock, enum cglock_typ typ, const char *file, const char *func, const int linenum)
{
	locklock();

	findlock(lock, typ, file, func, linenum);

	lockunlock();
}

void dsp_det(char *msg, LOCKSTAT *stat)
{
	struct tm *tm;
	time_t dt;

    dt=stat->tv.tv_sec;
    tm=localtime(&dt);

	LOCKMSGMORE("%s id=%"PRIu64" by %s %s():%d at %d-%02d-%02d %02d:%02d:%02d",
			msg,
			stat->lock_id,
			stat->file,
			stat->func,
			stat->linenum,
			tm->tm_year + 1900,
			tm->tm_mon + 1,
			tm->tm_mday,
			tm->tm_hour,
			tm->tm_min,
			tm->tm_sec);
}

void dsp_lock(LOCKINFO *info)
{
	LOCKLINE *line;
	char *status;

	LOCKMSG("Lock %p created by %s %s():%d",
		info->lock,
		info->file,
		info->func,
		info->linenum);
	LOCKMSGMORE("gets:%"PRIu64" gots:%"PRIu64" tries:%"PRIu64
		    " dids:%"PRIu64" didnts:%"PRIu64" unlocks:%"PRIu64,
			info->gets,
			info->gots,
			info->tries,
			info->dids,
			info->didnts,
			info->unlocks);

	if(info->gots > 0 || info->dids > 0) {
		if(info->unlocks < info->gots + info->dids)
            status="Last got/did still HELD";
		else
            status="Last got/did (idle)";

		dsp_det(status, &(info->lastgot));
	} else
		LOCKMSGMORE("... unused ...");

	if(info->lockgets) {
		LOCKMSGMORE("BLOCKED gets (%"PRIu64")", info->gets - info->gots);
        line=info->lockgets;
		while(line) {
			dsp_det("", line->stat);
            line=line->next;
		}
	} else
		LOCKMSGMORE("no blocked gets");

	if(info->locktries) {
		LOCKMSGMORE("BLOCKED tries (%"PRIu64")", info->tries - info->dids - info->didnts);
        line=info->lockgets;
		while(line) {
			dsp_det("", line->stat);
            line=line->next;
		}
	} else
		LOCKMSGMORE("no blocked tries");
}

void show_locks()
{
	LOCKLIST *list;

	locklock();

	lockmsgnow();

    list=lockhead;
	if(!list)
		LOCKMSG("no locks?!?\n");
	else {
		while(list) {
			dsp_lock(list->info);
            list=list->next;
		}
	}

	LOCKMSGFLUSH();

	lockunlock();
}
#endif

static void lockstats(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
#if LOCK_TRACKING
	show_locks();
	message(io_data, MSG_LOCKOK, 0, NULL);
#else
	message(io_data, MSG_LOCKDIS, 0, NULL);
#endif
}

static void apiversion(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
    struct api_data *root=NULL;

	message(io_data, MSG_VERSION, 0, NULL);
	io_add(io_data, COMSTR JSON_VERSION);

    root=api_add_string(root, "CGMiner", VERSION, false);
    root=api_add_const(root, "API", APIVERSION, false);

#if defined(FIRMWARE_NAME) && defined(FIRMWARE_VERSION)
	root=api_add_string(root, FIRMWARE_NAME, FIRMWARE_VERSION, false);
#endif

	root=print_data(io_data, root, false);
	io_close(io_data);
}

#if defined(HAVE_AN_ASIC)
static const char *status2str(enum alive status)
{
	switch (status) {
		case LIFE_WELL:
			return ALIVE;
		case LIFE_SICK:
			return SICK;
		case LIFE_DEAD:
			return DEAD;
		case LIFE_NOSTART:
			return NOSTART;
		case LIFE_INIT:
			return INIT;
		default:
			return UNKNOWN;
	}
}
#endif

#ifdef HAVE_AN_ASIC
static void ascstatus(struct io_data *io_data, int asc, bool isjson, bool precom)
{
    struct api_data *root=NULL;
	char *enabled;
	char *status;
    int numasc=numascs();

	if(numasc > 0 && asc >= 0 && asc < numasc) {
        int dev=ascdevice(asc);
		if(dev < 0) // Should never happen
			return;

        struct cgpu_info *cgpu=get_devices(dev);
        float temp=cgpu->temp;
		double dev_runtime;

        dev_runtime=cgpu_runtime(cgpu);

        cgpu->utility=cgpu->accepted / dev_runtime * 60;

		if(cgpu->deven != DEV_DISABLED)
			enabled=(char *)YES_STR;
		else
			enabled=(char *)NO_STR;

        status=(char *)status2str(cgpu->status);

        root=api_add_int(root, "ASC", &asc, false);
        root=api_add_string(root, "Name", cgpu->drv->name, false);
        root=api_add_int(root, "ID", &(cgpu->device_id), false);
        root=api_add_string(root, "Enabled", enabled, false);
        root=api_add_string(root, "Status", status, false);
        root=api_add_temp(root, "Temperature", &temp, false);
        double mhs=cgpu->total_mhashes / dev_runtime;
		root=api_add_int(root, "MHS 1m", &cgpu->rolling1, false);
		root=api_add_int(root, "MHS 5m", &cgpu->rolling5, false);
		root=api_add_int(root, "MHS 15m", &cgpu->rolling15, false);
		root=api_add_int(root, "MHS av", &mhs, false);
        root=api_add_int(root, "Accepted", &(cgpu->accepted), false);
        root=api_add_int(root, "Rejected", &(cgpu->rejected), false);
        root=api_add_int(root, "Hardware Errors", &(cgpu->hw_errors), false);
        root=api_add_utility(root, "Utility", &(cgpu->utility), false);
        int last_share_pool=cgpu->last_share_pool_time > 0 ?
					cgpu->last_share_pool : -1;
        root=api_add_int(root, "Last Share Pool", &last_share_pool, false);
        root=api_add_time(root, "Last Share Time", &(cgpu->last_share_pool_time), false);
        root=api_add_mhtotal(root, "Total MH", &(cgpu->total_mhashes), false);
        root=api_add_int64(root, "Diff1 Work", &(cgpu->diff1), false);
		root=api_add_int64(root, "Difficulty Accepted", &(cgpu->diff_accepted), false);
		root=api_add_int64(root, "Difficulty Rejected", &(cgpu->diff_rejected), false);
		root=api_add_int64(root, "Last Share Difficulty", &(cgpu->last_share_diff), false);
        root=api_add_time(root, "Last Valid Work", &(cgpu->last_device_valid_work), false);
        double rejp=cgpu->diff1 ?
				(double)(cgpu->diff_rejected) / (double)(cgpu->diff1) : 0;
        root=api_add_percent(root, "Device Rejected%", &rejp, false);
        root=api_add_elapsed(root, "Device Elapsed", &(dev_runtime), false);

        root=print_data(io_data, root, isjson, precom);
	}
}
#endif

static void devstatus(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
#if defined(HAVE_AN_ASIC)
    int devcount=0;
	int i;
	int numasc;
    numasc=numascs();
	if(numasc==0)
#endif
	{
		message(io_data, MSG_NODEVS, 0, NULL);
		return;
	}
	message(io_data, MSG_DEVS, 0, NULL);
	io_add(io_data, COMSTR JSON_DEVS);
#ifdef HAVE_AN_ASIC
	if(numasc > 0)
	{
		for(i=0; i<numasc; i++)
		{
			ascstatus(io_data, i, isjson, isjson && devcount > 0);
			devcount++;
		}
	}
#endif
	io_close(io_data);
}

static void edevstatus(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
#if defined(HAVE_AN_ASIC)
    int devcount=0;
	int i;
#endif
#if defined(HAVE_AN_ASIC)
	int numasc;
	numasc=numascs();
#endif
#if defined(HAVE_AN_ASIC)
	if(numasc==0)
#endif
    {
		message(io_data, MSG_NODEVS, 0, NULL);
		return;
	}
	message(io_data, MSG_DEVS, 0, NULL);
	io_add(io_data, COMSTR JSON_DEVS);
#ifdef HAVE_AN_ASIC
    if(numasc > 0)
    {
		for(i=0; i<numasc; i++)
        {
			ascstatus(io_data, i, isjson, isjson && devcount > 0);
			devcount++;
		}
	}
#endif
	io_close(io_data);
}

static void poolstatus(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
    struct api_data *root=NULL;
	int64_t stratum_diff_0=0;
	char *status, *lp;
	int i;

	if(total_pools==0)
	{
		message(io_data, MSG_NOPOOL, 0, NULL);
		return;
	}

	message(io_data, MSG_POOL, 0, NULL);
	io_add(io_data, COMSTR JSON_POOLS);

	for(i=0; i<total_pools; i++)
	{
        struct pool *pool=pools[i];

		if(pool->removed)
			continue;

		switch (pool->enabled)
		{
			case POOL_DISABLED:
                status=(char *)DISABLED;
				break;
			case POOL_REJECTING:
                status=(char *)REJECTING;
				break;
			case POOL_ENABLED:
				if(pool->idle)
                    status=(char *)DEAD;
				else
                    status=(char *)ALIVE;
				break;
			default:
                status=(char *)UNKNOWN;
				break;
		}

		if(pool->hdr_path)
			lp=(char *)YES_STR;
		else
			lp=(char *)NO_STR;

        root=api_add_int(root, "POOL", &i, false);
        root=api_add_escape(root, "URL", pool->rpc_url, false);
        root=api_add_string(root, "Status", status, false);
        root=api_add_int(root, "Priority", &(pool->prio), false);
        root=api_add_int(root, "Quota", &pool->quota, false);
        root=api_add_string(root, "Long Poll", lp, false);
        root=api_add_uint(root, "Getworks", &(pool->getwork_requested), false);
        root=api_add_int64(root, "Accepted", &(pool->accepted), false);
        root=api_add_int64(root, "Rejected", &(pool->rejected), false);
        root=api_add_int(root, "Works", &pool->works, false);
        root=api_add_uint(root, "Discarded", &(pool->discarded_work), false);
        root=api_add_uint(root, "Stale", &(pool->stale_shares), false);
        root=api_add_uint(root, "Get Failures", &(pool->getfail_occasions), false);
        root=api_add_uint(root, "Remote Failures", &(pool->remotefail_occasions), false);
        root=api_add_escape(root, "User", pool->rpc_user, false);
        root=api_add_time(root, "Last Share Time", &(pool->last_share_time), false);
		uint32_t lsLag=pool->cgminer_pool_stats.last_share_result_lag.tv_sec*1000+pool->cgminer_pool_stats.last_share_result_lag.tv_usec/1000;
		root=api_add_uint32(root, "Last Share Lag mSec", &lsLag, true);
		root=api_add_int64(root, "Diff1 Shares", &(pool->diff1), false);
		if(pool->rpc_proxy)
		{
            root=api_add_const(root, "Proxy Type", proxytype(pool->rpc_proxytype), false);
            root=api_add_escape(root, "Proxy", pool->rpc_proxy, false);
		}
		else
		{
            root=api_add_const(root, "Proxy Type", BLANK, false);
            root=api_add_const(root, "Proxy", BLANK, false);
		}
		root=api_add_int64(root, "Difficulty Accepted", &(pool->diff_accepted), false);
		root=api_add_int64(root, "Difficulty Rejected", &(pool->diff_rejected), false);
		root=api_add_int64(root, "Difficulty Stale", &(pool->diff_stale), false);
		root=api_add_int64(root, "Last Share Difficulty", &(pool->last_share_diff), false);
		root=api_add_int64(root, "Work Difficulty", &(pool->cgminer_pool_stats.last_diff), false);
        root=api_add_bool(root, "Has Stratum", &(pool->has_stratum), false);
        root=api_add_bool(root, "Stratum Active", &(pool->stratum_active), false);
		if(pool->stratum_active)
		{
            root=api_add_escape(root, "Stratum URL", pool->stratum_url, false);
			root=api_add_int64(root, "Stratum Difficulty", &(pool->stratum_diff), false);
		}
		else
		{
            root=api_add_const(root, "Stratum URL", BLANK, false);
			root=api_add_int64(root, "Stratum Difficulty", &stratum_diff_0, true);
		}
		root=api_add_int64(root, "Best Share", &(pool->best_diff), true);
		double rejp=(pool->diff_accepted + pool->diff_rejected + pool->diff_stale) ? (double)(pool->diff_rejected) / (double)(pool->diff_accepted + pool->diff_rejected + pool->diff_stale) : 0;
        root=api_add_percent(root, "Pool Rejected%", &rejp, false);
		double stalep=(pool->diff_accepted + pool->diff_rejected + pool->diff_stale) ? (double)(pool->diff_stale) / (double)(pool->diff_accepted + pool->diff_rejected + pool->diff_stale) : 0;
        root=api_add_percent(root, "Pool Stale%", &stalep, false);
        root=api_add_uint64(root, "Bad Work", &(pool->bad_work), true);
        root=api_add_uint32(root, "Current Block Height", &(pool->current_height), true);
		uint32_t nversion=(uint32_t)strtoul(pool->bbversion, NULL, 16);
        root=api_add_uint32(root, "Current Block Version", &nversion, true);
		root=print_data(io_data, root, i);
	}
	io_add(io_data, JSON_ARRAY_END);
	io_close(io_data);
}

static void summary(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
    struct api_data *root=NULL;
	double utility, work_utility;
	uint32_t hr_1m, hr_5m, hr_15m, hr_av;

	hr_1m=g_hr_rolling1m;
	hr_5m=g_hr_rolling5m;
	hr_15m=g_hr_rolling15m;
	hr_av=total_ghashes_done / total_secs;

	message(io_data, MSG_SUMM, 0, NULL);
	io_add(io_data, COMSTR JSON_SUMMARY);

	// stop hashmeter() changing some while copying
	mutex_lock(&hash_lock);

#if defined(USE_ANTMINER_L3)
	total_diff1=total_diff_accepted + total_diff_rejected + total_diff_stale;
#endif

	utility=total_accepted / ( total_secs ? total_secs : 1 ) * 60;
	work_utility=total_diff1 / ( total_secs ? total_secs : 1 ) * 60;
	root=api_add_elapsed(root, "Elapsed", &(total_secs), true);
    //utility=total_accepted / ( total_secs ? total_secs : 1 ) * 60;
	root=api_add_uint32(root, "GHS 1m", &hr_1m, true);
	root=api_add_uint32(root, "GHS 5m", &hr_5m, true);
	root=api_add_uint32(root, "GHS 15m", &hr_15m, true);
	root=api_add_uint32(root, "GHS av", &hr_av, true);
    root=api_add_uint(root, "Found Blocks", &(found_blocks), true);
    root=api_add_int64(root, "Getworks", &(total_getworks), true);
    root=api_add_int64(root, "Accepted", &(total_accepted), true);
    root=api_add_int64(root, "Rejected", &(total_rejected), true);
    root=api_add_int(root, "Hardware Errors", &(hw_errors), true);
    root=api_add_utility(root, "Utility", &(utility), false);
    root=api_add_int64(root, "Discarded", &(total_discarded), true);
    root=api_add_int64(root, "Stale", &(total_stale), true);
    root=api_add_uint(root, "Get Failures", &(total_go), true);
    root=api_add_uint(root, "Local Work", &(local_work), true);
    root=api_add_uint(root, "Remote Failures", &(total_ro), true);
    root=api_add_uint(root, "Network Blocks", &(new_blocks), true);
	root=api_add_mhtotal(root, "Total GH", &(total_ghashes_done), true);
    root=api_add_utility(root, "Work Utility", &(work_utility), false);
	root=api_add_int64(root, "Difficulty Accepted", &(total_diff_accepted), true);
	root=api_add_int64(root, "Difficulty Rejected", &(total_diff_rejected), true);
	root=api_add_int64(root, "Difficulty Stale", &(total_diff_stale), true);
	root=api_add_int64(root, "Best Share", &(best_diff), true);
	double rejp=total_diff1 ? total_diff_rejected / total_diff1 : 0;
    root=api_add_percent(root, "Device Rejected%", &rejp, false);
	double prejp=(total_diff_accepted + total_diff_rejected + total_diff_stale) ? total_diff_rejected / (total_diff_accepted + total_diff_rejected + total_diff_stale) : 0;
    root=api_add_percent(root, "Pool Rejected%", &prejp, false);
	double stalep=(total_diff_accepted + total_diff_rejected + total_diff_stale) ? total_diff_stale / (total_diff_accepted + total_diff_rejected + total_diff_stale) : 0;
    root=api_add_percent(root, "Pool Stale%", &stalep, false);
    root=api_add_time(root, "Last getwork", &last_getwork, false);

	mutex_unlock(&hash_lock);

	root=print_data(io_data, root, false);
	io_close(io_data);
}

static void switchpool(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
	struct pool *pool;
	int id;

	if(total_pools==0)
	{
		message(io_data, MSG_NOPOOL, 0, NULL);
		return;
	}

    if(param==NULL || *param=='\0') {
		message(io_data, MSG_MISPID, 0, NULL);
		return;
	}

    id=atoi(param);
	cg_rlock(&control_lock);
	if(id < 0 || id >= total_pools)
	{
		cg_runlock(&control_lock);
		message(io_data, MSG_INVPID, id, NULL);
		return;
	}

    pool=pools[id];
    pool->enabled=POOL_ENABLED;
	cg_runlock(&control_lock);
	switch_pools(pool);

	message(io_data, MSG_SWITCHP, id, NULL);
}

static void copyadvanceafter(char ch, char **param, char **buf)
{
#define src_p (*param)
#define dst_b (*buf)

	while(*src_p && *src_p != ch) {
        if(*src_p=='\\' && *(src_p+1) != '\0')
			src_p++;

        *(dst_b++)=*(src_p++);
	}
	if(*src_p)
		src_p++;

    *(dst_b++)='\0';
}

static bool pooldetails(char *param, char **url, char **user, char **pass)
{
	char *ptr, *buf;

    ptr=buf=cgmalloc(strlen(param)+1);

    *url=buf;

	// copy url
	copyadvanceafter(',', &param, &buf);

	if(!(*param)) // missing user
		goto exitsama;

    *user=buf;

	// copy user
	copyadvanceafter(',', &param, &buf);

	if(!*param) // missing pass
		goto exitsama;

    *pass=buf;

	// copy pass
	copyadvanceafter(',', &param, &buf);

	return true;

exitsama:
	free(ptr);
	return false;
}

static void addpool(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
	char *url, *user, *pass;
	struct pool *pool;
	char *ptr;

    if(param==NULL || *param=='\0') {
		message(io_data, MSG_MISPDP, 0, NULL);
		return;
	}

	if(!pooldetails(param, &url, &user, &pass)) {
		ptr=escape_string(param, true);
		message(io_data, MSG_INVPDP, 0, ptr);
		if(ptr != param)
			free(ptr);
        ptr=NULL;
		return;
	}

    pool=add_pool();
	detect_stratum(pool, url);
	add_pool_details(pool, true, url, user, pass);

	ptr=escape_string(url, true);
	message(io_data, MSG_ADDPOOL, pool->pool_no, ptr);
	if(ptr != url)
		free(ptr);
    ptr=NULL;
}

static void enablepool(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
	struct pool *pool;
	int id;

    if(total_pools==0) {
		message(io_data, MSG_NOPOOL, 0, NULL);
		return;
	}

    if(param==NULL || *param=='\0') {
		message(io_data, MSG_MISPID, 0, NULL);
		return;
	}

    id=atoi(param);
	if(id < 0 || id >= total_pools) {
		message(io_data, MSG_INVPID, id, NULL);
		return;
	}

    pool=pools[id];
    if(pool->enabled==POOL_ENABLED) {
		message(io_data, MSG_ALRENAP, id, NULL);
		return;
	}

    pool->enabled=POOL_ENABLED;
	if(pool->prio < current_pool()->prio)
		switch_pools(pool);

	message(io_data, MSG_ENAPOOL, id, NULL);
}

static void poolpriority(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
	char *ptr, *next;
    int i, pr, prio=0;

	// TODO: all cgminer code needs a mutex added everywhere for change
	//	access to total_pools and also parts of the pools[] array,
	//	just copying total_pools here wont solve that

	if(total_pools==0)
	{
		message(io_data, MSG_NOPOOL, 0, NULL);
		return;
	}

	if(param==NULL || *param=='\0')
	{
		message(io_data, MSG_MISPID, 0, NULL);
		return;
	}

	bool pools_changed[total_pools];
	int new_prio[total_pools];
	for(i=0; i<total_pools; ++i)
        pools_changed[i]=false;

    next=param;
	while(next && *next) {
        ptr=next;
        next=strchr(ptr, ',');
		if(next)
            *(next++)='\0';

        i=atoi(ptr);
		if(i<0 || i >= total_pools) {
			message(io_data, MSG_INVPID, i, NULL);
			return;
		}

		if(pools_changed[i]) {
			message(io_data, MSG_DUPPID, i, NULL);
			return;
		}

        pools_changed[i]=true;
        new_prio[i]=prio++;
	}

	// Only change them if no errors
	for(i=0; i<total_pools; i++) {
		if(pools_changed[i])
            pools[i]->prio=new_prio[i];
	}

	// In priority order, cycle through the unchanged pools and append them
    for(pr=0; pr < total_pools; pr++)
	{
		for(i=0; i<total_pools; i++)
		{
			if(!pools_changed[i] && pools[i]->prio==pr)
			{
                pools[i]->prio=prio++;
                pools_changed[i]=true;
				break;
			}
		}
	}
	if(current_pool()->prio)
		switch_pools(NULL);

	message(io_data, MSG_POOLPRIO, 0, NULL);
}

static void poolquota(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
	struct pool *pool;
	int quota, id;
	char *comma;

    if(total_pools==0) {
		message(io_data, MSG_NOPOOL, 0, NULL);
		return;
	}

    if(param==NULL || *param=='\0') {
		message(io_data, MSG_MISPID, 0, NULL);
		return;
	}

    comma=strchr(param, ',');
	if(!comma) {
		message(io_data, MSG_CONVAL, 0, param);
		return;
	}

    *(comma++)='\0';

    id=atoi(param);
	if(id < 0 || id >= total_pools) {
		message(io_data, MSG_INVPID, id, NULL);
		return;
	}
    pool=pools[id];

    quota=atoi(comma);
	if(quota < 0) {
		message(io_data, MSG_INVNEG, quota, pool->rpc_url);
		return;
	}

    pool->quota=quota;
	adjust_quota_gcd();
	message(io_data, MSG_SETQUOTA, quota, pool->rpc_url);
}

static void disablepool(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
	struct pool *pool;
	int id;

    if(total_pools==0) {
		message(io_data, MSG_NOPOOL, 0, NULL);
		return;
	}

    if(param==NULL || *param=='\0') {
		message(io_data, MSG_MISPID, 0, NULL);
		return;
	}

    id=atoi(param);
	if(id < 0 || id >= total_pools) {
		message(io_data, MSG_INVPID, id, NULL);
		return;
	}

    pool=pools[id];
    if(pool->enabled==POOL_DISABLED) {
		message(io_data, MSG_ALRDISP, id, NULL);
		return;
	}

	if(enabled_pools <= 1) {
		message(io_data, MSG_DISLASTP, id, NULL);
		return;
	}

    pool->enabled=POOL_DISABLED;
    if(pool==current_pool())
		switch_pools(NULL);

	message(io_data, MSG_DISPOOL, id, NULL);
}

static void removepool(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
	struct pool *pool;
	char *rpc_url;
    bool dofree=false;
	int id;

	if(total_pools==0)
	{
		message(io_data, MSG_NOPOOL, 0, NULL);
		return;
	}

    if(param==NULL || *param=='\0') {
		message(io_data, MSG_MISPID, 0, NULL);
		return;
	}

    id=atoi(param);
	if(id < 0 || id >= total_pools)
	{
		message(io_data, MSG_INVPID, id, NULL);
		return;
	}

	if(total_pools <= 1)
	{
		message(io_data, MSG_REMLASTP, id, NULL);
		return;
	}

    pool=pools[id];
    if(pool==current_pool())
	{
		switch_pools(NULL);
	}

	if(pool==current_pool())
	{
		message(io_data, MSG_ACTPOOL, id, NULL);
		return;
	}

    pool->enabled=POOL_DISABLED;
	rpc_url=escape_string(pool->rpc_url, true);
	if(rpc_url != pool->rpc_url)
	{
		dofree=true;
	}

	remove_pool(pool);

	message(io_data, MSG_REMPOOL, id, rpc_url);

	if(dofree)
		free(rpc_url);
    rpc_url=NULL;
}

void doquit(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
	io_put(io_data, JSON_BYE);
    bye=true;
    do_a_quit=true;
}

void dorestart(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
	io_put(io_data, JSON_RESTART);
    bye=true;
    do_a_restart=true;
}

void privileged(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
	message(io_data, MSG_ACCOK, 0, NULL);
}

void notifystatus(struct io_data *io_data, int device, struct cgpu_info *cgpu, bool isjson, char group)
{
    struct api_data *root=NULL;
	char *reason;

    if(cgpu->device_last_not_well==0)
        reason=REASON_NONE;
	else
		switch(cgpu->device_not_well_reason)
		{
			case REASON_THREAD_FAIL_INIT:
                reason=REASON_THREAD_FAIL_INIT_STR;
				break;
			case REASON_THREAD_ZERO_HASH:
                reason=REASON_THREAD_ZERO_HASH_STR;
				break;
			case REASON_THREAD_FAIL_QUEUE:
                reason=REASON_THREAD_FAIL_QUEUE_STR;
				break;
			case REASON_DEV_SICK_IDLE_60:
                reason=REASON_DEV_SICK_IDLE_60_STR;
				break;
			case REASON_DEV_DEAD_IDLE_600:
                reason=REASON_DEV_DEAD_IDLE_600_STR;
				break;
			case REASON_DEV_NOSTART:
                reason=REASON_DEV_NOSTART_STR;
				break;
			case REASON_DEV_OVER_HEAT:
                reason=REASON_DEV_OVER_HEAT_STR;
				break;
			case REASON_DEV_THERMAL_CUTOFF:
                reason=REASON_DEV_THERMAL_CUTOFF_STR;
				break;
			case REASON_DEV_COMMS_ERROR:
                reason=REASON_DEV_COMMS_ERROR_STR;
				break;
			default:
                reason=REASON_UNKNOWN_STR;
				break;
		}

	// ALL counters (and only counters) must start the name with a '*'
	// Simplifies future external support for identifying new counters
    root=api_add_int(root, "NOTIFY", &device, false);
    root=api_add_string(root, "Name", cgpu->drv->name, false);
    root=api_add_int(root, "ID", &(cgpu->device_id), false);
    root=api_add_time(root, "Last Well", &(cgpu->device_last_well), false);
    root=api_add_time(root, "Last Not Well", &(cgpu->device_last_not_well), false);
    root=api_add_string(root, "Reason Not Well", reason, false);
    root=api_add_int(root, "*Thread Fail Init", &(cgpu->thread_fail_init_count), false);
    root=api_add_int(root, "*Thread Zero Hash", &(cgpu->thread_zero_hash_count), false);
    root=api_add_int(root, "*Thread Fail Queue", &(cgpu->thread_fail_queue_count), false);
    root=api_add_int(root, "*Dev Sick Idle 60s", &(cgpu->dev_sick_idle_60_count), false);
    root=api_add_int(root, "*Dev Dead Idle 600s", &(cgpu->dev_dead_idle_600_count), false);
    root=api_add_int(root, "*Dev Nostart", &(cgpu->dev_nostart_count), false);
    root=api_add_int(root, "*Dev Over Heat", &(cgpu->dev_over_heat_count), false);
    root=api_add_int(root, "*Dev Thermal Cutoff", &(cgpu->dev_thermal_cutoff_count), false);
    root=api_add_int(root, "*Dev Comms Error", &(cgpu->dev_comms_error_count), false);
    root=api_add_int(root, "*Dev Throttle", &(cgpu->dev_throttle_count), false);
	print_data(io_data, root, isjson && (device > 0));
}

static void notify(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
	struct cgpu_info *cgpu;
	int i;

	if(total_devices==0)
	{
		message(io_data, MSG_NODEVS, 0, NULL);
		return;
	}

	message(io_data, MSG_NOTIFY, 0, NULL);
	io_add(io_data, COMSTR JSON_NOTIFY);

	for(i=0; i<total_devices; i++)
	{
        cgpu=get_devices(i);
		notifystatus(io_data, i, cgpu, isjson, group);
	}

	io_close(io_data);
}

static void devdetails(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
    struct api_data *root=NULL;
	struct cgpu_info *cgpu;
	int i;

	if(total_devices==0)
	{
		message(io_data, MSG_NODEVS, 0, NULL);
		return;
	}

	message(io_data, MSG_DEVDETAILS, 0, NULL);
	io_add(io_data, COMSTR JSON_DEVDETAILS);

	for(i=0; i<total_devices; i++)
	{
        cgpu=get_devices(i);
        root=api_add_int(root, "DEVDETAILS", &i, false);
        root=api_add_string(root, "Name", cgpu->drv->name, false);
        root=api_add_int(root, "ID", &(cgpu->device_id), false);
        root=api_add_string(root, "Driver", cgpu->drv->dname, false);
		root=api_add_const(root, "Kernel", cgpu->kname ? cgpu->kname : BLANK, false);
		root=api_add_const(root, "Model", cgpu->name ? cgpu->name : BLANK, false);
		root=api_add_const(root, "Device Path", cgpu->device_path ? cgpu->device_path : BLANK, false);
		root=print_data(io_data, root, i>0);
	}

	io_close(io_data);
}

static int itemstats(struct io_data *io_data, int i, char *id, struct api_data *extra, bool isjson)
{
	struct api_data *root=NULL;
	uint32_t hr_1m, hr_5m, hr_15m, hr_av;

	hr_1m=g_hr_rolling1m;
	hr_5m=g_hr_rolling5m;
	hr_15m=g_hr_rolling15m;
	hr_av=total_ghashes_done / total_secs;

#if defined(USE_ANTMINER_L3)
	hr_av=total_mhashes_done / total_secs;
#else
	hr_av=total_ghashes_done / total_secs;
#endif

	root=api_add_int(root, "STATS", &i, true);
	root=api_add_string(root, "ID", id, false);
	root=api_add_elapsed(root, "Elapsed", &total_secs, false);

	root=api_add_uint32(root, "GHS 1m", &hr_1m, true);
	root=api_add_uint32(root, "GHS 5m", &hr_5m, true);
	root=api_add_uint32(root, "GHS 15m", &hr_15m, true);
	root=api_add_uint32(root, "GHS av", &hr_av, true);

	if(extra)
    {
        root=api_add_extra(root, extra);
    }

	print_data(io_data, root, isjson && (i>0));
	return ++i;
}

static void minerstats(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
	struct cgpu_info *cgpu;
	struct api_data *extra;
	char id[20];
	int i, j;

	message(io_data, MSG_MINESTATS, 0, NULL);
	io_add(io_data, COMSTR JSON_MINESTATS);

	for(i=0, j=0; j < total_devices; j++)
	{
        cgpu=get_devices(j);
		if(cgpu && cgpu->drv)
		{
			extra=cgpu->drv->get_api_stats(cgpu);
			sprintf(id, "%s%d", cgpu->drv->name, cgpu->device_id);
			i=itemstats(io_data, i, id, extra, true);
		}
	}

	io_close(io_data);
}

static void minerestats(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
	struct cgpu_info *cgpu;
	struct api_data *extra;
	char id[20];
	int i, j;
	message(io_data, MSG_MINESTATS, 0, NULL);
	io_add(io_data, COMSTR JSON_MINESTATS);
    i=0;
    for(j=0; j < total_devices; j++)
    {
        cgpu=get_devices(j);
		if(!cgpu)
        {
            continue;
        }
        if(cgpu->drv)
        {
			extra=cgpu->drv->get_api_stats(cgpu);
			sprintf(id, "%s%d", cgpu->drv->name, cgpu->device_id);
			i=itemstats(io_data, i, id, extra, true);
		}
	}
	io_close(io_data);
}

static void failoveronly(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
	message(io_data, MSG_DEPRECATED, 0, param);
}

static void minecoin(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
    struct api_data *root=NULL;

	message(io_data, MSG_MINECOIN, 0, NULL);
	io_add(io_data, COMSTR JSON_MINECOIN);

	root=api_add_const(root, "Hash Method", SHA256_STR, false);

	cg_rlock(&ch_lock);
    root=api_add_timeval(root, "Current Block Time", &block_timeval, true);
    root=api_add_string(root, "Current Block Hash", current_hash, true);
	cg_runlock(&ch_lock);

    root=api_add_bool(root, "LP", &have_longpoll, false);
	root=api_add_int64(root, "Network Difficulty", &current_diff, true);

	root=print_data(io_data, root, false);
	io_close(io_data);
}

static void getconfig(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
	char *cfg_jsonbuf;
	cfg_jsonbuf=json_dumps(config_json_data, JSON_COMPACT);
	message(io_data, MSG_CONFIG, 0, NULL);
	io_add(io_data, COMSTR JSON_CONFIG);
	io_add(io_data, cfg_jsonbuf);
	free(cfg_jsonbuf);
	io_close(io_data);
}

static void setconfig(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
	json_error_t err;
	json_t *new_config_json_data;

	new_config_json_data=json_loadb(param, strlen(param), 0, &err);
	if(!json_is_object(new_config_json_data))
	{
		message(io_data, MSG_INVJSON, 0, NULL);
		return;
	}
	message(io_data, MSG_CONFIG, 0, NULL);

	if(json_is_object(config_json_data))
	{
		json_decref(config_json_data);
	}

	config_json_data=new_config_json_data;

	/* Parse the config now, so we can override it. That can keep pointers
	 * so don't free config object. */
	parse_config(config_json_data, true);
}

static void usbstats(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
	message(io_data, MSG_NOUSTA, 0, NULL);
}

static void dozero(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
    if(param==NULL || *param=='\0') {
		message(io_data, MSG_ZERMIS, 0, NULL);
		return;
	}

    char *sum=strchr(param, ',');
	if(sum)
        *(sum++)='\0';
	if(!sum || !*sum) {
		message(io_data, MSG_MISBOOL, 0, NULL);
		return;
	}

    bool all=false;
    bool bs=false;
    if(strcasecmp(param, "all")==0)
	{
		all=true;
	}
    else if(strcasecmp(param, "bestshare")==0)
	{
		bs=true;
	}

	if(all==false && bs==false)
	{
		message(io_data, MSG_ZERINV, 0, param);
		return;
	}

    *sum=tolower(*sum);
	if(*sum != 't' && *sum != 'f')
	{
		message(io_data, MSG_INVBOOL, 0, NULL);
		return;
	}

    bool dosum=(*sum=='t');
	if(dosum)
		print_summary();

	if(all)
		zero_stats();
	if(bs)
		zero_bestshare();

	if(dosum)
		message(io_data, MSG_ZERSUM, 0, all ? "All" : "BestShare");
	else
		message(io_data, MSG_ZERNOSUM, 0, all ? "All" : "BestShare");
}

#ifdef HAVE_AN_ASIC
static void ascdev(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
    int numasc=numascs();
	int id;

    if(numasc==0) {
		message(io_data, MSG_ASCNON, 0, NULL);
		return;
	}

    if(param==NULL || *param=='\0') {
		message(io_data, MSG_MISID, 0, NULL);
		return;
	}

    id=atoi(param);
	if(id < 0 || id >= numasc) {
		message(io_data, MSG_INVASC, id, NULL);
		return;
	}

	message(io_data, MSG_ASCDEV, id, NULL);
	io_add(io_data, COMSTR JSON_ASC);
	ascstatus(io_data, id, true, false);
	io_close(io_data);
}

static void ascenable(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
	struct cgpu_info *cgpu;
    int numasc=numascs();
	struct thr_info *thr;
	int asc;
	int id;
	int i;

    if(numasc==0) {
		message(io_data, MSG_ASCNON, 0, NULL);
		return;
	}

    if(param==NULL || *param=='\0') {
		message(io_data, MSG_MISID, 0, NULL);
		return;
	}

    id=atoi(param);
	if(id < 0 || id >= numasc) {
		message(io_data, MSG_INVASC, id, NULL);
		return;
	}

    int dev=ascdevice(id);
	if(dev < 0) { // Should never happen
		message(io_data, MSG_INVASC, id, NULL);
		return;
	}

    cgpu=get_devices(dev);

	applog(LOG_DEBUG, "API: request to ascenable ascid %d device %d %s%u",
			id, dev, cgpu->drv->name, cgpu->device_id);

	if(cgpu->deven != DEV_DISABLED) {
		message(io_data, MSG_ASCLRENA, id, NULL);
		return;
	}

#if 0 /* A DISABLED device wont change status FIXME: should disabling make it WELL? */
	if(cgpu->status != LIFE_WELL) {
		message(io_data, MSG_ASCUNW, id, NULL);
		return;
	}
#endif

	for(i=0; i<mining_threads; i++)
    {
        thr=get_thread(i);
        asc=thr->cgpu->cgminer_id;
        if(asc==dev) {
            cgpu->deven=DEV_ENABLED;
			applog(LOG_DEBUG, "API: Pushing sem post to thread %d", thr->id);
			cgsem_post(&thr->sem);
		}
	}

	message(io_data, MSG_ASCENA, id, NULL);
}

static void ascdisable(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
	struct cgpu_info *cgpu;
    int numasc=numascs();
	int id;

    if(numasc==0) {
		message(io_data, MSG_ASCNON, 0, NULL);
		return;
	}

    if(param==NULL || *param=='\0') {
		message(io_data, MSG_MISID, 0, NULL);
		return;
	}

    id=atoi(param);
	if(id < 0 || id >= numasc) {
		message(io_data, MSG_INVASC, id, NULL);
		return;
	}

    int dev=ascdevice(id);
	if(dev < 0) { // Should never happen
		message(io_data, MSG_INVASC, id, NULL);
		return;
	}

    cgpu=get_devices(dev);

	applog(LOG_DEBUG, "API: request to ascdisable ascid %d device %d %s%u",
			id, dev, cgpu->drv->name, cgpu->device_id);

    if(cgpu->deven==DEV_DISABLED) {
		message(io_data, MSG_ASCLRDIS, id, NULL);
		return;
	}

    cgpu->deven=DEV_DISABLED;

	message(io_data, MSG_ASCDIS, id, NULL);
}

static void ascidentify(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
	struct cgpu_info *cgpu;
	struct device_drv *drv;
    int numasc=numascs();
	int id;

    if(numasc==0) {
		message(io_data, MSG_ASCNON, 0, NULL);
		return;
	}

    if(param==NULL || *param=='\0') {
		message(io_data, MSG_MISID, 0, NULL);
		return;
	}

    id=atoi(param);
	if(id < 0 || id >= numasc) {
		message(io_data, MSG_INVASC, id, NULL);
		return;
	}

    int dev=ascdevice(id);
	if(dev < 0) { // Should never happen
		message(io_data, MSG_INVASC, id, NULL);
		return;
	}

    cgpu=get_devices(dev);
    drv=cgpu->drv;

	if(!drv->identify_device)
		message(io_data, MSG_ASCNOID, id, NULL);
	else {
		drv->identify_device(cgpu);
		message(io_data, MSG_ASCIDENT, id, NULL);
	}
}
#endif

#if defined(USE_S21_DRIVER)
static void setpsuvoltage(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
	struct api_data *root=NULL;
	opt_psu_voltage=atof(param);
	if(opt_psu_voltage<APW17_MIN_VOLTAGE)
	{
		opt_psu_voltage=APW17_MIN_VOLTAGE;
	}
	else if(opt_psu_voltage>APW17_MAX_VOLTAGE)
	{
		opt_psu_voltage=APW17_MAX_VOLTAGE;
	}
	message(io_data, MSG_VOLTAGE, 0, NULL);
	io_add(io_data, COMSTR JSON_CONFIG);
	root=api_add_float(root, "psu-voltage", &opt_psu_voltage, false);
	root=print_data(io_data, root, false);
	io_close(io_data);
}

static void setautotunermode(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
	struct api_data *root=NULL;
	opt_autotuner_mode=atoi(param);
	message(io_data, MSG_TUNER_MODE, 0, NULL);
	io_add(io_data, COMSTR JSON_CONFIG);
	root=api_add_int(root, "autotuner-mode", &opt_autotuner_mode, false);
	root=print_data(io_data, root, false);
	io_close(io_data);
}

static void setfrequency(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
	struct api_data *root=NULL;
	opt_frequency=atoi(param);
	if(opt_frequency<S21_FREQUENCY_MIN)
	{
		opt_frequency=S21_FREQUENCY_MIN;
	}
	else if(opt_frequency>S21_FREQUENCY_MAX)
	{
		opt_frequency=S21_FREQUENCY_MAX;
	}
	message(io_data, MSG_FREQUENCY, 0, NULL);
	io_add(io_data, COMSTR JSON_CONFIG);
	root=api_add_int(root, "frequency", &opt_frequency, false);
	root=print_data(io_data, root, false);
	io_close(io_data);
}

static void settargettemperature(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
	struct api_data *root=NULL;
	opt_target_temperature=atof(param);
	if(opt_target_temperature<S21_WORKING_TEMPERATURE_MIN)
	{
		opt_target_temperature=S21_WORKING_TEMPERATURE_MIN;
	}
	else if(opt_target_temperature>S21_WORKING_TEMPERATURE_MAX)
	{
		opt_target_temperature=S21_WORKING_TEMPERATURE_MAX;
	}
	message(io_data, MSG_TEMPERATURE, 0, NULL);
	io_add(io_data, COMSTR JSON_CONFIG);
	root=api_add_float(root, "target-temperature", &opt_target_temperature, false);
	root=print_data(io_data, root, false);
	io_close(io_data);
}

static void settargethashrate(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
	struct api_data *root=NULL;
	opt_target_hashrate=atoi(param);
	if(opt_target_hashrate<S21_TARGET_HASHRATE_MIN)
	{
		opt_target_hashrate=S21_TARGET_HASHRATE_MIN;
	}
	else if(opt_target_hashrate>S21_TARGET_HASHRATE_MAX)
	{
		opt_target_hashrate=S21_TARGET_HASHRATE_MAX;
	}
	message(io_data, MSG_HASHRATE, 0, NULL);
	io_add(io_data, COMSTR JSON_CONFIG);
	root=api_add_int(root, "target-hashrate", &opt_target_hashrate, false);
	root=print_data(io_data, root, false);
	io_close(io_data);
}

static void settargetpowerconsumption(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
	struct api_data *root=NULL;
	opt_target_power_consumption=atoi(param);
	if(opt_target_power_consumption<S21_TARGET_POWER_CONSUMPTION_MIN)
	{
		opt_target_power_consumption=S21_TARGET_POWER_CONSUMPTION_MIN;
	}
	else if(opt_target_power_consumption>S21_TARGET_POWER_CONSUMPTION_MAX)
	{
		opt_target_power_consumption=S21_TARGET_POWER_CONSUMPTION_MAX;
	}
	message(io_data, MSG_CONSUMPTION, 0, NULL);
	io_add(io_data, COMSTR JSON_CONFIG);
	root=api_add_int(root, "target-power-consumption", &opt_target_power_consumption, false);
	root=print_data(io_data, root, false);
	io_close(io_data);
}

static void setfanspeedpercentage(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
	struct api_data *root=NULL;
	opt_fan_speed_percentage=atoi(param);
	if(opt_fan_speed_percentage<S21_FAN_SPEED_PERCENTAGE_MIN)
	{
		opt_fan_speed_percentage=S21_FAN_SPEED_PERCENTAGE_MIN;
	}
	else if(opt_fan_speed_percentage>S21_FAN_SPEED_PERCENTAGE_MAX)
	{
		opt_fan_speed_percentage=S21_FAN_SPEED_PERCENTAGE_MAX;
	}
	message(io_data, MSG_FANSPEED, 0, NULL);
	io_add(io_data, COMSTR JSON_CONFIG);
	root=api_add_int(root, "fan-speed-percentage", &opt_fan_speed_percentage, false);
	root=print_data(io_data, root, false);
	io_close(io_data);
}

#endif // USE_S21_DRIVER

static void asccount(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
    struct api_data *root=NULL;
    int count=0;

#ifdef HAVE_AN_ASIC
    count=numascs();
#endif

	message(io_data, MSG_NUMASC, 0, NULL);
	io_add(io_data, COMSTR JSON_ASCS);
	root=api_add_int(root, "Count", &count, true);
	root=print_data(io_data, root, false);
	io_close(io_data);
}

#ifdef HAVE_AN_ASIC
static void ascset(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
	struct cgpu_info *cgpu;
	struct device_drv *drv;
	char buf[TMPBUFSIZ];
    int numasc=numascs();

    if(numasc==0) {
		message(io_data, MSG_ASCNON, 0, NULL);
		return;
	}

    if(param==NULL || *param=='\0') {
		message(io_data, MSG_MISID, 0, NULL);
		return;
	}

    char *opt=strchr(param, ',');
	if(opt)
        *(opt++)='\0';
	if(!opt || !*opt) {
		message(io_data, MSG_MISASCOPT, 0, NULL);
		return;
	}

    int id=atoi(param);
	if(id < 0 || id >= numasc) {
		message(io_data, MSG_INVASC, id, NULL);
		return;
	}

    int dev=ascdevice(id);
	if(dev < 0) { // Should never happen
		message(io_data, MSG_INVASC, id, NULL);
		return;
	}

    cgpu=get_devices(dev);
    drv=cgpu->drv;

    char *set=strchr(opt, ',');
	if(set)
        *(set++)='\0';

	if(!drv->set_device)
		message(io_data, MSG_ASCNOSET, id, NULL);
	else {
        char *ret=drv->set_device(cgpu, opt, set, buf);
		if(ret) {
            if(strcasecmp(opt, "help")==0)
				message(io_data, MSG_ASCHELP, id, ret, true);
			else
				message(io_data, MSG_ASCSETERR, id, ret, true);
		} else
			message(io_data, MSG_ASCSETOK, id, NULL);
	}
}
#endif

static void checkcommand(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group);

struct CMDS
{
	char *name;
	void (*func)(struct io_data *, SOCKETTYPE, char *, bool, char);
	bool iswritemode;
} cmds[]=
{
	{ "version",		apiversion,	false},
	{ "getconfig",		getconfig,	false},
	{ "setconfig",		setconfig,	true},
	{ "devs",		devstatus,	false},
	{ "edevs",		edevstatus,	false},
	{ "pools",		poolstatus,	false},
	{ "summary",		summary,	false},
	{ "switchpool",		switchpool,	true},
	{ "addpool",		addpool,	true},
	{ "poolpriority",	poolpriority,	true},
	{ "poolquota",		poolquota,	true},
	{ "enablepool",		enablepool,	true},
	{ "disablepool",	disablepool,	true},
	{ "removepool",		removepool,	true},
	{ "quit",		doquit,		true},
	{ "privileged",		privileged,	true},
	{ "notify",		notify,		false},
	{ "devdetails",		devdetails,	false},
	{ "restart",		dorestart,	true},
	{ "stats",		minerstats,	false},
	{ "estats",		minerestats,	false},
	{ "check",		checkcommand,	false},
	{ "failover-only",	failoveronly,	true},
	{ "coin",		minecoin,	false},
	{ "usbstats",		usbstats,	false},
	{ "zero",		dozero,		true},
#ifdef HAVE_AN_ASIC
	{ "asc",		ascdev,		false},
	{ "ascenable",		ascenable,	true},
	{ "ascdisable",		ascdisable,	true},
	{ "ascidentify",	ascidentify,	true},
	{ "ascset",		ascset,		true},
#endif
#if defined(USE_S21_DRIVER)
	{ "setpsuvoltage",				setpsuvoltage, false},
	{ "setautotunermode",			setautotunermode, false},
	{ "setfrequency",				setfrequency, false},
	{ "settargettemperature",		settargettemperature, false},
	{ "settargethashrate",			settargethashrate, false},
	{ "settargetpowerconsumption",	settargetpowerconsumption, false},
	{ "setfanspeedpercentage",		setfanspeedpercentage, false},
#endif // USE_S21_DRIVER
	{ "asccount",		asccount,	false},
	{ "lockstats",		lockstats,	true},
	{ NULL,			NULL,		false}
};

static void checkcommand(struct io_data *io_data, SOCKETTYPE c, char *param, bool isjson, char group)
{
	struct api_data *root=NULL;
	char cmdbuf[256];
	bool found, access;
	int i;

	if(param==NULL || *param=='\0') {
		message(io_data, MSG_MISCHK, 0, NULL);
		return;
	}

	found=false;
	access=false;
	for(i=0; cmds[i].name != NULL; i++)
	{
		if(strcmp(cmds[i].name, param)==0)
		{
			found=true;
			sprintf(cmdbuf, "|%s|", param);
			if(ISPRIVGROUP(group) || strstr(COMMANDS(group), cmdbuf))
			{
				access=true;
			}
			break;
		}
	}

	message(io_data, MSG_CHECK, 0, NULL);
	io_add(io_data, COMSTR JSON_CHECK);
	root=api_add_const(root, "Exists", found ? YES_STR : NO_STR, false);
	root=api_add_const(root, "Access", access ? YES_STR : NO_STR, false);
	root=print_data(io_data, root, false);
	io_close(io_data);
}

static void head_join(struct io_data *io_data, char *cmdptr, bool isjson, bool *firstjoin)
{
	char *ptr;

	if(*firstjoin)
	{
		io_add(io_data, JSON0);
        *firstjoin=false;
	}
	else
	{
		io_add(io_data, JSON_BETWEEN_JOIN);
	}

	// External supplied string
	ptr=escape_string(cmdptr, true);

	io_add(io_data, JSON1);
	io_add(io_data, ptr);
	io_add(io_data, JSON4 JSON_ARRAY_BEGIN);

	if(ptr != cmdptr)
	{
		free(ptr);
	}
}

static void tail_join(struct io_data *io_data, bool isjson)
{
	io_data->close=false;
	io_add(io_data, JSON_END);
}

static void send_result(struct io_data *io_data, SOCKETTYPE c, bool isjson)
{
	int count, sendc, res, tosend, len, n;
    char *buf=io_data->ptr;

    len=strlen(buf);
    tosend=len+1;

	io_add(io_data, JSON_END);

	applog(LOG_DEBUG, "API: send reply: (%d) '%.10s%s'", tosend, buf, len > 10 ? "..." : BLANK);

    count=sendc=0;
	while(count < 5 && tosend > 0)
	{
		// allow 50ms per attempt
        struct timeval timeout={0, 50000};
		fd_set wd;

		FD_ZERO(&wd);
		FD_SET(c, &wd);
		if((res=select(c + 1, NULL, &wd, NULL, &timeout)) < 1)
		{
			applog(LOG_WARNING, "API: send select failed (%d)", res);
			return;
		}

        n=send(c, buf, tosend, 0);
		sendc++;

		if(SOCKETFAIL(n)) {
			count++;
			if(sock_blocks())
				continue;

			applog(LOG_WARNING, "API: send (%d:%d) failed: %s", len+1, (len+1 - tosend), SOCKERRMSG);

			return;
		} else {
			if(sendc <= 1) {
                if(n==tosend)
					applog(LOG_DEBUG, "API: sent all of %d first go", tosend);
				else
					applog(LOG_DEBUG, "API: sent %d of %d first go", n, tosend);
			} else {
                if(n==tosend)
					applog(LOG_DEBUG, "API: sent all of remaining %d (sendc=%d)", tosend, sendc);
				else
					applog(LOG_DEBUG, "API: sent %d of remaining %d (sendc=%d)", n, tosend, sendc);
			}

			tosend -= n;
			buf += n;

            if(n==0)
				count++;
		}
	}
}

static void tidyup(void *arg)
{
	mutex_lock(&quit_restart_lock);

    SOCKETTYPE *apisock=(SOCKETTYPE *)arg;

    bye=true;

	if(*apisock != INVSOCK) {
		shutdown(*apisock, SHUT_RDWR);
		CLOSESOCKET(*apisock);
        *apisock=INVSOCK;
	}

	if(ipaccess != NULL) {
		free(ipaccess);
        ipaccess=NULL;
	}

	io_free();

	mutex_unlock(&quit_restart_lock);
}

/*
 * Interpret --api-groups G:cmd1:cmd2:cmd3,P:cmd4,*,...
 */
static void setup_groups()
{
    char *api_groups=opt_api_groups ? opt_api_groups : (char *)BLANK;
	char *buf, *ptr, *next, *colon;
	char group;
	char commands[TMPBUFSIZ];
	char cmdbuf[100];
	char *cmd;
	bool addstar, did;
	int i;

    buf=cgmalloc(strlen(api_groups) + 1);

	strcpy(buf, api_groups);

    next=buf;
	// for each group defined
	while(next && *next) {
        ptr=next;
        next=strchr(ptr, ',');
		if(next)
            *(next++)='\0';

		// Validate the group
		if(*(ptr+1) != ':') {
            colon=strchr(ptr, ':');
			if(colon)
                *colon='\0';
			quit(1, "API invalid group name '%s'", ptr);
		}

        group=GROUP(*ptr);
		if(!VALIDGROUP(group))
			quit(1, "API invalid group name '%c'", *ptr);

        if(group==PRIVGROUP)
			quit(1, "API group name can't be '%c'", PRIVGROUP);

        if(group==NOPRIVGROUP)
			quit(1, "API group name can't be '%c'", NOPRIVGROUP);

		if(apigroups[GROUPOFFSET(group)].commands != NULL)
			quit(1, "API duplicate group name '%c'", *ptr);

		ptr += 2;

		// Validate the command list (and handle '*')
        cmd=&(commands[0]);
        *(cmd++)=SEPARATOR;
        *cmd='\0';
        addstar=false;
		while(ptr && *ptr) {
            colon=strchr(ptr, ':');
			if(colon)
                *(colon++)='\0';

            if(strcmp(ptr, "*")==0)
                addstar=true;
			else {
                did=false;
                for(i=0; cmds[i].name != NULL; i++) {
                    if(strcasecmp(ptr, cmds[i].name)==0) {
                        did=true;
						break;
					}
				}
				if(did) {
					// skip duplicates
					sprintf(cmdbuf, "|%s|", cmds[i].name);
                    if(strstr(commands, cmdbuf)==NULL) {
						strcpy(cmd, cmds[i].name);
						cmd += strlen(cmds[i].name);
                        *(cmd++)=SEPARATOR;
                        *cmd='\0';
					}
				} else {
					quit(1, "API unknown command '%s' in group '%c'", ptr, group);
				}
			}

            ptr=colon;
		}

        // *=allow all non-iswritemode commands
		if(addstar) {
            for(i=0; cmds[i].name != NULL; i++) {
                if(cmds[i].iswritemode==false) {
					// skip duplicates
					sprintf(cmdbuf, "|%s|", cmds[i].name);
                    if(strstr(commands, cmdbuf)==NULL) {
						strcpy(cmd, cmds[i].name);
						cmd += strlen(cmds[i].name);
                        *(cmd++)=SEPARATOR;
                        *cmd='\0';
					}
				}
			}
		}

        ptr=apigroups[GROUPOFFSET(group)].commands=cgmalloc(strlen(commands) + 1);

		strcpy(ptr, commands);
	}

	// Now define R (NOPRIVGROUP) as all non-iswritemode commands
    cmd=&(commands[0]);
    *(cmd++)=SEPARATOR;
    *cmd='\0';
    for(i=0; cmds[i].name != NULL; i++) {
        if(cmds[i].iswritemode==false) {
			strcpy(cmd, cmds[i].name);
			cmd += strlen(cmds[i].name);
            *(cmd++)=SEPARATOR;
            *cmd='\0';
		}
	}

    ptr=apigroups[GROUPOFFSET(NOPRIVGROUP)].commands=cgmalloc(strlen(commands) + 1);

	strcpy(ptr, commands);

	// W (PRIVGROUP) is handled as a special case since it simply means all commands

	free(buf);
	return;
}

/*
 * Interpret [W:]IP[/Prefix][,[R|W:]IP2[/Prefix2][,...]] --api-allow option
 *  ipv6 address should be enclosed with a pair of square brackets and the prefix left outside
 *	special case of 0/0 allows /0 (means all IP addresses)
 */
#define ALLIP "0/0"
/*
 * N.B. IP4 addresses are by Definition 32bit big endian on all platforms
 */
static void setup_ipaccess()
{
	char *buf, *ptr, *comma, *slash, *end, *dot;
	int ipcount, mask, i, shift;
	char tmp[64], original[64];
    bool ipv6=false;
	char group;

    buf=cgmalloc(strlen(opt_api_allow) + 1);

	strcpy(buf, opt_api_allow);

    ipcount=1;
    ptr=buf;
	while(*ptr)
        if(*(ptr++)==',')
			ipcount++;

	// possibly more than needed, but never less
    ipaccess=cgcalloc(ipcount, sizeof(struct IPACCESS));

    ips=0;
    ptr=buf;
	while(ptr && *ptr) {
        while(*ptr==' ' || *ptr=='\t')
			ptr++;

        if(*ptr==',') {
			ptr++;
			continue;
		}

        comma=strchr(ptr, ',');
		if(comma)
            *(comma++)='\0';

		strncpy(original, ptr, sizeof(original));
        original[sizeof(original)-1]='\0';
        group=NOPRIVGROUP;

        if(isalpha(*ptr) && *(ptr+1)==':') {
			if(DEFINEDGROUP(*ptr))
                group=GROUP(*ptr);

			ptr += 2;
		}

        ipaccess[ips].group=group;

        if(strcmp(ptr, ALLIP)==0) {
			for(i=0; i<16; i++) {
                ipaccess[ips].ip.s6_addr[i]=0;
                ipaccess[ips].mask.s6_addr[i]=0;
			}
		}
		else {
            end=strchr(ptr, '/');
			if(!end) {
				for(i=0; i<16; i++)
                    ipaccess[ips].mask.s6_addr[i]=0xff;
                end=ptr + strlen(ptr);
			}
            slash=end--;
            if(*ptr=='[' && *end==']') {
                *(ptr++)='\0';
                *(end--)='\0';
                ipv6=true;
			}
			else
                ipv6=false;
			if(*slash) {
                *(slash++)='\0';
                mask=atoi(slash);
				if(mask < 1 || (mask += ipv6 ? 0 : 96) > 128) {
					applog(LOG_ERR, "API: ignored address with "
							"invalid mask (%d) '%s'",
							mask, original);
					goto popipo; // skip invalid/zero
				}

				for(i=0; i<16; i++)
                    ipaccess[ips].mask.s6_addr[i]=0;

                i=0;
                shift=7;
				while(mask-- > 0) {
					ipaccess[ips].mask.s6_addr[i] |= 1 << shift;
                    if(shift--==0) {
						i++;
                        shift=7;
					}
				}
			}

			for(i=0; i<16; i++)
                ipaccess[ips].ip.s6_addr[i]=0; // missing default to '[::]'
			if(ipv6) {
				if(INET_PTON(AF_INET6, ptr, &(ipaccess[ips].ip)) != 1) {
					applog(LOG_ERR, "API: ignored invalid "
							"IPv6 address '%s'",
							original);
					goto popipo;
				}
			}
			else {
				/* v4 mapped v6 address,
				 * such as "::ffff:255.255.255.255"
				 * but pad on extra missing .0 as needed */
                dot=strchr(ptr, '.');
				if(!dot) {
					snprintf(tmp, sizeof(tmp),
						 "::ffff:%s.0.0.0",
						 ptr);
				} else {
                    dot=strchr(dot+1, '.');
					if(!dot) {
						snprintf(tmp, sizeof(tmp),
							 "::ffff:%s.0.0",
							 ptr);
					} else {
                        dot=strchr(dot+1, '.');
						if(!dot) {
							snprintf(tmp, sizeof(tmp),
								 "::ffff:%s.0",
								 ptr);
						} else {
							snprintf(tmp, sizeof(tmp),
								 "::ffff:%s",
								 ptr);
						}
					}
				}
				if(INET_PTON(AF_INET6, tmp, &(ipaccess[ips].ip)) != 1) {
					applog(LOG_ERR, "API: ignored invalid "
							"IPv4 address '%s' (as %s)",
							original, tmp);
					goto popipo;
				}
			}
			for(i=0; i<16; i++)
				ipaccess[ips].ip.s6_addr[i] &= ipaccess[ips].mask.s6_addr[i];
		}

		ips++;
popipo:
        ptr=comma;
	}

	free(buf);
}

static void *quit_thread(void *userdata)
{
	// allow thread creator to finish whatever it's doing
	mutex_lock(&quit_restart_lock);
	mutex_unlock(&quit_restart_lock);

	if(opt_debug)
		applog(LOG_DEBUG, "API: killing cgminer");

	kill_work();

	return NULL;
}

static void *restart_thread(void *userdata)
{
	// allow thread creator to finish whatever it's doing
	mutex_lock(&quit_restart_lock);
	mutex_unlock(&quit_restart_lock);

	if(opt_debug)
		applog(LOG_DEBUG, "API: restarting cgminer");

	app_restart();

	return NULL;
}

static bool check_connect(struct sockaddr_storage *cli, char **connectaddr, char *group)
{
    bool addrok=false;
	int i, j;
	bool match;
	char tmp[30];
	struct in6_addr client_ip;

    *connectaddr=cgmalloc(INET6_ADDRSTRLEN);
	getnameinfo((struct sockaddr *)cli, sizeof(*cli),
			*connectaddr, INET6_ADDRSTRLEN, NULL, 0, NI_NUMERICHOST);

	// v4 mapped v6 address, such as "::ffff:255.255.255.255"
    if(cli->ss_family==AF_INET) {
		sprintf(tmp, "::ffff:%s", *connectaddr);
		INET_PTON(AF_INET6, tmp, &client_ip);
	}
	else
		INET_PTON(AF_INET6, *connectaddr, &client_ip);

    *group=NOPRIVGROUP;
	if(opt_api_allow) {
		for(i=0; i<ips; i++) {
            match=true;
            for(j=0; j < 16; j++) {
				if((client_ip.s6_addr[j] & ipaccess[i].mask.s6_addr[j])
						!= ipaccess[i].ip.s6_addr[j]) {
                    match=false;
					break;
				}
			}
			if(match) {
                addrok=true;
                *group=ipaccess[i].group;
				break;
			}
		}
	} else {
		if(opt_api_network)
            addrok=true;
		else
            addrok=(strcmp(*connectaddr, localaddr)==0)
				|| IN6_IS_ADDR_LOOPBACK(&client_ip);
	}

	return addrok;
}

static void mcast()
{
	struct sockaddr_storage came_from;
	time_t bindstart;
	char *binderror;
    SOCKETTYPE mcast_sock=INVSOCK;
    SOCKETTYPE reply_sock=INVSOCK;
	socklen_t came_from_siz;
	char *connectaddr;
	ssize_t rep;
	int bound;
	int count;
	int reply_port;
	bool addrok;
	char group;

	char port_s[10], came_from_port[10];
	struct addrinfo hints, *res, *host, *client;

    char expect[]="cgminer-"; // first 8 bytes constant
	char *expect_code;
	size_t expect_code_len;
	char buf[1024];
	char replybuf[1024];

	sprintf(port_s, "%d", opt_api_mcast_port);
	memset(&hints, 0, sizeof(hints));
    hints.ai_family=AF_UNSPEC;
	if(getaddrinfo(opt_api_mcast_addr, port_s, &hints, &res) != 0)
		quit(1, "Invalid API Multicast Address");
    host=res;
	while(host != NULL) {
        mcast_sock=socket(res->ai_family, SOCK_DGRAM, 0);
		if(mcast_sock > 0)
			break;
        host=host->ai_next;
	}
    if(mcast_sock==INVSOCK) {
		freeaddrinfo(res);
		quit(1, "API mcast could not open socket");
	}

    int optval=1;
	if(SOCKETFAIL(setsockopt(mcast_sock, SOL_SOCKET, SO_REUSEADDR, (void *)(&optval), sizeof(optval)))) {
		applog(LOG_ERR, "API mcast setsockopt SO_REUSEADDR failed (%s)%s", SOCKERRMSG, MUNAVAILABLE);
		goto die;
	}

	// try for more than 1 minute ... in case the old one hasn't completely gone yet
    bound=0;
    bindstart=time(NULL);
	while(bound==0)
	{
		if(SOCKETFAIL(bind(mcast_sock, host->ai_addr, host->ai_addrlen)))
		{
            binderror=SOCKERRMSG;
			if((time(NULL) - bindstart) > 61)
				break;
			else
				cgsleep_ms(30000);
		}
		else
		{
			bound=1;
		}
	}

	if(bound==0)
	{
		applog(LOG_ERR, "API mcast bind to port %d failed (%s)%s", opt_api_mcast_port, binderror, MUNAVAILABLE);
		goto die;
	}

	switch (host->ai_family)
	{
		case AF_INET:
		{
			struct ip_mreq grp;
			memset(&grp, 0, sizeof(grp));
            grp.imr_multiaddr.s_addr=((struct sockaddr_in *)(host->ai_addr))->sin_addr.s_addr;
            grp.imr_interface.s_addr=INADDR_ANY;
			if(SOCKETFAIL(setsockopt(mcast_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *)(&grp), sizeof(grp))))
			{
				applog(LOG_ERR, "API mcast join failed (%s)%s", SOCKERRMSG, MUNAVAILABLE);
				goto die;
			}
			break;
		}
		case AF_INET6:
		{
			struct ipv6_mreq grp;
			memcpy(&grp.ipv6mr_multiaddr, &(((struct sockaddr_in6 *)(host->ai_addr))->sin6_addr), sizeof(struct in6_addr));
			grp.ipv6mr_interface=0;
			if(SOCKETFAIL(setsockopt(mcast_sock, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, (void *)(&grp), sizeof(grp))))
			{
				applog(LOG_ERR, "API mcast join failed (%s)%s", SOCKERRMSG, MUNAVAILABLE);
				goto die;
			}
			break;
		}
		default:
			break;
	}
	freeaddrinfo(res);

    expect_code_len=sizeof(expect) + strlen(opt_api_mcast_code);
    expect_code=cgmalloc(expect_code_len + 1);
	snprintf(expect_code, expect_code_len+1, "%s%s-", expect, opt_api_mcast_code);

    count=0;
	while(42)
	{
		cgsleep_ms(1000);

		count++;
        came_from_siz=sizeof(came_from);
		rep=recvfrom(mcast_sock, buf, sizeof(buf) - 1, 0, (struct sockaddr *)(&came_from), &came_from_siz);
		if(rep<0)
		{
			applog(LOG_DEBUG, "API mcast failed count=%d (%s) (%d)", count, SOCKERRMSG, (int)mcast_sock);
			continue;
		}

        addrok=check_connect(&came_from, &connectaddr, &group);
		applog(LOG_DEBUG, "API mcast from %s - %s", connectaddr, addrok ? "Accepted" : "Ignored");
		if(!addrok)
		{
			continue;
		}

        buf[rep]='\0';
        if(rep > 0 && buf[rep-1]=='\n')
		{
			buf[--rep]='\0';
		}

		getnameinfo((struct sockaddr *)(&came_from), came_from_siz, NULL, 0, came_from_port, sizeof(came_from_port), NI_NUMERICHOST);

		applog(LOG_DEBUG, "API mcast request rep=%d (%s) from [%s]:%s", (int)rep, buf, connectaddr, came_from_port);

		if((size_t)rep > expect_code_len && memcmp(buf, expect_code, expect_code_len)==0)
		{
			reply_port=atoi(&buf[expect_code_len]);
			if(reply_port < 1 || reply_port > 65535)
			{
				applog(LOG_DEBUG, "API mcast request ignored - invalid port (%s)", &buf[expect_code_len]);
			}
			else
			{
				applog(LOG_DEBUG, "API mcast request OK port %s=%d", &buf[expect_code_len], reply_port);

				if(getaddrinfo(connectaddr, &buf[expect_code_len], &hints, &res) != 0) {
					applog(LOG_ERR, "Invalid client address %s", connectaddr);
					continue;
				}
                client=res;
				while(client) {
                    reply_sock=socket(res->ai_family, SOCK_DGRAM, 0);
					if(mcast_sock > 0)
						break;
                    client=client->ai_next;
				}
                if(reply_sock==INVSOCK) {
					freeaddrinfo(res);
					applog(LOG_ERR, "API mcast could not open socket to client %s", connectaddr);
					continue;
				}

				snprintf(replybuf, sizeof(replybuf),
							"cgm-" API_MCAST_CODE "-%d-%s",
							opt_api_port, opt_api_mcast_des);

                rep=sendto(reply_sock, replybuf, strlen(replybuf)+1,
						0, client->ai_addr, client->ai_addrlen);
				freeaddrinfo(res);
				if(SOCKETFAIL(rep)) {
					applog(LOG_DEBUG, "API mcast send reply failed (%s) (%d)",
								SOCKERRMSG, (int)reply_sock);
				} else {
					applog(LOG_DEBUG, "API mcast send reply (%s) succeeded (%d) (%d)",
								replybuf, (int)rep, (int)reply_sock);
				}

				CLOSESOCKET(reply_sock);
			}
		} else
			applog(LOG_DEBUG, "API mcast request was no good");
	}

die:

	CLOSESOCKET(mcast_sock);
}

static void *mcast_thread(void *userdata)
{
    struct thr_info *mythr=userdata;

	pthread_detach(pthread_self());
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	RenameThread("APIMcast");

	mcast();

	mythr->pth=0UL;

	return NULL;
}

void mcast_init()
{
	struct thr_info *thr;

    thr=cgcalloc(1, sizeof(*thr));

	if(thr_info_create(thr, NULL, mcast_thread, thr))
		quit(1, "API mcast thread create failed");
}

void api(int api_thr_id)
{
	struct io_data *io_data;
	struct thr_info bye_thr;
	char buf[TMPBUFSIZ];
	char param_buf[TMPBUFSIZ];
	SOCKETTYPE c;
	int n, bound;
	char *connectaddr;
	char *binderror;
	time_t bindstart;
    short int port=opt_api_port;
	char port_s[10];
	struct sockaddr_storage cli;
	socklen_t clisiz;
	char cmdbuf[100];
    char *cmd=NULL;
	char *param;
	bool addrok;
	char group;
	json_error_t json_err;
	json_t *json_obj;
	json_t *json_val;
	bool did, isjoin, firstjoin;
	int i;
	struct addrinfo hints, *res, *host;
	SOCKETTYPE *apisock=cgmalloc(sizeof(*apisock));

    *apisock=INVSOCK;
	json_obj=NULL;
    isjoin=false;

	if(!opt_api_listen) {
		applog(LOG_DEBUG, "API not running%s", UNAVAILABLE);
		free(apisock);
		return;
	}

    io_data=sock_io_new();

	mutex_init(&quit_restart_lock);

	pthread_cleanup_push(tidyup, (void *)apisock);
    my_thr_id=api_thr_id;

	setup_groups();

	if(opt_api_allow) {
		setup_ipaccess();

        if(ips==0) {
			applog(LOG_WARNING, "API not running (no valid IPs specified)%s", UNAVAILABLE);
			free(apisock);
			return;
		}
	}

	/* This should be done before curl in needed
	 * to ensure curl has already called WSAStartup() in windows */
	cgsleep_ms(opt_log_interval*1000);

	sprintf(port_s, "%d", port);
	memset(&hints, 0, sizeof(hints));
    hints.ai_flags=AI_PASSIVE;
    hints.ai_family=AF_UNSPEC;
	if(getaddrinfo(opt_api_host, port_s, &hints, &res) != 0) {
		applog(LOG_ERR, "API failed to resolve %s", opt_api_host);
		free(apisock);
		return;
	}
    host=res;
	while(host) {
        *apisock=socket(res->ai_family, SOCK_STREAM, 0);
		if(*apisock > 0)
			break;
        host=host->ai_next;
	}
    if(*apisock==INVSOCK) {
		applog(LOG_ERR, "API initialisation failed (%s)%s", SOCKERRMSG, UNAVAILABLE);
		freeaddrinfo(res);
		free(apisock);
		return;
	}

	// On linux with SO_REUSEADDR, bind will get the port if the previous
	// socket is closed (even if it is still in TIME_WAIT) but fail if
	// another program has it open - which is what we want
    int optval=1;
	// If it doesn't work, we don't really care - just show a debug message
	if(SOCKETFAIL(setsockopt(*apisock, SOL_SOCKET, SO_REUSEADDR, (void *)(&optval), sizeof(optval))))
    {
        applog(LOG_DEBUG, "API setsockopt SO_REUSEADDR failed (ignored): %s", SOCKERRMSG);
    }

	// try for more than 1 minute ... in case the old one hasn't completely gone yet
    bound=0;
    bindstart=time(NULL);
    while(bound==0) {
		if(SOCKETFAIL(bind(*apisock, host->ai_addr, host->ai_addrlen))) {
            binderror=SOCKERRMSG;
			if((time(NULL) - bindstart) > 61)
				break;
			else {
				applog(LOG_WARNING, "API bind to port %d failed - trying again in 30sec", port);
				cgsleep_ms(30000);
			}
		} else
            bound=1;
	}
	freeaddrinfo(res);

    if(bound==0) {
		applog(LOG_ERR, "API bind to port %d failed (%s)%s", port, binderror, UNAVAILABLE);
		free(apisock);
		return;
	}

	if(SOCKETFAIL(listen(*apisock, QUEUE))) {
		applog(LOG_ERR, "API3 initialisation failed (%s)%s", SOCKERRMSG, UNAVAILABLE);
		CLOSESOCKET(*apisock);
		free(apisock);
		return;
	}

	if(opt_api_allow)
	{
		applog(LOG_WARNING, "API running in IP access mode on port %i (%i)", port, (int)*apisock);
	}
	else
	{
		if(opt_api_network)
			applog(LOG_WARNING, "API running in UNRESTRICTED read access mode on port %d (%d)", port, (int)*apisock);
		else
			applog(LOG_WARNING, "API running in local read access mode on port %d (%d)", port, (int)*apisock);
	}

	if(opt_api_mcast)
		mcast_init();

    strbufs=k_new_list("StrBufs", sizeof(SBITEM), ALLOC_SBITEMS, LIMIT_SBITEMS, false);

	while(!bye)
	{
        clisiz=sizeof(cli);
		if(SOCKETFAIL(c=accept(*apisock, (struct sockaddr *)(&cli), &clisiz)))
		{
			applog(LOG_ERR, "API failed (%s)%s (%d)", SOCKERRMSG, UNAVAILABLE, (int)*apisock);
			goto die;
		}

        addrok=check_connect((struct sockaddr_storage *)&cli, &connectaddr, &group);
		applog(LOG_DEBUG, "API: connection from %s - %s",
					connectaddr, addrok ? "Accepted" : "Ignored");

		if(addrok)
		{
            n=recv(c, &buf[0], TMPBUFSIZ-1, 0);
			if(n<0)
			{
				buf[0]='\0';
			}
			else
			{
				buf[n]='\0';
			}

			if(opt_debug)
			{
				if(SOCKETFAIL(n))
					applog(LOG_DEBUG, "API: recv failed: %s", SOCKERRMSG);
				else
					applog(LOG_DEBUG, "API: recv command: (%d) '%s'", n, buf);
			}

			if(!SOCKETFAIL(n))
			{
				// the time of the request in now
                when=time(NULL);
				io_reinit(io_data);

                did=false;
				param=NULL;

				if(buf[0]==JSON_START[0])
				{
					json_obj=json_loadb(buf, n, JSON_DECODE_ANY|JSON_REJECT_DUPLICATES, &json_err);
					if(json_is_object(json_obj))
					{
						json_val=json_object_get(json_obj, JSON_COMMAND);
						if(json_val==NULL)
						{
							message(io_data, MSG_MISCMD, 0, NULL);
							send_result(io_data, c, true);
							did=true;
						}
						else
						{
							if(!json_is_string(json_val))
							{
								message(io_data, MSG_INVCMD, 0, NULL);
								send_result(io_data, c, true);
								did=true;
							} else {
								cmd=(char *)json_string_value(json_val);
								json_val=json_object_get(json_obj, JSON_PARAMETER);
								if(json_is_string(json_val))
									param=(char *)json_string_value(json_val);
								else if(json_is_integer(json_val)) {
									sprintf(param_buf, "%d", (int)json_integer_value(json_val));
									param=param_buf;
								} else if(json_is_real(json_val)) {
									sprintf(param_buf, "%f", (double)json_real_value(json_val));
									param=param_buf;
								}
							}
						}
					}
					else
					{
						message(io_data, MSG_INVJSON, 0, NULL);
						send_result(io_data, c, true);
						did=true;
					}
				}
				else
				{
					cmd=buf;
				}

				if(!did)
				{
                    char *cmdptr, *cmdsbuf=NULL;
//					if(strchr(cmd, CMDJOIN))
//					{
//                        firstjoin=isjoin=true;
//						// cmd + leading+tailing '|' + '\0'
//                        cmdsbuf=cgmalloc(strlen(cmd) + 3);
//						strcpy(cmdsbuf, "|");
//                        param=NULL;
//					}
//					else
//					{
						firstjoin=isjoin=false;
//					}

                    cmdptr=cmd;
					do {
                        did=false;
						if(isjoin)
						{
//							cmd=strchr(cmdptr, CMDJOIN);
//							if(cmd)
//							{
//								*(cmd++)='\0';
//							}
							if(!*cmdptr)
							{
								goto inochi;
							}
						}

						for(i=0; cmds[i].name != NULL; i++)
						{
							if(strcmp(cmdptr, cmds[i].name)==0)
							{
								sprintf(cmdbuf, "|%s|", cmdptr);
								if(isjoin)
								{
									if(strstr(cmdsbuf, cmdbuf))
									{
                                        did=true;
										break;
									}
									strcat(cmdsbuf, cmdptr);
									strcat(cmdsbuf, "|");
									head_join(io_data, cmdptr, true, &firstjoin);
								}
								if(ISPRIVGROUP(group) || strstr(COMMANDS(group), cmdbuf))
								{
									(cmds[i].func)(io_data, c, param, true, group);
								}
								else
								{
									message(io_data, MSG_ACCDENY, 0, cmds[i].name);
									applog(LOG_DEBUG, "API: access denied to '%s' for '%s' command", connectaddr, cmds[i].name);
								}

                                did=true;
								if(isjoin)
									tail_join(io_data, true);
								else
									send_result(io_data, c, true);
								break;
							}
						}

						if(!did)
						{
							if(isjoin)
							{
								head_join(io_data, cmdptr, true, &firstjoin);
							}
							message(io_data, MSG_INVCMD, 0, NULL);
							if(isjoin)
								tail_join(io_data, true);
							else
								send_result(io_data, c, true);
						}
inochi:
						if(isjoin)
						{
							cmdptr=cmd;
						}
					} while(isjoin && cmdptr);
				}

				if(isjoin)
				{
					send_result(io_data, c, true);
				}

				if(json_is_object(json_obj))
				{
					json_decref(json_obj);
				}
			}
		}
		CLOSESOCKET(c);
	}
die:
	/* Blank line fix for older compilers since pthread_cleanup_pop is a
	 * macro that gets confused by a label existing immediately before it
	 */
	;
	pthread_cleanup_pop(true);

	free(apisock);

	if(opt_debug)
		applog(LOG_DEBUG, "API: terminating due to: %s",
				do_a_quit ? "QUIT" : (do_a_restart ? "RESTART" : (bye ? "BYE" : "UNKNOWN!")));

	mutex_lock(&quit_restart_lock);

	if(do_a_restart) {
		if(thr_info_create(&bye_thr, NULL, restart_thread, &bye_thr)) {
			mutex_unlock(&quit_restart_lock);
			quit(1, "API failed to initiate a restart - aborting");
		}
		pthread_detach(bye_thr.pth);
	} else if(do_a_quit) {
		if(thr_info_create(&bye_thr, NULL, quit_thread, &bye_thr)) {
			mutex_unlock(&quit_restart_lock);
			quit(1, "API failed to initiate a clean quit - aborting");
		}
		pthread_detach(bye_thr.pth);
	}

	mutex_unlock(&quit_restart_lock);
}
