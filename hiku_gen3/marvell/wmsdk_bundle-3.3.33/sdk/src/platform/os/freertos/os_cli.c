/*
 * Copyright 2008-2015, Marvell International Ltd.
 * All Rights Reserved.
 */

#include <inttypes.h>
#include <wmstdio.h>
#include <wm_os.h>

/* Freertos does no size check on this buffer and hence kept higher than
 * minimal size that would be required */
#define MAX_TASK_INFO_BUF 2048

void os_dump_threadinfo(char *name)
{
	uint8_t status;
	uint32_t stack_remaining, tcbnumber, priority, stack_start,
		stack_top;
	char tname[configMAX_TASK_NAME_LEN], *substr;
	char *task_info_buf = NULL;
	task_info_buf = (char *) os_mem_alloc(MAX_TASK_INFO_BUF);
	if (task_info_buf == NULL)
		return;
	vTaskList(task_info_buf);
	substr = strtok(task_info_buf, "\r\n");
	while (substr) {
		char *tabtab = strstr(substr, "\t\t");
		if (!tabtab) {
			wmprintf("Task list info corrupted."
					"Cannot parse\n\r");
			break;
		}
		/* NULL terminate the task name */
		*tabtab = 0;
		strncpy(tname, substr, sizeof(tname));
		/* advance to thread status */
		substr = tabtab + 2;
		sscanf((const char *)substr,
		       "%c\t%u\t%u\t%u\t%x\t%x\r\n", &status,
		       (unsigned int *)&priority,
		       (unsigned int *)&stack_remaining,
		       (unsigned int *)&tcbnumber,
		       (unsigned int *)&stack_start,
		       (unsigned int *)&stack_top);
		if (strncmp(tname, name, strlen(name)) == 0 || name == NULL) {
			wmprintf("%s:\r\n", tname);
			const char *status_str;
			switch (status) {
			case 'R':
				status_str = "ready";
				break;
			case 'B':
				status_str = "blocked";
				break;
			case 'S':
				status_str = "suspended";
				break;
			case 'D':
				status_str = "deleted";
				break;
			default:
				status_str = "unknown";
				break;
			}
			wmprintf("\tstate = %s\r\n", status_str);
			wmprintf("\tpriority = %u\r\n", priority);
			wmprintf("\tstack remaining = %d bytes\r\n",
				 stack_remaining * sizeof(portSTACK_TYPE));
			wmprintf("\ttcbnumber = %d\r\n", tcbnumber);
			wmprintf("\tstack_start: 0x%x stack_top: 0x%x\r\n",
				 stack_start, stack_top);
			if (name != NULL && task_info_buf) {
				os_mem_free(task_info_buf);
				return;
			}
		}

		substr = strtok(NULL, "\r\n");
	}

	if (name)
                /* If we got this far and didn't find the thread, say so. */
		wmprintf("ERROR: No thread named '%s'\r\n", name);
	if (task_info_buf)
		os_mem_free(task_info_buf);
}

