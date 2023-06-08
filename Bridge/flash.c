#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "nt36xxx.h"
#include "nt36xxx_i2c.h"

#define CTP_I2C_WRITE(client, i2c_addr, buf, length) CTP_I2C_WRITE(__func__, i2c_addr, buf, length)
#define CTP_I2C_READ(client, i2c_addr, buf, length) CTP_I2C_READ(__func__, i2c_addr, buf, length)


/*******************************************************
Description:
	Novatek touchscreen check and stop crc reboot loop.

return:
	n.a.
*******************************************************/
void nvt_stop_crc_reboot(void)
{
	uint8_t buf[8] = {0};
	int32_t retry = 0;

	//read dummy buffer to check CRC fail reboot is happening or not

	//---change I2C index to prevent geting 0xFF, but not 0xFC---
	nvt_set_page(CHIP_VER_TRIM_ADDR);

	//---read to check if buf is 0xFC which means IC is in CRC reboot ---
	buf[2+0] = CHIP_VER_TRIM_ADDR & 0xFF;
	CTP_I2C_READ(__func__, I2C_BLDR_Address, buf, 4);

	//debug
	//printf("CRC dummy read check : buf[2+1]=0x%02X, buf[2+2]=0x%02X, buf[2+3]=0x%02X\n", buf[2+1], buf[2+2], buf[2+3]);

	if ((buf[2+1]==0xFC) ||
		((buf[2+1]==0xFF) && (buf[2+2]==0xFF) && (buf[2+3]==0xFF))) {

		//IC is in CRC fail reboot loop, needs to be stopped!
		for (retry = 5; retry > 0; retry--) {

			//---write i2c cmds to reset idle : 1st---
			buf[2+0]=0x00;
			buf[2+1]=0xA5;
			CTP_I2C_WRITE(__func__, I2C_HW_Address, buf, 2);

			//---write i2c cmds to reset idle : 2rd---
			buf[2+0]=0x00;
			buf[2+1]=0xA5;
			CTP_I2C_WRITE(__func__, I2C_HW_Address, buf, 2);
			msleep(1);

			//---clear CRC_ERR_FLAG---
			nvt_set_page(0x3F135);

			buf[2+0] = 0x35;
			buf[2+1] = 0xA5;
			CTP_I2C_WRITE(__func__, I2C_BLDR_Address, buf, 2);

			//---check CRC_ERR_FLAG---
			nvt_set_page(0x3F135);

			buf[2+0] = 0x35;
			buf[2+1] = 0x00;
			CTP_I2C_READ(__func__, I2C_BLDR_Address, buf, 2);

//printf("CRC Flag :  buf[2+1]=0x%02X\n", buf[2+1]);

			if (buf[2+1] == 0xA5)
				break;

			if (retry == 0)
				printf("CRC auto reboot is not able to be stopped! buf[1]=0x%02X\n", buf[1]);
		}
	}

	return;
}

static int32_t nvt_get_fw_need_write_size(int iFileSize, const char au8FileData[])
{
	int32_t i = 0;
	int32_t total_sectors_to_check = 0;

	total_sectors_to_check = iFileSize / FLASH_SECTOR_SIZE;

	for (i = total_sectors_to_check; i > 0; i--) {
	  	if (strncmp(   &au8FileData[i * FLASH_SECTOR_SIZE - NVT_FLASH_END_FLAG_LEN], "NVT", NVT_FLASH_END_FLAG_LEN) == 0) {
			fw_need_write_size = i * FLASH_SECTOR_SIZE;
  			NVT_LOG("fw_need_write_size = %zu(0x%zx),%zu k, NVT end flag\n", fw_need_write_size, fw_need_write_size ,fw_need_write_size/1024);
			return 0;
		}
	  	if (strncmp(   &au8FileData[i * FLASH_SECTOR_SIZE - NVT_FLASH_END_FLAG_LEN], "MOD", NVT_FLASH_END_FLAG_LEN) == 0) {
			fw_need_write_size = i * FLASH_SECTOR_SIZE;
  			NVT_LOG("fw_need_write_size = %zu(0x%zx),%zu k, MOD end flag\n", fw_need_write_size, fw_need_write_size ,fw_need_write_size/1024);
			return 0;
		}
	}

	NVT_ERR("end flag \"NVT\" \"MOD\" not found!\n");
	return -1;
}

int GetBinary(char *path, ParameterCheck_t enParaCheck)
{
	int fFile;
	fFile=open(path, O_RDONLY);
	if(fFile<0)
		return -1;
	iFileSize = lseek(fFile, 0, SEEK_END);
	lseek(fFile, 0, SEEK_SET);
	if(iFileSize!=(read(fFile, BUFFER_DATA, iFileSize)))
		return -1;

	if(enParaCheck == enDoCheck)
	{
		if(nvt_get_fw_need_write_size(iFileSize, (char*)BUFFER_DATA))
		{
			NVT_ERR("[Error]fileSize [0x%5X], get fw need to write size fail!\n",iFileSize);
			return -1;
		}else{
			iFileSize = fw_need_write_size;
		}

		if( (BUFFER_DATA[FW_BIN_VER_OFFSET]+BUFFER_DATA[FW_BIN_VER_BAR_OFFSET]) != 0xFF)
		{
			printf("[Error] FW info is broken! fw_ver=0x%2X,  ~fw_ver=0x%2X\n"
					,BUFFER_DATA[FW_BIN_VER_OFFSET] ,BUFFER_DATA[FW_BIN_VER_BAR_OFFSET]);
			return -1;
		}
	}
	close(fFile);
	return 0;
}
/*******************************************************
Description:
Novatek touchscreen initial bootloader and flash
block function.

return:
Executive outcomes. 0---succeed. negative---failed.
 *******************************************************/
int32_t Init_BootLoader(void)
{
	uint8_t buf[2+64] = {0};
	int32_t ret = 0;
	int32_t retry = 0;

	// SW Reset & Idle
	nvt_sw_reset_idle();

	// Initiate Flash Block
	buf[2+0] = 0x00;
	buf[2+1] = 0x00;
	buf[2+2] = I2C_FW_Address;
	ret = CTP_I2C_WRITE(__func__, I2C_HW_Address, buf, 3);
	if (ret < 0) {
		printf("Inittial Flash Block error!!(%d)\n", ret);
		return ret;
	}

	// Check 0xAA (Initiate Flash Block)
	retry = 0;
	while(1) {
		msleep(1);
		buf[2+0]=0x00;
		buf[2+1]=0x00;
		ret = CTP_I2C_READ(__func__, I2C_HW_Address, buf, 2);
		if (ret < 0) {
			printf("Check 0xAA (Inittial Flash Block) error!!(%d)\n", ret);
			return ret;
		}
		if (buf[2+1] == 0xAA) {
			break;
		}
		retry++;
		if (retry > 20) {
			printf("Check 0xAA (Inittial Flash Block) error!! status=0x%02X\n", buf[2+1]);
			return -1;
		}
	}

	printf("Init OK \n");
	msleep(20);

	return 0;
}


/*******************************************************
Description:
Novatek touchscreen resume from deep power down function.

return:
Executive outcomes. 0---succeed. negative---failed.
 *******************************************************/
int32_t Resume_PD(void)
{
	uint8_t buf[8] = {0};
	int32_t ret = 0;
	int32_t retry = 0;

	// Resume Command
	buf[2+0]=0x00;
	buf[2+1]=0xAB;
	ret = CTP_I2C_WRITE(__func__, I2C_HW_Address, buf, 2);
	if (ret < 0) {
		printf("Write Enable error!!(%d)\n", ret);
		return ret;
	}

	// Check 0xAA (Resume Command)
	retry = 0;
	while(1) {
		msleep(1);
		buf[2+0] = 0x00;
		buf[2+1] = 0x00;
		ret = CTP_I2C_READ(__func__, I2C_HW_Address, buf, 2);
		if (ret < 0) {
			printf("Check 0xAA (Resume Command) error!!(%d)\n", ret);
			return ret;
		}
		if (buf[2+1] == 0xAA) {
			break;
		}
		retry++;
		if (retry > 20) {
			printf("Check 0xAA (Resume Command) error!! status=0x%02X\n", buf[2+1]);
			return -1;
		}
	}
	msleep(10);

	printf("Resume PD OK\n");
	return 0;
}

int32_t Erase_Flash(int EraseLength, int StartSector)
{
	uint8_t buf[64] = {0};
	int32_t ret = 0;
	int32_t count = 0;
	int32_t i = 0;
	int32_t Flash_Address = 0;
	int32_t retry = 0;

	// Write Enable
	buf[2+0] = 0x00;
	buf[2+1] = 0x06;
	ret = CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 2);
	if (ret < 0) {
		NVT_ERR("Write Enable (for Write Status Register) error!!(%d)\n", ret);
		return ret;
	}
	// Check 0xAA (Write Enable)
	retry = 0;
	while (1) {
		mdelay(1);
		buf[2+0] = 0x00;
		buf[2+1] = 0x00;
		ret = CTP_I2C_READ(ts->client, I2C_HW_Address, buf, 2);
		if (ret < 0) {
			NVT_ERR("Check 0xAA (Write Enable for Write Status Register) error!!(%d)\n", ret);
			return ret;
		}
		if (buf[2+1] == 0xAA) {
			break;
		}
		retry++;
		if (unlikely(retry > 20)) {
			NVT_ERR("Check 0xAA (Write Enable for Write Status Register) error!! status=0x%02X\n", buf[2+1]);
			return -1;
		}
	}

	// Write Status Register
	buf[2+0] = 0x00;
	buf[2+1] = 0x01;
	buf[2+2] = 0x00;
	ret = CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 3);
	if (ret < 0) {
		NVT_ERR("Write Status Register error!!(%d)\n", ret);
		return ret;
	}
	// Check 0xAA (Write Status Register)
	retry = 0;
	while (1) {
		mdelay(1);
		buf[2+0] = 0x00;
		buf[2+1] = 0x00;
		ret = CTP_I2C_READ(ts->client, I2C_HW_Address, buf, 2);
		if (ret < 0) {
			NVT_ERR("Check 0xAA (Write Status Register) error!!(%d)\n", ret);
			return ret;
		}
		if (buf[2+1] == 0xAA) {
			break;
		}
		retry++;
		if (unlikely(retry > 20)) {
			NVT_ERR("Check 0xAA (Write Status Register) error!! status=0x%02X\n", buf[2+1]);
			return -1;
		}
	}

	// Read Status
	retry = 0;
	while (1) {
		mdelay(5);
		buf[2+0] = 0x00;
		buf[2+1] = 0x05;
		ret = CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 2);
		if (ret < 0) {
			NVT_ERR("Read Status (for Write Status Register) error!!(%d)\n", ret);
			return ret;
		}

		// Check 0xAA (Read Status)
		buf[2+0] = 0x00;
		buf[2+1] = 0x00;
		buf[2+2] = 0x00;
		ret = CTP_I2C_READ(ts->client, I2C_HW_Address, buf, 3);
		if (ret < 0) {
			NVT_ERR("Check 0xAA (Read Status for Write Status Register) error!!(%d)\n", ret);
			return ret;
		}
		if ((buf[2+1] == 0xAA) && (buf[2+2] == 0x00)) {
			break;
		}
		retry++;
		if (unlikely(retry > 100)) {
			NVT_ERR("Check 0xAA (Read Status for Write Status Register) failed, buf[2+1]=0x%02X, buf[2+2]=0x%02X, retry=%d\n", buf[2+1], buf[2+2], retry);
			return -1;
		}
	}

	if (EraseLength % FLASH_SECTOR_SIZE)
		count = EraseLength / FLASH_SECTOR_SIZE + StartSector + 1;
	else
		count = EraseLength / FLASH_SECTOR_SIZE + StartSector;

	//NVT_LOG("\t Sector Cnt=[%d]\n",count);///jx

	for(i = StartSector; i < count; i++) {
		// Write Enable
		buf[2+0] = 0x00;
		buf[2+1] = 0x06;
		ret = CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 2);
		if (ret < 0) {
			NVT_ERR("Write Enable error!!(%d,%d)\n", ret, i);
			return ret;
		}
		// Check 0xAA (Write Enable)
		retry = 0;
		while (1) {
			mdelay(1);
			buf[2+0] = 0x00;
			buf[2+1] = 0x00;
			ret = CTP_I2C_READ(ts->client, I2C_HW_Address, buf, 2);
			if (ret < 0) {
				NVT_ERR("Check 0xAA (Write Enable) error!!(%d,%d)\n", ret, i);
				return ret;
			}
			if (buf[2+1] == 0xAA) {
				break;
			}
			retry++;
			if (unlikely(retry > 20)) {
				NVT_ERR("Check 0xAA (Write Enable) error!! status=0x%02X\n", buf[2+1]);
				return -1;
			}
		}

		Flash_Address = i * FLASH_SECTOR_SIZE;

		// Sector Erase
		buf[2+0] = 0x00;
		buf[2+1] = 0x20;    // Command : Sector Erase
		buf[2+2] = ((Flash_Address >> 16) & 0xFF);
		buf[2+3] = ((Flash_Address >> 8) & 0xFF);
		buf[2+4] = (Flash_Address & 0xFF);
		ret = CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 5);
		if (ret < 0) {
			NVT_ERR("Sector Erase error!!(%d,%d)\n", ret, i);
			return ret;
		}
		// Check 0xAA (Sector Erase)
		retry = 0;
		while (1) {
			mdelay(1);
			buf[2+0] = 0x00;
			buf[2+1] = 0x00;
			ret = CTP_I2C_READ(ts->client, I2C_HW_Address, buf, 2);
			if (ret < 0) {
				NVT_ERR("Check 0xAA (Sector Erase) error!!(%d,%d)\n", ret, i);
				return ret;
			}
			if (buf[2+1] == 0xAA) {
				break;
			}
			retry++;
			if (unlikely(retry > 20)) {
				NVT_ERR("Check 0xAA (Sector Erase) failed, buf[2+1]=0x%02X, retry=%d\n", buf[2+1], retry);
				return -1;
			}
		}

		// Read Status
		retry = 0;
		while (1) {
			mdelay(5);
			buf[2+0] = 0x00;
			buf[2+1] = 0x05;
			ret = CTP_I2C_WRITE(ts->client, I2C_HW_Address, buf, 2);
			if (ret < 0) {
				NVT_ERR("Read Status error!!(%d,%d)\n", ret, i);
				return ret;
			}

			// Check 0xAA (Read Status)
			buf[2+0] = 0x00;
			buf[2+1] = 0x00;
			buf[2+2] = 0x00;
			ret = CTP_I2C_READ(ts->client, I2C_HW_Address, buf, 3);
			if (ret < 0) {
				NVT_ERR("Check 0xAA (Read Status) error!!(%d,%d)\n", ret, i);
				return ret;
			}
			if ((buf[2+1] == 0xAA) && (buf[2+2] == 0x00)) {
				break;
			}
			retry++;
			if (unlikely(retry > 100)) {
				NVT_ERR("Check 0xAA (Read Status) failed, buf[2+1]=0x%02X, buf[2+2]=0x%02X, retry=%d\n", buf[2+1], buf[2+2], retry);
				return -1;
			}
		}
	}

	printf("Erase OK \n");
	return 0;
}


/*******************************************************
Description:
Novatek touchscreen update firmware function.

return:
Executive outcomes. 0---succeed. negative---failed.
 *******************************************************/

int32_t Write_Flash(void)
{
	uint8_t buf[2+64] = {0};
	uint32_t XDATA_Addr = ts->mmap->RW_FLASH_DATA_ADDR;
	uint32_t Flash_Address = 0;
	int32_t i = 0, j = 0, k = 0;
	uint8_t tmpvalue = 0;
	int32_t count = 0;
	int32_t ret = 0;
	int32_t retry = 0;
	int32_t	i32STEP = 10;
	int32_t i32pre = 0,i32show = 0;

	// change I2C buffer index
	buf[2+0] = 0xFF;
	buf[2+1] = XDATA_Addr >> 16;
	buf[2+2] = (XDATA_Addr >> 8) & 0xFF;
	ret = CTP_I2C_WRITE(__func__, I2C_BLDR_Address, buf, 3);
	if (ret < 0) {
		printf("change I2C buffer index error!!(%d)\n", ret);
		return ret;
	}

	if(iFileSize%256)
		count=iFileSize/256+1;
	else
		count=iFileSize/256;

	for (i = 0; i < count; i++) {
		Flash_Address = i * 256;

		// Write Enable
		buf[2+0] = 0x00;
		buf[2+1] = 0x06;
		ret = CTP_I2C_WRITE(__func__, I2C_HW_Address, buf, 2);
		if (ret < 0) {
			printf("Write Enable error!!(%d)\n", ret);
			return ret;
		}
		// Check 0xAA (Write Enable)
		retry = 0;
		while (1) {
			usleep(100);
			buf[2+0] = 0x00;
			buf[2+1] = 0x00;
			ret = CTP_I2C_READ(__func__, I2C_HW_Address, buf, 2);
			if (ret < 0) {
				printf("Check 0xAA (Write Enable) error!!(%d)\n", ret);
				return ret;
			}
			if (buf[2+1] == 0xAA) {
				break;
			}
			retry++;
			if (retry > 20) {
				printf("Check 0xAA (Write Enable) error!! status=0x%02X\n", buf[2+1]);
				return -1;
			}
		}

		// Write Page : 256 bytes
		for (j = 0; j < min(iFileSize - i*256, 256); j+=32){
			buf[2+0 ]=(XDATA_Addr+j)&0xFF;
			for (k = 0; k < 32; k++) {
				buf[2+ 1 + k] = BUFFER_DATA[Flash_Address + j + k];
			}
			ret = CTP_I2C_WRITE(__func__, I2C_BLDR_Address, buf, 33);
			if (ret < 0) {
				printf("Write Page error!!(%d), j=%d\n", ret, j);
				return ret;
			}
		}
		if(iFileSize-Flash_Address>=256)
			tmpvalue=(Flash_Address >> 16) + ((Flash_Address >> 8) & 0xFF) + (Flash_Address & 0xFF) + 0x00 + (255);
		else
			tmpvalue=(Flash_Address>>16)+((Flash_Address>>8)&0xFF)+(Flash_Address&0xFF)+0x00+(iFileSize-Flash_Address-1);

		for(k=0;k<min(iFileSize-Flash_Address,256);k++)
			tmpvalue+=BUFFER_DATA[Flash_Address+k];

		tmpvalue=255-tmpvalue+1;

		// Page Program
		buf[2+0] = 0x00;
		buf[2+1] = 0x02;
		buf[2+2] = ((Flash_Address >> 16) & 0xFF);
		buf[2+3] = ((Flash_Address >> 8) & 0xFF);
		buf[2+4] = (Flash_Address & 0xFF);
		buf[2+5] = 0x00;
		buf[2+6] = min(iFileSize - Flash_Address,(size_t)256) - 1;
		buf[2+7] = tmpvalue;
		ret = CTP_I2C_WRITE(__func__, I2C_HW_Address, buf, 8);
		if (ret < 0) {
			printf("Page Program error!!(%d), i=%d\n", ret, i);
			return ret;
		}
		// Check 0xAA (Page Program)
		retry = 0;
		while (1) {
			msleep(1);
			buf[2+0] = 0x00;
			buf[2+1] = 0x00;
			ret = CTP_I2C_READ(__func__, I2C_HW_Address, buf, 2);
			if (ret < 0) {
				printf("Page Program error!!(%d)\n", ret);
				return ret;
			}
			if (buf[2+1] == 0xAA || buf[2+1] == 0xEA) {
				break;
			}
			retry++;
			if (retry > 20) {
				printf("Check 0xAA (Page Program) failed, buf[2+1]=0x%02X, retry=%d\n", buf[2+1], retry);
				return -1;
			}
		}
		if (buf[2+1] == 0xEA) {
			printf("Page Program error!! i=%d\n", i);
			return -3;
		}

		// Read Status
		retry = 0;
		while (1) {
			msleep(5);
			buf[2+0] = 0x00;
			buf[2+1] = 0x05;
			ret = CTP_I2C_WRITE(__func__, I2C_HW_Address, buf, 2);
			if (ret < 0) {
				printf("Read Status error!!(%d)\n", ret);
				return ret;
			}

			// Check 0xAA (Read Status)
			buf[2+0] = 0x00;
			buf[2+1] = 0x00;
			buf[2+2] = 0x00;
			ret = CTP_I2C_READ(__func__, I2C_HW_Address, buf, 3);
			if (ret < 0) {
				printf("Check 0xAA (Read Status) error!!(%d)\n", ret);
				return ret;
			}
			if (((buf[2+1] == 0xAA) && (buf[2+2] == 0x00)) || (buf[2+1] == 0xEA)) {
				break;
			}
			retry++;
			if (retry > 100) {
				printf("Check 0xAA (Read Status) failed, buf[2+1]=0x%02X, buf[2+2]=0x%02X, retry=%d\n", buf[2+1], buf[2+2], retry);
				return -1;
			}
		}
		if (buf[2+1] == 0xEA) {
			printf("Page Program error!! i=%d\n", i);
			return -4;
		}

		i32show=((i*100)/i32STEP/count);
		if( i32pre!=i32show){
			if( i32pre==0)
				printf("Programming...%2d%%", i32show*i32STEP);
			else
				printf("...%2d%%", i32show*i32STEP);
			fflush(stdout);
			i32pre=i32show;
		}
	}
	printf("...%2d%%\n", 100);
	printf("Program OK         \n");
	return 0;
}

/*******************************************************
Description:
Novatek touchscreen verify checksum of written
flash function.

return:
Executive outcomes. 0---succeed. negative---failed.
 *******************************************************/

int32_t Verify_Flash(void)
{
	uint8_t buf[2+64] = {0};
	uint32_t XDATA_Addr = ts->mmap->READ_FLASH_CHECKSUM_ADDR;
	int32_t ret = 0;
	int32_t i = 0;
	int32_t k = 0;
	uint16_t WR_Filechksum[BLOCK_64KB_NUM] = {0};
	uint16_t RD_Filechksum[BLOCK_64KB_NUM] = {0};
	size_t fw_bin_size = 0;
	size_t len_in_blk = 0;
	int32_t retry = 0;

	fw_bin_size = iFileSize;

	for (i = 0; i < BLOCK_64KB_NUM; i++) {
		if (fw_bin_size > (i * SIZE_64KB)) {
			// Calculate WR_Filechksum of each 64KB block
			len_in_blk = min(fw_bin_size - i * SIZE_64KB, (size_t)SIZE_64KB);
			WR_Filechksum[i] = i + 0x00 + 0x00 + (((len_in_blk - 1) >> 8) & 0xFF) + ((len_in_blk - 1) & 0xFF);
			for (k = 0; k < len_in_blk; k++) {
				WR_Filechksum[i] += BUFFER_DATA[k + i * SIZE_64KB];
			}
			WR_Filechksum[i] = 65535 - WR_Filechksum[i] + 1;

			// Fast Read Command
			buf[2+0] = 0x00;
			buf[2+1] = 0x07;
			buf[2+2] = i;
			buf[2+3] = 0x00;
			buf[2+4] = 0x00;
			buf[2+5] = ((len_in_blk - 1) >> 8) & 0xFF;
			buf[2+6] = (len_in_blk - 1) & 0xFF;
			ret = CTP_I2C_WRITE(__func__, I2C_HW_Address, buf, 7);
			if (ret < 0) {
				printf("Fast Read Command error!!(%d)\n", ret);
				return ret;
			}
			// Check 0xAA (Fast Read Command)
			retry = 0;
			while (1) {
				msleep(80);
				buf[2+0] = 0x00;
				buf[2+1] = 0x00;
				ret = CTP_I2C_READ(__func__, I2C_HW_Address, buf, 2);
				if (ret < 0) {
					printf("Check 0xAA (Fast Read Command) error!!(%d)\n", ret);
					return ret;
				}
				if (buf[2+1] == 0xAA) {
					break;
				}
				retry++;
				if (retry > 5) {
					printf("Check 0xAA (Fast Read Command) failed, buf[2+1]=0x%02X, retry=%d\n", buf[2+1], retry);
					return -1;
				}
			}
			// Read Checksum (write addr high byte & middle byte)
			buf[2+0] = 0xFF;
			buf[2+1] = XDATA_Addr >> 16;
			buf[2+2] = (XDATA_Addr >> 8) & 0xFF;
			ret = CTP_I2C_WRITE(__func__, I2C_BLDR_Address, buf, 3);
			if (ret < 0) {
				printf("Read Checksum (write addr high byte & middle byte) error!!(%d)\n", ret);
				return ret;
			}
			// Read Checksum
			buf[2+0] = (XDATA_Addr) & 0xFF;
			buf[2+1] = 0x00;
			buf[2+2] = 0x00;
			ret = CTP_I2C_READ(__func__, I2C_BLDR_Address, buf, 3);
			if (ret < 0) {
				printf("Read Checksum error!!(%d)\n", ret);
				return ret;
			}

			RD_Filechksum[i] = (uint16_t)((buf[2+2] << 8) | buf[2+1]);
			if (WR_Filechksum[i] != RD_Filechksum[i]) {
				printf("Verify Fail%d!!\n", i);
				printf("RD_Filechksum[%d]=0x%04X, WR_Filechksum[%d]=0x%04X\n", i, RD_Filechksum[i], i, WR_Filechksum[i]);
				return -1;
			}
		}
	}

	printf("Verify OK \n");
	return 0;
}

/*******************************************************
Description:
Novatek touchscreen update firmware by BLD function.

return:
Executive outcomes. 0---succeed. negative---failed.
 *******************************************************/
int32_t UpdateFW_BLD(ParameterCheck_t enParaCheck)
{
	int32_t ret = 0;

	// Step 1 : initial bootloader
	ret = Init_BootLoader();
	if(ret)	{
		return ret;
	}

	// Step 2 : Resume PD
	ret = Resume_PD();
	if(ret)	{
		return ret;
	}

	// Step 3 : Erase
	ret = Erase_Flash(iFileSize, 0);
	if(ret)	{
		return ret;
	}

	// Stap 4 : Program
	ret = Write_Flash();
	if(ret)	{
		return ret;
	}

	// Stap 5 : Verify
	ret = Verify_Flash();
	if(ret)	{
		return ret;
	}

	// Step 6 : Bootloader Reset
	nvt_bootloader_reset();

	return ret;
}

/*******************************************************
Description:
Novatek touchscreen update firmware function.

return:
Executive outcomes. 0---succeed. negative---failed.
 *******************************************************/
int32_t UpdateFW(char *path, ParameterCheck_t enParaCheck)
{
	int32_t ret = 0;

	ret = GetBinary(path, enParaCheck);
	if(ret)	{
		printf("Get Binary FAIL\n");
		return 0;
	}

	nvt_sw_reset_idle();

	//---Stop CRC check to prevent IC auto reboot---
	nvt_stop_crc_reboot();

	ret = UpdateFW_BLD(enParaCheck);

	return ret;
}

int Read_Flash_Sector(uint32_t uFlash_AddrStart,uint32_t muFileSize)
{
	uint8_t buf[2+64]={0};
	char btmp[256]={0};
	FILE *pFile=0;
	uint32_t XDATA_Addr = ts->mmap->READ_FLASH_CHECKSUM_ADDR;//0x14000;
	uint32_t Flash_Address=0;
	uint32_t u32Cnt=0;
	uint32_t u32_SleepTime=300;
	int i=0, j=0, k=0;
	int count=0;
	uint16_t RD_Checksum=0, RD_Data_Checksum=0;
	uint16_t u16Polling=0;
	int32_t ret = 0;

	//<CheckParameter>
	if( uFlash_AddrStart + muFileSize > FW_512k)
	{
		printf("[Error!]@Read_Flash_Sector(),Start[0x%X],Size[0x%X]; MaxAddr [0x%X] > FW_512k\n"
				,uFlash_AddrStart ,muFileSize, uFlash_AddrStart + muFileSize);
		return -1;
	}
	else
	{
		printf("@Read_Flash_Sector(),Address_Start=[0x%X],Size=[0x%X]; MaxAddr is[0x%X]\n"
				,uFlash_AddrStart ,muFileSize, uFlash_AddrStart + muFileSize);
	}
	if((uFlash_AddrStart==0)&&(muFileSize == FW_116k))
		sprintf(btmp, "/data/local/tmp/dump_116k.bin");
	else if((uFlash_AddrStart==0)&&(muFileSize == FW_128k))
		sprintf(btmp, "/data/local/tmp/dump_128k.bin");
	else if((uFlash_AddrStart==0)&&(muFileSize == FW_256k))
		sprintf(btmp, "/data/local/tmp/dump_256k.bin");
	else if((uFlash_AddrStart==0)&&(muFileSize == FW_512k))
		sprintf(btmp, "/data/local/tmp/dump_512k.bin");
	else
		sprintf(btmp, "/data/local/tmp/dump.bin");

	pFile=fopen(btmp, "wb+");
	if(pFile == NULL) {
		printf("Open file %s failed\n", btmp);
		return -1;
	}

	// Disable Flash Protect
	buf[2+0]=0x00;
	buf[2+1]=0x35;
	ret = CTP_I2C_WRITE(__func__, I2C_HW_Address, buf, 2);
	if(ret<0)
	{
		printf("%s: Disable Flash Protect error!!\n", __func__);
		return -1;
	}
	usleep(u32_SleepTime);

	if(muFileSize%256)
		count=muFileSize/256+1;
	else
		count=muFileSize/256;

	for(i=0; i<count; i++)
	{
read_page_again:
		u32Cnt = i * 256;
		Flash_Address=i*256+uFlash_AddrStart;

		RD_Data_Checksum=0;
		RD_Data_Checksum+=((Flash_Address>>16)&0xFF);	// Addr_H
		RD_Data_Checksum+=((Flash_Address>>8)&0xFF);	// Addr_M
		RD_Data_Checksum+=(Flash_Address&0xFF);			// Add_L
		RD_Data_Checksum+=(((min(muFileSize-i*256, 256)-1)>>8)&0xFF);	// Length_H
		RD_Data_Checksum+=((min(muFileSize-i*256, 256)-1)&0xFF);	// Length_L

		// Read Flash
		buf[2+0]=0x00;
		buf[2+1]=0x03;
		buf[2+2]=(Flash_Address>>16)&0xFF;
		buf[2+3]=(Flash_Address>>8)&0xFF;
		buf[2+4]=Flash_Address&0xFF;
		buf[2+5]=((min(muFileSize-i*256, 256)-1)>>8)&0xFF;
		buf[2+6]=(min(muFileSize-i*256, 256)-1)&0xFF;
		ret = CTP_I2C_WRITE(__func__, I2C_HW_Address, buf, 7);
		if(ret<0)
		{
			printf("%s: Read Flash error!!\n", __func__);
			return -1;
		}

		for(u16Polling=0;u16Polling<1000;u16Polling++)
		{//<SafeWait>
			usleep(1);
			// Check 0xAA (Read flash)
			buf[2+0]=0x00;
			buf[2+1]=0x00;
			ret = CTP_I2C_READ(__func__, I2C_HW_Address, buf, 2);
			if (ret < 0) {
				printf("%s: Check 0xAA (Read Flash) error!!\n", __func__);
				return -1;
			}
			if(buf[2+1]==0xAA)
			{
				//if(u16Polling>=2)
				//printf("SafeWait=[%d]\n",u16Polling);
				break;
			}
		}

		if(buf[2+1]!=0xAA)
		{
			printf("%s: Check 0xAA (Read Flash) error!! status=0x%02X\n", __func__, buf[2+1]);
			return -1;
		}

		// Read Data and Checksum
		buf[2+0] = 0xFF;
		buf[2+1] = XDATA_Addr>>16;
		buf[2+2] = (XDATA_Addr>>8)&0xFF;
		ret = CTP_I2C_WRITE(__func__, I2C_FW_Address, buf, 3);
		if(ret<0)
		{
			printf("%s: change I2C buffer index error!!\n", __func__);
			return -1;
		}

		// Read Checksum first
		buf[2+0] = XDATA_Addr&0xFF;
		buf[2+1] = 0;
		buf[2+2] = 0;
		ret = CTP_I2C_READ(__func__, I2C_FW_Address, buf, 3);
		if(ret<0)
		{
			printf("%s: Read Checksum error!!, i=%d\n", __func__, i);
			return -1;
		}
		RD_Checksum = (uint16_t)(buf[2+2]<<8|buf[2+1]);
		//printf("%s: RD_Checksum = 0x%X\n", __func__, RD_Checksum);

		// Read Flash Data
		for(j=0; j<min(muFileSize-i*256, 256); j+=32)
		{
			buf[2+0]=(2+XDATA_Addr+j)&0xFF;
			ret = CTP_I2C_READ(__func__, I2C_FW_Address, buf, 33);
			if(ret<0)
			{
				printf("%s: Read Data error!!, j=%d\n", __func__, j);
				return -2;
			}

			for(k=0; k<32; k++)
			{
				BUFFER_DATA[u32Cnt++]=buf[2+1+k];
				RD_Data_Checksum+=buf[2+1+k];
				//printf("%02X ", buf[2+1+k]);
			}
			//printf("\n");
		}
		RD_Data_Checksum=65535-RD_Data_Checksum+1;
		//printf("RD_Data_Checksum=0x%X\n", RD_Data_Checksum);

		if(RD_Checksum != RD_Data_Checksum)
		{
			printf("%s: Checksum Read(0x%X) and Data Read calculated Checksum(0x%X) mismatch!\n", __func__, RD_Checksum, RD_Data_Checksum);
			goto read_page_again;
		}
	}

	fwrite(BUFFER_DATA, muFileSize, 1, pFile);
	printf("Dump to %s\n",btmp);
	fclose(pFile);

	return 0;
}
