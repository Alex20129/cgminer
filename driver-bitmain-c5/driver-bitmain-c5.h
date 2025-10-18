/*
 * Copyright 2016-2017 Fazio Bai <yang.bai@bitmain.com>
 * Copyright 2016-2017 Clement Duan <kai.duan@bitmain.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3 of the License, or (at your option)
 * any later version.  See COPYING for more details.
 */

#ifndef DRIVER_BITMAIN_C5_H
#define DRIVER_BITMAIN_C5_H

#include <sys/types.h>
#include <stdbool.h>

//FPGA rgister Address Map
#define HARDWARE_VERSION                (0x00000000/sizeof(int))
#define FAN_SPEED                       (0x00000004/sizeof(int))
#define HASH_ON_PLUG                    (0x00000008/sizeof(int))
#define BUFFER_SPACE                    (0x0000000c/sizeof(int))
#define RETURN_NONCE                    (0x00000010/sizeof(int))
#define NONCE_NUMBER_IN_FIFO            (0x00000018/sizeof(int))
#define NONCE_FIFO_INTERRUPT            (0x0000001c/sizeof(int))
#define TEMPERATURE_0_3                 (0x00000020/sizeof(int))
#define TEMPERATURE_4_7                 (0x00000024/sizeof(int))
#define TEMPERATURE_8_11                (0x00000028/sizeof(int))
#define TEMPERATURE_12_15               (0x0000002c/sizeof(int))
#define IIC_COMMAND                     (0x00000030/sizeof(int))
#define RESET_HASHBOARD_COMMAND         (0x00000034/sizeof(int))
#define BMC_CMD_COUNTER                 (0x00000038/sizeof(int))
#define TW_WRITE_COMMAND                (0x00000040/sizeof(int))
#define QN_WRITE_DATA_COMMAND           (0x00000080/sizeof(int))
#define FAN_CONTROL                     (0x00000084/sizeof(int))
#define TIME_OUT_CONTROL                (0x00000088/sizeof(int))
#define TICKET_MASK_FPGA                (0x0000008c/sizeof(int))
#define HASH_COUNTING_NUMBER_FPGA       (0x00000090/sizeof(int))
#define SNO                             (0x00000094/sizeof(int))
#define BC_WRITE_COMMAND                (0x000000c0/sizeof(int))
#define BC_COMMAND_BUFFER               (0x000000c4/sizeof(int))
#define FPGA_CHIP_ID_ADDR               (0x000000f0/sizeof(int))
#define CRC_ERROR_CNT_ADDR              (0x000000f8/sizeof(int))
#define DHASH_ACC_CONTROL               (0x00000100/sizeof(int))
#define COINBASE_AND_NONCE2_LENGTH      (0x00000104/sizeof(int))
#define WORK_NONCE_2                    (0x00000108/sizeof(int))
#define NONCE2_AND_JOBID_STORE_ADDRESS  (0x00000110/sizeof(int))
#define MERKLE_BIN_NUMBER               (0x00000114/sizeof(int))
#define JOB_START_ADDRESS               (0x00000118/sizeof(int))
#define JOB_LENGTH                      (0x0000011c/sizeof(int))
#define JOB_DATA_READY                  (0x00000120/sizeof(int))
#define JOB_ID                          (0x00000124/sizeof(int))
#define BLOCK_HEADER_VERSION            (0x00000130/sizeof(int))
#define TIME_STAMP                      (0x00000134/sizeof(int))
#define TARGET_BITS                     (0x00000138/sizeof(int))
#define PRE_HEADER_HASH                 (0x00000140/sizeof(int))

//FPGA registers bit map
//QN_WRITE_DATA_COMMAND
#define RESET_HASH_BOARD                (1 << 31)
#define RESET_ALL                       (1 << 23)
#define CHAIN_ID(id)                    (id << 16)
#define RESET_FPGA                      (1 << 15)
#define RESET_TIME(time)                (time << 0)
#define TIME_OUT_VALID                  (1 << 31)
//RETURN_NONCE
#define WORK_ID_OR_CRC                  (1 << 31)
#define WORK_ID_OR_CRC_VALUE(value)     ((value >> 16) & 0x7fff)
#define NONCE_INDICATOR                 (1 << 7)
#define CHAIN_NUMBER(value)             (value & 0xf)
#define REGISTER_DATA_CRC(value)        ((value >> 24) & 0x7f)
//BC_WRITE_COMMAND
#define BC_COMMAND_BUFFER_READY         (1 << 31)
#define BC_COMMAND_EN_CHAIN_ID          (1 << 23)
#define BC_COMMAND_EN_NULL_WORK         (1 << 22)
//NONCE2_AND_JOBID_STORE_ADDRESS
#define JOB_ID_OFFSET                   (0x0/sizeof(int))
#define HEADER_VERSION_OFFSET           (0x4/sizeof(int))
#define NONCE2_L_OFFSET                 (0x8/sizeof(int))
#define NONCE2_H_OFFSET                 (0xc/sizeof(int))
#define MIDSTATE_OFFSET                 0x20
//DHASH_ACC_CONTROL
#define VIL_MODE                        (1 << 15)
#define VIL_MIDSTATE_NUMBER(value)      ((value & 0x0f) << 8)
#define NEW_BLOCK                       (1 << 7)
#define RUN_BIT                         (1 << 6)
#define OPERATION_MODE                  (1 << 5)
//NONCE_FIFO_INTERRUPT
#define FLUSH_NONCE3_FIFO               (1 << 16)


//ASIC macro define
//ASIC register address
#define C5_VERSION              1
#define CHIP_ADDRESS            0x0
#define GOLDEN_NONCE_COUNTER    0x8
#define PLL_PARAMETER           0xc
#define START_NONCE_OFFSET      0x10
#define HASH_COUNTING_NUMBER    0x14
#define TICKET_MASK             0x18
#define MISC_CONTROL            0x1c
#define GENERAL_I2C_COMMAND     0X20

//ASIC command
#define SET_ADDRESS             0x1
#define SET_PLL_DIVIDER2        0x2
#define PATTERN_CONTROL         0x3
#define GET_STATUS              0x4
#define CHAIN_INACTIVE          0x5
#define SET_BAUD_OPS            0x6
#define SET_PLL_DIVIDER1        0x7
#define SET_CONFIG              0x8
#define COMMAND_FOR_ALL         0x80
//other ASIC macro define
#define MAX_BAUD_DIVIDER        26
#define DEFAULT_BAUD_DIVIDER    26
#define VIL_COMMAND_TYPE        (0x02 << 5)
#define VIL_ALL                 (0x01 << 4)
#define PAT                     (0x01 << 7)
#define GRAY                    (0x01 << 6)
#define INV_CLKO                (0x01 << 5)
#define LPD                     (0x01 << 4)
#define GATEBCLK                (0x01 << 7)
#define RFS                     (0x01 << 6)
#define MMEN                    (0x01 << 7)
#define TFS(x)                  ((x & 0x03) << 5)

// Pic
#define PIC_FLASH_POINTER_START_ADDRESS_H   0x03
#define PIC_FLASH_POINTER_START_ADDRESS_L   0x00
#define PIC_FLASH_POINTER_END_ADDRESS_H     0x0f
#define PIC_FLASH_POINTER_END_ADDRESS_L     0x7f
#define PIC_FLASH_SECTOR_LENGTH             32
#define PIC_SOFTWARE_VERSION_LENGTH         1
#define PIC_VOLTAGE_TIME_LENGTH             6
#define PIC_COMMAND_1                       0x55
#define PIC_COMMAND_2                       0xaa
#define SET_PIC_FLASH_POINTER               0x01
#define SEND_DATA_TO_IIC                    0x02    // just send data into pic's cache
#define READ_DATA_FROM_IIC                  0x03
#define ERASE_IIC_FLASH                     0x04    // erase 32 bytes one time
#define WRITE_DATA_INTO_PIC                 0x05    // tell pic write data into flash from cache
#define JUMP_FROM_LOADER_TO_APP             0x06
#define RESET_PIC                           0x07
#define GET_PIC_FLASH_POINTER               0x08
#define ERASE_PIC_APP_PROGRAM               0x09
#define SET_VOLTAGE                         0x10
#define SET_VOLTAGE_TIME                    0x11
#define SET_HASH_BOARD_ID                   0x12
#define GET_HASH_BOARD_ID                   0x13
#define SET_HOST_MAC_ADDRESS                0x14
#define ENABLE_VOLTAGE                      0x15
#define SEND_HEART_BEAT                     0x16
#define GET_PIC_SOFTWARE_VERSION            0x17
#define GET_VOLTAGE                         0x18
#define GET_DATE                            0x19
#define GET_WHICH_MAC                       0x20
#define GET_MAC                             0x21
#define WR_TEMP_OFFSET_VALUE                0x22
#define RD_TEMP_OFFSET_VALUE                0x23

//diff freq
#define PIC_FLASH_POINTER_FREQ_START_ADDRESS_H   0x0F
#define PIC_FLASH_POINTER_FREQ_START_ADDRESS_L   0xA0
#define PIC_FLASH_POINTER_FREQ_END_ADDRESS_H     0x0f
#define PIC_FLASH_POINTER_FREQ_END_ADDRESS_L     0xDF
#define FREQ_MAGIC                               0x7D

// BAD CORE NUM
#define PIC_FLASH_POINTER_BADCORE_START_ADDRESS_H   0x0F
#define PIC_FLASH_POINTER_BADCORE_START_ADDRESS_L   0x80
#define PIC_FLASH_POINTER_BADCORE_END_ADDRESS_H     0x0f
#define PIC_FLASH_POINTER_BADCORE_END_ADDRESS_L     0x9F
#define BADCORE_MAGIC                            0x23   // magic number for bad core num

#define HEART_BEAT_TIME_GAP                 10      // 10s
#define IIC_READ                            (1 << 25)
#define IIC_WRITE                           (~IIC_READ)
#define IIC_REG_ADDR_VALID                  (1 << 24)
//#define IIC_ADDR_HIGH_4_BIT                   (0x0A << 20)
#define IIC_CHAIN_NUMBER(x)                 ((x & 0x0f) << 16)
#define IIC_REG_ADDR(x)                     ((x & 0xff) << 8)

// AT24C02
#define AT24C02_ADDRESS     0x50
#define EEPROM_LENGTH       256
#define HASH_ID_ADDR        0x80
#define VOLTAGE_ADDR        0x90
#define SENSOR_OFFSET_ADDR  0x98
#define VOLTAGE_SET_TIME    0xA0
#define VOLTAGE_SET_TIME    0xA0
#define FREQ_BADCORE_ADDR   0x00    // 128 bytes   0 - 0x7F

//other FPGA macro define
#define TOTAL_LEN                       0x160
#define FPGA_MEM_TOTAL_LEN              (16*1024*1024)  // 16M bytes
#define HARDWARE_VERSION_VALUE          0xC501
#define NONCE2_AND_JOBID_STORE_SPACE    (2*1024*1024)   // 2M bytes
#define NONCE2_AND_JOBID_STORE_SPACE_ORDER  9           // for 2M bytes space
#define JOB_STORE_SPACE                 (1 << 16)       // for 64K bytes space
#define JOB_START_SPACE                 (1024*8)        // 8K bytes
#define JOB_START_ADDRESS_ALIGN         32              // JOB_START_ADDRESS need 32 bytes aligned
#define NONCE2_AND_JOBID_ALIGN          64              // NONCE2_AND_JOBID_STORE_SPACE need 64 bytes aligned
#define MAX_TIMEOUT_VALUE               0x1ffff         // defined in TIME_OUT_CONTROL
#define MAX_NONCE_NUMBER_IN_FIFO        511
#define NONCE_DATA_LENGTH               4               // 4 bytes
#define REGISTER_DATA_LENGTH            4               // 4 bytes
#define TW_WRITE_COMMAND_LEN            52
#define TW_WRITE_COMMAND_LEN_VIL        52
#define NEW_BLOCK_MARKER                0x11
#define NORMAL_BLOCK_MARKER             0x01

// ATTENTION: if MEM size is changed, must change this micro definition too!!!   use MAX size (BYTE) - 16 MB as FPGA start memory address
#define PHY_MEM_NONCE2_JOBID_ADDRESS_XILINX_1GB         ((1024-16)*1024*1024)
#define PHY_MEM_NONCE2_JOBID_ADDRESS_XILINX_512MB       ((512-16)*1024*1024)        // XILINX use 512MB memory
#define PHY_MEM_NONCE2_JOBID_ADDRESS_XILINX_256MB       ((256-16)*1024*1024)        // XILINX use 512MB memory

#define PHY_MEM_NONCE2_JOBID_ADDRESS_C5         ((1024-16)*1024*1024)
extern unsigned int PHY_MEM_NONCE2_JOBID_ADDRESS;

#define PHY_MEM_JOB_START_ADDRESS_1     (PHY_MEM_NONCE2_JOBID_ADDRESS + NONCE2_AND_JOBID_STORE_SPACE)
#define PHY_MEM_JOB_START_ADDRESS_2     (PHY_MEM_JOB_START_ADDRESS_1 + JOB_STORE_SPACE)

//#define R4        // if defined , for R4  63 chips
//#define S9_PLUS   // if defined , for T9  57 chips

#define RESET_KEEP_TIME     3   // keep reset signal for 1 secnods
#undef USE_OPENCORE_ONEBYONE    // if defined, we will use open core one by one, do 114 times on open core for each chain!  but NOT WORKS!??
#define ENABLE_PREHEAT
#undef ENABLE_REGISTER_CRC_CHECK    //if defined, will drop the register buffer with crc error!
#define REBOOT_TEST_ONCE_1HOUR  //if defined, will check hashrate after 1 hour, and reboot only once
#define ENABLE_FINAL_TEST_WITHOUT_REBOOT    // when REBOOT_TEST_ONCE_1HOUR enabeld and this defined, the miner will not reboot after test for 1 hours, then we can save time. test system will treat these miners as good with green color.
#define DISABLE_FINAL_TEST      //if defined, it will set rebootTestNum=0 and restartNum=2 to indicate the the miner fw is in normal user mode , not test mode
#define DISABLE_SHOWX_ENABLE_XTIMES // if defined, will disable x show on web UI, but will enable x times counter in 1 mins
#define FASTER_TESTPATTEN   // will use 9% timeout to test patten
#undef USE_OPENCORE_TWICE
#undef ENABLE_REINIT_WHEN_TESTFAILED
#define RESET_HASHBOARD_TIME            15
#define ENABLE_CHECK_PIC_FLASH_ADDR     // if enabled, will check PIC FLASH ADDR value , set and read back to compare from PIC
#define ENABLE_RESTORE_PIC_APP          // if enabled, will restore PIC APP when the version is not correct!!!

#ifdef R4
#define USE_N_OFFSET_FIX_TEMP   // if defined, we will use n and offset to fix temp value
#define EXTEND_TEMP_MODE        // if defined, we set temp value area from -64 to 191 as extended temp
#define ENABLE_HIGH_VOLTAGE_OPENCORE

#define PIC_VERSION                     0x03

#define CHAIN_ASIC_NUM                  63

#define R4_MAX_VOLTAGE_C5                   890
#define R4_MAX_VOLTAGE_XILINX               910

#define FIX_BAUD_VALUE                  1
#define UPRATE_PERCENT                  1   // means we need reserved more 1% rate

#define HIGHEST_VOLTAGE_LIMITED_HW      940 //measn the largest voltage, hw can support
#define USE_NEW_RESET_FPGA
#undef USE_PREINIT_OPENCORE // if defined, we will open core at first ,then get asicnum and do other init process
#endif

#ifdef S9_PLUS
#define ENABLE_HIGH_VOLTAGE_OPENCORE

#define S9_PLUS_VOLTAGE2    //if defined, then it support S9+ new board with new voltage controller

#define PIC_VERSION                     0x03

#define CHAIN_ASIC_NUM                  57
#define USE_N_OFFSET_FIX_TEMP   // if defined, we will use n and offset to fix temp value
#define EXTEND_TEMP_MODE        // if defined, we set temp value area from -64 to 191 as extended temp
#define HIGHEST_VOLTAGE_LIMITED_HW      970 //measn the largest voltage, hw can support
#define FIX_BAUD_VALUE                  1
#define UPRATE_PERCENT                  2   // means we need reserved more 2% rate
#define USE_NEW_RESET_FPGA
#undef USE_PREINIT_OPENCORE // if defined, we will open core at first ,then get asicnum and do other init process
#endif

#if defined(USE_ANTMINER_S9)
#define ENABLE_HIGH_VOLTAGE_OPENCORE
#define TEMP_FROM_SENSORS
#define MINER_TYPE "Antminer S9"
#define PIC_VERSION                     0x03

#define CHAIN_ASIC_NUM                  63
#define USE_N_OFFSET_FIX_TEMP   // if defined, we will use n and offset to fix temp value
#define EXTEND_TEMP_MODE        // if defined, we set temp value area from -64 to 191 as extended temp

#define FIX_BAUD_VALUE                  1
#define UPRATE_PERCENT                  1   // means we need reserved more 1% rate
#define HIGHEST_VOLTAGE_LIMITED_HW      940 //measn the largest voltage, hw can support
#define USE_NEW_RESET_FPGA
#undef USE_PREINIT_OPENCORE // if defined, we will open core at first ,then get asicnum and do other init process
#endif

#if defined(USE_ANTMINER_T9)
#define ENABLE_HIGH_VOLTAGE_OPENCORE    // T9+ use this , will cause error on chips, because the voltage changing need a long time to balance

#define PIC_VERSION                     0x03

#define CHAIN_ASIC_NUM                  18
#define USE_N_OFFSET_FIX_TEMP   // if defined, we will use n and offset to fix temp value
#define EXTEND_TEMP_MODE        // if defined, we set temp value area from -64 to 191 as extended temp

#define FIX_BAUD_VALUE                  1
#define UPRATE_PERCENT                  2   // means we need reserved more 2% rate
#define HIGHEST_VOLTAGE_LIMITED_HW      930 //measn the largest voltage, hw can support
#define USE_NEW_RESET_FPGA
#undef USE_PREINIT_OPENCORE // if defined, we will open core at first ,then get asicnum and do other init process
#endif

#ifdef USE_PREINIT_OPENCORE
#define ENABLE_SET_TICKETMASK_BEFORE_TESTPATTEN     // a bug, do not know reason: asic ticket mask > 0 even after reset asic!!!
#else
#undef ENABLE_SET_TICKETMASK_BEFORE_TESTPATTEN      // a bug, do not know reason: asic ticket mask > 0 even after reset asic!!!
#endif

#define ASIC_TYPE           1387    // 1385 or  1387
#define CHIP_ADDR_INTERVAL  4   // fix chip address interval=4 
#define DEFAULT_BAUD_VALUE  26
#define ASIC_CORE_NUM       114

#define BM1387_CORE_NUM     ASIC_CORE_NUM

// macro define about miner
#define BITMAIN_MAX_CHAIN_NUM           16
#define BITMAIN_MAX_FAN_NUM             8               // FPGA just can supports 8 fan
#define BITMAIN_DEFAULT_ASIC_NUM        64              // max support 64 ASIC on 1 HASH board
#define MIDSTATE_LEN                    32
#define DATA2_LEN                       12
#define WORK_QUEUE_SIZE				256
#define PREV_HASH_LEN                   32
#define MERKLE_BIN_LEN                  32
#define INIT_CONFIG_TYPE                0x51
#define STATUS_DATA_TYPE                0xa1
#define SEND_JOB_TYPE                   0x52
#define READ_JOB_TYPE                   0xa2
#define CHECK_SYSTEM_TIME_GAP           10000           // 10s
//fan

// BELOW IS ALL FOR DEBUG !!! normally all must be undefined!!!
#undef DEBUG_KEEP_USE_PIC_VOLTAGE_WITHOUT_CHECKING_VOLTAGE_OF_SEARCHFREQ    // if defined, will read pic voltage at first , and use this voltage in mining as working voltage, ignore the backup voltage of search freq
#undef DEBUG_ENABLE_I2C_TIMEOUT_PROCESS // if defined, sw will process I2C timeout, but normally FPGA will process timeout, SW do not need this
#undef DEBUG_PRINT_T9_PLUS_PIC_HEART_INFO   // if defined, used to debug T9+ bug: pic heart cmd failed!
#undef DEBUG_PIC_UPGRADE        // if defined, we will force to write PIC program data once!
#undef DEBUG_KEEP_REBOOT_EVERY_ONE_HOUR     // if defined, keep reboot every one hour!!!  this is for R4 
#undef DEBUG_NOT_CHECK_FAN_NUM      // if defined, we will ignore fan number checking, will keep run even without any fan!!!
#undef DEBUG_WITHOUT_FREQ_VOLTAGE_LIMIT // if defined, we will not limit freq according to voltage!

#undef DEBUG_DOWN_VOLTAGE_TEST
#ifdef DEBUG_DOWN_VOLTAGE_TEST
#define DEBUG_DOWN_VOLTAGE_VALUE    10  // means down 0.1 V 
#endif

#undef DEBUG_XILINX_NONCE_NOTENOUGH // will disable mutex lock on read temp and send work, but will disable one chain's read temp
#ifdef DEBUG_XILINX_NONCE_NOTENOUGH
#define DISABLE_REG_CHAIN_INDEX     5   //disable which chain's read register
#endif

#undef DEBUG_OPENCORE_TWICE
#undef ENABLE_REINIT_MINING // if defined, will enable hashrate check in mining, and re-init if low hashrate.  
#undef DEBUG_REINIT // reinit per 2mins and will not do pre heat patten test
#undef DEBUG_REBOOT // reboot every 30mins, for test
#undef DEBUG_218_FAN_FULLSPEED  //for debug on 218, full speed on fan
#undef DISABLE_TEMP_PROTECT
#undef TWO_CHIP_TEMP_S9
#undef SHOW_BOTTOM_TEMP
#undef HIGH_TEMP_TEST_S9    //if defined, will use 120 degree as the high temp
#undef CAPTURE_PATTEN

#define CHECK_RT_IDEAL_RATE_PERCENT     85  // RT rate / ideal rate >= 85% will be OK, or need re init

#define CHAIN_TABLE

typedef enum
{
    TEMP_POS_LOCAL=0,
    TEMP_POS_MIDDLE,
    TEMP_POS_BOTTOM,
    TEMP_POS_NUM=4, // always the last one, to identify the number of temp , must 4 bytes alignment
} TEMP_POSITION;

#ifdef R4
#define PWM_T   0   // 0 local temp,  1 middle temp,  2 bottom,  as above!!!

#define MIN_FAN_NUM                     1
#define MAX_FAN_SPEED                   3000
#define TEMP_INTERVAL                   2

// below are used for R4 on using one app to support C5 and XILINX board
extern int MIN_PWM_PERCENT;
extern int MID_PWM_PERCENT;
extern int MAX_PWM_PERCENT;
extern int MAX_TEMP;
extern int MAX_FAN_TEMP;
extern int MID_FAN_TEMP;
extern int MIN_FAN_TEMP;
extern int MAX_PCB_TEMP;
extern int MAX_FAN_PCB_TEMP;

#if PWM_T==1
#define MIN_PWM_PERCENT_C5              20
#define MID_PWM_PERCENT_C5              60
#define MAX_PWM_PERCENT_C5              100
#define MAX_TEMP_C5                     125
#define MAX_FAN_TEMP_C5                 110
#define MID_FAN_TEMP_C5                 90
#define MIN_FAN_TEMP_C5                 60
#define MAX_PCB_TEMP_C5                 100 //  use middle to control fan, but use pcb temp to check to stop or not!
#define MAX_FAN_PCB_TEMP_C5             85  //90 use middle to control fan, but use pcb temp to check to stop or not!

#define MIN_PWM_PERCENT_XILINX          20
#define MID_PWM_PERCENT_XILINX          60
#define MAX_PWM_PERCENT_XILINX          100
#define MAX_TEMP_XILINX                 125
#define MAX_FAN_TEMP_XILINX             110
#define MID_FAN_TEMP_XILINX             90
#define MIN_FAN_TEMP_XILINX             60
#define MAX_PCB_TEMP_XILINX             100 //  use middle to control fan, but use pcb temp to check to stop or not!
#define MAX_FAN_PCB_TEMP_XILINX         85  //90 use middle to control fan, but use pcb temp to check to stop or not!
#else
#define MIN_PWM_PERCENT_C5              50
#define MID_PWM_PERCENT_C5              90
#define MAX_PWM_PERCENT_C5              100
#define MAX_TEMP_C5                     90
#define MAX_FAN_TEMP_C5                 75
#define MID_FAN_TEMP_C5                 65
#define MIN_FAN_TEMP_C5                 25
#define MAX_PCB_TEMP_C5                 90  //  use middle to control fan, but use pcb temp to check to stop or not!
#define MAX_FAN_PCB_TEMP_C5             85  //90 use middle to control fan, but use pcb temp to check to stop or not!

#define MIN_PWM_PERCENT_XILINX          30
#define MID_PWM_PERCENT_XILINX          70
#define MAX_PWM_PERCENT_XILINX          100
#define MAX_TEMP_XILINX                 90
#define MAX_FAN_TEMP_XILINX             75
#define MID_FAN_TEMP_XILINX             65
#define MIN_FAN_TEMP_XILINX             25
#define MAX_PCB_TEMP_XILINX             90  //  use middle to control fan, but use pcb temp to check to stop or not!
#define MAX_FAN_PCB_TEMP_XILINX         85  //90 use middle to control fan, but use pcb temp to check to stop or not!
#endif

#define TEMP_INTERVAL                   2

#define MID_PWM_ADJUST_FACTOR           ((MAX_PWM_PERCENT-MID_PWM_PERCENT)/(MAX_FAN_TEMP-MID_FAN_TEMP))
#define PWM_ADJUST_FACTOR               ((MID_PWM_PERCENT-MIN_PWM_PERCENT)/(MID_FAN_TEMP-MIN_FAN_TEMP))
#else
// below is for S9
#define PWM_T   1   // 0 local temp,  1 middle temp,  2 bottom,  as above!!!

#define MIN_FAN_NUM                     2
#define MAX_FAN_SPEED                   6000
#if PWM_T==1
#define MIN_FAN_PWM                 5
#define MAX_FAN_PWM                 100

#ifdef HIGH_TEMP_TEST_S9
#define MAX_TEMP                        135 //125     135      145          release:135
#define MAX_FAN_TEMP                    120 // 115    125      135          release:120
#define MIN_FAN_TEMP                    70  //65       75        85            release:70
#define MAX_PCB_TEMP                    105 //100       105      110        release:105
#define MAX_FAN_PCB_TEMP                95  //95       100        105        release:95
#define MIN_FAN_PCB_TEMP                45  // Attention: MAX_FAN_PCB_TEMP - MIN_FAN_PCB_TEMP=MAX_FAN_TEMP - MIN_FAN_TEMP
#else
#ifdef TWO_CHIP_TEMP_S9
#define MAX_TEMP                        135 //125     135      145          release:135
#define MAX_FAN_TEMP                    120 // 115    125      135          release:120
#define MIN_FAN_TEMP                    70  //65       75        85            release:70
#define MAX_PCB_TEMP                    105 //100       105      110        release:105
#define MAX_FAN_PCB_TEMP                95  //95       100        105        release:95
#define MIN_FAN_PCB_TEMP                45  // Attention: MAX_FAN_PCB_TEMP - MIN_FAN_PCB_TEMP=MAX_FAN_TEMP - MIN_FAN_TEMP
#else
#define MAX_TEMP                        125 //125     135      145          release:125
#define MAX_FAN_TEMP                    90  // 115    125      135          release:115
#define MIN_FAN_TEMP                    40  //65       75        85            release:65
#define MAX_PCB_TEMP                    90  //100       105      110        release:95
#define MAX_FAN_PCB_TEMP                75  //95       100        105        release:85
#define MIN_FAN_PCB_TEMP                25  // Attention: MAX_FAN_PCB_TEMP - MIN_FAN_PCB_TEMP=MAX_FAN_TEMP - MIN_FAN_TEMP
#endif
#endif
#else
#define MIN_PWM_PERCENT                 20
#define MAX_PWM_PERCENT                 100
#define MAX_TEMP                        90
#define MAX_FAN_TEMP                    75
#define MIN_FAN_TEMP                    35
#define MAX_PCB_TEMP                    90  //  use middle to control fan, but use pcb temp to check to stop or not!
#endif
#define TEMP_INTERVAL                   2
#define PWM_ADJUST_FACTOR               (MAX_FAN_PWM-MIN_FAN_PWM)/(MAX_FAN_TEMP-MIN_FAN_TEMP)
#endif

#ifdef HIGH_TEMP_TEST_S9
#define MIN_TEMP_CONTINUE_DOWN_FAN      110 // release: 90
#define MAX_TEMP_NEED_UP_FANSTEP        120 // release: 100   if temp is higher than 100, then we need make fan much faster
#else
#define MIN_TEMP_CONTINUE_DOWN_FAN      80  // release: 90
#define MAX_TEMP_NEED_UP_FANSTEP        85  // release: 100   if temp is higher than 100, then we need make fan much faster
#endif

#define PWM_SCALE                       50  //50:   1M=1us,      20KHz??
//25:   40KHz

#define PWM_ADJ_SCALE                   9/10
//use for hash test
#define TEST_DHASH 0
#define DEVICE_DIFF 8
//use for status check

#define MAX_TEMPCHIP_NUM        8   // support 8 chip has temp

#define MIN_FREQ                4   // 8:300M   6:250M      4:200M
#define MAX_FREQ                113 //850M
#define MAX_SW_TEMP_OFFSET      -15
#define BMMINER_VERSION         3   // 3 for auto freq,  1 or 2 for normal ( the old version is 0)

// for c5, bmminer will detect board type and use it.
#define RED_LED_DEV_C5 "/sys/class/leds/hps_led2/brightness"
#define GREEN_LED_DEV_C5 "/sys/class/leds/hps_led0/brightness"

// for xilinx, bmminer will detect board type and use it.
#define RED_LED_DEV_XILINX "/sys/class/gpio/gpio37/value"
#define GREEN_LED_DEV_XILINX "/sys/class/gpio/gpio38/value"

// S9 , T9,  R4    PIC PROGRAM
#define PIC_PROGRAM "/etc/config/hash_s8_app.txt"

// T9+  PIC PROGRAM
#define DSPIC33EP16GS202_PIC_PROGRAM "/etc/config/dsPIC33EP16GS202_app.txt"


#define TIMESLICE 60

#if defined(USE_ANTMINER_T9)
#define IIC_ADDR_HIGH_4_BIT                 (0x04 << 20)
#define EEPROM_ADDR_HIGH_4_BIT              (0x0A << 20)
#define IIC_SELECT(x)                       ((x & 0x03) << 26)

unsigned int get_iic();
unsigned char set_iic(unsigned int data);
unsigned char T9_plus_write_pic_iic(bool read, bool reg_addr_valid, unsigned char reg_addr, unsigned char which_iic, unsigned char data);
int dsPIC33EP16GS202_jump_to_app_from_loader(unsigned char which_iic);
#else
#define IIC_ADDR_HIGH_4_BIT                 (0x0A << 20)
#endif


struct init_config
{
    u_int8_t     token_type;
    u_int8_t     version;
    u_int16_t    length;
    u_int8_t     reset                   :1;
    u_int8_t     fan_eft                 :1;
    u_int8_t     timeout_eft             :1;
    u_int8_t     frequency_eft           :1;
    u_int8_t     voltage_eft             :1;
    u_int8_t     chain_check_time_eft    :1;
    u_int8_t     chip_config_eft         :1;
    u_int8_t     hw_error_eft            :1;
    u_int8_t     beeper_ctrl             :1;
    u_int8_t     temp_ctrl               :1;
    u_int8_t     chain_freq_eft          :1;
    u_int8_t     reserved1               :5;
    u_int8_t     reserved2[2];
    u_int8_t     chain_num;
    u_int8_t     asic_num;
    u_int8_t     fan_pwm_percent;
    u_int8_t     temperature;
    u_int16_t    frequency;
    u_int8_t     voltage[2];
    u_int8_t     chain_check_time_integer;
    u_int8_t     chain_check_time_fractions;
    u_int8_t     timeout_data_integer;
    u_int8_t     timeout_data_fractions;
    u_int32_t    reg_data;
    u_int8_t     chip_address;
    u_int8_t     reg_address;
    u_int16_t    chain_min_freq;
    u_int16_t    chain_max_freq;
	//u_int16_t    crc;
} __attribute__((packed, aligned(4)));

//struct bitmain_c5_info
//{
//    u_int8_t     data_type;
//    u_int8_t     version;
//    u_int16_t    length;
//    u_int8_t     chip_value_eft  :1;
//    u_int8_t     reserved1       :7;
//    u_int8_t     chain_num;
//    u_int16_t    reserved2;
//    u_int8_t     fan_num;
//    u_int8_t     temp_num;
//    u_int8_t     reserved3[2];
//    u_int32_t    fan_exist;
//    u_int32_t    temp_exist;
//    u_int16_t    diff;
//    u_int16_t    reserved4;
//    u_int32_t    reg_value;
//    u_int32_t    chain_asic_exist[BITMAIN_MAX_CHAIN_NUM][BITMAIN_DEFAULT_ASIC_NUM/32];
//    u_int32_t    chain_asic_status[BITMAIN_MAX_CHAIN_NUM][BITMAIN_DEFAULT_ASIC_NUM/32];
//    u_int8_t     chain_asic_num[BITMAIN_MAX_CHAIN_NUM];
//    u_int8_t     temp[BITMAIN_MAX_CHAIN_NUM];
//    u_int8_t     fan_speed_value[BITMAIN_MAX_FAN_NUM];
//    u_int16_t    freq[BITMAIN_MAX_CHAIN_NUM];
//    pthread_t read_nonce_thr;

//    struct init_config c5_config;
//	//u_int16_t    crc;
//} __attribute__((packed, aligned(4)));

struct part_of_job
{
    u_int8_t     token_type;             // buf[0]
    u_int8_t     version;
    u_int16_t    reserved;
    u_int32_t    length;                 // buf[1]
    u_int8_t     new_block       :1;
    u_int8_t     asic_diff_valid :1;
    u_int8_t     reserved1       :6;
    u_int8_t     asic_diff;
    u_int8_t     reserved2[1];
    u_int32_t    job_id;                 // buf[3]
    u_int32_t    bbversion;              // buf[4]
    u_int8_t     prev_hash[32];          // buf[5] - buf[12]
    u_int32_t    ntime;                  // buf[13]
    u_int32_t    nbit;                   // buf[14]
    u_int16_t    coinbase_len;           // buf[15]
    u_int16_t    nonce2_offset;
    u_int16_t    nonce2_bytes_num;       // 4 or 8 bytes // buf[16]
    u_int16_t    merkles_num;
	u_int64_t     nonce2_start_value; //nonce2 start calculate value. // buf[17] - buf[18]
};

typedef struct
{
	struct thr_info *mining_control_thr;
    unsigned int    *current_job_start_address;
    unsigned int    pwm_value;
    unsigned char   chain_exist[BITMAIN_MAX_CHAIN_NUM];
	u_int8_t        chain_enabled[BITMAIN_MAX_CHAIN_NUM];
    unsigned int    timeout;
    unsigned int    temp_sensor_map;
    unsigned int    nonce_error;
    unsigned int    chain_asic_exist[BITMAIN_MAX_CHAIN_NUM][8];
    unsigned int    chain_asic_status[BITMAIN_MAX_CHAIN_NUM][8];
    signed char     chain_asic_temp_num[BITMAIN_MAX_CHAIN_NUM]; // the real number of temp chip
    unsigned char   TempChipType[BITMAIN_MAX_CHAIN_NUM][MAX_TEMPCHIP_NUM];
    unsigned char   TempChipAddr[BITMAIN_MAX_CHAIN_NUM][MAX_TEMPCHIP_NUM];  // each temp chip's address: chip index*4, index start from 0
    int16_t         chain_asic_temp[BITMAIN_MAX_CHAIN_NUM][MAX_TEMPCHIP_NUM][TEMP_POS_NUM]; // 4 kinds of temp
    int16_t         chain_asic_maxtemp[BITMAIN_MAX_CHAIN_NUM][TEMP_POS_NUM];    // 4 kinds of temp
    int16_t         chain_asic_mintemp[BITMAIN_MAX_CHAIN_NUM][TEMP_POS_NUM];    // 4 kinds of temp
    //int8_t          chain_asic_iic[CHAIN_ASIC_NUM];
    u_int32_t        chain_hw[BITMAIN_MAX_CHAIN_NUM];
	u_int64_t        chain_asic_nonce[BITMAIN_MAX_CHAIN_NUM][BITMAIN_DEFAULT_ASIC_NUM];
    char            chain_asic_status_string[BITMAIN_MAX_CHAIN_NUM][BITMAIN_DEFAULT_ASIC_NUM+8];

    unsigned char   fan_exist[BITMAIN_MAX_FAN_NUM];
    unsigned int    fan_speed_value[BITMAIN_MAX_FAN_NUM];
	int             temp[BITMAIN_MAX_CHAIN_NUM];
	u_int8_t        chain_asic_num[BITMAIN_MAX_CHAIN_NUM];
    unsigned char   check_bit;
    unsigned char   pwm_percent;
    unsigned char   chain_num;
    unsigned char   fan_num;
    unsigned char   temp_num;
    unsigned int    fan_speed_top1;
    int             temp_top1[TEMP_POS_NUM];
    int             temp_low1[TEMP_POS_NUM];
    int             temp_top1_last;
    unsigned char   corenum;
    unsigned char   addrInterval;
    unsigned char   max_asic_num_in_one_chain;
    unsigned char   baud;
    unsigned char   diff;
    u_int8_t         fan_eft;
    u_int8_t         fan_pwm;

    unsigned short int  frequency;
    char frequency_t[10];
    unsigned short int  freq[BITMAIN_MAX_CHAIN_NUM];
} __attribute__((packed, aligned(4))) device_parameters_t;

struct nonce_content
{
	u_int32_t    job_id;
	u_int32_t    work_id;
	u_int32_t    header_version;
	u_int64_t    nonce2;
	u_int32_t    nonce3;
	u_int32_t    chain_id;
	u_int8_t     midstate[MIDSTATE_LEN];
} __attribute__((packed, aligned(4)));

struct nonce_buf
{
    unsigned int p_wr;
    unsigned int p_rd;
    unsigned int nonce_num;
    struct nonce_content nonce_buffer[MAX_NONCE_NUMBER_IN_FIFO];
} __attribute__((packed, aligned(4)));

struct reg_content
{
    unsigned int reg_value;
    unsigned char crc;
    unsigned char chain_number;
} __attribute__((packed, aligned(4)));

struct reg_buf
{
    unsigned int p_wr;
    unsigned int p_rd;
    unsigned int reg_value_num;
    struct reg_content reg_buffer[MAX_NONCE_NUMBER_IN_FIFO];
} __attribute__((packed, aligned(4)));

struct freq_pll
{
    const char *freq;
    unsigned int fildiv1;
    unsigned int fildiv2;
    unsigned int vilpll;
    unsigned int freq_val;
};

#define Swap32(l) (((l) >> 24) | (((l) & 0x00ff0000) >> 8) | (((l) & 0x0000ff00) << 8) | ((l) << 24))

struct vil_work
{
    u_int8_t type;       // Bit[7:5]: Type,fixed 0x01.   Bit[4:0]:Reserved
    u_int8_t length;     // data length, from Byte0 to the end.
    u_int8_t wc_base;    // Bit[7]: Reserved.    Bit[6:0]: Work count base, muti-Midstate, each Midstate corresponding work count increase one by one.
    u_int8_t mid_num;    // Bit[7:3]: Reserved   Bit[2:0]: MSN, midstate num,now support 1,2,4.
    //u_int32_t sno;       // SPAT mode??Start Nonce Number    Normal mode??Reserved.
    u_int8_t midstate[32];
    u_int8_t data2[12];
};

struct vil_work_1387
{
    u_int8_t work_type;
    u_int8_t chain_id;
    u_int8_t reserved1[2];
    u_int32_t work_count;
    u_int8_t data[12];
    u_int8_t midstate[32];
};

extern bool opt_bitmain_fan_ctrl;
extern bool opt_bitmain_new_cmd_type_vil;
extern bool opt_pre_heat;
extern int opt_bitmain_fan_pwm;
extern int opt_bitmain_freq;
extern int opt_bitmain_voltage;
#if defined(ENABLE_ASIC_BOOST)
extern int opt_multi_version;
#endif
extern char opt_enabled_boards[BITMAIN_MAX_CHAIN_NUM];
extern int fpga_version;
extern bool opt_ignore_fan_errors;

extern device_parameters_t *device;
///////////// below they must be changed at same time!!!! ///////////////////////
typedef enum
{
    TEMP_BOTTOM=0,    // 0 is bottom ,  1  is middle
    TEMP_MIDDLE
} Temp_Type_E;
////////////////////////////////////////////////////////////////////////////

int get_neirbour_vol(int vol, int step);
int get_neirbour_freq(int freq, int step);
int get_pll_index(int freq);
void reinit_fan_params();
void bitmain_s9_reinit();
void doS9Reinit();
void SetHardReset(bool);
void thread_func();
bool CheckInitFinished();
void software_set_address_onChain(int chainIndex);
void SetVoltageForChains();
bool bitmain_reinit_w_lock(bool hard, int chain_bits);
void set_reset_hashboard(int chainIndex, int resetBit);
void set_reset_allhashboard(int resetBit);
void set_baud_with_addr(unsigned char bauddiv, int mode, unsigned char chip_addr, int chain, int iic, int open_core, int bottom_or_mid);
void set_PWM(unsigned char pwm_percent);
unsigned int wait_iic_ok(unsigned int chip_addr, unsigned int chain);
void read_temp(unsigned char asic, unsigned reg, unsigned char data, unsigned char write, unsigned char chip_addr, int chain);

#if defined(USE_ANTMINER_T9)
int chainIDFunction(int);
#endif
bool set_global_id(const unsigned char *id, unsigned int idLen, int chainIndex);
bool get_global_id(unsigned char *id, unsigned int idLen, int chainIndex);
    
#endif /* DRIVER_BITMAIN_C5_H */
