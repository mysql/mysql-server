/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

/* Recover an env.  The logs are in argv[1].  The new database is created in the cwd. */

// Test:
//    cd ../src/tests/tmpdir
//    ../../../newbrt/recover ../dir.test_log2.c.tdb

#include "cachetable.h"
#include "key.h"
#include "brt-internal.h"
#include "log-internal.h"
#include "log_header.h"
#include "toku_assert.h"
#include "kv-pair.h"


#include <fcntl.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

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
	    int r = toku_close_brt(cf_pairs[i].brt, 0);
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

void toku_recover_commit (LSN UU(lsn), TXNID UU(txnid)) {
}
void toku_recover_xabort (LSN UU(lsn), TXNID UU(txnid)) {
}

void create_dir_from_file (const char *fname) {
    int i;
    char *tmp=toku_strdup(fname);
    char ch;
    for (i=0; (ch=fname[i]); i++) {
	if (ch=='/') {
	    if (i>0) {
		tmp[i]=0;
		mode_t oldu = umask(0);
		int r = mkdir(tmp, 0700);
		if (r!=0 && errno!=EEXIST) {
		    printf("error: %s\n", strerror(errno));
		}
		assert (r==0 || (errno==EEXIST));
		umask(oldu);
		tmp[i]=ch;
	    }
	}
    }
    toku_free(tmp);
}

void toku_recover_fcreate (LSN UU(lsn), TXNID UU(txnid),BYTESTRING fname,u_int32_t mode) {
    char *fixed_fname = fixup_fname(&fname);
    create_dir_from_file(fixed_fname);
    int fd = creat(fixed_fname, mode);
    assert(fd>=0);
    toku_free(fixed_fname);
    toku_free_BYTESTRING(fname);
}

int toku_recover_note_cachefile (FILENUM fnum, CACHEFILE cf, BRT brt) {
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
    cf_pairs[n_cf_pairs-1].brt     = brt;
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

static void toku_recover_fheader (LSN UU(lsn), TXNID UU(txnid),FILENUM filenum,LOGGEDBRTHEADER header) {
    struct cf_pair *pair = NULL;
    int r = find_cachefile(filenum, &pair);
    assert(r==0);
    struct brt_header *MALLOC(h);
    assert(h);
    h->dirty=0;
    h->flags_array[0] = header.flags;
    h->nodesize = header.nodesize;
    h->freelist = header.freelist;
    h->unused_memory = header.unused_memory;
    h->n_named_roots = header.n_named_roots;
    r=toku_fifo_create(&h->fifo);
    assert(r==0);
    if ((signed)header.n_named_roots==-1) {
	MALLOC_N(1, h->roots);       assert(h->roots);
	MALLOC_N(1, h->root_hashes); assert(h->root_hashes);
	h->roots[0] = header.u.one.root;
	h->root_hashes[0].valid= FALSE;
    } else {
	assert(0);
    }
    u_int32_t fullhash = toku_cachetable_hash(pair->cf, 0);
    h->fullhash = fullhash;
    toku_cachetable_put(pair->cf, 0, fullhash, h, 0, toku_brtheader_flush_callback, toku_brtheader_fetch_callback, 0);
    if (pair->brt) {
	toku_free(pair->brt->h);
    }  else {
	r = toku_brt_create(&pair->brt);
	assert(r==0);
	pair->brt->cf = pair->cf;
	pair->brt->database_name = 0; // Special case, we don't know or care what the database name is for recovery.
	list_init(&pair->brt->cursors);
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

void toku_recover_newbrtnode (LSN lsn, FILENUM filenum,DISKOFF diskoff,u_int32_t height,u_int32_t nodesize,u_int8_t is_dup_sort,u_int32_t rand4fingerprint) {
    int r;
    struct cf_pair *pair = NULL;
    r = find_cachefile(filenum, &pair);
    assert(r==0);
    TAGMALLOC(BRTNODE, n);
    n->nodesize     = nodesize;
    n->thisnodename = diskoff;
    n->log_lsn = n->disk_lsn  = lsn;
    //printf("%s:%d %p->disk_lsn=%"PRId64"\n", __FILE__, __LINE__, n, n->disk_lsn.lsn);
    n->layout_version = BRT_LAYOUT_VERSION_7;
    n->height         = height;
    n->rand4fingerprint = rand4fingerprint;
    n->flags = is_dup_sort ? TOKU_DB_DUPSORT : 0; // Don't have TOKU_DB_DUP ???
    n->local_fingerprint = 0; // nothing there yet
    n->dirty = 1;
    if (height==0) {
	r=toku_omt_create(&n->u.l.buffer);
	assert(r==0);
	n->u.l.n_bytes_in_buffer=0;
	{
	    u_int32_t mpsize = n->nodesize + n->nodesize/4;
	    void *mp = toku_malloc(mpsize);
	    assert(mp);
	    toku_mempool_init(&n->u.l.buffer_mempool, mp, mpsize);
	}

    } else {
	n->u.n.n_children = 0;
	n->u.n.totalchildkeylens = 0;
	n->u.n.n_bytes_in_buffers = 0;
	MALLOC_N(3,n->u.n.childinfos);
	MALLOC_N(2,n->u.n.childkeys);
    }
    // Now put it in the cachetable
    u_int32_t fullhash = toku_cachetable_hash(pair->cf, diskoff);
    n->fullhash = fullhash;
    toku_cachetable_put(pair->cf, diskoff, fullhash, n, toku_serialize_brtnode_size(n),  toku_brtnode_flush_callback, toku_brtnode_fetch_callback, 0);

    VERIFY_COUNTS(n);

    n->log_lsn = lsn;
    r = toku_cachetable_unpin(pair->cf, diskoff, fullhash, 1, toku_serialize_brtnode_size(n));
    assert(r==0);
}

static void recover_setup_node (FILENUM filenum, DISKOFF diskoff, CACHEFILE *cf, BRTNODE *resultnode) {
    struct cf_pair *pair = NULL;
    int r = find_cachefile(filenum, &pair);
    assert(r==0);
    assert(pair->brt);
    void *node_v;
    u_int32_t fullhash = toku_cachetable_hash(pair->cf, diskoff);
    r = toku_cachetable_get_and_pin(pair->cf, diskoff, fullhash,
				    &node_v, NULL, toku_brtnode_flush_callback, toku_brtnode_fetch_callback, pair->brt);
    assert(r==0);
    BRTNODE node = node_v;
    assert(fullhash==node->fullhash);
    *resultnode = node;
    *cf = pair->cf;
}

static void toku_recover_deqrootentry (LSN lsn __attribute__((__unused__)), FILENUM filenum) {
    struct cf_pair *pair = NULL;
    int r = find_cachefile(filenum, &pair);
    assert(r==0);
    void *h_v;
    u_int32_t fullhash = toku_cachetable_hash(pair->cf, 0);
    r = toku_cachetable_get_and_pin(pair->cf, 0, fullhash,
				    &h_v, NULL, toku_brtheader_flush_callback, toku_brtheader_fetch_callback, 0);
    assert(r==0);
    struct brt_header *h=h_v;
    bytevec storedkey,storeddata;
    ITEMLEN storedkeylen, storeddatalen;
    TXNID storedxid;
    u_int32_t storedtype;
    r = toku_fifo_peek(h->fifo, &storedkey, &storedkeylen, &storeddata, &storeddatalen, &storedtype, &storedxid);
    assert(r==0);
    r = toku_fifo_deq(h->fifo);
    assert(r==0);
    r = toku_cachetable_unpin(pair->cf, 0, fullhash, 1, 0);
    assert(r==0);
}

void toku_recover_enqrootentry (LSN lsn __attribute__((__unused__)), FILENUM filenum, TXNID xid, u_int32_t typ, BYTESTRING key, BYTESTRING val) {
    struct cf_pair *pair = NULL;
    int r = find_cachefile(filenum, &pair);
    assert(r==0);
    void *h_v;
    u_int32_t fullhash = toku_cachetable_hash(pair->cf, 0);
    r = toku_cachetable_get_and_pin(pair->cf, 0, fullhash, &h_v, NULL, toku_brtheader_flush_callback, toku_brtheader_fetch_callback, 0);
    assert(r==0);
    struct brt_header *h=h_v;
    r = toku_fifo_enq(h->fifo, key.data, key.len, val.data, val.len, typ, xid); 
    assert(r==0);
    r = toku_cachetable_unpin(pair->cf, 0, fullhash, 1, 0);
    assert(r==0);
    toku_free(key.data);
    toku_free(val.data);
}

void toku_recover_brtdeq (LSN lsn, FILENUM filenum, DISKOFF diskoff, u_int32_t childnum) {
    CACHEFILE cf;
    BRTNODE node;
    int r;
    recover_setup_node(filenum, diskoff, &cf, &node);
    assert(node->height>0);
    //printf("deq: %lld expected_old_fingerprint=%08x actual=%08x new=%08x\n", diskoff, oldfingerprint, node->local_fingerprint, newfingerprint);
    bytevec actual_key=0, actual_data=0;
    ITEMLEN actual_keylen=0, actual_datalen=0;
    u_int32_t actual_type=0;
    TXNID   actual_xid=0;
    assert(childnum<(u_int32_t)node->u.n.n_children);
    r = toku_fifo_peek(BNC_BUFFER(node, childnum), &actual_key, &actual_keylen, &actual_data, &actual_datalen, &actual_type, &actual_xid);
    assert(r==0);
    u_int32_t sizediff = actual_keylen + actual_datalen + KEY_VALUE_OVERHEAD + BRT_CMD_OVERHEAD;
    node->local_fingerprint -= node->rand4fingerprint * toku_calccrc32_cmd(actual_type, actual_xid, actual_key, actual_keylen, actual_data, actual_datalen);
    node->log_lsn = lsn;
    node->u.n.n_bytes_in_buffers -= sizediff;
    BNC_NBYTESINBUF(node, childnum) -= sizediff;
    r = toku_fifo_deq(BNC_BUFFER(node, childnum)); // don't deq till were' done looking at the data.
    r = toku_cachetable_unpin(cf, diskoff, node->fullhash, 1, toku_serialize_brtnode_size(node));
    assert(r==0);
}

void toku_recover_brtenq (LSN lsn, FILENUM filenum, DISKOFF diskoff, u_int32_t childnum, TXNID xid, u_int32_t typ, BYTESTRING key, BYTESTRING data) {
    CACHEFILE cf;
    BRTNODE node;
    int r;
    recover_setup_node(filenum, diskoff, &cf, &node);
    assert(node->height>0);
    //printf("enq: %lld expected_old_fingerprint=%08x actual=%08x new=%08x\n", diskoff, oldfingerprint, node->local_fingerprint, newfingerprint);
    r = toku_fifo_enq(BNC_BUFFER(node, childnum), key.data, key.len, data.data, data.len, typ, xid);
    assert(r==0);
    node->local_fingerprint += node->rand4fingerprint * toku_calccrc32_cmd(typ, xid, key.data, key.len, data.data, data.len);
    node->log_lsn = lsn;
    u_int32_t sizediff = key.len + data.len + KEY_VALUE_OVERHEAD + BRT_CMD_OVERHEAD;
    r = toku_cachetable_unpin(cf, diskoff, node->fullhash, 1, toku_serialize_brtnode_size(node));
    assert(r==0);
    node->u.n.n_bytes_in_buffers += sizediff;
    BNC_NBYTESINBUF(node, childnum) += sizediff;
    toku_free(key.data);
    toku_free(data.data);
}

void toku_recover_addchild (LSN lsn, FILENUM filenum, DISKOFF diskoff, u_int32_t childnum, DISKOFF child, u_int32_t childfingerprint) {
    CACHEFILE cf;
    BRTNODE node;
    recover_setup_node(filenum, diskoff, &cf, &node);
    assert(node->height>0);
    assert(childnum <= (unsigned)node->u.n.n_children);
    unsigned int i;
    REALLOC_N(node->u.n.n_children+1, node->u.n.childinfos);
    REALLOC_N(node->u.n.n_children, node->u.n.childkeys);
    for (i=node->u.n.n_children; i>childnum; i--) {
	node->u.n.childinfos[i]=node->u.n.childinfos[i-1];
	BNC_NBYTESINBUF(node,i) = BNC_NBYTESINBUF(node,i-1);
	assert(i>=2);
	node->u.n.childkeys [i-1]      = node->u.n.childkeys [i-2];
    }
    if (childnum>0) {
	node->u.n.childkeys [childnum-1] = 0;
    }
    BNC_DISKOFF(node, childnum) = child;
    BNC_SUBTREE_FINGERPRINT(node, childnum) = childfingerprint;
    BNC_SUBTREE_LEAFENTRY_ESTIMATE(node, childnum) = 0;
    int r= toku_fifo_create(&BNC_BUFFER(node, childnum)); assert(r==0);
    BNC_NBYTESINBUF(node, childnum) = 0;
    node->u.n.n_children++;
    node->log_lsn = lsn;
    r = toku_cachetable_unpin(cf, diskoff, node->fullhash, 1, toku_serialize_brtnode_size(node));
    assert(r==0);
}

void toku_recover_delchild (LSN lsn, FILENUM filenum, DISKOFF diskoff, u_int32_t childnum, DISKOFF child, u_int32_t childfingerprint, BYTESTRING pivotkey) {
    struct cf_pair *pair = NULL;
    int r = find_cachefile(filenum, &pair);
    assert(r==0);
    void *node_v;
    assert(pair->brt);
    u_int32_t fullhash = toku_cachetable_hash(pair->cf, diskoff);
    r = toku_cachetable_get_and_pin(pair->cf, diskoff, fullhash, &node_v, NULL, toku_brtnode_flush_callback, toku_brtnode_fetch_callback, pair->brt);
    assert(r==0);
    BRTNODE node = node_v;
    assert(node->height>0);
    assert(node->fullhash==fullhash);

    assert(childnum < (unsigned)node->u.n.n_children);
    assert(node->u.n.childinfos[childnum].subtree_fingerprint == childfingerprint);
    assert(BNC_DISKOFF(node, childnum)==child);
    assert(toku_fifo_n_entries(BNC_BUFFER(node,childnum))==0);
    assert(BNC_NBYTESINBUF(node,childnum)==0);
    assert(node->u.n.n_children>2); // Must be at least two children.
    u_int32_t i;
    assert(childnum>0);
    node->u.n.totalchildkeylens -= toku_brt_pivot_key_len(pair->brt, node->u.n.childkeys[childnum-1]);
    toku_free((void*)node->u.n.childkeys[childnum-1]);
    toku_fifo_free(&BNC_BUFFER(node,childnum));
    for (i=childnum+1; i<(unsigned)node->u.n.n_children; i++) {
	node->u.n.childinfos[i-1] = node->u.n.childinfos[i];
	BNC_NBYTESINBUF(node,i-1) = BNC_NBYTESINBUF(node,i);
	node->u.n.childkeys[i-2] = node->u.n.childkeys[i-1];
    }
    node->u.n.n_children--;

    node->log_lsn = lsn;
    r = toku_cachetable_unpin(pair->cf, diskoff, node->fullhash, 1, toku_serialize_brtnode_size(node));
    assert(r==0);
    toku_free(pivotkey.data);
}

void toku_recover_setchild (LSN lsn, FILENUM filenum, DISKOFF diskoff, u_int32_t childnum, DISKOFF UU(oldchild), DISKOFF newchild) {
    struct cf_pair *pair = NULL;
    int r = find_cachefile(filenum, &pair);
    assert(r==0);
    void *node_v;
    assert(pair->brt);
    u_int32_t fullhash = toku_cachetable_hash(pair->cf, diskoff);
    r = toku_cachetable_get_and_pin(pair->cf, diskoff, fullhash, &node_v, NULL, toku_brtnode_flush_callback, toku_brtnode_fetch_callback, pair->brt);
    assert(r==0);
    BRTNODE node = node_v;
    assert(node->fullhash == fullhash);
    assert(node->height>0);
    assert(childnum < (unsigned)node->u.n.n_children);
    BNC_DISKOFF(node, childnum) = newchild;
    node->log_lsn = lsn;
    r = toku_cachetable_unpin(pair->cf, diskoff, node->fullhash, 1, toku_serialize_brtnode_size(node));
    assert(r==0);
}
void toku_recover_setpivot (LSN lsn, FILENUM filenum, DISKOFF diskoff, u_int32_t childnum, BYTESTRING pivotkey) {
    struct cf_pair *pair = NULL;
    int r = find_cachefile(filenum, &pair);
    assert(r==0);
    void *node_v;
    assert(pair->brt);
    u_int32_t fullhash = toku_cachetable_hash(pair->cf, diskoff);
    r = toku_cachetable_get_and_pin(pair->cf, diskoff, fullhash, &node_v, NULL, toku_brtnode_flush_callback, toku_brtnode_fetch_callback, pair->brt);
    assert(r==0);
    BRTNODE node = node_v;
    assert(node->fullhash==fullhash);
    assert(node->height>0);
    
    struct kv_pair *new_pivot = kv_pair_malloc(pivotkey.data, pivotkey.len, 0, 0);

    node->u.n.childkeys[childnum] = new_pivot;
    node->u.n.totalchildkeylens += toku_brt_pivot_key_len(pair->brt, node->u.n.childkeys[childnum]);

    node->log_lsn = lsn;
    r = toku_cachetable_unpin(pair->cf, diskoff, node->fullhash, 1, toku_serialize_brtnode_size(node));
    assert(r==0);

    toku_free(pivotkey.data);
}

void toku_recover_changechildfingerprint (LSN lsn, FILENUM filenum, DISKOFF diskoff, u_int32_t childnum, u_int32_t UU(oldfingerprint), u_int32_t newfingerprint) {
    struct cf_pair *pair = NULL;
    int r = find_cachefile(filenum, &pair);
    assert(r==0);
    void *node_v;
    assert(pair->brt);
    u_int32_t fullhash = toku_cachetable_hash(pair->cf, diskoff);
    r = toku_cachetable_get_and_pin(pair->cf, diskoff, fullhash, &node_v, NULL, toku_brtnode_flush_callback, toku_brtnode_fetch_callback, pair->brt);
    assert(r==0);
    BRTNODE node = node_v;
    assert(node->fullhash == fullhash);
    assert(node->height>0);
    assert((signed)childnum <= node->u.n.n_children); // we allow the childnum to be one too large.
    BNC_SUBTREE_FINGERPRINT(node, childnum) = newfingerprint;
    node->log_lsn = lsn;
    r = toku_cachetable_unpin(pair->cf, diskoff, node->fullhash, 1, toku_serialize_brtnode_size(node));
    assert(r==0);
    
}

void toku_recover_fopen (LSN UU(lsn), TXNID UU(txnid), BYTESTRING fname, FILENUM filenum) {
    char *fixedfname = fixup_fname(&fname);
    CACHEFILE cf;
    int fd = open(fixedfname, O_RDWR, 0);
    assert(fd>=0);
    BRT brt=0;
    int r = toku_brt_create(&brt);
    assert(r==0);
    brt->fname = fixedfname;
    brt->database_name = 0;
    brt->h=0;
    brt->compare_fun = 0;
    brt->dup_compare = 0;
    brt->db = 0;
    r = toku_cachetable_openfd(&cf, ct, fd, fixedfname);
    assert(r==0);
    brt->skey = brt->sval = 0;
    brt->cf=cf;
    toku_recover_note_cachefile(filenum, cf, brt);
    toku_free_BYTESTRING(fname);
}

void toku_recover_brtclose (LSN UU(lsn), BYTESTRING UU(fname), FILENUM filenum) {
    struct cf_pair *pair = NULL;
    int r = find_cachefile(filenum, &pair);
    assert(r==0);
    // Bump up the reference count
    toku_cachefile_refup(pair->cf);
    r = toku_close_brt(pair->brt, 0);
    assert(r==0);
    pair->brt=0;
    toku_free_BYTESTRING(fname);
}

void toku_recover_cfclose (LSN UU(lsn), BYTESTRING UU(fname), FILENUM filenum) {
    int i;
    for (i=0; i<n_cf_pairs; i++) {
	if (filenum.fileid==cf_pairs[i].filenum.fileid) {
	    int r = toku_cachefile_close(&cf_pairs[i].cf, 0);
	    assert(r==0);
	    cf_pairs[i] = cf_pairs[n_cf_pairs-1];
	    n_cf_pairs--;
	    break;
	}
    }
    toku_free_BYTESTRING(fname);
}

static int fill_buf (OMTVALUE lev, u_int32_t idx, void *varray) {
    LEAFENTRY le=lev;
    LEAFENTRY *array=varray;
    array[idx]=le;
    return 0;
}


// The memory for the new node should have already been allocated.
void toku_recover_leafsplit (LSN lsn, FILENUM filenum, DISKOFF old_diskoff, DISKOFF new_diskoff, u_int32_t old_n, u_int32_t new_n, u_int32_t new_node_size, u_int32_t new_rand4, u_int8_t is_dup_sort) {
    struct cf_pair *pair = NULL;
    int r = find_cachefile(filenum, &pair);
    void *nodeA_v;
    assert(pair->brt);
    u_int32_t oldn_fullhash = toku_cachetable_hash(pair->cf, old_diskoff);
    r = toku_cachetable_get_and_pin(pair->cf, old_diskoff, oldn_fullhash, &nodeA_v, NULL, toku_brtnode_flush_callback, toku_brtnode_fetch_callback, pair->brt);
    assert(r==0);
    BRTNODE oldn = nodeA_v;
    assert(oldn->fullhash==oldn_fullhash);
    assert(oldn->height==0);

    TAGMALLOC(BRTNODE, newn);
    assert(newn);
    //printf("%s:%d leafsplit %p (%lld) %p (%lld)\n", __FILE__, __LINE__, oldn, old_diskoff, newn, new_diskoff);

    newn->fullhash     = toku_cachetable_hash(pair->cf, new_diskoff);
    newn->nodesize     = new_node_size;
    newn->thisnodename = new_diskoff;
    newn->log_lsn = newn->disk_lsn  = lsn;
    //printf("%s:%d %p->disk_lsn=%"PRId64"\n", __FILE__, __LINE__, n, n->disk_lsn.lsn);
    newn->layout_version = BRT_LAYOUT_VERSION_7;
    newn->height         = 0;
    newn->rand4fingerprint = new_rand4;
    newn->flags = is_dup_sort ? TOKU_DB_DUPSORT : 0; // Don't have TOKU_DB_DUP ???
    newn->dirty = 1;

    {
	u_int32_t mpsize = newn->nodesize + newn->nodesize/4;
	void *mp = toku_malloc(mpsize);
	assert(mp);
	toku_mempool_init(&newn->u.l.buffer_mempool, mp, mpsize);
    }

    assert(toku_omt_size(oldn->u.l.buffer)==old_n);

    u_int32_t n_leafentries = old_n;
    OMTVALUE *MALLOC_N(n_leafentries, leafentries);
    assert(leafentries);
    toku_omt_iterate(oldn->u.l.buffer, fill_buf, leafentries);
    {
	u_int32_t i;
	u_int32_t new_fp = 0, new_size = 0;
	for (i=new_n; i<n_leafentries; i++) {
	    LEAFENTRY oldle = leafentries[i];
	    LEAFENTRY newle = toku_mempool_malloc(&newn->u.l.buffer_mempool, leafentry_memsize(oldle), 1);
	    assert(newle);
	    new_fp   += toku_le_crc(oldle);
	    new_size += OMT_ITEM_OVERHEAD + leafentry_disksize(oldle);
	    memcpy(newle, oldle, leafentry_memsize(oldle));
	    toku_mempool_mfree(&oldn->u.l.buffer_mempool, oldle, leafentry_memsize(oldle));
	    leafentries[i] = newle;
	}
	toku_omt_destroy(&oldn->u.l.buffer);
	r = toku_omt_create_from_sorted_array(&newn->u.l.buffer, leafentries+new_n, n_leafentries-new_n);
	assert(r==0);
	newn->u.l.n_bytes_in_buffer = new_size;
	newn->local_fingerprint     = newn->rand4fingerprint * new_fp;
    }
    {
	u_int32_t i;
	u_int32_t old_fp = 0, old_size = 0;
	for (i=0; i<new_n; i++) {
	    LEAFENTRY oldle = leafentries[i];
	    old_fp   += toku_le_crc(oldle);
	    old_size += OMT_ITEM_OVERHEAD + leafentry_disksize(oldle);
	}
	r = toku_omt_create_from_sorted_array(&oldn->u.l.buffer, leafentries,      new_n);
	oldn->u.l.n_bytes_in_buffer = old_size;
	oldn->local_fingerprint     = oldn->rand4fingerprint * old_fp;
    }
    toku_free(leafentries);
    //r = toku_omt_split_at(oldn->u.l.buffer, &newn->u.l.buffer, new_n);

    toku_verify_all_in_mempool(oldn);    toku_verify_counts(oldn);
    toku_verify_all_in_mempool(newn);    toku_verify_counts(newn);

    toku_cachetable_put(pair->cf, new_diskoff, newn->fullhash,
			newn, toku_serialize_brtnode_size(newn), toku_brtnode_flush_callback, toku_brtnode_fetch_callback, 0);
    newn->log_lsn = lsn;
    r = toku_cachetable_unpin(pair->cf, new_diskoff, newn->fullhash, 1, toku_serialize_brtnode_size(newn));
    assert(r==0);
    oldn->log_lsn = lsn;
    r = toku_cachetable_unpin(pair->cf, old_diskoff, oldn->fullhash, 1, toku_serialize_brtnode_size(oldn));
    assert(r==0);
}

void toku_recover_insertleafentry (LSN lsn, FILENUM filenum, DISKOFF diskoff, u_int32_t idx, LEAFENTRY newleafentry) {
    struct cf_pair *pair = NULL;
    int r = find_cachefile(filenum, &pair);
    assert(r==0);
    void *node_v;
    assert(pair->brt);
    u_int32_t fullhash = toku_cachetable_hash(pair->cf, diskoff);
    r = toku_cachetable_get_and_pin(pair->cf, diskoff, fullhash, &node_v, NULL, toku_brtnode_flush_callback, toku_brtnode_fetch_callback, pair->brt);
    assert(r==0);
    BRTNODE node = node_v;
    assert(node->fullhash==fullhash);
    assert(node->height==0);
    VERIFY_COUNTS(node);
    node->log_lsn = lsn;
    {
	int memsize = leafentry_memsize(newleafentry);
	void *mem = mempool_malloc_from_omt(node->u.l.buffer, &node->u.l.buffer_mempool, memsize);
	assert(mem);
	memcpy(mem, newleafentry, memsize);
	r = toku_omt_insert_at(node->u.l.buffer, mem, idx);
	assert(r==0);
	node->u.l.n_bytes_in_buffer += OMT_ITEM_OVERHEAD + leafentry_disksize(newleafentry);
	node->local_fingerprint += node->rand4fingerprint * toku_le_crc(newleafentry);
    }
    r = toku_cachetable_unpin(pair->cf, diskoff, node->fullhash, 1, toku_serialize_brtnode_size(node));
    assert(r==0);
    toku_free_LEAFENTRY(newleafentry);
}

void toku_recover_deleteleafentry (LSN lsn, FILENUM filenum, DISKOFF diskoff, u_int32_t idx) {
    struct cf_pair *pair = NULL;
    int r = find_cachefile(filenum, &pair);
    assert(r==0);
    void *node_v;
    assert(pair->brt);
    u_int32_t fullhash = toku_cachetable_hash(pair->cf, diskoff);
    r = toku_cachetable_get_and_pin(pair->cf, diskoff, fullhash, &node_v, NULL, toku_brtnode_flush_callback, toku_brtnode_fetch_callback, pair->brt);
    assert(r==0);
    BRTNODE node = node_v;
    assert(node->fullhash==fullhash);
    assert(node->height==0);
    VERIFY_COUNTS(node);
    node->log_lsn = lsn;
    {
	OMTVALUE data = 0;
	r=toku_omt_fetch(node->u.l.buffer, idx, &data, NULL);
	assert(r==0);
	LEAFENTRY oldleafentry=data;
	u_int32_t  len = leafentry_memsize(oldleafentry);
	assert(memcmp(oldleafentry, data, len)==0);
	node->u.l.n_bytes_in_buffer -= OMT_ITEM_OVERHEAD + leafentry_disksize(oldleafentry);
	node->local_fingerprint -= node->rand4fingerprint * toku_le_crc(oldleafentry);
	toku_mempool_mfree(&node->u.l.buffer_mempool, oldleafentry, len);
	r = toku_omt_delete_at(node->u.l.buffer, idx);
	assert(r==0);
    }
    r = toku_cachetable_unpin(pair->cf, diskoff, node->fullhash, 1, toku_serialize_brtnode_size(node));
    assert(r==0);
}

void toku_recover_changeunnamedroot (LSN UU(lsn), FILENUM filenum, DISKOFF UU(oldroot), DISKOFF newroot) {
    struct cf_pair *pair = NULL;
    int r = find_cachefile(filenum, &pair);
    assert(r==0);
    assert(pair->brt);
    r = toku_read_and_pin_brt_header(pair->cf, &pair->brt->h);
    assert(r==0);
    pair->brt->h->roots[0] = newroot;
    pair->brt->h->root_hashes[0].valid = FALSE;
    r = toku_unpin_brt_header(pair->brt);
}
void toku_recover_changenamedroot (LSN UU(lsn), FILENUM UU(filenum), BYTESTRING UU(name), DISKOFF UU(oldroot), DISKOFF UU(newroot)) { assert(0); }

void toku_recover_changeunusedmemory (LSN UU(lsn), FILENUM filenum, DISKOFF UU(oldunused), DISKOFF newunused) {
    struct cf_pair *pair = NULL;
    int r = find_cachefile(filenum, &pair);
    assert(r==0);
    assert(pair->brt);
    r = toku_read_and_pin_brt_header(pair->cf, &pair->brt->h);
    assert(r==0);
    pair->brt->h->unused_memory = newunused;
    r = toku_unpin_brt_header(pair->brt);
}

static int toku_recover_checkpoint (LSN UU(lsn)) {
    return 0;
}

static int toku_recover_xbegin (LSN UU(lsn), TXNID UU(parent)) {
    return 0;
}

int tokudb_recover(const char *data_dir, const char *log_dir) {
    int r;
    int entrycount=0;
    char **logfiles;

    int lockfd;

    {
	int namelen=strlen(data_dir);
	char lockfname[namelen+20];

	snprintf(lockfname, sizeof(lockfname), "%s/__recoverylock_dont_delete_me", data_dir);
	lockfd = open(lockfname, O_RDWR|O_CREAT, S_IRUSR | S_IWUSR);
	if (lockfd<0) {
	    printf("Couldn't open %s\n", lockfname);
	    return errno;
	}
	r=flock(lockfd, LOCK_EX | LOCK_NB);
	if (r!=0) {
	    printf("Couldn't run recovery because some other process holds the recovery lock %s\n", lockfname);
	    return errno;
	}
    }

    r = toku_logger_find_logfiles(log_dir, &logfiles);
    if (r!=0) return r;
    int i;
    toku_recover_init();
    char org_wd[1000];
    {
	char *wd=getcwd(org_wd, sizeof(org_wd));
	assert(wd!=0);
	//printf("%s:%d org_wd=\"%s\"\n", __FILE__, __LINE__, org_wd);
    }
    char data_wd[1000];
    {
	r=chdir(data_dir); assert(r==0);
	char *wd=getcwd(data_wd, sizeof(data_wd));
	assert(wd!=0);
	//printf("%s:%d data_wd=\"%s\"\n", __FILE__, __LINE__, data_wd);
    }
    for (i=0; logfiles[i]; i++) {
	//fprintf(stderr, "Opening %s\n", logfiles[i]);
	r=chdir(org_wd);
	assert(r==0);
	FILE *f = fopen(logfiles[i], "r");
	struct log_entry le;
	u_int32_t version;
	//printf("Reading file %s\n", logfiles[i]);
	r=toku_read_and_print_logmagic(f, &version);
	assert(r==0 && version==0);
	r=chdir(data_wd);
	assert(r==0);
	while ((r = toku_log_fread(f, &le))==0) {
	    //printf("%lld: Got cmd %c\n", (long long)le.u.commit.lsn.lsn, le.cmd);
	    logtype_dispatch_args(&le, toku_recover_);
	    entrycount++;
	}
	if (r!=EOF) {
	    if (r==DB_BADFORMAT) {
		fprintf(stderr, "Bad log format at record %d\n", entrycount);
		return r;
	    } else {
		fprintf(stderr, "Huh? %s\n", strerror(r));
		return r;
	    }
	}
	fclose(f);
    }
    toku_recover_cleanup();
    for (i=0; logfiles[i]; i++) {
	toku_free(logfiles[i]);
    }
    toku_free(logfiles);

    r=flock(lockfd, LOCK_UN);
    if (r!=0) return errno;

    r=chdir(org_wd);
    if (r!=0) return errno;

    //printf("%s:%d recovery successful! ls -l says\n", __FILE__, __LINE__);
    //system("ls -l");
    return 0;
}
