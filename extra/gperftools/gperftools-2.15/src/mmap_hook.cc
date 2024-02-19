/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*-
 * Copyright (c) 2023, gperftools Contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <config.h>

#include "mmap_hook.h"

#include "base/spinlock.h"
#include "base/logging.h"

#include <atomic>

#if HAVE_SYS_SYSCALL_H
#include <sys/syscall.h>
#endif

// Disable the glibc prototype of mremap(), as older versions of the
// system headers define this function with only four arguments,
// whereas newer versions allow an optional fifth argument:
#ifdef HAVE_MMAP
# define mremap glibc_mremap
# include <sys/mman.h>
# ifndef MAP_ANONYMOUS
#  define MAP_ANONYMOUS MAP_ANON
# endif
#include <sys/types.h>
# undef mremap
#endif

// __THROW is defined in glibc systems.  It means, counter-intuitively,
// "This function will never throw an exception."  It's an optional
// optimization tool, but we may need to use it to match glibc prototypes.
#ifndef __THROW    // I guess we're not on a glibc system
# define __THROW   // __THROW is just an optimization, so ok to make it ""
#endif

// Used in initial hooks to call into heap checker
// initialization. Defined empty and weak inside malloc_hooks and
// proper definition is in heap_checker.cc
extern "C" int MallocHook_InitAtFirstAllocation_HeapLeakChecker();

namespace tcmalloc {

namespace {

struct MappingHookDescriptor {
  MappingHookDescriptor(MMapEventFn fn) : fn(fn) {}

  const MMapEventFn fn;

  std::atomic<bool> inactive{false};
  std::atomic<MappingHookDescriptor*> next;
};

static_assert(sizeof(MappingHookDescriptor) ==
              (sizeof(MappingHookSpace) - offsetof(MappingHookSpace, storage)), "");
static_assert(alignof(MappingHookDescriptor) == alignof(MappingHookSpace), "");

class MappingHooks {
public:
  MappingHooks(base::LinkerInitialized) {}

  static MappingHookDescriptor* SpaceToDesc(MappingHookSpace* space) {
    return reinterpret_cast<MappingHookDescriptor*>(space->storage);
  }

  void Add(MappingHookSpace *space, MMapEventFn fn) {
    MappingHookDescriptor* desc = SpaceToDesc(space);
    if (space->initialized) {
      desc->inactive.store(false);
      return;
    }

    space->initialized = true;
    new (desc) MappingHookDescriptor(fn);

    MappingHookDescriptor* next_candidate = list_head_.load(std::memory_order_relaxed);
    do {
      desc->next.store(next_candidate, std::memory_order_relaxed);
    } while (!list_head_.compare_exchange_strong(next_candidate, desc));
  }

  void Remove(MappingHookSpace* space) {
    RAW_CHECK(space->initialized, "");
    SpaceToDesc(space)->inactive.store(true);
  }

  void InvokeAll(const MappingEvent& evt) {
    if (!ran_initial_hooks_.load(std::memory_order_relaxed)) {
      bool already_ran = ran_initial_hooks_.exchange(true, std::memory_order_seq_cst);
      if (!already_ran) {
        MallocHook_InitAtFirstAllocation_HeapLeakChecker();
      }
    }

    std::atomic<MappingHookDescriptor*> *place = &list_head_;
    while (MappingHookDescriptor* desc = place->load(std::memory_order_acquire)) {
      place = &desc->next;
      if (!desc->inactive) {
        desc->fn(evt);
      }
    }
  }

  void InvokeSbrk(void* result, intptr_t increment) {
    MappingEvent evt;
    evt.is_sbrk = 1;
    if (increment > 0) {
      evt.after_address = result;
      evt.after_length = increment;
      evt.after_valid = 1;
    } else {
      intptr_t res_addr = reinterpret_cast<uintptr_t>(result);
      intptr_t new_brk = res_addr + increment;
      evt.before_address = reinterpret_cast<void*>(new_brk);
      evt.before_length = -increment;
      evt.before_valid = 1;
    }

    InvokeAll(evt);
  }

private:
  std::atomic<MappingHookDescriptor*> list_head_;
  std::atomic<bool> ran_initial_hooks_;
} mapping_hooks{base::LINKER_INITIALIZED};

}  // namespace

void HookMMapEvents(MappingHookSpace* place, MMapEventFn callback) {
  mapping_hooks.Add(place, callback);
}

void UnHookMMapEvents(MappingHookSpace* place) {
  mapping_hooks.Remove(place);
}

}  // namespace tcmalloc

#if defined(__linux__) && HAVE_SYS_SYSCALL_H
static void* do_sys_mmap(long sysnr, void* start, size_t length, int prot, int flags, int fd, long offset) {
#if defined(__s390__)
  long args[6] = {
    (long)start, (long)length,
    (long)prot, (long)flags, (long)fd, (long)offset };
  return reinterpret_cast<void*>(syscall(sysnr, args));
#else
  return reinterpret_cast<void*>(
    syscall(sysnr, reinterpret_cast<uintptr_t>(start), length, prot, flags, fd, offset));
#endif
}

static void* do_mmap(void* start, size_t length, int prot, int flags, int fd, int64_t offset) {
#ifdef SYS_mmap2
  static int pagesize = 0;
  if (!pagesize) {
    pagesize = getpagesize();
  }
  if ((offset & (pagesize - 1))) {
    errno = EINVAL;
    return MAP_FAILED;
  }
  offset /= pagesize;

#if !defined(_LP64) && !defined(__x86_64__)
  // 32-bit and not x32 (which has "honest" 64-bit syscalls args)
  uintptr_t truncated_offset = offset;
  // This checks offset being too large for page number still not
  // fitting into 32-bit pgoff argument.
  if (static_cast<int64_t>(truncated_offset) != offset) {
    errno = EINVAL;
    return MAP_FAILED;
  }
#else
  int64_t truncated_offset = offset;
#endif
  return do_sys_mmap(SYS_mmap2, start, length, prot, flags, fd, truncated_offset);
#else

  return do_sys_mmap(SYS_mmap, start, length, prot, flags, fd, offset);
#endif
}

#define DEFINED_DO_MMAP

#endif  // __linux__

// Note, we're not risking syscall-ing mmap with 64-bit off_t on
// 32-bit on BSDs.
#if defined(__FreeBSD__) && defined(_LP64) && HAVE_SYS_SYSCALL_H
static void* do_mmap(void* start, size_t length, int prot, int flags, int fd, int64_t offset) {
  // BSDs need __syscall to deal with 64-bit args
  return reinterpret_cast<void*>(__syscall(SYS_mmap, start, length, prot, flags, fd, offset));
}

#define DEFINED_DO_MMAP
#endif  // 64-bit FreeBSD

#ifdef DEFINED_DO_MMAP

static inline ATTRIBUTE_ALWAYS_INLINE
void* do_mmap_with_hooks(void* start, size_t length, int prot, int flags, int fd, int64_t offset) {
  void* result = do_mmap(start, length, prot, flags, fd, offset);
  if (result == MAP_FAILED) {
    return result;
  }

  tcmalloc::MappingEvent evt;
  evt.before_address = start;
  evt.after_address = result;
  evt.after_length = length;
  evt.after_valid = 1;
  evt.file_fd = fd;
  evt.file_off = offset;
  evt.file_valid = 1;
  evt.flags = flags;
  evt.prot = prot;

  tcmalloc::mapping_hooks.InvokeAll(evt);

  return result;
}

static int do_munmap(void* start, size_t length) {
  return syscall(SYS_munmap, start, length);
}
#endif  // DEFINED_DO_MMAP


// On systems where we know how, we override mmap/munmap/mremap/sbrk
// to provide support for calling the related hooks (in addition,
// of course, to doing what these functions normally do).

// Some Linux libcs already have "future on" by default and ship with
// native 64-bit off_t-s. One example being musl. We cannot rule out
// glibc changing defaults in future, somehow, or people introducing
// more 32-bit systems with 64-bit off_t (x32 already being one). So
// we check for the case of 32-bit system that has wide off_t.
//
// Note, it would be nice to test some define that is available
// everywhere when off_t is 64-bit, but sadly stuff isn't always
// consistent. So we detect 32-bit system that doesn't have
// _POSIX_V7_ILP32_OFF32 set to 1, which looks less robust than we'd
// like. But from some tests and code inspection this check seems to
// cover glibc, musl, uclibc and bionic.
#if defined(__linux__) && (defined(_LP64) || (!defined(_POSIX_V7_ILP32_OFF32) || _POSIX_V7_ILP32_OFF32 < 0))
#define GOOD_LINUX_SYSTEM 1
#else
#define GOOD_LINUX_SYSTEM 0
#endif

#if defined(DEFINED_DO_MMAP) && (!defined(__linux__) || GOOD_LINUX_SYSTEM)
// Simple case for 64-bit kernels or 32-bit systems that have native
// 64-bit off_t. On all those systems there are no off_t complications
static_assert(sizeof(int64_t) == sizeof(off_t), "");

// We still export mmap64 just in case. Linux libcs tend to have it. But since off_t is 64-bit they're identical
// Also, we can safely assume gcc-like compiler and elf.

#undef mmap64
#undef mmap

extern "C" void* mmap64(void* start, size_t length, int prot, int flags, int fd, off_t off)
  __THROW ATTRIBUTE_SECTION(malloc_hook);
extern "C" void* mmap(void* start, size_t length, int prot, int flags, int fd, off_t off)
  __THROW ATTRIBUTE_SECTION(malloc_hook);

void* mmap64(void* start, size_t length, int prot, int flags, int fd, off_t off) __THROW {
  return do_mmap_with_hooks(start, length, prot, flags, fd, off);
}
void* mmap(void* start, size_t length, int prot, int flags, int fd, off_t off) __THROW {
  return do_mmap_with_hooks(start, length, prot, flags, fd, off);
}

#define HOOKED_MMAP

#elif defined(DEFINED_DO_MMAP) && defined(__linux__) && !GOOD_LINUX_SYSTEM
// Linuxes with 32-bit off_t. We're being careful with mmap64 being
// 64-bit and mmap being 32-bit.

static_assert(sizeof(int32_t) == sizeof(off_t), "");

extern "C" void* mmap64(void* start, size_t length, int prot, int flags, int fd, int64_t off)
  __THROW ATTRIBUTE_SECTION(malloc_hook);
extern "C" void* mmap(void* start, size_t length, int prot, int flags, int fd, off_t off)
  __THROW ATTRIBUTE_SECTION(malloc_hook);

void* mmap(void *start, size_t length, int prot, int flags, int fd, off_t off) __THROW {
  return do_mmap_with_hooks(start, length, prot, flags, fd, off);
}

void* mmap64(void *start, size_t length, int prot, int flags, int fd, int64_t off) __THROW {
  return do_mmap_with_hooks(start, length, prot, flags, fd, off);
}

#define HOOKED_MMAP

#endif  // Linux/32-bit off_t case


#ifdef HOOKED_MMAP

extern "C" int munmap(void* start, size_t length) __THROW ATTRIBUTE_SECTION(malloc_hook);
int munmap(void* start, size_t length) __THROW {
  int result = tcmalloc::DirectMUnMap(/* invoke_hooks=*/ false, start, length);
  if (result < 0) {
    return result;
  }

  tcmalloc::MappingEvent evt;
  evt.before_address = start;
  evt.before_length = length;
  evt.before_valid = 1;

  tcmalloc::mapping_hooks.InvokeAll(evt);

  return result;
}
#else // !HOOKED_MMAP
// No mmap/munmap interceptions. But we still provide (internal) DirectXYZ APIs.
#define do_mmap mmap
#define do_munmap munmap
#endif

tcmalloc::DirectAnonMMapResult tcmalloc::DirectAnonMMap(bool invoke_hooks, size_t length) {
  tcmalloc::DirectAnonMMapResult result;
  if (invoke_hooks) {
    result.addr = mmap(nullptr, length, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
  } else {
    result.addr = do_mmap(nullptr, length, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
  }
  result.success = (result.addr != MAP_FAILED);
  return result;
}

int tcmalloc::DirectMUnMap(bool invoke_hooks, void *start, size_t length) {
  if (invoke_hooks) {
    return munmap(start, length);
  }

  return do_munmap(start, length);
}

#if __linux__
extern "C" void* mremap(void* old_addr, size_t old_size, size_t new_size,
                        int flags, ...) __THROW ATTRIBUTE_SECTION(malloc_hook);
// We only handle mremap on Linux so far.
void* mremap(void* old_addr, size_t old_size, size_t new_size,
             int flags, ...) __THROW {
  va_list ap;
  va_start(ap, flags);
  void *new_address = va_arg(ap, void *);
  va_end(ap);
  void* result = (void*)syscall(SYS_mremap, old_addr, old_size, new_size, flags,
                                new_address);

  if (result != MAP_FAILED) {
    tcmalloc::MappingEvent evt;
    evt.before_address = old_addr;
    evt.before_length = old_size;
    evt.before_valid = 1;
    evt.after_address = result;
    evt.after_length = new_size;
    evt.after_valid = 1;
    evt.flags = flags;

    tcmalloc::mapping_hooks.InvokeAll(evt);
  }

  return result;
}

#endif

#if defined(__linux__) && HAVE___SBRK
// glibc's version:
extern "C" void* __sbrk(intptr_t increment);

extern "C" void* sbrk(intptr_t increment) __THROW ATTRIBUTE_SECTION(malloc_hook);

void* sbrk(intptr_t increment) __THROW {
  void *result = __sbrk(increment);
  if (increment == 0 || result == reinterpret_cast<void*>(static_cast<intptr_t>(-1))) {
    return result;
  }

  tcmalloc::mapping_hooks.InvokeSbrk(result, increment);

  return result;
}

#define HOOKED_SBRK

#endif

#if defined(__FreeBSD__) && defined(_LP64)
extern "C" void* sbrk(intptr_t increment) __THROW ATTRIBUTE_SECTION(malloc_hook);

void* sbrk(intptr_t increment) __THROW {
  uintptr_t curbrk = __syscall(SYS_break, nullptr);
  uintptr_t badbrk = static_cast<uintptr_t>(static_cast<intptr_t>(-1));
  if (curbrk == badbrk) {
  nomem:
    errno = ENOMEM;
    return reinterpret_cast<void*>(badbrk);
  }

  if (increment == 0) {
    return reinterpret_cast<void*>(curbrk);
  }

  if (increment > 0) {
    if (curbrk + static_cast<uintptr_t>(increment) < curbrk) {
      goto nomem;
    }
  } else {
    if (curbrk + static_cast<uintptr_t>(increment) > curbrk) {
      goto nomem;
    }
  }

  if (brk(reinterpret_cast<void*>(curbrk + increment)) < 0) {
    goto nomem;
  }

  auto result = reinterpret_cast<void*>(curbrk);
  tcmalloc::mapping_hooks.InvokeSbrk(result, increment);

  return result;
}

#define HOOKED_SBRK

#endif

namespace tcmalloc {
#ifdef HOOKED_MMAP
const bool mmap_hook_works = true;
#else
const bool mmap_hook_works = false;
#endif

#ifdef HOOKED_SBRK
const bool sbrk_hook_works = true;
#else
const bool sbrk_hook_works = false;
#endif
}  // namespace tcmalloc
