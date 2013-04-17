/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

/* Buffered repository tree.
 * Observation:  The in-memory representation of a node doesn't have to be the same as the on-disk representation.
 *  Goal for the in-memory representation:  fast
 *  Goal for on-disk: small
 * 
 * So to get this running fast, I'll  make a version that doesn't do range queries:
 *   use a hash table for in-memory
 *   simply write the strings on disk.
 * Later I'll do a PMA or a skiplist for the in-memory version.
 * Also, later I'll convert the format to network order fromn host order.
 * Later, for on disk, I'll compress it (perhaps with gzip, perhaps with the bzip2 algorithm.)
 *
 * The collection of nodes forms a data structure like a B-tree.  The complexities of keeping it balanced apply.
 *
 * We always write nodes to a new location on disk.
 * The nodes themselves contain the information about the tree structure.
 * Q: During recovery, how do we find the root node without looking at every block on disk?
 * A: The root node is either the designated root near the front of the freelist.
 *    The freelist is updated infrequently.  Before updating the stable copy of the freelist, we make sure that
 *    the root is up-to-date.  We can make the freelist-and-root update be an arbitrarily small fraction of disk bandwidth.
 *     
 */

#include "includes.h"

long long n_items_malloced;

static void verify_local_fingerprint_nonleaf (BRTNODE node);
static int toku_dump_brtnode (BRT brt, BLOCKNUM blocknum, int depth, bytevec lorange, ITEMLEN lolen, bytevec hirange, ITEMLEN hilen);

typedef struct kvpair {
    bytevec key;
    unsigned int keylen;
    bytevec val;
    unsigned int vallen;
} *KVPAIR;

// Simple LCG random number generator.  Not high quality, but good enough.
static int r_seeded=0;
static u_int32_t rstate=1;
static inline void mysrandom (int s) {
    rstate=s;
    r_seeded=1;
}
static inline u_int32_t myrandom (void) {
    if (!r_seeded) {
	struct timeval tv;
	gettimeofday(&tv, 0);
	mysrandom(tv.tv_sec);
    }
    rstate = (279470275ull*(u_int64_t)rstate)%4294967291ull;
    return rstate;
}

static int
handle_split_of_child_simple (BRT t, BRTNODE node, int childnum,
			      BRTNODE childa, BRTNODE childb,
			      DBT *splitk, /* the data in the childsplitk is previously alloc'd and is consumed by this call. */
			      TOKULOGGER logger);
static int brt_nonleaf_split (BRT t, BRTNODE node, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk, TOKULOGGER logger);



//#define MAX_PATHLEN_TO_ROOT 40


static const char *unparse_cmd_type (enum brt_cmd_type typ) __attribute__((__unused__));
static const char *unparse_cmd_type (enum brt_cmd_type typ) {
    switch (typ) {
    case BRT_NONE: return "NONE";
    case BRT_INSERT: return "INSERT";
    case BRT_DELETE_ANY: return "DELETE_ANY";
    case BRT_DELETE_BOTH: return "DELETE_BOTH";
    case BRT_ABORT_ANY: return "ABORT_ANY";
    case BRT_ABORT_BOTH: return "ABORT_BOTH";
    case BRT_COMMIT_ANY: return "COMMIT_ANY";
    case BRT_COMMIT_BOTH: return "COMMIT_BOTH";
    }
    return "?";
}

static int brtnode_put_cmd (BRT t, BRTNODE node, BRT_CMD cmd,
			    int *did_split, BRTNODE *nodea, BRTNODE *nodeb,
			    DBT *split,
			    TOKULOGGER);
static int
brtnode_put_cmd_simple (BRT t, BRTNODE node, BRT_CMD cmd, TOKULOGGER logger,
			BOOL *should_split, BOOL *should_merge);

// The maximum row size is 16KB according to the PRD.  That means the max pivot key size is 16KB.
#define MAX_PIVOT_KEY_SIZE (1<<14)

/* key is not in the buffer.  Either put the key-value pair in the child, or put it in the node. */
static int push_brt_cmd_down_only_if_it_wont_push_more_else_put_here (BRT t, BRTNODE node, BRTNODE child,
								      BRT_CMD cmd,
								      int childnum_of_node,
								      TOKULOGGER logger) {
    assert(node->height>0); /* Not a leaf. */
    DBT *k = cmd->u.id.key;
    DBT *v = cmd->u.id.val;
    unsigned int oldsize = toku_serialize_brtnode_size(child);
    unsigned int newsize_bounded = oldsize + k->size + v->size + KEY_VALUE_OVERHEAD + LE_OVERHEAD_BOUND + MAX_PIVOT_KEY_SIZE;
    newsize_bounded += (child->height > 0) ? BRT_CMD_OVERHEAD : OMT_ITEM_OVERHEAD;
    
    int to_child = newsize_bounded <= child->nodesize;
    if (0) {
	printf("%s:%d pushing %s to %s %d", __FILE__, __LINE__, (char*)k->data, to_child? "child" : "hash", childnum_of_node);
	if (childnum_of_node+1<node->u.n.n_children) {
	    DBT k2;
	    printf(" nextsplitkey=%s\n", (char*)node->u.n.childkeys[childnum_of_node]);
	    assert(t->compare_fun(t->db, k, toku_fill_dbt(&k2, node->u.n.childkeys[childnum_of_node], toku_brt_pivot_key_len(t, node->u.n.childkeys[childnum_of_node])))<=0);
	} else {
	    printf("\n");
	}
    }
    int r;
    if (to_child) {
	int again_split=-1; BRTNODE againa,againb;
	DBT againk;
	toku_init_dbt(&againk);
	//printf("%s:%d hello!\n", __FILE__, __LINE__);
	r = brtnode_put_cmd(t, child, cmd,
			    &again_split, &againa, &againb, &againk,
			    logger);
	if (r!=0) return r;
	assert(again_split==0); /* I only did the insert if I knew it wouldn't push down, and hence wouldn't split. */
    } else {
	r=insert_to_buffer_in_nonleaf(node, childnum_of_node, k, v, cmd->type, cmd->xid);
    }
    if (newsize_bounded < toku_serialize_brtnode_size(child)) {
	fprintf(stderr, "%s:%d size estimate is messed up. newsize_bounded=%u actual_size=%u child_height=%d to_child=%d\n",
		__FILE__, __LINE__, newsize_bounded, toku_serialize_brtnode_size(child), child->height, to_child);
	fprintf(stderr, "  cmd->type=%s cmd->xid=%llu\n", unparse_cmd_type(cmd->type), (unsigned long long)cmd->xid);
	fprintf(stderr, "  oldsize=%u k->size=%u v->size=%u\n", oldsize, k->size, v->size);
	assert(toku_serialize_brtnode_size(child)<=child->nodesize);
	//assert(newsize_bounded >= toku_serialize_brtnode_size(child)); // Don't abort on this
    }
    fixup_child_fingerprint(node, childnum_of_node, child, t, logger);
    return r;
}

static int push_a_brt_cmd_down_simple (BRT t, BRTNODE node, BRTNODE child, int childnum,
				       BRT_CMD cmd,
				       BOOL *must_split, BOOL *must_merge,
				       TOKULOGGER logger) {
    //if (debug) printf("%s:%d %*sinserting down\n", __FILE__, __LINE__, debug, "");
    //printf("%s:%d hello!\n", __FILE__, __LINE__);
    assert(node->height>0);
    {
	int r = brtnode_put_cmd_simple(t, child, cmd, logger,
				       must_split, must_merge);
	if (r!=0) return r;
    }

    DBT *k = cmd->u.id.key;
    DBT *v = cmd->u.id.val;
    //if (debug) printf("%s:%d %*sinserted down child_did_split=%d\n", __FILE__, __LINE__, debug, "", child_did_split);
    u_int32_t old_fingerprint = node->local_fingerprint;
    u_int32_t new_fingerprint = old_fingerprint - node->rand4fingerprint*toku_calc_fingerprint_cmdstruct(cmd);
    node->local_fingerprint = new_fingerprint;
    if (t->txn_that_created != cmd->xid) {
	int r = toku_log_brtdeq(logger, &node->log_lsn, 0, toku_cachefile_filenum(t->cf), node->thisnodename, childnum);
	assert(r==0);
    }
    {
	int r = toku_fifo_deq(BNC_BUFFER(node,childnum));
	//printf("%s:%d deleted status=%d\n", __FILE__, __LINE__, r);
	if (r!=0) return r;
    }
    {
	int n_bytes_removed = (k->size + v->size + KEY_VALUE_OVERHEAD + BRT_CMD_OVERHEAD);
	node->u.n.n_bytes_in_buffers -= n_bytes_removed;
	BNC_NBYTESINBUF(node, childnum) -= n_bytes_removed;
        node->dirty = 1;
    }
    fixup_child_fingerprint(node, childnum,   child, t, logger);
    return 0;
}

static int push_a_brt_cmd_down (BRT t, BRTNODE node, BRTNODE child, int childnum,
				BRT_CMD cmd,
				int *child_did_split, BRTNODE *childa, BRTNODE *childb,
				DBT *childsplitk,
				TOKULOGGER logger) {
    //if (debug) printf("%s:%d %*sinserting down\n", __FILE__, __LINE__, debug, "");
    //printf("%s:%d hello!\n", __FILE__, __LINE__);
    assert(node->height>0);
    {
	int r = brtnode_put_cmd(t, child, cmd,
				child_did_split, childa, childb, childsplitk,
				logger);
	if (r!=0) return r;
    }

    DBT *k = cmd->u.id.key;
    DBT *v = cmd->u.id.val;
    //if (debug) printf("%s:%d %*sinserted down child_did_split=%d\n", __FILE__, __LINE__, debug, "", child_did_split);
    u_int32_t old_fingerprint = node->local_fingerprint;
    u_int32_t new_fingerprint = old_fingerprint - node->rand4fingerprint*toku_calc_fingerprint_cmdstruct(cmd);
    node->local_fingerprint = new_fingerprint;
    if (t->txn_that_created != cmd->xid) {
	int r = toku_log_brtdeq(logger, &node->log_lsn, 0, toku_cachefile_filenum(t->cf), node->thisnodename, childnum);
	assert(r==0);
    }
    {
	int r = toku_fifo_deq(BNC_BUFFER(node,childnum));
	//printf("%s:%d deleted status=%d\n", __FILE__, __LINE__, r);
	if (r!=0) return r;
    }
    {
	int n_bytes_removed = (k->size + v->size + KEY_VALUE_OVERHEAD + BRT_CMD_OVERHEAD);
	node->u.n.n_bytes_in_buffers -= n_bytes_removed;
	BNC_NBYTESINBUF(node, childnum) -= n_bytes_removed;
        node->dirty = 1;
    }
    if (*child_did_split) {
	// Don't try to fix these up.
	//fixup_child_fingerprint(node, childnum,   *childa, t, logger);
	//fixup_child_fingerprint(node, childnum+1, *childb, t, logger);
    } else {
	fixup_child_fingerprint(node, childnum,   child, t, logger);
    }
    return 0;
}

static int brtnode_maybe_push_down(BRT t, BRTNODE node, int *did_split, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk, TOKULOGGER logger);

static int split_count=0;



/* NODE is a node with a child.
 * childnum was split into two nodes childa, and childb.  childa is the same as the original child.  childb is a new child.
 * We must slide things around, & move things from the old table to the new tables.
 * We also move things to the new children as much as we can without doing any pushdowns or splitting of the child.
 * We must delete the old buffer (but the old child is already deleted.)
 * We also unpin the new children.
 */
static int handle_split_of_child (BRT t, BRTNODE node, int childnum,
				  BRTNODE childa, BRTNODE childb,
				  DBT *childsplitk, /* the data in the childsplitk is alloc'd and is consumed by this call. */
				  int *did_split, BRTNODE *nodea, BRTNODE *nodeb,
				  DBT *splitk,
				  TOKULOGGER logger) {
    assert(node->height>0);
    assert(0 <= childnum && childnum < node->u.n.n_children);
    FIFO      old_h = BNC_BUFFER(node,childnum);
    int       old_count = BNC_NBYTESINBUF(node, childnum);
    int cnum;
    int r;
    assert(node->u.n.n_children<=TREE_FANOUT);

    if (toku_brt_debug_mode) {
	int i;
	printf("%s:%d Child %d did split on %s\n", __FILE__, __LINE__, childnum, (char*)childsplitk->data);
	printf("%s:%d oldsplitkeys:", __FILE__, __LINE__);
	for(i=0; i<node->u.n.n_children-1; i++) printf(" %s", (char*)node->u.n.childkeys[i]);
	printf("\n");
    }

    node->dirty = 1;

    //verify_local_fingerprint_nonleaf(node);

    REALLOC_N(node->u.n.n_children+2, node->u.n.childinfos);
    REALLOC_N(node->u.n.n_children+1, node->u.n.childkeys);
    // Slide the children over.
    BNC_SUBTREE_FINGERPRINT       (node, node->u.n.n_children+1)=0;
    BNC_SUBTREE_LEAFENTRY_ESTIMATE(node, node->u.n.n_children+1)=0;
    for (cnum=node->u.n.n_children; cnum>childnum+1; cnum--) {
	node->u.n.childinfos[cnum] = node->u.n.childinfos[cnum-1];
    }
    r = toku_log_addchild(logger, (LSN*)0, 0, toku_cachefile_filenum(t->cf), node->thisnodename, childnum+1, childb->thisnodename, 0);
    node->u.n.n_children++;

    assert(BNC_BLOCKNUM(node, childnum).b==childa->thisnodename.b); // use the same child
    BNC_BLOCKNUM(node, childnum+1) = childb->thisnodename;
    BNC_HAVE_FULLHASH(node, childnum+1) = TRUE;
    BNC_FULLHASH(node, childnum+1) = childb->fullhash;
    // BNC_SUBTREE_FINGERPRINT(node, childnum)=0; // leave the subtreefingerprint alone for the child, so we can log the change
    BNC_SUBTREE_FINGERPRINT       (node, childnum+1)=0;
    BNC_SUBTREE_LEAFENTRY_ESTIMATE(node, childnum+1)=0;
    fixup_child_fingerprint(node, childnum,   childa, t, logger);
    fixup_child_fingerprint(node, childnum+1, childb, t, logger);
    r=toku_fifo_create(&BNC_BUFFER(node,childnum+1)); assert(r==0);
    //verify_local_fingerprint_nonleaf(node);    // The fingerprint hasn't changed and everhything is still there.
    r=toku_fifo_create(&BNC_BUFFER(node,childnum));   assert(r==0); // ??? SHould handle this error case
    BNC_NBYTESINBUF(node, childnum) = 0;
    BNC_NBYTESINBUF(node, childnum+1) = 0;

    // Remove all the cmds from the local fingerprint.  Some may get added in again when we try to push to the child.
    FIFO_ITERATE(old_h, skey, skeylen, sval, svallen, type, xid,
		 {
		     u_int32_t old_fingerprint   = node->local_fingerprint;
		     u_int32_t new_fingerprint   = old_fingerprint - node->rand4fingerprint*toku_calc_fingerprint_cmd(type, xid, skey, skeylen, sval, svallen);
		     if (t->txn_that_created != xid) {
			 r = toku_log_brtdeq(logger, &node->log_lsn, 0, toku_cachefile_filenum(t->cf), node->thisnodename, childnum);
			 assert(r==0);
		     }
		     node->local_fingerprint = new_fingerprint;
		 });

    //verify_local_fingerprint_nonleaf(node);

    // Slide the keys over
    {
	struct kv_pair *pivot = childsplitk->data;
	BYTESTRING bs = { .len  = childsplitk->size,
			  .data = kv_pair_key(pivot) };
	r = toku_log_setpivot(logger, (LSN*)0, 0, toku_cachefile_filenum(t->cf), node->thisnodename, childnum, bs);
	if (r!=0) return r;

	for (cnum=node->u.n.n_children-2; cnum>childnum; cnum--) {
	    node->u.n.childkeys[cnum] = node->u.n.childkeys[cnum-1];
	}
	//if (logger) assert((t->flags&TOKU_DB_DUPSORT)==0); // the setpivot is wrong for TOKU_DB_DUPSORT, so recovery will be broken.
	node->u.n.childkeys[childnum]= pivot;
	node->u.n.totalchildkeylens += toku_brt_pivot_key_len(t, pivot);
    }

    if (toku_brt_debug_mode) {
	int i;
	printf("%s:%d splitkeys:", __FILE__, __LINE__);
	for(i=0; i<node->u.n.n_children-2; i++) printf(" %s", (char*)node->u.n.childkeys[i]);
	printf("\n");
    }

    //verify_local_fingerprint_nonleaf(node);

    node->u.n.n_bytes_in_buffers -= old_count; /* By default, they are all removed.  We might add them back in. */
    /* Keep pushing to the children, but not if the children would require a pushdown */
    FIFO_ITERATE(old_h, skey, skeylen, sval, svallen, type, xid, {
	DBT skd; DBT svd;
        BRT_CMD_S brtcmd = build_brt_cmd((enum brt_cmd_type)type, xid, toku_fill_dbt(&skd, skey, skeylen), toku_fill_dbt(&svd, sval, svallen));
	//verify_local_fingerprint_nonleaf(childa); 	verify_local_fingerprint_nonleaf(childb);
	int pusha = 0; int pushb = 0;
	switch (type) {
	case BRT_INSERT:
	case BRT_DELETE_BOTH:
	case BRT_DELETE_ANY:
	case BRT_ABORT_BOTH:
	case BRT_ABORT_ANY:
	case BRT_COMMIT_BOTH:
	case BRT_COMMIT_ANY:
	    if ((type!=BRT_DELETE_ANY && type!=BRT_ABORT_ANY && type!=BRT_COMMIT_ANY) || 0==(t->flags&TOKU_DB_DUPSORT)) {
		// If it's an INSERT or DELETE_BOTH or there are no duplicates then we just put the command into one subtree
		int cmp = brt_compare_pivot(t, &skd, &svd, childsplitk->data);
		if (cmp <= 0) pusha = 1;
		else          pushb = 1;
	    } else {
		assert((type==BRT_DELETE_ANY || type==BRT_ABORT_ANY || type==BRT_COMMIT_ANY) && t->flags&TOKU_DB_DUPSORT);
		// It is a DELETE or ABORT_ANY and it's a DUPSORT database,
		// in which case if the comparison function comes up 0 we must write the command to both children.  (See #201)
		int cmp = brt_compare_pivot(t, &skd, 0, childsplitk->data);
		if (cmp<=0)   pusha=1;
		if (cmp>=0)   pushb=1;  // Could be that both pusha and pushb are set
	    }
	    if (pusha) {
		// If we already have something in the buffer, we must add the new command to the buffer so that commands don't get out of order.
		if (toku_fifo_n_entries(BNC_BUFFER(node,childnum))==0) {
		    r=push_brt_cmd_down_only_if_it_wont_push_more_else_put_here(t, node, childa, &brtcmd, childnum, logger);
		} else {
		    r=insert_to_buffer_in_nonleaf(node, childnum, &skd, &svd, type, xid);
		}
	    }
	    if (pushb) {
		// If we already have something in the buffer, we must add the new command to the buffer so that commands don't get out of order.
		if (toku_fifo_n_entries(BNC_BUFFER(node,childnum+1))==0) {
		    r=push_brt_cmd_down_only_if_it_wont_push_more_else_put_here(t, node, childb, &brtcmd, childnum+1, logger);
		} else {
		    r=insert_to_buffer_in_nonleaf(node, childnum+1, &skd, &svd, type, xid);
		}
	    }
	    //verify_local_fingerprint_nonleaf(childa); 	verify_local_fingerprint_nonleaf(childb); 
	    if (r!=0) printf("r=%d\n", r);
	    assert(r==0);

	    goto ok;


	case BRT_NONE:
	    // Don't have to do anything in this case, can just drop the command
            goto ok;
	}
	printf("Bad type %d\n", type); // Don't use default: because I want a compiler warning if I forget a enum case, and I want a runtime error if the type isn't one of the expected ones.
	assert(0);
     ok: /*nothing*/;
		     });

    toku_fifo_free(&old_h);

    //verify_local_fingerprint_nonleaf(childa);
    //verify_local_fingerprint_nonleaf(childb);
    //verify_local_fingerprint_nonleaf(node);

    VERIFY_NODE(t, node);
    VERIFY_NODE(t, childa);
    VERIFY_NODE(t, childb);

    r=toku_unpin_brtnode(t, childa);
    assert(r==0);
    r=toku_unpin_brtnode(t, childb);
    assert(r==0);
		
    if (node->u.n.n_children>TREE_FANOUT) {
	//printf("%s:%d about to split having pushed %d out of %d keys\n", __FILE__, __LINE__, i, n_pairs);
	r=brt_nonleaf_split(t, node, nodea, nodeb, splitk, logger);
	if (r!=0) return r;
	//printf("%s:%d did split\n", __FILE__, __LINE__);
	split_count++;
	*did_split=1;
	assert((*nodea)->height>0);
	assert((*nodeb)->height>0);
	assert((*nodea)->u.n.n_children>0);
	assert((*nodeb)->u.n.n_children>0);
	assert(BNC_BLOCKNUM(*nodea, (*nodea)->u.n.n_children-1).b!=0);
	assert(BNC_BLOCKNUM(*nodeb, (*nodeb)->u.n.n_children-1).b!=0);
	assert(toku_serialize_brtnode_size(*nodea)<=(*nodea)->nodesize);
	assert(toku_serialize_brtnode_size(*nodeb)<=(*nodeb)->nodesize);
	//verify_local_fingerprint_nonleaf(*nodea);
	//verify_local_fingerprint_nonleaf(*nodeb);
    } else {
	*did_split=0;
        if (toku_serialize_brtnode_size(node) > node->nodesize) {
            /* lighten the node by pushing down its buffers.  this may cause
               the current node to split and go away */
            r = brtnode_maybe_push_down(t, node, did_split, nodea, nodeb, splitk, logger);
            assert(r == 0);
        }
	if (*did_split == 0) assert(toku_serialize_brtnode_size(node)<=node->nodesize);
    }
    return 0;
}

static int
push_some_brt_cmds_down_simple (BRT t, BRTNODE node, int childnum, BOOL *must_split, BOOL *must_merge, TOKULOGGER logger) {
    int r;
    assert(node->height>0);
    BLOCKNUM targetchild = BNC_BLOCKNUM(node, childnum);
    assert(targetchild.b>=0 && targetchild.b<t->h->unused_blocks.b); // This assertion could fail in a concurrent setting since another process might have bumped unused memory.
    u_int32_t childfullhash = compute_child_fullhash(t->cf, node, childnum);
    void *childnode_v;
    r = toku_cachetable_get_and_pin(t->cf, targetchild, childfullhash, &childnode_v, NULL, 
				    toku_brtnode_flush_callback, toku_brtnode_fetch_callback, t->h);
    if (r!=0) return r;
    //printf("%s:%d pin %p\n", __FILE__, __LINE__, childnode_v);
    BRTNODE child = childnode_v;
    assert(child->thisnodename.b!=0);
    //verify_local_fingerprint_nonleaf(child);
    VERIFY_NODE(t, child);
    //printf("%s:%d height=%d n_bytes_in_buffer = {%d, %d, %d, ...}\n", __FILE__, __LINE__, child->height, child->n_bytes_in_buffer[0], child->n_bytes_in_buffer[1], child->n_bytes_in_buffer[2]);
    //printf("%s:%d before pushing into Node %" PRIu64 ", disksize=%d", __FILE__, __LINE__, child->thisnodename.b, toku_serialize_brtnode_size(child));
    //if (child->height==0) printf(" omtsize=%d", toku_omt_size(child->u.l.buffer));
    //printf("\n");
    assert(toku_serialize_brtnode_size(child)<=child->nodesize);
    if (child->height>0 && child->u.n.n_children>0) assert(BNC_BLOCKNUM(child, child->u.n.n_children-1).b!=0);
  
    if (0) {
	static int count=0;
	count++;
	printf("%s:%d pushing %d count=%d\n", __FILE__, __LINE__, childnum, count);
    }
    BOOL some_must_split = FALSE;
    BOOL some_must_merge = FALSE;
    int pushed_count = 0;
    {
	bytevec key,val;
	ITEMLEN keylen, vallen;
	//printf("%s:%d Try random_pick, weight=%d \n", __FILE__, __LINE__, BNC_NBYTESINBUF(node, childnum));
	assert(toku_fifo_n_entries(BNC_BUFFER(node,childnum))>0);
	u_int32_t type;
	TXNID xid;
        while(0==toku_fifo_peek(BNC_BUFFER(node,childnum), &key, &keylen, &val, &vallen, &type, &xid)) {
	    DBT hk,hv;
	    DBT childsplitk;
	    BOOL this_must_split, this_must_merge;

	    BRT_CMD_S brtcmd = { (enum brt_cmd_type)type, xid, .u.id= {toku_fill_dbt(&hk, key, keylen),
								       toku_fill_dbt(&hv, val, vallen)} };

	    //printf("%s:%d random_picked\n", __FILE__, __LINE__);
	    toku_init_dbt(&childsplitk);
	    pushed_count++;
	    r = push_a_brt_cmd_down_simple (t, node, child, childnum,
					    &brtcmd,
					    &this_must_split, &this_must_merge,
					    logger);

	    if (0) {
		unsigned int sum=0;
		FIFO_ITERATE(BNC_BUFFER(node,childnum), subhk __attribute__((__unused__)), hkl, hd __attribute__((__unused__)), hdl, subtype __attribute__((__unused__)), subxid __attribute__((__unused__)),
                             sum+=hkl+hdl+KEY_VALUE_OVERHEAD+BRT_CMD_OVERHEAD);
		printf("%s:%d sum=%u\n", __FILE__, __LINE__, sum);
		assert(sum==BNC_NBYTESINBUF(node, childnum));
	    }
	    if (BNC_NBYTESINBUF(node, childnum)>0) assert(toku_fifo_n_entries(BNC_BUFFER(node,childnum))>0);
	    //printf("%s:%d %d=push_a_brt_cmd_down=();  child_did_split=%d (weight=%d)\n", __FILE__, __LINE__, r, child_did_split, BNC_NBYTESINBUF(node, childnum));
	    if (r!=0) return r;
	    some_must_split |= this_must_split;
	    some_must_merge |= this_must_merge;
	}
	if (0) printf("%s:%d done random picking\n", __FILE__, __LINE__);
    }
    assert(toku_serialize_brtnode_size(node)<=node->nodesize);
    //verify_local_fingerprint_nonleaf(node);
    //printf("%s:%d after pushing %d into Node %" PRIu64 ", disksize=%d", __FILE__, __LINE__, pushed_count, child->thisnodename.b, toku_serialize_brtnode_size(child));
    //if (child->height==0) printf(" omtsize=%d", toku_omt_size(child->u.l.buffer));
    //printf("\n");
    r=toku_unpin_brtnode(t, child);
    if (r!=0) return r;
    *must_split = some_must_split;
    *must_merge = some_must_merge;
    return 0;
}


static int push_some_brt_cmds_down (BRT t, BRTNODE node, int childnum,
				    int *did_split, BRTNODE *nodea, BRTNODE *nodeb,
				    DBT *splitk,
				    TOKULOGGER logger) {
    void *childnode_v;
    BRTNODE child;
    int r;
    assert(node->height>0);
    BLOCKNUM targetchild = BNC_BLOCKNUM(node, childnum);
    assert(targetchild.b>=0 && targetchild.b<t->h->unused_blocks.b); // This assertion could fail in a concurrent setting since another process might have bumped unused memory.
    u_int32_t childfullhash = compute_child_fullhash(t->cf, node, childnum);
    r = toku_cachetable_get_and_pin(t->cf, targetchild, childfullhash, &childnode_v, NULL, 
				    toku_brtnode_flush_callback, toku_brtnode_fetch_callback, t->h);
    if (r!=0) return r;
    //printf("%s:%d pin %p\n", __FILE__, __LINE__, childnode_v);
    child=childnode_v;
    assert(child->thisnodename.b!=0);
    //verify_local_fingerprint_nonleaf(child);
    VERIFY_NODE(t, child);
    //printf("%s:%d height=%d n_bytes_in_buffer = {%d, %d, %d, ...}\n", __FILE__, __LINE__, child->height, child->n_bytes_in_buffer[0], child->n_bytes_in_buffer[1], child->n_bytes_in_buffer[2]);
    if (child->height>0 && child->u.n.n_children>0) assert(BNC_BLOCKNUM(child, child->u.n.n_children-1).b!=0);
  
    if (0) {
	static int count=0;
	count++;
	printf("%s:%d pushing %d count=%d\n", __FILE__, __LINE__, childnum, count);
    }
    ...

    assert(toku_serialize_brtnode_size(node)<=node->nodesize);
    //verify_local_fingerprint_nonleaf(node);
    r=toku_unpin_brtnode(t, child);
    if (r!=0) return r;
    *did_split=0;
    return 0;
}

static int brtnode_maybe_push_down(BRT t, BRTNODE node, int *did_split, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk, TOKULOGGER logger)
/* If the buffer is too full, then push down.  Possibly the child will split.  That may make us split. */
{
    assert(node->height>0);
    if (toku_serialize_brtnode_size(node) > node->nodesize ) {
	{
	    /* Push to a child. */
	    /* Find the heaviest child, and push stuff to it.  Keep pushing to the child until we run out.
	     * But if the child pushes something to its child and our buffer has gotten small enough, then we stop pushing. */
	    int childnum;
	    find_heaviest_child(node, &childnum);
	    assert(BNC_BLOCKNUM(node, childnum).b!=0);
	    int r = push_some_brt_cmds_down(t, node, childnum, did_split, nodea, nodeb, splitk, logger);
	    if (r!=0) return r;
	    assert(*did_split==0 || *did_split==1);
	    if (*did_split) {
		assert(toku_serialize_brtnode_size(*nodea)<=(*nodea)->nodesize);		
		assert(toku_serialize_brtnode_size(*nodeb)<=(*nodeb)->nodesize);		
		assert((*nodea)->u.n.n_children>0);
		assert((*nodeb)->u.n.n_children>0);
		assert(BNC_BLOCKNUM(*nodea, (*nodea)->u.n.n_children-1).b!=0);
		assert(BNC_BLOCKNUM(*nodeb, (*nodeb)->u.n.n_children-1).b!=0);
		//verify_local_fingerprint_nonleaf(*nodea);
		//verify_local_fingerprint_nonleaf(*nodeb);
	    } else {
		assert(toku_serialize_brtnode_size(node)<=node->nodesize);
	    }
	}
    } else {
	*did_split=0;
	assert(toku_serialize_brtnode_size(node)<=node->nodesize);
    }
    //if (*did_split) {
    //	verify_local_fingerprint_nonleaf(*nodea);
    //	verify_local_fingerprint_nonleaf(*nodeb);
    //} else {
    //  verify_local_fingerprint_nonleaf(node);
    //}
    return 0;
}

// Whenever anything provisional is happening, it's XID must match the cmd's.

static int
brt_leaf_put_cmd_simple (BRT t, BRTNODE node, BRT_CMD cmd, TOKULOGGER logger,
			 u_int64_t *new_size /*OUT*/
			 )
// Effect: Put a cmd into a leaf.
//  Return the serialization size in *new_size.
// The leaf could end up "too big".  It is up to the caller to fix that up.
{
//    toku_pma_verify_fingerprint(node->u.l.buffer, node->rand4fingerprint, node->subtree_fingerprint);
    VERIFY_NODE(t, node);
    assert(node->height==0);

    LEAFENTRY storeddata;
    OMTVALUE storeddatav=NULL;

    u_int32_t idx;
    int r;
    int compare_both = should_compare_both_keys(node, cmd);
    struct cmd_leafval_bessel_extra be = {t, cmd, compare_both};

    //static int counter=0;
    //counter++;
    //printf("counter=%d\n", counter);

    switch (cmd->type) {
    case BRT_INSERT:
        if (node->u.l.seqinsert) {
            idx = toku_omt_size(node->u.l.buffer);
            r = toku_omt_fetch(node->u.l.buffer, idx-1, &storeddatav, NULL);
            if (r != 0) goto fz;
            storeddata = storeddatav;
            int cmp = toku_cmd_leafval_bessel(storeddata, &be);
            if (cmp >= 0) goto fz;
            r = DB_NOTFOUND;
        } else {
        fz:
            r = toku_omt_find_zero(node->u.l.buffer, toku_cmd_leafval_bessel, &be,
                                   &storeddatav, &idx, NULL);
        }
	if (r==DB_NOTFOUND) {
	    storeddata = 0;
	} else if (r!=0) {
	    return r;
	} else {
	    storeddata=storeddatav;
	}
	
	r = brt_leaf_apply_cmd_once(t, node, cmd, logger, idx, storeddata);
	if (r!=0) return r;

        // if the insertion point is within a window of the right edge of
        // the leaf then it is sequential

        // window = min(32, number of leaf entries/16)
        u_int32_t s = toku_omt_size(node->u.l.buffer);
        u_int32_t w = s / 16;
        if (w == 0) w = 1; 
        if (w > 32) w = 32;

        // within the window?
        if (s - idx <= w) {
            node->u.l.seqinsert += 1;
        } else {
            node->u.l.seqinsert = 0;
        }
	break;
    case BRT_DELETE_BOTH:
    case BRT_ABORT_BOTH:
    case BRT_COMMIT_BOTH:

	// Delete the one item
	r = toku_omt_find_zero(node->u.l.buffer, toku_cmd_leafval_bessel,  &be,
			       &storeddatav, &idx, NULL);
	if (r == DB_NOTFOUND) break;
	if (r != 0) return r;
	storeddata=storeddatav;

	VERIFY_NODE(t, node);

	static int count=0;
	count++;
	r = brt_leaf_apply_cmd_once(t, node, cmd, logger, idx, storeddata);
	if (r!=0) return r;

	VERIFY_NODE(t, node);
	break;

    case BRT_DELETE_ANY:
    case BRT_ABORT_ANY:
    case BRT_COMMIT_ANY:
	// Delete all the matches

	r = toku_omt_find_zero(node->u.l.buffer, toku_cmd_leafval_bessel, &be,
			       &storeddatav, &idx, NULL);
	if (r == DB_NOTFOUND) break;
	if (r != 0) return r;
	storeddata=storeddatav;
	    
	while (1) {
	    int   vallen   = le_any_vallen(storeddata);
	    void *save_val = toku_memdup(le_any_val(storeddata), vallen);

	    r = brt_leaf_apply_cmd_once(t, node, cmd, logger, idx, storeddata);
	    if (r!=0) return r;

	    // Now we must find the next one.
	    DBT valdbt;
	    BRT_CMD_S ncmd = { cmd->type, cmd->xid, .u.id={cmd->u.id.key, toku_fill_dbt(&valdbt, save_val, vallen)}};
	    struct cmd_leafval_bessel_extra nbe = {t, &ncmd, 1};
	    r = toku_omt_find(node->u.l.buffer, toku_cmd_leafval_bessel,  &nbe, +1,
			      &storeddatav, &idx, NULL);
	    
	    toku_free(save_val);
	    if (r!=0) break;
	    storeddata=storeddatav;
	    {   // Continue only if the next record that we found has the same key.
		DBT adbt;
		if (t->compare_fun(t->db,
				   toku_fill_dbt(&adbt, le_any_key(storeddata), le_any_keylen(storeddata)),
				   cmd->u.id.key) != 0)
		    break;
	    }
	}

	break;

    case BRT_NONE: return EINVAL;
    }
    /// All done doing the work

    node->dirty = 1;
	
//	toku_pma_verify_fingerprint(node->u.l.buffer, node->rand4fingerprint, node->subtree_fingerprint);

    VERIFY_NODE(t, node);
    *new_size = toku_serialize_brtnode_size(node);
    return 0;
}

static int brt_leaf_put_cmd (BRT t, BRTNODE node, BRT_CMD cmd,
			     int *did_split, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk,
			     TOKULOGGER logger) {
    u_int64_t leaf_size MAYBE_INIT(0);
    int r = brt_leaf_put_cmd_simple(t, node, cmd, logger, &leaf_size);
    if (r!=0) return r;
    // If it doesn't fit, then split the leaf.
    if (leaf_size > node->nodesize) {
	FILENUM filenum = toku_cachefile_filenum(t->cf);
	r = brtleaf_split (logger, filenum, t, node, nodea, nodeb, splitk);
	if (r!=0) return r;
	//printf("%s:%d splitkey=%s\n", __FILE__, __LINE__, (char*)*splitkey);
	split_count++;
	*did_split = 1;
	assert(toku_serialize_brtnode_size(*nodea)<=(*nodea)->nodesize);
	assert(toku_serialize_brtnode_size(*nodeb)<=(*nodeb)->nodesize);
	VERIFY_NODE(t, *nodea);
	VERIFY_NODE(t, *nodeb);
    } else {
	*did_split = 0;
    }
    return 0;
}

/* put a cmd into a nodes child */
static int
brt_nonleaf_put_cmd_child_node_simple (BRT t, BRTNODE node, int childnum, BOOL maybe, BRT_CMD cmd, TOKULOGGER logger,
				       BOOL *should_split /* OUT */,
				       BOOL *should_merge)
// Effect: Put CMD into the child of node.
// If MAYBE is false and the child is not in main memory, then don't do anything.
// If we return 0, then store *must_split and *must_merge appropriately.
{
    int r;
    void *child_v;
    BRTNODE child;
    int child_did_split;

    BLOCKNUM childblocknum=BNC_BLOCKNUM(node, childnum);
    u_int32_t fullhash = compute_child_fullhash(t->cf, node, childnum);
    if (maybe) 
        r = toku_cachetable_maybe_get_and_pin(t->cf, childblocknum, fullhash, &child_v);
    else 
        r = toku_cachetable_get_and_pin(t->cf, childblocknum, fullhash, &child_v, NULL, 
					toku_brtnode_flush_callback, toku_brtnode_fetch_callback, t->h);
    if (r != 0)
        return r;

    child = child_v;

    child_did_split = 0;
    r = brtnode_put_cmd_simple(t, child, cmd, logger, should_split, should_merge);
    if (r != 0) {
        /* putting to the child failed for some reason, so unpin the child and return the error code */
	int rr = toku_unpin_brtnode(t, child);
        assert(rr == 0);
        return r;
    }
    {    
	//verify_local_fingerprint_nonleaf(child);
	fixup_child_fingerprint(node, childnum, child, t, logger);
	int rr = toku_unpin_brtnode(t, child);
        assert(rr == 0);
    }
    return r;
}

/* put a cmd into a nodes child */
static int brt_nonleaf_put_cmd_child_node (BRT t, BRTNODE node, BRT_CMD cmd,
                                           int *did_split, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk, 
                                           TOKULOGGER logger, int childnum, int maybe) {
    int r;
    void *child_v;
    BRTNODE child;
    int child_did_split;
    BRTNODE childa, childb;
    DBT childsplitk;

    *did_split = 0;

    BLOCKNUM childblocknum=BNC_BLOCKNUM(node, childnum);
    u_int32_t fullhash = compute_child_fullhash(t->cf, node, childnum);
    if (maybe) 
        r = toku_cachetable_maybe_get_and_pin(t->cf, childblocknum, fullhash, &child_v);
    else 
        r = toku_cachetable_get_and_pin(t->cf, childblocknum, fullhash, &child_v, NULL, 
					toku_brtnode_flush_callback, toku_brtnode_fetch_callback, t->h);
    if (r != 0)
        return r;

    child = child_v;

    child_did_split = 0;
    r = brtnode_put_cmd(t, child, cmd,
                        &child_did_split, &childa, &childb, &childsplitk, logger);
    if (r != 0) {
        /* putting to the child failed for some reason, so unpin the child and return the error code */
	int rr = toku_unpin_brtnode(t, child);
        assert(rr == 0);
        return r;
    }
    if (child_did_split) {
        if (0) printf("brt_nonleaf_insert child_split %p\n", child);
        r = handle_split_of_child(t, node, childnum,
                                  childa, childb, &childsplitk,
                                  did_split, nodea, nodeb, splitk,
                                  logger);
        assert(r == 0);
    } else {
	//verify_local_fingerprint_nonleaf(child);
	fixup_child_fingerprint(node, childnum, child, t, logger);
	int rr = toku_unpin_brtnode(t, child);
        assert(rr == 0);
    }
    return r;
}

int toku_brt_do_push_cmd = 1;

/* put a cmd into a node at childnum */
static int brt_nonleaf_put_cmd_child (BRT t, BRTNODE node, BRT_CMD cmd,
                                      int *did_split, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk,
                                      TOKULOGGER logger, unsigned int childnum, int can_push, int *do_push_down) {
    //verify_local_fingerprint_nonleaf(node);

    /* try to push the cmd to the subtree if the buffer is empty and pushes are enabled */
    if (BNC_NBYTESINBUF(node, childnum) == 0 && can_push && toku_brt_do_push_cmd) {
        int r = brt_nonleaf_put_cmd_child_node(t, node, cmd, did_split, nodea, nodeb, splitk, logger, childnum, 1);
        if (r == 0)
            return r;
    }
    //verify_local_fingerprint_nonleaf(node);

    /* append the cmd to the child buffer */
    {
        int type = cmd->type;
        DBT *k = cmd->u.id.key;
        DBT *v = cmd->u.id.val;

	int r = log_and_save_brtenq(logger, t, node, childnum, cmd->xid, type, k->data, k->size, v->data, v->size, &node->local_fingerprint);
	if (r!=0) return r;
	int diff = k->size + v->size + KEY_VALUE_OVERHEAD + BRT_CMD_OVERHEAD;
        r=toku_fifo_enq(BNC_BUFFER(node,childnum), k->data, k->size, v->data, v->size, type, cmd->xid);
	assert(r==0);
	node->u.n.n_bytes_in_buffers += diff;
	BNC_NBYTESINBUF(node, childnum) += diff;
        node->dirty = 1;
    }
    *do_push_down = 1;
    return 0;
}

static int
merge (void) {
    static int printcount=0;
    printcount++;
    if (0==(printcount & (printcount-1))) {// is printcount a power of two?
	printf("%s:%d %s not ready (%d invocations)\n", __FILE__, __LINE__, __func__, printcount);
    }
    return 0;
}

static inline int
brt_serialize_size_of_child (BRT t, BRTNODE node, int childnum, int *fanout) {
    assert(node->height>0);
    BLOCKNUM childblocknum = BNC_BLOCKNUM(node, childnum);
    u_int32_t fullhash = compute_child_fullhash(t->cf, node, childnum);
    void *childnode_v;
    int r = toku_cachetable_get_and_pin(t->cf, childblocknum, fullhash, &childnode_v, NULL, toku_brtnode_flush_callback, toku_brtnode_fetch_callback, t->h);
    BRTNODE childnode = childnode_v;
    int size = toku_serialize_brtnode_size(childnode);
    assert(r==0);
    *fanout = (childnode->height==0) ? 0 : childnode->u.n.n_children;
    r = toku_cachetable_unpin(t->cf, childnode->thisnodename, childnode->fullhash, 0, brtnode_memory_size(childnode));
    assert(r==0);
    return size;
}

// Split or merge the child, if the child too large or too small.
// Return the new fanout of node.
static int
brt_nonleaf_maybe_split_or_merge (BRT t, BRTNODE node, int childnum, BOOL should_split, BOOL should_merge, TOKULOGGER logger, u_int32_t *new_fanout) {
    //printf("%s:%d Node %" PRIu64 " is size %d, child %d is Node %" PRIu64 " size is %d\n", __FILE__, __LINE__, node->thisnodename.b, toku_serialize_brtnode_size(node), childnum, BNC_BLOCKNUM(node, childnum).b, brt_serialize_size_of_child(t, node, childnum));
    assert(!(should_split && should_merge));
    if (should_split) { int r = brt_split_child(t, node, childnum, logger); if (r!=0) return r; }
    if (should_merge) { int r = merge(); if (r!=0) return r; }
    *new_fanout = node->u.n.n_children;
    return 0;
}

/* Put a cmd into a node at childnum */
/* May result in the data being pushed to a child.
 * Which may cause that child to split, which may cause the fanout to become larger.
 * Return the new fanout. */
static int
brt_nonleaf_put_cmd_child_simple (BRT t, BRTNODE node, unsigned int childnum, BRT_CMD cmd, TOKULOGGER logger, u_int32_t *new_fanout) {
    //verify_local_fingerprint_nonleaf(node);

    /* Push the cmd to the subtree if the buffer is empty */
    //printf("%s:%d %s\n",__FILE__,__LINE__,__func__);
    if (BNC_NBYTESINBUF(node, childnum) == 0) {
	BOOL must_split MAYBE_INIT(FALSE);
	BOOL must_merge MAYBE_INIT(FALSE);
	//printf("%s:%d fix up fingerprint?\n", __FILE__, __LINE__);
        int r = brt_nonleaf_put_cmd_child_node_simple(t, node, childnum, TRUE, cmd, logger, &must_split, &must_merge);
	//printf("%s:%d Put in child, must_split=%d must_merge=%d\n", __FILE__, __LINE__, must_split, must_merge);
	if (r==0) {
	    return brt_nonleaf_maybe_split_or_merge(t, node, childnum, must_split, must_merge, logger, new_fanout);
	}
	// Otherwise fall out and append it to the child buffer.
	//printf("%s:%d fall out\n", __FILE__, __LINE__);
    }
    //verify_local_fingerprint_nonleaf(node);

    /* append the cmd to the child buffer */
    {
        int type = cmd->type;
        DBT *k = cmd->u.id.key;
        DBT *v = cmd->u.id.val;

	int r = log_and_save_brtenq(logger, t, node, childnum, cmd->xid, type, k->data, k->size, v->data, v->size, &node->local_fingerprint);
	if (r!=0) return r;
	int diff = k->size + v->size + KEY_VALUE_OVERHEAD + BRT_CMD_OVERHEAD;
        r=toku_fifo_enq(BNC_BUFFER(node,childnum), k->data, k->size, v->data, v->size, type, cmd->xid);
	assert(r==0);
	node->u.n.n_bytes_in_buffers += diff;
	BNC_NBYTESINBUF(node, childnum) += diff;
        node->dirty = 1;
    }
    if (toku_serialize_brtnode_size(node) > node->nodesize) {
	int biggest_child;
	BOOL must_split MAYBE_INIT(FALSE);
	BOOL must_merge MAYBE_INIT(FALSE);
	find_heaviest_child(node, &biggest_child);
	{
	    int cfan;
	    int csize;
	    csize = brt_serialize_size_of_child(t, node, biggest_child, &cfan);
	    if (0) printf("%s:%d Node %" PRIu64 " fanout=%d Pushing into child %d (Node %" PRIu64 ", size=%d, fanout=%d estimate=%" PRId64 ")\n", __FILE__, __LINE__,
			  node->thisnodename.b, node->u.n.n_children,
			  biggest_child, BNC_BLOCKNUM(node, biggest_child).b, csize, cfan, BNC_SUBTREE_LEAFENTRY_ESTIMATE(node, biggest_child));
	}
	// printf("%s:%d fix up fingerprint?\n", __FILE__, __LINE__);
	int r = push_some_brt_cmds_down_simple(t, node, biggest_child, &must_split, &must_merge, logger);
	if (r!=0) return r;
	return brt_nonleaf_maybe_split_or_merge(t, node, biggest_child, must_split, must_merge, logger, new_fanout);
    }
    *new_fanout = node->u.n.n_children;
    if (0) {
	printf("%s:%d Done pushing Node %" PRIu64 " n_children=%d: estimates=", __FILE__, __LINE__, node->thisnodename.b, node->u.n.n_children);
	int i;
	int64_t total=0;
	for (i=0; i<node->u.n.n_children; i++) {
	    int64_t v = BNC_SUBTREE_LEAFENTRY_ESTIMATE(node, i);
	    total+=v;
	    printf(" %" PRId64, v);
	}
	printf(" total=%" PRId64 " \n", total);
    }
    return 0;
}

static int
brt_nonleaf_cmd_once_simple (BRT t, BRTNODE node, BRT_CMD cmd, TOKULOGGER logger,
			     u_int32_t *new_fanout) {
    //verify_local_fingerprint_nonleaf(node);

    /* find the right subtree */
    unsigned int childnum = toku_brtnode_which_child(node, cmd->u.id.key, cmd->u.id.val, t);

    /* put the cmd in the subtree */
    return brt_nonleaf_put_cmd_child_simple(t, node, childnum, cmd, logger, new_fanout);
}


/* delete in all subtrees starting from the left most one which contains the key */

/* delete in all subtrees starting from the left most one which contains the key */
static int brt_nonleaf_cmd_many (BRT t, BRTNODE node, BRT_CMD cmd,
				 int *did_split, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk,
				 TOKULOGGER logger) {
    int r;

    /* find all children that need a copy of the command */
    int sendchild[TREE_FANOUT], delidx = 0;
#define sendchild_append(i) \
        if (delidx == 0 || sendchild[delidx-1] != i) sendchild[delidx++] = i;
    int i;
    for (i = 0; i < node->u.n.n_children-1; i++) {
        int cmp = brt_compare_pivot(t, cmd->u.id.key, 0, node->u.n.childkeys[i]);
        if (cmp > 0) {
            continue;
        } else if (cmp < 0) {
            sendchild_append(i);
            break;
        } else if (t->flags & TOKU_DB_DUPSORT) {
            sendchild_append(i);
            sendchild_append(i+1);
        } else {
            sendchild_append(i);
            break;
        }
    }

    if (delidx == 0)
        sendchild_append(node->u.n.n_children-1);
#undef sendchild_append

    /* issue the to all of the children found previously */
    int do_push_down = 0;
    for (i=0; i<delidx; i++) {
        r = brt_nonleaf_put_cmd_child(t, node, cmd, did_split, nodea, nodeb, splitk, logger, sendchild[i], delidx == 1, &do_push_down);
        assert(r == 0);
    }

    if (do_push_down) {
        /* maybe push down */
        //verify_local_fingerprint_nonleaf(node);
        r = brtnode_maybe_push_down(t, node, did_split, nodea, nodeb, splitk, logger);
        if (r!=0) return r;
        if (*did_split) {
            assert(toku_serialize_brtnode_size(*nodea)<=(*nodea)->nodesize);
            assert(toku_serialize_brtnode_size(*nodeb)<=(*nodeb)->nodesize);
            assert((*nodea)->u.n.n_children>0);
            assert((*nodeb)->u.n.n_children>0);
            assert(BNC_BLOCKNUM(*nodea,(*nodea)->u.n.n_children-1).b!=0);
            assert(BNC_BLOCKNUM(*nodeb,(*nodeb)->u.n.n_children-1).b!=0);
        } else {
            assert(toku_serialize_brtnode_size(node)<=node->nodesize);
        }
        //if (*did_split) {
        //	verify_local_fingerprint_nonleaf(*nodea);
        //	verify_local_fingerprint_nonleaf(*nodeb);
        //} else {
        //	verify_local_fingerprint_nonleaf(node);
        //}
    }
    return 0;
}

static int brt_nonleaf_put_cmd (BRT t, BRTNODE node, BRT_CMD cmd,
				int *did_split, BRTNODE *nodea, BRTNODE *nodeb,
				DBT *splitk,
				TOKULOGGER logger) {
    switch (cmd->type) {
    case BRT_INSERT:
    case BRT_DELETE_BOTH:
    case BRT_ABORT_BOTH:
    case BRT_COMMIT_BOTH:
    do_once:
        return brt_nonleaf_cmd_once(t, node, cmd, did_split, nodea, nodeb, splitk, logger);
    case BRT_DELETE_ANY:
    case BRT_ABORT_ANY:
    case BRT_COMMIT_ANY:
	if (0 == (node->flags & TOKU_DB_DUPSORT)) goto do_once; // nondupsort delete_any is just do once.
        return brt_nonleaf_cmd_many(t, node, cmd, did_split, nodea, nodeb, splitk, logger);
    case BRT_NONE:
	break;
    }
    return EINVAL;
}

static int brtnode_put_cmd (BRT t, BRTNODE node, BRT_CMD cmd,
			    int *did_split, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk,
			    TOKULOGGER logger) {
    //static int counter=0; // FOO
    //static int oldcounter=0;
    //int        tmpcounter;
    //u_int32_t  oldfingerprint=node->local_fingerprint;
    int r;
    //counter++; tmpcounter=counter;
    if (node->height==0) {
//	toku_pma_verify_fingerprint(node->u.l.buffer, node->rand4fingerprint, node->subtree_fingerprint);
	r = brt_leaf_put_cmd(t, node, cmd,
			     did_split, nodea, nodeb, splitk,
			     logger);
    } else {
	r = brt_nonleaf_put_cmd(t, node, cmd,
				did_split, nodea, nodeb, splitk,
				logger);
    }
    //oldcounter=tmpcounter;
    // Watch out.  If did_split then the original node is no longer allocated.
    if (*did_split) {
	assert(toku_serialize_brtnode_size(*nodea)<=(*nodea)->nodesize);
	assert(toku_serialize_brtnode_size(*nodeb)<=(*nodeb)->nodesize);
//	if ((*nodea)->height==0) {
//	    toku_pma_verify_fingerprint((*nodea)->u.l.buffer, (*nodea)->rand4fingerprint, (*nodea)->subtree_fingerprint);
//	    toku_pma_verify_fingerprint((*nodeb)->u.l.buffer, (*nodeb)->rand4fingerprint, (*nodeb)->subtree_fingerprint);
//	}
    } else {
	assert(toku_serialize_brtnode_size(node)<=node->nodesize);
//	if (node->height==0) {
//	    toku_pma_verify_fingerprint(node->u.l.buffer, node->rand4fingerprint, node->local_fingerprint);
//	} else {
//	    verify_local_fingerprint_nonleaf(node);
//	}
    }
    //if (node->local_fingerprint==3522421844U) {
//    if (*did_split) {
//	verify_local_fingerprint_nonleaf(*nodea);
//	verify_local_fingerprint_nonleaf(*nodeb);
//    }
    return r;
}

int toku_brt_get_fd(BRT brt, int *fdp) {
    *fdp = toku_cachefile_fd(brt->cf);
    return 0;
}

int toku_brt_reopen(BRT brt, const char *fname, const char *fname_in_env, TOKUTXN txn) {
    int r;

    // create a new file
    int fd = -1;
    r = brt_open_file(brt, fname, fname_in_env, TRUE, txn, &fd);
    if (r != 0) return r;

    // set the cachefile
    r = toku_cachefile_set_fd(brt->cf, fd, fname_in_env);
    assert(r == 0);
    brt->h = 0;  // set_fd should close the header
    toku_logger_log_fopen(txn, fname_in_env, toku_cachefile_filenum(brt->cf));

    // init the tree header
    r = toku_read_brt_header_and_store_in_cachefile(brt->cf, &brt->h);
    if (r == -1) {
        r = brt_alloc_init_header(brt, NULL, txn);
    }
    return r;
}

int toku_brt_remove_subdb(BRT brt, const char *dbname, u_int32_t flags) {
    int i;
    int found = -1;

    assert(flags == 0);
    assert(brt->h);

    assert(brt->h->n_named_roots>=0);
    for (i = 0; i < brt->h->n_named_roots; i++) {
        if (strcmp(brt->h->names[i], dbname) == 0) {
            found = i;
            break;
        }
    }
    if (found == -1) {
        //Should not be possible.
        return ENOENT;
    }
    //Free old db name
    toku_free(brt->h->names[found]);
    //TODO: Free Diskblocks including root
    
    for (i = found + 1; i < brt->h->n_named_roots; i++) {
        brt->h->names[i - 1]       = brt->h->names[i];
        brt->h->roots[i - 1]       = brt->h->roots[i];
	brt->h->root_hashes[i - 1] = brt->h->root_hashes[i];
    }
    brt->h->n_named_roots--;
    brt->h->dirty = 1;
    // Q: What if n_named_roots becomes 0?  A:  Don't do anything.  an empty list of named roots is OK.
    XREALLOC_N(brt->h->n_named_roots, brt->h->names);
    XREALLOC_N(brt->h->n_named_roots, brt->h->roots);
    XREALLOC_N(brt->h->n_named_roots, brt->h->root_hashes);
    return 0;

}

int toku_brt_flush (BRT brt) {
    return toku_cachefile_flush(brt->cf);
}

//strcmp(key,"hello387")==0;

static int push_something_simple(BRT brt, BRTNODE *nodep, CACHEKEY *rootp, BRT_CMD cmd, TOKULOGGER logger) {
    BRTNODE node = *nodep;
    BOOL should_split =-1;
    BOOL should_merge =-1;
    {
	int r = brtnode_put_cmd_simple(brt, node, cmd, logger, &should_split, &should_merge);
	if (r!=0) return r;
	//if (should_split) printf("%s:%d Pushed something simple, should_split=1\n", __FILE__, __LINE__); 

    }
    assert(should_split!=(BOOL)-1 && should_merge!=(BOOL)-1);
    assert(!(should_split && should_merge));
    //printf("%s:%d should_split=%d node_size=%" PRIu64 "\n", __FILE__, __LINE__, should_split, brtnode_memory_size(node));
    if (should_split) {
	BRTNODE nodea,nodeb;
	DBT splitk;
	if (node->height==0) {
	    int r = brtleaf_split(logger, toku_cachefile_filenum(brt->cf), brt, node, &nodea, &nodeb, &splitk);
	    if (r!=0) return r;
	} else {
	    int r = brt_nonleaf_split(brt, node, &nodea, &nodeb, &splitk, logger);
	    if (r!=0) return r;
	}
	return brt_init_new_root(brt, nodea, nodeb, splitk, rootp, logger, nodep);
    } else if (should_merge) {
	return 0; // Cannot merge anything at the root, so return happy.
    } else {
	return 0;
    }
}

#if 0
static int push_something(BRT brt, BRTNODE *nodep, CACHEKEY *rootp, BRT_CMD cmd, TOKULOGGER logger) {
    int did_split = 0;
    BRTNODE nodea=0, nodeb=0;
    DBT splitk;
    int result = brtnode_put_cmd(brt, *nodep, cmd,
				 &did_split, &nodea, &nodeb, &splitk,
				 logger);
    int r;
    if (did_split) {
	// node is unpinned, so now we have to proceed to update the root with a new node.

	//printf("%s:%d did_split=%d nodeb=%p nodeb->thisnodename=%lld nodeb->nodesize=%d\n", __FILE__, __LINE__, did_split, nodeb, nodeb->thisnodename, nodeb->nodesize);
	//printf("Did split, splitkey=%s\n", splitkey);
	if (nodeb->height>0) assert(BNC_BLOCKNUM(nodeb,nodeb->u.n.n_children-1).b!=0);
	assert(nodeb->nodesize>0);
        r = brt_init_new_root(brt, nodea, nodeb, splitk, rootp, logger, nodep);
        assert(r == 0);
    } else {
	if ((*nodep)->height>0)
	    assert((*nodep)->u.n.n_children<=TREE_FANOUT);
    }
    //assert(0==toku_cachetable_assert_all_unpinned(brt->cachetable));
    return result;
}
#endif

int toku_brt_root_put_cmd(BRT brt, BRT_CMD cmd, TOKULOGGER logger) {
    void *node_v;
    BRTNODE node;
    CACHEKEY *rootp;
    int r;
    //assert(0==toku_cachetable_assert_all_unpinned(brt->cachetable));
    assert(brt->h);

    brt->h->root_put_counter = global_root_put_counter++;
    u_int32_t fullhash;
    rootp = toku_calculate_root_offset_pointer(brt, &fullhash);
    //assert(fullhash==toku_cachetable_hash(brt->cf, *rootp));
    if ((r=toku_cachetable_get_and_pin(brt->cf, *rootp, fullhash, &node_v, NULL, 
				       toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt->h))) {
	return r;
    }
    //printf("%s:%d pin %p\n", __FILE__, __LINE__, node_v);
    node=node_v;

    assert(node->fullhash==fullhash);
    // push the fifo stuff
    {
	DBT okey,odata;
	BRT_CMD_S ocmd;
	while (0==toku_fifo_peek_cmdstruct(brt->h->fifo, &ocmd,  &okey, &odata)) {
	    if ((r = push_something_simple(brt, &node, rootp, &ocmd, logger))) return r;
	    r = toku_fifo_deq(brt->h->fifo);
	    assert(r==0);
	}
    }

    if ((r = push_something_simple(brt, &node, rootp, cmd, logger))) return r;
    r = toku_unpin_brtnode(brt, node);
    assert(r == 0);
    return 0;
}

int toku_brt_insert (BRT brt, DBT *key, DBT *val, TOKUTXN txn) {
    int r;
    if (txn && (brt->txn_that_created != toku_txn_get_txnid(txn))) {
	toku_cachefile_refup(brt->cf);
	BYTESTRING keybs  = {key->size, toku_memdup_in_rollback(txn, key->data, key->size)};
	BYTESTRING databs = {val->size, toku_memdup_in_rollback(txn, val->data, val->size)};
	r = toku_logger_save_rollback_cmdinsert(txn, toku_txn_get_txnid(txn), toku_cachefile_filenum(brt->cf), keybs, databs);
	if (r!=0) return r;
	r = toku_txn_note_brt(txn, brt);
	if (r!=0) return r;
    }
    BRT_CMD_S brtcmd = { BRT_INSERT, toku_txn_get_txnid(txn), .u.id={key,val}};
    r = toku_brt_root_put_cmd(brt, &brtcmd, toku_txn_logger(txn));
    if (r!=0) return r;
    return r;
}

int toku_brt_delete_both(BRT brt, DBT *key, DBT *val, TOKUTXN txn) {
    //{ unsigned i; printf("del %p keylen=%d key={", brt->db, key->size); for(i=0; i<key->size; i++) printf("%d,", ((char*)key->data)[i]); printf("} datalen=%d data={", val->size); for(i=0; i<val->size; i++) printf("%d,", ((char*)val->data)[i]); printf("}\n"); }
    int r;
    if (txn && (brt->txn_that_created != toku_txn_get_txnid(txn))) {
	BYTESTRING keybs  = {key->size, toku_memdup_in_rollback(txn, key->data, key->size)};
	BYTESTRING databs = {val->size, toku_memdup_in_rollback(txn, val->data, val->size)};
	toku_cachefile_refup(brt->cf);
	r = toku_logger_save_rollback_cmddeleteboth(txn, toku_txn_get_txnid(txn), toku_cachefile_filenum(brt->cf), keybs, databs);
	if (r!=0) return r;
	r = toku_txn_note_brt(txn, brt);
	if (r!=0) return r;
    }
    BRT_CMD_S brtcmd = { BRT_DELETE_BOTH, toku_txn_get_txnid(txn), .u.id={key,val}};
    r = toku_brt_root_put_cmd(brt, &brtcmd, toku_txn_logger(txn));
    return r;
}

#if 0
static int show_brtnode_blocknumbers (BRT brt, DISKOFF off) {
    BRTNODE node;
    void *node_v;
    int i,r;
    assert(off%brt->h->nodesize==0);
    if ((r = toku_cachetable_get_and_pin(brt->cf, off, &node_v, NULL,
				    toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt->h))) {
	if (0) { died0: toku_cachetable_unpin(brt->cf, off, 0, 0); }
	return r;
    }
    printf("%s:%d pin %p\n", __FILE__, __LINE__, node_v);
    node=node_v;
    printf(" %lld", off/brt->h->nodesize);
    if (node->height>0) {
	for (i=0; i<node->u.n.n_children; i++) {
	    if ((r=show_brtnode_blocknumbers(brt, BNC_BLOCKNUM(node, i)))) goto died0;
	}
    }
    r = toku_cachetable_unpin(brt->cf, off, 0, 0);
    return r;
}

int show_brt_blocknumbers (BRT brt) {
    int r;
    CACHEKEY *rootp;
    if ((r = toku_read_and_pin_brt_header(brt->cf, &brt->h))) {
	if (0) { died0: toku_unpin_brt_header(brt); }
	return r;
    }
    rootp = toku_calculate_root_offset_pointer(brt);
    printf("BRT %p has blocks:", brt);
    if ((r=show_brtnode_blocknumbers (brt, *rootp, 0))) goto died0;
    printf("\n");
    if ((r = toku_unpin_brt_header(brt))!=0) return r;
    return 0;
}
#endif

typedef struct brt_split {
    int did_split;
    BRTNODE nodea;
    BRTNODE nodeb;
    DBT splitk;
} BRT_SPLIT;

static inline void brt_split_init(BRT_SPLIT *split) {
    split->did_split = 0;
    split->nodea = split->nodeb = 0;
    toku_init_dbt(&split->splitk);
}

static int brt_search_node(BRT brt, BRTNODE node, brt_search_t *search, DBT *newkey, DBT *newval, BRT_SPLIT *split, TOKULOGGER logger, OMTCURSOR);


BOOL toku_brt_cursor_uninitialized(BRT_CURSOR c) {
    return brt_cursor_not_set(c);
}

DBT *brt_cursor_peek_prev_key(BRT_CURSOR cursor)
// Effect: Return a pointer to a DBT for the previous key.  
// Requires:  The caller may not modify that DBT or the memory at which it points.
{
    return &cursor->prevkey;
}

DBT *brt_cursor_peek_prev_val(BRT_CURSOR cursor)
// Effect: Return a pointer to a DBT for the previous val
// Requires:  The caller may not modify that DBT or the memory at which it points.
{
    return &cursor->prevval;
}

void brt_cursor_peek_current(BRT_CURSOR cursor, const DBT **pkey, const DBT **pval)
// Effect: Retrieves a pointer to the DBTs for the current key and value.
// Requires:  The caller may not modify the DBTs or the memory at which they points.
{
    if (cursor->current_in_omt) load_dbts_from_omt(cursor, &cursor->key, &cursor->val);
    *pkey = &cursor->key;
    *pval = &cursor->val;
}

DBT *brt_cursor_peek_current_key(BRT_CURSOR cursor)
// Effect: Return a pointer to a DBT for the current key.  
// Requires:  The caller may not modify that DBT or the memory at which it points.
{
    if (cursor->current_in_omt) load_dbts_from_omt(cursor, &cursor->key, NULL);
    return &cursor->key;
}

DBT *brt_cursor_peek_current_val(BRT_CURSOR cursor)
// Effect: Return a pointer to a DBT for the current val
// Requires:  The caller may not modify that DBT or the memory at which it points.
{
    if (cursor->current_in_omt) load_dbts_from_omt(cursor, NULL, &cursor->val);
    return &cursor->val;
}

int toku_brt_dbt_set(DBT* key, DBT* key_source) {
    int r = toku_dbt_set_value(key, (bytevec*)&key_source->data, key_source->size, NULL, FALSE);
    return r;
}

int toku_brt_cursor_dbts_set(BRT_CURSOR cursor,
                        DBT* key, DBT* key_source, BOOL key_disposable,
                        DBT* val, DBT* val_source, BOOL val_disposable) {
    void** key_staticp = cursor->is_temporary_cursor ? &cursor->brt->skey : &cursor->skey;
    void** val_staticp = cursor->is_temporary_cursor ? &cursor->brt->sval : &cursor->sval;
    int r;
    r = toku_dbt_set_two_values(key, (bytevec*)&key_source->data, key_source->size, key_staticp, key_disposable,
                                val, (bytevec*)&val_source->data, val_source->size, val_staticp, val_disposable);
    return r;
}

int toku_brt_cursor_dbts_set_with_dat(BRT_CURSOR cursor, BRT pdb,
                                      DBT* key, DBT* key_source, BOOL key_disposable,
                                      DBT* val, DBT* val_source, BOOL val_disposable,
                                      DBT* dat, DBT* dat_source, BOOL dat_disposable) {
    void** key_staticp = cursor->is_temporary_cursor ? &cursor->brt->skey : &cursor->skey;
    void** val_staticp = cursor->is_temporary_cursor ? &cursor->brt->sval : &cursor->sval;
    void** dat_staticp = &pdb->sval;
    int r;
    r = toku_dbt_set_three_values(key, (bytevec*)&key_source->data, key_source->size, key_staticp, key_disposable,
                                  val, (bytevec*)&val_source->data, val_source->size, val_staticp, val_disposable,
                                  dat, (bytevec*)&dat_source->data, dat_source->size, dat_staticp, dat_disposable);
    return r;
}

void brt_cursor_restore_state_from_prev(BRT_CURSOR cursor) {
    toku_omt_cursor_invalidate(cursor->omtcursor);
    swap_cursor_dbts(cursor);
}

int toku_brt_cursor_peek_prev(BRT_CURSOR cursor, DBT *outkey, DBT *outval) {
    if (toku_omt_cursor_is_valid(cursor->omtcursor)) {
	{
	    assert(cursor->brt->h);
	    u_int64_t h_counter = cursor->brt->h->root_put_counter;
	    if (h_counter != cursor->root_put_counter) return -1;
	}
	OMTVALUE le;
        u_int32_t index = 0;
        int r = toku_omt_cursor_current_index(cursor->omtcursor, &index);
        assert(r==0);
        OMT omt = toku_omt_cursor_get_omt(cursor->omtcursor);
get_prev:;
        if (index>0) {
            r = toku_omt_fetch(omt, --index, &le, NULL); 
            if (r==0) {
                if (le_is_provdel(le)) goto get_prev;
                toku_fill_dbt(outkey, le_latest_key(le), le_latest_keylen(le));
                toku_fill_dbt(outval, le_latest_val(le), le_latest_vallen(le));
                return 0;
            }
        }
    }
    return -1;
}

int toku_brt_cursor_peek_next(BRT_CURSOR cursor, DBT *outkey, DBT *outval) {
    if (toku_omt_cursor_is_valid(cursor->omtcursor)) {
	{
	    assert(cursor->brt->h);
	    u_int64_t h_counter = cursor->brt->h->root_put_counter;
	    if (h_counter != cursor->root_put_counter) return -1;
	}
	OMTVALUE le;
        u_int32_t index = UINT32_MAX;
        int r = toku_omt_cursor_current_index(cursor->omtcursor, &index);
        assert(r==0);
        OMT omt = toku_omt_cursor_get_omt(cursor->omtcursor);
get_next:;
        if (++index<toku_omt_size(omt)) {
            r = toku_omt_fetch(omt, index, &le, NULL); 
            if (r==0) {
                if (le_is_provdel(le)) goto get_next;
                toku_fill_dbt(outkey, le_latest_key(le), le_latest_keylen(le));
                toku_fill_dbt(outval, le_latest_val(le), le_latest_vallen(le));
                return 0;
            }
        }
    }
    return -1;
}

int toku_brt_cursor_after(BRT_CURSOR cursor, DBT *key, DBT *val, DBT *outkey, DBT *outval, TOKUTXN txn) {
    TOKULOGGER logger = toku_txn_logger(txn);
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_next, BRT_SEARCH_LEFT, key, val, cursor->brt);
    return brt_cursor_search(cursor, &search, outkey, outval, logger);
}

int toku_brt_cursor_before(BRT_CURSOR cursor, DBT *key, DBT *val, DBT *outkey, DBT *outval, TOKUTXN txn) {
    TOKULOGGER logger = toku_txn_logger(txn);
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_prev, BRT_SEARCH_RIGHT, key, val, cursor->brt);
    return brt_cursor_search(cursor, &search, outkey, outval, logger);
}

static int brt_cursor_compare_heavi(brt_search_t *search, DBT *x, DBT *y) {
    HEAVI_WRAPPER wrapper = search->context; 
    int r = wrapper->h(x, y, wrapper->extra_h);
    // wrapper->r_h must have the same signus as the final chosen element.
    // it is initialized to -1 or 1.  0's are closer to the min (max) that we
    // want so once we hit 0 we keep it.
    if (r==0) wrapper->r_h = 0;
    return (search->direction&BRT_SEARCH_LEFT) ? r>=0 : r<=0;
}

//We pass in toku_dbt_fake to the search functions, since it will not pass the
//key(or val) to the heaviside function if key(or val) is NULL. 
//It is not used for anything else,
//the actual 'extra' information for the heaviside function is inside the
//wrapper.
static const DBT __toku_dbt_fake;
static const DBT* const toku_dbt_fake = &__toku_dbt_fake;

int toku_brt_cursor_get_heavi (BRT_CURSOR cursor, DBT *outkey, DBT *outval, TOKUTXN txn, int direction, HEAVI_WRAPPER wrapper) {
    TOKULOGGER logger = toku_txn_logger(txn);
    brt_search_t search; brt_search_init(&search, brt_cursor_compare_heavi,
                                         direction < 0 ? BRT_SEARCH_RIGHT : BRT_SEARCH_LEFT,
                                         (DBT*)toku_dbt_fake,
                                         cursor->brt->flags & TOKU_DB_DUPSORT ? (DBT*)toku_dbt_fake : NULL,
                                         wrapper);
    return brt_cursor_search(cursor, &search, outkey, outval, logger);
}
int toku_brt_height_of_root(BRT brt, int *height) {
    // for an open brt, return the current height.
    int r;
    assert(brt->h);
    u_int32_t fullhash;
    CACHEKEY *rootp = toku_calculate_root_offset_pointer(brt, &fullhash);
    void *node_v;
    //assert(fullhash == toku_cachetable_hash(brt->cf, *rootp));
    if ((r=toku_cachetable_get_and_pin(brt->cf, *rootp, fullhash, &node_v, NULL, 
				       toku_brtnode_flush_callback, toku_brtnode_fetch_callback, brt->h))) {
	return r;
    }
    BRTNODE node = node_v;
    *height = node->height;
    r = toku_unpin_brtnode(brt, node);   assert(r==0);
    return 0;
}

int toku_brt_get_cursor_count (BRT brt) {
    int n = 0;
    struct list *list;
    for (list = brt->cursors.next; list != &brt->cursors; list = list->next)
        n += 1;
    return n;
}

