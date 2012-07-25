/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#ident "$Id$"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <db.h>
#include "toku_assert.h"
#include "ydb-internal.h"
#include "ydb_cursor.h"
#include "ydb_row_lock.h"

static YDB_C_LAYER_STATUS_S ydb_c_layer_status;
#ifdef STATUS_VALUE
#undef STATUS_VALUE
#endif
#define STATUS_VALUE(x) ydb_c_layer_status.status[x].value.num

#define STATUS_INIT(k,t,l) { \
        ydb_c_layer_status.status[k].keyname = #k; \
        ydb_c_layer_status.status[k].type    = t;  \
        ydb_c_layer_status.status[k].legend  = l; \
    }

static void
ydb_c_layer_status_init (void) {
    // Note, this function initializes the keyname, type, and legend fields.
    // Value fields are initialized to zero by compiler.
    ydb_c_layer_status.initialized = true;
}
#undef STATUS_INIT

void
ydb_c_layer_get_status(YDB_C_LAYER_STATUS statp) {
    if (!ydb_c_layer_status.initialized)
        ydb_c_layer_status_init();
    *statp = ydb_c_layer_status;
}


/* lightweight cursor methods. */
static int toku_c_getf_current_binding(DBC *c, uint32_t flag, YDB_CALLBACK_FUNCTION f, void *extra);

//Get the main portion of a cursor flag (excluding the bitwise or'd components).
static int 
get_main_cursor_flag(uint32_t flags) {
    return flags & DB_OPFLAGS_MASK;
}

static int 
get_nonmain_cursor_flags(uint32_t flags) {
    return flags & ~(DB_OPFLAGS_MASK);
}

static inline bool 
toku_c_uninitialized(DBC* c) {
    return toku_ft_cursor_uninitialized(dbc_struct_i(c)->c);
}            

typedef struct query_context_wrapped_t {
    DBT               *key;
    DBT               *val;
    struct simple_dbt *skey;
    struct simple_dbt *sval;
} *QUERY_CONTEXT_WRAPPED, QUERY_CONTEXT_WRAPPED_S;

static inline void
query_context_wrapped_init(QUERY_CONTEXT_WRAPPED context, DBC *c, DBT *key, DBT *val) {
    context->key  = key;
    context->val  = val;
    context->skey = dbc_struct_i(c)->skey;
    context->sval = dbc_struct_i(c)->sval;
}

static int
c_get_wrapper_callback(DBT const *key, DBT const *val, void *extra) {
    QUERY_CONTEXT_WRAPPED context = (QUERY_CONTEXT_WRAPPED) extra;
    int r;
              r = toku_dbt_set(key->size, key->data, context->key, context->skey);
    if (r==0) r = toku_dbt_set(val->size, val->data, context->val, context->sval);
    return r;
}

static int 
toku_c_get_current_unconditional(DBC* c, uint32_t flags, DBT* key, DBT* val) {
    int r;
    QUERY_CONTEXT_WRAPPED_S context; 
    query_context_wrapped_init(&context, c, key, val);
    r = toku_c_getf_current_binding(c, flags, c_get_wrapper_callback, &context);
    return r;
}

static inline uint32_t 
get_cursor_prelocked_flags(uint32_t flags, DBC* dbc) {
    uint32_t lock_flags = flags & (DB_PRELOCKED | DB_PRELOCKED_WRITE);

    //DB_READ_UNCOMMITTED and DB_READ_COMMITTED transactions 'own' all read locks for user-data dictionaries.
    if (dbc_struct_i(dbc)->iso != TOKU_ISO_SERIALIZABLE) {
        lock_flags |= DB_PRELOCKED;
    }
    return lock_flags;
}

//This is the user level callback function given to ydb layer functions like
//toku_c_getf_first

typedef struct query_context_base_t {
    FT_CURSOR  c;
    DB_TXN     *txn;
    DB         *db;
    YDB_CALLBACK_FUNCTION f;
    void       *f_extra;
    int         r_user_callback;
    bool        do_locking;
    bool        is_write_op;
    toku_lock_request lock_request;
} *QUERY_CONTEXT_BASE, QUERY_CONTEXT_BASE_S;

typedef struct query_context_t {
    QUERY_CONTEXT_BASE_S  base;
} *QUERY_CONTEXT, QUERY_CONTEXT_S;

typedef struct query_context_with_input_t {
    QUERY_CONTEXT_BASE_S  base;
    DBT                  *input_key;
    DBT                  *input_val;
} *QUERY_CONTEXT_WITH_INPUT, QUERY_CONTEXT_WITH_INPUT_S;

static void
query_context_base_init(QUERY_CONTEXT_BASE context, DBC *c, uint32_t flag, bool is_write_op, YDB_CALLBACK_FUNCTION f, void *extra) {
    context->c       = dbc_struct_i(c)->c;
    context->txn     = dbc_struct_i(c)->txn;
    context->db      = c->dbp;
    context->f       = f;
    context->f_extra = extra;
    context->is_write_op = is_write_op;
    uint32_t lock_flags = get_cursor_prelocked_flags(flag, c);
    if (context->is_write_op) 
        lock_flags &= DB_PRELOCKED_WRITE; // Only care about whether already locked for write
    context->do_locking = (bool)(context->db->i->lt!=NULL && !(lock_flags & (DB_PRELOCKED|DB_PRELOCKED_WRITE)));
    context->r_user_callback = 0;
    toku_lock_request_default_init(&context->lock_request);
}

static void
query_context_base_destroy(QUERY_CONTEXT_BASE context) {
    toku_lock_request_destroy(&context->lock_request);
}

static void
query_context_init_read(QUERY_CONTEXT context, DBC *c, uint32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    bool is_write = false;
    query_context_base_init(&context->base, c, flag, is_write, f, extra);
}

static void
query_context_init_write(QUERY_CONTEXT context, DBC *c, uint32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    bool is_write = true;
    query_context_base_init(&context->base, c, flag, is_write, f, extra);
}

static void
query_context_with_input_init(QUERY_CONTEXT_WITH_INPUT context, DBC *c, uint32_t flag, DBT *key, DBT *val, YDB_CALLBACK_FUNCTION f, void *extra) {
    // grab write locks if the DB_RMW flag is set or the cursor was created with the DB_RMW flag
    bool is_write = ((flag & DB_RMW) != 0) || dbc_struct_i(c)->rmw;
    query_context_base_init(&context->base, c, flag, is_write, f, extra);
    context->input_key = key;
    context->input_val = val;
}

static int c_getf_first_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra, bool);

static void 
c_query_context_init(QUERY_CONTEXT context, DBC *c, uint32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    bool is_write_op = false;
    // grab write locks if the DB_RMW flag is set or the cursor was created with the DB_RMW flag
    if ((flag & DB_RMW) || dbc_struct_i(c)->rmw)
        is_write_op = true;
    if (is_write_op)
        query_context_init_write(context, c, flag, f, extra);
    else
        query_context_init_read(context, c, flag, f, extra);
}

static void 
c_query_context_destroy(QUERY_CONTEXT context) {
    query_context_base_destroy(&context->base);
}

static int
toku_c_getf_first(DBC *c, uint32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);
    int r = 0;
    QUERY_CONTEXT_S context; //Describes the context of this query.
    c_query_context_init(&context, c, flag, f, extra);
    while (r == 0) {
        //toku_ft_cursor_first will call c_getf_first_callback(..., context) (if query is successful)
        r = toku_ft_cursor_first(dbc_struct_i(c)->c, c_getf_first_callback, &context);
        if (r == DB_LOCK_NOTGRANTED)
            r = toku_lock_request_wait_with_default_timeout(&context.base.lock_request, c->dbp->i->lt);
        else {
            if (r == TOKUDB_USER_CALLBACK_ERROR)
                r = context.base.r_user_callback;
            break;
        }
    }
    c_query_context_destroy(&context);
    return r;
}

//result is the result of the query (i.e. 0 means found, DB_NOTFOUND, etc..)
static int
c_getf_first_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra, bool lock_only) {
    QUERY_CONTEXT      super_context = (QUERY_CONTEXT) extra;
    QUERY_CONTEXT_BASE context       = &super_context->base;

    int r;
    DBT found_key = { .data = (void *) key, .size = keylen };

    if (context->do_locking) {
        const DBT *left_key = toku_lt_neg_infinity;
        const DBT *right_key = key != NULL ? &found_key : toku_lt_infinity;
        r = start_range_lock(context->db, context->txn, left_key, right_key, 
                             context->is_write_op ? LOCK_REQUEST_WRITE : LOCK_REQUEST_READ, &context->lock_request);
    } else 
        r = 0;

    //Call application-layer callback if found and locks were successfully obtained.
    if (r==0 && key!=NULL && !lock_only) {
        DBT found_val = { .data = (void *) val, .size = vallen };
        context->r_user_callback = context->f(&found_key, &found_val, context->f_extra);
        r = context->r_user_callback;
    }

    //Give brt-layer an error (if any) to return from toku_ft_cursor_first
    return r;
}

static int c_getf_last_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra, bool);

static int
toku_c_getf_last(DBC *c, uint32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);
    int r = 0;
    QUERY_CONTEXT_S context; //Describes the context of this query.
    c_query_context_init(&context, c, flag, f, extra); 
    while (r == 0) {
        //toku_ft_cursor_last will call c_getf_last_callback(..., context) (if query is successful)
        r = toku_ft_cursor_last(dbc_struct_i(c)->c, c_getf_last_callback, &context);
        if (r == DB_LOCK_NOTGRANTED)
            r = toku_lock_request_wait_with_default_timeout(&context.base.lock_request, c->dbp->i->lt);
        else {
            if (r == TOKUDB_USER_CALLBACK_ERROR)
                r = context.base.r_user_callback;
            break;
        }
    }
    c_query_context_destroy(&context);
    return r;
}

//result is the result of the query (i.e. 0 means found, DB_NOTFOUND, etc..)
static int
c_getf_last_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra, bool lock_only) {
    QUERY_CONTEXT      super_context = (QUERY_CONTEXT) extra;
    QUERY_CONTEXT_BASE context       = &super_context->base;

    int r;
    DBT found_key = { .data = (void *) key, .size = keylen };

    if (context->do_locking) {
        const DBT *left_key = key != NULL ? &found_key : toku_lt_neg_infinity;
        const DBT *right_key = toku_lt_infinity;
        r = start_range_lock(context->db, context->txn, left_key, right_key, 
                             context->is_write_op ? LOCK_REQUEST_WRITE : LOCK_REQUEST_READ, &context->lock_request);
    } else 
        r = 0;

    //Call application-layer callback if found and locks were successfully obtained.
    if (r==0 && key!=NULL && !lock_only) {
        DBT found_val = { .data = (void *) val, .size = vallen };
        context->r_user_callback = context->f(&found_key, &found_val, context->f_extra);
        r = context->r_user_callback;
    }

    //Give brt-layer an error (if any) to return from toku_ft_cursor_last
    return r;
}

static int c_getf_next_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra, bool);

static int
toku_c_getf_next(DBC *c, uint32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    int r;
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);
    if (toku_c_uninitialized(c)) 
        r = toku_c_getf_first(c, flag, f, extra);
    else {
        r = 0;
        QUERY_CONTEXT_S context; //Describes the context of this query.
        c_query_context_init(&context, c, flag, f, extra); 
        while (r == 0) {
            //toku_ft_cursor_next will call c_getf_next_callback(..., context) (if query is successful)
            r = toku_ft_cursor_next(dbc_struct_i(c)->c, c_getf_next_callback, &context);
            if (r == DB_LOCK_NOTGRANTED)
                r = toku_lock_request_wait_with_default_timeout(&context.base.lock_request, c->dbp->i->lt);
            else {
                if (r == TOKUDB_USER_CALLBACK_ERROR)
                    r = context.base.r_user_callback;
                break;
            }
        }
        c_query_context_destroy(&context);
    }
    return r;
}

//result is the result of the query (i.e. 0 means found, DB_NOTFOUND, etc..)
static int
c_getf_next_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra, bool lock_only) {
    QUERY_CONTEXT      super_context = (QUERY_CONTEXT) extra;
    QUERY_CONTEXT_BASE context       = &super_context->base;

    int r;

    DBT found_key = { .data = (void *) key, .size = keylen };

    if (context->do_locking) {
        const DBT *prevkey, *prevval;
        toku_ft_cursor_peek(context->c, &prevkey, &prevval);
        const DBT *left_key = prevkey;
        const DBT *right_key = key != NULL ? &found_key : toku_lt_infinity;
        r = start_range_lock(context->db, context->txn, left_key, right_key, 
                             context->is_write_op ? LOCK_REQUEST_WRITE : LOCK_REQUEST_READ, &context->lock_request);
    } else 
        r = 0;

    //Call application-layer callback if found and locks were successfully obtained.
    if (r==0 && key!=NULL && !lock_only) {
        DBT found_val = { .data = (void *) val, .size = vallen };
        context->r_user_callback = context->f(&found_key, &found_val, context->f_extra);
        r = context->r_user_callback;
    }

    //Give brt-layer an error (if any) to return from toku_ft_cursor_next
    return r;
}

static int c_getf_prev_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra, bool);

static int
toku_c_getf_prev(DBC *c, uint32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    int r;
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);
    if (toku_c_uninitialized(c)) 
        r = toku_c_getf_last(c, flag, f, extra);
    else {
        r = 0;
        QUERY_CONTEXT_S context; //Describes the context of this query.
        c_query_context_init(&context, c, flag, f, extra);
        while (r == 0) {
            //toku_ft_cursor_prev will call c_getf_prev_callback(..., context) (if query is successful)
            r = toku_ft_cursor_prev(dbc_struct_i(c)->c, c_getf_prev_callback, &context);
            if (r == DB_LOCK_NOTGRANTED)
                r = toku_lock_request_wait_with_default_timeout(&context.base.lock_request, c->dbp->i->lt);
            else {
                if (r == TOKUDB_USER_CALLBACK_ERROR)
                    r = context.base.r_user_callback;
                break;
            }
        }
        c_query_context_destroy(&context);
    }
    return r;
}

//result is the result of the query (i.e. 0 means found, DB_NOTFOUND, etc..)
static int
c_getf_prev_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra, bool lock_only) {
    QUERY_CONTEXT      super_context = (QUERY_CONTEXT) extra;
    QUERY_CONTEXT_BASE context       = &super_context->base;

    int r;
    DBT found_key = { .data = (void *) key, .size = keylen };

    if (context->do_locking) {
        const DBT *prevkey, *prevval;
        toku_ft_cursor_peek(context->c, &prevkey, &prevval);
        const DBT *left_key = key != NULL ? &found_key : toku_lt_neg_infinity;
        const DBT *right_key = prevkey;
        r = start_range_lock(context->db, context->txn, left_key, right_key, 
                             context->is_write_op ? LOCK_REQUEST_WRITE : LOCK_REQUEST_READ, &context->lock_request);
    } else 
        r = 0;

    //Call application-layer callback if found and locks were successfully obtained.
    if (r==0 && key!=NULL && !lock_only) {
        DBT found_val = { .data = (void *) val, .size = vallen };
        context->r_user_callback = context->f(&found_key, &found_val, context->f_extra);
        r = context->r_user_callback;
    }

    //Give brt-layer an error (if any) to return from toku_ft_cursor_prev
    return r;
}

static int c_getf_current_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra, bool);

static int
toku_c_getf_current(DBC *c, uint32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);

    QUERY_CONTEXT_S context; //Describes the context of this query.
    c_query_context_init(&context, c, flag, f, extra); 
    //toku_ft_cursor_current will call c_getf_current_callback(..., context) (if query is successful)
    int r = toku_ft_cursor_current(dbc_struct_i(c)->c, DB_CURRENT, c_getf_current_callback, &context);
    if (r == TOKUDB_USER_CALLBACK_ERROR) r = context.base.r_user_callback;
    c_query_context_destroy(&context);
    return r;
}

//result is the result of the query (i.e. 0 means found, DB_NOTFOUND, etc..)
static int
c_getf_current_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra, bool lock_only) {
    QUERY_CONTEXT      super_context = (QUERY_CONTEXT) extra;
    QUERY_CONTEXT_BASE context       = &super_context->base;

    int r;

    //Call application-layer callback if found.
    if (key!=NULL && !lock_only) {
        DBT found_key = { .data = (void *) key, .size = keylen };
        DBT found_val = { .data = (void *) val, .size = vallen };
        context->r_user_callback = context->f(&found_key, &found_val, context->f_extra);
        r = context->r_user_callback;
    } else
        r = 0;

    //Give brt-layer an error (if any) to return from toku_ft_cursor_current
    return r;
}

static int
toku_c_getf_current_binding(DBC *c, uint32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);

    QUERY_CONTEXT_S context; //Describes the context of this query.
    c_query_context_init(&context, c, flag, f, extra); 
    //toku_ft_cursor_current will call c_getf_current_callback(..., context) (if query is successful)
    int r = toku_ft_cursor_current(dbc_struct_i(c)->c, DB_CURRENT_BINDING, c_getf_current_callback, &context);
    if (r == TOKUDB_USER_CALLBACK_ERROR) r = context.base.r_user_callback;
    c_query_context_destroy(&context);
    return r;
}

static int c_getf_set_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra, bool);

int
toku_c_getf_set(DBC *c, uint32_t flag, DBT *key, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);

    int r = 0;
    QUERY_CONTEXT_WITH_INPUT_S context; //Describes the context of this query.
    query_context_with_input_init(&context, c, flag, key, NULL, f, extra); 
    while (r == 0) {
        //toku_ft_cursor_set will call c_getf_set_callback(..., context) (if query is successful)
        r = toku_ft_cursor_set(dbc_struct_i(c)->c, key, c_getf_set_callback, &context);
        if (r == DB_LOCK_NOTGRANTED)
            r = toku_lock_request_wait_with_default_timeout(&context.base.lock_request, c->dbp->i->lt);
        else {
            if (r == TOKUDB_USER_CALLBACK_ERROR)
                r = context.base.r_user_callback;
            break;
        }
    }
    query_context_base_destroy(&context.base);
    return r;
}

//result is the result of the query (i.e. 0 means found, DB_NOTFOUND, etc..)
static int
c_getf_set_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra, bool lock_only) {
    QUERY_CONTEXT_WITH_INPUT super_context = (QUERY_CONTEXT_WITH_INPUT) extra;
    QUERY_CONTEXT_BASE       context       = &super_context->base;

    int r;

    //Lock:
    //  left(key,val)  = (input_key, -infinity)
    //  right(key,val) = (input_key, found ? found_val : infinity)
    if (context->do_locking) {
        r = start_range_lock(context->db, context->txn, super_context->input_key, super_context->input_key, 
                             context->is_write_op ? LOCK_REQUEST_WRITE : LOCK_REQUEST_READ, &context->lock_request);
    } else 
        r = 0;

    //Call application-layer callback if found and locks were successfully obtained.
    if (r==0 && key!=NULL && !lock_only) {
        DBT found_key = { .data = (void *) key, .size = keylen };
        DBT found_val = { .data = (void *) val, .size = vallen };
        context->r_user_callback = context->f(&found_key, &found_val, context->f_extra);
        r = context->r_user_callback;
    }

    //Give brt-layer an error (if any) to return from toku_ft_cursor_set
    return r;
}

static int c_getf_set_range_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra, bool);

static int
toku_c_getf_set_range(DBC *c, uint32_t flag, DBT *key, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);

    int r = 0;
    QUERY_CONTEXT_WITH_INPUT_S context; //Describes the context of this query.
    query_context_with_input_init(&context, c, flag, key, NULL, f, extra); 
    while (r == 0) {
        //toku_ft_cursor_set_range will call c_getf_set_range_callback(..., context) (if query is successful)
        r = toku_ft_cursor_set_range(dbc_struct_i(c)->c, key, c_getf_set_range_callback, &context);
        if (r == DB_LOCK_NOTGRANTED)
            r = toku_lock_request_wait_with_default_timeout(&context.base.lock_request, c->dbp->i->lt);
        else {
            if (r == TOKUDB_USER_CALLBACK_ERROR)
                r = context.base.r_user_callback;
            break;
        }
    }
    query_context_base_destroy(&context.base);
    return r;
}

//result is the result of the query (i.e. 0 means found, DB_NOTFOUND, etc..)
static int
c_getf_set_range_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra, bool lock_only) {
    QUERY_CONTEXT_WITH_INPUT super_context = (QUERY_CONTEXT_WITH_INPUT) extra;
    QUERY_CONTEXT_BASE       context       = &super_context->base;

    int r;
    DBT found_key = { .data = (void *) key, .size = keylen };

    //Lock:
    //  left(key,val)  = (input_key, -infinity)
    //  right(key) = found ? found_key : infinity
    //  right(val) = found ? found_val : infinity
    if (context->do_locking) {
        const DBT *left_key = super_context->input_key;
        const DBT *right_key = key != NULL ? &found_key : toku_lt_infinity;
        r = start_range_lock(context->db, context->txn, left_key, right_key, 
                             context->is_write_op ? LOCK_REQUEST_WRITE : LOCK_REQUEST_READ, &context->lock_request);
    } else 
        r = 0;

    //Call application-layer callback if found and locks were successfully obtained.
    if (r==0 && key!=NULL && !lock_only) {
        DBT found_val = { .data = (void *) val, .size = vallen };
        context->r_user_callback = context->f(&found_key, &found_val, context->f_extra);
        r = context->r_user_callback;
    }

    //Give brt-layer an error (if any) to return from toku_ft_cursor_set_range
    return r;
}

static int c_getf_set_range_reverse_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra, bool);

static int
toku_c_getf_set_range_reverse(DBC *c, uint32_t flag, DBT *key, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);

    int r = 0;
    QUERY_CONTEXT_WITH_INPUT_S context; //Describes the context of this query.
    query_context_with_input_init(&context, c, flag, key, NULL, f, extra); 
    while (r == 0) {
        //toku_ft_cursor_set_range_reverse will call c_getf_set_range_reverse_callback(..., context) (if query is successful)
        r = toku_ft_cursor_set_range_reverse(dbc_struct_i(c)->c, key, c_getf_set_range_reverse_callback, &context);
        if (r == DB_LOCK_NOTGRANTED)
            r = toku_lock_request_wait_with_default_timeout(&context.base.lock_request, c->dbp->i->lt);
        else {
            if (r == TOKUDB_USER_CALLBACK_ERROR)
                r = context.base.r_user_callback;
            break;
        }
    }
    query_context_base_destroy(&context.base);
    return r;
}

//result is the result of the query (i.e. 0 means found, DB_NOTFOUND, etc..)
static int
c_getf_set_range_reverse_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra, bool lock_only) {
    QUERY_CONTEXT_WITH_INPUT super_context = (QUERY_CONTEXT_WITH_INPUT) extra;
    QUERY_CONTEXT_BASE       context       = &super_context->base;

    int r;
    DBT found_key = { .data = (void *) key, .size = keylen };

    //Lock:
    //  left(key) = found ? found_key : -infinity
    //  left(val) = found ? found_val : -infinity
    //  right(key,val)  = (input_key, infinity)
    if (context->do_locking) {
        const DBT *left_key = key != NULL ? &found_key : toku_lt_neg_infinity;
        const DBT *right_key = super_context->input_key;
        r = start_range_lock(context->db, context->txn, left_key, right_key, 
                             context->is_write_op ? LOCK_REQUEST_WRITE : LOCK_REQUEST_READ, &context->lock_request);
    } else 
        r = 0;

    //Call application-layer callback if found and locks were successfully obtained.
    if (r==0 && key!=NULL && !lock_only) {
        DBT found_val = { .data = (void *) val, .size = vallen };
        context->r_user_callback = context->f(&found_key, &found_val, context->f_extra);
        r = context->r_user_callback;
    }

    //Give brt-layer an error (if any) to return from toku_ft_cursor_set_range_reverse
    return r;
}

// Close a cursor.
// Does not require the ydb lock held when called.
int 
toku_c_close(DBC * c) {
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);
    int r = toku_ft_cursor_close(dbc_struct_i(c)->c);
    toku_sdbt_cleanup(&dbc_struct_i(c)->skey_s);
    toku_sdbt_cleanup(&dbc_struct_i(c)->sval_s);
#if !TOKUDB_NATIVE_H
    toku_free(dbc_struct_i(c));
#endif
    toku_free(c);
    return r;
}

// these next two static functions are defined
// both here and ydb.c. We should find a good
// place for them.
static int
ydb_getf_do_nothing(DBT const* UU(key), DBT const* UU(val), void* UU(extra)) {
    return 0;
}

static inline DBT*
init_dbt_realloc(DBT *dbt) {
    memset(dbt, 0, sizeof(*dbt));
    dbt->flags = DB_DBT_REALLOC;
    return dbt;
}

// Return the number of entries whose key matches the key currently 
// pointed to by the brt cursor.  
static int 
toku_c_count(DBC *cursor, db_recno_t *count, uint32_t flags) {
    HANDLE_PANICKED_DB(cursor->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(cursor);
    int r;
    DBC *count_cursor = 0;
    DBT currentkey;

    init_dbt_realloc(&currentkey);
    uint32_t lock_flags = get_cursor_prelocked_flags(flags, cursor);
    flags &= ~lock_flags;
    if (flags != 0) {
        r = EINVAL; goto finish;
    }

    r = toku_c_get_current_unconditional(cursor, lock_flags, &currentkey, NULL);
    if (r != 0) goto finish;

    //TODO: Optimization
    //if (do_locking) {
    //   do a lock from currentkey,-infinity to currentkey,infinity
    //   lock_flags |= DB_PRELOCKED
    //}
    
    r = toku_db_cursor_internal(cursor->dbp, dbc_struct_i(cursor)->txn, &count_cursor, DBC_DISABLE_PREFETCHING, 0);
    if (r != 0) goto finish;

    r = toku_c_getf_set(count_cursor, lock_flags, &currentkey, ydb_getf_do_nothing, NULL);
    if (r==0) {
        *count = 1; // there is a key, so the count is one (since we don't have DUP dbs anymore, the only answers are 0 or 1.
    } else {
        *count = 0;
    }
    r = 0;
finish:
    if (currentkey.data) toku_free(currentkey.data);
    if (count_cursor) {
        int rr = toku_c_close(count_cursor); assert(rr == 0);
    }
    return r;
}

static int
toku_c_pre_acquire_range_lock(DBC *dbc, const DBT *key_left, const DBT *key_right) {
    DB *db = dbc->dbp;
    DB_TXN *txn = dbc_struct_i(dbc)->txn;
    HANDLE_PANICKED_DB(db);
    toku_ft_cursor_set_range_lock(dbc_struct_i(dbc)->c, key_left, key_right,
                                   (key_left == toku_lt_neg_infinity),
                                   (key_right == toku_lt_infinity));
    if (!db->i->lt || !txn)
        return 0;
    //READ_UNCOMMITTED and READ_COMMITTED transactions do not need read locks.
    if (!dbc_struct_i(dbc)->rmw && dbc_struct_i(dbc)->iso != TOKU_ISO_SERIALIZABLE)
        return 0;

    toku_lock_type lock_type = dbc_struct_i(dbc)->rmw ? LOCK_REQUEST_WRITE : LOCK_REQUEST_READ;
    int r = get_range_lock(db, txn, key_left, key_right, lock_type);
    return r;
}

int
toku_c_get(DBC* c, DBT* key, DBT* val, uint32_t flag) {
    //This function exists for legacy (test compatibility) purposes/parity with bdb.
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);

    uint32_t main_flag       = get_main_cursor_flag(flag);
    uint32_t remaining_flags = get_nonmain_cursor_flags(flag);
    int r;
    QUERY_CONTEXT_WRAPPED_S context;
    //Passing in NULL for a key or val means that it is NOT an output.
    //    Both key and val are output:
    //        query_context_wrapped_init(&context, c, key,  val);
    //    Val is output, key is not:
    //            query_context_wrapped_init(&context, c, NULL, val);
    //    Neither key nor val are output:
    //            query_context_wrapped_init(&context, c, NULL, NULL); // Used for DB_GET_BOTH
    switch (main_flag) {
        case (DB_FIRST):
            query_context_wrapped_init(&context, c, key,  val);
            r = toku_c_getf_first(c, remaining_flags, c_get_wrapper_callback, &context);
            break;
        case (DB_LAST):
            query_context_wrapped_init(&context, c, key,  val);
            r = toku_c_getf_last(c, remaining_flags, c_get_wrapper_callback, &context);
            break;
        case (DB_NEXT):
        case (DB_NEXT_NODUP):
            query_context_wrapped_init(&context, c, key,  val);
            r = toku_c_getf_next(c, remaining_flags, c_get_wrapper_callback, &context);
            break;
        case (DB_PREV):
        case (DB_PREV_NODUP):
            query_context_wrapped_init(&context, c, key,  val);
            r = toku_c_getf_prev(c, remaining_flags, c_get_wrapper_callback, &context);
            break;
#ifdef DB_PREV_DUP
        case (DB_PREV_DUP):
            query_context_wrapped_init(&context, c, key,  val);
            r = toku_c_getf_prev_dup(c, remaining_flags, c_get_wrapper_callback, &context);
            break;
#endif
        case (DB_CURRENT):
            query_context_wrapped_init(&context, c, key,  val);
            r = toku_c_getf_current(c, remaining_flags, c_get_wrapper_callback, &context);
            break;
        case (DB_CURRENT_BINDING):
            query_context_wrapped_init(&context, c, key,  val);
            r = toku_c_getf_current_binding(c, remaining_flags, c_get_wrapper_callback, &context);
            break;

        case (DB_SET):
            query_context_wrapped_init(&context, c, NULL, val);
            r = toku_c_getf_set(c, remaining_flags, key, c_get_wrapper_callback, &context);
            break;
        case (DB_SET_RANGE):
            query_context_wrapped_init(&context, c, key,  val);
            r = toku_c_getf_set_range(c, remaining_flags, key, c_get_wrapper_callback, &context);
            break;
        case (DB_SET_RANGE_REVERSE):
            query_context_wrapped_init(&context, c, key,  val);
            r = toku_c_getf_set_range_reverse(c, remaining_flags, key, c_get_wrapper_callback, &context);
            break;
        default:
            r = EINVAL;
            break;
    }
    return r;
}

int 
toku_db_cursor_internal(DB * db, DB_TXN * txn, DBC ** c, uint32_t flags, int is_temporary_cursor) {
    HANDLE_PANICKED_DB(db);
    HANDLE_DB_ILLEGAL_WORKING_PARENT_TXN(db, txn);
    DB_ENV* env = db->dbenv;

    if (flags & ~(DB_SERIALIZABLE | DB_INHERIT_ISOLATION | DB_RMW | DBC_DISABLE_PREFETCHING)) {
        return toku_ydb_do_error(
            env, 
            EINVAL, 
            "Invalid flags set for toku_db_cursor\n"
            );
    }

    int r = 0;
    
    struct __toku_dbc_external *XMALLOC(eresult); // so the internal stuff is stuck on the end
    memset(eresult, 0, sizeof(*eresult));
    DBC *result = &eresult->external_part;

    // these methods DO NOT grab the ydb lock
#define SCRS(name) result->name = toku_ ## name
    SCRS(c_get);
    SCRS(c_count);
    SCRS(c_getf_first);
    SCRS(c_getf_last);
    SCRS(c_getf_next);
    SCRS(c_getf_prev);
    SCRS(c_getf_current);
    SCRS(c_getf_current_binding);
    SCRS(c_getf_set);
    SCRS(c_getf_set_range);
    SCRS(c_getf_set_range_reverse);
    SCRS(c_pre_acquire_range_lock);
    SCRS(c_close);
#undef SCRS

#if !TOKUDB_NATIVE_H
    MALLOC(result->i); // otherwise it is allocated as part of result->ii
    assert(result->i);
#endif
    result->dbp = db;

    dbc_struct_i(result)->txn = txn;
    dbc_struct_i(result)->skey_s = (struct simple_dbt){0,0};
    dbc_struct_i(result)->sval_s = (struct simple_dbt){0,0};
    if (is_temporary_cursor) {
        dbc_struct_i(result)->skey = &db->i->skey;
        dbc_struct_i(result)->sval = &db->i->sval;
    } else {
        dbc_struct_i(result)->skey = &dbc_struct_i(result)->skey_s;
        dbc_struct_i(result)->sval = &dbc_struct_i(result)->sval_s;
    }
    if (flags & DB_SERIALIZABLE) {
        dbc_struct_i(result)->iso = TOKU_ISO_SERIALIZABLE;
    } else {
        dbc_struct_i(result)->iso = txn ? db_txn_struct_i(txn)->iso : TOKU_ISO_SERIALIZABLE;
    }
    dbc_struct_i(result)->rmw = (flags & DB_RMW) != 0;
    bool is_snapshot_read = false;
    if (txn) {
        is_snapshot_read = (dbc_struct_i(result)->iso == TOKU_ISO_READ_COMMITTED || 
                            dbc_struct_i(result)->iso == TOKU_ISO_SNAPSHOT);
    }
    r = toku_ft_cursor(
        db->i->ft_handle, 
        &dbc_struct_i(result)->c,
        txn ? db_txn_struct_i(txn)->tokutxn : NULL,
        is_snapshot_read,
        ((flags & DBC_DISABLE_PREFETCHING) != 0)
        );
    assert(r == 0 || r == TOKUDB_MVCC_DICTIONARY_TOO_NEW);
    if (r == 0) {
        // Set the is_temporary_cursor boolean inside the brt node so
        // that a query only needing one cursor will not perform
        // unecessary malloc calls.
        if (is_temporary_cursor) {
            toku_ft_cursor_set_temporary(dbc_struct_i(result)->c);
        }

        *c = result;
    }
    else {
#if !TOKUDB_NATIVE_H
        toku_free(result->i); // otherwise it is allocated as part of result->ii
#endif
        toku_free(result);
    }
    return r;
}

static inline int 
autotxn_db_cursor(DB *db, DB_TXN *txn, DBC **c, uint32_t flags) {
    if (!txn && (db->dbenv->i->open_flags & DB_INIT_TXN)) {
        return toku_ydb_do_error(db->dbenv, EINVAL,
              "Cursors in a transaction environment must have transactions.\n");
    }
    return toku_db_cursor_internal(db, txn, c, flags, 0);
}

// Create a cursor on a db.
// Called without holding the ydb lock.
int 
toku_db_cursor(DB *db, DB_TXN *txn, DBC **c, uint32_t flags) {
    int r = autotxn_db_cursor(db, txn, c, flags);
    return r;
}

#undef STATUS_VALUE

#include <valgrind/helgrind.h>
void __attribute__((constructor)) toku_ydb_cursor_helgrind_ignore(void);
void
toku_ydb_cursor_helgrind_ignore(void) {
    HELGRIND_VALGRIND_HG_DISABLE_CHECKING(&ydb_c_layer_status, sizeof ydb_c_layer_status);
}
