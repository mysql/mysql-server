/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#ifndef TOKU_RACE_TOOLS_H
#define TOKU_RACE_TOOLS_H

#include "config.h"

#if defined(__linux__) && USE_VALGRIND

# include <valgrind/helgrind.h>
# include <valgrind/drd.h>

# define TOKU_ANNOTATE_NEW_MEMORY(p, size) ANNOTATE_NEW_MEMORY(p, size)
# define TOKU_VALGRIND_HG_ENABLE_CHECKING(p, size) VALGRIND_HG_ENABLE_CHECKING(p, size)
# define TOKU_VALGRIND_HG_DISABLE_CHECKING(p, size) VALGRIND_HG_DISABLE_CHECKING(p, size)
# define TOKU_DRD_IGNORE_VAR(v) DRD_IGNORE_VAR(v)
# define TOKU_DRD_STOP_IGNORING_VAR(v) DRD_STOP_IGNORING_VAR(v)

/*
 * How to make helgrind happy about tree rotations and new mutex orderings:
 *
 * // Tell helgrind that we unlocked it so that the next call doesn't get a "destroyed a locked mutex" error.
 * // Tell helgrind that we destroyed the mutex.
 * VALGRIND_HG_MUTEX_UNLOCK_PRE(&locka);
 * VALGRIND_HG_MUTEX_DESTROY_PRE(&locka);
 *
 * // And recreate it.  It would be better to simply be able to say that the order on these two can now be reversed, because this code forgets all the ordering information for this mutex.
 * // Then tell helgrind that we have locked it again.
 * VALGRIND_HG_MUTEX_INIT_POST(&locka, 0);
 * VALGRIND_HG_MUTEX_LOCK_POST(&locka);
 *
 * When the ordering of two locks changes, we don't need tell Helgrind about do both locks.  Just one is good enough.
 */

# define TOKU_VALGRIND_RESET_MUTEX_ORDERING_INFO(mutex)  \
    VALGRIND_HG_MUTEX_UNLOCK_PRE(mutex); \
    VALGRIND_HG_MUTEX_DESTROY_PRE(mutex); \
    VALGRIND_HG_MUTEX_INIT_POST(mutex, 0); \
    VALGRIND_HG_MUTEX_LOCK_POST(mutex);

#else // !defined(__linux__) || !USE_VALGRIND

# define NVALGRIND 1
# define TOKU_ANNOTATE_NEW_MEMORY(p, size) ((void) 0)
# define TOKU_VALGRIND_HG_ENABLE_CHECKING(p, size) ((void) 0)
# define TOKU_VALGRIND_HG_DISABLE_CHECKING(p, size) ((void) 0)
# define TOKU_DRD_IGNORE_VAR(v)
# define TOKU_DRD_STOP_IGNORING_VAR(v)
# define TOKU_VALGRIND_RESET_MUTEX_ORDERING_INFO(mutex)

#endif

#endif // TOKU_RACE_TOOLS_H
