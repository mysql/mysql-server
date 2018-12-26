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

#include "dim.h"
#include "metadata_cache.h"
#include "mysql/harness/loader_config.h"
#include "mysqlrouter/mysql_session.h"  // kSslModePreferred
#include "plugin_config.h"

#include <string>
#include <thread>
#ifndef _WIN32
#include <termios.h>
#include <unistd.h>
#endif

#include "keyring/keyring_manager.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/utils.h"
#include "tcp_address.h"

using metadata_cache::LookupResult;
using mysql_harness::TCPAddress;
using std::string;
IMPORT_LOG_FUNCTIONS()

static const mysql_harness::AppInfo *g_app_info;
static const string kSectionName = "metadata_cache";
static const char *kKeyringAttributePassword = "password";

static void init(mysql_harness::PluginFuncEnv *env) {
  g_app_info = get_app_info(env);
  // If a valid configuration object was found.
  if (g_app_info && g_app_info->config) {
    // if a valid metadata_cache section was found in the router
    // configuration.
    if (g_app_info->config->get(kSectionName).empty()) {
      log_error("[metadata_cache] section is empty");  // TODO remove after
                                                       // Loader starts logging
      set_error(env, mysql_harness::kConfigInvalidArgument,
                "[metadata_cache] section is empty");
    }
  }
}

static std::string get_option(const mysql_harness::ConfigSection *section,
                              const std::string &key,
                              const std::string &def_value) {
  if (section->has(key)) return section->get(key);
  return def_value;
}

static mysqlrouter::SSLOptions make_ssl_options(
    const mysql_harness::ConfigSection *section) {
  mysqlrouter::SSLOptions options;

  options.mode = get_option(section, "ssl_mode",
                            mysqlrouter::MySQLSession::kSslModePreferred);
  options.cipher = get_option(section, "ssl_cipher", "");
  options.tls_version = get_option(section, "tls_version", "");
  options.ca = get_option(section, "ssl_ca", "");
  options.capath = get_option(section, "ssl_capath", "");
  options.crl = get_option(section, "ssl_crl", "");
  options.crlpath = get_option(section, "ssl_crlpath", "");

  return options;
}

/**
 * Initialize the metadata cache for fetching the information from the
 * metadata servers.
 *
 * @param env plugin's environment
 */
static void start(mysql_harness::PluginFuncEnv *env) {
  const mysql_harness::ConfigSection *section = get_config_section(env);

  // launch metadata cache
  try {
    MetadataCachePluginConfig config(section);
    std::chrono::milliseconds ttl{config.ttl};
    string metadata_cluster{config.metadata_cluster};

    // Initialize the defaults.
    metadata_cluster = metadata_cluster.empty()
                           ? metadata_cache::kDefaultMetadataCluster
                           : metadata_cluster;

    std::string password;
    try {
      password = mysql_harness::get_keyring()
                     ? mysql_harness::get_keyring()->fetch(
                           config.user, kKeyringAttributePassword)
                     : "";
    } catch (const std::out_of_range &) {
      std::string msg = "Could not find the password for user '" + config.user +
                        "' in the keyring. "
                        "metadata_cache not initialized properly.";
      throw std::runtime_error(msg);
    }

    log_info("Starting Metadata Cache");
    // Initialize the metadata cache.
    metadata_cache::MetadataCacheAPI::instance()->cache_init(
        config.bootstrap_addresses, config.user, password, ttl,
        make_ssl_options(section), metadata_cluster, config.connect_timeout,
        config.read_timeout, config.thread_stack_size);
  } catch (const std::runtime_error &exc) {  // metadata_cache::metadata_error
                                             // inherits from runtime_error
    log_error("%s", exc.what());  // TODO remove after Loader starts logging
    set_error(env, mysql_harness::kRuntimeError, "%s", exc.what());
    clear_running(env);
  } catch (const std::invalid_argument &exc) {
    log_error("%s", exc.what());  // TODO remove after Loader starts logging
    set_error(env, mysql_harness::kConfigInvalidArgument, "%s", exc.what());
    clear_running(env);
  } catch (...) {
    log_error(
        "Unexpected exception");  // TODO remove after Loader starts logging
    set_error(env, mysql_harness::kUndefinedError, "Unexpected exception");
    clear_running(env);
  }

  // keep it running until Harness tells us to shut down
  wait_for_stop(env, 0);
  metadata_cache::MetadataCacheAPI::instance()->cache_stop();
}

extern "C" {

mysql_harness::Plugin METADATA_API harness_plugin_metadata_cache = {
    mysql_harness::PLUGIN_ABI_VERSION,
    mysql_harness::ARCHITECTURE_DESCRIPTOR,
    "Metadata Cache, managing information fetched from the Metadata Server",
    VERSION_NUMBER(0, 0, 1),
    0,
    NULL,  // Requires
    0,
    NULL,  // Conflicts
    init,
    NULL,
    start,  // start
    NULL    // stop
};
}
