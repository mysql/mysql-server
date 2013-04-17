/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <db.h>
#include <memory.h>
#include <string.h>
#include <fttypes.h>

#include "ybt.h"

DBT *
toku_init_dbt(DBT *ybt) {
    memset(ybt, 0, sizeof(*ybt));
    return ybt;
}

DBT *
toku_init_dbt_flags(DBT *ybt, uint32_t flags) {
    toku_init_dbt(ybt);
    ybt->flags = flags;
    return ybt;
}

void
toku_destroy_dbt(DBT *dbt) {
    switch (dbt->flags) {
    case DB_DBT_MALLOC:
    case DB_DBT_REALLOC:
        toku_free(dbt->data);
        dbt->data = NULL;
        break;
    }
}

DBT *
toku_fill_dbt(DBT *dbt, bytevec k, ITEMLEN len) {
    dbt->flags = 0;
    dbt->ulen = 0;
    dbt->size=len;
    dbt->data=(char*)k;
    return dbt;
}

DBT *toku_copyref_dbt(DBT *dst, const DBT src) {
    dst->flags = 0;
    dst->ulen = 0;
    dst->size = src.size;
    dst->data = src.data;
    return dst;
}

DBT *toku_clone_dbt(DBT *dst, const DBT &src) {
    dst->flags = DB_DBT_MALLOC;
    dst->ulen = 0;
    dst->size = src.size;
    dst->data = toku_xmemdup(src.data, src.size);
    return dst;
}

void
toku_sdbt_cleanup(struct simple_dbt *sdbt) {
    if (sdbt->data) toku_free(sdbt->data);
    memset(sdbt, 0, sizeof(*sdbt));
}

static inline int
sdbt_realloc(struct simple_dbt *sdbt) {
    void *new_data = toku_realloc(sdbt->data, sdbt->len);
    int r;
    if (new_data == NULL) {
        r = get_error_errno();
    } else {
        sdbt->data = new_data;
        r = 0;
    }
    return r;
}

static inline int
dbt_realloc(DBT *dbt) {
    void *new_data = toku_realloc(dbt->data, dbt->ulen);
    int r;
    if (new_data == NULL) {
        r = get_error_errno();
    } else {
        dbt->data = new_data;
        r = 0;
    }
    return r;
}

int
toku_dbt_set (ITEMLEN len, bytevec val, DBT *d, struct simple_dbt *sdbt) {
// sdbt is the static value used when flags==0
// Otherwise malloc or use the user-supplied memory, as according to the flags in d->flags.
    int r;
    if (!d) r = 0;
    else {
        switch (d->flags) {
        case (DB_DBT_USERMEM):
            d->size = len;
            if (d->ulen<len) r = DB_BUFFER_SMALL;
            else {
                memcpy(d->data, val, len);
                r = 0;
            }
            break;
        case (DB_DBT_MALLOC):
            d->data = NULL;
            d->ulen = 0;
            //Fall through to DB_DBT_REALLOC
        case (DB_DBT_REALLOC):
            if (d->ulen < len) {
                d->ulen = len*2;
                r = dbt_realloc(d);
            }
            else if (d->ulen > 16 && d->ulen > len*4) {
                d->ulen = len*2 < 16 ? 16 : len*2;
                r = dbt_realloc(d);
            }
            else if (d->data==NULL) {
                d->ulen = len;
                r = dbt_realloc(d);
            }
            else r=0;

            if (r==0) {
                memcpy(d->data, val, len);
                d->size = len;
            }
            break;
        case (0):
            if (sdbt->len < len) {
                sdbt->len = len*2;
                r = sdbt_realloc(sdbt);
            }
            else if (sdbt->len > 16 && sdbt->len > len*4) {
                sdbt->len = len*2 < 16 ? 16 : len*2;
                r = sdbt_realloc(sdbt);
            }
            else r=0;

            if (r==0) {
                memcpy(sdbt->data, val, len);
                d->data = sdbt->data;
                d->size = len;
            }
            break;
        default:
            r = EINVAL;
            break;
        }
    }
    return r;
}

const DBT *toku_dbt_positive_infinity(void) {
    static DBT positive_infinity_dbt = {};
    return &positive_infinity_dbt;
}

const DBT *toku_dbt_negative_infinity(void) {
    static DBT negative_infinity_dbt = {};
    return &negative_infinity_dbt;
}

bool toku_dbt_is_infinite(const DBT *dbt) {
    return dbt == toku_dbt_positive_infinity() || dbt == toku_dbt_negative_infinity();
}

int toku_dbt_infinite_compare(const DBT *a, const DBT *b) {
    if (a == b) {
        return 0;
    } else if (a == toku_dbt_positive_infinity()) {
        return 1;
    } else if (b == toku_dbt_positive_infinity()) {
        return -1;
    } else if (a == toku_dbt_negative_infinity()) {
        return -1;
    } else {
        invariant(b == toku_dbt_negative_infinity());     
        return 1;
    }
}

bool toku_dbt_equals(const DBT *a, const DBT *b) {
    if (!toku_dbt_is_infinite(a) && !toku_dbt_is_infinite(b)) {
        return a->data == b->data && a->size == b->size;
    } else {
        // a or b is infinite, so they're equal if they are the same infinite
        return a == b ? true : false;
    }
}
