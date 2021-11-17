TS_CMDLINE = NT36523_Cmd_HID_I2C
BDG_CMDLINE = Bdg_Cmd_M252
LIBS = -lm -lrt
CFLAGS = -Wall -O3
TOOLCHAIN ?= gcc

all:
	rm -f *Cmd_*
	${TOOLCHAIN} hid.c main_i2c.c nt36xxx.c xdata_lib.c xdata.c flash.c \
            --static -o ${TS_CMDLINE} ${LIBS} ${CFLAGS} -D_NT36523
	${TOOLCHAIN} Bridge.c --static -o ${BDG_CMDLINE} ${LIBS} ${CFLAGS}
	rm -rf libs obj
