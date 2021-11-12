#ifndef     _LINUX_NVT_NT36XXX_H
#define     _LINUX_NVT_NT36XXX_H
/*
 * include header file
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>// uint8_t
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include <linux/input.h>// for struct input_event event;

/*
 * variable define
 */

//---I2C driver info.---
#define I2C_BLDR_Address       (0x01)
#define I2C_FW_Address         (0x01)
#define I2C_HW_Address         (0x62)
#define _HW_ID_62              (0x62)
#define _HW_ID_70              (0x70)
//[20170323,jx]Sync with 772& 206
#define PRJ_ID                 (1024*  4)	//prjID
#define FW_116k                (1024*116)	//NT36772
#define FW_124k                (1024*124)	//NT11206
#define FW_128k                (1024*128)
#define FW_256k                (1024*256)	//NT36675
#define FW_512k                (1024*512)
#define _NO_PD_ENTER_          (0x61)	//(0x60)
#define _NO_PD_LEAVE_          (0x62)	//(0x70)
#define NORMAL_MODE            (0x00)
#define TEST_MODE_1            (0x21)	//before algo.
#define TEST_MODE_2            (0x22)	//after algo.
#define MP_MODE_CC             (0x41)
#define FREQ_HOP_DISABLE       (0x66)
#define FREQ_HOP_ENABLE        (0x65)
#define HANDSHAKING_HOST_READY (0xBB)
#define XDATA_SECTOR_SIZE      (256)
#define _fopenMode_NewFile     (0x00)
#define _fopenMode_Append      (0x01)
#define _MSG_Hide              (0x00)
#define _MSG_Show              (0x03)

#define SIZE_4KB 				4096
#define FLASH_SECTOR_SIZE 		SIZE_4KB
#define SIZE_64KB 				65536
#define BLOCK_64KB_NUM 			4
#define FLASH_PAGE_SIZE         256
#define FW_BIN_VER_OFFSET (fw_need_write_size - SIZE_4KB)
#define FW_BIN_VER_BAR_OFFSET (FW_BIN_VER_OFFSET + 1)
#define NVT_FLASH_END_FLAG_LEN 3
#define NVT_FLASH_END_FLAG_ADDR (fw_need_write_size - NVT_FLASH_END_FLAG_LEN)
#define FW_BIN_TYPE_OFFSET		(FW_BIN_VER_OFFSET + 0x0D)

//---Macro define
#define min(a, b)                 (a < b ? a : b)
#define Max(a, b)                 (a > b ? a : b)
#define _i16From2Char(HiB, LoB)   ( (short)( ( (uint8_t)HiB ) << 8 | ( (uint8_t)LoB ) ) )
#define _get8Bit_fromU32(u32, n)  ( ( u32 >> (8*n) ) & 0xff )
#define _getSPIL7Bit_fromU32(u32) (u32 & 0x7F)

#define likely(x) __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)
#define mdelay(x) msleep(x)

#define NVT_ERR(msg, args...) printf("%s %d: " msg, __func__, __LINE__, ##args)
#define NVT_LOG(msg, args...) printf("%s %d: " msg, __func__, __LINE__, ##args)

#define TOUCH_DEFAULT_MAX_WIDTH  1080
#define TOUCH_DEFAULT_MAX_HEIGHT 1920

typedef unsigned char   uint8_t;
typedef unsigned short  uint16_t;
typedef unsigned int  	uint32_t;

typedef enum {
	RESET_STATE_INIT = 0xA0,// IC reset
	RESET_STATE_REK,		// ReK baseline
	RESET_STATE_REK_FINISH,	// baseline is ready
	RESET_STATE_NORMAL_RUN	// normal run
} RST_COMPLETE_STATE;

typedef enum {
	EVENT_MAP_HOST_CMD                      = 0x50,
	EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE   = 0x51,
	EVENT_MAP_RESET_COMPLETE                = 0x60,
	EVENT_MAP_FWINFO                        = 0x78,
} I2C_EVENT_MAP;

typedef enum {
	enFW_206_124K           = 1,//enPre_124K
	enFW_772_116K           = 2,
	en4K_0x1E000            = 3,//[30]
	en4K_0x1F000            = 4,//[31]
	enLog_1st128K           = 5,//[ 0~31]
	//------
	enLog_129to256K         = 6,//[32~63]
	enLog_256K              = 7,//[00~63]
	enLog_512K				= 8,//[00~127],ultra_TestOnly

}Sector_t;

typedef enum {
	eILM,	//ILM
	eDLM,	//DLM,
	eHWREG,	//HwRegister
} ENUM_DUMP_ZONE;

typedef enum {
	enNoCheck    = 0x00,
	enDoCheck    = 0x01,
	enFlash_WriteZeroBeforeErase    = 0x02,
}ParameterCheck_t;

typedef enum {
	enShow_Off    	= 0x00,
	enShow_On,
	enShow_Parsing,
}DataShowType;

typedef enum {
	IC_Master       = 0x00,
	IC_Slave_1st    = 0x01,
	IC_Slave_2nd    = 0x02,
} ICMSType;

typedef enum {
	SPIDMA_NOSUPPORT  = 0x00,
	SPIDMA_V1         = 0x01,
	SPIDMA_V2         = 0x02,
} SPIDMAType;

int fdev;
int fdev_hid;
char node_path[32];

//:~From Driver Code---
struct cmdArray60Byte {
	uint8_t u8ID;
	uint8_t u8Len;
	uint8_t u8Data[64];
	uint32_t u32Addr;
};

struct nvt_ts_data {
	uint8_t fw_ver;
	uint8_t x_num;
	uint8_t y_num;
	uint8_t button_num;
	uint16_t abs_x_max;
	uint16_t abs_y_max;
	uint8_t hw_crc;
	SPIDMAType spidma;
	uint32_t SWRST_N8_ADDR;
	uint32_t SPI_RD_FAST_ADDR;
	char strA_nvtChipName[20];
	const struct nvt_ts_mem_map *mmap;
	const struct nvt_ts_mem_map2nd *mmap2;
	const struct nvt_ts_ram_map *rmap;
	const struct nvt_ts_f2c_reg *f2creg;
	const struct nvt_ts_spi_dma_reg *spidmareg;
	uint16_t query_config_ver;
	uint16_t nvt_pid;
	uint8_t flash_mid;
	uint16_t flash_did; /* 2 bytes did read by 9Fh cmd */
};

union {
	uint8_t		u8[4];	//Unit_min
	int8_t  	i8[4];
	uint16_t	u16[2];
	int16_t  	i16[2];
	uint32_t 	u32;	//Unit_MAX
	int32_t  	i32;
}cell;

typedef enum {
	Unit_ERR=0,
	Unit_U8,
	Unit_I8,
	Unit_U16,
	Unit_I16,
	Unit_U32,
	Unit_I32
} uType;


/*
 * extern variable
 */
struct nvt_ts_data *ts;
uint8_t xdata_tmp[4096];
int32_t xdata[2048];
uint32_t G_START_ADDR;
int iFileSize;
uint8_t BUFFER_DATA[FW_512k + 1];
size_t fw_need_write_size ;


/*
 * extern functions
 */
int32_t CTP_NVT_READ(const char* caller_Fun, uint16_t I2C_addr, uint8_t *buf, uint16_t len);
int32_t CTP_NVT_WRITE(const char* caller_Fun, uint16_t I2C_addr, uint8_t *buf, uint16_t len);
void msleep(int32_t delay);
void nvt_ts_chip_mmap(void);
void print_cmd_arg_err_msg(int32_t argc, char* main_argv[], ...);

int32_t nvt_get_fw_ver(void);
int32_t nvt_get_fw_info(void);
void nvt_get_trim_id(void);
int32_t nvt_read_slave_addr(uint32_t addr, uint8_t *buf, uint16_t u8stageLEN);
int32_t nvt_write_slave_addr(uint32_t addr, uint8_t *data, uint16_t u16Len);

int g_dbg;
#endif	/* _LINUX_NVT_NT36XXX_H */
