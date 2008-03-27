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

int toku_dbt_set_value (DBT *ybt, bytevec val, ITEMLEN vallen, void **staticptrp) {
    int r = ENOSYS;
    if (ybt->flags==DB_DBT_MALLOC) {
    domalloc:
	ybt->data = toku_malloc(vallen);
	if (!ybt->data && vallen > 0) { r = errno; goto cleanup; }
    } else if (ybt->flags==DB_DBT_REALLOC) {
	if (ybt->data==0) goto domalloc;
	/* tmp is used to prevent a memory leak if realloc fails */
        void* tmp = toku_realloc(ybt->data, vallen);
	if (!tmp && vallen > 0) { r = errno; goto cleanup; }
        ybt->data = tmp;
    } else if (ybt->flags==DB_DBT_USERMEM) {
        ybt->size = vallen;
        if (ybt->ulen < vallen) { r = DB_BUFFER_SMALL; goto cleanup; }
    } else {
	if (staticptrp==0) return -1;
	void *staticptr=*staticptrp;
	//void *old=staticptr;
	if (staticptr==0) { 
	    staticptr = toku_malloc(vallen);
            if (!staticptr && vallen > 0) { r = errno; goto cleanup; }
        }
	else {
	    /* tmp is used to prevent a memory leak if realloc fails */
            void* tmp = toku_realloc(staticptr, vallen);
            if (!tmp && vallen > 0) { r = errno; goto cleanup; }
            staticptr = tmp;
        }
	//if (old!=staticptr) printf("%s:%d MALLOC --> %p\n", __FILE__, __LINE__, staticptr);
	*staticptrp = staticptr;
	ybt->data = vallen > 0 ? staticptr : 0;
    }
    ybt->size = vallen;
    if (ybt->size>0) {
	memcpy(ybt->data, val, vallen);
    }
    r = 0;
cleanup:
    return r;
}

