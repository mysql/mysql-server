#ifndef TOKU_PQUEUE_H
#define TOKU_PQUEUE_H

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
