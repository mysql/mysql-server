#ifndef BRTTYPES_H
#define BRTTYPES_H

#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <sys/types.h>
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif
#define _FILE_OFFSET_BITS 64

#include <db.h>
#include <inttypes.h>

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

typedef struct brt *BRT;
struct brt_header;
struct wbuf;

typedef struct descriptor {
    u_int32_t version;
    DBT       dbt;
} *DESCRIPTOR, DESCRIPTOR_S;

typedef unsigned int ITEMLEN;
typedef const void *bytevec;
//typedef const void *bytevec;

typedef int64_t DISKOFF;  /* Offset in a disk. -1 is the NULL pointer. */
typedef u_int64_t TXNID;
#define TXNID_NONE_LIVING ((TXNID)0)
#define TXNID_NONE        ((TXNID)0)

typedef struct s_blocknum { int64_t b; } BLOCKNUM; // make a struct so that we will notice type problems.
#define ROLLBACK_NONE     ((BLOCKNUM){0})

static inline BLOCKNUM make_blocknum(int64_t b) { BLOCKNUM result={b}; return result; }

typedef struct {
    u_int32_t len;
    char *data;
} BYTESTRING;

/* Make the LSN be a struct instead of an integer so that we get better type checking. */
typedef struct __toku_lsn { u_int64_t lsn; } LSN;
#define ZERO_LSN ((LSN){0})
#define MAX_LSN  ((LSN){UINT64_MAX})

/* At the brt layer, a FILENUM uniquely identifies an open file.
 * At the ydb layer, a DICTIONARY_ID uniquely identifies an open dictionary.
 * With the introduction of the loader (ticket 2216), it is possible for the file that holds
 * an open dictionary to change, so these are now separate and independent unique identifiers.
 */
typedef struct {u_int32_t fileid;} FILENUM;
#define FILENUM_NONE ((FILENUM){UINT32_MAX})

typedef struct {u_int32_t dictid;} DICTIONARY_ID;
#define DICTIONARY_ID_NONE ((DICTIONARY_ID){0})

typedef struct {
    u_int32_t num;
    FILENUM  *filenums;
} FILENUMS;

#if !TOKU_WINDOWS && !defined(BOOL_DEFINED)
#define BOOL_DEFINED
typedef enum __toku_bool { FALSE=0, TRUE=1} BOOL;
#endif

typedef struct tokulogger *TOKULOGGER;
#define NULL_LOGGER ((TOKULOGGER)0)
typedef struct tokutxn    *TOKUTXN;
typedef struct txninfo    *TXNINFO;
#define NULL_TXN ((TOKUTXN)0)

struct logged_btt_pair {
    DISKOFF off;
    int32_t size;
};

typedef struct cachetable *CACHETABLE;
typedef struct cachefile *CACHEFILE;

/* tree command types */
enum brt_msg_type {
    BRT_NONE = 0,
    BRT_INSERT = 1,
    BRT_DELETE_ANY = 2,  // Delete any matching key.  This used to be called BRT_DELETE.
    BRT_DELETE_BOTH = 3,
    BRT_ABORT_ANY = 4,   // Abort any commands on any matching key.
    BRT_ABORT_BOTH  = 5, // Abort commands that match both the key and the value
    BRT_COMMIT_ANY  = 6,
    BRT_COMMIT_BOTH = 7,
    BRT_COMMIT_BROADCAST_ALL = 8, // Broadcast to all leafentries, (commit all transactions).
    BRT_COMMIT_BROADCAST_TXN = 9, // Broadcast to all leafentries, (commit specific transaction).
    BRT_ABORT_BROADCAST_TXN  = 10, // Broadcast to all leafentries, (commit specific transaction).
    BRT_INSERT_NO_OVERWRITE = 11,
};

typedef struct xids_t *XIDS;
typedef struct fifo_msg_t *FIFO_MSG;
/* tree commands */
struct brt_msg {
    enum brt_msg_type type;
    XIDS         xids;
    union {
        /* insert or delete */
        struct brt_cmd_insert_delete {
            DBT *key;
            DBT *val;
        } id;
    } u;
};
// Message sent into brt to implement command (insert, delete, etc.)
// This structure supports nested transactions, and obsoletes brt_msg.
typedef struct brt_msg BRT_MSG_S, *BRT_MSG;

typedef int (*brt_compare_func)(DB *, const DBT *, const DBT *);

typedef int (*generate_row_for_put_func)(DB *dest_db, DB *src_db, DBT *dest_key, DBT *dest_val, const DBT *src_key, const DBT *src_val, void *extra);
typedef int (*generate_row_for_del_func)(DB *dest_db, DB *src_db, DBT *dest_val, const DBT *src_key, const DBT *src_val, void *extra);

#define UU(x) x __attribute__((__unused__))

typedef struct memarena *MEMARENA;
typedef struct rollback_log_node *ROLLBACK_LOG_NODE;

#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif

#endif

