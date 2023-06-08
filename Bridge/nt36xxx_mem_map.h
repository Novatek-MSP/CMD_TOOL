#define CHIP_VER_TRIM_ADDR 0x3F004
#define CHIP_VER_TRIM_OLD_ADDR 0x1F64E

typedef struct nvt_ts_reg {
	uint32_t addr; /* byte in which address */
	uint8_t mask; /* in which bits of that byte */
} nvt_ts_reg_t;

struct nvt_ts_mem_map {
	uint32_t EVENT_BUF_ADDR;
	uint32_t READ_FLASH_CHECKSUM_ADDR;
	uint32_t RW_FLASH_DATA_ADDR;
	uint32_t G_ILM_CHECKSUM_ADDR;
	uint32_t G_DLM_CHECKSUM_ADDR;
	uint32_t R_ILM_CHECKSUM_ADDR;
	uint32_t R_DLM_CHECKSUM_ADDR;
};

struct nvt_ts_hw_info {
	uint8_t hw_crc;
	uint8_t use_gcm;
};

/* tddi */
static const struct nvt_ts_mem_map NT36523_memory_map = {
	.EVENT_BUF_ADDR           = 0x2FE00,
	.READ_FLASH_CHECKSUM_ADDR = 0x24000,
	.RW_FLASH_DATA_ADDR       = 0x24002,
	.G_ILM_CHECKSUM_ADDR      = 0x3F100,
	.G_DLM_CHECKSUM_ADDR      = 0x3F104,
	.R_ILM_CHECKSUM_ADDR      = 0x3F120,
	.R_DLM_CHECKSUM_ADDR      = 0x3F124,
};


static struct nvt_ts_hw_info NT36523_hw_info = {
	.hw_crc         = 2,
};

#define NVT_ID_BYTE_MAX 6
struct nvt_ts_trim_id_table {
	uint8_t id[NVT_ID_BYTE_MAX];
	uint8_t mask[NVT_ID_BYTE_MAX];
	const struct nvt_ts_mem_map *mmap;
	const struct nvt_ts_mem_map *mmap_casc;
	const struct nvt_ts_hw_info *hwinfo;
};

static const struct nvt_ts_trim_id_table trim_id_table[] = {
	{.id = {0x20, 0xFF, 0xFF, 0x23, 0x65, 0x03}, .mask = {1, 0, 0, 1, 1, 1},
		.mmap = &NT36523_memory_map,  .hwinfo = &NT36523_hw_info},
	{.id = {0x0C, 0xFF, 0xFF, 0x23, 0x65, 0x03}, .mask = {1, 0, 0, 1, 1, 1},
		.mmap = &NT36523_memory_map,  .hwinfo = &NT36523_hw_info},
	{.id = {0x0B, 0xFF, 0xFF, 0x23, 0x65, 0x03}, .mask = {1, 0, 0, 1, 1, 1},
		.mmap = &NT36523_memory_map,  .hwinfo = &NT36523_hw_info},
	{.id = {0x0A, 0xFF, 0xFF, 0x23, 0x65, 0x03}, .mask = {1, 0, 0, 1, 1, 1},
		.mmap = &NT36523_memory_map,  .hwinfo = &NT36523_hw_info},
	{.id = {0xFF, 0xFF, 0xFF, 0x23, 0x65, 0x03}, .mask = {0, 0, 0, 1, 1, 1},
		.mmap = &NT36523_memory_map,  .hwinfo = &NT36523_hw_info},
};
