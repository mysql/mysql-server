/*
  Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.

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

#define MYSQL_ROUTER_LOG_DOMAIN \
  ::mysql_harness::logging::kMainLogger  // must precede #include "logging.h"
#include "mysql/harness/loader.h"

////////////////////////////////////////
// Package include files
#include "builtin_plugins.h"
#include "common.h"  // mysql_harness::rename_thread()
#include "designator.h"
#include "dim.h"
#include "exception.h"
#include "harness_assert.h"
#include "my_stacktrace.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/logging/registry.h"
#include "mysql/harness/plugin.h"
#include "utilities.h"
IMPORT_LOG_FUNCTIONS()

#include "my_compiler.h"

////////////////////////////////////////
// Standard include files
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstring>
#include <exception>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>

#ifndef _WIN32
#include <dlfcn.h>
#include <unistd.h>
#endif

// <cassert> places assert() in global namespace on Ubuntu14.04, but might
// place it in std:: on other platforms
#include <assert.h>

// safer than using cstdarg because va_* family of things might be macros or
// functions on different platforms, in which case they will either have std::
// prefix or they won't. Here's an example from QNX:
//   https://svn.boost.org/trac/boost/ticket/3133
#include <stdarg.h>

// need POSIX signals and threads to support signal handling (pthread_sigmask(),
// sigaction() and friends). For platforms that do not have them (e.g. Windows),
// a different mechanism is used instead (see proxy_main()).
// Compiler on Solaris does not always define _POSIX_C_SOURCE although the
// signal handling is there
#if (!defined _WIN32)
#define USE_POSIX_SIGNALS
#endif

#ifdef USE_POSIX_SIGNALS
#include <pthread.h>
#endif

using mysql_harness::utility::find_range_first;
using mysql_harness::utility::make_range;
using mysql_harness::utility::reverse;

using mysql_harness::Config;
using mysql_harness::Path;

using std::ostringstream;

/**
 * @defgroup Loader Plugin loader
 *
 * Plugin loader for loading and working with plugins.
 */

////////////////////////////////////////////////////////////////////////////////
//
// Signal handling
//
////////////////////////////////////////////////////////////////////////////////

std::mutex we_might_shutdown_cond_mutex;
std::condition_variable we_might_shutdown_cond;

enum ShutdownReason { SHUTDOWN_NONE, SHUTDOWN_REQUESTED, SHUTDOWN_FATAL_ERROR };

// set when the Router receives a signal to shut down or some fatal error
// condition occurred
static std::atomic<ShutdownReason> g_shutdown_pending{SHUTDOWN_NONE};

// the thread that is setting the g_shutdown_pending to SHUTDOWN_FATAL_ERROR is
// supposed to set this error message so that it bubbles up and ends up on the
// console
static std::string shutdown_fatal_error_message;

std::mutex log_reopen_cond_mutex;
std::condition_variable log_reopen_cond;
static std::atomic<bool> g_log_reopen_requested{false};

/**
 * request application shutdown.
 *
 * @throws std::system_error same as std::unique_lock::lock does
 */
static void request_application_shutdown(const ShutdownReason reason) {
  {
    std::unique_lock<std::mutex> lk(we_might_shutdown_cond_mutex);
    std::unique_lock<std::mutex> lk2(log_reopen_cond_mutex);
    g_shutdown_pending = reason;
  }

  we_might_shutdown_cond.notify_one();
  // let's wake the log_reopen_thread too
  log_reopen_cond.notify_one();
}

/**
 * request application shutdown.
 *
 * @throws std::system_error same as std::unique_lock::lock does
 */
void request_application_shutdown() {
  request_application_shutdown(SHUTDOWN_REQUESTED);
}

/**
 * notify a "log_reopen" is requested.
 *
 * @throws std::system_error same as std::unique_lock::lock does
 */
static void request_log_reopen() {
  {
    std::unique_lock<std::mutex> lk(log_reopen_cond_mutex);
    g_log_reopen_requested = true;
  }
  log_reopen_cond.notify_one();
}

namespace {
#ifdef USE_POSIX_SIGNALS
const std::array<int, 6> g_fatal_signals{SIGSEGV, SIGABRT, SIGBUS,
                                         SIGILL,  SIGFPE,  SIGTRAP};
#endif
}  // namespace

static void block_all_nonfatal_signals() {
#ifdef USE_POSIX_SIGNALS
  sigset_t ss;
  sigfillset(&ss);
  // we can't block those signals globally and rely on our handler thread, as
  // these are only received by the offending thread itself.
  // see "man signal" for more details
  for (const auto &sig : g_fatal_signals) {
    sigdelset(&ss, sig);
  }
  if (0 != pthread_sigmask(SIG_SETMASK, &ss, nullptr)) {
    throw std::runtime_error("pthread_sigmask() failed: " +
                             std::string(std::strerror(errno)));
  }
#endif  // USE_POSIX_SIGNALS
}

#ifdef USE_POSIX_SIGNALS
static void handle_fatal_signal(int sig) {
  my_safe_printf_stderr("Application got fatal signal: %d\n", sig);
#ifdef HAVE_STACKTRACE
  my_print_stacktrace(nullptr, 0);
#endif  // HAVE_STACKTRACE
}
#endif  // USE_POSIX_SIGNALS

static void register_fatal_signal_handler() {
#ifdef USE_POSIX_SIGNALS
#ifdef HAVE_STACKTRACE
  my_init_stacktrace();
#endif  // HAVE_STACKTRACE

  struct sigaction sa;
  (void)sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESETHAND;
  sa.sa_handler = handle_fatal_signal;
  for (const auto &sig : g_fatal_signals) {
    (void)sigaction(sig, &sa, NULL);
  }
#endif  // USE_POSIX_SIGNALS
}

static void start_and_detach_signal_handler_thread() {
#ifdef USE_POSIX_SIGNALS
  std::promise<void> signal_handler_thread_setup_done;

  std::thread signal_thread([&signal_handler_thread_setup_done] {
    mysql_harness::rename_thread("sig handler");

    sigset_t ss;
    sigemptyset(&ss);
    sigaddset(&ss, SIGINT);
    sigaddset(&ss, SIGTERM);
    sigaddset(&ss, SIGHUP);

    signal_handler_thread_setup_done.set_value();
    int sig = 0;

    while (true) {
      if (0 == sigwait(&ss, &sig)) {
        if (sig == SIGHUP) {
          request_log_reopen();
        } else {
          harness_assert(sig == SIGINT || sig == SIGTERM);
          request_application_shutdown();
          return;
        }
      } else {
        // man sigwait() says, it should only fail if we provided invalid
        // signals.
        harness_assert_this_should_not_execute();
      }
    }
  });

  // wait until the signal handler is setup
  signal_handler_thread_setup_done.get_future().wait();

  // let the signal handler thread be independent of the rest of the app
  signal_thread.detach();
#endif
}

static void log_reopen_thread_function() {
  auto &logging_registry = mysql_harness::DIM::instance().get_LoggingRegistry();
  bool request_shutdown{false};
  {
    std::unique_lock<std::mutex> lk(log_reopen_cond_mutex);
    log_reopen_cond.wait(lk, [&] {
      if (g_shutdown_pending) {
        return true;
      }
      if (g_log_reopen_requested) {
        g_log_reopen_requested = false;
        try {
          logging_registry.flush_all_loggers();
        } catch (const std::exception &e) {
          // we have to postpone the shutdown request as it locks the
          // mutex that we currently hold
          request_shutdown = true;
          shutdown_fatal_error_message = e.what();
          return true;
        }
      }
      return false;
    });
  }

  if (request_shutdown) {
    request_application_shutdown(SHUTDOWN_FATAL_ERROR);
  }
}

#ifdef _WIN32
static BOOL WINAPI ctrl_c_handler(DWORD ctrl_type) {
  if (ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_BREAK_EVENT) {
    // user presed Ctrl+C or we got Ctrl+Break request
    request_application_shutdown();
    return TRUE;  // don't pass this event to further handlers
  } else {
    // some other event
    return FALSE;  // let the default Windows handler deal with it
  }
}

void register_ctrl_c_handler() {
  if (!SetConsoleCtrlHandler(ctrl_c_handler, TRUE)) {
    std::cerr << "Could not install Ctrl+C handler, exiting.\n";
    exit(1);
  }
}
#endif

namespace mysql_harness {

////////////////////////////////////////////////////////////////////////////////
//
// PluginFuncEnv
//
////////////////////////////////////////////////////////////////////////////////

PluginFuncEnv::PluginFuncEnv(const AppInfo *info, const ConfigSection *section,
                             bool running /*= false*/)
    : app_info_(info), config_section_(section), running_(running) {}

//----[ further config getters ]----------------------------------------------

const ConfigSection *PluginFuncEnv::get_config_section() const noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  assert(config_section_);
  return config_section_;
}

const AppInfo *PluginFuncEnv::get_app_info() const noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  assert(app_info_);
  return app_info_;
}

//----[ running flag ]--------------------------------------------------------

void PluginFuncEnv::set_running() noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  running_ = true;
}

void PluginFuncEnv::clear_running() noexcept {
  std::unique_lock<std::mutex> lock(mutex_);
  running_ = false;
  lock.unlock();
  cond_.notify_all();  // for wait_for_stop()
}

bool PluginFuncEnv::is_running() const noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  return running_;
}

bool PluginFuncEnv::wait_for_stop(uint32_t milliseconds) const noexcept {
  auto pred = [this]() noexcept->bool { return !running_; };

  std::unique_lock<std::mutex> lock(mutex_);
  if (milliseconds)  // 0 = wait forever
    cond_.wait_for(lock, std::chrono::milliseconds(milliseconds), pred);
  else
    cond_.wait(lock, pred);
  return pred();
}

//----[ error handling ]------------------------------------------------------

bool PluginFuncEnv::exit_ok() const noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  return error_type_ == kNoError;
}

void PluginFuncEnv::set_error(ErrorType error_type, const char *fmt,
                              va_list ap) noexcept {
  std::lock_guard<std::mutex> lock(mutex_);

  assert(error_message_.empty());   // \_ previous message wasn't consumed
  assert(error_type_ == kNoError);  // /
  assert(error_type != kNoError);   // what would be the purpose of that?

  error_type_ = error_type;
  if (fmt) {
    char buf[1024] = {0};
    vsnprintf(buf, sizeof(buf), fmt, ap);
    error_message_ = buf;
  } else {
    error_message_ = "<empty message>";
  }
}

////////////////////////////////////////////////////////////////////////////////
//
// Harness API
//
////////////////////////////////////////////////////////////////////////////////

//----[ further config getters ]----------------------------------------------

const AppInfo *get_app_info(const PluginFuncEnv *env) noexcept {
  return env->get_app_info();
}

const ConfigSection *get_config_section(const PluginFuncEnv *env) noexcept {
  return env->get_config_section();
}

//----[ running flag ]--------------------------------------------------------

bool is_running(const PluginFuncEnv *env) noexcept { return env->is_running(); }

bool wait_for_stop(const PluginFuncEnv *env, uint32_t milliseconds) noexcept {
  return env->wait_for_stop(milliseconds);
}

void clear_running(PluginFuncEnv *env) noexcept { return env->clear_running(); }

//----[ error handling ]------------------------------------------------------
MY_ATTRIBUTE((format(printf, 3, 4)))
void set_error(PluginFuncEnv *env, ErrorType error_type, const char *fmt,
               ...) noexcept {
  va_list args;
  va_start(args, fmt);
  env->set_error(error_type, fmt, args);
  va_end(args);
}

std::tuple<std::string, std::exception_ptr>
PluginFuncEnv::pop_error() noexcept {
  std::lock_guard<std::mutex> lock(mutex_);

  // At the time of writing, the exception type was used in Router's main.cc
  // to discriminate between error types, to give the user a hint of what
  // caused the problem (configuration error, runtime error, etc).
  std::tuple<std::string, std::exception_ptr> ret;
  switch (error_type_) {
    case kRuntimeError:
      ret = std::make_tuple(
          error_message_,
          std::make_exception_ptr(std::runtime_error(error_message_)));
      break;

    case kConfigInvalidArgument:
      ret = std::make_tuple(
          error_message_,
          std::make_exception_ptr(std::invalid_argument(error_message_)));
      break;

    case kConfigSyntaxError:
      ret = std::make_tuple(
          error_message_,
          std::make_exception_ptr(mysql_harness::syntax_error(error_message_)));
      break;

    case kUndefinedError:
      ret = std::make_tuple(
          error_message_,
          std::make_exception_ptr(std::runtime_error(error_message_)));
      break;

    case kNoError:
      assert(0);  // this function shouldn't be called in such case

      // defensive programming:
      // on production systems, default to runtime_error and go on
      ret = std::make_tuple(
          error_message_,
          std::make_exception_ptr(std::runtime_error(error_message_)));
      break;
  }

  error_type_ = kNoError;
  error_message_.clear();

  return ret;
}

// PluginThreads

/**
 * join all threads.
 *
 * @throws std::system_error from std::thread::join()
 */
void PluginThreads::join() {
  // wait for all plugin-threads to join
  for (auto &thr : threads_) {
    if (thr.joinable()) thr.join();
  }
}

void PluginThreads::push_back(std::thread &&thr) {
  // if push-back throws it won't inc' 'running_' which is good.
  threads_.push_back(std::move(thr));
  ++running_;
}

void PluginThreads::try_stopped(std::exception_ptr &first_exc) {
  std::exception_ptr exc;
  while (running_ > 0 && plugin_stopped_events_.try_pop(exc)) {
    --running_;

    if (exc) {
      first_exc = exc;
      return;
    }
  }
}

void PluginThreads::wait_all_stopped(std::exception_ptr &first_exc) {
  // wait until all plugins signaled their return value
  for (; running_ > 0; --running_) {
    auto exc = plugin_stopped_events_.pop();
    if (!first_exc) first_exc = exc;
  }
}

////////////////////////////////////////////////////////////////////////////////
//
// Loader
//
////////////////////////////////////////////////////////////////////////////////

Loader::~Loader() {}

Plugin *Loader::load_from(const std::string &plugin_name,
                          const std::string &library_name) {
  std::string error;
  setup_info();

  // We always load the library (even if it is already loaded) to
  // honor potential dynamic library open/close reference counts. It
  // is up to the platform implementation to ensure that multiple
  // instances of a library can be handled.

  PluginInfo info(plugin_folder_, library_name);  // throws bad_plugin

  info.load_plugin(plugin_name);  // throws bad_plugin

  // Check that ABI version and architecture match
  auto plugin = info.plugin;
  if ((plugin->abi_version & 0xFF00) != (PLUGIN_ABI_VERSION & 0xFF00) ||
      (plugin->abi_version & 0xFF) > (PLUGIN_ABI_VERSION & 0xFF)) {
    ostringstream buffer;
    buffer.setf(std::ios::hex, std::ios::basefield);
    buffer.setf(std::ios::showbase);
    buffer << "Bad ABI version - plugin version: " << plugin->abi_version
           << ", loader version: " << PLUGIN_ABI_VERSION;
    throw bad_plugin(buffer.str());
  }

  // Recursively load dependent modules, we skip NULL entries since
  // the user might have added these by accident (for example, he
  // assumed that the array was NULL-terminated) and they can safely
  // be ignored instead of raising an error.
  for (auto req : make_range(plugin->requires, plugin->requires_length)) {
    if (req != nullptr) {
      // Parse the designator to extract the plugin and constraints.
      Designator designator(req);

      // Load the plugin using the plugin name.
      Plugin *dep_plugin{nullptr};

      try {
        dep_plugin =
            load(designator.plugin);  // throws bad_plugin and bad_section
      } catch (const bad_section &) {
        log_error(
            "Plugin '%s' needs plugin '%s' which is missing in the "
            "configuration",
            plugin_name.c_str(), designator.plugin.c_str());
        throw;
      }

      // Check that the version of the plugin match what the
      // designator expected and raise an exception if they don't
      // match.
      if (!designator.version_good(Version(dep_plugin->plugin_version))) {
        Version version(dep_plugin->plugin_version);
        std::ostringstream buffer;
        buffer << designator.plugin << ": plugin version was " << version
               << ", expected " << designator.constraint;
        throw bad_plugin(buffer.str());
      }
    }
  }

  // If all went well, we register the plugin and return a
  // pointer to it.
  plugins_.emplace(plugin_name, std::move(info));
  log_debug("  plugin '%s' loaded ok", plugin_name.c_str());
  return plugin;
}

Plugin *Loader::load(const std::string &plugin_name, const std::string &key) {
  log_debug("  plugin '%s:%s' loading", plugin_name.c_str(), key.c_str());

  if (BuiltinPlugins::instance().has(plugin_name)) {
    Plugin *plugin = BuiltinPlugins::instance().get_plugin(plugin_name);
    PluginInfo info(nullptr, plugin);
    plugins_.emplace(plugin_name, std::move(info));
    return plugin;
  } else {
    ConfigSection &plugin =
        config_.get(plugin_name, key);  // throws bad_section
    const std::string &library_name = plugin.get("library");
    return load_from(plugin_name,
                     library_name);  // throws bad_plugin and bad_section
  }
}

Plugin *Loader::load(const std::string &plugin_name) {
  log_debug("  plugin '%s' loading", plugin_name.c_str());

  Config::SectionList plugins = config_.get(plugin_name);  // throws bad_section
  if (plugins.size() > 1) {
    std::ostringstream buffer;
    buffer << "Section name '" << plugin_name
           << "' is ambiguous. Alternatives are:";
    for (const ConfigSection *plugin : plugins) buffer << " " << plugin->key;
    throw bad_section(buffer.str());
  } else if (plugins.size() == 0) {
    std::ostringstream buffer;
    buffer << "Section name '" << plugin_name << "' does not exist";
    throw bad_section(buffer.str());
  }

  assert(plugins.size() == 1);
  const ConfigSection *section = plugins.front();
  const std::string &library_name = section->get("library");
  return load_from(plugin_name, library_name);  // throws bad_plugin
}

void Loader::start() {
  // unload plugins on exit
  std::shared_ptr<void> exit_guard(nullptr, [this](void *) { unload_all(); });

  // check if there is anything to load; if not we currently treat is as an
  // error, not letting the user to run "idle" router that would close right
  // away
  if (external_plugins_to_load_count() == 0) {
    throw std::runtime_error(
        "Error: MySQL Router not configured to load or start any plugin. "
        "Exiting.");
  }

  // load plugins
  load_all();  // throws bad_plugin on load error, causing an early return

  // init and run plugins
  std::exception_ptr first_eptr = run();
  if (first_eptr) {
    std::rethrow_exception(first_eptr);
  }
}

size_t Loader::external_plugins_to_load_count() {
  size_t result = 0;
  for (std::pair<const std::string &, std::string> name : available()) {
    if (!BuiltinPlugins::instance().has(name.first)) {
      result++;
    }
  }

  return result;
}

void Loader::load_all() {
  log_debug("Loading all plugins.");

  platform_specific_init();
  for (std::pair<const std::string &, std::string> name : available()) {
    try {
      load(name.first, name.second);
    } catch (const bad_plugin &e) {
      log_error("  plugin '%s' failed to load: %s", name.first.c_str(),
                e.what());
      throw;
    }
  }
}

void Loader::unload_all() {
  // this stage has no implementation so far; however, we want to flag that we
  // reached this stage
  log_debug("Unloading all plugins.");
  // If that ever gets implemented make sure to not attempt unloading
  // built-in plugins
}

/**
 * If a isn't set, return b.
 *
 * like ?:, but ensures that b is _always_ evaluated first.
 */
template <class T>
T value_or(T a, T b) {
  return a ? a : b;
}

class LogReopenThread {
 public:
  /**
   * @throws std::system_error if out of threads
   */
  LogReopenThread() : reopen_thr_{log_reopen_thread_function} {}

  /**
   * stop the log_reopen_thread_function.
   *
   * @throws std::system_error from request_application_shutdown()
   */
  void stop() { request_application_shutdown(); }

  /**
   * join the log_reopen thread.
   *
   * @throws std::system_error same as std::thread::join
   */
  void join() { reopen_thr_.join(); }

  /**
   * destruct the thread.
   *
   * Same as std::thread it may call std::terminate in case the thread isn't
   * joined yet, but joinable.
   *
   * In casing join() fails as best-effort, a log-message is attempted to be
   * written.
   */
  ~LogReopenThread() {
    // if it didn't throw in the constructor, it is joinable and we have to
    // signal its shutdown
    if (reopen_thr_.joinable()) {
      try {
        // if stop throws ... the join will block
        stop();

        // if join throws, log it and expect std::thread::~thread to call
        // std::terminate
        join();
      } catch (const std::exception &e) {
        try {
          log_error("~LogReopenThread failed to join its thread: %s", e.what());
        } catch (...) {
          // ignore it, we did our best to tell the user why std::terminate will
          // be called in a bit
        }
      }
    }
  }

 private:
  std::thread reopen_thr_;
};

std::exception_ptr Loader::run() {
  // initialize plugins
  std::exception_ptr first_eptr = init_all();

  // run plugins if initialization didn't fail
  if (!first_eptr) {
    try {
      start_all();  // if start() throws, exception is forwarded to
                    // main_loop()

      // may throw std::system_error
      LogReopenThread log_reopen_thread;

      first_eptr = main_loop();
    } catch (const std::exception &e) {
      log_error("failed running start/main: %s", e.what());
      first_eptr = stop_and_wait_all();
    }
  }

  // not strict requiremnt, just good measure (they're no longer needed at
  // this point)
  assert(plugin_start_env_.empty());

  // deinitialize plugins
  first_eptr = value_or(first_eptr, deinit_all());

  // return the first exception that was triggered by an error returned from
  // any plugin function
  return first_eptr;
}

std::list<Config::SectionKey> Loader::available() const {
  return config_.section_names();
}

void Loader::setup_info() {
  logging_folder_ = config_.get_default("logging_folder");
  plugin_folder_ = config_.get_default("plugin_folder");
  runtime_folder_ = config_.get_default("runtime_folder");
  config_folder_ = config_.get_default("config_folder");
  data_folder_ = config_.get_default("data_folder");

  appinfo_.logging_folder = logging_folder_.c_str();
  appinfo_.plugin_folder = plugin_folder_.c_str();
  appinfo_.runtime_folder = runtime_folder_.c_str();
  appinfo_.config_folder = config_folder_.c_str();
  appinfo_.data_folder = data_folder_.c_str();
  appinfo_.config = &config_;
  appinfo_.program = program_.c_str();
}

static void call_plugin_function(PluginFuncEnv *env, std::exception_ptr &eptr,
                                 void (*fptr)(PluginFuncEnv *),
                                 const char *fnc_name, const char *plugin_name,
                                 const char *plugin_key = nullptr) noexcept {
  auto handle_plugin_exception =
      [](std::exception_ptr & first_eptr, const std::string &func_name,
         const char *plug_name, const char *plug_key,
         const std::exception *e) noexcept->void {
    // Plugins are not allowed to throw, so let's alert the devs. But in
    // production, we want to be robust and try to handle this gracefully
    assert(0);

    if (!first_eptr) first_eptr = std::current_exception();

    std::string what = e ? (std::string(": ") + e->what()) : ".";
    if (plug_key)
      log_error(
          "  plugin '%s:%s' %s threw unexpected exception "
          "- please contact plugin developers for more information%s",
          plug_name, plug_key, func_name.c_str(), what.c_str());
    else
      log_error(
          "  plugin '%s' %s threw unexpected exception "
          "- please contact plugin developers for more information%s",
          plug_name, func_name.c_str(), what.c_str());
  };

  // This try/catch block is about defensive programming - plugins are not
  // allowed to throw. But if the exception is caught anyway, we have to
  // handle it somehow. In debug builds, we throw an assertion. In release
  // builds, we whine about it in logs, but otherwise handle it like a
  // normal error. This behavior is officially undefined, thus we are free
  // to change this at our discretion.
  try {
    // call the plugin
    fptr(env);

    // error handling
    if (env->exit_ok()) {
      if (plugin_key)
        log_debug("  plugin '%s:%s' %s exit ok", plugin_name, plugin_key,
                  fnc_name);
      else
        log_debug("  plugin '%s' %s exit ok", plugin_name, fnc_name);
    } else {
      std::string message;
      if (!eptr) {
        std::tie(message, eptr) = env->pop_error();
      } else {
        std::tie(message, std::ignore) = env->pop_error();
      }
      if (plugin_key)
        log_error("  plugin '%s:%s' %s failed: %s", plugin_name, plugin_key,
                  fnc_name, message.c_str());
      else
        log_error("  plugin '%s' %s failed: %s", plugin_name, fnc_name,
                  message.c_str());
    }

  } catch (const std::exception &e) {
    handle_plugin_exception(eptr, fnc_name, plugin_name, plugin_key, &e);
  } catch (...) {
    handle_plugin_exception(eptr, fnc_name, plugin_name, plugin_key, nullptr);
  }
}

// returns first exception triggered by init()
std::exception_ptr Loader::init_all() {
  log_debug("Initializing all plugins.");

  if (!topsort()) throw std::logic_error("Circular dependencies in plugins");
  order_.reverse();  // we need reverse-topo order for non-built-in plugins

  // we put the built-in plugins at the beginning
  for (const std::pair<const std::string, PluginInfo> &plugin : plugins_) {
    if (BuiltinPlugins::instance().has(plugin.first)) {
      order_.push_front(plugin.first);
    }
  }

  for (auto it = order_.begin(); it != order_.end(); ++it) {
    const std::string &plugin_name = *it;
    PluginInfo &info = plugins_.at(plugin_name);

    if (!info.plugin->init) {
      log_debug("  plugin '%s' doesn't implement init()", plugin_name.c_str());
      continue;
    }

    log_debug("  plugin '%s' initializing", plugin_name.c_str());
    PluginFuncEnv env(&appinfo_, nullptr);

    std::exception_ptr eptr;
    call_plugin_function(&env, eptr, info.plugin->init, "init",
                         plugin_name.c_str());
    if (eptr) {
      // erase this and all remaining plugins from the list, so that
      // deinit_all() will not try to run deinit() on them
      order_.erase(it, order_.end());
      return eptr;
    }

  }  // for (auto it = order_.begin(); it != order_.end(); ++it)

  return nullptr;
}

// forwards first exception triggered by start() to main_loop()
void Loader::start_all() {
  log_debug("Starting all plugins.");

  // block non-fatal signal handling for all threads
  //
  // - no other thread than the signal-handler thread should receive signals
  // - syscalls should not get interrupted by signals either
  //
  // on windows, this is a no-op
  block_all_nonfatal_signals();

  // for the fatal signals we want to have a handler that prints the stack-trace
  // if possible
  register_fatal_signal_handler();

  try {
    // start all the plugins (call plugin's start() function)
    for (const ConfigSection *section : config_.sections()) {
      PluginInfo &plugin = plugins_.at(section->name);
      void (*fptr)(PluginFuncEnv *) = plugin.plugin->start;

      if (!fptr) {
        log_debug("  plugin '%s:%s' doesn't implement start()",
                  section->name.c_str(), section->key.c_str());

        // create a env object for later
        assert(plugin_start_env_.count(section) == 0);
        plugin_start_env_[section] =
            std::make_shared<PluginFuncEnv>(nullptr, section, false);

        continue;
      }

      // future will remain valid even after promise is destructed
      std::promise<std::shared_ptr<PluginFuncEnv>> env_promise;

      // plugin start() will run in this new thread
      std::thread plugin_thread([fptr, section, &env_promise, this]() {
        log_debug("  plugin '%s:%s' starting", section->name.c_str(),
                  section->key.c_str());

        // init env object and unblock harness thread
        std::shared_ptr<PluginFuncEnv> this_thread_env =
            std::make_shared<PluginFuncEnv>(nullptr, section, true);
        env_promise.set_value(this_thread_env);  // shared_ptr gets copied here
                                                 // (future will own a copy)

        std::exception_ptr eptr;
        call_plugin_function(this_thread_env.get(), eptr, fptr, "start",
                             section->name.c_str(), section->key.c_str());

        {
          std::lock_guard<std::mutex> lock(we_might_shutdown_cond_mutex);
          plugin_threads_.push_exit_status(std::move(eptr));
        }
        we_might_shutdown_cond.notify_one();
      });

      // we could combine the thread creation with emplace_back
      // but that sometimes leads to a crash on ASAN build (when the thread
      // limit is reached apparently sometimes half-baked thread object gets
      // added to the vector and its destructor crashes later on when the vector
      // gets destroyed)
      plugin_threads_.push_back(std::move(plugin_thread));

      // block until starter thread is started
      // then save the env object for later
      assert(plugin_start_env_.count(section) == 0);
      plugin_start_env_[section] =
          env_promise.get_future()
              .get();  // returns shared_ptr to PluginFuncEnv;
                       // PluginFuncEnv exists on heap

    }  // for (const ConfigSection* section: config_.sections())
  } catch (const std::system_error &e) {
    throw std::system_error(e.code(), "starting plugin-threads failed");
  }

  try {
    // We wait with this until after we launch all plugin threads, to avoid
    // a potential race if a signal was received while plugins were still
    // launching.
    start_and_detach_signal_handler_thread();
  } catch (const std::system_error &e) {
    // should we unblock the signals again?
    throw std::system_error(e.code(), "starting signal-handler-thread failed");
  }
}

/**
 * wait for shutdown signal or plugins exit.
 *
 * blocks until one of the following happens:
 *
 * - shutdown signal is received
 * - one plugin return an exception
 * - all plugins finished
 *
 * @returns first exception returned by any of the plugins start() or stop()
 * functions
 * @retval nullptr if no exception was returned
 */
std::exception_ptr Loader::main_loop() {
  // RouterRoutingTest::RoutingPluginCantSpawnMoreThreads is waiting for this
  // log-message to appear in the log to get a predictible test-scenario.
  //
  // Changing or moving this message, will break that test.
  log_debug("Running.");

  std::exception_ptr first_eptr;
  // wait for a reason to shutdown
  {
    std::unique_lock<std::mutex> lk(we_might_shutdown_cond_mutex);

    we_might_shutdown_cond.wait(lk, [&first_eptr, this] {
      // external shutdown
      if (g_shutdown_pending == SHUTDOWN_REQUESTED) return true;

      // shutdown due to a fatal error originating from Loader and its callees
      // (but NOT from plugins)
      if (g_shutdown_pending == SHUTDOWN_FATAL_ERROR) {
        // there is a request to shut down due to a fatal error; generate an
        // exception with requested message so that it bubbles up and ends up on
        // the console as an error message
        try {
          throw std::runtime_error(shutdown_fatal_error_message);
        } catch (const std::exception &) {
          first_eptr = std::current_exception();  // capture
        }
        return true;
      }

      plugin_threads_.try_stopped(first_eptr);
      if (first_eptr) return true;

      // all plugins stop successfully
      if (plugin_threads_.running() == 0) return true;

      return false;
    });
  }

  return value_or(first_eptr, stop_and_wait_all());
}

std::exception_ptr Loader::stop_and_wait_all() {
  std::exception_ptr first_eptr;

  // stop all plugins
  first_eptr = value_or(first_eptr, stop_all());

  plugin_threads_.wait_all_stopped(first_eptr);
  try {
    plugin_threads_.join();
  } catch (...) {
    // may throw due to deadlocks and other system-related reasons.
    if (!first_eptr) {
      first_eptr = std::current_exception();
    }
  }

  // we will no longer need the env objects for start(), might as well
  // clean them up now for good measure
  plugin_start_env_.clear();

  // We just return the first exception that was raised (if any). If there
  // are other exceptions, they are ignored.
  return first_eptr;
}

// returns first exception triggered by stop()
std::exception_ptr Loader::stop_all() {
  // This function runs exactly once - it will be called even if all plugins
  // exit by themselves (thus there's nothing to stop).
  log_debug("Shutting down. Stopping all plugins.");

  // iterate over all plugin instances
  std::exception_ptr first_eptr;
  for (const ConfigSection *section : config_.sections()) {
    PluginInfo &plugin = plugins_.at(section->name);
    void (*fptr)(PluginFuncEnv *) = plugin.plugin->stop;

    assert(plugin_start_env_.count(section));
    assert(plugin_start_env_[section]->get_config_section() == section);

    // flag plugin::start() to exit (if one exists and it's running)
    plugin_start_env_[section]->clear_running();

    if (!fptr) {
      log_debug("  plugin '%s:%s' doesn't implement stop()",
                section->name.c_str(), section->key.c_str());
      continue;
    }

    log_debug("  plugin '%s:%s' stopping", section->name.c_str(),
              section->key.c_str());

    PluginFuncEnv stop_env(nullptr, section);
    call_plugin_function(&stop_env, first_eptr, fptr, "stop",
                         section->name.c_str(), section->key.c_str());

  }  // for (const ConfigSection* section: config_.sections())

  return first_eptr;
}

// returns first exception triggered by deinit()
std::exception_ptr Loader::deinit_all() {
  log_debug("Deinitializing all plugins.");

  // we could just reverse order_ and that would work too,
  // but by leaving it intact it's easier to unit-test it
  std::list<std::string> deinit_order = order_;
  deinit_order.reverse();

  // call deinit() on all plugins that support the call
  std::exception_ptr first_eptr;
  for (const std::string &plugin_name : deinit_order) {
    const PluginInfo &info = plugins_.at(plugin_name);

    if (!info.plugin->deinit) {
      log_debug("  plugin '%s' doesn't implement deinit()",
                plugin_name.c_str());
      continue;
    }

    log_debug("  plugin '%s' deinitializing", plugin_name.c_str());
    PluginFuncEnv env(&appinfo_, nullptr);

    call_plugin_function(&env, first_eptr, info.plugin->deinit, "deinit",
                         plugin_name.c_str());
  }

  return first_eptr;
}

bool Loader::topsort() {
  std::map<std::string, Loader::Status> status;
  std::list<std::string> order;

  // for the non-builtin plugins do the sorting that takes their dependencies
  // into account
  for (std::pair<const std::string, PluginInfo> &plugin : plugins_) {
    if (!BuiltinPlugins::instance().has(plugin.first)) {
      bool succeeded = visit(plugin.first, &status, &order);
      if (!succeeded) return false;
    }
  }

  order_.swap(order);
  return true;
}

bool Loader::visit(const std::string &designator,
                   std::map<std::string, Loader::Status> *status,
                   std::list<std::string> *order) {
  Designator info(designator);
  switch ((*status)[info.plugin]) {
    case Status::VISITED:
      return true;

    case Status::ONGOING:
      // If we see a node we are processing, it's not a DAG and cannot
      // be topologically sorted.
      return false;

    case Status::UNVISITED: {
      (*status)[info.plugin] = Status::ONGOING;
      if (Plugin *plugin = plugins_.at(info.plugin).plugin) {
        for (auto required :
             make_range(plugin->requires, plugin->requires_length)) {
          assert(required != NULL);
          bool succeeded = visit(required, status, order);
          if (!succeeded) return false;
        }
      }
      (*status)[info.plugin] = Status::VISITED;
      order->push_front(info.plugin);
      return true;
    }
  }
  return true;
}

}  // namespace mysql_harness

// unit test access - DON'T USE IN PRODUCTION CODE!
// (unfortunately we cannot guard this with #ifdef FRIEND_TEST)
namespace unittest_backdoor {
HARNESS_EXPORT
void set_shutdown_pending(bool shutdown_pending) {
  g_shutdown_pending = shutdown_pending ? SHUTDOWN_REQUESTED : SHUTDOWN_NONE;
}
}  // namespace unittest_backdoor
