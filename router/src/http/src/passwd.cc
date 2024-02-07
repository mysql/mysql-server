/*
  Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#include "passwd.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

#include "mysql/harness/arg_handler.h"
#include "mysql/harness/filesystem.h"  // make_file_private
#include "mysql/harness/utility/string.h"
#include "mysqlrouter/utils.h"         // prompt_password
#include "print_version.h"             // build_version
#include "router_config.h"             // MYSQL_ROUTER_PACKAGE_NAME
#include "welcome_copyright_notice.h"  // ORACLE_WELCOME_COPYRIGHT_NOTICE

#include "http_auth_backend.h"
#include "kdf_pbkdf2.h"
#include "kdf_sha_crypt.h"

static constexpr char kKdfNameSha256Crypt[]{"sha256-crypt"};
static constexpr char kKdfNameSha512Crypt[]{"sha512-crypt"};
static constexpr char kKdfNamePkbdf2Sha256[]{"pbkdf2-sha256"};
static constexpr char kKdfNamePkbdf2Sha512[]{"pbkdf2-sha512"};

// map kdf-name to kdf-id
static std::map<std::string, PasswdFrontend::Kdf> supported_kdfs{
    {kKdfNameSha256Crypt, PasswdFrontend::Kdf::Sha256_crypt},
    {kKdfNameSha512Crypt, PasswdFrontend::Kdf::Sha512_crypt},
    {kKdfNamePkbdf2Sha256, PasswdFrontend::Kdf::Pbkdf2_sha256},
    {kKdfNamePkbdf2Sha512, PasswdFrontend::Kdf::Pbkdf2_sha512}};

void PasswdFrontend::init_from_arguments(
    const std::vector<std::string> &arguments) {
  prepare_command_options();
  try {
    arg_handler_.process(arguments);
  } catch (const std::invalid_argument &e) {
    // unknown options
    throw UsageError(e.what());
  }
}

PasswdFrontend::PasswdFrontend(const std::string &exe_name,
                               const std::vector<std::string> &args,
                               std::ostream &os, std::ostream &es)
    : program_name_{exe_name}, arg_handler_{true}, cout_{os}, cerr_{es} {
  init_from_arguments(args);
}

std::string PasswdFrontend::get_version() noexcept {
  std::stringstream os;

  std::string version_string;
  build_version(std::string(MYSQL_ROUTER_PACKAGE_NAME), &version_string);

  os << version_string << std::endl;
  os << ORACLE_WELCOME_COPYRIGHT_NOTICE("2018") << std::endl;

  return os.str();
}

std::string PasswdFrontend::get_help(const size_t screen_width) const {
  std::stringstream os;

  os << "Usage" << std::endl;
  os << std::endl;

  os << mysql_harness::join(
            mysql_harness::utility::wrap_string(
                program_name_ + " " + "[opts] <cmd> <filename> [<username>]",
                screen_width, 2),
            "\n")
     << std::endl;
  os << mysql_harness::join(
            mysql_harness::utility::wrap_string(program_name_ + " " + "--help",
                                                screen_width, 2),
            "\n")
     << std::endl;
  os << mysql_harness::join(
            mysql_harness::utility::wrap_string(
                program_name_ + " " + "--version", screen_width, 2),
            "\n")
     << std::endl;

  os << std::endl;
  os << "Commands" << std::endl;
  os << std::endl;

  std::vector<std::pair<std::string, std::string>> cmd_help{
      {"delete", "Delete username (if it exists) from <filename>."},
      {"list", "list one or all accounts of <filename>."},
      {"set", "add or overwrite account of <username> in <filename>."},
      {"verify",
       "verify if password matches <username>'s credentials in <filename>."},
  };

  for (const auto &kv : cmd_help) {
    os << "  " << kv.first << std::endl
       << mysql_harness::join(
              mysql_harness::utility::wrap_string(kv.second, screen_width, 6),
              "\n")
       << std::endl;
  }

  os << std::endl;

  os << "Options" << std::endl;

  os << std::endl;

  for (const auto &line : arg_handler_.option_descriptions(screen_width, 6)) {
    os << line << std::endl;
  }

  return os.str();
}

std::string PasswdFrontend::read_password() {
  return mysqlrouter::prompt_password("Please enter password");
}

int PasswdFrontend::run() {
  switch (config_.cmd) {
    case PasswdFrontend::Cmd::ShowHelp:
      cout_ << get_help() << std::endl;
      return EXIT_SUCCESS;
    case PasswdFrontend::Cmd::ShowVersion:
      cout_ << get_version() << std::endl;
      return EXIT_SUCCESS;
    default:
      break;
  }

  // handle positional args
  {
    const auto &rest_args = arg_handler_.get_rest_arguments();
    const auto rest_args_count = rest_args.size();

    if (rest_args_count == 0) {
      throw UsageError("expected a <cmd>");
    }

    std::map<std::string, PasswdFrontend::Cmd> cmds{
        {"set", PasswdFrontend::Cmd::Set},
        {"verify", PasswdFrontend::Cmd::Verify},
        {"delete", PasswdFrontend::Cmd::Delete},
        {"list", PasswdFrontend::Cmd::List}};

    const auto cmd = cmds.find(rest_args[0]);
    if (cmd == cmds.end()) {
      throw UsageError("unknown command: " + rest_args[0]);
    }

    config_.cmd = cmd->second;

    switch (rest_args_count) {
      case 3:
        config_.filename = rest_args[1];
        config_.username = rest_args[2];

        break;
      case 2:
        if (config_.cmd == PasswdFrontend::Cmd::List) {
          config_.filename = rest_args[1];
          config_.username = "";

          break;
        }
        [[fallthrough]];
      default:
        if (config_.cmd == PasswdFrontend::Cmd::List) {
          throw UsageError("expected at least one extra argument: <filename>");
        } else {
          throw UsageError("expected <filename> and <username>");
        }
    }

    const auto valid_chars =
        std::find_if(config_.username.begin(), config_.username.end(),
                     [](char cur) { return cur == ':' || cur == '\n'; });
    if (config_.username.end() != valid_chars) {
      // username must not contain a colon
      throw FrontendError(
          std::string("<username> contained '") + *valid_chars + "' at pos " +
          std::to_string(std::distance(config_.username.begin(), valid_chars)) +
          ", allowed are [a-zA-Z0-9]+");
    }
  }

  HttpAuthBackendHtpasswd backend;
  {
    std::fstream f(config_.filename, std::ios_base::in);
    if (f.is_open()) {
      if (auto ec = backend.from_stream(f)) {
        throw FrontendError("failed to parse file '" + config_.filename +
                            "': " + ec.message());
      }
    } else {
      switch (config_.cmd) {
        case Cmd::Set:
          // set will create a new file if the file doesn't exist
          break;
        default:
          throw FrontendError("can't open file '" + config_.filename + "'");
      }
    }
  }

  bool changed = false;
  switch (config_.cmd) {
    case Cmd::List: {
      if (config_.username.empty()) {
        // dump all
        backend.to_stream(cout_);
      } else {
        auto it = backend.find(config_.username);
        if (it == backend.end()) {
          cerr_ << "user '" << config_.username << "' not found" << std::endl;
          return EXIT_FAILURE;
        }

        // dump the named one
        cout_ << it->first << ":" << it->second << std::endl;
      }
      break;
    }
    case Cmd::Verify: {
      std::string passwd = read_password();

      auto ec = backend.authenticate(config_.username, passwd);

      if (ec) {
        cerr_ << ec.message() << std::endl;
      }

      return !ec ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    case Cmd::Delete: {
      changed = (backend.erase(config_.username) != 0);

      if (!changed) {
        cerr_ << "user '" << config_.username << "' not found" << std::endl;
        return EXIT_FAILURE;
      }
      break;
    }
    case Cmd::Set: {
      changed = true;

      switch (config_.kdf) {
        case PasswdFrontend::Kdf::Sha256_crypt:
        case PasswdFrontend::Kdf::Sha512_crypt: {
          // add user to map
          ShaCryptMcfAdaptor::Type kdf =
              config_.kdf == PasswdFrontend::Kdf::Sha256_crypt
                  ? ShaCryptMcfAdaptor::Type::Sha256
                  : ShaCryptMcfAdaptor::Type::Sha512;

          ShaCryptMcfAdaptor mcf_adaptor(kdf, config_.cost, ShaCrypt::salt(),
                                         "");
          {
            std::string passwd = read_password();
            mcf_adaptor.hash(passwd);
          }
          std::string auth_data = mcf_adaptor.to_mcf();

          backend.set(config_.username, auth_data);
        } break;

        case PasswdFrontend::Kdf::Pbkdf2_sha256:
        case PasswdFrontend::Kdf::Pbkdf2_sha512: {
          Pbkdf2::Type kdf = config_.kdf == PasswdFrontend::Kdf::Pbkdf2_sha256
                                 ? Pbkdf2::Type::Sha_256
                                 : Pbkdf2::Type::Sha_512;

          Pbkdf2McfAdaptor mcf_adaptor(kdf, config_.cost, Pbkdf2::salt(), {});
          {
            std::string passwd = read_password();
            mcf_adaptor.derive(passwd);
          }
          std::string auth_data = mcf_adaptor.to_mcf();

          backend.set(config_.username, auth_data);
        } break;
        default: {
          // a kdf-value that doesn't match enum
          cerr_ << "unknown kdf: " << static_cast<int>(config_.kdf)
                << std::endl;
          return EXIT_FAILURE;
        }
      }
      break;
    }
    case Cmd::ShowHelp:
    case Cmd::ShowVersion:
      // unreachable
      abort();
      break;
  }

  // write passwd file back, if it changed
  if (changed) {
    std::fstream f(config_.filename, std::ios::out);
    if (!f.is_open()) {
      cerr_ << "opening '" + config_.filename + "' for writing failed"
            << std::endl;
      return EXIT_FAILURE;
    }

    try {
      mysql_harness::make_file_private(config_.filename);
    } catch (const std::exception &e) {
      cerr_ << e.what() << std::endl;
      return EXIT_FAILURE;
    }

    backend.to_stream(f);
    if (f.bad()) {
      cerr_ << "writing to '" << config_.filename << "' failed" << std::endl;
      return EXIT_FAILURE;
    }
    f.close();

    if (f.bad()) {
      cerr_ << "closing '" << config_.filename << "' failed" << std::endl;
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}

void PasswdFrontend::prepare_command_options() {
  // prepare default-kdf-name and the list of supported names
  std::string default_kdf_name;
  std::string supported_kdf_names;
  for (auto it = supported_kdfs.cbegin(); it != supported_kdfs.cend(); ++it) {
    auto kv = *it;
    if (kv.second == config_.kdf) {
      default_kdf_name = kv.first;
    }

    if (it != supported_kdfs.cbegin()) supported_kdf_names += ", ";
    supported_kdf_names += kv.first;
  }

  arg_handler_.add_option(
      CmdOption::OptionNames({"-?", "--help"}), "Display this help and exit.",
      CmdOptionValueReq::none, "", [this](const std::string &) {
        config_.cmd = PasswdFrontend::Cmd::ShowHelp;
      });
  arg_handler_.add_option(
      CmdOption::OptionNames({"--kdf"}),
      "Key Derivation Function for 'set'. One of " + supported_kdf_names +
          ". default: " + default_kdf_name,
      CmdOptionValueReq::required, "name", [this](const std::string &value) {
        auto it = supported_kdfs.find(value);

        if (it == supported_kdfs.end()) {
          throw UsageError("unknown kdf: " + value);
        } else {
          config_.kdf = (*it).second;
        }
      });
  arg_handler_.add_option(
      //
      CmdOption::OptionNames({"-V", "--version"}),
      "Display version information and exit.", CmdOptionValueReq::none, "",
      [this](const std::string &) {
        config_.cmd = PasswdFrontend::Cmd::ShowVersion;
      });
  arg_handler_.add_option(
      CmdOption::OptionNames({"--work-factor"}),
      "Work-factor hint for KDF if account is updated.",
      CmdOptionValueReq::required, "num", [this](const std::string &value) {
        try {
          size_t end_pos;
          long num = std::stol(value, &end_pos);
          if (num < 0) {
            throw UsageError("--work-factor is negative (must be positive)");
          }
          if (end_pos != value.size()) {
            throw UsageError("--work-factor is not a positive integer");
          }
          config_.cost = num;
        } catch (const std::out_of_range &) {
          throw UsageError("--work-factor is larger than " +
                           std::to_string(std::numeric_limits<long>::max()));
        } catch (const std::invalid_argument &) {
          throw UsageError(
              "--work-factor is not an integer (must be an integer)");
        }
      });
}
