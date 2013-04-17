#ifndef TOKU_LOGGGER_H
#define TOKU_LOGGGER_H

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_portability.h>
#include <errno.h>

#include "../include/db.h"
#include "brttypes.h"
#include "memory.h"
#include "x1764.h"

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

typedef void(*voidfp)(void *thunk);
typedef void(*YIELDF)(voidfp, void *fpthunk, void *yieldthunk);

struct roll_entry;

#include "logger.h"
#include "rollback.h"
#include "recover.h"
#include "txn.h"

static inline int toku_copy_BYTESTRING(BYTESTRING *target, BYTESTRING val) {
    target->len = val.len;
    target->data = (char *) toku_memdup(val.data, (size_t)val.len);
    if (target->data==0) return errno;
    return 0;
}
static inline void toku_free_BYTESTRING(BYTESTRING val) { toku_free(val.data); }
static inline void toku_free_FILENUMS(FILENUMS val) { toku_free(val.filenums); }

void toku_set_lsn_increment (uint64_t incr) __attribute__((__visibility__("default")));

int toku_maybe_upgrade_log (const char *env_dir, const char *log_dir);
uint64_t toku_log_upgrade_get_footprint(void);


#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif

#endif
