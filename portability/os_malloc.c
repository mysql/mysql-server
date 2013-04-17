/* -*- mode: C; c-basic-offset: 4 -*- */

#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."

#include <config.h>

#include <toku_portability.h>
#include <stdlib.h>
#if defined(HAVE_MALLOC_H)
# include <malloc.h>
#elif defined(HAVE_SYS_MALLOC_H)
# include <sys/malloc.h>
#endif

void *
os_malloc(size_t size)
{
    return malloc(size);
}

void *
os_realloc(void *p, size_t size)
{
    return realloc(p, size);
}

void
os_free(void* p)
{
    free(p);
}
