#ifndef SYSINCLUDES_H
#define SYSINCLUDES_H

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif
#define _FILE_OFFSET_BITS 64

// Portability first!
#include "stdint.h"
#include "toku_portability.h"
#include "toku_os.h"

#if TOKU_WINDOWS
#include "zlib.h"
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
#include <zlib.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "block_allocator.h"
#include "brt.h"
#include "brt-internal.h"
#include "cachetable.h"
#include "rwlock.h"
#include "fifo.h"
#include "list.h"
#include "key.h"
#include "kv-pair.h"
#include "leafentry.h"
#include "log-internal.h"
#include "log_header.h"
#include "mempool.h"
#include "rbuf.h"
#include "threadpool.h"
#include "toku_assert.h"
#include "wbuf.h"

#include "../include/db.h"

#endif
