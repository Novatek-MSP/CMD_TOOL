LIBS = -lm -lrt
CFLAGS = -Wall -O3
CC ?= gcc

all:
	# Bridge
	rm -f *Cmd_*
	${CC} Bridge/hid.c Bridge/main_i2c.c Bridge/nt36xxx.c Bridge/xdata_lib.c \
		Bridge/xdata.c Bridge/flash.c --static -o NT36523_Cmd_HID_I2C ${LIBS} ${CFLAGS}
	${CC} Bridge/Bridge.c --static -o Bdg_Cmd_M252 ${LIBS} ${CFLAGS}

	# No Bridge
	rm -f *cmd_*
	${CC} nvt_hid_main.c nvt_hid_utils.c nvt_hid_flash_gcm.c nvt_hid_selftest.c \
		--static -o nvt_ts_hid_cmd ${LIBS} ${CFLAGS}

	rm -rf libs obj
