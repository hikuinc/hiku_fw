/*
 *  Copyright (C) 2008-2015 Marvell International Ltd.
 *  All Rights Reserved.
 */
/*
 * This file contains essential code necessary to build any CPP app with
 * WMSDK.
 */

#include <stdio.h>
#include <stdint.h>
#include <new>

#include <wm_os.h>

/* Linker error XXX ADI */
int __dso_handle=0;

//
// Define the 'new' operator for C++ to use the freeRTOS memory management
// functions. THIS IS NOT OPTIONAL!
//
void *operator new(size_t size){
	void *p=os_mem_alloc(size);
#ifdef	__EXCEPTIONS
	if (p==0) // did pvPortMalloc succeed?
		throw std::bad_alloc(); // ANSI/ISO compliant behavior
#endif
	return p;
}

//
// Define the 'delete' operator for C++ to use the freeRTOS memory management
// functions. THIS IS NOT OPTIONAL!
//
void operator delete(void *p){
	os_mem_free( p );
}

void *operator new[](size_t size) throw()
{
	void *p=os_mem_alloc(size);
#ifdef	__EXCEPTIONS
	if (p==0) // did pvPortMalloc succeed?
		throw std::bad_alloc(); // ANSI/ISO compliant behavior
#endif
	return p;
}

void operator delete[](void *p) throw()
{
	os_mem_free( p );
}



