/*
 * hiku_board.h
 *
 *  Created on: May 18, 2016
 *      Author: rajan-work
 */

#ifndef SRC_HIKU_BOARD_H_
#define SRC_HIKU_BOARD_H_

#include "hiku_common.h"

#define STATUS_LED GPIO_40
#define BUTTON	   GPIO_24

int hiku_board_init(void);
int hiku_board_get_serial(char * buf);

#endif /* SRC_HIKU_BOARD_H_ */
