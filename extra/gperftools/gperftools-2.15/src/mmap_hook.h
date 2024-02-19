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

// mmap_hook.h holds strictly non-public API for hooking mmap/sbrk
// events as well invoking mmap/munmap with ability to bypass hooks
// (i.e. for low_level_alloc).
#ifndef MMAP_HOOK_H
#define MMAP_HOOK_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "base/basictypes.h"

namespace tcmalloc {

struct DirectAnonMMapResult {
  void* addr;
  bool success;
};

// DirectAnonMMap does mmap of r+w anonymous memory. Optionally
// bypassing or not mmap hooks.
ATTRIBUTE_VISIBILITY_HIDDEN DirectAnonMMapResult DirectAnonMMap(bool invoke_hooks, size_t length);
// DirectMUnMap does munmap of given region optionally bypassing mmap hooks.
ATTRIBUTE_VISIBILITY_HIDDEN int DirectMUnMap(bool invoke_hooks, void* start, size_t length);

// We use those by tests to see what parts we think should work.
extern ATTRIBUTE_VISIBILITY_HIDDEN const bool mmap_hook_works;
extern ATTRIBUTE_VISIBILITY_HIDDEN const bool sbrk_hook_works;

// MMapEventFn gets this struct with all the details of
// mmap/munmap/mremap/sbrk event.
struct MappingEvent {
  MappingEvent() {
    memset(this, 0, sizeof(*this));
  }

  // before_XXX fields describe address space chunk that was removed
  // from address space (say via munmap or mremap)
  void* before_address;
  size_t before_length;

  // after_XXX fields describe address space chunk that was added to
  // address space.
  void* after_address;
  size_t after_length;

  // This group of fields gets populated from mmap file, flags, prot
  // fields.
  int prot;
  int flags;
  int file_fd;
  int64_t file_off;

  unsigned after_valid:1;
  unsigned before_valid:1;
  unsigned file_valid:1;
  unsigned is_sbrk:1;
};

// Pass this to Hook/Unhook function below. Note, nature of
// implementation requires that this chunk of memory must be valid
// even after unhook. So typical use-case is to use global variable
// storage.
//
// All fields are private.
class MappingHookSpace {
public:
  constexpr MappingHookSpace() = default;

  bool initialized = false;

  static constexpr size_t kSize = sizeof(void*) * 3;
  alignas(alignof(void*)) char storage[kSize] = {};
};

using MMapEventFn = void (*)(const MappingEvent& evt);

// HookMMapEvents address hook for mmap events, using given place to
// store relevant metadata (linked list membership etc).
//
// It does no memory allocation and is safe to be called from hooks of all kinds.
ATTRIBUTE_VISIBILITY_HIDDEN void HookMMapEvents(MappingHookSpace* place, MMapEventFn callback);

// UnHookMMapEvents undoes effect of HookMMapEvents. This one is also
// entirely safe to be called from out of anywhere. Including from
// inside MMapEventFn invokations.
//
// As noted on MappingHookSpace the place ***must not** be deallocated or
// reused for anything even after unhook. This requirement makes
// implementation simple enough and fits our internal usage use-case
// fine.
ATTRIBUTE_VISIBILITY_HIDDEN void UnHookMMapEvents(MappingHookSpace* place);

}  // namespace tcmalloc


#endif  // MMAP_HOOK_H
