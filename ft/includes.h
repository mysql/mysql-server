/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef SYSINCLUDES_H
#define SYSINCLUDES_H

#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <config.h>

// Portability first!
#include <toku_portability.h>

#if TOKU_WINDOWS
#include "toku_pthread.h"
#include <dirent.h>
#else
#include <dirent.h>
#include <inttypes.h>
#include <toku_pthread.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include <ctype.h>
#include <errno.h>
#if defined(HAVE_MALLOC_H)
# include <malloc.h>
#elif defined(HAVE_SYS_MALLOC_H)
# include <sys/malloc.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "block_allocator.h"
#include "ft-ops.h"
#include "ft.h"
#include "ft-internal.h"
#include "cachetable.h"
#include "rwlock.h"
#include "fifo.h"
#include "toku_list.h"
#include "key.h"
#include "leafentry.h"
#include "log-internal.h"
#include <ft/log_header.h>
#include "logcursor.h"
#include "logfilemgr.h"
#include "rbuf.h"
#include "threadpool.h"
#include "toku_assert.h"
#include "wbuf.h"

#include <db.h>
#include "tokuconst.h"

#endif
