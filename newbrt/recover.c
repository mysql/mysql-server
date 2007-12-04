/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

/* Recover an env.  The logs are in argv[1].  The new database is created in the cwd. */

// Test:
//    cd ../src/tests/tmpdir
//    ../../../newbrt/recover ../dir.test_log2.c.tdb

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "log_header.h"
#include "log-internal.h"
#include "cachetable.h"
#include "key.h"

static CACHETABLE ct;
static struct cf_pair {
    FILENUM filenum;
    CACHEFILE cf;
    BRT       brt;
} *cf_pairs;
static int n_cf_pairs=0, max_cf_pairs=0;

static DB * const null_db=0;

static int find_cachefile (FILENUM fnum, CACHEFILE *cf, BRT *brt) {
    int i;
    for (i=0; i<n_cf_pairs; i++) {
	if (fnum.fileid==cf_pairs[i].filenum.fileid) {
	    *cf = cf_pairs[i].cf;
	    *brt = cf_pairs[i].brt;
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
    

static void toku_recover_commit (struct logtype_commit *c) {
    c=c; // !!! We need to do something, but for now we assume everything commits and so we don't have do anything to remember what commited and how to unroll.
}
static void toku_recover_delete (struct logtype_delete *c) {c=c;fprintf(stderr, "%s:%d\n", __FILE__, __LINE__); abort(); }
static void toku_recover_fcreate (struct logtype_fcreate *c) {
    char *fname = fixup_fname(&c->fname);
    int fd = creat(fname, c->mode);
    assert(fd>=0);
    toku_free(fname);
    toku_free(c->fname.data);
}
static void toku_recover_fheader (struct logtype_fheader *c) {
    CACHEFILE cf;
    BRT brt;
    int r = find_cachefile(c->filenum, &cf, &brt);
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
    toku_cachetable_put(cf, 0, h, 0, toku_brtheader_flush_callback, toku_brtheader_fetch_callback, 0);
}

static void toku_recover_newbrtnode (struct logtype_newbrtnode *c) {
    int r;
    CACHEFILE cf;
    BRT brt;
    r = find_cachefile(c->filenum, &cf, &brt);
    assert(r==0);
    TAGMALLOC(BRTNODE, n);
    n->nodesize     = c->nodesize;
    n->thisnodename = c->diskoff;
    n->log_lsn = n->disk_lsn  = c->lsn;
    n->layout_version = 0;
    n->parent_brtnode = 0;
    n->height         = c->height;
    n->rand4fingerprint = c->rand4fingerprint;
    // !!! is_dup_sort is not stored in the log !!!
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
    toku_cachetable_put(cf, c->diskoff, n, toku_serialize_brtnode_size(n),  toku_brtnode_flush_callback, toku_brtnode_fetch_callback, 0);
    r = toku_cachetable_unpin(cf, c->diskoff, 1, toku_serialize_brtnode_size(n));
    assert(r==0);
}
static void toku_recover_fopen (struct logtype_fopen *c) {
    char *fname = fixup_fname(&c->fname);
    CACHEFILE cf;
    int fd = open(fname, O_RDWR, 0);
    assert(fd>=0);
    int r = toku_cachetable_openfd(&cf, ct, fd);
    assert(r==0);
    if (max_cf_pairs==0) {
	n_cf_pairs=1;
	max_cf_pairs=2;
	MALLOC_N(max_cf_pairs, cf_pairs);
    } else {
	if (n_cf_pairs>=max_cf_pairs) {
	    max_cf_pairs*=2;
	    cf_pairs = toku_realloc(cf_pairs, max_cf_pairs*sizeof(*cf_pairs));
	}
	n_cf_pairs++;
    }
    cf_pairs[n_cf_pairs-1].filenum = c->filenum;
    cf_pairs[n_cf_pairs-1].cf      = cf;
    toku_free(fname);
    toku_free(c->fname.data);
}

static void toku_recover_insertinleaf (struct logtype_insertinleaf *c) {
    CACHEFILE cf;
    BRT brt;
    int r = find_cachefile(c->filenum, &cf, &brt);
    assert(r==0);
    void *node_v;
    r = toku_cachetable_get_and_pin(cf, c->diskoff, &node_v, NULL, toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt);
    assert(r==0);
    BRTNODE node = node_v;
    assert(node->height==0);
    DBT key,data;
    r = toku_pma_set_at_index(node->u.l.buffer, c->pmaidx, toku_fill_dbt(&key, c->key.data, c->key.len), toku_fill_dbt(&data, c->data.data, c->data.len));
    assert(r==0);
    node->local_fingerprint += node->rand4fingerprint*toku_calccrc32_kvpair(c->key.data, c->key.len,c->data.data, c->data.len);
    node->u.l.n_bytes_in_buffer += KEY_VALUE_OVERHEAD + c->key.len + c->data.len; 
    r = toku_cachetable_unpin(cf, c->diskoff, 1, toku_serialize_brtnode_size(node));
    assert(r==0);
}

static void toku_recover_resizepma (struct logtype_resizepma *c) {
    CACHEFILE cf;
    BRT brt;
    int r = find_cachefile(c->filenum, &cf, &brt);
    assert(r==0);
    void *node_v;
    r = toku_cachetable_get_and_pin (cf, c->diskoff, &node_v, NULL, toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt);
    assert(r==0);
    BRTNODE node = node_v;
    assert(node->height==0);
    r = toku_resize_pma_exactly (node->u.l.buffer, c->oldsize, c->newsize);
    assert(r==0);
    
    r = toku_cachetable_unpin(cf, c->diskoff, 1, toku_serialize_brtnode_size(node));
    assert(r==0);
}

static void toku_recover_pmadistribute (struct logtype_pmadistribute *c) {
    CACHEFILE cf;
    BRT brt;
    int r = find_cachefile(c->filenum, &cf, &brt);
    assert(r==0);
    void *node_v;
    r = toku_cachetable_get_and_pin(cf, c->diskoff, &node_v, NULL, toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt);
    assert(r==0);
    BRTNODE node = node_v;
    assert(node->height==0);
    r = toku_pma_move_indices (node->u.l.buffer, c->fromto);
    // The bytes in bufer and fingerprint shouldn't change

    r = toku_cachetable_unpin(cf, c->diskoff, 1, toku_serialize_brtnode_size(node));
    assert(r==0);
}

int main (int argc, char *argv[]) {
    const char *dir;
    int r;
    assert(argc==2);
    dir = argv[1];
    int n_logfiles;
    char **logfiles;
    r = toku_logger_find_logfiles(dir, &n_logfiles, &logfiles);
    if (r!=0) exit(1);
    int i;
    r = toku_create_cachetable(&ct, 1<<25, (LSN){0}, 0);
    for (i=0; i<n_logfiles; i++) {
	fprintf(stderr, "Opening %s\n", logfiles[i]);
	FILE *f = fopen(logfiles[i], "r");
	struct log_entry le;
	u_int32_t version;
	r=toku_read_and_print_logmagic(f, &version);
	assert(r==0 && version==0);
	while ((r = toku_log_fread(f, &le))==0) {
	    printf("Got cmd %c\n", le.cmd);
	    logtype_dispatch(le, toku_recover_);
	}
	if (r!=EOF) {
	    if (r==DB_BADFORMAT) {
		fprintf(stderr, "Bad log format\n");
		exit(1);
	    } else {
		fprintf(stderr, "Huh? %s\n", strerror(r));
		exit(1);
	    }
	}
	fclose(f);
    }
    for (i=0; i<n_cf_pairs; i++) {
	r = toku_cachefile_close(&cf_pairs[i].cf);
	assert(r==0);
    }
    toku_free(cf_pairs);
    r = toku_cachetable_close(&ct);
    assert(r==0);
    for (i=0; i<n_logfiles; i++) {
	toku_free(logfiles[i]);
    }
    toku_free(logfiles);
    return 0;
}
