#include "cachetable.h"
#include "memory.h"
#include "yerror.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>

//#define TRACE_CACHETABLE
#ifdef TRACE_CACHETABLE
#define WHEN_TRACE_CT(x) x
#else
#define WHEN_TRACE_CT(x) ((void)0)
#endif

typedef struct ctpair *PAIR;
struct ctpair {
    enum typ_tag tag;
    long long pinned;
    char     dirty;
    CACHEKEY key;
    void    *value;
    PAIR     next,prev; // In LRU list.
    PAIR     hash_chain;
    CACHEFILE cachefile;
    void (*flush_callback)(CACHEFILE,CACHEKEY,void*, int write_me, int keep_me);
    int (*fetch_callback)(CACHEFILE,CACHEKEY,void**,void*extrargs);
    void*extraargs;
};

struct cachetable {
    enum typ_tag tag;
    int n_in_table;
    int table_size;
    PAIR *table;
    PAIR  head,tail; // of LRU list.  head is the most recently used.  tail is least recently used.
    CACHEFILE cachefiles;
};

struct fileid {
    dev_t st_dev; /* device and inode are enough to uniquely identify a file in unix. */
    ino_t st_ino;
};

struct cachefile {
    CACHEFILE next;
    int refcount; /* CACHEFILEs are shared. Use a refcount to decide when to really close it. */
    int fd;       /* Bug: If a file is opened read-only, then it is stuck in read-only.  If it is opened read-write, then subsequent writers can write to it too. */
    CACHETABLE cachetable;
    struct fileid fileid;
};

int create_cachetable (CACHETABLE *result, int n_entries) {
    TAGMALLOC(CACHETABLE, t);
    int i;
    t->n_in_table = 0;
    t->table_size = n_entries;
    t->table = toku_calloc(t->table_size, sizeof(struct ctpair));
    assert(t->table);
    t->head = t->tail = 0;
    for (i=0; i<t->table_size; i++) {
	t->table[i]=0;
    }
    t->cachefiles = 0;
    *result = t;
    return 0;
}

int cachetable_openf (CACHEFILE *cf, CACHETABLE t, const char *fname, int flags, mode_t mode) {
    int r;
    CACHEFILE extant;
    struct stat statbuf;
    struct fileid fileid;
    int fd = open(fname, flags, mode);
    if (fd<0) return errno;
    memset(&fileid, 0, sizeof(fileid));
    r=fstat(fd, &statbuf);
    assert(r==0);
    fileid.st_dev = statbuf.st_dev;
    fileid.st_ino = statbuf.st_ino;
    for (extant = t->cachefiles; extant; extant=extant->next) {
	if (memcmp(&extant->fileid, &fileid, sizeof(fileid))==0) {
	    close(fd);
	    extant->refcount++;
	    *cf = extant;
	    return 0;
	}
    }
    {
	CACHEFILE MALLOC(newcf);
	newcf->next = t->cachefiles;
	newcf->refcount = 1;
	newcf->fd = fd;
	newcf->cachetable = t;
	newcf->fileid = fileid;
	t->cachefiles = newcf;
	*cf = newcf;
	return 0;
    }
}

CACHEFILE remove_cf_from_list (CACHEFILE cf, CACHEFILE list) {
    if (list==0) return 0;
    else if (list==cf) {
	return list->next;
    } else {
	list->next = remove_cf_from_list(cf, list->next);
	return list;
    }
}

int cachefile_flush (CACHEFILE cf);

int cachefile_close (CACHEFILE *cfp) {
    CACHEFILE cf = *cfp;
    assert(cf->refcount>0);
    cf->refcount--;
    if (cf->refcount==0) {
	int r;
	if ((r = cachefile_flush(cf))) return r;
	r = close(cf->fd);
	cf->cachetable->cachefiles = remove_cf_from_list(cf, cf->cachetable->cachefiles);
	toku_free(cf);
	*cfp=0;
	return r;
    } else {
	*cfp=0;
	return 0;
    }
}

int cachetable_assert_all_unpinned (CACHETABLE t) {
    int i;
    int some_pinned=0;
    for (i=0; i<t->table_size; i++) {
	PAIR p;
	for (p=t->table[i]; p; p=p->hash_chain) {
	    assert(p->pinned>=0);
	    if (p->pinned) {
		printf("%s:%d pinned: %lld (%p)\n", __FILE__, __LINE__, p->key, p->value);
		some_pinned=1;
	    }
	}
    }
    return some_pinned;
}

int cachefile_count_pinned (CACHEFILE cf, int print_them) {
    int i;
    int n_pinned=0;
    CACHETABLE t = cf->cachetable;
    for (i=0; i<t->table_size; i++) {
	PAIR p;
	for (p=t->table[i]; p; p=p->hash_chain) {
	    assert(p->pinned>=0);
	    if (p->pinned && p->cachefile==cf) {
		if (print_them) printf("%s:%d pinned: %lld (%p)\n", __FILE__, __LINE__, p->key, p->value);
		n_pinned++;
	    }
	}
    }
    return n_pinned;
}

static unsigned int hash_key (const char *key, int keylen) {
    /* From Sedgewick.  There are probably better hash functions. */
    unsigned int b    = 378551;
    unsigned int a    = 63689;
    unsigned int hash = 0;
    int i;
    for (i = 0; i < keylen; i++ ) {
	hash = hash * a + key[i];
	a *= b;
    }
    return hash;
}

unsigned int ct_hash_longlong (unsigned long long l) {
    unsigned int r = hash_key((char*)&l, 8);
    printf("%lld --> %d --> %d\n", l, r, r%64);
    return  r;
}

static unsigned int hashit (CACHETABLE t, CACHEKEY key) {
    return hash_key((char*)&key, sizeof(key))%t->table_size;
}


static void lru_remove (CACHETABLE t, PAIR p) {
    if (p->next) {
	p->next->prev = p->prev;
    } else {
	assert(t->tail==p);
	t->tail = p->prev;
    }
    if (p->prev) {
	p->prev->next = p->next;
    } else {
	assert(t->head==p);
	t->head = p->next;
    }
    p->prev = p->next = 0;
}

static void lru_add_to_list (CACHETABLE t, PAIR p) {
    // requires that touch_me is not currently in the table.
    assert(p->prev==0);
    p->prev = 0;
    p->next = t->head;
    if (t->head) {
	t->head->prev = p;
    } else {
	assert(!t->tail);
	t->tail = p;
    }
    t->head = p; 
}

static void lru_touch (CACHETABLE t, PAIR p) {
    lru_remove(t,p);
    lru_add_to_list(t,p);
}

static PAIR remove_from_hash_chain (PAIR remove_me, PAIR list) {
    if (remove_me==list) return list->hash_chain;
    list->hash_chain = remove_from_hash_chain(remove_me, list->hash_chain);
    return list;
}

static void flush_and_remove (CACHETABLE t, PAIR remove_me, int write_me) {
    unsigned int h = hashit(t, remove_me->key);
    lru_remove(t, remove_me);
    //printf("flush_callback(%lld,%p)\n", remove_me->key, remove_me->value);
    WHEN_TRACE_CT(printf("%s:%d CT flush_callback(%lld, %p, dirty=%d, 0)\n", __FILE__, __LINE__, remove_me->key, remove_me->value, remove_me->dirty && write_me)); 
    //printf("%s:%d TAG=%x p=%p\n", __FILE__, __LINE__, remove_me->tag, remove_me);
    //printf("%s:%d dirty=%d\n", __FILE__, __LINE__, remove_me->dirty);
    remove_me->flush_callback(remove_me->cachefile, remove_me->key, remove_me->value, remove_me->dirty && write_me, 0);
    t->n_in_table--;
    // Remove it from the hash chain.
    t->table[h] = remove_from_hash_chain (remove_me, t->table[h]);
    toku_free(remove_me);
}

static void flush_and_keep (PAIR flush_me) {
    if (flush_me->dirty) {
	WHEN_TRACE_CT(printf("%s:%d CT flush_callback(%lld, %p, dirty=1, 0)\n", __FILE__, __LINE__, flush_me->key, flush_me->value)); 
        flush_me->flush_callback(flush_me->cachefile, flush_me->key, flush_me->value, 1, 1);
	flush_me->dirty=0;
    }
}

static int maybe_flush_some (CACHETABLE t) {
 again:
    if (t->n_in_table>=t->table_size) {
	/* Try to remove one. */
	PAIR remove_me;
	for (remove_me = t->tail; remove_me; remove_me = remove_me->prev) {
	    if (!remove_me->pinned) {
		flush_and_remove(t, remove_me, 1);
		goto again;
	    }
	}
	/* All were pinned. */
	printf("All are pinned\n");
	return 1;
    }
    return 0;
}

int cachetable_put (CACHEFILE cachefile, CACHEKEY key, void*value,
		    void (*flush_callback)(CACHEFILE,CACHEKEY,void*, int /*write_me*/, int /*keep_me*/),
		    int (*fetch_callback)(CACHEFILE,CACHEKEY,void**,void*/*extraargs*/),
		    void*extraargs
		    ) {
    int h = hashit(cachefile->cachetable, key);
    WHEN_TRACE_CT(printf("%s:%d CT cachetable_put(%lld)=%p\n", __FILE__, __LINE__, key, value));
    {
	PAIR p;
	for (p=cachefile->cachetable->table[h]; p; p=p->hash_chain) {
	    if (p->key==key && p->cachefile==cachefile) {
		// Semantically, these two asserts are not strictly right.  After all, when are two functions eq?
		// In practice, the functions better be the same.
		assert(p->flush_callback==flush_callback);
		assert(p->fetch_callback==fetch_callback);
		return -1; /* Already present. */
	    }
	}
    }
    if (maybe_flush_some(cachefile->cachetable)) return -2;
    
    {
	TAGMALLOC(PAIR, p);
	p->pinned=1;
	p->dirty =1;
	//printf("%s:%d p=%p dirty=%d\n", __FILE__, __LINE__, p, p->dirty);
	p->key = key;
	p->value = value;
	p->next = p->prev = 0;
	p->cachefile = cachefile;
	p->flush_callback = flush_callback;
	p->fetch_callback = fetch_callback;
	p->extraargs = extraargs;
	lru_add_to_list(cachefile->cachetable, p);
	p->hash_chain = cachefile->cachetable->table[h];
	cachefile->cachetable->table[h] = p;
	cachefile->cachetable->n_in_table++;
	return 0;
    }
}

int cachetable_get_and_pin (CACHEFILE cachefile, CACHEKEY key, void**value,
			    void(*flush_callback)(CACHEFILE,CACHEKEY,void*,int write_me, int keep_me),
			    int(*fetch_callback)(CACHEFILE, CACHEKEY key, void**value,void*extraargs), /* If we are asked to fetch something, get it by calling this back. */
			    void*extraargs
			    ) {
    CACHETABLE t = cachefile->cachetable;
    int h = hashit(t,key);
    PAIR p;
    for (p=t->table[h]; p; p=p->hash_chain) {
	if (p->key==key && p->cachefile==cachefile) {
	    *value = p->value;
	    p->pinned++;
	    lru_touch(t,p);
	    WHEN_TRACE_CT(printf("%s:%d cachtable_get_and_pin(%lld)--> %p\n", __FILE__, __LINE__, key, *value));
	    return 0;
	}
    }
    if (maybe_flush_some(t)) return -2;
    {
	void *toku_value;
	int r;
	WHEN_TRACE_CT(printf("%s:%d CT: fetch_callback(%lld...)\n", __FILE__, __LINE__, key));
	if ((r=fetch_callback(cachefile, key, &toku_value,extraargs))) return r;
	cachetable_put(cachefile, key, toku_value, flush_callback, fetch_callback,extraargs);
	*value = toku_value;
    }
    WHEN_TRACE_CT(printf("%s:%d did fetch: cachtable_get_and_pin(%lld)--> %p\n", __FILE__, __LINE__, key, *value));
    return 0;
}

int cachetable_maybe_get_and_pin (CACHEFILE cachefile, CACHEKEY key, void**value) {
    CACHETABLE t = cachefile->cachetable;
    int h = hashit(t,key);
    PAIR p;
    for (p=t->table[h]; p; p=p->hash_chain) {
	if (p->key==key && p->cachefile==cachefile) {
	    *value = p->value;
	    p->pinned++;
	    lru_touch(t,p);
	    printf("%s:%d cachetable_maybe_get_and_pin(%lld)--> %p\n", __FILE__, __LINE__, key, *value);
	    return 0;
	}
    }
    return -1;
}


int cachetable_unpin (CACHEFILE cachefile, CACHEKEY key, int dirty) {
    CACHETABLE t = cachefile->cachetable;
    int h = hashit(t,key);
    PAIR p;
    WHEN_TRACE_CT(printf("%s:%d unpin(%lld)", __FILE__, __LINE__, key));
    //printf("%s:%d is dirty now=%d\n", __FILE__, __LINE__, dirty);
    for (p=t->table[h]; p; p=p->hash_chain) {
	if (p->key==key && p->cachefile==cachefile) {
	    assert(p->pinned>0);
	    p->pinned--;
	    p->dirty  |= dirty;
	    WHEN_TRACE_CT(printf("[count=%lld]\n", p->pinned));
	    return 0;
	}
    }
    printf("\n");
    return 0;
}

int cachetable_flush (CACHETABLE t) {
    int i;
    for (i=0; i<t->table_size; i++) {
	PAIR p;
	while ((p = t->table[i]))
	    flush_and_remove(t, p, 1); // Must be careful, since flush_and_remove kills the linked list.
	}
    return 0;
}

int cachefile_flush (CACHEFILE cf) {
    int i;
    CACHETABLE t = cf->cachetable;
    for (i=0; i<t->table_size; i++) {
	PAIR p;
    again:
	p = t->table[i];
	while (p) {
	    if (p->cachefile==cf) {
		flush_and_remove(t, p, 1); // Must be careful, since flush_and_remove kills the linked list.
		goto again;
	    } else {
		p=p->next;
	    }
	}
    }
    return 0;
}


/* Require that it all be flushed. */
int cachetable_close (CACHETABLE *tp) {
    CACHETABLE t=*tp;
    int i;
    int r;
    if ((r=cachetable_flush(t))) return r;
    for (i=0; i<t->table_size; i++) {
	if (t->table[i]) return -1;
    }
    toku_free(t->table);
    toku_free(t);
    *tp = 0;
    return 0;
}

int cachetable_remove (CACHEFILE cachefile, CACHEKEY key, int write_me) {
    /* Removing something already present is OK. */
    CACHETABLE t = cachefile->cachetable;
    int h = hashit(t,key);
    PAIR p;
    for (p=t->table[h]; p; p=p->hash_chain) {
	if (p->key==key && p->cachefile==cachefile) {
	    flush_and_remove(t, p, write_me);
	    return 0;
	}
    }
    return 0;
}

static int cachetable_fsync_pairs (CACHETABLE t, PAIR p) {
    if (p) {
	int r = cachetable_fsync_pairs(t, p->hash_chain);
	if (r!=0) return r;
	flush_and_keep(p);
    }
    return 0;
}

int cachetable_fsync (CACHETABLE t) {
    int i;
    int r;
    for (i=0; i<t->table_size; i++) {
	r=cachetable_fsync_pairs(t, t->table[i]);
	if (r!=0) return r;
    }
    return 0;
}

#if 0
int cachefile_pwrite (CACHEFILE cf, const void *buf, size_t count, off_t offset) {
    ssize_t r = pwrite(cf->fd, buf, count, offset);
    if (r==-1) return errno;
    assert((size_t)r==count);
    return 0;
}
int cachefile_pread  (CACHEFILE cf, void *buf, size_t count, off_t offset) {
    ssize_t r = pread(cf->fd, buf, count, offset);
    if (r==-1) return errno;
    if (r==0) return -1; /* No error for EOF ??? */
    assert((size_t)r==count);
    return 0;
}
#endif

int cachefile_fd (CACHEFILE cf) {
    return cf->fd;
}
