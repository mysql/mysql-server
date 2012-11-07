/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef FTTYPES_H
#define FTTYPES_H

#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <sys/types.h>
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif
#define _FILE_OFFSET_BITS 64

#include "toku_assert.h"
#include <db.h>
#include <inttypes.h>


// Use the C++ bool and constants (true false), rather than BOOL, TRUE, and FALSE.

typedef struct ft_handle *FT_HANDLE;
typedef struct ftnode *FTNODE;
typedef struct ftnode_disk_data *FTNODE_DISK_DATA;
typedef struct ftnode_leaf_basement_node *BASEMENTNODE;
typedef struct ftnode_nonleaf_childinfo *NONLEAF_CHILDINFO;
typedef struct sub_block *SUB_BLOCK;
typedef struct ft *FT;
typedef struct ft_header *FT_HEADER;
typedef struct ft_options *FT_OPTIONS;

struct wbuf;
struct dbuf;

typedef unsigned int ITEMLEN;
typedef const void *bytevec;

typedef int64_t DISKOFF;  /* Offset in a disk. -1 is the NULL pointer. */
typedef uint64_t TXNID;
#define TXNID_NONE_LIVING ((TXNID)0)
#define TXNID_NONE        ((TXNID)0)

typedef struct blocknum_s { int64_t b; } BLOCKNUM; // make a struct so that we will notice type problems.
typedef struct gid_s { uint8_t *gid; } GID; // the gid is of size [DB_GID_SIZE]
typedef TOKU_XA_XID *XIDP; // this is the type that's passed to the logger code (so that we don't have to copy all 152 bytes when only a subset are even valid.)
#define ROLLBACK_NONE     ((BLOCKNUM){0})

static inline BLOCKNUM make_blocknum(int64_t b) { BLOCKNUM result={b}; return result; }

// This struct hold information about values stored in the cachetable.
// As one can tell from the names, we are probably violating an
// abstraction layer by placing names.
//
// The purpose of having this struct is to have a way for the 
// cachetable to accumulate the some totals we are interested in.
// Breaking this abstraction layer by having these names was the 
// easiest way.
//
typedef struct pair_attr_s {
    long size; // size PAIR's value takes in memory
    long nonleaf_size; // size if PAIR is a nonleaf node, 0 otherwise, used only for engine status
    long leaf_size; // size if PAIR is a leaf node, 0 otherwise, used only for engine status
    long rollback_size; // size of PAIR is a rollback node, 0 otherwise, used only for engine status
    long cache_pressure_size; // amount PAIR contributes to cache pressure, is sum of buffer sizes and workdone counts
    bool is_valid;
} PAIR_ATTR;

static inline PAIR_ATTR make_pair_attr(long size) { 
#if 1 || (!defined(__cplusplus) && !defined(__cilkplusplus))
    PAIR_ATTR result={
        .size = size, 
        .nonleaf_size = 0, 
        .leaf_size = 0, 
        .rollback_size = 0, 
        .cache_pressure_size = 0,
        .is_valid = true
    }; 
#else
    PAIR_ATTR result = {size, 0, 0, 0, 0, true};
#endif
    return result; 
}

typedef struct {
    uint32_t len;
    char *data;
} BYTESTRING;

/* Log Sequence Number (LSN)
 * Make the LSN be a struct instead of an integer so that we get better type checking. */
typedef struct __toku_lsn { uint64_t lsn; } LSN;
#define ZERO_LSN ((LSN){0})
#define MAX_LSN  ((LSN){UINT64_MAX})

/* Message Sequence Number (MSN)
 * Make the MSN be a struct instead of an integer so that we get better type checking. */
typedef struct __toku_msn { uint64_t msn; } MSN;
#define ZERO_MSN ((MSN){0})                 // dummy used for message construction, to be filled in when msg is applied to tree
#define MIN_MSN  ((MSN){(uint64_t)1 << 62})  // first 2^62 values reserved for messages created before Dr. No (for upgrade)
#define MAX_MSN  ((MSN){UINT64_MAX})

typedef struct {
    int64_t numrows;           // delta versions in basements could be negative
    int64_t numbytes;
} STAT64INFO_S, *STAT64INFO;

static const STAT64INFO_S ZEROSTATS = {0,0};

/* At the brt layer, a FILENUM uniquely identifies an open file.
 * At the ydb layer, a DICTIONARY_ID uniquely identifies an open dictionary.
 * With the introduction of the loader (ticket 2216), it is possible for the file that holds
 * an open dictionary to change, so these are now separate and independent unique identifiers.
 */
typedef struct {uint32_t fileid;} FILENUM;
#define FILENUM_NONE ((FILENUM){UINT32_MAX})

typedef struct {uint64_t dictid;} DICTIONARY_ID;
#define DICTIONARY_ID_NONE ((DICTIONARY_ID){0})

typedef struct {
    uint32_t num;
    FILENUM  *filenums;
} FILENUMS;

typedef struct tokulogger *TOKULOGGER;
typedef struct txn_manager *TXN_MANAGER;
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
typedef struct ctpair *PAIR;
typedef class checkpointer *CHECKPOINTER;

/* tree command types */
enum ft_msg_type {
    FT_NONE = 0,
    FT_INSERT = 1,
    FT_DELETE_ANY = 2,  // Delete any matching key.  This used to be called FT_DELETE.
    //FT_DELETE_BOTH = 3,
    FT_ABORT_ANY = 4,   // Abort any commands on any matching key.
    //FT_ABORT_BOTH  = 5, // Abort commands that match both the key and the value
    FT_COMMIT_ANY  = 6,
    //FT_COMMIT_BOTH = 7,
    FT_COMMIT_BROADCAST_ALL = 8, // Broadcast to all leafentries, (commit all transactions).
    FT_COMMIT_BROADCAST_TXN = 9, // Broadcast to all leafentries, (commit specific transaction).
    FT_ABORT_BROADCAST_TXN  = 10, // Broadcast to all leafentries, (commit specific transaction).
    FT_INSERT_NO_OVERWRITE = 11,
    FT_OPTIMIZE = 12,             // Broadcast
    FT_OPTIMIZE_FOR_UPGRADE = 13, // same as FT_OPTIMIZE, but record version number in leafnode
    FT_UPDATE = 14,
    FT_UPDATE_BROADCAST_ALL = 15
};

static inline bool
ft_msg_type_applies_once(enum ft_msg_type type)
{
    bool ret_val;
    switch (type) {
    case FT_INSERT_NO_OVERWRITE:
    case FT_INSERT:
    case FT_DELETE_ANY:
    case FT_ABORT_ANY:
    case FT_COMMIT_ANY:
    case FT_UPDATE:
        ret_val = true;
        break;
    case FT_COMMIT_BROADCAST_ALL:
    case FT_COMMIT_BROADCAST_TXN:
    case FT_ABORT_BROADCAST_TXN:
    case FT_OPTIMIZE:
    case FT_OPTIMIZE_FOR_UPGRADE:
    case FT_UPDATE_BROADCAST_ALL:
    case FT_NONE:
        ret_val = false;
        break;
    default:
        assert(false);
    }
    return ret_val;
}

static inline bool
ft_msg_type_applies_all(enum ft_msg_type type)
{
    bool ret_val;
    switch (type) {
    case FT_NONE:
    case FT_INSERT_NO_OVERWRITE:
    case FT_INSERT:
    case FT_DELETE_ANY:
    case FT_ABORT_ANY:
    case FT_COMMIT_ANY:
    case FT_UPDATE:
        ret_val = false;
        break;
    case FT_COMMIT_BROADCAST_ALL:
    case FT_COMMIT_BROADCAST_TXN:
    case FT_ABORT_BROADCAST_TXN:
    case FT_OPTIMIZE:
    case FT_OPTIMIZE_FOR_UPGRADE:
    case FT_UPDATE_BROADCAST_ALL:
        ret_val = true;
        break;
    default:
        assert(false);
    }
    return ret_val;
}

static inline bool
ft_msg_type_does_nothing(enum ft_msg_type type)
{
    return (type == FT_NONE);
}

typedef struct xids_t *XIDS;
typedef struct fifo_msg_t *FIFO_MSG;
/* tree commands */
struct ft_msg {
    enum ft_msg_type type;
    MSN          msn;          // message sequence number
    XIDS         xids;
    union {
        /* insert or delete */
        struct ft_cmd_insert_delete {
            const DBT *key;   // for insert, delete, upsertdel
            const DBT *val;   // for insert, delete, (and it is the "extra" for upsertdel, upsertdel_broadcast_all)
        } id;
    } u;
};
// Message sent into brt to implement command (insert, delete, etc.)
// This structure supports nested transactions, and obsoletes ft_msg.
typedef struct ft_msg FT_MSG_S;
typedef const struct ft_msg *FT_MSG;

typedef int (*ft_compare_func)(DB *, const DBT *, const DBT *);
typedef void (*setval_func)(const DBT *, void *);
typedef int (*ft_update_func)(DB *, const DBT *, const DBT *, const DBT *, setval_func, void *);
typedef void (*on_redirect_callback)(FT_HANDLE, void*);
typedef void (*remove_ft_ref_callback)(FT, void*);

#define UU(x) x __attribute__((__unused__))

typedef struct memarena *MEMARENA;
typedef struct rollback_log_node *ROLLBACK_LOG_NODE;
typedef struct serialized_rollback_log_node *SERIALIZED_ROLLBACK_LOG_NODE;

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

typedef struct ancestors *ANCESTORS;
typedef struct pivot_bounds const * const PIVOT_BOUNDS;
typedef struct ftnode_fetch_extra *FTNODE_FETCH_EXTRA;
typedef struct unlockers *UNLOCKERS;

enum reactivity {
    RE_STABLE,
    RE_FUSIBLE,
    RE_FISSIBLE
};

enum split_mode {
    SPLIT_EVENLY,
    SPLIT_LEFT_HEAVY,
    SPLIT_RIGHT_HEAVY
};

#endif
