/*
  Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

////////////////////////////////////////////////////////////////////////////////
//
// These unit tests test the harness for proper plugin lifecycle management.
// They focus on four plugin API functions: init(), start(), stop() and
// deinit(). A special plugin was written (lifecycle.cc) which is the workhorse
// of these tests. It has configurable exit strategies (see comments in the
// source file), which allows us to test different scenarios.
// Also, another secondary plugin was written (lifecycle2.cc); that one is much
// simpler and it has an (artificial) dependency on lifecycle.cc plugin. It is
// used to help in testing initialisation/deinitialisation behaviour.
//
// Since we have to shutdown the harness many times, we also test the harness
// shutdown functionality (signal handling) while testing plugin lifecycle.
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//
// lifecycle test plugins dependency chart:
//
//                                           ,--(depends on)--> lifecycle3
// lifecycle2 --(depends on)--> lifecycle --<
//                                           `--(depends on)--> magic
//
////////////////////////////////////////////////////////////////////////////////

// must have this first, before #includes that rely on it
#include <gtest/gtest_prod.h>

////////////////////////////////////////
// Harness include files
#include "exception.h"
#include "filesystem.h"
#include "lifecycle.h"
#include "loader.h"
#include "mysql/harness/logging/registry.h"
#include "mysql/harness/plugin.h"
#include "test/helpers.h"
#include "utilities.h"

////////////////////////////////////////
// Third-party include files
#include "gmock/gmock.h"
#include "gtest/gtest.h"

////////////////////////////////////////
// Standard include files
#include <signal.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <future>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// see loader.cc for more info on this define
#ifndef _WIN32
#define USE_POSIX_SIGNALS
#endif

#define USE_DLCLOSE 1

// disable dlclose() when built with lsan
//
// clang has __has_feature(address_sanitizer)
// gcc has __SANITIZE_ADDRESS__
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#undef USE_DLCLOSE
#define USE_DLCLOSE 0
#endif
#endif

#if defined(__SANITIZE_ADDRESS__) && __SANITIZE_ADDRESS__ == 1
#undef USE_DLCLOSE
#define USE_DLCLOSE 0
#endif

using ::testing::HasSubstr;
using mysql_harness::Loader;
using mysql_harness::Path;
using mysql_harness::Plugin;
using mysql_harness::test::LifecyclePluginSyncBus;
namespace ch = std::chrono;

// try increasing these if unit tests fail
const int kSleepShutdown = 10;

Path g_here;

class TestLoader : public Loader {
 public:
  TestLoader(const std::string &program, mysql_harness::LoaderConfig &config)
      : Loader(program, config) {
    unittest_backdoor::set_shutdown_pending(0);
  }

  // lifecycle plugin exposes all four lifecycle functions. But we can
  // override any of them into nullptr using this struct in
  // init_lifecycle_plugin()
  struct ApiFunctionEnableSwitches {
    bool init;
    bool start;
    bool stop;
    bool deinit;
  };

  void read(std::istream &stream) {
    config_.Config::read(stream);
    config_.fill_and_check();
  }

  // Loader::load_all() with ability to disable functions
  void load_all(ApiFunctionEnableSwitches switches) {
    Loader::load_all();
    init_lifecycle_plugin(switches);
  }

  LifecyclePluginSyncBus &get_msg_bus_from_lifecycle_plugin(const char *key) {
    return *lifecycle_plugin_itc_->get_bus_from_key(key);
  }

 private:
  void init_lifecycle_plugin(ApiFunctionEnableSwitches switches) {
    Plugin *plugin = plugins_.at("lifecycle").plugin;

#if !USE_DLCLOSE
    // with address sanitizer we don't unload the plugin which means
    // the overwritten plugin hooks don't get reset to their initial values
    //
    // we need to capture original pointers and reset them
    // each time
    static Plugin virgin_plugin = *plugin;
    *plugin = virgin_plugin;
#endif

    // signal plugin to reset state and init our lifecycle_plugin_itc_
    // (we use a special hack (tag the pointer with last bit=1) to tell it that
    // this is not a normal init() call, but a special call meant to
    // pre-initialize the plugin for the test). In next line, +1 = pointer tag
    uintptr_t ptr = reinterpret_cast<uintptr_t>(&lifecycle_plugin_itc_) + 1;
    plugin->init(reinterpret_cast<mysql_harness::PluginFuncEnv *>(ptr));

    // override plugin functions as requested
    if (!switches.init) plugin->init = nullptr;
    if (!switches.start) plugin->start = nullptr;
    if (!switches.stop) plugin->stop = nullptr;
    if (!switches.deinit) plugin->deinit = nullptr;
  }

  // struct to expose additional things we need from lifecycle plugin,
  // pointer owner = lifecycle plugin
  mysql_harness::test::LifecyclePluginITC *lifecycle_plugin_itc_;
};  // class TestLoader

class BasicConsoleOutputTest : public ::testing::Test {
 protected:
  void clear_log() {
    log.str("");
    log.clear();
  }

  std::stringstream log;

 private:
  void SetUp() {
    std::ostream *log_stream =
        mysql_harness::logging::get_default_logger_stream();

    orig_log_stream_ = log_stream->rdbuf();
    log_stream->rdbuf(log.rdbuf());
  }

  void TearDown() {
    if (orig_log_stream_) {
      std::ostream *log_stream =
          mysql_harness::logging::get_default_logger_stream();
      log_stream->rdbuf(orig_log_stream_);
    }
  }

  std::streambuf *orig_log_stream_;
};

class LifecycleTest : public BasicConsoleOutputTest {
 public:
  LifecycleTest()
      : params_{{"program", "harness"}, {"prefix", g_here.c_str()}},
        config_(params_, std::vector<std::string>(),
                mysql_harness::Config::allow_keys),
        loader_("harness", config_) {
    const std::string test_data_dir =
        mysql_harness::get_tests_data_dir(g_here.str());
    config_text_ << "[DEFAULT]                                      \n"
                    "logging_folder =                               \n"
                    "plugin_folder  = " +
                        mysql_harness::get_plugin_dir(g_here.str()) + "\n" +
                        "runtime_folder = " + test_data_dir + "\n" +
                        "config_folder  = " + test_data_dir + "\n" +
                        "data_folder    = " + test_data_dir + "\n" +
                        // TODO: restore this after after [logger] hack is
                        // reverted (grep for g_HACK_default_log_level)
                        //    " \n"
                        //    "[logger] \n" "level = DEBUG \n"
                        "                                               \n"
                        "[lifecycle3]                                   \n"
                        "                                               \n"
                        "[magic]                                        \n"
                        "suki = magic                                   \n"
                        "                                               \n"
                        "[lifecycle:instance1]                          \n";
  }

  void init_test(std::istream &config_text,
                 TestLoader::ApiFunctionEnableSwitches switches = {
                     true, true, true, true}) {
    loader_.read(config_text);
    loader_.load_all(switches);
    clear_log();
  }

  void init_test_without_lifecycle_plugin(std::istream &config_text) {
    loader_.read(config_text);
    clear_log();
  }

  void refresh_log() {
    // the getline() loop below runs until EOF, therefore on subsequent calls
    // we need to clear the EOF flag before we can read again
    log.clear();

    std::string line;
    while (std::getline(log, line)) {
      log_lines_.push_back(line);
    }
  }

  // NOTE:
  // Despite the name, LifecyclePluginSyncBus is additionally used for 2-way
  // synchronisation (please rename it if you have a better name). This is
  // because if we freeze_bus(), an attempt to pass another message from plugin
  // will block it, until we unfreeze_and_wait_for_msg().

  LifecyclePluginSyncBus &msg_bus(const char *key) {
    return loader_.get_msg_bus_from_lifecycle_plugin(key);
  }

  void freeze_bus(LifecyclePluginSyncBus &bus) {
    bus.mtx.lock();  // so that we don't miss a signal
  }

  void unfreeze_and_wait_for_msg(LifecyclePluginSyncBus &bus, const char *msg) {
    std::unique_lock<std::mutex> lock(bus.mtx, std::adopt_lock);

    // block until we receive message we're interested in
    bus.cv.wait(lock,
                [&bus, msg]() { return bus.msg.find(msg) != bus.msg.npos; });
  }

  long count_in_log(const char *needle) {
    long cnt = 0;
    for (const std::string &line : log_lines_)
      if (line.find(needle) != line.npos) cnt++;
    return cnt;
  }

  const std::map<std::string, std::string> params_;
  mysql_harness::LoaderConfig config_;
  TestLoader loader_;
  std::stringstream config_text_;

  std::vector<std::string> log_lines_;
};  // class LifecycleTest

void delayed_shutdown() {
  std::this_thread::sleep_for(ch::milliseconds(kSleepShutdown));
  request_application_shutdown();
}

int time_diff(const ch::time_point<ch::steady_clock> &t0,
              const ch::time_point<ch::steady_clock> &t1) {
  ch::milliseconds duration = ch::duration_cast<ch::milliseconds>(t1 - t0);
  return static_cast<int>(duration.count());
}

void run_then_signal_shutdown(const std::function<void()> &l) {
  ch::time_point<ch::steady_clock> t0 = ch::steady_clock::now();
  std::thread(delayed_shutdown).detach();
  l();
  ch::time_point<ch::steady_clock> t1 = ch::steady_clock::now();
  EXPECT_LE(kSleepShutdown, time_diff(t0, t1));
}

////////////////////////////////////////////////////////////////////////////////
//
// UNIT TESTS: PLATFORM SPECIFIC STUFF
//
////////////////////////////////////////////////////////////////////////////////

TEST(StdLibrary, FutureWaitUntil) {
  // Here we test undocumented/ambiguous behaviour of std::future::wait_until():
  // what will be returned when you call it when future is ready BUT
  // timeout is expired? In principle, both return values are plausible:
  //
  //   future_status::ready
  //   future_status::timeout
  //
  // On Ubuntu 14.04, it returns future_status::ready, which seems reasonable.
  // However to ensure it works all platforms, we have a unit test here to to
  // guard against a nasty surprise in Loader::main_loop() which relies on this
  // behaviour.

  // fulfill the promise
  std::promise<int> p;
  std::future<int> f = p.get_future();
  p.set_value(42);

  // set timeout
  ch::steady_clock::time_point timepoint =
      ch::steady_clock::now() + ch::milliseconds(10);

  // sleep beyond the timeout
  std::this_thread::sleep_for(ch::milliseconds(30));

  // wait_until() should return that our future is ready,
  // regardless of the expired timeout
  std::future_status status = f.wait_until(timepoint);
  ASSERT_EQ(std::future_status::ready, status);
  EXPECT_EQ(42, f.get());
}

////////////////////////////////////////////////////////////////////////////////
//
// UNIT TESTS: SIMPLE
//
////////////////////////////////////////////////////////////////////////////////

// 2017.03.24: at the time of writing, this is what the "meat" of Loader looked
// like. In the tests below, load_all() is executed in TestLoader::load_all()
// (called from init_test()), and run() (either as a whole or in parts)
// should be called directly from unit tests.
//
//   void Loader::start() {
//     load_all();
//     std::exception_ptr first_eptr = run();
//     unload_all();
//
//     if (first_eptr) {
//       std::rethrow_exception(first_eptr);
//     }
//   }
//
//   void Loader::load_all() {
//     platform_specific_init();
//     for (std::pair<const std::string&, std::string> name : available()) {
//       load(name.first, name.second);
//     }
//   }
//
//   std::exception_ptr Loader::run() {
//
//     // initialize plugins
//     std::exception_ptr first_eptr = init_all();
//
//     // run plugins if initialization didn't fail
//     if (!first_eptr) {
//       start_all();  // if start() throws, exception is forwarded to
//       main_loop() first_eptr = main_loop(); // calls stop_all() before
//       exiting
//     }
//     assert(plugin_start_env_.empty());  // stop_all() should have ran and
//     cleaned them up
//
//     // deinitialize plugins
//     std::exception_ptr tmp = deinit_all();
//     if (!first_eptr) {
//       first_eptr = tmp;
//     }
//
//     // return the first exception that was triggered by an error returned
//     from
//     // any plugin function
//     return first_eptr;
//   }

TEST_F(LifecycleTest, Simple_None) {
  init_test(config_text_, {false, false, false, false});

  EXPECT_EQ(loader_.init_all(), nullptr);
  loader_.start_all();
  EXPECT_EQ(loader_.main_loop(), nullptr);
  EXPECT_EQ(loader_.deinit_all(), nullptr);

  const std::list<std::string> initialized = {"magic", "lifecycle3",
                                              "lifecycle"};
  EXPECT_EQ(initialized, loader_.order_);

  refresh_log();
  EXPECT_EQ(0, count_in_log("lifecycle:all init():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:all init():EXIT"));
  EXPECT_EQ(0, count_in_log("lifecycle:instance1 start():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:instance1 start():EXIT"));
  EXPECT_EQ(0, count_in_log("lifecycle:instance1 stop():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:instance1 stop():EXIT"));
  EXPECT_EQ(0, count_in_log("lifecycle:all deinit():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:all deinit():EXIT"));
}

TEST_F(LifecycleTest, Simple_AllFunctions) {
  config_text_ << "init   = exit           \n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n"
               << "deinit = exit           \n";
  init_test(config_text_, {true, true, true, true});
  LifecyclePluginSyncBus &bus = msg_bus("instance1");

  EXPECT_EQ(loader_.init_all(), nullptr);
  freeze_bus(bus);
  loader_.start_all();
  unfreeze_and_wait_for_msg(
      bus, "lifecycle:instance1 start():EXIT_ON_STOP:sleeping");

  const std::list<std::string> initialized = {"magic", "lifecycle3",
                                              "lifecycle"};
  EXPECT_EQ(initialized, loader_.order_);

  refresh_log();
  EXPECT_EQ(1, count_in_log("lifecycle:all init():begin"));
  EXPECT_EQ(1, count_in_log("lifecycle:all init():EXIT."));
  EXPECT_EQ(1, count_in_log("lifecycle:instance1 start():begin"));
  EXPECT_EQ(1,
            count_in_log("lifecycle:instance1 start():EXIT_ON_STOP:sleeping"));
  EXPECT_EQ(0, count_in_log("lifecycle:instance1 start():EXIT_ON_STOP:done"));
  EXPECT_EQ(0, count_in_log("lifecycle:instance1 stop():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:instance1 stop():EXIT"));
  EXPECT_EQ(0, count_in_log("lifecycle:all deinit():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:all deinit():EXIT"));

  // signal shutdown after 10ms, main_loop() should block until then
  run_then_signal_shutdown([&]() { EXPECT_EQ(loader_.main_loop(), nullptr); });

  refresh_log();
  EXPECT_EQ(1, count_in_log("Shutting down. Stopping all plugins."));
  EXPECT_EQ(1, count_in_log("lifecycle:instance1 start():EXIT_ON_STOP:done"));
  EXPECT_EQ(1, count_in_log("lifecycle:instance1 stop():begin"));
  EXPECT_EQ(1, count_in_log("lifecycle:instance1 stop():EXIT."));
  EXPECT_EQ(0, count_in_log("lifecycle:all deinit():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:all deinit():EXIT."));

  EXPECT_EQ(loader_.deinit_all(), nullptr);

  refresh_log();
  EXPECT_EQ(1, count_in_log("lifecycle:all deinit():begin"));
  EXPECT_EQ(1, count_in_log("lifecycle:all deinit():EXIT."));
}

TEST_F(LifecycleTest, Simple_Init) {
  config_text_ << "init = exit\n";
  init_test(config_text_, {true, false, false, false});

  EXPECT_EQ(loader_.init_all(), nullptr);
  loader_.start_all();
  EXPECT_EQ(loader_.main_loop(), nullptr);
  EXPECT_EQ(loader_.deinit_all(), nullptr);

  const std::list<std::string> initialized = {"magic", "lifecycle3",
                                              "lifecycle"};
  EXPECT_EQ(initialized, loader_.order_);

  refresh_log();
  EXPECT_EQ(1, count_in_log("lifecycle:all init():begin"));
  EXPECT_EQ(1, count_in_log("lifecycle:all init():EXIT"));
  EXPECT_EQ(0, count_in_log("lifecycle:instance1 start():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:instance1 start():EXIT"));
  EXPECT_EQ(0, count_in_log("lifecycle:instance1 stop():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:instance1 stop():EXIT"));
  EXPECT_EQ(0, count_in_log("lifecycle:all deinit():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:all deinit():EXIT"));
}

TEST_F(LifecycleTest, Simple_StartStop) {
  config_text_ << "start = exitonstop\n";
  config_text_ << "stop  = exit\n";
  init_test(config_text_, {false, true, true, false});
  LifecyclePluginSyncBus &bus = msg_bus("instance1");

  EXPECT_EQ(loader_.init_all(), nullptr);
  freeze_bus(bus);
  loader_.start_all();
  unfreeze_and_wait_for_msg(
      bus, "lifecycle:instance1 start():EXIT_ON_STOP:sleeping");

  const std::list<std::string> initialized = {"magic", "lifecycle3",
                                              "lifecycle"};
  EXPECT_EQ(initialized, loader_.order_);

  refresh_log();
  EXPECT_EQ(0, count_in_log("lifecycle:all init():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:all init():EXIT."));
  EXPECT_EQ(1, count_in_log("lifecycle:instance1 start():begin"));
  EXPECT_EQ(1,
            count_in_log("lifecycle:instance1 start():EXIT_ON_STOP:sleeping"));
  EXPECT_EQ(0, count_in_log("lifecycle:instance1 start():EXIT_ON_STOP:done"));
  EXPECT_EQ(0, count_in_log("lifecycle:instance1 stop():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:instance1 stop():EXIT"));
  EXPECT_EQ(0, count_in_log("lifecycle:all deinit():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:all deinit():EXIT"));

  // signal shutdown after 10ms, main_loop() should block until then
  run_then_signal_shutdown([&]() { EXPECT_EQ(loader_.main_loop(), nullptr); });

  EXPECT_EQ(loader_.deinit_all(), nullptr);

  refresh_log();
  EXPECT_EQ(1, count_in_log("Shutting down. Stopping all plugins."));
  EXPECT_EQ(1, count_in_log("lifecycle:instance1 start():EXIT_ON_STOP:done"));
  EXPECT_EQ(1, count_in_log("lifecycle:instance1 stop():begin"));
  EXPECT_EQ(1, count_in_log("lifecycle:instance1 stop():EXIT."));
  EXPECT_EQ(0, count_in_log("lifecycle:all deinit():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:all deinit():EXIT."));
}

TEST_F(LifecycleTest, Simple_StartStopBlocking) {
  // Same test as Simple_StartStop, but start() uses blocking API call to wait
  // until told to shut down, vs actively polling the "running" flag

  config_text_ << "start = exitonstop_s\n";  // <--- note the "_s" postfix
  config_text_ << "stop  = exit\n";
  init_test(config_text_, {false, true, true, false});
  LifecyclePluginSyncBus &bus = msg_bus("instance1");

  EXPECT_EQ(loader_.init_all(), nullptr);
  freeze_bus(bus);
  loader_.start_all();
  unfreeze_and_wait_for_msg(
      bus, "lifecycle:instance1 start():EXIT_ON_STOP_SYNC:sleeping");

  const std::list<std::string> initialized = {"magic", "lifecycle3",
                                              "lifecycle"};
  EXPECT_EQ(initialized, loader_.order_);

  refresh_log();
  EXPECT_EQ(0, count_in_log("lifecycle:all init():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:all init():EXIT."));
  EXPECT_EQ(1, count_in_log("lifecycle:instance1 start():begin"));
  EXPECT_EQ(1, count_in_log(
                   "lifecycle:instance1 start():EXIT_ON_STOP_SYNC:sleeping"));
  EXPECT_EQ(0,
            count_in_log("lifecycle:instance1 start():EXIT_ON_STOP_SYNC:done"));
  EXPECT_EQ(0, count_in_log("lifecycle:instance1 stop():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:instance1 stop():EXIT"));
  EXPECT_EQ(0, count_in_log("lifecycle:all deinit():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:all deinit():EXIT"));

  // signal shutdown after 10ms, main_loop() should block until then
  run_then_signal_shutdown([&]() { EXPECT_EQ(loader_.main_loop(), nullptr); });

  EXPECT_EQ(loader_.deinit_all(), nullptr);

  refresh_log();
  EXPECT_EQ(1, count_in_log("Shutting down. Stopping all plugins."));
  EXPECT_EQ(1,
            count_in_log("lifecycle:instance1 start():EXIT_ON_STOP_SYNC:done"));
  EXPECT_EQ(1, count_in_log("lifecycle:instance1 stop():begin"));
  EXPECT_EQ(1, count_in_log("lifecycle:instance1 stop():EXIT."));
  EXPECT_EQ(0, count_in_log("lifecycle:all deinit():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:all deinit():EXIT."));
}

TEST_F(LifecycleTest, Simple_Start) {
  config_text_ << "start = exitonstop\n";
  init_test(config_text_, {false, true, false, false});
  LifecyclePluginSyncBus &bus = msg_bus("instance1");

  EXPECT_EQ(loader_.init_all(), nullptr);
  freeze_bus(bus);
  loader_.start_all();
  unfreeze_and_wait_for_msg(
      bus, "lifecycle:instance1 start():EXIT_ON_STOP:sleeping");

  const std::list<std::string> initialized = {"magic", "lifecycle3",
                                              "lifecycle"};
  EXPECT_EQ(initialized, loader_.order_);

  refresh_log();
  EXPECT_EQ(0, count_in_log("lifecycle:all init():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:all init():EXIT."));
  EXPECT_EQ(1, count_in_log("lifecycle:instance1 start():begin"));
  EXPECT_EQ(1,
            count_in_log("lifecycle:instance1 start():EXIT_ON_STOP:sleeping"));
  EXPECT_EQ(0, count_in_log("lifecycle:instance1 start():EXIT_ON_STOP:done"));
  EXPECT_EQ(0, count_in_log("lifecycle:instance1 stop():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:instance1 stop():EXIT"));
  EXPECT_EQ(0, count_in_log("lifecycle:all deinit():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:all deinit():EXIT"));

  // signal shutdown after 10ms, main_loop() should block until then
  run_then_signal_shutdown([&]() { EXPECT_EQ(loader_.main_loop(), nullptr); });

  EXPECT_EQ(loader_.deinit_all(), nullptr);

  refresh_log();
  EXPECT_EQ(1, count_in_log("Shutting down. Stopping all plugins."));
  EXPECT_EQ(1, count_in_log("lifecycle:instance1 start():EXIT_ON_STOP:done"));
  EXPECT_EQ(0, count_in_log("lifecycle:instance1 stop():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:instance1 stop():EXIT."));
  EXPECT_EQ(0, count_in_log("lifecycle:all deinit():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:all deinit():EXIT."));
}

TEST_F(LifecycleTest, Simple_Stop) {
  config_text_ << "stop = exit\n";
  init_test(config_text_, {false, false, true, false});

  EXPECT_EQ(loader_.init_all(), nullptr);
  loader_.start_all();

  const std::list<std::string> initialized = {"magic", "lifecycle3",
                                              "lifecycle"};
  EXPECT_EQ(initialized, loader_.order_);

  refresh_log();
  EXPECT_EQ(0, count_in_log("lifecycle:all init():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:all init():EXIT"));
  EXPECT_EQ(0, count_in_log("lifecycle:instance1 start():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:instance1 start():EXIT"));
  EXPECT_EQ(0, count_in_log("lifecycle:instance1 stop():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:instance1 stop():EXIT"));
  EXPECT_EQ(0, count_in_log("lifecycle:all deinit():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:all deinit():EXIT"));

  EXPECT_EQ(loader_.main_loop(), nullptr);

  refresh_log();
  EXPECT_EQ(1, count_in_log("lifecycle:instance1 stop():begin"));
  EXPECT_EQ(1, count_in_log("lifecycle:instance1 stop():EXIT"));
  EXPECT_EQ(0, count_in_log("lifecycle:all deinit():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:all deinit():EXIT"));

  EXPECT_EQ(loader_.deinit_all(), nullptr);

  refresh_log();
  EXPECT_EQ(0, count_in_log("lifecycle:all deinit():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:all deinit():EXIT"));
}

TEST_F(LifecycleTest, Simple_Deinit) {
  config_text_ << "deinit = exit\n";
  init_test(config_text_, {false, false, false, true});

  EXPECT_EQ(loader_.init_all(), nullptr);
  loader_.start_all();
  EXPECT_EQ(loader_.main_loop(), nullptr);

  const std::list<std::string> initialized = {"magic", "lifecycle3",
                                              "lifecycle"};
  EXPECT_EQ(initialized, loader_.order_);

  refresh_log();
  EXPECT_EQ(0, count_in_log("lifecycle:all init():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:all init():EXIT"));
  EXPECT_EQ(0, count_in_log("lifecycle:instance1 start():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:instance1 start():EXIT"));
  EXPECT_EQ(0, count_in_log("lifecycle:instance1 stop():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:instance1 stop():EXIT"));
  EXPECT_EQ(0, count_in_log("lifecycle:all deinit():begin"));
  EXPECT_EQ(0, count_in_log("lifecycle:all deinit():EXIT"));

  EXPECT_EQ(loader_.deinit_all(), nullptr);

  refresh_log();
  EXPECT_EQ(1, count_in_log("lifecycle:all deinit():begin"));
  EXPECT_EQ(1, count_in_log("lifecycle:all deinit():EXIT"));
}

////////////////////////////////////////////////////////////////////////////////
//
// UNIT TESTS: COMPLEX
//
////////////////////////////////////////////////////////////////////////////////

TEST_F(LifecycleTest, ThreeInstances_NoError) {
  // In this testcase we do thorough checking, and provide elaborate comments.
  // We won't do it in other tests, so read this one for better understanding
  // of others.

  // init() and deinit() config is taken from first instance. This is because
  // init() and deinit() run only once per plugin, not per plugin instance.
  config_text_ << "init   = exit           \n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n"
               << "deinit = exit           \n"
               << "                        \n"
               << "[lifecycle:instance2]   \n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n"
               << "                        \n"
               << "[lifecycle:instance3]   \n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n";
  init_test(config_text_);

  // signal shutdown after 10ms, run() should block until then
  run_then_signal_shutdown([&]() { loader_.run(); });

  // all 3 plugins should have remained on the list of "to be deinitialized",
  // since they all should have initialized properly
  const std::list<std::string> initialized = {"magic", "lifecycle3",
                                              "lifecycle"};
  EXPECT_EQ(initialized, loader_.order_);

  refresh_log();

  // initialisation proceeds in defined order
  EXPECT_EQ(1, count_in_log("Initializing all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'magic' initializing"));
  EXPECT_EQ(1, count_in_log("  plugin 'magic' init exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3' initializing"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3' init exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle' initializing"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle' init exit ok"));

  // plugins may be started in arbitrary order (they run in separate threads)
  EXPECT_EQ(1, count_in_log("Starting all plugins."));
  EXPECT_EQ(1,
            count_in_log("  plugin 'lifecycle3:' doesn't implement start()"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance1' starting"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance2' starting"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance3' starting"));
  EXPECT_EQ(1, count_in_log("  plugin 'magic:' starting"));
  EXPECT_EQ(1, count_in_log("  plugin 'magic:' start exit ok"));

  // similarly, they may stop in arbitrary order
  EXPECT_EQ(1, count_in_log("Shutting down. Stopping all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance1' stopping"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance1' stop exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance2' stopping"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance2' stop exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance3' stopping"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance3' stop exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3:' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin 'magic:' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance2' start exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance3' start exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance1' start exit ok"));

  // deinitializasation proceeds in reverse order of initialisation
  EXPECT_EQ(1, count_in_log("Deinitializing all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle' deinitializing"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3' deinitializing"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'magic' doesn't implement deinit()"));

  // this is a sunny day scenario, nothing should fail
  EXPECT_EQ(0, count_in_log("failed"));

  // failure messages would look like this:
  // init()   -> "plugin 'lifecycle' init failed: <message>"
  // start()  -> "plugin 'lifecycle:instance1' start terminated with exception:
  // <message>" stop()   -> "plugin 'lifecycle:instance1' stop failed:
  // <message>" deinit() -> "plugin 'lifecycle' deinit failed: <message>"
}

TEST_F(LifecycleTest, BothLifecycles_NoError) {
  config_text_ << "init   = exit           \n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n"
               << "deinit = exit           \n"
               << "                        \n"
               << "[lifecycle2]            \n";
  init_test(config_text_);

  // signal shutdown after 10ms, run() should block until then
  run_then_signal_shutdown([&]() { loader_.run(); });

  const std::list<std::string> initialized = {"magic", "lifecycle3",
                                              "lifecycle", "lifecycle2"};
  EXPECT_EQ(initialized, loader_.order_);

  refresh_log();

  EXPECT_EQ(1, count_in_log("Initializing all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'magic' init exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3' init exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle' init exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle2' init exit ok"));

  EXPECT_EQ(1, count_in_log("Starting all plugins."));
  EXPECT_EQ(1,
            count_in_log("  plugin 'lifecycle3:' doesn't implement start()"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance1' starting"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle2:' start exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'magic:' start exit ok"));

  EXPECT_EQ(1, count_in_log("Shutting down. Stopping all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance1' stop exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle2:' stop exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3:' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin 'magic:' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance1' start exit ok"));

  EXPECT_EQ(1, count_in_log("Deinitializing all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle2' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'magic' doesn't implement deinit()"));

  EXPECT_EQ(0, count_in_log("failed"));
}

TEST_F(LifecycleTest, OneInstance_NothingPersists_NoError) {
  config_text_ << "init   = exit           \n"
               << "start  = exit           \n"
               << "stop   = exit           \n"
               << "deinit = exit           \n";
  init_test(config_text_);

  // Router should just shut down on it's own, since there's nothing to run
  // (all plugin start() functions just exit)
  loader_.run();

  refresh_log();

  EXPECT_EQ(1, count_in_log("Initializing all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'magic' init exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3' init exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle' init exit ok"));

  EXPECT_EQ(1, count_in_log("Starting all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'magic:' start exit ok"));
  EXPECT_EQ(1,
            count_in_log("  plugin 'lifecycle3:' doesn't implement start()"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance1' starting"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance1' start exit ok"));

  EXPECT_EQ(1, count_in_log("Shutting down. Stopping all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance1' stop exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3:' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin 'magic:' doesn't implement stop()"));

  EXPECT_EQ(1, count_in_log("Deinitializing all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'magic' doesn't implement deinit()"));

  EXPECT_EQ(0, count_in_log("failed"));
}

TEST_F(LifecycleTest, OneInstance_NothingPersists_StopFails) {
  config_text_ << "init   = exit           \n"
               << "start  = exit           \n"
               << "stop   = error          \n"
               << "deinit = exit           \n";
  init_test(config_text_);

  // Router should just shut down on it's own, since there's nothing to run
  // (all plugin start() functions just exit)
  try {
    std::exception_ptr e = loader_.run();
    if (e) std::rethrow_exception(e);
    FAIL() << "stop() should throw std::runtime_error";
  } catch (const std::runtime_error &e) {
    EXPECT_STREQ("lifecycle:instance1 stop(): I'm returning error!", e.what());
  } catch (...) {
    FAIL() << "stop() should throw std::runtime_error";
  }

  refresh_log();

  EXPECT_EQ(1, count_in_log("Initializing all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'magic' init exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3' init exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle' init exit ok"));

  EXPECT_EQ(1, count_in_log("Starting all plugins."));
  EXPECT_EQ(1,
            count_in_log("  plugin 'lifecycle3:' doesn't implement start()"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance1' starting"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance1' start exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'magic:' start exit ok"));

  EXPECT_EQ(1, count_in_log("Shutting down. Stopping all plugins."));
  EXPECT_EQ(1,
            count_in_log("  plugin 'lifecycle:instance1' stop failed: "
                         "lifecycle:instance1 stop(): I'm returning error!"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3:' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin 'magic:' doesn't implement stop()"));

  EXPECT_EQ(1, count_in_log("Deinitializing all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'magic' doesn't implement deinit()"));
}

TEST_F(LifecycleTest, ThreeInstances_InitFails) {
  config_text_ << "init   = error          \n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n"
               << "deinit = exit           \n"
               << "                        \n"
               << "[lifecycle:instance2]   \n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n"
               << "                        \n"
               << "[lifecycle:instance3]   \n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n";
  init_test(config_text_);

  try {
    std::exception_ptr e = loader_.run();
    if (e) std::rethrow_exception(e);
    FAIL() << "init() should throw std::runtime_error";
  } catch (const std::runtime_error &e) {
    EXPECT_STREQ("lifecycle:all init(): I'm returning error!", e.what());
  } catch (...) {
    FAIL() << "init() should throw std::runtime_error";
  }

  // lifecycle should not be on the list of to-be-deinitialized, since it
  // failed initialisation
  const std::list<std::string> initialized = {"magic", "lifecycle3"};
  EXPECT_EQ(initialized, loader_.order_);

  refresh_log();

  // lifecycle2 should not be initialized
  EXPECT_EQ(1, count_in_log("Initializing all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'magic' init exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3' init exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle' init failed: "
                            "lifecycle:all init(): I'm returning error!"));
  // start() and stop() shouldn't run
  EXPECT_EQ(0, count_in_log("Starting all plugins."));
  EXPECT_EQ(0, count_in_log("Shutting down. Stopping all plugins."));

  // lifecycle2 should not be deinintialized
  EXPECT_EQ(1, count_in_log("Deinitializing all plugins."));
  EXPECT_EQ(0, count_in_log("  plugin 'lifecycle' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'magic' doesn't implement deinit()"));
}

TEST_F(LifecycleTest, BothLifecycles_InitFails) {
  config_text_ << "init   = error          \n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n"
               << "deinit = exit           \n"
               << "                        \n"
               << "[lifecycle2]            \n";
  init_test(config_text_);

  try {
    std::exception_ptr e = loader_.run();
    if (e) std::rethrow_exception(e);
    FAIL() << "init() should throw std::runtime_error";
  } catch (const std::runtime_error &e) {
    EXPECT_STREQ("lifecycle:all init(): I'm returning error!", e.what());
  } catch (...) {
    FAIL() << "init() should throw std::runtime_error";
  }

  // lifecycle should not be on the list of to-be-deinitialized, since it
  // failed initialisation; neither should lifecycle2, because it never reached
  // initialisation phase
  const std::list<std::string> initialized = {"magic", "lifecycle3"};
  EXPECT_EQ(initialized, loader_.order_);

  refresh_log();

  // lifecycle2 should not be initialized
  EXPECT_EQ(1, count_in_log("Initializing all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'magic' init exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3' init exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle' initializing"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle' init failed: "
                            "lifecycle:all init(): I'm returning error!"));
  EXPECT_EQ(0, count_in_log("  plugin 'lifecycle2' initializing"));

  // start() and stop() shouldn't run
  EXPECT_EQ(0, count_in_log("Starting all plugins."));
  EXPECT_EQ(0, count_in_log("Shutting down. Stopping all plugins."));

  // lifecycle2 should not be deinintialized
  EXPECT_EQ(1, count_in_log("Deinitializing all plugins."));
  EXPECT_EQ(0, count_in_log("  plugin 'lifecycle2' deinitializing"));
  EXPECT_EQ(0, count_in_log("  plugin 'lifecycle' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'magic' doesn't implement deinit()"));
}

TEST_F(LifecycleTest, ThreeInstances_Start1Fails) {
  config_text_ << "init   = exit           \n"
               << "start  = error          \n"
               << "stop   = exit           \n"
               << "deinit = exit           \n"
               << "                        \n"
               << "[lifecycle:instance2]   \n"
               << "start  = exit           \n"
               << "stop   = exit           \n"
               << "                        \n"
               << "[lifecycle:instance3]   \n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n";
  init_test(config_text_);

  try {
    std::exception_ptr e = loader_.run();
    if (e) std::rethrow_exception(e);
    FAIL() << "start() should throw std::runtime_error";
  } catch (const std::runtime_error &e) {
    EXPECT_STREQ("lifecycle:instance1 start(): I'm returning error!", e.what());
  } catch (...) {
    FAIL() << "start() should throw std::runtime_error";
  }

  const std::list<std::string> initialized = {"magic", "lifecycle3",
                                              "lifecycle"};
  EXPECT_EQ(initialized, loader_.order_);

  refresh_log();

  EXPECT_EQ(1, count_in_log("Initializing all plugins."));

  EXPECT_EQ(1, count_in_log("Starting all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'magic:' start exit ok"));
  EXPECT_EQ(1,
            count_in_log("  plugin 'lifecycle3:' doesn't implement start()"));
  EXPECT_EQ(1,
            count_in_log("  plugin 'lifecycle:instance1' start failed: "
                         "lifecycle:instance1 start(): I'm returning error!"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance2' starting"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance3' starting"));

  EXPECT_EQ(1, count_in_log("Shutting down. Stopping all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'magic:' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3:' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance2' stop exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance3' stop exit ok"));

  EXPECT_EQ(1, count_in_log("Deinitializing all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'magic' doesn't implement deinit()"));
}

TEST_F(LifecycleTest, ThreeInstances_Start2Fails) {
  config_text_ << "init   = exit           \n"
               << "start  = exit           \n"
               << "stop   = exit           \n"
               << "deinit = exit           \n"
               << "                        \n"
               << "[lifecycle:instance2]   \n"
               << "start  = error          \n"
               << "stop   = exit           \n"
               << "                        \n"
               << "[lifecycle:instance3]   \n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n";
  init_test(config_text_);

  try {
    std::exception_ptr e = loader_.run();
    if (e) std::rethrow_exception(e);
    FAIL() << "start() should throw std::runtime_error";
  } catch (const std::runtime_error &e) {
    EXPECT_STREQ("lifecycle:instance2 start(): I'm returning error!", e.what());
  } catch (...) {
    FAIL() << "start() should throw std::runtime_error";
  }

  const std::list<std::string> initialized = {"magic", "lifecycle3",
                                              "lifecycle"};
  EXPECT_EQ(initialized, loader_.order_);

  refresh_log();

  EXPECT_EQ(1, count_in_log("Initializing all plugins."));

  EXPECT_EQ(1, count_in_log("Starting all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'magic:' start exit ok"));
  EXPECT_EQ(1,
            count_in_log("  plugin 'lifecycle3:' doesn't implement start()"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance1' starting"));
  EXPECT_EQ(1,
            count_in_log("  plugin 'lifecycle:instance2' start failed: "
                         "lifecycle:instance2 start(): I'm returning error!"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance3' starting"));

  EXPECT_EQ(1, count_in_log("Shutting down. Stopping all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'magic:' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3:' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance1' stop exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance3' stop exit ok"));

  EXPECT_EQ(1, count_in_log("Deinitializing all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'magic' doesn't implement deinit()"));
}

TEST_F(LifecycleTest, ThreeInstances_Start3Fails) {
  config_text_ << "init   = exit           \n"
               << "start  = exit           \n"
               << "stop   = exit           \n"
               << "deinit = exit           \n"
               << "                        \n"
               << "[lifecycle:instance2]   \n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n"
               << "                        \n"
               << "[lifecycle:instance3]   \n"
               << "start  = error          \n"
               << "stop   = exit           \n";
  init_test(config_text_);

  try {
    std::exception_ptr e = loader_.run();
    if (e) std::rethrow_exception(e);
    FAIL() << "start() should throw std::runtime_error";
  } catch (const std::runtime_error &e) {
    EXPECT_STREQ("lifecycle:instance3 start(): I'm returning error!", e.what());
  } catch (...) {
    FAIL() << "start() should throw std::runtime_error";
  }

  const std::list<std::string> initialized = {"magic", "lifecycle3",
                                              "lifecycle"};
  EXPECT_EQ(initialized, loader_.order_);

  refresh_log();

  EXPECT_EQ(1, count_in_log("Initializing all plugins."));

  EXPECT_EQ(1, count_in_log("Starting all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'magic:' start exit ok"));
  EXPECT_EQ(1,
            count_in_log("  plugin 'lifecycle3:' doesn't implement start()"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance1' starting"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance2' starting"));
  EXPECT_EQ(1,
            count_in_log("  plugin 'lifecycle:instance3' start failed: "
                         "lifecycle:instance3 start(): I'm returning error!"));

  EXPECT_EQ(1, count_in_log("Shutting down. Stopping all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'magic:' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3:' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance1' stop exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance2' stop exit ok"));

  EXPECT_EQ(1, count_in_log("Deinitializing all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'magic' doesn't implement deinit()"));
}

TEST_F(LifecycleTest, ThreeInstances_2StartsFail) {
  config_text_ << "init   = exit           \n"
               << "start  = error          \n"
               << "stop   = exit           \n"
               << "deinit = exit           \n"
               << "                        \n"
               << "[lifecycle:instance2]   \n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n"
               << "                        \n"
               << "[lifecycle:instance3]   \n"
               << "start  = error          \n"
               << "stop   = exit           \n";
  init_test(config_text_);

  try {
    std::exception_ptr e = loader_.run();
    if (e) std::rethrow_exception(e);
    FAIL() << "start() should throw std::runtime_error";
  } catch (const std::runtime_error &e) {
    // instance1 or instance3, undeterministic
    EXPECT_TRUE(strstr(e.what(), "start(): I'm returning error!"));
  } catch (...) {
    FAIL() << "start() should throw std::runtime_error";
  }

  const std::list<std::string> initialized = {"magic", "lifecycle3",
                                              "lifecycle"};
  EXPECT_EQ(initialized, loader_.order_);

  refresh_log();

  EXPECT_EQ(1, count_in_log("Initializing all plugins."));

  EXPECT_EQ(1, count_in_log("Starting all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'magic:' start exit ok"));
  EXPECT_EQ(1,
            count_in_log("  plugin 'lifecycle3:' doesn't implement start()"));
  EXPECT_EQ(1,
            count_in_log("  plugin 'lifecycle:instance1' start failed: "
                         "lifecycle:instance1 start(): I'm returning error!"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance2' starting"));
  EXPECT_EQ(1,
            count_in_log("  plugin 'lifecycle:instance3' start failed: "
                         "lifecycle:instance3 start(): I'm returning error!"));

  EXPECT_EQ(1, count_in_log("Shutting down. Stopping all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'magic:' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3:' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance2' stop exit ok"));

  EXPECT_EQ(1, count_in_log("Deinitializing all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'magic' doesn't implement deinit()"));
}

TEST_F(LifecycleTest, ThreeInstances_StopFails) {
  config_text_ << "init   = exit           \n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n"
               << "deinit = exit           \n"
               << "                        \n"
               << "[lifecycle:instance2]   \n"
               << "start  = exitonstop     \n"
               << "stop   = error          \n"
               << "                        \n"
               << "[lifecycle:instance3]   \n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n";
  init_test(config_text_);

  // signal shutdown after 10ms, run() should block until then
  run_then_signal_shutdown([&]() {
    try {
      std::exception_ptr e = loader_.run();
      if (e) std::rethrow_exception(e);
      FAIL() << "stop() should throw std::runtime_error";
    } catch (const std::runtime_error &e) {
      EXPECT_STREQ("lifecycle:instance2 stop(): I'm returning error!",
                   e.what());
    } catch (...) {
      FAIL() << "stop() should throw std::runtime_error";
    }
  });

  refresh_log();

  EXPECT_EQ(1, count_in_log("Initializing all plugins."));

  EXPECT_EQ(1, count_in_log("Starting all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'magic:' starting"));
  EXPECT_EQ(1, count_in_log("  plugin 'magic:' start exit ok"));
  EXPECT_EQ(1,
            count_in_log("  plugin 'lifecycle3:' doesn't implement start()"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance1' starting"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance2' starting"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance3' starting"));

  EXPECT_EQ(1, count_in_log("Shutting down. Stopping all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'magic:' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3:' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance1' stop exit ok"));
  EXPECT_EQ(1,
            count_in_log("  plugin 'lifecycle:instance2' stop failed: "
                         "lifecycle:instance2 stop(): I'm returning error!"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance3' stop exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance1' start exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance2' start exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance3' start exit ok"));

  EXPECT_EQ(1, count_in_log("Deinitializing all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'magic' doesn't implement deinit()"));
}

TEST_F(LifecycleTest, ThreeInstances_DeinintFails) {
  config_text_ << "init   = exit           \n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n"
               << "deinit = error          \n"
               << "                        \n"
               << "[lifecycle:instance2]   \n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n"
               << "                        \n"
               << "[lifecycle:instance3]   \n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n";
  init_test(config_text_);

  // signal shutdown after 10ms, run() should block until then
  run_then_signal_shutdown([&]() {
    try {
      std::exception_ptr e = loader_.run();
      if (e) std::rethrow_exception(e);
      FAIL() << "deinit() should throw std::runtime_error";
    } catch (const std::runtime_error &e) {
      EXPECT_STREQ("lifecycle:all deinit(): I'm returning error!", e.what());
    } catch (...) {
      FAIL() << "deinit() should throw std::runtime_error";
    }
  });

  refresh_log();

  EXPECT_EQ(1, count_in_log("Initializing all plugins."));

  EXPECT_EQ(1, count_in_log("Starting all plugins."));

  EXPECT_EQ(1, count_in_log("Shutting down. Stopping all plugins."));

  EXPECT_EQ(1, count_in_log("Deinitializing all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle' deinit failed: "
                            "lifecycle:all deinit(): I'm returning error!"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'magic' doesn't implement deinit()"));
}

TEST_F(LifecycleTest, ThreeInstances_StartStopDeinitFail) {
  config_text_ << "init   = exit           \n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n"
               << "deinit = error          \n"
               << "                        \n"
               << "[lifecycle:instance2]   \n"
               << "start  = error          \n"
               << "stop   = exit           \n"
               << "                        \n"
               << "[lifecycle:instance3]   \n"
               << "start  = exitonstop     \n"
               << "stop   = error          \n";
  init_test(config_text_);

  // exception from start() should get propagated
  try {
    std::exception_ptr e = loader_.run();
    if (e) std::rethrow_exception(e);
    FAIL() << "start() should throw std::runtime_error";
  } catch (const std::runtime_error &e) {
    EXPECT_STREQ("lifecycle:instance2 start(): I'm returning error!", e.what());
  } catch (...) {
    FAIL() << "start() should throw std::runtime_error";
  }

  const std::list<std::string> initialized = {"magic", "lifecycle3",
                                              "lifecycle"};
  EXPECT_EQ(initialized, loader_.order_);

  refresh_log();

  EXPECT_EQ(1, count_in_log("Initializing all plugins."));

  EXPECT_EQ(1, count_in_log("Starting all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'magic:' start exit ok"));
  EXPECT_EQ(1,
            count_in_log("  plugin 'lifecycle3:' doesn't implement start()"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance1' starting"));
  EXPECT_EQ(1,
            count_in_log("  plugin 'lifecycle:instance2' start failed: "
                         "lifecycle:instance2 start(): I'm returning error!"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance3' starting"));

  EXPECT_EQ(1, count_in_log("Shutting down. Stopping all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'magic:' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3:' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle:instance1' stop exit ok"));
  EXPECT_EQ(1,
            count_in_log("  plugin 'lifecycle:instance3' stop failed: "
                         "lifecycle:instance3 stop(): I'm returning error!"));

  EXPECT_EQ(1, count_in_log("Deinitializing all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle' deinit failed: "
                            "lifecycle:all deinit(): I'm returning error!"));
  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle3' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin 'magic' doesn't implement deinit()"));
}

TEST_F(LifecycleTest, NoInstances) {
  const std::string plugin_dir = mysql_harness::get_plugin_dir(g_here.str());
  config_text_.str(
      "[DEFAULT]                                      \n"
      "logging_folder =                               \n"
      "plugin_folder  = " +
      plugin_dir +
      "\n"
      "runtime_folder = {prefix}                      \n"
      "config_folder  = {prefix}                      \n"
      // TODO: restore this after after [logger] hack is reverted (grep for
      // g_HACK_default_log_level)
      //    "                                               \n"
      //    "[logger]                                       \n"
      //    "level = DEBUG                                  \n"
      "                                               \n");
  init_test_without_lifecycle_plugin(config_text_);

  loader_.run();

  refresh_log();

  EXPECT_EQ(1, count_in_log("Initializing all plugins."));
  EXPECT_EQ(1, count_in_log("Starting all plugins."));
  EXPECT_EQ(1, count_in_log("Shutting down. Stopping all plugins."));
  EXPECT_EQ(1, count_in_log("Deinitializing all plugins."));
  EXPECT_EQ(0, count_in_log("failed"));
}

// note: we don't test an equivalent scenario when the plugin throws (an empty
//       "what" field), because to accomplish this, plugin would have to throw
//       something like:
//
//           throw std::runtime_error();
//
//       which (on GCC 4.8.4 anyway) emits std::logic_error with a message
//       complaining about a null std::string. In other words, what() returning
//       a null string in harness' catch block is not likely.
TEST_F(LifecycleTest, EmptyErrorMessage) {
  // this test tests PluginFuncEnv::set_error() function, when passed a null
  // string.

  config_text_ << "init   = error_empty    \n"
               << "start  = exit           \n"
               << "stop   = exit           \n"
               << "deinit = exit           \n";
  init_test(config_text_, {true, false, false, false});

  // null string should be replaced with '<empty message>'
  try {
    std::exception_ptr e = loader_.run();
    if (e) std::rethrow_exception(e);
    FAIL() << "init() should throw std::runtime_error";
  } catch (const std::runtime_error &e) {
    EXPECT_STREQ("<empty message>", e.what());
  } catch (...) {
    FAIL() << "init() should throw std::runtime_error";
  }

  // null string should be replaced with '<empty message>'
  refresh_log();
  EXPECT_EQ(1,
            count_in_log("  plugin 'lifecycle' init failed: <empty message>"));
}

// maybe these should be moved to test_loader.cc (or wherever PluginFuncEnv
// class ends up) these tests probably obsolete the EmptyErrorMessage test above
TEST_F(LifecycleTest, set_error_message) {
  std::string emsg;
  mysql_harness::PluginFuncEnv ctx(nullptr, nullptr);

  // simple
  mysql_harness::set_error(&ctx, mysql_harness::kRuntimeError, "foo");
  std::tie(emsg, std::ignore) = ctx.pop_error();
  EXPECT_STREQ("foo", emsg.c_str());

  // complex
  mysql_harness::set_error(&ctx, mysql_harness::kRuntimeError, "[%s:%s] %d",
                           "foo", "bar", 42);
  std::tie(emsg, std::ignore) = ctx.pop_error();
  EXPECT_STREQ("[foo:bar] 42", emsg.c_str());

  // cornercase: empty
#ifndef __GNUC__
  // gcc/clang catch it at compile time: error: zero-length gnu_printf format
  // string [-Werror=format-zero-length]
  mysql_harness::set_error(&ctx, mysql_harness::kRuntimeError, "");
  std::tie(emsg, std::ignore) = ctx.pop_error();
  EXPECT_STREQ("", emsg.c_str());
#endif

  // cornercase: NULL
  mysql_harness::set_error(&ctx, mysql_harness::kRuntimeError, nullptr);
  std::tie(emsg, std::ignore) = ctx.pop_error();
  EXPECT_STREQ("<empty message>", emsg.c_str());

#ifndef __GNUC__
  // gcc/clang catch it at compile time: error: too many arguments for format
  // [-Werror=format-extra-args] cornercase: NULL + arg
  mysql_harness::set_error(&ctx, mysql_harness::kRuntimeError, nullptr, "foo");
  std::tie(emsg, std::ignore) = ctx.pop_error();
  EXPECT_STREQ("<empty message>", emsg.c_str());
#endif

#ifndef __GNUC__
  // gcc/clang catch it at compile time

  // cornercase: extra arg
  mysql_harness::set_error(&ctx, mysql_harness::kRuntimeError, "foo", "bar");
  std::tie(emsg, std::ignore) = ctx.pop_error();
  EXPECT_STREQ("foo", emsg.c_str());
#endif
}

TEST_F(LifecycleTest, set_error_exception) {
  std::exception_ptr eptr;
  mysql_harness::PluginFuncEnv ctx(nullptr, nullptr);

  // test all supported exception types

  mysql_harness::set_error(&ctx, mysql_harness::kRuntimeError, nullptr);
  std::tie(std::ignore, eptr) = ctx.pop_error();
  EXPECT_THROW({ std::rethrow_exception(eptr); }, std::runtime_error);

  mysql_harness::set_error(&ctx, mysql_harness::kConfigInvalidArgument,
                           nullptr);
  std::tie(std::ignore, eptr) = ctx.pop_error();
  EXPECT_THROW({ std::rethrow_exception(eptr); }, std::invalid_argument);

  mysql_harness::set_error(&ctx, mysql_harness::kConfigSyntaxError, nullptr);
  std::tie(std::ignore, eptr) = ctx.pop_error();
  EXPECT_THROW({ std::rethrow_exception(eptr); }, mysql_harness::syntax_error);

  mysql_harness::set_error(&ctx, mysql_harness::kUndefinedError, nullptr);
  std::tie(std::ignore, eptr) = ctx.pop_error();
  EXPECT_THROW({ std::rethrow_exception(eptr); }, std::runtime_error);
}

#ifdef USE_POSIX_SIGNALS  // these don't make sense on Windows
TEST_F(LifecycleTest, send_signals) {
  // this test verifies that:
  // - sending SIGINT or SIGTERM will trigger shutdown
  //   (we only test SIGINT here, and SIGTERM in the next test)
  // - sending any other signal will do nothing

  config_text_ << "init   = exit           \n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n"
               << "deinit = exit           \n";
  init_test(config_text_, {true, true, true, true});
  LifecyclePluginSyncBus &bus = msg_bus("instance1");

  EXPECT_EQ(loader_.init_all(), nullptr);
  freeze_bus(bus);
  loader_.start_all();
  unfreeze_and_wait_for_msg(
      bus, "lifecycle:instance1 start():EXIT_ON_STOP:sleeping");

  // nothing should happen - all signals but the ones we care about should be
  // ignored (here we only test a few, the rest is assumed to behave the same)
  kill(getpid(), SIGUSR1);
  kill(getpid(), SIGALRM);

  // signal shutdown after 10ms, main_loop() should block until then
  auto call_SIGINT = []() {
    std::this_thread::sleep_for(ch::milliseconds(kSleepShutdown));
    kill(getpid(), SIGINT);
  };
  std::thread(call_SIGINT).detach();
  EXPECT_EQ(loader_.main_loop(), nullptr);

  refresh_log();
  EXPECT_EQ(1, count_in_log("Shutting down. Stopping all plugins."));
}

TEST_F(LifecycleTest, send_signals2) {
  // continuation of the previous test (test SIGTERM this time)

  config_text_ << "init   = exit           \n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n"
               << "deinit = exit           \n";
  init_test(config_text_, {true, true, true, true});
  LifecyclePluginSyncBus &bus = msg_bus("instance1");

  EXPECT_EQ(loader_.init_all(), nullptr);
  freeze_bus(bus);
  loader_.start_all();
  unfreeze_and_wait_for_msg(
      bus, "lifecycle:instance1 start():EXIT_ON_STOP:sleeping");

  // signal shutdown after 10ms, main_loop() should block until then
  auto call_SIGTERM = []() {
    std::this_thread::sleep_for(ch::milliseconds(kSleepShutdown));
    kill(getpid(), SIGTERM);
  };
  std::thread(call_SIGTERM).detach();
  EXPECT_EQ(loader_.main_loop(), nullptr);

  refresh_log();
  EXPECT_EQ(1, count_in_log("Shutting down. Stopping all plugins."));
}
#endif

TEST_F(LifecycleTest, wait_for_stop) {
  // This test is really about testing Harness API function wait_for_stop(),
  // when passed a timeout value. It is used when start/stop = exit_slow, and
  // here we verify its behaviour.
  //
  // SCENARIO:
  // while start() is running, "Router is running", thus start() should block,
  // waiting for Harness to progress to "stopping" state. That will not occur
  // until all plugins have exited, in this case meaning, the start() exits
  // (until its wait_for_stop() returns, thus the plugin is really waiting for
  // itself :)).
  // Once that all plugins have exited, Harness will be in the "stopping" state,
  // thus plugin's stop() function will be called.  It also calls
  // wait_for_stop(), but this time it should exit immediately, since the
  // Harness is shutting down.
  //
  // EXPECTATIONS:
  // wait_for_stop() inside start() should block for 100ms, return false
  // wait_for_stop() inside stop() should return immediately, return true

  config_text_ << "start = exit_slow\n";  // \_ they run
  config_text_ << "stop  = exit_slow\n";  // /  wait_for_stop() inside
  init_test(config_text_, {false, true, true, false});
  LifecyclePluginSyncBus &bus = msg_bus("instance1");

  EXPECT_EQ(loader_.init_all(), nullptr);
  freeze_bus(bus);

  // mark time start
  ch::time_point<ch::steady_clock> t0 = ch::steady_clock::now();

  loader_.start_all();
  unfreeze_and_wait_for_msg(bus,
                            "lifecycle:instance1 start():EXIT_SLOW:sleeping");

  // wait_for_stop() in start() should be sleeping right now, blocking progress
  refresh_log();
  EXPECT_EQ(1, count_in_log("lifecycle:instance1 start():begin"));
  EXPECT_EQ(1, count_in_log("lifecycle:instance1 start():EXIT_SLOW:sleeping"));
  EXPECT_EQ(
      0,
      count_in_log(
          "lifecycle:instance1 start():EXIT_SLOW:done, stop request received"));
  EXPECT_EQ(
      0, count_in_log("lifecycle:instance1 start():EXIT_SLOW:done, timed out"));
  EXPECT_EQ(0, count_in_log("lifecycle:instance1 stop():begin"));
  EXPECT_EQ(
      0,
      count_in_log(
          "lifecycle:instance1 stop():EXIT_SLOW:done, stop request received"));
  EXPECT_EQ(
      0, count_in_log("lifecycle:instance1 stop():EXIT_SLOW:done, timed out"));

  // wait for it to exit (it will unblock main_loop())
  EXPECT_EQ(loader_.main_loop(), nullptr);

  // verify that:
  //   - wait_for_stop() in start() blocked
  //   - wait_for_stop() in stop() did not block
  ch::time_point<ch::steady_clock> t1 = ch::steady_clock::now();
  EXPECT_LE(100, time_diff(t0, t1));  // 100 = kPersistDuration in lifecycle.cc
  EXPECT_GT(
      200, time_diff(
               t0, t1));  // 200 would mean that both start() and stop() blocked

  // verify what both wait_for_stop()'s returned
  refresh_log();
  EXPECT_EQ(
      0,
      count_in_log(
          "lifecycle:instance1 start():EXIT_SLOW:done, stop request received"));
  EXPECT_EQ(
      1, count_in_log("lifecycle:instance1 start():EXIT_SLOW:done, timed out"));
  EXPECT_EQ(1, count_in_log("lifecycle:instance1 stop():begin"));
  EXPECT_EQ(
      1,
      count_in_log(
          "lifecycle:instance1 stop():EXIT_SLOW:done, stop request received"));
  EXPECT_EQ(
      0, count_in_log("lifecycle:instance1 stop():EXIT_SLOW:done, timed out"));
}

// Next 4 tests should only run in release builds. Code in debug builds throws
// assertion to warn the plugin developers that their plugins throw. But we
// wouldn't want to do this on production systems, so instead, we handle this
// error gracefully. Note that officially this behaviour is undefined, thus
// we are free to change this behaviour as we see fit.
#ifdef NDEBUG  // cmake's -DCMAKE_BUILD_TYPE=Release or RelWithDebInfo (not
               // case sensitive) will define it
TEST_F(LifecycleTest, InitThrows) {
  config_text_ << "init = throw\n";
  init_test(config_text_, {true, false, false, false});

  try {
    std::exception_ptr e = loader_.run();
    if (e) std::rethrow_exception(e);
    FAIL() << "init() should throw std::runtime_error";
  } catch (const std::runtime_error &e) {
    EXPECT_STREQ("lifecycle:all init(): I'm throwing!", e.what());
  } catch (...) {
    FAIL() << "init() should throw std::runtime_error";
  }

  refresh_log();

  EXPECT_EQ(
      1,
      count_in_log(
          "  plugin 'lifecycle' init threw unexpected "
          "exception - please contact plugin developers for more information: "
          "lifecycle:all init(): I'm throwing!"));
}

TEST_F(LifecycleTest, StartThrows) {
  config_text_ << "start = throw\n";
  init_test(config_text_, {false, true, false, false});

  try {
    std::exception_ptr e = loader_.run();
    if (e) std::rethrow_exception(e);
    FAIL() << "start() should throw std::runtime_error";
  } catch (const std::runtime_error &e) {
    EXPECT_STREQ("lifecycle:instance1 start(): I'm throwing!", e.what());
  } catch (...) {
    FAIL() << "start() should throw std::runtime_error";
  }

  refresh_log();

  EXPECT_EQ(
      1, count_in_log(
             "  plugin 'lifecycle:instance1' start threw "
             "unexpected exception - please contact plugin developers for more "
             "information: lifecycle:instance1 start(): I'm throwing!"));
}

TEST_F(LifecycleTest, StopThrows) {
  config_text_ << "stop = throw\n";
  init_test(config_text_, {false, false, true, false});

  try {
    std::exception_ptr e = loader_.run();
    if (e) std::rethrow_exception(e);
    FAIL() << "stop() should throw std::runtime_error";
  } catch (const std::runtime_error &e) {
    EXPECT_STREQ("lifecycle:instance1 stop(): I'm throwing!", e.what());
  } catch (...) {
    FAIL() << "stop() should throw std::runtime_error";
  }

  refresh_log();

  EXPECT_EQ(
      1, count_in_log(
             "  plugin 'lifecycle:instance1' stop threw "
             "unexpected exception - please contact plugin developers for more "
             "information: lifecycle:instance1 stop(): I'm throwing!"));
}

TEST_F(LifecycleTest, DeinitThrows) {
  config_text_ << "deinit = throw\n";
  init_test(config_text_, {false, false, false, true});

  try {
    std::exception_ptr e = loader_.run();
    if (e) std::rethrow_exception(e);
    FAIL() << "deinit() should throw std::runtime_error";
  } catch (const std::runtime_error &e) {
    EXPECT_STREQ("lifecycle:all deinit(): I'm throwing!", e.what());
  } catch (...) {
    FAIL() << "deinit() should throw std::runtime_error";
  }

  refresh_log();

  EXPECT_EQ(
      1,
      count_in_log(
          "  plugin 'lifecycle' deinit threw unexpected "
          "exception - please contact plugin developers for more information: "
          "lifecycle:all deinit(): I'm throwing!"));
}

// The following 4 are the same as above 4, but this time we throw unusual
// exceptions (not derived from std::exception), to test catch(...) logic
// in Loader's code
TEST_F(LifecycleTest, InitThrowsWeird) {
  config_text_ << "init = throw_weird\n";
  init_test(config_text_, {true, false, false, false});

  try {
    std::exception_ptr e = loader_.run();
    if (e) std::rethrow_exception(e);
    FAIL() << "init() should throw non-standard exception object";
  } catch (const std::runtime_error &e) {
    FAIL() << "init() should throw non-standard exception object";
  } catch (...) {
  }

  refresh_log();

  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle' init threw unexpected "
                            "exception - please contact plugin developers for "
                            "more information."));
}

TEST_F(LifecycleTest, StartThrowsWeird) {
  config_text_ << "start = throw_weird\n";
  init_test(config_text_, {false, true, false, false});

  try {
    std::exception_ptr e = loader_.run();
    if (e) std::rethrow_exception(e);
    FAIL() << "start() should throw non-standard exception object";
  } catch (const std::runtime_error &e) {
    FAIL() << "start() should throw non-standard exception object";
  } catch (...) {
  }

  refresh_log();

  EXPECT_EQ(
      1, count_in_log("  plugin 'lifecycle:instance1' start threw unexpected "
                      "exception - please contact plugin developers for more "
                      "information."));
}

TEST_F(LifecycleTest, StopThrowsWeird) {
  config_text_ << "stop = throw_weird\n";
  init_test(config_text_, {false, false, true, false});

  try {
    std::exception_ptr e = loader_.run();
    if (e) std::rethrow_exception(e);
    FAIL() << "stop() should throw non-standard exception object";
  } catch (const std::runtime_error &e) {
    FAIL() << "stop() should throw non-standard exception object";
  } catch (...) {
  }

  refresh_log();

  EXPECT_EQ(1,
            count_in_log("  plugin 'lifecycle:instance1' stop threw unexpected "
                         "exception - please contact plugin developers for "
                         "more information."));
}

TEST_F(LifecycleTest, DeinitThrowsWeird) {
  config_text_ << "deinit = throw_weird\n";
  init_test(config_text_, {false, false, false, true});

  try {
    std::exception_ptr e = loader_.run();
    if (e) std::rethrow_exception(e);
    FAIL() << "deinit() should throw non-standard exception object";
  } catch (const std::runtime_error &e) {
    FAIL() << "deinit() should throw non-standard exception object";
  } catch (...) {
  }

  refresh_log();

  EXPECT_EQ(1, count_in_log("  plugin 'lifecycle' deinit threw unexpected "
                            "exception - please contact plugin developers for "
                            "more information."));
}

#endif  // #ifdef NDEBUG

TEST_F(LifecycleTest, LoadingNonExistentPlugin) {
  clear_log();

  config_text_
      << "[nonexistent_plugin]\n";  // should cause Loader::load_all() to throw
  config_text_
      << "[nonexistent_plugin_2]\n";  // no attempt to load this should be made
  loader_.read(config_text_);

  try {
    loader_.start();
    FAIL() << "Loader::start() should throw bad_plugin";
  } catch (const bad_plugin &e) {
    EXPECT_THAT(e.what(), HasSubstr("nonexistent_plugin"));
  } catch (const std::exception &e) {
    FAIL() << "Loader::start() should throw bad_plugin, but got: " << e.what();
  }

  // Expect something like so:
  // "2017-07-13 14:38:57 main ERROR [7ffff7fd5780]   plugin
  // 'nonexistent_plugin' failed to load: <OS-specific error text>" "2017-07-13
  // 14:38:57 main INFO [7ffff7fd5780] Unloading all plugins."
  refresh_log();
  EXPECT_EQ(1,
            count_in_log("]   plugin 'nonexistent_plugin' failed to load: "));
  EXPECT_EQ(1, count_in_log("] Unloading all plugins."));

  // Loader::load_all() should have stopped loading as soon as it encountered
  // 'nonexistent_plugin'. Therefore, it should not attempt to load the next
  // plugin, 'nonexistent_plugin_2', thus we should find no trace of such string
  // in the log.
  EXPECT_EQ(0, count_in_log("nonexistent_plugin_2"));
}

int main(int argc, char *argv[]) {
  g_here = Path(argv[0]).dirname();
  init_test_logger();

  ::testing::InitGoogleTest(&argc, argv);
  int res = RUN_ALL_TESTS();

  return res;
}
