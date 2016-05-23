#ifndef __ROM_HEAP_4_H__
#define __ROM_HEAP_4_H__

void *ROM_pvPortMalloc(size_t xWantedSize);
void ROM_vPortFree(void *pv);
void *ROM_pvPortReAlloc(void *pv, size_t xWantedSize);
void ROM_prvHeapInit(unsigned char *startAddress,
                                unsigned char *endAddress);

#endif /* __ROM_HEAP_4_H__ */

