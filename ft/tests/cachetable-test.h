/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id: cachetable-checkpointer_test.cc 45903 2012-07-19 13:06:39Z leifwalsh $"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."

#include "includes.h"
#include "cachetable-internal.h"

//
// Dummy callbacks for checkpointing
//
static void dummy_log_fassociate(CACHEFILE UU(cf), void* UU(p)) { }
static void dummy_log_rollback(CACHEFILE UU(cf), void* UU(p)) { }
static void dummy_close_usr(CACHEFILE UU(cf), int UU(i), void* UU(p), bool UU(b), LSN UU(lsn))  { }
static void dummy_chckpnt_usr(CACHEFILE UU(cf), int UU(i), void* UU(p)) { }
static void dummy_begin(LSN UU(lsn), void* UU(p)) { }
static void dummy_end(CACHEFILE UU(cf), int UU(i), void* UU(p)) { }
static void dummy_note_pin(CACHEFILE UU(cf), void* UU(p)) { }
static void dummy_note_unpin(CACHEFILE UU(cf), void* UU(p)) { }

//
// Helper function to set dummy functions in given cachefile.
//
static UU() void
create_dummy_functions(CACHEFILE cf)
{
    void *ud = NULL;
    toku_cachefile_set_userdata(cf,
                               ud,
                               &dummy_log_fassociate,
                               &dummy_log_rollback,
                               &dummy_close_usr,
                               &dummy_chckpnt_usr,
                               &dummy_begin,
                               &dummy_end,
                               &dummy_note_pin,
                               &dummy_note_unpin);
};
