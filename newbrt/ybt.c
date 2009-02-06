#include "includes.h"

DBT*
toku_init_dbt (DBT *ybt) {
    memset(ybt, 0, sizeof(*ybt));
    return ybt;
}

DBT*
toku_fill_dbt(DBT *dbt, bytevec k, ITEMLEN len) {
    toku_init_dbt(dbt);
    dbt->size=len;
    dbt->data=(char*)k;
    return dbt;
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
    if (new_data == NULL) r = errno;
    else {
        sdbt->data = new_data;
        r = 0;
    }
    return r;
}

static inline int
dbt_realloc(DBT *dbt) {
    void *new_data = toku_realloc(dbt->data, dbt->ulen);
    int r;
    if (new_data == NULL) r = errno;
    else {
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

