/*
  Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#define MYSQL_ROUTER_LOG_DOMAIN "io"

/**
 * io plugin.
 *
 * manages the configuration of the io-threads and the io-backends.
 */
#include "io_plugin.h"

#include <algorithm>  // max
#include <array>
#include <map>
#include <memory>     // unique_ptr
#include <stdexcept>  // runtime_error
#include <string>
#include <thread>

// Harness interface include files
#include "my_thread.h"  // my_thread_self_setname
#include "mysql/harness/config_option.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/plugin.h"
#include "mysql/harness/plugin_config.h"
#include "mysql/harness/utility/string.h"  // join
#include "mysqlrouter/io_component.h"
#include "mysqlrouter/io_export.h"
#include "mysqlrouter/io_thread.h"

IMPORT_LOG_FUNCTIONS()

static constexpr const char kSectionName[]{"io"};

// max io-threads the user can spawn.
//
// The limit is in place to protect the user from creating more threads
// the system can handle in a reasonable way without running out of memory.
//
// It is assumed that 1-thread-per-CPU is optimal, and that currently
// (2020) the max cpu-threads per system is 256:
//
// - EPYC 7702: 64 cores/128 threads, 2x sockets
static constexpr const size_t kMaxThreads{1024};

using StringOption = mysql_harness::StringOption;
template <class T>
using IntOption = mysql_harness::IntOption<T>;

static constexpr std::array<const char *, 2> supported_options{"backend",
                                                               "threads"};

#define GET_OPTION_CHECKED(option, section, name, value)                    \
  static_assert(mysql_harness::str_in_collection(supported_options, name)); \
  option = get_option(section, name, value);

class IoPluginConfig : public mysql_harness::BasePluginConfig {
 public:
  std::string backend;
  uint16_t num_threads;

  explicit IoPluginConfig(const mysql_harness::ConfigSection *section)
      : mysql_harness::BasePluginConfig(section) {
    GET_OPTION_CHECKED(backend, section, "backend", StringOption{});
    auto num_threads_op = IntOption<uint32_t>{0, kMaxThreads};
    GET_OPTION_CHECKED(num_threads, section, "threads", num_threads_op);
  }

  std::string get_default(const std::string &option) const override {
    const std::map<std::string, std::string> defaults{
        {"backend", IoBackend::preferred()},
        {"threads", "0"},
    };

    auto it = defaults.find(option);
    if (it == defaults.end()) {
      return std::string();
    }
    return it->second;
  }

  bool is_required(const std::string & /* option */) const override {
    return false;
  }
};

static void init(mysql_harness::PluginFuncEnv *env) {
  const mysql_harness::AppInfo *info = get_app_info(env);

  if (info == nullptr || nullptr == info->config) {
    return;
  }

  // assume there is only one section for us
  try {
    bool section_found{false};

    std::string backend_name = IoBackend::preferred();
    size_t num_threads{};

    for (const mysql_harness::ConfigSection *section :
         info->config->sections()) {
      if (section->name != kSectionName) {
        continue;
      }

      if (section_found) {
        set_error(env, mysql_harness::kConfigInvalidArgument,
                  "[%s] found another config-section '%s', only one allowed",
                  kSectionName, section->key.c_str());
        return;
      }

      if (!section->key.empty()) {
        set_error(env, mysql_harness::kConfigInvalidArgument,
                  "[%s] section does not expect a key, found '%s'",
                  kSectionName, section->key.c_str());
        return;
      }

      IoPluginConfig config{section};
      num_threads = config.num_threads;
      backend_name = config.backend;

      section_found = true;

      break;
    }

    if (num_threads == 0) {
      num_threads = std::max(std::thread::hardware_concurrency(), 1u);
    }

    log_info("starting %zu io-threads, using backend '%s'", num_threads,
             backend_name.c_str());

    const auto init_res =
        IoComponent::get_instance().init(num_threads, backend_name);
    if (!init_res) {
      const auto ec = init_res.error();

      if (ec == make_error_code(IoComponentErrc::unknown_backend)) {
        set_error(env, mysql_harness::kConfigInvalidArgument,
                  "[%s] backend '%s' is not known. Known backends are: %s",
                  kSectionName, backend_name.c_str(),
                  mysql_harness::join(IoBackend::supported(), ", ").c_str());
      } else if (ec == make_error_condition(
                           std::errc::resource_unavailable_try_again)) {
        // libc++ returns system:35
        // libstdc++ returns generic:35
        // aka, make_error_condition() needs to be used.
        set_error(env, mysql_harness::kConfigInvalidArgument,
                  "[%s] failed to spawn %zu threads", kSectionName,
                  num_threads);
      } else {
        set_error(env, mysql_harness::kConfigInvalidArgument, "%s",
                  ec.message().c_str());
      }
    }
  } catch (const std::invalid_argument &exc) {
    set_error(env, mysql_harness::kConfigInvalidArgument, "%s", exc.what());
  } catch (const std::exception &exc) {
    set_error(env, mysql_harness::kRuntimeError, "%s", exc.what());
  } catch (...) {
    set_error(env, mysql_harness::kUndefinedError, "Unexpected exception");
  }
}

static void run(mysql_harness::PluginFuncEnv * /* env */) {
  my_thread_self_setname("io_main");
  // run events in the mainloop until the app signals a shutdown
  IoComponent::get_instance().run();
}

static void deinit(mysql_harness::PluginFuncEnv * /* env */) {
  // cleanup before the other plugins are unloaded.
  IoComponent::get_instance().reset();
}

static std::array<const char *, 1> required = {{
    "logger",
}};

extern "C" {
mysql_harness::Plugin IO_EXPORT harness_plugin_io = {
    mysql_harness::PLUGIN_ABI_VERSION,       // abi-version
    mysql_harness::ARCHITECTURE_DESCRIPTOR,  // arch-descriptor
    "IO",
    VERSION_NUMBER(0, 0, 1),
    // requires
    required.size(),
    required.data(),
    // conflicts
    0,
    nullptr,
    init,     // init
    deinit,   // deinit
    run,      // run
    nullptr,  // on_signal_stop
    false,    // signals ready
    supported_options.size(),
    supported_options.data(),
};
}
