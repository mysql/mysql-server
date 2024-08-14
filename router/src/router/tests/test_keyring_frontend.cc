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

/**
 * Test the mysqlrouter_keyring tool.
 */
#include "keyring_frontend.h"

#include <array>
#include <bitset>
#include <fstream>
#include <numeric>
#include <span>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include <gmock/gmock.h>

#include "dim.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/loader_config.h"
#include "mysql/harness/logging/registry.h"
#include "mysql/harness/stdx/filesystem.h"
#include "mysql/harness/string_utils.h"    // mysql_harness::split_string
#include "mysql/harness/utility/string.h"  // mysql_harness::join
#include "mysqlrouter/utils.h"             // set_prompt_password
#include "print_version.h"                 // build_version
#include "router_config.h"                 // MYSQL_ROUTER_PACKAGE_NAME
#include "welcome_copyright_notice.h"      // ORACLE_WELCOME_COPYRIGHT_NOTICE

constexpr const char kAppExeFileName[]{"mysqlrouter_keyring"};

using namespace std::string_literals;
using mysql_harness::join;

mysql_harness::Path g_origin_path;

constexpr size_t kOptIndent = 2;
constexpr size_t kDescIndent = 6;

static void ParamPrinter(
    const std::vector<std::pair<std::string, std::string>> &fields,
    std::ostream *os) {
  *os << "(";
  bool is_first{true};
  for (const auto &kv : fields) {
    if (is_first) {
      is_first = false;
    } else {
      *os << ", ";
    }
    *os << kv.first << ": " << kv.second;
  }
  *os << ")";
}

struct Option {
  std::vector<std::string> opts;
  std::string arg;
  std::string desc;
};

static const auto cmdline_opts = std::to_array<Option>({
    // should be alphabetically ordered
    {{"-?", "--help"}, "", "Display this help and exit."},
    {{"-V", "--version"}, "", "Display version information and exit."},
    {{"--master-key-file"}, "<VALUE>", "Filename of the master keyfile."},
    {{"--master-key-reader"},
     "<VALUE>",
     "Executable which provides the master key for the keyfile."},
    {{"--master-key-writer"},
     "<VALUE>",
     "Executable which can store the master key for the keyfile."},
});

static const auto cmdline_cmds =
    std::to_array<std::pair<std::string, std::string>>({
        {"init", "initialize a keyring."},
        {"set", "add or overwrite account of <username> in <filename>."},
        {"delete", "delete entry from keyring."},
        {"list", "list all entries in keyring."},
        {"export", "export all entries of keyring as JSON."},
        {"get", "field from keyring entry"},
        {"master-delete", "keyring from master-keyfile"},
        {"master-list", "list entries from master-keyfile"},
        {"master-rename", "renames and entry in a master-keyfile"},
    });

// build param of description
static std::string format_desc_opt(const Option &opt) {
  auto val = opt.arg;
  return join(std::accumulate(
                  opt.opts.begin(), opt.opts.end(), std::vector<std::string>(),
                  [&val](std::vector<std::string> acc, std::string cur) {
                    acc.push_back(cur + (val.empty() ? "" : " " + val));

                    return acc;
                  }),
              ", ");
}

// build help-text from options
static std::string help_builder(const std::span<const Option> &opts) {
  std::vector<std::string> out;

  {
    out.push_back("Usage");
    out.push_back("");

    // opts
    std::vector<std::string> formatted_options;
    formatted_options.push_back(kAppExeFileName);
    formatted_options.push_back("[opts]");
    formatted_options.push_back("<cmd>");
    formatted_options.push_back("<filename>");
    formatted_options.push_back("[<username>]");

    std::string line{" "};
    for (const auto &opt : formatted_options) {
      if (line.size() + 1 + opt.size() > 93) {
        out.push_back(line);

        // prepare next line
        line = " ";
      }
      line += " " + opt;
    }

    out.push_back(line);

    // --help
    formatted_options.clear();
    formatted_options.push_back(kAppExeFileName);
    formatted_options.push_back("--help");

    line = " ";
    for (const auto &opt : formatted_options) {
      if (line.size() + 1 + opt.size() > 93) {
        out.push_back(line);

        // prepare next line
        line = " ";
      }
      line += " " + opt;
    }

    out.push_back(line);

    // --version
    formatted_options.clear();
    formatted_options.push_back(kAppExeFileName);
    formatted_options.push_back("--version");

    line = " ";
    for (const auto &opt : formatted_options) {
      if (line.size() + 1 + opt.size() > 93) {
        out.push_back(line);

        // prepare next line
        line = " ";
      }
      line += " " + opt;
    }

    out.push_back(line);
  }

  if (!cmdline_cmds.empty()) {
    out.push_back("");
    out.push_back("Commands");
    out.push_back("");
    for (const auto &opt : cmdline_cmds) {
      out.push_back(std::string(kOptIndent, ' ') + opt.first);
      out.push_back(std::string(kDescIndent, ' ') + opt.second);
    }
  }

  if (!opts.empty()) {
    out.push_back("");
    out.push_back("Options");
    out.push_back("");

    for (const auto &opt : opts) {
      out.push_back(std::string(kOptIndent, ' ') + format_desc_opt(opt));
      out.push_back(std::string(kDescIndent, ' ') + opt.desc);
    }
  }

  // enforce a newline at the end
  out.push_back("");
  return join(out, "\n");
}

static std::string version_builder() {
  std::string version_string;
  build_version(MYSQL_ROUTER_PACKAGE_NAME, &version_string);

  std::stringstream os;
  os << version_string << std::endl
     << ORACLE_WELCOME_COPYRIGHT_NOTICE("2019") << std::endl;

  return os.str();
}

const std::string kHelpText(help_builder(cmdline_opts));
const std::string kVersionText(version_builder());

// placeholder in the opts to replace by the temp-filename
const std::string kKeyringPlaceholder("@keyringfile@");
const std::string kMasterKeyfilePlaceholder("@masterkeyringfile@");
const std::string kMasterKeyWriterPlaceholder("@masterkeywriter@");
const std::string kMasterKeyReaderPlaceholder("@masterkeyreader@");

const std::string kMasterKeyReaderSucceeding(
#ifndef _WIN32
    "#!/bin/sh\n"
    "echo foobar\n"
    "exit 0"
#else
    "@echo off\n"
    "echo foobar\n"
    "exit 0"
#endif
);

const std::string kMasterKeyReaderKeyNotFound(
#ifndef _WIN32
    "#!/bin/sh\n"
    "exit 0"
#else
    "@echo off\n"
    "exit 0"
#endif
);

const std::string kMasterKeyReaderFailing(
#ifndef _WIN32
    "#!/bin/sh\n"
    "exit -1"
#else
    "@echo off\n"
    "exit 1"
#endif
);

const std::string kMasterKeyWriterSucceeding(
#ifndef _WIN32
    "#!/bin/sh\n"
    "exit 0"
#else
    "@echo off\n"
    "exit 0"
#endif
);
const std::string kMasterKeyWriterFailing(
#ifndef _WIN32
    "#!/bin/sh\n"
    "exit -1"
#else
    "@echo off\n"
    "exit 1"
#endif
);

// count how many bits are required to represent 'max_value'
constexpr size_t max_bits(size_t max_value) {
  size_t used_bits{0};
  while (max_value) {
    used_bits++;
    max_value >>= 1;
  }

  return used_bits;
}

static_assert(max_bits(0) == 0);
static_assert(max_bits(1) == 1);
static_assert(max_bits(2) == 2);
static_assert(max_bits(3) == 2);
static_assert(max_bits(4) == 3);
static_assert(max_bits(7) == 3);
static_assert(max_bits(8) == 4);

template <class T, class Values, class Prev, size_t max_value>
class ChainedBitset {
 public:
  using prev = Prev;
  using bitset_type = uint64_t;
  using value_type = Values;

  static constexpr value_type from_bitset(uint64_t v) {
    return static_cast<value_type>((v & bit_mask) >> bit_shift);
  }

  static constexpr bitset_type to_bitset(value_type v) {
    if (static_cast<bitset_type>(v) > max_value)
      throw std::out_of_range("value is larger than announced max_value");
    auto r = static_cast<bitset_type>(v) << bit_shift;

    return r;
  }

  static constexpr size_t bit_shift{prev::bit_shift + prev::bit_mask_width};
  static constexpr size_t bit_mask_width{max_bits(max_value)};
  static constexpr uint64_t bit_mask{((1 << bit_mask_width) - 1) << bit_shift};
};

struct StartingPoint {
  static constexpr size_t bit_shift{0};
  static constexpr size_t bit_mask_width{0};
};

namespace PreCond {
enum class KeyringValues {
  none,
  empty,
  minimal,
  one_user_one_property,
  many_user_one_property,
  long_property,
  long_username,
  special_properties,
  no_entries,
  one_entry,
  inited,
};

struct Keyring
    : public ChainedBitset<Keyring, KeyringValues, StartingPoint,
                           static_cast<uint64_t>(KeyringValues::inited)> {
  using Values = KeyringValues;
  static constexpr uint64_t none() { return to_bitset(Values::none); }
  static constexpr uint64_t empty() { return to_bitset(Values::empty); }
  static constexpr uint64_t minimal() { return to_bitset(Values::minimal); }
  static constexpr uint64_t one_user_one_property() {
    return to_bitset(Values::one_user_one_property);
  }
  static constexpr uint64_t many_user_one_property() {
    return to_bitset(Values::many_user_one_property);
  }
  static constexpr uint64_t long_property() {
    return to_bitset(Values::long_property);
  }
  static constexpr uint64_t long_username() {
    return to_bitset(Values::long_username);
  }
  static constexpr uint64_t special_properties() {
    return to_bitset(Values::special_properties);
  }
  static constexpr uint64_t no_entries() {
    return to_bitset(Values::no_entries);
  }
  static constexpr uint64_t one_entry() { return to_bitset(Values::one_entry); }
  static constexpr uint64_t inited() { return to_bitset(Values::inited); }
};

enum class KeyringFilenameValues {
  none,                     // just tmpdir/keyring
  special_chars,            // tmpdir/key ring
  with_directory,           // make filename with subdir, create subdir
  with_no_exist_directory,  // make filename with subdir, but don't create
                            // subdir
  absolute,                 // make filename absolute
};

struct KeyringFilename
    : public ChainedBitset<KeyringFilename, KeyringFilenameValues, Keyring,
                           static_cast<uint64_t>(
                               KeyringFilenameValues::absolute)> {
  using Values = KeyringFilenameValues;
  static constexpr uint64_t none() { return to_bitset(Values::none); }
  static constexpr uint64_t special_chars() {
    return to_bitset(Values::special_chars);
  }
  static constexpr uint64_t with_directory() {
    return to_bitset(Values::with_directory);
  }
  static constexpr uint64_t with_no_exist_directory() {
    return to_bitset(Values::with_no_exist_directory);
  }
  static constexpr uint64_t absolute() { return to_bitset(Values::absolute); }
};

enum class MasterKeyfileValues {
  none,
  empty,
  minimal,
  valid_one_entry,    // one entry, foo.key
  valid_foo_bar_baz,  // three entries: foo.key, bar.key, and baz.key
  insecure,
};

struct MasterKeyfile
    : public ChainedBitset<MasterKeyfile, MasterKeyfileValues, KeyringFilename,
                           static_cast<uint64_t>(
                               MasterKeyfileValues::insecure)> {
  using Values = MasterKeyfileValues;

  static constexpr uint64_t none() { return to_bitset(Values::none); }
  static constexpr uint64_t empty() { return to_bitset(Values::empty); }
  static constexpr uint64_t minimal() { return to_bitset(Values::minimal); }
  static constexpr uint64_t valid_one_entry() {
    return to_bitset(Values::valid_one_entry);
  }
  static constexpr uint64_t valid_foo_bar_baz() {
    return to_bitset(Values::valid_foo_bar_baz);
  }
  static constexpr uint64_t insecure() { return to_bitset(Values::insecure); }
};

enum class MasterKeyfileFilenameValues {
  none,
  special_chars,
  with_directory,
  with_no_exist_directory,
};

struct MasterKeyfileFilename
    : public ChainedBitset<
          MasterKeyfileFilename, MasterKeyfileFilenameValues, MasterKeyfile,
          static_cast<uint64_t>(
              MasterKeyfileFilenameValues::with_no_exist_directory)> {
  using Values = MasterKeyfileFilenameValues;
  static constexpr uint64_t none() { return to_bitset(Values::none); }
  static constexpr uint64_t special_chars() {
    return to_bitset(Values::special_chars);
  }
  static constexpr uint64_t with_directory() {
    return to_bitset(Values::with_directory);
  }
  static constexpr uint64_t with_no_exist_directory() {
    return to_bitset(Values::with_no_exist_directory);
  }
};

enum class MasterKeyReaderValues {
  none,
  succeeding,
  failing,
  not_executable,
  key_not_found,
};

struct MasterKeyReader
    : public ChainedBitset<
          MasterKeyReader, MasterKeyReaderValues, MasterKeyfileFilename,
          static_cast<uint64_t>(MasterKeyReaderValues::key_not_found)> {
  using Values = MasterKeyReaderValues;

  static constexpr uint64_t none() { return to_bitset(Values::none); }
  static constexpr uint64_t succeeding() {
    return to_bitset(Values::succeeding);
  }
  static constexpr uint64_t failing() { return to_bitset(Values::failing); }
  static constexpr uint64_t not_executable() {
    return to_bitset(Values::not_executable);
  }
  static constexpr uint64_t key_not_found() {
    return to_bitset(Values::key_not_found);
  }
};

enum class MasterKeyWriterValues {
  none,
  succeeding,
  failing,
  not_executable,
};

struct MasterKeyWriter
    : public ChainedBitset<
          MasterKeyWriter, MasterKeyWriterValues, MasterKeyReader,
          static_cast<uint64_t>(MasterKeyWriterValues::not_executable)> {
  using Values = MasterKeyWriterValues;

  static constexpr uint64_t none() { return to_bitset(Values::none); }
  static constexpr uint64_t succeeding() {
    return to_bitset(Values::succeeding);
  }
  static constexpr uint64_t failing() { return to_bitset(Values::failing); }
  static constexpr uint64_t not_executable() {
    return to_bitset(Values::not_executable);
  }
};
}  // namespace PreCond

namespace PostCond {
enum class KeyringValues {
  none,
  exists_and_secure,
  not_exists,
};
struct Keyring
    : public ChainedBitset<Keyring, KeyringValues, PreCond::MasterKeyWriter,
                           static_cast<uint64_t>(KeyringValues::not_exists)> {
  using Values = KeyringValues;

  static constexpr uint64_t none() { return to_bitset(Values::none); }
  static constexpr uint64_t exists_and_secure() {
    return to_bitset(Values::exists_and_secure);
  }
  static constexpr uint64_t not_exists() {
    return to_bitset(Values::not_exists);
  }
};

enum class MasterKeyfileValues {
  none,
  exists,
  exists_and_secure,
  not_exists,
};

struct MasterKeyfile
    : public ChainedBitset<MasterKeyfile, MasterKeyfileValues, Keyring,
                           static_cast<uint64_t>(
                               MasterKeyfileValues::not_exists)> {
  using Values = MasterKeyfileValues;

  static constexpr uint64_t none() { return to_bitset(Values::none); }
  static constexpr uint64_t exists() { return to_bitset(Values::exists); }
  static constexpr uint64_t exists_and_secure() {
    return to_bitset(Values::exists_and_secure);
  }
  static constexpr uint64_t not_exists() {
    return to_bitset(Values::not_exists);
  }
};

enum class KeyringExportValues {
  none,
  empty_keys,
  user_a_password_stdin_value,
  user_a_password_foo,
  user_a_password_other,
  many_user_one_property,
  many_user_one_property_no_c_password,
  many_user_one_property_b_removed,
};

struct KeyringExport
    : public ChainedBitset<
          KeyringExport, KeyringExportValues, MasterKeyfile,
          static_cast<uint64_t>(
              KeyringExportValues::many_user_one_property_b_removed)> {
  using Values = KeyringExportValues;

  static constexpr uint64_t none() { return to_bitset(Values::none); }
  static constexpr uint64_t empty_keys() {
    return to_bitset(Values::empty_keys);
  }
  static constexpr uint64_t user_a_password_stdin_value() {
    return to_bitset(Values::user_a_password_stdin_value);
  }
  static constexpr uint64_t user_a_password_foo() {
    return to_bitset(Values::user_a_password_foo);
  }
  static constexpr uint64_t user_a_password_other() {
    return to_bitset(Values::user_a_password_other);
  }
  static constexpr uint64_t many_user_one_property() {
    return to_bitset(Values::many_user_one_property);
  }
  static constexpr uint64_t many_user_one_property_no_c_password() {
    return to_bitset(Values::many_user_one_property_no_c_password);
  }
  static constexpr uint64_t many_user_one_property_b_removed() {
    return to_bitset(Values::many_user_one_property_b_removed);
  }
};

enum class MasterListValues {
  none,                           // no checks
  empty,                          // no output
  one_entry,                      // foo.key
  contains_keyfile,               // contains keyfile, maybe others
  contains_keyfile_and_one_more,  // contains keyfile + one more
  bar_baz,                        // bar.key, baz.key
};

struct MasterList
    : public ChainedBitset<MasterList, MasterListValues, KeyringExport,
                           static_cast<uint64_t>(MasterListValues::bar_baz)> {
  using Values = MasterListValues;

  static constexpr uint64_t none() { return to_bitset(Values::none); }
  static constexpr uint64_t empty() { return to_bitset(Values::empty); }
  static constexpr uint64_t one_entry() { return to_bitset(Values::one_entry); }
  static constexpr uint64_t contains_keyfile() {
    return to_bitset(Values::contains_keyfile);
  }
  static constexpr uint64_t contains_keyfile_and_one_more() {
    return to_bitset(Values::contains_keyfile_and_one_more);
  }
  static constexpr uint64_t bar_baz() { return to_bitset(Values::bar_baz); }
};
}  // namespace PostCond

struct KeyringFrontendTestParam {
  std::string test_name;
  std::string test_scenario_id;

  std::vector<std::string> cmdline_args;
  int exit_code;
  std::string stdin_content;
  std::string stdout_content;
  std::string stderr_content;
  uint64_t options;

  friend void PrintTo(const KeyringFrontendTestParam &p, std::ostream *os) {
    ParamPrinter(
        {
            {"cmdline", ::testing::PrintToString(p.cmdline_args)},
        },
        os);
  }
};

class KeyringFrontendTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<KeyringFrontendTestParam> {};

class TempDirectory {
 public:
  explicit TempDirectory(const std::string &prefix = "router")
      : name_{mysql_harness::get_tmp_dir(prefix)} {}

  ~TempDirectory() { mysql_harness::delete_dir_recursive(name_); }

  std::string name() const { return name_; }

 private:
  std::string name_;
};

static constexpr auto kKeyringMinimal = std::to_array<unsigned char>(
    {0x4d, 0x52, 0x4b, 0x52, 0x00, 0x00, 0x00, 0x00});

// keyring with no entries, but with header
static constexpr auto kKeyringNoEntry = std::to_array<unsigned char>({
  0x4d, 0x52, 0x4b, 0x52,
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
      0x00, 0x00, 0x00, 0x20,
#else
      0x20, 0x00, 0x00, 0x00,
#endif
      0x2f, 0x59, 0x62, 0x58, 0x23, 0x50, 0x74, 0x66, 0x5e, 0x3c, 0x29, 0x6a,
      0x33, 0x50, 0x36, 0x5a, 0x44, 0x3a, 0x4e, 0x73, 0x51, 0x58, 0x79, 0x49,
      0x5e, 0x2b, 0x42, 0x3a, 0x38, 0x6d, 0x4f, 0x39, 0x95, 0x96, 0x74, 0x76,
      0x97, 0xaa, 0xcf, 0xbd, 0xd1, 0x5c, 0xce, 0xdb, 0x6f, 0xa1, 0xcf, 0xaf
});

static constexpr auto kKeyringOneEntry = std::to_array<unsigned char>({
  0x4d, 0x52, 0x4b, 0x52,
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
      0x00, 0x00, 0x00, 0x20,
#else
      0x20, 0x00, 0x00, 0x00,
#endif
      0x2f, 0x59, 0x62, 0x58, 0x23, 0x50, 0x74, 0x66, 0x5e, 0x3c, 0x29, 0x6a,
      0x33, 0x50, 0x36, 0x5a, 0x44, 0x3a, 0x4e, 0x73, 0x51, 0x58, 0x79, 0x49,
      0x5e, 0x2b, 0x42, 0x3a, 0x38, 0x6d, 0x4f, 0x39, 0x01, 0x77, 0x33, 0xb8,
      0x6a, 0x70, 0x91, 0x3d, 0x46, 0x1b, 0xeb, 0x17, 0x62, 0x8e, 0xe1, 0x55,
      0x53, 0xdf, 0x11, 0x08, 0x04, 0x42, 0x51, 0xc3, 0x8c, 0x67, 0xc8, 0x88,
      0xaa, 0xe1, 0xbd, 0x02, 0xa5, 0x60, 0x2b, 0x75, 0xbb, 0x59, 0x63, 0xba,
      0x5d, 0xaf, 0xfb, 0x71, 0xf1, 0xfd, 0xeb, 0x14
});

// valid, one entry (for "foo.key"), masterkeyfile.
//
// the master-key-ring is not endian-ness-safe:
//
// - master-key-file created on sparc can't be read on x86
static constexpr auto kMasterKeyfileOneEntry = std::to_array<unsigned char>({
  0x4d, 0x52, 0x4b, 0x46, 0x00,
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
      0x00, 0x00, 0x00, 0x38,
#else
      0x38, 0x00, 0x00, 0x00,
#endif
      0x66, 0x6f, 0x6f, 0x2e, 0x6b, 0x65, 0x79, 0x00, 0x30, 0x37, 0xf2, 0x4b,
      0xc0, 0xd6, 0x8d, 0x33, 0xb8, 0xd9, 0x39, 0xa2, 0x07, 0xa5, 0xc8, 0xc4,
      0xe2, 0x0a, 0x2e, 0xb9, 0x4f, 0x4a, 0x34, 0xa4, 0x39, 0xe8, 0x12, 0xc1,
      0x03, 0x52, 0xc7, 0x73, 0x71, 0x79, 0x04, 0xb9, 0x01, 0x53, 0x54, 0x11,
      0x3d, 0x8e, 0xa9, 0xd4, 0xe8, 0x99, 0x8a, 0x91
});

static constexpr auto kMasterKeyfileFooBarBaz = std::to_array<unsigned char>({
  0x4d, 0x52, 0x4b, 0x46, 0x00,
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
      0x00, 0x00, 0x00, 0x38,
#else
      0x38, 0x00, 0x00, 0x00,
#endif
      0x66, 0x6f, 0x6f, 0x2e, 0x6b, 0x65, 0x79, 0x00, 0x07, 0x85, 0x1a, 0xed,
      0xa7, 0x1d, 0xc8, 0xe7, 0x10, 0x37, 0x88, 0xd5, 0x92, 0x8b, 0xcc, 0xfd,
      0xe6, 0xbe, 0xa0, 0x81, 0xf4, 0xfe, 0x40, 0x97, 0xd1, 0x95, 0xec, 0xc8,
      0x10, 0x47, 0xd6, 0xa7, 0x77, 0xb6, 0x5a, 0xa8, 0xe1, 0x02, 0x0a, 0x7d,
      0xd0, 0x08, 0x70, 0x6f, 0x9a, 0xc9, 0xd6, 0x38,
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
      0x00, 0x00, 0x00, 0x38,
#else
      0x38, 0x00, 0x00, 0x00,
#endif
      0x62, 0x61, 0x72, 0x2e, 0x6b, 0x65, 0x79, 0x00, 0x80, 0xc9, 0x16, 0x02,
      0x75, 0x4f, 0xd1, 0xc2, 0x36, 0x1f, 0x89, 0x24, 0x31, 0x5d, 0x60, 0x78,
      0xc7, 0x92, 0xa0, 0xc0, 0xcb, 0xc2, 0xdc, 0xe7, 0x03, 0x85, 0x72, 0x53,
      0x8c, 0x41, 0xee, 0x9b, 0xe5, 0x38, 0x75, 0x81, 0xb0, 0xe8, 0x1e, 0xbb,
      0x67, 0x3d, 0x7a, 0x86, 0xda, 0x7f, 0x3c, 0x33,
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
      0x00, 0x00, 0x00, 0x38,
#else
      0x38, 0x00, 0x00, 0x00,
#endif
      0x62, 0x61, 0x7a, 0x2e, 0x6b, 0x65, 0x79, 0x00, 0x1f, 0xfa, 0x59, 0x74,
      0xcd, 0x23, 0x0c, 0x9b, 0x05, 0x51, 0xcf, 0xed, 0x26, 0xb0, 0x2c, 0xb9,
      0x18, 0x4c, 0x7a, 0x53, 0xb9, 0x2a, 0x11, 0x9d, 0xe2, 0x3a, 0x0d, 0x1c,
      0x18, 0x77, 0xc6, 0xf0, 0x8d, 0x69, 0x3c, 0x03, 0xc2, 0x00, 0x19, 0xbd,
      0x7a, 0xcd, 0x54, 0x21, 0xc8, 0x91, 0x90, 0x7d
});

// valid, empty masterkeyfile
static constexpr auto kMasterKeyfileInitialized =
    std::to_array<unsigned char>({0x4d, 0x52, 0x4b, 0x46, 0x00});

template <class T, size_t N>
std::ostream &operator<<(std::ostream &os, const std::array<T, N> &arr) {
  std::copy(arr.begin(), arr.end(), std::ostream_iterator<T>(os));
  return os;
}

static void run(const std::vector<std::string> &args,
                const std::string &stdin_content, std::string &out,
                std::string &err, int expected_exit_code = 0) {
  std::istringstream cin(stdin_content);
  std::ostringstream cout;
  std::ostringstream cerr;

  SCOPED_TRACE("// running "s + kAppExeFileName + " " +
               mysql_harness::join(args, " "));
  int exit_code = 0;
  try {
    exit_code = KeyringFrontend(kAppExeFileName, args, cin, cout, cerr).run();
  } catch (const FrontendError &e) {
    cerr << e.what() << std::endl;
    exit_code = EXIT_FAILURE;
  } catch (const std::exception &e) {
    FAIL() << e.what();
  }

  out = cout.str();
  err = cerr.str();

  // success, empty json-object and no error
  ASSERT_EQ(exit_code, expected_exit_code) << err;
}

void build_master_list_cmd_args(const std::vector<std::string> &args,
                                std::vector<std::string> &out_args) {
  out_args.push_back("master-list");

  bool copy_next{false};
  for (const auto &arg : args) {
    ASSERT_NE(arg, "--version");
    ASSERT_NE(arg, "--help");

    if (arg == "--master-key-file") {
      out_args.push_back(arg);

      copy_next = true;
    } else if (copy_next) {
      out_args.push_back(arg);
      copy_next = false;
    }
  }
}

void build_export_cmd_args(const std::vector<std::string> &args,
                           std::vector<std::string> &out_args) {
  out_args.push_back("export");

  size_t no_opt_arg{0};
  bool copy_next{false};
  for (const auto &arg : args) {
    ASSERT_NE(arg, "--version");
    ASSERT_NE(arg, "--help");

    if (arg == "--master-key-reader" || arg == "--master-key-writer" ||
        arg == "--master-key-file") {
      copy_next = true;
      out_args.push_back(arg);
    } else if (copy_next) {
      out_args.push_back(arg);
      copy_next = false;
    } else if (arg.substr(0, 2) == "--") {
      // do we have to capture that arg too?
      EXPECT_TRUE(false) << arg;
    } else {
      if (no_opt_arg == 1) {
        // only capture the keyfile, the first non-opt-arg (the one
        // after the cmd)
        out_args.push_back(arg);
      }
      ++no_opt_arg;
    }
  }
}
template <class T, size_t N>
static void create_file(const std::string &filename,
                        const std::array<T, N> &data) {
  std::fstream kr(filename, std::ios::out | std::ios::binary);
  ASSERT_TRUE(kr.is_open());
  kr << data;
  kr.close();
}

static void create_file(const std::string &filename, const std::string &data) {
  std::fstream kr(filename, std::ios::out | std::ios::binary);
  ASSERT_TRUE(kr.is_open());
  kr << data;
  kr.close();
}

static void create_file(const std::string &filename) {
  std::fstream kr(filename, std::ios::out | std::ios::binary);
  ASSERT_TRUE(kr.is_open());
  kr.close();
}

template <class T, size_t N>
static void create_private_file(const std::string &filename,
                                const std::array<T, N> &data) {
  create_file(filename, data);

  mysql_harness::make_file_private(filename);
}

template <class T, size_t N>
static void create_executable_file(std::string &filename,
                                   const std::array<T, N> &data) {
#ifdef _WIN32
  // append .bat to make the file "executable"
  filename.append(".bat");
#endif

  create_file(filename, data);
#ifndef _WIN32
  ::chmod(filename.c_str(), 0700);
#endif
}

static void create_executable_file(std::string &filename,
                                   const std::string &data) {
#ifdef _WIN32
  // append .bat to make the file "executable"
  filename.append(".bat");
#endif

  create_file(filename, data);

#ifndef _WIN32
  ::chmod(filename.c_str(), 0700);
#endif
}

static void create_private_file(const std::string &filename) {
  create_file(filename);

  mysql_harness::make_file_private(filename);
}

static void create_insecure_file(const std::string &filename) {
  create_file(filename);

  mysql_harness::make_file_public(filename);
}

/**
 * @test ensure PasswordFrontend behaves correctly.
 */
TEST_P(KeyringFrontendTest, ensure) {
  // record the test-scenario from the test-plan in the --gtest-output file
  RecordProperty("scenario", GetParam().test_scenario_id);

  TempDirectory tmpdir;

  // use spaces in names to test special characters all the time
  std::string keyring_filename(
      mysql_harness::Path(tmpdir.name()).join("keyring").str());
  std::string master_keyring_filename(
      mysql_harness::Path(tmpdir.name()).join("master_keyring").str());
  std::string master_key_reader(
      mysql_harness::Path(tmpdir.name()).join("master key reader").str());
  std::string master_key_writer(
      mysql_harness::Path(tmpdir.name()).join("master key writer").str());

  SCOPED_TRACE("// applying pre-conditions");
  switch (PreCond::KeyringFilename::from_bitset(GetParam().options)) {
    case PreCond::KeyringFilename::Values::none:
      break;
    case PreCond::KeyringFilename::Values::special_chars:
      keyring_filename =
          mysql_harness::Path(tmpdir.name()).join("Key ring").str();
      break;
    case PreCond::KeyringFilename::Values::with_directory: {
      mysql_harness::Path subdir(tmpdir.name());
      subdir = subdir.join("subdir");
      mysql_harness::mkdir(subdir.c_str(), 0700);
      keyring_filename = subdir.join("Key ring").str();
    } break;
    case PreCond::KeyringFilename::Values::with_no_exist_directory: {
      mysql_harness::Path subdir(tmpdir.name());
      subdir = subdir.join("subdir");
      keyring_filename = subdir.join("Key ring").str();
    } break;
    case PreCond::KeyringFilename::Values::absolute: {
      bool is_absolute{false};
#ifdef _WIN32
      // c:/
      is_absolute = tmpdir.name().at(1) == ':';
#else
      is_absolute = tmpdir.name().at(0) == '/';
#endif
      if (!is_absolute) {
        // current_path throws if something goes wrong.
        keyring_filename =
            mysql_harness::Path(stdx::filesystem::current_path().native())
                .join(tmpdir.name())
                .join("Key ring")
                .str();
      } else {
        keyring_filename =
            mysql_harness::Path(tmpdir.name()).join("Key ring").str();
      }
    } break;
  }

  switch (PreCond::MasterKeyfileFilename::from_bitset(GetParam().options)) {
    case PreCond::MasterKeyfileFilename::Values::none:
      break;
    case PreCond::MasterKeyfileFilename::Values::special_chars:
      master_keyring_filename =
          mysql_harness::Path(tmpdir.name()).join("master Key ring").str();
      break;
    case PreCond::MasterKeyfileFilename::Values::with_directory: {
      mysql_harness::Path subdir(tmpdir.name());
      subdir = subdir.join("subdir");
      mysql_harness::mkdir(subdir.c_str(), 0700);
      master_keyring_filename = subdir.join("master Key ring").str();
    } break;
    case PreCond::MasterKeyfileFilename::Values::with_no_exist_directory: {
      mysql_harness::Path subdir(tmpdir.name());
      subdir = subdir.join("subdir");
      master_keyring_filename = subdir.join("master Key ring").str();
    } break;
  }

  switch (PreCond::Keyring::from_bitset(GetParam().options)) {
    case PreCond::Keyring::Values::none:
      break;
    case PreCond::Keyring::Values::empty:
      create_private_file(keyring_filename);
      break;
    case PreCond::Keyring::Values::no_entries:
      create_private_file(keyring_filename, kKeyringNoEntry);
      break;
    case PreCond::Keyring::Values::one_entry:
      create_private_file(keyring_filename, kKeyringOneEntry);
      break;
    case PreCond::Keyring::Values::inited: {
      std::string out;
      std::string err;
      ASSERT_NO_FATAL_FAILURE(
          run({"init", keyring_filename, "--master-key-file",
               master_keyring_filename},
              "", out, err));
      ASSERT_EQ(out, "");
      ASSERT_EQ(err, "");
    } break;
    case PreCond::Keyring::Values::minimal:
      create_private_file(keyring_filename, kKeyringMinimal);
      break;
    case PreCond::Keyring::Values::one_user_one_property: {
      std::string out;
      std::string err;

      ASSERT_NO_FATAL_FAILURE(
          run({"init", keyring_filename, "--master-key-file",
               master_keyring_filename},
              "", out, err));
      ASSERT_EQ(out, "");
      ASSERT_EQ(err, "");

      std::vector<std::tuple<std::string, std::string, std::string>> props{
          std::make_tuple("a", "password", "foo"),
      };

      for (auto const &prop : props) {
        ASSERT_NO_FATAL_FAILURE(
            run({"set", keyring_filename, "--master-key-file",
                 master_keyring_filename, std::get<0>(prop), std::get<1>(prop),
                 std::get<2>(prop)},
                "", out, err));
        ASSERT_EQ(out, "");
        ASSERT_EQ(err, "");
      }

    } break;
    case PreCond::Keyring::Values::many_user_one_property: {
      std::string out;
      std::string err;

      ASSERT_NO_FATAL_FAILURE(
          run({"init", keyring_filename, "--master-key-file",
               master_keyring_filename},
              "", out, err));
      ASSERT_EQ(out, "");
      ASSERT_EQ(err, "");

      std::vector<std::tuple<std::string, std::string, std::string>> props{
          std::make_tuple("a", "password", "foo"),
          std::make_tuple("b", "password", "bar"),
          std::make_tuple("c", "password", "baz"),
          std::make_tuple("c", "Key1", "fuu"),
          std::make_tuple("c", "key1", "fuU"),
      };

      for (auto const &prop : props) {
        ASSERT_NO_FATAL_FAILURE(
            run({"set", keyring_filename, "--master-key-file",
                 master_keyring_filename, std::get<0>(prop), std::get<1>(prop),
                 std::get<2>(prop)},
                "", out, err));
        ASSERT_EQ(out, "");
        ASSERT_EQ(err, "");
      }

    } break;
    case PreCond::Keyring::Values::long_property: {
      std::string out;
      std::string err;

      ASSERT_NO_FATAL_FAILURE(
          run({"init", keyring_filename, "--master-key-file",
               master_keyring_filename},
              "", out, err));
      ASSERT_EQ(out, "");
      ASSERT_EQ(err, "");

      std::vector<std::tuple<std::string, std::string, std::string>> props{
          std::make_tuple("a", "password", "foo"),
          std::make_tuple("b", "password", "bar"),
          std::make_tuple("c", "password", "baz"),
          std::make_tuple("c", "Key1", "fuu"),
          std::make_tuple("c", "key1", "fuU"),
          std::make_tuple("long", "long", std::string(128 * 1024, 'a')),
      };

      for (auto const &prop : props) {
        ASSERT_NO_FATAL_FAILURE(
            run({"set", keyring_filename, "--master-key-file",
                 master_keyring_filename, std::get<0>(prop), std::get<1>(prop),
                 std::get<2>(prop)},
                "", out, err));
        ASSERT_EQ(out, "");
        ASSERT_EQ(err, "");
      }

    } break;
    case PreCond::Keyring::Values::long_username: {
      std::string out;
      std::string err;

      ASSERT_NO_FATAL_FAILURE(
          run({"init", keyring_filename, "--master-key-file",
               master_keyring_filename},
              "", out, err));
      ASSERT_EQ(out, "");
      ASSERT_EQ(err, "");

      std::vector<std::tuple<std::string, std::string, std::string>> props{
          std::make_tuple(std::string(128 * 1024, 'a'), "password", "foo"),
      };

      for (auto const &prop : props) {
        ASSERT_NO_FATAL_FAILURE(
            run({"set", keyring_filename, "--master-key-file",
                 master_keyring_filename, std::get<0>(prop), std::get<1>(prop),
                 std::get<2>(prop)},
                "", out, err));
        ASSERT_EQ(out, "");
        ASSERT_EQ(err, "");
      }

    } break;
    case PreCond::Keyring::Values::special_properties: {
      std::string out;
      std::string err;

      ASSERT_NO_FATAL_FAILURE(
          run({"init", keyring_filename, "--master-key-file",
               master_keyring_filename},
              "", out, err));
      ASSERT_EQ(out, "");
      ASSERT_EQ(err, "");

      std::vector<std::tuple<std::string, std::string, std::string>> props{
          std::make_tuple("A", "<", ">"),
          std::make_tuple("A", "\n", std::string("\0", 1)),
          std::make_tuple("A", "name", ""),
          std::make_tuple("B", "password", "bar"),
          std::make_tuple("{", "password", "baz"),
          std::make_tuple("\"", "Key1", "fuu"),
          std::make_tuple("\n", "key1", "fuU"),
          std::make_tuple("\r", "key1", "fuU"),
          std::make_tuple("\t", "key1", "fuU"),
          std::make_tuple(std::string("\0", 1), "key1", "fuU"),
          std::make_tuple("'", "key1", "fuU"),
          std::make_tuple("\"NULL\"", "key1", "fuU"),
      };

      for (auto const &prop : props) {
        ASSERT_NO_FATAL_FAILURE(
            run({"set", keyring_filename, "--master-key-file",
                 master_keyring_filename, std::get<0>(prop), std::get<1>(prop),
                 std::get<2>(prop)},
                "", out, err));
        ASSERT_EQ(out, "");
        ASSERT_EQ(err, "");
      }

    } break;
    default:
      ASSERT_TRUE(false);
  }

  switch (PreCond::MasterKeyfile::from_bitset(GetParam().options)) {
    case PreCond::MasterKeyfile::Values::none:
      break;
    case PreCond::MasterKeyfile::Values::empty:
      create_private_file(master_keyring_filename);
      break;
    case PreCond::MasterKeyfile::Values::insecure:
      create_insecure_file(master_keyring_filename);
      break;
    case PreCond::MasterKeyfile::Values::minimal:
      create_private_file(master_keyring_filename, kMasterKeyfileInitialized);
      break;
    case PreCond::MasterKeyfile::Values::valid_one_entry:
      create_private_file(master_keyring_filename, kMasterKeyfileOneEntry);
      break;
    case PreCond::MasterKeyfile::Values::valid_foo_bar_baz:
      create_private_file(master_keyring_filename, kMasterKeyfileFooBarBaz);
      break;
  }

  switch (PreCond::MasterKeyReader::from_bitset(GetParam().options)) {
    case PreCond::MasterKeyReader::Values::none:
      break;
    case PreCond::MasterKeyReader::Values::succeeding:
      // may modify master_key_reader
      create_executable_file(master_key_reader, kMasterKeyReaderSucceeding);
      break;
    case PreCond::MasterKeyReader::Values::key_not_found:
      // may modify master_key_reader
      create_executable_file(master_key_reader, kMasterKeyReaderKeyNotFound);
      break;
    case PreCond::MasterKeyReader::Values::failing:
      // may modify master_key_reader
      create_executable_file(master_key_reader, kMasterKeyReaderFailing);
      break;
    case PreCond::MasterKeyReader::Values::not_executable:
      create_file(master_key_reader, kMasterKeyReaderSucceeding);
      break;
  }

  switch (PreCond::MasterKeyWriter::from_bitset(GetParam().options)) {
    case PreCond::MasterKeyWriter::Values::none:
      break;
    case PreCond::MasterKeyWriter::Values::succeeding:
      // may modify master_key_writer
      create_executable_file(master_key_writer, kMasterKeyWriterSucceeding);
      break;
    case PreCond::MasterKeyWriter::Values::failing:
      // may modify master_key_writer
      create_executable_file(master_key_writer, kMasterKeyWriterFailing);
      break;
    case PreCond::MasterKeyWriter::Values::not_executable:
      create_file(master_key_writer, kMasterKeyWriterFailing);
      break;
  }

  // replace the placeholder with the name of the temp passwd-file
  std::vector<std::string> args{GetParam().cmdline_args};
  for (auto &arg : args) {
    if (arg == kKeyringPlaceholder) {
      arg = keyring_filename;
    } else if (arg == kMasterKeyfilePlaceholder) {
      arg = master_keyring_filename;
    } else if (arg == kMasterKeyReaderPlaceholder) {
      arg = master_key_reader;
    } else if (arg == kMasterKeyWriterPlaceholder) {
      arg = master_key_writer;
    }
  }

  SCOPED_TRACE("// running test");
  // do what keyring_cli.cc's main() does
  {
    std::istringstream cin(GetParam().stdin_content);
    std::ostringstream cout;
    std::ostringstream cerr;

    mysqlrouter::set_prompt_password([&cin](const std::string &) {
      std::string s;
      std::getline(cin, s);
      return s;
    });

    int exit_code = 0;
    try {
      exit_code = KeyringFrontend(kAppExeFileName, args, cin, cout, cerr).run();
    } catch (const FrontendError &e) {
      cerr << e.what() << std::endl;
      exit_code = EXIT_FAILURE;
    } catch (const std::exception &e) {
      FAIL() << e.what();
    }

    EXPECT_EQ(exit_code, GetParam().exit_code);
    EXPECT_EQ(GetParam().stdout_content, cout.str());
    EXPECT_THAT(cerr.str(),
                ::testing::ContainsRegex(GetParam().stderr_content));
  }

  SCOPED_TRACE("// checking post-conditions");
  switch (PostCond::Keyring::from_bitset(GetParam().options)) {
    case PostCond::Keyring::Values::none:
      break;
    case PostCond::Keyring::Values::not_exists:
      EXPECT_FALSE(mysql_harness::Path(keyring_filename).exists());

      break;
    case PostCond::Keyring::Values::exists_and_secure:
      EXPECT_TRUE(mysql_harness::Path(keyring_filename).exists());
      EXPECT_NO_THROW(
          mysql_harness::check_file_access_rights(keyring_filename));
      break;
  }

  switch (PostCond::KeyringExport::from_bitset(GetParam().options)) {
    case PostCond::KeyringExport::Values::none:
      break;
    case PostCond::KeyringExport::Values::empty_keys: {
      std::vector<std::string> export_args;
      build_export_cmd_args(args, export_args);

      std::string cout;
      std::string cerr;
      EXPECT_NO_FATAL_FAILURE(
          run(export_args, GetParam().stdin_content, cout, cerr));

      // success, empty json-object and no error
      EXPECT_EQ("{}\n"s, cout);
      EXPECT_EQ(""s, cerr);
    }

    break;
    case PostCond::KeyringExport::Values::user_a_password_stdin_value: {
      std::vector<std::string> export_args;
      build_export_cmd_args(args, export_args);

      std::string cout;
      std::string cerr;
      EXPECT_NO_FATAL_FAILURE(
          run(export_args, GetParam().stdin_content, cout, cerr));

      EXPECT_EQ(
          "{\n"
          "    \"a\": {\n"
          "        \"password\": \"" +
              GetParam().stdin_content +
              "\"\n"
              "    }\n"
              "}\n",
          cout);
      EXPECT_EQ(""s, cerr);
    } break;
    case PostCond::KeyringExport::Values::user_a_password_foo: {
      std::vector<std::string> export_args;
      build_export_cmd_args(args, export_args);

      std::string cout;
      std::string cerr;
      EXPECT_NO_FATAL_FAILURE(
          run(export_args, GetParam().stdin_content, cout, cerr));

      EXPECT_EQ(
          "{\n"
          "    \"a\": {\n"
          "        \"password\": \"foo\"\n"
          "    }\n"
          "}\n",
          cout);
      EXPECT_EQ(""s, cerr);
    } break;
    case PostCond::KeyringExport::Values::user_a_password_other: {
      std::vector<std::string> export_args;
      build_export_cmd_args(args, export_args);

      std::string cout;
      std::string cerr;
      EXPECT_NO_FATAL_FAILURE(
          run(export_args, GetParam().stdin_content, cout, cerr));

      EXPECT_EQ(
          "{\n"
          "    \"a\": {\n"
          "        \"password\": \"other\"\n"
          "    }\n"
          "}\n",
          cout);
      EXPECT_EQ(""s, cerr);
    } break;
    case PostCond::KeyringExport::Values::
        many_user_one_property_no_c_password: {
      std::vector<std::string> export_args;
      build_export_cmd_args(args, export_args);

      std::string cout;
      std::string cerr;
      EXPECT_NO_FATAL_FAILURE(
          run(export_args, GetParam().stdin_content, cout, cerr));

      EXPECT_EQ(
          "{\n"
          "    \"a\": {\n"
          "        \"password\": \"foo\"\n"
          "    },\n"
          "    \"b\": {\n"
          "        \"password\": \"bar\"\n"
          "    },\n"
          "    \"c\": {\n"
          "        \"Key1\": \"fuu\",\n"
          "        \"key1\": \"fuU\"\n"
          "    }\n"
          "}\n",
          cout);
      EXPECT_EQ(""s, cerr);
    } break;
    case PostCond::KeyringExport::Values::many_user_one_property_b_removed: {
      std::vector<std::string> export_args;
      build_export_cmd_args(args, export_args);

      std::string cout;
      std::string cerr;
      EXPECT_NO_FATAL_FAILURE(
          run(export_args, GetParam().stdin_content, cout, cerr));

      EXPECT_EQ(
          "{\n"
          "    \"a\": {\n"
          "        \"password\": \"foo\"\n"
          "    },\n"
          "    \"c\": {\n"
          "        \"Key1\": \"fuu\",\n"
          "        \"key1\": \"fuU\",\n"
          "        \"password\": \"baz\"\n"
          "    }\n"
          "}\n",
          cout);
      EXPECT_EQ(""s, cerr);
    } break;
    case PostCond::KeyringExport::Values::many_user_one_property: {
      std::vector<std::string> export_args;
      build_export_cmd_args(args, export_args);

      std::string cout;
      std::string cerr;
      EXPECT_NO_FATAL_FAILURE(
          run(export_args, GetParam().stdin_content, cout, cerr));

      EXPECT_EQ(
          "{\n"
          "    \"a\": {\n"
          "        \"password\": \"foo\"\n"
          "    },\n"
          "    \"b\": {\n"
          "        \"password\": \"bar\"\n"
          "    },\n"
          "    \"c\": {\n"
          "        \"Key1\": \"fuu\",\n"
          "        \"key1\": \"fuU\",\n"
          "        \"password\": \"baz\"\n"
          "    }\n"
          "}\n",
          cout);
      EXPECT_EQ(""s, cerr);
    } break;
  }

  switch (PostCond::MasterKeyfile::from_bitset(GetParam().options)) {
    case PostCond::MasterKeyfile::Values::none:
      break;
    case PostCond::MasterKeyfile::Values::exists:
      EXPECT_TRUE(mysql_harness::Path(master_keyring_filename).exists());
      break;
    case PostCond::MasterKeyfile::Values::not_exists:
      EXPECT_FALSE(mysql_harness::Path(master_keyring_filename).exists());
      break;
    case PostCond::MasterKeyfile::Values::exists_and_secure:
      EXPECT_TRUE(mysql_harness::Path(master_keyring_filename).exists());

      EXPECT_NO_THROW(
          mysql_harness::check_file_access_rights(master_keyring_filename));
      break;
  }

  switch (PostCond::MasterList::from_bitset(GetParam().options)) {
    case PostCond::MasterList::Values::none:
      break;
    case PostCond::MasterList::Values::empty: {
      std::vector<std::string> cmd_args;
      ASSERT_NO_FATAL_FAILURE(build_master_list_cmd_args(args, cmd_args));

      std::string cout;
      std::string cerr;
      EXPECT_NO_FATAL_FAILURE(
          run(cmd_args, GetParam().stdin_content, cout, cerr));

      EXPECT_EQ(""s, cout);
      EXPECT_EQ(""s, cerr);
    } break;
    case PostCond::MasterList::Values::one_entry: {
      std::vector<std::string> cmd_args;
      ASSERT_NO_FATAL_FAILURE(build_master_list_cmd_args(args, cmd_args));

      std::string cout;
      std::string cerr;
      EXPECT_NO_FATAL_FAILURE(
          run(cmd_args, GetParam().stdin_content, cout, cerr));

      EXPECT_EQ("foo.key\n"s, cout);
      EXPECT_EQ(""s, cerr);
    } break;
    case PostCond::MasterList::Values::contains_keyfile: {
      std::vector<std::string> cmd_args;
      ASSERT_NO_FATAL_FAILURE(build_master_list_cmd_args(args, cmd_args));

      std::string cout;
      std::string cerr;
      EXPECT_NO_FATAL_FAILURE(
          run(cmd_args, GetParam().stdin_content, cout, cerr));

      EXPECT_THAT(mysql_harness::split_string(cout, '\n'),
                  ::testing::Contains(keyring_filename));
      EXPECT_EQ(""s, cerr);
    } break;
    case PostCond::MasterList::Values::contains_keyfile_and_one_more: {
      std::vector<std::string> cmd_args;
      ASSERT_NO_FATAL_FAILURE(build_master_list_cmd_args(args, cmd_args));

      std::string cout;
      std::string cerr;
      EXPECT_NO_FATAL_FAILURE(
          run(cmd_args, GetParam().stdin_content, cout, cerr));

      EXPECT_THAT(mysql_harness::split_string(cout, '\n'),
                  ::testing::AllOf(::testing::Contains(keyring_filename),
                                   ::testing::Contains("foo.key")));
      EXPECT_EQ(""s, cerr);
    } break;
    case PostCond::MasterList::Values::bar_baz: {
      std::vector<std::string> cmd_args;
      ASSERT_NO_FATAL_FAILURE(build_master_list_cmd_args(args, cmd_args));

      std::string cout;
      std::string cerr;
      EXPECT_NO_FATAL_FAILURE(
          run(cmd_args, GetParam().stdin_content, cout, cerr));

      EXPECT_THAT(mysql_harness::split_string(cout, '\n'),
                  ::testing::AllOf(::testing::Contains("bar.key"),
                                   ::testing::Contains("baz.key")));
      EXPECT_EQ(""s, cerr);
    } break;
  }
}

// cleanup test-names to satisfy googletest's requirements
const KeyringFrontendTestParam password_frontend_param[]{
    {"dashdash_help",
     "WL12974::TS_H_1",
     {"--help"},
     EXIT_SUCCESS,
     "",
     kHelpText + "\n",
     "",
     PreCond::Keyring::none()},

    {"dash_questionmark",
     "WL12974::TS_H_2",
     {"-?"},
     EXIT_SUCCESS,
     "",
     kHelpText + "\n",
     "^$",
     PreCond::Keyring::none()},

    {"dash_questionmark_and_dashdash_help",
     "WL12974::TS_H_4",
     {"-?", "--help"},
     EXIT_SUCCESS,
     "",
     kHelpText + "\n",
     "^$",
     PreCond::Keyring::none()},

    {"dashdash_version",
     "WL12974::TS_V_1",
     {"--version"},
     EXIT_SUCCESS,
     "",
     kVersionText + "\n",
     "^$",
     PreCond::Keyring::none()},

    {"dash_V",
     "WL12974::TS_V_2",
     {"-V"},
     EXIT_SUCCESS,
     "",
     kVersionText + "\n",
     "^$",
     PreCond::Keyring::none()},

    {"dash_V_and_dash_questionmark",
     "",
     {"-V", "-?"},
     EXIT_SUCCESS,
     "",
     kVersionText + "\n",
     "^$",
     PreCond::Keyring::none()},

    {"dash_questionmark_and_dash_V",
     "",
     {"-?", "-V"},
     EXIT_SUCCESS,
     "",
     kHelpText + "\n",
     "^$",
     PreCond::Keyring::none()},

    {"dash_version_and_dash_v",
     "WL12974::TS_V_4",
     {"-V", "--version"},
     EXIT_SUCCESS,
     "",
     kVersionText + "\n",
     "^$",
     PreCond::Keyring::none()},

    {"dashdash_version_and_unknown_options",
     "WL12974::TS_AS_1",
     {"--version", "--unknown-option"},
     EXIT_FAILURE,
     "",
     "",
     "unknown option '--unknown-option'",
     PreCond::Keyring::none()},

    {"dashdash_version_and_unknown_command",
     "",
     {"unknown-command", "--version"},
     EXIT_FAILURE,
     "",
     "",
     "expected no extra arguments",
     PreCond::Keyring::none()},

    {"list_master_key_writer_empty",
     "",
     {"list", "keyring", "--master-key-writer", ""},
     EXIT_FAILURE,
     "",
     "",
     "^expected --master-key-writer to be not empty",
     PreCond::Keyring::none()},

    {"list_master_key_reader_empty",
     "",
     {"list", "keyring", "--master-key-reader", ""},
     EXIT_FAILURE,
     "",
     "",
     "^expected --master-key-reader to be not empty",
     PreCond::Keyring::none()},

    {"list_master_key_file_empty",
     "",
     {"list", "keyring", "--master-key-file", ""},
     EXIT_FAILURE,
     "",
     "",
     "^expected --master-key-file to be not empty",
     PreCond::Keyring::none()},

    {
        "init_create_keyring",
        "WL12974::TS_FR6_1",
        {
            "init",
            kKeyringPlaceholder,
            "--master-key-file",
            kMasterKeyfilePlaceholder,
        },
        EXIT_SUCCESS,
        "",
        "",
        "^$",
        PreCond::Keyring::none() | PreCond::MasterKeyfile::none() |
            PostCond::Keyring::exists_and_secure() |
            PostCond::MasterKeyfile::exists_and_secure() |
            PostCond::MasterList::contains_keyfile(),
    },

    {"init_create_keyring_subdir_no_exist",
     "WL12974::TS_FR6_1",
     {
         "init",
         kKeyringPlaceholder,
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_FAILURE,
     "",
     "",
     "^failed saving keyring: Failed to open keyring file for writing: .*",
     PreCond::Keyring::none() | PreCond::MasterKeyfile::none() |
         PreCond::KeyringFilename::with_no_exist_directory() |
         PostCond::Keyring::not_exists()},

    {"init_create_keyring_masterkeyfile_subdir_no_exist",
     "WL12974::TS_FR6_1",
     {
         "init",
         kKeyringPlaceholder,
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_FAILURE,
     "",
     "",
     "^failed saving master-key-file: Could not open master key file",
     PreCond::Keyring::none() | PreCond::MasterKeyfile::none() |
         PreCond::MasterKeyfileFilename::with_no_exist_directory() |
         PostCond::Keyring::not_exists()},

    {"init_update_keyring_create_master_keyfile",
     "WL12974::TS_FR6_2",
     {
         "init",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
         kKeyringPlaceholder,
     },
     EXIT_SUCCESS,
     "",
     "",
     "^$",
     PreCond::Keyring::minimal() | PreCond::MasterKeyfile::none() |
         PostCond::Keyring::exists_and_secure() |
         PostCond::MasterKeyfile::exists_and_secure()},

    // TS_FR6_3 tested by routertest_component_bootstrap

    {"init_keyring_with_master_reader",
     "WL12974::TS_FR6_4",
     {
         "init",
         kKeyringPlaceholder,
         "--master-key-reader",
         kMasterKeyReaderPlaceholder,
         "--master-key-writer",
         kMasterKeyWriterPlaceholder,
     },
     EXIT_SUCCESS,
     "",
     "",
     "^$",
     PreCond::Keyring::none() | PreCond::MasterKeyfile::none() |
         PreCond::MasterKeyReader::succeeding() |
         PreCond::MasterKeyWriter::succeeding() |
         PostCond::Keyring::exists_and_secure() |
         PostCond::KeyringExport::empty_keys() |
         PostCond::MasterKeyfile::not_exists()},

    {"init_without_keyring_with_master_reader",
     "WL12974::TS_FR6_5",
     {
         "init",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_FAILURE,
     "",
     "",
     "expected .*<filename>, got ",
     PreCond::Keyring::none() | PreCond::MasterKeyfile::minimal() |
         PostCond::Keyring::not_exists() |
         PostCond::MasterKeyfile::exists_and_secure()},

    // TS_FR6_6 (same as TS_FR6_1)

    // TS_FR7_1 tested by TS_FR6_1
    // TS_FR8_1 tested by TS_FR6_1

    {
        "init_create_keyring_with_existing_master_key_file_with_one_entry",
        "WL12974::TS_FR8_2",
        {
            "init",
            kKeyringPlaceholder,
            "--master-key-file",
            kMasterKeyfilePlaceholder,
        },
        EXIT_SUCCESS,
        "",
        "",
        "^$",
        PreCond::Keyring::none() | PreCond::MasterKeyfile::valid_one_entry() |
            PostCond::Keyring::exists_and_secure() |
            PostCond::MasterKeyfile::exists_and_secure() |
            PostCond::MasterList::contains_keyfile(),
    },

    // TS_FR8_3 tested by routertest_component_bootstrap

    {
        "init_create_keyring_with_invalid_master_key_file",
        "WL12974::TS_FR8_4",
        {
            "init",
            kKeyringPlaceholder,
            "--master-key-file",
            kMasterKeyfilePlaceholder,
        },
        EXIT_FAILURE,
        "",
        "",
        "opening master-key-file failed: Master key file '.*' has invalid "
        "file signature",
        PreCond::Keyring::none() | PreCond::MasterKeyfile::empty() |
            PostCond::Keyring::not_exists() |
            PostCond::MasterKeyfile::exists_and_secure(),
    },

    {"init_one_entry_keyring_without_master_key_file",
     "Bug#29949336",
     {
         "init",
         kKeyringPlaceholder,
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_FAILURE,
     "",
     "",
     "^keyfile '.*' already exists and has entries",
     PreCond::Keyring::one_entry() | PreCond::MasterKeyfile::none() |
         PostCond::MasterKeyfile::not_exists()},

    {"init_no_entry_keyring_without_master_key_file",
     "Bug#29949336",
     {
         "init",
         kKeyringPlaceholder,
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_SUCCESS,
     "",
     "",
     "^$",
     PreCond::Keyring::no_entries() | PreCond::MasterKeyfile::none() |
         PostCond::MasterKeyfile::exists_and_secure()},

    {
        "init_create_keyring_with_insecure_master_key_file",
        "WL12974::TS_FR8_4",
        {
            "init",
            kKeyringPlaceholder,
            "--master-key-file",
            kMasterKeyfilePlaceholder,
        },
        EXIT_FAILURE,
        "",
        "",
        "^opening master-key-file failed: '.*' has insecure permissions"
        ": " +
            make_error_code(std::errc::permission_denied).message(),
        PreCond::Keyring::none() | PreCond::MasterKeyfile::insecure() |
            PostCond::Keyring::not_exists() | PostCond::MasterKeyfile::exists(),
    },

    // Expectation of TS_FR8_6 is invalid:
    //
    // - "init" creates keyring if it doesn't exist.

    {
        "init_create_keyring_with_master_key_writer",
        "WL12974::TS_FR9_1",
        {
            "init",
            kKeyringPlaceholder,
            "--master-key-writer",
            kMasterKeyWriterPlaceholder,
            "--master-key-reader",
            kMasterKeyReaderPlaceholder,
        },
        EXIT_SUCCESS,
        "",
        "",
        "^$",
        PreCond::Keyring::none() | PreCond::MasterKeyfile::none() |
            PreCond::MasterKeyReader::succeeding() |
            PreCond::MasterKeyWriter::succeeding() |
            PostCond::Keyring::exists_and_secure(),
    },

    {
        "init_update_broken_keyring_master_with_key_writer",
        "WL12974::TS_FR9_2",
        {
            "init",
            kKeyringPlaceholder,
            "--master-key-writer",
            kMasterKeyWriterPlaceholder,
            "--master-key-reader",
            kMasterKeyReaderPlaceholder,
        },
        EXIT_FAILURE,
        "",
        "",
        "reading file-header of '.*' failed: File is too small",
        PreCond::Keyring::empty() | PreCond::MasterKeyfile::none() |
            PreCond::MasterKeyReader::succeeding() |
            PreCond::MasterKeyWriter::succeeding() |
            PostCond::Keyring::exists_and_secure() |
            PostCond::MasterKeyfile::not_exists(),
    },

    {
        "init_with_empty_keyring_filename",
        "",
        {
            "init",
            "",
            "--master-key-file",
            kMasterKeyfilePlaceholder,
        },
        EXIT_FAILURE,
        "",
        "",
        "^expected <keyring> to be not empty",
        PreCond::MasterKeyfile::none() | PostCond::Keyring::not_exists() |
            PostCond::MasterKeyfile::not_exists(),
    },

    {
        "init_with_subdirs",
        "WL12974::TS_IN_2",
        {
            "init",
            kKeyringPlaceholder,
            "--master-key-file",
            kMasterKeyfilePlaceholder,
        },
        EXIT_SUCCESS,
        "",
        "",
        "^$",
        PreCond::KeyringFilename::with_directory() | PreCond::Keyring::none() |
            PreCond::MasterKeyfileFilename::with_directory() |
            PreCond::MasterKeyfile::none() |
            PostCond::Keyring::exists_and_secure() |
            PostCond::MasterKeyfile::exists_and_secure() |
            PostCond::MasterList::contains_keyfile(),
    },

    {
        "list_broken_master_key_reader",
        "WL12974::TS_FR10_xxx",
        {
            "list",
            kKeyringPlaceholder,
            "--master-key-writer",
            kMasterKeyWriterPlaceholder,
            "--master-key-reader",
            kMasterKeyReaderPlaceholder,
        },
        EXIT_FAILURE,
        "",
        "",
        "failed reading master-key for '.*' from master-key-reader '.*'",
        PreCond::Keyring::minimal() | PreCond::MasterKeyfile::none() |
            PreCond::MasterKeyReader::not_executable() |
            PostCond::Keyring::exists_and_secure() |
            PostCond::MasterKeyfile::not_exists(),
    },

    {
        "list_insecure_master_key_file",
        "WL12974::TS_FR10_1",
        {
            "list",
            kKeyringPlaceholder,
            "--master-key-file",
            kMasterKeyfilePlaceholder,
        },
        EXIT_FAILURE,
        "",
        "",
        "^opening master-key-file failed: '.*' has insecure permissions.",
        PreCond::Keyring::minimal() | PreCond::MasterKeyfile::insecure() |
            PostCond::Keyring::exists_and_secure() |
            PostCond::MasterKeyfile::exists(),
    },

    {
        "list_broken_master_key_file",
        "WL12974::TS_FR10_2",
        {
            "list",
            kKeyringPlaceholder,
            "--master-key-file",
            kMasterKeyfilePlaceholder,
        },
        EXIT_FAILURE,
        "",
        "",
        "^opening master-key-file failed: Master key file '.*' has invalid "
        "file signature",
        PreCond::Keyring::minimal() | PreCond::MasterKeyfile::empty() |
            PostCond::Keyring::exists_and_secure() |
            PostCond::MasterKeyfile::exists_and_secure(),
    },

    {"list_multiple_users_with_master_key_file",
     "WL12974::TS_FR11_1",
     {
         "list",
         kKeyringPlaceholder,
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_SUCCESS,
     "",
     "a\nb\nc\n",
     "^$",
     PreCond::Keyring::many_user_one_property() |
         PreCond::MasterKeyfile::none() |
         PostCond::Keyring::exists_and_secure()},

    {"list_properties_of_user_with_master_key_file",
     "WL12974::TS_FR12_1",
     {
         "list",
         kKeyringPlaceholder,
         "c",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_SUCCESS,
     "",
     "Key1\nkey1\npassword\n",
     "^$",
     PreCond::Keyring::many_user_one_property() |
         PreCond::MasterKeyfile::none() |
         PostCond::Keyring::exists_and_secure()},

    {"list_unknown_user_with_master_key_file",
     "WL12974::TS_FR13_1",
     {
         "list",
         kKeyringPlaceholder,
         "d",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_FAILURE,
     "",
     "",
     "^$",
     PreCond::Keyring::many_user_one_property() |
         PreCond::MasterKeyfile::none() |
         PostCond::Keyring::exists_and_secure()},

    {"list_unknown_property_with_master_key_file",
     "WL12974::TS_FR13_1",
     {
         "list",
         kKeyringPlaceholder,
         "d",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_FAILURE,
     "",
     "",
     "^$",
     PreCond::Keyring::many_user_one_property() |
         PreCond::MasterKeyfile::none() |
         PostCond::Keyring::exists_and_secure()},

    {"list_long_username_with_master_key_file",
     "WL12974::TS_LI_1",
     {
         "list",
         kKeyringPlaceholder,
         std::string(128 * 1024, 'a'),
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_SUCCESS,
     "",
     "password\n",
     "^$",
     PreCond::Keyring::long_username() |
         PostCond::MasterKeyfile::exists_and_secure() |
         PostCond::Keyring::exists_and_secure()},

    {"get_property_of_user_with_master_key_file",
     "WL12974::TS_FR14_1",
     {
         "get",
         kKeyringPlaceholder,
         "c",
         "password",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_SUCCESS,
     "",
     "baz\n",
     "^$",
     PreCond::Keyring::many_user_one_property() |
         PreCond::MasterKeyfile::none() |
         PostCond::Keyring::exists_and_secure()},

    {"get_long_property_of_user_with_master_key_file",
     "WL12974::TS_FR14_2",  // and TS_FR18_2
     {
         "get",
         kKeyringPlaceholder,
         "long",
         "long",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_SUCCESS,
     "",
     std::string(128 * 1024, 'a') + "\n",
     "^$",
     PreCond::Keyring::long_property() | PreCond::MasterKeyfile::none() |
         PostCond::Keyring::exists_and_secure()},

    {"get_unknown_property_of_user_with_master_key_file",
     "WL12974::TS_FR15_1",
     {
         "get",
         kKeyringPlaceholder,
         "long",
         "unknown",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_FAILURE,
     "",
     "",
     "^'unknown' not found for user 'long'",
     PreCond::Keyring::long_property() | PreCond::MasterKeyfile::none() |
         PostCond::Keyring::exists_and_secure()},

    {"get_unknown_user_with_master_key_file",
     "WL12974::TS_FR16_1",
     {
         "get",
         kKeyringPlaceholder,
         "unknown",
         "unknown",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_FAILURE,
     "",
     "",
     "^'unknown' not found for user 'unknown'",
     PreCond::Keyring::many_user_one_property() |
         PreCond::MasterKeyfile::none() |
         PostCond::Keyring::exists_and_secure()},

    {"get_property_of_user_with_master_key_file_and_broken_keyfile",
     "WL12974::TS_GE_1",
     {
         "get",
         kKeyringPlaceholder,
         "c",
         "password",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_FAILURE,
     "",
     "",
     "^opening keyring failed: reading file-header of '.*' failed: File is "
     "too "
     "small",
     PreCond::Keyring::empty() | PreCond::MasterKeyfile::none() |
         PostCond::Keyring::exists_and_secure()},

    {"export_with_master_key_file",
     "WL12974::TS_FR17_1",
     {
         "export",
         kKeyringPlaceholder,
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_SUCCESS,
     "",
     "{\n"
     "    \"\\u0000\": {\n"
     "        \"key1\": \"fuU\"\n"
     "    },\n"
     "    \"\\t\": {\n"
     "        \"key1\": \"fuU\"\n"
     "    },\n"
     "    \"\\n\": {\n"
     "        \"key1\": \"fuU\"\n"
     "    },\n"
     "    \"\\r\": {\n"
     "        \"key1\": \"fuU\"\n"
     "    },\n"
     "    \"\\\"\": {\n"
     "        \"Key1\": \"fuu\"\n"
     "    },\n"
     "    \"\\\"NULL\\\"\": {\n"
     "        \"key1\": \"fuU\"\n"
     "    },\n"
     "    \"'\": {\n"
     "        \"key1\": \"fuU\"\n"
     "    },\n"
     "    \"A\": {\n"
     "        \"\\n\": \"\\u0000\",\n"
     "        \"<\": \">\",\n"
     "        \"name\": \"\"\n"
     "    },\n"
     "    \"B\": {\n"
     "        \"password\": \"bar\"\n"
     "    },\n"
     "    \"{\": {\n"
     "        \"password\": \"baz\"\n"
     "    }\n"
     "}\n",
     "^$",
     PreCond::Keyring::special_properties() | PreCond::MasterKeyfile::none() |
         PostCond::Keyring::exists_and_secure()},

    {"export_with_broken_keyring_and_master_key_file",
     "WL12974::TS_EX_1",
     {
         "export",
         kKeyringPlaceholder,
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_FAILURE,
     "",
     "",
     "^opening keyring failed: reading file-header of '.*' failed: File is "
     "too "
     "small",
     PreCond::Keyring::empty() | PreCond::MasterKeyfile::none() |
         PostCond::Keyring::exists_and_secure()},

    {"set_with_master_key_file",
     "WL12974::TS_FR18_1",
     {
         // set is tested but preparing the right keyring
         "export",
         kKeyringPlaceholder,
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_SUCCESS,
     "",
     "{\n"
     "    \"a\": {\n"
     "        \"password\": \"foo\"\n"
     "    },\n"
     "    \"b\": {\n"
     "        \"password\": \"bar\"\n"
     "    },\n"
     "    \"c\": {\n"
     "        \"Key1\": \"fuu\",\n"
     "        \"key1\": \"fuU\",\n"
     "        \"password\": \"baz\"\n"
     "    }\n"
     "}\n",

     "^$",
     PreCond::Keyring::many_user_one_property() |
         PreCond::MasterKeyfile::none() |
         PostCond::Keyring::exists_and_secure()},

    {"set_with_value_from_stdin_master_key_file",
     "WL12974::TS_FR18_3",
     {
         "set",
         kKeyringPlaceholder,
         "a",
         "password",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_SUCCESS,
     "somevalue",
     "",
     "^$",
     PreCond::Keyring::inited() | PostCond::Keyring::exists_and_secure() |
         PostCond::KeyringExport::user_a_password_stdin_value()},

    {"set_with_empty_value_from_stdin_master_key_file",
     "WL12974::TS_FR18_4",
     {
         "set",
         kKeyringPlaceholder,
         "a",
         "password",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_SUCCESS,
     "",
     "",
     "^$",
     PreCond::Keyring::inited() | PostCond::Keyring::exists_and_secure() |
         PostCond::KeyringExport::user_a_password_stdin_value()},

    {"set_same_with_master_key_file",
     "WL12974::TS_FR18_5",
     {
         "set",
         kKeyringPlaceholder,
         "a",
         "password",
         "foo",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_SUCCESS,
     "",
     "",
     "^$",
     PreCond::Keyring::one_user_one_property() |
         PostCond::Keyring::exists_and_secure() |
         PostCond::KeyringExport::user_a_password_foo()},

    {"set_other_password_with_master_key_file",
     "WL12974::TS_FR18_6",
     {
         "set",
         kKeyringPlaceholder,
         "a",
         "password",
         "other",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_SUCCESS,
     "",
     "",
     "^$",
     PreCond::Keyring::one_user_one_property() |
         PostCond::Keyring::exists_and_secure() |
         PostCond::KeyringExport::user_a_password_other()},

    {"set_value_in_empty_keyring_with_master_key_file",
     "WL12974::TS_FR18_7",
     {
         "set",
         kKeyringPlaceholder,
         "a",
         "password",
         "other",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_SUCCESS,
     "",
     "",
     "^$",
     PreCond::Keyring::inited() | PostCond::Keyring::exists_and_secure() |
         PostCond::KeyringExport::user_a_password_other()},

    // TS_SE_1 can't be implemented
    // TS_SE_2 can't be implemented

    {"delete_value_with_master_key_file",
     "WL12974::TS_FR19_1",
     {
         "delete",
         kKeyringPlaceholder,
         "c",
         "password",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_SUCCESS,
     "",
     "",
     "^$",
     PreCond::Keyring::many_user_one_property() |
         PostCond::Keyring::exists_and_secure() |
         PostCond::KeyringExport::many_user_one_property_no_c_password()},

    {"delete_value_empty_prop_with_master_key_file",
     "WL12974::TS_FR19_2",
     {
         "delete",
         kKeyringPlaceholder,
         "b",
         "password",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_SUCCESS,
     "",
     "",
     "^$",
     PreCond::Keyring::many_user_one_property() |
         PostCond::Keyring::exists_and_secure() |
         PostCond::KeyringExport::many_user_one_property_b_removed()},

    {"delete_unknown_user_with_property_with_master_key_file",
     "WL12974::TS_FR20_1",
     {
         "delete",
         kKeyringPlaceholder,
         "unknown",
         "password",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_FAILURE,
     "",
     "",
     "^$",
     PreCond::Keyring::many_user_one_property() |
         PostCond::Keyring::exists_and_secure() |
         PostCond::KeyringExport::many_user_one_property()},

    {"delete_unknown_prop_with_master_key_file",
     "WL12974::TS_FR20_2",
     {
         "delete",
         kKeyringPlaceholder,
         "a",
         "unknown",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_FAILURE,
     "",
     "",
     "^$",
     PreCond::Keyring::many_user_one_property() |
         PostCond::Keyring::exists_and_secure() |
         PostCond::KeyringExport::many_user_one_property()},

    {"delete_user_from_one_entry_keyring_with_master_key_file",
     "WL12974::TS_FR21_1",
     {
         "delete",
         kKeyringPlaceholder,
         "a",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_SUCCESS,
     "",
     "",
     "^$",
     PreCond::Keyring::one_user_one_property() |
         PostCond::Keyring::exists_and_secure() |
         PostCond::KeyringExport::empty_keys()},

    {"delete_user_from_many_entry_keyring_with_master_key_file",
     "WL12974::TS_FR21_2",
     {
         "delete",
         kKeyringPlaceholder,
         "b",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_SUCCESS,
     "",
     "",
     "^$",
     PreCond::Keyring::many_user_one_property() |
         PostCond::Keyring::exists_and_secure() |
         PostCond::KeyringExport::many_user_one_property_b_removed()},

    // TS_FR21_3 is implicitly tested by all
    {"delete_unknown_user_with_master_key_file",
     "WL12974::TS_FR21_2",
     {
         "delete",
         kKeyringPlaceholder,
         "unknown",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_FAILURE,
     "",
     "",
     "^$",
     PreCond::Keyring::many_user_one_property() |
         PostCond::Keyring::exists_and_secure() |
         PostCond::KeyringExport::many_user_one_property()},

    {"delete_unknown_user_empty_keyring_with_master_key_file",
     "WL12974::TS_FR22_1",
     {
         "delete",
         kKeyringPlaceholder,
         "unknown",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_FAILURE,
     "",
     "",
     "^$",
     PreCond::Keyring::inited() | PostCond::Keyring::exists_and_secure() |
         PostCond::KeyringExport::empty_keys()},

    {"delete_unknown_user_one_entry_keyring_with_master_key_file",
     "WL12974::TS_FR22_2",
     {
         "delete",
         kKeyringPlaceholder,
         "unknown",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_FAILURE,
     "",
     "",
     "^$",
     PreCond::Keyring::one_user_one_property() |
         PostCond::Keyring::exists_and_secure() |
         PostCond::KeyringExport::user_a_password_foo()},

    {"master_list_with_subdir_keyring",
     "WL12974::TS_FR23_1",
     {
         "init",
         kKeyringPlaceholder,
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_SUCCESS,
     "",
     "",
     "^$",
     PreCond::KeyringFilename::with_directory() |
         PreCond::MasterKeyfile::none() |
         PostCond::MasterList::contains_keyfile()},

    {"master_list_with_two_entry_master_key_file",
     "WL12974::TS_FR23_2",
     {
         "init",
         kKeyringPlaceholder,
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_SUCCESS,
     "",
     "",
     "^$",
     PreCond::MasterKeyfile::valid_one_entry() |
         PostCond::MasterList::contains_keyfile_and_one_more()},

    {"master_list_with_two_entry_master_key_file_no_keyrings",
     "WL12974::TS_FR23_3",
     {
         "master-list",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_SUCCESS,
     "",
     "foo.key\n",
     "^$",
     PreCond::MasterKeyfile::valid_one_entry()},

    {"master_list_with_empty_master_key_file",
     "WL12974::TS_FR23_4",
     {
         "master-list",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_SUCCESS,
     "",
     "",
     "^$",
     PreCond::MasterKeyfile::minimal()},

    {"master_list_with_broken_master_key_file",
     "WL12974::TS_MKL_1",
     {
         "master-list",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_FAILURE,
     "",
     "",
     "^opening master-key-file failed: Master key file '.*' has invalid file "
     "signature",
     PreCond::MasterKeyfile::empty()},

    {"master_delete_with_master_key_file",
     "WL12974::TS_FR24_1",
     {
         "master-delete",
         "foo.key",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_SUCCESS,
     "",
     "",
     "^$",
     PreCond::MasterKeyfile::valid_one_entry() | PostCond::MasterList::none()},

    {"master_delete_with_empty_master_key_file",
     "WL12974::TS_FR24_2",
     {
         "master-delete",
         "foo.key",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_FAILURE,
     "",
     "",
     "^Keyring '.*' not found in master-key-file '.*'",
     PreCond::MasterKeyfile::minimal() | PostCond::MasterList::none()},

    {"master_delete_from_many_with_master_key_file",
     "WL12974::TS_FR24_3",
     {
         "master-delete",
         "foo.key",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_SUCCESS,
     "",
     "",
     "^$",
     PreCond::MasterKeyfile::valid_foo_bar_baz() |
         PostCond::MasterList::bar_baz()},

    {"master_delete_with_broken_master_key_file",
     "WL12974::TS_FR24_4",
     {
         "master-delete",
         "foo.key",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_FAILURE,
     "",
     "",
     "^opening master-key-file failed: Master key file '.*' has invalid file "
     "signature",
     PreCond::MasterKeyfile::empty() | PostCond::MasterList::none()},

    {"master_delete_unknown_with_master_key_file",
     "WL12974::TS_FR24_5",
     {
         "master-delete",
         "unknown",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_FAILURE,
     "",
     "",
     "^Keyring 'unknown' not found in master-key-file '.*'",
     PreCond::MasterKeyfile::valid_one_entry()},

    {"master_delete_absolute_path_with_master_key_file",
     "WL12974::TS_FR24_6",
     {
         "master-delete",
         kKeyringPlaceholder,
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_SUCCESS,
     "",
     "",
     "^$",
     PreCond::KeyringFilename::absolute() | PreCond::Keyring::inited() |
         PostCond::MasterList::empty()},

    {"master_delete_missing_keyring_with_master_key_file",
     "WL12974::TS_FR24_7",
     {
         "master-delete",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_FAILURE,
     "",
     "",
     "^expected .*<filename>.*, got",
     PreCond::MasterKeyfile::valid_one_entry()},

    {"master_delete_keyring_with_master_key_reader",
     "WL12974::TS_FR24_8",
     {
         "master-delete",
         "foo.key",
         "--master-key-reader",
         kMasterKeyReaderPlaceholder,
         "--master-key-writer",
         kMasterKeyWriterPlaceholder,
     },
     EXIT_FAILURE,
     "",
     "",
     "^expected --master-key-file to be not empty",
     PreCond::MasterKeyfile::valid_one_entry()},

    {"master_rename",
     "WL12974::TS_FR25_1",
     {
         "master-rename",
         kKeyringPlaceholder,
         "foo.key",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_SUCCESS,
     "",
     "",
     "^$",
     PreCond::Keyring::inited() | PostCond::MasterList::one_entry() |
         PostCond::MasterKeyfile::exists_and_secure()},

    {"master_rename_keyring_not_exists",
     "WL12974::TS_FR25_2",
     {
         "master-rename",
         "foo.key",
         kKeyringPlaceholder,
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_SUCCESS,
     "",
     "",
     "^$",
     PreCond::MasterKeyfile::valid_one_entry() |
         PostCond::MasterList::contains_keyfile() |
         PostCond::MasterKeyfile::exists_and_secure()},

    {"master_rename_0_char",
     "WL12974::TS_FR25_2.2",
     {
         "master-rename",
         "foo.key",
         std::string("\0", 1),
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_FAILURE,
     "",
     "",
     "^expected <new-key> to contain only printable characters",
     PreCond::MasterKeyfile::valid_one_entry() |
         PostCond::MasterList::one_entry() |
         PostCond::MasterKeyfile::exists_and_secure()},

    {"master_rename_empty_new_key",
     "WL12974::TS_FR25_2.3",
     {
         "master-rename",
         "foo.key",
         "",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_FAILURE,
     "",
     "",
     "^expected <new-key> to be not empty",
     PreCond::MasterKeyfile::valid_one_entry() |
         PostCond::MasterList::one_entry() |
         PostCond::MasterKeyfile::exists_and_secure()},

    {"master_rename_missing_new_key",
     "WL12974::TS_FR25_3",
     {
         "master-rename",
         "foo.key",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_FAILURE,
     "",
     "",
     "^expected 2 arguments <old-key> <new-key>, got 1",
     PreCond::MasterKeyfile::valid_one_entry() |
         PostCond::MasterList::one_entry() |
         PostCond::MasterKeyfile::exists_and_secure()},

    {"master_rename_unknown_old_key",
     "WL12974::TS_FR25_4",
     {
         "master-rename",
         "unknown",
         "foo.key",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_FAILURE,
     "",
     "",
     "^old-key 'unknown' not found in master-key-file '.*'",
     PreCond::MasterKeyfile::valid_one_entry() |
         PostCond::MasterKeyfile::exists_and_secure()},

    {"master_rename_same_key",
     "WL12974::TS_FR25_5",
     {
         "master-rename",
         "foo.key",
         "foo.key",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_FAILURE,
     "",
     "",
     "^new-key 'foo.key' already exists in master-key-file '.*'",
     PreCond::MasterKeyfile::valid_one_entry() |
         PostCond::MasterKeyfile::exists_and_secure()},

    {"master_rename_broken_master_key_file",
     "WL12974::TS_FR25_6",
     {
         "master-rename",
         "foo.key",
         "bar.key",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_FAILURE,
     "",
     "",
     "^opening master-key-file failed: Master key file '.*' has invalid file "
     "signature",
     PreCond::MasterKeyfile::empty() |
         PostCond::MasterKeyfile::exists_and_secure()},

    {"master_rename_missing_new_key_and_old_key",
     "WL12974::TS_FR25_7",
     {
         "master-rename",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_FAILURE,
     "",
     "",
     "^expected 2 arguments <old-key> <new-key>, got 0",
     PreCond::MasterKeyfile::valid_one_entry() |
         PostCond::MasterList::one_entry() |
         PostCond::MasterKeyfile::exists_and_secure()},

};

INSTANTIATE_TEST_SUITE_P(Spec, KeyringFrontendTest,
                         ::testing::ValuesIn(password_frontend_param),
                         [](const auto &param_info) {
                           return param_info.param.test_name +
                                  (param_info.param.exit_code == EXIT_SUCCESS
                                       ? "_succeeds"s
                                       : "_fails"s);
                         });

#define TS_FR1_1(CMD, ...)                                                     \
  {                                                                            \
    CMD "_with_master_key_file_and_master_key_reader", "WL12974::TS-FR1_1",    \
        {                                                                      \
            CMD,   __VA_ARGS__,           "--master-key-file",                 \
            "foo", "--master-key-reader", "bar",                               \
        },                                                                     \
        EXIT_FAILURE, "", "",                                                  \
        "--master-key-file and --master-key-reader can't be used together", {} \
  }

#define TS_FR1_2(CMD, ...)                                                     \
  {                                                                            \
    CMD "_with_master_key_file_and_master_key_writer", "WL12974::TS-FR1_2",    \
        {                                                                      \
            CMD,   __VA_ARGS__,           "--master-key-file",                 \
            "foo", "--master-key-writer", "bar",                               \
        },                                                                     \
        EXIT_FAILURE, "", "",                                                  \
        "--master-key-file and --master-key-writer can't be used together", {} \
  }

#define TS_FR1_3(CMD, ...)                                                     \
  {                                                                            \
    CMD "_with_master_key_file_and_master_key_reader_and_master_key_writer",   \
        "WL12974::TS-FR1_3",                                                   \
        {                                                                      \
            CMD,                                                               \
            __VA_ARGS__,                                                       \
            "--master-key-file",                                               \
            "foo",                                                             \
            "--master-key-writer",                                             \
            "bar",                                                             \
            "--master-key-reader",                                             \
            "baz",                                                             \
                                                                               \
        },                                                                     \
        EXIT_FAILURE, "", "",                                                  \
        "--master-key-file and --master-key-reader can't be used together", {} \
  }

#define TS_FR1_4(CMD, ...)                                                    \
  {                                                                           \
    CMD "_empty_master_key", "WL12974::TS-FR1_4",                             \
        {                                                                     \
            CMD,                                                              \
            __VA_ARGS__,                                                      \
        },                                                                    \
        EXIT_FAILURE, "", "", "expected master-key for '.*' to be not empty", \
    {}                                                                        \
  }

#define TS_H_3(CMD, ...)                                        \
  {                                                             \
    "dashdash_help_and_" CMD, "WL12974::TS_H_3",                \
        {                                                       \
            "--help",                                           \
            CMD,                                                \
            __VA_ARGS__,                                        \
        },                                                      \
        EXIT_FAILURE, "", "", "expected no extra arguments", {} \
  }

#define TS_V_3(CMD, ...)                                        \
  {                                                             \
    "dashdash_version_and_" CMD, "WL12974::TS_V_3",             \
        {                                                       \
            "--version",                                        \
            CMD,                                                \
            __VA_ARGS__,                                        \
        },                                                      \
        EXIT_FAILURE, "", "", "expected no extra arguments", {} \
  }

#define TS_KR_1(CMD, ...)                                                      \
  {                                                                            \
    CMD "_with_master_key_reader_empty_and_master_key_writer",                 \
        "WL12974::TS_KR_1",                                                    \
        {                                                                      \
            CMD,   __VA_ARGS__,           "--master-key-writer",               \
            "bar", "--master-key-reader", "",                                  \
                                                                               \
        },                                                                     \
        EXIT_FAILURE, "", "", "^expected --master-key-reader to be not empty", \
    {}                                                                         \
  }

#define TS_KR_2(CMD, ...)                                                 \
  {                                                                       \
    CMD "_with_broken_master_key_reader_empty_and_master_key_writer",     \
        "WL12974::TS_KR_2",                                               \
        {                                                                 \
            CMD,                                                          \
            __VA_ARGS__,                                                  \
            "--master-key-writer",                                        \
            kMasterKeyWriterPlaceholder,                                  \
            "--master-key-reader",                                        \
            kMasterKeyReaderPlaceholder,                                  \
                                                                          \
        },                                                                \
        EXIT_FAILURE, "", "",                                             \
        "failed reading master-key for '.*' from master-key-reader '.*'", \
        PreCond::MasterKeyReader::failing()                               \
  }

#define TS_KR_3(CMD, ...)                                                     \
  {                                                                           \
    CMD "_with_not_executable_master_key_reader_empty_and_master_key_writer", \
        "WL12974::TS_KR_3",                                                   \
        {                                                                     \
            CMD,                                                              \
            __VA_ARGS__,                                                      \
            "--master-key-writer",                                            \
            kMasterKeyWriterPlaceholder,                                      \
            "--master-key-reader",                                            \
            kMasterKeyReaderPlaceholder,                                      \
                                                                              \
        },                                                                    \
        EXIT_FAILURE, "", "",                                                 \
        "failed reading master-key for '.*' from master-key-reader '.*'",     \
        PreCond::MasterKeyReader::not_executable()                            \
  }

#define TS_KW_1(CMD, ...)                                                      \
  {                                                                            \
    CMD "_with_master_key_writer_empty_and_master_key_reader",                 \
        "WL12974::TS_KW_1",                                                    \
        {                                                                      \
            CMD, __VA_ARGS__,           "--master-key-writer",                 \
            "",  "--master-key-reader", kMasterKeyReaderPlaceholder,           \
                                                                               \
        },                                                                     \
        EXIT_FAILURE, "", "", "^expected --master-key-writer to be not empty", \
        PreCond::MasterKeyReader::succeeding()                                 \
  }

#define TS_KW_2(CMD, ...)                                               \
  {                                                                     \
    CMD "_with_broken_master_key_writer_and_master_key_reader",         \
        "WL12974::TS_KW_2",                                             \
        {                                                               \
            CMD,                                                        \
            __VA_ARGS__,                                                \
            "--master-key-writer",                                      \
            kMasterKeyWriterPlaceholder,                                \
            "--master-key-reader",                                      \
            kMasterKeyReaderPlaceholder,                                \
                                                                        \
        },                                                              \
        EXIT_FAILURE, "", "",                                           \
        "failed writing master-key for '.*' to master-key-writer '.*'", \
        PreCond::MasterKeyReader::key_not_found() |                     \
            PreCond::MasterKeyWriter::failing()                         \
  }

#define TS_KW_3(CMD, ...)                                               \
  {                                                                     \
    CMD "_with_not_executable_master_key_writer_and_master_key_reader", \
        "WL12974::TS_KW_3",                                             \
        {                                                               \
            CMD,                                                        \
            __VA_ARGS__,                                                \
            "--master-key-writer",                                      \
            kMasterKeyWriterPlaceholder,                                \
            "--master-key-reader",                                      \
            kMasterKeyReaderPlaceholder,                                \
                                                                        \
        },                                                              \
        EXIT_FAILURE, "", "",                                           \
        "failed writing master-key for '.*' to master-key-writer '.*'", \
        PreCond::MasterKeyReader::key_not_found() |                     \
            PreCond::MasterKeyWriter::not_executable()                  \
  }

#define TS_KF_1(CMD, ...)                                         \
  {                                                               \
    CMD "_with_not_existing_master_key_file", "WL12974::TS_KF_1", \
        {                                                         \
            CMD,                                                  \
            __VA_ARGS__,                                          \
            "--master-key-file",                                  \
            kMasterKeyfilePlaceholder,                            \
        },                                                        \
        EXIT_FAILURE, "", "",                                     \
        "opening master-key-file failed: Can't open file ",       \
        PreCond::Keyring::minimal()                               \
  }

#define TS_KF_1_no_args(CMD)                                      \
  {                                                               \
    CMD "_with_not_existing_master_key_file", "WL12974::TS_KF_1", \
        {                                                         \
            CMD,                                                  \
            "--master-key-file",                                  \
            kMasterKeyfilePlaceholder,                            \
        },                                                        \
        EXIT_FAILURE, "", "",                                     \
        "opening master-key-file failed: Can't open file ",       \
        PreCond::Keyring::minimal()                               \
  }

#define TS_KF_2(CMD, ...)                                                    \
  {                                                                          \
    CMD "_with_master_key_file_empty", "WL12974::TS_KF_2",                   \
        {                                                                    \
            CMD,                                                             \
            __VA_ARGS__,                                                     \
            "--master-key-file",                                             \
            "",                                                              \
        },                                                                   \
        EXIT_FAILURE, "", "", "^expected --master-key-file to be not empty", \
        PreCond::Keyring::minimal()                                          \
  }

#define TS_KF_2_no_args(CMD)                                                 \
  {                                                                          \
    CMD "_with_master_key_file_empty", "WL12974::TS_KF_2",                   \
        {                                                                    \
            CMD,                                                             \
            "--master-key-file",                                             \
            "",                                                              \
        },                                                                   \
        EXIT_FAILURE, "", "", "^expected --master-key-file to be not empty", \
        PreCond::Keyring::minimal()                                          \
  }

#define TS_KF_3(CMD, ...)                                                    \
  {                                                                          \
    CMD "_with_no_master_key_file", "WL12974::TS_KF_3",                      \
        {                                                                    \
            CMD,                                                             \
            __VA_ARGS__,                                                     \
        },                                                                   \
        EXIT_FAILURE, "", "", "^expected --master-key-file to be not empty", \
        PreCond::Keyring::minimal()                                          \
  }

#define TS_KF_3_no_args(CMD)                                                 \
  {                                                                          \
    CMD "_with_no_master_key_file", "WL12974::TS_KF_3",                      \
        {                                                                    \
            CMD,                                                             \
        },                                                                   \
        EXIT_FAILURE, "", "", "^expected --master-key-file to be not empty", \
        PreCond::Keyring::minimal()                                          \
  }

#define TS_AS_3(CMD, ...)                                          \
  {                                                                \
    CMD "_with_unknown_option", "WL12974::TS_AS_3",                \
        {                                                          \
            CMD,                                                   \
            __VA_ARGS__,                                           \
            "--unknown-option",                                    \
        },                                                         \
        EXIT_FAILURE, "", "", "unknown option '--unknown-option'", \
        PreCond::Keyring::minimal()                                \
  }

#define TS_AS_4(CMD, ...)                                      \
  {                                                            \
    CMD "_with_extra_argument", "WL12974::TS_AS_4",            \
        {                                                      \
            CMD, __VA_ARGS__, "some", "extra", "args",         \
        },                                                     \
        EXIT_FAILURE, "", "", "^expected .*<filename>.*, got", \
        PreCond::Keyring::minimal(),                           \
  }

const KeyringFrontendTestParam frontend_fail_param[]{
    {"list_without_filename",
     "WL12974::TS-1_2",
     {
         "list",
         "--master-key-file",
         kMasterKeyfilePlaceholder,
     },
     EXIT_FAILURE,
     "",
     "",
     "^expected <filename> and optionally <username>",
     {}},
    TS_FR1_1("init", kKeyringPlaceholder),
    TS_FR1_1("list", kKeyringPlaceholder),
    TS_FR1_1("get", kKeyringPlaceholder, "someuser", "somekey"),
    TS_FR1_1("export", kKeyringPlaceholder),
    TS_FR1_1("set", kKeyringPlaceholder, "someuser", "somekey", "somevalue"),
    TS_FR1_1("delete", kKeyringPlaceholder, "someuser"),

    TS_FR1_2("init", kKeyringPlaceholder),
    TS_FR1_2("list", kKeyringPlaceholder),
    TS_FR1_2("get", kKeyringPlaceholder, "someuser", "somekey"),
    TS_FR1_2("export", kKeyringPlaceholder),
    TS_FR1_2("set", kKeyringPlaceholder, "someuser", "somekey", "somevalue"),
    TS_FR1_2("delete", kKeyringPlaceholder, "someuser"),

    TS_FR1_3("init", kKeyringPlaceholder),
    TS_FR1_3("list", kKeyringPlaceholder),
    TS_FR1_3("get", kKeyringPlaceholder, "someuser", "somekey"),
    TS_FR1_3("export", kKeyringPlaceholder),
    TS_FR1_3("set", kKeyringPlaceholder, "someuser", "somekey", "somevalue"),
    TS_FR1_3("delete", kKeyringPlaceholder, "someuser"),

    TS_FR1_4("init", kKeyringPlaceholder),
    TS_FR1_4("list", kKeyringPlaceholder),
    TS_FR1_4("get", kKeyringPlaceholder, "someuser", "somekey"),
    TS_FR1_4("export", kKeyringPlaceholder),
    TS_FR1_4("set", kKeyringPlaceholder, "someuser", "somekey", "somevalue"),
    TS_FR1_4("delete", kKeyringPlaceholder, "someuser"),

    TS_H_3("init", kKeyringPlaceholder),
    TS_H_3("list", kKeyringPlaceholder),
    TS_H_3("get", kKeyringPlaceholder, "someuser", "somekey"),
    TS_H_3("export", kKeyringPlaceholder),
    TS_H_3("set", kKeyringPlaceholder, "someuser", "somekey", "somevalue"),
    TS_H_3("delete", kKeyringPlaceholder, "someuser"),

    TS_V_3("init", kKeyringPlaceholder),
    TS_V_3("list", kKeyringPlaceholder),
    TS_V_3("get", kKeyringPlaceholder, "someuser", "somekey"),
    TS_V_3("export", kKeyringPlaceholder),
    TS_V_3("set", kKeyringPlaceholder, "someuser", "somekey", "somevalue"),
    TS_V_3("delete", kKeyringPlaceholder, "someuser"),

    TS_KR_1("init", kKeyringPlaceholder),
    TS_KR_1("list", kKeyringPlaceholder),
    TS_KR_1("get", kKeyringPlaceholder, "someuser", "somekey"),
    TS_KR_1("export", kKeyringPlaceholder),
    TS_KR_1("set", kKeyringPlaceholder, "someuser", "somekey", "somevalue"),
    TS_KR_1("delete", kKeyringPlaceholder, "someuser"),

    TS_KR_2("init", kKeyringPlaceholder),
    TS_KR_2("list", kKeyringPlaceholder),
    TS_KR_2("get", kKeyringPlaceholder, "someuser", "somekey"),
    TS_KR_2("export", kKeyringPlaceholder),
    TS_KR_2("set", kKeyringPlaceholder, "someuser", "somekey", "somevalue"),
    TS_KR_2("delete", kKeyringPlaceholder, "someuser"),

    TS_KR_3("init", kKeyringPlaceholder),
    TS_KR_3("list", kKeyringPlaceholder),
    TS_KR_3("get", kKeyringPlaceholder, "someuser", "somekey"),
    TS_KR_3("export", kKeyringPlaceholder),
    TS_KR_3("set", kKeyringPlaceholder, "someuser", "somekey", "somevalue"),
    TS_KR_3("delete", kKeyringPlaceholder, "someuser"),

    TS_KW_1("init", kKeyringPlaceholder),

    TS_KW_2("init", kKeyringPlaceholder),

    TS_KW_3("init", kKeyringPlaceholder),

    //    TS_KF_1("init", kKeyringPlaceholder),
    TS_KF_1("list", kKeyringPlaceholder),
    TS_KF_1("get", kKeyringPlaceholder, "someuser", "somekey"),
    TS_KF_1("export", kKeyringPlaceholder),
    TS_KF_1("set", kKeyringPlaceholder, "someuser", "somekey", "somevalue"),
    TS_KF_1("delete", kKeyringPlaceholder, "someuser"),
    TS_KF_1_no_args("master-list"),
    TS_KF_1("master-rename", kKeyringPlaceholder, "new"),
    TS_KF_1("master-delete", kKeyringPlaceholder),

    TS_KF_2("init", kKeyringPlaceholder),
    TS_KF_2("list", kKeyringPlaceholder),
    TS_KF_2("get", kKeyringPlaceholder, "someuser", "somekey"),
    TS_KF_2("export", kKeyringPlaceholder),
    TS_KF_2("set", kKeyringPlaceholder, "someuser", "somekey", "somevalue"),
    TS_KF_2("delete", kKeyringPlaceholder, "someuser"),
    TS_KF_2_no_args("master-list"),
    TS_KF_2("master-rename", kKeyringPlaceholder, "new"),
    TS_KF_2("master-delete", kKeyringPlaceholder),

    TS_KF_3_no_args("master-list"),
    TS_KF_3("master-rename", kKeyringPlaceholder, "new"),
    TS_KF_3("master-delete", kKeyringPlaceholder),

    TS_AS_3("init", kKeyringPlaceholder),
    TS_AS_3("list", kKeyringPlaceholder),
    TS_AS_3("get", kKeyringPlaceholder, "someuser", "somekey"),
    TS_AS_3("export", kKeyringPlaceholder),
    TS_AS_3("set", kKeyringPlaceholder, "someuser", "somekey", "somevalue"),
    TS_AS_3("delete", kKeyringPlaceholder, "someuser"),

    TS_AS_4("init", kKeyringPlaceholder),
    TS_AS_4("list", kKeyringPlaceholder),
    TS_AS_4("get", kKeyringPlaceholder, "someuser", "somekey"),
    TS_AS_4("export", kKeyringPlaceholder),
    TS_AS_4("set", kKeyringPlaceholder, "someuser", "somekey", "somevalue"),
    TS_AS_4("delete", kKeyringPlaceholder, "someuser"),

};

INSTANTIATE_TEST_SUITE_P(Fail, KeyringFrontendTest,
                         ::testing::ValuesIn(frontend_fail_param),
                         [](const auto &param_info) {
                           std::string test_name = param_info.param.test_name;
                           for (auto &c : test_name) {
                             if (c == '-') c = '_';
                           }
                           return test_name +
                                  (param_info.param.exit_code == EXIT_SUCCESS
                                       ? "_succeeds"s
                                       : "_fails"s);
                         });

static void init_DIM() {
  static mysql_harness::logging::Registry static_registry;

  mysql_harness::DIM &dim = mysql_harness::DIM::instance();

  // logging facility
  dim.set_static_LoggingRegistry(&static_registry);

  mysql_harness::logging::Registry &registry = dim.get_LoggingRegistry();

  mysql_harness::logging::create_module_loggers(
      registry, mysql_harness::logging::LogLevel::kWarning,
      {mysql_harness::logging::kMainLogger, "sql"},
      mysql_harness::logging::kMainLogger);
  mysql_harness::logging::create_main_log_handler(registry, "", "", true);

  registry.set_ready();
}

int main(int argc, char *argv[]) {
  init_DIM();
  g_origin_path = mysql_harness::Path(argv[0]).dirname();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
