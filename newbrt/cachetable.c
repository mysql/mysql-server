/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "includes.h"

// execute the cachetable callbacks using a writer thread 0->no 1->yes
#define DO_WRITER_THREAD 1
#if DO_WRITER_THREAD
static void *cachetable_writer(void *);
#endif

// we use 4 threads since gunzip is 4 times faster than gzip
#define MAX_WRITER_THREADS 4

// use cachetable locks 0->no 1->yes
#define DO_CACHETABLE_LOCK 1

// unlock the cachetable while executing callbacks 0->no 1->yes
#define DO_CALLBACK_UNLOCK 1

// simulate long latency write operations with usleep. time in milliseconds.
#define DO_CALLBACK_USLEEP 0
#define DO_CALLBACK_BUSYWAIT 0

//#define TRACE_CACHETABLE
#ifdef TRACE_CACHETABLE
#define WHEN_TRACE_CT(x) x
#else
#define WHEN_TRACE_CT(x) ((void)0)
#endif

typedef struct ctpair *PAIR;
struct ctpair {
    enum typ_tag tag;
    char     dirty;
    char     verify_flag;         // Used in verify_cachetable()
    char     writing;             // writing back
    char     write_me;
    CACHEKEY key;
    void    *value;
    long     size;
    PAIR     next,prev;           // In LRU list.
    PAIR     hash_chain;
    CACHEFILE cachefile;
    CACHETABLE_FLUSH_CALLBACK flush_callback;
    CACHETABLE_FETCH_CALLBACK fetch_callback;
    void    *extraargs;
    LSN      modified_lsn;       // What was the LSN when modified (undefined if not dirty)
    LSN      written_lsn;        // What was the LSN when written (we need to get this information when we fetch)
    u_int32_t fullhash;

    PAIR     next_wq;            // the ctpair's are linked into a write queue when evicted
    struct ctpair_rwlock rwlock; // reader writer lock used to grant an exclusive lock to the writeback thread
    struct writequeue *cq;       // writers sometimes return ctpair's using this queue
};

#include "cachetable-writequeue.h"

static inline void ctpair_destroy(PAIR p) {
    ctpair_rwlock_destroy(&p->rwlock);
    toku_free(p);
}

// The cachetable is as close to an ENV as we get.
struct cachetable {
    enum typ_tag tag;
    u_int32_t n_in_table;
    u_int32_t table_size;
    PAIR *table;            // hash table
    PAIR  head,tail;        // of LRU list. head is the most recently used. tail is least recently used.
    CACHEFILE cachefiles;   // list of cachefiles that use this cachetable
    long size_current;      // the sum of the sizes of the pairs in the cachetable
    long size_limit;        // the limit to the sum of the pair sizes
    long size_writing;      // the sum of the sizes of the pairs being written
    LSN lsn_of_checkpoint;  // the most recent checkpoint in the log.
    TOKULOGGER logger;
    pthread_mutex_t mutex;  // coarse lock that protects the cachetable, the cachefiles, and the pair's
    struct writequeue wq;   // write queue for the writer threads
    THREADPOOL threadpool;  // pool of writer threads
    char checkpointing;     // checkpoint in progress
};

// lock the cachetable mutex

static inline void cachetable_lock(CACHETABLE ct __attribute__((unused))) {
#if DO_CACHETABLE_LOCK
    int r = pthread_mutex_lock(&ct->mutex); assert(r == 0);
#endif
}

// unlock the cachetable mutex

static inline void cachetable_unlock(CACHETABLE ct __attribute__((unused))) {
#if DO_CACHETABLE_LOCK
    int r = pthread_mutex_unlock(&ct->mutex); assert(r == 0);
#endif
}

// wait for writes to complete if the size in the write queue is 1/2 of 
// the cachetable

static inline void cachetable_wait_write(CACHETABLE ct) {
    while (2*ct->size_writing > ct->size_current) {
        writequeue_wait_write(&ct->wq, &ct->mutex);
    }
}

struct fileid {
    dev_t st_dev; /* device and inode are enough to uniquely identify a file in unix. */
    ino_t st_ino;
};

struct cachefile {
    CACHEFILE next;
    u_int64_t refcount; /* CACHEFILEs are shared. Use a refcount to decide when to really close it.
			 * The reference count is one for every open DB.
			 * Plus one for every commit/rollback record.  (It would be harder to keep a count for every open transaction,
			 * because then we'd have to figure out if the transaction was already counted.  If we simply use a count for
			 * every record in the transaction, we'll be ok.  Hence we use a 64-bit counter to make sure we don't run out.
			 */
    int fd;       /* Bug: If a file is opened read-only, then it is stuck in read-only.  If it is opened read-write, then subsequent writers can write to it too. */
    CACHETABLE cachetable;
    struct fileid fileid;
    FILENUM filenum;
    char *fname;

    void *userdata;
    int (*close_userdata)(CACHEFILE cf, void *userdata); // when closing the last reference to a cachefile, first call this function.
};

int toku_create_cachetable(CACHETABLE *result, long size_limit, LSN initial_lsn, TOKULOGGER logger) {
    {
	static int did_mallopt = 0;
	if (!did_mallopt) {
	    mallopt(M_MMAP_THRESHOLD, 1024*64); // 64K and larger should be malloced with mmap().
	    did_mallopt = 1;
	}
    }
    TAGMALLOC(CACHETABLE, t);
    if (t == 0) return ENOMEM;
    t->n_in_table = 0;
    t->table_size = 4;
    MALLOC_N(t->table_size, t->table);
    assert(t->table);
    t->head = t->tail = 0;
    u_int32_t i;
    for (i=0; i<t->table_size; i++) {
	t->table[i]=0;
    }
    t->cachefiles = 0;
    t->size_current = 0;
    t->size_limit = size_limit;
    t->size_writing = 0;
    t->lsn_of_checkpoint = initial_lsn;
    t->logger = logger;
    t->checkpointing = 0;
    int r;
    writequeue_init(&t->wq);
    r = pthread_mutex_init(&t->mutex, 0); assert(r == 0);

    // set the max number of writeback threads to min(MAX_WRITER_THREADS,nprocs_online)
    int nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    if (nprocs > MAX_WRITER_THREADS) nprocs = MAX_WRITER_THREADS;
    r = threadpool_create(&t->threadpool, nprocs); assert(r == 0);

#if DO_WRITER_THREAD
    threadpool_maybe_add(t->threadpool, cachetable_writer, t);
#endif
    *result = t;
    return 0;
}

// What cachefile goes with particular fd?
int toku_cachefile_of_filenum (CACHETABLE t, FILENUM filenum, CACHEFILE *cf) {
    CACHEFILE extant;
    for (extant = t->cachefiles; extant; extant=extant->next) {
	if (extant->filenum.fileid==filenum.fileid) {
	    *cf = extant;
	    return 0;
	}
    }
    return ENOENT;
}

static FILENUM next_filenum_to_use={0};

static void cachefile_init_filenum(CACHEFILE newcf, int fd, const char *fname, struct fileid fileid) \
{
    newcf->fd = fd;
    newcf->fileid = fileid;
    newcf->fname  = fname ? toku_strdup(fname) : 0;
}

// If something goes wrong, close the fd.  After this, the caller shouldn't close the fd, but instead should close the cachefile.
int toku_cachetable_openfd (CACHEFILE *cf, CACHETABLE t, int fd, const char *fname) {
    int r;
    CACHEFILE extant;
    struct stat statbuf;
    struct fileid fileid;
    memset(&fileid, 0, sizeof(fileid));
    r=fstat(fd, &statbuf);
    if (r != 0) { r=errno; close(fd); }
    fileid.st_dev = statbuf.st_dev;
    fileid.st_ino = statbuf.st_ino;
    for (extant = t->cachefiles; extant; extant=extant->next) {
	if (memcmp(&extant->fileid, &fileid, sizeof(fileid))==0) {
	    r = close(fd);
            assert(r == 0);
	    extant->refcount++;
	    *cf = extant;
	    return 0;
	}
    }
 try_again:
    for (extant = t->cachefiles; extant; extant=extant->next) {
	if (next_filenum_to_use.fileid==extant->filenum.fileid) {
	    next_filenum_to_use.fileid++;
	    goto try_again;
	}
    }
    {
	CACHEFILE MALLOC(newcf);
        newcf->cachetable = t;
        newcf->filenum.fileid = next_filenum_to_use.fileid++;
        cachefile_init_filenum(newcf, fd, fname, fileid);
	newcf->refcount = 1;
	newcf->next = t->cachefiles;
	t->cachefiles = newcf;

	newcf->userdata = 0;
	newcf->close_userdata = 0;

	*cf = newcf;
	return 0;
    }
}

int toku_cachetable_openf (CACHEFILE *cf, CACHETABLE t, const char *fname, int flags, mode_t mode) {
    int fd = open(fname, flags, mode);
    if (fd<0) return errno;
    return toku_cachetable_openfd (cf, t, fd, fname);
}

int toku_cachefile_set_fd (CACHEFILE cf, int fd, const char *fname) {
    int r;
    struct stat statbuf;
    r=fstat(fd, &statbuf);
    if (r != 0) { 
        r=errno; close(fd); return r; 
    }
    if (cf->close_userdata && (r = cf->close_userdata(cf, cf->userdata))) {
        return r;
    }
    cf->close_userdata = NULL;
    cf->userdata = NULL;

    close(cf->fd);
    cf->fd = -1;
    if (cf->fname) {
        toku_free(cf->fname);
        cf->fname = 0;
    }
    struct fileid fileid;
    memset(&fileid, 0, sizeof fileid);
    fileid.st_dev = statbuf.st_dev;
    fileid.st_ino = statbuf.st_ino;
    cachefile_init_filenum(cf, fd, fname, fileid);
    return 0;
}

int toku_cachefile_fd (CACHEFILE cf) {
    return cf->fd;
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

static int cachefile_write_maybe_remove (CACHETABLE, CACHEFILE cf, BOOL do_remove);

// Increment the reference count
void toku_cachefile_refup (CACHEFILE cf) {
    cf->refcount++;
}

int toku_cachefile_close (CACHEFILE *cfp, TOKULOGGER logger) {
    CACHEFILE cf = *cfp;
    CACHETABLE ct = cf->cachetable;
    cachetable_lock(ct);
    assert(cf->refcount>0);
    cf->refcount--;
    if (cf->refcount==0) {
	int r;
	if ((r = cachefile_write_maybe_remove(ct, cf, TRUE))) {
            cachetable_unlock(ct);
            return r;
        }
	if (cf->close_userdata && (r = cf->close_userdata(cf, cf->userdata))) {
	    cachetable_unlock(ct);
	    return r;
	}
	cf->close_userdata = NULL;
	cf->userdata = NULL;
        cf->cachetable->cachefiles = remove_cf_from_list(cf, cf->cachetable->cachefiles);
        cachetable_unlock(ct);
	r = close(cf->fd);
	assert(r == 0);
        cf->fd = -1;
	if (logger) {
	    //assert(cf->fname);
	    //BYTESTRING bs = {.len=strlen(cf->fname), .data=cf->fname};
	    //r = toku_log_cfclose(logger, 0, 0, bs, cf->filenum);
	}
	if (cf->fname)
	    toku_free(cf->fname);
	toku_free(cf);
	*cfp=0;
	return r;
    } else {
        cachetable_unlock(ct);
	*cfp=0;
	return 0;
    }
}

int toku_cachefile_flush (CACHEFILE cf) {
    CACHETABLE ct = cf->cachetable;
    cachetable_lock(ct);
    int r = cachefile_write_maybe_remove(ct, cf, TRUE);
    cachetable_unlock(ct);
    return r;
}

// This hash function comes from Jenkins:  http://burtleburtle.net/bob/c/lookup3.c
// The idea here is to mix the bits thoroughly so that we don't have to do modulo by a prime number.
// Instead we can use a bitmask on a table of size power of two.
// This hash function does yield improved performance on ./db-benchmark-test-tokudb and ./scanscan
static inline u_int32_t rot(u_int32_t x, u_int32_t k) {
    return (x<<k) | (x>>(32-k));
}
static inline u_int32_t final (u_int32_t a, u_int32_t b, u_int32_t c) {
    c ^= b; c -= rot(b,14);
    a ^= c; a -= rot(c,11);
    b ^= a; b -= rot(a,25);
    c ^= b; c -= rot(b,16);
    a ^= c; a -= rot(c,4); 
    b ^= a; b -= rot(a,14);
    c ^= b; c -= rot(b,24);
    return c;
}

u_int32_t toku_cachetable_hash (CACHEFILE cachefile, BLOCKNUM key)
// Effect: Return a 32-bit hash key.  The hash key shall be suitable for using with bitmasking for a table of size power-of-two.
{
    return final(cachefile->filenum.fileid, (u_int32_t)(key.b>>32), (u_int32_t)key.b);
}

#if 0
static unsigned int hashit (CACHETABLE t, CACHEKEY key, CACHEFILE cachefile) {
    assert(0==(t->table_size & (t->table_size -1))); // make sure table is power of two
    return (toku_cachetable_hash(key,cachefile))&(t->table_size-1);
}
#endif

static void cachetable_rehash (CACHETABLE t, u_int32_t newtable_size) {
    // printf("rehash %p %d %d %d\n", t, primeindexdelta, t->n_in_table, t->table_size);

    assert(newtable_size>=4 && ((newtable_size & (newtable_size-1))==0));
    PAIR *newtable = toku_calloc(newtable_size, sizeof(*t->table));
    u_int32_t i;
    //printf("%s:%d newtable_size=%d\n", __FILE__, __LINE__, newtable_size);
    assert(newtable!=0);
    u_int32_t oldtable_size = t->table_size;
    t->table_size=newtable_size;
    for (i=0; i<newtable_size; i++) newtable[i]=0;
    for (i=0; i<oldtable_size; i++) {
	PAIR p;
	while ((p=t->table[i])!=0) {
	    unsigned int h = p->fullhash&(newtable_size-1);
	    t->table[i] = p->hash_chain;
	    p->hash_chain = newtable[h];
	    newtable[h] = p;
	}
    }
    toku_free(t->table);
    // printf("Freed\n");
    t->table=newtable;
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
    return (BOOL)(p->dirty
		  && p->modified_lsn.lsn>=t->lsn_of_checkpoint.lsn   // nonstrict
		  && p->written_lsn.lsn < t->lsn_of_checkpoint.lsn); // strict
}

// Remove a pair from the cachetable
// Effects: the pair is removed from the LRU list and from the cachetable's hash table.
// The size of the objects in the cachetable is adjusted by the size of the pair being
// removed.

static void cachetable_remove_pair (CACHETABLE ct, PAIR p) {
    lru_remove(ct, p);

    assert(ct->n_in_table>0);
    ct->n_in_table--;
    // Remove it from the hash chain.
    {
	unsigned int h = p->fullhash&(ct->table_size-1);
	ct->table[h] = remove_from_hash_chain (p, ct->table[h]);
    }
    ct->size_current -= p->size; assert(ct->size_current >= 0);
}

// Maybe remove a pair from the cachetable and free it, depending on whether
// or not there are any threads interested in the pair.  The flush callback
// is called with write_me and keep_me both false, and the pair is destroyed.

static void cachetable_maybe_remove_and_free_pair (CACHETABLE ct, PAIR p) {
    if (ctpair_users(&p->rwlock) == 0) {
        cachetable_remove_pair(ct, p);
#if DO_CALLBACK_UNLOCK
        cachetable_unlock(ct);
#endif
        p->flush_callback(p->cachefile, p->key, p->value, p->extraargs, p->size, FALSE, FALSE, 
                          ct->lsn_of_checkpoint, need_to_rename_p(ct, p));
        ctpair_destroy(p);
#if DO_CALLBACK_UNLOCK
        cachetable_lock(ct);
#endif
    }
}

static void cachetable_complete_write_pair (CACHETABLE ct, PAIR p, BOOL do_remove);

// Write a pair to storage
// Effects: an exclusive lock on the pair is obtained, the write callback is called,
// the pair dirty state is adjusted, and the write is completed.  The write_me boolean
// is true when the pair is dirty and the pair is requested to be written.  The keep_me
// boolean is true, so the pair is not yet evicted from the cachetable.

static void cachetable_write_pair(CACHETABLE ct, PAIR p) {
    ctpair_write_lock(&p->rwlock, &ct->mutex);
#if DO_CALLBACK_UNLOCK
    cachetable_unlock(ct);
#endif
    // write callback
    p->flush_callback(p->cachefile, p->key, p->value, p->extraargs, p->size, (BOOL)(p->dirty && p->write_me), TRUE,
                      ct->lsn_of_checkpoint, need_to_rename_p(ct, p));
#if DO_CALLBACK_USLEEP
    usleep(DO_CALLBACK_USLEEP);
#endif
#if DO_CALLBACK_BUSYWAIT
    struct timeval tstart;
    gettimeofday(&tstart, 0);
    long long ltstart = tstart.tv_sec * 1000000 + tstart.tv_usec;
    while (1) {
        struct timeval t;
        gettimeofday(&t, 0);
        long long lt = t.tv_sec * 1000000 + t.tv_usec;
        if (lt - ltstart > DO_CALLBACK_BUSYWAIT)
            break;
    }
#endif
#if DO_CALLBACK_UNLOCK
    cachetable_lock(ct);
#endif

    // the pair is no longer dirty once written
    if (p->dirty && p->write_me)
        p->dirty = FALSE;

    // stuff it into a completion queue for delayed completion if a completion queue exists
    // otherwise complete the write now
    if (p->cq)
        writequeue_enq(p->cq, p);
    else
        cachetable_complete_write_pair(ct, p, TRUE);
}

// complete the write of a pair by reseting the writing flag, adjusting the write
// pending size, and maybe removing the pair from the cachetable if there are no
// references to it

static void cachetable_complete_write_pair (CACHETABLE ct, PAIR p, BOOL do_remove) {
    p->cq = 0;
    p->writing = 0;

    // maybe wakeup any stalled writers when the pending writes fall below 
    // 1/8 of the size of the cachetable
    ct->size_writing -= p->size; 
    assert(ct->size_writing >= 0);
    if (8*ct->size_writing <= ct->size_current)
        writequeue_wakeup_write(&ct->wq);

    ctpair_write_unlock(&p->rwlock);
    if (do_remove)
        cachetable_maybe_remove_and_free_pair(ct, p);
}

// flush and remove a pair from the cachetable.  the callbacks are run by a thread in
// a thread pool.

static void flush_and_remove (CACHETABLE ct, PAIR p, int write_me) {
    p->writing = 1;
    ct->size_writing += p->size; assert(ct->size_writing >= 0);
    p->write_me = (char)(write_me?1:0);
#if DO_WRITER_THREAD
    if (!p->dirty || !p->write_me) {
        // evictions without a write can be run in the current thread
        cachetable_write_pair(ct, p);
    } else {
        threadpool_maybe_add(ct->threadpool, cachetable_writer, ct);
        writequeue_enq(&ct->wq, p);
    }
#else
    cachetable_write_pair(ct, p);
#endif
}

static unsigned long toku_maxrss=0;

unsigned long toku_get_maxrss(void) {
    return toku_maxrss;
}

static unsigned long check_maxrss (void) __attribute__((__unused__));
static unsigned long check_maxrss (void) {
    pid_t pid = getpid();
    char fname[100];
    snprintf(fname, sizeof(fname), "/proc/%d/statm", pid);
    FILE *f = fopen(fname, "r");
    unsigned long ignore, rss;
    fscanf(f, "%lu %lu", &ignore, &rss);
    fclose(f);
    if (toku_maxrss<rss) toku_maxrss=rss;
    return rss;
}


static int maybe_flush_some (CACHETABLE t, long size) {
    int r = 0;
again:
    if (size + t->size_current > t->size_limit + t->size_writing) {
	{
	    unsigned long rss __attribute__((__unused__)) = check_maxrss();
	    //printf("this-size=%.6fMB projected size = %.2fMB  limit=%2.fMB  rss=%2.fMB\n", size/(1024.0*1024.0), (size+t->size_current)/(1024.0*1024.0), t->size_limit/(1024.0*1024.0), rss/256.0);
	    //struct mallinfo m = mallinfo();
	    //printf(" arena=%d hblks=%d hblkhd=%d\n", m.arena, m.hblks, m.hblkhd);
	}
        /* Try to remove one. */
	PAIR remove_me;
	for (remove_me = t->tail; remove_me; remove_me = remove_me->prev) {
	    if (!ctpair_users(&remove_me->rwlock) && !remove_me->writing) {
		flush_and_remove(t, remove_me, 1);
		goto again;
	    }
	}
	/* All were pinned. */
	//printf("All are pinned\n");
	return 0; // Don't indicate an error code.  Instead let memory get overfull.
    }

    if ((4 * t->n_in_table < t->table_size) && t->table_size > 4)
        cachetable_rehash(t, t->table_size/2);

    return r;
}

static int cachetable_insert_at(CACHEFILE cachefile, u_int32_t fullhash, CACHEKEY key, void *value, long size,
                                CACHETABLE_FLUSH_CALLBACK flush_callback,
                                CACHETABLE_FETCH_CALLBACK fetch_callback,
                                void *extraargs, int dirty,
				LSN   written_lsn) {
    TAGMALLOC(PAIR, p);
    memset(p, 0, sizeof *p);
    ctpair_rwlock_init(&p->rwlock);
    p->fullhash = fullhash;
    p->dirty = (char)(dirty ? 1 : 0);           //printf("%s:%d p=%p dirty=%d\n", __FILE__, __LINE__, p, p->dirty);
    p->size = size;
    p->writing = 0;
    p->key = key;
    p->value = value;
    p->next = p->prev = 0;
    p->cachefile = cachefile;
    p->flush_callback = flush_callback;
    p->fetch_callback = fetch_callback;
    p->extraargs = extraargs;
    p->modified_lsn.lsn = 0;
    p->written_lsn  = written_lsn;
    p->fullhash = fullhash;
    CACHETABLE ct = cachefile->cachetable;
    ctpair_read_lock(&p->rwlock, &ct->mutex);
    p->cq = 0;
    lru_add_to_list(ct, p);
    u_int32_t h = fullhash & (ct->table_size-1);
    p->hash_chain = ct->table[h];
    ct->table[h] = p;
    ct->n_in_table++;
    ct->size_current += size;
    if (ct->n_in_table > ct->table_size) {
        cachetable_rehash(ct, ct->table_size*2);
    }
    return 0;
}

enum { hash_histogram_max = 100 };
static unsigned long long hash_histogram[hash_histogram_max];
void print_hash_histogram (void) {
    int i;
    for (i=0; i<hash_histogram_max; i++)
	if (hash_histogram[i]) printf("%d:%llu ", i, hash_histogram[i]);
    printf("\n");
}

static void
note_hash_count (int count) {
    if (count>=hash_histogram_max) count=hash_histogram_max-1;
    hash_histogram[count]++;
}

int toku_cachetable_put(CACHEFILE cachefile, CACHEKEY key, u_int32_t fullhash, void*value, long size,
			CACHETABLE_FLUSH_CALLBACK flush_callback, 
                        CACHETABLE_FETCH_CALLBACK fetch_callback, void *extraargs) {
    WHEN_TRACE_CT(printf("%s:%d CT cachetable_put(%lld)=%p\n", __FILE__, __LINE__, key, value));
    CACHETABLE ct = cachefile->cachetable;
    int count=0;
    cachetable_lock(ct);
    cachetable_wait_write(ct);
    {
	PAIR p;
	for (p=ct->table[fullhash&(cachefile->cachetable->table_size-1)]; p; p=p->hash_chain) {
	    count++;
	    if (p->key.b==key.b && p->cachefile==cachefile) {
		// Semantically, these two asserts are not strictly right.  After all, when are two functions eq?
		// In practice, the functions better be the same.
		assert(p->flush_callback==flush_callback);
		assert(p->fetch_callback==fetch_callback);
                ctpair_read_lock(&p->rwlock, &ct->mutex);
                cachetable_unlock(ct);
		note_hash_count(count);
		return -1; /* Already present. */
	    }
	}
    }
    int r;
    if ((r=maybe_flush_some(ct, size))) {
        cachetable_unlock(ct);
        return r;
    }
    // flushing could change the table size, but wont' change the fullhash
    r = cachetable_insert_at(cachefile, fullhash, key, value, size, flush_callback, fetch_callback, extraargs, 1, ZERO_LSN);
    cachetable_unlock(ct);
    note_hash_count(count);
    return r;
}

int toku_cachetable_get_and_pin(CACHEFILE cachefile, CACHEKEY key, u_int32_t fullhash, void**value, long *sizep,
			        CACHETABLE_FLUSH_CALLBACK flush_callback, 
                                CACHETABLE_FETCH_CALLBACK fetch_callback, void *extraargs) {
    CACHETABLE t = cachefile->cachetable;
    PAIR p;
    int count=0;
    cachetable_lock(t);
    cachetable_wait_write(t);
    for (p=t->table[fullhash&(t->table_size-1)]; p; p=p->hash_chain) {
	count++;
	if (p->key.b==key.b && p->cachefile==cachefile) {
	    *value = p->value;
            if (sizep) *sizep = p->size;
            ctpair_read_lock(&p->rwlock, &t->mutex);
	    lru_touch(t,p);
            cachetable_unlock(t);
	    note_hash_count(count);
	    WHEN_TRACE_CT(printf("%s:%d cachtable_get_and_pin(%lld)--> %p\n", __FILE__, __LINE__, key, *value));
	    return 0;
	}
    }
    note_hash_count(count);
    int r;
    // Note.  hashit(t,key) may have changed as a result of flushing.  But fullhash won't have changed.
    {
	void *toku_value; 
        long size = 1; // compat
	LSN written_lsn;
	WHEN_TRACE_CT(printf("%s:%d CT: fetch_callback(%lld...)\n", __FILE__, __LINE__, key));
	if ((r=fetch_callback(cachefile, key, fullhash, &toku_value, &size, extraargs, &written_lsn))) {
            cachetable_unlock(t);
            return r;
	}
	cachetable_insert_at(cachefile, fullhash, key, toku_value, size, flush_callback, fetch_callback, extraargs, 0, written_lsn);
	*value = toku_value;
        if (sizep)
            *sizep = size;
    }
    r = maybe_flush_some(t, 0);
    cachetable_unlock(t);
    WHEN_TRACE_CT(printf("%s:%d did fetch: cachtable_get_and_pin(%lld)--> %p\n", __FILE__, __LINE__, key, *value));
    return r;
}

// Lookup a key in the cachetable.  If it is found and it is not being written, then
// acquire a read lock on the pair, update the LRU list, and return sucess.  However,
// if it is being written, then allow the writer to evict it.  This prevents writers
// being suspended on a block that was just selected for eviction.
int toku_cachetable_maybe_get_and_pin (CACHEFILE cachefile, CACHEKEY key, u_int32_t fullhash, void**value) {
    CACHETABLE t = cachefile->cachetable;
    PAIR p;
    int count = 0;
    cachetable_lock(t);
    for (p=t->table[fullhash&(t->table_size-1)]; p; p=p->hash_chain) {
	count++;
	if (p->key.b==key.b && p->cachefile==cachefile && !p->writing) {
	    *value = p->value;
	    ctpair_read_lock(&p->rwlock, &t->mutex);
	    lru_touch(t,p);
            cachetable_unlock(t);
	    note_hash_count(count);
	    //printf("%s:%d cachetable_maybe_get_and_pin(%lld)--> %p\n", __FILE__, __LINE__, key, *value);
	    return 0;
	}
    }
    cachetable_unlock(t);
    note_hash_count(count);
    return -1;
}


int toku_cachetable_unpin(CACHEFILE cachefile, CACHEKEY key, u_int32_t fullhash, int dirty, long size) {
    CACHETABLE t = cachefile->cachetable;
    PAIR p;
    WHEN_TRACE_CT(printf("%s:%d unpin(%lld)", __FILE__, __LINE__, key));
    //printf("%s:%d is dirty now=%d\n", __FILE__, __LINE__, dirty);
    int count = 0;
    //assert(fullhash == toku_cachetable_hash(cachefile, key));
    cachetable_lock(t);
    for (p=t->table[fullhash&(t->table_size-1)]; p; p=p->hash_chain) {
	count++;
	if (p->key.b==key.b && p->cachefile==cachefile) {
	    assert(p->rwlock.pinned>0);
            ctpair_read_unlock(&p->rwlock);
	    if (dirty) p->dirty = TRUE;
            if (size != 0) {
                t->size_current -= p->size; if (p->writing) t->size_writing -= p->size;
                p->size = size;
                t->size_current += p->size; if (p->writing) t->size_writing += p->size;
            }
	    WHEN_TRACE_CT(printf("[count=%lld]\n", p->pinned));
	    {
		int r;
		if ((r=maybe_flush_some(t, 0))) {
                    cachetable_unlock(t);
                    return r;
                }
	    }
            cachetable_unlock(t);
	    note_hash_count(count);
	    return 0;
	}
    }
    cachetable_unlock(t);
    note_hash_count(count);
    return -1;
}

// effect:   Move an object from one key to another key.
// requires: The object is pinned in the table
int toku_cachetable_rename (CACHEFILE cachefile, CACHEKEY oldkey, CACHEKEY newkey) {
    CACHETABLE t = cachefile->cachetable;
    PAIR *ptr_to_p,p;
    int count = 0;
    u_int32_t fullhash = toku_cachetable_hash(cachefile, oldkey);
    cachetable_lock(t);
    for (ptr_to_p = &t->table[fullhash&(t->table_size-1)],  p = *ptr_to_p;
         p;
         ptr_to_p = &p->hash_chain,                p = *ptr_to_p) {
        count++;
        if (p->key.b==oldkey.b && p->cachefile==cachefile) {
            note_hash_count(count);
            *ptr_to_p = p->hash_chain;
            p->key = newkey;
            u_int32_t new_fullhash = toku_cachetable_hash(cachefile, newkey);
            u_int32_t nh = new_fullhash&(t->table_size-1);
            p->fullhash = new_fullhash;
            p->hash_chain = t->table[nh];
            t->table[nh] = p;
            cachetable_unlock(t);
            return 0;
        }
    }
    cachetable_unlock(t);
    note_hash_count(count);
    return -1;
}

void toku_cachefile_verify (CACHEFILE cf) {
    toku_cachetable_verify(cf->cachetable);
}

void toku_cachetable_verify (CACHETABLE t) {
    cachetable_lock(t);

    // First clear all the verify flags by going through the hash chains
    {
	u_int32_t i;
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
	    u_int32_t fullhash = p->fullhash;
	    //assert(fullhash==toku_cachetable_hash(p->cachefile, p->key));
	    for (p2=t->table[fullhash&(t->table_size-1)]; p2; p2=p2->hash_chain) {
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
	u_int32_t i;
	for (i=0; i<t->table_size; i++) {
	    PAIR p;
	    for (p=t->table[i]; p; p=p->hash_chain) {
		assert(p->verify_flag);
	    }
	}
    }

    cachetable_unlock(t);
}

static void assert_cachefile_is_flushed_and_removed (CACHETABLE t, CACHEFILE cf) {
    u_int32_t i;
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

// Write all of the pairs associated with a cachefile to storage.  Maybe remove
// these pairs from the cachetable after they have been written.

static int cachefile_write_maybe_remove(CACHETABLE ct, CACHEFILE cf, BOOL do_remove) {
    unsigned nfound = 0;
    struct writequeue cq;
    writequeue_init(&cq);
    unsigned i;
    for (i=0; i < ct->table_size; i++) {
	PAIR p;
	for (p = ct->table[i]; p; p = p->hash_chain) {
 	    if (cf == 0 || p->cachefile==cf) {
                nfound++;
                p->cq = &cq;
                if (!p->writing)
                    flush_and_remove(ct, p, 1);
	    }
	}
    }
    for (i=0; i<nfound; i++) {
        PAIR p = 0;
        int r = writequeue_deq(&cq, &ct->mutex, &p); assert(r == 0);
        cachetable_complete_write_pair(ct, p, do_remove);
    }
    writequeue_destroy(&cq);
    if (do_remove)
        assert_cachefile_is_flushed_and_removed(ct, cf);

    if ((4 * ct->n_in_table < ct->table_size) && (ct->table_size>4))
        cachetable_rehash(ct, ct->table_size/2);

    return 0;
}

/* Require that it all be flushed. */
int toku_cachetable_close (CACHETABLE *tp) {
    CACHETABLE t=*tp;
    int r;
    cachetable_lock(t);
    if ((r=cachefile_write_maybe_remove(t, 0, TRUE))) {
        cachetable_unlock(t);
        return r;
    }
    u_int32_t i;
    for (i=0; i<t->table_size; i++) {
	if (t->table[i]) return -1;
    }
    assert(t->size_writing == 0);
    writequeue_set_closed(&t->wq);
    cachetable_unlock(t);
    threadpool_destroy(&t->threadpool);
    writequeue_destroy(&t->wq);
    r = pthread_mutex_destroy(&t->mutex); assert(r == 0);
    toku_free(t->table);
    toku_free(t);
    *tp = 0;
    return 0;
}

#if 0
// this is broken. needs to wait for writebacks to complete
int toku_cachetable_remove (CACHEFILE cachefile, CACHEKEY key, int write_me) {
    /* Removing something already present is OK. */
    CACHETABLE t = cachefile->cachetable;
    PAIR p;
    int count = 0;
    u_int32_t fullhash = toku_cachetable_hash(cachefile, key);
    cachetable_lock(t);
    for (p=t->table[fullhash&(t->table_size-1)]; p; p=p->hash_chain) {
	count++;
	if (p->key.b==key.b && p->cachefile==cachefile) {
	    flush_and_remove(t, p, write_me);
            if ((4 * t->n_in_table < t->table_size) && (t->table_size>4))
                cachetable_rehash(t, t->table_size/2);
	    goto done;
	}
    }
 done:
    cachetable_unlock(t);
    note_hash_count(count);
    return 0;
}
#endif

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

int toku_cachetable_checkpoint (CACHETABLE ct) {
    // Requires: Everything is unpinned.  (In the multithreaded version we have to wait for things to get unpinned and then
    //  grab them (or else the unpinner has to do something.)
    // Algorithm:  Write a checkpoint record to the log, noting the LSN of that record.
    //  Note the LSN of the previous checkpoint (stored in lsn_of_checkpoint)
    //  For every (unpinnned) dirty node in which the LSN is newer than the prev checkpoint LSN:
    //      flush the node (giving it a new nodeid, and fixing up the downpointer in the parent)
    // Watch out since evicting the node modifies the hash table.

    //?? This is a skeleton.  It compiles, but doesn't do anything reasonable yet.
    //??    log_the_checkpoint();

    struct writequeue cq;
    writequeue_init(&cq);

    cachetable_lock(ct);
    
    // set the checkpoint in progress flag. if already set then just return.
    if (!ct->checkpointing) {
        ct->checkpointing = 1;
        
        unsigned nfound = 0;
        unsigned i;
        for (i=0; i < ct->table_size; i++) {
            PAIR p;
            for (p = ct->table[i]; p; p=p->hash_chain) {
                // p->dirty && p->modified_lsn.lsn>ct->lsn_of_checkpoint.lsn
                if (1) {
                    nfound++;
                    p->cq = &cq;
                    if (!p->writing)
                        flush_and_remove(ct, p, 1);
                }
            }
        }
        for (i=0; i<nfound; i++) {
            PAIR p = 0;
            int r = writequeue_deq(&cq, &ct->mutex, &p); assert(r == 0);
            cachetable_complete_write_pair(ct, p, FALSE);
        }

        ct->checkpointing = 0; // clear the checkpoint in progress flag
    }

    cachetable_unlock(ct);
    writequeue_destroy(&cq);

    return 0;
}

TOKULOGGER toku_cachefile_logger (CACHEFILE cf) {
    return cf->cachetable->logger;
}

FILENUM toku_cachefile_filenum (CACHEFILE cf) {
    return cf->filenum;
}

#if DO_WRITER_THREAD

// The writer thread waits for work in the write queue and writes the pair

static void *cachetable_writer(void *arg) {
    // printf("%lu:%s:start %p\n", pthread_self(), __FUNCTION__, arg);
    CACHETABLE ct = arg;
    int r;
    cachetable_lock(ct);
    while (1) {
        threadpool_set_thread_idle(ct->threadpool);
        PAIR p = 0;
        r = writequeue_deq(&ct->wq, &ct->mutex, &p);
        if (r != 0)
            break;
        threadpool_set_thread_busy(ct->threadpool);
        cachetable_write_pair(ct, p);
    }
    cachetable_unlock(ct);
    // printf("%lu:%s:exit %p\n", pthread_self(), __FUNCTION__, arg);
    return arg;
}

#endif

// debug functions

int toku_cachetable_assert_all_unpinned (CACHETABLE t) {
    u_int32_t i;
    int some_pinned=0;
    cachetable_lock(t);
    for (i=0; i<t->table_size; i++) {
	PAIR p;
	for (p=t->table[i]; p; p=p->hash_chain) {
	    assert(ctpair_pinned(&p->rwlock)>=0);
	    if (ctpair_pinned(&p->rwlock)) {
		printf("%s:%d pinned: %"PRId64" (%p)\n", __FILE__, __LINE__, p->key.b, p->value);
		some_pinned=1;
	    }
	}
    }
    cachetable_unlock(t);
    return some_pinned;
}

int toku_cachefile_count_pinned (CACHEFILE cf, int print_them) {
    u_int32_t i;
    int n_pinned=0;
    CACHETABLE t = cf->cachetable;
    cachetable_lock(t);
    for (i=0; i<t->table_size; i++) {
	PAIR p;
	for (p=t->table[i]; p; p=p->hash_chain) {
	    assert(ctpair_pinned(&p->rwlock)>=0);
	    if (ctpair_pinned(&p->rwlock) && (cf==0 || p->cachefile==cf)) {
		if (print_them) printf("%s:%d pinned: %"PRId64" (%p)\n", __FILE__, __LINE__, p->key.b, p->value);
		n_pinned++;
	    }
	}
    }
    cachetable_unlock(t);
    return n_pinned;
}

void toku_cachetable_print_state (CACHETABLE ct) {
    u_int32_t i;
    cachetable_lock(ct);
    for (i=0; i<ct->table_size; i++) {
        PAIR p = ct->table[i];
        if (p != 0) {
            printf("t[%u]=", i);
            for (p=ct->table[i]; p; p=p->hash_chain) {
                printf(" {%"PRId64", %p, dirty=%d, pin=%d, size=%ld}", p->key.b, p->cachefile, p->dirty, p->rwlock.pinned, p->size);
            }
            printf("\n");
        }
    }
    cachetable_unlock(ct);
}

void toku_cachetable_get_state (CACHETABLE ct, int *num_entries_ptr, int *hash_size_ptr, long *size_current_ptr, long *size_limit_ptr) {
    cachetable_lock(ct);
    if (num_entries_ptr) 
        *num_entries_ptr = ct->n_in_table;
    if (hash_size_ptr)
        *hash_size_ptr = ct->table_size;
    if (size_current_ptr)
        *size_current_ptr = ct->size_current;
    if (size_limit_ptr)
        *size_limit_ptr = ct->size_limit;
    cachetable_unlock(ct);
}

int toku_cachetable_get_key_state (CACHETABLE ct, CACHEKEY key, CACHEFILE cf, void **value_ptr,
				   int *dirty_ptr, long long *pin_ptr, long *size_ptr) {
    PAIR p;
    int count = 0;
    int r = -1;
    u_int32_t fullhash = toku_cachetable_hash(cf, key);
    cachetable_lock(ct);
    for (p = ct->table[fullhash&(ct->table_size-1)]; p; p = p->hash_chain) {
	count++;
        if (p->key.b == key.b && p->cachefile == cf) {
	    note_hash_count(count);
            if (value_ptr)
                *value_ptr = p->value;
            if (dirty_ptr)
                *dirty_ptr = p->dirty;
            if (pin_ptr)
                *pin_ptr = p->rwlock.pinned;
            if (size_ptr)
                *size_ptr = p->size;
            r = 0;
            break;
        }
    }
    cachetable_unlock(ct);
    note_hash_count(count);
    return r;
}

void toku_cachefile_set_userdata (CACHEFILE cf, void *userdata, int (*close_userdata)(CACHEFILE, void*)) {
    cf->userdata = userdata;
    cf->close_userdata = close_userdata;
}
void *toku_cachefile_get_userdata(CACHEFILE cf) {
    return cf->userdata;
}
