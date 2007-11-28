#include "memory.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

int memory_check=0;

#define WHEN_MEM_DEBUG(x) ({if (memory_check) ({x});})


long long n_items_malloced=0;

/* Memory checking */
enum { items_limit = 1000 };
int overflowed=0;
static void *items[items_limit];
static long sizes[items_limit];

void note_did_malloc (void *p, long size) {
    static long long count=0;
    WHEN_MEM_DEBUG(
		   if (n_items_malloced<items_limit) { items[n_items_malloced]=p; sizes[n_items_malloced]=size; }
		   else overflowed=1;
		   //printf("%s:%d %p=malloc(%ld)\n", __FILE__, __LINE__, p, size);
		   );
    n_items_malloced++;
    count++;
}

void note_did_free(void *p) {
    WHEN_MEM_DEBUG(
		   if (!overflowed) {
		       int i;
		       //printf("not overflowed\n");
		       for (i=0; i<n_items_malloced; i++) {
			   if (items[i]==p) {
			       items[i]=items[n_items_malloced-1];
			       sizes[i]=sizes[n_items_malloced-1];
			       // printf("items[%d] replaced, now %p\n", i, items[i]); 
			       goto ok;
			   }
		       }
		       printf("%s:%d freed something (%p) not alloced\n", __FILE__, __LINE__, p);
		       abort();
		   ok:;
		   }
		   //printf("%s:%d free(%p)\n", __FILE__, __LINE__, p);
		   );
    n_items_malloced--;
}


//#define BUFFERED_MALLOC
#ifdef BUFFERED_MALLOC

enum { BUFFERING = 4096 };
void mark_buffer (char *p, int size) {
    unsigned int *pl = (unsigned int*)p;
    int i;
    for (i=0; i<BUFFERING/4; i++) {
	pl[i] = 0xdeadbeef;
    }
    pl[BUFFERING/8] = size;
}
int check_buffer (char *p) {
    unsigned int *pl = (unsigned int*)p;
    int i;
    for (i=0; i<BUFFERING/4; i++) {
	if (i!=BUFFERING/8) {
	    assert(pl[i] == 0xdeadbeef);
	}
    }
    return pl[BUFFERING/8];
}

void check_all_buffers (void) {
    int i;
    if (!overflowed) {
	for (i=0; i<n_items_malloced; i++) {
	    int size = check_buffer(((char*)items[i])-BUFFERING);
	    check_buffer(((char*)items[i])+size);
	}
    }
}

void *actual_malloc(long size) {
    char *r = malloc(size+BUFFERING*2);
    mark_buffer(r, size);
    mark_buffer(r+size+BUFFERING, size);
    check_all_buffers();
    return r+BUFFERING;
}

void actual_free(void *pv) {
    char *p = pv;
    int size=check_buffer(p-BUFFERING);
    check_buffer(p+size);
    check_all_buffers();
    //free(p-BUFFERING);
}
void *actual_realloc(void *pv, long size) {
    check_all_buffers();
    {
	char *p = pv;
	char *r = realloc(p-BUFFERING, size+BUFFERING*2);
	mark_buffer(r, size);
	mark_buffer(r+size+BUFFERING, size);
	return r+BUFFERING;
    }
}
void *actual_calloc (long nmemb, long size) {
    return actual_malloc(nmemb*size);
}

void do_memory_check (void) {
    check_all_buffers();
}
#else
#define actual_malloc malloc
#define actual_free free
#define actual_realloc realloc
#define actual_calloc calloc
#endif


void *toku_calloc(long nmemb, long size) {
    void *r;
    errno=0;
    r = actual_calloc(nmemb, size);
    //printf("%s:%d calloc(%ld,%ld)->%p\n", __FILE__, __LINE__, nmemb, size, r);
    note_did_malloc(r, nmemb*size);
    //if ((long)r==0x80523f8) { printf("%s:%d %p\n", __FILE__, __LINE__, r);  }
    return r;
}
#define FREELIST_LIMIT (64+1)
static void *freelist[FREELIST_LIMIT];

#define MALLOC_SIZE_COUNTING_LIMIT 512
int malloc_counts[MALLOC_SIZE_COUNTING_LIMIT]; // We rely on static variables being initialized to 0.
int fresh_malloc_counts[MALLOC_SIZE_COUNTING_LIMIT];  // We rely on static variables being initialized to 0.
int other_malloc_count=0, fresh_other_malloc_count=0;
void *toku_malloc(unsigned long size) {
    void * r;
    errno=0;
    if (size<MALLOC_SIZE_COUNTING_LIMIT) malloc_counts[size]++;
    else other_malloc_count++;

    if (size>=sizeof(void*) && size<FREELIST_LIMIT) {
	if (freelist[size]) {
	    r = freelist[size];
	    freelist[size] = *(void**)r;
	    note_did_malloc(r, size);
	    return r;
	}
    }
    if (size<MALLOC_SIZE_COUNTING_LIMIT) fresh_malloc_counts[size]++;
    else fresh_other_malloc_count++;
    r=actual_malloc(size);
    //printf("%s:%d malloc(%ld)->%p\n", __FILE__, __LINE__, size,r);
    note_did_malloc(r, size);
    //if ((long)r==0x80523f8) { printf("%s:%d %p size=%ld\n", __FILE__, __LINE__, r, size);   }
    return r;
}
void *tagmalloc(unsigned long size, int typtag) {
    //printf("%s:%d tagmalloc\n", __FILE__, __LINE__);
    void *r = toku_malloc(size);
    assert(size>sizeof(int));
    ((int*)r)[0] = typtag;
    return r;
}

void *toku_realloc(void *p, long size) {
    void *newp;
    note_did_free(p);
    errno=0;
    newp = actual_realloc(p, size);
    //printf("%s:%d realloc(%p,%ld)-->%p\n", __FILE__, __LINE__, p, size, newp);
    note_did_malloc(newp, size);
    return newp;
}

void toku_free(void* p) {
    //printf("%s:%d free(%p)\n", __FILE__, __LINE__, p);
    note_did_free(p);
    actual_free(p);
}

void toku_free_n(void* p, unsigned long size) {
    //printf("%s:%d free(%p)\n", __FILE__, __LINE__, p);
    note_did_free(p);
    if (size>=sizeof(void*) && size<FREELIST_LIMIT) {
	//printf("freelist[%lu] ||= %p\n", size, p);
	*(void**)p = freelist[size];
	freelist[size]=p;
    } else {
	actual_free(p);
    }
}

void *memdup (const void *v, unsigned int len) {
    void *r=toku_malloc(len);
    memcpy(r,v,len);
    return r;
}
char *toku_strdup (const char *s) {
    return memdup(s, strlen(s)+1);
}

void memory_check_all_free (void) {
    if (n_items_malloced>0) {
	printf("n_items_malloced=%lld\n", n_items_malloced);
	if (memory_check)
	    printf(" one item is %p size=%ld\n", items[0], sizes[0]);
    }
    assert(n_items_malloced==0);
}

int get_n_items_malloced (void) { return n_items_malloced; }
void print_malloced_items (void) {
    int i;
    for (i=0; i<n_items_malloced; i++) {
	printf(" %p size=%ld\n", items[i], sizes[i]);
    }
}

void malloc_report (void) {
    int i;
    printf("malloc report:\n");
    for (i=0; i<MALLOC_SIZE_COUNTING_LIMIT; i++) {
	if (malloc_counts[i] || fresh_malloc_counts[i]) printf("%d: %d (%d fresh)\n", i, malloc_counts[i], fresh_malloc_counts[i]);
    }
    printf("Other: %d (%d fresh)\n", other_malloc_count, fresh_other_malloc_count);
}

void malloc_cleanup (void) {
    int i;
    for (i=0; i<FREELIST_LIMIT; i++) {
	void *p;
	while ((p = freelist[i])) {
	    freelist[i] = *(void**)p;
	    actual_free(p);
	}
    }
}
