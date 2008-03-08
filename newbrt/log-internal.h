#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include "log.h"
#include "toku_assert.h"
#include "list.h"
#include "yerror.h"
#include <stdio.h>
#include <sys/types.h>

#define LOGGER_BUF_SIZE (1<<24)
struct tokulogger {
    int is_open;
    int is_panicked;
    int panic_errno;
    enum typ_tag tag;
    char *directory;
    int fd;
    int n_in_file;
    long long next_log_file_number;
    LSN lsn;
    char buf[LOGGER_BUF_SIZE];
    int  n_in_buf;
    CACHETABLE ct;
    struct list live_txns; // just a linked list.  Should be a hashtable.
    int lg_max; // The size of the single file in the log.  Default is 100MB in TokuDB
};

int toku_logger_find_next_unused_log_file(const char *directory, long long *result);
int toku_logger_find_logfiles (const char *directory, int *n_resultsp, char ***resultp);

enum lt_command {
    LT_COMMIT                   = 'C',
    LT_DELETE                   = 'D',
    LT_FCREATE                  = 'F',
    LT_FHEADER                  = 'H',
    LT_INSERT_WITH_NO_OVERWRITE = 'I',
    LT_NEWBRTNODE               = 'N',
    LT_FOPEN                    = 'O',
    LT_CHECKPOINT               = 'P',
    LT_BLOCK_RENAME             = 'R',
    LT_UNLINK                   = 'U'
};

struct tokutxn {
    enum typ_tag tag;
    u_int64_t txnid64;
    TOKULOGGER logger;
    TOKUTXN    parent;
    LSN        last_lsn; /* Everytime anything is logged, update the LSN.  (We need to atomically record the LSN along with writing into the log.) */
    struct roll_entry *oldest_logentry,*newest_logentry; /* Only logentries with rollbacks are here. There is a list going from newest to oldest. */
    struct list live_txns_link;
};

int toku_logger_finish (TOKULOGGER logger, struct wbuf *wbuf);

static inline int toku_logsizeof_u_int8_t (u_int32_t v __attribute__((__unused__))) {
    return 1;
}

static inline int toku_logsizeof_u_int32_t (u_int32_t v __attribute__((__unused__))) {
    return 4;
}

static inline int toku_logsizeof_FILENUM (FILENUM v __attribute__((__unused__))) {
    return 4;
}

static inline int toku_logsizeof_DISKOFF (DISKOFF v __attribute__((__unused__))) {
    return 8;
}

static inline int toku_logsizeof_TXNID (TXNID txnid __attribute__((__unused__))) {
    return 8;
}

static inline int toku_logsizeof_BYTESTRING (BYTESTRING bs) {
    return 4+bs.len;
}

static inline int toku_logsizeof_LOGGEDBRTHEADER (LOGGEDBRTHEADER bs) {
    assert(bs.n_named_roots=0);
    return 4+4+4+8+8+4+8;
}

static inline int toku_logsizeof_INTPAIRARRAY (INTPAIRARRAY pa) {
    return 4+(4+4)*pa.size;
}

