/*  Copyright 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 *
 * All the functions defined in this file are legacy. They use newer API to
 * implement legacy API's. Use of newer API's is recommended.
 */
#include <wm_os.h>
#include "psm-internal.h"

int psm_get_handle(void);
int psm_set_variable(int phandle, const char *variable,
		     const void *value, uint32_t len);
int psm_get_variable(int phandle, const char *variable,
		     void *value, uint32_t max_len);
int psm_format(int phandle);
int psm_object_delete(int phandle, const char *name);
int psm_delete_single(const char *module, const char *variable);

/* Deprecated. Please PSM-v2 */
typedef struct psm_handle {
	/** The module name */
	char   psmh_mod_name[32];
} psm_handle_t;

#define FULL_VAR_NAME_SIZE 64

/* Deprecated. Use psm_object_open() instead */
int psm_open(psm_handle_t *handle, const char *module_name)
{
	uint32_t len = strlen(module_name);
	if (len > sizeof(handle->psmh_mod_name) - 1) {
		psm_e("Too long module name: %d\r\n", len);
		return -WM_FAIL;
	}

	strcpy(handle->psmh_mod_name, module_name);
	return WM_SUCCESS;
}

void psm_close(psm_handle_t *handle)
{
	memset(handle, 0x00, sizeof(psm_handle_t));
	/* Obsoleted */
}

int psm_set(psm_handle_t *handle, const char *variable, const char *value)
{
	char full_name[FULL_VAR_NAME_SIZE];
	if (snprintf(full_name, FULL_VAR_NAME_SIZE,
		     "%s.%s", handle->psmh_mod_name,
		     variable) > (FULL_VAR_NAME_SIZE - 1))
		return -WM_E_INVAL;

	int hnd = psm_get_handle();
	if (!hnd) {
		psm_e("PSM not init'ed yet\r\n");
		return -WM_FAIL;
	}

	uint32_t value_len = strlen(value);
	return psm_set_variable(hnd, full_name, value, value_len);
}

int psm_register_module(const char *module_name, const char *partition_key,
			short flags)
{
	/* Obsoleted */
	return WM_SUCCESS;
}

int psm_get(psm_handle_t *handle, const char *variable,
	    char *value, int value_len)
{
	char full_name[FULL_VAR_NAME_SIZE];
	if (snprintf(full_name, FULL_VAR_NAME_SIZE,
		     "%s.%s", handle->psmh_mod_name,
		     variable) > (FULL_VAR_NAME_SIZE - 1))
		return -WM_E_INVAL;

	int hnd = psm_get_handle();
	if (!hnd) {
		psm_e("PSM not init'ed yet\r\n");
		return -WM_FAIL;
	}

	int read_len = psm_get_variable(hnd, full_name,
					      value, value_len - 1);
	if (read_len >= 0) {
		value[read_len] = 0;
		return WM_SUCCESS;
	}

	return -WM_FAIL;
}

int psm_commit(psm_handle_t *handle)
{
	/* Obsolete */
	return WM_SUCCESS;
}

int psm_erase_and_init()
{
	int hnd = psm_get_handle();
	if (!hnd) {
		psm_e("PSM not init'ed yet\r\n");
		return -WM_FAIL;
	}

	return psm_format(hnd);
}

int psm_erase_partition(short partition_id)
{
	return psm_erase_and_init();
}

int psm_get_partition_id(const char *module_name)
{
	/* Stub */
	return 0;
}

int psm_get_single(const char *module, const char *variable,
		   char *value, unsigned max_len)
{
	psm_handle_t handle;
	int rv = psm_open(&handle, module);
	if (rv != WM_SUCCESS) {
		psm_e("Open failed. Unable to read variable %s. "
			 "rv: %d\r\n", variable, rv);
		return -WM_FAIL;
	}

	rv = psm_get(&handle, variable, value, max_len);
	psm_close(&handle);
	return rv;
}

int psm_set_single(const char *module, const char *variable, const char *value)
{
	psm_handle_t handle;
	int rv = psm_open(&handle, module);
	if (rv != WM_SUCCESS) {
		psm_e("Open failed. Unable to write variable %s. "
			"rv: %d\r\n", variable, rv);
		return -WM_FAIL;
	}

	rv = psm_set(&handle, variable, value);
	if (rv != WM_SUCCESS) {
		psm_e("Unable to write variable %s. Value: %s"
			"rv: %d\r\n", variable, value, rv);
	}

	psm_close(&handle);
	return rv;
}

int psm_delete(psm_handle_t *handle, const char *variable)
{
	int rv;
	char full_name[FULL_VAR_NAME_SIZE];
	if (snprintf(full_name, FULL_VAR_NAME_SIZE,
		     "%s.%s", handle->psmh_mod_name,
		     variable) > (FULL_VAR_NAME_SIZE - 1))
		return -WM_E_INVAL;

	int hnd = psm_get_handle();
	if (!hnd) {
		psm_e("PSM not init'ed yet\r\n");
		return -WM_FAIL;
	}
	rv = psm_object_delete(hnd, full_name);
	return rv;
}

int psm_delete_single(const char *module, const char *variable)
{
	int rv;
	char full_name[FULL_VAR_NAME_SIZE];
	if (snprintf(full_name, FULL_VAR_NAME_SIZE,
		     "%s.%s", module,
		     variable) > (FULL_VAR_NAME_SIZE - 1))
		return -WM_E_INVAL;

	int hnd = psm_get_handle();
	if (!hnd) {
		psm_e("PSM not init'ed yet\r\n");
		return -WM_FAIL;
	}

	rv = psm_object_delete(hnd, full_name);
	return rv;
}
