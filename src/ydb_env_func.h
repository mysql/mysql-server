/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
// This file defines the public interface to the ydb library

#if !defined(TOKU_YDB_ENV_FUNC_H)
#define TOKU_YDB_ENV_FUNC_H

extern void (*checkpoint_callback_f)(void*);
extern void * checkpoint_callback_extra;
extern void (*checkpoint_callback2_f)(void*);
extern void * checkpoint_callback2_extra;

extern uint32_t engine_status_enable;

// Called to use dlmalloc functions.
void setup_dlmalloc(void) __attribute__((__visibility__("default")));

// Test-only function
void toku_env_increase_last_xid(DB_ENV *env, uint64_t increment);

#endif
