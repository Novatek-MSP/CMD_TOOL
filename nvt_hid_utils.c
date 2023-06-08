// SPDX-License-Identifier: Apache-2.0

#include "nvt_hid_main.h"

void msleep(int32_t delay)
{
	int32_t i = 0;

	for (i = 0; i < delay; i++)
		usleep(1000);
}

void memset16(uint8_t *buf, uint16_t val, uint16_t count)
{
	while (count--) {
		*buf++ = val & 0xFF;
		*buf++ = (val >> 8) & 0xFF;
	}
}

int32_t confirm(void)
{
	int32_t ret = 0;

	NVT_LOG("Are you sure ? (y/n)");
	if (getchar() != 'y') {
		NVT_ERR("Cancel this operation");
		ret = -EAGAIN;
		goto end;
	}
	ret = 0;
end:
	return ret;
}

int32_t ctp_hid_read(uint32_t addr, uint8_t *data, uint16_t len)
{
	uint8_t *buf = NULL;
	uint32_t read_len = 0, buf_size_max = 0;
	int32_t ret = 0;

	if (!skip_zero_addr_chk && !addr) {
		NVT_ERR("addr does not exist");
		ret = -EINVAL;
		goto end;
	}
	if (!data) {
		NVT_ERR("data buf is not allocated");
		ret = -EINVAL;
		goto end;
	}
	if (!len) {
		NVT_ERR("length can not be 0");
		ret = -EINVAL;
		goto end;
	}

	if (bus_type == BUS_I2C) {
		read_len = len + 3;
		buf_size_max = read_len > 12 ? read_len : 12;
		buf = malloc(buf_size_max);
		if (!buf) {
			NVT_ERR("No memory for %d bytes", buf_size_max);
			ret = -ENOMEM;
			goto end;
		}

		// Set start address and report length
		buf[0] = DEFAULT_REPORT_ID_ENG;
		buf[1] = 0x0B;
		buf[2] = 0x00;
		buf[3] = _u32_to_n_u8(ts->mmap->hid_i2c_eng_addr, 0);
		buf[4] = _u32_to_n_u8(ts->mmap->hid_i2c_eng_addr, 1);
		buf[5] = _u32_to_n_u8(ts->mmap->hid_i2c_eng_addr, 2);
		buf[6] = _u32_to_n_u8(addr, 0);
		buf[7] = _u32_to_n_u8(addr, 1);
		buf[8] = _u32_to_n_u8(addr, 2);
		buf[9] = 0x00;
		buf[10] = _u32_to_n_u8(read_len, 0);
		buf[11] = _u32_to_n_u8(read_len, 1);
		ret = ioctl(fd_node, HIDIOCSFEATURE(12), buf);
		if (ret < 0) {
			NVT_ERR("Can not set start address and report length, ret = %d",
				ret);
			goto end;
		}

		// Read data
		memset(buf, 0, buf_size_max);
		buf[0] = DEFAULT_REPORT_ID_ENG;
		buf[1] = _u32_to_n_u8(read_len, 0);
		buf[2] = _u32_to_n_u8(read_len, 1);
		buf[3] = DEFAULT_REPORT_ID_ENG;
		ret = ioctl(fd_node, HIDIOCGFEATURE(read_len - 2), buf);
		if (ret < 0) {
			NVT_ERR("Can not read data, read_len = %d, ret = %d",
				read_len, ret);
			goto end;
		}

		memcpy(data, buf + 1, len); // drop id
	} else {
		if (!recovery_node) {
			read_len = len + (4 - (len % 4));
			buf_size_max = read_len + 1 > 9 ? read_len + 1 : 9;
			buf = malloc(buf_size_max);
			if (!buf) {
				NVT_ERR("No memory for %d bytes", buf_size_max);
				ret = -ENOMEM;
				goto end;
			}

			buf[0] = CONTENT_ID_FOR_READ;
			buf[1] = _u32_to_n_u8(len,
					      0); // actural len here
			buf[2] = _u32_to_n_u8(len, 1);
			buf[3] = _u32_to_n_u8(addr, 0);
			buf[4] = _u32_to_n_u8(addr, 1);
			buf[5] = _u32_to_n_u8(addr, 2);
			buf[6] = 0x00;
			buf[7] = 0x00;
			buf[8] = 0x00;
			ret = ioctl(fd_node, HIDIOCSFEATURE(9), buf);
			if (ret < 0) {
				NVT_ERR("Can not write data, ret = %d :", ret);
				NVT_ERR_HEX(buf, 9);
			} else {
				ret = 0;
			}

			//buf[0] = CONTENT_ID_FOR_READ;
			ret = ioctl(fd_node, HIDIOCGFEATURE(read_len), buf);
			if (ret < 0) {
				NVT_ERR("Can not read data, read_len = %d, ret = %d",
					read_len, ret);
				goto end;
			}
			memcpy(data, buf, len);
		} else {
			//recovery node here
			read_len = len + HID_SPI_PACKET_LEN;
			buf = malloc(read_len);
			if (!buf) {
				NVT_ERR("No memory for %d bytes", read_len);
				ret = -ENOMEM;
				goto end;
			}
			buf[0] = OPCODE_READ;
			buf[1] = _u32_to_n_u8(addr, 2);
			buf[2] = _u32_to_n_u8(addr, 1);
			buf[3] = _u32_to_n_u8(addr, 0);
			buf[4] = 0xFF;
			ret = ioctl(fd_node, HIDIOCGFEATURE(read_len), buf);
			if (ret < 0) {
				NVT_ERR("Can not read data, read_len = %d, ret = %d",
					read_len, ret);
				goto end;
			}
			memcpy(data, buf + HID_SPI_PACKET_LEN, len);
		}
	}

	ret = 0;
	free(buf);
end:
	return ret;
}

int32_t ctp_hid_write(uint32_t addr, uint8_t *data, uint16_t len)
{
	uint8_t *buf = NULL;
	uint32_t write_len = 0, report_len = 0;
	int32_t ret = 0;

	if (!skip_zero_addr_chk && !addr) {
		NVT_ERR("addr does not exist");
		ret = -EINVAL;
		goto end;
	}
	if (!data) {
		NVT_ERR("data buf is not allocated");
		ret = -EINVAL;
		goto end;
	}
	if (!len) {
		NVT_ERR("length can not be 0");
		ret = -EINVAL;
		goto end;
	}

	if (bus_type == BUS_I2C) {
		write_len = len + 5;
		report_len = write_len + 1;
		buf = malloc(report_len);
		if (!buf) {
			NVT_ERR("No memory for %d bytes", report_len);
			ret = -ENOMEM;
			goto end;
		}

		buf[0] = DEFAULT_REPORT_ID_ENG;
		buf[1] = _u32_to_n_u8(write_len, 0);
		buf[2] = _u32_to_n_u8(write_len, 1);
		buf[3] = _u32_to_n_u8(addr, 0);
		buf[4] = _u32_to_n_u8(addr, 1);
		buf[5] = _u32_to_n_u8(addr, 2);
		memcpy(buf + 6, data, len);
		ret = ioctl(fd_node, HIDIOCSFEATURE(report_len), buf);
		if (ret < 0) {
			NVT_ERR("Can not write data, ret = %d :", ret);
			NVT_ERR_HEX(buf, report_len);
		} else {
			ret = 0;
		}
	} else {
		if (!recovery_node) {
			write_len = len + 5;
			report_len = write_len + 1;
			buf = malloc(report_len);
			if (!buf) {
				NVT_ERR("No memory for %d bytes", report_len);
				ret = -ENOMEM;
				goto end;
			}
			memset(buf, 0, report_len);

			buf[0] = DEFAULT_REPORT_ID_ENG;
			buf[1] = _u32_to_n_u8(write_len, 0); // eg. new_len
			buf[2] = _u32_to_n_u8(write_len, 1);
			buf[3] = _u32_to_n_u8(addr, 0);
			buf[4] = _u32_to_n_u8(addr, 1);
			buf[5] = _u32_to_n_u8(addr, 2);
			memcpy(buf + 6, data, len);
			ret = ioctl(fd_node, HIDIOCSFEATURE(report_len), buf);
			if (ret < 0) {
				NVT_ERR("Can not write data, ret = %d :", ret);
				NVT_ERR_HEX(buf, report_len);
			}
		} else {
			//recovery_node
			if (!ts->mmap->hid_spi_cmd_addr) {
				NVT_ERR("hid_spi_cmd_addr is not in mmap");
				exit(-EINVAL);
			}
			write_len = len + 5;
			report_len = write_len + 8;
			buf = malloc(report_len);
			if (!buf) {
				NVT_ERR("No memory for %d bytes", report_len);
				ret = -ENOMEM;
				goto end;
			}

			buf[0] = OPCODE_WRITE;
			buf[1] = _u32_to_n_u8(ts->mmap->hid_spi_cmd_addr, 2);
			buf[2] = _u32_to_n_u8(ts->mmap->hid_spi_cmd_addr, 1);
			buf[3] = _u32_to_n_u8(ts->mmap->hid_spi_cmd_addr, 0);

			buf[4] = 0xff;

			buf[5] = _u32_to_n_u8(write_len, 0); // eg. new_len
			buf[6] = _u32_to_n_u8(write_len, 1);
			buf[7] = DEFAULT_REPORT_ID_ENG;
			buf[8] = _u32_to_n_u8(write_len, 0); // eg. new_len
			buf[9] = _u32_to_n_u8(write_len, 1);
			buf[10] = _u32_to_n_u8(addr, 0);
			buf[11] = _u32_to_n_u8(addr, 1);
			buf[12] = _u32_to_n_u8(addr, 2);
			memcpy(buf + 13, data, len);
			ret = ioctl(fd_node, HIDIOCSFEATURE(report_len), buf);
			if (ret < 0) {
				NVT_ERR("Can not write data, ret = %d :", ret);
				NVT_ERR_HEX(buf, report_len);
			}
		}
	}
	ret = 0;
	free(buf);
end:
	return ret;
}

int32_t set_recovery_pkt(void)
{
	int32_t ret = 0;

	if (bus_type == BUS_SPI) {
		ret = 0;
		goto end;
	}
	if (!recovery_node) {
		NVT_DBG("not using recovery node, ignoring set_recovery_pkt");
		ret = 0;
		goto end;
	}
	memcpy(ts->hid_i2c_prefix_pkt,
	       trim_id_table[ts->idx].hid_i2c_prefix_pkt_recovery,
	       HID_I2C_PREFIX_PKT_LENGTH);
	ret = write_prefix_packet();
end:
	return ret;
}

void nvt_bootloader_reset(void)
{
	uint8_t buf[1] = { 0 };

	NVT_DBG("0x69 to 0x%06X", ts->mmap->swrst_sif_addr);
	buf[0] = 0x69;
	ctp_hid_write(ts->mmap->swrst_sif_addr, buf, 1);
	msleep(200);
}

int32_t check_crc_done(void)
{
	uint8_t buf[1] = { 0 }, retry = 20;
	int32_t ret = 0;

	while (--retry) {
		msleep(500);
		buf[0] = 0;
		ctp_hid_read(ts->mmap->crc_flag_addr, buf, 1);
		if (buf[0] & 0x04)
			break;
	}

	if (!retry) {
		NVT_ERR("Failed to get crc done");
		ret = -EAGAIN;
	} else {
		NVT_DBG("crc done ok");
	}

	return ret;
}

void nvt_sw_reset_and_idle(void)
{
	uint8_t buf[1] = { 0 };

	if (!(recovery_node || (bus_type == BUS_SPI))) {
		NVT_ERR("not using recovery node or hid_spi, skip idle");
		return;
	}

	if (ts->ic == IC_36532) {
		buf[0] = 0;
		ctp_hid_write(0x1FB129, buf, 1);
	}
	NVT_DBG("0xAA to 0x%06X", ts->mmap->swrst_sif_addr);
	buf[0] = 0xAA;
	ctp_hid_write(ts->mmap->swrst_sif_addr, buf, 1);
	msleep(50);
}

void nvt_sw_idle(void)
{
	uint8_t buf[1] = { 0 };

	if (ts->ic == IC_36532) {
		buf[0] = 0;
		ctp_hid_write(0x1FB129, buf, 1);
	}
	buf[0] = 0x01;
	ctp_hid_write(ts->mmap->sw_pd_idle_addr, buf, 1);
	NVT_DBG("Write 0x%02X to 0x%06X", buf[0], ts->mmap->sw_pd_idle_addr);
	buf[0] = 0x55;
	ctp_hid_write(ts->mmap->suslo_addr, buf, 1);
	NVT_DBG("Write 0x%02X to 0x%06X", buf[0], ts->mmap->suslo_addr);
	msleep(50);
}

void nvt_sw_pd(void)
{
	uint8_t buf[1] = { 0 };

	buf[0] = 0x02;
	ctp_hid_write(ts->mmap->sw_pd_idle_addr, buf, 1);
	NVT_DBG("Write 0x%02X to 0x%06X", buf[0], ts->mmap->sw_pd_idle_addr);
	buf[0] = 0x55;
	ctp_hid_write(ts->mmap->suslo_addr, buf, 1);
	NVT_DBG("Write 0x%02X to 0x%06X", buf[0], ts->mmap->suslo_addr);
}

void nvt_stop_crc_reboot(void)
{
	uint8_t buf[1] = { 0 };
	uint8_t retry = 20;

	NVT_DBG("%s (0xA5 to 0x%06X) %d times", __func__,
		ts->mmap->bld_spe_pups_addr, retry);
	while (retry--) {
		buf[0] = 0xA5;
		ctp_hid_write(ts->mmap->bld_spe_pups_addr, buf, 1);
	}
	msleep(5);
}

int32_t nvt_check_fw_reset_state(uint8_t state)
{
	uint8_t buf[1] = { 0 };
	int32_t ret = 0, retry = 100;

	// first clear
	ctp_hid_write(ts->mmap->event_buf_reset_state_addr, buf, 1);

	while (--retry) {
		msleep(10);
		ctp_hid_read(ts->mmap->event_buf_reset_state_addr, buf, 1);

		if ((buf[0] >= state) && (buf[0] <= RESET_STATE_MAX)) {
			ret = 0;
			break;
		}
	}

	if (unlikely(retry == 0)) {
		NVT_ERR("error, buf[0] = 0x%02X", buf[0]);
		ret = -EAGAIN;
	}

	return ret;
}

int32_t nvt_check_fw_status(void)
{
	uint8_t buf[1] = { 0 }, retry = 20;
	int32_t ret = 0;

	NVT_DBG("start");
	while (--retry) {
		ctp_hid_read(ts->mmap->event_buf_hs_sub_cmd_addr, buf, 1);
		if ((buf[0] & 0xF0) == 0xA0)
			break;
		msleep(10);
	}

	if (!retry) {
		NVT_ERR("failed, buf[0]=0x%02X\n", buf[0]);
		ret = -EAGAIN;
	}

	return ret;
}

int32_t nvt_get_fw_ver(void)
{
	uint8_t buf[2] = { 0 }, retry = 10;
	int32_t ret = 0;

	while (--retry) {
		ctp_hid_read(ts->mmap->event_map_fwinfo_addr, buf, 2);
		if ((buf[0] + buf[1]) == 0xFF)
			break;
	}

	if (!retry) {
		NVT_ERR("FW info is broken, fw_ver=0x%02X, ~fw_ver=0x%02X",
			buf[0], buf[1]);
		ret = -EAGAIN;
		goto end;
	}

	ts->fw_ver = buf[0];
	NVT_DBG("fw_ver = 0x%02X", ts->fw_ver);
end:
	return ret;
}

int32_t nvt_get_fw_ver_and_info(uint8_t show)
{
	uint8_t buf[39] = { 0 }, retry = 10;
	int32_t ret = 0;

	while (--retry) {
		ctp_hid_read(ts->mmap->event_map_fwinfo_addr, buf, 39);
		if ((buf[0] + buf[1]) == 0xFF)
			break;
	}

	if (!retry) {
		NVT_ERR("FW info is broken, fw_ver=0x%02X, ~fw_ver=0x%02X",
			buf[0], buf[1]);
		ret = -EAGAIN;
		goto end;
	}

	ts->fw_ver = buf[0];
	ts->touch_x_num = buf[2];
	ts->touch_y_num = buf[3];
	ts->pen_x_num = ts->touch_x_num;
	ts->pen_y_num = ts->touch_y_num;
	ts->pen_x_num_gang = buf[37];
	ts->pen_y_num_gang = buf[38];
	ts->touch_abs_x_max = ((uint16_t)buf[4] << 8) + buf[5];
	ts->touch_abs_y_max = ((uint16_t)buf[6] << 8) + buf[7];

	ret = dump_flash_gcm_pid();

	if (show) {
		NVT_LOG("fw_ver = 0x%02X, flash_pid = %04X, touch_x_num = %d, touch_y_num = %d, "
			"touch_abs_x_max = %d, touch_abs_y_max = %d, "
			"pen_x_num_gang = %d, pen_y_num_gang = %d",
			ts->fw_ver, ts->flash_pid, ts->touch_x_num,
			ts->touch_y_num, ts->touch_abs_x_max,
			ts->touch_abs_y_max, ts->pen_x_num_gang,
			ts->pen_y_num_gang);
	}

end:
	return ret;
}

int32_t nvt_read_reg_bits(nvt_ts_reg_t reg, uint8_t *val)
{
	uint8_t mask = 0;
	uint8_t shift = 0;
	uint8_t buf[8] = { 0 };
	uint8_t temp = 0;
	uint32_t addr = 0;
	int32_t ret = 0;

	addr = reg.addr;
	mask = reg.mask;
	temp = reg.mask;
	shift = 0;
	while (1) {
		if ((temp >> shift) & 0x01)
			break;
		if (shift == 8) {
			NVT_ERR("mask all bits zero!\n");
			break;
		}
		shift++;
	}
	buf[0] = 0;
	ret = ctp_hid_read(addr, buf, 1);
	if (ret < 0) {
		NVT_ERR("ctp_hid_read failed!(%d)\n", ret);
		goto end;
	}
	*val = ((buf[0] & mask) >> shift);

end:
	return ret;
}

int32_t nvt_write_reg_bits(nvt_ts_reg_t reg, uint8_t val)
{
	uint8_t mask = 0, shift = 0, temp = 0;
	uint8_t buf[8] = { 0 };
	uint32_t addr = 0;
	int32_t ret = 0;

	addr = reg.addr;
	mask = reg.mask;
	temp = reg.mask;
	shift = 0;
	while (1) {
		if ((temp >> shift) & 0x01)
			break;
		if (shift == 8) {
			NVT_ERR("mask all bits zero!\n");
			break;
		}
		shift++;
	}
	ret = ctp_hid_read(addr, buf, 1);
	if (ret < 0) {
		NVT_ERR("ctp_hid_read failed!(%d)\n", ret);
		goto end;
	}
	temp = buf[0] & (~mask);
	temp |= ((val << shift) & mask);
	buf[0] = temp;
	ret = ctp_hid_write(addr, buf, 1);
	if (ret < 0) {
		NVT_ERR("ctp_hid_write failed!(%d)\n", ret);
		goto end;
	}

end:
	return ret;
}

int32_t write_prefix_packet(void)
{
	char pfx_buf[5] = { 0 };
	int32_t ret = 0;

	ret = write(fd_node, ts->hid_i2c_prefix_pkt, HID_I2C_PREFIX_PKT_LENGTH);
	if (ret < 0)
		goto end;
	lseek(fd_node, 0, SEEK_SET);
	NVT_DBG("Write prefix :");
	NVT_DBG_HEX(ts->hid_i2c_prefix_pkt, HID_I2C_PREFIX_PKT_LENGTH);
	ret = read(fd_node, pfx_buf, HID_I2C_PREFIX_PKT_LENGTH);
	if (ret < 0)
		goto end;
	lseek(fd_node, 0, SEEK_SET);
	NVT_DBG("Read prefix :");
	NVT_DBG_HEX(pfx_buf, HID_I2C_PREFIX_PKT_LENGTH);
	if (memcmp(pfx_buf, ts->hid_i2c_prefix_pkt,
		   HID_I2C_PREFIX_PKT_LENGTH)) {
		NVT_ERR("Prefix are not the same, can not set prefix packet?");
		ret = -EFAULT;
	}
end:
	return ret;
}

int32_t nvt_user_set_feature_report(uint16_t len, char *main_argv[])
{
	uint8_t buf[NVT_TRANSFER_LEN] = { 0 }, i = 0;
	uint32_t write_len = 0;
	int32_t ret = 0;

	if (len > NVT_TRANSFER_LEN) {
		NVT_ERR("length %d over %d is not supported", len,
			NVT_TRANSFER_LEN);
		ret = -EINVAL;
		goto end;
	}

	write_len = strtoul(main_argv[2], NULL, 10);
	if (write_len != len) {
		NVT_ERR("Wrong length, length should be %d", len);
		ret = -EINVAL;
		goto end;
	}
	if (write_len > NVT_TRANSFER_LEN) {
		NVT_ERR("write_len %d over %d is not supported", write_len,
			NVT_TRANSFER_LEN);
		ret = -EINVAL;
		goto end;
	}

	for (i = 0; i < len; i++)
		buf[i] = strtoul(main_argv[i + 3], NULL, 16);

	NVT_LOG("Data sent to set feature report :");
	NVT_LOG_HEX(buf, len);
	ret = ioctl(fd_node, HIDIOCSFEATURE(len), buf);
	if (ret < 0) {
		NVT_ERR("Can not write data, ret = %d :", ret);
	} else {
		ret = 0;
	}
end:
	return ret;
}

int32_t nvt_user_get_feature_report(uint16_t len, char *main_argv[])
{
	uint8_t buf[NVT_TRANSFER_LEN] = { 0 }, i = 0;
	uint32_t read_len = 0;
	int32_t ret = 0;

	if (len > NVT_TRANSFER_LEN) {
		NVT_ERR("length %d over %d is not supported", len,
			NVT_TRANSFER_LEN);
		ret = -EINVAL;
		goto end;
	}

	read_len = strtoul(main_argv[2], NULL, 10);
	if (read_len > NVT_TRANSFER_LEN) {
		NVT_ERR("read_len %d over %d is not supported", read_len,
			NVT_TRANSFER_LEN);
		ret = -EINVAL;
		goto end;
	}

	for (i = 0; i < len; i++)
		buf[i] = strtoul(main_argv[i + 3], NULL, 16);

	NVT_LOG("Data sent to get feature report :");
	NVT_LOG_HEX(buf, len);
	ret = ioctl(fd_node, HIDIOCGFEATURE(read_len), buf);
	if (ret < 0) {
		NVT_ERR("Can not write data, ret = %d :", ret);
	} else {
		NVT_LOG("Data received :");
		NVT_LOG_HEX(buf, read_len);
		ret = 0;
	}
end:
	return ret;
}

void map_mmap_sng_casc(void)
{
	if (trim_id_table[ts->idx].mmap) {
		NVT_DBG("map single IC");
		ts->mmap = trim_id_table[ts->idx].mmap;
	} else if (trim_id_table[ts->idx].mmap_casc) {
		NVT_DBG("map cascade IC");
		ts->mmap = trim_id_table[ts->idx].mmap_casc;
	}
}

void choose_mmap_sng_casc(void)
{
	uint8_t buf[1] = { 0 };
	// when single and cascade exist, check the enb_casc
	if (trim_id_table[ts->idx].mmap && trim_id_table[ts->idx].mmap_casc) {
		if (!ts->mmap->enb_casc_addr) {
			NVT_ERR("when single and cascade exist, cascade should have enb_casc");
			exit(-EINVAL);
		} else {
			ctp_hid_read(ts->mmap->enb_casc_addr, buf, 1);
			if (buf[0] & 0x01) {
				NVT_DBG("Choose cascade IC");
				ts->mmap = trim_id_table[ts->idx].mmap_casc;
			} else {
				NVT_DBG("Choose single IC");
				ts->mmap = trim_id_table[ts->idx].mmap;
			}
		}
	}
}

int32_t match_trim_id(uint8_t *buf)
{
	uint8_t idx = 0, table_size = 0, i = 0;
	int32_t ret = 0;

	table_size = sizeof(trim_id_table) / sizeof(trim_id_table[0]);
	for (idx = 0; idx < table_size; idx++) {
		for (i = 0; i < NVT_ID_BYTE_MAX; i++) {
			if (trim_id_table[idx].mask[i]) {
				if (buf[i] != trim_id_table[idx].id[i])
					break;
			}
		}

		if (i == NVT_ID_BYTE_MAX) { // all id matches
			NVT_DBG("Found matching NVT_ID "
				"[%02x] [%02x] [%02x] [%02x] [%02x] [%02x]",
				buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
			// update it when trim id detection finished
			ts->idx = idx;
			map_mmap_sng_casc(); // map again for same trim type
			choose_mmap_sng_casc(); // determine single/cascade if having both
			ts->ic = trim_id_table[ts->idx].ic;
			ts->need_lock_flash =
				trim_id_table[ts->idx].need_lock_flash;
			ts->fmap = trim_id_table[ts->idx].fmap;
			ret = 0;
			goto end;
		}
	}

	ret = -EAGAIN;

end:
	return ret;
}

int32_t nvt_ts_detect_chip_ver_trim(uint8_t reset_idle)
{
	uint8_t buf[6] = { 0 }, retry = 20;
	int32_t ret = 0;

	if (bus_type == BUS_I2C) {
		ret = set_recovery_pkt();
		if (ret)
			return ret;
	}

	if (!reset_idle) {
		while (--retry) {
			for (ts->idx = 0; ts->idx < TRIM_TYPE_MAX; ts->idx++) {
				NVT_DBG("Current table idx = %d", ts->idx);
				//try a map
				map_mmap_sng_casc();
				ctp_hid_read(ts->mmap->chip_ver_trim_addr, buf,
					     6);
				NVT_DBG("Read trim [%02x] [%02x] [%02x] [%02x] [%02x] [%02x]",
					buf[0], buf[1], buf[2], buf[3], buf[4],
					buf[5]);
				ret = match_trim_id(buf);
				if (ret == 0)
					goto end;
				msleep(50);
			}
		}
	} else {
		while (--retry) {
			for (ts->idx = 0; ts->idx < TRIM_TYPE_MAX; ts->idx++) {
				NVT_DBG("Current table idx = %d", ts->idx);
				//try a map
				map_mmap_sng_casc();
				ctp_hid_read(ts->mmap->chip_ver_trim_addr, buf,
					     6);
				NVT_DBG("Read trim [%02x] [%02x] [%02x] [%02x] [%02x] [%02x]",
					buf[0], buf[1], buf[2], buf[3], buf[4],
					buf[5]);
				ret = match_trim_id(buf);
				if (ret == 0) {
					if (!recovery_node) {
						ret = nvt_block_jump();
						if (ret)
							goto end;
					}
					nvt_bootloader_reset();
					if (recovery_node) {
						nvt_sw_reset_and_idle();
						nvt_stop_crc_reboot();
					} else {
						if (check_if_in_bootload())
							continue;
					}
					goto end;
				}
				msleep(50);
			}
		}
	}

	if (!retry) {
		NVT_ERR("No matching NVT_ID found [%02x] [%02x] [%02x] [%02x] [%02x] [%02x]",
			buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
		ret = -EAGAIN;
		NVT_ERR("Tool support HID IC list:\n"
			" [ NT56810 HID_SPI], \n"
			" [ NT36532 HID_I2C/HID_SPI]");
	}

end:
	return ret;
}

int32_t find_i2c_hid_dev(char *i2c_hid_driver_path, char *dev_i2c_bus_addr)
{
	DIR *d1 = NULL, *d2 = NULL, *d3 = NULL;
	struct dirent *dir1 = NULL, *dir2 = NULL, *dir3 = NULL;
	char ihid_drv_p[MAX_NODE_PATH_SIZE] = { 0 };
	char ihid_dev_p[MAX_NODE_PATH_SIZE] = { 0 };
	int32_t ret = 0;

	d1 = opendir(NODE_PATH_I2C_DRIVERS);
	if (d1) {
		// /sys/bus/i2c/drivers
		while ((dir1 = readdir(d1)) != NULL) {
			// /sys/bus/i2c/drivers/*
			NVT_DBG("Trying %s/%s", NODE_PATH_I2C_DRIVERS,
				dir1->d_name);
			if (strncmp("i2c_hid", dir1->d_name, 7) == 0) {
				// /sys/bus/i2c/drivers/i2c_hid*
				snprintf(ihid_drv_p, MAX_NODE_PATH_SIZE,
					 "%s/%s", NODE_PATH_I2C_DRIVERS,
					 dir1->d_name);
				strncpy(i2c_hid_driver_path, ihid_drv_p,
					strlen(ihid_drv_p) + 1);
				d2 = opendir(ihid_drv_p);
				if (d2) {
					// /sys/bus/i2c/drivers/i2c_hid*/*
					while ((dir2 = readdir(d2)) != NULL) {
						snprintf(ihid_dev_p,
							 MAX_NODE_PATH_SIZE,
							 "%s/%s", ihid_drv_p,
							 dir2->d_name);
						strncpy(dev_i2c_bus_addr,
							dir2->d_name,
							strlen(dir2->d_name) +
								1);
						d3 = opendir(ihid_dev_p);
						if (d3) {
							// /sys/bus/i2c/drivers/i2c_hid*/*/*
							// expect a  0018:0603* here
							while ((dir3 = readdir(
									d3)) !=
							       NULL) {
								if (strncmp(I2C_BUS_STR
									    ":" NVT_VID_STR,
									    dir3->d_name,
									    strlen(I2C_BUS_STR
										   ":" NVT_VID_STR)) ==
								    0) {
									ret = 0;
									goto end;
								}
							}
						}
					}
				} else {
					NVT_DBG("%s is not a directory",
						i2c_hid_driver_path);
					continue;
				}
			}
		}
		NVT_ERR("Can not find HID I2C device. \"Rebind\" requires the device");
		NVT_ERR("    to at least be able to enumerate. (Short FW corrupted?)");
	} else {
		NVT_ERR("Can not open %s", NODE_PATH_I2C_DRIVERS);
	}

	ret = -EINVAL;
end:
	return ret;
}

int32_t rebind_i2c_hid_driver(void)
{
	char i2c_hid_driver_path[MAX_NODE_PATH_SIZE] = { 0 };
	char dev_i2c_bus_addr[MAX_NODE_NAME_SIZE] = { 0 };
	char system_cmd[MAX_SYSTEM_CMD_SIZE] = { 0 };
	int32_t ret = 0;

	ret = find_i2c_hid_dev(i2c_hid_driver_path, dev_i2c_bus_addr);
	if (ret) {
		NVT_ERR("Failed to find i2c hid path");
		goto end;
	}
	// unbind
	snprintf(system_cmd, MAX_SYSTEM_CMD_SIZE, "echo %s > %s/%s",
		 dev_i2c_bus_addr, i2c_hid_driver_path, UNBIND);
	NVT_DBG("system cmd : %s", system_cmd);
	ret = system(system_cmd);
	if (ret) {
		NVT_ERR("Failed to run command \"%s\"", system_cmd);
		goto end;
	}
	msleep(500);
	// bind
	snprintf(system_cmd, MAX_SYSTEM_CMD_SIZE, "echo %s > %s/%s",
		 dev_i2c_bus_addr, i2c_hid_driver_path, BIND);
	NVT_DBG("system cmd : %s", system_cmd);
	ret = system(system_cmd);
	if (ret) {
		NVT_ERR("Failed to run command \"%s\"", system_cmd);
		goto end;
	}
end:
	return ret;
}

int32_t find_generic_hidraw_path(char *path)
{
	DIR *d1 = NULL;
	struct dirent *dir1 = NULL;
	struct hidraw_devinfo info = { 0 };
	char hidraw_p[MAX_NODE_PATH_SIZE] = { 0 };
	int32_t ret = 0;

	d1 = opendir(NODE_PATH_DEV);
	if (d1) {
		while ((dir1 = readdir(d1)) != NULL) {
			//NVT_DBG("Trying %s/%s", NODE_PATH_DEV, dir1->d_name);
			if (strncmp("hidraw", dir1->d_name, 6) == 0) {
				snprintf(hidraw_p, MAX_NODE_PATH_SIZE, "%s/%s",
					 NODE_PATH_DEV, dir1->d_name);
				fd_node = open(hidraw_p, O_RDWR | O_NONBLOCK);
				ret = ioctl(fd_node, HIDIOCGRAWINFO, &info);
				NVT_DBG("Found %s/%s as hidraw", NODE_PATH_DEV,
					dir1->d_name);
				NVT_DBG("vendor id = %04X", info.vendor);
				if (info.vendor == NVT_VID_NUM) {
					NVT_DBG("Found %s/%s as nvt hidraw",
						NODE_PATH_DEV, dir1->d_name);
					strncpy(path, hidraw_p,
						MAX_NODE_PATH_SIZE);
					if (info.bustype == BUS_I2C) {
						bus_type = BUS_I2C;
						ret = 0;
						goto end;
					} else if (info.bustype == BUS_SPI) {
						bus_type = BUS_SPI;
						ret = 0;
						goto end;
					} else {
						NVT_ERR("bus type %04X is not supported",
							info.bustype);
						ret = -EINVAL;
						goto end;
					}
				}
			}
		}
	}
	NVT_ERR("Can not find /dev/hidraw*");
	ret = -EINVAL;
end:
	return ret;
}

int32_t check_if_in_bootload(void)
{
	int32_t ret = 0;
	uint8_t buf[1] = { 0 };
	ret = ctp_hid_read(ts->mmap->event_buf_reset_state_addr, buf, 1);
	if (ret == 0 && ((buf[0] & 0xF0) == 0x70)) {
		NVT_LOG("Device is already in bootload");
		ret = 0;
	} else {
		NVT_DBG("Device is not in bootload");
		ret = -ENODEV;
	}
	return ret;
}

int32_t nvt_block_jump(void)
{
	int32_t ret = 0;
	uint8_t buf[JUMP_KEY_SIZE] = { 0, 1, 2,	 3,  4,	 5,  6,	 7,
				       8, 9, 10, 11, 12, 13, 14, 15 };
	if (!ts->mmap->block_jump_addr) {
		NVT_DBG("skip");
		goto end;
	}
	if (recovery_node) {
		NVT_DBG("skip for recovery node for reset idle");
		goto end;
	}
	ret = ctp_hid_write(ts->mmap->block_jump_addr, buf, JUMP_KEY_SIZE);
	if (!ret) {
		NVT_DBG("block at 0x%06X", ts->mmap->block_jump_addr);
	} else {
		NVT_DBG("block failed");
	}
end:
	return ret;
}

void nvt_unblock_jump(void)
{
	uint8_t buf[JUMP_KEY_SIZE] = { 0 };
	if (!ts->mmap->block_jump_addr) {
		NVT_DBG("skip");
		return;
	}
	if (recovery_node) {
		NVT_DBG("skip for recovery node for reset idle");
		return;
	}
	ctp_hid_write(ts->mmap->block_jump_addr, buf, JUMP_KEY_SIZE);
	NVT_DBG("unblock");
}
