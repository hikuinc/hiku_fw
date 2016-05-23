/*
 * Copyright 2010-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

/**
 * @file timer.c
 * @brief Linux implementation of the timer interface.
 */

#include <stddef.h>
#include <wm_os.h>
#include <timer_interface.h>

char expired(Timer* timer) {
	if (left_ms(timer) > 0)
		return 0;
	else
		return 1;
}

void countdown_ms(Timer* timer, unsigned int timeout) {
	timer->timeout = timeout;
	timer->start_timestamp = os_get_timestamp();
}

void countdown(Timer* timer, unsigned int timeout) {
	/* converting timeout in milliseconds */
	timer->timeout = timeout * 1000;
	timer->start_timestamp = os_get_timestamp();
}

int left_ms(Timer* timer) {
	uint32_t current_timestamp = os_get_timestamp();
	int time_diff_ms = (int)((current_timestamp - timer->start_timestamp)/1000);
	return (timer->timeout - time_diff_ms);  
}

void InitTimer(Timer* timer) {
	// do nothing
}
