/*  Copyright 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <ctype.h>
#include <wm_os.h>
#include <wmstdio.h>
#include <psm-v2.h>

static int dumped_object_count;
static psm_hnd_t psm_hnd;

static int psm_util_dump_cb(const uint8_t *name)
{
	const int READ_BUFFER_SIZE = 80;
	uint8_t value[READ_BUFFER_SIZE];


	psm_object_handle_t ohandle;
	int obj_size = psm_object_open(psm_hnd, (char *)name, PSM_MODE_READ,
				       sizeof(value) - 1, NULL, &ohandle);
	if (obj_size < 0) {
		wmprintf("<invalid length of data: %d>\r\n", obj_size);
		return 1;
	}

	int read_size = obj_size <= READ_BUFFER_SIZE ? obj_size
		: READ_BUFFER_SIZE;

	if (read_size) {
		int rv = psm_object_read(ohandle, value, read_size);
		if (rv != read_size) {
			wmprintf("Could not read requested bytes: %d>\r\n", rv);
			psm_object_close(&ohandle);
			return 1;
		}
	}

	/* Print prefix */
	if (is_psm_encrypted(psm_hnd))
			wmprintf("(enc) ");

	/* Print the name */
	wmprintf("%s = ", name);

	bool binary_data = false;
	int index;
	for (index = 0; index < read_size; index++) {
		if (!isprint(value[index])) {
			wmprintf("(hex) ");
			binary_data = true;
			break;
		}
	}

	for (index = 0; index < read_size;) {
		if (binary_data) {
			wmprintf("%x", value[index++]);
			if (!(index % 8) && (index != read_size))
				wmprintf("\r\n");
		} else {
			wmprintf("%c", value[index++]);
		}
	}

	if (read_size < obj_size)
		wmprintf("..(contd) (%d bytes)\r\n", obj_size);
	else
		wmprintf(" (%d bytes)\r\n", obj_size);

	dumped_object_count++;

	psm_object_close(&ohandle);
	return WM_SUCCESS;
}

void psm_util_dump(psm_hnd_t hnd)
{
	if (!hnd)
		return;

	if (is_psm_encrypted(hnd))
		wmprintf("Note: Entries prepended with \"(enc)\" "
			 "are encrypted\n\r");

	psm_hnd = hnd;
	dumped_object_count = 0;
	psm_objects_list(hnd, psm_util_dump_cb);

	if (!dumped_object_count)
		wmprintf("No objects found\r\n");
	else
		wmprintf("\r\n%d total objects.\r\n", dumped_object_count);
}
