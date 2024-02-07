/*
  Copyright (c) 2017, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <chrono>
#include <thread>

#include <gmock/gmock.h>

#include "router_component_test.h"

/** @file
 *
 * These tests are special - they test our component test framework rather than
 * our Router. For the tests we do here we need tailored simple executables
 * which communicate with our tests, serving as the other side of the test. To
 * avoid the overhead of having to create many small programs, instead we inline
 * their code here, inside of special disabled testcases, and we launch those
 * specific testcases from their corresponding real testcase. This is why all
 * the following tests are arranged in pairs, with names:
 *
 *   - "<test_description>_tester" (the test)
 *   - "DISABLED_<test_description>_testee" (the inlined executable)
 *
 * It's a hack, but it works.
 *
 */

using namespace std::chrono_literals;

const char *g_this_exec_path;

constexpr auto kSleepDuration =
    2000ms;  // you may want to decrease this to speed up tests

class ComponentTestFrameworkTest : public RouterComponentTest {
 protected:
  static std::string show_output(ProcessWrapper &process,
                                 const std::string &process_description) {
    return process_description + ":\n" + process.get_full_output() +
           "-(end)-\n";
  }

  const std::string arglist_prefix_ =
      "--gtest_filter=ComponentTestFrameworkTest.DISABLED_";
};

static void autoresponder_testee(
    std::chrono::milliseconds leak_interval = 0ms) {
  if (leak_interval.count()) {
    auto slow_cout = [leak_interval](char c) {
      std::cout << c << std::flush;
      std::this_thread::sleep_for(leak_interval);
    };

    slow_cout('S');
    slow_cout('y');
    slow_cout('n');
    std::cout << std::endl;
  } else {
    std::cout << "Syn" << std::endl;
  }

  // react to 1st autoresponse
  {
    std::string response;
    std::cin >> response;
    if (response == "Syn+Ack") {
      // we're good
      std::cout << "Ack" << std::endl;
      std::cout << "Fin" << std::endl;
    } else {
      // unexpected response
      std::cout << "Reset" << std::endl;
    }
  }

  // react to 2nd autoresponse
  {
    std::string response;
    std::cin >> response;
    if (response == "Ack") {
      // we're good
      std::cout << "OK" << std::endl;
    } else {
      // unexpected response
      std::cout << "UNEXPECTED" << std::endl;
    }
  }
}

TEST_F(ComponentTestFrameworkTest, autoresponder_simple_tester) {
  /**
   * @test This test tests framework's autoresponder in a simple scenario
   *
   * @note GTest will automatically add more of its own lines
   *       before and after our stuff, giving this scenario a twist:
   *       First read the autoresponder will see is very likely to contain
   *       multiple lines. Thus this test turns into a test that also tests
   *       autoresponder's ability to deal with multiple lines.
   */

  // In this test we explain what's going on, use this test as a guideline
  // for understanding other tests.

  // launch the DISABLED_autoresponder_simple_testee testcase as a separate
  // executable
  const ProcessWrapper::OutputResponder responder{
      [](const std::string &line) -> std::string {
        if (line == "Syn") return "Syn+Ack\n";
        if (line == "Fin") return "Ack\n";

        return "";
      }};

  auto &testee =
      launch_command(g_this_exec_path,
                     {"--gtest_also_run_disabled_tests",
                      arglist_prefix_ + "autoresponder_simple_testee"},
                     EXIT_SUCCESS, true, -1ms, responder);

  // test for what should come out
  // NOTE: expect_output() will keep reading and autoresponding to output,
  //       until it encounters the string we passed as an argument
  EXPECT_TRUE(testee.expect_output("Syn\nAck\nFin\nOK"))
      << show_output(testee, "ROUTER OUTPUT");

  // wait for child
  check_exit_code(testee, EXIT_SUCCESS);
}

TEST_F(ComponentTestFrameworkTest, DISABLED_autoresponder_simple_testee) {
  autoresponder_testee();
}

// TODO: Re-enable this test after fixing autoresponder (reported as
// BUG#27035695)
//
// This test will fail if [THIS_LINE] is removed, because autoresponder is
// buggy. [THIS_LINE] causes "Syn\n" to be read in entirety by 1 read(), instead
// of being read 1 byte at a time.  As soon as [THIS_LINE] is removed,
// autoresponder will have to deal with each byte separately, and that's when it
// fails.
#if 0
TEST_F(ComponentTestFrameworkTest, autoresponder_segmented_triggers_tester) {
  /**
   * @test This test tests is just like autoresponder_simple_tester, but the
   * testee will send back "Syn\n" (first autorespond trigger) one byte at a
   * time. It verifies that autoresponder can properly deal with segmented lines
   *       (which may also be a result of short read() due to full buffer)
   */

  auto &testee = launch_command(
      g_this_exec_path,
      {"--gtest_also_run_disabled_tests",
       arglist_prefix_ + "autoresponder_simple_testee"},
      EXIT_SUCCESS, true, -1ms, {{"Syn", "Syn+Ack\n"}, {"Fin", "Ack\n"}});

  EXPECT_TRUE(testee.expect_output("Syn\nAck\nFin\nOK", false))
      << show_output(testee, "ROUTER OUTPUT");

  check_exit_code(testee, EXIT_SUCCESS);
}

TEST_F(ComponentTestFrameworkTest, autoresponder_segmented_triggers_testee) {
  autoresponder_testee(100ms);
}
#endif

static void sleepy_testee() {
  std::cout << "Hello, I'm feeling sleepy. Yawn." << std::endl;
  std::this_thread::sleep_for(kSleepDuration);
  std::cout << "Yes, I'm still alive." << std::endl;
}

TEST_F(ComponentTestFrameworkTest, sleepy_tester) {
  /**
   * @test This test verifies framework's behavior when the process is silent
   * for a longer period of time
   */

  ProcessWrapper &testee = launch_command(
      g_this_exec_path,
      {"--gtest_also_run_disabled_tests", arglist_prefix_ + "sleepy_testee"});

  // first and second sentence should arrive kSleepDurationMs ms apart,
  // expect_output() should not give up reading during that time
  EXPECT_TRUE(testee.expect_output(
      "Hello, I'm feeling sleepy. Yawn.\nYes, I'm still alive.\n", false,
      kSleepDuration + kSleepDuration / 2))
      << show_output(testee, "TESTED PROCESS");

  check_exit_code(testee, EXIT_SUCCESS);
}
TEST_F(ComponentTestFrameworkTest, DISABLED_sleepy_testee) { sleepy_testee(); }

TEST_F(ComponentTestFrameworkTest, sleepy_blind_tester) {
  /**
   * @test This test is similar to sleepy_tester(), but this time we just wait
   * for the child without looking at its output. wait_for_exit() should consume
   * it.
   */

  ProcessWrapper &testee = launch_command(
      g_this_exec_path, {"--gtest_also_run_disabled_tests",
                         arglist_prefix_ + "sleepy_blind_testee"});

  EXPECT_EQ(testee.wait_for_exit(kSleepDuration + kSleepDuration / 2), 0);
}
TEST_F(ComponentTestFrameworkTest, DISABLED_sleepy_blind_testee) {
  sleepy_testee();
}

TEST_F(ComponentTestFrameworkTest, sleepy_blind_autoresponder_tester) {
  /**
   * @test This tests a particular scenario that used to trigger a bug: the
   * child is silent for a while before writing a prompt and blocking while
   * awaiting our response. The buggy wait_for_exit() used to attempt reading
   * (with autoresponder active) for a while, then (while the child was silent)
   * it assumed no more output would follow, and moved on to just waiting for
   * the child to close (no longer attempting to read and autorespond). When the
   * child eventually prompted for password, wait_for_exit() would not "hear
   * it", resulting in a deadlock and eventually timing out with error: "Timed
   * out waiting for the process to exit: No child processes"
   */
  const ProcessWrapper::OutputResponder responder{
      [](const std::string &line) -> std::string {
        if (line == "Syn") return "Syn+Ack\n";
        if (line == "Fin") return "Ack\n";

        return "";
      }};

  auto &testee =
      launch_command(g_this_exec_path,
                     {"--gtest_also_run_disabled_tests",
                      arglist_prefix_ + "sleepy_blind_autoresponder_testee"},
                     EXIT_SUCCESS, true, -1ms, responder);

  // wait for child (while reading and issuing autoresponses)
  EXPECT_EQ(testee.wait_for_exit(kSleepDuration + kSleepDuration / 2), 0);
}
TEST_F(ComponentTestFrameworkTest, DISABLED_sleepy_blind_autoresponder_testee) {
  std::this_thread::sleep_for(kSleepDuration);
  autoresponder_testee();
}

TEST_F(ComponentTestFrameworkTest, wait_for_exit_with_low_timeout_tester) {
  /**
   * @test This test verifies that calling ProcessWrapper::wait_for_exit() with
   * a very low timeout (0 in this case) will behave as expected (throw
   * std::system_error due to timeout).
   */

  ProcessWrapper &testee = launch_command(
      g_this_exec_path,
      {"--gtest_also_run_disabled_tests",
       arglist_prefix_ + "wait_for_exit_with_low_timeout_testee"});

  // wait with very short timeout
  EXPECT_THROW_LIKE(testee.wait_for_exit(std::chrono::seconds(0)),
                    std::system_error,
                    make_error_code(std::errc::timed_out).message());

  // now let's just wait for the process to shut down naturally (test cleanup)
  EXPECT_EQ(testee.wait_for_exit(kSleepDuration + kSleepDuration / 2), 0);
}
TEST_F(ComponentTestFrameworkTest,
       DISABLED_wait_for_exit_with_low_timeout_testee) {
  sleepy_testee();
}

int main(int argc, char *argv[]) {
  g_this_exec_path = argv[0];

  init_windows_sockets();
  ProcessManager::set_origin(Path(argv[0]).dirname());
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
