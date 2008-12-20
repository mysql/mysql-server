/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "toku_portability.h"
#include <malloc.h>

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
