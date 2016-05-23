void *_sbrk(unsigned size)
{
	extern char end; /* Defined by the linker */
	extern char __stack; /* Defined by the linker */
	static char *heap_end;
	char *prev_heap_end;

	if (heap_end == 0)
		heap_end = &end;

	prev_heap_end = heap_end;
	if ((heap_end + size) > &__stack)
		return (void *) -1;

	heap_end += size;
	return (void *) prev_heap_end;
}
