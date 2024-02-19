// -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*-
// Copyright (c) 2005, Google Inc.
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
// Author: Sanjay Ghemawat
//
// This has the implementation details of malloc_hook that are needed
// to use malloc-hook inside the tcmalloc system.  It does not hold
// any of the client-facing calls that are used to add new hooks.

#ifndef _MALLOC_HOOK_INL_H_
#define _MALLOC_HOOK_INL_H_

#include <stddef.h>
#include <sys/types.h>

#include <atomic>

#include "base/basictypes.h"
#include <gperftools/malloc_hook.h>

#include "common.h" // for UNLIKELY

namespace base { namespace internal {

// Capacity of 8 means that HookList is 9 words.
static const int kHookListCapacity = 8;
// last entry is reserved for deprecated "singular" hooks. So we have
// 7 "normal" hooks per list
static const int kHookListMaxValues = 7;
static const int kHookListSingularIdx = 7;

// HookList: a class that provides synchronized insertions and removals and
// lockless traversal.  Most of the implementation is in malloc_hook.cc.
template <typename T>
struct PERFTOOLS_DLL_DECL HookList {
  static_assert(sizeof(T) <= sizeof(uintptr_t), "must fit in uintptr_t");

  constexpr HookList() = default;
  explicit constexpr HookList(T priv_data_initial) : priv_end{1}, priv_data{priv_data_initial} {}

  // Adds value to the list.  Note that duplicates are allowed.  Thread-safe and
  // blocking (acquires hooklist_spinlock).  Returns true on success; false
  // otherwise (failures include invalid value and no space left).
  bool Add(T value);

  void FixupPrivEndLocked();

  // Removes the first entry matching value from the list.  Thread-safe and
  // blocking (acquires hooklist_spinlock).  Returns true on success; false
  // otherwise (failures include invalid value and no value found).
  bool Remove(T value);

  // Store up to n values of the list in output_array, and return the number of
  // elements stored.  Thread-safe and non-blocking.  This is fast (one memory
  // access) if the list is empty.
  int Traverse(T* output_array, int n) const;

  // Fast inline implementation for fast path of Invoke*Hook.
  bool empty() const {
    return priv_end.load(std::memory_order_relaxed) == 0;
  }

  // Used purely to handle deprecated singular hooks
  T GetSingular() const {
    return bit_cast<T>(cast_priv_data(kHookListSingularIdx)->load(std::memory_order_relaxed));
  }

  T ExchangeSingular(T new_val);

  // This internal data is not private so that the class is an aggregate and can
  // be initialized by the linker.  Don't access this directly.  Use the
  // INIT_HOOK_LIST macro in malloc_hook.cc.

  // One more than the index of the last valid element in priv_data.  During
  // 'Remove' this may be past the last valid element in priv_data, but
  // subsequent values will be 0.
  //
  // Index kHookListCapacity-1 is reserved as 'deprecated' single hook pointer
  std::atomic<uintptr_t> priv_end;
  T priv_data[kHookListCapacity];

  // C++ 11 doesn't let us initialize array of atomics, so we made
  // priv_data regular array of T and cast when reading and writing
  // (which is portable in practice)
  std::atomic<T>* cast_priv_data(int index) {
    return reinterpret_cast<std::atomic<T>*>(priv_data + index);
  }
  std::atomic<T> const * cast_priv_data(int index) const {
    return reinterpret_cast<std::atomic<T> const *>(priv_data + index);
  }
};

ATTRIBUTE_VISIBILITY_HIDDEN extern HookList<MallocHook::NewHook> new_hooks_;
ATTRIBUTE_VISIBILITY_HIDDEN extern HookList<MallocHook::DeleteHook> delete_hooks_;

} }  // namespace base::internal

// The following method is DEPRECATED
inline MallocHook::NewHook MallocHook::GetNewHook() {
  return base::internal::new_hooks_.GetSingular();
}

inline void MallocHook::InvokeNewHook(const void* p, size_t s) {
  if (PREDICT_FALSE(!base::internal::new_hooks_.empty())) {
    InvokeNewHookSlow(p, s);
  }
}

// The following method is DEPRECATED
inline MallocHook::DeleteHook MallocHook::GetDeleteHook() {
  return base::internal::delete_hooks_.GetSingular();
}

inline void MallocHook::InvokeDeleteHook(const void* p) {
  if (PREDICT_FALSE(!base::internal::delete_hooks_.empty())) {
    InvokeDeleteHookSlow(p);
  }
}

#endif /* _MALLOC_HOOK_INL_H_ */
