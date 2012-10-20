/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "rollback-ct-callbacks.h"

#include <toku_portability.h>
#include <memory.h>
#include "ft-internal.h"
#include "fttypes.h"
#include "memarena.h"
#include "rollback.h"


// Address used as a sentinel. Otherwise unused.
static struct serialized_rollback_log_node cloned_rollback;

// Cleanup the rollback memory
static void
rollback_log_destroy(ROLLBACK_LOG_NODE log) {
    make_rollback_log_empty(log);
    toku_free(log);
}

// flush an ununused log to disk, by allocating a size 0 blocknum in
// the blocktable
static void
toku_rollback_flush_unused_log(
    ROLLBACK_LOG_NODE log,
    BLOCKNUM logname,
    int fd,
    FT ft,
    bool write_me,
    bool keep_me,
    bool for_checkpoint,
    bool is_clone
    )
{
    if (write_me) {
        DISKOFF offset;
        toku_blocknum_realloc_on_disk(ft->blocktable, logname, 0, &offset,
                                      ft, fd, for_checkpoint);
    }
    if (!keep_me && !is_clone) {
        toku_free(log);
    }
}

// flush a used log to disk by serializing and writing the node out
static void
toku_rollback_flush_used_log (
    ROLLBACK_LOG_NODE log,
    SERIALIZED_ROLLBACK_LOG_NODE serialized,
    int fd,
    FT ft,
    bool write_me,
    bool keep_me,
    bool for_checkpoint,
    bool is_clone
    )
{

    if (write_me) {
        int r = toku_serialize_rollback_log_to(fd, log, serialized, is_clone, ft, for_checkpoint);
        assert(r == 0);
    }
    if (!keep_me) {
        if (is_clone) {
            toku_serialized_rollback_log_destroy(serialized);
        }
        else {
            rollback_log_destroy(log);
        }
    }
}

// Write something out.  Keep trying even if partial writes occur.
// On error: Return negative with errno set.
// On success return nbytes.
void toku_rollback_flush_callback (
    CACHEFILE UU(cachefile),
    int fd,
    BLOCKNUM logname,
    void *rollback_v,
    void** UU(disk_data),
    void *extraargs,
    PAIR_ATTR size,
    PAIR_ATTR* new_size,
    bool write_me,
    bool keep_me,
    bool for_checkpoint,
    bool is_clone
    )
{
    ROLLBACK_LOG_NODE log = nullptr;
    SERIALIZED_ROLLBACK_LOG_NODE serialized = nullptr;
    bool is_unused = false;
    if (is_clone) {
        is_unused = (rollback_v == &cloned_rollback);
        CAST_FROM_VOIDP(serialized, rollback_v);
    }
    else {
        CAST_FROM_VOIDP(log, rollback_v);
        is_unused = rollback_log_is_unused(log);
    }
    *new_size = size;
    FT ft;
    CAST_FROM_VOIDP(ft, extraargs);
    if (is_unused) {
        toku_rollback_flush_unused_log(
            log,
            logname,
            fd,
            ft,
            write_me,
            keep_me,
            for_checkpoint,
            is_clone
            );
    }
    else {
        toku_rollback_flush_used_log(
            log,
            serialized,
            fd,
            ft,
            write_me,
            keep_me,
            for_checkpoint,
            is_clone
            );
    }
}

int toku_rollback_fetch_callback (CACHEFILE cachefile, PAIR p, int fd, BLOCKNUM logname, uint32_t fullhash,
                                 void **rollback_pv,  void** UU(disk_data), PAIR_ATTR *sizep, int * UU(dirtyp), void *extraargs) {
    int r;
    FT CAST_FROM_VOIDP(h, extraargs);
    assert(h->cf == cachefile);
    ROLLBACK_LOG_NODE *result = (ROLLBACK_LOG_NODE*)rollback_pv;
    r = toku_deserialize_rollback_log_from(fd, logname, fullhash, result, h);
    if (r==0) {
        (*result)->ct_pair = p;
        *sizep = rollback_memory_size(*result);
    }
    return r;
}

void toku_rollback_pe_est_callback(
    void* rollback_v, 
    void* UU(disk_data),
    long* bytes_freed_estimate, 
    enum partial_eviction_cost *cost, 
    void* UU(write_extraargs)
    )
{
    assert(rollback_v != NULL);
    *bytes_freed_estimate = 0;
    *cost = PE_CHEAP;
}

// callback for partially evicting a cachetable entry
int toku_rollback_pe_callback (
    void *rollback_v, 
    PAIR_ATTR UU(old_attr), 
    PAIR_ATTR* new_attr, 
    void* UU(extraargs)
    ) 
{
    assert(rollback_v != NULL);
    *new_attr = old_attr;
    return 0;
}

// partial fetch is never required for a rollback log node
bool toku_rollback_pf_req_callback(void* UU(ftnode_pv), void* UU(read_extraargs)) {
    return false;
}

// a rollback node should never be partial fetched, 
// because we always say it is not required.
// (pf req callback always returns false)
int toku_rollback_pf_callback(void* UU(ftnode_pv),  void* UU(disk_data), void* UU(read_extraargs), int UU(fd), PAIR_ATTR* UU(sizep)) {
    assert(false);
    return 0;
}

// the cleaner thread should never choose a rollback node for cleaning
int toku_rollback_cleaner_callback (
    void* UU(ftnode_pv),
    BLOCKNUM UU(blocknum),
    uint32_t UU(fullhash),
    void* UU(extraargs)
    )
{
    assert(false);
    return 0;
}

void toku_rollback_clone_callback(
    void* value_data,
    void** cloned_value_data,
    PAIR_ATTR* new_attr,
    bool UU(for_checkpoint),
    void* UU(write_extraargs)
    )
{
    ROLLBACK_LOG_NODE CAST_FROM_VOIDP(log, value_data);
    SERIALIZED_ROLLBACK_LOG_NODE serialized = nullptr;
    if (!rollback_log_is_unused(log)) {
        XMALLOC(serialized);
        toku_serialize_rollback_log_to_memory_uncompressed(log, serialized);
        *cloned_value_data = serialized;
    }
    else {
        *cloned_value_data = &cloned_rollback;
    }
    new_attr->is_valid = false;
}

