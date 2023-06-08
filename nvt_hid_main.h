// SPDX-License-Identifier: Apache-2.0

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/hidraw.h>
#include <dirent.h>
#include <pthread.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

#include "nvt_hid_flash_gcm.h"
#include "nvt_hid_mem_map.h"

#define BUILD_VERSION "1.4.8"
#define DEBUG_OPTION 1 // 0 = Always turn on debug

// Config
#define FLASH_PAGE_SIZE 256
#define NVT_TRANSFER_LEN 256
#define DEFAULT_REPORT_ID_ENG 0x0B

#ifndef TRIM_TYPE_MAX
#define TRIM_TYPE_MAX 1
#endif

#define BUS_I2C 0x18

#define BUS_SPI 0x1C
#define CONTENT_ID_FOR_READ 0x0A
#define CONTENT_ID_FOR_NO_INT_QUIRK 0xFF
#define SPI_HID_INPUT_HEADER_VERSION 0x03
#define SINGLE_FRAGMENT_SUPPORT 0x01
#define LAST_FRAG_NO_CONTENT 0x40
#define SYNC_CONST 0x5A
#define OPCODE_READ 0x0B
#define OPCODE_WRITE 0x02

// Functions
#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

#define NVT_ERR(fmt, ...)                                              \
	fprintf(stderr, "\n#Error# {%s} %s +%d : " fmt "\n", __func__, \
		__FILE__, __LINE__, ##__VA_ARGS__)

#define NVT_ERR_HEX(data, len)                                \
	do {                                                  \
		for (int i = 0; i < len; i++) {               \
			if (i % 16 == 0)                      \
				fprintf(stderr, "#Error#: "); \
			fprintf(stderr, "%02X", *(data + i)); \
			if ((i + 1) % 16 == 0)                \
				fprintf(stderr, "\n");        \
			else                                  \
				fprintf(stderr, " ");         \
		}                                             \
		fprintf(stderr, "\n");                        \
	} while (0)

#define NVT_LOG(fmt, ...) printf("[Info ] " fmt "\n", ##__VA_ARGS__)

#define NVT_LOG_HEX(data, len)                       \
	do {                                         \
		for (int i = 0; i < len; i++) {      \
			printf("%02X", *(data + i)); \
			if ((i + 1) % 16 == 0)       \
				printf("\n");        \
			else                         \
				printf(" ");         \
		}                                    \
		if (len % 16 != 0)                   \
			printf("\n");                \
	} while (0)

#define NVT_DBG(fmt, ...)                                                   \
	do {                                                                \
		if (term_debug)                                             \
			printf("[Debug] {%s} %s +%d : " fmt "\n", __func__, \
			       __FILE__, __LINE__, ##__VA_ARGS__);          \
	} while (0)

#define NVT_DBG_HEX(data, len)                               \
	do {                                                 \
		if (term_debug) {                            \
			for (int i = 0; i < len; i++) {      \
				printf("%02X", *(data + i)); \
				if ((i + 1) % 16 == 0)       \
					printf("\n");        \
				else                         \
					printf(" ");         \
			}                                    \
			if (len % 16 != 0)                   \
				printf("\n");                \
		}                                            \
	} while (0)

#define NVT_CLN_HEX(data, len)                       \
	do {                                         \
		for (int i = 0; i < len; i++) {      \
			printf("%02X", *(data + i)); \
			if ((i + 1) % 16 == 0)       \
				printf("\n");        \
			else                         \
				printf(" ");         \
		}                                    \
		if (len % 16 != 0)                   \
			printf("\n");                \
	} while (0)

#define _u32_to_n_u8(u32, n) (((u32) >> (8 * n)) & 0xff)
#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)

// NAME or PATH
#define NODE_PATH_HIDRAW_RECOVERY "/dev/nvt_ts_hidraw_flash" // recovery node
#define NODE_PATH_HIDRAW_RECOVERY_SPI \
	"/dev/nvt_ts_hidraw_flash_spi" // recovery node spi
#define NODE_PATH_I2C_DRIVERS "/sys/bus/i2c/drivers"
#define NODE_PATH_DEV "/dev"
#define NVT_MP_CRITERIA_FILENAME "nvt_ts_mp_criteria.txt"
#define UNBIND "unbind"
#define BIND "bind"
#define I2C_BUS_STR "0018"
#define NVT_VID_STR "0603"
#define CHIP_ID_FILE_NAME "nvt_chip_id_file" // depend on time
#define CHIP_ID_FILE_NAME_FORCE "nvt_chip_id_file_force" // not depend on time

// Numbers
#define NVT_VID_NUM 0x0603
#define SIZE_4KB (1024 * 4)
#define SIZE_64KB (1024 * 64)
#define SIZE_320KB (1024 * 320)
#define BLOCK_64KB_NUM 4
#define FLASH_SECTOR_SIZE SIZE_4KB
#define MAX_BIN_SIZE SIZE_320KB
#define MAX_NODE_NAME_SIZE 64
#define MAX_NODE_PATH_SIZE 512
#define MAX_SYSTEM_CMD_SIZE 512
#define I2C_DEBOUNCE_DELAY_MS 5
#define JUMP_KEY_SIZE 16
#define HID_SPI_PACKET_LEN 5

int32_t fd_node;
uint8_t recovery_node;
struct nvt_ts_data *ts;
struct nvt_ts_mem_map memory_map;
uint8_t term_debug;
uint8_t skip_zero_addr_chk;
uint32_t bus_type;
uint8_t chip_id_file_exist_and_correct;
//hm
uint32_t hm_size;
uint8_t *hm_buf;

struct nvt_ts_data {
	uint16_t ic;
	uint8_t report_id;
	uint8_t fw_ver;
	uint8_t touch_x_num;
	uint8_t touch_y_num;
	uint8_t pen_x_num;
	uint8_t pen_y_num;
	uint8_t pen_x_num_gang;
	uint8_t pen_y_num_gang;
	uint16_t touch_abs_x_max;
	uint16_t touch_abs_y_max;
	const struct nvt_ts_mem_map *mmap;
	const struct nvt_ts_flash_map *fmap;
	uint8_t *hid_i2c_prefix_pkt;
	uint8_t flash_mid;
	uint16_t flash_did;
	uint16_t flash_pid;
	const flash_info_t *match_finfo;
	uint8_t flash_prog_data_cmd;
	uint8_t flash_read_data_cmd;
	uint8_t flash_read_pem_byte_len;
	uint8_t flash_read_dummy_byte_len;
	bool need_lock_flash;
	uint8_t idx;
};

typedef struct gcm_transfer {
	uint8_t flash_cmd;
	uint32_t flash_addr;
	uint16_t flash_checksum;
	uint8_t flash_addr_len;
	uint8_t pem_byte_len;
	uint8_t dummy_byte_len;
	uint8_t *tx_buf;
	uint16_t tx_len;
	uint8_t *rx_buf;
	uint16_t rx_len;
} gcm_xfer_t;

enum {
	RESET_STATE_INIT = 0xA0,
	RESET_STATE_REK_BASELINE,
	RESET_STATE_REK_FINISH,
	RESET_STATE_NORMAL_RUN,
	RESET_STATE_MAX = 0xAF
};

enum {
	FLASH_FORCE,
	FLASH_NORMAL,
	//FLASH_SHORT, not needed just use force
};

int32_t confirm(void);
void msleep(int32_t delay);
void memset16(uint8_t *buf, uint16_t val, uint16_t count);
int32_t ctp_hid_read(uint32_t addr, uint8_t *data, uint16_t len);
int32_t ctp_hid_write(uint32_t addr, uint8_t *data, uint16_t len);
void nvt_bootloader_reset(void);
void nvt_sw_idle(void);
int32_t nvt_get_fw_ver_and_info(uint8_t show);
int32_t nvt_get_fw_ver(void);
int32_t nvt_read_reg_bits(nvt_ts_reg_t reg, uint8_t *val);
int32_t nvt_write_reg_bits(nvt_ts_reg_t reg, uint8_t val);
int32_t nvt_user_read_reg(char *main_argv[], uint8_t icms, uint8_t show);
int32_t nvt_user_write_reg(char *main_argv[], uint8_t icms);
void nvt_stop_crc_reboot(void);
int32_t nvt_ts_detect_chip_ver_trim(uint8_t user_rw);
int32_t user_dump_flash_gcm(char *main_argv[]);
int32_t dump_flash_gcm_pid(void);
int32_t update_firmware(char *path, uint8_t flash_option);
int32_t nvt_check_fw_reset_state(uint8_t state);
int32_t nvt_check_fw_status(void);
int32_t nvt_selftest_open(uint8_t pen_support, char *criteria_path);
int32_t find_i2c_hid_dev(char *i2c_hid_driver_path, char *dev_i2c_bus_addr);
int32_t rebind_i2c_hid_driver(void);
int32_t find_generic_hidraw_path(char *path);
int32_t write_prefix_packet(void);
int32_t set_recovery_pkt(void);
int32_t check_crc_done(void);
int32_t User_compare_bin_and_flash_sector_checksum(char *path);
int32_t nvt_user_set_feature_report(uint16_t len, char *main_argv[]);
int32_t nvt_user_get_feature_report(uint16_t len, char *main_argv[]);
int32_t nvt_block_jump(void);
void nvt_unblock_jump(void);
int32_t check_if_in_bootload(void);
