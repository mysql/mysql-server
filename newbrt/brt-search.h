#ident "$Id: brt.c 11200 2009-04-10 22:28:41Z yfogel $"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#ifndef BRT_SEARCH_H
#define BRT_SEARCH_H

enum brt_search_direction_e {
    BRT_SEARCH_LEFT = 1,  /* search left -> right, finds min xy as defined by the compare function */
    BRT_SEARCH_RIGHT = 2, /* search right -> left, finds max xy as defined by the compare function */
};

struct brt_search;

/* the search compare function should return 0 for all xy < kv and 1 for all xy >= kv
   the compare function should be a step function from 0 to 1 for a left to right search
   and 1 to 0 for a right to left search */

typedef int (*brt_search_compare_func_t)(struct brt_search */*so*/, DBT */*x*/, DBT */*y*/);

/* the search object contains the compare function, search direction, and the kv pair that
   is used in the compare function.  the context is the user's private data */

typedef struct brt_search {
    brt_search_compare_func_t compare;
    enum brt_search_direction_e direction;
    DBT *k;
    DBT *v;
    void *context;
} brt_search_t;

/* initialize the search compare object */
static inline brt_search_t *brt_search_init(brt_search_t *so, brt_search_compare_func_t compare, enum brt_search_direction_e direction, DBT *k, DBT *v, void *context) {
    so->compare = compare; so->direction = direction; so->k = k; so->v = v; so->context = context;
    return so;
}

#endif
