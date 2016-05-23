#ifndef __MFG_INTERNAL_H__
#define __MFG_INTERNAL_H__

#include <wmlog.h>
#define mfg_e(...)				\
	wmlog_e("mfg", ##__VA_ARGS__)
#define mfg_w(...)				\
	wmlog_w("mfg", ##__VA_ARGS__)
#ifdef CONFIG_MFG_DEBUG
#define mfg_d(...)				\
	wmlog("mfg", ##__VA_ARGS__)
#else
#define mfg_d(...)
#endif /* ! CONFIG_PSM_DEBUG */

#endif /* __MFG_INTERNAL_H__ */
