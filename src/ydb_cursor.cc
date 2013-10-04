/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
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

#define STATUS_INIT(k,c,t,l,inc) TOKUDB_STATUS_INIT(ydb_c_layer_status, k, c, t, l, inc)

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
c_uninitialized(DBC* c) {
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
    int r = toku_dbt_set(key->size, key->data, context->key, context->skey);
    if (r == 0) {
        r = toku_dbt_set(val->size, val->data, context->val, context->sval);
    }
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
//c_getf_first

typedef struct query_context_base_t {
    FT_CURSOR  c;
    DB_TXN     *txn;
    DB         *db;
    YDB_CALLBACK_FUNCTION f;
    void       *f_extra;
    int         r_user_callback;
    bool        do_locking;
    bool        is_write_op;
    toku::lock_request request;
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
    if (context->is_write_op) {
        lock_flags &= DB_PRELOCKED_WRITE; // Only care about whether already locked for write
    }
    context->do_locking = (context->db->i->lt != nullptr && !(lock_flags & (DB_PRELOCKED | DB_PRELOCKED_WRITE)));
    context->r_user_callback = 0;
    uint64_t lock_wait_time = context->txn ? context->txn->mgrp->i->ltm.get_lock_wait_time() : 0;
    context->request.create(lock_wait_time);
}

static toku::lock_request::type
query_context_determine_lock_type(QUERY_CONTEXT_BASE context) {
    return context->is_write_op ?
        toku::lock_request::type::WRITE : toku::lock_request::type::READ;
}

static void
query_context_base_destroy(QUERY_CONTEXT_BASE context) {
    context->request.destroy();
}

static void
query_context_init_read(QUERY_CONTEXT context, DBC *c, uint32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    const bool is_write = false;
    query_context_base_init(&context->base, c, flag, is_write, f, extra);
}

static void
query_context_init_write(QUERY_CONTEXT context, DBC *c, uint32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    const bool is_write = true;
    query_context_base_init(&context->base, c, flag, is_write, f, extra);
}

static void
query_context_with_input_init(QUERY_CONTEXT_WITH_INPUT context, DBC *c, uint32_t flag, DBT *key, DBT *val, YDB_CALLBACK_FUNCTION f, void *extra) {
    // grab write locks if the DB_RMW flag is set or the cursor was created with the DB_RMW flag
    const bool is_write = ((flag & DB_RMW) != 0) || dbc_struct_i(c)->rmw;
    query_context_base_init(&context->base, c, flag, is_write, f, extra);
    context->input_key = key;
    context->input_val = val;
}

static int c_getf_first_callback(ITEMLEN keylen, bytevec key, ITEMLEN vallen, bytevec val, void *extra, bool);

static void 
c_query_context_init(QUERY_CONTEXT context, DBC *c, uint32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    bool is_write_op = false;
    // grab write locks if the DB_RMW flag is set or the cursor was created with the DB_RMW flag
    if ((flag & DB_RMW) || dbc_struct_i(c)->rmw) {
        is_write_op = true;
    }
    if (is_write_op) {
        query_context_init_write(context, c, flag, f, extra);
    } else {
        query_context_init_read(context, c, flag, f, extra);
    }
}

static void 
c_query_context_destroy(QUERY_CONTEXT context) {
    query_context_base_destroy(&context->base);
}

static int
c_getf_first(DBC *c, uint32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);
    int r = 0;
    QUERY_CONTEXT_S context; //Describes the context of this query.
    c_query_context_init(&context, c, flag, f, extra);
    while (r == 0) {
        //toku_ft_cursor_first will call c_getf_first_callback(..., context) (if query is successful)
        r = toku_ft_cursor_first(dbc_struct_i(c)->c, c_getf_first_callback, &context);
        if (r == DB_LOCK_NOTGRANTED) {
            r = toku_db_wait_range_lock(context.base.db, context.base.txn, &context.base.request);
        } else {
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
        const DBT *left_key = toku_dbt_negative_infinity();
        const DBT *right_key = key != NULL ? &found_key : toku_dbt_positive_infinity();
        r = toku_db_start_range_lock(context->db, context->txn, left_key, right_key, 
                             query_context_determine_lock_type(context), &context->request);
    } else {
        r = 0;
    }

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
c_getf_last(DBC *c, uint32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);
    int r = 0;
    QUERY_CONTEXT_S context; //Describes the context of this query.
    c_query_context_init(&context, c, flag, f, extra); 
    while (r == 0) {
        //toku_ft_cursor_last will call c_getf_last_callback(..., context) (if query is successful)
        r = toku_ft_cursor_last(dbc_struct_i(c)->c, c_getf_last_callback, &context);
        if (r == DB_LOCK_NOTGRANTED) {
            r = toku_db_wait_range_lock(context.base.db, context.base.txn, &context.base.request);
        } else {
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
        const DBT *left_key = key != NULL ? &found_key : toku_dbt_negative_infinity();
        const DBT *right_key = toku_dbt_positive_infinity();
        r = toku_db_start_range_lock(context->db, context->txn, left_key, right_key, 
                             query_context_determine_lock_type(context), &context->request);
    } else {
        r = 0;
    }

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
c_getf_next(DBC *c, uint32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    int r;
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);
    if (c_uninitialized(c)) {
        r = c_getf_first(c, flag, f, extra);
    } else {
        r = 0;
        QUERY_CONTEXT_S context; //Describes the context of this query.
        c_query_context_init(&context, c, flag, f, extra); 
        while (r == 0) {
            //toku_ft_cursor_next will call c_getf_next_callback(..., context) (if query is successful)
            r = toku_ft_cursor_next(dbc_struct_i(c)->c, c_getf_next_callback, &context);
            if (r == DB_LOCK_NOTGRANTED) {
                r = toku_db_wait_range_lock(context.base.db, context.base.txn, &context.base.request);
            } else {
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
        const DBT *right_key = key != NULL ? &found_key : toku_dbt_positive_infinity();
        r = toku_db_start_range_lock(context->db, context->txn, left_key, right_key, 
                             query_context_determine_lock_type(context), &context->request);
    } else {
        r = 0;
    }

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
c_getf_prev(DBC *c, uint32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    int r;
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);
    if (c_uninitialized(c)) {
        r = c_getf_last(c, flag, f, extra);
    } else {
        r = 0;
        QUERY_CONTEXT_S context; //Describes the context of this query.
        c_query_context_init(&context, c, flag, f, extra);
        while (r == 0) {
            //toku_ft_cursor_prev will call c_getf_prev_callback(..., context) (if query is successful)
            r = toku_ft_cursor_prev(dbc_struct_i(c)->c, c_getf_prev_callback, &context);
            if (r == DB_LOCK_NOTGRANTED) {
                r = toku_db_wait_range_lock(context.base.db, context.base.txn, &context.base.request);
            } else {
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
        const DBT *left_key = key != NULL ? &found_key : toku_dbt_negative_infinity();
        const DBT *right_key = prevkey;
        r = toku_db_start_range_lock(context->db, context->txn, left_key, right_key, 
                             query_context_determine_lock_type(context), &context->request);
    } else {
        r = 0;
    }

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
c_getf_current(DBC *c, uint32_t flag, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);

    QUERY_CONTEXT_S context; //Describes the context of this query.
    c_query_context_init(&context, c, flag, f, extra); 
    //toku_ft_cursor_current will call c_getf_current_callback(..., context) (if query is successful)
    int r = toku_ft_cursor_current(dbc_struct_i(c)->c, DB_CURRENT, c_getf_current_callback, &context);
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
    } else {
        r = 0;
    }

    //Give brt-layer an error (if any) to return from toku_ft_cursor_current
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
        if (r == DB_LOCK_NOTGRANTED) {
            r = toku_db_wait_range_lock(context.base.db, context.base.txn, &context.base.request);
        } else {
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
        r = toku_db_start_range_lock(context->db, context->txn, super_context->input_key, super_context->input_key, 
                             query_context_determine_lock_type(context), &context->request);
    } else {
        r = 0;
    }

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
c_getf_set_range(DBC *c, uint32_t flag, DBT *key, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);

    int r = 0;
    QUERY_CONTEXT_WITH_INPUT_S context; //Describes the context of this query.
    query_context_with_input_init(&context, c, flag, key, NULL, f, extra); 
    while (r == 0) {
        //toku_ft_cursor_set_range will call c_getf_set_range_callback(..., context) (if query is successful)
        r = toku_ft_cursor_set_range(dbc_struct_i(c)->c, key, c_getf_set_range_callback, &context);
        if (r == DB_LOCK_NOTGRANTED) {
            r = toku_db_wait_range_lock(context.base.db, context.base.txn, &context.base.request);
        } else {
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
        const DBT *right_key = key != NULL ? &found_key : toku_dbt_positive_infinity();
        r = toku_db_start_range_lock(context->db, context->txn, left_key, right_key, 
                             query_context_determine_lock_type(context), &context->request);
    } else {
        r = 0;
    }

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
c_getf_set_range_reverse(DBC *c, uint32_t flag, DBT *key, YDB_CALLBACK_FUNCTION f, void *extra) {
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);

    int r = 0;
    QUERY_CONTEXT_WITH_INPUT_S context; //Describes the context of this query.
    query_context_with_input_init(&context, c, flag, key, NULL, f, extra); 
    while (r == 0) {
        //toku_ft_cursor_set_range_reverse will call c_getf_set_range_reverse_callback(..., context) (if query is successful)
        r = toku_ft_cursor_set_range_reverse(dbc_struct_i(c)->c, key, c_getf_set_range_reverse_callback, &context);
        if (r == DB_LOCK_NOTGRANTED) {
            r = toku_db_wait_range_lock(context.base.db, context.base.txn, &context.base.request);
        } else {
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
        const DBT *left_key = key != NULL ? &found_key : toku_dbt_negative_infinity();
        const DBT *right_key = super_context->input_key;
        r = toku_db_start_range_lock(context->db, context->txn, left_key, right_key, 
                             query_context_determine_lock_type(context), &context->request);
    } else {
        r = 0;
    }

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
int 
toku_c_close(DBC * c) {
    HANDLE_PANICKED_DB(c->dbp);
    HANDLE_CURSOR_ILLEGAL_WORKING_PARENT_TXN(c);
    toku_ft_cursor_close(dbc_struct_i(c)->c);
    toku_sdbt_cleanup(&dbc_struct_i(c)->skey_s);
    toku_sdbt_cleanup(&dbc_struct_i(c)->sval_s);
    toku_free(c);
    return 0;
}

static int
c_set_bounds(DBC *dbc, const DBT *left_key, const DBT *right_key, bool pre_acquire, int out_of_range_error) {
    if (out_of_range_error != DB_NOTFOUND &&
        out_of_range_error != TOKUDB_OUT_OF_RANGE &&
        out_of_range_error != 0) {
        return toku_ydb_do_error(
            dbc->dbp->dbenv,
            EINVAL,
            "Invalid out_of_range_error [%d] for %s\n",
            out_of_range_error,
            __FUNCTION__ 
            );
    }
    if (left_key == toku_dbt_negative_infinity() && right_key == toku_dbt_positive_infinity()) {
        out_of_range_error = 0;
    }
    DB *db = dbc->dbp;
    DB_TXN *txn = dbc_struct_i(dbc)->txn;
    HANDLE_PANICKED_DB(db);
    toku_ft_cursor_set_range_lock(dbc_struct_i(dbc)->c, left_key, right_key,
                                   (left_key == toku_dbt_negative_infinity()),
                                   (right_key == toku_dbt_positive_infinity()),
                                   out_of_range_error);
    if (!db->i->lt || !txn || !pre_acquire)
        return 0;
    //READ_UNCOMMITTED and READ_COMMITTED transactions do not need read locks.
    if (!dbc_struct_i(dbc)->rmw && dbc_struct_i(dbc)->iso != TOKU_ISO_SERIALIZABLE)
        return 0;

    toku::lock_request::type lock_type = dbc_struct_i(dbc)->rmw ?
        toku::lock_request::type::WRITE : toku::lock_request::type::READ;
    int r = toku_db_get_range_lock(db, txn, left_key, right_key, lock_type);
    return r;
}

static void
c_remove_restriction(DBC *dbc) {
    toku_ft_cursor_remove_restriction(dbc_struct_i(dbc)->c);
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
    //            query_context_wrapped_init(&context, c, NULL, NULL);
    switch (main_flag) {
        case (DB_FIRST):
            query_context_wrapped_init(&context, c, key,  val);
            r = c_getf_first(c, remaining_flags, c_get_wrapper_callback, &context);
            break;
        case (DB_LAST):
            query_context_wrapped_init(&context, c, key,  val);
            r = c_getf_last(c, remaining_flags, c_get_wrapper_callback, &context);
            break;
        case (DB_NEXT):
            query_context_wrapped_init(&context, c, key,  val);
            r = c_getf_next(c, remaining_flags, c_get_wrapper_callback, &context);
            break;
        case (DB_PREV):
            query_context_wrapped_init(&context, c, key,  val);
            r = c_getf_prev(c, remaining_flags, c_get_wrapper_callback, &context);
            break;
#ifdef DB_PREV_DUP
        case (DB_PREV_DUP):
            query_context_wrapped_init(&context, c, key,  val);
            r = toku_c_getf_prev_dup(c, remaining_flags, c_get_wrapper_callback, &context);
            break;
#endif
        case (DB_CURRENT):
            query_context_wrapped_init(&context, c, key,  val);
            r = c_getf_current(c, remaining_flags, c_get_wrapper_callback, &context);
            break;
        case (DB_SET):
            query_context_wrapped_init(&context, c, NULL, val);
            r = toku_c_getf_set(c, remaining_flags, key, c_get_wrapper_callback, &context);
            break;
        case (DB_SET_RANGE):
            query_context_wrapped_init(&context, c, key,  val);
            r = c_getf_set_range(c, remaining_flags, key, c_get_wrapper_callback, &context);
            break;
        case (DB_SET_RANGE_REVERSE):
            query_context_wrapped_init(&context, c, key,  val);
            r = c_getf_set_range_reverse(c, remaining_flags, key, c_get_wrapper_callback, &context);
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

#define SCRS(name) result->name = name
    SCRS(c_getf_first);
    SCRS(c_getf_last);
    SCRS(c_getf_next);
    SCRS(c_getf_prev);
    SCRS(c_getf_current);
    SCRS(c_getf_set_range);
    SCRS(c_getf_set_range_reverse);
    SCRS(c_set_bounds);
    SCRS(c_remove_restriction);
#undef SCRS

    result->c_get = toku_c_get;
    result->c_getf_set = toku_c_getf_set;
    result->c_close = toku_c_close;

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
int 
toku_db_cursor(DB *db, DB_TXN *txn, DBC **c, uint32_t flags) {
    int r = autotxn_db_cursor(db, txn, c, flags);
    return r;
}

#undef STATUS_VALUE

#include <toku_race_tools.h>
void __attribute__((constructor)) toku_ydb_cursor_helgrind_ignore(void);
void
toku_ydb_cursor_helgrind_ignore(void) {
    TOKU_VALGRIND_HG_DISABLE_CHECKING(&ydb_c_layer_status, sizeof ydb_c_layer_status);
}
