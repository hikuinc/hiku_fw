/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */
#include <wm_os.h>
#include <wmstdio.h>
#include <wmsysinfo.h>
#include <cli.h>

struct node {
	struct node *next;
	char data[100];
};

static struct node head = {
	.next = NULL
};


static int add_node(bool add_to_head)
{
	struct node *node = os_mem_alloc(sizeof(struct node));
	if (!node)
		return -WM_FAIL;

	if (add_to_head) {
		node->next = head.next;
		head.next = node;
	} else {
		static struct node *tail = &head;
		tail->next = node;
		node->next = NULL;
		tail = node;
	}
	wmprintf("allocated node  : %p\r\n", node);

	return WM_SUCCESS;
}

static int remove_node()
{
	struct node *to_free = head.next;
	if (to_free) {
		head.next = to_free->next;
		os_mem_free(to_free);
	} else
		return -WM_FAIL;

	return WM_SUCCESS;
}

static int alloc_cnt;
static void head_alloc(int argc, char **argv)
{
	while (add_node(true) == WM_SUCCESS)
		alloc_cnt++;
	wmprintf("Allocated nodes: %d\r\n", alloc_cnt);
	wmprintf("1. Please check heap status with 'sysinfo heap'\r\n");
	wmprintf("2. Then execute 'free-all'\r\n");
}

static void free_all(int argc, char **argv)
{
	while (remove_node() == WM_SUCCESS)
		alloc_cnt--;
	wmprintf("Pending allocations: %d\r\n", alloc_cnt);
	wmprintf("1. Please check heap status with 'sysinfo heap'\r\n");
	wmprintf("2. Then execute 'head-alloc' or 'tail-alloc'\r\n");
}

static void tail_alloc(int argc, char **argv)
{
	while (add_node(false) == WM_SUCCESS)
		alloc_cnt++;
	wmprintf("Allocated nodes: %d\r\n", alloc_cnt);
	wmprintf("1. Please check heap status with 'sysinfo heap'\r\n");
	wmprintf("2. Then execute 'free-all'\r\n");
}


static struct cli_command tests[] = {
	{"head-alloc", NULL, head_alloc},
	{"free-all", NULL, free_all},
	{"tail-alloc", NULL, tail_alloc},
};

int main()
{
	wmstdio_init(UART0_ID, 0);
	cli_init();
	sysinfo_init();

	int i;
	for (i = 0; i < sizeof(tests) / sizeof(struct cli_command); i++)
		if (cli_register_command(&tests[i]))
			return -WM_FAIL;


	wmprintf("Get output of cmd 'sysinfo heap'\r\n");
	wmprintf("Then run 'head-alloc' or 'tail-alloc'\r\n");

	return 0;
}
