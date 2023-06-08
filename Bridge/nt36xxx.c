#include "nt36xxx.h"
#include "nt36xxx_mem_map.h"

void nvt_ts_chip_mmap(void)
{
#ifdef _NT36523
	ts->SWRST_N8_ADDR = 0x3F0FE;
	ts->mmap = &NT36523_memory_map;
	ts->hw_crc = NT36523_hw_info.hw_crc;
#endif
	return;
}

void msleep(int32_t delay)
{
    int32_t i;
    for (i = 0; i < delay; i++)
        usleep(1000);
}

void print_cmd_arg_err_msg(int32_t argc, char* main_argv[], ...)
{
	va_list ap;
	char *msg;

	printf("\tError: Parameters Error, Argc=%d\n", argc);

	va_start(ap, main_argv);
	msg = va_arg(ap, char *);
	if (msg != NULL) {
		printf("%s\n", msg);
	}
	va_end(ap);
}
