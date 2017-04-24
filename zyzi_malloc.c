#include <stdlib.h>

#include "zyzi_malloc.h"

#define PREFIX_SIZE sizeof(size_t)

void *zyzi_malloc(size_t size)
{
    void *realptr = malloc(size + PREFIX_SIZE);

    if (realptr) {
        *((size_t *) realptr) = size;

        return (char *) realptr + PREFIX_SIZE;
    }

    return NULL;
}

void zyzi_free(void *ptr)
{
    if (ptr) {
        free((char *) ptr - PREFIX_SIZE);
    }
}

void *zyzi_realloc(void *ptr , size_t size)
{
    void *realptr , *newptr;

    if (!ptr) return NULL;

    realptr = (char *) ptr - PREFIX_SIZE;
    newptr  = realloc(realptr , size + PREFIX_SIZE);

    if (newptr == NULL) {
        newptr = realptr;
    } else {
        *((size_t *) newptr) = size;
    }

    return (char *) newptr + PREFIX_SIZE;
}

size_t zyzi_malloc_size(void *ptr )
{
    void *realptr;

    if (ptr == NULL) return 0;

    realptr = (char *) ptr - PREFIX_SIZE;

    return *((size_t *) realptr);
}
