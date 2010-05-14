#ifndef TOKU_PQUEUE_H
#define TOKU_PQUEUE_H

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

typedef struct brt_pqueue_node_t
{
    DBT   *key;
    DBT   *val;
    int      i;
} pqueue_node_t;

typedef struct brt_pqueue_t
{
    size_t size;
    size_t avail;
    size_t step;

    int which_db;
    DB *db;  // needed for compare function
    brt_compare_func compare;
    pqueue_node_t **d;
    int dup_error;

    struct error_callback_s *error_callback;

} pqueue_t;

int pqueue_init(pqueue_t **result, size_t n, int which_db, DB *db, brt_compare_func compare, struct error_callback_s *err_callback);
void pqueue_free(pqueue_t *q);
size_t pqueue_size(pqueue_t *q);
int pqueue_insert(pqueue_t *q, pqueue_node_t *d);
int pqueue_pop(pqueue_t *q, pqueue_node_t **d);

#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif

#endif //TOKU_PQUEUE_H
