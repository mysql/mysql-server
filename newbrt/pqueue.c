/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id: pqueue.c$"
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_portability.h>
#include "toku_os.h"
#include "brt-internal.h"
#include "brtloader-internal.h"
#include "pqueue.h"

#define pqueue_left(i)   ((i) << 1)
#define pqueue_right(i)  (((i) << 1) + 1)
#define pqueue_parent(i) ((i) >> 1)

int pqueue_init(pqueue_t **result, size_t n, int which_db, DB *db, brt_compare_func compare, struct error_callback_s *err_callback)
{
    pqueue_t *q;
    if (!(q = toku_malloc(sizeof(pqueue_t))))
        return errno;

    /* Need to allocate n+1 elements since element 0 isn't used. */
    if (!(q->d = toku_malloc((n + 1) * sizeof(pqueue_node_t *)))) {
	int r = errno;
        toku_free(q);
        return r;
    }
    q->size = 1;
    q->avail = q->step = (n+1);  /* see comment above about n+1 */

    q->which_db = which_db;
    q->db = db;
    q->compare = compare;
    q->dup_error = 0;

    q->error_callback = err_callback;

    *result = q;
    return 0;
}

void pqueue_free(pqueue_t *q)
{
    toku_free(q->d);
    toku_free(q);
}


size_t pqueue_size(pqueue_t *q)
{
    /* queue element 0 exists but doesn't count since it isn't used. */
    return (q->size - 1);
}

static int pqueue_compare(pqueue_t *q, DBT *next_key, DBT *next_val, DBT *curr_key)
{
    int r = q->compare(q->db, next_key, curr_key);
    if ( r == 0 ) { // duplicate key : next_key == curr_key
        q->dup_error = 1; 
        if (q->error_callback)
            brt_loader_set_error_and_callback(q->error_callback, DB_KEYEXIST, q->db, q->which_db, next_key, next_val);
    }
    return ( r > -1 );
}

static void pqueue_bubble_up(pqueue_t *q, size_t i)
{
    size_t parent_node;
    pqueue_node_t *moving_node = q->d[i];
    DBT *moving_key = moving_node->key;

    for (parent_node = pqueue_parent(i);
         ((i > 1) && pqueue_compare(q, q->d[parent_node]->key, q->d[parent_node]->val, moving_key));
         i = parent_node, parent_node = pqueue_parent(i))
    {
        q->d[i] = q->d[parent_node];
    }

    q->d[i] = moving_node;
}


static size_t pqueue_maxchild(pqueue_t *q, size_t i)
{
    size_t child_node = pqueue_left(i);

    if (child_node >= q->size)
        return 0;

    if ((child_node+1) < q->size &&
        pqueue_compare(q, q->d[child_node]->key, q->d[child_node]->val, q->d[child_node+1]->key))
        child_node++; /* use right child instead of left */

    return child_node;
}


static void pqueue_percolate_down(pqueue_t *q, size_t i)
{
    size_t child_node;
    pqueue_node_t *moving_node = q->d[i];
    DBT *moving_key = moving_node->key;
    DBT *moving_val = moving_node->val;

    while ((child_node = pqueue_maxchild(q, i)) &&
           pqueue_compare(q, moving_key, moving_val, q->d[child_node]->key))
    {
        q->d[i] = q->d[child_node];
        i = child_node;
    }

    q->d[i] = moving_node;
}


int pqueue_insert(pqueue_t *q, pqueue_node_t *d)
{
    size_t i;

    if (!q) return 1;
    if (q->size >= q->avail) return 1;

    /* insert item */
    i = q->size++;
    q->d[i] = d;
    pqueue_bubble_up(q, i);

    if ( q->dup_error ) return DB_KEYEXIST;
    return 0;
}

int pqueue_pop(pqueue_t *q, pqueue_node_t **d)
{
    if (!q || q->size == 1) {
        *d = NULL;
        return 0;
    }

    *d = q->d[1];
    q->d[1] = q->d[--q->size];
    pqueue_percolate_down(q, 1);

    if ( q->dup_error ) return DB_KEYEXIST;
    return 0;
}
