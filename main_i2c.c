#include "nt36xxx_i2c.h"
#include <linux/hidraw.h>

int main(int32_t argc, char *argv[])
{
	int ret = 0;

	ts = malloc(sizeof(struct nvt_ts_data));

	nvt_ts_chip_mmap();

	if (!argv[1] || !argv[2]) {
		printf("Please specify the device path and options\n");
		goto info;
	}

	strcpy(node_path, argv[1]);

	fdev_hid = open(node_path, O_RDWR|O_NONBLOCK);
	if (fdev_hid < 0) {
		printf("open %s error!\n", node_path);
		ret = -1;
		goto out;
	}

	if (strcmp(argv[2], "-v") == 0) {
		return nvt_get_fw_ver();
	} else if (strcmp(argv[2], "-i") == 0) {
		nvt_get_fw_info();
		goto out;
	} else if (strcmp(argv[2], "-id") == 0) {
		nvt_get_trim_id();
		goto out;
	} else if ( (strcmp(argv[2], "-u") == 0) ) {
		if (!argv[3]) {
			printf("Enter the Bin file path\n");
		} else {
			return UpdateFW(argv[3], enDoCheck);
		}
	} else if (strcmp(argv[2], "-xw") == 0) {
		if (argc < 6) {
			print_cmd_arg_err_msg(argc, argv);
			goto out;
		}
		xI2CWrite(argc, argv, IC_Master);
		goto out;
	} else if (strcmp(argv[2], "-xr") == 0) {
		if (argc < 5) {
			print_cmd_arg_err_msg(argc, argv);
			goto out;
		}
		xI2CRead(argc, argv, IC_Master);
		goto out;
	} else if (strcmp(argv[2], "-R") == 0) {
		printf("bootloader_Reset\n");
		nvt_bootloader_reset();
		goto out;
	} else if (strcmp(argv[2], "-rstidle") == 0) {
		nvt_sw_reset_idle();
		printf("Reset&Idle now!!! Waiting next RESET back to work\n");
		goto out;
	}

info:
	printf("[dev/hidraw*] [-rstidle]         - Reset Idle \n");
	printf("[dev/hidraw*] [-R]               - Bootloader reset \n");
	printf("[dev/hidraw*] [-id]              - Get Trim ID\n");
	printf("[dev/hidraw*] [-v]               - Firmware Version\n");
	printf("[dev/hidraw*] [-i]               - Firmware Info\n");
	printf("[dev/hidraw*] [-u] <fw.bin>      - Update Firmware\n");
	printf("[dev/hidraw*] [-xw] <addr> <len> <data0> <data1> ... - Write Reg Data\n");
	printf("[dev/hidraw*] [-xr] <addr> <len> - Read Reg Data\n");

out:
	if (fdev_hid >= 0)
		close(fdev_hid);

	free(ts);
	return ret;
}
