/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef ROLLBACK_CT_CALLBACKS_H
#define ROLLBACK_CT_CALLBACKS_H

#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."


#include "cachetable.h"
#include "fttypes.h"

void toku_rollback_flush_callback(CACHEFILE cachefile, int fd, BLOCKNUM logname, void *rollback_v, void** UU(disk_data), void *extraargs, PAIR_ATTR size, PAIR_ATTR* new_size, bool write_me, bool keep_me, bool for_checkpoint, bool UU(is_clone));
int toku_rollback_fetch_callback(CACHEFILE cachefile, PAIR p, int fd, BLOCKNUM logname, uint32_t fullhash, void **rollback_pv,  void** UU(disk_data), PAIR_ATTR *sizep, int * UU(dirtyp), void *extraargs);
void toku_rollback_pe_est_callback(
    void* rollback_v, 
    void* UU(disk_data),
    long* bytes_freed_estimate, 
    enum partial_eviction_cost *cost, 
    void* UU(write_extraargs)
    );
int toku_rollback_pe_callback (
    void *rollback_v, 
    PAIR_ATTR UU(old_attr), 
    PAIR_ATTR* new_attr, 
    void* UU(extraargs)
    ) ;
bool toku_rollback_pf_req_callback(void* UU(ftnode_pv), void* UU(read_extraargs)) ;
int toku_rollback_pf_callback(void* UU(ftnode_pv),  void* UU(disk_data), void* UU(read_extraargs), int UU(fd), PAIR_ATTR* UU(sizep));
void toku_rollback_clone_callback(void* value_data, void** cloned_value_data, long* clone_size, PAIR_ATTR* new_attr, bool for_checkpoint, void* write_extraargs);

int toku_rollback_cleaner_callback (
    void* UU(ftnode_pv),
    BLOCKNUM UU(blocknum),
    uint32_t UU(fullhash),
    void* UU(extraargs)
    );

static inline CACHETABLE_WRITE_CALLBACK get_write_callbacks_for_rollback_log(FT h) {
    CACHETABLE_WRITE_CALLBACK wc;
    wc.flush_callback = toku_rollback_flush_callback;
    wc.pe_est_callback = toku_rollback_pe_est_callback;
    wc.pe_callback = toku_rollback_pe_callback;
    wc.cleaner_callback = toku_rollback_cleaner_callback;
    wc.clone_callback = toku_rollback_clone_callback;
    wc.checkpoint_complete_callback = nullptr;
    wc.write_extraargs = h;
    return wc;
}


#endif // ROLLBACK_CT_CALLBACKS_H
