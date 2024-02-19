// -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*-
// Copyright (c) 2011, Google Inc.
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

// ----
// Author: llib@google.com (Bill Clarke)

#include "config_for_unittests.h"
#include <assert.h>
#include <stdio.h>
#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>    // for sleep()
#endif
#include <algorithm>
#include <string>
#include <vector>
#include <gperftools/malloc_hook.h>
#include "malloc_hook-inl.h"
#include "base/logging.h"
#include "base/simple_mutex.h"
#include "base/sysinfo.h"
#include "tests/testutil.h"

// On systems (like freebsd) that don't define MAP_ANONYMOUS, use the old
// form of the name instead.
#ifndef MAP_ANONYMOUS
# define MAP_ANONYMOUS MAP_ANON
#endif

namespace {

std::vector<void (*)()> g_testlist;  // the tests to run

#define TEST(a, b)                                      \
  struct Test_##a##_##b {                               \
    Test_##a##_##b() { g_testlist.push_back(&Run); }    \
    static void Run();                                  \
  };                                                    \
  static Test_##a##_##b g_test_##a##_##b;               \
  void Test_##a##_##b::Run()


static int RUN_ALL_TESTS() {
  std::vector<void (*)()>::const_iterator it;
  for (it = g_testlist.begin(); it != g_testlist.end(); ++it) {
    (*it)();   // The test will error-exit if there's a problem.
  }
  fprintf(stderr, "\nPassed %d tests\n\nPASS\n",
          static_cast<int>(g_testlist.size()));
  return 0;
}

void Sleep(int seconds) {
#ifdef _MSC_VER
  _sleep(seconds * 1000);   // Windows's _sleep takes milliseconds argument
#else
  sleep(seconds);
#endif
}

using base::internal::kHookListMaxValues;

// Since HookList is a template and is defined in malloc_hook.cc, we can only
// use an instantiation of it from malloc_hook.cc.  We then reinterpret those
// values as integers for testing.
typedef base::internal::HookList<MallocHook::NewHook> TestHookList;


const MallocHook::NewHook kTestValue = reinterpret_cast<MallocHook::NewHook>(69);
const MallocHook::NewHook kAnotherTestValue = reinterpret_cast<MallocHook::NewHook>(42);
const MallocHook::NewHook kThirdTestValue = reinterpret_cast<MallocHook::NewHook>(7);

TEST(HookListTest, InitialValueExists) {
  TestHookList list{kTestValue};
  MallocHook::NewHook values[2] = {};
  EXPECT_EQ(1, list.Traverse(values, 2));
  EXPECT_EQ(kTestValue, values[0]);
  EXPECT_EQ(1, list.priv_end);
}

TEST(HookListTest, CanRemoveInitialValue) {
  TestHookList list{kTestValue};
  ASSERT_TRUE(list.Remove(kTestValue));
  EXPECT_EQ(0, list.priv_end);

  MallocHook::NewHook values[2] = {};
  EXPECT_EQ(0, list.Traverse(values, 2));
}

TEST(HookListTest, AddAppends) {
  TestHookList list{kTestValue};
  ASSERT_TRUE(list.Add(kAnotherTestValue));
  EXPECT_EQ(2, list.priv_end);

  MallocHook::NewHook values[2] = {};
  EXPECT_EQ(2, list.Traverse(values, 2));
  EXPECT_EQ(kTestValue, values[0]);
  EXPECT_EQ(kAnotherTestValue, values[1]);
}

TEST(HookListTest, RemoveWorksAndWillClearSize) {
  TestHookList list{kTestValue};
  ASSERT_TRUE(list.Add(kAnotherTestValue));

  ASSERT_TRUE(list.Remove(kTestValue));
  EXPECT_EQ(2, list.priv_end);

  MallocHook::NewHook values[2] = {};
  EXPECT_EQ(1, list.Traverse(values, 2));
  EXPECT_EQ(kAnotherTestValue, values[0]);

  ASSERT_TRUE(list.Remove(kAnotherTestValue));
  EXPECT_EQ(0, list.priv_end);
  EXPECT_EQ(0, list.Traverse(values, 2));
}

TEST(HookListTest, AddPrependsAfterRemove) {
  TestHookList list{kTestValue};
  ASSERT_TRUE(list.Add(kAnotherTestValue));

  ASSERT_TRUE(list.Remove(kTestValue));
  EXPECT_EQ(2, list.priv_end);

  ASSERT_TRUE(list.Add(kThirdTestValue));
  EXPECT_EQ(2, list.priv_end);

  MallocHook::NewHook values[3] = {};
  EXPECT_EQ(2, list.Traverse(values, 3));
  EXPECT_EQ(kThirdTestValue, values[0]);
  EXPECT_EQ(kAnotherTestValue, values[1]);
}

TEST(HookListTest, InvalidAddRejected) {
  TestHookList list{kTestValue};
  EXPECT_FALSE(list.Add(nullptr));

  MallocHook::NewHook values[2] = {};
  EXPECT_EQ(1, list.Traverse(values, 2));
  EXPECT_EQ(kTestValue, values[0]);
  EXPECT_EQ(1, list.priv_end);
}

TEST(HookListTest, FillUpTheList) {
  TestHookList list{kTestValue};
  int num_inserts = 0;
  while (list.Add(reinterpret_cast<MallocHook::NewHook>(++num_inserts))) {
    // empty
  }
  EXPECT_EQ(kHookListMaxValues, num_inserts);
  EXPECT_EQ(kHookListMaxValues, list.priv_end);

  MallocHook::NewHook values[kHookListMaxValues + 1];
  EXPECT_EQ(kHookListMaxValues, list.Traverse(values,
                                              kHookListMaxValues));
  EXPECT_EQ(kTestValue, values[0]);
  for (int i = 1; i < kHookListMaxValues; ++i) {
    EXPECT_EQ(reinterpret_cast<MallocHook::NewHook>(i), values[i]);
  }
}

void MultithreadedTestThread(TestHookList* list, int shift,
                             int thread_num) {
  std::string message;
  char buf[64];
  for (int i = 1; i < 1000; ++i) {
    // In each loop, we insert a unique value, check it exists, remove it, and
    // check it doesn't exist.  We also record some stats to log at the end of
    // each thread.  Each insertion location and the length of the list is
    // non-deterministic (except for the very first one, over all threads, and
    // after the very last one the list should be empty).
    const auto value = reinterpret_cast<MallocHook::NewHook>((i << shift) + thread_num);
    EXPECT_TRUE(list->Add(value));

    sched_yield();  // Ensure some more interleaving.

    MallocHook::NewHook values[kHookListMaxValues + 1];
    int num_values = list->Traverse(values, kHookListMaxValues + 1);
    EXPECT_LT(0, num_values);

    int value_index;
    for (value_index = 0;
         value_index < num_values && values[value_index] != value;
         ++value_index) {
      // empty
    }

    EXPECT_LT(value_index, num_values);  // Should have found value.
    snprintf(buf, sizeof(buf), "[%d/%d; ", value_index, num_values);
    message += buf;

    sched_yield();

    EXPECT_TRUE(list->Remove(value));

    sched_yield();

    num_values = list->Traverse(values, kHookListMaxValues);
    for (value_index = 0;
         value_index < num_values && values[value_index] != value;
         ++value_index) {
      // empty
    }

    EXPECT_EQ(value_index, num_values);  // Should not have found value.
    snprintf(buf, sizeof(buf), "%d]", num_values);
    message += buf;

    sched_yield();
  }
  fprintf(stderr, "thread %d: %s\n", thread_num, message.c_str());
}

static volatile int num_threads_remaining;
static TestHookList list{kTestValue};
static Mutex threadcount_lock;

void MultithreadedTestThreadRunner(int thread_num) {
  // Wait for all threads to start running.
  {
    MutexLock ml(&threadcount_lock);
    assert(num_threads_remaining > 0);
    --num_threads_remaining;

    // We should use condvars and the like, but for this test, we'll
    // go simple and busy-wait.
    while (num_threads_remaining > 0) {
      threadcount_lock.Unlock();
      Sleep(1);
      threadcount_lock.Lock();
    }
  }

  // shift is the smallest number such that (1<<shift) > kHookListMaxValues
  int shift = 0;
  for (int i = kHookListMaxValues; i > 0; i >>= 1) {
    shift += 1;
  }

  MultithreadedTestThread(&list, shift, thread_num);
}


TEST(HookListTest, MultithreadedTest) {
  ASSERT_TRUE(list.Remove(kTestValue));
  ASSERT_EQ(0, list.priv_end);

  // Run kHookListMaxValues thread, each running MultithreadedTestThread.
  // First, we need to set up the rest of the globals.
  num_threads_remaining = kHookListMaxValues;   // a global var
  RunManyThreadsWithId(&MultithreadedTestThreadRunner, num_threads_remaining,
                       1 << 15);

  MallocHook::NewHook values[kHookListMaxValues + 1];
  EXPECT_EQ(0, list.Traverse(values, kHookListMaxValues + 1));
  EXPECT_EQ(0, list.priv_end);
}

}  // namespace

int main(int argc, char** argv) {
  return RUN_ALL_TESTS();
}
