/*
  Copyright (c) 2016, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include "my_config.h"

////////////////////////////////////////
// Harness include files
#include "exception.h"
#include "lifecycle.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/loader.h"
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
#include <stdexcept>
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

// dlopen/dlclose work differently on Alpine
#if defined(LINUX_ALPINE)
#undef USE_DLCLOSE
#define USE_DLCLOSE 0
#endif

static const std::string kPluginNameLifecycle("routertestplugin_lifecycle");
static const std::string kPluginNameLifecycle2("routertestplugin_lifecycle2");
static const std::string kPluginNameLifecycle3("routertestplugin_lifecycle3");
static const std::string kPluginNameMagic("routertestplugin_magic");

using mysql_harness::Loader;
using mysql_harness::Path;
using mysql_harness::Plugin;
using mysql_harness::test::LifecyclePluginSyncBus;
using ::testing::HasSubstr;
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
    Plugin *plugin = plugins_.at(kPluginNameLifecycle).plugin;

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
                        "                                               \n"
                        "[logger]                                       \n"
                        "level = DEBUG                                  \n"
                        "                                               \n"
                        "[" +
                        kPluginNameLifecycle3 +
                        "]                  \n"
                        "                                               \n"
                        "[" +
                        kPluginNameMagic +
                        "]                       \n"
                        "suki = magic                                   \n"
                        "                                               \n"
                        "[" +
                        kPluginNameLifecycle + ":instance1]         \n";
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
    loader_.Loader::load_all();
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

  long count_in_log(const std::string &needle) {
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

  const std::list<std::string> initialized = {
      "logger", kPluginNameMagic, kPluginNameLifecycle3, kPluginNameLifecycle};
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

  const std::list<std::string> initialized = {
      "logger", kPluginNameMagic, kPluginNameLifecycle3, kPluginNameLifecycle};
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

  const std::list<std::string> initialized = {
      "logger", kPluginNameMagic, kPluginNameLifecycle3, kPluginNameLifecycle};
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

  const std::list<std::string> initialized = {
      "logger", kPluginNameMagic, kPluginNameLifecycle3, kPluginNameLifecycle};
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

  const std::list<std::string> initialized = {
      "logger", kPluginNameMagic, kPluginNameLifecycle3, kPluginNameLifecycle};
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

  const std::list<std::string> initialized = {
      "logger", kPluginNameMagic, kPluginNameLifecycle3, kPluginNameLifecycle};
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

  const std::list<std::string> initialized = {
      "logger", kPluginNameMagic, kPluginNameLifecycle3, kPluginNameLifecycle};
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

  const std::list<std::string> initialized = {
      "logger", kPluginNameMagic, kPluginNameLifecycle3, kPluginNameLifecycle};
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
               << "[" + kPluginNameLifecycle + ":instance2]\n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n"
               << "                        \n"
               << "[" + kPluginNameLifecycle + ":instance3]\n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n";
  init_test(config_text_);

  // signal shutdown after 10ms, run() should block until then
  run_then_signal_shutdown([&]() { loader_.run(); });

  // all 3 plugins should have remained on the list of "to be deinitialized",
  // since they all should have initialized properly
  const std::list<std::string> initialized = {
      "logger", kPluginNameMagic, kPluginNameLifecycle3, kPluginNameLifecycle};
  EXPECT_EQ(initialized, loader_.order_);

  refresh_log();

  // initialisation proceeds in defined order
  EXPECT_EQ(1, count_in_log("Initializing all plugins."));
  EXPECT_EQ(1,
            count_in_log("  plugin '" + kPluginNameMagic + "' initializing"));
  EXPECT_EQ(1,
            count_in_log("  plugin '" + kPluginNameMagic + "' init exit ok"));
  EXPECT_EQ(
      1, count_in_log("  plugin '" + kPluginNameLifecycle3 + "' initializing"));
  EXPECT_EQ(
      1, count_in_log("  plugin '" + kPluginNameLifecycle3 + "' init exit ok"));
  EXPECT_EQ(
      1, count_in_log("  plugin '" + kPluginNameLifecycle + "' initializing"));
  EXPECT_EQ(
      1, count_in_log("  plugin '" + kPluginNameLifecycle + "' init exit ok"));

  // plugins may be started in arbitrary order (they run in separate threads)
  EXPECT_EQ(1, count_in_log("Starting all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            ":' doesn't implement start()"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance1' starting"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance2' starting"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance3' starting"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameMagic + ":' starting"));
  EXPECT_EQ(1,
            count_in_log("  plugin '" + kPluginNameMagic + ":' start exit ok"));

  // similarly, they may stop in arbitrary order
  EXPECT_EQ(1, count_in_log("Shutting down. Stopping all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance1' stopping"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance1' stop exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance2' stopping"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance2' stop exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance3' stopping"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance3' stop exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            ":' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameMagic +
                            ":' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance2' start exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance3' start exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance1' start exit ok"));

  // deinitializasation proceeds in reverse order of initialisation
  EXPECT_EQ(1, count_in_log("Deinitializing all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            "' deinitializing"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            "' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            "' deinitializing"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            "' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameMagic +
                            "' doesn't implement deinit()"));

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
               << "[" + kPluginNameLifecycle2 + "]\n";
  init_test(config_text_);

  // signal shutdown after 10ms, run() should block until then
  run_then_signal_shutdown([&]() { loader_.run(); });

  const std::list<std::string> initialized = {
      "logger", kPluginNameMagic, kPluginNameLifecycle3, kPluginNameLifecycle,
      kPluginNameLifecycle2};
  EXPECT_EQ(initialized, loader_.order_);

  refresh_log();

  EXPECT_EQ(1, count_in_log("Initializing all plugins."));
  EXPECT_EQ(1,
            count_in_log("  plugin '" + kPluginNameMagic + "' init exit ok"));
  EXPECT_EQ(
      1, count_in_log("  plugin '" + kPluginNameLifecycle3 + "' init exit ok"));
  EXPECT_EQ(
      1, count_in_log("  plugin '" + kPluginNameLifecycle + "' init exit ok"));
  EXPECT_EQ(
      1, count_in_log("  plugin '" + kPluginNameLifecycle2 + "' init exit ok"));

  EXPECT_EQ(1, count_in_log("Starting all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            ":' doesn't implement start()"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance1' starting"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle2 +
                            ":' start exit ok"));
  EXPECT_EQ(1,
            count_in_log("  plugin '" + kPluginNameMagic + ":' start exit ok"));

  EXPECT_EQ(1, count_in_log("Shutting down. Stopping all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance1' stop exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle2 +
                            ":' stop exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            ":' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameMagic +
                            ":' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance1' start exit ok"));

  EXPECT_EQ(1, count_in_log("Deinitializing all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle2 +
                            "' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            "' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            "' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameMagic +
                            "' doesn't implement deinit()"));

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
  EXPECT_EQ(1,
            count_in_log("  plugin '" + kPluginNameMagic + "' init exit ok"));
  EXPECT_EQ(
      1, count_in_log("  plugin '" + kPluginNameLifecycle3 + "' init exit ok"));
  EXPECT_EQ(
      1, count_in_log("  plugin '" + kPluginNameLifecycle + "' init exit ok"));

  EXPECT_EQ(1, count_in_log("Starting all plugins."));
  EXPECT_EQ(1,
            count_in_log("  plugin '" + kPluginNameMagic + ":' start exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            ":' doesn't implement start()"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance1' starting"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance1' start exit ok"));

  EXPECT_EQ(1, count_in_log("Shutting down. Stopping all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance1' stop exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            ":' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameMagic +
                            ":' doesn't implement stop()"));

  EXPECT_EQ(1, count_in_log("Deinitializing all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            "' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            "' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameMagic +
                            "' doesn't implement deinit()"));

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
  EXPECT_EQ(1,
            count_in_log("  plugin '" + kPluginNameMagic + "' init exit ok"));
  EXPECT_EQ(
      1, count_in_log("  plugin '" + kPluginNameLifecycle3 + "' init exit ok"));
  EXPECT_EQ(
      1, count_in_log("  plugin '" + kPluginNameLifecycle + "' init exit ok"));

  EXPECT_EQ(1, count_in_log("Starting all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            ":' doesn't implement start()"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance1' starting"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance1' start exit ok"));
  EXPECT_EQ(1,
            count_in_log("  plugin '" + kPluginNameMagic + ":' start exit ok"));

  EXPECT_EQ(1, count_in_log("Shutting down. Stopping all plugins."));
  EXPECT_EQ(1,
            count_in_log("  plugin '" + kPluginNameLifecycle +
                         ":instance1' stop failed: "
                         "lifecycle:instance1 stop(): I'm returning error!"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            ":' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameMagic +
                            ":' doesn't implement stop()"));

  EXPECT_EQ(1, count_in_log("Deinitializing all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            "' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            "' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameMagic +
                            "' doesn't implement deinit()"));
}

TEST_F(LifecycleTest, ThreeInstances_InitFails) {
  config_text_ << "init   = error          \n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n"
               << "deinit = exit           \n"
               << "                        \n"
               << "[" + kPluginNameLifecycle + ":instance2]   \n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n"
               << "                        \n"
               << "[" + kPluginNameLifecycle + ":instance3]   \n"
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
  const std::list<std::string> initialized = {"logger", kPluginNameMagic,
                                              kPluginNameLifecycle3};
  EXPECT_EQ(initialized, loader_.order_);

  refresh_log();

  // lifecycle2 should not be initialized
  EXPECT_EQ(1, count_in_log("Initializing all plugins."));
  EXPECT_EQ(1,
            count_in_log("  plugin '" + kPluginNameMagic + "' init exit ok"));
  EXPECT_EQ(
      1, count_in_log("  plugin '" + kPluginNameLifecycle3 + "' init exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            "' init failed: "
                            "lifecycle:all init(): I'm returning error!"));
  // start() and stop() shouldn't run
  EXPECT_EQ(0, count_in_log("Starting all plugins."));
  EXPECT_EQ(0, count_in_log("Shutting down. Stopping all plugins."));

  // lifecycle2 should not be deinintialized
  EXPECT_EQ(1, count_in_log("Deinitializing all plugins."));
  EXPECT_EQ(0, count_in_log("  plugin '" + kPluginNameLifecycle +
                            "' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            "' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameMagic +
                            "' doesn't implement deinit()"));
}

TEST_F(LifecycleTest, BothLifecycles_InitFails) {
  config_text_ << "init   = error          \n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n"
               << "deinit = exit           \n"
               << "                        \n"
               << "[" + kPluginNameLifecycle2 + "]            \n";
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
  const std::list<std::string> initialized = {"logger", kPluginNameMagic,
                                              kPluginNameLifecycle3};
  EXPECT_EQ(initialized, loader_.order_);

  refresh_log();

  // lifecycle2 should not be initialized
  EXPECT_EQ(1, count_in_log("Initializing all plugins."));
  EXPECT_EQ(1,
            count_in_log("  plugin '" + kPluginNameMagic + "' init exit ok"));
  EXPECT_EQ(
      1, count_in_log("  plugin '" + kPluginNameLifecycle3 + "' init exit ok"));
  EXPECT_EQ(
      1, count_in_log("  plugin '" + kPluginNameLifecycle + "' initializing"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            "' init failed: "
                            "lifecycle:all init(): I'm returning error!"));
  EXPECT_EQ(
      0, count_in_log("  plugin " + kPluginNameLifecycle2 + "' initializing"));

  // start() and stop() shouldn't run
  EXPECT_EQ(0, count_in_log("Starting all plugins."));
  EXPECT_EQ(0, count_in_log("Shutting down. Stopping all plugins."));

  // lifecycle2 should not be deinintialized
  EXPECT_EQ(1, count_in_log("Deinitializing all plugins."));
  EXPECT_EQ(0, count_in_log("  plugin " + kPluginNameLifecycle2 +
                            "' deinitializing"));
  EXPECT_EQ(0, count_in_log("  plugin '" + kPluginNameLifecycle +
                            "' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            "' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameMagic +
                            "' doesn't implement deinit()"));
}

TEST_F(LifecycleTest, ThreeInstances_Start1Fails) {
  config_text_ << "init   = exit           \n"
               << "start  = error          \n"
               << "stop   = exit           \n"
               << "deinit = exit           \n"
               << "                        \n"
               << "[" + kPluginNameLifecycle + ":instance2]   \n"
               << "start  = exit           \n"
               << "stop   = exit           \n"
               << "                        \n"
               << "[" + kPluginNameLifecycle + ":instance3]   \n"
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

  const std::list<std::string> initialized = {
      "logger", kPluginNameMagic, kPluginNameLifecycle3, kPluginNameLifecycle};
  EXPECT_EQ(initialized, loader_.order_);

  refresh_log();

  EXPECT_EQ(1, count_in_log("Initializing all plugins."));

  EXPECT_EQ(1, count_in_log("Starting all plugins."));
  EXPECT_EQ(1,
            count_in_log("  plugin '" + kPluginNameMagic + ":' start exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            ":' doesn't implement start()"));
  EXPECT_EQ(1,
            count_in_log("  plugin '" + kPluginNameLifecycle +
                         ":instance1' start failed: "
                         "lifecycle:instance1 start(): I'm returning error!"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance2' starting"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance3' starting"));

  EXPECT_EQ(1, count_in_log("Shutting down. Stopping all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameMagic +
                            ":' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            ":' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance2' stop exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance3' stop exit ok"));

  EXPECT_EQ(1, count_in_log("Deinitializing all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            "' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            "' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameMagic +
                            "' doesn't implement deinit()"));
}

TEST_F(LifecycleTest, ThreeInstances_Start2Fails) {
  config_text_ << "init   = exit           \n"
               << "start  = exit           \n"
               << "stop   = exit           \n"
               << "deinit = exit           \n"
               << "                        \n"
               << "[" + kPluginNameLifecycle + ":instance2]   \n"
               << "start  = error          \n"
               << "stop   = exit           \n"
               << "                        \n"
               << "[" + kPluginNameLifecycle + ":instance3]   \n"
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

  const std::list<std::string> initialized = {
      "logger", kPluginNameMagic, kPluginNameLifecycle3, kPluginNameLifecycle};
  EXPECT_EQ(initialized, loader_.order_);

  refresh_log();

  EXPECT_EQ(1, count_in_log("Initializing all plugins."));

  EXPECT_EQ(1, count_in_log("Starting all plugins."));
  EXPECT_EQ(1,
            count_in_log("  plugin '" + kPluginNameMagic + ":' start exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            ":' doesn't implement start()"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance1' starting"));
  EXPECT_EQ(1,
            count_in_log("  plugin '" + kPluginNameLifecycle +
                         ":instance2' start failed: "
                         "lifecycle:instance2 start(): I'm returning error!"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance3' starting"));

  EXPECT_EQ(1, count_in_log("Shutting down. Stopping all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameMagic +
                            ":' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            ":' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance1' stop exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance3' stop exit ok"));

  EXPECT_EQ(1, count_in_log("Deinitializing all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            "' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            "' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameMagic +
                            "' doesn't implement deinit()"));
}

TEST_F(LifecycleTest, ThreeInstances_Start3Fails) {
  config_text_ << "init   = exit           \n"
               << "start  = exit           \n"
               << "stop   = exit           \n"
               << "deinit = exit           \n"
               << "                        \n"
               << "[" + kPluginNameLifecycle + ":instance2]   \n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n"
               << "                        \n"
               << "[" + kPluginNameLifecycle + ":instance3]   \n"
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

  const std::list<std::string> initialized = {
      "logger", kPluginNameMagic, kPluginNameLifecycle3, kPluginNameLifecycle};
  EXPECT_EQ(initialized, loader_.order_);

  refresh_log();

  EXPECT_EQ(1, count_in_log("Initializing all plugins."));

  EXPECT_EQ(1, count_in_log("Starting all plugins."));
  EXPECT_EQ(1,
            count_in_log("  plugin '" + kPluginNameMagic + ":' start exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            ":' doesn't implement start()"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance1' starting"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance2' starting"));
  EXPECT_EQ(1,
            count_in_log("  plugin '" + kPluginNameLifecycle +
                         ":instance3' start failed: "
                         "lifecycle:instance3 start(): I'm returning error!"));

  EXPECT_EQ(1, count_in_log("Shutting down. Stopping all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameMagic +
                            ":' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            ":' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance1' stop exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance2' stop exit ok"));

  EXPECT_EQ(1, count_in_log("Deinitializing all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            "' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            "' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameMagic +
                            "' doesn't implement deinit()"));
}

TEST_F(LifecycleTest, ThreeInstances_2StartsFail) {
  config_text_ << "init   = exit           \n"
               << "start  = error          \n"
               << "stop   = exit           \n"
               << "deinit = exit           \n"
               << "                        \n"
               << "[" + kPluginNameLifecycle + ":instance2]   \n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n"
               << "                        \n"
               << "[" + kPluginNameLifecycle + ":instance3]   \n"
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

  const std::list<std::string> initialized = {
      "logger", kPluginNameMagic, kPluginNameLifecycle3, kPluginNameLifecycle};
  EXPECT_EQ(initialized, loader_.order_);

  refresh_log();

  EXPECT_EQ(1, count_in_log("Initializing all plugins."));

  EXPECT_EQ(1, count_in_log("Starting all plugins."));
  EXPECT_EQ(1,
            count_in_log("  plugin '" + kPluginNameMagic + ":' start exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            ":' doesn't implement start()"));
  EXPECT_EQ(1,
            count_in_log("  plugin '" + kPluginNameLifecycle +
                         ":instance1' start failed: "
                         "lifecycle:instance1 start(): I'm returning error!"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance2' starting"));
  EXPECT_EQ(1,
            count_in_log("  plugin '" + kPluginNameLifecycle +
                         ":instance3' start failed: "
                         "lifecycle:instance3 start(): I'm returning error!"));

  EXPECT_EQ(1, count_in_log("Shutting down. Stopping all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameMagic +
                            ":' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            ":' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance2' stop exit ok"));

  EXPECT_EQ(1, count_in_log("Deinitializing all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            "' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            "' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameMagic +
                            "' doesn't implement deinit()"));
}

TEST_F(LifecycleTest, ThreeInstances_StopFails) {
  config_text_ << "init   = exit           \n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n"
               << "deinit = exit           \n"
               << "                        \n"
               << "[" + kPluginNameLifecycle + ":instance2]   \n"
               << "start  = exitonstop     \n"
               << "stop   = error          \n"
               << "                        \n"
               << "[" + kPluginNameLifecycle + ":instance3]   \n"
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
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameMagic + ":' starting"));
  EXPECT_EQ(1,
            count_in_log("  plugin '" + kPluginNameMagic + ":' start exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            ":' doesn't implement start()"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance1' starting"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance2' starting"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance3' starting"));

  EXPECT_EQ(1, count_in_log("Shutting down. Stopping all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameMagic +
                            ":' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            ":' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance1' stop exit ok"));
  EXPECT_EQ(1,
            count_in_log("  plugin '" + kPluginNameLifecycle +
                         ":instance2' stop failed: "
                         "lifecycle:instance2 stop(): I'm returning error!"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance3' stop exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance1' start exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance2' start exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance3' start exit ok"));

  EXPECT_EQ(1, count_in_log("Deinitializing all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            "' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            "' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameMagic +
                            "' doesn't implement deinit()"));
}

TEST_F(LifecycleTest, ThreeInstances_DeinintFails) {
  config_text_ << "init   = exit           \n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n"
               << "deinit = error          \n"
               << "                        \n"
               << "[" + kPluginNameLifecycle + ":instance2]   \n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n"
               << "                        \n"
               << "[" + kPluginNameLifecycle + ":instance3]   \n"
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
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            "' deinit failed: "
                            "lifecycle:all deinit(): I'm returning error!"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameMagic +
                            "' doesn't implement deinit()"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            "' deinit exit ok"));
}

TEST_F(LifecycleTest, ThreeInstances_StartStopDeinitFail) {
  config_text_ << "init   = exit           \n"
               << "start  = exitonstop     \n"
               << "stop   = exit           \n"
               << "deinit = error          \n"
               << "                        \n"
               << "[" + kPluginNameLifecycle + ":instance2]   \n"
               << "start  = error          \n"
               << "stop   = exit           \n"
               << "                        \n"
               << "[" + kPluginNameLifecycle + ":instance3]   \n"
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

  const std::list<std::string> initialized = {
      "logger", "" + kPluginNameMagic + "", "" + kPluginNameLifecycle3 + "",
      "" + kPluginNameLifecycle + ""};
  EXPECT_EQ(initialized, loader_.order_);

  refresh_log();

  EXPECT_EQ(1, count_in_log("Initializing all plugins."));

  EXPECT_EQ(1, count_in_log("Starting all plugins."));
  EXPECT_EQ(1,
            count_in_log("  plugin '" + kPluginNameMagic + ":' start exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            ":' doesn't implement start()"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance1' starting"));
  EXPECT_EQ(1,
            count_in_log("  plugin '" + kPluginNameLifecycle +
                         ":instance2' start failed: "
                         "lifecycle:instance2 start(): I'm returning error!"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance3' starting"));

  EXPECT_EQ(1, count_in_log("Shutting down. Stopping all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameMagic +
                            ":' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            ":' doesn't implement stop()"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            ":instance1' stop exit ok"));
  EXPECT_EQ(1,
            count_in_log("  plugin '" + kPluginNameLifecycle +
                         ":instance3' stop failed: "
                         "lifecycle:instance3 stop(): I'm returning error!"));

  EXPECT_EQ(1, count_in_log("Deinitializing all plugins."));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            "' deinit failed: "
                            "lifecycle:all deinit(): I'm returning error!"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle3 +
                            "' deinit exit ok"));
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameMagic +
                            "' doesn't implement deinit()"));
}

TEST_F(LifecycleTest, NoInstances) {
  // This test tests Loader's ability to correctly start up and shut down
  // without any plugins.  However note, that currently we expect our Router to
  // exit with an error when there's not plugins to run, but that is a
  // higher-level concern.  So while the check happens inside Loader (because
  // it's not possible to check from the outside), this test bypasses this
  // check.
  const std::string plugin_dir = mysql_harness::get_plugin_dir(g_here.str());
  config_text_.str(
      "[DEFAULT]                                      \n"
      "logging_folder =                               \n"
      "plugin_folder  = " +
      plugin_dir +
      "\n"
      "runtime_folder = {prefix}                      \n"
      "config_folder  = {prefix}                      \n"
      "                                               \n"
      "[logger]                                       \n"
      "level = DEBUG                                  \n"
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
  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            "' init "
                            "failed: <empty message>"));
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

/**
 * @test
 * This test verifies operation of Harness API function wait_for_stop().
 * It is tested in two scenarios:
 *   1. when Router is "running": it should block until timeout expires
 *   2. when Router is "stopping": it should exit immediately
 */
TEST_F(LifecycleTest, wait_for_stop) {
  // SCENARIO #1: When Router is "running"
  // EXPECTATION:
  //   wait_for_stop() inside should block for 100ms, then return false (time
  //   out)
  // EXPLANATION:
  //   When plugin function start() is called, Router will be in a "running"
  //   state. Inside start() calls wait_for_stop(timeout = 100ms), which
  //   means wait_for_stop() SHOULD block and time out after 100ms. Then the
  //   start() will just exit, and when it does that, it will cause Router to
  //   initiate shutdown (and set the shutdown flag), as there are no more
  //   plugins running.
  config_text_ << "start = exitonstop_shorttimeout\n";

  // SCENARIO #2: When Router is "stopping"
  // EXPECTATION:
  //   wait_for_stop() inside should return immediately, then return true (due
  //   to shut down flag being set)
  // EXPLANATION:
  //   Now that start() has exited, Router has progressed to "stopping" state,
  //   and as a result, plugin function stop() will be called. stop() makes a
  //   call to wait_for_stop(<big timeout value>). Since this time around,
  //   Router is already in the "stopping" state, the function SHOULD exit
  //   immediately, returing control back to stop(), which just exits after.
  config_text_ << "stop  = exitonstop_longtimeout\n";

  init_test(config_text_, {false, true, true, false});
  LifecyclePluginSyncBus &bus = msg_bus("instance1");

  EXPECT_EQ(loader_.init_all(), nullptr);
  freeze_bus(bus);

  ch::time_point<ch::steady_clock> t0, t1;

  // run scenarios #1 and #2
  {
    t0 = ch::steady_clock::now();
    loader_.start_all();

    // wait to enter scenario #1
    unfreeze_and_wait_for_msg(
        bus, "lifecycle:instance1 start():EXIT_ON_STOP_SHORT_TIMEOUT:sleeping");

    // we are now in scenario #1
    // (wait_for_stop() in start() should be sleeping right now; main_loop() is
    // blocked waiting for start() to exit)
    refresh_log();
    // clang-format off
    EXPECT_EQ(1, count_in_log("lifecycle:instance1 start():begin"));
    EXPECT_EQ(1, count_in_log("lifecycle:instance1 start():EXIT_ON_STOP_SHORT_TIMEOUT:sleeping"));
    EXPECT_EQ(0, count_in_log("lifecycle:instance1 start():EXIT_ON_STOP_SHORT_TIMEOUT:done, ret = true (stop request received)"));
    EXPECT_EQ(0, count_in_log("lifecycle:instance1 start():EXIT_ON_STOP_SHORT_TIMEOUT:done, ret = false (timed out)"));
    EXPECT_EQ(0, count_in_log("lifecycle:instance1 stop():begin"));
    EXPECT_EQ(0, count_in_log("lifecycle:instance1 stop():EXIT_ON_STOP_LONG_TIMEOUT:done, ret = true (stop request received)"));
    EXPECT_EQ(0, count_in_log("lifecycle:instance1 stop():EXIT_ON_STOP_LONG_TIMEOUT:done, ret = false (timed out)"));
    // clang-format on

    // wait for scenario #1 to finish and scenario #2 to run
    // (start() should exit without error, causing main_loop() to unblock and
    // progress to calling stop(), then finally return)
    EXPECT_EQ(loader_.main_loop(), nullptr);

    // stop the timer
    t1 = ch::steady_clock::now();
  }

  // verify expectations
  {
    // first, we measure the time to run scenarios #1 and #2:
    // - Scenario #1 should take 100+ ms to execute (wait_for_stop() should
    //   block for 100ms, everything else is fast)
    // - Scenario #2 should take close to 0 ms to execute (wait_for_stop()
    //   should return immiedately, everything else is fast)
    //
    // Therefore, we expect that the cumulative time should be close to just
    // over 100ms:
    // - if it was less than 100ms, scenario #1 must have failed
    //   (wait_for_stop() failed to block).
    // - if it takes 10 seconds or more, scenario #2 must have failed
    //   (wait_for_stop(timeout = 10 seconds) timed out, instead of returning
    //   immediately)

    // NOTE about a choice of timeout (10 seconds):
    // 10s timeout is a little arbitrary.  In theory, all we need is something
    // just a little over 100ms, since Scenario #2 has no blocking states and
    // should run really quick.  So we might be tempted to pick something like
    // 110ms or 200ms, however as we have learned, it's possible to exceed such
    // timeout on a busy OSX machine and fail the test.  The fault lies with
    // calls to std::condition_variable::wait_for() (called inside of
    // wait_for_stop()) which calls syscall psync_cvwait().  Deeper underneath,
    // it turns out that unless a thread making this syscall has heightened
    // priority (which it does not), OSX is free to delay delivering signal
    // for performance reasons.
    //
    // We don't bother #ifdef-ing the timeout for OSX, because in principle,
    // many/all non-RT OSes probably have no tight guarrantees for wait_for()
    // just like OSX, and an excessive timeout value does not slow down the
    // test run time.

    // expect 100ms <= (t1-t0) < 10s
    EXPECT_LE(100, time_diff(t0, t1));        // 100 = scenario #1 timeout
    EXPECT_GT(10 * 1000, time_diff(t0, t1));  // 10000 = scenario #2 timeout

    // verify what both wait_for_stop()'s returned
    refresh_log();
    // clang-format off
    EXPECT_EQ(0, count_in_log("lifecycle:instance1 start():EXIT_ON_STOP_SHORT_TIMEOUT:done, ret = true (stop request received)"));
    EXPECT_EQ(1, count_in_log("lifecycle:instance1 start():EXIT_ON_STOP_SHORT_TIMEOUT:done, ret = false (timed out)"));
    EXPECT_EQ(1, count_in_log("lifecycle:instance1 stop():begin"));
    EXPECT_EQ(1, count_in_log("lifecycle:instance1 stop():EXIT_ON_STOP_LONG_TIMEOUT:done, ret = true (stop request received)"));
    EXPECT_EQ(0, count_in_log("lifecycle:instance1 stop():EXIT_ON_STOP_LONG_TIMEOUT:done, ret = false (timed out)"));
    // clang-format on
  }
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
          "  plugin '" + kPluginNameLifecycle +
          "' init threw unexpected "
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
             "  plugin '" + kPluginNameLifecycle +
             ":instance1' start threw "
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
             "  plugin '" + kPluginNameLifecycle +
             ":instance1' stop threw "
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
          "  plugin '" + kPluginNameLifecycle +
          "' deinit threw unexpected "
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

  EXPECT_EQ(1, count_in_log("  plugin '" + kPluginNameLifecycle +
                            "' init threw unexpected "
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
      1, count_in_log("  plugin '" + kPluginNameLifecycle +
                      ":instance1' start "
                      "threw unexpected "
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

  EXPECT_THAT(log_lines_,
              ::testing::Contains(::testing::HasSubstr(
                  "  plugin '" + kPluginNameLifecycle +
                  ":instance1' stop threw "
                  "unexpected "
                  "exception - please contact plugin developers for "
                  "more information.")));
}

TEST_F(LifecycleTest, DeinitThrowsWeird) {
  config_text_ << "deinit = throw_weird\n";
  init_test(config_text_, {false, false, false, true});

  try {
    std::exception_ptr e = loader_.run();
    if (e) std::rethrow_exception(e);
    FAIL() << "deinit() should throw non-standard exception object";
  } catch (const std::runtime_error &e) {
    FAIL() << "deinit() should throw non-standard exception object, got "
           << e.what();
  } catch (...) {
  }

  refresh_log();

  EXPECT_THAT(log_lines_,
              ::testing::Contains(::testing::HasSubstr(
                  "  plugin '" + kPluginNameLifecycle +
                  "' deinit threw unexpected "
                  "exception - please contact plugin developers for "
                  "more information.")));
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
