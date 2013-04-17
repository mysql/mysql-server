/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef TOKU_LOGGGER_H
#define TOKU_LOGGGER_H

#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_portability.h>
#include <errno.h>

#include <db.h>
#include "fttypes.h"
#include "memory.h"
#include "x1764.h"

struct roll_entry;

#include "logger.h"
#include "rollback.h"
#include "recover.h"
#include "txn.h"

static inline int toku_copy_BYTESTRING(BYTESTRING *target, BYTESTRING val) {
    target->len = val.len;
    target->data = (char *) toku_memdup(val.data, (size_t)val.len);
    if (target->data==0) {
        return get_error_errno();
    }
    return 0;
}
static inline void toku_free_TXNID(TXNID txnid __attribute__((__unused__))) {}
static inline void toku_free_LSN(LSN lsn __attribute__((__unused__))) {}
static inline void toku_free_uint64_t(uint64_t u __attribute__((__unused__))) {}
static inline void toku_free_uint32_t(uint32_t u __attribute__((__unused__))) {}
static inline void toku_free_uint8_t(uint8_t u __attribute__((__unused__))) {}
static inline void toku_free_FILENUM(FILENUM u __attribute__((__unused__))) {}
static inline void toku_free_BLOCKNUM(BLOCKNUM u __attribute__((__unused__))) {}
static inline void toku_free_bool(bool u __attribute__((__unused__))) {}
static inline void toku_free_XIDP(XIDP xidp) { toku_free(xidp); }
static inline void toku_free_BYTESTRING(BYTESTRING val) { toku_free(val.data); }
static inline void toku_free_FILENUMS(FILENUMS val) { toku_free(val.filenums); }

int toku_maybe_upgrade_log (const char *env_dir, const char *log_dir, LSN * lsn_of_clean_shutdown, bool * upgrade_in_progress);
uint64_t toku_log_upgrade_get_footprint(void);


#endif
