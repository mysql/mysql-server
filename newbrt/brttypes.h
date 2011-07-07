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
typedef struct brtnode *BRTNODE;
typedef struct brtnode_leaf_basement_node *BASEMENTNODE;
typedef struct brtnode_nonleaf_childinfo *NONLEAF_CHILDINFO;
typedef struct sub_block *SUB_BLOCK;
typedef struct subtree_estimates *SUBTREE_EST;
struct brt_header;
struct wbuf;
struct dbuf;

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

/* Log Sequence Number (LSN)
 * Make the LSN be a struct instead of an integer so that we get better type checking. */
typedef struct __toku_lsn { u_int64_t lsn; } LSN;
#define ZERO_LSN ((LSN){0})
#define MAX_LSN  ((LSN){UINT64_MAX})

/* Message Sequence Number (MSN)
 * Make the MSN be a struct instead of an integer so that we get better type checking. */
typedef struct __toku_msn { u_int64_t msn; } MSN;
#define ZERO_MSN ((MSN){0})                 // dummy used for message construction, to be filled in when msg is applied to tree
#define MIN_MSN  ((MSN){(u_int64_t)1<<32})  // first 2**32 values reserved for messages created before Dr. No (for upgrade)
#define MAX_MSN  ((MSN){UINT64_MAX})

/* At the brt layer, a FILENUM uniquely identifies an open file.
 * At the ydb layer, a DICTIONARY_ID uniquely identifies an open dictionary.
 * With the introduction of the loader (ticket 2216), it is possible for the file that holds
 * an open dictionary to change, so these are now separate and independent unique identifiers.
 */
typedef struct {u_int32_t fileid;} FILENUM;
#define FILENUM_NONE ((FILENUM){UINT32_MAX})

typedef struct {u_int64_t dictid;} DICTIONARY_ID;
#define DICTIONARY_ID_NONE ((DICTIONARY_ID){0})

typedef struct {
    u_int32_t num;
    FILENUM  *filenums;
} FILENUMS;

#include <stdbool.h>
#ifndef TRUE
// In the future, use the stdbool bool and constants (true false), rather than BOOL, TRUE, and FALSE.
#define TRUE true
#define FALSE false
typedef bool BOOL;
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
    //BRT_DELETE_BOTH = 3,
    BRT_ABORT_ANY = 4,   // Abort any commands on any matching key.
    //BRT_ABORT_BOTH  = 5, // Abort commands that match both the key and the value
    BRT_COMMIT_ANY  = 6,
    //BRT_COMMIT_BOTH = 7,
    BRT_COMMIT_BROADCAST_ALL = 8, // Broadcast to all leafentries, (commit all transactions).
    BRT_COMMIT_BROADCAST_TXN = 9, // Broadcast to all leafentries, (commit specific transaction).
    BRT_ABORT_BROADCAST_TXN  = 10, // Broadcast to all leafentries, (commit specific transaction).
    BRT_INSERT_NO_OVERWRITE = 11,
    BRT_OPTIMIZE = 12,             // Broadcast
    BRT_OPTIMIZE_FOR_UPGRADE = 13, // same as BRT_OPTIMIZE, but record version number in leafnode
    BRT_UPDATE = 14,
    BRT_UPDATE_BROADCAST_ALL = 15
};

typedef struct xids_t *XIDS;
typedef struct fifo_msg_t *FIFO_MSG;
/* tree commands */
struct brt_msg {
    enum brt_msg_type type;
    MSN          msn;          // message sequence number
    XIDS         xids;
    union {
        /* insert or delete */
        struct brt_cmd_insert_delete {
            const DBT *key;   // for insert, delete, upsertdel
            const DBT *val;   // for insert, delete, (and it is the "extra" for upsertdel, upsertdel_broadcast_all)
        } id;
    } u;
};
// Message sent into brt to implement command (insert, delete, etc.)
// This structure supports nested transactions, and obsoletes brt_msg.
typedef struct brt_msg BRT_MSG_S;
typedef const struct brt_msg *BRT_MSG;

typedef int (*brt_compare_func)(DB *, const DBT *, const DBT *);
typedef void (*setval_func)(const DBT *, void *);
typedef int (*brt_update_func)(DB *, const DBT *, const DBT *, const DBT *, setval_func, void *);

#define UU(x) x __attribute__((__unused__))

typedef struct memarena *MEMARENA;
typedef struct rollback_log_node *ROLLBACK_LOG_NODE;

//
// Types of snapshots that can be taken by a tokutxn
//  - TXN_SNAPSHOT_NONE: means that there is no snapshot. Reads do not use snapshot reads.
//                       used for SERIALIZABLE and READ UNCOMMITTED
//  - TXN_SNAPSHOT_ROOT: means that all tokutxns use their root transaction's snapshot
//                       used for REPEATABLE READ
//  - TXN_SNAPSHOT_CHILD: means that each child tokutxn creates its own snapshot
//                        used for READ COMMITTED
//

typedef enum __TXN_SNAPSHOT_TYPE { 
    TXN_SNAPSHOT_NONE=0,
    TXN_SNAPSHOT_ROOT=1,
    TXN_SNAPSHOT_CHILD=2
} TXN_SNAPSHOT_TYPE;


#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif

#endif

