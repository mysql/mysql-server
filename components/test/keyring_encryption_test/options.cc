/*
   Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <cstring>
#include <iostream>
#include <string>

#include <m_ctype.h>                  /* Character set */
#include <my_alloc.h>                 /* MEM_ROOT */
#include <my_default.h>               /* print_defaults */
#include <my_getopt.h>                /* Options handling */
#include <my_inttypes.h>              /* typedefs */
#include <my_macros.h>                /* STRINGIFY_ARG */
#include <mysql.h>                    /* MYSQL */
#include <mysql/service_mysql_alloc.h>/* my_strdup */
#include <mysql_com.h>                /* get_tty_password */
#include <print_version.h>            /* print_version */
#include <typelib.h>                  /* find_type_or_exit */
#include <welcome_copyright_notice.h> /* ORACLE_WELCOME_COPYRIGHT_NOTICE */

#include "options.h"

namespace options {

/** MEM_ROOT for arguments */
static MEM_ROOT argv_alloc{PSI_NOT_INSTRUMENTED, 512};

enum migration_options {
  OPT_COMPONENT_DIR = 512,
  OPT_KEYRING,
  /* Add new value above this */
  OPT_LAST
};

char *Options::s_component_dir = nullptr;
char *Options::s_keyring = nullptr;

/** Options group */
static const char *load_default_groups[] = {"mysql_keyring_encryption_test",
                                            nullptr};

/** Command line options */
static struct my_option my_long_options[] = {
    {"help", '?', "Display this help and exit.", nullptr, nullptr, nullptr,
     GET_NO_ARG, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"component_dir", OPT_COMPONENT_DIR, "Directory for components/plugins.",
     &Options::s_component_dir, &Options::s_component_dir, nullptr, GET_STR,
     REQUIRED_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"keyring", OPT_KEYRING, "Keyring name (without extension)",
     &Options::s_keyring, &Options::s_keyring, nullptr, GET_STR, REQUIRED_ARG,
     0, 0, 0, nullptr, 0, nullptr},
    /* Must be the last one */
    {nullptr, 0, nullptr, nullptr, nullptr, nullptr, GET_NO_ARG, NO_ARG, 0, 0,
     0, nullptr, 0, nullptr}};

static void usage(bool version_only) {
  print_version();
  if (version_only) return;
  std::cout << ORACLE_WELCOME_COPYRIGHT_NOTICE("2021") << std::endl;
  std::cout << "MySQL Keyring Encryption Test Utility" << std::endl;
  std::cout << "Usage: " << my_progname << " [OPTIONS] " << std::endl;
  my_print_help(my_long_options);
  print_defaults("my", load_default_groups);
  my_print_variables(my_long_options);
}

bool get_one_option(int optid, const struct my_option *, char *) {
  switch (optid) {
    case 'V':
      usage(true);
      break;
    case 'I':
      [[fallthrough]];
    case '?':
      usage(false);
      break;
  }
  return false;
}

static bool check_options_for_sanity() {
  if (Options::s_component_dir == nullptr || !*Options::s_component_dir ||
      Options::s_keyring == nullptr || !*Options::s_keyring) {
    return false;
  }
  return true;
}

static bool get_options(int argc, char **argv, int &exit_code) {
  exit_code = handle_options(&argc, &argv, my_long_options, get_one_option);
  if (exit_code != 0) {
    return false;
  }

  if (check_options_for_sanity() == false) return false;

  return true;
}

bool process_options(int *argc, char ***argv, int &exit_code) {
  exit_code = 0;
#ifdef _WIN32
  /* Convert command line parameters from UTF16LE to UTF8MB4. */
  my_win_translate_command_line_args(&my_charset_utf8mb4_bin, argc, argv);
#endif

  my_getopt_use_args_separator = true;
  if (load_defaults("my", load_default_groups, argc, argv, &argv_alloc)) {
    return false;
  }
  my_getopt_use_args_separator = false;

  bool save_skip_unknown = my_getopt_skip_unknown;
  my_getopt_skip_unknown = true;
  bool ret = get_options(*argc, *argv, exit_code);
  my_getopt_skip_unknown = save_skip_unknown;
  return ret;
}

}  // namespace options
