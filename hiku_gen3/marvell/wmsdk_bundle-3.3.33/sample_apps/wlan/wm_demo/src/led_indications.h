/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef _LED_INDICATIONS_H_
#define _LED_INDICATIONS_H_

/* LED indications to indicate device is in provisioning state */
void indicate_provisioning_state();

/* LED indications to indicate device is in connecting state */
void indicate_normal_connecting_state();

/* LED indications to indicate device is in connected state */
void indicate_normal_connected_state();

#endif /* ! _LED_INDICATIONS_H_ */
