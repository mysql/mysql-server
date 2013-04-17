#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."

#include <toku_portability.h>
#include "memory.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "toku_assert.h"

int toku_memory_check=0;

typedef void *(*malloc_fun_t)(size_t);
typedef void  (*free_fun_t)(void*);
typedef void *(*realloc_fun_t)(void*,size_t);

static malloc_fun_t  t_malloc  = 0;
static free_fun_t    t_free    = 0;
static realloc_fun_t t_realloc = 0;

void *toku_malloc(size_t size) {
    void *p;
    if (t_malloc)
	p = t_malloc(size);
    else
	p = os_malloc(size);
    return p;
}

void *
toku_calloc(size_t nmemb, size_t size)
{
    size_t newsize = nmemb * size;
    void *vp = toku_malloc(newsize);
    if (vp) memset(vp, 0, newsize);
    return vp;
}

void *
toku_realloc(void *p, size_t size)
{
    void *q;
    if (t_realloc)
	q = t_realloc(p, size);
    else
	q = os_realloc(p, size);
    return q;
}

void *
toku_memdup (const void *v, size_t len)
{
    void *r=toku_malloc(len);
    if (r) memcpy(r,v,len);
    return r;
}

char *
toku_strdup (const char *s)
{
    return toku_memdup(s, strlen(s)+1);
}

void
toku_free(void *p)
{
    if (t_free)
	t_free(p);
    else
	os_free(p);
}

void
toku_free_n(void* p, size_t size __attribute__((unused)))
{
    toku_free(p);
}

void *
toku_xmalloc(size_t size) {
    void *r = toku_malloc(size);
    if (r==0) abort();
    return r;
}

void *
toku_xcalloc(size_t nmemb, size_t size)
{
    size_t newsize = nmemb * size;
    void *vp = toku_xmalloc(newsize);
    if (vp) memset(vp, 0, newsize);
    return vp;
}

void *
toku_xrealloc(void *v, size_t size)
{
    void *r = toku_realloc(v, size);
    if (r==0) abort();
    return r;
}

void *
toku_xmemdup (const void *v, size_t len)
{
    void *r=toku_xmalloc(len);
    memcpy(r,v,len);
    return r;
}

char *
toku_xstrdup (const char *s)
{
    return toku_xmemdup(s, strlen(s)+1);
}

int
toku_set_func_malloc(malloc_fun_t f) {
    t_malloc = f;
    return 0;
}

int
toku_set_func_realloc(realloc_fun_t f) {
    t_realloc = f;
    return 0;
}

int
toku_set_func_free(free_fun_t f) {
    t_free = f;
    return 0;
}
