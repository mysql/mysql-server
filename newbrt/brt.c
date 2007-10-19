/* -*- mode: C; c-basic-offset: 4 -*- */
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

#include "brttypes.h"
#include "brt.h"
#include "memory.h"
#include "brt-internal.h"
#include "cachetable.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

const BRTNODE null_brtnode=0;

extern long long n_items_malloced;

/* Frees a node, including all the stuff in the hash table. */
void brtnode_free (BRTNODE *nodep) {
    BRTNODE node=*nodep;
    int i;
    //printf("%s:%d %p->mdict[0]=%p\n", __FILE__, __LINE__, node, node->mdicts[0]);
    if (node->height>0) {
	for (i=0; i<node->u.n.n_children-1; i++) {
	    toku_free((void*)node->u.n.childkeys[i]);
	}
	for (i=0; i<node->u.n.n_children; i++) {
	    if (node->u.n.htables[i]) {
		toku_hashtable_free(&node->u.n.htables[i]);
	    }
            assert(node->u.n.n_cursors[i] == 0);
	}
    } else {
	if (node->u.l.buffer) // The buffer may have been freed already, in some cases.
	    pma_free(&node->u.l.buffer);
    }
    toku_free(node);
    *nodep=0;
}

long brtnode_size(BRTNODE node) {
    long size;
    assert(node->tag == TYP_BRTNODE);
    if (node->height > 0)
        size = node->u.n.n_bytes_in_hashtables;
    else
        size = node->u.l.n_bytes_in_buffer;
    return size;
}

void fix_up_parent_pointers_of_children (BRT t, BRTNODE node) {
    int i;
    assert(node->height>0);
    for (i=0; i<node->u.n.n_children; i++) {
	void *v;
	int r = cachetable_maybe_get_and_pin(t->cf, node->u.n.children[i], &v);
	if (r==0) {
	    BRTNODE child = v;
	    //printf("%s:%d pin %p\n", __FILE__, __LINE__, v);
	    child->parent_brtnode = node;
	    r=cachetable_unpin(t->cf, node->u.n.children[i], child->dirty, brtnode_size(child));
	}
    }
}

void fix_up_parent_pointers_of_children_now_that_parent_is_gone (CACHEFILE cf, BRTNODE node) {
    int i;
    if (node->height==0) return;
    for (i=0; i<node->u.n.n_children; i++) {
	void *v;
	int r = cachetable_maybe_get_and_pin(cf, node->u.n.children[i], &v);
	if (r==0) {
	    BRTNODE child = v;
	    //printf("%s:%d pin %p\n", __FILE__, __LINE__, v);
	    child->parent_brtnode = 0;
	    r=cachetable_unpin(cf, node->u.n.children[i], child->dirty, brtnode_size(child));
	}
    }
}


void brtnode_flush_callback (CACHEFILE cachefile, diskoff nodename, void *brtnode_v, long size __attribute((unused)), int write_me, int keep_me) {
    BRTNODE brtnode = brtnode_v;
    if (0) {
	printf("%s:%d brtnode_flush_callback %p keep_me=%d height=%d", __FILE__, __LINE__, brtnode, keep_me, brtnode->height);
	if (brtnode->height==0) printf(" pma=%p", brtnode->u.l.buffer);
	printf("\n");
    }
    fix_up_parent_pointers_of_children_now_that_parent_is_gone(cachefile, brtnode);
    assert(brtnode->thisnodename==nodename);
    {
	BRTNODE parent = brtnode->parent_brtnode;
	//printf("%s:%d Looking at %p (offset=%lld) tag=%d parent=%p height=%d\n", __FILE__, __LINE__, brtnode, nodename, brtnode->tag, parent, brtnode->height);
	if (parent!=0) {
	    /* make sure we are one of the children of the parent. */
	    int i;
	    //int pheight=0;//parent->height;
	    //int nc = 0;//parent->u.n.n_children;
	    //printf("%s:%d parent height=%d has %d children: The first few are", __FILE__, __LINE__, pheight, nc);
	    assert(parent->u.n.n_children<=TREE_FANOUT+1);
	    for (i=0; i<parent->u.n.n_children; i++) {
		//printf(" %lld\n", parent->u.n.children[i]);
		if (parent->u.n.children[i]==nodename) goto ok;
	    }
	    printf("%s:%d Whoops, the parent of %p (%p) isn't right\n", __FILE__, __LINE__, brtnode, parent);
	    assert(0);
	ok: ;
	    //printf("\n");
	}
    }
    //printf("%s:%d %p->mdict[0]=%p\n", __FILE__, __LINE__, brtnode, brtnode->mdicts[0]);
    if (write_me) {
	serialize_brtnode_to(cachefile_fd(cachefile), brtnode->thisnodename, brtnode->nodesize, brtnode);
    }
    //printf("%s:%d %p->mdict[0]=%p\n", __FILE__, __LINE__, brtnode, brtnode->mdicts[0]);
    if (!keep_me) {
	brtnode_free(&brtnode);
    }
    //printf("%s:%d n_items_malloced=%lld\n", __FILE__, __LINE__, n_items_malloced);
}

int brtnode_fetch_callback (CACHEFILE cachefile, diskoff nodename, void **brtnode_pv, long *sizep __attribute__((unused)), void*extraargs) {
    long nodesize=(long)extraargs;
    BRTNODE *result=(BRTNODE*)brtnode_pv;
    int r = deserialize_brtnode_from(cachefile_fd(cachefile), nodename, result, nodesize);
    if (r == 0)
        *sizep = brtnode_size(*result);
    //(*result)->parent_brtnode = 0; /* Don't know it right now. */
    //printf("%s:%d installed %p (offset=%lld)\n", __FILE__, __LINE__, *result, nodename);
    return r;
}
	
void brtheader_flush_callback (CACHEFILE cachefile, diskoff nodename, void *header_v, long size __attribute((unused)), int write_me, int keep_me) {
    struct brt_header *h = header_v;
    assert(nodename==0);
    assert(!h->dirty); // shouldn't be dirty once it is unpinned.
    if (write_me) {
	serialize_brt_header_to(cachefile_fd(cachefile), h);
    }
    if (!keep_me) {
	if (h->n_named_roots>0) {
	    int i;
	    for (i=0; i<h->n_named_roots; i++) {
		toku_free(h->names[i]);
	    }
	    toku_free(h->names);
	    toku_free(h->roots);
	}
	toku_free(h);
    }
}

int brtheader_fetch_callback (CACHEFILE cachefile, diskoff nodename, void **headerp_v, long *sizep __attribute__((unused)), void*extraargs __attribute__((__unused__))) {
    struct brt_header **h = (struct brt_header **)headerp_v;
    assert(nodename==0);
    int r = deserialize_brtheader_from(cachefile_fd(cachefile), nodename, h);
    return r;
}

int read_and_pin_brt_header (CACHEFILE cf, struct brt_header **header) {
    void *header_p;
    //fprintf(stderr, "%s:%d read_and_pin_brt_header(...)\n", __FILE__, __LINE__);
    int r = cachetable_get_and_pin(cf, 0, &header_p, NULL,
				   brtheader_flush_callback, brtheader_fetch_callback, 0);
    if (r!=0) return r;
    *header = header_p;
    return 0;
}

int unpin_brt_header (BRT brt) {
    int r = cachetable_unpin(brt->cf, 0, brt->h->dirty, 0);
    brt->h->dirty=0;
    brt->h=0;
    return r;
}


typedef struct kvpair {
    bytevec key;
    unsigned int keylen;
    bytevec val;
    unsigned int vallen;
} *KVPAIR;

int kvpair_compare (const void *av, const void *bv) {
    const KVPAIR a = (const KVPAIR)av;
    const KVPAIR b = (const KVPAIR)bv;
    int r = keycompare(a->key, a->keylen, b->key, b->keylen);
    //printf("keycompare(%s,\n           %s)-->%d\n", a->key, b->key, r);
    return r;
}

/* Forgot to handle the case where there is something in the freelist. */
diskoff malloc_diskblock_header_is_in_memory (BRT brt, int size) {
    diskoff result = brt->h->unused_memory;
    brt->h->unused_memory+=size;
    return result;
}

diskoff malloc_diskblock (BRT brt, int size) {
#if 0
    int r = read_and_pin_brt_header(brt->fd, &brt->h);
    assert(r==0);
    {
	diskoff result = malloc_diskblock_header_is_in_memory(brt, size);
	r = write_brt_header(brt->fd, &brt->h);
	assert(r==0);
	return result;
    }
#else
    return malloc_diskblock_header_is_in_memory(brt,size);
#endif
}

static void initialize_brtnode (BRT t, BRTNODE n, diskoff nodename, int height) {
    int i;
    n->tag = TYP_BRTNODE;
    n->nodesize = t->h->nodesize;
    n->thisnodename = nodename;
    n->height       = height;
    brtnode_set_dirty(n);
    assert(height>=0);
    if (height>0) {
	n->u.n.n_children   = 0;
	for (i=0; i<TREE_FANOUT; i++) {
	    n->u.n.childkeys[i] = 0;
	    n->u.n.childkeylens[i] = 0;
	}
	n->u.n.totalchildkeylens = 0;
	for (i=0; i<TREE_FANOUT+1; i++) {
	    n->u.n.children[i] = 0;
	    n->u.n.htables[i] = 0;
	    n->u.n.n_bytes_in_hashtable[i] = 0;
            n->u.n.n_cursors[i] = 0;
	}
	n->u.n.n_bytes_in_hashtables = 0;
    } else {
	int r = pma_create(&n->u.l.buffer, t->compare_fun, n->nodesize);
	static int rcount=0;
	assert(r==0);
	//printf("%s:%d n PMA= %p (rcount=%d)\n", __FILE__, __LINE__, n->u.l.buffer, rcount); 
	rcount++;
	n->u.l.n_bytes_in_buffer = 0;
    }
}

static void create_new_brtnode (BRT t, BRTNODE *result, int height, BRTNODE parent_brtnode) {
    TAGMALLOC(BRTNODE, n);
    int r;
    diskoff name = malloc_diskblock(t, t->h->nodesize);
    assert(n);
    assert(t->h->nodesize>0);
    //printf("%s:%d malloced %lld (and malloc again=%lld)\n", __FILE__, __LINE__, name, malloc_diskblock(t, t->nodesize));
    initialize_brtnode(t, n, name, height);
    *result = n;
    assert(n->nodesize>0);
    n->parent_brtnode = parent_brtnode;
    //printf("%s:%d putting %p (%lld) parent=%p\n", __FILE__, __LINE__, n, n->thisnodename, parent_brtnode);
    r=cachetable_put(t->cf, n->thisnodename, n, brtnode_size(n),
			  brtnode_flush_callback, brtnode_fetch_callback, (void*)(long)t->h->nodesize);
    assert(r==0);
}

void delete_node (BRT t, BRTNODE node) {
    int i;
    assert(node->height>=0);
    if (node->height==0) {
	if (node->u.l.buffer) {
	    pma_free(&node->u.l.buffer);
	}
	node->u.l.n_bytes_in_buffer=0;
    } else {
	for (i=0; i<node->u.n.n_children; i++) {
	    if (node->u.n.htables[i]) {
		toku_hashtable_free(&node->u.n.htables[i]);
	    }
	    node->u.n.n_bytes_in_hashtable[0]=0;
            assert(node->u.n.n_cursors[i] == 0);
	}
	node->u.n.n_bytes_in_hashtables = 0;
	node->u.n.totalchildkeylens=0;
	node->u.n.n_children=0;
	node->height=0;
	node->u.l.buffer=0; /* It's a leaf now (height==0) so set the buffer to NULL. */
    }
    cachetable_remove(t->cf, node->thisnodename, 0); /* Don't write it back to disk. */
}

#define USE_PMA_SPLIT 1
#if ! USE_PMA_SPLIT
static void insert_to_buffer_in_leaf (BRTNODE node, DBT *k, DBT *v, DB *db) {
    unsigned int n_bytes_added = KEY_VALUE_OVERHEAD + k->size + v->size;
    int r = pma_insert(node->u.l.buffer, k, v, db);
    assert(r==0);
    node->u.l.n_bytes_in_buffer += n_bytes_added;
}
#endif

static int insert_to_hash_in_nonleaf (BRTNODE node, int childnum, DBT *k, DBT *v, int type) {
    unsigned int n_bytes_added = BRT_CMD_OVERHEAD + KEY_VALUE_OVERHEAD + k->size + v->size;
    int r = toku_hash_insert(node->u.n.htables[childnum], k->data, k->size, v->data, v->size, type);
    if (r!=0) return r;
    node->u.n.n_bytes_in_hashtable[childnum] += n_bytes_added;
    node->u.n.n_bytes_in_hashtables += n_bytes_added;
    brtnode_set_dirty(node);
    return 0;
}


int brtleaf_split (BRT t, BRTNODE node, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk, void *app_private, DB *db) {
    BRTNODE A,B;
    assert(node->height==0);
    assert(t->h->nodesize>=node->nodesize); /* otherwise we might be in trouble because the nodesize shrank. */
    create_new_brtnode(t, &A, 0, node->parent_brtnode);
    create_new_brtnode(t, &B, 0, node->parent_brtnode);
    //printf("%s:%d A PMA= %p\n", __FILE__, __LINE__, A->u.l.buffer); 
    //printf("%s:%d B PMA= %p\n", __FILE__, __LINE__, A->u.l.buffer); 
    assert(A->nodesize>0);
    assert(B->nodesize>0);
    assert(node->nodesize>0);
    //printf("%s:%d A is at %lld\n", __FILE__, __LINE__, A->thisnodename);
    //printf("%s:%d B is at %lld nodesize=%d\n", __FILE__, __LINE__, B->thisnodename, B->nodesize);
    assert(node->height>0 || node->u.l.buffer!=0);
#if USE_PMA_SPLIT
{
    int r;

    r = pma_split(node->u.l.buffer, &node->u.l.n_bytes_in_buffer,
        A->u.l.buffer, &A->u.l.n_bytes_in_buffer, 
        B->u.l.buffer, &B->u.l.n_bytes_in_buffer);
    assert(r == 0);

    r = pma_get_last(A->u.l.buffer, splitk, 0); 
    assert(r == 0);

    /* unused */
    app_private = app_private;
    db = db;
}
#else
{
    int did_split = 0;
    PMA_ITERATE(node->u.l.buffer, key, keylen, val, vallen,
		({
		    DBT k,v;
		    if (!did_split) {
			insert_to_buffer_in_leaf(A, fill_dbt_ap(&k, key, keylen, app_private), fill_dbt(&v, val, vallen), db);
			if (A->u.l.n_bytes_in_buffer *2 >= node->u.l.n_bytes_in_buffer) {
			    fill_dbt(splitk, memdup(key, keylen), keylen);
			    did_split=1;
			}
		    } else {
			insert_to_buffer_in_leaf(B, fill_dbt_ap(&k, key, keylen, app_private), fill_dbt(&v, val, vallen), db);
		    }
		}));
    assert(did_split==1);
}
#endif
    assert(node->height>0 || node->u.l.buffer!=0);
    /* Remove it from the cache table, and free its storage. */
    //printf("%s:%d old pma = %p\n", __FILE__, __LINE__, node->u.l.buffer);
    brt_update_cursors_leaf_split(t, node, A, B);
    delete_node(t, node);

    *nodea = A;
    *nodeb = B;
    assert(serialize_brtnode_size(A)<A->nodesize);
    assert(serialize_brtnode_size(B)<B->nodesize);
    return 0;
}

/* Side effect: sets splitk->data pointer to a malloc'd value */
void brt_nonleaf_split (BRT t, BRTNODE node, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk) {
    int n_children_in_a = node->u.n.n_children/2;
    BRTNODE A,B;
    assert(node->height>0);
    assert(node->u.n.n_children>=2); // Otherwise, how do we split?  We need at least two children to split. */
    assert(t->h->nodesize>=node->nodesize); /* otherwise we might be in trouble because the nodesize shrank. */
    create_new_brtnode(t, &A, node->height, node->parent_brtnode);
    create_new_brtnode(t, &B, node->height, node->parent_brtnode);
    A->u.n.n_children=n_children_in_a;
    B->u.n.n_children=node->u.n.n_children-n_children_in_a;
    //printf("%s:%d %p (%lld) becomes %p and %p\n", __FILE__, __LINE__, node, node->thisnodename, A, B);
    //printf("%s:%d A is at %lld\n", __FILE__, __LINE__, A->thisnodename);
    {
	/* The first n_children_in_a go into node a.
	 * That means that the first n_children_in_a-1 keys go into node a.
	 * The splitter key is key number n_children_in_a */
	int i;
	for (i=0; i<n_children_in_a; i++) {
	    A->u.n.children[i]   = node->u.n.children[i];
	    A->u.n.htables[i]    = node->u.n.htables[i];
	    A->u.n.n_bytes_in_hashtables += (A->u.n.n_bytes_in_hashtable[i] = node->u.n.n_bytes_in_hashtable[i]);

	    node->u.n.htables[i] = 0;
	    node->u.n.n_bytes_in_hashtables -= node->u.n.n_bytes_in_hashtable[i];
	    node->u.n.n_bytes_in_hashtable[i] = 0;
	}
	for (i=n_children_in_a; i<node->u.n.n_children; i++) {
	    int targchild = i-n_children_in_a;
	    B->u.n.children[targchild]   = node->u.n.children[i];
	    B->u.n.htables[targchild]    = node->u.n.htables[i];
	    B->u.n.n_bytes_in_hashtables += (B->u.n.n_bytes_in_hashtable[targchild] = node->u.n.n_bytes_in_hashtable[i]);

	    node->u.n.htables[i] = 0;
	    node->u.n.n_bytes_in_hashtables -= node->u.n.n_bytes_in_hashtable[i];
	    node->u.n.n_bytes_in_hashtable[i] = 0;
	}
	for (i=0; i<n_children_in_a-1; i++) {
	    A->u.n.childkeys[i] = node->u.n.childkeys[i];
	    A->u.n.childkeylens[i] = node->u.n.childkeylens[i];
	    A->u.n.totalchildkeylens += node->u.n.childkeylens[i];
	    node->u.n.totalchildkeylens -= node->u.n.childkeylens[i];
	    node->u.n.childkeys[i] = 0;
	    node->u.n.childkeylens[i] = 0;
	}
	splitk->data = (void*)(node->u.n.childkeys[n_children_in_a-1]);
	splitk->size = node->u.n.childkeylens[n_children_in_a-1];
	node->u.n.totalchildkeylens -= node->u.n.childkeylens[n_children_in_a-1];
	node->u.n.childkeys[n_children_in_a-1]=0;
	node->u.n.childkeylens[n_children_in_a-1]=0;
	for (i=n_children_in_a; i<node->u.n.n_children-1; i++) {
	    B->u.n.childkeys[i-n_children_in_a] = node->u.n.childkeys[i];
	    B->u.n.childkeylens[i-n_children_in_a] = node->u.n.childkeylens[i];
	    B->u.n.totalchildkeylens += node->u.n.childkeylens[i];
	    node->u.n.totalchildkeylens -= node->u.n.childkeylens[i];
	    node->u.n.childkeys[i] = 0;
	    node->u.n.childkeylens[i] = 0;
	}
	assert(node->u.n.totalchildkeylens==0);

	fix_up_parent_pointers_of_children(t, A);
	fix_up_parent_pointers_of_children(t, B);

    }

    {
	int i;
	for (i=0; i<TREE_FANOUT+1; i++) {
	    assert(node->u.n.htables[i]==0);
	    assert(node->u.n.n_bytes_in_hashtable[i]==0);
	}
	assert(node->u.n.n_bytes_in_hashtables==0);
    }
    /* The buffer is all divied up between them, since just moved the hashtables over. */

    *nodea = A;
    *nodeb = B;

    /* Remove it from the cache table, and free its storage. */
    //printf("%s:%d removing %lld\n", __FILE__, __LINE__, node->thisnodename);
    brt_update_cursors_nonleaf_split(t, node, A, B);
    delete_node(t, node);
    assert(serialize_brtnode_size(A)<A->nodesize);
    assert(serialize_brtnode_size(B)<B->nodesize);
}

void find_heaviest_child (BRTNODE node, int *childnum) {
    int max_child = 0;
    int max_weight = node->u.n.n_bytes_in_hashtable[0];
    int i;

    if (0) printf("%s:%d weights: %d", __FILE__, __LINE__, max_weight);
    assert(node->u.n.n_children>0);
    for (i=1; i<node->u.n.n_children; i++) {
	int this_weight = node->u.n.n_bytes_in_hashtable[i];
	if (0) printf(" %d", this_weight);
	if (max_weight < this_weight) {
	    max_child = i;
	    max_weight = this_weight;
	}
    }
    *childnum = max_child;
    if (0) printf("\n");
}

static int brtnode_put_cmd (BRT t, BRTNODE node, BRT_CMD *cmd,
			    int *did_split, BRTNODE *nodea, BRTNODE *nodeb,
			    DBT *split,
			    int debug,
			    TOKUTXN txn);

/* key is not in the hashtable in node.  Either put the key-value pair in the child, or put it in the node. */
static int push_brt_cmd_down_only_if_it_wont_push_more_else_put_here (BRT t, BRTNODE node, BRTNODE child,
								      BRT_CMD *cmd,
								      int childnum_of_node,
								      TOKUTXN txn) {
    assert(node->height>0); /* Not a leaf. */
    DBT *k = cmd->u.id.key;
    DBT *v = cmd->u.id.val;
    int to_child=serialize_brtnode_size(child)+k->size+v->size+KEY_VALUE_OVERHEAD <= child->nodesize;
    if (brt_debug_mode) {
	printf("%s:%d pushing %s to %s %d", __FILE__, __LINE__, (char*)k->data, to_child? "child" : "hash", childnum_of_node);
	if (childnum_of_node+1<node->u.n.n_children) {
	    DBT k2;
	    printf(" nextsplitkey=%s\n", (char*)node->u.n.childkeys[childnum_of_node]);
	    assert(t->compare_fun(cmd->u.id.db, k, fill_dbt(&k2, node->u.n.childkeys[childnum_of_node], node->u.n.childkeylens[childnum_of_node]))<=0);
	} else {
	    printf("\n");
	}
    }
    if (to_child) {
	int again_split=-1; BRTNODE againa,againb;
	DBT againk;
	init_dbt(&againk);
	//printf("%s:%d hello!\n", __FILE__, __LINE__);
	int r = brtnode_put_cmd(t, child, cmd,
				&again_split, &againa, &againb, &againk,
				0,
				txn);
	if (r!=0) return r;
	assert(again_split==0); /* I only did the insert if I knew it wouldn't push down, and hence wouldn't split. */
	return r;
    } else {
	int r=insert_to_hash_in_nonleaf(node, childnum_of_node, k, v, cmd->type);
	return r;
    }
}

static int push_a_brt_cmd_down (BRT t, BRTNODE node, BRTNODE child, int childnum,
				BRT_CMD *cmd,
				int *child_did_split, BRTNODE *childa, BRTNODE *childb,
				DBT *childsplitk,
				TOKUTXN txn) {
    //if (debug) printf("%s:%d %*sinserting down\n", __FILE__, __LINE__, debug, "");
    //printf("%s:%d hello!\n", __FILE__, __LINE__);
    assert(node->height>0);
    
    {
	int r = brtnode_put_cmd(t, child, cmd,
				child_did_split, childa, childb, childsplitk,
				0,
				txn);
	if (r!=0) return r;
    }

    DBT *k = cmd->u.id.key;
    DBT *v = cmd->u.id.val;
    //if (debug) printf("%s:%d %*sinserted down child_did_split=%d\n", __FILE__, __LINE__, debug, "", child_did_split);
    {
	int r = toku_hash_delete(node->u.n.htables[childnum], k->data, k->size); // Must delete after doing the insert, to avoid operating on freed' key
	//printf("%s:%d deleted status=%d\n", __FILE__, __LINE__, r);
	if (r!=0) return r;
    }
    {
	int n_bytes_removed = (k->size + v->size + KEY_VALUE_OVERHEAD + BRT_CMD_OVERHEAD);
	node->u.n.n_bytes_in_hashtables -= n_bytes_removed;
	node->u.n.n_bytes_in_hashtable[childnum] -= n_bytes_removed;
        brtnode_set_dirty(node);
    }

    return 0;
}

int split_count=0;

/* NODE is a node with a child.
 * childnum was split into two nodes childa, and childb.
 * We must slide things around, & move things from the old table to the new tables.
 * We also move things to the new children as much as we an without doing any pushdowns or splitting of the child.
 * We must delete the old hashtable (but the old child is already deleted.)
 * We also unpin the new children.
 */
static int handle_split_of_child (BRT t, BRTNODE node, int childnum,
				  BRTNODE childa, BRTNODE childb,
				  DBT *childsplitk, /* the data in the childsplitk is alloc'd and is consumed by this call. */
				  int *did_split, BRTNODE *nodea, BRTNODE *nodeb,
				  DBT *splitk,
				  void *app_private,
				  DB *db,
				  TOKUTXN txn) {
    assert(node->height>0);
    assert(0 <= childnum && childnum < node->u.n.n_children);
    HASHTABLE old_h = node->u.n.htables[childnum];
    int       old_count = node->u.n.n_bytes_in_hashtable[childnum];
    int cnum;
    int r;
    assert(node->u.n.n_children<=TREE_FANOUT);

    if (brt_debug_mode) {
	int i;
	printf("%s:%d Child %d did split on %s\n", __FILE__, __LINE__, childnum, (char*)childsplitk->data);
	printf("%s:%d oldsplitkeys:", __FILE__, __LINE__);
	for(i=0; i<node->u.n.n_children-1; i++) printf(" %s", (char*)node->u.n.childkeys[i]);
	printf("\n");
    }

    brtnode_set_dirty(node);

    // Slide the children over.
    for (cnum=node->u.n.n_children; cnum>childnum+1; cnum--) {
	node->u.n.children[cnum] = node->u.n.children[cnum-1];
	node->u.n.htables[cnum] = node->u.n.htables[cnum-1];
	node->u.n.n_bytes_in_hashtable[cnum] = node->u.n.n_bytes_in_hashtable[cnum-1];
        node->u.n.n_cursors[cnum] = node->u.n.n_cursors[cnum-1];
    }
    node->u.n.children[childnum]   = childa->thisnodename;
    node->u.n.children[childnum+1] = childb->thisnodename;
    toku_hashtable_create(&node->u.n.htables[childnum]);
    toku_hashtable_create(&node->u.n.htables[childnum+1]);
    node->u.n.n_bytes_in_hashtable[childnum] = 0;
    node->u.n.n_bytes_in_hashtable[childnum+1] = 0;
    // Slide the keys over
    for (cnum=node->u.n.n_children-1; cnum>childnum; cnum--) {
	node->u.n.childkeys[cnum] = node->u.n.childkeys[cnum-1];
	node->u.n.childkeylens[cnum] = node->u.n.childkeylens[cnum-1];
    }
    node->u.n.childkeys[childnum]= (char*)childsplitk->data;
    node->u.n.childkeylens[childnum]= childsplitk->size;
    node->u.n.totalchildkeylens += childsplitk->size;
    node->u.n.n_children++;

    brt_update_cursors_nonleaf_expand(t, node, childnum, childa, childb);

    if (brt_debug_mode) {
	int i;
	printf("%s:%d splitkeys:", __FILE__, __LINE__);
	for(i=0; i<node->u.n.n_children-1; i++) printf(" %s", (char*)node->u.n.childkeys[i]);
	printf("\n");
    }

    node->u.n.n_bytes_in_hashtables -= old_count; /* By default, they are all removed.  We might add them back in. */
    /* Keep pushing to the children, but not if the children would require a pushdown */
    HASHTABLE_ITERATE(old_h, skey, skeylen, sval, svallen, type, ({
        DBT skd, svd;
	fill_dbt_ap(&skd, skey, skeylen, app_private);
	fill_dbt(&svd, sval, svallen);
        BRT_CMD brtcmd;
        brtcmd.type = type; brtcmd.u.id.key = &skd; brtcmd.u.id.val = &svd; brtcmd.u.id.db = db;
	if (t->compare_fun(db, &skd, childsplitk)<=0) {
	    r=push_brt_cmd_down_only_if_it_wont_push_more_else_put_here(t, node, childa, &brtcmd, childnum, txn);
	} else {
	    r=push_brt_cmd_down_only_if_it_wont_push_more_else_put_here(t, node, childb, &brtcmd, childnum+1, txn);
	}
	if (r!=0) return r;
    }));

    toku_hashtable_free(&old_h);

    r=cachetable_unpin(t->cf, childa->thisnodename, childa->dirty, brtnode_size(childa));
    assert(r==0);
    r=cachetable_unpin(t->cf, childb->thisnodename, childb->dirty, brtnode_size(childb));
    assert(r==0);
		

    verify_counts(node);
    verify_counts(childa);
    verify_counts(childb);

    if (node->u.n.n_children>TREE_FANOUT) {
	//printf("%s:%d about to split having pushed %d out of %d keys\n", __FILE__, __LINE__, i, n_pairs);
	brt_nonleaf_split(t, node, nodea, nodeb, splitk);
	//printf("%s:%d did split\n", __FILE__, __LINE__);
	split_count++;
	*did_split=1;
	assert((*nodea)->height>0);
	assert((*nodeb)->height>0);
	assert((*nodea)->u.n.n_children>0);
	assert((*nodeb)->u.n.n_children>0);
	assert((*nodea)->u.n.children[(*nodea)->u.n.n_children-1]!=0);
	assert((*nodeb)->u.n.children[(*nodeb)->u.n.n_children-1]!=0);
	assert(serialize_brtnode_size(*nodea)<=(*nodea)->nodesize);
	assert(serialize_brtnode_size(*nodeb)<=(*nodeb)->nodesize);
    } else {
	*did_split=0;
	assert(serialize_brtnode_size(node)<=node->nodesize);
    }
    return 0;
}

static int push_some_brt_cmds_down (BRT t, BRTNODE node, int childnum,
				    int *did_split, BRTNODE *nodea, BRTNODE *nodeb,
				    DBT *splitk,
				    int debug,
				    void *app_private,
				    DB *db,
				    TOKUTXN txn) {
    void *childnode_v;
    BRTNODE child;
    int r;
    assert(node->height>0);
    diskoff targetchild = node->u.n.children[childnum]; 
    assert(targetchild>=0 && targetchild<t->h->unused_memory); // This assertion could fail in a concurrent setting since another process might have bumped unused memory.
    r = cachetable_get_and_pin(t->cf, targetchild, &childnode_v, NULL, 
			       brtnode_flush_callback, brtnode_fetch_callback, (void*)(long)t->h->nodesize);
    if (r!=0) return r;
    //printf("%s:%d pin %p\n", __FILE__, __LINE__, childnode_v);
    child=childnode_v;
    child->parent_brtnode = node;
    verify_counts(child);
    //printf("%s:%d height=%d n_bytes_in_hashtable = {%d, %d, %d, ...}\n", __FILE__, __LINE__, child->height, child->n_bytes_in_hashtable[0], child->n_bytes_in_hashtable[1], child->n_bytes_in_hashtable[2]);
    if (child->height>0 && child->u.n.n_children>0) assert(child->u.n.children[child->u.n.n_children-1]!=0);
    if (debug) printf("%s:%d %*spush_some_brt_cmds_down to %lld\n", __FILE__, __LINE__, debug, "", child->thisnodename);
    /* I am exposing the internals of the hash table here, mostly because I am not thinking of a really
     * good way to do it otherwise.  I want to loop over the elements of the hash table, deleting some as I
     * go.  The HASHTABLE_ITERATE macro will break if I delete something from the hash table. */
  
    if (0) {
	static int count=0;
	count++;
	printf("%s:%d pushing %d count=%d\n", __FILE__, __LINE__, childnum, count);
    }
    {
	bytevec key,val;
	ITEMLEN keylen, vallen;
	long int randomnumber = random();
	//printf("%s:%d Try random_pick, weight=%d \n", __FILE__, __LINE__, node->u.n.n_bytes_in_hashtable[childnum]);
	assert(toku_hashtable_n_entries(node->u.n.htables[childnum])>0);
	int type;
        while(0==toku_hashtable_random_pick(node->u.n.htables[childnum], &key, &keylen, &val, &vallen, &type, &randomnumber)) {
	    int child_did_split=0; BRTNODE childa, childb;
	    DBT hk,hv;
	    DBT childsplitk;
            BRT_CMD brtcmd;

            fill_dbt_ap(&hk, key, keylen, app_private);
            fill_dbt(&hv, val, vallen);
            brtcmd.type = type;
            brtcmd.u.id.key = &hk;
            brtcmd.u.id.val = &hv;
            brtcmd.u.id.db = db;

	    //printf("%s:%d random_picked\n", __FILE__, __LINE__);
	    init_dbt(&childsplitk);
	    childsplitk.app_private = splitk->app_private;

	    if (debug) printf("%s:%d %*spush down %s\n", __FILE__, __LINE__, debug, "", (char*)key);
	    r = push_a_brt_cmd_down (t, node, child, childnum,
				     &brtcmd,
				     &child_did_split, &childa, &childb,
				     &childsplitk,
				     txn);

	    if (0){
		unsigned int sum=0;
		HASHTABLE_ITERATE(node->u.n.htables[childnum], hk __attribute__((__unused__)), hkl, hd __attribute__((__unused__)), hdl, type __attribute__((__unused__)),
				  sum+=hkl+hdl+KEY_VALUE_OVERHEAD+BRT_CMD_OVERHEAD);
		printf("%s:%d sum=%d\n", __FILE__, __LINE__, sum);
		assert(sum==node->u.n.n_bytes_in_hashtable[childnum]);
	    }
	    if (node->u.n.n_bytes_in_hashtable[childnum]>0) assert(toku_hashtable_n_entries(node->u.n.htables[childnum])>0);
	    //printf("%s:%d %d=push_a_brt_cmd_down=();  child_did_split=%d (weight=%d)\n", __FILE__, __LINE__, r, child_did_split, node->u.n.n_bytes_in_hashtable[childnum]);
	    if (r!=0) return r;
	    if (child_did_split) {
		// If the child splits, we don't push down any further.
		if (debug) printf("%s:%d %*shandle split splitkey=%s\n", __FILE__, __LINE__, debug, "", (char*)childsplitk.data);
		r=handle_split_of_child (t, node, childnum,
					 childa, childb, &childsplitk,
					 did_split, nodea, nodeb, splitk,
					 app_private, db, txn);
		return r; /* Don't do any more pushing if the child splits. */ 
	    }
	}
	if (0) printf("%s:%d done random picking\n", __FILE__, __LINE__);
    }
    if (debug) printf("%s:%d %*sdone push_some_brt_cmds_down, unpinning %lld\n", __FILE__, __LINE__, debug, "", targetchild);
    r=cachetable_unpin(t->cf, targetchild, child->dirty, brtnode_size(child));
    if (r!=0) return r;
    *did_split=0;
    assert(serialize_brtnode_size(node)<=node->nodesize);
    return 0;
}

int debugp1 (int debug) {
    return debug ? debug+1 : 0;
}

static int brtnode_maybe_push_down(BRT t, BRTNODE node, int *did_split, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk, int debug, void *app_private, DB *db, TOKUTXN txn)
/* If the buffer is too full, then push down.  Possibly the child will split.  That may make us split. */
{
    assert(node->height>0);
    if (debug) printf("%s:%d %*sIn maybe_push_down in_buffer=%d childkeylens=%d size=%d\n", __FILE__, __LINE__, debug, "", node->u.n.n_bytes_in_hashtables, node->u.n.totalchildkeylens, serialize_brtnode_size(node));
    if (serialize_brtnode_size(node) > node->nodesize ) {
	if (debug) printf("%s:%d %*stoo full, height=%d\n", __FILE__, __LINE__, debug, "", node->height);	
	{
	    /* Push to a child. */
	    /* Find the heaviest child, and push stuff to it.  Keep pushing to the child until we run out.
	     * But if the child pushes something to its child and our buffer has gotten small enough, then we stop pushing. */
	    int childnum;
	    if (0) printf("%s:%d %*sfind_heaviest_data\n", __FILE__, __LINE__, debug, "");
	    find_heaviest_child(node, &childnum);
	    if (0) printf("%s:%d %*spush some down from %lld into %lld (child %d)\n", __FILE__, __LINE__, debug, "", node->thisnodename, node->u.n.children[childnum], childnum);
	    assert(node->u.n.children[childnum]!=0);
	    int r = push_some_brt_cmds_down(t, node, childnum, did_split, nodea, nodeb, splitk, debugp1(debug), app_private, db, txn);
	    if (r!=0) return r;
	    assert(*did_split==0 || *did_split==1);
	    if (debug) printf("%s:%d %*sdid push_some_brt_cmds_down did_split=%d\n", __FILE__, __LINE__, debug, "", *did_split);
	    if (*did_split) {
		assert(serialize_brtnode_size(*nodea)<=(*nodea)->nodesize);		
		assert(serialize_brtnode_size(*nodeb)<=(*nodeb)->nodesize);		
		assert((*nodea)->u.n.n_children>0);
		assert((*nodeb)->u.n.n_children>0);
		assert((*nodea)->u.n.children[(*nodea)->u.n.n_children-1]!=0);
		assert((*nodeb)->u.n.children[(*nodeb)->u.n.n_children-1]!=0);
	    } else {
		assert(serialize_brtnode_size(node)<=node->nodesize);
	    }
	}
    } else {
	*did_split=0;
	assert(serialize_brtnode_size(node)<=node->nodesize);
    }
    return 0;
}

#define INSERT_ALL_AT_ONCE

static int brt_leaf_put_cmd (BRT t, BRTNODE node, BRT_CMD *cmd,
			     int *did_split, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk,
			     int debug,
			     TOKUTXN txn) {
    if  (cmd->type == BRT_INSERT) {
        DBT *k = cmd->u.id.key;
        DBT *v = cmd->u.id.val;
        DB *db = cmd->u.id.db;
#ifdef INSERT_ALL_AT_ONCE
        int replaced_v_size;
        enum pma_errors pma_status = pma_insert_or_replace(node->u.l.buffer, k, v, &replaced_v_size, db, txn, node->thisnodename);
        assert(pma_status==BRT_OK);
        //printf("replaced_v_size=%d\n", replaced_v_size);
        if (replaced_v_size>=0) {
            node->u.l.n_bytes_in_buffer += v->size - replaced_v_size;
        } else {
            node->u.l.n_bytes_in_buffer += k->size + v->size + KEY_VALUE_OVERHEAD;
        }
#else
        DBT v2;
        enum pma_errors pma_status = pma_lookup(node->u.l.buffer, k, init_dbt(&v2), db);
        if (pma_status==BRT_OK) {
            pma_status = pma_delete(node->u.l.buffer, k, db);
            assert(pma_status==BRT_OK);
            node->u.l.n_bytes_in_buffer -= k->size + v2.size + KEY_VALUE_OVERHEAD;
        }
        pma_status = pma_insert(node->u.l.buffer, k, v, db);
        node->u.l.n_bytes_in_buffer += k->size + v->size + KEY_VALUE_OVERHEAD;
#endif
        brtnode_set_dirty(node);
        // If it doesn't fit, then split the leaf.
        if (serialize_brtnode_size(node) > node->nodesize) {
            int r = brtleaf_split (t, node, nodea, nodeb, splitk, k->app_private, db);
            if (r!=0) return r;
            //printf("%s:%d splitkey=%s\n", __FILE__, __LINE__, (char*)*splitkey);
            split_count++;
            *did_split = 1;
            verify_counts(*nodea); verify_counts(*nodeb);
            if (debug) printf("%s:%d %*snodeb->thisnodename=%lld nodeb->size=%d\n", __FILE__, __LINE__, debug, "", (*nodeb)->thisnodename, (*nodeb)->nodesize);
            assert(serialize_brtnode_size(*nodea)<=(*nodea)->nodesize);
            assert(serialize_brtnode_size(*nodeb)<=(*nodeb)->nodesize);
        } else {
            *did_split = 0;
        }
        return 0;
    }
    
    if (cmd->type == BRT_DELETE) {
        int r;
        DBT val;

        /* TODO combine lookup and delete */
        init_dbt(&val);
        r = pma_lookup(node->u.l.buffer, cmd->u.id.key, &val, cmd->u.id.db);
        if (r == 0) {
            r = pma_delete(node->u.l.buffer, cmd->u.id.key, cmd->u.id.db);
            assert(r == BRT_OK);
            node->u.l.n_bytes_in_buffer -= cmd->u.id.key->size + val.size + KEY_VALUE_OVERHEAD;
            brtnode_set_dirty(node);
        }
        *did_split = 0;
        return r;
    }

    /* unknown message */
    assert(0);
    return 0;
}

static unsigned int brtnode_which_child (BRTNODE node , DBT *k, BRT t, DB *db) {
    int i;
    assert(node->height>0);
    for (i=0; i<node->u.n.n_children-1; i++) {
	DBT k2;
	if (t->compare_fun(db, k, fill_dbt(&k2, node->u.n.childkeys[i], node->u.n.childkeylens[i]))<=0) {
	    return i;
	}
    }
    return node->u.n.n_children-1;
}

static int brt_nonleaf_put_cmd_child (BRT t, BRTNODE node, BRT_CMD *cmd,
			              int *did_split, BRTNODE *nodea, BRTNODE *nodeb,
                                      DBT *splitk, int debug, TOKUTXN txn, int childnum, int maybe) {
    int r;
    void *child_v;
    BRTNODE child;
    int child_did_split;
    BRTNODE childa, childb;
    DBT childsplitk;

    *did_split = 0;

    if (maybe) 
        r = cachetable_maybe_get_and_pin(t->cf, node->u.n.children[childnum], &child_v);
    else 
        r = cachetable_get_and_pin(t->cf, node->u.n.children[childnum], &child_v, NULL, 
                                   brtnode_flush_callback, brtnode_fetch_callback, (void*)(long)t->h->nodesize);
    if (r != 0)
        return r;

    child = child_v;
    child->parent_brtnode = node;

    child_did_split = 0;
    r = brtnode_put_cmd(t, child, cmd,
                        &child_did_split, &childa, &childb, &childsplitk, debug, txn);
    if (r != 0) {
        /* putting to the child failed for some reason, so unpin the child and return the error code */
        int rr = cachetable_unpin(t->cf, child->thisnodename, child->dirty, brtnode_size(child));
        assert(rr == 0);
        return r;
    }
    if (child_did_split) {
        if (0) printf("brt_nonleaf_insert child_split %p\n", child);
        assert(cmd->type == BRT_INSERT || cmd->type == BRT_DELETE);
        DBT *k = cmd->u.id.key;
        DB *db = cmd->u.id.db;
        r = handle_split_of_child(t, node, childnum,
                                  childa, childb, &childsplitk,
                                  did_split, nodea, nodeb, splitk,
                                  k->app_private, db, txn);
        assert(r == 0);
    } else {
        int rr = cachetable_unpin(t->cf, child->thisnodename, child->dirty, brtnode_size(child));
        assert(rr == 0);
    }
    return r;
}

int brt_do_push_cmd = 1;

static int brt_nonleaf_put_cmd (BRT t, BRTNODE node, BRT_CMD *cmd,
				int *did_split, BRTNODE *nodea, BRTNODE *nodeb,
				DBT *splitk,
				int debug,
				TOKUTXN txn) {
    bytevec olddata;
    ITEMLEN olddatalen;
    unsigned int childnum;
    int found;

    int type = cmd->type;
    DBT *k = cmd->u.id.key;
    DBT *v = cmd->u.id.val;
    DB *db = cmd->u.id.db;

    childnum = brtnode_which_child(node, k, t, db);

    /* non-buffering mode when cursors are open on this child */
    if (node->u.n.n_cursors[childnum] > 0) {
        assert(node->u.n.n_bytes_in_hashtable[childnum] == 0);
        int r = brt_nonleaf_put_cmd_child(t, node, cmd, did_split, nodea, nodeb, splitk, debug, txn, childnum, 0);
        return r;
    }


    found = !toku_hash_find(node->u.n.htables[childnum], k->data, k->size, &olddata, &olddatalen, &type);

    if (debug) printf("%s:%d %*sDoing hash_insert\n", __FILE__, __LINE__, debug, "");
    verify_counts(node);
    if (found) {
	int r = toku_hash_delete(node->u.n.htables[childnum], k->data, k->size);
	int diff = k->size + olddatalen + KEY_VALUE_OVERHEAD + BRT_CMD_OVERHEAD;
	assert(r==0);
	node->u.n.n_bytes_in_hashtables -= diff;
	node->u.n.n_bytes_in_hashtable[childnum] -= diff;
        brtnode_set_dirty(node);
	//printf("%s:%d deleted %d bytes\n", __FILE__, __LINE__, diff);
    }

    /* if the child is in the cache table then push the cmd to it 
       otherwise just put it into this node's buffer */
    if (brt_do_push_cmd) {
        int r = brt_nonleaf_put_cmd_child(t, node, cmd, did_split, nodea, nodeb, splitk, debug, txn, childnum, 1);
        if (r == 0)
            return r;
    }
    {
	int diff = k->size + v->size + KEY_VALUE_OVERHEAD + BRT_CMD_OVERHEAD;
        int r=toku_hash_insert(node->u.n.htables[childnum], k->data, k->size, v->data, v->size, type);
	assert(r==0);
	node->u.n.n_bytes_in_hashtables += diff;
	node->u.n.n_bytes_in_hashtable[childnum] += diff;
        brtnode_set_dirty(node);
    }
    if (debug) printf("%s:%d %*sDoing maybe_push_down\n", __FILE__, __LINE__, debug, "");
    int r = brtnode_maybe_push_down(t, node, did_split, nodea, nodeb, splitk, debugp1(debug), k->app_private, db, txn);
    if (r!=0) return r;
    if (debug) printf("%s:%d %*sDid maybe_push_down\n", __FILE__, __LINE__, debug, "");
    if (*did_split) {
	assert(serialize_brtnode_size(*nodea)<=(*nodea)->nodesize);
	assert(serialize_brtnode_size(*nodeb)<=(*nodeb)->nodesize);
	assert((*nodea)->u.n.n_children>0);
	assert((*nodeb)->u.n.n_children>0);
	assert((*nodea)->u.n.children[(*nodea)->u.n.n_children-1]!=0);
	assert((*nodeb)->u.n.children[(*nodeb)->u.n.n_children-1]!=0);
	verify_counts(*nodea);
	verify_counts(*nodeb);
    } else {
	assert(serialize_brtnode_size(node)<=node->nodesize);
	verify_counts(node);
    }
    return 0;
}


static int brtnode_put_cmd (BRT t, BRTNODE node, BRT_CMD *cmd,
			    int *did_split, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk,
			    int debug,
			    TOKUTXN txn) {
    if (node->height==0) {
	return brt_leaf_put_cmd(t, node, cmd,
				did_split, nodea, nodeb, splitk,
				debug, txn);
    } else {
	return brt_nonleaf_put_cmd(t, node, cmd,
				   did_split, nodea, nodeb, splitk,
				   debug, txn);
    }
}

int brt_create_cachetable_size(CACHETABLE *ct, int hashsize, long cachesize) {
    return create_cachetable(ct, hashsize, cachesize);
}

//enum {n_nodes_in_cache =64};
enum {n_nodes_in_cache =127};

int brt_create_cachetable (CACHETABLE *ct, int cachelines) {
    if (cachelines==0) cachelines=n_nodes_in_cache;
    assert(cachelines>0);
    return brt_create_cachetable_size(ct, cachelines, (cachelines+1)*1024*1024);
}

static int setup_brt_root_node (BRT t, diskoff offset) {
    int r;
    TAGMALLOC(BRTNODE, node);
    assert(node);
    //printf("%s:%d\n", __FILE__, __LINE__);
    initialize_brtnode(t, node,
		       offset, /* the location is one nodesize offset from 0. */
		       0);
    node->parent_brtnode=0;
    if (0) {
	printf("%s:%d for tree %p node %p mdict_create--> %p\n", __FILE__, __LINE__, t, node, node->u.l.buffer);
	printf("%s:%d put root at %lld\n", __FILE__, __LINE__, offset);
    }
    //printf("%s:%d putting %p (%lld)\n", __FILE__, __LINE__, node, node->thisnodename);
    r=cachetable_put(t->cf, offset, node, brtnode_size(node),
			  brtnode_flush_callback, brtnode_fetch_callback, (void*)(long)t->h->nodesize);
    if (r!=0) {
	toku_free(node);
	return r;
    }
    //printf("%s:%d created %lld\n", __FILE__, __LINE__, node->thisnodename);
    verify_counts(node);
    r=cachetable_unpin(t->cf, node->thisnodename, node->dirty, brtnode_size(node));
    if (r!=0) {
	toku_free(node);
	return r;
    }
    return 0;
}

//#define BRT_TRACE
#ifdef BRT_TRACE
#define WHEN_BRTTRACE(x) x
#else
#define WHEN_BRTTRACE(x) ((void)0)
#endif

int open_brt (const char *fname, const char *dbname, int is_create, BRT *newbrt, int nodesize, CACHETABLE cachetable,
	      int (*compare_fun)(DB*,const DBT*,const DBT*)) {
    /* If dbname is NULL then we setup to hold a single tree.  Otherwise we setup an array. */
    int r;
    BRT t;
    char *malloced_name=0;
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); print_malloced_items();
    WHEN_BRTTRACE(fprintf(stderr, "BRTTRACE: %s:%d open_brt(%s, \"%s\", %d, %p, %d, %p)\n",
			  __FILE__, __LINE__, fname, dbname, is_create, newbrt, nodesize, cachetable));
    if ((MALLOC(t))==0) {
	assert(errno==ENOMEM);
	r = ENOMEM;
	if (0) { died0: toku_free(t); }
	return r;
    }
    t->compare_fun = compare_fun;
    t->skey = t->sval = 0;
    if (dbname) {
	malloced_name = toku_strdup(dbname);
	if (malloced_name==0) {
	    r = ENOMEM;
	    if (0) { died0a: if(malloced_name) toku_free(malloced_name); }
	    goto died0;
	}
    }
    t->database_name = malloced_name;
    r=cachetable_openf(&t->cf, cachetable, fname, O_RDWR | (is_create ? O_CREAT : 0), 0777);
    if (r!=0) {
	if (0) { died1: cachefile_close(&t->cf); }
	goto died0a;
    }
    assert(nodesize>0);
    //printf("%s:%d %d alloced\n", __FILE__, __LINE__, get_n_items_malloced()); print_malloced_items();
    if (is_create) {
	r = read_and_pin_brt_header(t->cf, &t->h);
	if (r==-1) {
	    /* construct a new header. */
	    if ((MALLOC(t->h))==0) {
		assert(errno==ENOMEM);
		r = ENOMEM;
		if (0) { died2: toku_free(t->h); }
		goto died1;
	    }
	    t->h->dirty=1;
	    t->h->nodesize=nodesize;
	    t->h->freelist=-1;
	    t->h->unused_memory=2*nodesize;
	    if (dbname) {
		t->h->unnamed_root = -1;
		t->h->n_named_roots = 1;
		if ((MALLOC_N(1, t->h->names))==0)             { assert(errno==ENOMEM); r=ENOMEM; if (0) { died3: toku_free(t->h->names); } goto died2; }
		if ((MALLOC_N(1, t->h->roots))==0)             { assert(errno==ENOMEM); r=ENOMEM; if (0) { died4: toku_free(t->h->roots); } goto died3; }
		if ((t->h->names[0] = toku_strdup(dbname))==0) { assert(errno==ENOMEM); r=ENOMEM; if (0) { died5: toku_free(t->h->names[0]); } goto died4; }
		t->h->roots[0] = nodesize;
	    } else {
		t->h->unnamed_root = nodesize;
		t->h->n_named_roots = -1;
		t->h->names=0;
		t->h->roots=0;
	    }
	    if ((r=setup_brt_root_node(t, nodesize))!=0) { if (dbname) goto died5; else goto died2;	}
	    if ((r=cachetable_put(t->cf, 0, t->h, 0, brtheader_flush_callback, brtheader_fetch_callback, 0))) {  if (dbname) goto died5; else goto died2; }
	} else {
	    int i;
	    assert(r==0);
	    assert(t->h->unnamed_root==-1);
	    assert(t->h->n_named_roots>=0);
	    for (i=0; i<t->h->n_named_roots; i++) {
		if (strcmp(t->h->names[i], dbname)==0) {
		    r = EEXIST;
		    goto died1; /* deallocate everything. */
		}
	    }
	    if ((t->h->names = toku_realloc(t->h->names, (1+t->h->n_named_roots)*sizeof(*t->h->names))) == 0)   { assert(errno==ENOMEM); r=ENOMEM; goto died1; }
	    if ((t->h->roots = toku_realloc(t->h->roots, (1+t->h->n_named_roots)*sizeof(*t->h->roots))) == 0)   { assert(errno==ENOMEM); r=ENOMEM; goto died1; }
	    t->h->n_named_roots++;
	    if ((t->h->names[t->h->n_named_roots-1] = toku_strdup(dbname)) == 0)                                { assert(errno==ENOMEM); r=ENOMEM; goto died1; }
	    printf("%s:%d t=%p\n", __FILE__, __LINE__, t);
	    t->h->roots[t->h->n_named_roots-1] = malloc_diskblock_header_is_in_memory(t, t->h->nodesize);
	    t->h->dirty = 1;
	    if ((r=setup_brt_root_node(t, t->h->roots[t->h->n_named_roots-1]))!=0) goto died1;
	}
    } else {
	if ((r = read_and_pin_brt_header(t->cf, &t->h))!=0) goto died1;
	if (!dbname) {
	    if (t->h->n_named_roots!=-1) { r = -2; /* invalid args??? */; goto died1; }
	} else {
	    int i;
	    printf("%s:%d n_roots=%d\n", __FILE__, __LINE__, t->h->n_named_roots);
	    for (i=0; i<t->h->n_named_roots; i++) {
		if (strcmp(t->h->names[i], dbname)==0) {
		    goto found_it;
		}

	    }
	    r=ENOENT; /* the database doesn't exist */
	    goto died1;
	}
    found_it: ;
    }
    assert(t->h);
    if ((r = unpin_brt_header(t)) !=0) goto died1;
    assert(t->h==0);
    WHEN_BRTTRACE(fprintf(stderr, "BRTTRACE -> %p\n", t));
    t->cursors_head = t->cursors_tail = 0;
    *newbrt = t;
    return 0;
}

int close_brt (BRT brt) {
    int r;
    while (brt->cursors_head) {
	BRT_CURSOR c = brt->cursors_head;
	r=brt_cursor_close(c);
	if (r!=0) return r;
    }
    assert(0==cachefile_count_pinned(brt->cf, 1));
    //printf("%s:%d closing cachetable\n", __FILE__, __LINE__);
    if ((r = cachefile_close(&brt->cf))!=0) return r;
    if (brt->database_name) toku_free(brt->database_name);
    if (brt->skey) { toku_free(brt->skey); }
    if (brt->sval) { toku_free(brt->sval); }
    toku_free(brt);
    return 0;
}

int brt_debug_mode = 0;//strcmp(key,"hello387")==0;

CACHEKEY* calculate_root_offset_pointer (BRT brt) {
    if (brt->database_name==0) {
	return &brt->h->unnamed_root;
    } else {
	int i;
	for (i=0; i<brt->h->n_named_roots; i++) {
	    if (strcmp(brt->database_name, brt->h->names[i])==0) {
		return &brt->h->roots[i];
	    }
	}
    }
    abort();
}

int brt_init_new_root(BRT brt, BRTNODE nodea, BRTNODE nodeb, DBT splitk, CACHEKEY *rootp) {
    TAGMALLOC(BRTNODE, newroot);
    int r;
    diskoff newroot_diskoff=malloc_diskblock(brt, brt->h->nodesize);
    assert(newroot);
    *rootp=newroot_diskoff;
    brt->h->dirty=1;
    // printf("new_root %lld\n", newroot_diskoff);
    initialize_brtnode (brt, newroot, newroot_diskoff, nodea->height+1);
    newroot->parent_brtnode=0;
    newroot->u.n.n_children=2;
    //printf("%s:%d Splitkey=%p %s\n", __FILE__, __LINE__, splitkey, splitkey);
    newroot->u.n.childkeys[0] = splitk.data;
    newroot->u.n.childkeylens[0] = splitk.size;
    newroot->u.n.totalchildkeylens=splitk.size;
    newroot->u.n.children[0]=nodea->thisnodename;
    newroot->u.n.children[1]=nodeb->thisnodename;
    r=toku_hashtable_create(&newroot->u.n.htables[0]); if (r!=0) return r;
    r=toku_hashtable_create(&newroot->u.n.htables[1]); if (r!=0) return r;
    verify_counts(newroot);
    r=cachetable_unpin(brt->cf, nodea->thisnodename, nodea->dirty, brtnode_size(nodea)); 
    if (r!=0) return r;
    r=cachetable_unpin(brt->cf, nodeb->thisnodename, nodeb->dirty, brtnode_size(nodeb)); 
    if (r!=0) return r;
    //printf("%s:%d put %lld\n", __FILE__, __LINE__, brt->root);
    cachetable_put(brt->cf, newroot_diskoff, newroot, brtnode_size(newroot),
                   brtnode_flush_callback, brtnode_fetch_callback, (void*)(long)brt->h->nodesize);
    brt_update_cursors_new_root(brt, newroot, nodea, nodeb);
    return 0;
}

int brt_root_put_cmd(BRT brt, BRT_CMD *cmd, TOKUTXN txn) {
    void *node_v;
    BRTNODE node;
    CACHEKEY *rootp;
    int result;
    int r;
    int did_split; BRTNODE nodea=0, nodeb=0;
    DBT splitk;
    int debug = brt_debug_mode;//strcmp(key,"hello387")==0;
    //assert(0==cachetable_assert_all_unpinned(brt->cachetable));
    if ((r = read_and_pin_brt_header(brt->cf, &brt->h))) {
	if (0) { died0: unpin_brt_header(brt); }
	return r;
    }
    rootp = calculate_root_offset_pointer(brt);
    if (debug) printf("%s:%d Getting %lld\n", __FILE__, __LINE__, *rootp);
    if ((r=cachetable_get_and_pin(brt->cf, *rootp, &node_v, NULL, 
				  brtnode_flush_callback, brtnode_fetch_callback, (void*)(long)brt->h->nodesize))) {
	goto died0;
    }
    //printf("%s:%d pin %p\n", __FILE__, __LINE__, node_v);
    node=node_v;
    node->parent_brtnode = 0;
    if (debug) printf("%s:%d node inserting\n", __FILE__, __LINE__);
    did_split = 0;
    result = brtnode_put_cmd(brt, node, cmd,
			     &did_split, &nodea, &nodeb, &splitk,
			     debug,
			     txn);
    if (debug) printf("%s:%d did_insert\n", __FILE__, __LINE__);
    if (did_split) {
	//printf("%s:%d did_split=%d nodeb=%p nodeb->thisnodename=%lld nodeb->nodesize=%d\n", __FILE__, __LINE__, did_split, nodeb, nodeb->thisnodename, nodeb->nodesize);
	//printf("Did split, splitkey=%s\n", splitkey);
	if (nodeb->height>0) assert(nodeb->u.n.children[nodeb->u.n.n_children-1]!=0);
	assert(nodeb->nodesize>0);
    }
    int dirty;
    long size;
    if (did_split) {
        r = brt_init_new_root(brt, nodea, nodeb, splitk, rootp);
        assert(r == 0);
        dirty = 1;
        size = 0;
    } else {
	if (node->height>0)
	    assert(node->u.n.n_children<=TREE_FANOUT);
        dirty = node->dirty;
        size = brtnode_size(node);
    }
    cachetable_unpin(brt->cf, *rootp, dirty, size);
    r = unpin_brt_header(brt);
    assert(r == 0);
    //assert(0==cachetable_assert_all_unpinned(brt->cachetable));
    return result;
}

int brt_insert (BRT brt, DBT *key, DBT *val, DB* db, TOKUTXN txn) {
    int r;
    BRT_CMD brtcmd;

    brtcmd.type = BRT_INSERT;
    brtcmd.u.id.key = key;
    brtcmd.u.id.val = val;
    brtcmd.u.id.db = db;
    r = brt_root_put_cmd(brt, &brtcmd, txn);
    return r;
}

int brt_lookup_node (BRT brt, diskoff off, DBT *k, DBT *v, DB *db, BRTNODE parent_brtnode) {
    int result;
    void *node_v;
    int r = cachetable_get_and_pin(brt->cf, off, &node_v, NULL,
				   brtnode_flush_callback, brtnode_fetch_callback, (void*)(long)brt->h->nodesize);
    if (r!=0) 
        return r;

    BRTNODE node = node_v;
    assert(node->tag == TYP_BRTNODE);
    int childnum;

    //printf("%s:%d pin %p height=%d children=%d\n", __FILE__, __LINE__, node_v, node->height, node->u.n.n_children);

    node->parent_brtnode = parent_brtnode;

    if (node->height==0) {
	result = pma_lookup(node->u.l.buffer, k, v, db);
	//printf("%s:%d looked up something, got answerlen=%d\n", __FILE__, __LINE__, answerlen);
	r = cachetable_unpin(brt->cf, off, 0, 0);
	assert(r == 0);
        return result;
    }

    childnum = brtnode_which_child(node, k, brt, db);
    {
	bytevec hanswer;
	ITEMLEN hanswerlen;
        int type;
	if (toku_hash_find (node->u.n.htables[childnum], k->data, k->size, &hanswer, &hanswerlen, &type)==0) {
	    if (type == BRT_INSERT) {
                //printf("Found %d bytes\n", *vallen);
                ybt_set_value(v, hanswer, hanswerlen, &brt->sval);
                //printf("%s:%d Returning %p\n", __FILE__, __LINE__, v->data);
                 result = 0;
            } else if (type == BRT_DELETE) {
                result = DB_NOTFOUND;
            } else {
                assert(0);
                result = -1; // some versions of gcc complain
            }
            r = cachetable_unpin(brt->cf, off, 0, 0);
            assert(r == 0);
            return result;
	}
    }
    
    result = brt_lookup_node(brt, node->u.n.children[childnum], k, v, db, node);
    r = cachetable_unpin(brt->cf, off, 0, 0);
    assert(r == 0);
    return result;
}


int brt_lookup (BRT brt, DBT *k, DBT *v, DB *db) {
    int r;
    CACHEKEY *rootp;
    assert(0==cachefile_count_pinned(brt->cf, 1));
    if ((r = read_and_pin_brt_header(brt->cf, &brt->h))) {
	printf("%s:%d\n", __FILE__, __LINE__);
	if (0) { died0: unpin_brt_header(brt); }
	// printf("%s:%d returning %d\n", __FILE__, __LINE__, r);
	assert(0==cachefile_count_pinned(brt->cf, 1));
	return r;
    }
    rootp = calculate_root_offset_pointer(brt);
    if ((r = brt_lookup_node(brt, *rootp, k, v, db, 0))) {
	// printf("%s:%d\n", __FILE__, __LINE__);
	goto died0;
    }
    //printf("%s:%d r=%d", __FILE__, __LINE__, r); if (r==0) printf(" vallen=%d", *vallen); printf("\n");
    if ((r = unpin_brt_header(brt))!=0) return r;
    assert(0==cachefile_count_pinned(brt->cf, 1));
    return 0;
}

int brt_delete(BRT brt, DBT *key, DB *db) {
    int r;
    BRT_CMD brtcmd;
    DBT val;

    init_dbt(&val);
    val.size = 0;
    brtcmd.type = BRT_DELETE;
    brtcmd.u.id.key = key;
    brtcmd.u.id.val = &val;
    brtcmd.u.id.db = db;
    r = brt_root_put_cmd(brt, &brtcmd, 0);
    return r;
}

int verify_brtnode (BRT brt, diskoff off, bytevec lorange, ITEMLEN lolen, bytevec hirange, ITEMLEN hilen, int recurse, BRTNODE parent_brtnode);

int dump_brtnode (BRT brt, diskoff off, int depth, bytevec lorange, ITEMLEN lolen, bytevec hirange, ITEMLEN hilen, BRTNODE parent_brtnode) {
    int result=0;
    BRTNODE node;
    void *node_v;
    int r = cachetable_get_and_pin(brt->cf, off, &node_v, NULL,
				   brtnode_flush_callback, brtnode_fetch_callback, (void*)(long)brt->h->nodesize);
    assert(r==0);
    printf("%s:%d pin %p\n", __FILE__, __LINE__, node_v);
    node=node_v;
    node->parent_brtnode = parent_brtnode;
    result=verify_brtnode(brt, off, lorange, lolen, hirange, hilen, 0, parent_brtnode);
    printf("%*sNode=%p\n", depth, "", node);
    if (node->height>0) {
	printf("%*sNode %lld nodesize=%d height=%d n_children=%d  n_bytes_in_hashtables=%d keyrange=%s %s\n",
	       depth, "", off, node->nodesize, node->height, node->u.n.n_children, node->u.n.n_bytes_in_hashtables, (char*)lorange, (char*)hirange);
	//printf("%s %s\n", lorange ? lorange : "NULL", hirange ? hirange : "NULL");
	{
	    int i;
	    for (i=0; i< node->u.n.n_children-1; i++) {
		printf("%*schild %d buffered (%d entries):\n", depth+1, "", i, toku_hashtable_n_entries(node->u.n.htables[i]));
		HASHTABLE_ITERATE(node->u.n.htables[i], key, keylen, data, datalen, type,
				  ({
				      printf("%*s %s %s %d\n", depth+2, "", (char*)key, (char*)data, type);
				      assert(strlen((char*)key)+1==keylen);
				      assert(strlen((char*)data)+1==datalen);
				  }));
	    }
	    for (i=0; i<node->u.n.n_children; i++) {
		printf("%*schild %d\n", depth, "", i);
		if (i>0) {
		    printf("%*spivot %d=%s\n", depth+1, "", i-1, (char*)node->u.n.childkeys[i-1]);
		}
		dump_brtnode(brt, node->u.n.children[i], depth+4,
			     (i==0) ? lorange : node->u.n.childkeys[i-1],
			     (i==0) ? lolen   : node->u.n.childkeylens[i-1],
			     (i==node->u.n.n_children-1) ? hirange : node->u.n.childkeys[i],
			     (i==node->u.n.n_children-1) ? hilen   : node->u.n.childkeylens[i],
			     node
			     );
	    }
	}
    } else {
	printf("%*sNode %lld nodesize=%d height=%d n_bytes_in_buffer=%d keyrange=%s %s\n",
	       depth, "", off, node->nodesize, node->height, node->u.l.n_bytes_in_buffer, (char*)lorange, (char*)hirange);
	PMA_ITERATE(node->u.l.buffer, key, keylen, val, vallen,
		    ( keylen=keylen, vallen=vallen, printf(" %s:%s", (char*)key, (char*)val)));
	printf("\n");
    }
    r = cachetable_unpin(brt->cf, off, 0, 0);
    assert(r==0);
    return result;
}

int dump_brt (BRT brt) {
    int r;
    CACHEKEY *rootp;
    if ((r = read_and_pin_brt_header(brt->cf, &brt->h))) {
	if (0) { died0: unpin_brt_header(brt); }
	return r;
    }
    rootp = calculate_root_offset_pointer(brt);
    printf("split_count=%d\n", split_count);
    if ((r = dump_brtnode(brt, *rootp, 0, 0, 0, 0, 0, null_brtnode))) goto died0;
    if ((r = unpin_brt_header(brt))!=0) return r;
    return 0;
}

int show_brtnode_blocknumbers (BRT brt, diskoff off, BRTNODE parent_brtnode) {
    BRTNODE node;
    void *node_v;
    int i,r;
    assert(off%brt->h->nodesize==0);
    if ((r = cachetable_get_and_pin(brt->cf, off, &node_v, NULL,
				    brtnode_flush_callback, brtnode_fetch_callback, (void*)(long)brt->h->nodesize))) {
	if (0) { died0: cachetable_unpin(brt->cf, off, 0, 0); }
	return r;
    }
    printf("%s:%d pin %p\n", __FILE__, __LINE__, node_v);
    node=node_v;
    node->parent_brtnode = parent_brtnode;
    printf(" %lld", off/brt->h->nodesize);
    if (node->height>0) {
	for (i=0; i<node->u.n.n_children; i++) {
	    if ((r=show_brtnode_blocknumbers(brt, node->u.n.children[i], node))) goto died0;
	}
    }
    r = cachetable_unpin(brt->cf, off, 0, 0);
    return r;
}

int show_brt_blocknumbers (BRT brt) {
    int r;
    CACHEKEY *rootp;
    if ((r = read_and_pin_brt_header(brt->cf, &brt->h))) {
	if (0) { died0: unpin_brt_header(brt); }
	return r;
    }
    rootp = calculate_root_offset_pointer(brt);
    printf("BRT %p has blocks:", brt);
    if ((r=show_brtnode_blocknumbers (brt, *rootp, 0))) goto died0;
    printf("\n");
    if ((r = unpin_brt_header(brt))!=0) return r;
    return 0;
}

int verify_brtnode (BRT brt, diskoff off, bytevec lorange, ITEMLEN lolen, bytevec hirange, ITEMLEN hilen, int recurse, BRTNODE parent_brtnode) {
    int result=0;
    BRTNODE node;
    void *node_v;
    int r;
    if ((r = cachetable_get_and_pin(brt->cf, off, &node_v, NULL,
				    brtnode_flush_callback, brtnode_fetch_callback, (void*)(long)brt->h->nodesize)))
	return r;
    //printf("%s:%d pin %p\n", __FILE__, __LINE__, node_v);
    node=node_v;
    node->parent_brtnode = parent_brtnode;
    if (node->height>0) {
	int i;
	for (i=0; i< node->u.n.n_children-1; i++) {
	    bytevec thislorange,thishirange;
	    ITEMLEN thislolen,  thishilen;
	    if (node->u.n.n_children==0 || i==0) {
		thislorange=lorange;
		thislolen  =lolen;
	    } else {
		thislorange=node->u.n.childkeys[i-1];
		thislolen  =node->u.n.childkeylens[i-1];
	    }
	    if (node->u.n.n_children==0 || i+1>=node->u.n.n_children) {
		thishirange=hirange;
		thishilen  =hilen;
	    } else {
		thishirange=node->u.n.childkeys[i];
		thishilen  =node->u.n.childkeylens[i];
	    }
	    {
		void verify_pair (bytevec key, unsigned int keylen,
				  bytevec data __attribute__((__unused__)), 
                                  unsigned int datalen __attribute__((__unused__)),
                                  int type __attribute__((__unused__)),
				  void *ignore __attribute__((__unused__))) {
		    if (thislorange) assert(keycompare(thislorange,thislolen,key,keylen)<0);
		    if (thishirange && keycompare(key,keylen,thishirange,thishilen)>0) {
			printf("%s:%d in buffer %d key %s is bigger than %s\n", __FILE__, __LINE__, i, (char*)key, (char*)thishirange);
			result=1;
		    }
		}
		toku_hashtable_iterate(node->u.n.htables[i], verify_pair, 0);
	    }
	}
	for (i=0; i<node->u.n.n_children; i++) {
	    if (i>0) {
		if (lorange) assert(keycompare(lorange,lolen, node->u.n.childkeys[i-1], node->u.n.childkeylens[i-1])<0);
		if (hirange) assert(keycompare(node->u.n.childkeys[i-1], node->u.n.childkeylens[i-1], hirange, hilen)<=0);
	    }
	    if (recurse) {
		result|=verify_brtnode(brt, node->u.n.children[i],
				       (i==0) ? lorange : node->u.n.childkeys[i-1],
				       (i==0) ? lolen   : node->u.n.childkeylens[i-1],
				       (i==node->u.n.n_children-1) ? hirange : node->u.n.childkeys[i],
				       (i==node->u.n.n_children-1) ? hilen   : node->u.n.childkeylens[i],
				       recurse,
				       node);
	    }
	}
    }
    if ((r = cachetable_unpin(brt->cf, off, 0, 0))) return r;
    return result;
}

int verify_brt (BRT brt) {
    int r;
    CACHEKEY *rootp;
    if ((r = read_and_pin_brt_header(brt->cf, &brt->h))) {
	if (0) { died0: unpin_brt_header(brt); }
	return r;
    }
    rootp = calculate_root_offset_pointer(brt);
    if ((r=verify_brtnode(brt, *rootp, 0, 0, 0, 0, 1, null_brtnode))) goto died0;
    if ((r = unpin_brt_header(brt))!=0) return r;
    return 0;
}

int brt_flush_debug = 0;

/*
 * Flush the buffer for a child of a node.  
 * If the node split when pushing kvpairs to a child of the node
 * then reflect the node split up the cursor path towards the tree root.
 * If the root is reached then create a new root
 */  
void brt_flush_child(BRT t, BRTNODE node, int childnum, BRT_CURSOR cursor, void *app_private, DB *db, TOKUTXN txn) {
    int r;
    int child_did_split;
    BRTNODE childa, childb;
    DBT child_splitk;

    if (brt_flush_debug) {
        printf("brt_flush_child %lld %d\n", node->thisnodename, childnum);
        brt_cursor_print(cursor);
    }

    init_dbt(&child_splitk);
    r = push_some_brt_cmds_down(t, node, childnum, 
				&child_did_split, &childa, &childb, &child_splitk, brt_flush_debug, app_private, db, txn);
    assert(r == 0);
    if (brt_flush_debug) {
        printf("brt_flush_child done %lld %d\n", node->thisnodename, childnum);
        brt_cursor_print(cursor);
    }
    if (child_did_split) {
        int i;

        for (i=cursor->path_len-1; i >= 0; i--) {
            if (cursor->path[i] == childa || cursor->path[i] == childb)
                break;
        }
        assert(i == cursor->path_len-1);
        while (child_did_split) {
            child_did_split = 0;

            if (0) printf("child_did_split %lld %lld\n", childa->thisnodename, childb->thisnodename);
            if (i == 0) {
                CACHEKEY *rootp = calculate_root_offset_pointer(t);
                r = brt_init_new_root(t, childa, childb, child_splitk, rootp);
                assert(r == 0);
                r = cachetable_unpin(t->cf, *rootp, CACHETABLE_DIRTY, 0);
                assert(r == 0);
            } else {
                BRTNODE upnode;

                assert(i > 0);
                i = i-1;
                upnode = cursor->path[i];
                childnum = cursor->pathcnum[i];
                r = handle_split_of_child(t, upnode, childnum,
					  childa, childb, &child_splitk,
					  &child_did_split, &childa, &childb, &child_splitk,
					  app_private, db, txn);
                assert(r == 0);
            }
        }
    }
} 

/*
 * Add a cursor to child of a node.  Increment the cursor count on the child.  Flush the buffer associated with the child.
 */
void brt_node_add_cursor(BRTNODE node, int childnum, BRT_CURSOR cursor) {
    if (node->height > 0) {
        if (0) printf("brt_node_add_cursor %lld %d %p\n", node->thisnodename, childnum, cursor);
        node->u.n.n_cursors[childnum] += 1;
    }
}

/*
 * Remove a cursor from the child of a node.  Decrement the cursor count on the child.
 */
void brt_node_remove_cursor(BRTNODE node, int childnum, BRT_CURSOR cursor __attribute__((unused))) {
    if (node->height > 0) {
        if (0) printf("brt_node_remove_cursor %lld %d %p\n", node->thisnodename, childnum, cursor);
        assert(node->u.n.n_cursors[childnum] > 0);
        node->u.n.n_cursors[childnum] -= 1;
    }
}

int brt_update_debug = 0;

void brt_update_cursors_new_root(BRT t, BRTNODE newroot, BRTNODE left, BRTNODE right) {
    BRT_CURSOR cursor;

    if (brt_update_debug) printf("brt_update_cursors_new_root %lld %lld %lld\n", newroot->thisnodename,
        left->thisnodename, right->thisnodename);
    for (cursor = t->cursors_head; cursor; cursor = cursor->next) {
        if (brt_cursor_active(cursor)) {
            brt_cursor_new_root(cursor, t, newroot, left, right);
        }
    }
}

void brt_update_cursors_leaf_split(BRT t, BRTNODE oldnode, BRTNODE left, BRTNODE right) {
    BRT_CURSOR cursor;

    if (brt_update_debug) printf("brt_update_cursors_leaf_split %lld %lld %lld\n", oldnode->thisnodename,
        left->thisnodename, right->thisnodename);
    for (cursor = t->cursors_head; cursor; cursor = cursor->next) {
        if (brt_cursor_active(cursor)) {
            brt_cursor_leaf_split(cursor, t, oldnode, left, right);
        }
    }
}

void brt_update_cursors_nonleaf_expand(BRT t, BRTNODE node, int childnum, BRTNODE left, BRTNODE right) {
    BRT_CURSOR cursor;

    if (brt_update_debug) printf("brt_update_cursors_nonleaf_expand %lld h=%d c=%d nc=%d %lld %lld\n", node->thisnodename, node->height, childnum,
                  node->u.n.n_children, left->thisnodename, right->thisnodename);
    for (cursor = t->cursors_head; cursor; cursor = cursor->next) {
        if (brt_cursor_active(cursor)) {
            brt_cursor_nonleaf_expand(cursor, t, node, childnum, left, right);
        }
    }
}

void brt_update_cursors_nonleaf_split(BRT t, BRTNODE oldnode, BRTNODE left, BRTNODE right) {
    BRT_CURSOR cursor;

    if (brt_update_debug) printf("brt_update_cursors_nonleaf_split %lld %lld %lld\n", oldnode->thisnodename,
        left->thisnodename, right->thisnodename);
    for (cursor = t->cursors_head; cursor; cursor = cursor->next) {
        if (brt_cursor_active(cursor)) {
            brt_cursor_nonleaf_split(cursor, t, oldnode, left, right);
        }
    }
}

void brt_cursor_new_root(BRT_CURSOR cursor, BRT t, BRTNODE newroot, BRTNODE left, BRTNODE right) {
    int i;
    int childnum;
    int r;
    void *v;

    assert(!brt_cursor_path_full(cursor));

    if (0) printf("brt_cursor_new_root %p %lld newroot %lld\n", cursor, cursor->path[0]->thisnodename, newroot->thisnodename);

    assert(cursor->path[0] == left || cursor->path[0] == right);

    /* make room for the newroot at the path base */
    for (i=cursor->path_len; i>0; i--) {
        cursor->path[i] = cursor->path[i-1];
        cursor->pathcnum[i] = cursor->pathcnum[i-1];
    }
    cursor->path_len++;

    /* shift the newroot */
    cursor->path[0] = newroot;
    childnum = cursor->path[1] == left ? 0 : 1;
    cursor->pathcnum[0] = childnum;
    r = cachetable_maybe_get_and_pin(t->cf, newroot->thisnodename, &v);
    assert(r == 0 && v == newroot);
    brt_node_add_cursor(newroot, childnum, cursor);
}

void brt_cursor_leaf_split(BRT_CURSOR cursor, BRT t, BRTNODE oldnode, BRTNODE left, BRTNODE right) {
    int r;
    BRTNODE newnode;
    PMA pma;
    void *v;

    assert(oldnode->height == 0);
    if (cursor->path[cursor->path_len-1] == oldnode) {
        assert(left->height == 0 && right->height == 0);

        r = pma_cursor_get_pma(cursor->pmacurs, &pma);
        assert(r == 0);
        if (pma == left->u.l.buffer)
            newnode = left;
        else if (pma == right->u.l.buffer)
            newnode = right;
        else
            newnode = 0;
        assert(newnode);

        if (0) printf("brt_cursor_leaf_split %p oldnode %lld newnode %lld\n", cursor, 
                      oldnode->thisnodename, newnode->thisnodename);

        r = cachetable_unpin(t->cf, oldnode->thisnodename, oldnode->dirty, brtnode_size(oldnode));
        assert(r == 0);
        r = cachetable_maybe_get_and_pin(t->cf, newnode->thisnodename, &v);
        assert(r == 0 && v == newnode);
        cursor->path[cursor->path_len-1] = newnode;
    }
}

void brt_cursor_nonleaf_expand(BRT_CURSOR cursor, BRT t __attribute__((unused)), BRTNODE node, int childnum, BRTNODE left, BRTNODE right) {
    int i;
    int oldchildnum, newchildnum;

    assert(node->height > 0);

//    i = cursor->path_len - node->height - 1;
//    if (i < 0)
//        i = cursor->path_len - 1;
//    if (i >= 0 && cursor->path[i] == node) {
//    }
    if (0) brt_cursor_print(cursor);
    for (i = 0; i < cursor->path_len; i++)
        if (cursor->path[i] == node)
            break;
    if (i < cursor->path_len) {
        if (cursor->pathcnum[i] < childnum)
            return;
        if (cursor->pathcnum[i] > childnum) {
        setnewchild:
            oldchildnum = cursor->pathcnum[i];
            newchildnum = oldchildnum + 1;
            brt_node_remove_cursor(node, oldchildnum, cursor);
            brt_node_add_cursor(node, newchildnum, cursor);
            cursor->pathcnum[i] = newchildnum;
            return;
        } 
        if (i == cursor->path_len-1 && (cursor->op == DB_PREV || cursor->op == DB_LAST)) {
            goto setnewchild;
        }
        if (i+1 < cursor->path_len) {
            assert(cursor->path[i+1] == left || cursor->path[i+1] == right);
            if (cursor->path[i+1] == right) {
                goto setnewchild;
            }
        } 
    }
}

void brt_cursor_nonleaf_split(BRT_CURSOR cursor, BRT t, BRTNODE oldnode, BRTNODE left, BRTNODE right) {
    int i;
    BRTNODE newnode;
    int r;
    void *v;
    int childnum;

    assert(oldnode->height > 0 && left->height > 0 && right->height > 0);
//    i = cursor->path_len - oldnode->height - 1;
//    if (i < 0)
//        i = cursor->path_len - 1;
//    if (i >= 0 && cursor->path[i] == oldnode) {
    for (i = 0; i < cursor->path_len; i++)
        if (cursor->path[i] == oldnode)
            break;
    if (i < cursor->path_len) {
        childnum = cursor->pathcnum[i];
        brt_node_remove_cursor(oldnode, childnum, cursor);
        if (childnum < left->u.n.n_children) {
            newnode = left;
        } else {
            newnode = right;
            childnum -= left->u.n.n_children;
        }

        if (0) printf("brt_cursor_nonleaf_split %p oldnode %lld newnode %lld\n",
		      cursor, oldnode->thisnodename, newnode->thisnodename);

        r = cachetable_unpin(t->cf, oldnode->thisnodename, oldnode->dirty, brtnode_size(oldnode));
        assert(r == 0);
        r = cachetable_maybe_get_and_pin(t->cf, newnode->thisnodename, &v);
        assert(r == 0 && v == newnode); 
        brt_node_add_cursor(newnode, childnum, cursor);
        cursor->path[i] = newnode;
        cursor->pathcnum[i] = childnum;
    }
}

int brt_cursor (BRT brt, BRT_CURSOR*cursor) {
    BRT_CURSOR MALLOC(result);
    assert(result);
    result->brt = brt;
    result->path_len = 0;
    result->pmacurs = 0;

    if (brt->cursors_head) {
	brt->cursors_head->prev = result;
    } else {
	brt->cursors_tail = result;
    }
    result->next = brt->cursors_head;
    result->prev = 0;
    brt->cursors_head = result;
    *cursor = result;
    return 0;

}

static int unpin_cursor(BRT_CURSOR);

int brt_cursor_close (BRT_CURSOR curs) {
    BRT brt = curs->brt;
    int r=unpin_cursor(curs);
    if (curs->prev==0) {
	assert(brt->cursors_head==curs);
	brt->cursors_head = curs->next;
    } else {
	curs->prev->next = curs->next;
    }
    if (curs->next==0) {
	assert(brt->cursors_tail==curs);
	brt->cursors_tail = curs->prev;
    } else {
	curs->next->prev = curs->prev;
    }
    if (curs->pmacurs) {
	int r2=pma_cursor_free(&curs->pmacurs);
	if (r==0) r=r2;
    }
    toku_free(curs);
    return r;
}

/*
 * Print the path of a cursor
 */
void brt_cursor_print(BRT_CURSOR cursor) {
    int i;

    printf("cursor %p: ", cursor);
    for (i=0; i<cursor->path_len; i++) {
        printf("%lld", cursor->path[i]->thisnodename);
        if (cursor->path[i]->height > 0)
            printf(",%d:%d ", cursor->pathcnum[i], cursor->path[i]->u.n.n_children);
        else
            printf(" ");
    }
    printf("\n");
}

int brtcurs_set_position_last (BRT_CURSOR cursor, diskoff off, DBT *key, DB *db, TOKUTXN txn, BRTNODE parent_brtnode) {
    BRT brt=cursor->brt;
    void *node_v;
 
    int r = cachetable_get_and_pin(brt->cf, off, &node_v, NULL,
				   brtnode_flush_callback, brtnode_fetch_callback, (void*)(long)brt->h->nodesize);
    if (r!=0) {
	if (0) { died0: cachetable_unpin(brt->cf, off, 1, 0); }
	return r;
    }
    BRTNODE node = node_v;
    node->parent_brtnode = parent_brtnode;
    assert(cursor->path_len<CURSOR_PATHLEN_LIMIT);
    cursor->path[cursor->path_len++] = node;
    if (node->height>0) {
        int childnum;

    try_last_child:
	childnum = node->u.n.n_children-1;

    try_prev_child:
	cursor->pathcnum[cursor->path_len-1] = childnum;
	brt_node_add_cursor(node, childnum, cursor);
        if (node->u.n.n_bytes_in_hashtable[childnum] > 0) {
            brt_flush_child(cursor->brt, node, childnum, cursor, key->app_private, db, txn);
            /*
             * the flush may have been partially successfull.  it may have also
             * changed the tree such that the current node have expanded or been
             * replaced.  lets start over.
             */
            node = cursor->path[cursor->path_len-1];
            childnum = cursor->pathcnum[cursor->path_len-1];
            brt_node_remove_cursor(node, childnum, cursor);
            goto try_last_child;
        }
        r=brtcurs_set_position_last (cursor, node->u.n.children[childnum], key, db, txn, node);
	if (r == 0)
            return 0;
        assert(node == cursor->path[cursor->path_len-1]);
        brt_node_remove_cursor(node, childnum, cursor);
        if (r==DB_NOTFOUND) {
	    if (childnum>0) {
		childnum--;
		goto try_prev_child;
	    }
	}
        /* we ran out of children without finding anything, or had some other trouble. */ 
        cursor->path_len--;
        goto died0;
    } else {
	r=pma_cursor(node->u.l.buffer, &cursor->pmacurs);
	if (r!=0) {
	    if (0) { died10: pma_cursor_free(&cursor->pmacurs); }
	    cursor->path_len--;
	    goto died0;
	}
	r=pma_cursor_set_position_last(cursor->pmacurs);
	if (r!=0) goto died10; /* we'll deallocate this cursor, and unpin this node, and go back up. */
	return 0;
    }
}

int brtcurs_set_position_first (BRT_CURSOR cursor, diskoff off, DBT *key, DB *db, TOKUTXN txn, BRTNODE parent_brtnode) {
    BRT brt=cursor->brt;
    void *node_v;

    int r = cachetable_get_and_pin(brt->cf, off, &node_v, NULL,
				   brtnode_flush_callback, brtnode_fetch_callback, (void*)(long)brt->h->nodesize);
    if (r!=0) {
	if (0) { died0: cachetable_unpin(brt->cf, off, 1, 0); }
	return r;
    }
    BRTNODE node = node_v;
    node->parent_brtnode = parent_brtnode;
    assert(cursor->path_len<CURSOR_PATHLEN_LIMIT);
    cursor->path[cursor->path_len++] = node;
    if (node->height>0) {
	int childnum
;
    try_first_child:
        childnum = 0;

    try_next_child:
	cursor->pathcnum[cursor->path_len-1] = childnum;
        brt_node_add_cursor(node, childnum, cursor);        
        if (node->u.n.n_bytes_in_hashtable[childnum] > 0) {
            brt_flush_child(cursor->brt, node, childnum, cursor, key->app_private, db, txn);
            /*
             * the flush may have been partially successfull.  it may have also
             * changed the tree such that the current node have expanded or been
             * replaced.  lets start over.
             */
            node = cursor->path[cursor->path_len-1];
            childnum = cursor->pathcnum[cursor->path_len-1];
            brt_node_remove_cursor(node, childnum, cursor);
            goto try_first_child;
        }
        r=brtcurs_set_position_first (cursor, node->u.n.children[childnum], key, db, txn, node);
        if (r == 0)
            return r;
        assert(node == cursor->path[cursor->path_len-1]);
	brt_node_remove_cursor(node, childnum, cursor);
        if (r==DB_NOTFOUND) {
	    if (childnum+1<node->u.n.n_children) {
		childnum++;
		goto try_next_child;
	    }
	}

	/* we ran out of children without finding anything, or had some other trouble. */ 
        cursor->path_len--;
        goto died0;
    } else {
	r=pma_cursor(node->u.l.buffer, &cursor->pmacurs);
	if (r!=0) {
	    if (0) { died10: pma_cursor_free(&cursor->pmacurs); }
	    cursor->path_len--;
	    goto died0;
	}
	r=pma_cursor_set_position_first(cursor->pmacurs);
	if (r!=0) goto died10; /* we'll deallocate this cursor, and unpin this node, and go back up. */
	return 0;
    }
}

int brtcurs_set_position_next2(BRT_CURSOR cursor, DBT *key, DB *db, TOKUTXN txn) {
    BRTNODE node;
    int childnum;
    int r;
    int more;

    assert(cursor->path_len > 0);

    /* pop the node and childnum from the cursor path */
    node = cursor->path[cursor->path_len-1];
    childnum = cursor->pathcnum[cursor->path_len-1];
    cursor->path_len -= 1;
    cachetable_unpin(cursor->brt->cf, node->thisnodename, node->dirty, brtnode_size(node));

    if (brt_cursor_path_empty(cursor))
        return DB_NOTFOUND;

    /* set position first in the next right tree */
    node = cursor->path[cursor->path_len-1];
    childnum = cursor->pathcnum[cursor->path_len-1];
    assert(node->height > 0);
    brt_node_remove_cursor(node, childnum, cursor);

    childnum += 1;
    while (childnum < node->u.n.n_children) {
        cursor->pathcnum[cursor->path_len-1] = childnum;
        brt_node_add_cursor(node, childnum, cursor);
        for (;;) {
            more = node->u.n.n_bytes_in_hashtable[childnum];
            if (more == 0)
                break;
            brt_flush_child(cursor->brt, node, childnum, cursor, key->app_private, db, txn);
            node = cursor->path[cursor->path_len-1];
            childnum = cursor->pathcnum[cursor->path_len-1];
	}
        r = brtcurs_set_position_first(cursor, node->u.n.children[childnum], key, db, txn, node);
        if (r == 0)
            return 0;
        assert(node == cursor->path[cursor->path_len-1]);
        brt_node_remove_cursor(node, childnum, cursor);
        childnum += 1;
    }

    return brtcurs_set_position_next2(cursor, key, db, txn);
}

/* requires that the cursor is initialized. */
int brtcurs_set_position_next (BRT_CURSOR cursor, DBT *key, DB *db, TOKUTXN txn) {
    int r = pma_cursor_set_position_next(cursor->pmacurs);
    if (r==DB_NOTFOUND) {
	/* We fell off the end of the pma. */
	if (cursor->path_len==1) return DB_NOTFOUND;
        /* Part of the trickyness is we need to leave the cursor pointing at the current (possibly deleted) value if there is no next value. */
        r = pma_cursor_free(&cursor->pmacurs);
        assert(r == 0);
        return brtcurs_set_position_next2(cursor, key, db, txn);
    }
    return 0;
}

int brtcurs_set_position_prev2(BRT_CURSOR cursor, DBT *key, DB *db, TOKUTXN txn)  {
    BRTNODE node;
    int childnum;
    int r;
    int more;

    assert(cursor->path_len > 0);

    /* pop the node and childnum from the cursor path */
    node = cursor->path[cursor->path_len-1];
    childnum = cursor->pathcnum[cursor->path_len-1];
    cursor->path_len -= 1;
    cachetable_unpin(cursor->brt->cf, node->thisnodename, node->dirty, brtnode_size(node));

    if (brt_cursor_path_empty(cursor))
        return DB_NOTFOUND;

    /* set position last in the next left tree */
    node = cursor->path[cursor->path_len-1];
    childnum = cursor->pathcnum[cursor->path_len-1];
    assert(node->height > 0);
    brt_node_remove_cursor(node, childnum, cursor);

    childnum -= 1;
    while (childnum >= 0) {
        cursor->pathcnum[cursor->path_len-1] = childnum;
        brt_node_add_cursor(node, childnum, cursor);
        for (;;) {
            more = node->u.n.n_bytes_in_hashtable[childnum];
            if (more == 0)
                break;
            brt_flush_child(cursor->brt, node, childnum, cursor, key->app_private, db, txn);
            node = cursor->path[cursor->path_len-1];
            childnum = cursor->pathcnum[cursor->path_len-1];
        }
        r = brtcurs_set_position_last(cursor, node->u.n.children[childnum], key, db, txn, node);
        if (r == 0)
            return 0;
        assert(node == cursor->path[cursor->path_len-1]);
        brt_node_remove_cursor(node, childnum, cursor);
        childnum -= 1;
    }

    return brtcurs_set_position_prev2(cursor, key, db, txn);
}

int brtcurs_set_position_prev (BRT_CURSOR cursor, DBT *key, DB *db, TOKUTXN txn) {
    int r = pma_cursor_set_position_prev(cursor->pmacurs);
    if (r==DB_NOTFOUND) {
	if (cursor->path_len==1) 
            return DB_NOTFOUND;
        r = pma_cursor_free(&cursor->pmacurs);
        assert(r == 0);
        return brtcurs_set_position_prev2(cursor, key, db, txn);
    }
    return 0;
}

int brtcurs_set_key(BRT_CURSOR cursor, diskoff off, DBT *key, DBT *val, int flag, DB *db, TOKUTXN txn, BRTNODE parent_brtnode) {
    BRT brt = cursor->brt;
    void *node_v;
    int r;
    r = cachetable_get_and_pin(brt->cf, off, &node_v, NULL, brtnode_flush_callback,
                               brtnode_fetch_callback, (void*)(long)brt->h->nodesize);
    if (r != 0)
        return r;

    BRTNODE node = node_v;
    int childnum;
    node->parent_brtnode = parent_brtnode;

    if (node->height > 0) {
        cursor->path_len += 1;
        for (;;) {
            childnum = brtnode_which_child(node, key, brt, db);
            cursor->path[cursor->path_len-1] = node;
            cursor->pathcnum[cursor->path_len-1] = childnum;
            brt_node_add_cursor(node, childnum, cursor);
            int more = node->u.n.n_bytes_in_hashtable[childnum];
            if (more > 0) {
                brt_flush_child(cursor->brt, node, childnum, cursor, key->app_private, db, txn);
                node = cursor->path[cursor->path_len-1];
                childnum = cursor->pathcnum[cursor->path_len-1];
                brt_node_remove_cursor(node, childnum, cursor);
                /* the node may have split. search the node keys again */
                continue;
            }
            break;
        }
        r = brtcurs_set_key(cursor, node->u.n.children[childnum], key, val, flag, db, txn, node);
        if (r != 0)
            brt_node_remove_cursor(node, childnum, cursor);
    } else {
        cursor->path_len += 1;
        cursor->path[cursor->path_len-1] = node;
        r = pma_cursor(node->u.l.buffer, &cursor->pmacurs);
        if (r == 0) {
            if (flag == DB_SET)
                r = pma_cursor_set_key(cursor->pmacurs, key, db);
            else if (flag == DB_GET_BOTH)
                r = pma_cursor_set_both(cursor->pmacurs, key, val, db);
            else {
                assert(0);
                r = DB_NOTFOUND;
            }
            if (r != 0) {
                int rr = pma_cursor_free(&cursor->pmacurs);
                assert(rr == 0);
            }
        }
     }

    if (r != 0) {
        cursor->path_len -= 1;
        cachetable_unpin(brt->cf, off, node->dirty, brtnode_size(node));
    }
    return r;
}

int brtcurs_set_range(BRT_CURSOR cursor, diskoff off, DBT *key, DB *db, TOKUTXN txn, BRTNODE parent_brtnode) {
    BRT brt = cursor->brt;
    void *node_v;
    int r;
    r = cachetable_get_and_pin(brt->cf, off, &node_v, NULL, brtnode_flush_callback,
                               brtnode_fetch_callback, (void*)(long)brt->h->nodesize);
    if (r != 0)
        return r;

    BRTNODE node = node_v;
    int childnum;
    node->parent_brtnode = parent_brtnode;

    if (node->height > 0) {
        cursor->path_len += 1;
        /* select a subtree by key */
        childnum = brtnode_which_child(node, key, brt, db);
    next_child:        
        for (;;) {
            cursor->path[cursor->path_len-1] = node;
            cursor->pathcnum[cursor->path_len-1] = childnum;
            brt_node_add_cursor(node, childnum, cursor);
            int more = node->u.n.n_bytes_in_hashtable[childnum];
            if (more > 0) {
                brt_flush_child(cursor->brt, node, childnum, cursor, key->app_private, db, txn);
                node = cursor->path[cursor->path_len-1];
                childnum = cursor->pathcnum[cursor->path_len-1];
                brt_node_remove_cursor(node, childnum, cursor);
                continue;
            }
            break;
        }
        r = brtcurs_set_range(cursor, node->u.n.children[childnum], key, db, txn, node);
        if (r != 0) {
            node = cursor->path[cursor->path_len-1];
            childnum = cursor->pathcnum[cursor->path_len-1];
            brt_node_remove_cursor(node, childnum, cursor);
            /* no key in the child subtree is >= key, need to search the next child */
            childnum += 1;
            if (0) printf("set_range %d %d\n", childnum, node->u.n.n_children);
            if (childnum < node->u.n.n_children)
                goto next_child;
        }
    } else {
        cursor->path_len += 1;
        cursor->path[cursor->path_len-1] = node;
        r = pma_cursor(node->u.l.buffer, &cursor->pmacurs);
        if (r == 0) {
            r = pma_cursor_set_range(cursor->pmacurs, key, db);
            if (r != 0) {
                int rr = pma_cursor_free(&cursor->pmacurs);
                assert(rr == 0);
            }
        }
    }

    if (r != 0) {
        cursor->path_len -= 1;
        cachetable_unpin(brt->cf, off, node->dirty, brtnode_size(node));
    }
    return r;
}

static int unpin_cursor (BRT_CURSOR cursor) {
    BRT brt=cursor->brt;
    int i;
    int r=0;
    for (i=0; i<cursor->path_len; i++) {
        BRTNODE node = cursor->path[i];
        brt_node_remove_cursor(node, cursor->pathcnum[i], cursor);
	int r2 = cachetable_unpin(brt->cf, node->thisnodename, node->dirty, brtnode_size(node));
	if (r==0) r=r2;
    }
    if (cursor->pmacurs) {
        r = pma_cursor_free(&cursor->pmacurs);
        assert(r == 0);
    }
    cursor->path_len=0;
    return r;
}

static void assert_cursor_path(BRT_CURSOR cursor) {
    int i;
    BRTNODE node;
    int child;

    if (cursor->path_len <= 0)
        return;
    for (i=0; i<cursor->path_len-1; i++) {
        node = cursor->path[i];
        child = cursor->pathcnum[i];
        assert(node->height > 0);
        assert(node->u.n.n_bytes_in_hashtable[child] == 0);
        assert(node->u.n.n_cursors[child] > 0);
    }
    node = cursor->path[i];
    assert(node->height == 0);
}

int brt_cursor_get (BRT_CURSOR cursor, DBT *kbt, DBT *vbt, int flags, DB *db, TOKUTXN txn) {
    int do_rmw=0;
    int r;
    CACHEKEY *rootp;
    
    //dump_brt(cursor->brt);
    //fprintf(stderr, "%s:%d in brt_c_get(...)\n", __FILE__, __LINE__);
    if ((r = read_and_pin_brt_header(cursor->brt->cf, &cursor->brt->h))) {
	if (0) { died0: unpin_brt_header(cursor->brt); }
	return r;
    }
    rootp = calculate_root_offset_pointer(cursor->brt);
    if (flags&DB_RMW) {
	do_rmw=1;
	flags &= ~DB_RMW;
    }
    cursor->op = flags;
    switch (flags) {
    case DB_LAST:
    do_db_last:
	r=unpin_cursor(cursor); if (r!=0) goto died0;
	assert(cursor->pmacurs == 0);
        r=brtcurs_set_position_last(cursor, *rootp, kbt, db, txn, null_brtnode); if (r!=0) goto died0;
        r=pma_cursor_get_current(cursor->pmacurs, kbt, vbt);
	if (r == 0) assert_cursor_path(cursor);
        break;
    case DB_FIRST:
    do_db_first:
	r=unpin_cursor(cursor); if (r!=0) goto died0;
	assert(cursor->pmacurs == 0);
        r=brtcurs_set_position_first(cursor, *rootp, kbt, db, txn, null_brtnode); if (r!=0) goto died0;
        r=pma_cursor_get_current(cursor->pmacurs, kbt, vbt);
	if (r == 0) assert_cursor_path(cursor);
        break;
    case DB_NEXT:
	if (cursor->path_len<=0)
	    goto do_db_first;
	r=brtcurs_set_position_next(cursor, kbt, db, txn); if (r!=0) goto died0;
	r=pma_cursor_get_current(cursor->pmacurs, kbt, vbt); if (r!=0) goto died0;
	if (r == 0) assert_cursor_path(cursor);
        break;
    case DB_PREV:
        if (cursor->path_len<= 0)
            goto do_db_last;
        r = brtcurs_set_position_prev(cursor, kbt, db, txn); if (r!=0) goto died0;
        r = pma_cursor_get_current(cursor->pmacurs, kbt, vbt); if (r!=0) goto died0;
        if (r == 0) assert_cursor_path(cursor);
        break;
    case DB_SET:
        r = unpin_cursor(cursor);
        assert(r == 0);
        r = brtcurs_set_key(cursor, *rootp, kbt, vbt, DB_SET, db, txn, null_brtnode);
        if (r != 0) goto died0;
        r = pma_cursor_get_current(cursor->pmacurs, kbt, vbt);
        if (r != 0) goto died0;
        break;
    case DB_GET_BOTH:
        r = unpin_cursor(cursor);
        assert(r == 0);
        r = brtcurs_set_key(cursor, *rootp, kbt, vbt, DB_GET_BOTH, db, txn, null_brtnode);
        if (r != 0) goto died0;
        break;
    case DB_SET_RANGE:
        r = unpin_cursor(cursor);
        assert(r == 0);
        r = brtcurs_set_range(cursor, *rootp, kbt, db, txn, null_brtnode);
        if (r != 0) goto died0;
        r = pma_cursor_get_current(cursor->pmacurs, kbt, vbt);
        if (r != 0) goto died0;
        break;
    default:
	fprintf(stderr, "%s:%d c_get(...,%d) not ready\n", __FILE__, __LINE__, flags);
	abort();
    }
    //printf("%s:%d unpinning header\n", __FILE__, __LINE__);
    if ((r = unpin_brt_header(cursor->brt))!=0) return r;
    return 0;
}

/* delete the key and value under the cursor */
int brt_cursor_delete(BRT_CURSOR cursor, int flags __attribute__((__unused__))) {
    int r;

    if (cursor->path_len > 0) {
        BRTNODE node = cursor->path[cursor->path_len-1];
        assert(node->height == 0);
        int kvsize;
        r = pma_cursor_delete_under(cursor->pmacurs, &kvsize);
        if (r == 0) {
            node->u.l.n_bytes_in_buffer -= KEY_VALUE_OVERHEAD + kvsize;
            brtnode_set_dirty(node);
        }
    } else
        r = DB_NOTFOUND;

    return r;
}
