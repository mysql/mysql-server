// -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*-
// Copyright (c) 2021, gperftools Contributors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// This file contains "generic" stack frame pointer backtracing
// code. Attempt is made to minimize amount of arch- or os-specific
// code and keep everything as generic as possible. Currently
// supported are x86-64, aarch64 and riscv.
#ifndef BASE_STACKTRACE_GENERIC_FP_INL_H_
#define BASE_STACKTRACE_GENERIC_FP_INL_H_

#if HAVE_SYS_UCONTEXT_H
#include <sys/ucontext.h>
#elif HAVE_UCONTEXT_H
#include <ucontext.h>
#endif

// This is only used on OS-es with mmap support.
#include <sys/mman.h>

#if HAVE_SYS_UCONTEXT_H || HAVE_UCONTEXT_H

#define DEFINE_TRIVIAL_GET
#include "getpc.h"

#if !defined(HAVE_TRIVIAL_GET) && !defined(__NetBSD__)
#error sanity
#endif

#define HAVE_GETPC 1
#endif

#include <base/spinlock.h>

#include "check_address-inl.h"

// our Autoconf setup enables -fno-omit-frame-pointer, but lets still
// ask for it just in case.
//
// Note: clang doesn't know about optimize attribute. But clang (and
// gcc too, apparently) automagically forces generation of frame
// pointer whenever __builtin_frame_address is used.
#if defined(__GNUC__) && defined(__has_attribute)
#if __has_attribute(optimize)
#define ENABLE_FP_ATTRIBUTE __attribute__((optimize("no-omit-frame-pointer")))
#endif
#endif

#ifndef ENABLE_FP_ATTRIBUTE
#define ENABLE_FP_ATTRIBUTE
#endif

namespace {
namespace stacktrace_generic_fp {

#if __x86_64__ && !_LP64
// x32 uses 64-bit stack entries but 32-bit addresses.
#define PAD_FRAME
#endif

#if __aarch64__
// Aarch64 has pointer authentication and uses the upper 16bit of a stack
// or return address to sign it. These bits needs to be strip in order for
// stacktraces to work.
void *strip_PAC(void* _ptr) {
  void *ret;
  asm volatile(
      "mov x30, %1\n\t"
      "hint #7\n\t"  // xpaclri, is NOP for < armv8.3-a
      "mov %0, x30\n\t"
      : "=r"(ret)
      : "r"(_ptr)
      : "x30");
  return ret;
}

#define STRIP_PAC(x) (strip_PAC((x)))
#else
#define STRIP_PAC(x) (x)
#endif

struct frame {
  uintptr_t parent;
#ifdef PAD_FRAME
  uintptr_t padding0;
#endif
  void* pc;
#ifdef PAD_FRAME
  uintptr_t padding1;
#endif
};

frame* adjust_fp(frame* f) {
#ifdef __riscv
  return f - 1;
#else
  return f;
#endif
}

bool CheckPageIsReadable(void* ptr, void* checked_ptr) {
  static uintptr_t pagesize;
  if (pagesize == 0) {
    pagesize = getpagesize();
  }

  uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
  uintptr_t parent_frame = reinterpret_cast<uintptr_t>(checked_ptr);

  parent_frame &= ~(pagesize - 1);
  addr &= ~(pagesize - 1);

  if (parent_frame != 0 && addr == parent_frame) {
    return true;
  }

  return CheckAddress(addr, pagesize);
}

template <bool UnsafeAccesses, bool WithSizes>
ATTRIBUTE_NOINLINE // forces architectures with link register to save it
ENABLE_FP_ATTRIBUTE
int capture(void **result, int max_depth, int skip_count,
            void* initial_frame, void* const * initial_pc,
            int *sizes) {
  int i = 0;

  if (initial_pc != nullptr) {
    // This is 'with ucontext' case. We take first pc from ucontext
    // and then skip_count is ignored as we assume that caller only
    // needed stack trace up to signal handler frame.
    skip_count = 0;
    if (max_depth == 0) {
      return 0;
    }
    result[0] = STRIP_PAC(*initial_pc);

    i++;
  }

  max_depth += skip_count;

  constexpr uintptr_t kTooSmallAddr = 16 << 10;
  constexpr uintptr_t kFrameSizeThreshold = 128 << 10;

#ifdef __arm__
  // note, (32-bit, legacy) arm support is not entirely functional
  // w.r.t. frame-pointer-based backtracing. Only recent clangs
  // generate "right" frame pointer setup and only with
  // --enable-frame-pointers. Current gcc-s are hopeless (somewhat
  // older gcc's (circa gcc 6 or so) did something that looks right,
  // but not recent ones).
  constexpr uintptr_t kAlignment = 4;
#else
  // This is simplistic yet. Here we're targeting x86, aarch64 and
  // riscv. They all have 16 bytes stack alignment (even 32 bit
  // riscv). This can be made more elaborate as we consider more
  // architectures.
  constexpr uintptr_t kAlignment = 16;
#endif

  uintptr_t current_frame_addr = reinterpret_cast<uintptr_t>(__builtin_frame_address(0));
  uintptr_t initial_frame_addr = reinterpret_cast<uintptr_t>(initial_frame);
  if (((initial_frame_addr + sizeof(frame)) & (kAlignment - 1)) != 0) {
    return i;
  }
  if (initial_frame_addr < kTooSmallAddr) {
    return i;
  }
  if (initial_frame_addr - current_frame_addr > kFrameSizeThreshold) {
    return i;
  }

  // Note, we assume here that this functions frame pointer is not
  // bogus. Which is true if this code is built with
  // -fno-omit-frame-pointer.
  frame* prev_f = reinterpret_cast<frame*>(current_frame_addr);
  frame *f = adjust_fp(reinterpret_cast<frame*>(initial_frame));

  while (i < max_depth) {
    if (!UnsafeAccesses
        && !CheckPageIsReadable(&f->parent, prev_f)) {
      break;
    }

    void* pc = f->pc;
    if (pc == nullptr) {
      break;
    }

    if (i >= skip_count) {
      if (WithSizes) {
        sizes[i - skip_count] = reinterpret_cast<uintptr_t>(prev_f) - reinterpret_cast<uintptr_t>(f);
      }
      result[i - skip_count] = STRIP_PAC(pc);
    }

    i++;

    uintptr_t parent_frame_addr = f->parent;
    uintptr_t child_frame_addr = reinterpret_cast<uintptr_t>(f);

    if (parent_frame_addr < kTooSmallAddr) {
      break;
    }

    // stack grows towards smaller addresses, so if we didn't see
    // frame address increased (going from child to parent), it is bad
    // frame. We also test if frame is too big since that is another
    // sign of bad stack frame.
    if (parent_frame_addr - child_frame_addr > kFrameSizeThreshold) {
      break;
    }

    if (((parent_frame_addr + sizeof(frame)) & (kAlignment - 1)) != 0) {
      // not aligned, so we keep it safe and assume frame is bogus
      break;
    }

    prev_f = f;

    f = adjust_fp(reinterpret_cast<frame*>(parent_frame_addr));
  }
  if (WithSizes && i > 0 && skip_count == 0) {
    sizes[0] = 0;
  }
  return i - skip_count;
}

}  // namespace stacktrace_generic_fp
}  // namespace

#endif  // BASE_STACKTRACE_GENERIC_FP_INL_H_

// Note: this part of the file is included several times.
// Do not put globals below.

// The following 4 functions are generated from the code below:
//   GetStack{Trace,Frames}()
//   GetStack{Trace,Frames}WithContext()
//
// These functions take the following args:
//   void** result: the stack-trace, as an array
//   int* sizes: the size of each stack frame, as an array
//               (GetStackFrames* only)
//   int max_depth: the size of the result (and sizes) array(s)
//   int skip_count: how many stack pointers to skip before storing in result
//   void* ucp: a ucontext_t* (GetStack{Trace,Frames}WithContext only)

// Set this to true to disable "probing" of addresses that are read to
// make backtracing less-safe, but faster.
#ifndef TCMALLOC_UNSAFE_GENERIC_FP_STACKTRACE
#define TCMALLOC_UNSAFE_GENERIC_FP_STACKTRACE 0
#endif

ENABLE_FP_ATTRIBUTE
static int GET_STACK_TRACE_OR_FRAMES {
  if (max_depth == 0) {
    return 0;
  }

#if IS_STACK_FRAMES
  constexpr bool WithSizes = true;
  memset(sizes, 0, sizeof(*sizes) * max_depth);
#else
  constexpr bool WithSizes = false;
  int * const sizes = nullptr;
#endif

  // one for this function
  skip_count += 1;

  void* const * initial_pc = nullptr;
  void* initial_frame = __builtin_frame_address(0);
  int n;

#if IS_WITH_CONTEXT && (HAVE_SYS_UCONTEXT_H || HAVE_UCONTEXT_H)
  if (ucp) {
    auto uc = static_cast<const ucontext_t*>(ucp);

    // We have to resort to macro since different architectures have
    // different concrete types for those args.
#define SETUP_FRAME(pc_ptr, frame_addr)         \
    do { \
      initial_pc = reinterpret_cast<void* const *>(pc_ptr); \
      initial_frame = reinterpret_cast<void*>(frame_addr); \
    } while (false)

#if __linux__ && __riscv
    SETUP_FRAME(&uc->uc_mcontext.__gregs[REG_PC], uc->uc_mcontext.__gregs[REG_S0]);
#elif __linux__ && __aarch64__
    SETUP_FRAME(&uc->uc_mcontext.pc, uc->uc_mcontext.regs[29]);
#elif __linux__ && __arm__
    // Note: arm's frame pointer support is borked in recent GCC-s.
    SETUP_FRAME(&uc->uc_mcontext.arm_pc, uc->uc_mcontext.arm_fp);
#elif __linux__ && __i386__
    SETUP_FRAME(&uc->uc_mcontext.gregs[REG_EIP], uc->uc_mcontext.gregs[REG_EBP]);
#elif __linux__ && __x86_64__
    SETUP_FRAME(&uc->uc_mcontext.gregs[REG_RIP], uc->uc_mcontext.gregs[REG_RBP]);
#elif __FreeBSD__ && __x86_64__
    SETUP_FRAME(&uc->uc_mcontext.mc_rip, uc->uc_mcontext.mc_rbp);
#elif __FreeBSD__ && __i386__
    SETUP_FRAME(&uc->uc_mcontext.mc_eip, uc->uc_mcontext.mc_ebp);
#elif __NetBSD__
    // NetBSD has those portable defines. Nice!
    SETUP_FRAME(&_UC_MACHINE_PC(uc), _UC_MACHINE_FP(uc));
#elif defined(HAVE_GETPC)
    // So if we're dealing with architecture that doesn't belong to
    // one of cases above, we still have plenty more cases supported
    // by pc_from_ucontext facility we have for cpu profiler. We'll
    // get top-most instruction pointer from context, and rest will be
    // grabbed by frame pointer unwinding (with skipping active).
    //
    // It is a bit of a guess, but it works for x86 (makes
    // stacktrace_unittest ucontext test pass). Main idea is skip
    // count we have will skip just past 'sigreturn' trampoline or
    // whatever OS has. And those tend to be built without frame
    // pointers, which causes last "skipping" step to skip past the
    // frame we need. Also, this is how our CPU profiler is built. It
    // always places "pc from ucontext" first and then if necessary
    // deduplicates it from backtrace.

    result[0] = GetPC(*uc);
    if (result[0] == nullptr) {
      // This OS/HW combo actually lacks known way to extract PC.
      ucp = nullptr;
    }
#else
    ucp = nullptr;
#endif

#undef SETUP_FRAME
  }
#elif !IS_WITH_CONTEXT
  void * const ucp = nullptr;
#endif  // IS_WITH_CONTEXT

  constexpr bool UnsafeAccesses = (TCMALLOC_UNSAFE_GENERIC_FP_STACKTRACE != 0);

  if (ucp && !initial_pc) {
    // we're dealing with architecture that doesn't have proper ucontext integration
    n = stacktrace_generic_fp::capture<UnsafeAccesses, WithSizes>(
      result + 1, max_depth - 1, skip_count,
      initial_frame, initial_pc, sizes);
    n++;
  } else {
    n = stacktrace_generic_fp::capture<UnsafeAccesses, WithSizes>(
      result, max_depth, skip_count,
      initial_frame, initial_pc, sizes);
  }

  if (n > 0) {
    // make sure we don't tail-call capture
    (void)*(const_cast<void * volatile *>(result));
  }

  return n;
}
