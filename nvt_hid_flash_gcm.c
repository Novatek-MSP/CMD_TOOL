// SPDX-License-Identifier: Apache-2.0

#include "nvt_hid_main.h"

#define BIN_END_FLAG_LEN_NORMAL 4
#define BIN_END_FLAG_LEN_FULL 3
#define BIN_END_FLAG_LEN_MAX 4
#define HID_FW_BIN_END_NAME_NORMAL "BNVT"
#define HID_FW_BIN_END_NAME_FULL "NVT"

struct fw_bin {
	uint8_t *bin_data;
	uint32_t bin_size;
	uint32_t flash_start_addr;
} fwb, tmpb;

int32_t strtou16(const char *s, uint32_t base, uint16_t *res)
{
	unsigned long long tmp = 0;
	char *endptr = NULL;

	tmp = strtoull(s, &endptr, base);
	if (!endptr || tmp == 0)
		return -EIO;
	if (tmp != (unsigned long long)(uint16_t)tmp)
		return -ERANGE;
	memcpy(res, &tmp, 2);

	return 0;
}

int32_t switch_gcm(uint8_t enable)
{
	uint8_t buf[3] = { 0 }, retry = 0, retry_max = 0;
	uint32_t gcm_code_addr = 0, gcm_flag_addr = 0;
	int32_t ret = 0;

	retry_max = 3;
	gcm_code_addr = ts->mmap->gcm_code_addr;
	gcm_flag_addr = ts->mmap->gcm_flag_addr;

	fflush(stdout);

	while (1) {
		if (enable) {
			buf[0] = 0x55;
			buf[1] = 0xFF;
			buf[2] = 0xAA;
		} else {
			buf[0] = 0xAA;
			buf[1] = 0x55;
			buf[2] = 0xFF;
		}
		ret = ctp_hid_write(gcm_code_addr, buf, 3);
		if (ret < 0) {
			goto end;
		}
		ret = ctp_hid_read(gcm_flag_addr, buf, 1);
		if (ret < 0) {
			goto end;
		}
		if (enable) {
			if ((buf[0] & 0x01) == 0x01) {
				ret = 0;
				break;
			}
		} else {
			if ((buf[0] & 0x01) == 0x00) {
				ret = 0;
				break;
			}
		}
		NVT_DBG("Result mismatch, retry");
		retry++;
		if (unlikely(retry == retry_max)) {
			if (enable)
				NVT_ERR("Enable gcm failed");
			else
				NVT_ERR("Disable gcm failed");
			ret = -EAGAIN;
			break;
		}
	}

	if (!ret) {
		if (enable)
			NVT_DBG("Enable gcm OK");
		else
			NVT_DBG("Disable gcm OK");
	}

end:
	return ret;
}

int32_t nvt_gcm_xfer(gcm_xfer_t *xfer)
{
	uint8_t *buf = NULL;
	uint32_t flash_cmd_addr = 0, flash_cmd_issue_addr = 0;
	uint32_t rw_flash_data_addr = 0, tmp_addr = 0;
	int32_t ret = 0, tmp_len = 0, i = 0, transfer_len = 0;
	int32_t total_buf_size = 0, wait_cmd_issue_cnt = 0, write_len = 0;

	wait_cmd_issue_cnt = 0;
	flash_cmd_addr = ts->mmap->flash_cmd_addr;
	flash_cmd_issue_addr = ts->mmap->flash_cmd_issue_addr;
	rw_flash_data_addr = ts->mmap->rw_flash_data_addr;

	if (NVT_TRANSFER_LEN <= FLASH_PAGE_SIZE)
		transfer_len = NVT_TRANSFER_LEN;
	else
		transfer_len = FLASH_PAGE_SIZE;

	total_buf_size = 64 + xfer->tx_len + xfer->rx_len;
	buf = malloc(total_buf_size);
	if (!buf) {
		NVT_ERR("No memory for %d bytes", total_buf_size);
		ret = -EAGAIN;
		goto end;
	}
	memset(buf, 0, total_buf_size);

	if ((xfer->tx_len > 0) && xfer->tx_buf) {
		for (i = 0; i < xfer->tx_len; i += transfer_len) {
			tmp_addr = rw_flash_data_addr + i;
			tmp_len = MIN(xfer->tx_len - i, transfer_len);
			memcpy(buf, xfer->tx_buf + i, tmp_len);
			ret = ctp_hid_write(tmp_addr, buf, tmp_len);
			if (ret < 0) {
				NVT_ERR("Write tx data error");
				goto end;
			}
		}
	}

	memset(buf, 0, total_buf_size);
	buf[0] = xfer->flash_cmd;
	if (xfer->flash_addr_len > 0) {
		buf[1] = xfer->flash_addr & 0xFF;
		buf[2] = (xfer->flash_addr >> 8) & 0xFF;
		buf[3] = (xfer->flash_addr >> 16) & 0xFF;
	} else {
		buf[1] = 0x00;
		buf[2] = 0x00;
		buf[3] = 0x00;
	}
	write_len = xfer->flash_addr_len + xfer->pem_byte_len +
		    xfer->dummy_byte_len + xfer->tx_len;
	if (write_len > 0) {
		buf[5] = write_len & 0xFF;
		buf[6] = (write_len >> 8) & 0xFF;
	} else {
		buf[5] = 0x00;
		buf[6] = 0x00;
	}
	if (xfer->rx_len > 0) {
		buf[7] = xfer->rx_len & 0xFF;
		buf[8] = (xfer->rx_len >> 8) & 0xFF;
	} else {
		buf[7] = 0x00;
		buf[8] = 0x00;
	}
	buf[9] = xfer->flash_checksum & 0xFF;
	buf[10] = (xfer->flash_checksum >> 8) & 0xFF;
	buf[11] = 0xC2;
	ret = ctp_hid_write(flash_cmd_addr, buf, 12);
	if (ret < 0) {
		NVT_ERR("Write enter GCM error");
		goto end;
	}

	wait_cmd_issue_cnt = 0;
	while (1) {
		// check flash cmd issue complete
		ret = ctp_hid_read(flash_cmd_issue_addr, buf, 1);
		if (ret < 0) {
			NVT_ERR("Read flash_cmd_issue_addr status error");
			goto end;
		}
		if (buf[0] == 0x00) {
			break;
		}
		wait_cmd_issue_cnt++;
		if (unlikely(wait_cmd_issue_cnt > 2000)) {
			NVT_ERR("write GCM cmd 0x%02X failed", xfer->flash_cmd);
			ret = -1;
			goto end;
		}
		msleep(1);
	}

	if ((xfer->rx_len > 0) && xfer->rx_buf) {
		memset(buf, 0, xfer->rx_len);
		for (i = 0; i < xfer->rx_len; i += transfer_len) {
			tmp_addr = rw_flash_data_addr + i;
			tmp_len = MIN(xfer->rx_len - i, transfer_len);
			ret = ctp_hid_read(tmp_addr, buf, tmp_len);
			if (ret < 0) {
				NVT_ERR("Read rx data fail error");
				goto end;
			}
			memcpy(xfer->rx_buf + i, buf, tmp_len);
		}
	}

	ret = 0;
end:
	if (buf) {
		free(buf);
	}

	return ret;
}

int32_t resume_pd_gcm(void)
{
	int32_t ret = 0;
	gcm_xfer_t xfer = { 0 };

	fflush(stdout);

	memset(&xfer, 0, sizeof(gcm_xfer_t));
	xfer.flash_cmd = 0xAB;
	ret = nvt_gcm_xfer(&xfer);
	if (ret) {
		NVT_ERR("Resume PD failed, ret = %d", ret);
		ret = -EAGAIN;
	} else {
		NVT_DBG("Resume PD OK");
		ret = 0;
	}

	return ret;
}

int32_t nvt_find_match_flash_info(void)
{
	int32_t i = 0, total_info_items = 0;
	const struct flash_info *finfo = NULL;

	total_info_items =
		sizeof(flash_info_table) / sizeof(flash_info_table[0]);
	for (i = 0; i < total_info_items; i++) {
		if (flash_info_table[i].mid == ts->flash_mid) {
			/* mid of this flash info item match current flash's mid */
			if (flash_info_table[i].did == ts->flash_did) {
				/* specific mid and did of this flash info item
				 * match current flash's mid and did */
				break;
			} else if (flash_info_table[i].did == FLASH_DID_ALL) {
				/* mid of this flash info item match current
				 * flash's mid, and all did have same flash info */
				break;
			}
		} else if (flash_info_table[i].mid == FLASH_MFR_UNKNOWN) {
			/* reach the last item of flash_info_table, no flash info item matched */
			break;
		} else {
			/* mid of this flash info item not math current flash's mid */
			continue;
		}
	}
	ts->match_finfo = &flash_info_table[i];
	finfo = ts->match_finfo;
	NVT_DBG("matched flash info item %d:", i);
	NVT_DBG("mid = 0x%02X, did = 0x%04X, qeb_pos = %d", finfo->mid,
		finfo->did, finfo->qeb_info.qeb_pos);
	NVT_DBG("qeb_order = %d, rd_method = %d, prog_method = %d",
		finfo->qeb_info.qeb_order, finfo->rd_method,
		finfo->prog_method);
	NVT_DBG("wrsr_method = %d, rdsr1_cmd_ = 0x%02X", finfo->wrsr_method,
		finfo->rdsr1_cmd);

	return 0;
}

int32_t read_flash_mid_did_gcm(void)
{
	uint8_t buf[3];
	int32_t ret = 0;
	gcm_xfer_t xfer = { 0 };

	fflush(stdout);

	memset(&xfer, 0, sizeof(gcm_xfer_t));
	xfer.flash_cmd = 0x9F;
	xfer.rx_buf = buf;
	xfer.rx_len = 3;
	ret = nvt_gcm_xfer(&xfer);
	if (ret) {
		NVT_ERR("Read Flash MID DID GCM fail, ret = %d", ret);
		ret = -EAGAIN;
		goto end;
	} else {
		ts->flash_mid = buf[0];
		ts->flash_did = (buf[1] << 8) | buf[2];
		NVT_DBG("Flash MID = 0x%02X, DID = 0x%04X", ts->flash_mid,
			ts->flash_did);
		nvt_find_match_flash_info();
		NVT_DBG("Read MID DID OK");
		ret = 0;
	}

end:
	return ret;
}

int32_t nvt_set_read_flash_method(void)
{
	uint8_t bld_rd_io_sel = 0;
	uint8_t bld_rd_addr_sel = 0;
	int32_t ret = 0;
	flash_read_method_t rd_method = 0;

	bld_rd_io_sel = 0;
	bld_rd_addr_sel = 0;
	ret = 0;
	rd_method = ts->match_finfo->rd_method;
	switch (rd_method) {
	case SISO_0x03:
		ts->flash_read_data_cmd = 0x03;
		ts->flash_read_pem_byte_len = 0;
		ts->flash_read_dummy_byte_len = 0;
		bld_rd_io_sel = 0;
		bld_rd_addr_sel = 0;
		break;
	case SISO_0x0B:
		ts->flash_read_data_cmd = 0x0B;
		ts->flash_read_pem_byte_len = 0;
		ts->flash_read_dummy_byte_len = 1;
		bld_rd_io_sel = 0;
		bld_rd_addr_sel = 0;
		break;
	case SIQO_0x6B:
		ts->flash_read_data_cmd = 0x6B;
		ts->flash_read_pem_byte_len = 0;
		ts->flash_read_dummy_byte_len = 4;
		bld_rd_io_sel = 2;
		bld_rd_addr_sel = 0;
		break;
	case QIQO_0xEB:
		ts->flash_read_data_cmd = 0xEB;
		ts->flash_read_pem_byte_len = 1;
		ts->flash_read_dummy_byte_len = 2;
		bld_rd_io_sel = 2;
		bld_rd_addr_sel = 1;
		break;
	default:
		NVT_ERR("flash read method %d not support!", rd_method);
		ret = -EINVAL;
		goto end;
		break;
	}
	NVT_DBG("rd_method = %d, ts->flash_read_data_cmd = 0x%02X", rd_method,
		ts->flash_read_data_cmd);
	NVT_DBG("ts->flash_read_pem_byte_len = %d, ts->flash_read_dummy_byte_len = %d",
		ts->flash_read_pem_byte_len, ts->flash_read_dummy_byte_len);
	NVT_DBG("bld_rd_io_sel = %d, bld_rd_addr_sel = %d", bld_rd_io_sel,
		bld_rd_addr_sel);

	if (ts->mmap->bld_rd_io_sel_reg.addr) {
		ret = nvt_write_reg_bits(ts->mmap->bld_rd_io_sel_reg,
					 bld_rd_io_sel);
		if (ret < 0) {
			NVT_ERR("set bld_rd_io_sel_reg failed, ret = %d", ret);
			goto end;
		} else {
			NVT_DBG("set bld_rd_io_sel_reg=%d done", bld_rd_io_sel);
		}
	}
	if (ts->mmap->bld_rd_addr_sel_reg.addr) {
		ret = nvt_write_reg_bits(ts->mmap->bld_rd_addr_sel_reg,
					 bld_rd_addr_sel);
		if (ret < 0) {
			NVT_ERR("set bld_rd_addr_sel_reg failed, ret = %d",
				ret);
			goto end;
		} else {
			NVT_DBG("set bld_rd_addr_sel_reg=%d done",
				bld_rd_addr_sel);
		}
	}

end:
	return ret;
}

int32_t nvt_set_prog_flash_method(void)
{
	flash_prog_method_t prog_method = 0;
	uint8_t pp4io_en = 0;
	uint8_t q_wr_cmd = 0;
	uint8_t bld_rd_addr_sel = 0;
	uint8_t buf[4] = { 0 };
	int32_t ret = 0;

	prog_method = ts->match_finfo->prog_method;
	switch (prog_method) {
	case SPP_0x02:
		ts->flash_prog_data_cmd = 0x02;
		pp4io_en = 0;
		q_wr_cmd = 0x00; /* not 0x02, must 0x00! */
		break;
	case QPP_0x32:
		ts->flash_prog_data_cmd = 0x32;
		pp4io_en = 1;
		q_wr_cmd = 0x32;
		bld_rd_addr_sel = 0;
		break;
	case QPP_0x38:
		ts->flash_prog_data_cmd = 0x38;
		pp4io_en = 1;
		q_wr_cmd = 0x38;
		bld_rd_addr_sel = 1;
		break;
	default:
		NVT_ERR("flash program method %d not support!", prog_method);
		ret = -EINVAL;
		goto end;
		break;
	}
	NVT_DBG("prog_method=%d, ts->flash_prog_data_cmd=0x%02X", prog_method,
		ts->flash_prog_data_cmd);
	NVT_DBG("pp4io_en=%d, q_wr_cmd=0x%02X, bld_rd_addr_sel=0x%02X",
		pp4io_en, q_wr_cmd, bld_rd_addr_sel);

	if (ts->mmap->pp4io_en_reg.addr) {
		ret = nvt_write_reg_bits(ts->mmap->pp4io_en_reg, pp4io_en);
		if (ret < 0) {
			NVT_ERR("set pp4io_en_reg failed, ret = %d", ret);
			goto end;
		} else {
			NVT_DBG("set pp4io_en_reg=%d done", pp4io_en);
		}
	}
	if (ts->mmap->q_wr_cmd_addr) {
		buf[0] = q_wr_cmd;
		ret = ctp_hid_write(ts->mmap->q_wr_cmd_addr, buf, 1);
		if (ret < 0) {
			NVT_ERR("set q_wr_cmd_addr failed, ret = %d", ret);
			goto end;
		} else {
			NVT_DBG("set Q_WR_CMD_ADDR=0x%02X done", q_wr_cmd);
		}
	}
	if (pp4io_en) {
		if (ts->mmap->bld_rd_addr_sel_reg.addr) {
			ret = nvt_write_reg_bits(ts->mmap->bld_rd_addr_sel_reg,
						 bld_rd_addr_sel);
			if (ret < 0) {
				NVT_ERR("set bld_rd_addr_sel_reg failed, ret = %d",
					ret);
				goto end;
			} else {
				NVT_DBG("set bld_rd_addr_sel_reg=%d done",
					bld_rd_addr_sel);
			}
		}
	}

end:
	return ret;
}

int32_t get_checksum_gcm(uint32_t flash_addr, uint32_t data_len,
			 uint16_t *checksum)
{
	gcm_xfer_t xfer = { 0 };
	uint8_t buf[2] = { 0 };
	int32_t ret = 0;

	memset(&xfer, 0, sizeof(gcm_xfer_t));
	xfer.flash_cmd = ts->flash_read_data_cmd;
	xfer.flash_addr = flash_addr;
	xfer.flash_addr_len = 3;
	xfer.pem_byte_len = ts->flash_read_pem_byte_len;
	xfer.dummy_byte_len = ts->flash_read_dummy_byte_len;
	xfer.rx_len = data_len;
	ret = nvt_gcm_xfer(&xfer);
	if (ret) {
		NVT_ERR("Get Checksum GCM fail, ret = %d", ret);
		ret = -EAGAIN;
		goto end;
	} else {
		ret = 0;
	}

	buf[0] = 0x00;
	buf[1] = 0x00;
	ret = ctp_hid_read(ts->mmap->read_flash_checksum_addr, buf, 2);
	if (ret < 0) {
		NVT_ERR("Get checksum error, ret = %d", ret);
		ret = -EAGAIN;
		goto end;
	}
	*checksum = (buf[1] << 8) | buf[0];

	ret = 0;

end:
	return ret;
}

int32_t write_enable_gcm(void)
{
	gcm_xfer_t xfer = { 0 };
	int32_t ret = 0;

	memset(&xfer, 0, sizeof(gcm_xfer_t));
	xfer.flash_cmd = 0x06;
	ret = nvt_gcm_xfer(&xfer);
	if (ret) {
		NVT_ERR("Write Enable failed, ret = %d", ret);
		ret = -EAGAIN;
	} else {
		ret = 0;
	}

	return ret;
}

int32_t write_status_gcm(uint8_t status)
{
	uint8_t sr1 = 0;
	int32_t ret = 0;
	flash_wrsr_method_t wrsr_method = 0;
	gcm_xfer_t xfer = { 0 };

	memset(&xfer, 0, sizeof(gcm_xfer_t));
	wrsr_method = ts->match_finfo->wrsr_method;
	if (wrsr_method == WRSR_01H1BYTE) {
		xfer.flash_cmd = 0x01;
		xfer.flash_addr = status << 16;
		xfer.flash_addr_len = 1;
	} else if (wrsr_method == WRSR_01H2BYTE) {
		xfer.flash_cmd = ts->match_finfo->rdsr1_cmd;
		xfer.rx_len = 1;
		xfer.rx_buf = &sr1;
		ret = nvt_gcm_xfer(&xfer);
		if (ret) {
			NVT_ERR("Read Status Register-1 fail!!(%d)", ret);
			ret = -EINVAL;
			goto end;
		} else {
			NVT_DBG("Read Status Register-1 OK. sr1=0x%02X", sr1);
		}

		memset(&xfer, 0, sizeof(gcm_xfer_t));
		xfer.flash_cmd = 0x01;
		xfer.flash_addr = (status << 16) | (sr1 << 8);
		xfer.flash_addr_len = 2;
	} else {
		NVT_ERR("Unknown or not support write status register method(%d)!",
			wrsr_method);
		goto end;
	}
	ret = nvt_gcm_xfer(&xfer);
	if (ret) {
		NVT_ERR("Write Status GCM fail, ret = %d", ret);
		ret = -EAGAIN;
	} else {
		ret = 0;
	}
end:
	return ret;
}

int32_t read_status_gcm(uint8_t *status)
{
	gcm_xfer_t xfer = { 0 };
	int32_t ret = 0;

	memset(&xfer, 0, sizeof(gcm_xfer_t));
	xfer.flash_cmd = 0x05;
	xfer.rx_len = 1;
	xfer.rx_buf = status;
	ret = nvt_gcm_xfer(&xfer);
	if (ret) {
		NVT_ERR("Read Status GCM fail, ret = %d", ret);
		ret = -EAGAIN;
	} else {
		ret = 0;
	}

	return ret;
}

int32_t read_status_register_1_gcm(uint8_t *status)
{
	int32_t ret = 0;
	gcm_xfer_t xfer = { 0 };

	if (ts->match_finfo->rdsr1_cmd == 0xFF) {
		NVT_ERR("Read Status Register-1 GCM not support!");
		ret = -EINVAL;
		goto end;
	}

	memset(&xfer, 0, sizeof(gcm_xfer_t));
	xfer.flash_cmd = ts->match_finfo->rdsr1_cmd;
	xfer.rx_len = 1;
	xfer.rx_buf = status;
	ret = nvt_gcm_xfer(&xfer);
	if (ret) {
		NVT_ERR("Read Status Register-1 GCM fail!!(%d)", ret);
	} else {
		NVT_DBG("Read Status Register 1 OK. status=0x%02X", *status);
	}
end:
	return ret;
}

int32_t sector_erase_gcm(uint32_t flash_addr)
{
	gcm_xfer_t xfer = { 0 };
	int32_t ret = 0;

	memset(&xfer, 0, sizeof(gcm_xfer_t));
	xfer.flash_cmd = 0x20;
	xfer.flash_addr = flash_addr;
	xfer.flash_addr_len = 3;
	ret = nvt_gcm_xfer(&xfer);
	if (ret) {
		NVT_ERR("Sector Erase GCM fail, ret = %d", ret);
		ret = -EAGAIN;
	} else {
		ret = 0;
	}

	return ret;
}

int32_t erase_flash_gcm(void)
{
	flash_mfr_t mid = 0;
	uint8_t status = 0;
	int32_t ret = 0;
	int32_t count = 0;
	int32_t i = 0;
	int32_t flash_address = 0;
	int32_t retry = 0;
	int32_t erase_length = 0;
	int32_t start_sector = 0;
	const flash_qeb_info_t *qeb_info_p = NULL;

	fflush(stdout);

	if (fwb.flash_start_addr % FLASH_SECTOR_SIZE) {
		NVT_ERR("flash_start_addr should be n*%d", FLASH_SECTOR_SIZE);
		ret = -EINVAL;
		goto end;
	}

	start_sector = fwb.flash_start_addr / FLASH_SECTOR_SIZE;
	erase_length = fwb.bin_size - fwb.flash_start_addr;
	if (erase_length < 0) {
		NVT_ERR("Wrong erase_length = %d", erase_length);
		return -EINVAL;
	}

	// Write Enable
	ret = write_enable_gcm();
	if (ret < 0) {
		NVT_ERR("Write Enable error, ret = %d", ret);
		ret = -EAGAIN;
		goto end;
	}

	mid = ts->match_finfo->mid;
	qeb_info_p = &ts->match_finfo->qeb_info;
	if ((mid != FLASH_MFR_UNKNOWN) &&
	    (qeb_info_p->qeb_pos != QEB_POS_UNKNOWN)) {
		/* check if QE bit is in Status Register byte 1, if yes set it back to 1 */
		if (qeb_info_p->qeb_pos == QEB_POS_SR_1B) {
			status = (0x01 << qeb_info_p->qeb_order);
		} else {
			status = 0x00;
		}
		// Write Status Register
		ret = write_status_gcm(status);
		if (ret < 0) {
			NVT_ERR("Write Status Register error, ret = %d", ret);
			ret = -EAGAIN;
			goto end;
		}
		NVT_DBG("Write Status Register byte 0x%02X OK", status);
		msleep(1);
	}
	// Read Status
	retry = 0;
	while (1) {
		retry++;
		msleep(5);
		if (unlikely(retry > 100)) {
			NVT_ERR("Read Status failed, status = 0x%02X", status);
			ret = -EAGAIN;
			goto end;
		}
		ret = read_status_gcm(&status);
		if (ret < 0) {
			NVT_ERR("Read Status Register error, ret = %d", ret);
			continue;
		}
		if ((status & 0x03) == 0x00) {
			NVT_DBG("Read Status Register byte 0x%02X OK", status);
			break;
		}
	}

	if (erase_length % FLASH_SECTOR_SIZE)
		count = erase_length / FLASH_SECTOR_SIZE + start_sector + 1;
	else
		count = erase_length / FLASH_SECTOR_SIZE + start_sector;

	for (i = start_sector; i < count; i++) {
		// Write Enable
		ret = write_enable_gcm();
		if (ret < 0) {
			NVT_ERR("Write enable error, ret = %d, page at = %d",
				ret, i * FLASH_SECTOR_SIZE);
			ret = -EAGAIN;
			goto end;
		}

		flash_address = i * FLASH_SECTOR_SIZE;

		// Sector Erase
		ret = sector_erase_gcm(flash_address);
		if (ret < 0) {
			NVT_ERR("Sector erase error, ret = %d, page at = %d",
				ret, i * FLASH_SECTOR_SIZE);
			ret = -EAGAIN;
			goto end;
		}
		msleep(25);

		retry = 0;
		while (1) {
			retry++;
			if (unlikely(retry > 100)) {
				NVT_ERR("Wait sector erase timeout");
				ret = -EAGAIN;
				goto end;
			}
			ret = read_status_gcm(&status);
			if (ret < 0) {
				NVT_ERR("Read status register error, ret = %d",
					ret);
				continue;
			}
			if ((status & 0x03) == 0x00) {
				ret = 0;
				break;
			}
			msleep(5);
		}
	}

	NVT_LOG("Erase OK");

end:
	return ret;
}

int32_t page_program_gcm(uint32_t flash_addr, uint16_t data_len, uint8_t *data)
{
	gcm_xfer_t xfer = { 0 };
	uint16_t checksum = 0;
	int32_t ret = 0;
	int32_t i = 0;

	/* calculate checksum */
	checksum = (flash_addr & 0xFF);
	checksum += ((flash_addr >> 8) & 0xFF);
	checksum += ((flash_addr >> 16) & 0xFF);
	checksum += ((data_len + 3) & 0xFF);
	checksum += (((data_len + 3) >> 8) & 0xFF);
	for (i = 0; i < data_len; i++) {
		checksum += data[i];
	}
	checksum = ~checksum + 1;

	/* prepare gcm command transfer */
	memset(&xfer, 0, sizeof(gcm_xfer_t));
	xfer.flash_cmd = ts->flash_prog_data_cmd;
	xfer.flash_addr = flash_addr;
	xfer.flash_addr_len = 3;
	xfer.tx_buf = data;
	xfer.tx_len = data_len;
	xfer.flash_checksum = checksum & 0xFFFF;
	ret = nvt_gcm_xfer(&xfer);
	if (ret) {
		NVT_ERR("Page Program GCM fail, ret = %d", ret);
		ret = -EAGAIN;
	} else {
		ret = 0;
	}

	return ret;
}

int32_t write_flash_gcm(void)
{
	uint8_t page_program_retry = 0;
	uint8_t buf[1] = { 0 };
	uint32_t flash_address = 0;
	uint32_t flash_cksum_status_addr = ts->mmap->flash_cksum_status_addr;
	uint32_t step = 10, pre = 0, show = 0;
	int32_t ret = 0;
	int32_t i = 0;
	int32_t count = 0;
	int32_t retry = 0;
	uint8_t status = 0;

	nvt_set_prog_flash_method();

	fflush(stdout);

	count = (fwb.bin_size - fwb.flash_start_addr) / FLASH_PAGE_SIZE;
	if ((fwb.bin_size - fwb.flash_start_addr) % FLASH_PAGE_SIZE)
		count++;

	for (i = 0; i < count; i++) {
		flash_address = i * FLASH_PAGE_SIZE + fwb.flash_start_addr;
		page_program_retry = 0;

page_program_start:
		// Write Enable
		ret = write_enable_gcm();
		if (ret < 0) {
			NVT_ERR("Write Enable error, ret = %d", ret);
			ret = -EAGAIN;
			goto end;
		}
		// Write Page : FLASH_PAGE_SIZE bytes
		// Page Program
		ret = page_program_gcm(flash_address,
				       MIN(fwb.bin_size - flash_address,
					   FLASH_PAGE_SIZE),
				       &fwb.bin_data[flash_address]);
		if (ret < 0) {
			NVT_ERR("Page Program error, ret = %d, i= %d", ret, i);
			ret = -EAGAIN;
			goto end;
		}

		// check flash checksum status
		retry = 0;
		while (1) {
			buf[0] = 0x00;
			ret = ctp_hid_read(flash_cksum_status_addr, buf, 1);
			if (buf[0] == 0xAA) { /* checksum pass */
				ret = 0;
				break;
			} else if (buf[0] == 0xEA) { /* checksum error */
				if (page_program_retry < 1) {
					page_program_retry++;
					goto page_program_start;
				} else {
					NVT_ERR("Check Flash Checksum Status error");
					ret = -EAGAIN;
					goto end;
				}
			}
			retry++;
			if (unlikely(retry > 20)) {
				NVT_ERR("Check flash checksum fail, buf[0] = 0x%02X",
					buf[0]);
				ret = -EAGAIN;
				goto end;
			}
			msleep(1);
		}

		// Read Status
		retry = 0;
		while (1) {
			retry++;
			if (unlikely(retry > 200)) {
				NVT_ERR("Wait Page Program timeout");
				ret = -EAGAIN;
				goto end;
			}
			// Read Status
			ret = read_status_gcm(&status);
			if (ret < 0) {
				NVT_ERR("Read Status Register error, ret = %d",
					ret);
				continue;
			}
			if ((status & 0x03) == 0x00) {
				ret = 0;
				break;
			}
			msleep(1);
		}

		// show progress
		show = ((i * 100) / step / count);
		if (pre != show) {
			if (pre == 0)
				printf("[Info ] Programming...%2d%%",
				       show * step);
			else
				printf("...%2d%%", show * step);
			fflush(stdout);
			pre = show;
		}
	}
	printf("...%2d%%\n", 100);
	NVT_LOG("Program OK");
	fflush(stdout);

end:
	return ret;
}

int32_t verify_flash_gcm(void)
{
	uint16_t write_checksum = 0;
	uint16_t read_checksum = 0;
	uint32_t flash_addr = 0;
	uint32_t data_len = 0;
	int32_t ret = 0;
	int32_t total_sector_need_check = 0;
	int32_t i = 0;
	int32_t j = 0;

	nvt_set_read_flash_method();

	total_sector_need_check =
		(fwb.bin_size - fwb.flash_start_addr) / SIZE_4KB;
	for (i = 0; i < total_sector_need_check; i++) {
		flash_addr = i * SIZE_4KB + fwb.flash_start_addr;
		data_len = SIZE_4KB;
		/* calculate write_checksum of each 4KB block */
		write_checksum = (flash_addr & 0xFF);
		write_checksum += ((flash_addr >> 8) & 0xFF);
		write_checksum += ((flash_addr >> 16) & 0xFF);
		write_checksum += ((data_len)&0xFF);
		write_checksum += (((data_len) >> 8) & 0xFF);
		for (j = 0; j < data_len; j++) {
			write_checksum += fwb.bin_data[flash_addr + j];
		}
		write_checksum = ~write_checksum + 1;

		ret = get_checksum_gcm(flash_addr, data_len, &read_checksum);
		if (ret < 0) {
			NVT_ERR("Get Checksum failed, ret = %d, i = %d", ret,
				i);
			ret = -EAGAIN;
			goto end;
		}
		if (write_checksum != read_checksum) {
			NVT_ERR("Verify Failed, i = %d, write_checksum = 0x%04X, "
				"read_checksum = 0x%04X",
				i, write_checksum, read_checksum);
			ret = -EAGAIN;
			goto end;
		}
	}

	NVT_LOG("Verify OK");
	fflush(stdout);

end:
	return ret;
}

int32_t read_flash_sector_gcm(uint32_t flash_addr_start, uint32_t file_size)
{
	gcm_xfer_t xfer = { 0 };
	uint8_t buf[2] = { 0 }, retry = 0;
	uint16_t rd_checksum = 0, rd_data_checksum = 0;
	uint32_t flash_address = 0, Cnt = 0;
	int32_t i = 0, j = 0, fs_cnt = 0, ret = 0;
	char str[5] = { 0 };

	nvt_set_read_flash_method();

	tmpb.bin_size = file_size;
	tmpb.bin_data = malloc(tmpb.bin_size);

	if (file_size % 256)
		fs_cnt = file_size / 256 + 1;
	else
		fs_cnt = file_size / 256;

	for (i = 0; i < fs_cnt; i++) {
		retry = 10;
		while (--retry) {
			Cnt = i * 256;
			flash_address = i * 256 + flash_addr_start;

			rd_data_checksum = 0;
			rd_data_checksum += ((flash_address >> 16) & 0xFF);
			rd_data_checksum += ((flash_address >> 8) & 0xFF);
			rd_data_checksum += (flash_address & 0xFF);
			rd_data_checksum +=
				((MIN(file_size - i * 256, 256) >> 8) & 0xFF);
			rd_data_checksum +=
				(MIN(file_size - i * 256, 256) & 0xFF);

			// Read Flash
			memset(&xfer, 0, sizeof(gcm_xfer_t));
			xfer.flash_cmd = ts->flash_read_data_cmd;
			xfer.flash_addr = flash_address;
			xfer.flash_addr_len = 3;
			xfer.pem_byte_len = ts->flash_read_pem_byte_len;
			xfer.dummy_byte_len = ts->flash_read_dummy_byte_len;
			xfer.rx_buf = tmpb.bin_data + Cnt;
			xfer.rx_len = MIN(file_size - i * 256, 256);
			ret = nvt_gcm_xfer(&xfer);
			if (ret) {
				NVT_ERR("Read Flash Data error");
				return -EAGAIN;
			}

			ret = ctp_hid_read(ts->mmap->read_flash_checksum_addr,
					   buf, 2);
			if (ret < 0) {
				NVT_ERR("Read Checksum error, i = %d", i);
				return -EAGAIN;
			}
			rd_checksum = (uint16_t)(buf[1] << 8 | buf[0]);

			// Calculate Checksum from Read Flash Data
			for (j = 0; j < MIN(file_size - i * 256, 256); j++) {
				rd_data_checksum += tmpb.bin_data[Cnt + j];
			}
			rd_data_checksum = 65535 - rd_data_checksum + 1;

			if (rd_checksum != rd_data_checksum) {
				NVT_ERR("Read checksum = 0x%X and calculated Checksum = 0x%X mismatch"
					", retry = %d",
					rd_checksum, rd_data_checksum, retry);
			} else {
				break;
			}
		}
		if (!retry)
			return -EAGAIN;
	}

	if (flash_addr_start == ts->fmap->flash_pid_addr) {
		snprintf(str, 5, "%c%c%c%c", tmpb.bin_data[2], tmpb.bin_data[3],
			 tmpb.bin_data[0], tmpb.bin_data[1]);
		NVT_DBG("PID = %c%c%c%c(%02X%02X%02X%02X)", tmpb.bin_data[2],
			tmpb.bin_data[3], tmpb.bin_data[0], tmpb.bin_data[1],
			tmpb.bin_data[2], tmpb.bin_data[3], tmpb.bin_data[0],
			tmpb.bin_data[1]);
		if (strtou16(str, 16, &ts->flash_pid)) {
			NVT_ERR("Can not convert pid");
			return -EINVAL;
		}
	} else {
		NVT_CLN_HEX(tmpb.bin_data, tmpb.bin_size);
	}

	return 0;
}

int32_t dump_flash_gcm_pid(void)
{
	uint32_t addr = 0, size = 0;
	int32_t ret = 0;

	addr = ts->fmap->flash_pid_addr;
	size = 4;

	NVT_DBG("Dump flash at addr 0x%X, size = %d", addr, size);

	if (addr + size > MAX_BIN_SIZE) {
		NVT_ERR("Addr(%X) + size(0x%X) is out of MAX_BIN_SIZE(%d)",
			addr, size, MAX_BIN_SIZE);
		ret = -EAGAIN;
		goto end;
	}

	ret = switch_gcm(1);
	if (ret)
		goto end;

	ret = resume_pd_gcm();
	if (ret)
		goto end;

	ret = read_flash_mid_did_gcm();
	if (ret)
		goto end;

	ret = read_flash_sector_gcm(addr, size);

end:
	return ret;
}

int32_t get_binary_and_flash_start_addr(char *path, uint8_t flash_option)
{
	int fd = 0, ret = 0;
	char end_char[4] = { 0 };

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		NVT_ERR("Can not open path %s, fd = %d", path, fd);
		ret = -EINVAL;
		goto end;
	}
	fwb.bin_size = lseek(fd, 0, SEEK_END);
	fwb.bin_data = malloc(fwb.bin_size);
	if (!fwb.bin_data) {
		ret = -ENOMEM;
		goto end;
	}
	lseek(fd, 0, SEEK_SET);
	if (fwb.bin_size != read(fd, fwb.bin_data, fwb.bin_size)) {
		NVT_ERR("Get binary failed");
		ret = -EAGAIN;
		goto end;
	}

	memcpy(end_char,
	       (char *)fwb.bin_data + fwb.bin_size - BIN_END_FLAG_LEN_MAX,
	       BIN_END_FLAG_LEN_MAX);
	if (flash_option == FLASH_FORCE) {
		NVT_LOG("Force flash without reading end flag");
		fwb.flash_start_addr = 0;
	} else if (memcmp(end_char, HID_FW_BIN_END_NAME_NORMAL,
			  BIN_END_FLAG_LEN_NORMAL) == 0) {
		if (!ts->fmap->flash_normal_fw_start_addr) {
			NVT_ERR("Normal FW requires flash start address");
			ret = -EFAULT;
			goto end;
		}
		fwb.flash_start_addr = ts->fmap->flash_normal_fw_start_addr;
	} else if (memcmp(end_char + 1, HID_FW_BIN_END_NAME_FULL,
			  BIN_END_FLAG_LEN_FULL) == 0) {
		if (ts->fmap->flash_normal_fw_start_addr) {
			NVT_ERR("Full FW flash start address should be 0");
			ret = -EFAULT;
			goto end;
		}
		fwb.flash_start_addr = ts->fmap->flash_normal_fw_start_addr;
	} else {
		NVT_ERR("binary end flag [%c%c%c%c] does not match [%s] or [%s], abort.",
			end_char[0], end_char[1], end_char[2], end_char[3],
			HID_FW_BIN_END_NAME_NORMAL, HID_FW_BIN_END_NAME_FULL);
		ret = -EINVAL;
		goto end;
	}

	NVT_LOG("Found HID FW bin file, end_char = [%c%c%c%c]", end_char[0],
		end_char[1], end_char[2], end_char[3]);
	NVT_LOG("Flashing starts from 0x%X", fwb.flash_start_addr);
	NVT_LOG("Size of bin = 0x%05X", fwb.bin_size);
	NVT_LOG("Get binary OK");
	close(fd);

	ret = 0;
end:
	return ret;
}

int32_t lock_flash_gcm(void)
{
	flash_mfr_t mid = 0;
	uint8_t status = 0;
	int32_t ret = 0;
	int32_t retry = 0;
	const flash_qeb_info_t *qeb_info_p = NULL;

	fflush(stdout);

	//check if flash lock method exist
	if (ts->match_finfo->lock_method == FLASH_LOCK_METHOD_UNKNOWN) {
		NVT_ERR("lock_method for flash is not specified");
		ret = -EINVAL;
		goto end;
	}

	// Write Enable
	ret = write_enable_gcm();
	if (ret < 0) {
		NVT_ERR("Write Enable error, ret = %d", ret);
		ret = -EAGAIN;
		goto end;
	}

	mid = ts->match_finfo->mid;
	qeb_info_p = &ts->match_finfo->qeb_info;
	if ((mid != FLASH_MFR_UNKNOWN) &&
	    (qeb_info_p->qeb_pos != QEB_POS_UNKNOWN)) {
		/* check if QE bit is in Status Register byte 1, if yes set it back to 1 */
		if (qeb_info_p->qeb_pos == QEB_POS_SR_1B) {
			status = (0x01 << qeb_info_p->qeb_order);
		} else {
			status = 0x00;
		}
		if (ts->match_finfo->lock_method ==
		    FLASH_LOCK_METHOD_SW_BP_ALL) {
			status |= ts->match_finfo->sr_bp_bits_all;
		}
		// Write Status Register
		ret = write_status_gcm(status);
		if (ret < 0) {
			NVT_ERR("Write Status Register error, ret = %d", ret);
			ret = -EAGAIN;
			goto end;
		}
		NVT_DBG("Write Status Register byte 0x%02X OK", status);
		msleep(1);
	}
	// Read Status
	retry = 0;
	while (1) {
		retry++;
		msleep(5);
		if (unlikely(retry > 100)) {
			NVT_ERR("Read Status failed, status = 0x%02X", status);
			ret = -EAGAIN;
			goto end;
		}
		ret = read_status_gcm(&status);
		if (ret < 0) {
			NVT_ERR("Read Status Register error, ret = %d", ret);
			continue;
		}
		if ((status & 0x03) == 0x00) {
			NVT_DBG("Read Status Register byte 0x%02X OK", status);
			break;
		}
	}

	NVT_LOG("Lock OK");

end:
	return ret;
}

int32_t update_firmware(char *path, uint8_t flash_option)
{
	int32_t ret = 0;

	NVT_LOG("Get binary and flash start address");
	ret = get_binary_and_flash_start_addr(path, flash_option);
	if (ret)
		goto end;

	NVT_LOG("Enable gcm");
	ret = switch_gcm(1);
	if (ret)
		goto end;

	NVT_LOG("Resume PD");
	ret = resume_pd_gcm();
	if (ret)
		goto end;

	NVT_LOG("Read MID DID");
	ret = read_flash_mid_did_gcm();
	if (ret)
		goto end;

	NVT_LOG("Erase");
	ret = erase_flash_gcm();
	if (ret)
		goto end;

	NVT_LOG("Program");
	ret = write_flash_gcm();
	if (ret)
		goto end;

	NVT_LOG("Verify");
	ret = verify_flash_gcm();
	if (ret)
		goto end;

	if (!ts->need_lock_flash) {
		ret = 0;
		goto end;
	} else {
		NVT_LOG("Lock");
		ret = lock_flash_gcm();
	}

end:
	return ret;
}

int32_t User_compare_bin_and_flash_sector_checksum(char *path)
{
	int32_t ret = 0;
	int32_t i = 0;
	int32_t total_sector_need_check = 0;
	int32_t j = 0;
	uint32_t flash_addr = 0;
	uint16_t data_len = 0;
	uint16_t write_checksum = 0;
	uint16_t read_checksum = 0;
	uint32_t fw_need_write_size = 0;

	NVT_LOG("Get binary and flash start address");
	ret = get_binary_and_flash_start_addr(path, FLASH_NORMAL);
	if (ret)
		goto end;

	ret = switch_gcm(1);
	if (ret)
		goto end;

	ret = resume_pd_gcm();
	if (ret)
		goto end;

	ret = read_flash_mid_did_gcm();
	if (ret)
		goto end;

	nvt_set_read_flash_method();

	fw_need_write_size = fwb.bin_size - fwb.flash_start_addr;
	NVT_LOG("Check fw checksum at addr 0x%X, size = 0x%05X",
		fwb.flash_start_addr, fw_need_write_size);

	total_sector_need_check = fw_need_write_size / SIZE_4KB;
	for (i = 0; i < total_sector_need_check; i++) {
		flash_addr = i * SIZE_4KB + fwb.flash_start_addr;
		data_len = SIZE_4KB;
		/* calculate write_checksum of each 4KB block */
		write_checksum = (flash_addr & 0xFF);
		write_checksum += ((flash_addr >> 8) & 0xFF);
		write_checksum += ((flash_addr >> 16) & 0xFF);
		write_checksum += ((data_len)&0xFF);
		write_checksum += (((data_len) >> 8) & 0xFF);
		for (j = 0; j < data_len; j++) {
			write_checksum += fwb.bin_data[flash_addr + j];
		}
		write_checksum = ~write_checksum + 1;

		ret = get_checksum_gcm(flash_addr, data_len, &read_checksum);
		if (ret < 0) {
			NVT_ERR("Get Checksum failed(%d,%d)", ret, i);
			ret = -EINVAL;
			goto end;
		}
		if (write_checksum != read_checksum) {
			NVT_ERR("firmware checksum not match, i=%d, write_checksum=0x%04X, read_checksum=0x%04X",
				i, write_checksum, read_checksum);
			ret = -EINVAL;
			goto end;
		} else {
			/* firmware checksum match */
			ret = 0;
		}
	}

	NVT_LOG("firmware checksum match");
end:
	return ret;
}
