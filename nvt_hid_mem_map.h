// SPDX-License-Identifier: Apache-2.0

#define NVT_ID_BYTE_MAX 6
#define HID_I2C_PREFIX_PKT_LENGTH 5

typedef struct nvt_ts_reg {
	uint32_t addr;
	uint8_t mask;
} nvt_ts_reg_t;

struct nvt_ts_mem_map {
	nvt_ts_reg_t pp4io_en_reg;
	nvt_ts_reg_t bld_rd_addr_sel_reg;
	nvt_ts_reg_t bld_rd_io_sel_reg;
	uint32_t chip_ver_trim_addr;
	uint32_t swrst_sif_addr;
	uint32_t sw_pd_idle_addr;
	uint32_t idle_int_en_addr;
	uint32_t event_buf_cmd_addr;
	uint32_t event_buf_hs_sub_cmd_addr;
	uint32_t event_buf_reset_state_addr;
	uint32_t event_map_fwinfo_addr;
	uint32_t read_flash_checksum_addr;
	uint32_t rw_flash_data_addr;
	uint32_t bld_spe_pups_addr;
	// only specify it when both singel and casc map exist
	uint32_t enb_casc_addr;
	//hid
	uint32_t block_jump_addr;
	//hid i2c
	uint32_t hid_i2c_eng_addr;
	uint32_t hid_i2c_eng_get_rpt_length_addr;
	//hid_spi
	uint32_t input_report_header_addr;
	uint32_t input_report_header_sc_addr;
	uint32_t input_report_header_hw_addr;
	uint32_t input_report_header_sc_hw_addr;
	uint32_t input_report_body_addr;
	//uint32_t input_report_body_sc_addr;
	uint32_t input_report_body_hw_addr;
	uint32_t input_report_body_sc_hw_addr;
	uint32_t hid_spi_cmd_addr; // for recovery only
	//flash
	uint32_t gcm_code_addr;
	uint32_t gcm_flag_addr;
	uint32_t flash_cmd_addr;
	uint32_t flash_cmd_issue_addr;
	uint32_t flash_cksum_status_addr;
	uint32_t q_wr_cmd_addr;
	uint32_t crc_flag_addr;
	uint32_t suslo_addr;
	uint32_t mmap_history_event0;
	uint32_t mmap_history_event1;
	uint32_t cmd_reg_addr;
	// mp addr
	uint32_t touch_raw_pipe0_addr;
	uint32_t touch_raw_pipe1_addr;
	uint32_t touch_baseline_addr;
	uint32_t touch_diff_pipe0_addr;
	uint32_t touch_diff_pipe1_addr;
	uint32_t pen_2d_bl_tip_x_addr;
	uint32_t pen_2d_bl_tip_y_addr;
	uint32_t pen_2d_bl_ring_x_addr;
	uint32_t pen_2d_bl_ring_y_addr;
	uint32_t pen_2d_diff_max_tip_x_addr;
	uint32_t pen_2d_diff_max_tip_y_addr;
	uint32_t pen_2d_diff_max_ring_x_addr;
	uint32_t pen_2d_diff_max_ring_y_addr;
	uint32_t pen_2d_diff_min_tip_x_addr;
	uint32_t pen_2d_diff_min_tip_y_addr;
	uint32_t pen_2d_diff_min_ring_x_addr;
	uint32_t pen_2d_diff_min_ring_y_addr;
};

struct nvt_ts_flash_map {
	uint32_t flash_normal_fw_start_addr;
	uint32_t flash_pid_addr;
};

// ===============================================================
static const struct nvt_ts_mem_map nt36532_cascade_memory_map = {
	.read_flash_checksum_addr = 0x100000,
	.rw_flash_data_addr = 0x100002,
	.pen_2d_bl_tip_x_addr = 0x10F940,
	.pen_2d_bl_tip_y_addr = 0x10FD40,
	.pen_2d_bl_ring_x_addr = 0x110140,
	.pen_2d_bl_ring_y_addr = 0x110540,
	.pen_2d_diff_max_tip_x_addr = 0x111140,
	.pen_2d_diff_max_tip_y_addr = 0x111540,
	.pen_2d_diff_max_ring_x_addr = 0x111940,
	.pen_2d_diff_max_ring_y_addr = 0x111D40,
	.pen_2d_diff_min_tip_x_addr = 0x126E00,
	.pen_2d_diff_min_tip_y_addr = 0x127210,
	.pen_2d_diff_min_ring_x_addr = 0x127620,
	.pen_2d_diff_min_ring_y_addr = 0x127A30,
	.block_jump_addr = 0x12FFF0, // 1fb series
	.hid_spi_cmd_addr = 0x125750,
	.event_map_fwinfo_addr = 0x125800,
	.event_buf_cmd_addr = 0x125850,
	.event_buf_hs_sub_cmd_addr = 0x125851,
	.event_buf_reset_state_addr = 0x125860,
	.input_report_header_addr = 0x125900,
	.input_report_header_sc_addr = 0x125908,
	.input_report_body_addr = 0x125910,
	.touch_raw_pipe0_addr = 0x10B200,
	.touch_raw_pipe1_addr = 0x10B200,
	.touch_baseline_addr = 0x109E00,
	.touch_diff_pipe0_addr = 0x128140,
	.touch_diff_pipe1_addr = 0x129540,
	.mmap_history_event0 = 0x121AF8,
	.mmap_history_event1 = 0x121B38,
	.chip_ver_trim_addr = 0x1FB104,
	.enb_casc_addr = 0x1FB12C,
	.sw_pd_idle_addr = 0x1FB200,
	.suslo_addr = 0x1FB201,
	.idle_int_en_addr = 0x1FB238,
	.swrst_sif_addr = 0x1FB43E,
	.input_report_header_hw_addr = 0x1FB484,
	.input_report_body_hw_addr = 0x1FB488,
	.input_report_header_sc_hw_addr = 0x1FB48C,
	.input_report_body_sc_hw_addr = 0x1FB490,
	.crc_flag_addr = 0x1FB533,
	.bld_spe_pups_addr = 0x1FB535,
	.cmd_reg_addr = 0x1FB464,
	.hid_i2c_eng_addr = 0x1FB468,
	.hid_i2c_eng_get_rpt_length_addr = 0x1FB46C,
	.gcm_code_addr = 0x1FB540,
	.flash_cmd_addr = 0x1FB543,
	.flash_cmd_issue_addr = 0x1FB54E,
	.flash_cksum_status_addr = 0x1FB54F,
	.gcm_flag_addr = 0x1FB553,
};

static const struct nvt_ts_flash_map nt36532_flash_map = {
	.flash_normal_fw_start_addr = 0x2000,
	.flash_pid_addr = 0x3F004,
};

struct nvt_ts_trim_id_table {
	uint8_t id[NVT_ID_BYTE_MAX];
	uint8_t mask[NVT_ID_BYTE_MAX];
	uint16_t ic;
	const struct nvt_ts_mem_map *mmap;
	const struct nvt_ts_mem_map *mmap_casc;
	const struct nvt_ts_flash_map *fmap;
	const uint8_t hid_i2c_prefix_pkt_recovery[HID_I2C_PREFIX_PKT_LENGTH];
	bool need_lock_flash;
};

enum {
	IC_36532 = 1,
};

static const struct nvt_ts_trim_id_table trim_id_table[] = {
	// place mmap for different kind of chip addr and sif addr on the top
	{
		.id = { 0x20, 0xFF, 0xFF, 0x32, 0x65, 0x03 },
		.mask = { 1, 0, 0, 1, 1, 1 },
		.ic = IC_36532,
		.mmap_casc = &nt36532_cascade_memory_map,
		.fmap = &nt36532_flash_map,
		.hid_i2c_prefix_pkt_recovery = { 0xFF, 0xFF, 0x0B, 0x00, 0x00 },
	},
	// place repeated chip addr and sif addr mmap below
};
