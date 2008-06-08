#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <errno.h>
#include <sys/types.h>
#include <stdint.h>

typedef void *OMTVALUE;
#include "omt.h"
#include "../newbrt/memory.h"
#include "../newbrt/toku_assert.h"
#include "../include/db.h"
#include "../newbrt/brttypes.h"

typedef u_int32_t node_idx;
static const node_idx NODE_NULL = UINT32_MAX;

typedef struct omt_node *OMT_NODE;
struct omt_node {
    u_int32_t weight; /* Size of subtree rooted at this node (including this one). */
    node_idx  left;   /* Index of left  subtree. */
    node_idx  right;  /* Index of right subtree. */
    OMTVALUE  value;  /* The value stored in the node. */
} __attribute__((__packed__));

struct omt {
    node_idx   root;

    u_int32_t  node_capacity;
    OMT_NODE   nodes;
    node_idx   free_idx;

    u_int32_t  tmparray_size;
    node_idx*  tmparray;

    OMTCURSOR  associated; // the OMTs associated with this.
};

struct omt_cursor {
    OMT omt;   // The omt this cursor is associated with.  NULL if not present.
    u_int32_t index; // This is the state for the initial implementation
    OMTCURSOR next,prev; // circular linked list of all OMTCURSORs associated with omt.
};

static int omt_create_internal(OMT *omtp, u_int32_t num_starting_nodes) {
    if (num_starting_nodes < 2) num_starting_nodes = 2;
    OMT MALLOC(result);
    if (result==NULL) return errno;
    result->root=NODE_NULL;
    result->node_capacity = num_starting_nodes*2;
    MALLOC_N(result->node_capacity, result->nodes);
    if (result->nodes==NULL) {
        toku_free(result);
        return errno;
    }
    result->tmparray_size = num_starting_nodes*2;
    MALLOC_N(result->tmparray_size, result->tmparray);
    if (result->tmparray==NULL) {
        toku_free(result->nodes);
        toku_free(result);
        return errno;
    }
    result->free_idx = 0;
    result->associated = NULL;
    *omtp = result;
    return 0;
}

int toku_omt_create (OMT *omtp) {
    return omt_create_internal(omtp, 2);
}

int toku_omt_cursor_create (OMTCURSOR *omtcp) {
    OMTCURSOR MALLOC(c);
    if (c==NULL) return errno;
    c->omt = NULL;
    c->next = c->prev = NULL;
    *omtcp = c;
    return 0;
}

void toku_omt_cursor_invalidate (OMTCURSOR c) {
    if (c==NULL || c->omt==NULL) return;
    if (c->next == c) {
	// It's the last one.
	c->omt->associated = NULL;
    } else {
	OMTCURSOR next = c->next;
	OMTCURSOR prev = c->prev;
	if (c->omt->associated == c) {
	    c->omt->associated = next;
	}
	next->prev = prev;
	prev->next = next;
    }
    c->next = c->prev = NULL;
    c->omt = NULL;
}

void toku_omt_cursor_destroy (OMTCURSOR *p) {
    toku_omt_cursor_invalidate(*p);
    toku_free(*p);
    *p = NULL;
}

static void invalidate_cursors (OMT omt) {
    OMTCURSOR assoced;
    while ((assoced = omt->associated)) {
	toku_omt_cursor_invalidate(assoced);
    }
}

static void associate (OMT omt, OMTCURSOR c)
{
    if (c->omt==omt) return;
    toku_omt_cursor_invalidate(c);
    if (omt->associated==NULL) {
	c->prev = c;
	c->next = c;
	omt->associated = c;
    } else {
	c->prev = omt->associated->prev;
	c->next = omt->associated;
	omt->associated->prev->next = c;
	omt->associated->prev = c;
    }
    c->omt = omt;
}

void toku_omt_destroy(OMT *omtp) {
    OMT omt=*omtp;
    invalidate_cursors(omt);
    toku_free(omt->nodes);
    toku_free(omt->tmparray);
    toku_free(omt);
    *omtp=NULL;
}

static inline u_int32_t nweight(OMT omt, node_idx idx) {
    if (idx==NODE_NULL) return 0;
    else return (omt->nodes+idx)->weight;
}

u_int32_t toku_omt_size(OMT V) {
    return nweight(V, V->root);
}

static inline node_idx omt_node_malloc(OMT omt) {
    assert(omt->free_idx < omt->node_capacity);
    return omt->free_idx++;
}

static inline void omt_node_free(OMT omt, node_idx idx) {
    assert(idx < omt->node_capacity);
}

static inline void fill_array_with_subtree_values(OMT omt, OMTVALUE *array, node_idx tree_idx) {
    if (tree_idx==NODE_NULL) return;
    OMT_NODE tree = omt->nodes+tree_idx;
    fill_array_with_subtree_values(omt, array, tree->left);
    array[nweight(omt, tree->left)] = tree->value;
    fill_array_with_subtree_values(omt, array+nweight(omt, tree->left)+1, tree->right); 
}

// Example:  numvalues=4,  halfway=2,  left side is values of size 2
//                                     right side is values+3 of size 1
//           numvalues=3,  halfway=1,  left side is values of size 1
//                                     right side is values+2 of size 1
//           numvalues=2,  halfway=1,  left side is values of size 1
//                                     right side is values+2 of size 0
//           numvalues=1,  halfway=0,  left side is values of size 0
//                                     right side is values of size 0.
static inline void create_from_sorted_array_internal(OMT omt, node_idx *n_idxp,
                                                     OMTVALUE *values, u_int32_t numvalues) {
    if (numvalues==0) {
        *n_idxp = NODE_NULL;
    } else {
        u_int32_t halfway = numvalues/2;
        node_idx newidx   = omt_node_malloc(omt);
        OMT_NODE newnode  = omt->nodes+newidx;
        newnode->weight   = numvalues;
        newnode->value    = values[halfway]; 
        create_from_sorted_array_internal(omt, &newnode->left,  values,           halfway);
        create_from_sorted_array_internal(omt, &newnode->right, values+halfway+1, numvalues-(halfway+1));
        *n_idxp = newidx;
    }
}

int toku_omt_create_from_sorted_array(OMT *omtp, OMTVALUE *values, u_int32_t numvalues) {
    OMT omt = NULL;
    int r;
    if ((r = omt_create_internal(&omt, numvalues))) return r;
    create_from_sorted_array_internal(omt, &omt->root, values, numvalues);
    *omtp=omt;
    return 0;
}

enum build_choice { MAYBE_REBUILD, JUST_RESIZE };

static inline int maybe_resize_and_rebuild(OMT omt, u_int32_t n, enum build_choice choice) {
    node_idx *new_tmparray = NULL;
    OMT_NODE  new_nodes    = NULL;
    OMTVALUE *tmp_values   = NULL;
    int r = ENOSYS;
    u_int32_t new_size = n<=2 ? 4 : 2*n;

    if (omt->tmparray_size<n ||
        (omt->tmparray_size/2 >= new_size)) {
        /* Malloc and free instead of realloc (saves the memcpy). */
        MALLOC_N(new_size, new_tmparray);
        if (new_tmparray==NULL) { r = errno; goto cleanup; }
    }
    /* Rebuild/realloc the nodes array iff any of the following:
     *  The array is smaller than the number of elements we want.
     *  We are increasing the number of elements and there is no free space.
     *  The array is too large. */
    u_int32_t num_nodes = nweight(omt, omt->root);
    if ((omt->node_capacity/2 >= new_size) ||
        (omt->free_idx>=omt->node_capacity && num_nodes<n) ||
        (omt->node_capacity<n)) {
        if (choice==MAYBE_REBUILD) {
            MALLOC_N(num_nodes, tmp_values);
            if (tmp_values==NULL) { r = errno; goto cleanup;}
        }
        MALLOC_N(new_size, new_nodes);
        if (new_nodes==NULL)  { r = errno; goto cleanup; }
    }

    /* Nothing can fail now.  Atomically update both sizes. */
    if (new_tmparray) {
       toku_free(omt->tmparray); 
       omt->tmparray      = new_tmparray;
       omt->tmparray_size = new_size;
    }
    if (new_nodes) {
        /* Rebuild the tree in the new array, leftshifted, in preorder */
        if (choice==MAYBE_REBUILD) {
            fill_array_with_subtree_values(omt, tmp_values, omt->root);
        }
        toku_free(omt->nodes);
        omt->nodes         = new_nodes;
        omt->node_capacity = new_size;
        omt->free_idx      = 0; /* Allocating from mempool starts over. */
        omt->root          = NODE_NULL;
        if (choice==MAYBE_REBUILD) {
            create_from_sorted_array_internal(omt, &omt->root, tmp_values, num_nodes);
        }
    }
    r = 0;
cleanup:
    if (r!=0) {
        if (new_tmparray) toku_free(new_tmparray);
        if (new_nodes)    toku_free(new_nodes);
    }
    if (tmp_values)       toku_free(tmp_values);
    return r;
}

static inline void fill_array_with_subtree_idxs(OMT omt, node_idx *array, node_idx tree_idx) {
    if (tree_idx==NODE_NULL) return;
    OMT_NODE tree = omt->nodes+tree_idx;
    fill_array_with_subtree_idxs(omt, array, tree->left);
    array[nweight(omt, tree->left)] = tree_idx;
    fill_array_with_subtree_idxs(omt, array+nweight(omt, tree->left)+1, tree->right); 
}

/* Reuses existing OMT_NODE structures (used for rebalancing). */
static inline void rebuild_subtree_from_idxs(OMT omt, node_idx *n_idxp, node_idx *idxs,
                                             u_int32_t numvalues) {
    if (numvalues==0) {
        *n_idxp=NODE_NULL;
    } else {
        u_int32_t halfway = numvalues/2;
        node_idx newidx   = idxs[halfway];
        OMT_NODE newnode  = omt->nodes+newidx;
        newnode->weight   = numvalues;
        // value is already in there.
        rebuild_subtree_from_idxs(omt, &newnode->left,  idxs,           halfway);
        rebuild_subtree_from_idxs(omt, &newnode->right, idxs+halfway+1, numvalues-(halfway+1));
        *n_idxp = newidx;
    }
}

static inline void rebalance(OMT omt, node_idx *n_idxp) {
    node_idx idx = *n_idxp;
    OMT_NODE n   = omt->nodes+idx;
    fill_array_with_subtree_idxs(omt, omt->tmparray, idx);
    rebuild_subtree_from_idxs(omt, n_idxp, omt->tmparray, n->weight);
}

static inline BOOL will_need_rebalance(OMT omt, node_idx n_idx, int leftmod, int rightmod) {
    if (n_idx==NODE_NULL) return FALSE;
    OMT_NODE n = omt->nodes+n_idx;
    // one of the 1's is for the root.
    // the other is to take ceil(n/2)
    u_int32_t weight_left  = nweight(omt, n->left)  + leftmod;
    u_int32_t weight_right = nweight(omt, n->right) + rightmod;
    return ((1+weight_left < (1+1+weight_right)/2)
            ||
            (1+weight_right < (1+1+weight_left)/2));
} 

static inline void insert_internal(OMT omt, node_idx *n_idxp, OMTVALUE value, u_int32_t index, node_idx **rebalance_idx) {
    if (*n_idxp==NODE_NULL) {
        assert(index==0);
        node_idx newidx  = omt_node_malloc(omt);
        OMT_NODE newnode = omt->nodes+newidx;
        newnode->weight  = 1;
        newnode->left    = NODE_NULL;
        newnode->right   = NODE_NULL;
        newnode->value   = value;
        *n_idxp = newidx;
    } else {
        node_idx idx = *n_idxp;
        OMT_NODE n   = omt->nodes+idx;
        n->weight++;
        if (index <= nweight(omt, n->left)) {
            if (*rebalance_idx==NULL && will_need_rebalance(omt, idx, 1, 0)) {
                *rebalance_idx = n_idxp;
            }
            insert_internal(omt, &n->left,  value, index, rebalance_idx);
        } else {
            if (*rebalance_idx==NULL && will_need_rebalance(omt, idx, 0, 1)) {
                *rebalance_idx = n_idxp;
            }
            u_int32_t sub_index = index-nweight(omt, n->left)-1;
            insert_internal(omt, &n->right, value, sub_index, rebalance_idx);
        }
    }
}

int toku_omt_insert_at(OMT omt, OMTVALUE value, u_int32_t index) {
    int r;
    invalidate_cursors(omt);
    if (index>nweight(omt, omt->root)) return EINVAL;
    if ((r=maybe_resize_and_rebuild(omt, 1+nweight(omt, omt->root), MAYBE_REBUILD))) return r;
    node_idx* rebalance_idx = NULL;
    insert_internal(omt, &omt->root, value, index, &rebalance_idx);
    if (rebalance_idx) rebalance(omt, rebalance_idx);
    return 0;
}

static inline void set_at_internal(OMT omt, node_idx n_idx, OMTVALUE v, u_int32_t index) {
    assert(n_idx!=NODE_NULL);
    OMT_NODE n = omt->nodes+n_idx;
    if (index<nweight(omt, n->left))
	set_at_internal(omt, n->left, v, index);
    else if (index==nweight(omt, n->left)) {
	n->value = v;
    } else {
	set_at_internal(omt, n->right, v, index-nweight(omt, n->left)-1);
    }
}

int toku_omt_set_at (OMT omt, OMTVALUE value, u_int32_t index) {
    if (index>=nweight(omt, omt->root)) return EINVAL;
    set_at_internal(omt, omt->root, value, index);
    return 0;
}

static inline void delete_internal(OMT omt, node_idx *n_idxp, u_int32_t index, OMTVALUE *vp, node_idx **rebalance_idx) {
    assert(*n_idxp!=NODE_NULL);
    OMT_NODE n = omt->nodes+*n_idxp;
    if (index < nweight(omt, n->left)) {
        n->weight--;
        if (*rebalance_idx==NULL && will_need_rebalance(omt, *n_idxp, -1, 0)) {
            *rebalance_idx = n_idxp;
        }
        delete_internal(omt, &n->left, index, vp, rebalance_idx);
    } else if (index == nweight(omt, n->left)) {
        if (n->left==NODE_NULL) {
            u_int32_t idx = *n_idxp;
            *n_idxp = n->right;
            *vp     = n->value;
            omt_node_free(omt, idx);
        } else if (n->right==NODE_NULL) {
            u_int32_t idx = *n_idxp;
            *n_idxp = n->left;
            *vp     = n->value;
            omt_node_free(omt, idx);
        } else {
            OMTVALUE zv;
            // delete the successor of index, get the value, and store it here.
            if (*rebalance_idx==NULL && will_need_rebalance(omt, *n_idxp, 0, -1)) {
                *rebalance_idx = n_idxp;
            }
            delete_internal(omt, &n->right, 0, &zv, rebalance_idx);
            n->value = zv;
            n->weight--;
        }
    } else {
        n->weight--;
        if (*rebalance_idx==NULL && will_need_rebalance(omt, *n_idxp, 0, -1)) {
            *rebalance_idx = n_idxp;
        }
        delete_internal(omt, &n->right, index-nweight(omt, n->left)-1, vp, rebalance_idx);
    }
}

int toku_omt_delete_at(OMT omt, u_int32_t index) {
    OMTVALUE v;
    int r;
    invalidate_cursors(omt);
    if (index>=nweight(omt, omt->root)) return EINVAL;
    if ((r=maybe_resize_and_rebuild(omt, -1+nweight(omt, omt->root), MAYBE_REBUILD))) return r;
    node_idx* rebalance_idx = NULL;
    delete_internal(omt, &omt->root, index, &v, &rebalance_idx);
    if (rebalance_idx) rebalance(omt, rebalance_idx);
    return 0;
}

static inline void fetch_internal(OMT V, node_idx idx, u_int32_t i, OMTVALUE *v) {
    OMT_NODE n = V->nodes+idx;
    if (i < nweight(V, n->left)) {
        fetch_internal(V, n->left,  i, v);
    } else if (i == nweight(V, n->left)) {
        *v = n->value;
    } else {
        fetch_internal(V, n->right, i-nweight(V, n->left)-1, v);
    }
}

int toku_omt_fetch(OMT V, u_int32_t i, OMTVALUE *v, OMTCURSOR c) {
    if (i>=nweight(V, V->root)) return EINVAL;
    fetch_internal(V, V->root, i, v);
    if (c) {
	associate(V,c);
	c->index = i;
    }
    return 0;
}

static inline int iterate_internal(OMT omt, u_int32_t left, u_int32_t right,
                                   node_idx n_idx, u_int32_t idx,
                                   int (*f)(OMTVALUE, u_int32_t, void*), void*v) {
    int r;
    if (n_idx==NODE_NULL) return 0;
    OMT_NODE n = omt->nodes+n_idx;
    u_int32_t idx_root = idx+nweight(omt,n->left);
    if (left< idx_root && (r=iterate_internal(omt, left, right, n->left, idx, f, v))) return r;
    if (left<=idx_root && idx_root<right && (r=f(n->value, idx_root, v))) return r;
    if (idx_root+1<right) return iterate_internal(omt, left, right, n->right, idx_root+1, f, v);
    return 0;
}

int toku_omt_iterate(OMT omt, int (*f)(OMTVALUE, u_int32_t, void*), void*v) {
    return iterate_internal(omt, 0, nweight(omt, omt->root), omt->root, 0, f, v);
}

int toku_omt_iterate_on_range(OMT omt, u_int32_t left, u_int32_t right, int (*f)(OMTVALUE, u_int32_t, void*), void*v) {
    return iterate_internal(omt, left, right, omt->root, 0, f, v);
}

int toku_omt_insert(OMT omt, OMTVALUE value, int(*h)(OMTVALUE, void*v), void *v, u_int32_t *index) {
    int r;
    u_int32_t idx;

    invalidate_cursors(omt);

    r = toku_omt_find_zero(omt, h, v, NULL, &idx, NULL);
    if (r==0) {
        if (index) *index = idx;
        return DB_KEYEXIST;
    }
    if (r!=DB_NOTFOUND) return r;

    if ((r = toku_omt_insert_at(omt, value, idx))) return r;
    if (index) *index = idx;

    return 0;
}

static inline int find_internal_zero(OMT omt, node_idx n_idx, int (*h)(OMTVALUE, void*extra), void*extra, OMTVALUE *value, u_int32_t *index)
// requires: index!=NULL
{
    if (n_idx==NODE_NULL) {
	*index = 0;
	return DB_NOTFOUND;
    }
    OMT_NODE n = omt->nodes+n_idx;
    int hv = h(n->value, extra);
    if (hv<0) {
        int r = find_internal_zero(omt, n->right, h, extra, value, index);
        *index += nweight(omt, n->left)+1;
        return r;
    } else if (hv>0) {
        return find_internal_zero(omt, n->left, h, extra, value, index);
    } else {
        int r = find_internal_zero(omt, n->left, h, extra, value, index);
        if (r==DB_NOTFOUND) {
            *index = nweight(omt, n->left);
            if (value!=NULL) *value = n->value;
            r = 0;
        }
        return r;
    }
}

int toku_omt_find_zero(OMT V, int (*h)(OMTVALUE, void*extra), void*extra, OMTVALUE *value, u_int32_t *index, OMTCURSOR c) {
    u_int32_t tmp_index;
    if (index==NULL) index=&tmp_index;
    int r = find_internal_zero(V, V->root, h, extra, value, index);
    if (c && r==0) {
	associate(V,c);
	c->index = *index;
    } else {
	toku_omt_cursor_invalidate(c);
    }
    return r;
}

//  If direction <0 then find the largest  i such that h(V_i,extra)<0.
static inline int find_internal_minus(OMT omt, node_idx n_idx, int (*h)(OMTVALUE, void*extra), void*extra, OMTVALUE *value, u_int32_t *index)
// requires: index!=NULL
{
    if (n_idx==NODE_NULL) return DB_NOTFOUND;
    OMT_NODE n = omt->nodes+n_idx;
    int hv = h(n->value, extra);
    if (hv<0) {
        int r = find_internal_minus(omt, n->right, h, extra, value, index);
        if (r==0) *index += nweight(omt, n->left)+1;
        else if (r==DB_NOTFOUND) {
            *index = nweight(omt, n->left);
            if (value!=NULL) *value = n->value;
            r = 0;
        }
        return r;
    } else {
        return find_internal_minus(omt, n->left, h, extra, value, index);
    }
}

//  If direction >0 then find the smallest i such that h(V_i,extra)>0.
static inline int find_internal_plus(OMT omt, node_idx n_idx, int (*h)(OMTVALUE, void*extra), void*extra, OMTVALUE *value, u_int32_t *index)
// requires: index!=NULL
{
    if (n_idx==NODE_NULL) return DB_NOTFOUND;
    OMT_NODE n = omt->nodes+n_idx;
    int hv = h(n->value, extra);
    if (hv>0) {
        int r = find_internal_plus(omt, n->left, h, extra, value, index);
        if (r==DB_NOTFOUND) {
            *index = nweight(omt, n->left);
            if (value!=NULL) *value = n->value;
            r = 0;
        }
        return r;
    } else {
        int r = find_internal_plus(omt, n->right, h, extra, value, index);
        if (r==0) *index += nweight(omt, n->left)+1;
        return r;
    }
}

int toku_omt_find(OMT V, int (*h)(OMTVALUE, void*extra), void*extra, int direction, OMTVALUE *value, u_int32_t *index, OMTCURSOR c) {
    u_int32_t tmp_index;
    int r;
    if (index==NULL) index=&tmp_index;
    if (direction==0) {
	abort();
    } else if (direction<0) {
        r = find_internal_minus(V, V->root, h, extra, value, index);
    } else {
        r = find_internal_plus( V, V->root, h, extra, value, index);
    }
    if (c && r==0) {
	associate(V,c);
	c->index=*index;
    } else {
	toku_omt_cursor_invalidate(c);
    }
    return r;
}

int toku_omt_split_at(OMT omt, OMT *newomtp, u_int32_t index) {
    int r                = ENOSYS;
    OMT newomt           = NULL;
    OMTVALUE *tmp_values = NULL;
    invalidate_cursors(omt);
    if (index>nweight(omt, omt->root)) { r = EINVAL; goto cleanup; }
    u_int32_t newsize = nweight(omt, omt->root)-index;
    if ((r = omt_create_internal(&newomt, newsize))) goto cleanup;
    MALLOC_N(nweight(omt, omt->root), tmp_values);
    if (tmp_values==NULL) { r = errno; goto cleanup; }
    fill_array_with_subtree_values(omt, tmp_values, omt->root);
    // Modify omt's array at the last possible moment, since after this nothing can fail.
    if ((r = maybe_resize_and_rebuild(omt, index, TRUE))) goto cleanup;
    create_from_sorted_array_internal(omt,    &omt->root,    tmp_values,       index);
    create_from_sorted_array_internal(newomt, &newomt->root, tmp_values+index, newsize);
    *newomtp = newomt;
    r = 0;
cleanup:
    if (r!=0) {
        if (newomt) toku_omt_destroy(&newomt);
    }
    if (tmp_values) toku_free(tmp_values);
    return r;
}
    
int toku_omt_merge(OMT leftomt, OMT rightomt, OMT *newomtp) {
    int r                = ENOSYS;
    OMT newomt           = NULL;
    OMTVALUE *tmp_values = NULL;
    invalidate_cursors(leftomt);
    invalidate_cursors(rightomt);
    u_int32_t newsize = toku_omt_size(leftomt)+toku_omt_size(rightomt);
    if ((r = omt_create_internal(&newomt, newsize))) goto cleanup;
    MALLOC_N(newsize, tmp_values);
    if (tmp_values==NULL) { r = errno; goto cleanup; }

    fill_array_with_subtree_values(leftomt,  tmp_values,                        leftomt->root);
    fill_array_with_subtree_values(rightomt, tmp_values+toku_omt_size(leftomt), rightomt->root);
    create_from_sorted_array_internal(newomt, &newomt->root, tmp_values, newsize);
    toku_omt_destroy(&leftomt);
    toku_omt_destroy(&rightomt);
    *newomtp = newomt;
    r = 0;
cleanup:
    if (r!=0) {
        if (newomt) toku_omt_destroy(&newomt);
    }
    if (tmp_values) toku_free(tmp_values);
    return r;
}

void toku_omt_clear(OMT omt) {
    invalidate_cursors(omt);
    omt->free_idx = 0;
    omt->root     = NODE_NULL;
}

unsigned long toku_omt_memory_size (OMT omt) {
    return sizeof(*omt)+omt->node_capacity*sizeof(omt->nodes[0]) + omt->tmparray_size*sizeof(omt->tmparray[0]);
}

int toku_omt_cursor_is_valid (OMTCURSOR c) {
    return c->omt!=NULL;
}

int toku_omt_cursor_next (OMTCURSOR c, OMTVALUE *v) {
    if (c->omt == NULL) return EINVAL;
    c->index++;
    int r = toku_omt_fetch(c->omt, c->index, v, NULL);
    if (r!=0) toku_omt_cursor_invalidate(c);
    return r;
}

int toku_omt_cursor_prev (OMTCURSOR c, OMTVALUE *v) {
    if (c->omt == NULL) return EINVAL;
    if (c->index==0) {
       toku_omt_cursor_invalidate(c);
       return EINVAL;
    }
    c->index--;
    int r = toku_omt_fetch(c->omt, c->index, v, NULL);
    if (r!=0) toku_omt_cursor_invalidate(c);
    return r;
}

int toku_omt_cursor_current (OMTCURSOR c, OMTVALUE *v) {
    if (c->omt == NULL) return EINVAL;
    int r = toku_omt_fetch(c->omt, c->index, v, NULL);
    if (r!=0) toku_omt_cursor_invalidate(c);
    return r;
}

