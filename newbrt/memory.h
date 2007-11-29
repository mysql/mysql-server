#ifndef MEMORY_H
#define MEMORY_H

#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

//#include <stdlib.h>

/* Tokutek memory allocation functions and macros.
 * These are functions for malloc and free */

/* Generally: errno is set to 0 or a value to indicate problems. */

/* Everything should call toku_malloc() instead of malloc(), and toku_calloc() instead of calloc() */
void *toku_calloc(long nmemb, long size);
void *toku_malloc(unsigned long size);
/* tagmalloc() performs a malloc(size), but fills in the first 4 bytes with typ.
 * This "tag" is useful if you are debugging and run across a void* that is
 * really a (struct foo *), and you want to figure out what it is.
 */
void *tagmalloc(unsigned long size, int typ);
void toku_free(void*);
/* toku_free_n() should be used if the caller knows the size of the malloc'd object. */
void toku_free_n(void*, unsigned long size);
void *toku_realloc(void *, long size);

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
#define MALLOC(v) v = toku_malloc(sizeof(*v))
/* MALLOC_N is like calloc:  It makes an array.  Write
 *   int *MALLOC_N(5,x);
 * to make an array of 5 integers.
 */
#define MALLOC_N(n,v) v = toku_malloc((n)*sizeof(*v))

/* If you have a type such as 
 *    struct pma *PMA;
 * and you define a corresponding int constant, such as
 *    enum typ_tag { TYP_PMA };  
 * then if you do
 *     TAGMALLOC(PMA,v);
 * you declare a variable v of type PMA and malloc a struct pma, and fill
 * in that "tag" with tagmalloc().
 */
#define TAGMALLOC(t,v) t v = tagmalloc(sizeof(*v), TYP_ ## t);

/* Copy memory.  Analogous to strdup() */
void *memdup (const void *v, unsigned int len);
/* Toku-version of strdup.  Use this so that it calls toku_malloc() */
char *toku_strdup (const char *s);

void malloc_cleanup (void); /* Before exiting, call this function to free up any internal data structures from toku_malloc.  Otherwise valgrind will complain of memory leaks. */

/* Check to see if everything malloc'd was free.  Might be a no-op depending on how memory.c is configured. */
void memory_check_all_free (void);
/* Check to see if memory is "sane".  Might be a no-op.  Probably better to simply use valgrind. */
void do_memory_check(void);

extern int memory_check; // Set to nonzero to get a (much) slower version of malloc that does (much) more checking.

int get_n_items_malloced(void); /* How many items are malloc'd but not free'd.  May return 0 depending on the configuration of memory.c */
void print_malloced_items(void); /* Try to print some malloced-but-not-freed items.  May be a noop. */
void malloc_report (void); /* report on statistics about number of mallocs.  Maybe a no-op. */ 

#endif
