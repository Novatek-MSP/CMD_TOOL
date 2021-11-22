// SPDX-License-Identifier: GPL-2.0
/*
 * Hidraw Userspace Example
 *
 * Copyright (c) 2010 Alan Ott <alan@signal11.us>
 * Copyright (c) 2010 Signal 11 Software
 *
 * The code may be used by anyone for any purpose,
 * and can serve as a starting point for developing
 * applications using hidraw.
 */

/* Linux */
#include <linux/types.h>
#include <linux/input.h>
#include <linux/hidraw.h>

/*
 * Ugly hack to work around failing compilation on systems that don't
 * yet populate new version of hidraw.h to userspace.
 */
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

#define ERR 255
#define LDROM_MODE 1
#define APROM_MODE 2

#define PID_ADDR 0x1000
#define REPORT_ID 0x07
#define REPORT_SIZE 66
#define CMD_GET_APROM_VER 0x00
#define CMD_UPDATE_APROM 0xa0
#define CMD_SYNC_PACKNO 0xa4
#define CMD_BACK_LDROM 0xac
#define CMD_LDROM_RESET 0xad
#define CMD_WRITE_CHECKSUM  0xb0
#define CMD_GET_DEVICEID 0xb1
#define NO_AA_CHECK 0xff

#define PF(fmt, ...) printf(fmt"\n\n", ##__VA_ARGS__)
#define PE(fmt, ...) printf("Error : "fmt"\n\n", ##__VA_ARGS__)

/*
 * xbuf[0] = REPOERT_ID
 * xbuf[1] = STATUS 05/AA
 * xbuf[2~65] = DATA
 */
char xbuf[66];
char* buf_bin;
unsigned int fsize;
int fdev_hid;
unsigned int bin_aprom_len;
unsigned int bin_aprom_chksum;
char node_path[256];

void rst_xbuf(void)
{
	memset(xbuf, 0, REPORT_SIZE);
}

char hid_wr(char* buf)
{
	int res, retry = 100, ret_check = 1;

	if (!buf[0])
		buf[0] = REPORT_ID;

	if (buf[1] == NO_AA_CHECK) {
		ret_check = 0;
		buf[1] = 0x00;
	}

	res = ioctl(fdev_hid, HIDIOCSFEATURE(REPORT_SIZE), buf);

	if (!ret_check) {
		res = ioctl(fdev_hid, HIDIOCGFEATURE(REPORT_SIZE), buf); // dummy read
		usleep(50000);
		return 0;
	}

	while (retry) {
		usleep(1000);
		res = ioctl(fdev_hid, HIDIOCGFEATURE(REPORT_SIZE), buf);
		if (buf[1] == 0xAA)
			break;
		retry--;
	}

	if (res < 0 || retry == 0) {
		PE("hid_wr failed, res = %d", res);
		return ERR;
	}

	return 0;
}

char get_dev_id(void)
{
	int ret;

	rst_xbuf();
	xbuf[2] = CMD_GET_DEVICEID;
	xbuf[6] = 7;
	ret = hid_wr(xbuf);
	if (ret)
		return ret;
	if (strncmp("LDROM", xbuf + 14, 5) == 0)
		ret = LDROM_MODE;
	else if (strncmp("APROM", xbuf + 14, 5) == 0)
		ret = APROM_MODE;
	else
		ret = ERR;
	return ret;
};

// madatory before flashing
char jump_to_ldrom(void)
{
	rst_xbuf();
	xbuf[1] = NO_AA_CHECK;
	xbuf[2] = CMD_BACK_LDROM;
	hid_wr(xbuf);
	if (get_dev_id() == LDROM_MODE) {
		PF("Device jumps to ldrom");
		return 0;
	} else {
		PE("Failed to jump to ldrom");
		return ERR;
	}
}

void connect_to_ldrom(void)
{
	rst_xbuf();
	xbuf[2] = CMD_SYNC_PACKNO;
	xbuf[6] = 0x01;
	xbuf[10] = 0x01;
	hid_wr(xbuf);
	rst_xbuf();
	xbuf[2] = CMD_SYNC_PACKNO;
	xbuf[6] = 0x03;
	xbuf[10] = 0x03;
	hid_wr(xbuf);
	rst_xbuf();
	xbuf[2] = CMD_SYNC_PACKNO;
	xbuf[6] = 0x05;
	hid_wr(xbuf);
	rst_xbuf();
	xbuf[2] = CMD_SYNC_PACKNO;
	xbuf[6] = 0x07;
	hid_wr(xbuf);
	rst_xbuf();
	xbuf[2] = CMD_SYNC_PACKNO;
	xbuf[6] = 0x09;
	hid_wr(xbuf);
}

char get_binary(char *path)
{
	int file;
	file = open(path, O_RDONLY);
	if(file < 0)
		return ERR;
	fsize = lseek(file, 0, SEEK_END);
	if (buf_bin)
		free(buf_bin);
	buf_bin = malloc(sizeof(char) * fsize);
	lseek(file, 0, SEEK_SET);
	if (fsize != read(file, buf_bin, fsize))
		return ERR;
	bin_aprom_len = *(buf_bin + fsize - 8) + \
			(*(buf_bin + fsize - 7) << 8) + \
			(*(buf_bin + fsize - 6) << 16) + \
			(*(buf_bin + fsize - 5) << 24);
	bin_aprom_chksum = *(buf_bin + fsize - 4) + \
			(*(buf_bin + fsize - 3) << 8) + \
			(*(buf_bin + fsize - 2) << 16) + \
			(*(buf_bin + fsize - 1) << 24);
	PF("bin size = %u", fsize);
	PF("bin_aprom_len = %u", bin_aprom_len);
	PF("bin_aprom_chksum = %u", bin_aprom_chksum);
	close(file);
	return 0;
}

/*
 * Compare the checksum between values in ldrom and aprom.
 * If they are the same, jump to aprom.
 */
char ldrom_rst(void)
{
	PF("ldrom reset");
	rst_xbuf();
	xbuf[1] = NO_AA_CHECK;
	xbuf[2] = CMD_LDROM_RESET;
	hid_wr(xbuf);
	if (get_dev_id() == APROM_MODE) {
		PF("Reset completed");
		PF("Device jumps to aprom");
		return 0;
	} else {
		PE("Reset failed");
		return ERR;
	}
}

char get_aprom_ver(void)
{
	int ver, ret;
	if (get_dev_id() != APROM_MODE) {
		PE("Device not in aprom, set version to 0");
		return 0;
	}

	rst_xbuf();
	xbuf[0] = 0x0b;
	xbuf[2] = CMD_GET_APROM_VER;
	ret = hid_wr(xbuf);
	if (ret)
		return 0; // return 0 to force fw update
	ver = (xbuf[7] << 24) + (xbuf[6] << 16) + (xbuf[5] << 8) + xbuf[4];
	PF("aprom version = %d", ver);

	if (ver > (ERR - 1)) {
		PE("aprom version = %d, it should not exceed %d", ver, ERR - 1);
		return 0; // return 0 to force fw update
	}

	return ver;
}

void write_ldrom_checksum(void)
{
	rst_xbuf();
	xbuf[2] = CMD_WRITE_CHECKSUM;
	xbuf[14] = bin_aprom_len & 0xff;
	xbuf[15] = (bin_aprom_len >> 8) & 0xff;
	xbuf[16] = (bin_aprom_len >> 16) & 0xff;
	xbuf[17] = (bin_aprom_len >> 24) & 0xff;
	xbuf[18] = bin_aprom_chksum & 0xff;
	xbuf[19] = (bin_aprom_chksum >> 8) & 0xff;
	xbuf[20] = (bin_aprom_chksum >> 16) & 0xff;
	xbuf[21] = (bin_aprom_chksum >> 24) & 0xff;
	hid_wr(xbuf);
}

char update_buf_pid_checksum(char* pid_str)
{
	int ret, i;
	uint32_t chksum;
	uint16_t pid[1];

	if (!buf_bin) {
		PE("Need to get binary first");
		return ERR;
	}

	ret = sscanf(pid_str, "%hx", pid);
	if (!ret) {
		PE("Invalid PID value : %s", pid_str);
		return ERR;
	}

	buf_bin[PID_ADDR] = (uint8_t)(pid[0] & 0xff);
	buf_bin[PID_ADDR + 1] = (uint8_t)(pid[0] >> 8);

	chksum = 0;
	for (i = 0; i < bin_aprom_len; i += 4) {
		chksum += buf_bin[i] + (buf_bin[i + 1] << 8) +
				(buf_bin[i + 2] << 16) +
				(buf_bin[i + 3] << 24);
	}
	buf_bin[fsize - 4] = chksum & 0xff;
	buf_bin[fsize - 3] = chksum >> 8;
	buf_bin[fsize - 2] = chksum >> 16;
	buf_bin[fsize - 1] = chksum >> 24;
	bin_aprom_chksum = chksum;
	PF("update buf pid to %02x%02x", buf_bin[PID_ADDR + 1], buf_bin[PID_ADDR]);
	PF("update buf checksum to %u", chksum);

	return 0;
}

char update_aprom(int mode, char* bin_path, char* pid_str)
{
	int ret, i, mod, rem, rem_len, csize, oft, idx;

	ret = get_binary(bin_path);
	if (ret) {
		PE("Failed to get the bin");
		return ERR;
	}

	if ((mode != LDROM_MODE) && jump_to_ldrom())
		return ERR;

	ret = update_buf_pid_checksum(pid_str);
	if (ret) {
		PE("Failed to update buf for pid and checksum");
		return ERR;
	}

	PF("Start Flashing");
	rst_xbuf();
	xbuf[2] = CMD_SYNC_PACKNO;
	xbuf[6] = 0x01;
	xbuf[10] = 0x01;
	hid_wr(xbuf);
	rst_xbuf();
	xbuf[2] = CMD_UPDATE_APROM;
	xbuf[6] = 0x03;
	xbuf[14] = bin_aprom_len & 0xff;
	xbuf[15] = (bin_aprom_len >> 8) & 0xff;
	xbuf[16] = (bin_aprom_len >> 16) & 0xff;
	xbuf[17] = (bin_aprom_len >> 24) & 0xff;
	memcpy(xbuf + 18, buf_bin, 48);
	hid_wr(xbuf);

	idx = 5;
	csize = 56;
	oft = 48;
	mod = (bin_aprom_len - oft) / csize;
	rem = (bin_aprom_len - oft) % csize;
	for( i = 0; i < mod; i++) {
		rst_xbuf();
		xbuf[6] = idx & 0xff;
		xbuf[7] = (idx >> 8) & 0xff;
		xbuf[8] = (idx >> 16) & 0xff;
		xbuf[9] = (idx >> 24) & 0xff;
		memcpy(xbuf + 10, buf_bin + oft, csize);
		hid_wr(xbuf);

		oft += csize;
		idx += 2;
	}
	if (rem) {
		rem_len = bin_aprom_len - oft;
		rst_xbuf();
		xbuf[6] = idx & 0xff;
		xbuf[7] = (idx >> 8) & 0xff;
		xbuf[8] = (idx >> 16) & 0xff;
		xbuf[9] = (idx >> 24) & 0xff;
		memcpy(xbuf + 10, buf_bin + bin_aprom_len - rem_len, rem_len);
		hid_wr(xbuf);
	}

	PF("Flashing completed");

	write_ldrom_checksum();
	ret = ldrom_rst();

	return ret;
}

char check_rom_status(void)
{
	int mode;

	mode = get_dev_id();
	switch (mode) {
	case LDROM_MODE:
		PF("Device is in LDROM");
		break;
	case APROM_MODE:
		PF("Device is in APROM");
		break;
	case ERR:
		PE("Wrong device or abnormal I2C signal");
		break;
	}
	return mode;
}

int main(int argc, char *argv[])
{
	int mode, ret;

	ret = 0;

	if (!argv[1] || !argv[2]) {
		PE("Please specify the device path and options");
		ret = ERR;
		goto info;
	}

	strcpy(node_path, argv[1]);

	fdev_hid = open(node_path, O_RDWR|O_NONBLOCK);

	if (fdev_hid < 0) {
		PE("open %s error!",node_path);
		return ERR;
	}

	mode = check_rom_status();

	if (mode == ERR)
		return ERR;

	if (strcmp(argv[2], "-v") == 0) {
		ret = get_aprom_ver();
		goto end;
	} else if (strcmp(argv[2], "-i") == 0) {
		ret = check_rom_status();
		goto end;
	} else if (strcmp(argv[2], "-u") == 0) {
		if (!argv[3]) {
			PE("Bin path is not specified");
			return ERR;
		}
		if (!argv[4]) {
			argv[4] = "F7FC";
			PF("PID is not specified, set to default F7FC");
		}
		ret = update_aprom(mode, argv[3], argv[4]);
		if (ret) {
			PE("Failed to update aprom");
			return ERR;
		} else {
			PF("Update aprom success");
			goto end;
		}
	}
info:
	PF("[dev/hidraw*] [-v]                      - show aprom version");
	PF("[dev/hidraw*] [-i]                      - show rom status");
	PF("[dev/hidraw*] [-u] <aprom_fw.bin> <PID> - update aprom fw with PID");
end:
	return ret;
};
