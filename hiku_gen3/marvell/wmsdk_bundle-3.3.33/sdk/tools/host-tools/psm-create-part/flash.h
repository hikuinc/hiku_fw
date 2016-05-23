/*! \file flash.h
 * \brief Flash Driver for host psm create utility
 *
 * This is a cut down copy of flash.h from the SDK. This file should be
 * included instead of the SDK one. The host implementation of the
 * flash_drv_* functions actually write to a file on the host.
 */
/*
 *  Copyright 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

#ifndef _FLASH_H_
#define _FLASH_H_

#include <string.h>
#include <wmtypes.h>

typedef void mdev_t;

/** The flash descriptor
 *
 * All components that work with flash refer to the flash descriptor. The flash
 * descriptor captures the details of the component resident in flash.
 */
typedef struct flash_desc {
	/** The flash device */
	uint8_t   fl_dev;
	/** The start address on flash */
	uint32_t  fl_start;
	/** The size on flash  */
	uint32_t  fl_size;
} flash_desc_t;

/**  Initialize the Flash Driver
 *
 * This will initialize the internal as well as external (if defined)
 * flash driver and register it with the mdev interface.
 *
 * \return void
 */
extern void flash_drv_init(void);

/**  Open the Flash Driver
 *
 * This will open the flash driver for the flash
 * device specified using fl_dev.
 *
 * \param fl_dev Flash device to be opened
 * \return mdev handle if successful, NULL otherwise
 */
extern mdev_t *flash_drv_open(int fl_dev);

/**  Close the flash driver
 *
 * This will close the flash driver
 *
 * \param  dev mdev handle to the driver
 * \return void
 */
extern int flash_drv_close(mdev_t *dev);

/**  Write data to flash
 *
 * This will write specified number of bytes to flash.
 * Note that flash needs to be erased before writing.
 *
 * \param dev mdev handle to the driver
 * \param buf Pointer to the data to be written
 * \param len Length of data to be written
 * \param addr Write address in flash
 * \return 0 on success
 */
extern int flash_drv_write(mdev_t *dev, uint8_t *buf,
			   uint32_t len, uint32_t addr);

/**  Read data from flash
 *
 * This will read specified number of bytes from flash.
 *
 * \param dev mdev handle to the driver
 * \param buf Pointer to store the read data
 * \param len Length of data to be read
 * \param addr Read address in flash
 * \return 0 on success
 */
extern int flash_drv_read(mdev_t *dev, uint8_t *buf,
			  uint32_t len, uint32_t addr);

/**  Erase flash
 *
 * This will erase the flash for the specified start address
 * and size
 *
 * \param dev mdev handle to the driver
 * \param start Start address in flash
 * \param size Number of bytes to be erased
 * \return 0 on success
 */
extern int flash_drv_erase(mdev_t *dev, unsigned long start,
					unsigned long size);

/**  Erase entire flash
 *
 * This will erase the entire flash chip
 *
 * \param dev mdev handle to the driver
 * \return 0 on success
 */
extern int flash_drv_eraseall(mdev_t *dev);

/** Get the unique id for flash
 * This will get the unique id for the internal flash
 * \param dev mdev handle to the driver
 * \param id a pointer to the 64 bit unique id
 * \return 0 on success
 * \return -WM_FAIL if handle is not for internal flash or id is NULL
 */
extern int flash_drv_get_id(mdev_t *dev, uint64_t *id);

#endif /* _FLASH_H_ */
