#include "includes.h"

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

static inline int dbt_set_preprocess(DBT* ybt, ITEMLEN len, void** staticptrp) {
    int r = ENOSYS;
    if (ybt) {
        if (ybt->flags==DB_DBT_USERMEM) {
           if (ybt->ulen < len) {
               ybt->size = len;
               r = DB_BUFFER_SMALL;
               goto cleanup;
           }
        }
        else if (ybt->flags==0) {
            if (!staticptrp) { r = -1; goto cleanup; }
        }
        else if (ybt->flags!=DB_DBT_MALLOC && ybt->flags!=DB_DBT_REALLOC) {
            r = EINVAL; goto cleanup;
        }
    }
    r = 0;
cleanup:
    return r;
}

static inline int dbt_set_copy(DBT* ybt, bytevec* datap, ITEMLEN len, void** staticptrp, BOOL input_disposable) {
    if (ybt) {
        if (ybt->flags==DB_DBT_USERMEM) {
            if ((ybt->size=len) > 0) memcpy(ybt->data, *datap, (size_t)len);
            return 0;
        }
        void* tempdata;
        BOOL do_malloc = TRUE;

        if (input_disposable) {
            tempdata  = (void*)*datap;
            do_malloc = FALSE;
        }
        else if (ybt->flags==DB_DBT_REALLOC && ybt->data) {
            if (!ybt->ulen) ybt->ulen = ybt->size;
            if (ybt->ulen>=len && ybt->ulen/2<=len) {
                tempdata  = ybt->data;
                do_malloc = FALSE;
            }
        }
        //Malloc new buffer 
        if (do_malloc) {
            tempdata = toku_malloc(len);
            if (!tempdata) return errno;
        }
        if (input_disposable || do_malloc) {
            //Set ulen
            if (ybt->flags==DB_DBT_REALLOC) ybt->ulen = len;
            //Freeing
            if (ybt->flags==DB_DBT_REALLOC && ybt->data) toku_free(ybt->data);
            if (ybt->flags==0) {
                if (*staticptrp) toku_free(*staticptrp);
                //Set static pointer
                *staticptrp = tempdata;
            }
        }
        //Set ybt->data
        if (ybt->flags!=0 || len>0) ybt->data = tempdata;
        else                        ybt->data = NULL;
        //Set ybt->size and memcpy
        if ((ybt->size=len) > 0 && !input_disposable) memcpy(ybt->data, *datap, (size_t)len);
        if (input_disposable) *datap = NULL;
    }
    return 0;
 }

/* Atomically set three dbts, such that they either both succeed, or
 * there is no side effect. */
int toku_dbt_set_three_values(
        DBT* ybt1, bytevec* ybt1_data, ITEMLEN ybt1_len, void** ybt1_staticptrp, BOOL ybt1_disposable,
        DBT* ybt2, bytevec* ybt2_data, ITEMLEN ybt2_len, void** ybt2_staticptrp, BOOL ybt2_disposable,
        DBT* ybt3, bytevec* ybt3_data, ITEMLEN ybt3_len, void** ybt3_staticptrp, BOOL ybt3_disposable) {
    int r = ENOSYS;

    /* Do all mallocs and check for all possible errors. */
    if ((r = dbt_set_preprocess(ybt1, ybt1_len, ybt1_staticptrp))) goto cleanup;
    if ((r = dbt_set_preprocess(ybt2, ybt2_len, ybt2_staticptrp))) goto cleanup;
    if ((r = dbt_set_preprocess(ybt3, ybt3_len, ybt3_staticptrp))) goto cleanup;

    /* Copy/modify atomically the dbts. */
    if ((r = dbt_set_copy(ybt1, ybt1_data, ybt1_len, ybt1_staticptrp, ybt1_disposable))) goto cleanup;
    if ((r = dbt_set_copy(ybt2, ybt2_data, ybt2_len, ybt2_staticptrp, ybt2_disposable))) goto cleanup;
    if ((r = dbt_set_copy(ybt3, ybt3_data, ybt3_len, ybt3_staticptrp, ybt3_disposable))) goto cleanup;
    
    r = 0;
cleanup:
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
    if ((r = dbt_set_preprocess(ybt1, ybt1_len, ybt1_staticptrp))) goto cleanup;
    if ((r = dbt_set_preprocess(ybt2, ybt2_len, ybt2_staticptrp))) goto cleanup;

    /* Copy/modify atomically the dbts. */
    if ((r = dbt_set_copy(ybt1, ybt1_data, ybt1_len, ybt1_staticptrp, ybt1_disposable))) goto cleanup;
    if ((r = dbt_set_copy(ybt2, ybt2_data, ybt2_len, ybt2_staticptrp, ybt2_disposable))) goto cleanup;
    
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
    if ((r = dbt_set_preprocess(ybt1, ybt1_len, ybt1_staticptrp))) goto cleanup;

    /* Copy/modify atomically the dbts. */
    if ((r = dbt_set_copy(ybt1, ybt1_data, ybt1_len, ybt1_staticptrp, ybt1_disposable))) goto cleanup;
    
    r = 0;
cleanup:
    if (r!=0) {
        if (tmp_ybt1_data) toku_free(tmp_ybt1_data);
    }
    return r;
}

