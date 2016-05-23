
#ifndef _SRP_UTIL_H_
#define _SRP_UTIL_H_

#include <wmstdio.h>
#include <wmlog.h>

//#define CONFIG_SRP_DEBUG

#ifdef CONFIG_SRP_DEBUG
#define srp_d(...)				\
	wmlog("supplicant", ##__VA_ARGS__)
#else
#define srp_d(...)

#endif /* CONFIG_SRP_DEBUG */

/** ENTER print */
#define ENTER()         srp_d("Enter: %s, %s:%i", __FUNCTION__, \
                            __FILE__, __LINE__)

/** LEAVE print */
#define LEAVE()         srp_d("Leave: %s, %s:%i", __FUNCTION__, \
                            __FILE__, __LINE__)

#endif /* _SRP_UTIL_H_ */
