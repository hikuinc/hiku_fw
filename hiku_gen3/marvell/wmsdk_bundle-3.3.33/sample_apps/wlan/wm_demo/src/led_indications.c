/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* System includes */
#include <led_indicator.h>
#include <board.h>

#include "led_indications.h"

void indicate_provisioning_state()
{
	/* Start Slow Blink */
	led_slow_blink(board_led_1());
}

void indicate_normal_connecting_state()
{
	/* Start Fast Blink */
	led_fast_blink(board_led_1());
}

void indicate_normal_connected_state()
{
	/* Start LED */
	led_on(board_led_1());
}
