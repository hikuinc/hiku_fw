#include <wm_os.h>
#include <wm_utils.h>
#include <stdlib.h>

static os_thread_stack_define(test_thread_stack, 512);

#define THREAD_COUNT 10
static os_thread_t test_thread_handle[THREAD_COUNT];


static int test_count;
static os_mutex_t recursive_mutex;

static int random_sleep_time()
{
	int random = rand() % 128;
	return os_msec_to_ticks(((int)random));
}

static void test_thread(void *arg)
{
	int thread_index = (int) arg;
	/* wmprintf("Rec. mutex thread %d\r\n", thread_index); */
	os_thread_sleep(random_sleep_time());
	os_recursive_mutex_get(&recursive_mutex, OS_WAIT_FOREVER);
	/* wmprintf("%d: Increase count\r\n", thread_index); */
	test_count += 2;
	os_thread_sleep(random_sleep_time());
	os_recursive_mutex_get(&recursive_mutex, OS_WAIT_FOREVER);
	/* wmprintf("%d: Decrease count\r\n", thread_index); */
	test_count--;
	os_thread_sleep(random_sleep_time());
	os_recursive_mutex_put(&recursive_mutex);
	/* wmprintf("%d: Decrease count to zero\r\n", thread_index); */
	test_count--;
	os_thread_sleep(random_sleep_time());
	os_recursive_mutex_put(&recursive_mutex);

	test_thread_handle[thread_index] = NULL;
	os_thread_delete(NULL);
}

static int get_random_thread_priority()
{
	return rand() % OS_PRIO_0;
}

int mutex_demo()
{
	int rv = os_recursive_mutex_create(&recursive_mutex, "rec-m-tst");
	if (rv != WM_SUCCESS) {
		wmprintf("%s: mutex creation failed\r\n", __func__);
		return rv;
	}

	char name_buf[20];
	int index;
	for (index = 0; index < THREAD_COUNT; index++) {
		snprintf(name_buf, sizeof(name_buf), "rec-m-%d", index);
		rv = os_thread_create(&test_thread_handle[index],
				      name_buf, test_thread, (void *) index,
				      &test_thread_stack,
				      get_random_thread_priority());
	}

	int seconds_to_wait = 5;
	while (seconds_to_wait--) {
		for (index = 0; index < THREAD_COUNT; index++) {
			if (test_thread_handle[index])
				break;
		}

		if (index == THREAD_COUNT)
			break;

		os_thread_sleep(os_msec_to_ticks(1000));
	}

	os_mutex_delete(&recursive_mutex);

	if (index == THREAD_COUNT) {
		if (test_count) {
			wmprintf("FAILED: Recursive mutex test.\r\n");
			return -WM_FAIL;
		} else {
			wmprintf("SUCCESS: Recursive mutex test\r\n");
			return WM_SUCCESS;
		}
	}

	wmprintf("FAILED: Recursive mutex test. Timeout\r\n");
	return -WM_FAIL;
}
