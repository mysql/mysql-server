/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "event_tracking_test.h"
#include "event_tracking_registry.h"

#include <cstdlib>
#include <iostream>

#include "my_alloc.h"                  /* MEM_ROOT */
#include "my_dbug.h"                   /* DEBUG macros */
#include "my_default.h"                /* print_defaults */
#include "my_getopt.h"                 /* Options handling */
#include "my_inttypes.h"               /* typedefs */
#include "my_macros.h"                 /* STRINGIFY_ARG */
#include "my_sys.h"                    /* MY_INIT */
#include "mysql.h"                     /* MYSQL */
#include "mysql/service_mysql_alloc.h" /* my_strdup */
#include "mysql/strings/m_ctype.h"     /* Character set */
#include "mysql_com.h"                 /* get_tty_password */
#include "print_version.h"             /* print_version */
#include "scope_guard.h"               /* create_scope_guard */
#include "typelib.h"                   /* find_type_or_exit */
#include "welcome_copyright_notice.h"  /* ORACLE_WELCOME_COPYRIGHT_NOTICE */

namespace {
/** MEM_ROOT for arguments */
static MEM_ROOT argv_alloc{PSI_NOT_INSTRUMENTED, 512};

enum migration_options {
  OPT_COMPONENT_DIR = 512,
  /* Add new value above this */
  OPT_LAST
};

char *g_component_dir = nullptr;

/** Options group */
static const char *load_default_groups[] = {"test_event_tracking", nullptr};

/** Command line options */
static struct my_option my_long_options[] = {
    {"help", '?', "Display this help and exit.", nullptr, nullptr, nullptr,
     GET_NO_ARG, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"component_dir", OPT_COMPONENT_DIR, "Directory for components",
     &g_component_dir, &g_component_dir, nullptr, GET_STR, REQUIRED_ARG, 0, 0,
     0, nullptr, 0, nullptr},
    /* Must be the last one */
    {nullptr, 0, nullptr, nullptr, nullptr, nullptr, GET_NO_ARG, NO_ARG, 0, 0,
     0, nullptr, 0, nullptr}};

static void usage(bool version_only) {
  print_version();
  if (version_only) return;
  std::cout << ORACLE_WELCOME_COPYRIGHT_NOTICE("2021") << std::endl;
  std::cout << "MySQL Event Tracking Test Utility" << std::endl;
  std::cout << "Usage: " << my_progname << " [OPTIONS] " << std::endl;
  my_print_help(my_long_options);
  print_defaults("my", load_default_groups);
  my_print_variables(my_long_options);
}

static bool get_one_option(int optid, const struct my_option *, char *) {
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
  if (g_component_dir == nullptr || !*g_component_dir) {
    return true;
  }
  return false;
}

static bool get_options(int argc, char **argv, int &exit_code) {
  exit_code = handle_options(&argc, &argv, my_long_options, get_one_option);
  if (exit_code != 0) {
    return true;
  }
  return check_options_for_sanity();
}

static bool process_options(int *argc, char ***argv, int &exit_code) {
  exit_code = 0;
#ifdef _WIN32
  /* Convert command line parameters from UTF16LE to UTF8MB4. */
  my_win_translate_command_line_args(&my_charset_utf8mb4_bin, argc, argv);
#endif

  my_getopt_use_args_separator = true;
  if (load_defaults("my", load_default_groups, argc, argv, &argv_alloc)) {
    return true;
  }
  my_getopt_use_args_separator = false;

  bool save_skip_unknown = my_getopt_skip_unknown;
  my_getopt_skip_unknown = true;
  bool ret = get_options(*argc, *argv, exit_code);
  my_getopt_skip_unknown = save_skip_unknown;
  return ret;
}

const char *consumer_a = "component_test_event_tracking_consumer_a";
const char *producer_a = "component_test_event_tracking_producer_a";

const char *consumer_b = "component_test_event_tracking_consumer_b";
const char *producer_b = "component_test_event_tracking_producer_b";

const char *consumer_c = "component_test_event_tracking_consumer_c";
}  // namespace

int main(int argc, char **argv) {
  MY_INIT(argv[0]);
  DBUG_TRACE;
  DBUG_PROCESS(argv[0]);
  int exit_status = EXIT_FAILURE;

  /* Process options */
  [[maybe_unused]] int exit_code;
  if (process_options(&argc, &argv, exit_code)) {
    std::cerr << "Error processing options" << std::endl;
    return exit_status;
  }

  /* Initialize registry */
  init_registry();

  auto cleanup_guard = create_scope_guard([&] {
    /* Deinitialize registry */
    deinit_registry();
  });

  auto dynamic_loader = get_dynamic_loader();

  if (!dynamic_loader) {
    std::cerr << "Could not get handle of dynamic loader" << std::endl;
    return exit_status;
  }

  auto load_component = [&dynamic_loader](const char *component_dir,
                                          const char *component) -> bool {
    std::string full_path{"file://"};
    full_path.append(component_dir);
    full_path.append("/");
    full_path.append(component);
    const char *urn[] = {full_path.c_str()};
    return dynamic_loader->load(urn, 1) ? true : false;
  };

  auto unload_component = [&dynamic_loader](const char *component_dir,
                                            const char *component) -> void {
    std::string full_path{"file://"};
    full_path.append(component_dir);
    full_path.append("/");
    full_path.append(component);
    const char *urn[] = {full_path.c_str()};
    dynamic_loader->unload(urn, 1);
  };

  /* Load consumer A */
  if (!load_component(g_component_dir, consumer_a)) {
    std::cout << "Loaded consumer: " << consumer_a << std::endl;
    /* Load producer A */
    std::cout << "Loading producer: " << producer_a << std::endl;
    std::cout << producer_a << "'s init method will emit various events"
              << std::endl;
    if (!load_component(g_component_dir, producer_a)) {
      /* Unload producer A */
      unload_component(g_component_dir, producer_a);
      std::cout << "Successfully completed all tests." << std::endl;
      exit_status = EXIT_SUCCESS;
    } else {
      std::cerr << "Error loading producer component: " << producer_a
                << std::endl;
      std::cerr << "One or more tests encoutered error. Please check the log."
                << std::endl;
    }
    /* Unload consumer A */
    unload_component(g_component_dir, consumer_a);
  } else {
    std::cerr << "Error loading consumer component: " << consumer_a
              << std::endl;
  }

  /* Load consumer B and consumer C */
  if (!load_component(g_component_dir, consumer_b) &&
      !load_component(g_component_dir, consumer_c)) {
    std::cout << "Loaded consumer: " << consumer_b << std::endl;
    std::cout << "Loaded consumer: " << consumer_c << std::endl;
    /* Load producer B */
    std::cout << "Loading producer: " << producer_b << std::endl;
    std::cout << producer_b << "'s init method will emit various events"
              << std::endl;
    if (!load_component(g_component_dir, producer_b)) {
      /* Unload producer B */
      unload_component(g_component_dir, producer_b);
      std::cout << "Successfully completed all tests." << std::endl;
      exit_status = EXIT_SUCCESS;
    } else {
      std::cerr << "Error loading producer component: " << producer_b
                << std::endl;
      std::cerr << "One or more tests encoutered error. Please check the log."
                << std::endl;
    }
    /* Unload consumer B and consumer C*/
    unload_component(g_component_dir, consumer_b);
    unload_component(g_component_dir, consumer_c);
  } else {
    std::cerr << "Error loading consumer components" << std::endl;
  }

  my_end(0);
  return exit_status;
}
