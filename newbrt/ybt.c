#define _FILE_OFFSET_BITS 64
#include "ybt.h"
#include "memory.h"
#include <errno.h>
#include <string.h>

DBT *toku_init_dbt (DBT *ybt) {
    memset(ybt, 0, sizeof(*ybt));
    return ybt;
}

DBT *toku_fill_dbt(DBT *dbt, bytevec k, ITEMLEN len) {
    toku_init_dbt(dbt);
    dbt->size=len;
    dbt->data=(char*)k;
    return dbt;
}

static inline int dbt_set_preprocess(DBT* ybt, ITEMLEN len, void** staticptrp, void** tmp_data, BOOL input_disposable) {
    int r = ENOSYS;
    if (ybt) {
        if (ybt->flags==DB_DBT_USERMEM) {
           if (ybt->ulen < len) {
               ybt->size = len;
               r = DB_BUFFER_SMALL;
               goto cleanup;
           }
        }
        else if (ybt->flags==DB_DBT_MALLOC || ybt->flags==DB_DBT_REALLOC || ybt->flags==0) {
            if (ybt->flags==0 && staticptrp==NULL) { r = -1; goto cleanup; }
            if (!input_disposable) {
                *tmp_data = toku_malloc(len);
                if (!*tmp_data) { r = errno; goto cleanup; }
            }
        }
        else { r = EINVAL; goto cleanup; }
    }
    r = 0;
cleanup:
    return r;
}

static inline void dbt_set_copy(DBT* ybt, bytevec* datap, ITEMLEN len, void** staticptrp, void** tmp_data, BOOL input_disposable) {
    if (ybt) {
        bytevec data = *datap;
        if (ybt->flags==DB_DBT_USERMEM) input_disposable = FALSE;
        else if (input_disposable) {
            *tmp_data = (void*)data;
            *datap = NULL;
        }
        if (ybt->flags==DB_DBT_REALLOC && ybt->data) toku_free(ybt->data);
        else if (ybt->flags==0) {
            if (*staticptrp) toku_free(*staticptrp);
            *staticptrp = *tmp_data;
        }
        if (ybt->flags!=DB_DBT_USERMEM) {
            if (ybt->flags!=0 || len>0) ybt->data = *tmp_data;
            else                        ybt->data = NULL;
        }
        if ((ybt->size = len) > 0 && !input_disposable)  memcpy(ybt->data, data, (size_t)len);
    }
 }

/* Atomically set three dbts, such that they either both succeed, or
 * there is no side effect. */
int toku_dbt_set_three_values(
        DBT* ybt1, bytevec* ybt1_data, ITEMLEN ybt1_len, void** ybt1_staticptrp, BOOL ybt1_disposable,
        DBT* ybt2, bytevec* ybt2_data, ITEMLEN ybt2_len, void** ybt2_staticptrp, BOOL ybt2_disposable,
        DBT* ybt3, bytevec* ybt3_data, ITEMLEN ybt3_len, void** ybt3_staticptrp, BOOL ybt3_disposable) {
    int r = ENOSYS;
    void* tmp_ybt1_data = NULL;
    void* tmp_ybt2_data = NULL;
    void* tmp_ybt3_data = NULL;

    /* Do all mallocs and check for all possible errors. */
    if ((r = dbt_set_preprocess(ybt1, ybt1_len, ybt1_staticptrp, &tmp_ybt1_data, ybt1_disposable))) goto cleanup;
    if ((r = dbt_set_preprocess(ybt2, ybt2_len, ybt2_staticptrp, &tmp_ybt2_data, ybt2_disposable))) goto cleanup;
    if ((r = dbt_set_preprocess(ybt3, ybt3_len, ybt3_staticptrp, &tmp_ybt3_data, ybt3_disposable))) goto cleanup;

    /* Copy/modify atomically the dbts. */
    dbt_set_copy(ybt1, ybt1_data, ybt1_len, ybt1_staticptrp, &tmp_ybt1_data, ybt1_disposable);
    dbt_set_copy(ybt2, ybt2_data, ybt2_len, ybt2_staticptrp, &tmp_ybt2_data, ybt2_disposable);
    dbt_set_copy(ybt3, ybt3_data, ybt3_len, ybt3_staticptrp, &tmp_ybt3_data, ybt3_disposable);
    
    r = 0;
cleanup:
    if (r!=0) {
        if (tmp_ybt1_data) toku_free(tmp_ybt1_data);
        if (tmp_ybt2_data) toku_free(tmp_ybt2_data);
        if (tmp_ybt3_data) toku_free(tmp_ybt3_data);
    }
    return r;
}

/* Atomically set two dbts, such that they either both succeed, or
 * there is no side effect. */
int toku_dbt_set_two_values(
        DBT* ybt1, bytevec *ybt1_data, ITEMLEN ybt1_len, void** ybt1_staticptrp, BOOL ybt1_disposable,
        DBT* ybt2, bytevec *ybt2_data, ITEMLEN ybt2_len, void** ybt2_staticptrp, BOOL ybt2_disposable) {
    int r = ENOSYS;
    void* tmp_ybt1_data = NULL;
    void* tmp_ybt2_data = NULL;

    /* Do all mallocs and check for all possible errors. */
    if ((r = dbt_set_preprocess(ybt1, ybt1_len, ybt1_staticptrp, &tmp_ybt1_data, ybt1_disposable))) goto cleanup;
    if ((r = dbt_set_preprocess(ybt2, ybt2_len, ybt2_staticptrp, &tmp_ybt2_data, ybt2_disposable))) goto cleanup;

    /* Copy/modify atomically the dbts. */
    dbt_set_copy(ybt1, ybt1_data, ybt1_len, ybt1_staticptrp, &tmp_ybt1_data, ybt1_disposable);
    dbt_set_copy(ybt2, ybt2_data, ybt2_len, ybt2_staticptrp, &tmp_ybt2_data, ybt2_disposable);
    
    r = 0;
cleanup:
    if (r!=0) {
        if (tmp_ybt1_data) toku_free(tmp_ybt1_data);
        if (tmp_ybt2_data) toku_free(tmp_ybt2_data);
    }
    return r;
}

int toku_dbt_set_value(DBT* ybt1, bytevec* ybt1_data, ITEMLEN ybt1_len, void** ybt1_staticptrp, BOOL ybt1_disposable) {
    int r = ENOSYS;
    void* tmp_ybt1_data = NULL;

    /* Do all mallocs and check for all possible errors. */
    if ((r = dbt_set_preprocess(ybt1, ybt1_len, ybt1_staticptrp, &tmp_ybt1_data, ybt1_disposable))) goto cleanup;

    /* Copy/modify atomically the dbts. */
    dbt_set_copy(ybt1, ybt1_data, ybt1_len, ybt1_staticptrp, &tmp_ybt1_data, ybt1_disposable);
    
    r = 0;
cleanup:
    if (r!=0) {
        if (tmp_ybt1_data) toku_free(tmp_ybt1_data);
    }
    return r;
}

