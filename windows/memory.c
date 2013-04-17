#include "toku_portability.h"
#include "memory.h"
#include <string.h>
#include <stdlib.h>
#include "toku_assert.h"

int toku_memory_check=0;

int toku_calloc_counter = 0;
int toku_malloc_counter = 0;
int toku_realloc_counter = 0;
int toku_free_counter = 0;

typedef void *(*malloc_fun_t)(size_t);
typedef void  (*free_fun_t)(void*);
typedef void *(*realloc_fun_t)(void*,size_t);

static malloc_fun_t  t_malloc  = 0;
static free_fun_t    t_free    = 0;
static realloc_fun_t t_realloc = 0;

void *toku_malloc(size_t size) {
    toku_malloc_counter++;
    if (t_malloc)
	return t_malloc(size);
    else
	return os_malloc(size);
}

void *
toku_calloc(size_t nmemb, size_t size)
{
    size_t newsize = nmemb * size;
    toku_calloc_counter++;
    void *vp = toku_malloc(newsize);
    if (vp) memset(vp, 0, newsize);
    return vp;
}

void *
toku_xmalloc(size_t size) {
    void *r = toku_malloc(size);
    if (r==0) abort();
    return r;
}

void *
toku_xrealloc(void *v, size_t size)
{
    void *r = toku_realloc(v, size);
    if (r==0) abort();
    return r;
}

void *
toku_tagmalloc(size_t size, enum typ_tag typtag)
{
    //printf("%s:%d tagmalloc\n", __FILE__, __LINE__);
    void *r = toku_malloc(size);
    if (!r) return 0;
    assert(size>sizeof(int));
    ((int*)r)[0] = typtag;
    return r;
}

void *
toku_realloc(void *p, size_t size)
{
    toku_realloc_counter++;
    if (t_realloc)
	return t_realloc(p, size);
    else
	return os_realloc(p, size);
}

void
toku_free(void* p)
{
    (void)__sync_fetch_and_add(&toku_free_counter, 1);
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
toku_memory_check_all_free (void)
{
}

int
toku_get_n_items_malloced (void)
{
    return 0;
}

void
toku_print_malloced_items (void)
{
}

void
toku_malloc_report (void)
{
}

void
toku_malloc_cleanup (void)
{
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
