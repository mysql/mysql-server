/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

/* rollback and rollforward routines. */

#include <stdlib.h>
#include <inttypes.h>

#include "log_header.h"
#include "log-internal.h"
#include "cachetable.h"
#include "key.h"

//#define DO_VERIFY_COUNTS
#ifdef DO_VERIFY_COUNTS
#define VERIFY_COUNTS(n) toku_verify_counts(n)
#else
#define VERIFY_COUNTS(n) ((void)0)
#endif

static DB * const null_db=0;

// These data structures really should be part of a recovery data structure.  Recovery could be multithreaded (on different environments...)  But this is OK since recovery can only happen in one
static CACHETABLE ct;
static struct cf_pair {
    FILENUM filenum;
    CACHEFILE cf;
    BRT       brt; // set to zero on an fopen, but filled in when an fheader is seen.
} *cf_pairs;
static int n_cf_pairs=0, max_cf_pairs=0;;

int toku_recover_init (void) {
    int r = toku_create_cachetable(&ct, 1<<25, (LSN){0}, 0);
    return r;
}
void toku_recover_cleanup (void) {
    int i;
    for (i=0; i<n_cf_pairs; i++) {
	if (cf_pairs[i].brt) {
	    int r = toku_close_brt(cf_pairs[i].brt);
	    //r = toku_cachefile_close(&cf_pairs[i].cf);
	    assert(r==0);
	}
    }
    toku_free(cf_pairs);
    {
	int r = toku_cachetable_close(&ct);
	assert(r==0);
    }
}

int toku_recover_note_cachefile (FILENUM fnum, CACHEFILE cf) {
    if (max_cf_pairs==0) {
	n_cf_pairs=1;
	max_cf_pairs=2;
	MALLOC_N(max_cf_pairs, cf_pairs);
	if (cf_pairs==0) return errno;
    } else {
	if (n_cf_pairs>=max_cf_pairs) {
	    max_cf_pairs*=2;
	    cf_pairs = toku_realloc(cf_pairs, max_cf_pairs*sizeof(*cf_pairs));
	}
	n_cf_pairs++;
    }
    cf_pairs[n_cf_pairs-1].filenum = fnum;
    cf_pairs[n_cf_pairs-1].cf      = cf;
    cf_pairs[n_cf_pairs-1].brt     = 0;
    return 0;
}

static int find_cachefile (FILENUM fnum, struct cf_pair **cf_pair) {
    int i;
    for (i=0; i<n_cf_pairs; i++) {
	if (fnum.fileid==cf_pairs[i].filenum.fileid) {
	    *cf_pair = cf_pairs+i;
	    return 0;
	}
    }
    return 1;
}

static char *fixup_fname(BYTESTRING *f) {
    assert(f->len>0);
    char *fname = toku_malloc(f->len+1);
    memcpy(fname, f->data, f->len);
    fname[f->len]=0;
    return fname;
}


void toku_recover_commit (struct logtype_commit *c) {
    c=c; // !!! We need to do something, but for now we assume everything commits and so we don't have do anything to remember what commited and how to unroll.
}

// Rolling back a commit doesn't make much sense.  It won't happen during an abort.  But it doesn't hurt to ignore it.
int toku_rollback_commit (struct logtype_commit *le __attribute__((__unused__)), TOKUTXN txn __attribute__((__unused__))) {
    return 0;
}

#define ABORTIT { le=le; txn=txn; fprintf(stderr, "%s:%d (%s) not ready to go\n", __FILE__, __LINE__, __func__); abort(); }
int toku_rollback_delete (struct logtype_delete *le, TOKUTXN txn)               ABORTIT
void toku_recover_delete (struct logtype_delete *c) {c=c;fprintf(stderr, "%s:%d\n", __FILE__, __LINE__); abort(); }

void toku_recover_fcreate (struct logtype_fcreate *c) {
    char *fname = fixup_fname(&c->fname);
    int fd = creat(fname, c->mode);
    assert(fd>=0);
    toku_free(fname);
    toku_free(c->fname.data);
}

void toku_recover_fheader (struct logtype_fheader *c) {
    struct cf_pair *pair;
    int r = find_cachefile(c->filenum, &pair);
    assert(r==0);
    struct brt_header *MALLOC(h);
    assert(h);
    h->dirty=0;
    h->flags = c->header.flags;
    h->nodesize = c->header.nodesize;
    h->freelist = c->header.freelist;
    h->unused_memory = c->header.unused_memory;
    h->n_named_roots = c->header.n_named_roots;
    if ((signed)c->header.n_named_roots==-1) {
	h->unnamed_root = c->header.u.one.root;
    } else {
	assert(0);
    }
    toku_cachetable_put(pair->cf, 0, h, 0, toku_brtheader_flush_callback, toku_brtheader_fetch_callback, 0);
    if (pair->brt) {
	free(pair->brt->h);
    }  else {
	MALLOC(pair->brt);
	pair->brt->cf = pair->cf;
	pair->brt->database_name = 0; // Special case, we don't know or care what the database name is for recovery.
	pair->brt->cursors_head = pair->brt->cursors_tail = 0;
	pair->brt->compare_fun = 0;
	pair->brt->dup_compare = 0;
	pair->brt->db = 0;
	pair->brt->skey = pair->brt->sval = 0;
    }
    pair->brt->h = h;
    pair->brt->nodesize = h->nodesize;
    pair->brt->flags    = h->nodesize;
    r = toku_unpin_brt_header(pair->brt);
    assert(r==0);
}

void toku_recover_newbrtnode (struct logtype_newbrtnode *c) {
    int r;
    struct cf_pair *pair;
    r = find_cachefile(c->filenum, &pair);
    assert(r==0);
    TAGMALLOC(BRTNODE, n);
    n->nodesize     = c->nodesize;
    n->thisnodename = c->diskoff;
    n->log_lsn = n->disk_lsn  = c->lsn; printf("%s:%d %p->disk_lsn=%"PRId64"\n", __FILE__, __LINE__, n, n->disk_lsn.lsn);
    n->layout_version = 1;
    n->height         = c->height;
    n->rand4fingerprint = c->rand4fingerprint;
    n->flags = c->is_dup_sort ? TOKU_DB_DUPSORT : 0; // Don't have TOKU_DB_DUP ???
    n->local_fingerprint = 0; // nothing there yet
    n->dirty = 1;
    if (c->height==0) {
	r=toku_pma_create(&n->u.l.buffer, toku_dont_call_this_compare_fun, null_db, c->filenum, c->nodesize);
	assert(r==0);
	n->u.l.n_bytes_in_buffer=0;
    } else {
	fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);
	abort();
    }
    // Now put it in the cachetable
    toku_cachetable_put(pair->cf, c->diskoff, n, toku_serialize_brtnode_size(n),  toku_brtnode_flush_callback, toku_brtnode_fetch_callback, 0);

    VERIFY_COUNTS(n);

    r = toku_cachetable_unpin(pair->cf, c->diskoff, 1, toku_serialize_brtnode_size(n));
    assert(r==0);
}

void toku_recover_fopen (struct logtype_fopen *c) {
    char *fname = fixup_fname(&c->fname);
    CACHEFILE cf;
    int fd = open(fname, O_RDWR, 0);
    assert(fd>=0);
    int r = toku_cachetable_openfd(&cf, ct, fd);
    assert(r==0);
    toku_recover_note_cachefile(c->filenum, cf);
    toku_free(fname);
    toku_free(c->fname.data);
}

void toku_recover_insertinleaf (struct logtype_insertinleaf *c) {
    struct cf_pair *pair;
    int r = find_cachefile(c->filenum, &pair);
    assert(r==0);
    void *node_v;
    assert(pair->brt);
    r = toku_cachetable_get_and_pin(pair->cf, c->diskoff, &node_v, NULL, toku_brtnode_flush_callback, toku_brtnode_fetch_callback, pair->brt);
    assert(r==0);
    BRTNODE node = node_v;
    assert(node->height==0);
    VERIFY_COUNTS(node);
    DBT key,data;
    r = toku_pma_set_at_index(node->u.l.buffer, c->pmaidx, toku_fill_dbt(&key, c->key.data, c->key.len), toku_fill_dbt(&data, c->data.data, c->data.len));
    assert(r==0);
    node->local_fingerprint += node->rand4fingerprint*toku_calccrc32_kvpair(c->key.data, c->key.len,c->data.data, c->data.len);
    node->u.l.n_bytes_in_buffer += PMA_ITEM_OVERHEAD + KEY_VALUE_OVERHEAD + c->key.len + c->data.len; 

    VERIFY_COUNTS(node);

    r = toku_cachetable_unpin(pair->cf, c->diskoff, 1, toku_serialize_brtnode_size(node));
    assert(r==0);
    toku_free(c->key.data);
    toku_free(c->data.data);
}

int toku_rollback_insertinleaf (struct logtype_insertinleaf *c, TOKUTXN txn)  {
    CACHEFILE cf;
    BRT brt;
    void *node_v;
    int r = toku_cachefile_of_filenum(txn->logger->ct, c->filenum, &cf, &brt);
    assert(r==0);
    r = toku_cachetable_get_and_pin(cf, c->diskoff, &node_v, NULL, toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt);
    if (r!=0) return r;
    BRTNODE node = node_v;
    r = toku_pma_clear_at_index(node->u.l.buffer, c->pmaidx);
    if (r!=0) return r;
    node->local_fingerprint -= node->rand4fingerprint*toku_calccrc32_kvpair(c->key.data, c->key.len,c->data.data, c->data.len);
    node->u.l.n_bytes_in_buffer -= PMA_ITEM_OVERHEAD + KEY_VALUE_OVERHEAD + c->key.len + c->data.len; 
    VERIFY_COUNTS(node);
    r = toku_cachetable_unpin(cf, c->diskoff, 1, toku_serialize_brtnode_size(node));
    return r;
}


// a newbrtnode should have been done before this
void toku_recover_resizepma (struct logtype_resizepma *c) {
    struct cf_pair *pair;
    int r = find_cachefile(c->filenum, &pair);
    assert(r==0);
    void *node_v;
    assert(pair->brt);
    r = toku_cachetable_get_and_pin (pair->cf, c->diskoff, &node_v, NULL, toku_brtnode_flush_callback, toku_brtnode_fetch_callback, pair->brt);
    assert(r==0);
    BRTNODE node = node_v;
    assert(node->height==0);
    r = toku_resize_pma_exactly (node->u.l.buffer, c->oldsize, c->newsize);
    assert(r==0);
    
    VERIFY_COUNTS(node);

    r = toku_cachetable_unpin(pair->cf, c->diskoff, 1, toku_serialize_brtnode_size(node));
    assert(r==0);
}

void toku_recover_pmadistribute (struct logtype_pmadistribute *c) {
    struct cf_pair *pair;
    int r = find_cachefile(c->filenum, &pair);
    assert(r==0);
    void *node_v;
    assert(pair->brt);
    r = toku_cachetable_get_and_pin(pair->cf, c->diskoff, &node_v, NULL, toku_brtnode_flush_callback, toku_brtnode_fetch_callback, pair->brt);
    assert(r==0);
    BRTNODE node = node_v;
    assert(node->height==0);
    {
	unsigned int i;
	for (i=0; i<c->fromto.size; i++) {
	    assert(c->fromto.array[i].a < toku_pma_index_limit(node->u.l.buffer));
	    assert(c->fromto.array[i].b < toku_pma_index_limit(node->u.l.buffer));
	}
    }
    r = toku_pma_move_indices (node->u.l.buffer, c->fromto);
    // The bytes in bufer and fingerprint shouldn't change

    VERIFY_COUNTS(node);

    r = toku_cachetable_unpin(pair->cf, c->diskoff, 1, toku_serialize_brtnode_size(node));
    assert(r==0);

    toku_free(c->fromto.array);
}

int toku_rollback_fcreate (struct logtype_fcreate *le, TOKUTXN txn)             ABORTIT
int toku_rollback_fheader (struct logtype_fheader *le, TOKUTXN txn)             ABORTIT
int toku_rollback_newbrtnode (struct logtype_newbrtnode *le, TOKUTXN txn)       ABORTIT
int toku_rollback_fopen (struct logtype_fopen *le, TOKUTXN txn)                 ABORTIT
int toku_rollback_resizepma (struct logtype_resizepma *le, TOKUTXN txn)         ABORTIT
int toku_rollback_pmadistribute (struct logtype_pmadistribute *le, TOKUTXN txn) ABORTIT
