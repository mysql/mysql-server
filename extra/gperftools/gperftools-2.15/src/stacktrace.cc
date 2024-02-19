// -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*-
// Copyright (c) 2005, Google Inc.
// Copyright (c) 2023, gperftools Contributors.
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

// ---
// Original Author: Sanjay Ghemawat.
//
// Most recent significant rework and extensions: Aliaksei
// Kandratsenka (all bugs are mine).
//
// Produce stack trace.
//
// There few different ways we can try to get the stack trace:
//
// 1) Our hand-coded stack-unwinder.  This depends on a certain stack
//    layout, which is used by various ABIs. It uses the frame
//    pointer to do its work.
//
// 2) The libunwind library. It also doesn't call malloc (in most
//    configurations). Note, there are at least 3 libunwind
//    implementations currently available. "Original" libunwind,
//    llvm's and Android's. Only original library has been tested so
//    far.
//
// 3) The "libgcc" unwinder -- also the one used by the c++ exception
//    code. It uses _Unwind_Backtrace facility of modern ABIs. Some
//    implementations occasionally call into malloc (which we're able
//    to handle). Some implementations also use some internal locks,
//    so it is not entirely compatible with backtracing from signal
//    handlers.
//
// 4) backtrace() unwinder (available in glibc and execinfo on some
//    BSDs). It is typically, but not always implemented on top of
//    "libgcc" unwinder. So we have it. We use this one on OSX.
//
// 5) On windows we use RtlCaptureStackBackTrace.
//
// Note: if you add a new implementation here, make sure it works
// correctly when GetStackTrace() is called with max_depth == 0.
// Some code may do that.

#include <config.h>
#include <stdlib.h> // for getenv
#include <string.h> // for strcmp
#include <stdio.h> // for fprintf
#include "gperftools/stacktrace.h"
#include "base/commandlineflags.h"
#include "base/googleinit.h"
#include "getenv_safe.h"


// we're using plain struct and not class to avoid any possible issues
// during initialization. Struct of pointers is easy to init at
// link-time.
struct GetStackImplementation {
  int (*GetStackFramesPtr)(void** result, int* sizes, int max_depth,
                           int skip_count);

  int (*GetStackFramesWithContextPtr)(void** result, int* sizes, int max_depth,
                                      int skip_count, const void *uc);

  int (*GetStackTracePtr)(void** result, int max_depth,
                          int skip_count);

  int (*GetStackTraceWithContextPtr)(void** result, int max_depth,
                                  int skip_count, const void *uc);

  const char *name;
};

#if HAVE_DECL_BACKTRACE
#define STACKTRACE_INL_HEADER "stacktrace_generic-inl.h"
#define GST_SUFFIX generic
#include "stacktrace_impl_setup-inl.h"
#undef GST_SUFFIX
#undef STACKTRACE_INL_HEADER
#define HAVE_GST_generic
#endif

#ifdef HAVE_UNWIND_BACKTRACE
#define STACKTRACE_INL_HEADER "stacktrace_libgcc-inl.h"
#define GST_SUFFIX libgcc
#include "stacktrace_impl_setup-inl.h"
#undef GST_SUFFIX
#undef STACKTRACE_INL_HEADER
#define HAVE_GST_libgcc
#endif

// libunwind uses __thread so we check for both libunwind.h and
// __thread support
#if defined(USE_LIBUNWIND) && defined(HAVE_TLS)
#define STACKTRACE_INL_HEADER "stacktrace_libunwind-inl.h"
#define GST_SUFFIX libunwind
#include "stacktrace_impl_setup-inl.h"
#undef GST_SUFFIX
#undef STACKTRACE_INL_HEADER
#define HAVE_GST_libunwind
#endif // USE_LIBUNWIND

#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__) || defined(__aarch64__) || defined(__riscv) || defined(__arm__))
// NOTE: legacy 32-bit arm works fine with recent clangs, but is broken in gcc: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=92172
#define STACKTRACE_INL_HEADER "stacktrace_generic_fp-inl.h"
#define GST_SUFFIX generic_fp
#include "stacktrace_impl_setup-inl.h"
#undef GST_SUFFIX
#undef STACKTRACE_INL_HEADER
#define HAVE_GST_generic_fp

#undef TCMALLOC_UNSAFE_GENERIC_FP_STACKTRACE
#define TCMALLOC_UNSAFE_GENERIC_FP_STACKTRACE 1

#define STACKTRACE_INL_HEADER "stacktrace_generic_fp-inl.h"
#define GST_SUFFIX generic_fp_unsafe
#include "stacktrace_impl_setup-inl.h"
#undef GST_SUFFIX
#undef STACKTRACE_INL_HEADER
#define HAVE_GST_generic_fp_unsafe
#endif

#if defined(__ppc__) || defined(__PPC__)
#if defined(__linux__)
#define STACKTRACE_INL_HEADER "stacktrace_powerpc-linux-inl.h"
#else
#define STACKTRACE_INL_HEADER "stacktrace_powerpc-darwin-inl.h"
#endif
#define GST_SUFFIX ppc
#include "stacktrace_impl_setup-inl.h"
#undef GST_SUFFIX
#undef STACKTRACE_INL_HEADER
#define HAVE_GST_ppc
#endif

#if defined(__arm__)
#define STACKTRACE_INL_HEADER "stacktrace_arm-inl.h"
#define GST_SUFFIX arm
#include "stacktrace_impl_setup-inl.h"
#undef GST_SUFFIX
#undef STACKTRACE_INL_HEADER
#define HAVE_GST_arm
#endif

#ifdef TCMALLOC_ENABLE_INSTRUMENT_STACKTRACE
#define STACKTRACE_INL_HEADER "stacktrace_instrument-inl.h"
#define GST_SUFFIX instrument
#include "stacktrace_impl_setup-inl.h"
#undef GST_SUFFIX
#undef STACKTRACE_INL_HEADER
#define HAVE_GST_instrument
#endif

// The Windows case -- probably cygwin and mingw will use one of the
// x86-includes above, but if not, we can fall back to windows intrinsics.
#if defined(_WIN32) || defined(__CYGWIN__) || defined(__CYGWIN32__) || defined(__MINGW32__)
#define STACKTRACE_INL_HEADER "stacktrace_win32-inl.h"
#define GST_SUFFIX win32
#include "stacktrace_impl_setup-inl.h"
#undef GST_SUFFIX
#undef STACKTRACE_INL_HEADER
#define HAVE_GST_win32
#endif

#if __cplusplus >= 202302L
# ifndef HAS_SOME_STACKTRACE_IMPL
#  warning "Warning: no stacktrace capturing implementation for your OS"
# endif
#endif

#if (__x86_64__ || __i386__) && FORCED_FRAME_POINTERS
// x86-es (even i386 this days) default to no frame pointers. But
// historically we defaulted to frame pointer unwinder whenever
// --enable-frame-pointers is given. So we keep this behavior.
#define PREFER_FP_UNWINDER 1
#elif TCMALLOC_DONT_PREFER_LIBUNWIND && !defined(PREFER_LIBGCC_UNWINDER)
#define PREFER_FP_UNWINDER 1
#else
#define PREFER_FP_UNWINDER 0
#endif

#if defined(PREFER_LIBGCC_UNWINDER) && !defined(HAVE_GST_libgcc)
#error user asked for libgcc unwinder to be default but it is not available
#endif

static int null_GetStackFrames(void** result, int* sizes, int max_depth,
                               int skip_count) {
  return 0;
}

static int null_GetStackFramesWithContext(void** result, int* sizes, int max_depth,
                                          int skip_count, const void *uc) {
  return 0;
}

static int null_GetStackTrace(void** result, int max_depth,
                              int skip_count) {
  return 0;
}

static int null_GetStackTraceWithContext(void** result, int max_depth,
                                         int skip_count, const void *uc) {
  return 0;
}

static GetStackImplementation impl__null = {
  null_GetStackFrames,
  null_GetStackFramesWithContext,
  null_GetStackTrace,
  null_GetStackTraceWithContext,
  "null"
};

static GetStackImplementation *all_impls[] = {
#ifdef HAVE_GST_instrument
  &impl__instrument,
#endif
#ifdef HAVE_GST_win32
  &impl__win32,
#endif
#ifdef HAVE_GST_ppc
  &impl__ppc,
#endif
#if defined(HAVE_GST_generic_fp) && PREFER_FP_UNWINDER
  &impl__generic_fp,
  &impl__generic_fp_unsafe,
#endif
#if defined(HAVE_GST_libgcc) && defined(PREFER_LIBGCC_UNWINDER)
  &impl__libgcc,
#endif
#ifdef HAVE_GST_libunwind
  &impl__libunwind,
#endif
#if defined(HAVE_GST_libgcc) && !defined(PREFER_LIBGCC_UNWINDER)
  &impl__libgcc,
#endif
#ifdef HAVE_GST_generic
  &impl__generic,
#endif
#if defined(HAVE_GST_generic_fp) && !PREFER_FP_UNWINDER
  &impl__generic_fp,
  &impl__generic_fp_unsafe,
#endif
#ifdef HAVE_GST_arm
  &impl__arm,
#endif
  &impl__null
};

static bool get_stack_impl_inited;
static GetStackImplementation *get_stack_impl;

#if 0
// This is for the benefit of code analysis tools that may have
// trouble with the computed #include above.
# include "stacktrace_libunwind-inl.h"
# include "stacktrace_generic-inl.h"
# include "stacktrace_generic_fp-inl.h"
# include "stacktrace_powerpc-linux-inl.h"
# include "stacktrace_win32-inl.h"
# include "stacktrace_arm-inl.h"
# include "stacktrace_instrument-inl.h"
#endif

static void init_default_stack_impl_inner(void);

namespace tcmalloc {
  bool EnterStacktraceScope(void);
  void LeaveStacktraceScope(void);
}

namespace {
using tcmalloc::EnterStacktraceScope;
using tcmalloc::LeaveStacktraceScope;

class StacktraceScope {
  bool stacktrace_allowed;
public:
  StacktraceScope() {
    stacktrace_allowed = true;
    stacktrace_allowed = EnterStacktraceScope();
  }
  bool IsStacktraceAllowed() {
    return stacktrace_allowed;
  }
  // NOTE: noinline here ensures that we don't tail-call GetStackXXX
  // calls below. Which is crucial due to us having to pay attention
  // to skip_count argument.
  ATTRIBUTE_NOINLINE ~StacktraceScope() {
    if (stacktrace_allowed) {
      LeaveStacktraceScope();
    }
  }
};

}  // namespace

ATTRIBUTE_NOINLINE
PERFTOOLS_DLL_DECL int GetStackFrames(void** result, int* sizes, int max_depth,
                                      int skip_count) {
  StacktraceScope scope;
  if (!scope.IsStacktraceAllowed()) {
    return 0;
  }
  init_default_stack_impl_inner();
  return get_stack_impl->GetStackFramesPtr(result, sizes,
                                           max_depth, skip_count);
}

ATTRIBUTE_NOINLINE
PERFTOOLS_DLL_DECL int GetStackFramesWithContext(void** result, int* sizes, int max_depth,
                                                 int skip_count, const void *uc) {
  StacktraceScope scope;
  if (!scope.IsStacktraceAllowed()) {
    return 0;
  }
  init_default_stack_impl_inner();
  return get_stack_impl->GetStackFramesWithContextPtr(result, sizes, max_depth,
                                                      skip_count, uc);
}

ATTRIBUTE_NOINLINE
PERFTOOLS_DLL_DECL int GetStackTrace(void** result, int max_depth,
                                     int skip_count) {
  StacktraceScope scope;
  if (!scope.IsStacktraceAllowed()) {
    return 0;
  }
  init_default_stack_impl_inner();
  return get_stack_impl->GetStackTracePtr(result, max_depth, skip_count);
}

ATTRIBUTE_NOINLINE
PERFTOOLS_DLL_DECL int GetStackTraceWithContext(void** result, int max_depth,
                                                int skip_count, const void *uc) {
  StacktraceScope scope;
  if (!scope.IsStacktraceAllowed()) {
    return 0;
  }
  init_default_stack_impl_inner();
  return get_stack_impl->GetStackTraceWithContextPtr(result, max_depth,
                                                     skip_count, uc);
}

#if STACKTRACE_IS_TESTED
static void init_default_stack_impl_inner() {
}

extern "C" {
const char* TEST_bump_stacktrace_implementation(const char* suggestion) {
  static int selection;
  constexpr int n = sizeof(all_impls)/sizeof(all_impls[0]);

  if (!get_stack_impl_inited) {
    fprintf(stderr, "Supported stacktrace methods:\n");
    for (int i = 0; i < n; i++) {
      fprintf(stderr, "* %s\n", all_impls[i]->name);
    }
    fprintf(stderr, "\n\n");
    get_stack_impl_inited = true;
  }

  do {
    if (selection == n) {
      return nullptr;
    }
    get_stack_impl = all_impls[selection++];

    if (suggestion && strcmp(suggestion, get_stack_impl->name) != 0) {
      continue;
    }
    if (get_stack_impl == &impl__null) {
      // skip null implementation
      continue;
    }
    break;
  } while (true);

  return get_stack_impl->name;
}
}

#else  // !STACKTRACE_IS_TESTED

ATTRIBUTE_NOINLINE
static void maybe_convert_libunwind_to_generic_fp() {
#if defined(HAVE_GST_libunwind) && defined(HAVE_GST_generic_fp)
  if (get_stack_impl != &impl__libunwind) {
    return;
  }

  bool want_to_replace = false;

  // Sometime recently, aarch64 had completely borked libunwind, so
  // lets test this case and fall back to frame pointers (which is
  // nearly but not quite perfect). So lets check this case.
  void* stack[4];
  int rv = get_stack_impl->GetStackTracePtr(stack, 4, 0);
  want_to_replace = (rv <= 2);

  if (want_to_replace) {
    get_stack_impl = &impl__generic_fp;
  }
#endif  // have libunwind and generic_fp
}

static void init_default_stack_impl_inner(void) {
  if (get_stack_impl_inited) {
    return;
  }
  get_stack_impl = all_impls[0];
  get_stack_impl_inited = true;
  const char *val = TCMallocGetenvSafe("TCMALLOC_STACKTRACE_METHOD");
  if (!val || !*val) {
    // If no explicit implementation is requested, consider changing
    // libunwind->generic_fp in some cases.
    maybe_convert_libunwind_to_generic_fp();
    return;
  }
  for (int i = 0; i < sizeof(all_impls) / sizeof(all_impls[0]); i++) {
    GetStackImplementation *c = all_impls[i];
    if (strcmp(c->name, val) == 0) {
      get_stack_impl = c;
      return;
    }
  }
  fprintf(stderr, "Unknown or unsupported stacktrace method requested: %s. Ignoring it\n", val);
}

ATTRIBUTE_NOINLINE
static void init_default_stack_impl(void) {
  init_default_stack_impl_inner();
  if (EnvToBool("TCMALLOC_STACKTRACE_METHOD_VERBOSE", false)) {
    fprintf(stderr, "Chosen stacktrace method is %s\nSupported methods:\n", get_stack_impl->name);
    for (int i = 0; i < sizeof(all_impls) / sizeof(all_impls[0]); i++) {
      GetStackImplementation *c = all_impls[i];
      fprintf(stderr, "* %s\n", c->name);
    }
    fputs("\nUse TCMALLOC_STACKTRACE_METHOD environment variable to override\n", stderr);
  }
}

REGISTER_MODULE_INITIALIZER(stacktrace_init_default_stack_impl, init_default_stack_impl());

#endif  // !STACKTRACE_IS_TESTED
