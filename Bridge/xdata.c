#include "nt36xxx_i2c.h"
#include "limits.h"
#include <errno.h>

int32_t CTP_I2C_READ(const char* caller_Fun, uint16_t i2c_addr, uint8_t *buf, uint16_t len)
{
	nvt_read_addr_hid(i2c_addr, buf, len);
	return 0;
}

int32_t CTP_I2C_WRITE(const char* caller_Fun, uint16_t i2c_addr, uint8_t *buf, uint16_t len)
{
	nvt_write_addr_hid(i2c_addr, buf, len);
	return 0;
}

int32_t nvt_set_page(uint32_t addr)
{
	uint8_t buf[2 + 3] = {0};
	buf[2 + 0] = 0xFF;	//Set Index/Page/Addr command
	buf[2 + 1] = _get8Bit_fromU32(addr, 2);   //[Add_H]
	buf[2 + 2] = _get8Bit_fromU32(addr, 1);   //[Add_M]

	return CTP_I2C_WRITE(__func__, I2C_FW_Address, buf, 3);
}

int32_t xI2CWrite(int32_t argc, char* main_argv[], ICMSType isMS)
{
	uint16_t u16Len,u16Loop;
	uint8_t buf[3 + NVT_TRANSFER_LEN];
	uint32_t u32Addr;
	uint8_t u8AddrL;
	int32_t ret = 0;

	u16Len = (uint16_t)(strtoul(main_argv[4], NULL, 10)&0xFFFF);
	if ( (argc-u16Len) != 4 ) {
		printf("\tError: argc=%d, u8Len=%d lenDiff(%d)!=5\n", argc, u16Len, argc-u16Len);
		return -1;
	}

	u32Addr = (uint32_t)(strtoul(main_argv[3], NULL, 16)&0xFFFFFFFF);
	u16Len  = (uint16_t)(strtoul(main_argv[4], NULL, 10)&0xFFFF);
	u8AddrL = _get8Bit_fromU32(u32Addr, 0);

	printf("Write Addr[0x%X]Len[%d]AddrL[0x%X]\n",u32Addr,u16Len,u8AddrL);

	//<set addr><write>
	nvt_set_page(u32Addr);
	buf[2+0]= u8AddrL;
	for (u16Loop = 0; u16Loop < u16Len; u16Loop++){
		buf[2+ 1 + u16Loop] = (uint8_t)(strtoul(main_argv[5 + u16Loop], NULL, 16)&0xFF);
	}
	ret = CTP_I2C_WRITE(__func__, I2C_FW_Address, buf,  1 + u16Len);//1 is AddrL
	if (ret) {
		printf("\tError:read data fail!\n");
	}

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);

	return 0;
}

uint32_t parseUnit(uint8_t* buf, char* sUnitType)
{
	uint8_t uintLen=0;
	uint32_t type = Unit_ERR;
	uint8_t u8Loop=0;

	if (strcmp(sUnitType, "U8") == 0){
		type=Unit_U8;
		uintLen=1;
	}else if (strcmp(sUnitType, "I8") == 0){
		type=Unit_I8;
		uintLen=1;
	}else if (strcmp(sUnitType, "U16") == 0){
		type=Unit_U16;
		uintLen=2;
	}else if (strcmp(sUnitType, "I16") == 0){
		type=Unit_I16;
		uintLen=2;
	}else if (strcmp(sUnitType, "U32") == 0){
		type=Unit_U32;
		uintLen=4;
	}else if (strcmp(sUnitType, "I32") == 0){
		type=Unit_I32;
		uintLen=4;
	}else{
		type=Unit_ERR;
		uintLen=0;
		return uintLen;
	}


	//<Init>
		cell.u32=0;
	//<Move>
	for(u8Loop=0; u8Loop < uintLen; u8Loop ++){
		cell.u8[u8Loop]=buf[u8Loop];
	}

	switch(type){
		case Unit_U8:	printf("%5d",cell.u8[0]);		break;
		case Unit_I8:	printf("%5d",cell.i8[0]);		break;
		case Unit_U16:	printf("%5d",cell.u16[0]);		break;
		case Unit_I16:	printf("%5d",cell.i16[0]);		break;
		case Unit_U32:	printf("%5d",cell.u32);		break;
		case Unit_I32:	printf("%5d",cell.i32);		break;
	}
	return uintLen;
}

int32_t xI2CRead(int32_t argc,	char* main_argv[], ICMSType isMS)
{
	DataShowType EnShowData = enShow_Off;
	uint8_t buf[2+NVT_TRANSFER_LEN];

	uint8_t u8ID;
	uint32_t u32Addr;
	uint32_t u32Len;
	uint16_t u16stageLEN = NVT_TRANSFER_LEN;
	uint32_t u32Loop = 0;
	uint32_t u32Cnt = 0;
	uint16_t u16PrtCnt;

	EnShowData = enShow_On;
	u8ID	= I2C_FW_Address;
	u32Addr = (uint32_t)(strtoul(main_argv[3], NULL, 16)&0xFFFFFFFF);
	u32Len  = (uint32_t)(strtoul(main_argv[4], NULL, 10)&0xFFFF);

	if (EnShowData != enShow_Off)
		printf("\tShow Addr[0x%06X]Len[%3d]\n", u32Addr, u32Len);

	u32Loop = u32Len / u16stageLEN;
	if(u32Len % u16stageLEN)
		u32Loop += 1;

	for(u32Cnt = 0; u32Cnt < u32Loop; u32Cnt ++)
	{
		if (_get8Bit_fromU32(u32Addr, 0) == 0xFF){
			printf("\tError:Can't Acess the offset 0xFF of %5X\n", u32Addr);
			return -1;
		}
		if (unlikely(u32Cnt == u32Len / u16stageLEN)){
			u16stageLEN = u32Len % u16stageLEN;
		}

		nvt_set_page(u32Addr);
		buf[2+0] = _get8Bit_fromU32(u32Addr, 0);
		CTP_I2C_READ(__func__, u8ID, buf, 1 + u16stageLEN );

		for (u16PrtCnt = 0; u16PrtCnt < u16stageLEN; u16PrtCnt++){
			if (unlikely(u16PrtCnt == 0))
				;//Do Nothing
			else if (u16PrtCnt%16 == 0)
				printf("\n");
			else if (u16PrtCnt%8 == 0)
				printf("\t");
		    printf("%02X ", buf[2+1+u16PrtCnt]);
		}
		printf("\n");
		u32Addr += (uint32_t)u16stageLEN;
	}
	printf("\n");

	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);
	return 0;
}

void SW_Reset()
{
	uint8_t buf[4];

	buf[0] = I2C_HW_Address | I2C_W;
	buf[1] = 2;
	buf[2] = 0x00;
	buf[3] = 0x5A;     //5AH = Reset MCU. (no boot)
	CTP_I2C_WRITE(__func__, I2C_HW_Address, buf, 2);
	printf("SW_Reset\n");
}


void nvt_bootloader_reset(void)
{
	uint8_t buf[4];

	buf[0] = I2C_HW_Address | I2C_W;
	buf[1] = 2;
	buf[2] = 0x00;
	buf[3] = 0x69;     //69H = Reset MCU. (boot)
	CTP_I2C_WRITE(__func__, I2C_HW_Address, buf, 2);
	printf("nvt_bootloader_reset\n");
}

void nvt_change_mode(uint8_t mode)
{
	uint8_t buf[2+8] = {0};

	//---set xdata index to EVENT BUF ADDR---
	nvt_set_page(ts->mmap->EVENT_BUF_ADDR);

	//---set mode---
	buf[2+0] = EVENT_MAP_HOST_CMD;
	buf[2+1] = mode;
	CTP_I2C_WRITE(__func__, I2C_FW_Address, buf, 2);

	if (mode == NORMAL_MODE) {
		buf[2+0] = EVENT_MAP_HANDSHAKING_or_SUB_CMD_BYTE;
		buf[2+1] = HANDSHAKING_HOST_READY;
		CTP_I2C_WRITE(__func__, I2C_FW_Address, buf, 2);
		msleep(20);
	}
}

void nvt_sw_reset_idle()
{
	uint8_t buf[4]={0};

	//---write i2c cmds to reset idle---
	buf[2+0]=0x00;
	buf[2+1]=0xA5;
	CTP_I2C_WRITE(__func__, I2C_HW_Address, buf, 2);

	printf("%s \n",__func__);

	msleep(10);
}

int32_t nvt_write_addr(uint32_t addr, uint8_t data)
{
	int32_t ret = 0;
	uint8_t buf[5] = {0};

	nvt_set_page(addr);

	//---write data to index---
	buf[2+0] = addr & (0xFF);
	buf[2+1] = data;
	ret = CTP_I2C_WRITE(__func__, I2C_FW_Address, buf, 2);
	if (ret) {
		NVT_ERR("write data to 0x%06X failed, ret = %d\n", addr, ret);
		return ret;
	}

    return ret;
}
