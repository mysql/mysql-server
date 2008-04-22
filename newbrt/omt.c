#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include <errno.h>
#include <sys/types.h>

typedef struct value *OMTVALUE;
#include "omt.h"
#include "../newbrt/memory.h"
#include "../newbrt/toku_assert.h"
#include "../include/db.h"


typedef struct omt_node *OMT_NODE;
struct omt_node {
    u_int32_t  weight; // how many values below us (including this node)
    OMT_NODE   left, right;
    OMTVALUE   value;
};

struct omt {
    OMT_NODE root;

    u_int32_t tmparray_size;
    OMT_NODE *tmparray;
};

int toku_omt_create (OMT *omtp) {
    OMT MALLOC(result);
    if (result==NULL) return errno;
    result->root=NULL;
    result->tmparray_size = 4;
    MALLOC_N(result->tmparray_size, result->tmparray);
    if (result->tmparray==0) {
        toku_free(result);
        return errno;
    }
    *omtp = result;
    return 0;
}

static u_int32_t nweight (OMT_NODE n) {
    if (n==NULL) return 0;
    else return n->weight;
}

static void fill_array_from_omt_nodes_tree (OMT_NODE *array, OMT_NODE tree) {
    if (tree==NULL) return;
    fill_array_from_omt_nodes_tree(array, tree->left);
    array[nweight(tree->left)] = tree;
    fill_array_from_omt_nodes_tree(array+nweight(tree->left)+1, tree->right); 
}

static void rebuild_from_sorted_array_of_omt_nodes(OMT_NODE *np, OMT_NODE *nodes, u_int32_t numvalues) {
    if (numvalues==0) {
        *np=NULL;
    } else {
        u_int32_t halfway = numvalues/2;
        OMT_NODE newnode = nodes[halfway];
        newnode->weight = numvalues;
        // value is already in there.
        rebuild_from_sorted_array_of_omt_nodes(&newnode->left,  nodes,           halfway);
        rebuild_from_sorted_array_of_omt_nodes(&newnode->right, nodes+halfway+1, numvalues-(halfway+1));
        *np = newnode;
    }
}

static void maybe_rebalance (OMT omt, OMT_NODE *np) {
    OMT_NODE n = *np;
    if (n==0) return;
    // one of the 1's is for the root.
    // the other is to take ceil(n/2)
    if (((1+nweight(n->left)) < (1+1+nweight(n->right))/2)
        ||
        ((1+nweight(n->right)) < (1+1+nweight(n->left))/2)) {
        // Must rebalance the tree.
        fill_array_from_omt_nodes_tree(omt->tmparray, *np);
        rebuild_from_sorted_array_of_omt_nodes(np, omt->tmparray, nweight(*np));
    }
}

static int insert_internal (OMT omt, OMT_NODE *np, OMTVALUE value, u_int32_t index) {
    if (*np==0) {
        assert(index==0);
        OMT_NODE MALLOC(newnode);
        if (newnode==0) return errno;
        newnode->weight = 1;
        newnode->left   = NULL;
        newnode->right  = NULL;
        newnode->value  = value;
        *np = newnode;
        return 0;
    } else {
        OMT_NODE n=*np;
        int r;
        if (index <= nweight(n->left)) {
            if ((r = insert_internal(omt, &n->left, value,  index))) return r;
        } else {
            if ((r = insert_internal(omt, &n->right, value, index-nweight(n->left)-1))) return r;
        }
        n->weight++;
        maybe_rebalance(omt, np);
        return 0;
    }
}

static int make_sure_array_is_sized_ok (OMT omt, u_int32_t n) {
    u_int32_t new_size;
    if (omt->tmparray_size < n) {
        new_size = 2*n;
    do_realloc: ;
        OMT_NODE *newarray = toku_realloc(omt->tmparray, new_size * sizeof(*newarray));
        if (newarray==0) return errno;
        omt->tmparray = newarray;
        omt->tmparray_size = new_size;
    } else if (omt->tmparray_size/4 > n && n>=2) {
        new_size = 2*n;
        goto do_realloc;
    }
    return 0;
}

int toku_omt_insert_at (OMT omt, OMTVALUE value, u_int32_t index) {
    int r;
    if ((r=make_sure_array_is_sized_ok(omt, 1+nweight(omt->root)))) return r;
    return insert_internal(omt, &omt->root, value, index);
}

static void set_at_internal (OMT_NODE n, OMTVALUE v, u_int32_t index) {
    assert(n);
    if (index<nweight(n->left))
	set_at_internal(n->left, v, index);
    else if (index==nweight(n->left)) {
	n->value = v;
    } else {
	set_at_internal(n->right, v, index-nweight(n->left)-1);
    }
}

int toku_omt_set_at (OMT omt, OMTVALUE value, u_int32_t index) {
    if (index>=nweight(omt->root)) return ERANGE;
    set_at_internal(omt->root, value, index);
    return 0;
}


int toku_omt_insert(OMT omt, OMTVALUE value, int(*h)(OMTVALUE, void*v), void *v, u_int32_t *index) {
    int r;
    u_int32_t idx;

    r = toku_omt_find(omt, h, v, +1, NULL, &idx);
    if (r==DB_NOTFOUND) idx=toku_omt_size(omt);
    else if (r!=0) return r;

    if ((r = toku_omt_insert_at(omt, value, idx)))      return r;
    if (index) *index = idx;
    return 0;
}

static void delete_internal (OMT omt, OMT_NODE *np, u_int32_t index, OMTVALUE *vp) {
    OMT_NODE n=*np;
    if (index < nweight(n->left)) {
        delete_internal(omt, &n->left, index, vp);
        n->weight--;
    } else if (index == nweight(n->left)) {
        if (n->left==NULL) {
            *np = n->right;
            *vp = n->value;
            toku_free(n);
        } else if (n->right==NULL) {
            *np = n->left;
            *vp = n->value;
            toku_free(n);
        } else {
            OMTVALUE zv;
            // delete the successor of index, get the value, and store it here.
            delete_internal(omt, &n->right, 0, &zv);
            n->value = zv;
            n->weight--;
        }
    } else {
        delete_internal(omt, &n->right, index-nweight(n->left)-1, vp);
        n->weight--;
    }
    maybe_rebalance(omt, np);
}

int toku_omt_delete_at(OMT omt, u_int32_t index) {
    OMTVALUE v;
    int r;
    if (index>=nweight(omt->root)) return ERANGE;
    if ((r=make_sure_array_is_sized_ok(omt, -1+nweight(omt->root)))) return r;
    delete_internal(omt, &omt->root, index, &v);
    return 0;
}

static int fetch_internal (OMT_NODE n, u_int32_t i, OMTVALUE *v) {
    if (n==NULL) return ERANGE;
    if (i < nweight(n->left)) {
        return fetch_internal(n->left,  i, v);
    } else if (i == nweight(n->left)) {
        *v = n->value;
        return 0;
    } else {
        return fetch_internal(n->right, i-nweight(n->left)-1, v);
    }
}

int toku_omt_fetch (OMT V, u_int32_t i, OMTVALUE *v) {
    return fetch_internal(V->root, i, v);
}

static int find_internal_zero (OMT_NODE n, int (*h)(OMTVALUE, void*extra), void*extra, OMTVALUE *value, u_int32_t *index) {
    if (n==NULL) {
	if (index!=NULL) (*index)=0;
	return DB_NOTFOUND;
    }
    int hv = h(n->value, extra);
    if (hv<0) {
        int r = find_internal_zero(n->right, h, extra, value, index);
        if (index!=NULL) (*index) += nweight(n->left)+1;
        return r;
    } else if (hv>0) {
        return find_internal_zero(n->left, h, extra, value, index);
    } else {
        int r = find_internal_zero(n->left, h, extra, value, index);
        if (r==DB_NOTFOUND) {
            if (index!=NULL) *index = nweight(n->left);
            if (value!=NULL) *value = n->value;
            r = 0;
        }
        return r;
    }
}

int toku_omt_find_zero (OMT t, int (*h)(OMTVALUE, void*extra), void*extra, OMTVALUE *value, u_int32_t *index) {
    return find_internal_zero(t->root, h, extra, value, index);
}

//  If direction <0 then find the largest  i such that h(V_i,extra)<0.
static int find_internal_minus (OMT_NODE n, int (*h)(OMTVALUE, void*extra), void*extra, OMTVALUE *value, u_int32_t *index) {
    if (n==NULL) return DB_NOTFOUND;
    int hv = h(n->value, extra);
    if (hv<0) {
        int r = find_internal_minus(n->right, h, extra, value, index);
        if (r==0 && index!=NULL) (*index) += nweight(n->left)+1;
        else if (r==DB_NOTFOUND) {
            if (index!=NULL) *index = nweight(n->left);
            if (value!=NULL) *value = n->value;
            r = 0;
        }
        return r;
    } else {
        return find_internal_minus(n->left, h, extra, value, index);
    }
}

//  If direction >0 then find the smallest i such that h(V_i,extra)>0.
static int find_internal_plus (OMT_NODE n, int (*h)(OMTVALUE, void*extra), void*extra, OMTVALUE *value, u_int32_t *index) {
    if (n==NULL) return DB_NOTFOUND;
    int hv = h(n->value, extra);
    if (hv>0) {
        int r = find_internal_plus(n->left, h, extra, value, index);
        if (r==DB_NOTFOUND) {
            if (index!=NULL) *index = nweight(n->left);
            if (value!=NULL) *value = n->value;
            r = 0;
        }
        return r;
    } else {
        int r = find_internal_plus(n->right, h, extra, value, index);
        if (r==0 && index!=NULL) (*index) += nweight(n->left)+1;
        return r;
    }
}

int toku_omt_find(OMT V, int (*h)(OMTVALUE, void*extra), void*extra, int direction, OMTVALUE *value, u_int32_t *index) {
    if (direction==0) {
	abort();
    } else if (direction<0) {
        return find_internal_minus(V->root, h, extra, value, index);
    } else {
        return find_internal_plus(V->root, h, extra, value, index);
    }
}

static void free_omt_nodes (OMT_NODE n) {
    if (n==0) return;
    free_omt_nodes(n->left);
    free_omt_nodes(n->right);
    toku_free(n);
}

// Example:  numvalues=4,  halfway=2,  left side is values of size 2
//                                     right side is values+3 of size 1
//           numvalues=3,  halfway=1,  left side is values of size 1
//                                     right side is values+2 of size 1
//           numvalues=2,  halfway=1,  left side is values of size 1
//                                     right side is values+2 of size 0
//           numvalues=1,  halfway=0,  left side is values of size 0
//                                     right side is values of size 0.
static int create_from_sorted_array_internal(OMT_NODE *np, OMTVALUE *values, u_int32_t numvalues) {
    if (numvalues==0) {
        *np=NULL;
        return 0;
    } else {
        int r;
        u_int32_t halfway = numvalues/2;
        OMT_NODE MALLOC(newnode);
        if (newnode==NULL) return errno;
        newnode->weight = numvalues;
        newnode->value  = values[halfway]; 
        if ((r = create_from_sorted_array_internal(&newnode->left,  values,           halfway))) {
            toku_free(newnode);
            return r;
        }
        if ((r = create_from_sorted_array_internal(&newnode->right, values+halfway+1, numvalues-(halfway+1)))) {
            free_omt_nodes(newnode->left);
            toku_free(newnode);
            return r;
        }
        *np = newnode;
        return 0;
    }
}

int toku_omt_create_from_sorted_array(OMT *omtp, OMTVALUE *values, u_int32_t numvalues) {
    OMT omt;
    int r;
    if ((r = toku_omt_create(&omt))) return r;
    if ((r = create_from_sorted_array_internal(&omt->root, values, numvalues))) {
        toku_omt_destroy(&omt);
        return r;
    }
    if ((r=make_sure_array_is_sized_ok(omt, numvalues))) {
        toku_omt_destroy(&omt);
        return r;
    }
    *omtp=omt;
    return 0;
}

void toku_omt_destroy(OMT *omtp) {
    OMT omt=*omtp;
    free_omt_nodes(omt->root);
    toku_free(omt->tmparray);
    toku_free(omt);
    *omtp=NULL;
}

u_int32_t toku_omt_size(OMT V) {
    return nweight(V->root);
}

static int iterate_internal(OMT_NODE n, u_int32_t idx, int (*f)(OMTVALUE, u_int32_t, void*), void*v) {
    int r;
    if (n==NULL) return 0;
    if ((r=iterate_internal(n->left, idx, f, v))) return r;
    if ((r=f(n->value, idx+nweight(n->left), v))) return r;
    return iterate_internal(n->right, idx+nweight(n->left)+1, f, v);
}

int toku_omt_iterate(OMT omt, int (*f)(OMTVALUE, u_int32_t, void*), void*v) {
    return iterate_internal(omt->root, 0, f, v);
}

int toku_omt_split_at(OMT omt, OMT *newomtp, u_int32_t index) {
    if (index>=nweight(omt->root)) return ERANGE;
    int r;
    u_int32_t newsize = toku_omt_size(omt)-index;
    OMT newomt;
    if ((r = toku_omt_create(&newomt))) return r;
    if ((r = make_sure_array_is_sized_ok(newomt, newsize))) {
    fail:
        toku_omt_destroy(&newomt);
        return r;
    }
    OMT_NODE *MALLOC_N(toku_omt_size(omt), nodes);
    if (nodes==0) {
        r = errno;
        goto fail;
    }
    // Modify omt's array at the last possible moment, since after this nothing can fail.
    if ((r = make_sure_array_is_sized_ok(omt, index))) {
        toku_free(nodes);
        goto fail;
    }
    fill_array_from_omt_nodes_tree(nodes, omt->root);
    rebuild_from_sorted_array_of_omt_nodes(&newomt->root, nodes+index, newsize);
    rebuild_from_sorted_array_of_omt_nodes(&omt->root,    nodes,       index);
    toku_free(nodes);
    *newomtp = newomt;
    return 0;
}
    
int toku_omt_merge(OMT leftomt, OMT rightomt, OMT *newomtp) {
    int r;
    OMT newomt;
    u_int32_t newsize = toku_omt_size(leftomt)+toku_omt_size(rightomt);
    if ((r = toku_omt_create(&newomt))) return r;
    if ((r = make_sure_array_is_sized_ok(newomt, newsize))) {
        toku_omt_destroy(&newomt);
        return r;
    }
    fill_array_from_omt_nodes_tree(newomt->tmparray,                        leftomt->root);
    fill_array_from_omt_nodes_tree(newomt->tmparray+toku_omt_size(leftomt), rightomt->root);
    rebuild_from_sorted_array_of_omt_nodes(&newomt->root, newomt->tmparray, newsize);
    leftomt->root = rightomt->root = NULL;
    toku_omt_destroy(&leftomt);
    toku_omt_destroy(&rightomt);
    *newomtp = newomt;
    return 0;
}

