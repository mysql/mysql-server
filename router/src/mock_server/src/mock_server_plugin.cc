/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifdef _WIN32
#include <direct.h>  // getcwd
#else
#include <unistd.h>  // getcwd
#endif

#include "mock_server_plugin.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin.h"
#include "mysql_server_mock.h"
#include "mysqlrouter/plugin_config.h"

IMPORT_LOG_FUNCTIONS()

#ifndef PATH_MAX
#ifdef _MAX_PATH
// windows has _MAX_PATH instead
#define PATH_MAX _MAX_PATH
#endif
#endif

static constexpr const char kSectionName[]{"mock_server"};

class PluginConfig : public mysqlrouter::BasePluginConfig {
 public:
  std::string trace_filename;
  std::string module_prefix;
  std::string srv_address;
  uint16_t srv_port;

  explicit PluginConfig(const mysql_harness::ConfigSection *section)
      : mysqlrouter::BasePluginConfig(section),
        trace_filename(get_option_string(section, "filename")),
        module_prefix(get_option_string(section, "module_prefix")),
        srv_address(get_option_string(section, "bind_address")),
        srv_port(get_uint_option<uint16_t>(section, "port")) {}

  std::string get_default(const std::string &option) const override {
    char cwd[PATH_MAX];

    if (nullptr == getcwd(cwd, sizeof(cwd))) {
      throw std::system_error(errno, std::generic_category());
    }

    const std::map<std::string, std::string> defaults{
        {"bind_address", "0.0.0.0"},
        {"module_prefix", cwd},
        {"port", "3306"},
    };

    auto it = defaults.find(option);
    if (it == defaults.end()) {
      return std::string();
    }
    return it->second;
  }

  bool is_required(const std::string &option) const override {
    if (option == "filename") return true;
    return false;
  }
};

static std::map<std::string, std::shared_ptr<server_mock::MySQLServerMock>>
    mock_servers;

static void init(mysql_harness::PluginFuncEnv *env) {
  const mysql_harness::AppInfo *info = get_app_info(env);
  bool has_started = false;

  try {
    if (info->config != nullptr) {
      for (const mysql_harness::ConfigSection *section :
           info->config->sections()) {
        if (section->name != kSectionName) {
          continue;
        }
        if (has_started) {
          // ignore all the other sections for now
          break;
        }

        has_started = true;

        PluginConfig config{section};
        mock_servers.emplace(std::make_pair(
            section->name, std::make_shared<server_mock::MySQLServerMock>(
                               config.trace_filename, config.module_prefix,
                               config.srv_port, 0)));

        MockServerComponent::getInstance().init(mock_servers.at(section->name));
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

static void start(mysql_harness::PluginFuncEnv *env) {
  const mysql_harness::ConfigSection *section = get_config_section(env);

  std::string name;
  if (!section->key.empty()) {
    name = section->name + ":" + section->key;
  } else {
    name = section->name;
  }

  try {
    auto srv = mock_servers.at(get_config_section(env)->name);

    srv->run(env);
  } catch (const std::invalid_argument &exc) {
    set_error(env, mysql_harness::kConfigInvalidArgument, "%s", exc.what());
  } catch (const std::runtime_error &exc) {
    set_error(env, mysql_harness::kRuntimeError, "%s: %s", name.c_str(),
              exc.what());
  } catch (const std::exception &exc) {
    set_error(env, mysql_harness::kUndefinedError, "%s: %s", name.c_str(),
              exc.what());
  } catch (...) {
    set_error(env, mysql_harness::kUndefinedError, "Unexpected exception");
  }
}

extern "C" {
mysql_harness::Plugin MOCK_SERVER_EXPORT harness_plugin_mock_server = {
    mysql_harness::PLUGIN_ABI_VERSION,
    mysql_harness::ARCHITECTURE_DESCRIPTOR,
    "Routing MySQL connections between MySQL clients/connectors and servers",
    VERSION_NUMBER(0, 0, 1),
    0,
    nullptr,  // requires
    0,
    nullptr,  // Conflicts
    init,     // init
    nullptr,  // deinit
    start,    // start
    nullptr   // stop
};
}
