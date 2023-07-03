/*
  Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#include "keyring_frontend.h"

#include <algorithm>
#include <cctype>  // isprint()
#include <fstream>
#include <sstream>
#include <vector>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>

#include "keyring/keyring_file.h"
#include "keyring/master_key_file.h"
#include "mysql/harness/arg_handler.h"
#include "mysql/harness/utility/string.h"
#include "mysqlrouter/keyring_info.h"
#include "mysqlrouter/utils.h"
#include "print_version.h"  // build_version
#include "random_generator.h"
#include "router_config.h"             // MYSQL_ROUTER_PACKAGE_NAME
#include "welcome_copyright_notice.h"  // ORACLE_WELCOME_COPYRIGHT_NOTICE

using namespace std::string_literals;

static const unsigned kKeyLength{32};

void KeyringFrontend::init_from_arguments(
    const std::vector<std::string> &arguments) {
  prepare_command_options();
  try {
    arg_handler_.process(arguments);
  } catch (const std::invalid_argument &e) {
    // unknown options
    throw UsageError(e.what());
  }
}

KeyringFrontend::KeyringFrontend(const std::string &exe_name,
                                 const std::vector<std::string> &args,
                                 std::istream &is, std::ostream &os,
                                 std::ostream &es)
    : program_name_{exe_name},
      arg_handler_{true},
      cin_{is},
      cout_{os},
      cerr_{es} {
  init_from_arguments(args);
}

std::string KeyringFrontend::get_version() noexcept {
  std::stringstream os;

  std::string version_string;
  build_version(std::string(MYSQL_ROUTER_PACKAGE_NAME), &version_string);

  os << version_string << std::endl;
  os << ORACLE_WELCOME_COPYRIGHT_NOTICE("2019") << std::endl;

  return os.str();
}

std::string KeyringFrontend::get_help(const size_t screen_width) const {
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
      {"init", "initialize a keyring."},
      {"set", "add or overwrite account of <username> in <filename>."},
      {"delete", "delete entry from keyring."},
      {"list", "list all entries in keyring."},
      {"export", "export all entries of keyring as JSON."},
      {"get", "field from keyring entry"},
      {"master-delete", "keyring from master-keyfile"},
      {"master-list", "list entries from master-keyfile"},
      {"master-rename", "renames and entry in a master-keyfile"},
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

/**
 * load the masterkeyfile.
 *
 * @throws FrontendError on anything else.
 * @returns success
 * @retval true if loaded
 * @retval false if file didn't exist
 */
static bool master_key_file_load(mysql_harness::MasterKeyFile &mkf) {
  try {
    mkf.load();
  } catch (const std::system_error &e) {
    // if it doesn't exist, we'll later create it
    if (e.code() != std::errc::no_such_file_or_directory) {
      throw FrontendError("opening master-key-file failed: "s + e.what());
    }

    return false;
  } catch (const std::exception &e) {
    throw FrontendError("opening master-key-file failed: "s + e.what());
  }

  return true;
}

/**
 * prepare the keyring.
 *
 * if keyring doesn't exist, generates a new random
 *
 * @throws FrontendError on anything else
 * @returns if keyring changed and the keyrings random
 */
static std::pair<bool, std::string> keyring_file_prepare(
    mysql_harness::KeyringFile &kf, const std::string &keyring_filename) {
  if (keyring_filename.empty()) {
    throw UsageError("expected <keyring> to be not empty");
  }

  try {
    return {false, kf.read_header(keyring_filename)};
  } catch (const std::system_error &e) {
    // if it doesn't exist, we'll later create it
    if (e.code() != std::errc::no_such_file_or_directory) {
      throw FrontendError(e.what());
    }

    mysql_harness::RandomGenerator rg;
    std::string kf_random = rg.generate_strong_password(kKeyLength);
    kf.set_header(kf_random);

    return {true, kf_random};
  } catch (const std::exception &e) {
    throw FrontendError(e.what());
  }
}

static bool keyring_file_load(mysql_harness::KeyringFile &kf,
                              const std::string &keyring_filename,
                              const std::string &kf_key) {
  if (keyring_filename.empty()) {
    throw UsageError("expected <keyring> to be not empty");
  }

  // load it, if it exists to be able to later save it with the same content
  try {
    kf.load(keyring_filename, kf_key);

    return true;
  } catch (const std::system_error &e) {
    if (e.code() != std::errc::no_such_file_or_directory) {
      throw FrontendError(e.what());
    }

    return false;
  } catch (const std::exception &e) {
    throw FrontendError("loading failed?"s + e.what());
  }
}

/**
 * prepare master-key-file for keyring.
 *
 * if keyring-file isn't known in master-key-file:
 *
 * - generates encryption-key for keyring
 * - adds keyring to master-key-file
 *
 * otherwise, gets encryption-key from master-key-file
 *
 * @returns if-master-key-file-changed and the encryption-key for the keyring
 */
static std::pair<bool, std::string> master_key_file_prepare(
    mysql_harness::MasterKeyFile &mkf, mysql_harness::KeyringFile &kf,
    const std::string &keyring_filename, const std::string &kf_random) {
  std::string kf_key;
  try {
    kf_key = mkf.get(keyring_filename, kf_random);
    if (kf_key.empty()) {
      // master-keyring doesn't exist or keyfile not known in master-keyring
      mysql_harness::RandomGenerator rg;
      kf_key = rg.generate_strong_password(kKeyLength);

      mkf.add(keyring_filename, kf_key, kf_random);

      return {true, kf_key};
    } else {
      keyring_file_load(kf, keyring_filename, kf_key);
    }
  } catch (const mysql_harness::decryption_error &) {
    // file is known, but our key doesn't match
    throw FrontendError(
        "master-key-file knows key-file, but key doesn't match.");
  }

  return {false, kf_key};
}

static void cmd_init_with_master_key_file(
    const std::string &keyring_filename,
    const std::string &master_keyring_filename) {
  mysql_harness::KeyringFile kf;
  bool kf_changed{false};
  std::string kf_random;
  std::tie(kf_changed, kf_random) = keyring_file_prepare(kf, keyring_filename);

  if (!kf_changed) {
    size_t file_size;
    try {
      std::fstream f(keyring_filename);
      f.seekg(0, std::ios_base::end);
      file_size = f.tellg();
      f.close();
    } catch (const std::exception &e) {
      throw FrontendError(e.what());
    }

    if (file_size > (4                   // marker
                     + 4                 // header length
                     + kf_random.size()  // header
                     + 4                 // payload-signature
                     + 4                 // payload-version
                     + 4                 // entries
                     + 4                 // no idea
                     )) {
      // keyring exists and header was parsed
      throw FrontendError("keyfile '" + keyring_filename +
                          "' already exists and has entries");
    }
  }

  mysql_harness::MasterKeyFile mkf(master_keyring_filename);

  master_key_file_load(mkf);

  bool mkf_changed{false};
  std::string kf_key;
  std::tie(mkf_changed, kf_key) =
      master_key_file_prepare(mkf, kf, keyring_filename, kf_random);

  // save master-key-file first to not create a keyring entry that can't be
  // decoded.
  try {
    if (mkf_changed) mkf.save();
  } catch (const std::exception &e) {
    throw FrontendError("failed saving master-key-file: "s + e.what());
  }
  try {
    if (kf_changed) kf.save(keyring_filename, kf_key);
  } catch (const std::exception &e) {
    throw FrontendError("failed saving keyring: "s + e.what());
  }
}

static std::pair<bool, std::string> master_key_reader_load(
    const std::string &master_key_reader, const std::string &keyring_filename) {
  try {
    KeyringInfo kinfo;
    kinfo.set_master_key_reader(master_key_reader);
    if (!kinfo.read_master_key()) {
      throw FrontendError("failed reading master-key for '" + keyring_filename +
                          "' from master-key-reader '"s + master_key_reader +
                          "'");
    }
    std::string kf_key = kinfo.get_master_key();
    if (kf_key.empty()) {
      // master-keyring doesn't exist or keyfile not known in master-keyring
      mysql_harness::RandomGenerator rg;

      return {true, rg.generate_strong_password(kKeyLength)};
    } else {
      return {false, kf_key};
    }
  } catch (const FrontendError &) {
    throw;
  } catch (const std::exception &e) {
    throw FrontendError(
        "failed reading master-key from --master-key-reader: "s + e.what());
  }
}

static void cmd_init_with_master_key_reader(
    const std::string &keyring_filename, const std::string &master_key_reader,
    const std::string &master_key_writer) {
  mysql_harness::KeyringFile kf;
  bool kf_changed{false};
  std::string kf_random;
  std::tie(kf_changed, kf_random) = keyring_file_prepare(kf, keyring_filename);

  std::string kf_key;
  bool mk_changed{false};
  std::tie(mk_changed, kf_key) =
      master_key_reader_load(master_key_reader, keyring_filename);

  keyring_file_load(kf, keyring_filename, kf_key);

  if (mk_changed) {
    KeyringInfo kinfo;
    kinfo.set_master_key_writer(master_key_writer);
    if (!kinfo.write_master_key()) {
      throw FrontendError("failed writing master-key for '" + keyring_filename +
                          "' to master-key-writer '" + master_key_writer + "'");
    }
  }
  try {
    if (kf_changed) kf.save(keyring_filename, kf_key);
  } catch (const std::exception &e) {
    throw FrontendError("failed saving keyfile: "s + e.what());
  }
}

static void cmd_init_with_master_key(const std::string &keyring_filename,
                                     const std::string &kf_key) {
  if (kf_key.empty()) {
    throw FrontendError("expected master-key for '" + keyring_filename +
                        "' to be not empty, but it is");
  }

  bool kf_changed{false};
  mysql_harness::KeyringFile kf;
  std::string kf_random;
  std::tie(kf_changed, kf_random) = keyring_file_prepare(kf, keyring_filename);

  keyring_file_load(kf, keyring_filename, kf_key);

  try {
    if (kf_changed) kf.save(keyring_filename, kf_key);
  } catch (const std::exception &e) {
    throw FrontendError("failed saving keyfile: "s + e.what());
  }
}

static void cmd_master_delete(const std::string &master_keyring_filename,
                              const std::string &keyring_filename) {
  mysql_harness::MasterKeyFile mkf(master_keyring_filename);
  try {
    mkf.load();
  } catch (const std::exception &e) {
    throw FrontendError("opening master-key-file failed: "s + e.what());
  }

  bool mkf_changed = mkf.remove(keyring_filename);

  if (mkf_changed) {
    mkf.save();
  } else {
    throw FrontendError("Keyring '" + keyring_filename +
                        "' not found in master-key-file '" +
                        master_keyring_filename + "'");
  }
}

static void cmd_master_list(std::ostream &cout,
                            const std::string &master_keyring_filename) {
  mysql_harness::MasterKeyFile mkf(master_keyring_filename);
  try {
    mkf.load();
  } catch (const std::exception &e) {
    throw FrontendError("opening master-key-file failed: "s + e.what());
  }

  for (auto it = mkf.entries().begin(); it != mkf.entries().end(); ++it) {
    cout << it->first << std::endl;
  }
}

static void cmd_master_rename(const std::string &master_keyring_filename,
                              const std::string &old_key,
                              const std::string &new_key) {
  mysql_harness::MasterKeyFile mkf(master_keyring_filename);
  try {
    mkf.load();
  } catch (const std::exception &e) {
    throw FrontendError("opening master-key-file failed: "s + e.what());
  }

  try {
    mkf.add_encrypted(new_key, mkf.get_encrypted(old_key));
  } catch (const std::out_of_range &) {
    throw FrontendError("old-key '" + old_key +
                        "' not found in master-key-file '" +
                        master_keyring_filename + "'");
  } catch (const std::invalid_argument &) {
    throw FrontendError("new-key '" + new_key +
                        "' already exists in master-key-file '" +
                        master_keyring_filename + "'");
  }
  mkf.remove(old_key);

  mkf.save();
}

static void cmd_export(std::ostream &os, const mysql_harness::KeyringFile &kf) {
  rapidjson::StringBuffer json_buf;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> json_writer(json_buf);
  rapidjson::Document json_doc;
  rapidjson::Document::AllocatorType &allocator = json_doc.GetAllocator();
  json_doc.SetObject();
  for (auto const &entry : kf.entries()) {
    rapidjson::Value el;

    el.SetObject();

    for (auto const &kv : entry.second) {
      el.AddMember(
          rapidjson::Value(kv.first.c_str(), kv.first.size(), allocator),
          rapidjson::Value(kv.second.c_str(), kv.second.size(), allocator),
          allocator);
    }
    json_doc.AddMember(
        rapidjson::Value(entry.first.c_str(), entry.first.size(), allocator),
        el.Move(), allocator);

  }  // free json_doc and json_writer early
  json_doc.Accept(json_writer);

  os << json_buf.GetString() << std::endl;
}

static bool cmd_list(std::ostream &os, mysql_harness::KeyringFile &kf,
                     const std::string &username) {
  bool found{false};
  if (username.empty()) {
    found = true;
    for (auto const &entry : kf.entries()) {
      os << entry.first << std::endl;
    }
  } else {
    for (auto const &entry : kf.entries()) {
      if (entry.first == username) {
        found = true;
        for (auto const &kv : entry.second) {
          os << kv.first << std::endl;
        }
      }
    }
  }
  return found;
}

static bool cmd_delete(mysql_harness::KeyringFile &kf,
                       const std::string &username, const std::string &field) {
  if (field.empty()) {
    return kf.remove(username);
  } else {
    return kf.remove_attribute(username, field);
  }
}

static void cmd_set(mysql_harness::KeyringFile &kf, const std::string &username,
                    const std::string &field, const std::string &value) {
  kf.store(username, field, value);
}

static void cmd_get(std::ostream &os, mysql_harness::KeyringFile &kf,
                    const std::string &username, const std::string &field) {
  try {
    os << kf.fetch(username, field) << std::endl;
  } catch (const std::out_of_range &) {
    throw FrontendError("'" + field + "' not found for user '" + username +
                        "'");
  }
}

/**
 * prepare arguments and cmds.
 *
 * - check command name
 * - check argument counts
 *
 * @throws UsageError on error
 */
void KeyringFrontend::prepare_args() {
  // handle positional args
  const auto &rest_args = arg_handler_.get_rest_arguments();
  auto rest_args_count = rest_args.size();

  if (config_.cmd != Cmd::ShowHelp && config_.cmd != Cmd::ShowVersion) {
    if (rest_args_count == 0) {
      throw UsageError("expected a <cmd>");
    }

    std::map<std::string, KeyringFrontend::Cmd> cmds{
        {"init", KeyringFrontend::Cmd::Init},
        {"set", KeyringFrontend::Cmd::Set},
        {"delete", KeyringFrontend::Cmd::Delete},
        {"list", KeyringFrontend::Cmd::List},
        {"export", KeyringFrontend::Cmd::Export},
        {"get", KeyringFrontend::Cmd::Get},
        {"master-delete", KeyringFrontend::Cmd::MasterDelete},
        {"master-list", KeyringFrontend::Cmd::MasterList},
        {"master-rename", KeyringFrontend::Cmd::MasterRename},
    };

    const auto cmd = cmds.find(rest_args[0]);
    if (cmd == cmds.end()) {
      throw UsageError("unknown command: " + rest_args[0]);
    }

    config_.cmd = cmd->second;

    --rest_args_count;
  }

  switch (config_.cmd) {
    case Cmd::MasterDelete:
    case Cmd::Init:
    case Cmd::Export:
      if (rest_args_count != 1) {
        throw UsageError("expected one argument <filename>, got " +
                         std::to_string(rest_args_count) + " arguments");
      }
      config_.keyring_filename = rest_args[1];
      break;
    case Cmd::MasterList:
    case Cmd::ShowVersion:
    case Cmd::ShowHelp:
      if (rest_args_count != 0) {
        throw UsageError("expected no extra arguments, got " +
                         std::to_string(rest_args_count) + " arguments");
      }
      break;
    case Cmd::List:
      if (rest_args_count < 1 || rest_args_count > 2) {
        throw UsageError("expected <filename> and optionally <username>, got " +
                         std::to_string(rest_args_count) + " arguments");
      }

      config_.keyring_filename = rest_args[1];
      if (rest_args_count > 1) config_.username = rest_args[2];
      break;
    case Cmd::Get:
      if (rest_args_count != 3) {
        throw UsageError("expected <filename> <username> <key>, got " +
                         std::to_string(rest_args_count) + " arguments");
      }

      config_.keyring_filename = rest_args[1];
      config_.username = rest_args[2];
      config_.field = rest_args[3];
      break;
    case Cmd::Set:
      if (rest_args_count != 4 && rest_args_count != 3) {
        throw UsageError(
            "expected <filename> <username> <key>, optionally <value>, got " +
            std::to_string(rest_args_count) + " arguments");
      }

      config_.keyring_filename = rest_args[1];
      config_.username = rest_args[2];
      config_.field = rest_args[3];
      if (rest_args_count == 4) {
        config_.value = rest_args[4];
      } else {
        config_.value =
            mysqlrouter::prompt_password("value for " + config_.field);
      }
      break;
    case Cmd::Delete:
      if (rest_args_count != 2 && rest_args_count != 3) {
        throw UsageError(
            "expected <filename> <username>, and optionally <key>, got " +
            std::to_string(rest_args_count) + " arguments");
      }

      config_.keyring_filename = rest_args[1];
      config_.username = rest_args[2];
      if (rest_args_count == 3) config_.field = rest_args[3];
      break;
    case Cmd::MasterRename:
      if (rest_args_count != 2) {
        throw UsageError("expected 2 arguments <old-key> <new-key>, got " +
                         std::to_string(rest_args_count) + " arguments");
      }

      config_.keyring_filename = rest_args[1];
      config_.username = rest_args[2];
      break;
  }
}

int KeyringFrontend::run() {
  prepare_args();

  switch (config_.cmd) {
    case Cmd::ShowHelp:
      cout_ << get_help() << std::endl;
      return EXIT_SUCCESS;
    case Cmd::ShowVersion:
      cout_ << get_version() << std::endl;
      return EXIT_SUCCESS;
    default:
      break;
  }

  if (!config_.master_keyring_filename.empty() &&
      !config_.master_key_reader.empty()) {
    throw UsageError(
        "--master-key-file and --master-key-reader can't be used together");
  }

  if (!config_.master_keyring_filename.empty() &&
      !config_.master_key_writer.empty()) {
    throw UsageError(
        "--master-key-file and --master-key-writer can't be used together");
  }

  switch (config_.cmd) {
    case Cmd::Init:
      if (!config_.master_keyring_filename.empty()) {
        cmd_init_with_master_key_file(config_.keyring_filename,
                                      config_.master_keyring_filename);

      } else if (!config_.master_key_reader.empty() ||
                 !config_.master_key_writer.empty()) {
        cmd_init_with_master_key_reader(config_.keyring_filename,
                                        config_.master_key_reader,
                                        config_.master_key_writer);

      } else {
        cmd_init_with_master_key(
            config_.keyring_filename,
            mysqlrouter::prompt_password("Please enter master key"));
      }

      return EXIT_SUCCESS;
    case Cmd::MasterDelete:
      if (config_.master_keyring_filename.empty()) {
        throw UsageError("expected --master-key-file to be not empty");
      }

      cmd_master_delete(config_.master_keyring_filename,
                        config_.keyring_filename);

      return EXIT_SUCCESS;
    case Cmd::MasterList:
      if (config_.master_keyring_filename.empty()) {
        throw UsageError("expected --master-key-file to be not empty");
      }

      cmd_master_list(cout_, config_.master_keyring_filename);
      return EXIT_SUCCESS;
    case Cmd::MasterRename:
      // master-rename uses the 'config_' struct slightly differently then
      // the other commands:
      //
      // * config_.keyring_filename -> <old_key>
      // * config_.username         -> <new_key>
      //
      if (config_.master_keyring_filename.empty()) {
        throw UsageError("expected --master-key-file to be not empty");
      }
      if (config_.keyring_filename.empty()) {
        throw UsageError("expected <old-key> to be not empty");
      }

      if (config_.username.empty()) {
        throw UsageError("expected <new-key> to be not empty");
      }

      for (const char c : config_.keyring_filename) {
        if (!std::isprint(c)) {
          throw UsageError(
              "expected <old-key> to contain only printable characters");
        }
      }

      for (const char c : config_.username) {
        if (!std::isprint(c)) {
          throw UsageError(
              "expected <new-key> to contain only printable characters");
        }
      }
      cmd_master_rename(config_.master_keyring_filename,
                        config_.keyring_filename, config_.username);
      return EXIT_SUCCESS;
    default:
      break;
  }

  // Cmd::Init, Cmd::Master* are already handled.
  //
  // All other commands require a key from
  // - the master-key-ring,
  // - stdin
  // - master-key-reader

  std::string kf_key;
  mysql_harness::KeyringFile kf;

  if (!config_.master_keyring_filename.empty()) {
    std::string kf_random;
    try {
      kf_random = kf.read_header(config_.keyring_filename);
    } catch (const std::exception &e) {
      throw FrontendError("opening keyring failed: "s + e.what());
    }
    mysql_harness::MasterKeyFile mkf(config_.master_keyring_filename);
    try {
      mkf.load();
      kf_key = mkf.get(config_.keyring_filename, kf_random);
    } catch (const std::exception &e) {
      throw FrontendError("opening master-key-file failed: "s + e.what());
    }

    if (kf_key.empty()) {
      throw FrontendError("couldn't find master key for " +
                          config_.keyring_filename + " in master-key-file " +
                          config_.master_keyring_filename);
    }
  } else if (!config_.master_key_reader.empty()) {
    KeyringInfo kinfo;
    kinfo.set_master_key_reader(config_.master_key_reader);
    if (!kinfo.read_master_key()) {
      throw FrontendError(
          "failed reading master-key for '" + config_.keyring_filename +
          "' from master-key-reader '" + config_.master_key_reader + "'");
    }
    kf_key = kinfo.get_master_key();
  } else {
    kf_key = mysqlrouter::prompt_password("Please enter master key");
  }

  if (kf_key.empty()) {
    throw FrontendError("expected master-key for '" + config_.keyring_filename +
                        "' to be not empty, but it is");
  }

  // load keyring
  keyring_file_load(kf, config_.keyring_filename, kf_key);

  bool kf_changed = false;
  switch (config_.cmd) {
    case Cmd::ShowHelp:
    case Cmd::ShowVersion:
    case Cmd::Init:
    case Cmd::MasterList:
    case Cmd::MasterDelete:
    case Cmd::MasterRename:
      // unreachable
      abort();
      break;
    case Cmd::Get:
      cmd_get(cout_, kf, config_.username, config_.field);
      break;
    case Cmd::Set:
      cmd_set(kf, config_.username, config_.field, config_.value);
      kf_changed = true;

      break;
    case Cmd::Delete: {
      if (!cmd_delete(kf, config_.username, config_.field)) {
        return EXIT_FAILURE;
      }
      kf_changed = true;
      break;
    }
    case Cmd::Export:
      cmd_export(cout_, kf);
      break;
    case Cmd::List:
      if (!cmd_list(cout_, kf, config_.username)) {
        return EXIT_FAILURE;
      }
      break;
  }

  if (kf_changed) {
    kf.save(config_.keyring_filename, kf_key);
  }

  return EXIT_SUCCESS;
}

void KeyringFrontend::prepare_command_options() {
  // prepare default-kdf-name and the list of supported names
  arg_handler_.add_option(
      CmdOption::OptionNames({"-?", "--help"}), "Display this help and exit.",
      CmdOptionValueReq::none, "", [this](const std::string &) {
        if (config_.cmd != Cmd::ShowVersion) {
          config_.cmd = Cmd::ShowHelp;
        }
      });
  arg_handler_.add_option(
      //
      CmdOption::OptionNames({"-V", "--version"}),
      "Display version information and exit.", CmdOptionValueReq::none, "",
      [this](const std::string &) {
        if (config_.cmd != Cmd::ShowHelp) {
          config_.cmd = Cmd::ShowVersion;
        }
      });
  arg_handler_.add_option(
      CmdOption::OptionNames({"--master-key-file"}),
      "Filename of the master keyfile.", CmdOptionValueReq::required, "",
      [this](const std::string &v) {
        if (v.empty()) {
          throw UsageError("expected --master-key-file to be not empty.");
        }
        config_.master_keyring_filename = v;
      });
  arg_handler_.add_option(
      CmdOption::OptionNames({"--master-key-reader"}),
      "Executable which provides the master key for the keyfile.",
      CmdOptionValueReq::required, "", [this](const std::string &v) {
        if (v.empty()) {
          throw UsageError("expected --master-key-reader to be not empty.");
        }
        config_.master_key_reader = v;
      });
  arg_handler_.add_option(
      CmdOption::OptionNames({"--master-key-writer"}),
      "Executable which can store the master key for the keyfile.",
      CmdOptionValueReq::required, "", [this](const std::string &v) {
        if (v.empty()) {
          throw UsageError("expected --master-key-writer to be not empty.");
        }
        config_.master_key_writer = v;
      });
}
