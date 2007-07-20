#define _FILE_OFFSET_BITS 64
#include "ybt.h"
#include "memory.h"
#include <errno.h>
#include <string.h>

int ybt_init (DBT *ybt) {
    memset(ybt, 0, sizeof(*ybt));
    return 0;
}

int ybt_set_value (DBT *ybt, bytevec val, ITEMLEN vallen, void **staticptrp) {
    if (ybt->flags==DB_DBT_MALLOC) {
    domalloc:
	ybt->data = toku_malloc(vallen);
	if (errno!=0) return errno;
	ybt->ulen = vallen;
    } else if (ybt->flags==DB_DBT_REALLOC) {
	if (ybt->data==0) goto domalloc;
	ybt->data = toku_realloc(ybt->data, vallen);
	if (errno!=0) return errno;
	ybt->ulen = vallen;

    } else if (ybt->flags==DB_DBT_USERMEM) {
	/*nothing*/
    } else {
	if (staticptrp==0) return -1;
	void *staticptr=*staticptrp;
	if (staticptr==0) 
	    staticptr = toku_malloc(vallen);
	else
	    staticptr = toku_realloc(staticptr, vallen);
	if (errno!=0) return errno;
	*staticptrp = staticptr;
	ybt->data = staticptr;
	ybt->ulen = vallen;
    }
    ybt->size = vallen;
    if (ybt->ulen>0) {
	if (ybt->ulen<vallen) vallen=ybt->ulen;
	memcpy(ybt->data, val, vallen);
    }
    return 0;
}
