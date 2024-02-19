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

#include "config_for_unittests.h"

#include "base/logging.h"
#include "base/spinlock.h"

#include "check_address-inl.h"

#ifdef CHECK_ADDRESS_USES_SIGPROCMASK
#define CheckAddress CheckAddressPipes
#define FORCE_PIPES
#include "check_address-inl.h"
#undef CheckAddress
#undef FORCE_PIPES
#endif

#include "tests/testutil.h"

void* unreadable = mmap(0, getpagesize(), PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

static void TestFn(bool (*access_check_fn)(uintptr_t,int)) {
  int pagesize = getpagesize();

  CHECK(!access_check_fn(0, pagesize));
  CHECK(access_check_fn(reinterpret_cast<uintptr_t>(&pagesize), pagesize));

  CHECK(!access_check_fn(reinterpret_cast<uintptr_t>(unreadable), pagesize));

  for (int i = (256 << 10); i > 0; i--) {
    // Lets ensure that pipes access method is forced eventually to drain pipe
    CHECK(noopt(access_check_fn)(reinterpret_cast<uintptr_t>(&pagesize), pagesize));
  }
}

int main() {
  CHECK_NE(unreadable, MAP_FAILED);

  puts("Checking main access fn");
  TestFn([] (uintptr_t a, int ps) {
    // note, this looks odd, but we do it so that each access_check_fn
    // call above reads CheckAddress freshly.
    return CheckAddress(a, ps);
  });

#ifdef CHECK_ADDRESS_USES_SIGPROCMASK
  puts("Checking pipes access fn");
  TestFn(CheckAddressPipes);

  CHECK_EQ(CheckAddress, CheckAccessSingleSyscall);

  puts("Checking two sigprocmask access fn");
  TestFn(CheckAccessTwoSyscalls);
#endif

  puts("PASS");
}
