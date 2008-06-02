/**
  \brief OMT implementation header
*/

#if !defined(OMTI_H)
#define OMTI_H

#ident "Copyright (c) 2008 Tokutek Inc.  All rights reserved."

#include <stdint.h>

/** Type for the node index */
typedef u_int32_t node_idx;


/** Define a NULL index in the node array */
#define NODE_NULL UINT32_MAX

/** OMT node */
typedef struct omt_node *OMT_NODE;
struct omt_node {
    u_int32_t weight; /* Size of subtree rooted at this node 
                         (including this one). */
    node_idx  left;   /* Index of left  subtree. */
    node_idx  right;  /* Index of right subtree. */
    OMTVALUE  value;  /* The value stored in the node. */
};

/** Order Maintenance Tree */
struct omt {
    node_idx   root;

    u_int32_t  node_capacity;
    OMT_NODE   nodes;
    node_idx   free_idx;

    u_int32_t  tmparray_size;
    node_idx*  tmparray;
};

//Initial max size of root-to-leaf path
#define TOKU_OMTCURSOR_INITIAL_SIZE 4

// Cursor for order maintenance tree
struct omtcursor {
    u_int32_t max_pathlen; //Max (root to leaf) path length;
    u_int32_t pathlen;     //Length of current path
    node_idx *path;
    OMT       omt;         //Associated OMT
};


#endif  /* #ifndef OMTI_H */

