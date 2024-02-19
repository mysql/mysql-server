// -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*-
// Copyright (c) 2021, IBM Ltd.
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
// Author: Chris Cambly <ccambly@ca.ibm.com>
//
// Used to override malloc routines on AIX

#ifndef TCMALLOC_LIBC_OVERRIDE_AIX_INL_H_
#define TCMALLOC_LIBC_OVERRIDE_AIX_INL_H_

#ifndef _AIX
# error libc_override_aix.h is for AIX systems only.
#endif

extern "C" {
  // AIX user-defined malloc replacement routines
  void* __malloc__(size_t size) __THROW               ALIAS(tc_malloc);
  void __free__(void* ptr) __THROW                    ALIAS(tc_free);
  void* __realloc__(void* ptr, size_t size) __THROW   ALIAS(tc_realloc);
  void* __calloc__(size_t n, size_t size) __THROW     ALIAS(tc_calloc);
  int __posix_memalign__(void** r, size_t a, size_t s) __THROW ALIAS(tc_posix_memalign);
  int __mallopt__(int cmd, int value) __THROW         ALIAS(tc_mallopt);
#ifdef HAVE_STRUCT_MALLINFO
  struct mallinfo __mallinfo__(void) __THROW          ALIAS(tc_mallinfo);
#endif
#ifdef HAVE_STRUCT_MALLINFO2
  struct mallinfo2 __mallinfo2__(void) __THROW        ALIAS(tc_mallinfo2);
#endif
  void __malloc_init__(void)               { tc_free(tc_malloc(1));}
  void* __malloc_prefork_lock__(void)      { /* nothing to lock */ }
  void* __malloc_postfork_unlock__(void)   { /* nothing to unlock */}
}   // extern "C"

#endif  // TCMALLOC_LIBC_OVERRIDE_AIX_INL_H_
