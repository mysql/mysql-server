/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include "cachetable.h"
#include "memory.h"
#include "yerror.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include "hashfun.h"
#include "primes.h"

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
    long size;
    char     dirty;
    CACHEKEY key;
    void    *value;
    PAIR     next,prev; // In LRU list.
    PAIR     hash_chain;
    CACHEFILE cachefile;
    CACHETABLE_FLUSH_FUNC_T flush_callback;
    CACHETABLE_FETCH_FUNC_T fetch_callback;
    void    *extraargs;
    int      verify_flag; /* Used in verify_cachetable() */
    LSN      modified_lsn; // What was the LSN when modified (undefined if not dirty)
    LSN      written_lsn;  // What was the LSN when written (we need to get this information when we fetch)
};

// The cachetable is as close to an ENV as we get.
struct cachetable {
    enum typ_tag tag;
    int n_in_table;
    int table_size;
    PAIR *table;
    PAIR  head,tail; // of LRU list.  head is the most recently used.  tail is least recently used.
    CACHEFILE cachefiles;
    long size_current, size_limit;
    int primeidx;
    LSN lsn_of_checkpoint;  // the most recent checkpoint in the log.
    TOKULOGGER logger;
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
    FILENUM filenum;
};

int toku_create_cachetable(CACHETABLE *result, long size_limit, LSN initial_lsn, TOKULOGGER logger) {
    TAGMALLOC(CACHETABLE, t);
    int i;
    t->n_in_table = 0;
    t->primeidx = 0;
    t->table_size = toku_get_prime(t->primeidx);
    MALLOC_N(t->table_size, t->table);
    assert(t->table);
    t->head = t->tail = 0;
    for (i=0; i<t->table_size; i++) {
	t->table[i]=0;
    }
    t->cachefiles = 0;
    t->size_current = 0;
    t->size_limit = size_limit;
    t->lsn_of_checkpoint = initial_lsn;
    t->logger = logger; 
    *result = t;
    return 0;
}

int toku_cachetable_openfd (CACHEFILE *cf, CACHETABLE t, int fd) {
    int r;
    CACHEFILE extant;
    FILENUM max_filenum_in_use={0};
    struct stat statbuf;
    struct fileid fileid;
    memset(&fileid, 0, sizeof(fileid));
    r=fstat(fd, &statbuf);
    assert(r==0);
    fileid.st_dev = statbuf.st_dev;
    fileid.st_ino = statbuf.st_ino;
    for (extant = t->cachefiles; extant; extant=extant->next) {
	if (max_filenum_in_use.fileid<extant->filenum.fileid) max_filenum_in_use=extant->filenum;
	if (memcmp(&extant->fileid, &fileid, sizeof(fileid))==0) {
	    r = close(fd);
            assert(r == 0);
	    extant->refcount++;
	    *cf = extant;
	    return 0;
	}
    }
    {
	CACHEFILE MALLOC(newcf);
	newcf->filenum.fileid = 1+max_filenum_in_use.fileid;
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

int toku_cachetable_openf (CACHEFILE *cf, CACHETABLE t, const char *fname, int flags, mode_t mode) {
    int fd = open(fname, flags, mode);
    if (fd<0) return errno;
    return toku_cachetable_openfd (cf, t, fd);
}

static CACHEFILE remove_cf_from_list (CACHEFILE cf, CACHEFILE list) {
    if (list==0) return 0;
    else if (list==cf) {
	return list->next;
    } else {
	list->next = remove_cf_from_list(cf, list->next);
	return list;
    }
}

static int cachefile_flush_and_remove (CACHEFILE cf);

int toku_cachefile_close (CACHEFILE *cfp) {
    CACHEFILE cf = *cfp;
    assert(cf->refcount>0);
    cf->refcount--;
    if (cf->refcount==0) {
	int r;
	if ((r = cachefile_flush_and_remove(cf))) return r;
	r = close(cf->fd);
	assert(r == 0);
        cf->fd = -1;
        cf->cachetable->cachefiles = remove_cf_from_list(cf, cf->cachetable->cachefiles);
	toku_free(cf);
	*cfp=0;
	return r;
    } else {
	*cfp=0;
	return 0;
    }
}

int toku_cachetable_assert_all_unpinned (CACHETABLE t) {
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

int toku_cachefile_count_pinned (CACHEFILE cf, int print_them) {
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

#if 0
unsigned int ct_hash_longlong (unsigned long long l) {
    unsigned int r = hash_key((unsigned char*)&l, 8);
    printf("%lld --> %d --> %d\n", l, r, r%64);
    return  r;
}
#endif

static unsigned int hashit (CACHETABLE t, CACHEKEY key) {
    return hash_key((unsigned char*)&key, sizeof(key))%t->table_size;
}

static void cachetable_rehash (CACHETABLE t, int primeindexdelta) {
    // printf("rehash %p %d %d %d\n", t, primeindexdelta, t->n_in_table, t->table_size);

    int newprimeindex = primeindexdelta+t->primeidx;
    if (newprimeindex < 0)
        return;
    int newtable_size = toku_get_prime(newprimeindex);
    PAIR *newtable = toku_calloc(newtable_size, sizeof(*t->table));
    int i;
    //printf("%s:%d newtable_size=%d\n", __FILE__, __LINE__, newtable_size);
    assert(newtable!=0);
    t->primeidx=newprimeindex;
    for (i=0; i<newtable_size; i++) newtable[i]=0;
    for (i=0; i<t->table_size; i++) {
	PAIR p;
	while ((p=t->table[i])!=0) {
	    unsigned int h = hash_key((unsigned char *)&p->key, sizeof (p->key))%newtable_size;
	    t->table[i] = p->hash_chain;
	    p->hash_chain = newtable[h];
	    newtable[h] = p;
	}
    }
    toku_free(t->table);
    // printf("Freed\n");
    t->table=newtable;
    t->table_size=newtable_size;
    //printf("Done growing or shrinking\n");
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

// Predicate to determine if a node must be renamed.  Nodes are renamed on the time they are written
// after a checkpoint.
//   Thus we need to rename it if it is dirty,
//    if it has been modified within the current checkpoint regime (hence non-strict inequality)
//    and the last time it was written was in a previous checkpoint regime (strict inequality)
static BOOL need_to_rename_p (CACHETABLE t, PAIR p) {
    return (p->dirty
	    && p->modified_lsn.lsn>=t->lsn_of_checkpoint.lsn   // nonstrict
	    && p->written_lsn.lsn < t->lsn_of_checkpoint.lsn); // strict
}

static void flush_and_remove (CACHETABLE t, PAIR remove_me, int write_me) {
    lru_remove(t, remove_me);
    //printf("flush_callback(%lld,%p)\n", remove_me->key, remove_me->value);
    WHEN_TRACE_CT(printf("%s:%d CT flush_callback(%lld, %p, dirty=%d, 0)\n", __FILE__, __LINE__, remove_me->key, remove_me->value, remove_me->dirty && write_me)); 
    //printf("%s:%d TAG=%x p=%p\n", __FILE__, __LINE__, remove_me->tag, remove_me);
    //printf("%s:%d dirty=%d\n", __FILE__, __LINE__, remove_me->dirty);
    remove_me->flush_callback(remove_me->cachefile, remove_me->key, remove_me->value, remove_me->size, remove_me->dirty && write_me, 0,
			      t->lsn_of_checkpoint, need_to_rename_p(t, remove_me));
    t->n_in_table--;
    // Remove it from the hash chain.
    {
	unsigned int h = hashit(t, remove_me->key);
	t->table[h] = remove_from_hash_chain (remove_me, t->table[h]);
    }
    t->size_current -= remove_me->size;
    toku_free(remove_me);
}

static int maybe_flush_some (CACHETABLE t, long size __attribute__((unused))) {
    int r = 0;
again:
//    if (t->n_in_table >= t->table_size) {
    if (size + t->size_current > t->size_limit) {
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
	r = 1;
    }

    if (4 * t->n_in_table < t->table_size)
        cachetable_rehash(t, -1);

    return r;
}

static int cachetable_insert_at(CACHEFILE cachefile, int h, CACHEKEY key, void *value, long size,
                                cachetable_flush_func_t flush_callback,
                                cachetable_fetch_func_t fetch_callback,
                                void *extraargs, int dirty,
				LSN   written_lsn) {
    TAGMALLOC(PAIR, p);
    p->pinned = 1;
    p->dirty = dirty;
    p->size = size;
    //printf("%s:%d p=%p dirty=%d\n", __FILE__, __LINE__, p, p->dirty);
    p->key = key;
    p->value = value;
    p->next = p->prev = 0;
    p->cachefile = cachefile;
    p->flush_callback = flush_callback;
    p->fetch_callback = fetch_callback;
    p->extraargs = extraargs;
    p->modified_lsn.lsn = 0;
    p->written_lsn  = written_lsn;
    CACHETABLE ct = cachefile->cachetable;
    lru_add_to_list(ct, p);
    p->hash_chain = ct->table[h];
    ct->table[h] = p;
    ct->n_in_table++;
    ct->size_current += size;
    if (ct->n_in_table > ct->table_size) {
        cachetable_rehash(ct, +1);
    }
    return 0;
}

int toku_cachetable_put(CACHEFILE cachefile, CACHEKEY key, void*value, long size,
                   cachetable_flush_func_t flush_callback, cachetable_fetch_func_t fetch_callback, void *extraargs) {
    WHEN_TRACE_CT(printf("%s:%d CT cachetable_put(%lld)=%p\n", __FILE__, __LINE__, key, value));
    {
	PAIR p;
	for (p=cachefile->cachetable->table[hashit(cachefile->cachetable, key)]; p; p=p->hash_chain) {
	    if (p->key==key && p->cachefile==cachefile) {
		// Semantically, these two asserts are not strictly right.  After all, when are two functions eq?
		// In practice, the functions better be the same.
		assert(p->flush_callback==flush_callback);
		assert(p->fetch_callback==fetch_callback);
		return -1; /* Already present. */
	    }
	}
    }
    if (maybe_flush_some(cachefile->cachetable, size)) 
        return -2;
    // flushing could change the result from hashit()
    int r = cachetable_insert_at(cachefile, hashit(cachefile->cachetable, key), key, value, size, flush_callback, fetch_callback, extraargs, 1, ZERO_LSN);
    return r;
}

int toku_cachetable_get_and_pin(CACHEFILE cachefile, CACHEKEY key, void**value, long *sizep,
				cachetable_flush_func_t flush_callback, cachetable_fetch_func_t fetch_callback, void *extraargs) {
    CACHETABLE t = cachefile->cachetable;
    int tsize __attribute__((__unused__)) = t->table_size;
    PAIR p;
    for (p=t->table[hashit(t,key)]; p; p=p->hash_chain) {
	if (p->key==key && p->cachefile==cachefile) {
	    *value = p->value;
            if (sizep) *sizep = p->size;
	    p->pinned++;
	    lru_touch(t,p);
	    WHEN_TRACE_CT(printf("%s:%d cachtable_get_and_pin(%lld)--> %p\n", __FILE__, __LINE__, key, *value));
	    return 0;
	}
    }
    if (maybe_flush_some(t, 1)) return -2;
    // Note.  hashit(t,key) may have changed as a result of flushing.
    {
	void *toku_value; 
        long size = 1; // compat
	int r;
	LSN written_lsn;
	WHEN_TRACE_CT(printf("%s:%d CT: fetch_callback(%lld...)\n", __FILE__, __LINE__, key));
	if ((r=fetch_callback(cachefile, key, &toku_value, &size, extraargs, &written_lsn))) 
            return r;
	cachetable_insert_at(cachefile, hashit(t,key), key, toku_value, size, flush_callback, fetch_callback, extraargs, 0, written_lsn);
	*value = toku_value;
        if (sizep)
            *sizep = size;
        // maybe_flush_some(t, size);
    }
    WHEN_TRACE_CT(printf("%s:%d did fetch: cachtable_get_and_pin(%lld)--> %p\n", __FILE__, __LINE__, key, *value));
    return 0;
}

int toku_cachetable_maybe_get_and_pin (CACHEFILE cachefile, CACHEKEY key, void**value) {
    CACHETABLE t = cachefile->cachetable;
    PAIR p;
    for (p=t->table[hashit(t,key)]; p; p=p->hash_chain) {
	if (p->key==key && p->cachefile==cachefile) {
	    *value = p->value;
	    p->pinned++;
	    lru_touch(t,p);
	    //printf("%s:%d cachetable_maybe_get_and_pin(%lld)--> %p\n", __FILE__, __LINE__, key, *value);
	    return 0;
	}
    }
    return -1;
}


int toku_cachetable_unpin(CACHEFILE cachefile, CACHEKEY key, int dirty, long size) {
    CACHETABLE t = cachefile->cachetable;
    PAIR p;
    WHEN_TRACE_CT(printf("%s:%d unpin(%lld)", __FILE__, __LINE__, key));
    //printf("%s:%d is dirty now=%d\n", __FILE__, __LINE__, dirty);
    for (p=t->table[hashit(t,key)]; p; p=p->hash_chain) {
	if (p->key==key && p->cachefile==cachefile) {
	    assert(p->pinned>0);
	    p->pinned--;
	    p->dirty |= dirty;
            if (size != 0) {
                t->size_current -= p->size;
                p->size = size;
                t->size_current += p->size;
            }
	    WHEN_TRACE_CT(printf("[count=%lld]\n", p->pinned));
	    return 0;
	}
    }
    return 0;
}

// effect:   Move an object from one key to another key.
// requires: The object is pinned in the table
int toku_cachetable_rename (CACHEFILE cachefile, CACHEKEY oldkey, CACHEKEY newkey) {
  CACHETABLE t = cachefile->cachetable;
  PAIR *ptr_to_p,p;
  for (ptr_to_p = &t->table[hashit(t, oldkey)],  p = *ptr_to_p;
       p;
       ptr_to_p = &p->hash_chain,                p = *ptr_to_p) {
    if (p->key==oldkey && p->cachefile==cachefile) {
      *ptr_to_p = p->hash_chain;
      p->key = newkey;
      int nh = hashit(t, newkey);
      p->hash_chain = t->table[nh];
      t->table[nh] = p;
      return 0;
    }
  }
  return -1;
}

static int cachetable_flush (CACHETABLE t) {
    int i;
    for (i=0; i<t->table_size; i++) {
	PAIR p;
	while ((p = t->table[i]))
	    flush_and_remove(t, p, 1); // Must be careful, since flush_and_remove kills the linked list.
	}
    return 0;
}

void toku_cachefile_verify (CACHEFILE cf) {
    toku_cachetable_verify(cf->cachetable);
}

void toku_cachetable_verify (CACHETABLE t) {
    // First clear all the verify flags by going through the hash chains
    {
	int i;
	for (i=0; i<t->table_size; i++) {
	    PAIR p;
	    for (p=t->table[i]; p; p=p->hash_chain) {
		p->verify_flag=0;
	    }
	}
    }
    // Now go through the LRU chain, make sure everything in the LRU chain is hashed, and set the verify flag.
    {
	PAIR p;
	for (p=t->head; p; p=p->next) {
	    assert(p->verify_flag==0);
	    PAIR p2;
	    for (p2=t->table[hashit(t,p->key)]; p2; p2=p2->hash_chain) {
		if (p2==p) {
		    /* found it */
		    goto next;
		}
	    }
	    fprintf(stderr, "Something in the LRU chain is not hashed\n");
	    assert(0);
	next:
	    p->verify_flag = 1;
	}
    }
    // Now make sure everything in the hash chains has the verify_flag set to 1.
    {
	int i;
	for (i=0; i<t->table_size; i++) {
	    PAIR p;
	    for (p=t->table[i]; p; p=p->hash_chain) {
		assert(p->verify_flag);
	    }
	}
    }
}

static void assert_cachefile_is_flushed_and_removed (CACHEFILE cf) {
    CACHETABLE t = cf->cachetable;
    int i;
    // Check it two ways
    // First way: Look through all the hash chains
    for (i=0; i<t->table_size; i++) {
	PAIR p;
	for (p=t->table[i]; p; p=p->hash_chain) {
	    assert(p->cachefile!=cf);
	}
    }
    // Second way: Look through the LRU list.
    {
	PAIR p;
	for (p=t->head; p; p=p->next) {
	    assert(p->cachefile!=cf);
	}
    }
}


static int cachefile_flush_and_remove (CACHEFILE cf) {
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
		p=p->hash_chain;
	    }
	}
    }
    assert_cachefile_is_flushed_and_removed(cf);

    if (4 * t->n_in_table < t->table_size)
        cachetable_rehash(t, -1);

    return 0;
}

/* Require that it all be flushed. */
int toku_cachetable_close (CACHETABLE *tp) {
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

int toku_cachetable_remove (CACHEFILE cachefile, CACHEKEY key, int write_me) {
    /* Removing something already present is OK. */
    CACHETABLE t = cachefile->cachetable;
    PAIR p;
    for (p=t->table[hashit(t,key)]; p; p=p->hash_chain) {
	if (p->key==key && p->cachefile==cachefile) {
	    flush_and_remove(t, p, write_me);
            if (4 * t->n_in_table < t->table_size)
                cachetable_rehash(t, -1);
	    goto done;
	}
    }
 done:
    return 0;
}

#if 0
static void flush_and_keep (PAIR flush_me) {
    if (flush_me->dirty) {
	WHEN_TRACE_CT(printf("%s:%d CT flush_callback(%lld, %p, dirty=1, 0)\n", __FILE__, __LINE__, flush_me->key, flush_me->value)); 
        flush_me->flush_callback(flush_me->cachefile, flush_me->key, flush_me->value, flush_me->size, 1, 1);
	flush_me->dirty=0;
    }
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
#endif

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

int toku_cachefile_fd (CACHEFILE cf) {
    return cf->fd;
}

/* debug functions */

void toku_cachetable_print_state (CACHETABLE ct) {
     int i;
     for (i=0; i<ct->table_size; i++) {
         PAIR p = ct->table[i];
         if (p != 0) {
             printf("t[%d]=", i);
             for (p=ct->table[i]; p; p=p->hash_chain) {
                 printf(" {%lld, %p, dirty=%d, pin=%lld, size=%ld}", p->key, p->cachefile, p->dirty, p->pinned, p->size);
             }
             printf("\n");
         }
     }
 }

void toku_cachetable_get_state (CACHETABLE ct, int *num_entries_ptr, int *hash_size_ptr, long *size_current_ptr, long *size_limit_ptr) {
    if (num_entries_ptr) 
        *num_entries_ptr = ct->n_in_table;
    if (hash_size_ptr)
        *hash_size_ptr = ct->table_size;
    if (size_current_ptr)
        *size_current_ptr = ct->size_current;
    if (size_limit_ptr)
        *size_limit_ptr = ct->size_limit;
}

int toku_cachetable_get_key_state (CACHETABLE ct, CACHEKEY key, void **value_ptr,
				   int *dirty_ptr, long long *pin_ptr, long *size_ptr) {
    PAIR p;
    for (p = ct->table[hashit(ct, key)]; p; p = p->hash_chain) {
        if (p->key == key) {
            if (value_ptr)
                *value_ptr = p->value;
            if (dirty_ptr)
                *dirty_ptr = p->dirty;
            if (pin_ptr)
                *pin_ptr = p->pinned;
            if (size_ptr)
                *size_ptr = p->size;
            return 0;
        }
    }
    return 1;
}

int toku_cachetable_checkpoint (CACHETABLE ct) {
    // Single threaded checkpoint.
    // In future: for multithreaded checkpoint we should not proceed if the previous checkpoint has not finished.
    // Requires: Everything is unpinned.  (In the multithreaded version we have to wait for things to get unpinned and then
    //  grab them (or else the unpinner has to do something.)
    // Algorithm:  Write a checkpoint record to the log, noting the LSN of that record.
    //  Note the LSN of the previous checkpoint (stored in lsn_of_checkpoint)
    //  For every (unpinnned) dirty node in which the LSN is newer than the prev checkpoint LSN:
    //      flush the node (giving it a new nodeid, and fixing up the downpointer in the parent)
    // Watch out since evicting the node modifies the hash table.

//?? This is a skeleton.  It compiles, but doesn't do anything reasonable yet.
//??    log_the_checkpoint();
    int n_saved=0;
    int n_in_table = ct->n_in_table;
    struct save_something {
	CACHEFILE cf;
	DISKOFF   key;
	void     *value;
	long      size;
	LSN       modified_lsn;
	CACHETABLE_FLUSH_FUNC_T flush_callback;
    } *MALLOC_N(n_in_table, info);
    {
	PAIR pair;
	for (pair=ct->head; pair; pair=pair->next) {
	    assert(!pair->pinned);
	    if (pair->dirty && pair->modified_lsn.lsn>ct->lsn_of_checkpoint.lsn) {
//??		/save_something_about_the_pair(); // This read-only so it doesn't modify the table.
		n_saved++;
	    }
	}
    }
    {
	int i;
	for (i=0; i<n_saved; i++) {
	    info[i].flush_callback(info[i].cf, info[i].key, info[i].value, info[i].size, 1, 1, info[i].modified_lsn, 0);
	}
    }
    toku_free(info);
    return 0;
}

TOKULOGGER toku_cachefile_logger (CACHEFILE cf) {
    return cf->cachetable->logger;
}

FILENUM toku_cachefile_filenum (CACHEFILE cf) {
    return cf->filenum;
}
