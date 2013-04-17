/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"

#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."

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
