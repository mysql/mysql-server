#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <errno.h>
#include <sys/types.h>
#include <stdint.h>
#include <string.h>

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


struct omt_array {
    u_int32_t  start_idx;
    u_int32_t  num_values;
    OMTVALUE  *values;
};

struct omt_tree {
    node_idx   root;

    OMT_NODE   nodes;
    node_idx   free_idx;
};

struct omt {
    BOOL       is_array;
    u_int32_t  capacity;
    union {
        struct omt_array a;
        struct omt_tree t;
    } i;
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
    result->is_array       = TRUE;
    result->capacity       = 2*num_starting_nodes;
    result->i.a.num_values = 0;
    result->i.a.start_idx  = 0;
    MALLOC_N(result->capacity, result->i.a.values);
    if (result->i.a.values==NULL) {
        toku_free(result);
        return errno;
    }
    result->associated = NULL;
    *omtp = result;
    return 0;
}

int toku_omt_cursor_create (OMTCURSOR *omtcp) {
    OMTCURSOR MALLOC(c);
    if (c==NULL) return errno;
    c->omt = NULL;
    c->next = c->prev = NULL;
    *omtcp = c;
    return 0;
}

OMT toku_omt_cursor_get_omt(OMTCURSOR c) {
    return c->omt;
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

static inline u_int32_t nweight(OMT omt, node_idx idx) {
    if (idx==NODE_NULL) return 0;
    else return (omt->i.t.nodes+idx)->weight;
}

static inline u_int32_t omt_size(OMT omt) {
    return omt->is_array ? omt->i.a.num_values : nweight(omt, omt->i.t.root);
}

static inline node_idx omt_node_malloc(OMT omt) {
    assert(omt->i.t.free_idx < omt->capacity);
    return omt->i.t.free_idx++;
}

static inline void omt_node_free(OMT omt, node_idx idx) {
    assert(idx < omt->capacity);
}

static inline void fill_array_with_subtree_values(OMT omt, OMTVALUE *array, node_idx tree_idx) {
    if (tree_idx==NODE_NULL) return;
    OMT_NODE tree = omt->i.t.nodes+tree_idx;
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
static inline void rebuild_from_sorted_array(OMT omt, node_idx *n_idxp,
                                             OMTVALUE *values, u_int32_t numvalues) {
    if (numvalues==0) {
        *n_idxp = NODE_NULL;
    } else {
        u_int32_t halfway = numvalues/2;
        node_idx newidx   = omt_node_malloc(omt);
        OMT_NODE newnode  = omt->i.t.nodes+newidx;
        newnode->weight   = numvalues;
        newnode->value    = values[halfway]; 
        *n_idxp = newidx; // update everything before the recursive calls so the second call can be a tail call.
        rebuild_from_sorted_array(omt, &newnode->left,  values,           halfway);
        rebuild_from_sorted_array(omt, &newnode->right, values+halfway+1, numvalues-(halfway+1));
    }
}

static inline int maybe_resize_array(OMT omt, u_int32_t n) {
    u_int32_t new_size = n<=2 ? 4 : 2*n;
    u_int32_t room = omt->capacity - omt->i.a.start_idx;

    if (room<n || omt->capacity/2>=new_size) {
        OMTVALUE *MALLOC_N(new_size, tmp_values);
        if (tmp_values==NULL) return errno;
        memcpy(tmp_values, omt->i.a.values+omt->i.a.start_idx,
               omt->i.a.num_values*sizeof(*tmp_values));
        omt->i.a.start_idx = 0;
        omt->capacity  = new_size;
        toku_free(omt->i.a.values);
        omt->i.a.values    = tmp_values;
    }
    return 0;
}

static int omt_convert_to_tree(OMT omt) {
    if (!omt->is_array) return 0;
    u_int32_t num_nodes = omt_size(omt);
    u_int32_t new_size  = num_nodes*2;
    new_size = new_size < 4 ? 4 : new_size;

    OMT_NODE MALLOC_N(new_size, new_nodes);
    if (new_nodes==NULL) return errno;
    OMTVALUE *values     = omt->i.a.values;
    OMTVALUE *tmp_values = values + omt->i.a.start_idx;
    omt->is_array          = FALSE;
    omt->i.t.nodes         = new_nodes;
    omt->capacity          = new_size;
    omt->i.t.free_idx      = 0; /* Allocating from mempool starts over. */
    omt->i.t.root          = NODE_NULL;
    rebuild_from_sorted_array(omt, &omt->i.t.root, tmp_values, num_nodes);
    toku_free(values);
    return 0;
}

static int omt_convert_to_array(OMT omt) {
    if (omt->is_array) return 0;
    u_int32_t num_values = omt_size(omt);
    u_int32_t new_size  = 2*num_values;
    new_size = new_size < 4 ? 4 : new_size;

    OMTVALUE *MALLOC_N(new_size, tmp_values);
    if (tmp_values==NULL) return errno;
    fill_array_with_subtree_values(omt, tmp_values, omt->i.t.root);
    toku_free(omt->i.t.nodes);
    omt->is_array       = TRUE;
    omt->capacity       = new_size;
    omt->i.a.num_values = num_values;
    omt->i.a.values     = tmp_values;
    omt->i.a.start_idx  = 0;
    return 0;
}

static inline int maybe_resize_or_convert(OMT omt, u_int32_t n) {
    if (omt->is_array) return maybe_resize_array(omt, n);

    u_int32_t new_size = n<=2 ? 4 : 2*n;

    /* Rebuild/realloc the nodes array iff any of the following:
     *  The array is smaller than the number of elements we want.
     *  We are increasing the number of elements and there is no free space.
     *  The array is too large. */
    //Rebuilding means we first turn it to an array.
    //Lets pause at the array form.
    u_int32_t num_nodes = nweight(omt, omt->i.t.root);
    if ((omt->capacity/2 >= new_size) ||
        (omt->i.t.free_idx>=omt->capacity && num_nodes<n) ||
        (omt->capacity<n)) {
        return omt_convert_to_array(omt);
    }
    return 0;
}

static inline void fill_array_with_subtree_idxs(OMT omt, node_idx *array, node_idx tree_idx) {
    if (tree_idx==NODE_NULL) return;
    OMT_NODE tree = omt->i.t.nodes+tree_idx;
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
        OMT_NODE newnode  = omt->i.t.nodes+newidx;
        newnode->weight   = numvalues;
        // value is already in there.
        rebuild_subtree_from_idxs(omt, &newnode->left,  idxs,           halfway);
        rebuild_subtree_from_idxs(omt, &newnode->right, idxs+halfway+1, numvalues-(halfway+1));
        *n_idxp = newidx;
    }
}

static inline void rebalance(OMT omt, node_idx *n_idxp) {
    node_idx idx = *n_idxp;
    if (idx==omt->i.t.root) {
        //Try to convert to an array.
        //If this fails, (malloc) nothing will have changed.
        //In the failure case we continue on to the standard rebalance
        //algorithm.
        int r = omt_convert_to_array(omt);
        if (r==0) return;
    }
    OMT_NODE n   = omt->i.t.nodes+idx;
    node_idx *tmp_array;
    size_t mem_needed = n->weight*sizeof(*tmp_array);
    size_t mem_free   = (omt->capacity-omt->i.t.free_idx)*sizeof(*omt->i.t.nodes);
    BOOL malloced;
    if (mem_needed<=mem_free) {
        //There is sufficient free space at the end of the nodes array
        //to hold enough node indexes to rebalance.
        malloced  = FALSE;
        tmp_array = (node_idx*)(omt->i.t.nodes+omt->i.t.free_idx);
    }
    else {
        malloced  = TRUE;
        MALLOC_N(n->weight, tmp_array);
        if (tmp_array==NULL) return;    //Don't rebalance.  Still a working tree.
    }
    fill_array_with_subtree_idxs(omt, tmp_array, idx);
    rebuild_subtree_from_idxs(omt, n_idxp, tmp_array, n->weight);
    if (malloced) toku_free(tmp_array);
}

static inline BOOL will_need_rebalance(OMT omt, node_idx n_idx, int leftmod, int rightmod) {
    if (n_idx==NODE_NULL) return FALSE;
    OMT_NODE n = omt->i.t.nodes+n_idx;
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
        OMT_NODE newnode = omt->i.t.nodes+newidx;
        newnode->weight  = 1;
        newnode->left    = NODE_NULL;
        newnode->right   = NODE_NULL;
        newnode->value   = value;
        *n_idxp = newidx;
    } else {
        node_idx idx = *n_idxp;
        OMT_NODE n   = omt->i.t.nodes+idx;
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

static inline void set_at_internal_array(OMT omt, OMTVALUE v, u_int32_t index) {
    omt->i.a.values[omt->i.a.start_idx+index] = v;
}

static inline void set_at_internal(OMT omt, node_idx n_idx, OMTVALUE v, u_int32_t index) {
    assert(n_idx!=NODE_NULL);
    OMT_NODE n = omt->i.t.nodes+n_idx;
    if (index<nweight(omt, n->left))
	set_at_internal(omt, n->left, v, index);
    else if (index==nweight(omt, n->left)) {
	n->value = v;
    } else {
	set_at_internal(omt, n->right, v, index-nweight(omt, n->left)-1);
    }
}

static inline void delete_internal(OMT omt, node_idx *n_idxp, u_int32_t index, OMTVALUE *vp, node_idx **rebalance_idx) {
    assert(*n_idxp!=NODE_NULL);
    OMT_NODE n = omt->i.t.nodes+*n_idxp;
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

static inline void fetch_internal_array(OMT V, u_int32_t i, OMTVALUE *v) {
    *v = V->i.a.values[V->i.a.start_idx+i];
}

static inline void fetch_internal(OMT V, node_idx idx, u_int32_t i, OMTVALUE *v) {
    OMT_NODE n = V->i.t.nodes+idx;
    if (i < nweight(V, n->left)) {
        fetch_internal(V, n->left,  i, v);
    } else if (i == nweight(V, n->left)) {
        *v = n->value;
    } else {
        fetch_internal(V, n->right, i-nweight(V, n->left)-1, v);
    }
}

static inline int iterate_internal_array(OMT omt,
                                  u_int32_t left, u_int32_t right,
                                  int (*f)(OMTVALUE, u_int32_t, void*), void*v) {
    int r;
    u_int32_t i;

    for (i = left; i < right; i++) {
        r = f(omt->i.a.values[i+omt->i.a.start_idx], i, v);
        if (r!=0) return r;
    }
    return 0;
}

static inline int iterate_internal(OMT omt, u_int32_t left, u_int32_t right,
                                   node_idx n_idx, u_int32_t idx,
                                   int (*f)(OMTVALUE, u_int32_t, void*), void*v) {
    int r;
    if (n_idx==NODE_NULL) return 0;
    OMT_NODE n = omt->i.t.nodes+n_idx;
    u_int32_t idx_root = idx+nweight(omt,n->left);
    if (left< idx_root && (r=iterate_internal(omt, left, right, n->left, idx, f, v))) return r;
    if (left<=idx_root && idx_root<right && (r=f(n->value, idx_root, v))) return r;
    if (idx_root+1<right) return iterate_internal(omt, left, right, n->right, idx_root+1, f, v);
    return 0;
}

static inline int find_internal_zero_array(OMT omt, int (*h)(OMTVALUE, void*extra), void*extra, OMTVALUE *value, u_int32_t *index) {
    u_int32_t min   = omt->i.a.start_idx;
    u_int32_t limit = omt->i.a.start_idx + omt->i.a.num_values;
    u_int32_t best_pos  = NODE_NULL;
    u_int32_t best_zero = NODE_NULL;

    while (min!=limit) {
        u_int32_t mid = (min + limit) / 2;
        int hv = h(omt->i.a.values[mid], extra);
        if (hv<0) {
            min = mid+1;
        }
        else if (hv>0) {
            best_pos  = mid; 
            limit     = mid;
        }
        else {
            best_zero = mid;
            limit     = mid;
        }
    }
    if (best_zero!=NODE_NULL) {
        //Found a zero
        if (value!=NULL) *value = omt->i.a.values[best_zero];
        *index = best_zero - omt->i.a.start_idx;
        return 0;
    }
    if (best_pos!=NODE_NULL) *index = best_pos - omt->i.a.start_idx;
    else                     *index = omt->i.a.num_values;
    return DB_NOTFOUND;
}

static inline int find_internal_zero(OMT omt, node_idx n_idx, int (*h)(OMTVALUE, void*extra), void*extra, OMTVALUE *value, u_int32_t *index)
// requires: index!=NULL
{
    if (n_idx==NODE_NULL) {
	*index = 0;
	return DB_NOTFOUND;
    }
    OMT_NODE n = omt->i.t.nodes+n_idx;
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


static inline int find_internal_plus_array(OMT omt, int (*h)(OMTVALUE, void*extra), void*extra, OMTVALUE *value, u_int32_t *index) {
    u_int32_t min   = omt->i.a.start_idx;
    u_int32_t limit = omt->i.a.start_idx + omt->i.a.num_values;
    u_int32_t best  = NODE_NULL;

    while (min!=limit) {
        u_int32_t mid = (min + limit) / 2;
        int hv = h(omt->i.a.values[mid], extra);
        if (hv>0) {
            best  = mid;
            limit = mid;
        }
        else {
            min = mid+1;
        }
    }
    if (best==NODE_NULL) return DB_NOTFOUND;
    if (value!=NULL) *value = omt->i.a.values[best];
    *index = best - omt->i.a.start_idx;
    return 0;
}

static inline int find_internal_minus_array(OMT omt, int (*h)(OMTVALUE, void*extra), void*extra, OMTVALUE *value, u_int32_t *index) {
    u_int32_t min   = omt->i.a.start_idx;
    u_int32_t limit = omt->i.a.start_idx + omt->i.a.num_values;
    u_int32_t best  = NODE_NULL;

    while (min!=limit) {
        u_int32_t mid = (min + limit) / 2;
        int hv = h(omt->i.a.values[mid], extra);
        if (hv<0) {
            best = mid;
            min  = mid+1;
        }
        else {
            limit = mid;
        }
    }
    if (best==NODE_NULL) return DB_NOTFOUND;
    if (value!=NULL) *value = omt->i.a.values[best];
    *index = best - omt->i.a.start_idx;
    return 0;
}

//  If direction <0 then find the largest  i such that h(V_i,extra)<0.
static inline int find_internal_minus(OMT omt, node_idx n_idx, int (*h)(OMTVALUE, void*extra), void*extra, OMTVALUE *value, u_int32_t *index)
// requires: index!=NULL
{
    if (n_idx==NODE_NULL) return DB_NOTFOUND;
    OMT_NODE n = omt->i.t.nodes+n_idx;
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
    OMT_NODE n = omt->i.t.nodes+n_idx;
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

int toku_omt_cursor_current_index(OMTCURSOR c, u_int32_t *index) {
    if (c->omt == NULL) return EINVAL;
    *index = c->index;
    return 0;
} 

//TODO: Put all omt API functions here.
int toku_omt_create (OMT *omtp) {
    return omt_create_internal(omtp, 2);
}

void toku_omt_destroy(OMT *omtp) {
    OMT omt=*omtp;
    invalidate_cursors(omt);
    if (omt->is_array) toku_free(omt->i.a.values);
    else               toku_free(omt->i.t.nodes);
    toku_free(omt);
    *omtp=NULL;
}

u_int32_t toku_omt_size(OMT V) {
    return omt_size(V);
}

int toku_omt_create_from_sorted_array(OMT *omtp, OMTVALUE *values, u_int32_t numvalues) {
    OMT omt = NULL;
    int r;
    if ((r = omt_create_internal(&omt, numvalues))) return r;
    memcpy(omt->i.a.values, values, numvalues*sizeof(*values));
    omt->i.a.num_values = numvalues;
    *omtp=omt;
    return 0;
}

int toku_omt_insert_at(OMT omt, OMTVALUE value, u_int32_t index) {
    int r;
    invalidate_cursors(omt);
    if (index>omt_size(omt)) return EINVAL;
    if ((r=maybe_resize_or_convert(omt, 1+omt_size(omt)))) return r;
    if (omt->is_array && index!=omt->i.a.num_values &&
        (index!=0 || omt->i.a.start_idx==0)) {
        if ((r=omt_convert_to_tree(omt))) return r;
    }
    if (omt->is_array) {
        if (index==omt->i.a.num_values) {
            omt->i.a.values[omt->i.a.start_idx+(omt->i.a.num_values)] = value;
        }
        else {
            omt->i.a.values[--omt->i.a.start_idx] = value;
        }
        omt->i.a.num_values++;
    }
    else {
        node_idx* rebalance_idx = NULL;
        insert_internal(omt, &omt->i.t.root, value, index, &rebalance_idx);
        if (rebalance_idx) rebalance(omt, rebalance_idx);
    }
    return 0;
}

int toku_omt_set_at (OMT omt, OMTVALUE value, u_int32_t index) {
    if (index>=omt_size(omt)) return EINVAL;
    if (omt->is_array) {
        set_at_internal_array(omt, value, index);
    }
    else {
        set_at_internal(omt, omt->i.t.root, value, index);
    }
    return 0;
}

int toku_omt_delete_at(OMT omt, u_int32_t index) {
    OMTVALUE v;
    int r;
    invalidate_cursors(omt);
    if (index>=omt_size(omt)) return EINVAL;
    if ((r=maybe_resize_or_convert(omt, -1+omt_size(omt)))) return r;
    if (omt->is_array && index!=0 && index!=omt->i.a.num_values-1) {
        if ((r=omt_convert_to_tree(omt))) return r;
    }
    if (omt->is_array) {
        //Testing for 0 does not rule out it being the last entry.
        //Test explicitly for num_values-1
        if (index!=omt->i.a.num_values-1) omt->i.a.start_idx++;
        omt->i.a.num_values--;
    }
    else {
        node_idx* rebalance_idx = NULL;
        delete_internal(omt, &omt->i.t.root, index, &v, &rebalance_idx);
        if (rebalance_idx) rebalance(omt, rebalance_idx);
    }
    return 0;
}

int toku_omt_fetch(OMT V, u_int32_t i, OMTVALUE *v, OMTCURSOR c) {
    if (i>=omt_size(V)) return EINVAL;
    if (V->is_array) {
        fetch_internal_array(V, i, v);
    }
    else {
        fetch_internal(V, V->i.t.root, i, v);
    }
    if (c) {
	associate(V,c);
	c->index = i;
    }
    return 0;
}

int toku_omt_iterate(OMT omt, int (*f)(OMTVALUE, u_int32_t, void*), void*v) {
    if (omt->is_array) {
        return iterate_internal_array(omt, 0, omt_size(omt), f, v);
    }
    return iterate_internal(omt, 0, nweight(omt, omt->i.t.root), omt->i.t.root, 0, f, v);
}

int toku_omt_iterate_on_range(OMT omt, u_int32_t left, u_int32_t right, int (*f)(OMTVALUE, u_int32_t, void*), void*v) {
    if (right>omt_size(omt)) return EINVAL;
    if (omt->is_array) {
        return iterate_internal_array(omt, left, right, f, v);
    }
    return iterate_internal(omt, left, right, omt->i.t.root, 0, f, v);
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

int toku_omt_find_zero(OMT V, int (*h)(OMTVALUE, void*extra), void*extra, OMTVALUE *value, u_int32_t *index, OMTCURSOR c) {
    u_int32_t tmp_index;
    if (index==NULL) index=&tmp_index;
    int r;
    if (V->is_array) {
        r = find_internal_zero_array(V, h, extra, value, index);
    }
    else {
        r = find_internal_zero(V, V->i.t.root, h, extra, value, index);
    }
    if (c && r==0) {
	associate(V,c);
	c->index = *index;
    } else {
	toku_omt_cursor_invalidate(c);
    }
    return r;
}

int toku_omt_find(OMT V, int (*h)(OMTVALUE, void*extra), void*extra, int direction, OMTVALUE *value, u_int32_t *index, OMTCURSOR c) {
    u_int32_t tmp_index;
    int r;
    if (index==NULL) index=&tmp_index;
    if (direction==0) {
	abort();
    } else if (direction<0) {
        if (V->is_array) {
            r = find_internal_minus_array(V, h, extra, value, index);
        }
        else {
            r = find_internal_minus(V, V->i.t.root, h, extra, value, index);
        }
    } else {
        if (V->is_array) {
            r = find_internal_plus_array(V, h, extra, value, index);
        }
        else {
            r = find_internal_plus( V, V->i.t.root, h, extra, value, index);
        }
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
    int r;
    OMT newomt;
    invalidate_cursors(omt);
    if (index>omt_size(omt)) return EINVAL;

    if ((r=omt_convert_to_array(omt))) return r;
    u_int32_t newsize = omt_size(omt)-index;
    if ((r=toku_omt_create_from_sorted_array(&newomt,
                                   omt->i.a.values+omt->i.a.start_idx+index,
                                   newsize))) return r;
    omt->i.a.num_values = index;
    if ((r=maybe_resize_array(omt, index))) {
        //Restore size.
        omt->i.a.num_values += newsize;
        toku_omt_destroy(&newomt);
        return r;
    }
    *newomtp = newomt;
    return 0;
}
    
int toku_omt_merge(OMT leftomt, OMT rightomt, OMT *newomtp) {
    int r;
    OMT newomt;
    invalidate_cursors(leftomt);
    invalidate_cursors(rightomt);
    u_int32_t newsize = omt_size(leftomt)+omt_size(rightomt);
    if ((r = omt_create_internal(&newomt, newsize))) return r;

    if (leftomt->is_array) {
        memcpy(newomt->i.a.values,
               leftomt->i.a.values+leftomt->i.a.start_idx,
               leftomt->i.a.num_values*sizeof(*newomt->i.a.values));
    }
    else {
        fill_array_with_subtree_values(leftomt,  newomt->i.a.values,                   leftomt->i.t.root);
    }
    if (rightomt->is_array) {
        memcpy(newomt->i.a.values+omt_size(leftomt),
               rightomt->i.a.values+rightomt->i.a.start_idx,
               rightomt->i.a.num_values*sizeof(*newomt->i.a.values));
    }
    else {
        fill_array_with_subtree_values(rightomt, newomt->i.a.values+omt_size(leftomt), rightomt->i.t.root);
    }
    newomt->i.a.num_values = newsize;
    toku_omt_destroy(&leftomt);
    toku_omt_destroy(&rightomt);
    *newomtp = newomt;
    return 0;
}

void toku_omt_clear(OMT omt) {
    invalidate_cursors(omt);
    if (omt->is_array) {
        omt->i.a.start_idx  = 0;
        omt->i.a.num_values = 0;
    }
    else {
        omt->i.t.free_idx = 0;
        omt->i.t.root     = NODE_NULL;
        int r = omt_convert_to_array(omt);
        assert((!omt->is_array) == (r!=0));
        //If we fail to convert (malloc), then nothing has changed.
        //Continue anyway.
    }
}

unsigned long toku_omt_memory_size (OMT omt) {
    if (omt->is_array) {
        return sizeof(*omt)+omt->capacity*sizeof(omt->i.a.values[0]);
    }
    return sizeof(*omt)+omt->capacity*sizeof(omt->i.t.nodes[0]);
}

