/*
  Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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
// This test plugin is used to test Harness' handling of plugin lifecycle.
// The plugin exposes all 4 lifecycle functions (init, start, stop and deinit),
// and what they do (how they exit, i.e. whether they throw, exit, block, etc)
// depends on the plugin configuration. For details, see comments in
// init_exit_strategies().
//
////////////////////////////////////////////////////////////////////////////////

#include "lifecycle.h"

#include <array>
#include <bitset>
#include <chrono>
#include <condition_variable>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <thread>

#include "harness_assert.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin.h"

#include "my_compiler.h"

IMPORT_LOG_FUNCTIONS()

namespace mysql_harness {
class PluginFuncEnv;
}

const int kExitCheckInterval = 1;
const int kExitOnStopShortTimeout = 100;
const int kExitOnStopLongTimeout = 60 * 1000;

////////////////////////////////////////////////////////////////////////////////
// ITC STUFF (InterThread Communication)
////////////////////////////////////////////////////////////////////////////////

namespace {

static std::array<mysql_harness::test::LifecyclePluginSyncBus, 3>
    g_lifecycle_plugin_logs;

static mysql_harness::test::LifecyclePluginSyncBus *get_bus_from_key(
    const std::string &k) {
  if (k == "instance1" || k == "all")
    return &g_lifecycle_plugin_logs[0];
  else if (k == "instance2")
    return &g_lifecycle_plugin_logs[1];
  else if (k == "instance3")
    return &g_lifecycle_plugin_logs[2];

  // unsupported instance name
  harness_assert_this_should_not_execute();
}

MY_ATTRIBUTE((format(printf, 3, 4)))
static void log_info(bool notify, const std::string &key, const char *format,
                     ...) {
  char buf[1024];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);

  // call the real log_info()
  ::log_info("%s", buf);

  // and also post notification on ITC bus, if requested
  if (notify) {
    mysql_harness::test::LifecyclePluginSyncBus &bus = *get_bus_from_key(key);
    bus.mtx.lock();
    bus.msg = buf;
    bus.mtx.unlock();
    bus.cv.notify_one();
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// PLUGIN IMPLEMENTATION
////////////////////////////////////////////////////////////////////////////////

// proxy useful for debugging, keep it disabled unless developing this code
#if 0
#include <stdarg.h>  // some things not in std:: in cstdarg (Ubuntu 14.04)
#include <iostream>

  static void LOG_INFO(const char* format, ...) {
    char buf[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    std::cerr << "===>" << buf << std::endl;
    log_info("%s", buf);
  }
#define log_info LOG_INFO
#endif

namespace {

// we start with 123 so that we're likely to detect a bug if the enum is not set
// (uninitialized memory often contains 0)
enum ExitType {
  ET_EXIT = 123,
  ET_EXIT_ON_STOP_SHORT_TIMEOUT,
  ET_EXIT_ON_STOP_LONG_TIMEOUT,
  ET_EXIT_ON_STOP,
  ET_EXIT_ON_STOP_SYNC,
  ET_THROW,
  ET_THROW_WEIRD,
  ET_ERROR,
  ET_ERROR_EMPTY,
};

struct ExitStrategy {
  std::map<std::string, ExitType> exit_type;
  bool strategy_set;
};

std::map<std::string, ExitStrategy> g_strategies;
std::mutex g_strategies_mtx;

// called at the earliest opportunity, needs to run only once
// (since last reset)
void init_exit_strategies(const mysql_harness::ConfigSection *section) {
  std::lock_guard<std::mutex> lock(g_strategies_mtx);

  // running more than once doesn't change anything, just wastes cycles
  // and obfuscates purpose of this code
  if (g_strategies[section->key].strategy_set) {
    return;
  } else {
    g_strategies[section->key].strategy_set = true;
  }

  // clang-format off
  //
  // Each function's behavior (exit strategy) is defined inside the
  // configuration file, one line per function. General definition form:
  //
  //   (init|start|stop|deinit) = <option>
  //
  // where <option> is one of:
  //   exit         - exit right away
  //   exitonstop_shorttimeout - exit after stop() or short timeout
  //   exitonstop_longtimeout  - exit after stop() or long timeout
  //   exitonstop   - exit after stop(), async polling (valid for start() only)
  //   exitonstop_s - exit after stop(), blocking      (valid for start() only)
  //   throw        - throw a typical exception (derived from std::exception)
  //   throw_weird  - throw an unusual exception (not derived from std::exception)
  //   error        - exit with error (like 'exit', but call set_error() before exiting)
  //   error_empty  - like above, but set_error(..., NULL)
  //
  // Example configuration section:
  //
  //   [lifecycle]
  //   init   = exit        # init() will exit
  //   start  = exitonstop  # start() will exit after it gets notified to do so
  //   stop   = throw       # stop() will throw
  //   deinit = exitonstop_shorttimeout   # deinit() will never exit
  //
  // clang-format on

  // process configuration
  for (const char *func : {"init", "start", "stop", "deinit"}) {
    if (section->has(func)) {
      const std::string &line = section->get(func);

      // assign exit strategy
      if (line.find("exitonstop_shorttimeout") != std::string::npos) {
        g_strategies[section->key].exit_type[func] =
            ET_EXIT_ON_STOP_SHORT_TIMEOUT;
      } else if (line.find("exitonstop_longtimeout") != std::string::npos) {
        g_strategies[section->key].exit_type[func] =
            ET_EXIT_ON_STOP_LONG_TIMEOUT;
      } else if (line.find("throw_weird") != std::string::npos) {
        g_strategies[section->key].exit_type[func] = ET_THROW_WEIRD;
      } else if (line.find("throw") != std::string::npos) {
        g_strategies[section->key].exit_type[func] = ET_THROW;
      } else if (line.find("error_empty") != std::string::npos) {
        g_strategies[section->key].exit_type[func] = ET_ERROR_EMPTY;
      } else if (line.find("error") != std::string::npos) {
        g_strategies[section->key].exit_type[func] = ET_ERROR;
      } else if (line.find("exitonstop_s") != std::string::npos &&
                 strcmp(func, "start") == 0) {
        g_strategies[section->key].exit_type[func] = ET_EXIT_ON_STOP_SYNC;
      } else if (line.find("exitonstop") != std::string::npos &&
                 strcmp(func, "start") == 0) {
        g_strategies[section->key].exit_type[func] = ET_EXIT_ON_STOP;
      } else if (line.find("exit") != std::string::npos) {
        g_strategies[section->key].exit_type[func] = ET_EXIT;
      } else {
        // invalid exit strategy (your unit test is messed up)
        harness_assert_this_should_not_execute();
      }
    }
  }
}

void execute_exit_strategy(const std::string &func,
                           mysql_harness::PluginFuncEnv *env) {
  // init() and deinit() are called only once per plugin (not per plugin
  // instance), but we need an instance name for our logic to work, therefore
  // we pick the first plugin instance in such case
  const std::string &key = (func == "init" || func == "deinit")
                               ? get_app_info(env)
                                     ->config->get("routertestplugin_lifecycle")
                                     .front()
                                     ->key
                               : get_config_section(env)->key;

  std::unique_lock<std::mutex> lock(g_strategies_mtx);

  // init() and deinit() are called only once per plugin, so "all" is less
  // confusing for those functions
  const char *key_for_log =
      (func == "init" || func == "deinit") ? "all" : key.c_str();

  // in case of start() function (which runs in a separate thread), in
  // addition to logging, we also want to post a notification on ITC bus
  bool notify = (func == "start");

  switch (g_strategies[key].exit_type[func]) {
    case ET_EXIT:
      log_info(notify, key_for_log, "  lifecycle:%s %s():EXIT.", key_for_log,
               func.c_str());
      return;  // added . here ^ so EXIT* don't match str search

    case ET_THROW:  // added . here v so THROW_WEIRD don't match str search
      log_info(notify, key_for_log, "  lifecycle:%s %s():THROW.", key_for_log,
               func.c_str());
      throw std::runtime_error(std::string("lifecycle:") + key_for_log + " " +
                               func + "(): I'm throwing!");
    case ET_THROW_WEIRD:
      log_info(notify, key_for_log, "  lifecycle:%s %s():THROW_WEIRD",
               key_for_log, func.c_str());
      throw int(42);  // throw something that's not a std::exception

    case ET_ERROR:
      log_info(notify, key_for_log, "  lifecycle:%s %s():ERROR", key_for_log,
               func.c_str());
      set_error(env, mysql_harness::kRuntimeError,
                "lifecycle:%s %s(): I'm returning error!", key_for_log,
                func.c_str());
      return;

    case ET_ERROR_EMPTY:
      log_info(notify, key_for_log, "  lifecycle:%s %s():ERROR_EMPTY",
               key_for_log, func.c_str());
      set_error(env, mysql_harness::kRuntimeError, nullptr);
      return;

    case ET_EXIT_ON_STOP_SHORT_TIMEOUT:
      log_info(notify, key_for_log,
               "  lifecycle:%s %s():EXIT_ON_STOP_SHORT_TIMEOUT:sleeping",
               key_for_log, func.c_str());
      if (wait_for_stop(env, kExitOnStopShortTimeout))
        log_info(notify, key_for_log,
                 "  lifecycle:%s %s():EXIT_ON_STOP_SHORT_TIMEOUT:done, ret = "
                 "true (stop request received)",
                 key_for_log, func.c_str());
      else
        log_info(notify, key_for_log,
                 "  lifecycle:%s %s():EXIT_ON_STOP_SHORT_TIMEOUT:done, ret = "
                 "false (timed out)",
                 key_for_log, func.c_str());
      return;

    case ET_EXIT_ON_STOP_LONG_TIMEOUT:
      log_info(notify, key_for_log,
               "  lifecycle:%s %s():EXIT_ON_STOP_LONG_TIMEOUT:sleeping",
               key_for_log, func.c_str());
      if (wait_for_stop(env, kExitOnStopLongTimeout))
        log_info(notify, key_for_log,
                 "  lifecycle:%s %s():EXIT_ON_STOP_LONG_TIMEOUT:done, ret = "
                 "true (stop request received)",
                 key_for_log, func.c_str());
      else
        log_info(notify, key_for_log,
                 "  lifecycle:%s %s():EXIT_ON_STOP_LONG_TIMEOUT:done, ret = "
                 "false (timed out)",
                 key_for_log, func.c_str());
      return;

    case ET_EXIT_ON_STOP:
      log_info(notify, key_for_log, "  lifecycle:%s %s():EXIT_ON_STOP:sleeping",
               key_for_log, func.c_str());
      harness_assert(func == "start");
      lock.unlock();  // we don't need it anymore
      while (is_running(env)) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(kExitCheckInterval));
      }
      log_info(notify, key_for_log, "  lifecycle:%s %s():EXIT_ON_STOP:done",
               key_for_log, func.c_str());
      return;

    case ET_EXIT_ON_STOP_SYNC:
      log_info(notify, key_for_log,
               "  lifecycle:%s %s():EXIT_ON_STOP_SYNC:sleeping", key_for_log,
               func.c_str());
      harness_assert(func == "start");
      lock.unlock();  // we don't need it anymore
      wait_for_stop(env, 0);
      log_info(notify, key_for_log,
               "  lifecycle:%s %s():EXIT_ON_STOP_SYNC:done", key_for_log,
               func.c_str());
      return;
  }
}

}  // unnamed namespace

////////////////////////////////////////////////////////////////////////////////
// PLUGIN API
////////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER) && defined(routertestplugin_lifecycle_EXPORTS)
/* We are building this library */
#define LIFECYCLE_API __declspec(dllexport)
#else
#define LIFECYCLE_API
#endif

static std::array<const char *, 2> requires_plugins = {
    "routertestplugin_magic (>>1.0)",
    "routertestplugin_lifecycle3",
};

static void init(mysql_harness::PluginFuncEnv *env) {
  const auto *info = get_app_info(env);

  // init() and deinit() are called only once per plugin (not per plugin
  // instance), but we need an instance name for our logic to work, therefore
  // we pick the first plugin instance in such case
  const auto *section = info->config->get("routertestplugin_lifecycle").front();

  // only 3 predefined instances are supported
  harness_assert(section->key == "instance1" || section->key == "instance2" ||
                 section->key == "instance3");

  log_info(false, section->key, "%s", "lifecycle:all init():begin");

  init_exit_strategies(section);
  execute_exit_strategy("init", env);
}

static void start(mysql_harness::PluginFuncEnv *env) {
  const auto *section = get_config_section(env);

  log_info(true, section->key, "lifecycle:%s start():begin",
           section->key.c_str());

  init_exit_strategies(section);
  execute_exit_strategy("start", env);
}

static void stop(mysql_harness::PluginFuncEnv *env) {
  const auto *section = get_config_section(env);

  log_info(false, section->key, "lifecycle:%s stop():begin",
           section->key.c_str());

  init_exit_strategies(section);
  execute_exit_strategy("stop", env);
}

static void deinit(mysql_harness::PluginFuncEnv *env) {
  const auto *info = get_app_info(env);

  // init() and deinit() are called only once per plugin (not per plugin
  // instance), but we need an instance name for our logic to work, therefore
  // we pick the first plugin instance in such case
  const auto *section = info->config->get("routertestplugin_lifecycle").front();

  // only 3 predefined instances are supported
  harness_assert(section->key == "instance1" || section->key == "instance2" ||
                 section->key == "instance3");

  log_info(false, section->key, "%s", "lifecycle:all deinit():begin");

  init_exit_strategies(section);
  execute_exit_strategy("deinit", env);
}

extern "C" {
LIFECYCLE_API mysql_harness::Plugin harness_plugin_routertestplugin_lifecycle =
    {
        mysql_harness::PLUGIN_ABI_VERSION,       // abi-version
        mysql_harness::ARCHITECTURE_DESCRIPTOR,  // arch
        "Lifecycle test plugin",                 // name
        VERSION_NUMBER(1, 0, 0),
        // requires_plugins
        requires_plugins.size(),
        requires_plugins.data(),
        // conflicts
        0,
        nullptr,
        init,    // init
        deinit,  // deinit
        start,   // start
        stop,    // stop
        false,   // declares_readiness
        0,
        nullptr,
};

LIFECYCLE_API void lifecycle_init(int flags) {
  using namespace mysql_harness::test::PluginDescriptorFlags;

  auto &plugin_info = harness_plugin_routertestplugin_lifecycle;

  plugin_info.init = (flags & NoInit) ? nullptr : init;
  plugin_info.deinit = (flags & NoDeinit) ? nullptr : deinit;
  plugin_info.start = (flags & NoStart) ? nullptr : start;
  plugin_info.stop = (flags & NoStop) ? nullptr : stop;

  // initialize the plugin
  {
    std::lock_guard<std::mutex> lock(g_strategies_mtx);
    g_strategies.clear();

    for (const char *key : {"instance1", "instance2", "instance3"}) {
      g_strategies[key].strategy_set = false;  // optimisation,
    }                                          // doesn't affect behavior
  }
}

LIFECYCLE_API mysql_harness::test::LifecyclePluginSyncBus *
lifecycle_get_bus_from_key(const std::string &name) {
  return get_bus_from_key(name);
}
}
