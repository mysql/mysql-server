#ifndef TOKULOGGGER_H
#define TOKULOGGGER_H

#ident "$Id$"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_portability.h>
#include <errno.h>

#include "../include/db.h"
#include "brttypes.h"
#include "memory.h"
#include "bread.h"
#include "x1764.h"

struct logbytes {
    struct logbytes *next;
    int nbytes;
    LSN lsn;
    char bytes[1];
};

#define MALLOC_LOGBYTES(n) toku_malloc(sizeof(struct logbytes)+n -1)

typedef void(*voidfp)(void);
typedef void(*YIELDF)(voidfp, void*);
struct roll_entry;

#include "logger.h"
#include "rollback.h"
#include "recover.h"
#include "txn.h"

static inline int toku_copy_BYTESTRING(BYTESTRING *target, BYTESTRING val) {
    target->len = val.len;
    target->data = toku_memdup(val.data, (size_t)val.len);
    if (target->data==0) return errno;
    return 0;
}
static inline void toku_free_BYTESTRING(BYTESTRING val) { toku_free(val.data); }

void toku_set_lsn_increment (uint64_t incr) __attribute__((__visibility__("default")));

#endif
