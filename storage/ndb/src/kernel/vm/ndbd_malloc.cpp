/*
   Copyright (c) 2005, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "ndbd_malloc.hpp"
#include <NdbThread.h>
#include <ndb_global.h>
#include <NdbOut.hpp>
#include <algorithm>
#include "debugger/EventLogger.hpp"
#include "my_sys.h"
#include "portlib/NdbMem.h"
#include "util/require.h"

// #define TRACE_MALLOC
#ifdef TRACE_MALLOC
#include <stdio.h>
#endif

#include "memory_debugging.h"

#define JAM_FILE_ID 287

#define TOUCH_PARALLELISM 8
#define MIN_START_THREAD_SIZE (128 * 1024 * 1024)
#define NUM_PAGES_BETWEEN_WATCHDOG_SETS 32768

struct AllocTouchMem {
  volatile Uint32 *watchCounter;
  size_t sz;
  void *p;
  Uint32 index;
  bool make_readwritable;
};

// Enable/disable debug check for reads from uninitialized memory.
#if defined(VM_TRACE) || defined(ERROR_INSERT)
const bool debugUinitMemUse = true;
#else
const bool debugUinitMemUse = false;
#endif

static void *touch_mem(void *arg) {
  struct AllocTouchMem *touch_mem_ptr = (struct AllocTouchMem *)arg;

#if defined(VM_TRACE_MEM)
  g_eventLogger->info(
      "Touching memory: %zu bytes at %p, thread index %u, "
      "watch dog %p",
      touch_mem_ptr->sz, touch_mem_ptr->p, touch_mem_ptr->index,
      touch_mem_ptr->watchCounter);
#endif

  size_t sz = touch_mem_ptr->sz;
  Uint32 index = touch_mem_ptr->index;
  bool make_readwritable = touch_mem_ptr->make_readwritable;
  unsigned char *p = (unsigned char *)touch_mem_ptr->p;
  size_t num_pages_per_thread = 1;
  size_t first_page;

  const size_t TOUCH_PAGE_SIZE = NdbMem_GetSystemPageSize();
  size_t tot_pages = (sz + (TOUCH_PAGE_SIZE - 1)) / TOUCH_PAGE_SIZE;

  volatile Uint32 *watchCounter = touch_mem_ptr->watchCounter;
  require(watchCounter != nullptr);

  const bool whole_pages =
      ((uintptr_t)p % TOUCH_PAGE_SIZE == 0) && (sz % TOUCH_PAGE_SIZE == 0);

  if (make_readwritable) {
    /*
     * make_readwritable must call NdbMem_PopulateSpace to change page
     * protection to read-write, and that function requires whole pages.
     *
     * This is needed when memory for example have only been reserved by
     * NdbMem_ReserveSpace.
     */
    require(whole_pages);
  }

  if (tot_pages > TOUCH_PARALLELISM) {
    num_pages_per_thread =
        ((tot_pages + (TOUCH_PARALLELISM - 1)) / TOUCH_PARALLELISM);
  }

  first_page = index * num_pages_per_thread;

  if (first_page >= tot_pages) {
    return NULL; /* We're done, no page to handle */
  } else if ((tot_pages - first_page) < num_pages_per_thread) {
    num_pages_per_thread = tot_pages - first_page;
  }

  unsigned char *ptr = (unsigned char *)(p + (first_page * TOUCH_PAGE_SIZE));
  const unsigned char *end = p + sz;

  for (Uint32 i = 0; i < num_pages_per_thread;
       i += NUM_PAGES_BETWEEN_WATCHDOG_SETS,
              ptr += NUM_PAGES_BETWEEN_WATCHDOG_SETS * TOUCH_PAGE_SIZE) {
    const size_t size =
        std::min({ptrdiff_t(end - ptr),
                  ptrdiff_t(NUM_PAGES_BETWEEN_WATCHDOG_SETS * TOUCH_PAGE_SIZE),
                  ptrdiff_t(num_pages_per_thread * TOUCH_PAGE_SIZE)});

    if (make_readwritable) {
      // Populate address space earlier Reserved.
      require(NdbMem_PopulateSpace(ptr, size) == 0);
    } else {
      for (Uint32 j = 0; j < size; j += TOUCH_PAGE_SIZE) {
        ptr[j] = 0;
      }
    }
    *watchCounter = 9;

    if (debugUinitMemUse) {
      /*
        Initialize the memory to something likely to trigger access violations
        if used as a pointer or array index, to make it easier to detect use of
        uninitialized memory. See also TRASH macro.
       */
      MEM_CHECK_ADDRESSABLE(ptr, size);
      memset(ptr, 0xfb, size);
      /*
        Mark memory as being undefined for valgrind, so that valgrind may
        know that reads from this memory is an error.
       */
      MEM_UNDEFINED(ptr, size);
      *watchCounter = 9;
    }
  }
  return NULL;
}

void ndbd_alloc_touch_mem(void *p, size_t sz, volatile Uint32 *watchCounter,
                          bool make_readwritable) {
  struct NdbThread *thread_ptr[TOUCH_PARALLELISM];
  struct AllocTouchMem touch_mem_struct[TOUCH_PARALLELISM];

  Uint32 dummy_watch_counter = 0;
  if (watchCounter == nullptr) {
    /*
     * Touching without watchdog is used by ndbd_malloc.
     *
     * We check that the amount of memory to be touched would not trigger
     * watchdog kick anyway.
     *
     */
    if (ndbd_malloc_need_watchdog(sz)) {
      g_eventLogger->warning(
          "Touching much memory, %zu bytes, without watchdog.", sz);
#if defined(VM_TRACE_MEM)
      /*
       * Assert to find big allocations not using watchdog.
       * These typically comes from allocating static arrays for some resources
       * for some configurations.
       */
      // assert(!ndbd_malloc_need_watchdog(sz));
#endif
    }
    watchCounter = &dummy_watch_counter;
  }

  for (Uint32 i = 0; i < TOUCH_PARALLELISM; i++) {
    touch_mem_struct[i].watchCounter = watchCounter;
    touch_mem_struct[i].sz = sz;
    touch_mem_struct[i].p = p;
    touch_mem_struct[i].index = i;
    touch_mem_struct[i].make_readwritable = make_readwritable;

    thread_ptr[i] = NULL;
    if (sz > MIN_START_THREAD_SIZE) {
      thread_ptr[i] =
          NdbThread_Create(touch_mem, (NDB_THREAD_ARG *)&touch_mem_struct[i], 0,
                           "touch_thread", NDB_THREAD_PRIO_MEAN);
    }
    if (thread_ptr[i] == NULL) {
      touch_mem((void *)&touch_mem_struct[i]);
    }
  }
  for (Uint32 i = 0; i < TOUCH_PARALLELISM; i++) {
    void *dummy_status;
    if (thread_ptr[i]) {
      NdbThread_WaitFor(thread_ptr[i], &dummy_status);
      NdbThread_Destroy(&thread_ptr[i]);
    }
  }
}

#ifdef TRACE_MALLOC
static void xxx(size_t size, size_t *s_m, size_t *s_k, size_t *s_b) {
  *s_m = size / 1024 / 1024;
  *s_k = (size - *s_m * 1024 * 1024) / 1024;
  *s_b = size - *s_m * 1024 * 1024 - *s_k * 1024;
}
#endif

static Uint64 g_allocated_memory;
void *ndbd_malloc_watched(size_t size, volatile Uint32 *watch_dog) {
  void *p = malloc(size);
  if (p) {
    g_allocated_memory += size;

    ndbd_alloc_touch_mem(p, size, watch_dog, false /* touch only */);

#ifdef TRACE_MALLOC
    {
      size_t s_m, s_k, s_b;
      xxx(size, &s_m, &s_k, &s_b);

      size_t s_m2, s_k2, s_b2;
      xxx(g_allocated_memory, &s_m2, &s_k2, &s_b2);
      g_eventLogger->info("%p malloc (%zum %zuk %zub) total (%zum %zuk %zub)",
                          p, s_m, s_k, s_b, s_m2, s_k2, s_b2);
    }
#endif
  }
  return p;
}

bool ndbd_malloc_need_watchdog(size_t size) {
  const size_t TOUCH_PAGE_SIZE = NdbMem_GetSystemPageSize();
  return (size >= NUM_PAGES_BETWEEN_WATCHDOG_SETS * TOUCH_PAGE_SIZE *
                      TOUCH_PARALLELISM);
}

void *ndbd_malloc(size_t size) { return ndbd_malloc_watched(size, nullptr); }

void ndbd_free(void *p, size_t size) {
  free(p);
  if (p) {
    g_allocated_memory -= size;
#ifdef TRACE_MALLOC
    g_eventLogger->info("%p free(%zu)", p, size);
#endif
  }
}
