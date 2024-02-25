/* Copyright (c) 2011, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <limits.h>
#include <thread>

#include "my_config.h"

#include "m_string.h"
#include "my_inttypes.h"
#include "my_stacktrace.h"
#include "unittest/gunit/test_utils.h"
#include "unittest/gunit/thread_utils.h"

namespace segfault_unittest {

using my_testing::Mock_error_handler;
using my_testing::Server_initializer;

size_t get_number_of_occurrences(std::string hay, std::string needle) {
  size_t pos = hay.find(needle);
  size_t count = 0;
  if (pos != std::string::npos) {
    for (;;) {
      count++;
      const auto new_pos = hay.substr(pos + 1).find(needle);
      if (new_pos == std::string::npos) {
        break;
      }
      pos += 1 + new_pos;
    }
  }
  return count;
}

MATCHER_P3(ContainsRangeOfOccurrences, n, m, str, "") {
  const auto count = get_number_of_occurrences(arg, str);
  *result_listener << "where the actual count found is " << count;
  return n <= static_cast<int>(count) && static_cast<int>(count) <= m;
}
static bool contains_cached_result;
MATCHER_P3(ContainsRangeOfOccurrencesCached, n, m, str, "") {
  const auto count = get_number_of_occurrences(arg, str);
  std::cout << "ContainsRangeOfOccurrencesCached(" << n << ", " << m
            << ") seen " << count << " occurrences and ";
  if (n <= static_cast<int>(count) && static_cast<int>(count) <= m) {
    contains_cached_result = true;
    std::cout << "matched." << std::endl;
  } else {
    std::cout << "did not match." << std::endl;
  }
  return true;
}

class FatalSignalDeathTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    initializer.SetUp();
  }
  void TearDown() override { initializer.TearDown(); }

  Server_initializer initializer;

  const std::string expected_backtrace_string =
      "Attempting backtrace. You can use the following "
      "information to find out";
};

TEST_F(FatalSignalDeathTest, Abort) {
#if defined(_WIN32)
  EXPECT_DEATH_IF_SUPPORTED(
      abort(), ContainsRangeOfOccurrences(1, 1, " UTC - mysqld got exception"));
#else
  EXPECT_DEATH_IF_SUPPORTED(
      abort(), ContainsRangeOfOccurrences(1, 1, " UTC - mysqld got signal 6"));
#endif
}

TEST_F(FatalSignalDeathTest, CrashOnMyAbort) {
  EXPECT_DEATH_IF_SUPPORTED(
      my_abort(), ContainsRangeOfOccurrences(1, 1, expected_backtrace_string));
}
TEST_F(FatalSignalDeathTest, CrashOnTerminate) {
  EXPECT_DEATH_IF_SUPPORTED(
      std::terminate(),
      ContainsRangeOfOccurrences(1, 1, expected_backtrace_string));
}

static void test_parallel_crash() {
  thread::Notification go;
  thread::Notification ready[10];
  auto test = [&ready, &go](int i) {
    my_thread_init();
    ready[i].notify();
    go.wait_for_notification();
    my_abort();
    my_thread_end();
  };
  std::thread *t[10];
  for (int i = 0; i < 10; ++i) {
    t[i] = new std::thread{test, i};
  }
  for (int i = 0; i < 10; ++i) {
    ready[i].wait_for_notification();
  }
  go.notify();
  for (int i = 0; i < 10; ++i) {
    t[i]->join();
    delete t[i];
  }
}

TEST_F(FatalSignalDeathTest, CrashOnParallelAbort) {
  contains_cached_result = false;
  for (size_t count = 0; count < 1000 && !contains_cached_result; ++count) {
    EXPECT_DEATH_IF_SUPPORTED(
        test_parallel_crash(),
        ContainsRangeOfOccurrencesCached(2, 10, expected_backtrace_string));
  }
  EXPECT_TRUE(contains_cached_result);
}

TEST_F(FatalSignalDeathTest, Segfault) {
#if defined(_WIN32)
  int *pint = NULL;
  /*
   After upgrading from gtest 1.5 to 1.6 this segfault is no longer
   caught by handle_fatal_signal(). We get an empty error message from the
   gtest library instead.
  */
  EXPECT_DEATH_IF_SUPPORTED(*pint = 42, "");
#elif defined(HAVE_ASAN)
/* gcc 4.8.1 with '-fsanitize=address -O1' */
/* Newer versions of ASAN give other error message, disable it */
// EXPECT_DEATH_IF_SUPPORTED(*pint= 42, ".*ASAN:SIGSEGV.*");
#elif defined(__APPLE__) && defined(__aarch64__) && defined(NDEBUG)
  // Disable also in non-debug mode on MacOS 11 arm, with -O1 or above, we get
  // Result: died but not with expected error.
  // Expected: contains regular expression ".* UTC - mysqld got signal .*"
  // Actual msg:
  // We do get: "Trace/BPT trap: 5" but not as part of the matcher input in
  // EXPECT_DEATH(statement, matcher);
#elif defined(HANDLE_FATAL_SIGNALS)
  int *pint = nullptr;
  /*
   On most platforms we get SIGSEGV == 11, but SIGBUS == 10 is also possible.
   And on Mac OsX we can get SIGILL == 4 (but only in optimized mode).
  */
  EXPECT_DEATH_IF_SUPPORTED(*pint = 42, ".* UTC - mysqld got signal .*");
#endif
}

// Verifies that my_safe_utoa behaves like sprintf(_, "%llu", _)
TEST(PrintUtilities, Utoa) {
  char buff[22];
  ulonglong intarr[] = {0, 1, 8, 12, 1234, 88888, ULONG_MAX, ULLONG_MAX};
  char sprintbuff[22];
  for (size_t ix = 0; ix < array_elements(intarr); ++ix) {
    char *my_res;
    sprintf(sprintbuff, "%llu", intarr[ix]);
    my_res = my_safe_utoa(10, intarr[ix], &buff[sizeof(buff) - 1]);
    EXPECT_STREQ(sprintbuff, my_res);

    if (intarr[ix] <= ULONG_MAX) {
      sprintf(sprintbuff, "%lu", (ulong)intarr[ix]);
      my_res = my_safe_utoa(10, (ulong)intarr[ix], &buff[sizeof(buff) - 1]);
      EXPECT_STREQ(sprintbuff, my_res);
    }
  }
}

// Verifies that my_safe_itoa behaves like sprintf(_, "%lld", _)
TEST(PrintUtilities, Itoa) {
  char buff[22];
  char sprintbuff[22];
  longlong intarr[] = {0, 1, 8, 12, 1234, 88888, LONG_MAX, LLONG_MAX};

  for (size_t ix = 0; ix < array_elements(intarr); ++ix) {
    char *my_res;
    sprintf(sprintbuff, "%lld", intarr[ix]);
    my_res = my_safe_itoa(10, intarr[ix], &buff[sizeof(buff) - 1]);
    EXPECT_STREQ(sprintbuff, my_res);

    ll2str(intarr[ix], buff, 10, false);
    EXPECT_STREQ(sprintbuff, buff);

    sprintf(sprintbuff, "%lld", -intarr[ix]);
    my_res = my_safe_itoa(10, -intarr[ix], &buff[sizeof(buff) - 1]);
    EXPECT_STREQ(sprintbuff, my_res);

    // This one fails ....
    // ll2str(-intarr[ix], buff, 10, false);
    // EXPECT_STREQ(sprintbuff, buff)
    //  << "failed for " << -intarr[ix];

    sprintf(sprintbuff, "%llx", intarr[ix]);
    my_res = my_safe_itoa(16, intarr[ix], &buff[sizeof(buff) - 1]);
    EXPECT_STREQ(sprintbuff, my_res);

    ll2str(intarr[ix], buff, 16, false);
    EXPECT_STREQ(sprintbuff, buff);

    sprintf(sprintbuff, "%llx", -intarr[ix]);
    my_res = my_safe_itoa(16, -intarr[ix], &buff[sizeof(buff) - 1]);
    EXPECT_STREQ(sprintbuff, my_res) << "failed for " << -intarr[ix];

    ll2str(-intarr[ix], buff, 16, false);
    EXPECT_STREQ(sprintbuff, buff);
  }
}

// Various tests for my_safe_snprintf.
TEST(PrintUtilities, Printf) {
  char buff[512];
  char sprintfbuff[512];
  const char *null_str = nullptr;

  my_safe_snprintf(buff, sizeof(buff), "hello");
  EXPECT_STREQ("hello", buff);

  my_safe_snprintf(buff, sizeof(buff), "hello %s hello", "hello");
  EXPECT_STREQ("hello hello hello", buff);
  my_safe_snprintf(buff, sizeof(buff), "hello %s hello", null_str);
  EXPECT_STREQ("hello (null) hello", buff);

  my_safe_snprintf(buff, sizeof(buff), "hello %d hello", 42);
  EXPECT_STREQ("hello 42 hello", buff);
  my_safe_snprintf(buff, sizeof(buff), "hello %i hello", 42);
  EXPECT_STREQ("hello 42 hello", buff);
  my_safe_snprintf(buff, sizeof(buff), "hello %u hello", (unsigned)42);
  EXPECT_STREQ("hello 42 hello", buff);

  my_safe_snprintf(buff, sizeof(buff), "hello %llu hello", ULLONG_MAX);
  sprintf(sprintfbuff, "hello %llu hello", ULLONG_MAX);
  EXPECT_STREQ(sprintfbuff, buff);

  my_safe_snprintf(buff, sizeof(buff), "hello %x hello", 42);
  EXPECT_STREQ("hello 2a hello", buff);

  my_safe_snprintf(buff, sizeof(buff), "hello %x hello", -42);
  sprintf(sprintfbuff, "hello %x hello", -42);
  EXPECT_STREQ("hello ffffffd6 hello", buff);
  EXPECT_STREQ(sprintfbuff, buff);

  my_safe_snprintf(buff, sizeof(buff), "hello %llx hello", (longlong)-42);
  sprintf(sprintfbuff, "hello %llx hello", (longlong)-42);
  EXPECT_STREQ("hello ffffffffffffffd6 hello", buff);
  EXPECT_STREQ(sprintfbuff, buff);

  void *p = this;
  my_safe_snprintf(buff, sizeof(buff), "hello 0x%p hello", p);
  snprintf(sprintfbuff, sizeof(sprintfbuff), "hello 0x%llx hello",
           (long long)p);
  EXPECT_STREQ(sprintfbuff, buff) << "my_safe_snprintf:" << buff;
}

}  // namespace segfault_unittest
