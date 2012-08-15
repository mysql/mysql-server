/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id: cachetable-checkpointer_test.cc 45903 2012-07-19 13:06:39Z leifwalsh $"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."

#include "includes.h"
#include "test.h"
#include "cachetable-internal.h"

//
// Wrapper for the checkpointer and necessary
// data to run the tests.
//
struct checkpointer_test {
  checkpointer m_cp;
  pair_list m_pl;
  void test_begin_checkpoint();
  void test_pending_bits();
  void test_update_cachefiles();
  void test_end_checkpoint();
};

//
// Dummy callbacks for checkpointing
//
static int dummy_log_fassociate(CACHEFILE UU(cf), void* UU(p))
{ return 0; }
static int dummy_log_rollback(CACHEFILE UU(cf), void* UU(p))
{ return 0; }
static int dummy_close_usr(CACHEFILE UU(cf), int UU(i), void* UU(p), char** UU(c), bool UU(b), LSN UU(lsn)) 
{ return 0; }
static int dummy_chckpnt_usr(CACHEFILE UU(cf), int UU(i), void* UU(p))
{ return 0; }
static int dummy_begin(LSN UU(lsn), void* UU(p)) 
{ return 0; }
static int dummy_end(CACHEFILE UU(cf), int UU(i), void* UU(p)) 
{ return 0; }
static int dummy_note_pin(CACHEFILE UU(cf), void* UU(p)) 
{ return 0; }
static int dummy_note_unpin(CACHEFILE UU(cf), void* UU(p))
{ return 0; }


//
// Helper function to set dummy functions in given cachefile.
//
static void
create_dummy_functions(CACHEFILE cf)
{
    void *ud = NULL;
    toku_cachefile_set_userdata (cf,
                                 ud,
                                 &dummy_log_fassociate,
                                 &dummy_log_rollback,
                                 &dummy_close_usr,
                                 &dummy_chckpnt_usr,
                                 &dummy_begin,
                                 &dummy_end,
                                 &dummy_note_pin,
                                 &dummy_note_unpin);
}


//------------------------------------------------------------------------------
// test_begin_checkpoint() -
//
// Description:
//
void checkpointer_test::test_begin_checkpoint() {
    int r = 0;
    
    cachefile_list cfl;
    cfl.init();
    
    cachetable ctbl;
    ctbl.list.init();
    
    m_cp.init(&ctbl, NULL, &cfl);

    // 1. Call checkpoint with NO cachefiles.
    r = m_cp.begin_checkpoint();
    if (r) { assert(!"CHECKPOINTER: Checkpoint with no cachefiles failed!\n"); }
    
    // 2. Call checkpoint with ONE cachefile.
    //cachefile cf;
    struct cachefile cf;
    cf.next = NULL;
    cf.for_checkpoint = false;
    m_cp.m_cf_list->m_head = &cf;
    create_dummy_functions(&cf);
    
    r = m_cp.begin_checkpoint();
    if (r) { assert(!"CHECKPOINTER: Checkpoint with one cachefile failed!\n"); }
    assert(m_cp.m_checkpoint_num_files == 1);
    assert(cf.for_checkpoint == true);

    // 3. Call checkpoint with MANY cachefiles.
    const uint32_t count = 3;
    struct cachefile cfs[count];
    m_cp.m_cf_list->m_head = &cfs[0];
    for (uint32_t i = 0; i < count; ++i) {
        cfs[i].for_checkpoint = false;
        create_dummy_functions(&cfs[i]);
        if (i == count - 1) {
            cfs[i].next = NULL;
        } else {
            cfs[i].next = &cfs[i + 1];
        }
    }
    
    r = m_cp.begin_checkpoint();
    if (r) { assert(!"CHECKPOINTER: Multiple checkpoint failed!\n"); }
    assert(m_cp.m_checkpoint_num_files == count);
    for (uint32_t i = 0; i < count; ++i) {
        assert(cfs[i].for_checkpoint == true);
    }
    ctbl.list.destroy();
    m_cp.destroy();
}

//------------------------------------------------------------------------------
// test_pending_bits() -
//
// Description:
//
void checkpointer_test::test_pending_bits() {
    cachefile_list cfl;
    cfl.init();
    
    cachetable ctbl;
    ctbl.list.init();
    m_cp.m_ct = &ctbl;
    
    //
    // 1. Empty hash chain.
    //
    m_cp.turn_on_pending_bits();
    
    //
    // 2. One entry in pair chain
    //
    struct cachefile cf;
    memset(&cf, 0, sizeof(cf));
    cf.next = NULL;
    cf.for_checkpoint = true;
    m_cp.m_cf_list->m_head = &cf;
    create_dummy_functions(&cf);
    
    CACHEKEY k;
    k.b = 0;
    uint32_t hash = toku_cachetable_hash(&cf, k);
    
    ctpair p;
    CACHETABLE_WRITE_CALLBACK cb;
    
    pair_attr_s attr;
    attr.size = 0;
    attr.nonleaf_size = 0;
    attr.leaf_size = 0;
    attr.rollback_size = 0;
    attr.cache_pressure_size = 0;
    attr.is_valid = true;
    
    pair_init(&p, 
        &cf, 
        k, 
        NULL, 
        attr, 
        CACHETABLE_CLEAN,
        hash,
        cb, 
        NULL, 
        &ctbl.list);

    m_cp.m_ct->list.put(&p);
    
    m_cp.turn_on_pending_bits();
    assert(p.checkpoint_pending);
    m_cp.m_ct->list.evict(&p);
    
    //
    // 3. Many hash chain entries.
    //
    const uint32_t count = 3;
    ctpair pairs[count];
    for (uint32_t i = 0; i < count; ++i) {
        CACHEKEY key;
        key.b = i;
        uint32_t full_hash = toku_cachetable_hash(&cf, key);
        pair_init(&(pairs[i]),
            &cf,
            key,
            NULL,
            attr,
            CACHETABLE_CLEAN,
            full_hash,
            cb,
            NULL,
            &ctbl.list);

        m_cp.m_ct->list.put(&pairs[i]);
    }
    
    m_cp.turn_on_pending_bits();
    
    for (uint32_t i = 0; i < count; ++i) {
        assert(pairs[i].checkpoint_pending);
    }
    for (uint32_t i = 0; i < count; ++i) {
        CACHEKEY key;
        key.b = i;
        uint32_t full_hash = toku_cachetable_hash(&cf, key);
        PAIR pp = m_cp.m_ct->list.find_pair(&cf, key, full_hash);
        assert(pp);
        m_cp.m_ct->list.evict(pp);
    }
    int r = ctbl.list.destroy();
    assert_zero(r);
}

//------------------------------------------------------------------------------
// test_update_cachefiles() -
//
// Description:
//
void checkpointer_test::test_update_cachefiles() {

}

//------------------------------------------------------------------------------
// test_end_checkpoint() -
//
// Description:
//
void checkpointer_test::test_end_checkpoint() {
    
    /************
    -add data
    -call begin checkpoint
    -add data
    -call end checkpoint
    -verify that 2nd added data NOT checkpointed
    -verify that 1st added data WAS checkpointed
    *************/
}


//------------------------------------------------------------------------------
// test_main() -
//
// Description:
//
int
test_main(int argc, const char *argv[]) {
    int r = 0;
    default_parse_args(argc, argv);
    checkpointer_test cp_test;
    
    // Run the tests.
    cp_test.test_begin_checkpoint();
    cp_test.test_pending_bits();
    cp_test.test_update_cachefiles();
    
    return r;
}


