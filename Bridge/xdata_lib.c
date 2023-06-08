#include <stdint.h>
#include "nt36xxx.h"
#include "nt36xxx_i2c.h"

struct timespec tsNow, tsLast, tsGap;

int32_t nvt_get_fw_ver(void)
{
	uint8_t buf[5] = {0};
	uint32_t retry = 3;

	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_FWINFO);

	while (retry) {
		buf[2] = EVENT_MAP_FWINFO;
		CTP_I2C_READ(__func__, I2C_FW_Address, buf, 2);
		if ((buf[3] + buf[4]) != 0xFF)
			retry--;
		else
			break;
	}

	if (retry == 0)
		ts->fw_ver = 0;
	else
		ts->fw_ver = buf[3];

	// Do not print fw version to std, this will interrupt shell update
	//printf("fw version = %d\n", ts->fw_ver);
	return ts->fw_ver;
}

int32_t nvt_get_fw_info(void)
{
	uint8_t buf[2+64] = {0};
	uint32_t retry_count = 0;
	int32_t ret = 0;

info_retry:
	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR | EVENT_MAP_FWINFO);

	//---read fw info---
	buf[2+0] = EVENT_MAP_FWINFO;
	CTP_I2C_READ(__func__, I2C_FW_Address, buf, 39);
	if ( (buf[2+1]+buf[2+2]) != 0xFF ) {
		NVT_ERR("FW info is broken! fw_ver=0x%02X, ~fw_ver=0x%02X\n", buf[1], buf[2]);
		if(retry_count < 3) {
			retry_count++;
			NVT_ERR("retry_count=%d\n", retry_count);
			goto info_retry;
		} else {
			ts->fw_ver = 0;
			ts->abs_x_max = TOUCH_DEFAULT_MAX_WIDTH;
			ts->abs_y_max = TOUCH_DEFAULT_MAX_HEIGHT;
			NVT_ERR("Set default fw_ver=%d, abs_x_max=%d, abs_y_max=%d\n",
					ts->fw_ver, ts->abs_x_max, ts->abs_y_max);
			ret = -1;
			goto out;
		}
	}

	ts->fw_ver = buf[2+1];
	ts->x_num  = buf[2+3];
	ts->y_num  = buf[2+4];
	ts->abs_x_max = (uint16_t)((buf[2+5] << 8) | buf[2+6]);
	ts->abs_y_max = (uint16_t)((buf[2+7] << 8) | buf[2+8]);
	ts->query_config_ver = (uint16_t)((buf[2+20] << 8) | buf[2+19]);
	ts->nvt_pid = (uint16_t)((buf[2+36] << 8) | buf[2+35]);
	ts->button_num = buf[2+11];

	printf("fw_ver=0x%02X, fw_type=0x%02X, x_num=%d, y_num=%d, button_num=%d PID=0x%04X\n"
			, ts->fw_ver, buf[2+14], ts->x_num, ts->y_num, ts->button_num, ts->nvt_pid);

	ret = 0;
out:

	return ret;
}

void nvt_get_trim_id(void)
{
	uint8_t buf[9] = {0};
	nvt_sw_reset_idle();
	nvt_set_page(CHIP_VER_TRIM_ADDR);
	buf[2+0] = CHIP_VER_TRIM_ADDR & 0xFF;
	CTP_I2C_READ(__func__, I2C_FW_Address, buf, 6);
	printf("trim ID : %02x %02x %02x %02x %02x %02x\n",buf[3],buf[4],buf[5],buf[6],buf[7],buf[8]);
	nvt_bootloader_reset();
}
