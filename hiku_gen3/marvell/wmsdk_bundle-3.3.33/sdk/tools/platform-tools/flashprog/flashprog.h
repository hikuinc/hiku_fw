/*
 * Copyright 2008-2015, Marvell International Ltd.
 * All Rights Reserved.
 */

#ifndef _FLASHPROG_H_
#define _FLASHPROG_H_

/* Version Define */
#define VERSION_BYTE_1 2
#define VERSION_BYTE_2 0
#define VERSION_BYTE_3 6

/* Data Type Define */

typedef unsigned int     u32;
typedef unsigned short   u16;
typedef unsigned char    u8;
typedef int              s32;
typedef short            s16;
typedef char             s8;


#if defined(CONFIG_CPU_MC200)
typedef struct flash_device_config {
	char name[16];
	u32 jedec_id;
	u32 chip_size;
	u32 sector_size;
	u32 block_size;
	u16 page_size;
} storage_info;
#endif

typedef union {
	u8 c[4];
	u16 s[2];
	u32 w;
} u;

FILE *fp_fl[3];

u32 spi_readbuf(void *buf, u32 addr, u32 size);
int fl_dev_read(int fl_dev, u32 addr, u8 *buf, u32 len);
int fl_dev_write(int fl_dev, u32 addr, u8 *buf, u32 len);
int fl_dev_erase(int fl_dev, u32 start, u32 end);
int fl_dev_eraseall(int fl_dev);
int flash_get_comp(const char *comp);
char *flash_get_fc_name(enum flash_comp fc);
int validate_flash_layout(struct partition_entry *f, int count);
void check_fl_layout_and_warn();
int fl_read_part_table(int device, uint32_t part_addr,
		struct partition_table *pt);
int fl_read_part_entries(int device, uint32_t addr);
int part_update(u32 addr, struct partition_table *pt,
				struct partition_entry *pe);

extern struct partition_table g_flash_table;
extern struct partition_entry g_flash_parts[MAX_FL_COMP];
extern u32 *gbuffer_1;

#endif /* _FLASHPROG_H_ */
