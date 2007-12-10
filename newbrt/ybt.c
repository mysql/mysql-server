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
    if (ybt->flags==DB_DBT_MALLOC) {
    domalloc:
	ybt->data = toku_malloc(vallen);
	if (errno!=0) return errno;
    } else if (ybt->flags==DB_DBT_REALLOC) {
	if (ybt->data==0) goto domalloc;
	ybt->data = toku_realloc(ybt->data, vallen);
	if (errno!=0) return errno;
    } else if (ybt->flags==DB_DBT_USERMEM) {
        ybt->size = vallen;
        if (ybt->ulen < vallen) return DB_BUFFER_SMALL;
    } else {
	if (staticptrp==0) return -1;
	void *staticptr=*staticptrp;
	//void *old=staticptr;
	if (staticptr==0) 
	    staticptr = toku_malloc(vallen);
	else
	    staticptr = toku_realloc(staticptr, vallen);
	if (errno!=0) return errno;
	//if (old!=staticptr) printf("%s:%d MALLOC --> %p\n", __FILE__, __LINE__, staticptr);
	*staticptrp = staticptr;
	ybt->data = staticptr;
    }
    ybt->size = vallen;
    if (ybt->size>0) {
	memcpy(ybt->data, val, vallen);
    }
    return 0;
}
