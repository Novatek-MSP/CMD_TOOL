#ifndef     _LINUX_NVT_NT36XXX_I2C_H
#define     _LINUX_NVT_NT36XXX_I2C_H

/*
 * include header file
 */
#include "nt36xxx.h"
#include "nt36xxx_mem_map.h"

/*
 * variable define
 */
#define I2C_W              0x00	// I2C Write
#define I2C_R              0x80	// I2C Read
#define NVT_TRANSFER_LEN   64	//max is 64

/*
 * extern functions
 */
int32_t CTP_I2C_READ(const char* caller_Fun, uint16_t I2C_addr, uint8_t *buf, uint16_t len);
int32_t CTP_I2C_WRITE(const char* caller_Fun, uint16_t I2C_addr, uint8_t *buf, uint16_t len);
int32_t xI2CRead(int32_t argc,	char* main_argv[], ICMSType isMS);
int32_t xI2CWrite(int32_t argc,	char* main_argv[], ICMSType isMS);
int32_t nvt_set_page(uint32_t addr);
int32_t nvt_write_addr(uint32_t addr, uint8_t data);
int32_t nvt_write_reg(nvt_ts_reg_t reg, uint8_t val);
int32_t UpdateFW(char *, ParameterCheck_t );
void nvt_bootloader_reset(void);
void nvt_sw_reset_idle();
void SW_Reset();
int32_t nvt_read_addr_hid(uint8_t i2c_addr, uint8_t *data, uint16_t len);
int32_t nvt_write_addr_hid(uint8_t i2c_addr, uint8_t *data, uint16_t len);

#endif	/* _LINUX_NVT_NT36XXX_I2C_H */
