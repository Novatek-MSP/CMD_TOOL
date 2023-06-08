/* Linux */
#include <linux/types.h>
#include <linux/input.h>
#include <linux/hidraw.h>

/* hid */
#ifndef HIDIOCSFEATURE
#warning Please have your distro update the userspace kernel headers
#define HIDIOCSFEATURE(len)    _IOC(_IOC_WRITE|_IOC_READ, 'H', 0x06, len)
#define HIDIOCGFEATURE(len)    _IOC(_IOC_WRITE|_IOC_READ, 'H', 0x07, len)
#endif

/* Unix */
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* C */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

/* Touch */
#include "nt36xxx_i2c.h"
#define REPORT_ID_ENG 0x0B
#define REPORT_SIZE 64

int32_t nvt_read_addr_hid(uint8_t i2c_addr, uint8_t *data, uint16_t len)
{
	int res = 0, retry = 100;
	uint8_t *buf;

	buf = malloc(sizeof(uint8_t) * REPORT_SIZE);
	memset(buf, 0, REPORT_SIZE);

	// Bridge CMD_I2C_BASIC_READ requires addtional write to setup address LSB
	if (i2c_addr != I2C_HW_Address)
		nvt_write_addr_hid(i2c_addr, data, 1);

	buf[0] = REPORT_ID_ENG; /* Report Number */
	buf[1] = 0x42; /* CMD */
	buf[2] = 0x00; /* TAG */
	buf[3] = 0x00; /* TAG */
	buf[4] = i2c_addr; /* SLAVE_ADDR */
	buf[5] = (uint8_t)len; /* LEN */
	res = ioctl(fdev_hid, HIDIOCSFEATURE(REPORT_SIZE), buf);

	while (retry) {
		usleep(100);
		res = ioctl(fdev_hid, HIDIOCGFEATURE(REPORT_SIZE), buf);
		if (buf[1] == 0xAA)
			break;
		retry--;
	}

	if (res < 0 || retry == 0) {
		perror("nvt_read_addr_hid failed");
		return -1;
	}

	memcpy(data+3,  buf+4, len);	/* data */
	return 0;
}

int32_t nvt_write_addr_hid(uint8_t i2c_addr, uint8_t *data, uint16_t len)
{
	int res = 0, retry = 100;
	uint8_t *buf = NULL;

	buf = malloc(sizeof(uint8_t) * REPORT_SIZE);
	memset(buf, 0, REPORT_SIZE);

	buf[0] = REPORT_ID_ENG; /* Report Number */
	buf[1] = 0x41; /* CMD */
	buf[2] = 0x00; /* TAG */
	buf[3] = 0x00; /* TAG */
	buf[4] = i2c_addr; /* SLAVE_ADDR */
	buf[5] = (uint8_t)len; /* LEN */
	memcpy(buf + 6, data + 2, len);	/* data */
	res = ioctl(fdev_hid, HIDIOCSFEATURE(REPORT_SIZE), buf);

	while (retry) {
		usleep(100);
		res = ioctl(fdev_hid, HIDIOCGFEATURE(REPORT_SIZE), buf);
		if (buf[1] == 0xAA)
			break;
		retry--;
	}

	if (res < 0 || retry == 0) {
		perror("nvt_write_addr_hid failed");
		return -1;
	}
	return res;
}
