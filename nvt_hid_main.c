// SPDX-License-Identifier: Apache-2.0

#include "nvt_hid_main.h"

int32_t main(int32_t _argc, char *_argv[])
{
	char node_path[MAX_NODE_PATH_SIZE] = { 0 }, **argv = NULL;
	uint8_t reset_idle = 0, user_sh_cmd = 0;
	uint16_t argc = 0;
	int32_t ret = 0, ts_size = 0, i = 0;
	struct timeval t1, t2, tf, result;

	reset_idle = 0;
	if (!_argc) {
		ret = -EFAULT;
		goto end_no_reset;
	}

	argc = (uint16_t)_argc;

	if (!_argv[1]) {
		ret = 0;
		goto end_no_reset;
	}

#if DEBUG_OPTION
	// Determine if to turn on debug log or not
	if ((strcmp("-debug", _argv[1]) == 0)) {
		if (argc == 2) {
			ret = 0;
			goto info;
		}
		argv = malloc(sizeof(char *) * (argc - 1));
		if (!argv) {
			ret = -ENOMEM;
			goto end_no_reset;
		}
		argv[0] = malloc(sizeof(char) * (strlen(_argv[0]) + 1));
		if (!argv[0]) {
			ret = -ENOMEM;
			goto end_no_reset;
		}
		strcpy(argv[0], _argv[0]);
		for (i = 1; i < argc - 1; i++) {
			argv[i] = malloc(sizeof(char) *
					 (strlen(_argv[i + 1]) + 1));
			if (!argv[i]) {
				ret = -ENOMEM;
				goto end_no_reset;
			}
			strcpy(argv[i], _argv[i + 1]);
		}
		term_debug = 1;
		argc--;
	} else {
		argv = _argv;
	}
#else
	// Always on
	term_debug = 1;
	argv = malloc(sizeof(char *) * argc);
	if (!argv) {
		ret = -ENOMEM;
		goto end_no_reset;
	}
	for (i = 0; i < argc; i++) {
		argv[i] = malloc(sizeof(char) * (strlen(_argv[i]) + 1));
		if (!argv[i]) {
			ret = -ENOMEM;
			goto end_no_reset;
		}
		strcpy(argv[i], _argv[i]);
	}
#endif

	NVT_DBG("Tool version tag = %s",
		BUILD_VERSION); // This should match git tag

	if (term_debug)
		gettimeofday(&t1, NULL);

	ts_size = sizeof(struct nvt_ts_data);
	ts = malloc(ts_size);
	if (!ts) {
		NVT_ERR("No memory for %d bytes", ts_size);
		ret = -EAGAIN;
		goto end_no_reset;
	}

	fd_node = open(NODE_PATH_HIDRAW_RECOVERY_SPI, O_RDWR | O_NONBLOCK);
	if (fd_node >= 0) {
		strcpy(node_path, NODE_PATH_HIDRAW_RECOVERY_SPI);
		recovery_node = 1;
		bus_type = BUS_SPI;
		goto found_node;
	}

	if (!term_debug) {
		fd_node = -EINVAL;
	} else {
		fd_node = open(NODE_PATH_HIDRAW_RECOVERY, O_RDWR | O_NONBLOCK);
	}

	if (fd_node < 0) {
		NVT_DBG("Failed to open %s, fd = %d", NODE_PATH_HIDRAW_RECOVERY,
			fd_node);
		ret = find_generic_hidraw_path(node_path);
		if (ret) {
			NVT_ERR("Can not open hidraw* and nvt_ts_hidraw");
			NVT_ERR("No node is available, Did you \"su\" or \" adb root\"?");
			goto end_no_reset;
		}
		recovery_node = 0;
		goto found_node;
	} else {
		NVT_DBG("fd_node = %d", fd_node);
		strcpy(node_path, NODE_PATH_HIDRAW_RECOVERY);
		recovery_node = 1;
		bus_type = BUS_I2C;
		ts->hid_i2c_prefix_pkt = malloc(HID_I2C_PREFIX_PKT_LENGTH);
	}

found_node:
	if (recovery_node)
		NVT_DBG("******************** [Recovery Node] *************************");
	else
		NVT_DBG("******************** [Generic Node] **************************");
	NVT_DBG("Open device at %s, bus_type %s", node_path,
		bus_type == BUS_I2C ? "HID_I2C" : "HID_SPI");
	NVT_DBG("**************************************************************");

	user_sh_cmd = (strcmp(argv[1], "-b") == 0) ||
		      (strcmp(argv[1], "-setf") == 0) ||
		      (strcmp(argv[1], "-getf") == 0);

	if (!user_sh_cmd) {
		if (!((strcmp(argv[1], "-s") == 0) ||
		      (strcmp(argv[1], "-vs") == 0))) // CROS
			reset_idle = 1;

		ret = nvt_ts_detect_chip_ver_trim(reset_idle);

		if (ret)
			goto end;
	} else {
		NVT_DBG("User send shell command, skip ver trim reading");
	}

	// Pretty print
	if (strcmp(argv[1], "-vs") == 0) {
		if (argc != 2) {
			NVT_ERR("Invalid format");
			goto info;
		} else {
			ret = nvt_get_fw_ver();
			if (ret) {
				NVT_ERR("Error reading FW version");
			} else {
				printf("%d\n", ts->fw_ver);
				if (term_debug) {
					gettimeofday(&t2, NULL);
					timersub(&t2, &t1, &result);
					NVT_DBG("Detect chip and read fw info took %ld.%06ld s",
						(long int)result.tv_sec,
						(long int)result.tv_usec);
				}
			}
			goto end;
		}
	} else if (strcmp(argv[1], "-ps") == 0) {
		if (argc != 2) {
			NVT_ERR("Invalid format");
			goto info;
		} else {
			ret = dump_flash_gcm_pid();
			if (ret) {
				NVT_ERR("Error reading PID in flash");
			} else {
				printf("%d\n", ts->flash_pid);
			}
			goto end;
		}
		goto end;
	}

	// Line above should not print log, except errors or term debug is on
	NVT_LOG("Tool version tag = %s",
		BUILD_VERSION); // This should match git tag
	NVT_LOG("Protocol = %s", bus_type == BUS_I2C ? "HID_I2C" : "HID_SPI");

	if (strcmp(argv[1], "-v") == 0) {
		NVT_LOG("User Get FW Version and Informations");
		if (argc != 2) {
			NVT_ERR("Invalid format");
			goto info;
		} else {
			ret = nvt_get_fw_ver_and_info(1);
			goto end;
		}
	} else if ((strcmp(argv[1], "-u") == 0)) {
		NVT_LOG("User Update Normal FW");
		if (argc != 3) {
			NVT_ERR("Invalid format");
			goto info;
		} else {
			ret = update_firmware(argv[2], FLASH_NORMAL);
			if (!ret) {
				NVT_LOG("Update Normal FW OK");
			} else {
				NVT_ERR("Update Normal FW Failed");
			}
			goto end;
		}
	} else if ((strcmp(argv[1], "-lock") == 0)) {
		NVT_LOG("User lock in short FW");
		goto end_no_reset;
	} else if (strcmp(argv[1], "-R") == 0) {
		NVT_LOG("User Bootloader Reset");
		goto end;
	} else if (strcmp(argv[1], "-ri") == 0) {
		NVT_LOG("User Reset then Idle");
		nvt_sw_idle();
		goto end_no_reset;
	} else if (strcmp(argv[1], "-s") == 0) { // selftest
		NVT_LOG("User Start Selftest");
		if (argc < 3) {
			NVT_ERR("Invalid format");
			goto info;
		} else if (strcmp(argv[2], "-h") == 0) { // hand only
			if ((argc == 5) &&
			    (strcmp(argv[3], "-c") == 0)) { // specify name
				nvt_selftest_open(0, argv[4]);
			} else if ((argc == 4) && (strcmp(argv[3], "-c") ==
						   0)) { // default name
				nvt_selftest_open(0, NVT_MP_CRITERIA_FILENAME);
			} else if (argc == 3) { // built-in criteria
				nvt_selftest_open(0, NULL);
			} else {
				NVT_ERR("Invalid format");
				ret = -EINVAL;
				goto info;
			}
		} else if (strcmp(argv[2], "-p") == 0) { // pen supported
			if ((argc == 5) &&
			    (strcmp(argv[3], "-c") == 0)) { // specify name
				nvt_selftest_open(1, argv[4]);
			} else if ((argc == 4) && (strcmp(argv[3], "-c") ==
						   0)) { // default name
				nvt_selftest_open(1, NVT_MP_CRITERIA_FILENAME);
			} else if (argc == 3) { // built-in criteria
				nvt_selftest_open(1, NULL);
			} else {
				NVT_ERR("Invalid format");
				ret = -EINVAL;
				goto info;
			}
		} else {
			NVT_ERR("Invalid format, please use -h or -p to choose");
			ret = -EINVAL;
			goto info;
		}
		goto end;
	} else if (strcmp(argv[1], "-b") == 0) { // Rebind i2c_hid
		NVT_LOG("User Rebind HID");
		if (argc != 2) {
			NVT_ERR("Invalid format");
			goto info;
		} else {
			ret = rebind_i2c_hid_driver();
			if (ret)
				NVT_LOG("Failed to rebind i2c hid driver");
			else
				NVT_LOG("Successfully rebind i2c hid driver");
		}
		goto end_no_reset;
	} else if (strcmp(argv[1], "-C") == 0) {
		NVT_LOG("User check flash FW by checksum");
		if (argc != 3) {
			NVT_ERR("Invalid format");
			goto info;
		} else {
			ret = User_compare_bin_and_flash_sector_checksum(
				argv[2]);
		}
		goto end;
	} else if (strcmp(argv[1], "-setf") == 0) {
		if (find_generic_hidraw_path(node_path)) {
			NVT_ERR("This option is only available for generic node");
			ret = -EINVAL;
			goto end;
		}
		NVT_LOG("User set feature with generic node");
		if (argc < 4) {
			NVT_ERR("Invalid format");
			goto info;
		} else {
			ret = nvt_user_set_feature_report(argc - 3, argv);
		}
		goto end_no_reset;
	} else if (strcmp(argv[1], "-getf") == 0) {
		if (find_generic_hidraw_path(node_path)) {
			NVT_ERR("This option is only available for generic node");
			ret = -EINVAL;
			goto end;
		}
		NVT_LOG("User get feature with generic node");
		if (argc < 3) {
			NVT_ERR("Invalid format");
			goto info;
		} else {
			ret = nvt_user_get_feature_report(argc - 3, argv);
		}
		goto end_no_reset;
	} else {
		NVT_ERR("Invalid option");
		goto info;
	}

info:
	goto end_no_reset;

end: // do not print after this line
	if (reset_idle) {
		if (bus_type == BUS_SPI) {
			nvt_unblock_jump();
			if (recovery_node)
				nvt_bootloader_reset();
		} else if (bus_type == BUS_I2C) {
			nvt_bootloader_reset();
		}
	}

end_no_reset:
	if (fd_node >= 0)
		close(fd_node);
	if (ts)
		free(ts);

	if (term_debug) {
		gettimeofday(&tf, NULL);
		timersub(&tf, &t1, &result);
		NVT_DBG("Total operation took %ld.%06ld s",
			(long int)result.tv_sec, (long int)result.tv_usec);
	}
	return ret;
}
