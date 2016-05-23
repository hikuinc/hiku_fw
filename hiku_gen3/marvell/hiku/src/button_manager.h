/*
 * button_manager.h
 *
 *  Created on: May 18, 2016
 *      Author: rajan-work
 */

#ifndef SRC_BUTTON_MANAGER_H_
#define SRC_BUTTON_MANAGER_H_

#include "hiku_common.h"
#include <push_button.h>

typedef enum{
	BUTTON_PRESSED = 0,
	BUTTON_RELEASED = 1
}BUTTON_STATE;

int button_manager_init(void);

#endif /* SRC_BUTTON_MANAGER_H_ */
