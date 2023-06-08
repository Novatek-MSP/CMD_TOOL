// SPDX-License-Identifier: Apache-2.0

#define FLASH_DID_ALL 0xFFFF /* all device id of same manufacture */

/* flash manufacturer idenfication */
typedef enum {
	FLASH_MFR_UNKNOWN = 0x00,
	FLASH_MFR_ESMT = 0x1C,
	FLASH_MFR_PUYA = 0x85, /* Puya */
	FLASH_MFR_MACRONIX = 0xC2, /* Macronix */
	FLASH_MFR_GIGADEVICE = 0xC8, /* GigaDevice */
	FLASH_MFR_WINBOND = 0xEF, /* Winbond */
	FLASH_MFR_MAX = 0xFF
} flash_mfr_t;

// find "QE" or "Status Register"
typedef enum {
	QEB_POS_UNKNOWN = 0,
	QEB_POS_SR_1B, /* QE bit in SR 1st byte */
	QEB_POS_OTHER, /* QE bit not in SR 1st byte */
	QEB_POS_MAX
} qeb_pos_t;

// search "write status register" or "wrsr"
typedef enum {
	FLASH_WRSR_METHOD_UNKNOWN = 0,
	WRSR_01H1BYTE, /* 01H (S7-S0) */
	WRSR_01H2BYTE, /* 01H (S7-S0) (S15-S8) */
	FLASH_WRSR_METHOD_MAX
} flash_wrsr_method_t;

typedef struct flash_qeb_info {
	qeb_pos_t qeb_pos; /* QE bit position type, ex. in SR 1st/2nd byte, etc. */
	uint8_t qeb_order; /* in which bit of that byte, start from bit 0 */
} flash_qeb_info_t;

// find "03h" or "Read Data Bytes"
typedef enum {
	FLASH_READ_METHOD_UNKNOWN = 0,
	SISO_0x03,
	SISO_0x0B,
	SIQO_0x6B,
	QIQO_0xEB,
	FLASH_READ_METHOD_MAX
} flash_read_method_t;

// find "page program"
typedef enum {
	FLASH_PROG_METHOD_UNKNOWN = 0,
	SPP_0x02, /* SingalPageProgram_0x02 */
	QPP_0x32, /* QuadPageProgram_0x32 */
	QPP_0x38, /* QuadPageProgram_0x38 */
	FLASH_PROG_METHOD_MAX
} flash_prog_method_t;

typedef enum {
	FLASH_LOCK_METHOD_UNKNOWN,
	FLASH_LOCK_METHOD_SW_BP_ALL,
	FLASH_LOCK_METHOD_MAX,
} flash_lock_method_t;

typedef struct flash_info {
	flash_mfr_t mid; /* manufacturer identification */
	uint16_t did; /* 2 bytes device identification read by 9fh cmd*/
	flash_qeb_info_t qeb_info;
	flash_read_method_t rd_method; /* flash read method */
	flash_prog_method_t prog_method; /* flash program method */
	flash_wrsr_method_t wrsr_method; /* write status register method */
	//find "rdsr" or "Read Status Register"
	uint8_t rdsr1_cmd; /* cmd for read status register-1 (S15-S8) */
	flash_lock_method_t lock_method; /* block protect position */
	uint8_t sr_bp_bits_all; /* BP all protect bits setting in SR for FLASH_LOCK_METHOD_SW_BP_ALL */
} flash_info_t;

static const flash_info_t flash_info_table[] = {
	/* Please put flash info items which will use quad mode and is verified before those with "did = FLASH_DID_ALL"! */
	{ .mid = FLASH_MFR_GIGADEVICE,
	  .did = 0x4013,
	  .qeb_info = { .qeb_pos = QEB_POS_OTHER, .qeb_order = 0xFF },
	  .rd_method = SISO_0x03,
	  .prog_method = SPP_0x02,
	  .wrsr_method = WRSR_01H2BYTE,
	  .rdsr1_cmd = 0x35 },
	{ .mid = FLASH_MFR_GIGADEVICE,
	  .did = 0x6012,
	  .qeb_info = { .qeb_pos = QEB_POS_OTHER, .qeb_order = 0xFF },
	  .rd_method = SISO_0x03,
	  .prog_method = SPP_0x02,
	  .wrsr_method = WRSR_01H1BYTE,
	  .rdsr1_cmd = 0xFF },
	{ .mid = FLASH_MFR_GIGADEVICE,
	  .did = 0x6016,
	  .qeb_info = { .qeb_pos = QEB_POS_OTHER, .qeb_order = 0xFF },
	  .rd_method = SISO_0x03,
	  .prog_method = SPP_0x02,
	  .wrsr_method = WRSR_01H2BYTE,
	  .rdsr1_cmd = 0x35 },
	{ .mid = FLASH_MFR_PUYA,
	  .did = 0x6015,
	  .qeb_info = { .qeb_pos = QEB_POS_OTHER, .qeb_order = 0xFF },
	  .rd_method = SISO_0x03,
	  .prog_method = SPP_0x02,
	  .wrsr_method = WRSR_01H2BYTE,
	  .rdsr1_cmd = 0x35 },
	{ .mid = FLASH_MFR_WINBOND,
	  .did = 0x3012,
	  .qeb_info = { .qeb_pos = QEB_POS_OTHER, .qeb_order = 0xFF },
	  .rd_method = SISO_0x03,
	  .prog_method = SPP_0x02,
	  .wrsr_method = WRSR_01H1BYTE,
	  .rdsr1_cmd = 0xFF },
	{ .mid = FLASH_MFR_WINBOND,
	  .did = 0x6016,
	  .qeb_info = { .qeb_pos = QEB_POS_OTHER, .qeb_order = 0xFF },
	  .rd_method = SISO_0x03,
	  .prog_method = SPP_0x02,
	  .wrsr_method = WRSR_01H1BYTE,
	  .rdsr1_cmd = 0x35 },
	{ .mid = FLASH_MFR_MACRONIX,
	  .did = 0x2813,
	  .qeb_info = { .qeb_pos = QEB_POS_SR_1B, .qeb_order = 6 },
	  .rd_method = SISO_0x03,
	  .prog_method = SPP_0x02,
	  .wrsr_method = WRSR_01H1BYTE,
	  .rdsr1_cmd = 0xFF },
	{ .mid = FLASH_MFR_WINBOND,
	  .did = 0x6012,
	  .qeb_info = { .qeb_pos = QEB_POS_OTHER, .qeb_order = 0xFF },
	  .rd_method = SISO_0x03,
	  .prog_method = SPP_0x02,
	  .wrsr_method = WRSR_01H1BYTE,
	  .rdsr1_cmd = 0x35 },

	/* Please note that flash info item which have the same info for all flash did of the same mid should be place after this line!
	 * Please make sure that the info item have the same info for all flash did of the same mid then you can add it in the section!
	 */

	/* Please note that the following flash info item should be keep at the last one! Do not move it! */
	{ .mid = FLASH_MFR_UNKNOWN,
	  .did = FLASH_DID_ALL,
	  .qeb_info = { .qeb_pos = QEB_POS_UNKNOWN, .qeb_order = 0xFF },
	  .rd_method = SISO_0x03,
	  .prog_method = SPP_0x02,
	  .wrsr_method = FLASH_WRSR_METHOD_UNKNOWN,
	  .rdsr1_cmd = 0xFF }
};
