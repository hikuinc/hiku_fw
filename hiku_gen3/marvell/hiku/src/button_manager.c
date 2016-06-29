/*
 * button_manager.c
 *
 *  Created on: May 18, 2016
 *      Author: rajan-work
 */


#include "button_manager.h"
#include "hiku_board.h"
#include <generic_io.h>
#include <mdev_pinmux.h>
#include <mdev_gpio.h>
#include <system-work-queue.h>

/** forward declarations */

static void set_button_state_cb(int pin, void * data);
//static void reset_to_factory_cb(int pin, void * data);
//static void reset_device_cb(int pin, void* data);
static void init_button_state(void);
static void _push_button_press_cb(int pin, void *data);
static int push_button_press_cb(void * data);

/** global variable definitions */

static input_gpio_cfg_t pushbutton = {
    .gpio = BUTTON,
    .type = GPIO_ACTIVE_LOW,
};

static int button_state = BUTTON_RELEASED;

/** function implementations */

static void set_button_state_cb(int pin, void* data)
{
	// this function is the active call back for any button pushes
	gpio_drv_read(NULL, pushbutton.gpio, &button_state);
	hiku_b("set_button_state_cb <button_state=%s>",(button_state?"BUTTON_RELEASED":"BUTTON_PRESSED"));
}

/*static void reset_to_factory_cb(int pin, void* data)
{
	// this is the function that will be called when the user presses it long enough to
	// go back to factory configuration
	// we may not need this
	hiku_b("resetting to factory defaults!! \r\n");
	if(psm_erase_and_init() != WM_SUCCESS)
	{
		hiku_b("failed to erase the psm!!\r\n");
	}
	else
	{
		hiku_b("erased the psm and re-initialized!!\r\n");
	}
	reset_device_cb(pin, data);
}*/

/*static void reset_device_cb(int pin, void* data)
{
	// this is the function that will reboot the device, we may not need this as well as we
	// might do that in hardware, just use it as a callback
	hiku_b("resetting device!!\r\n");
	pm_reboot_soc();
}*/

static void _push_button_press_cb(int pin, void *data)
{
	// disable the interrupt first
	gpio_drv_set_cb(NULL, pushbutton.gpio,
			GPIO_INT_DISABLE, NULL,
			NULL);

	wq_job_t job = {
		.job_func = push_button_press_cb,
		.owner[0] = 0,
		.param = data,
		.periodic_ms = 0,
		.initial_delay_ms = 0,
	};

	wq_handle_t wq_handle;

	wq_handle = sys_work_queue_get_handle();
	if (wq_handle)
		work_enqueue(wq_handle, &job, NULL);

	// Enable the interrupt back
	gpio_drv_set_cb(NULL, pushbutton.gpio,
				GPIO_INT_BOTH_EDGES, NULL,
				_push_button_press_cb);
}

static int push_button_press_cb(void * data)
{
	// This is where you handle the push button interrupt once it gets passed through the work queue
	set_button_state_cb(pushbutton.gpio, data);
	return WM_SUCCESS;
}

static void init_button_state(void)
{
	/* Configure the pin as a GPIO and set it as an input.
	 * This is being done every time just to safeguard against cases
	 * wherein someone has changed the GPIO configuration between two
	 * push_button_set_cb() calls for the same pin.
	 */
	hiku_b("Initializing pin state to read!!\r\n");
	mdev_t *pinmux_dev, *gpio_dev;

	pinmux_drv_init();
	gpio_drv_init();

	pinmux_dev = pinmux_drv_open("MDEV_PINMUX");
	gpio_dev = gpio_drv_open("MDEV_GPIO");
	pinmux_drv_setfunc(pinmux_dev, pushbutton.gpio,
			pinmux_drv_get_gpio_func(pushbutton.gpio));
	gpio_drv_setdir(gpio_dev, pushbutton.gpio, GPIO_INPUT);
	pinmux_drv_close(pinmux_dev);

	gpio_drv_read(gpio_dev, pushbutton.gpio, &button_state);

	int ret = gpio_drv_set_cb(gpio_dev, pushbutton.gpio,
			GPIO_INT_BOTH_EDGES, gpio_dev,
			_push_button_press_cb);

	hiku_b("button_cb registration_status: %d",ret);

	gpio_drv_close(gpio_dev);

	hiku_b("button_state=%d",button_state);
}

int button_manager_init(void)
{
	hiku_b("Button Manager initializing!!\r\n");

	if (sys_work_queue_init() != WM_SUCCESS)
		return -WM_FAIL;

	init_button_state();
/*
	if( push_button_set_cb(pushbutton, set_button_state_cb, 0, 0, NULL) != WM_SUCCESS)
	{
		hiku_b("set_button_state_cb failed!!\r\n");
		return WM_FAIL;
	}

	if (push_button_set_cb(pushbutton, reset_to_factory_cb, 60000, 0, NULL) != WM_SUCCESS)
	{
		hiku_b("reset_to_factory_cb failed!!\r\n");
		return WM_FAIL;
	}
	if (push_button_set_cb(pushbutton, reset_device_cb, 20000,0, NULL) != WM_SUCCESS)
	{
		hiku_b("reset_device_cb failed!! \r\n");
		return WM_FAIL;
	}
*/
	return WM_SUCCESS;
}
