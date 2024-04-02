/*
  Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_MYSQL_ROUTER_CONF_INCLUDED
#define ROUTER_MYSQL_ROUTER_CONF_INCLUDED

#include "mysqlrouter/router_export.h"

/** @file
 * @brief Defining the main class MySQLRouter
 *
 * This file defines the main class `MySQLRouter`.
 *
 */

#include "config_generator.h"
#include "dim.h"
#include "mysql/harness/arg_handler.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/signal_handler.h"
#include "mysqlrouter/keyring_info.h"
#include "mysqlrouter/sys_user_operations.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

class MySQLRouterConf {
 public:
  MySQLRouterConf(KeyringInfo &keyring_info,
                  std::ostream &out_stream = std::cout,
                  std::ostream &err_stream = std::cerr)
      : keyring_info_(keyring_info),
        out_stream_(out_stream),
        err_stream_(err_stream) {}

  virtual ~MySQLRouterConf() = default;

  virtual void prepare_command_options(
      CmdArgHandler &arg_handler,
      const std::string &bootstrap_uri = "") noexcept;

  bool is_bootstrap() const { return !bootstrap_uri_.empty(); }
  virtual bool is_legacy() const { return true; }

  bool skipped() const { return skipped_; }

  void add_option(const std::string &key, const std::string &value) {
    bootstrap_options_[key] = value;
  }

  const std::map<std::string, std::string> &bootstrap_options() const {
    return bootstrap_options_;
  }

 public:
  void connect();

  mysqlrouter::MySQLSession *session() const { return mysql_.get(); }

  std::string bootstrap(
      const std::string &program_name, const mysql_harness::Path &origin,
      bool allow_standalone, const std::string &plugin_folder
#ifndef _WIN32
      ,
      mysqlrouter::SysUserOperationsBase *sys_user_operations = nullptr
#endif
  );

 protected:
  friend class MySQLRouter;

#ifdef FRIEND_TEST
  FRIEND_TEST(ConfigGeneratorTest, ssl_stage1_cmdline_arg_parse);
  FRIEND_TEST(ConfigGeneratorTest, ssl_stage2_bootstrap_connection);
  FRIEND_TEST(ConfigGeneratorTest, ssl_stage3_create_config);
#endif

  /**
   * @brief Value of the argument passed to the -B or --bootstrap
   *        command line option for bootstrapping.
   */
  std::string bootstrap_uri_;
  /**
   * @brief Valueof the argument passed to the --directory command line option
   */
  std::string bootstrap_directory_;
  /**
   * @brief key/value map of additional configuration options for bootstrap
   */
  std::map<std::string, std::string> bootstrap_options_;

  /**
   * @brief key/list-of-values map of additional configuration options for
   * bootstrap
   */
  std::map<std::string, std::vector<std::string>> bootstrap_multivalue_options_;

  mysqlrouter::URI target_uri_;
  std::unique_ptr<mysqlrouter::MySQLSession> mysql_;

  KeyringInfo &keyring_info_;

  bool skipped_ = false;

  std::ostream &out_stream_;
  std::ostream &err_stream_;

  /** @brief Saves the selected command line option in the internal options
   * array after verifying it's value not empty and the router is doing
   * bootstrap.
   *
   *  Throws: std::runtime_error
   */
  void save_bootstrap_option_not_empty(const std::string &option_name,
                                       const std::string &save_name,
                                       const std::string &option_value);

  /**
   * @brief verify that bootstrap option (--bootstrap or -B) was given by user.
   *
   * @throw std::runtime_error if called in non-bootstrap mode.
   */
  void assert_bootstrap_mode(const std::string &option_name) const;

  int get_connect_timeout() const;
  int get_read_timeout() const;
  std::string get_bootstrap_socket() const;
};

class silent_exception : public std::exception {
 public:
  silent_exception() : std::exception() {}
};

#endif  // ROUTER_MYSQL_ROUTER_CONF_INCLUDED
