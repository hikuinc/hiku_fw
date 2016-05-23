/*  Copyright 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 *
 * This acts as a wrapper over core PSM module. It initializes the PSM
 * module to store simple name-value pairs. The details of PSM partition
 * are read from the partition table and are passed to core PSM
 * module. Apart from this important task this file provides a psm cli
 * interface to perform basic PSM operations.
 */
#include <wm_os.h>
#include <flash.h>
#include <boot_flags.h>
#include <cli.h>

#include <psm-v2.h>
#include <psm-utils.h>

/* #define CONFIG_PSM_DEBUG */

#include <wmlog.h>
#define psmw_e(...)				\
	wmlog_e("psmw", ##__VA_ARGS__)
#define psmw_w(...)				\
	wmlog_w("psmw", ##__VA_ARGS__)
#ifdef CONFIG_PSM_DEBUG
#define psmw_d(...)				\
	wmlog("psmw", ##__VA_ARGS__)
#else
#define psmw_d(...)
#endif /* ! CONFIG_PSM_DEBUG */

#define PSM_FLASH_SECTOR_SIZE 4096
/** The maximum size of the fully qualified variable name including the
 * trailing \\0. Fully qualified variable name is:
 * \<product_name\>.\<module_name\>.\<variable_name\>
 */
#define FULL_VAR_NAME_SIZE 64

psm_hnd_t psm_get_handle();

static void psm_generate_full_name(int argc, char **argv, char *full_name,
				   int full_name_len)
{
	if (argc == 3)
		snprintf(full_name, full_name_len, "%s.%s", argv[1], argv[2]);
	else if (argc == 2)
		snprintf(full_name, full_name_len, "%s", argv[1]);
	else
		snprintf(full_name, full_name_len, "unknown");
}

void psm_cli_get(int argc, char **argv)
{
	int rv;
	char value[32];
	char full_name[FULL_VAR_NAME_SIZE];

	if (argc != 2 && argc != 3) {
		wmprintf("[psm] Usage: %s <variable>\r\n", argv[0]);
		wmprintf("[psm] Error: invalid number of arguments\r\n");
		return;
	}

	psm_generate_full_name(argc, argv, full_name, sizeof(full_name));

	rv = psm_get_variable_str(psm_get_handle(), full_name,
				  value, sizeof(value));
	if (rv < 0) {
		wmprintf("Variable not found (%d)\r\n", rv);
		return;
	}

	if (rv == 0) {
		wmprintf("Zero length object");
		return;
	}

	wmprintf("%s = %s\r\n", full_name, value);
}

void psm_cli_delete(int argc, char **argv)
{
	int rv;
	char full_name[FULL_VAR_NAME_SIZE];

	if (argc != 2 && argc != 3) {
		wmprintf("[psm] Usage: %s <variable>\r\n", argv[0]);
		wmprintf("[psm] Error: invalid number of arguments\r\n");
		return;
	}

	psm_generate_full_name(argc, argv, full_name, sizeof(full_name));

	rv = psm_object_delete(psm_get_handle(), full_name);
	if (rv != 0) {
		wmprintf("Variable not found (%d)\r\n", rv);
		return;
	}
}

void psm_cli_set(int argc, char **argv)
{
	int rv;
	char full_name[FULL_VAR_NAME_SIZE];

	if (argc != 3 && argc != 4) {
		wmprintf("[psm] Usage: %s <variable> <value>\r\n", argv[0]);
		wmprintf("[psm] Error: invalid number of arguments\r\n");
		return;
	}

	psm_generate_full_name(argc - 1, argv, full_name, sizeof(full_name));

	char *value;
	if (argc == 4)
		value = argv[3];
	else if (argc == 3)
		value = argv[2];

	rv = psm_set_variable_str(psm_get_handle(), full_name, value);
	if (rv != 0) {
		wmprintf("psm_set_variable() failed with: %d\r\n", rv);
		return;
	}
}

void psm_erase(int argc, char **argv)
{
	int rv = psm_format(psm_get_handle());
	if (rv != WM_SUCCESS) {
		wmprintf("Could not erase\r\n");
		return;
	}
}

void psm_dump(int argc, char **argv)
{
	if (argc != 1) {
		wmprintf("[psm] Usage: %s\r\n", argv[0]);
		wmprintf("[psm] Error: invalid number of arguments\r\n");
		return;
	}

	psm_util_dump(psm_get_handle());
}

struct cli_command psm_commands[] = {
	{"psm-get", "<variable>", psm_cli_get},
	{"psm-set", "<variable> <value>", psm_cli_set},
	{"psm-delete", "<variable>", psm_cli_delete},
	{"psm-erase", NULL, psm_erase},
	{"psm-dump", NULL, psm_dump},
};

int psm_cli_init(void)
{
	int i;

	for (i = 0; i < sizeof(psm_commands) / sizeof(struct cli_command); i++)
		if (cli_register_command(&psm_commands[i]))
			return 1;
	return 0;
}
