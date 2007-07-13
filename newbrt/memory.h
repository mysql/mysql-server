//#include <stdlib.h>

/* errno is set to 0 or a value to indicate problems. */
void *my_calloc(long nmemb, long size);
void *my_malloc(long size);
void *tagmalloc(unsigned long size, int typ);
void my_free(void*);
void *my_realloc(void *, long size);

#define MALLOC(v) v = my_malloc(sizeof(*v))
#define MALLOC_N(n,v) v = my_malloc((n)*sizeof(*v))

#define TAGMALLOC(t,v) t v = tagmalloc(sizeof(*v), TYP_ ## t);

void *memdup (const void *v, unsigned int len);
char *mystrdup (const char *s);

void memory_check_all_free (void);
void do_memory_check(void);

extern int memory_check; // Set to nonzero to get a (much) slower version of malloc that does (much) more checking.

int get_n_items_malloced(void);
void print_malloced_items(void);
