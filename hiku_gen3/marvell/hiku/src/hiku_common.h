/*
 * hiku_common.h
 *
 *  Created on: May 14, 2016
 *      Author: rajan-work
 */

#ifndef INC_HIKU_COMMON_H_
#define INC_HIKU_COMMON_H_

#include <wmstdio.h>
#include <cli.h>
#include <psm.h>
#include <app_framework.h>
#include <wmlog.h>


#define hiku_m(...)				\
	wmlog("hiku_main", ##__VA_ARGS__)

#define hiku_h(...)				\
	wmlog("hiku_http", ##__VA_ARGS__)

#define hiku_c(...)				\
	wmlog("hiku_connect", ##__VA_ARGS__)

#define hiku_o(...)				\
	wmlog("hiku_ota", ##__VA_ARGS__)

#define hiku_b(...)				\
	wmlog("hiku_button", ##__VA_ARGS__)

#define hiku_a(...)				\
	wmlog("hiku_aws", ##__VA_ARGS__)

#define hiku_brd(...)	\
	wmlog("hiku_board",##__VA_ARGS__)

#endif /* INC_HIKU_COMMON_H_ */
