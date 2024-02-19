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

#include "config.h"

// When we end up running this on 32-bit glibc/uclibc/bionic system,
// lets ask for 64-bit off_t (only for stuff like lseek and ftruncate
// below). But ***only*** for this file.
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include "config_for_unittests.h"

#include "mmap_hook.h"

#include "base/logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" int MallocHook_InitAtFirstAllocation_HeapLeakChecker() {
  printf("first mmap!\n");
  return 1;
}

#ifdef HAVE_MMAP

#include <fcntl.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

static_assert(sizeof(off_t) == sizeof(int64_t), "");

static tcmalloc::MappingEvent last_evt;
static bool have_last_evt;

static void HandleMappingEvent(const tcmalloc::MappingEvent& evt) {
  CHECK(!have_last_evt);
  memcpy(&last_evt, &evt, sizeof(evt));
  have_last_evt = true;
}

static tcmalloc::MappingHookSpace hook_space;

void do_test_sbrk() {
  if (!tcmalloc::sbrk_hook_works) {
    puts("sbrk test SKIPPED");
    return;
  }

  tcmalloc::HookMMapEvents(&hook_space, &HandleMappingEvent);

  void* addr = sbrk(8);

  CHECK(last_evt.is_sbrk);
  CHECK(!last_evt.before_valid && !last_evt.file_valid && last_evt.after_valid);
  CHECK_EQ(last_evt.after_address, addr);
  CHECK_EQ(last_evt.after_length, 8);

  have_last_evt = false;

  void* addr2 = sbrk(16);

  CHECK(last_evt.is_sbrk);
  CHECK(!last_evt.before_valid && !last_evt.file_valid && last_evt.after_valid);
  CHECK_EQ(last_evt.after_address, addr2);
  CHECK_EQ(last_evt.after_length, 16);

  have_last_evt = false;

  char* addr3 = static_cast<char*>(sbrk(-13));

  CHECK(last_evt.is_sbrk);
  CHECK(last_evt.before_valid && !last_evt.file_valid && !last_evt.after_valid);
  CHECK_EQ(last_evt.before_address, addr3-13);
  CHECK_EQ(last_evt.before_length, 13);

  have_last_evt = false;

  tcmalloc::UnHookMMapEvents(&hook_space);

  puts("sbrk test PASS");
}

static off_t must_lseek(int fd, off_t off, int whence) {
  off_t lseek_result = lseek(fd, off, whence);
  PCHECK(lseek_result != off_t{-1});
  return lseek_result;
}

void do_test_mmap() {
  if (!tcmalloc::mmap_hook_works) {
    puts("mmap test SKIPPED");
    return;
  }
  FILE* f = tmpfile();
  PCHECK(f != nullptr);

  int fd = fileno(f);

  PCHECK(ftruncate(fd, off_t{1} << 40) >= 0);

  int pagesz = getpagesize();

  off_t test_off = (off_t{1} << 40) - pagesz * 2;
  CHECK_EQ(must_lseek(fd, -pagesz * 2, SEEK_END), test_off);

  static constexpr char contents[] = "foobarXYZ";

  PCHECK(write(fd, contents, sizeof(contents)) == sizeof(contents));

  tcmalloc::HookMMapEvents(&hook_space, &HandleMappingEvent);

  char* mm_addr = static_cast<char*>(mmap(nullptr, pagesz, PROT_READ|PROT_WRITE, MAP_SHARED, fd, test_off));
  CHECK(memcmp(mm_addr, contents, sizeof(contents)) == 0);

  CHECK(have_last_evt && !last_evt.before_valid && last_evt.after_valid && last_evt.file_valid);
  CHECK_EQ(last_evt.after_address, mm_addr);
  CHECK_EQ(last_evt.after_length, pagesz);
  CHECK_EQ(last_evt.file_fd, fd);
  CHECK_EQ(last_evt.file_off, test_off);
  CHECK_EQ(last_evt.flags, MAP_SHARED);
  CHECK_EQ(last_evt.prot, (PROT_READ|PROT_WRITE));

  have_last_evt = false;

#ifdef __linux__
  void* reserve = mmap(nullptr, pagesz * 2, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
  PCHECK(reserve != MAP_FAILED);
  CHECK(have_last_evt);

  have_last_evt = false;

  char* new_addr = static_cast<char*>(mremap(mm_addr, pagesz,
                                             pagesz * 2, MREMAP_MAYMOVE | MREMAP_FIXED,
                                             reserve));
  PCHECK(new_addr != MAP_FAILED);
  CHECK_EQ(new_addr, reserve);
  CHECK(have_last_evt);

  CHECK(!last_evt.is_sbrk && last_evt.after_valid && last_evt.before_valid && !last_evt.file_valid);
  CHECK_EQ(last_evt.after_address, new_addr);
  CHECK_EQ(last_evt.after_length, pagesz * 2);
  CHECK_EQ(last_evt.before_address, mm_addr);
  CHECK_EQ(last_evt.before_length, pagesz);

  have_last_evt = false;

  PCHECK(pwrite(fd, contents, sizeof(contents), test_off + pagesz + 1) == sizeof(contents));
  mm_addr = new_addr;

  CHECK_EQ(memcmp(mm_addr + pagesz + 1, contents, sizeof(contents)), 0);
  puts("mremap test PASS");
#endif

  PCHECK(munmap(mm_addr, pagesz) >= 0);

  CHECK(have_last_evt && !last_evt.is_sbrk && !last_evt.after_valid && last_evt.before_valid && !last_evt.file_valid);
  CHECK_EQ(last_evt.before_address, mm_addr);
  CHECK_EQ(last_evt.before_length, pagesz);

  have_last_evt = false;

  size_t sz = 10 * pagesz;
  auto result = tcmalloc::DirectAnonMMap(/* invoke_hooks = */false, sz);
  PCHECK(result.success);
  CHECK_NE(result.addr, MAP_FAILED);
  CHECK(!have_last_evt);

  PCHECK(tcmalloc::DirectMUnMap(false, result.addr, sz) == 0);

  sz = 13 * pagesz;
  result = tcmalloc::DirectAnonMMap(/* invoke_hooks = */true, sz);
  PCHECK(result.success);
  CHECK_NE(result.addr, MAP_FAILED);

  CHECK(have_last_evt && !last_evt.is_sbrk && !last_evt.before_valid && last_evt.after_valid);
  CHECK_EQ(last_evt.after_address, result.addr);
  CHECK_EQ(last_evt.after_length, sz);

  have_last_evt = false;

  sz = sz - pagesz; // lets also check unmapping sub-segment of previously allocated one
  PCHECK(tcmalloc::DirectMUnMap(true, result.addr, sz) == 0);
  CHECK(have_last_evt && !last_evt.is_sbrk && last_evt.before_valid && !last_evt.after_valid);
  CHECK_EQ(last_evt.before_address, result.addr);
  CHECK_EQ(last_evt.before_length, sz);

  puts("mmap test PASS");
}

int main() {
  do_test_sbrk();
  do_test_mmap();
}

#else // !HAVE_MMAP
int main() {}
#endif // !HAVE_MMAP
