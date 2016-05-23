/*
 * Copyright 2008-2015, Marvell International Ltd.
 * All Rights Reserved.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <flash.h>
#include <partition.h>
#include <flash_layout.h>
#include <lowlevel_drivers.h>
#include "flashprog.h"
#include "crc32.h"
#include "firmware.h"

#define STRINGIFY_1(x)  #x
#define S(x)		STRINGIFY_1(x)
#define MAX_BUFFER_SIZE 512
#define MAX_FILE_PATH   256
#define MAX_COMP_SIZE   32
#define BUF_SIZE        8
static char gbuffer[MAX_BUFFER_SIZE];

static const struct flash_device_config *flash;
static u *image;
u32 *gbuffer_1, g_part_table;

#ifdef __GNUC__

#include <unistd.h>
#include <fcntl.h>

#else

/*
 * Minimal unistd emulation for open(), read(), write(), lseek() and close()
 * using stdio calls.  The stdio layer is avoided when possible because it
 * buffers the data in small chunks causing extra semihosting requests with
 * non negligible overhead.
 */

#define O_RDONLY	0
#define O_WRONLY	1
#define O_RDWR		2
#define O_BINARY	4

#define NR_FILE_DESC	5

static FILE *file_desc[NR_FILE_DESC] = { stdin, stdout, stderr, };

typedef long ssize_t;
typedef long off_t;

int open(const char *pathname, int flags)
{
	char mode[3];
	int i;

	memset(mode, 0, sizeof(mode));
	if ((flags & 3) == O_RDONLY)
		mode[0] = 'r';
	else if ((flags & 3) == O_WRONLY)
		mode[0] = 'w';
	else
		return -1;
	if (flags & O_BINARY)
		mode[1] = 'b';

	for (i = 3; i < NR_FILE_DESC; i++) {
		if (file_desc[i])
			continue;
		file_desc[i] = fopen(pathname, mode);
		if (!file_desc[i])
			return -1;
		return i;
	}
	return -1;
}

int close(int fd)
{
	int ret = fclose(file_desc[fd]);
	file_desc[fd] = NULL;
	return ret;
}

ssize_t read(int fd, void *buf, size_t count)
{
	FILE *f = file_desc[fd];
	ssize_t ret = fread(buf, 1, count, f);
	if (!ret && ferror(f))
		ret = -1;
	return ret;
}

ssize_t write(int fd, const void *buf, size_t count)
{
	FILE *f = file_desc[fd];
	ssize_t ret = fwrite(buf, 1, count, f);
	if (!ret && ferror(f))
		ret = -1;
	return ret;
}

off_t lseek(int fd, off_t offset, int whence)
{
	FILE *f = file_desc[fd];
	if (fseek(f, offset, whence) != 0)
		return -1;
	return ftell(f);
}

#endif

#if defined(CONFIG_CPU_MC200)
static const struct flash_device_config flash_config = {
/*
 *      name,	jedec id,	chip_size,	sector_size,	block_size,
 *      page_size
 */
	"qspi0, qspi1", 0x202013, MC200_FLASH_CHIP_SIZE,
		MC200_FLASH_SECTOR_SIZE, MC200_FLASH_BLOCK_SIZE,
		MC200_FLASH_PAGE_SIZE
};
#endif

static char *convert_cygpath_to_windows(char *path)
{
	int status;

	status = strncmp(path, "/cygdrive", strlen("/cygdrive"));
	if (!status) {
		/* Cygwin style path, convert to windows path with regular
		 * slashes (C:/WINNT)
		 */
		path += strlen("/cygdrive");
		path[0] = path[1];
		path[1] = ':';
	}
	return path;
}

static int flashprog_open(const char *name, int flags)
{
	char path[MAX_FILE_PATH];

	/* Let keep source buffer sane */
	strncpy(path, name, sizeof(path));

	/* If it is cygwin style path then convert to windows one */
	return open(convert_cygpath_to_windows(path), flags);
}

static int load_partition_table(u32 addr, struct partition_table *ph)
{
	u32 crc;

	fl_dev_read(FL_PART_DEV, addr, (u8 *) ph, sizeof(*ph));
	crc = crc32(ph, sizeof(*ph), 0);

	if (crc != 0 || ph->magic != PARTITION_TABLE_MAGIC)
		return -1;
	return 0;
}

static u32 dump_partition_table(u32 addr)
{
	struct partition_table pH;
	int ret;

	ret = load_partition_table(addr, &pH);
	if (ret == 0) {
		printf("\n###### Partition Table : %s ######\n",
				(addr == g_part_table) ? "(Active)" : "");
		printf("Magic: 0x%x\n", (unsigned int)pH.magic);
		printf("Version: %d\n",
		       (unsigned int)pH.version);
		printf("Partition entries : %d\n",
			(unsigned int)pH.partition_entries_no);
		printf("Generation level : %d\n",
			(unsigned int)pH.gen_level);
		printf("Checksum : 0x%x\n\n", (unsigned int)pH.crc);
		return 0;
	} else {
		printf(">>> Error: Invalid partition table\n");
		return -1;
	}
}

static u32 dump_partition_entries()
{
	return 0;
}

static void partition_info(void)
{
	dump_partition_table(FL_PART1_START);
	dump_partition_table(FL_PART2_START);
	dump_partition_entries();
}

static unsigned int get_block_num(int total_blocks)
{
	unsigned int block;
	char str[10];

	printf("*** Enter details for flash read operation\n");

	do {
		fprintf(stderr, "Block [0-%d]: ", total_blocks);
		fgets(str, sizeof(str), stdin);
		sscanf(str, "%x", &block);

	} while (!(block <= total_blocks));

	return block;
}

static void dump_block(int type)
{
	u32 addr;
	u32 len = 0, offset = 0;
	u8 val[16];
	int i, n = 0;
	unsigned int total_blocks = flash->chip_size / flash->block_size;
	unsigned int block = get_block_num(total_blocks - 1);

	addr = block * flash->block_size;

#ifdef ARM_GNU
#define MAX_STRING_LEN 16
	char _tmp[MAX_STRING_LEN];
#endif /* ARM_GNU */
	memset(_tmp, 0, MAX_STRING_LEN);
	fprintf(stderr, "Offset [0x0-0xFFFF]: ");
	fgets((char *)val, sizeof(val), stdin);
#ifdef ARM_GNU
	sscanf((char *)val, "%s", _tmp);
	sscanf(_tmp, "%x", &offset);
#else
	scanf((char *)val, "%x", &offset);
#endif
	addr += offset;

	fprintf(stderr, "Length [0x0-0xFFFF]: ");
	fgets((char *)val, sizeof(val), stdin);
#ifdef ARM_GNU
	memset(_tmp, 0, MAX_STRING_LEN);
	sscanf((char *)val, "%s", _tmp);
	sscanf(_tmp, "%x", &len);
#else
	scanf((char *)val, "%x", &len);
#endif
	printf("\nReading 0x%04X-0x%04X:\n", addr, addr + len);

	while (len) {
		u32 cur_size = 16;
		if (cur_size > len)
			cur_size = len;
		fl_dev_read(type, addr, val, cur_size);
		printf("%04X ::", n);
		for (i = 0; i < cur_size; i++)
			printf(" %02X", val[i]);
		printf("\n");
		addr += cur_size;
		n += cur_size;
		len -= cur_size;
	}
}

static int write_boot2(char *file)
{
	char buffer[MAX_FILE_PATH];
	struct partition_entry *pe;
	int fd;
	u32 len;
	int error;

	image = (u *) gbuffer_1;

	if (file == NULL) {
		fprintf(stderr, ">>> Boot2 Image: ");
		gets(buffer);
	} else {
		strncpy(buffer, file, sizeof(buffer));
	}

	fd = flashprog_open(buffer, O_RDONLY | O_BINARY);
	if (fd < 0) {
		printf("Error: Cannot open %s for reading\n", buffer);
		return -1;
	}

	pe = part_get_layout_by_id(FC_COMP_BOOT2, 0);
	if (pe == NULL) {
		printf("Error: Flash component not found in config\n");
		return -1;
	}

	/* Partition entry for boot2 includes partition table and space for
	 * mfg data as well, and hence just erase standalone boot2 sector
	 * without disturbing other components.
	 */
	pe->size = FL_BOOT2_BLOCK_SIZE;
	pe->start = FL_BOOT2_START;

	lseek(fd, 0, SEEK_END);
	len = lseek(fd, 0, SEEK_CUR);
	lseek(fd, 0, SEEK_SET);
	if (len > pe->size) {
		fprintf(stderr,
			"Error: file %s doesn't fit in available flash "
			"space\n", buffer);
		close(fd);
		return -1;
	}

	if (read(fd, image, len) != len) {
		printf("Error: failed to read from file\n");
		close(fd);
		return -1;
	}

	close(fd);

	snprintf(gbuffer, sizeof(gbuffer), "Writing \"%s\" @0x%x "
			"(primary)...", pe->name, (unsigned int) pe->start);
	fprintf(stderr, "%s", gbuffer);
	error = fl_dev_erase(pe->device, pe->start,
			(pe->start + pe->size - 1));
	if (error == 0) {
		fprintf(stderr, "failed to erase flash:%d\n", error);
		return error;
	}
	error = fl_dev_write(pe->device, pe->start, (u8 *) image, len);
	printf("%s\n",
	       error == 0 ? "failed" : "done");

	return !error;
}

static void erase_complete_flash(int type)
{
	int error = 0;

	fprintf(stderr, "Erasing %s flash...",
	       (type == FL_INT) ? "primary" : "secondary");
	error = fl_dev_eraseall(type);
	printf("%s\n",
	       error == 0 ? "failed" : "done");
}

static void erase_psm()
{
	struct partition_entry *p;
	int error;

	p = part_get_layout_by_id(FC_COMP_PSM, NULL);
	if (!p) {
		fprintf(stderr, "No psm partition found\n");
	}
	fprintf(stderr, "Erasing psm partition...");
	error = fl_dev_erase(p->device, p->start,
			     (p->start + p->size - 1));
	printf("%s\n",
	       error == 0 ? "failed" : "done");
}

/** Write wireless firmware image */
static int write_wfw(char *file)
{
	char buffer[MAX_FILE_PATH];
	s32 act, flash_length;
	u32 flash_addr;
	int fd, error;
	u32 len;
	struct wlan_fw_header wf_header;
	struct partition_entry *f;

	image = (u *) gbuffer_1;

	if (file == NULL) {
		fprintf(stderr, ">>> WIFI Image: ");
		gets(buffer);
	} else {
		strncpy(buffer, file, sizeof(buffer));
	}
	fd = flashprog_open(buffer, O_RDONLY | O_BINARY);
	if (fd < 0) {
		printf("Error: Cannot open %s for reading\n", buffer);
		return -1;
	}

	f = part_get_layout_by_id(FC_COMP_WLAN_FW, 0);
	if (f == NULL) {
		printf("Error: Flash component not found in config\n");
		return -1;
	}

	flash_addr = f->start;
	flash_length = f->size;

	lseek(fd, 0, SEEK_END);
	len = lseek(fd, 0, SEEK_CUR);
	lseek(fd, 0, SEEK_SET);
	if ((len + sizeof(wf_header)) > flash_length) {
		fprintf(stderr,
			"Error: file %s doesn't fit in available flash "
			"space\n", buffer);
		close(fd);
		return -1;
	}

	snprintf(gbuffer, sizeof(gbuffer), "Writing \"%s\" @0x%x (%s).",
			f->name, flash_addr,
			(f->device == FL_INT) ? "primary" : "secondary");
	fprintf(stderr, "%s", gbuffer);

	/* Erase firmware flash segment */
	error = fl_dev_erase(f->device, flash_addr,
			     (flash_addr + flash_length - 1));
	if (error == 0) {
		fprintf(stderr, "failed to erase %d, %d\n", flash_addr,
			flash_length);
		goto out;
	}

	/* Write wireless firmware image header */
	wf_header.magic = WLAN_FW_MAGIC;
	wf_header.length = len;

	error = fl_dev_write(f->device, flash_addr,
			     (u8 *) &wf_header, sizeof(wf_header));
	if (error == 0)
		goto out;
	flash_addr += sizeof(wf_header);

	image = (u *) gbuffer_1;
	while (flash_length > 0) {
		act = read(fd, image, flash->block_size);
		if (act == 0)
			break;
		if (act < 0) {
			fprintf(stderr, "Error reading data file\n");
			error = -1;
			goto out;
		}

		error = fl_dev_write(f->device, flash_addr, (u8 *) image, act);
		if (error == 0)
			goto out;
		flash_addr += act;
		flash_length -= act;
		fprintf(stderr, ".");
	}
out:
	close(fd);
	printf("%s\n",
	       error == 0 ? "failed" : "done");
	return !error;
}

static int write_data(char *filename, struct partition_entry *f)
{
	int fd, error;
	u32 len, flash_addr, flash_length, act;

	image = (u *) gbuffer_1;

	fd = flashprog_open(filename, O_RDONLY | O_BINARY);
	if (fd < 0) {
		printf("Error: Cannot open %s for reading\n", filename);
		return -1;
	}

	lseek(fd, 0, SEEK_END);
	len = lseek(fd, 0, SEEK_CUR);
	lseek(fd, 0, SEEK_SET);
	if (len > f->size) {
		fprintf(stderr,
			"Error: file %s doesn't fit in available flash "
			"space\n", filename);
		close(fd);
		return -1;
	}

	flash_addr = f->start;
	flash_length = f->size;

	snprintf(gbuffer, sizeof(gbuffer), "Writing \"%s\" @0x%x (%s).",
			f->name, flash_addr,
			(f->device == FL_INT) ? "primary" : "secondary");
	fprintf(stderr, "%s", gbuffer);
	error = fl_dev_erase(f->device, flash_addr,
			     (flash_addr + flash_length - 1));
	if (error == 0) {
		fprintf(stderr, "failed to erase flash, %d\n", error);
		return !error;
	}

	image = (u *) gbuffer_1;
	while (flash_length > 0) {
		act = read(fd, image, flash->block_size);
		if (act == 0)
			break;
		if (act < 0) {
			fprintf(stderr, "Error reading data file\n");
			error = -1;
			goto out;
		}
		error = fl_dev_write(f->device, flash_addr, (u8 *) image, act);
		if (error == 0)
			goto out;
		flash_addr += act;
		flash_length -= act;
		fprintf(stderr, ".");
	}
out:
	close(fd);
	printf("%s\n",
	       error == 0 ? "failed" : "done");

	return !error;
}

static void advance_tool(void)
{
	u8 select[10];
	do {
		printf("************* Flash Programming Advanced Mode"
		       "*************\n");
		printf("* 1.Read flash (primary flash)\n");
		printf("* 2.Read flash (secondary flash)\n");
		printf("* 3.Read flash (secondary spi flash)\n");
		printf("* 4.Erase psm partition\n");
		printf("* 5.Erase entire flash (primary flash)\n");
		printf("* 6.Erase entire flash (secondary flash)\n");
		printf("* 7.Erase entire flash (secondary spi flash)\n");
		printf("* q.Exit\n");
		printf("****************************************************"
		       "******\n");
		fprintf(stderr, "Input ==>");
		gets((char *)select);

		switch (select[0]) {
		case '1':
			dump_block(FL_INT);
			break;
		case '2':
			dump_block(FL_EXT);
			break;
		case '3':
			dump_block(FL_SPI_EXT);
			break;
		case '4':
			erase_psm();
			break;
		case '5':
			erase_complete_flash(FL_INT);
			break;
		case '6':
			erase_complete_flash(FL_EXT);
			break;
		case '7':
			erase_complete_flash(FL_SPI_EXT);
			break;
		case 'q':
			printf("@@@ Exit Advanced Mode...\n");
			return;
		}
	} while (1);
}

#define MAX_FTFS_INDICES  6
static int get_ftfs_name(char *name, int len)
{
	short index = 0;
	short next_free = 0;
	int selection;
	/* Maximum 6 unique FTFS names are supported for now */
	char *ftfs_names[MAX_FTFS_INDICES];
	struct partition_entry *f1;

	while (1) {
		int i, found = 0;
		f1 = part_get_layout_by_id(FC_COMP_FTFS, &index);
		if (!f1)
			break;

		for (i = 0; i < next_free; i++) {
			if (strcmp(f1->name, ftfs_names[i]) == 0)
				found = 1;
		}

		if (!found) {
			ftfs_names[i] = f1->name;
			next_free = i + 1;
			if (next_free == MAX_FTFS_INDICES)
				/* Our buffer is full, ignore the rest */
				break;
		}
	}

	if (next_free == 0) {
		/* No ftfs component in the flash */
		return -1;
	}

	if (next_free > 1) {
		/* We have more than 1 names in the buffer, so the user has to
		 * choose the right name */
		int i;
		fprintf(stderr, ">>> Choose ftfs:\n");
		for (i = 0; i < next_free; i++) {
			fprintf(stderr, "%d. %s\n", i + 1, ftfs_names[i]);
		}
		fprintf(stderr, ">>> Choose ftfs: ");
		gets(name);
		selection = atoi(name);
		if (selection >= 0 && selection <= next_free) {
			strncpy(name, ftfs_names[selection - 1], len);
		} else {
			fprintf(stderr, "Invalid file system option\n");
			return -1;
		}

	} else {
		/* There's only one name in the list */
		strncpy(name, ftfs_names[0], len);
	}
	return 0;
}
/*
 * Description	: Update the File System(FTFS)
 * Input	: void
 * Output	: 0  => ok
 * 		: -1 => Fail
 */
static int write_ftfs(char *file, char *name)
{
	char buffer[MAX_FILE_PATH];
	int fd, error;
	u32 len, flash_addr, flash_length, act;
	struct partition_entry *f1, *f2, *f;
	short index;
	uint32_t act_gen_level;

	image = (u *) gbuffer_1;

	if (name == NULL) {
		/* Ask the user for the name */
		if (get_ftfs_name(buffer, sizeof(buffer)) != 0)
			return -1;
	} else {
		strncpy(buffer, name, sizeof(buffer));
	}

	index = 0;
	f1 = part_get_layout_by_name(buffer, &index);
	f2 = part_get_layout_by_name(buffer, &index);
	/*  Lets bother about primary component only */
	if (f1 == NULL) {
		printf("Error: Flash component not found in config\n");
		return -1;
	}

	if (f1 != NULL && f2 != NULL) {
		f = (f1->gen_level > f2->gen_level) ? f2 : f1;
		act_gen_level = (f1->gen_level > f2->gen_level) ?
			f1->gen_level + 1 : f2->gen_level + 1;
	} else {
		f = f1;
		act_gen_level = 1;
	}

	if (file == NULL) {
		fprintf(stderr, ">>> FTFS Image: ");
		gets(buffer);
	} else {
		strncpy(buffer, file, sizeof(buffer));
	}

	fd = flashprog_open(buffer, O_RDONLY | O_BINARY);
	if (fd < 0) {
		printf("Error: Cannot open %s for reading\n", buffer);
		return -1;
	}

	lseek(fd, 0, SEEK_END);
	len = lseek(fd, 0, SEEK_CUR);
	lseek(fd, 0, SEEK_SET);
	if (len > f->size) {
		fprintf(stderr,
			"Error: file %s doesn't fit in available flash "
			"space\n", buffer);
		close(fd);
		return -1;
	}

	flash_addr = f->start;
	flash_length = f->size;

	/* Make current partition active with higher generation level */
	f->gen_level = act_gen_level;

	snprintf(gbuffer, sizeof(gbuffer), "Writing \"%s\" @0x%x (%s).",
		f->name, (unsigned int)f->start,
		(f->device == FL_INT) ? "primary" : "secondary");
	fprintf(stderr, "%s", gbuffer);

	error = fl_dev_erase(f->device, f->start,
			     (f->start + f->size - 1));
	if (error == 0) {
		fprintf(stderr, "failed to erase flash, %d\n", error);
		return error;
	}

	image = (u *) gbuffer_1;
	while (flash_length > 0) {
		act = read(fd, image, flash->block_size);
		if (act == 0)
			break;
		if (act < 0) {
			fprintf(stderr, "Error reading data file\n");
			error = -1;
			goto out;
		}
		error =
		    fl_dev_write(f->device, flash_addr, (u8 *) image, act);
		if (error == 0)
			goto out;
		flash_addr += act;
		flash_length -= act;
		fprintf(stderr, ".");
	}

	/* See if partition table needs update */
	if (act_gen_level != 1)
		error = part_update(g_part_table, &g_flash_table,
							&g_flash_parts[0]);
out:
	close(fd);
	printf("%s\n",
	       error == 0 ? "failed" : "done");

	return !error;
}

static int update_firmware(char *file)
{
	char buffer[MAX_FILE_PATH];
	s32 act, flash_length, filesize;
	int fd;
	u32 flash_addr, flash_dev;
	struct partition_entry *f1, *f2, *f;
	int error, ret, hdr_check = 0;
	uint32_t act_gen_level;
	u32 *verify_buf = NULL;

	/*
	 * Blocks for firmware image 1
	 * Blocks for firmware image 2
	 */
	ret = 0;
	f1 = part_get_layout_by_id(FC_COMP_FW, (short *)&ret);
	f2 = part_get_layout_by_id(FC_COMP_FW, (short *)&ret);
	/* Lets bother about primary component only */
	if (f1 == NULL) {
		printf("Error: Flash component not found in config\n");
		return -1;
	}

	if (f1 != NULL && f2 != NULL) {
		f = (f1->gen_level > f2->gen_level) ? f2 : f1;
		act_gen_level = (f1->gen_level > f2->gen_level) ?
			f1->gen_level + 1 : f2->gen_level + 1;
	} else {
		f = f1;
		act_gen_level = 1;
	}

	flash_addr = f->start;
	flash_dev = f->device;
	flash_length = f->size;

	if (file == NULL) {
		fprintf(stderr, ">>> File to flash: ");
		gets(buffer);
	} else {
		strncpy(buffer, file, sizeof(buffer));
	}
	fd = flashprog_open(buffer, O_RDONLY | O_BINARY);
	if (fd < 0) {
		printf("Error: Cannot open %s for reading\n", buffer);
		return -1;
	}

	lseek(fd, 0, SEEK_END);
	filesize = lseek(fd, 0, SEEK_CUR);
	if (filesize <= 0 || filesize > flash_length ||
	    flash_addr + filesize > flash->chip_size) {
		fprintf(stderr,
			"Error: file %s doesn't fit in available flash "
			"space\n", buffer);
		close(fd);
		return -1;
	}
	lseek(fd, 0, SEEK_SET);

	/* Make current partition active with higher generation level */
	f->gen_level = act_gen_level;

	snprintf(gbuffer, sizeof(gbuffer), "Writing \"%s\" @0x%x (%s).",
			f->name, flash_addr,
			(f->device == FL_INT) ? "primary" : "secondary");
	fprintf(stderr, "%s", gbuffer);

	/* Erase firmware flash segment */
	error = fl_dev_erase(flash_dev, flash_addr,
			     (flash_addr + flash_length - 1));
	if (error == 0) {
		fprintf(stderr, " >>> failed to erase %d, %d\n", flash_addr,
			flash_length);
		close(fd);
		return -1;
	}

	verify_buf = (u32 *) malloc(flash->block_size);
	if (verify_buf == NULL) {
		printf("Err: malloc operation failed for verification buf\n");
		close(fd);
		return -1;
	}

	image = (u *) gbuffer_1;
	while (flash_length > 0) {
		act = read(fd, image, flash->block_size);
		if (act == 0)
			break;
		if (act < 0) {
			fprintf(stderr, " >>> Error reading data file >>> ");
			error = 0;
			goto out;
		}
		if (!hdr_check) {
			ret = verify_firmware_header(image, act);
			if (ret != 0) {
				printf(" >>> Firmware file \"%s\" is invalid"
					"  >>> ", buffer);
				error = 0;
				goto out;
			}
			hdr_check = 1;
		}
		error = fl_dev_write(flash_dev, flash_addr, (u8 *) image, act);
		if (error == 0) {
			printf(" >>> Write firmware to flash failed >>> ");
			goto out;
		}

		/* Verification Process */
		error = fl_dev_read(flash_dev, flash_addr, (u8 *)verify_buf,
				act);
		if (error == 0) {
			printf(">>> Read from flash for verification failed"
				" >>> ");
			goto out;
		}
		if (memcmp(image, verify_buf, act) != 0) {
			printf(" >>> Firmware verification failed >>> ");
			error = 0;
			goto out;
		}

		flash_addr += act;
		flash_length -= act;
		fprintf(stderr, ".");
	}

	/* See if partition table needs update */
	if (act_gen_level != 1)
		error = part_update(g_part_table, &g_flash_table,
							&g_flash_parts[0]);
out:
	free(verify_buf);
	close(fd);
	printf("%s\n",
	       error == 0 ? "failed" : "done");
	return !error;
}

#define BATCH_CONFIG "flashprog.config"

static int read_flash_contents()
{
	char component[MAX_COMP_SIZE];
	int error = -1, success = 1, ret;
	char partition[BUF_SIZE];
	char block_no[BUF_SIZE], offset_val[BUF_SIZE], len[BUF_SIZE];
	unsigned int block;
	u32 offset, length, addr;
	FILE *fout = NULL;
	uint8_t val[16];
	int n = 0, type = 0;
	ret = sscanf(gbuffer, "%"S(MAX_COMP_SIZE)"s" "%"S(BUF_SIZE)"s"
		     "%"S(BUF_SIZE)"s" "%"S(BUF_SIZE)"s" "%"S(BUF_SIZE)"s",
		     component, partition, block_no, offset_val, len);
	if (ret != 5) {
		printf("Invalid parameter. Please check --help for more "
		       "information\n");
		return error;
	}
	ret = sscanf(block_no, "%d", &block);
	if (ret != 1) {
		printf("Invalid parameter. Please check --help for more "
		       "information\n");
		return error;
	}

	ret = sscanf(offset_val, "%X", &offset);
	if (ret != 1) {
		printf("Invalid parameter. Please check --help for more "
		       "information\n");
		return error;
	}

	ret = sscanf(len, "%X", &length);
	if (ret != 1) {
		printf("Invalid parameter. Please check --help for more "
		       "information\n");
		return error;
	}

	if (!strcmp(partition, "int")) {
		fout = fopen("flash_int.bin", "w");
		type = 0;
	} else if (!strcmp(partition, "ext")) {
		type = 1;
		fout = fopen("flash_ext.bin", "w");
	}
	if (!fout) {
		printf("\nUnable to dump flash content in file\n");
		return error;
	} else {
		if (type) {
			printf("\nWriting flash content to file "
			       "flash_ext.bin\n");
		} else {
			printf("\nWriting flash content to file "
			       "flash_int.bin\n");
		}
	}

	addr = block * flash->block_size;
	addr += offset;
	while (length) {
		uint32_t cur_size = 16;
		int ret = 0;
		if (cur_size > length)
			cur_size = length;
		if (!fl_dev_read(type, addr, val, cur_size))
			printf("Unable to read flash content\n");
		ret = fwrite(&val, sizeof(unsigned char), cur_size, fout);
		if (ret != cur_size) {
			printf("Uable to dump flash content in file\n");
			printf("Failed at [block no] [offset] : %d %d\n",
			       block, n);
			break;
			}
		addr += cur_size;
		n += cur_size;
		length -= cur_size;
	}
	if (fout) {
		fclose(fout);
	}
	return success;
}

static void dump_flash_contents()
{
	int i;

	if (!g_flash_table.partition_entries_no) {
		printf("Error: Invalid partition table and/or "
			"entries, please update.\n");
		return;
	}
	printf("**** Current Flash configuration:\nName: %s, JEDEC ID: "
		"0x%x\nChip Size: 0x%x, Sector Size: 0x%x\n"
		"Block Size: 0x%x, Page Size: 0x%x\n\n",
		flash->name, flash->jedec_id, flash->chip_size,
		flash->sector_size, flash->block_size, flash->page_size);

	printf("**** Current flash layout:\n"
			" %-20s\toffset - length\n", "Component");
	for (i = 0; i < g_flash_table.partition_entries_no; i++) {
		printf(" %-20s\t0x%-6x - 0x%-6x %s\n",
				g_flash_parts[i].name,
				(unsigned int)g_flash_parts[i].start,
				(unsigned int)g_flash_parts[i].size,
				g_flash_parts[i].
				device ? "(ext flash)" : "");
	}
	printf("\n");
#ifdef CONFIG_CPU_MC200
	check_fl_layout_and_warn();
#endif
	printf("\n");
}


static int flash_write(int type, u32 addr, char *file)
{
	int fd, error = -1, ret;
	u32 len, flash_addr, flash_length, act, size = flash->block_size;
	u32 *verify_buf;

	fd = flashprog_open(file, O_RDONLY | O_BINARY);
	if (fd < 0) {
		printf("Error: Cannot open %s for reading\n", file);
		return 0;
	}
	lseek(fd, 0, SEEK_END);
	len = lseek(fd, 0, SEEK_CUR);
	lseek(fd, 0, SEEK_SET);

	flash_addr = addr;
	flash_length = len;

	verify_buf = (u32 *) malloc(size);
	if (verify_buf == NULL) {
		printf("Err: malloc operation failed for verification buf\n");
		close(fd);
		return 0;
	}

	image = (u *) gbuffer_1;
	while (flash_length > 0) {
		act = read(fd, image, size);
		if (act == 0)
			break;
		if (act < 0) {
			fprintf(stderr, "Error reading data file\n");
			ret = 0;
			goto end;
		}
		error =
		    fl_dev_write(type, flash_addr, (u8 *) image, act);
		if (error == 0) {
			printf("Write to flash failed\r\n");
			ret = 0;
			goto end;
		}

		/* Verification Process */
		error = fl_dev_read(type, flash_addr, (u8 *)verify_buf,
				act);
		if (error == 0) {
			printf("Read from flash for verification failed\r\n");
			ret = 0;
			goto end;
		}
		if (memcmp(image, verify_buf, act) != 0) {
			printf("Verification failed\r\n");
			ret = 0;
			goto end;
		}
		flash_addr += act;
		flash_length -= act;
		printf(".");
	}
	ret = 1;
end:
	free(verify_buf);
	close(fd);
	return ret;
}

static int flash_read(int type, u32 addr, u32 length, char *file)
{
	FILE *fout = fopen(file, "w");
	int n = 0, ret;

	if (!fout) {
		printf("\nUnable to dump flash content in file %s\n", file);
		return 0;
	}

	u32 cur_size = flash->block_size;
	u32 total_len = length;
	while (length) {
		int ret = 0;
		if (cur_size > length)
			cur_size = length;
		if (!fl_dev_read(type, addr, (u8 *)gbuffer_1, cur_size)) {
			printf("Unable to read flash content\n");
			ret = 0;
			goto end;
		}
		ret = fwrite((u8 *)gbuffer_1, sizeof(u8), cur_size, fout);
		if (ret != cur_size) {
			printf("Uable to dump flash content in file %s\n",
				file);
			ret = 0;
			goto end;
		}
		addr += cur_size;
		n += cur_size;
		length -= cur_size;
		u32 percent = ((total_len - length) * 100) / total_len;
		printf("%d%%\r\n", percent);
	}
	ret = 1;
end:
	if (fout)
		fclose(fout);
	return ret;
}

#define MFG_CONFIG "flashprog.mfg"
#define MAX_OPERATION_SIZE	10
static int handle_mfg_mode()
{
	FILE *fp = fopen(MFG_CONFIG, "r");
	char operation[MAX_OPERATION_SIZE], file_path[MAX_FILE_PATH];
	char address_buf[BUF_SIZE], length_buf[BUF_SIZE],
		flash_id_buf[BUF_SIZE];
	int ret, ret1;
	unsigned int flash_id, start_address, length;

	if (!fp)
		return -1;
	memset(gbuffer, 0, sizeof(gbuffer));
	while (fgets(gbuffer, sizeof(gbuffer), fp) != NULL) {
		/* Skip and continue if the line begins with # or is empty */
		if (gbuffer[0] == '#' || gbuffer[0] == '\n') {
			printf("Skipping comment or empty line\n");
			continue;
		}

		if (strncmp(gbuffer, "write", 5) == 0) {
			ret1 = sscanf(gbuffer, "%"S(MAX_OPERATION_SIZE)"s"
				"%"S(BUF_SIZE)"s"
				"%"S(BUF_SIZE)"s"
				"%"S(MAX_FILE_PATH)"s", operation, flash_id_buf,
				address_buf, file_path);
		} else {
			ret1 = sscanf(gbuffer, "%"S(MAX_OPERATION_SIZE)"s"
				"%"S(BUF_SIZE)"s"
				"%"S(BUF_SIZE)"s" "%"S(BUF_SIZE)"s"
				"%"S(MAX_FILE_PATH)"s", operation, flash_id_buf,
				address_buf, length_buf, file_path);
		}

		flash_id = strtoul(flash_id_buf, NULL, 10);

		if (address_buf[0] == '0' &&
			(address_buf[1] == 'X' || address_buf[1] == 'x'))
			start_address = strtoul(address_buf, NULL, 0);
		else
			start_address = strtoul(address_buf, NULL, 10);

		if (length_buf[0] == '0' &&
			(length_buf[1] == 'X' || length_buf[1] == 'x'))
			length = strtoul(length_buf, NULL, 0);
		else
			length = strtoul(length_buf, NULL, 10);

		if (!strcmp(operation, "read")) {
			if (ret1 != 5)
				goto out;
			printf("Creating file %s\r\n", file_path);
			printf("Reading %s flash\r\n",
				(flash_id == FL_INT) ? "primary" : "secondary");
			printf("Start Address : 0x%x\r\n", start_address);
			printf("Length : 0x%x\r\n", length);
			printf("Please wait.........\r\n");
			ret = flash_read(flash_id, start_address, length,
					file_path);
			printf("........ %s\n", ret == 0 ? "Failed" : "Done");
		} else if (!strcmp(operation, "write")) {
			if (ret1 != 4)
				goto out;
			fprintf(stderr, "Writing file %s to %s flash.. Start "
				"address: 0x%x...", file_path,
				(flash_id == FL_INT) ? "primary" : "secondary",
				start_address);
			ret = flash_write(flash_id, start_address, file_path);
			printf("%s\n", ret == 0 ? "failed" : "done");
		} else if (!strcmp(operation, "erase")) {
			if (ret1 != 4)
				goto out;
			uint32_t end_address = start_address + length - 1;
			fprintf(stderr, "Erasing %s flash.. Start "
				"address: 0x%x Length: 0x%x...",
				(flash_id == FL_INT) ? "primary" : "secondary",
				start_address, length);
			ret = fl_dev_erase(flash_id, start_address,
					end_address);
			printf("%s\n", ret == 0 ? "failed" : "done");
		} else if (!strcmp(operation, "erase-all")) {
			if (ret1 != 2)
				goto out;
			fprintf(stderr, "Erasing %s flash...",
				(flash_id == FL_INT) ? "primary" : "secondary");
			ret  = fl_dev_eraseall(flash_id);
			printf("%s\n", ret == 0 ? "failed" : "done");
		}
	}
	fclose(fp);
	return 0;
out:
	printf("Invalid parameter. Please check --help "
		"for more information\n");
	return -1;

}

static int batch_process()
{
	FILE *fp;
	char component[MAX_COMP_SIZE], file_path[MAX_FILE_PATH];
	int error = -1;
	struct partition_entry *f;
	short index = 0;
	fp = fopen(BATCH_CONFIG, "r");
	if (!fp)
		return -1;

	memset(gbuffer, 0, sizeof(gbuffer));
	while (fgets(gbuffer, sizeof(gbuffer), fp) != NULL) {
		sscanf(gbuffer, "%"S(MAX_COMP_SIZE)"s" "%"S(MAX_FILE_PATH)"s"
				, component, file_path);
		/* Debug mode, dump flash contents, partition tables etc. */
		if (!strcmp(component, "debug")
				&& !strcmp(file_path, "flash")) {
			dump_flash_contents();
			return 0;
		}

		/* Skip and continue if the line begins with # or is empty */
		if (gbuffer[0] == '#' || gbuffer[0] == '\n') {
			printf("Skipping comment or empty line\n");
			continue;
		}
		index = 0;
		f = part_get_layout_by_name(component, &index);
		if (f == NULL) {
			printf(">>> Error: No component '%s' "
			       "in layout file\n", component);
			error = -1;
			break;
		}
		switch (f->type) {
		case FC_COMP_BOOT2:
			error = write_boot2(file_path);
			break;
		case FC_COMP_FW:
			error = update_firmware(file_path);
			break;
		case FC_COMP_FTFS:
			error = write_ftfs(file_path, component);
			break;
		case FC_COMP_WLAN_FW:
			error = write_wfw(file_path);
			break;
		case FC_COMP_USER_APP:
			error = write_data(file_path, f);
			break;
		default:
			printf("Invalid component in config %s\n",
			component);
			break;
		}
		if (error != 0)
			break;
		memset(gbuffer, 0, sizeof(gbuffer));
	}
	fclose(fp);
	return error;
}

int update_partition_table()
{
	FILE *fp;
	int i, ret = -1, parts_no = 0;
	char buffer[MAX_FILE_PATH], comp[MAX_COMP_SIZE];
	struct partition_entry *fl = &g_flash_parts[0];
	struct partition_table ptemp;
	bool ext_flag = 0, ext_spi_flag = 0;

	memset(&g_flash_table, 0, sizeof(g_flash_table));
	memset(&ptemp, 0, sizeof(ptemp));
	memset(&g_flash_parts[0], 0, sizeof(g_flash_parts));

	strncpy(buffer, "flashprog.layout", sizeof(buffer));
	fp = fopen(buffer, "r");
	if (!fp) {
		ret = fl_read_part_table(FL_PART_DEV, FL_PART1_START,
				&g_flash_table);
		if (ret != 0) {
			printf("Error: Partition table 0 @0x%x"
					" is corrupted\n", FL_PART1_START);
			g_flash_table.gen_level = 0;
			g_flash_table.partition_entries_no = 0;
		}
		ret = fl_read_part_table(FL_PART_DEV, FL_PART2_START,
				&ptemp);
		if (ret != 0) {
			printf("Error: Partition table 1 @0x%x"
					" is corrupted\n", FL_PART2_START);
			ptemp.gen_level = 0;
			ptemp.partition_entries_no = 0;
		}

		/* Select active partition based on generation level */
		g_part_table = (ptemp.gen_level > g_flash_table.gen_level)
			? FL_PART2_START : FL_PART1_START;
		g_flash_table = (ptemp.gen_level > g_flash_table.gen_level)
			? ptemp : g_flash_table;

		ret = fl_read_part_entries(FL_PART_DEV, g_part_table);
		if (ret != 0) {
			printf("Error: Invalid partition table and/or "
					"entries, please update.\n");
			/* Invalidate no. of entries */
			g_flash_table.partition_entries_no = 0;
		}
		return ret;
	}

	i = 0;
	memset(gbuffer, 0, sizeof(gbuffer));
	while (fgets(gbuffer, sizeof(gbuffer), fp) != NULL && i < MAX_FL_COMP) {
		/* Skip and continue if the line begins with # or is empty */
		if (gbuffer[0] == '#' || gbuffer[0] == '\n')
			/*
			 * There is autogeneration header in layout file and
			 * hence printing here some log message will increase
			 * overall time.
			 */
			continue;
#ifndef ARM_GNU
		sscanf(gbuffer,
			"%"S(MAX_COMP_SIZE)"s""\t%x\t%x\t%d\t""%"S(MAX_NAME)"s"
			, comp,
			(unsigned int *)&fl[i].start,
			(unsigned int *)&fl[i].size,
			(unsigned int *)&fl[i].device,
			fl[i].name);
#else
		/* fixme */
		/* The above code should work but
		 * with nanolib in GNU tool chain if sscanf finds 0x0 in start
		 * of string it generates garbage for rest of the tokens being
		 * scanned.
		 */
#define MAX_STRING 10
		char _start_addr[MAX_STRING];
		sscanf(gbuffer,
			"%"S(MAX_COMP_SIZE)"s\t%"S(MAX_STRING)"s\t%x\t%d\t"
			"%"S(MAX_NAME)"s", comp,
			_start_addr,
			(unsigned int *)&fl[i].size,
			(unsigned int *)&fl[i].device,
			fl[i].name);
		sscanf(_start_addr, "%x", (unsigned int *)&fl[i].start);
#endif  /* ARM_GNU */
		if ((ret = flash_get_comp(comp)) != -1)
			fl[i].type = ret;
		else {
			fclose(fp);
			return ret;
		}

		if (fl[i].device == FL_EXT)
			ext_flag = 1;

		if (fl[i].device == FL_SPI_EXT)
			ext_spi_flag = 1;

		/* Default generation level for all partitions */
		fl[i].gen_level = 1;

		parts_no++;
		i++;
		memset(gbuffer, 0, sizeof(gbuffer));
	}

	fclose(fp);

	if (i > MAX_FL_COMP)
		printf("Only %d partition entries are supported, "
				"Truncating...\n",
				MAX_FL_COMP);

	g_flash_table.magic = PARTITION_TABLE_MAGIC;
	g_flash_table.version = PARTITION_TABLE_VERSION;
	g_flash_table.partition_entries_no = parts_no;
	g_flash_table.gen_level = 0;

	/* For first time active partition table is first partition */
	g_part_table = FL_PART1_START;

	/* Validate flash layout */
	ret = validate_flash_layout(&g_flash_parts[0], parts_no);
	if (ret == -1) {
		printf("Error: Flash layout verification failed\n");
		return ret;
	}

	/* Erase primary flash */
	erase_complete_flash(FL_INT);
	/* See if secondary flash erase is required */
	if (ext_flag)
		erase_complete_flash(FL_EXT);
	/* See if secondary spi flash erase is required */
	if (ext_spi_flag)
		erase_complete_flash(FL_SPI_EXT);

	/* Now update partition table */
	fprintf(stderr, "Writing new flash layout...");

	/* Update both partition tables */
	ret = part_update(FL_PART1_START, &g_flash_table, &g_flash_parts[0]);
	ret |= part_update(FL_PART2_START, &g_flash_table, &g_flash_parts[0]);
	if (ret == 0) {
		printf("failed\n");
		return -1;
	}

	printf("done\n");
	/* Avoid going to interactive mode, we have written new layout to
	 * flash */
	return 1;
}

int main(int argc, char *argv[])
{
	u8 select[10];
	int i, ret;

#if defined(CONFIG_CPU_MC200)
	/* Initialize flash power domain */
	PMU_PowerOnVDDIO(PMU_VDDIO_FL);
	/* Initialize secondary flash gpio power domain */
	PMU_PowerOnVDDIO(PMU_VDDIO_D2);
#elif defined(CONFIG_CPU_MW300)
	PMU_PowerOnVDDIO(PMU_VDDIO_2);
	GPIO_PinMuxFun(GPIO_28, GPIO28_QSPI_SSn);
	GPIO_PinMuxFun(GPIO_29, GPIO29_QSPI_CLK);
	GPIO_PinMuxFun(GPIO_30, GPIO30_QSPI_D0);
	GPIO_PinMuxFun(GPIO_31, GPIO31_QSPI_D1);
	GPIO_PinMuxFun(GPIO_32, GPIO32_QSPI_D2);
	GPIO_PinMuxFun(GPIO_33, GPIO33_QSPI_D3);
#endif
	/*
	 * It is observed that clock remains at 200MHz if application has
	 * initialized it even after reset-halt. And hence following clock
	 * divider mechanism will be needed, without relying on application.
	 * Max QSPI0/1 Clock Frequency is 50MHz.
	 *
	 * We do this by default.
	 */
#if defined(CONFIG_CPU_MW300)
	if (CLK_GetSystemClk() > 50000000)
#endif
		CLK_ModuleClkDivider(CLK_QSPI0, 4);
#if defined(CONFIG_CPU_MC200)
	QSPI0_Init_CLK();
#elif defined(CONFIG_CPU_MW300)
	QSPI->CONF.BF.CLK_PRESCALE = 0;
	FLASH_ResetFastReadQuad();
#endif

#ifdef CONFIG_SPI_FLASH_DRIVER
	spi_flash_init();
#endif

#ifdef CONFIG_XFLASH_DRIVER
	CLK_ModuleClkDivider(CLK_QSPI1, 4);
	QSPI1_Init_CLK();

	/* Initialize secondary QSPI gpios */
	GPIO_PinMuxFun(GPIO_72, GPIO72_QSPI1_SSn);
	GPIO_PinMuxFun(GPIO_73, GPIO73_QSPI1_CLK);
	GPIO_PinMuxFun(GPIO_76, GPIO76_QSPI1_D0);
	GPIO_PinMuxFun(GPIO_77, GPIO77_QSPI1_D1);
	GPIO_PinMuxFun(GPIO_78, GPIO78_QSPI1_D2);
	GPIO_PinMuxFun(GPIO_79, GPIO79_QSPI1_D3);

	XFLASH_PowerDown(DISABLE);
#endif

#if defined(CONFIG_CPU_MC200)
	flash = &flash_config;
#elif defined(CONFIG_CPU_MW300)
	/* Read the JEDEC id of the primary flash that is connected */
	uint32_t jedec_id = FLASH_GetJEDECID();

	/* Set the flash configuration as per the JEDEC id */
	ret = FLASH_SetConfig(jedec_id);
	/* In case of an error, print error message and continue */
	if (ret != SUCCESS)
		printf("Flash JEDEC ID %x not present in supported flash "
			"list, using default config for W25Q32BV", jedec_id);
	flash = FLASH_GetConfig();
#endif
	/* 64KiB malloc buffer */
	gbuffer_1 = (u32 *) malloc(flash->block_size);
	if (gbuffer_1 == NULL) {
		printf("Err: malloc operation failed\n");
		goto end;
	}

	printf("\nFlashprog version: %d.%d.%d",
		VERSION_BYTE_1, VERSION_BYTE_2, VERSION_BYTE_3);

	printf("\n");
	/* return -1: error */
	if (handle_mfg_mode() != -1) {
		goto end;
	}

	/* See whether new partition table is requested, else read older
	 *
	 * return 0: read flash layout from flash
	 * retrun 1: new partition table updated
	 * return -1: error, exit
	 * */
	ret = update_partition_table();
	if (ret == -1)
		goto end;

	/* Check whether batch processing is required */
	if (ret == 1) {
		/* Skip return check as we want to exit anyway */
		batch_process();
		goto end;
	} else {
		if (!batch_process())
			goto end;
	}

	do {
		printf("\nSemihosted Flash programming utility to read/write/"
		       "erase\nprimary and/or secondary flash using flash"
		       " layout stored\nin partition table.\n\n");

		printf("** Following flash layout will be used:\n"
		       " %-20s\toffset - length\n", "Component");
		for (i = 0; i < g_flash_table.partition_entries_no; i++) {
			printf(" %-20s\t0x%-6x - 0x%-6x %s\n",
			       g_flash_parts[i].name,
			       (unsigned int)g_flash_parts[i].start,
			       (unsigned int)g_flash_parts[i].size,
			       g_flash_parts[i].
			       device ? "(ext flash)" : "");
		}
		printf("****************************************************"
		       "******\n");
		printf("* 1.Write boot2 image (with bootrom header)\n");
		printf("* 2.Write firmware image (%s)\n",
				flash_get_fc_name(FC_COMP_FW));
		printf("* 3.Write file system image/s\n");
		printf("* 4.Write wireless firmware image (%s)\n",
				flash_get_fc_name(FC_COMP_WLAN_FW));
		printf("* 5.Partition information\n");
		printf("* 6.Advanced Mode\n");
		printf("* q.Exit\n");
		printf("****************************************************"
		       "******\n");
		fprintf(stderr, "Input ==>");
		gets((char *)select);
		switch (select[0]) {
		case '1':
			write_boot2(NULL);
			break;
		case '2':
			update_firmware(NULL);
			break;
		case '3':
			write_ftfs(NULL, NULL);
			break;
		case '4':
			write_wfw(NULL);
			break;
		case '5':
			partition_info();
			break;
		case '6':
			advance_tool();
			break;
		case 'q':
			printf("@@@ Program exit...\n\n");
			printf("Please press CTRL+C to exit.\n\n");
			break;
		};
	} while (select[0] != 'q' && select[0] != 'Q');

	return 0;

end:
	printf("Please press CTRL+C to exit.\n\n");
	return 0;
}
