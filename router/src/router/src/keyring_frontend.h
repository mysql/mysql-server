/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_KEYRING_FRONTEND_INCLUDED
#define ROUTER_KEYRING_FRONTEND_INCLUDED

#include <iostream>  // cerr
#include <istream>
#include <string>
#include <vector>

#include "mysql/harness/arg_handler.h"

/**
 * exception thrown by the frontend.
 *
 * Should be presented to the user.
 */
class FrontendError : public std::runtime_error {
 public:
  FrontendError(const std::string &what) : std::runtime_error(what) {}
};

/**
 * frontend error that involved the command-line options.
 *
 * should be handled by showing the user the help-text or a hint how to get the
 * help
 */
class UsageError : public FrontendError {
 public:
  UsageError(const std::string &what) : FrontendError(what) {}
};

/**
 * passwd file management frontend.
 */
class KeyringFrontend {
 public:
  enum class Cmd {
    Init,
    Set,
    Get,
    Delete,
    List,
    Export,
    MasterDelete,
    MasterList,
    MasterRename,
    ShowHelp,
    ShowVersion
  };
  struct Config {
    Cmd cmd{Cmd::Set};
    std::string keyring_filename;
    std::string master_keyring_filename;
    std::string master_key_reader;
    std::string master_key_writer;
    std::string username;
    std::string field;
    std::string value;
  };

  KeyringFrontend(const std::string &exe_name,
                  const std::vector<std::string> &args,
                  std::istream &is = std::cin, std::ostream &os = std::cout,
                  std::ostream &es = std::cerr);

  /**
   * run frontend according to configuration.
   *
   * @return exit-status
   * @retval EXIT_FAILURE on error
   * @retval EXIT_SUCCESS on success
   */
  int run();

  /**
   * get help text.
   *
   * @param screen_width wraps text at screen-width
   * @returns help text
   */
  std::string get_help(const size_t screen_width = 80) const;

  /**
   * get version text.
   */
  static std::string get_version() noexcept;

 private:
  void init_from_arguments(const std::vector<std::string> &arguments);

  void prepare_command_options();
  void prepare_args();
  std::string read_password();

  std::string program_name_;
  CmdArgHandler arg_handler_{true};  // allow arguments
  std::istream &cin_;
  std::ostream &cout_;
  std::ostream &cerr_;

  Config config_;  // must be last as init_from_arguments depends on cin, ...
                   // and arg_handler_
};
#endif
