#ifndef _ZYZI_MALLOC_H_
#define _ZYZI_MALLOC_H_

#include <sys/types.h>

void zyzi_free(void *ptr);
void *zyzi_malloc(size_t size);
size_t zyzi_malloc_size(void *ptr );
void *zyzi_realloc(void *ptr , size_t size);

#endif
