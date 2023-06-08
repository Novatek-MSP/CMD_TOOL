// SPDX-License-Identifier: Apache-2.0

#include "nvt_hid_main.h"

#define NORMAL_MODE 0x00
#define HANDSHAKING_HOST_READY 0xBB
#define TEST_MODE_2 0x22
#define MP_MODE_BASELINE_CC 0x41
#define FREQ_HOP_DISABLE 0x66
#define TOUCH_HZ 120
#define MAX_NUM_SIZE 10
#define MAX_TAG_SIZE 50
#define _INT16_MAX 32767
#define _INT16_MIN -32768
#define _BYTE_PER_POINT 2
#define MP_RESULT_ERROR_VALUE 1

// default mp limits
#define TOUCH_SHORT_LMT_P 14008
#define TOUCH_SHORT_LMT_N 0
#define TOUCH_OPEN_LMT_P 5120
#define TOUCH_OPEN_LMT_N -511
#define TOUCH_DIFF_LMT_P 25600
#define TOUCH_DIFF_LMT_N 0
#define TOUCH_BASELINE_LMT_P 512
#define TOUCH_BASELINE_LMT_N 0
#define TOUCH_CC_LMT_P 300
#define TOUCH_CC_LMT_N -300
#define PEN_TIP_X_BL_LMT_P 1050
#define PEN_TIP_X_BL_LMT_N -1050
#define PEN_TIP_Y_BL_LMT_P 1050
#define PEN_TIP_Y_BL_LMT_N -1050
#define PEN_RING_X_BL_LMT_P 1050
#define PEN_RING_X_BL_LMT_N -1050
#define PEN_RING_Y_BL_LMT_P 1050
#define PEN_RING_Y_BL_LMT_N -1050
#define PEN_TIP_X_DIFF_LMT_P 1050
#define PEN_TIP_X_DIFF_LMT_N -1050
#define PEN_TIP_Y_DIFF_LMT_P 1050
#define PEN_TIP_Y_DIFF_LMT_N -1050
#define PEN_RING_X_DIFF_LMT_P 1050
#define PEN_RING_X_DIFF_LMT_N -1050
#define PEN_RING_Y_DIFF_LMT_P 1050
#define PEN_RING_Y_DIFF_LMT_N -1050
#define DIFF_INIT_TEST_FRAME 50

enum { TEST_PASS, TEST_FAIL };

enum {
	TOUCH_TYPE,
	PEN_X_TYPE,
	PEN_Y_TYPE,
	SINGLE_TYPE,
	SINGLE_INPUT_TYPE, // input type has no result
};

enum {
	PRINT_RAW,
	PRINT_MP,
	PRINT_RESULT,
};

enum {
	LMT_TYPE_NONE,
	LMT_TYPE_UPPER,
	LMT_TYPE_LOWER,
};

enum {
	RAW_TOUCH_SHORT,
	RAW_TOUCH_OPEN,
	RAW_TOUCH_DIFF,
	RAW_TOUCH_DIFF_MAX,
	RAW_TOUCH_DIFF_MIN,
	RAW_TOUCH_BASELINE,
	RAW_TOUCH_CC,
	RAW_PEN_TIP_X_BASELINE,
	RAW_PEN_TIP_Y_BASELINE,
	RAW_PEN_RING_X_BASELINE,
	RAW_PEN_RING_Y_BASELINE,
	RAW_PEN_TIP_X_DIFF_MAX,
	RAW_PEN_TIP_X_DIFF_MIN,
	RAW_PEN_TIP_Y_DIFF_MAX,
	RAW_PEN_TIP_Y_DIFF_MIN,
	RAW_PEN_RING_X_DIFF_MAX,
	RAW_PEN_RING_X_DIFF_MIN,
	RAW_PEN_RING_Y_DIFF_MAX,
	RAW_PEN_RING_Y_DIFF_MIN,
};

enum {
	MP_TOUCH_SHORT_P,
	MP_TOUCH_SHORT_N,
	MP_TOUCH_OPEN_P,
	MP_TOUCH_OPEN_N,
	MP_TOUCH_DIFF_P,
	MP_TOUCH_DIFF_N,
	MP_TOUCH_BASELINE_P,
	MP_TOUCH_BASELINE_N,
	MP_TOUCH_CC_P,
	MP_TOUCH_CC_N,
	MP_PEN_TIP_X_BASELINE_P,
	MP_PEN_TIP_X_BASELINE_N,
	MP_PEN_TIP_Y_BASELINE_P,
	MP_PEN_TIP_Y_BASELINE_N,
	MP_PEN_RING_X_BASELINE_P,
	MP_PEN_RING_X_BASELINE_N,
	MP_PEN_RING_Y_BASELINE_P,
	MP_PEN_RING_Y_BASELINE_N,
	MP_PEN_TIP_X_DIFF_P,
	MP_PEN_TIP_X_DIFF_N,
	MP_PEN_TIP_Y_DIFF_P,
	MP_PEN_TIP_Y_DIFF_N,
	MP_PEN_RING_X_DIFF_P,
	MP_PEN_RING_X_DIFF_N,
	MP_PEN_RING_Y_DIFF_P,
	MP_PEN_RING_Y_DIFF_N,
	MP_DIFF_TEST_FRAME,
};

struct mp_data {
	char name[MAX_TAG_SIZE];
	uint8_t type;
	uint8_t lmt_type;
	uint16_t point_size;
	uint16_t byte_size;
	uint8_t matched;
	int16_t *array;
	int16_t *array_result;
	int16_t lmt; // default limit is the same for all point
	uint8_t line_num;
};
struct mp_data mp_data_table[] = {
	// order should match enum
	{ .name = "TOUCH_SHORT_P",
	  .type = TOUCH_TYPE,
	  .lmt = TOUCH_SHORT_LMT_P,
	  .lmt_type = LMT_TYPE_UPPER },
	{ .name = "TOUCH_SHORT_N",
	  .type = TOUCH_TYPE,
	  .lmt = TOUCH_SHORT_LMT_N,
	  .lmt_type = LMT_TYPE_LOWER },
	{ .name = "TOUCH_OPEN_P",
	  .type = TOUCH_TYPE,
	  .lmt = TOUCH_OPEN_LMT_P,
	  .lmt_type = LMT_TYPE_UPPER },
	{ .name = "TOUCH_OPEN_N",
	  .type = TOUCH_TYPE,
	  .lmt = TOUCH_OPEN_LMT_N,
	  .lmt_type = LMT_TYPE_LOWER },
	{ .name = "TOUCH_DIFF_P",
	  .type = TOUCH_TYPE,
	  .lmt = TOUCH_DIFF_LMT_P,
	  .lmt_type = LMT_TYPE_UPPER },
	{ .name = "TOUCH_DIFF_N",
	  .type = TOUCH_TYPE,
	  .lmt = TOUCH_DIFF_LMT_N,
	  .lmt_type = LMT_TYPE_LOWER },
	{ .name = "TOUCH_BASELINE_P",
	  .type = TOUCH_TYPE,
	  .lmt = TOUCH_BASELINE_LMT_P,
	  .lmt_type = LMT_TYPE_UPPER },
	{ .name = "TOUCH_BASELINE_N",
	  .type = TOUCH_TYPE,
	  .lmt = TOUCH_BASELINE_LMT_N,
	  .lmt_type = LMT_TYPE_LOWER },
	{ .name = "TOUCH_CC_P",
	  .type = TOUCH_TYPE,
	  .lmt = TOUCH_CC_LMT_P,
	  .lmt_type = LMT_TYPE_UPPER },
	{ .name = "TOUCH_CC_N",
	  .type = TOUCH_TYPE,
	  .lmt = TOUCH_CC_LMT_N,
	  .lmt_type = LMT_TYPE_LOWER },
	{ .name = "PEN_TIP_X_BASELINE_P",
	  .type = PEN_X_TYPE,
	  .lmt = PEN_TIP_X_BL_LMT_P,
	  .lmt_type = LMT_TYPE_UPPER },
	{ .name = "PEN_TIP_X_BASELINE_N",
	  .type = PEN_X_TYPE,
	  .lmt = PEN_TIP_X_BL_LMT_N,
	  .lmt_type = LMT_TYPE_LOWER },
	{ .name = "PEN_TIP_Y_BASELINE_P",
	  .type = PEN_Y_TYPE,
	  .lmt = PEN_TIP_Y_BL_LMT_P,
	  .lmt_type = LMT_TYPE_UPPER },
	{ .name = "PEN_TIP_Y_BASELINE_N",
	  .type = PEN_Y_TYPE,
	  .lmt = PEN_TIP_Y_BL_LMT_N,
	  .lmt_type = LMT_TYPE_LOWER },
	{ .name = "PEN_RING_X_BASELINE_P",
	  .type = PEN_X_TYPE,
	  .lmt = PEN_RING_X_BL_LMT_P,
	  .lmt_type = LMT_TYPE_UPPER },
	{ .name = "PEN_RING_X_BASELINE_N",
	  .type = PEN_X_TYPE,
	  .lmt = PEN_RING_X_BL_LMT_N,
	  .lmt_type = LMT_TYPE_LOWER },
	{ .name = "PEN_RING_Y_BASELINE_P",
	  .type = PEN_Y_TYPE,
	  .lmt = PEN_RING_Y_BL_LMT_P,
	  .lmt_type = LMT_TYPE_UPPER },
	{ .name = "PEN_RING_Y_BASELINE_N",
	  .type = PEN_Y_TYPE,
	  .lmt = PEN_RING_Y_BL_LMT_N,
	  .lmt_type = LMT_TYPE_LOWER },
	{ .name = "PEN_TIP_X_DIFF_P",
	  .type = PEN_X_TYPE,
	  .lmt = PEN_TIP_X_DIFF_LMT_P,
	  .lmt_type = LMT_TYPE_UPPER },
	{ .name = "PEN_TIP_X_DIFF_N",
	  .type = PEN_X_TYPE,
	  .lmt = PEN_TIP_X_DIFF_LMT_N,
	  .lmt_type = LMT_TYPE_LOWER },
	{ .name = "PEN_TIP_Y_DIFF_P",
	  .type = PEN_Y_TYPE,
	  .lmt = PEN_TIP_Y_DIFF_LMT_P,
	  .lmt_type = LMT_TYPE_UPPER },
	{ .name = "PEN_TIP_Y_DIFF_N",
	  .type = PEN_Y_TYPE,
	  .lmt = PEN_TIP_Y_DIFF_LMT_N,
	  .lmt_type = LMT_TYPE_LOWER },
	{ .name = "PEN_RING_X_DIFF_P",
	  .type = PEN_X_TYPE,
	  .lmt = PEN_RING_X_DIFF_LMT_P,
	  .lmt_type = LMT_TYPE_UPPER },
	{ .name = "PEN_RING_X_DIFF_N",
	  .type = PEN_X_TYPE,
	  .lmt = PEN_RING_X_DIFF_LMT_N,
	  .lmt_type = LMT_TYPE_LOWER },
	{ .name = "PEN_RING_Y_DIFF_P",
	  .type = PEN_Y_TYPE,
	  .lmt = PEN_RING_Y_DIFF_LMT_P,
	  .lmt_type = LMT_TYPE_UPPER },
	{ .name = "PEN_RING_Y_DIFF_N",
	  .type = PEN_Y_TYPE,
	  .lmt = PEN_RING_Y_DIFF_LMT_N,
	  .lmt_type = LMT_TYPE_LOWER },
	{ .name = "DIFF_TEST_FRAME",
	  .type = SINGLE_INPUT_TYPE,
	  .lmt = DIFF_INIT_TEST_FRAME,
	  .lmt_type = LMT_TYPE_NONE },
};

struct raw_data {
	char name[MAX_TAG_SIZE];
	uint8_t type;
	uint16_t point_size;
	uint16_t byte_size;
	int16_t *array;
	uint8_t line_num;
	uint8_t skip_print;
};
struct raw_data raw_data_table[] = {
	// order should match enum
	{ .name = "TOUCH_SHORT", .type = TOUCH_TYPE },
	{ .name = "TOUCH_OPEN", .type = TOUCH_TYPE },
	{ .name = "TOUCH_DIFF", .type = TOUCH_TYPE, .skip_print = 1 },
	{ .name = "TOUCH_DIFF_MAX", .type = TOUCH_TYPE },
	{ .name = "TOUCH_DIFF_MIN", .type = TOUCH_TYPE },
	{ .name = "TOUCH_BASELINE", .type = TOUCH_TYPE },
	{ .name = "TOUCH_CC", .type = TOUCH_TYPE },
	{ .name = "PEN_TIP_X_BASELINE", .type = PEN_X_TYPE },
	{ .name = "PEN_TIP_Y_BASELINE", .type = PEN_Y_TYPE },
	{ .name = "PEN_RING_X_BASELINE", .type = PEN_X_TYPE },
	{ .name = "PEN_RING_Y_BASELINE", .type = PEN_Y_TYPE },
	{ .name = "PEN_TIP_X_DIFF_MAX", .type = PEN_X_TYPE },
	{ .name = "PEN_TIP_X_DIFF_MIN", .type = PEN_X_TYPE },
	{ .name = "PEN_TIP_Y_DIFF_MAX", .type = PEN_Y_TYPE },
	{ .name = "PEN_TIP_Y_DIFF_MIN", .type = PEN_Y_TYPE },
	{ .name = "PEN_RING_X_DIFF_MAX", .type = PEN_X_TYPE },
	{ .name = "PEN_RING_X_DIFF_MIN", .type = PEN_X_TYPE },
	{ .name = "PEN_RING_Y_DIFF_MAX", .type = PEN_Y_TYPE },
	{ .name = "PEN_RING_Y_DIFF_MIN", .type = PEN_Y_TYPE },
};

uint8_t mp_data_table_size;
uint8_t raw_data_table_size;

uint8_t *rawdata_buf;

struct mp_data *mp_data_holder;
static int32_t match_tag(char *tag, uint8_t size, uint8_t pen_support)
{
	int32_t i = 0, ret = 0;
	char str[MAX_TAG_SIZE] = { 0 };

	strncpy(str, tag, size);

	for (i = 0; i < mp_data_table_size; i++) {
		if (strncmp(mp_data_table[i].name, str, size) == 0) {
			if (!pen_support &&
			    (mp_data_table[i].type == PEN_X_TYPE ||
			     mp_data_table[i].type == PEN_Y_TYPE)) {
				ret = 1;
				NVT_DBG("Ignore loading pen item %s",
					mp_data_table[i].name);
			} else {
				mp_data_holder = &mp_data_table[i];
				ret = 0;
			}
			goto end;
		}
	}
	NVT_ERR("Failed to match tag for %s", str);
	ret = -EINVAL;
end:
	return ret;
}

static int32_t read_mp_lmt_array_from_file(char *fdata, int32_t fsize,
					   uint8_t pen_support)
{
	uint8_t read_tag = 0, wait_next_tag = 0;
	uint32_t i = 0, num_idx = 0, tag_idx = 0, num_array_idx = 0;
	int32_t ret = 0;
	int64_t num_check = 0;
	char tag[MAX_TAG_SIZE] = { 0 };
	char num[MAX_NUM_SIZE] = { 0 };

	read_tag = 0;
	tag_idx = 0;
	num_idx = 0;
	num_array_idx = 0;
	wait_next_tag = 0;
	ret = 0;

	for (i = 0; i < fsize; i++) {
		switch (fdata[i]) {
		case '[':
			read_tag = 1;
			wait_next_tag = 0;
			if (num_array_idx) {
				if (num_array_idx !=
				    mp_data_holder->point_size) {
					NVT_ERR("%s array size mismatched (%d/%d)",
						mp_data_holder->name,
						num_array_idx,
						mp_data_holder->point_size);
					ret = -EINVAL;
					goto end;
				} else {
					mp_data_holder->matched = 1;
				}
			}
			num_array_idx = 0;
			break;
		case ']':
			if (read_tag) {
				ret = match_tag(tag, tag_idx + 1, pen_support);
				if (ret < 0)
					goto end;
				else if (ret == 1)
					wait_next_tag = 1;
				read_tag = 0;
				tag_idx = 0;
				memset(tag, 0, MAX_TAG_SIZE);
			} else {
				NVT_ERR("Mismatched [] detected");
				ret = -EINVAL;
				goto end;
			}
			break;
		case ' ':
		case '\n':
			if (wait_next_tag)
				continue;
			if (read_tag) {
				NVT_ERR("Mismatched [] detected");
				ret = -EINVAL;
				goto end;
			}
			if (num_idx) {
				if (num_array_idx ==
				    mp_data_holder->point_size) {
					NVT_ERR("%s point_size exceeding %d",
						mp_data_holder->name,
						mp_data_holder->point_size);
					ret = -EINVAL;
					goto end;
				}
				num[num_idx] = '\0';
				num_check = strtol(num, NULL, 10);
				if (num_check > _INT16_MAX) {
					NVT_ERR("number %ld is higher than %d",
						num_check, _INT16_MAX);
					ret = -EINVAL;
					goto end;
				}
				if (num_check < _INT16_MIN) {
					NVT_ERR("number %ld is lesser than %d",
						num_check, _INT16_MIN);
					ret = -EINVAL;
					goto end;
				}
				mp_data_holder->array[num_array_idx] =
					num_check;
				num_array_idx++;
			}
			num_idx = 0;
			memset(num, 0, MAX_NUM_SIZE);
			break;
		case '0' ... '9':
		case '-':
			if (wait_next_tag)
				continue;
			if (read_tag) {
				NVT_ERR("Mismatched [] detected");
				ret = -EINVAL;
				goto end;
			}
			if (num_idx == MAX_NUM_SIZE - 1) {
				NVT_ERR("num_idx exceeding %d",
					MAX_NUM_SIZE - 1);
				ret = -EINVAL;
				goto end;
			}
			num[num_idx] = fdata[i];
			num_idx++;
			break;
		case 'A' ... 'Z':
		case '_':
			if (read_tag) {
				tag[tag_idx] = fdata[i];
				tag_idx++;
			} else {
				NVT_ERR("alphabat outside tag reading detected");
				ret = -EINVAL;
				goto end;
			}
			break;
		default:
			NVT_ERR("Invalid character detected  fdata[%d] = %c", i,
				fdata[i]);
			ret = -EINVAL;
			goto end;
		}
	}

	if (i == fsize && num_array_idx != mp_data_holder->point_size) {
		NVT_ERR("%s array size mismatched (%d/%d)",
			mp_data_holder->name, num_array_idx,
			mp_data_holder->point_size);
		ret = -EINVAL;
		goto end;
	} else {
		mp_data_holder->matched = 1;
	}

	// make sure all arrays are fit in
	for (i = 0; i < mp_data_table_size; i++) {
		if (!pen_support && (mp_data_table[i].type == PEN_X_TYPE ||
				     mp_data_table[i].type == PEN_Y_TYPE))
			continue;
		if (!mp_data_table[i].matched) {
			NVT_ERR("%s array is not matched",
				mp_data_table[i].name);
			ret = -EINVAL;
			goto end;
		}
	}

end:
	return ret;
}

static void test_array(struct raw_data raw, struct mp_data mp)
{
	uint16_t i = 0;

	for (i = 0; i < mp.point_size; i++) {
		if (((mp.lmt_type == LMT_TYPE_UPPER) &&
		     (raw.array[i] > mp.array[i])) ||
		    (mp.lmt_type == LMT_TYPE_LOWER &&
		     raw.array[i] < mp.array[i]))
			mp.array_result[i] = MP_RESULT_ERROR_VALUE;
	}
}

// line_num is x size
static void print_array(int16_t *arr, uint16_t arr_size, uint8_t line_num)
{
	uint16_t i = 0, j = 0, num_size = 0, num_size_max = 0;
	char num[MAX_NUM_SIZE] = { 0 };

	num_size_max = 0;
	for (i = 0; i < arr_size; i++) {
		num_size = snprintf(NULL, 0, "%i", arr[i]);
		if (num_size > num_size_max)
			num_size_max = num_size;
	}

	for (i = 0; i < arr_size; i++) {
		num_size = snprintf(num, MAX_NUM_SIZE, "%d", arr[i]);

		for (j = 0; j < (num_size_max - num_size) + 1; j++)
			printf(" ");
		printf("%s", num);

		if ((i + 1) % line_num == 0)
			printf("\n");
	}
}

static int32_t nvt_mp_init(uint8_t pen_support, char *mp_lmt_path)
{
	int32_t fd = 0, fsize = 0, ret = 0, i = 0;
	uint16_t touch_point_size = 0, pen_x_point_size = 0,
		 pen_y_point_size = 0;
	char *fdata = NULL;

	if (mp_lmt_path) {
		fd = open(mp_lmt_path, O_RDONLY);
		if (fd < 0) {
			NVT_ERR("Can not open %s", mp_lmt_path);
			ret = -ENOENT;
			goto end;
		}
		fsize = lseek(fd, 0, SEEK_END);
		lseek(fd, 0, SEEK_SET);
		fdata = malloc(fsize);
		ret = read(fd, fdata, fsize);
		if (ret < 0) {
			NVT_ERR("Error opening %s", mp_lmt_path);
			goto end;
		}
	}

	touch_point_size = ts->touch_x_num * ts->touch_y_num;
	if (pen_support) {
		pen_x_point_size = ts->pen_x_num * ts->pen_y_num_gang;
		pen_y_point_size = ts->pen_y_num * ts->pen_x_num_gang;
	}

	mp_data_table_size = sizeof(mp_data_table) / sizeof(mp_data_table[0]);
	raw_data_table_size =
		sizeof(raw_data_table) / sizeof(raw_data_table[0]);

	// init raw
	for (i = 0; i < raw_data_table_size; i++) {
		if (!pen_support && (raw_data_table[i].type == PEN_X_TYPE ||
				     raw_data_table[i].type == PEN_Y_TYPE))
			continue;

		switch (raw_data_table[i].type) {
		case TOUCH_TYPE:
			raw_data_table[i].point_size = touch_point_size;
			raw_data_table[i].byte_size =
				touch_point_size * _BYTE_PER_POINT;
			raw_data_table[i].line_num = ts->touch_x_num;
			break;
		case PEN_X_TYPE:
			raw_data_table[i].point_size = pen_x_point_size;
			raw_data_table[i].byte_size =
				pen_x_point_size * _BYTE_PER_POINT;
			raw_data_table[i].line_num = ts->pen_x_num;
			break;
		case PEN_Y_TYPE:
			raw_data_table[i].point_size = pen_y_point_size;
			raw_data_table[i].byte_size =
				pen_y_point_size * _BYTE_PER_POINT;
			raw_data_table[i].line_num = ts->pen_x_num_gang;
			break;
		case SINGLE_INPUT_TYPE:
			raw_data_table[i].point_size = 1;
			raw_data_table[i].byte_size = 1 * _BYTE_PER_POINT;
			raw_data_table[i].line_num = 1;
			break;
		}
		raw_data_table[i].array = malloc(raw_data_table[i].byte_size);
		memset(raw_data_table[i].array, 0, raw_data_table[i].byte_size);
		if (!raw_data_table[i].array) {
			ret = -ENOMEM;
			goto end;
		}
	}

	// init criteria
	for (i = 0; i < sizeof(mp_data_table) / sizeof(mp_data_table[0]); i++) {
		if (!pen_support && (mp_data_table[i].type == PEN_X_TYPE ||
				     mp_data_table[i].type == PEN_Y_TYPE))
			continue;

		switch (mp_data_table[i].type) {
		case TOUCH_TYPE:
			mp_data_table[i].point_size = touch_point_size;
			mp_data_table[i].byte_size =
				touch_point_size * _BYTE_PER_POINT;
			mp_data_table[i].line_num = ts->touch_x_num;
			break;
		case PEN_X_TYPE:
			mp_data_table[i].point_size = pen_x_point_size;
			mp_data_table[i].byte_size =
				pen_x_point_size * _BYTE_PER_POINT;
			mp_data_table[i].line_num = ts->pen_x_num;
			break;
		case PEN_Y_TYPE:
			mp_data_table[i].point_size = pen_y_point_size;
			mp_data_table[i].byte_size =
				pen_y_point_size * _BYTE_PER_POINT;
			mp_data_table[i].line_num = ts->pen_x_num_gang;
			break;
		case SINGLE_INPUT_TYPE:
			mp_data_table[i].point_size = 1;
			mp_data_table[i].byte_size = 1 * _BYTE_PER_POINT;
			mp_data_table[i].line_num = 1;
			break;
		}
		mp_data_table[i].array = malloc(mp_data_table[i].byte_size);
		memset(mp_data_table[i].array, 0, mp_data_table[i].byte_size);
		mp_data_table[i].array_result =
			malloc(mp_data_table[i].byte_size);
		memset(mp_data_table[i].array_result, 0,
		       mp_data_table[i].byte_size);
		if (!mp_data_table[i].array || !mp_data_table[i].array_result) {
			ret = -ENOMEM;
			goto end;
		}

		if (!mp_lmt_path) {
			memset16((uint8_t *)mp_data_table[i].array,
				 mp_data_table[i].lmt,
				 mp_data_table[i].point_size);
		}
	}

	if (mp_lmt_path) {
		ret = read_mp_lmt_array_from_file(fdata, fsize, pen_support);
		if (ret) {
			NVT_ERR("Failed to parse mp limits from file");
			goto end;
		}
	}

end:
	return ret;
}

static int32_t nvt_polling_hand_shake_status(void)
{
	uint8_t buf[1] = { 0 };
	int32_t ret = 0, retry = 250;

	NVT_DBG("start");
	msleep(20);

	while (--retry) {
		buf[0] = 0x00;
		ctp_hid_read(ts->mmap->event_buf_hs_sub_cmd_addr, buf, 1);

		if ((buf[0] == 0xA0) || (buf[0] == 0xA1))
			goto end;

		msleep(10);
	}

	if (retry == 0) {
		NVT_ERR("polling hand shake status failed, buf[0]=0x%02X\n",
			buf[0]);
		ret = -EAGAIN;
	}
end:
	NVT_DBG("end");
	return ret;
}

static void nvt_change_mode(uint8_t mode)
{
	uint8_t buf[1] = { 0 };

	NVT_DBG("start");
	buf[0] = mode;
	ctp_hid_write(ts->mmap->event_buf_cmd_addr, buf, 1);

	if (mode == NORMAL_MODE) {
		msleep(20);
		buf[0] = HANDSHAKING_HOST_READY;
		ctp_hid_write(ts->mmap->event_buf_hs_sub_cmd_addr, buf, 1);
		msleep(20);
	}
}

static uint8_t nvt_get_fw_pipe(void)
{
	uint8_t buf[1] = { 0 };

	ctp_hid_read(ts->mmap->event_buf_hs_sub_cmd_addr, buf, 1);

	return (buf[0] & 0x01);
}

static int8_t nvt_switch_freq_hop(uint8_t freq_hop)
{
	uint8_t buf[1] = { 0 }, retry = 20;
	int32_t ret = 0;

	NVT_DBG("start");
	while (--retry) {
		buf[0] = freq_hop;
		ctp_hid_write(ts->mmap->event_buf_cmd_addr, buf, 1);

		msleep(35);

		buf[0] = 0xFF;
		ctp_hid_read(ts->mmap->event_buf_cmd_addr, buf, 1);

		if (buf[0] == 0x00)
			break;
	}

	if (unlikely(retry == 0)) {
		NVT_ERR("switch freq_hop 0x%02X failed, buf[0]=0x%02X\n",
			freq_hop, buf[0]);
		ret = -EAGAIN;
	}

	return ret;
}

static void nvt_get_mdata(uint32_t addr, int16_t *buf, uint16_t buf_size)
{
	uint16_t i = 0, res = 0;

	for (i = 0; i < buf_size / NVT_TRANSFER_LEN; i++) {
		ctp_hid_read(addr, (uint8_t *)buf, NVT_TRANSFER_LEN);
		addr += NVT_TRANSFER_LEN;
		buf += NVT_TRANSFER_LEN / 2;
	}

	res = buf_size % NVT_TRANSFER_LEN;
	if (res)
		ctp_hid_read(addr, (uint8_t *)buf, res);
}

static void nvt_read_touch_baseline(void)
{
	NVT_DBG("start");
	nvt_get_mdata(ts->mmap->touch_baseline_addr,
		      raw_data_table[RAW_TOUCH_BASELINE].array,
		      raw_data_table[RAW_TOUCH_BASELINE].byte_size);
}

static void nvt_read_touch_cc(void)
{
	NVT_DBG("start");
	nvt_get_mdata(nvt_get_fw_pipe() ? // CC: fw pipe 1 in diff pipe 0
			      ts->mmap->touch_diff_pipe0_addr :
			      ts->mmap->touch_diff_pipe1_addr,
		      raw_data_table[RAW_TOUCH_CC].array,
		      raw_data_table[RAW_TOUCH_CC].byte_size);
}

static void nvt_read_pen_baseline(void)
{
	NVT_DBG("start");
	nvt_get_mdata(ts->mmap->pen_2d_bl_tip_x_addr,
		      raw_data_table[RAW_PEN_TIP_X_BASELINE].array,
		      raw_data_table[RAW_PEN_TIP_X_BASELINE].byte_size);
	nvt_get_mdata(ts->mmap->pen_2d_bl_tip_y_addr,
		      raw_data_table[RAW_PEN_TIP_Y_BASELINE].array,
		      raw_data_table[RAW_PEN_TIP_Y_BASELINE].byte_size);
	nvt_get_mdata(ts->mmap->pen_2d_bl_ring_x_addr,
		      raw_data_table[RAW_PEN_RING_X_BASELINE].array,
		      raw_data_table[RAW_PEN_RING_X_BASELINE].byte_size);
	nvt_get_mdata(ts->mmap->pen_2d_bl_ring_y_addr,
		      raw_data_table[RAW_PEN_RING_Y_BASELINE].array,
		      raw_data_table[RAW_PEN_RING_Y_BASELINE].byte_size);
}

static void nvt_enable_noise_collect(uint8_t test_len)
{
	uint8_t buf[4] = { 0 };

	NVT_DBG("start");
	buf[0] = 0x47;
	buf[1] = 0xAA;
	buf[2] = test_len;
	buf[3] = 0x00;
	ctp_hid_write(ts->mmap->event_buf_cmd_addr, buf, 4);
}

static int32_t nvt_clear_fw_status(void)
{
	uint8_t buf[1], retry = 20;
	int32_t ret = 0;

	NVT_DBG("start");
	while (--retry) {
		buf[0] = 0x00;
		ctp_hid_write(ts->mmap->event_buf_hs_sub_cmd_addr, buf, 1);
		ctp_hid_read(ts->mmap->event_buf_hs_sub_cmd_addr, buf, 1);
		if (buf[0] == 0x00)
			break;
		msleep(10);
	}

	if (!retry) {
		NVT_ERR("failed, buf[0]=0x%02X\n", buf[0]);
		ret = -EAGAIN;
	}

	return ret;
}

static int32_t nvt_read_noise(uint8_t pen_support)
{
	uint32_t i = 0, test_len = 0;
	int32_t ret = 0;

	NVT_DBG("start");
	if (nvt_clear_fw_status()) {
		ret = -EAGAIN;
		goto end;
	}

	test_len = mp_data_table[MP_DIFF_TEST_FRAME].array[0] / 10;
	if (!test_len)
		test_len = 1;
	nvt_enable_noise_collect(test_len);
	msleep(test_len * (10000 / TOUCH_HZ));

	if (nvt_polling_hand_shake_status()) {
		ret = -EAGAIN;
		goto end;
	}

	nvt_get_mdata(nvt_get_fw_pipe() ? ts->mmap->touch_diff_pipe1_addr :
					  ts->mmap->touch_diff_pipe0_addr,
		      raw_data_table[RAW_TOUCH_DIFF].array,
		      raw_data_table[RAW_TOUCH_DIFF].byte_size);

	for (i = 0; i < raw_data_table[RAW_TOUCH_DIFF].point_size; i++) {
		raw_data_table[RAW_TOUCH_DIFF_MAX].array[i] =
			(int8_t)((raw_data_table[RAW_TOUCH_DIFF].array[i] >>
				  8) &
				 0xFF);
		raw_data_table[RAW_TOUCH_DIFF_MIN].array[i] =
			(int8_t)(raw_data_table[RAW_TOUCH_DIFF].array[i] &
				 0xFF);
	}

	if (pen_support) {
		nvt_get_mdata(ts->mmap->pen_2d_diff_max_tip_x_addr,
			      raw_data_table[RAW_PEN_TIP_X_DIFF_MAX].array,
			      raw_data_table[RAW_PEN_TIP_X_DIFF_MAX].byte_size);
		nvt_get_mdata(ts->mmap->pen_2d_diff_min_tip_x_addr,
			      raw_data_table[RAW_PEN_TIP_X_DIFF_MIN].array,
			      raw_data_table[RAW_PEN_TIP_X_DIFF_MIN].byte_size);
		nvt_get_mdata(ts->mmap->pen_2d_diff_max_tip_y_addr,
			      raw_data_table[RAW_PEN_TIP_Y_DIFF_MAX].array,
			      raw_data_table[RAW_PEN_TIP_Y_DIFF_MAX].byte_size);
		nvt_get_mdata(ts->mmap->pen_2d_diff_min_tip_y_addr,
			      raw_data_table[RAW_PEN_TIP_Y_DIFF_MIN].array,
			      raw_data_table[RAW_PEN_TIP_Y_DIFF_MIN].byte_size);
		nvt_get_mdata(
			ts->mmap->pen_2d_diff_max_ring_x_addr,
			raw_data_table[RAW_PEN_RING_X_DIFF_MAX].array,
			raw_data_table[RAW_PEN_RING_X_DIFF_MAX].byte_size);
		nvt_get_mdata(
			ts->mmap->pen_2d_diff_min_ring_x_addr,
			raw_data_table[RAW_PEN_RING_X_DIFF_MIN].array,
			raw_data_table[RAW_PEN_RING_X_DIFF_MIN].byte_size);
		nvt_get_mdata(
			ts->mmap->pen_2d_diff_max_ring_y_addr,
			raw_data_table[RAW_PEN_RING_Y_DIFF_MAX].array,
			raw_data_table[RAW_PEN_RING_Y_DIFF_MAX].byte_size);
		nvt_get_mdata(
			ts->mmap->pen_2d_diff_min_ring_y_addr,
			raw_data_table[RAW_PEN_RING_Y_DIFF_MIN].array,
			raw_data_table[RAW_PEN_RING_Y_DIFF_MIN].byte_size);
	}

	nvt_change_mode(NORMAL_MODE);
end:
	return ret;
}

static void nvt_enable_open_test(void)
{
	uint8_t buf[4] = { 0 };

	NVT_DBG("start");
	buf[0] = 0x45;
	buf[1] = 0xAA;
	buf[2] = 0x02;
	buf[3] = 0x00;
	ctp_hid_write(ts->mmap->event_buf_cmd_addr, buf, 4);
	return;
}

static void nvt_enable_short_test(void)
{
	uint8_t buf[4] = { 0 };

	NVT_DBG("start");
	buf[0] = 0x43;
	buf[1] = 0xAA;
	buf[2] = 0x02;
	buf[3] = 0x00;
	ctp_hid_write(ts->mmap->event_buf_cmd_addr, buf, 4);
	return;
}

static int32_t nvt_read_touch_open(void)
{
	int32_t ret = 0;

	NVT_DBG("start");
	if (nvt_clear_fw_status()) {
		ret = -EAGAIN;
		goto end;
	}

	nvt_enable_open_test();

	if (nvt_polling_hand_shake_status()) {
		ret = -EAGAIN;
		goto end;
	}

	nvt_get_mdata(nvt_get_fw_pipe() ? ts->mmap->touch_raw_pipe1_addr :
					  ts->mmap->touch_raw_pipe0_addr,
		      raw_data_table[RAW_TOUCH_OPEN].array,
		      raw_data_table[RAW_TOUCH_OPEN].byte_size);

	nvt_change_mode(NORMAL_MODE);
end:
	return ret;
}

static int32_t nvt_read_touch_short(void)
{
	int32_t ret = 0;

	NVT_DBG("start");
	if (nvt_clear_fw_status()) {
		ret = -EAGAIN;
		goto end;
	}

	nvt_enable_short_test();

	if (nvt_polling_hand_shake_status()) {
		ret = -EAGAIN;
		goto end;
	}

	nvt_get_mdata(nvt_get_fw_pipe() ? ts->mmap->touch_raw_pipe1_addr :
					  ts->mmap->touch_raw_pipe0_addr,
		      raw_data_table[RAW_TOUCH_SHORT].array,
		      raw_data_table[RAW_TOUCH_SHORT].byte_size);

	nvt_change_mode(NORMAL_MODE);
end:
	return ret;
}

int32_t nvt_selftest_open(uint8_t pen_support, char *criteria_path)
{
	uint8_t test_result = 0;
	uint8_t buf[64] = { 0 };
	int32_t i = 0, j = 0, ret = 0;

	NVT_DBG("start");

	ret = nvt_get_fw_ver_and_info(0);
	if (ret)
		goto end_err;

	ret = nvt_mp_init(pen_support, criteria_path);
	if (ret)
		goto end_err;

	ret = nvt_check_fw_reset_state(RESET_STATE_REK_BASELINE);
	if (ret)
		goto end_err;

	ret = nvt_switch_freq_hop(FREQ_HOP_DISABLE);
	if (ret)
		goto end_err;

	ret = nvt_check_fw_reset_state(RESET_STATE_NORMAL_RUN);
	if (ret)
		goto end_err;

	msleep(100);

	ret = nvt_clear_fw_status();
	if (ret)
		goto end_err;

	nvt_change_mode(MP_MODE_BASELINE_CC);

	ret = nvt_check_fw_status();
	if (ret)
		goto end_err;

	NVT_DBG("Read rawdata");

	nvt_read_touch_baseline();
	nvt_read_touch_cc();
	if (pen_support)
		nvt_read_pen_baseline();

	nvt_change_mode(NORMAL_MODE); // normal for noise, short, open

	ret = nvt_read_noise(pen_support);
	if (ret)
		goto end_err;
	ret = nvt_read_touch_short();
	if (ret)
		goto end_err;
	ret = nvt_read_touch_open();
	if (ret)
		goto end_err;

	// output result
	NVT_DBG("Test rawdata");
	printf("===================================\n");
	printf("========== [MP CRITERIA] ==========\n");
	printf("===================================\n\n");
	for (i = 0; i < mp_data_table_size; i++) {
		if (!pen_support && (mp_data_table[i].type == PEN_X_TYPE ||
				     mp_data_table[i].type == PEN_Y_TYPE))
			continue;
		printf("[%s]\n", mp_data_table[i].name);
		print_array(mp_data_table[i].array, mp_data_table[i].point_size,
			    mp_data_table[i].line_num);
		printf("\n");
	}

	printf("===============================\n");
	printf("========== [RAWDATA] ==========\n");
	printf("===============================\n\n");
	for (i = 0; i < raw_data_table_size; i++) {
		if (raw_data_table[i].skip_print)
			continue;
		if (!pen_support && (raw_data_table[i].type == PEN_X_TYPE ||
				     raw_data_table[i].type == PEN_Y_TYPE))
			continue;
		printf("[%s]\n", raw_data_table[i].name);
		print_array(raw_data_table[i].array,
			    raw_data_table[i].point_size,
			    raw_data_table[i].line_num);
		printf("\n");
	}

	printf("===================================\n");
	printf("========== [TEST RESULT] ==========\n");
	printf("===================================\n\n");
	test_array(raw_data_table[RAW_TOUCH_SHORT],
		   mp_data_table[MP_TOUCH_SHORT_P]);
	test_array(raw_data_table[RAW_TOUCH_SHORT],
		   mp_data_table[MP_TOUCH_SHORT_N]);
	test_array(raw_data_table[RAW_TOUCH_OPEN],
		   mp_data_table[MP_TOUCH_OPEN_P]);
	test_array(raw_data_table[RAW_TOUCH_OPEN],
		   mp_data_table[MP_TOUCH_OPEN_N]);
	test_array(raw_data_table[RAW_TOUCH_DIFF_MAX],
		   mp_data_table[MP_TOUCH_DIFF_P]);
	test_array(raw_data_table[RAW_TOUCH_DIFF_MAX],
		   mp_data_table[MP_TOUCH_DIFF_N]);
	test_array(raw_data_table[RAW_TOUCH_DIFF_MIN],
		   mp_data_table[MP_TOUCH_DIFF_P]);
	test_array(raw_data_table[RAW_TOUCH_DIFF_MIN],
		   mp_data_table[MP_TOUCH_DIFF_N]);
	test_array(raw_data_table[RAW_TOUCH_BASELINE],
		   mp_data_table[MP_TOUCH_BASELINE_P]);
	test_array(raw_data_table[RAW_TOUCH_BASELINE],
		   mp_data_table[MP_TOUCH_BASELINE_N]);
	test_array(raw_data_table[RAW_TOUCH_CC], mp_data_table[MP_TOUCH_CC_P]);
	test_array(raw_data_table[RAW_TOUCH_CC], mp_data_table[MP_TOUCH_CC_N]);
	if (pen_support) {
		test_array(raw_data_table[RAW_PEN_TIP_X_BASELINE],
			   mp_data_table[MP_PEN_TIP_X_BASELINE_P]);
		test_array(raw_data_table[RAW_PEN_TIP_X_BASELINE],
			   mp_data_table[MP_PEN_TIP_X_BASELINE_N]);
		test_array(raw_data_table[RAW_PEN_TIP_Y_BASELINE],
			   mp_data_table[MP_PEN_TIP_Y_BASELINE_P]);
		test_array(raw_data_table[RAW_PEN_TIP_Y_BASELINE],
			   mp_data_table[MP_PEN_TIP_Y_BASELINE_N]);
		test_array(raw_data_table[RAW_PEN_RING_X_BASELINE],
			   mp_data_table[MP_PEN_RING_X_BASELINE_P]);
		test_array(raw_data_table[RAW_PEN_RING_X_BASELINE],
			   mp_data_table[MP_PEN_RING_X_BASELINE_N]);
		test_array(raw_data_table[RAW_PEN_RING_Y_BASELINE],
			   mp_data_table[MP_PEN_RING_Y_BASELINE_P]);
		test_array(raw_data_table[RAW_PEN_RING_Y_BASELINE],
			   mp_data_table[MP_PEN_RING_Y_BASELINE_N]);

		test_array(raw_data_table[RAW_PEN_TIP_X_DIFF_MAX],
			   mp_data_table[MP_PEN_TIP_X_DIFF_P]);
		test_array(raw_data_table[RAW_PEN_TIP_X_DIFF_MAX],
			   mp_data_table[MP_PEN_TIP_X_DIFF_N]);
		test_array(raw_data_table[RAW_PEN_TIP_X_DIFF_MIN],
			   mp_data_table[MP_PEN_TIP_X_DIFF_P]);
		test_array(raw_data_table[RAW_PEN_TIP_X_DIFF_MIN],
			   mp_data_table[MP_PEN_TIP_X_DIFF_N]);

		test_array(raw_data_table[RAW_PEN_TIP_Y_DIFF_MAX],
			   mp_data_table[MP_PEN_TIP_Y_DIFF_P]);
		test_array(raw_data_table[RAW_PEN_TIP_Y_DIFF_MAX],
			   mp_data_table[MP_PEN_TIP_Y_DIFF_N]);
		test_array(raw_data_table[RAW_PEN_TIP_Y_DIFF_MIN],
			   mp_data_table[MP_PEN_TIP_Y_DIFF_P]);
		test_array(raw_data_table[RAW_PEN_TIP_Y_DIFF_MIN],
			   mp_data_table[MP_PEN_TIP_Y_DIFF_N]);

		test_array(raw_data_table[RAW_PEN_RING_X_DIFF_MAX],
			   mp_data_table[MP_PEN_RING_X_DIFF_P]);
		test_array(raw_data_table[RAW_PEN_RING_X_DIFF_MAX],
			   mp_data_table[MP_PEN_RING_X_DIFF_N]);
		test_array(raw_data_table[RAW_PEN_RING_X_DIFF_MIN],
			   mp_data_table[MP_PEN_RING_X_DIFF_P]);
		test_array(raw_data_table[RAW_PEN_RING_X_DIFF_MIN],
			   mp_data_table[MP_PEN_RING_X_DIFF_N]);

		test_array(raw_data_table[RAW_PEN_RING_Y_DIFF_MAX],
			   mp_data_table[MP_PEN_RING_Y_DIFF_P]);
		test_array(raw_data_table[RAW_PEN_RING_Y_DIFF_MAX],
			   mp_data_table[MP_PEN_RING_Y_DIFF_N]);
		test_array(raw_data_table[RAW_PEN_RING_Y_DIFF_MIN],
			   mp_data_table[MP_PEN_RING_Y_DIFF_P]);
		test_array(raw_data_table[RAW_PEN_RING_Y_DIFF_MIN],
			   mp_data_table[MP_PEN_RING_Y_DIFF_N]);
	}

	for (i = 0; i < mp_data_table_size; i++) {
		if (mp_data_table[i].type == SINGLE_INPUT_TYPE)
			continue;
		if (!pen_support && (mp_data_table[i].type == PEN_X_TYPE ||
				     mp_data_table[i].type == PEN_Y_TYPE))
			continue;

		test_result = TEST_PASS;
		for (j = 0; j < mp_data_table[i].point_size; j++) {
			if (mp_data_table[i].array_result[j] ==
			    MP_RESULT_ERROR_VALUE)
				test_result = TEST_FAIL;
		}

		if (test_result == TEST_PASS) {
			printf("%s PASS\n", mp_data_table[i].name);
		} else {
			printf("%s FAIL: (0 : pass, 1 : fail)\n",
			       mp_data_table[i].name);
			print_array(mp_data_table[i].array_result,
				    mp_data_table[i].point_size,
				    mp_data_table[i].line_num);
			printf("\n");
		}
	}

	NVT_DBG("end");
	// only bootloader reset if all test have performed
	nvt_bootloader_reset();
	goto end;

end_err:
	NVT_LOG("history 0x%X :", ts->mmap->mmap_history_event0);
	ctp_hid_read(ts->mmap->mmap_history_event0, buf, 64);
	NVT_LOG_HEX(buf, 64);
	NVT_LOG("history 0x%X :", ts->mmap->mmap_history_event1);
	ctp_hid_read(ts->mmap->mmap_history_event1, buf, 64);
	NVT_LOG_HEX(buf, 64);
end:
	return ret;
}
