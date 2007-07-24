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

extern long long n_items_malloced;

/* Frees a node, including all the stuff in the hash table. */
void brtnode_free (BRTNODE node) {
    int i;
    //printf("%s:%d %p->mdict[0]=%p\n", __FILE__, __LINE__, node, node->mdicts[0]);
    if (node->height>0) {
	for (i=0; i<node->u.n.n_children-1; i++) {
	    toku_free((void*)node->u.n.childkeys[i]);
	}
	for (i=0; i<node->u.n.n_children; i++) {
	    if (node->u.n.htables[i]) {
		hashtable_free(&node->u.n.htables[i]);
	    }
	}
    } else {
	if (node->u.l.buffer) // The buffer may have been freed already, in some cases.
	    pma_free(&node->u.l.buffer);
    }
    toku_free(node);
}

void brtnode_flush_callback (CACHEFILE cachefile, diskoff nodename, void *brtnode_v, int write_me, int keep_me) {
    BRTNODE brtnode = brtnode_v;
    if (0) {
	printf("%s:%d brtnode_flush_callback %p keep_me=%d height=%d", __FILE__, __LINE__, brtnode, keep_me, brtnode->height);
	if (brtnode->height==0) printf(" pma=%p", brtnode->u.l.buffer);
	printf("\n");
    }
    assert(brtnode->thisnodename==nodename);
    //printf("%s:%d %p->mdict[0]=%p\n", __FILE__, __LINE__, brtnode, brtnode->mdicts[0]);
    if (write_me) {
	serialize_brtnode_to(cachefile_fd(cachefile), brtnode->thisnodename, brtnode->nodesize, brtnode);
    }
    //printf("%s:%d %p->mdict[0]=%p\n", __FILE__, __LINE__, brtnode, brtnode->mdicts[0]);
    if (!keep_me) {
	brtnode_free(brtnode);
    }
    //printf("%s:%d n_items_malloced=%lld\n", __FILE__, __LINE__, n_items_malloced);
}

int brtnode_fetch_callback (CACHEFILE cachefile, diskoff nodename, void **brtnode_pv,void*extraargs) {
    long nodesize=(long)extraargs;
    BRTNODE *result=(BRTNODE*)brtnode_pv;
    return deserialize_brtnode_from(cachefile_fd(cachefile), nodename, result, nodesize);
}
	
void brtheader_flush_callback (CACHEFILE cachefile, diskoff nodename, void *header_v, int write_me, int keep_me) {
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

int brtheader_fetch_callback (CACHEFILE cachefile, diskoff nodename, void **headerp_v, void*extraargs __attribute__((__unused__))) {
    struct brt_header **h = (struct brt_header **)headerp_v;
    assert(nodename==0);
    return deserialize_brtheader_from(cachefile_fd(cachefile), nodename, h);
}

int read_and_pin_brt_header (CACHEFILE cf, struct brt_header **header) {
    void *header_p;
    //fprintf(stderr, "%s:%d read_and_pin_brt_header(...)\n", __FILE__, __LINE__);
    int r = cachetable_get_and_pin(cf, 0, &header_p,
				   brtheader_flush_callback, brtheader_fetch_callback, 0);
    if (r!=0) return r;
    *header = header_p;
    return 0;
}

int unpin_brt_header (BRT brt) {
    int r = cachetable_unpin(brt->cf, 0, brt->h->dirty);
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

#if 0
/* in a leaf, they are already sorted because they are in a PMA */ 
static void brtleaf_make_sorted_kvpairs (BRTNODE node, KVPAIR *pairs, int *n_pairs) {
    int n_entries = mdict_n_entries(node->mdicts[0]);
    KVPAIR result=my_calloc(n_entries, sizeof(*result));
    int resultcounter=0;
    assert(node->n_children==0 && node->height==0);
    MDICT_ITERATE(node->mdicts[0], key, keylen, data, datalen, ({
	result[resultcounter].key    = key;
	result[resultcounter].keylen = keylen;
	result[resultcounter].val    = data;
	result[resultcounter].vallen = datalen;
	resultcounter++;
    }));
    assert(resultcounter==n_entries);
    qsort(result, resultcounter, sizeof(*result), kvpair_compare);
    *pairs = result;
    *n_pairs = resultcounter;
//    {
//	innt i;
//	printf("Sorted pairs (sizeof *result=%d):\n", sizeof(*result));
//	for (i=0; i<resultcounter; i++) {
//	    printf(" %s\n", result[i].key);
//	}
//	    
//    }
}
#endif

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
	}
	n->u.n.n_bytes_in_hashtables = 0;
    } else {
	int r = pma_create(&n->u.l.buffer, t->compare_fun);
	static int rcount=0;
	assert(r==0);
	//printf("%s:%d n PMA= %p (rcount=%d)\n", __FILE__, __LINE__, n->u.l.buffer, rcount); 
	rcount++;
	n->u.l.n_bytes_in_buffer = 0;
    }
}

static void create_new_brtnode (BRT t, BRTNODE *result, int height) {
    TAGMALLOC(BRTNODE, n);
    int r;
    diskoff name = malloc_diskblock(t, t->h->nodesize);
    assert(n);
    assert(t->h->nodesize>0);
    //printf("%s:%d malloced %lld (and malloc again=%lld)\n", __FILE__, __LINE__, name, malloc_diskblock(t, t->nodesize));
    initialize_brtnode(t, n, name, height);
    *result = n;
    assert(n->nodesize>0);
    r=cachetable_put(t->cf, n->thisnodename, n,
		     brtnode_flush_callback, brtnode_fetch_callback, (void*)t->h->nodesize);
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
		hashtable_free(&node->u.n.htables[i]);
	    }
	    node->u.n.n_bytes_in_hashtable[0]=0;
	}
	node->u.n.n_bytes_in_hashtables = 0;
	node->u.n.totalchildkeylens=0;
	node->u.n.n_children=0;
	node->height=0;
	node->u.l.buffer=0; /* It's a leaf now (height==0) so set the buffer to NULL. */
    }
    cachetable_remove(t->cf, node->thisnodename, 0); /* Don't write it back to disk. */
}


static void insert_to_buffer_in_leaf (BRTNODE node, DBT *k, DBT *v, DB *db) {
    unsigned int n_bytes_added = KEY_VALUE_OVERHEAD + k->size + v->size;
    int r = pma_insert(node->u.l.buffer, k, v, db);
    assert(r==0);
    node->u.l.n_bytes_in_buffer += n_bytes_added;
}

static int insert_to_hash_in_nonleaf (BRTNODE node, int childnum, DBT *k, DBT *v) {
    unsigned int n_bytes_added = KEY_VALUE_OVERHEAD + k->size + v->size;
    int r = hash_insert(node->u.n.htables[childnum], k->data, k->size, v->data, v->size);
    if (r!=0) return r;
    node->u.n.n_bytes_in_hashtable[childnum] += n_bytes_added;
    node->u.n.n_bytes_in_hashtables += n_bytes_added;
    return 0;
}


int brtleaf_split (BRT t, BRTNODE node, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk, void *app_private, DB *db) {
    int did_split=0;
    BRTNODE A,B;
    assert(node->height==0);
    assert(t->h->nodesize>=node->nodesize); /* otherwise we might be in trouble because the nodesize shrank. */
    create_new_brtnode(t, &A, 0);
    create_new_brtnode(t, &B, 0);
    //printf("%s:%d A PMA= %p\n", __FILE__, __LINE__, A->u.l.buffer); 
    //printf("%s:%d B PMA= %p\n", __FILE__, __LINE__, A->u.l.buffer); 
    assert(A->nodesize>0);
    assert(B->nodesize>0);
    assert(node->nodesize>0);
    //printf("%s:%d A is at %lld\n", __FILE__, __LINE__, A->thisnodename);
    //printf("%s:%d B is at %lld nodesize=%d\n", __FILE__, __LINE__, B->thisnodename, B->nodesize);
    assert(node->height>0 || node->u.l.buffer!=0);
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
    assert(node->height>0 || node->u.l.buffer!=0);
    /* Remove it from the cache table, and free its storage. */
    //printf("%s:%d old pma = %p\n", __FILE__, __LINE__, node->u.l.buffer);
    delete_node(t, node);

    assert(did_split==1);
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
    create_new_brtnode(t, &A, node->height);
    create_new_brtnode(t, &B, node->height);
    A->u.n.n_children=n_children_in_a;
    B->u.n.n_children=node->u.n.n_children-n_children_in_a;
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

#if 0
void find_heaviest_data (BRTNODE node, int *childnum_ret, KVPAIR *pairs_ret, int *n_pairs_ret) {
    int child_weights[node->n_children];
    int child_counts[node->n_children];
    int i;
    for (i=0; i<node->n_children; i++) child_weights[i] = child_counts[i] = 0;

    HASHTABLE_ITERATE(node->hashtable, key, keylen, data __attribute__((__unused__)), datalen,
		      ({
			int cnum;
			for (cnum=0; cnum<node->n_children-1; cnum++) {
			  if (keycompare(key, keylen, node->childkeys[cnum], node->childkeylens[cnum])<=0) 
			    break;
			}
			child_weights[cnum] += keylen + datalen + KEY_VALUE_OVERHEAD;
			child_counts[cnum]++;
		      }));
    {
	int maxchild=0, maxchildweight=child_weights[0];
	for (i=1; i<node->n_children; i++) {
	    if (maxchildweight<child_weights[i]) {
		maxchildweight=child_weights[i];
		maxchild = i;
	    }
	}
	/* Now we know the maximum child. */
	{
	    int maxchildcount = child_counts[maxchild];
	    KVPAIR pairs = my_calloc(maxchildcount, sizeof(*pairs));
	    {
		int pairs_count=0;
		HASHTABLE_ITERATE(node->hashtable, key, keylen, data, datalen, ({
		    int cnum;
		    for (cnum=0; cnum<node->n_children-1; cnum++) {
			if (keycompare(key, keylen, node->childkeys[cnum], node->childkeylens[cnum])<=0) 
			    break;
		    }
		    if (cnum==maxchild) {
			pairs[pairs_count].key = key;
			pairs[pairs_count].keylen = keylen;
			pairs[pairs_count].val = data;
			pairs[pairs_count].vallen = datalen;
			pairs_count++;
		    }
		}));
	    }
	    /* Now we have the pairs. */
	    *childnum_ret = maxchild;
	    *pairs_ret = pairs;
	    *n_pairs_ret = maxchildcount;
	}
    }
}
#endif

static int brtnode_insert (BRT t, BRTNODE node, DBT *k, DBT *v,
			   int *did_split, BRTNODE *nodea, BRTNODE *nodeb,
			   DBT *split,
			   int debug,
			   DB *db);

/* key is not in the hashtable in node.  Either put the key-value pair in the child, or put it in the node. */
static int push_kvpair_down_only_if_it_wont_push_more_else_put_here (BRT t, BRTNODE node, BRTNODE child,
								     DBT *k, DBT *v,
								     int childnum_of_node,
								     DB *db) {
    assert(node->height>0); /* Not a leaf. */
    int to_child=serialize_brtnode_size(child)+k->size+v->size+KEY_VALUE_OVERHEAD <= child->nodesize;
    if (brt_debug_mode) {
	printf("%s:%d pushing %s to %s %d", __FILE__, __LINE__, (char*)k->data, to_child? "child" : "hash", childnum_of_node);
	if (childnum_of_node+1<node->u.n.n_children) {
	    DBT k2;
	    printf(" nextsplitkey=%s\n", (char*)node->u.n.childkeys[childnum_of_node]);
	    assert(t->compare_fun(db, k, fill_dbt(&k2, node->u.n.childkeys[childnum_of_node], node->u.n.childkeylens[childnum_of_node]))<=0);
	} else {
	    printf("\n");
	}
    }
    if (to_child) {
	int again_split=-1; BRTNODE againa,againb;
	DBT againk;
	init_dbt(&againk);
	//printf("%s:%d hello!\n", __FILE__, __LINE__);
	int r = brtnode_insert(t, child, k, v,
			       &again_split, &againa, &againb, &againk,
			       0,
			       db);
	if (r!=0) return r;
	assert(again_split==0); /* I only did the insert if I knew it wouldn't push down, and hence wouldn't split. */
	return r;
    } else {
	int r=insert_to_hash_in_nonleaf(node, childnum_of_node, k, v);
	return r;
    }
}

static int push_a_kvpair_down (BRT t, BRTNODE node, BRTNODE child, int childnum,
			       DBT *k, DBT *v,
			       int *child_did_split, BRTNODE *childa, BRTNODE *childb,
			       DBT *childsplitk,
			       DB *db) {
    //if (debug) printf("%s:%d %*sinserting down\n", __FILE__, __LINE__, debug, "");
    //printf("%s:%d hello!\n", __FILE__, __LINE__);
    assert(node->height>0);
    
    {
	int r = brtnode_insert(t, child, k, v,
			       child_did_split, childa, childb, childsplitk,
			       0,
			       db);
	if (r!=0) return r;
    }

    //if (debug) printf("%s:%d %*sinserted down child_did_split=%d\n", __FILE__, __LINE__, debug, "", child_did_split);
    {
	int r = hash_delete(node->u.n.htables[childnum], k->data, k->size); // Must delete after doing the insert, to avoid operating on freed' key
	//printf("%s:%d deleted status=%d\n", __FILE__, __LINE__, r);
	if (r!=0) return r;
    }
    {
	int n_bytes_removed = (k->size + v->size + KEY_VALUE_OVERHEAD);
	node->u.n.n_bytes_in_hashtables -= n_bytes_removed;
	node->u.n.n_bytes_in_hashtable[childnum] -= n_bytes_removed;
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
				  DB *db) {
    assert(node->height>0);
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

    // Slide the children over.
    for (cnum=node->u.n.n_children; cnum>childnum+1; cnum--) {
	node->u.n.children[cnum] = node->u.n.children[cnum-1];
	node->u.n.htables[cnum] = node->u.n.htables[cnum-1];
	node->u.n.n_bytes_in_hashtable[cnum] = node->u.n.n_bytes_in_hashtable[cnum-1];
    }
    node->u.n.children[childnum]   = childa->thisnodename;
    node->u.n.children[childnum+1] = childb->thisnodename;
    hashtable_create(&node->u.n.htables[childnum]);
    hashtable_create(&node->u.n.htables[childnum+1]);
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

    if (brt_debug_mode) {
	int i;
	printf("%s:%d splitkeys:", __FILE__, __LINE__);
	for(i=0; i<node->u.n.n_children-1; i++) printf(" %s", (char*)node->u.n.childkeys[i]);
	printf("\n");
    }

    node->u.n.n_bytes_in_hashtables -= old_count; /* By default, they are all removed.  We might add them back in. */
    /* Keep pushing to the children, but not if the children would require a pushdown */
    HASHTABLE_ITERATE(old_h, skey, skeylen, sval, svallen, ({
        DBT skd, svd;
	fill_dbt_ap(&skd, skey, skeylen, app_private);
	fill_dbt(&svd, sval, svallen);
	if (t->compare_fun(db, &skd, childsplitk)<=0) {
	    r=push_kvpair_down_only_if_it_wont_push_more_else_put_here(t, node, childa, &skd, &svd, childnum, db);
	} else {
	    r=push_kvpair_down_only_if_it_wont_push_more_else_put_here(t, node, childb, &skd, &svd, childnum+1, db);
	}
	if (r!=0) return r;
    }));
    hashtable_free(&old_h);

    r=cachetable_unpin(t->cf, childa->thisnodename, 1);
    assert(r==0);
    r=cachetable_unpin(t->cf, childb->thisnodename, 1);
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

static int push_some_kvpairs_down (BRT t, BRTNODE node, int childnum,
				   int *did_split, BRTNODE *nodea, BRTNODE *nodeb,
				   DBT *splitk,
				   int debug,
				   void *app_private,
				   DB *db) {
    void *childnode_v;
    BRTNODE child;
    int r;
    assert(node->height>0);
    diskoff targetchild = node->u.n.children[childnum]; 
    assert(targetchild>=0 && targetchild<t->h->unused_memory); // This assertion could fail in a concurrent setting since another process might have bumped unused memory.
    r = cachetable_get_and_pin(t->cf, targetchild, &childnode_v,
			       brtnode_flush_callback, brtnode_fetch_callback, (void*)t->h->nodesize);
    if (r!=0) return r;
    child=childnode_v;
    verify_counts(child);
    //printf("%s:%d height=%d n_bytes_in_hashtable = {%d, %d, %d, ...}\n", __FILE__, __LINE__, child->height, child->n_bytes_in_hashtable[0], child->n_bytes_in_hashtable[1], child->n_bytes_in_hashtable[2]);
    if (child->height>0 && child->u.n.n_children>0) assert(child->u.n.children[child->u.n.n_children-1]!=0);
    if (debug) printf("%s:%d %*spush_some_kvpairs_down to %lld\n", __FILE__, __LINE__, debug, "", child->thisnodename);
    /* I am exposing the internals of the hash table here, mostly because I am not thinking of a really
     * good way to do it otherwise.  I want to loop over the elements of the hash table, deleting some as I
     * go.  The HASHTABLE_ITERATE macro will break if I delete something from the hash table. */
  
    {
	bytevec key,val;
	ITEMLEN keylen, vallen;
	//printf("%s:%d Try random_pick, weight=%d \n", __FILE__, __LINE__, node->u.n.n_bytes_in_hashtable[childnum]);
	assert(hashtable_n_entries(node->u.n.htables[childnum])>0);
	while(0==hashtable_random_pick(node->u.n.htables[childnum], &key, &keylen, &val, &vallen)) {
	    int child_did_split=0; BRTNODE childa, childb;
	    DBT hk,hv;
	    DBT childsplitk;
	    //printf("%s:%d random_picked\n", __FILE__, __LINE__);
	    init_dbt(&childsplitk);
	    childsplitk.app_private = splitk->app_private;

	    if (debug) printf("%s:%d %*spush down %s\n", __FILE__, __LINE__, debug, "", (char*)key);
	    r = push_a_kvpair_down (t, node, child, childnum,
				    fill_dbt_ap(&hk, key, keylen, app_private), fill_dbt(&hv, val, vallen),
				    &child_did_split, &childa, &childb,
				    &childsplitk,
				    db);

	    if (0){
		unsigned int sum=0;
		HASHTABLE_ITERATE(node->u.n.htables[childnum], hk __attribute__((__unused__)), hkl, hd __attribute__((__unused__)), hdl,
				  sum+=hkl+hdl+KEY_VALUE_OVERHEAD);
		printf("%s:%d sum=%d\n", __FILE__, __LINE__, sum);
		assert(sum==node->u.n.n_bytes_in_hashtable[childnum]);
	    }
	    if (node->u.n.n_bytes_in_hashtable[childnum]>0) assert(hashtable_n_entries(node->u.n.htables[childnum])>0);
	    //printf("%s:%d %d=push_a_kvpair_down=();  child_did_split=%d (weight=%d)\n", __FILE__, __LINE__, r, child_did_split, node->u.n.n_bytes_in_hashtable[childnum]);
	    if (r!=0) return r;
	    if (child_did_split) {
		// If the child splits, we don't push down any further.
		if (debug) printf("%s:%d %*shandle split splitkey=%s\n", __FILE__, __LINE__, debug, "", (char*)childsplitk.data);
		r=handle_split_of_child (t, node, childnum,
					 childa, childb, &childsplitk,
					 did_split, nodea, nodeb, splitk,
					 app_private, db);
		return r; /* Don't do any more pushing if the child splits. */ 
	    }
	}
	if (0) printf("%s:%d done random picking\n", __FILE__, __LINE__);
    }
    if (debug) printf("%s:%d %*sdone push_some_kvpairs_down, unpinning %lld\n", __FILE__, __LINE__, debug, "", targetchild);
    r=cachetable_unpin(t->cf, targetchild, 1);
    if (r!=0) return r;
    *did_split=0;
    assert(serialize_brtnode_size(node)<=node->nodesize);
    return 0;
}

int debugp1 (int debug) {
    return debug ? debug+1 : 0;
}

static int brtnode_maybe_push_down(BRT t, BRTNODE node, int *did_split, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk, int debug, void *app_private, DB *db)
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
	    int r = push_some_kvpairs_down(t, node, childnum, did_split, nodea, nodeb, splitk, debugp1(debug), app_private, db);
	    if (r!=0) return r;
	    assert(*did_split==0 || *did_split==1);
	    if (debug) printf("%s:%d %*sdid push_some_kvpairs_down did_split=%d\n", __FILE__, __LINE__, debug, "", *did_split);
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

static int brt_leaf_insert (BRT t, BRTNODE node, DBT *k, DBT *v,
			    int *did_split, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk,
			    int debug,
			    DB *db) {
    DBT v2;
    enum pma_errors pma_status = pma_lookup(node->u.l.buffer, k, init_dbt(&v2), db);
    if (pma_status==BRT_OK) {
	pma_status = pma_delete(node->u.l.buffer, k, db);
	assert(pma_status==BRT_OK);
	node->u.l.n_bytes_in_buffer -= k->size + v2.size + KEY_VALUE_OVERHEAD;
    }
    pma_status = pma_insert(node->u.l.buffer, k, v, db);
    node->u.l.n_bytes_in_buffer += k->size + v->size + KEY_VALUE_OVERHEAD;
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


static int brt_nonleaf_insert (BRT t, BRTNODE node, DBT *k, DBT *v,
			       int *did_split, BRTNODE *nodea, BRTNODE *nodeb,
			       DBT *splitk,
			       int debug,
			       DB *db) {
    bytevec olddata;
    ITEMLEN olddatalen;
    unsigned int childnum = brtnode_which_child(node, k, t, db);
    int found = !hash_find(node->u.n.htables[childnum], k->data, k->size, &olddata, &olddatalen);

    if (0) { // It is faster to do this, except on yobiduck where things grind to a halt.
      void *child_v;
      if (node->height>0 &&
	  0 == cachetable_maybe_get_and_pin(t->cf, node->u.n.children[childnum], &child_v)) {
	  /* If the child is in memory, then go ahead and put it in the child. */
	  BRTNODE child = child_v;
	  if (found) {
	      int diff = k->size + olddatalen + KEY_VALUE_OVERHEAD;
	      int r = hash_delete(node->u.n.htables[childnum], k->data, k->size);
	      assert(r==0);
	      node->u.n.n_bytes_in_hashtables -= diff;
	      node->u.n.n_bytes_in_hashtable[childnum] -= diff;
	  }
	  {
	      int child_did_split;
	      BRTNODE childa, childb;
	      DBT childsplitk;
	      int r = brtnode_insert(t, child, k, v,
				     &child_did_split, &childa, &childb, &childsplitk, 0, db);
	      if (r!=0) return r;
	      if (child_did_split) {
		  r=handle_split_of_child(t, node, childnum,
					  childa, childb, &childsplitk,
					  did_split, nodea, nodeb, splitk,
					  k->app_private, db);
		  if (r!=0) return r;
	      } else {
		  cachetable_unpin(t->cf, child->thisnodename, 1);
		  *did_split = 0;
	      }
	  }
	  return 0;
      }
    }

    if (debug) printf("%s:%d %*sDoing hash_insert\n", __FILE__, __LINE__, debug, "");
    verify_counts(node);
    if (found) {
	int r = hash_delete(node->u.n.htables[childnum], k->data, k->size);
	int diff = k->size + olddatalen + KEY_VALUE_OVERHEAD;
	assert(r==0);
	node->u.n.n_bytes_in_hashtables -= diff;
	node->u.n.n_bytes_in_hashtable[childnum] -= diff;	
	//printf("%s:%d deleted %d bytes\n", __FILE__, __LINE__, diff);
    }
    {
	int diff = k->size + v->size + KEY_VALUE_OVERHEAD;
	int r=hash_insert(node->u.n.htables[childnum], k->data, k->size, v->data, v->size);
	assert(r==0);
	node->u.n.n_bytes_in_hashtables += diff;
	node->u.n.n_bytes_in_hashtable[childnum] += diff;

    }
    if (debug) printf("%s:%d %*sDoing maybe_push_down\n", __FILE__, __LINE__, debug, "");
    int r = brtnode_maybe_push_down(t, node, did_split, nodea, nodeb, splitk, debugp1(debug), k->app_private, db);
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


static int brtnode_insert (BRT t, BRTNODE node, DBT *k, DBT *v,
			   int *did_split, BRTNODE *nodea, BRTNODE *nodeb, DBT *splitk,
			   int debug,
			   DB *db) {
    if (node->height==0) {
	return brt_leaf_insert(t, node, k, v,
			       did_split, nodea, nodeb, splitk,
			       debug,
			       db);
    } else {
	return brt_nonleaf_insert(t, node, k, v,
				  did_split, nodea, nodeb, splitk,
				  debug,
				  db);
    }
}

enum {n_nodes_in_cache =64};

int brt_create_cachetable (CACHETABLE *ct, int cachelines) {
    if (cachelines==0) cachelines=n_nodes_in_cache;
    assert(cachelines>0);
    return create_cachetable(ct, cachelines);
}

static int setup_brt_root_node (BRT t, diskoff offset) {
    int r;
    BRTNODE MALLOC(node);
    assert(node);
    //printf("%s:%d\n", __FILE__, __LINE__);
    initialize_brtnode(t, node,
		       offset, /* the location is one nodesize offset from 0. */
		       0);
    if (0) {
	printf("%s:%d for tree %p node %p mdict_create--> %p\n", __FILE__, __LINE__, t, node, node->u.l.buffer);
	printf("%s:%d put root at %lld\n", __FILE__, __LINE__, offset);
    }
    r=cachetable_put(t->cf, offset, node,
		     brtnode_flush_callback, brtnode_fetch_callback, (void*)t->h->nodesize);
    if (r!=0) {
	toku_free(node);
	return r;
    }
    //printf("%s:%d created %lld\n", __FILE__, __LINE__, node->thisnodename);
    verify_counts(node);
    r=cachetable_unpin(t->cf, node->thisnodename, 1);
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
	malloced_name = mystrdup(dbname);
	if (malloced_name==0) {
	    r = ENOMEM;
	    if (0) { died0a: if(malloced_name) toku_free(malloced_name); }
	    goto died0;
	}
    }
    t->database_name = malloced_name;
    r=cachetable_openf(&t->cf, cachetable, fname, O_RDWR | (is_create ? O_CREAT : 0), 0777);
    if (r!=0) {
	if (0) { died1: cachefile_close(t->cf); }
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
	    t->h->nodesize=nodesize;
	    t->h->freelist=-1;
	    t->h->unused_memory=2*nodesize;
	    if (dbname) {
		t->h->unnamed_root = -1;
		t->h->n_named_roots = 1;
		if ((MALLOC_N(1, t->h->names))==0)           { assert(errno==ENOMEM); r=ENOMEM; if (0) { died3: toku_free(t->h->names); } goto died2; }
		if ((MALLOC_N(1, t->h->roots))==0)           { assert(errno==ENOMEM); r=ENOMEM; if (0) { died4: toku_free(t->h->roots); } goto died3; }
		if ((t->h->names[0] = mystrdup(dbname))==0)  { assert(errno==ENOMEM); r=ENOMEM; if (0) { died5: toku_free(t->h->names[0]); } goto died4; }
		t->h->roots[0] = nodesize;
	    } else {
		t->h->unnamed_root = nodesize;
		t->h->n_named_roots = -1;
		t->h->names=0;
		t->h->roots=0;
	    }
	    if ((r=setup_brt_root_node(t, nodesize))!=0) { if (dbname) goto died5; else goto died2;	}
	    if ((r=cachetable_put(t->cf, 0, t->h, brtheader_flush_callback, brtheader_fetch_callback, 0))) {  if (dbname) goto died5; else goto died2; }
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
	    if ((t->h->names[t->h->n_named_roots-1] = mystrdup(dbname)) == 0)                                 { assert(errno==ENOMEM); r=ENOMEM; goto died1; }
	    printf("%s:%d t=%p\n", __FILE__, __LINE__, t);
	    t->h->roots[t->h->n_named_roots-1] = malloc_diskblock_header_is_in_memory(t, t->h->nodesize);
	    if ((r=setup_brt_root_node(t, t->h->roots[t->h->n_named_roots-1]))!=0) goto died1;
	}
    } else {
	if ((r = read_and_pin_brt_header(t->cf, &t->h))!=0) goto died1;
	if (!dbname) {
	    if (t->h->n_named_roots!=-1) { r = -2; /* invalid args??? */; goto died1; }
	} else {
	    int i;
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
    printf("%s:%d closing cachetable\n", __FILE__, __LINE__);
    if ((r = cachefile_close(brt->cf))!=0) return r;
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

int brt_insert (BRT brt, DBT *k, DBT *v, DB* db) {
    void *node_v;
    BRTNODE node;
    CACHEKEY *rootp;
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
    if ((r=cachetable_get_and_pin(brt->cf, *rootp, &node_v,
				  brtnode_flush_callback, brtnode_fetch_callback, (void*)brt->h->nodesize))) {
	goto died0;
    }
    node=node_v;
    if (debug) printf("%s:%d node inserting\n", __FILE__, __LINE__);
    r = brtnode_insert(brt, node, k, v,
		       &did_split, &nodea, &nodeb, &splitk,
		       debug, db);
    if (r!=0) return r;
    if (debug) printf("%s:%d did_insert\n", __FILE__, __LINE__);
    if (did_split) {
	//printf("%s:%d did_split=%d nodeb=%p nodeb->thisnodename=%lld nodeb->nodesize=%d\n", __FILE__, __LINE__, did_split, nodeb, nodeb->thisnodename, nodeb->nodesize);
	//printf("Did split, splitkey=%s\n", splitkey);
	if (nodeb->height>0) assert(nodeb->u.n.children[nodeb->u.n.n_children-1]!=0);
	assert(nodeb->nodesize>0);
    }
    if (did_split) {
	/* We must cope. */
	BRTNODE MALLOC(newroot);
	diskoff newroot_diskoff=malloc_diskblock(brt, brt->h->nodesize);
	assert(newroot);
	*rootp=newroot_diskoff;
	brt->h->dirty=1;
	initialize_brtnode (brt, newroot, newroot_diskoff, nodea->height+1);
	newroot->u.n.n_children=2;
	//printf("%s:%d Splitkey=%p %s\n", __FILE__, __LINE__, splitkey, splitkey);
	newroot->u.n.childkeys[0] = splitk.data;
	newroot->u.n.childkeylens[0] = splitk.size;
	newroot->u.n.totalchildkeylens=splitk.size;
	newroot->u.n.children[0]=nodea->thisnodename;
	newroot->u.n.children[1]=nodeb->thisnodename;
	r=hashtable_create(&newroot->u.n.htables[0]); if (r!=0) return r;
	r=hashtable_create(&newroot->u.n.htables[1]); if (r!=0) return r;
	verify_counts(newroot);
	r=cachetable_unpin(brt->cf, nodea->thisnodename, 1); if (r!=0) return r;
	r=cachetable_unpin(brt->cf, nodeb->thisnodename, 1); if (r!=0) return r;
	//printf("%s:%d put %lld\n", __FILE__, __LINE__, brt->root);
	cachetable_put(brt->cf, newroot_diskoff, newroot,
		       brtnode_flush_callback, brtnode_fetch_callback, (void*)brt->h->nodesize);
    } else {
	if (node->height>0)
	    assert(node->u.n.n_children<=TREE_FANOUT);
    }
    cachetable_unpin(brt->cf, *rootp, 1);
    if ((r = unpin_brt_header(brt))!=0) return r;
    //assert(0==cachetable_assert_all_unpinned(brt->cachetable));
    return 0;
}

int brt_lookup_node (BRT brt, diskoff off, DBT *k, DBT *v, DB *db) {
    void *node_v;
    int r = cachetable_get_and_pin(brt->cf, off, &node_v,
				   brtnode_flush_callback, brtnode_fetch_callback, (void*)brt->h->nodesize);
    DBT answer;
    BRTNODE node;
    int childnum;
    if (r!=0) {
	int r2;
    died0:
	printf("%s:%d r=%d\n", __FILE__, __LINE__, r);
	r2 = cachetable_unpin(brt->cf, off, 0);
	return r;
    }
    node=node_v;
    if (node->height==0) {
	r = pma_lookup(node->u.l.buffer, k, &answer, db);
	//printf("%s:%d looked up something, got answerlen=%d\n", __FILE__, __LINE__, answerlen);
	if (r!=0) goto died0;
	if (r==0) {
	    *v = answer;
	}
	r = cachetable_unpin(brt->cf, off, 0);
	return r;
    }

    childnum = brtnode_which_child(node, k, brt, db);
    // Leaves have a single mdict, where the data is found.
    {
	bytevec hanswer;
	ITEMLEN hanswerlen;
	if (hash_find (node->u.n.htables[childnum], k->data, k->size, &hanswer, &hanswerlen)==0) {
	    //printf("Found %d bytes\n", *vallen);
	    ybt_set_value(v, hanswer, hanswerlen, &brt->sval);
	    //printf("%s:%d Returning %p\n", __FILE__, __LINE__, v->data);
	    r = cachetable_unpin(brt->cf, off, 0);
	    assert(r==0);
	    return 0;
	}
    }
    if (node->height==0) {
	r = cachetable_unpin(brt->cf, off, 0);
	if (r==0) return DB_NOTFOUND;
	else return r;
    }
    {
	int result = brt_lookup_node(brt, node->u.n.children[childnum], k, v, db);
	r = cachetable_unpin(brt->cf, off, 0);
	if (r!=0) return r;
	return result;
    }
}


int brt_lookup (BRT brt, DBT *k, DBT *v, DB *db) {
    int r;
    CACHEKEY *rootp;
    assert(0==cachefile_count_pinned(brt->cf, 1));
    if ((r = read_and_pin_brt_header(brt->cf, &brt->h))) {
	printf("%s:%d\n", __FILE__, __LINE__);
	if (0) { died0: unpin_brt_header(brt); }
	printf("%s:%d returning %d\n", __FILE__, __LINE__, r);
	assert(0==cachefile_count_pinned(brt->cf, 1));
	return r;
    }
    rootp = calculate_root_offset_pointer(brt);
    if ((r = brt_lookup_node(brt, *rootp, k, v, db))) {
	printf("%s:%d\n", __FILE__, __LINE__);
	goto died0;
    }
    //printf("%s:%d r=%d", __FILE__, __LINE__, r); if (r==0) printf(" vallen=%d", *vallen); printf("\n");
    if ((r = unpin_brt_header(brt))!=0) return r;
    assert(0==cachefile_count_pinned(brt->cf, 1));
    return 0;
}

int verify_brtnode (BRT brt, diskoff off, bytevec lorange, ITEMLEN lolen, bytevec hirange, ITEMLEN hilen, int recurse);

int dump_brtnode (BRT brt, diskoff off, int depth, bytevec lorange, ITEMLEN lolen, bytevec hirange, ITEMLEN hilen) {
    int result=0;
    BRTNODE node;
    void *node_v;
    int r = cachetable_get_and_pin(brt->cf, off, &node_v,
				   brtnode_flush_callback, brtnode_fetch_callback, (void*)brt->h->nodesize);
    assert(r==0);
    node=node_v;
    result=verify_brtnode(brt, off, lorange, lolen, hirange, hilen, 0);
    printf("%*sNode=%p\n", depth, "", node);
    if (node->height>0) {
	printf("%*sNode %lld nodesize=%d height=%d n_children=%d  n_bytes_in_hashtables=%d keyrange=%s %s\n",
	       depth, "", off, node->nodesize, node->height, node->u.n.n_children, node->u.n.n_bytes_in_hashtables, (char*)lorange, (char*)hirange);
	//printf("%s %s\n", lorange ? lorange : "NULL", hirange ? hirange : "NULL");
	{
	    int i;
	    for (i=0; i< node->u.n.n_children-1; i++) {
		printf("%*schild %d buffered (%d entries):\n", depth+1, "", i, hashtable_n_entries(node->u.n.htables[i]));
		HASHTABLE_ITERATE(node->u.n.htables[i], key, keylen, data, datalen,
				  ({
				      printf("%*s %s %s\n", depth+2, "", (char*)key, (char*)data);
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
			     (i==node->u.n.n_children-1) ? hilen   : node->u.n.childkeylens[i]
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
    r = cachetable_unpin(brt->cf, off, 0);
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
    if ((r = dump_brtnode(brt, *rootp, 0, 0, 0, 0, 0))) goto died0;
    if ((r = unpin_brt_header(brt))!=0) return r;
    return 0;
}

int show_brtnode_blocknumbers (BRT brt, diskoff off) {
    BRTNODE node;
    void *node_v;
    int i,r;
    assert(off%brt->h->nodesize==0);
    if ((r = cachetable_get_and_pin(brt->cf, off, &node_v,
				    brtnode_flush_callback, brtnode_fetch_callback, (void*)brt->h->nodesize))) {
	if (0) { died0: cachetable_unpin(brt->cf, off, 0); }
	return r;
    }
    node=node_v;
    printf(" %lld", off/brt->h->nodesize);
    if (node->height>0) {
	for (i=0; i<node->u.n.n_children; i++) {
	    if ((r=show_brtnode_blocknumbers(brt, node->u.n.children[i]))) goto died0;
	}
    }
    r = cachetable_unpin(brt->cf, off, 0);
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
    if ((r=show_brtnode_blocknumbers (brt, *rootp))) goto died0;
    printf("\n");
    if ((r = unpin_brt_header(brt))!=0) return r;
    return 0;
}

int verify_brtnode (BRT brt, diskoff off, bytevec lorange, ITEMLEN lolen, bytevec hirange, ITEMLEN hilen, int recurse) {
    int result=0;
    BRTNODE node;
    void *node_v;
    int r;
    if ((r = cachetable_get_and_pin(brt->cf, off, &node_v,
				    brtnode_flush_callback, brtnode_fetch_callback, (void*)brt->h->nodesize)))
	return r;
    node=node_v;
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
				  bytevec data __attribute__((__unused__)), unsigned int datalen __attribute__((__unused__)),
				  void *ignore __attribute__((__unused__))) {
		    if (thislorange) assert(keycompare(thislorange,thislolen,key,keylen)<0);
		    if (thishirange && keycompare(key,keylen,thishirange,thishilen)>0) {
			printf("%s:%d in buffer %d key %s is bigger than %s\n", __FILE__, __LINE__, i, (char*)key, (char*)thishirange);
			result=1;
		    }
		}
		hashtable_iterate(node->u.n.htables[i], verify_pair, 0);
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
				       recurse);
	    }
	}
    }
    if ((r = cachetable_unpin(brt->cf, off, 0))) return r;
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
    if ((r=verify_brtnode(brt, *rootp, 0, 0, 0, 0, 1))) goto died0;
    if ((r = unpin_brt_header(brt))!=0) return r;
    return 0;
}

#if 0
void brt_fsync (BRT brt) {
    int r = cachetable_fsync(brt->cachetable);
    assert(r==0);
    r = fsync(brt->fd);
    assert(r==0);
}

void brt_flush (BRT brt) {
    int r = cachetable_flush(brt->cachetable, brt);
    assert(r==0);
}
#endif

int brtnode_flush_child (BRT brt, BRTNODE node, int cnum) {
    brt=brt; node=node; cnum=cnum;
    abort(); /* Algorithm: For each key in the cnum'th mdict, insert it to the childnode.  It may cause a split. */
}

#define CURSOR_PATHLEN_LIMIT 256
struct brt_cursor {
    BRT brt;
    int path_len;  /* -1 if the cursor points nowhere. */
    BRTNODE path[CURSOR_PATHLEN_LIMIT]; /* Include the leaf (last).    These are all pinned. */
    int pathcnum[CURSOR_PATHLEN_LIMIT]; /* which child did we descend to from here? */
    PMA_CURSOR pmacurs; /* The cursor into the leaf.  NULL if the cursor doesn't exist. */
    BRT_CURSOR prev,next;
};
static int unpin_cursor (BRT_CURSOR cursor);

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

int brtcurs_set_position_last (BRT_CURSOR cursor, diskoff off) {
    BRT brt=cursor->brt;
    void *node_v;
    int r = cachetable_get_and_pin(brt->cf, off, &node_v,
				   brtnode_flush_callback, brtnode_fetch_callback, (void*)brt->h->nodesize);
    if (r!=0) {
	if (0) { died0: cachetable_unpin(brt->cf, off, 0); }
	return r;
    }
    BRTNODE node = node_v;
    assert(cursor->path_len<CURSOR_PATHLEN_LIMIT);
    cursor->path[cursor->path_len++] = node;
    if (node->height>0) {
	int childnum = node->u.n.n_children-1;
    try_prev_child:
	cursor->pathcnum[cursor->path_len-1] = childnum;
	r=brtcurs_set_position_last (cursor, node->u.n.children[childnum]);
	if (r==DB_NOTFOUND) {
	    if (childnum>0) {
		childnum--;
		goto try_prev_child;
	    }
	}
	if (r!=0) {
	    /* we ran out of children without finding anything, or had some other trouble. */ 
	    cursor->path_len--;
	    goto died0;
	}
	return 0;
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

int brtcurs_set_position_first (BRT_CURSOR cursor, diskoff off) {
    BRT brt=cursor->brt;
    void *node_v;
    int r = cachetable_get_and_pin(brt->cf, off, &node_v,
				   brtnode_flush_callback, brtnode_fetch_callback, (void*)brt->h->nodesize);
    if (r!=0) {
	if (0) { died0: cachetable_unpin(brt->cf, off, 0); }
	return r;
    }
    BRTNODE node = node_v;
    assert(cursor->path_len<CURSOR_PATHLEN_LIMIT);
    cursor->path[cursor->path_len++] = node;
    if (node->height>0) {
	int childnum = 0;
    try_next_child:
	cursor->pathcnum[cursor->path_len-1] = childnum;
	r=brtcurs_set_position_first (cursor, node->u.n.children[childnum]);
	if (r==DB_NOTFOUND) {
	    if (childnum+1<node->u.n.n_children) {
		childnum++;
		goto try_next_child;
	    }
	}
	if (r!=0) {
	    /* we ran out of children without finding anything, or had some other trouble. */ 
	    cursor->path_len--;
	    goto died0;
	}
	return 0;
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

/* requires that the cursor is initialized. */
int brtcurs_set_position_next (BRT_CURSOR cursor) {
    int r = pma_cursor_set_position_next(cursor->pmacurs);
    if (r==DB_NOTFOUND) {
	/* We fell off the end of the pma. */
	if (cursor->path_len==1) return DB_NOTFOUND;
	fprintf(stderr, "Need to deal with falling off the end of the pma in a cursor\n");
	/* Part of the trickyness is we need to leave the cursor pointing at the current (possibly deleted) value if there is no next value. */
	abort();
    }
    return 0;
}

static int unpin_cursor (BRT_CURSOR cursor) {
    BRT brt=cursor->brt;
    int i;
    int r=0;
    for (i=0; i<cursor->path_len; i++) {
	int r2 = cachetable_unpin(brt->cf, cursor->path[i]->thisnodename, 0);
	if (r==0) r=r2;
    }
    cursor->path_len=0;
    return r;
}

int brt_c_get (BRT_CURSOR cursor, DBT *kbt, DBT *vbt, int flags) {
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
    switch (flags) {
    case DB_LAST:
	r=unpin_cursor(cursor); if (r!=0) goto died0;
	r=brtcurs_set_position_last(cursor, *rootp); if (r!=0) goto died0;
	r=pma_cget_current(cursor->pmacurs, kbt, vbt);
	break;
    case DB_FIRST:
    do_db_first:
	r=unpin_cursor(cursor); if (r!=0) goto died0;
	r=brtcurs_set_position_first(cursor, *rootp); if (r!=0) goto died0;
	r=pma_cget_current(cursor->pmacurs, kbt, vbt);
	break;
    case DB_NEXT:
	if (cursor->path_len<=0) {
	    goto do_db_first;
	}
	assert(cursor->path_len>0);
	r=brtcurs_set_position_next(cursor); if (r!=0) goto died0;
	r=pma_cget_current(cursor->pmacurs, kbt, vbt); if (r!=0) goto died0;
	break;
    default:
	fprintf(stderr, "%s:%d c_get(...,%d) not ready\n", __FILE__, __LINE__, flags);
	abort();
    }
    //printf("%s:%d unpinning header\n", __FILE__, __LINE__);
    if ((r = unpin_brt_header(cursor->brt))!=0) return r;
    return 0;
}
