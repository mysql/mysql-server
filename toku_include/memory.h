#ifndef MEMORY_H
#define MEMORY_H

#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <stdlib.h>
#include <toku_portability.h>

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

/* Tokutek memory allocation functions and macros.
 * These are functions for malloc and free */

int toku_memory_startup(void);
void toku_memory_shutdown(void);

/* Generally: errno is set to 0 or a value to indicate problems. */

/* Everything should call toku_malloc() instead of malloc(), and toku_calloc() instead of calloc() */
void *toku_calloc(size_t nmemb, size_t size)  __attribute__((__visibility__("default")));
void *toku_xcalloc(size_t nmemb, size_t size)  __attribute__((__visibility__("default")));
void *toku_malloc(size_t size)  __attribute__((__visibility__("default")));

// xmalloc aborts instead of return NULL if we run out of memory
void *toku_xmalloc(size_t size);
void *toku_xrealloc(void*, size_t size) __attribute__((__visibility__("default")));

void toku_free(void*) __attribute__((__visibility__("default")));
/* toku_free_n() should be used if the caller knows the size of the malloc'd object. */
void toku_free_n(void*, size_t size);
void *toku_realloc(void *, size_t size)  __attribute__((__visibility__("default")));

size_t toku_malloc_usable_size(void *p) __attribute__((__visibility__("default")));

/* MALLOC is a macro that helps avoid a common error:
 * Suppose I write
 *    struct foo *x = malloc(sizeof(struct foo));
 * That works fine.  But if I change it to this, I've probably made an mistake:
 *    struct foo *x = malloc(sizeof(struct bar));
 * It can get worse, since one might have something like
 *    struct foo *x = malloc(sizeof(struct foo *))
 * which looks reasonable, but it allocoates enough to hold a pointer instead of the amount needed for the struct.
 * So instead, write
 *    struct foo *MALLOC(x);
 * and you cannot go wrong.
 */
#define MALLOC(v) v = cast_to_typeof(v) toku_malloc(sizeof(*v))
/* MALLOC_N is like calloc(Except no 0ing of data):  It makes an array.  Write
 *   int *MALLOC_N(5,x);
 * to make an array of 5 integers.
 */
#define MALLOC_N(n,v) v = cast_to_typeof(v) toku_malloc((n)*sizeof(*v))

//CALLOC_N is like calloc with auto-figuring out size of members
#define CALLOC_N(n,v) v = cast_to_typeof(v) toku_calloc((n), sizeof(*v)) 

#define CALLOC(v) CALLOC_N(1,v)

#define REALLOC_N(n,v) v = cast_to_typeof(v) toku_realloc(v, (n)*sizeof(*v))

// XMALLOC macros are like MALLOC except they abort if the operation fails
#define XMALLOC(v) v = cast_to_typeof(v) toku_xmalloc(sizeof(*v))
#define XMALLOC_N(n,v) v = cast_to_typeof(v) toku_xmalloc((n)*sizeof(*v))
#define XCALLOC_N(n,v) v = cast_to_typeof(v) toku_xcalloc((n), (sizeof(*v)))

#define XCALLOC(v) XCALLOC_N(1,(v))
#define XREALLOC_N(n,v) v = cast_to_typeof(v) toku_xrealloc(v, (n)*sizeof(*v))

/* Copy memory.  Analogous to strdup() */
void *toku_memdup (const void *v, size_t len);
/* Toku-version of strdup.  Use this so that it calls toku_malloc() */
char *toku_strdup (const char *s)   __attribute__((__visibility__("default")));

/* Copy memory.  Analogous to strdup() Crashes instead of returning NULL */
void *toku_xmemdup (const void *v, size_t len) __attribute__((__visibility__("default")));
/* Toku-version of strdup.  Use this so that it calls toku_xmalloc()  Crashes instead of returning NULL */
char *toku_xstrdup (const char *s)   __attribute__((__visibility__("default")));

void toku_malloc_cleanup (void); /* Before exiting, call this function to free up any internal data structures from toku_malloc.  Otherwise valgrind will complain of memory leaks. */

/* Check to see if everything malloc'd was free.  Might be a no-op depending on how memory.c is configured. */
void toku_memory_check_all_free (void);
/* Check to see if memory is "sane".  Might be a no-op.  Probably better to simply use valgrind. */
void toku_do_memory_check(void);

typedef void *(*malloc_fun_t)(size_t);
typedef void  (*free_fun_t)(void*);
typedef void *(*realloc_fun_t)(void*,size_t);

void toku_set_func_malloc(malloc_fun_t f);
void toku_set_func_xmalloc_only(malloc_fun_t f);
void toku_set_func_malloc_only(malloc_fun_t f);
void toku_set_func_realloc(realloc_fun_t f);
void toku_set_func_xrealloc_only(realloc_fun_t f);
void toku_set_func_realloc_only(realloc_fun_t f);
void toku_set_func_free(free_fun_t f);

typedef struct memory_status {
    uint64_t malloc_count;    // number of malloc operations
    uint64_t free_count;      // number of free operations
    uint64_t realloc_count;   // number of realloc operations
    uint64_t malloc_fail;     // number of malloc operations that failed 
    uint64_t realloc_fail;    // number of realloc operations that failed 
    uint64_t requested;       // number of bytes requested
    uint64_t used;            // number of bytes used (requested + overhead), obtained from malloc_usable_size()
    uint64_t freed;           // number of bytes freed;
    uint64_t max_in_use;      // maximum memory footprint (used - freed), approximate (not worth threadsafety overhead for exact)
    const char *mallocator_version;
    uint64_t mmap_threshold;
} MEMORY_STATUS_S, *MEMORY_STATUS;

void toku_memory_get_status(MEMORY_STATUS s);

size_t toku_memory_footprint(void * p, size_t touched);

#if defined(__cplusplus) || defined(__cilkplusplus)
}
#endif

#endif
