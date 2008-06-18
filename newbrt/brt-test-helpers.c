#include "brt-internal.h"
#include "toku_assert.h"

int toku_testsetup_leaf(BRT brt, DISKOFF *diskoff) {
    BRTNODE node;
    int r = toku_read_and_pin_brt_header(brt->cf, &brt->h);
    if (r!=0) return r;
    toku_create_new_brtnode(brt, &node, 0, (TOKULOGGER)0);

    *diskoff = node->thisnodename;
    r = toku_unpin_brtnode(brt, node);
    if (r!=0) return r;
    r = toku_unpin_brt_header(brt);
    if (r!=0) return r;
    return 0;
}

// Don't bother to clean up carefully if something goes wrong.  (E.g., it's OK to have malloced stuff that hasn't been freed.)
int toku_testsetup_nonleaf (BRT brt, int height, DISKOFF *diskoff, int n_children, DISKOFF *children, u_int32_t *subtree_fingerprints, char **keys, int *keylens) {
    BRTNODE node;
    assert(n_children<=BRT_FANOUT);
    int r = toku_read_and_pin_brt_header(brt->cf, &brt->h);
    if (r!=0) return r;
    toku_create_new_brtnode(brt, &node, height, (TOKULOGGER)0);
    node->u.n.n_children=n_children;
    MALLOC_N(n_children+1, node->u.n.childinfos);
    MALLOC_N(n_children,   node->u.n.childkeys);
    node->u.n.totalchildkeylens=0;
    node->u.n.n_bytes_in_buffers=0;
    int i;
    for (i=0; i<n_children; i++) {
	node->u.n.childinfos[i] = (struct brtnode_nonleaf_childinfo){ .subtree_fingerprint = subtree_fingerprints[i],
								      .leafentry_estimate  = 0,
								      .diskoff             = children[i],
								      .n_bytes_in_buffer   = 0 };
	r = toku_fifo_create(&BNC_BUFFER(node,i)); if (r!=0) return r;
    }
    for (i=0; i+1<n_children; i++) {
	node->u.n.childkeys[i] = kv_pair_malloc(keys[i], keylens[i], 0, 0);
	node->u.n.totalchildkeylens += keylens[i];
    }
    *diskoff = node->thisnodename;
    r = toku_unpin_brtnode(brt, node);
    if (r!=0) return r;
    r = toku_unpin_brt_header(brt);
    if (r!=0) return r;
    return 0;
}

int toku_testsetup_root(BRT brt, DISKOFF diskoff) {
    int r = toku_read_and_pin_brt_header(brt->cf, &brt->h);
    if (r!=0) return r;
    brt->h->roots[0] = diskoff;
    brt->h->root_hashes[0].valid = FALSE;
    r = toku_unpin_brt_header(brt);
    return r;
}

int toku_testsetup_get_sersize(BRT brt, DISKOFF diskoff) // Return the size on disk
{
    void *node_v;
    int r  = toku_cachetable_get_and_pin(brt->cf, diskoff, toku_cachetable_hash(brt->cf, diskoff), &node_v, NULL,
					 toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt);
    assert(r==0);
    int size = toku_serialize_brtnode_size(node_v);
    r = toku_unpin_brtnode(brt, node_v);
    assert(r==0);
    return size;
}

int toku_testsetup_insert_to_leaf (BRT brt, DISKOFF diskoff, char *key, int keylen, char *val, int vallen, u_int32_t *subtree_fingerprint) {
    void *node_v;
    int r;
    r = toku_cachetable_get_and_pin(brt->cf, diskoff, toku_cachetable_hash(brt->cf, diskoff), &node_v, NULL,
				    toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt);
    if (r!=0) return r;
    BRTNODE node=node_v;
    toku_verify_counts(node);
    assert(node->height==0);

    u_int32_t lesize, disksize;
    LEAFENTRY tmp_leafentry;
    r = le_committed(keylen, key, vallen, val, &lesize, &disksize, &tmp_leafentry);

    LEAFENTRY leafentry = mempool_malloc_from_omt(node->u.l.buffer, &node->u.l.buffer_mempool, lesize);
    memcpy(leafentry, tmp_leafentry, lesize);
    toku_free(tmp_leafentry);

    OMTVALUE storeddatav;
    u_int32_t idx;
    DBT keydbt,valdbt;
    BRT_CMD_S cmd = {BRT_INSERT, 0, .u.id={toku_fill_dbt(&keydbt, key, keylen),
					   toku_fill_dbt(&valdbt, val, vallen)}};
    struct cmd_leafval_bessel_extra be = {brt, &cmd, node->flags & TOKU_DB_DUPSORT};
    r = toku_omt_find_zero(node->u.l.buffer, toku_cmd_leafval_bessel, &be, &storeddatav, &idx, NULL);


    if (r==0) {
	LEAFENTRY storeddata=storeddatav;
	// It's already there.  So now we have to remove it and put the new one back in.
	node->u.l.n_bytes_in_buffer -= OMT_ITEM_OVERHEAD + leafentry_disksize(storeddata);
	node->local_fingerprint     -= node->rand4fingerprint*toku_le_crc(storeddata);
	toku_mempool_mfree(&node->u.l.buffer_mempool, storeddata, leafentry_memsize(storeddata));
	// Now put the new kv in.
	toku_omt_set_at(node->u.l.buffer, leafentry, idx);
    } else {
	r = toku_omt_insert(node->u.l.buffer, leafentry, toku_cmd_leafval_bessel, &be, 0);
	assert(r==0);
    }

    node->u.l.n_bytes_in_buffer += OMT_ITEM_OVERHEAD + disksize;
    node->local_fingerprint += node->rand4fingerprint*toku_le_crc(leafentry);

    node->dirty=1;
    *subtree_fingerprint = node->local_fingerprint;

    toku_verify_counts(node);

    r = toku_unpin_brtnode(brt, node_v);
    return r;
}

int toku_testsetup_insert_to_nonleaf (BRT brt, DISKOFF diskoff, enum brt_cmd_type cmdtype, char *key, int keylen, char *val, int vallen, u_int32_t *subtree_fingerprint) {
    void *node_v;
    int r;
    r = toku_cachetable_get_and_pin(brt->cf, diskoff, toku_cachetable_hash(brt->cf, diskoff), &node_v, NULL,
				    toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt);
    if (r!=0) return r;
    BRTNODE node=node_v;
    assert(node->height>0);

    DBT k,v;
    int childnum = toku_brtnode_which_child(node,
				       toku_fill_dbt(&k, key, keylen),
				       toku_fill_dbt(&v, val, vallen),
				       brt);
    
    r = toku_fifo_enq(BNC_BUFFER(node, childnum), key, keylen, val, vallen, cmdtype, (TXNID)0);
    assert(r==0);
    u_int32_t fdelta = node->rand4fingerprint * toku_calccrc32_cmd(cmdtype, (TXNID)0, key, keylen, val, vallen);
    node->local_fingerprint += fdelta;
    *subtree_fingerprint += fdelta;
    int sizediff = keylen + vallen + KEY_VALUE_OVERHEAD + BRT_CMD_OVERHEAD;
    node->u.n.n_bytes_in_buffers += sizediff;
    BNC_NBYTESINBUF(node, childnum) += sizediff;
    node->dirty = 1;

    r = toku_unpin_brtnode(brt, node_v);
    return r;
}
