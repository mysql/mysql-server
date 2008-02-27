/* a pma cursor is a key and value pair that may be contained in a pma */
typedef struct pma_cursor {
    PMA pma;
    DBT key;
    DBT val;
    void *sskey;
    void *ssval;
} *PMA_CURSOR;

/* create a pma cursor */
static int toku_pma_cursor(PMA pma, PMA_CURSOR *cursorptr, void **sskey, void **ssval) {
    PMA_CURSOR cursor = toku_malloc(sizeof *cursor);
    if (cursor == 0) return ENOMEM;
    cursor->pma = pma;
    toku_init_dbt(&cursor->key);
    toku_init_dbt(&cursor->val);
    cursor->sskey = sskey;
    cursor->ssval = ssval;
    *cursorptr = cursor;
    return 0;
}

static inline void toku_destroy_dbt(DBT *dbt) {
    if (dbt->data && (dbt->flags & DB_DBT_MALLOC)) {
        toku_free(dbt->data);
        dbt->data = 0;
    }
}

/* free a pma cursor */
static int toku_pma_cursor_free(PMA_CURSOR *cursorptr) {
    PMA_CURSOR cursor = *cursorptr; *cursorptr = 0;
    toku_destroy_dbt(&cursor->key);
    toku_destroy_dbt(&cursor->val);
    toku_free_n(cursor, sizeof *cursor);
    return 0;
}

/* bind a new key and value to the pma cursor */
static void pma_cursor_set_key_val(PMA_CURSOR cursor, DBT *newkey, DBT *newval) {
    toku_destroy_dbt(&cursor->key);
    toku_destroy_dbt(&cursor->val);
    cursor->key = *newkey; toku_init_dbt(newkey);
    cursor->val = *newval; toku_init_dbt(newval);
}

static int pma_cursor_compare_one(brt_search_t *so, DBT *x, DBT *y) {
    so = so; x = x; y = y;
    return 1;
}

static int toku_pma_cursor_set_position_first (PMA_CURSOR cursor) {
    DBT newkey; toku_init_dbt(&newkey); newkey.flags = DB_DBT_MALLOC;
    DBT newval; toku_init_dbt(&newval); newval.flags = DB_DBT_MALLOC;
    brt_search_t so; brt_search_init(&so, pma_cursor_compare_one, BRT_SEARCH_LEFT, 0, 0, 0);
    int r = toku_pma_search(cursor->pma, &so, &newkey, &newval);
    if (r == 0) 
        pma_cursor_set_key_val(cursor, &newkey, &newval);
    toku_destroy_dbt(&newkey);
    toku_destroy_dbt(&newval);
    return r;
}

static int toku_pma_cursor_set_position_last (PMA_CURSOR cursor) {
    DBT newkey; toku_init_dbt(&newkey); newkey.flags = DB_DBT_MALLOC;
    DBT newval; toku_init_dbt(&newval); newval.flags = DB_DBT_MALLOC;
    brt_search_t so; brt_search_init(&so, pma_cursor_compare_one, BRT_SEARCH_RIGHT, 0, 0, 0);
    int r = toku_pma_search(cursor->pma, &so, &newkey, &newval);
    if (r == 0) 
        pma_cursor_set_key_val(cursor, &newkey, &newval);
    toku_destroy_dbt(&newkey);
    toku_destroy_dbt(&newval);
    return r;
}

static int pma_cursor_compare_kv_xy(PMA pma, DBT *k, DBT *v, DBT *x, DBT *y) {
    int cmp = pma->compare_fun(pma->db, k, x);
    if (cmp == 0 && v && y)
        cmp = pma->compare_fun(pma->db, v, y);
    return cmp;
}

static int pma_cursor_compare_next(brt_search_t *so, DBT *x, DBT *y) {
    PMA pma = so->context;
    return pma_cursor_compare_kv_xy(pma, so->k, so->v, x, y) < 0;
}

static int toku_pma_cursor_set_position_next (PMA_CURSOR cursor) {
    DBT newkey; toku_init_dbt(&newkey); newkey.flags = DB_DBT_MALLOC;
    DBT newval; toku_init_dbt(&newval); newval.flags = DB_DBT_MALLOC;
    brt_search_t so; brt_search_init(&so, pma_cursor_compare_next, BRT_SEARCH_LEFT, &cursor->key, &cursor->val, cursor->pma);
    int r = toku_pma_search(cursor->pma, &so, &newkey, &newval);
    if (r == 0) 
        pma_cursor_set_key_val(cursor, &newkey, &newval);
    toku_destroy_dbt(&newkey);
    toku_destroy_dbt(&newval);
    return r;
}

static int pma_cursor_compare_prev(brt_search_t *so, DBT *x, DBT *y) {
    PMA pma = so->context;
    return pma_cursor_compare_kv_xy(pma, so->k, so->v, x, y) > 0;
}

static int toku_pma_cursor_set_position_prev (PMA_CURSOR cursor) {
    DBT newkey; toku_init_dbt(&newkey); newkey.flags = DB_DBT_MALLOC;
    DBT newval; toku_init_dbt(&newval); newval.flags = DB_DBT_MALLOC;
    brt_search_t so; brt_search_init(&so, pma_cursor_compare_prev, BRT_SEARCH_RIGHT, &cursor->key, &cursor->val, cursor->pma);
    int r = toku_pma_search(cursor->pma, &so, &newkey, &newval);
    if (r == 0) 
        pma_cursor_set_key_val(cursor, &newkey, &newval);
    toku_destroy_dbt(&newkey);
    toku_destroy_dbt(&newval);
    return r;
}

static int pma_cursor_compare_both(brt_search_t *so, DBT *x, DBT *y) {
    PMA pma = so->context;
    return pma_cursor_compare_kv_xy(pma, so->k, so->v, x, y) <= 0;
}

static int toku_pma_cursor_set_both(PMA_CURSOR cursor, DBT *key, DBT *val) {
    DBT newkey; toku_init_dbt(&newkey); newkey.flags = DB_DBT_MALLOC;
    DBT newval; toku_init_dbt(&newval); newval.flags = DB_DBT_MALLOC;
    brt_search_t so; brt_search_init(&so, pma_cursor_compare_both, BRT_SEARCH_LEFT, key, val, cursor->pma);
    int r = toku_pma_search(cursor->pma, &so, &newkey, &newval);
    if (r != 0 || pma_cursor_compare_kv_xy(cursor->pma, key, val, &newkey, &newval) != 0) {
        r = DB_NOTFOUND;
    } else
        pma_cursor_set_key_val(cursor, &newkey, &newval);
    toku_destroy_dbt(&newkey);
    toku_destroy_dbt(&newval);
    return r;
}

static int toku_pma_cursor_get_current(PMA_CURSOR cursor, DBT *key, DBT *val, int even_deleted) {
    assert(even_deleted == 0);
    if (cursor->key.data == 0 || cursor->val.data == 0)
        return EINVAL;

    DBT newkey; toku_init_dbt(&newkey); newkey.flags = DB_DBT_MALLOC;
    DBT newval; toku_init_dbt(&newval); newval.flags = DB_DBT_MALLOC;
    brt_search_t so; brt_search_init(&so, pma_cursor_compare_both, BRT_SEARCH_LEFT, &cursor->key, &cursor->val, cursor->pma);
    int r = toku_pma_search(cursor->pma, &so, &newkey, &newval);
    if (r != 0 || pma_cursor_compare_kv_xy(cursor->pma, &cursor->key, &cursor->val, &newkey, &newval) != 0) {
        r = DB_KEYEMPTY;
    }
    toku_destroy_dbt(&newkey);
    toku_destroy_dbt(&newval);
    if (r != 0)
        return r;

    if (key) 
        r = toku_dbt_set_value(key, cursor->key.data, cursor->key.size, cursor->sskey);
    if (val && r == 0) 
        r = toku_dbt_set_value(val, cursor->val.data, cursor->val.size, cursor->ssval);
    return r;
}

static int toku_pma_cursor_set_range_both(PMA_CURSOR cursor, DBT *key, DBT *val) {
    DBT newkey; toku_init_dbt(&newkey); newkey.flags = DB_DBT_MALLOC;
    DBT newval; toku_init_dbt(&newval); newval.flags = DB_DBT_MALLOC;
    brt_search_t so; brt_search_init(&so, pma_cursor_compare_both, BRT_SEARCH_LEFT, key, val, cursor->pma);
    int r = toku_pma_search(cursor->pma, &so, &newkey, &newval);
    if (r == 0)
        pma_cursor_set_key_val(cursor, &newkey, &newval);
    toku_destroy_dbt(&newkey);
    toku_destroy_dbt(&newval);
    return r;
}

static int toku_pma_cursor_delete_under(PMA_CURSOR cursor, u_int32_t *kvsize,
					TOKULOGGER logger, TXNID xid, DISKOFF diskoff,
					u_int32_t rand4sem, u_int32_t *fingerprint, LSN*node_lsn) {
    cursor = cursor; kvsize = kvsize; rand4sem = rand4sem; fingerprint = fingerprint;
    DBT key; toku_init_dbt(&key); key.flags = DB_DBT_MALLOC;
    DBT val; toku_init_dbt(&val); val.flags = DB_DBT_MALLOC;
    int r = toku_pma_cursor_get_current(cursor, &key, &val, 0);
    if (r == 0) {
        PMA pma = cursor->pma;
        r = toku_pma_delete(pma, &key, pma->dup_mode & TOKU_DB_DUPSORT ? &val : 0,
			    logger, xid, diskoff,
			    rand4sem, fingerprint, kvsize, node_lsn);
        assert(r == 0);
    }
    toku_destroy_dbt(&key);
    toku_destroy_dbt(&val);
    return r;
}
