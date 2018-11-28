/*
   Copyright (c) 2001, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "client/check/mysqlcheck.h"

#include <mysql_version.h>
#include <mysqld_error.h>
#include <stdlib.h>

#include "caching_sha2_passwordopt-vars.h"
#include "client/client_priv.h"
#include "m_ctype.h"
#include "my_alloc.h"
#include "my_dbug.h"
#include "my_default.h"
#include "my_inttypes.h"
#include "my_macros.h"
#include "mysql/service_mysql_alloc.h"
#include "print_version.h"
#include "sslopt-vars.h"
#include "typelib.h"
#include "welcome_copyright_notice.h" /* ORACLE_WELCOME_COPYRIGHT_NOTICE */

using namespace Mysql::Tools::Check;
using std::string;
using std::vector;

/* Exit codes */

#define EX_USAGE 1
#define EX_MYSQLERR 2

static MYSQL mysql_connection, *sock = 0;
static bool opt_alldbs = 0, opt_check_only_changed = 0, opt_extended = 0,
            opt_compress = 0, opt_databases = 0, opt_fast = 0,
            opt_medium_check = 0, opt_quick = 0, opt_all_in_1 = 0,
            opt_silent = 0, opt_auto_repair = 0, ignore_errors = 0,
            tty_password = 0, opt_frm = 0, debug_info_flag = 0,
            debug_check_flag = 0, opt_fix_table_names = 0, opt_fix_db_names = 0,
            opt_upgrade = 0, opt_write_binlog = 1;
static uint verbose = 0, opt_mysql_port = 0;
static uint opt_enable_cleartext_plugin = 0;
static bool using_opt_enable_cleartext_plugin = 0;
static int my_end_arg;
static char *opt_mysql_unix_port = 0;
static char *opt_password = 0, *current_user = 0, *default_charset = 0,
            *current_host = 0;
static char *opt_plugin_dir = 0, *opt_default_auth = 0;
static int first_error = 0;
static const char *opt_skip_database = "";
#if defined(_WIN32)
static char *shared_memory_base_name = 0;
#endif
static uint opt_protocol = 0;
static char *opt_bind_addr = NULL;

static struct my_option my_long_options[] = {
    {"all-databases", 'A',
     "Check all the databases. This is the same as --databases with all "
     "databases selected.",
     &opt_alldbs, &opt_alldbs, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"analyze", 'a', "Analyze given tables.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0,
     0, 0, 0, 0},
    {"all-in-1", '1',
     "Instead of issuing one query for each table, use one query per database, "
     "naming all tables in the database in a comma-separated list.",
     &opt_all_in_1, &opt_all_in_1, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"auto-repair", OPT_AUTO_REPAIR,
     "If a checked table is corrupted, automatically fix it. Repairing will be "
     "done after all tables have been checked, if corrupted ones were found.",
     &opt_auto_repair, &opt_auto_repair, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"bind-address", 0, "IP address to bind to.", (uchar **)&opt_bind_addr,
     (uchar **)&opt_bind_addr, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"character-sets-dir", OPT_CHARSETS_DIR,
     "Directory for character set files.", &charsets_dir, &charsets_dir, 0,
     GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"check", 'c', "Check table for errors.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0,
     0, 0, 0, 0},
    {"check-only-changed", 'C',
     "Check only tables that have changed since last check or haven't been "
     "closed properly.",
     0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"check-upgrade", 'g',
     "Check tables for version-dependent changes. May be used with "
     "--auto-repair to correct tables requiring version-dependent updates.",
     0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"compress", OPT_COMPRESS, "Use compression in server/client protocol.",
     &opt_compress, &opt_compress, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"databases", 'B',
     "Check several databases. Note the difference in usage; in this case no "
     "tables are given. All name arguments are regarded as database names.",
     &opt_databases, &opt_databases, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#ifdef DBUG_OFF
    {"debug", '#', "This is a non-debug version. Catch this and exit.", 0, 0, 0,
     GET_DISABLED, OPT_ARG, 0, 0, 0, 0, 0, 0},
    {"debug-check", OPT_DEBUG_CHECK,
     "This is a non-debug version. Catch this and exit.", 0, 0, 0, GET_DISABLED,
     NO_ARG, 0, 0, 0, 0, 0, 0},
    {"debug-info", OPT_DEBUG_INFO,
     "This is a non-debug version. Catch this and exit.", 0, 0, 0, GET_DISABLED,
     NO_ARG, 0, 0, 0, 0, 0, 0},
#else
    {"debug", '#', "Output debug log. Often this is 'd:t:o,filename'.", 0, 0, 0,
     GET_STR, OPT_ARG, 0, 0, 0, 0, 0, 0},
    {"debug-check", OPT_DEBUG_CHECK,
     "Check memory and open file usage at exit.", &debug_check_flag,
     &debug_check_flag, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"debug-info", OPT_DEBUG_INFO, "Print some debug info at exit.",
     &debug_info_flag, &debug_info_flag, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
    {"default-character-set", OPT_DEFAULT_CHARSET,
     "Set the default character set.", &default_charset, &default_charset, 0,
     GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"default_auth", OPT_DEFAULT_AUTH,
     "Default authentication client-side plugin to use.", &opt_default_auth,
     &opt_default_auth, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"enable_cleartext_plugin", OPT_ENABLE_CLEARTEXT_PLUGIN,
     "Enable/disable the clear text authentication plugin.",
     &opt_enable_cleartext_plugin, &opt_enable_cleartext_plugin, 0, GET_BOOL,
     OPT_ARG, 0, 0, 0, 0, 0, 0},
    {"fast", 'F', "Check only tables that haven't been closed properly.",
     &opt_fast, &opt_fast, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"force", 'f', "Continue even if we get an SQL error.", &ignore_errors,
     &ignore_errors, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"extended", 'e',
     "If you are using this option with CHECK TABLE, it will ensure that the "
     "table is 100 percent consistent, but will take a long time. If you are "
     "using this option with REPAIR TABLE, it will force using old slow repair "
     "with keycache method, instead of much faster repair by sorting.",
     &opt_extended, &opt_extended, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"help", '?', "Display this help message and exit.", 0, 0, 0, GET_NO_ARG,
     NO_ARG, 0, 0, 0, 0, 0, 0},
    {"host", 'h', "Connect to host.", &current_host, &current_host, 0,
     GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"medium-check", 'm',
     "Faster than extended-check, but only finds 99.99 percent of all errors. "
     "Should be good enough for most cases.",
     0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"write-binlog", OPT_WRITE_BINLOG,
     "Log ANALYZE, OPTIMIZE and REPAIR TABLE commands. Use --skip-write-binlog "
     "when commands should not be sent to replication slaves.",
     &opt_write_binlog, &opt_write_binlog, 0, GET_BOOL, NO_ARG, 1, 0, 0, 0, 0,
     0},
    {"optimize", 'o', "Optimize table.", 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0,
     0, 0, 0},
    {"password", 'p',
     "Password to use when connecting to server. If password is not given, "
     "it's solicited on the tty.",
     0, 0, 0, GET_PASSWORD, OPT_ARG, 0, 0, 0, 0, 0, 0},
#ifdef _WIN32
    {"pipe", 'W', "Use named pipes to connect to server.", 0, 0, 0, GET_NO_ARG,
     NO_ARG, 0, 0, 0, 0, 0, 0},
#endif
    {"plugin_dir", OPT_PLUGIN_DIR, "Directory for client-side plugins.",
     &opt_plugin_dir, &opt_plugin_dir, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0,
     0},
    {"port", 'P',
     "Port number to use for connection or 0 for default to, in "
     "order of preference, my.cnf, $MYSQL_TCP_PORT, "
#if MYSQL_PORT_DEFAULT == 0
     "/etc/services, "
#endif
     "built-in default (" STRINGIFY_ARG(MYSQL_PORT) ").",
     &opt_mysql_port, &opt_mysql_port, 0, GET_UINT, REQUIRED_ARG, 0, 0, 0, 0, 0,
     0},
    {"protocol", OPT_MYSQL_PROTOCOL,
     "The protocol to use for connection (tcp, socket, pipe, memory).", 0, 0, 0,
     GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"quick", 'q',
     "If you are using this option with CHECK TABLE, it prevents the check "
     "from scanning the rows to check for wrong links. This is the fastest "
     "check. If you are using this option with REPAIR TABLE, it will try to "
     "repair only the index tree. This is the fastest repair method for a "
     "table.",
     &opt_quick, &opt_quick, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"repair", 'r',
     "Can fix almost anything except unique keys that aren't unique.", 0, 0, 0,
     GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
#if defined(_WIN32)
    {"shared-memory-base-name", OPT_SHARED_MEMORY_BASE_NAME,
     "Base name of shared memory.", &shared_memory_base_name,
     &shared_memory_base_name, 0, GET_STR_ALLOC, REQUIRED_ARG, 0, 0, 0, 0, 0,
     0},
#endif
    {"silent", 's', "Print only error messages.", &opt_silent, &opt_silent, 0,
     GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"skip_database", 0, "Don't process the database specified as argument",
     &opt_skip_database, &opt_skip_database, 0, GET_STR, REQUIRED_ARG, 0, 0, 0,
     0, 0, 0},
    {"socket", 'S', "The socket file to use for connection.",
     &opt_mysql_unix_port, &opt_mysql_unix_port, 0, GET_STR, REQUIRED_ARG, 0, 0,
     0, 0, 0, 0},
#include "caching_sha2_passwordopt-longopts.h"
#include "sslopt-longopts.h"

    {"tables", OPT_TABLES, "Overrides option --databases (-B).", 0, 0, 0,
     GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"use-frm", OPT_FRM,
     "When used with REPAIR, get table structure from .frm file, so the table "
     "can be repaired even if .MYI header is corrupted.",
     &opt_frm, &opt_frm, 0, GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"user", 'u', "User for login if not current user.", &current_user,
     &current_user, 0, GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0},
    {"verbose", 'v', "Print info about the various stages.", 0, 0, 0,
     GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
    {"version", 'V', "Output version information and exit.", 0, 0, 0,
     GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}};

static const char *load_default_groups[] = {"mysqlcheck", "client", 0};

static void usage(void);
static int get_options(int *argc, char ***argv, MEM_ROOT *alloc);
static int dbConnect(char *host, char *user, char *passwd);
static void dbDisconnect(char *host);
static void DBerror(MYSQL *mysql, string when);
static void safe_exit(int error);

static int what_to_do = 0;

static void usage(void) {
  print_version();
  puts(ORACLE_WELCOME_COPYRIGHT_NOTICE("2000"));
  puts(
      "This program can be used to CHECK (-c, -m, -C), REPAIR (-r), ANALYZE "
      "(-a),");
  puts("or OPTIMIZE (-o) tables. Some of the options (like -e or -q) can be");
  puts(
      "used at the same time. Not all options are supported by all storage "
      "engines.");
  puts("Please consult the MySQL manual for latest information about the");
  puts(
      "above. The options -c, -r, -a, and -o are exclusive to each other, "
      "which");
  puts("means that the last option will be used, if several was specified.\n");
  puts("The option -c will be used by default, if none was specified. You");
  puts("can change the default behavior by making a symbolic link, or");
  puts("copying this file somewhere with another name, the alternatives are:");
  puts("mysqlrepair:   The default option will be -r");
  puts("mysqlanalyze:  The default option will be -a");
  puts("mysqloptimize: The default option will be -o\n");
  printf("Usage: %s [OPTIONS] database [tables]\n", my_progname);
  printf("OR     %s [OPTIONS] --databases DB1 [DB2 DB3...]\n", my_progname);
  printf("OR     %s [OPTIONS] --all-databases\n", my_progname);
  print_defaults("my", load_default_groups);
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
} /* usage */

extern "C" {
static bool get_one_option(int optid, const struct my_option *opt,
                           char *argument) {
  int orig_what_to_do = what_to_do;

  switch (optid) {
    case 'a':
      what_to_do = DO_ANALYZE;
      break;
    case 'c':
      what_to_do = DO_CHECK;
      break;
    case 'C':
      what_to_do = DO_CHECK;
      opt_check_only_changed = 1;
      break;
    case 'I': /* Fall through */
    case '?':
      usage();
      exit(0);
    case 'm':
      what_to_do = DO_CHECK;
      opt_medium_check = 1;
      break;
    case 'o':
      what_to_do = DO_OPTIMIZE;
      break;
    case 'p':
      if (argument == disabled_my_option)
        argument = (char *)""; /* Don't require password */
      if (argument) {
        char *start = argument;
        my_free(opt_password);
        opt_password = my_strdup(PSI_NOT_INSTRUMENTED, argument, MYF(MY_FAE));
        while (*argument) *argument++ = 'x'; /* Destroy argument */
        if (*start) start[1] = 0;            /* Cut length of argument */
        tty_password = 0;
      } else
        tty_password = 1;
      break;
    case 'r':
      what_to_do = DO_REPAIR;
      break;
    case 'g':
      what_to_do = DO_CHECK;
      opt_upgrade = 1;
      break;
    case 'W':
#ifdef _WIN32
      opt_protocol = MYSQL_PROTOCOL_PIPE;
#endif
      break;
    case '#':
      DBUG_PUSH(argument ? argument : "d:t:o");
      debug_check_flag = 1;
      break;
#include "sslopt-case.h"

    case OPT_TABLES:
      opt_databases = 0;
      break;
    case 'v':
      verbose++;
      break;
    case 'V':
      print_version();
      exit(0);
    case OPT_ENABLE_CLEARTEXT_PLUGIN:
      using_opt_enable_cleartext_plugin = true;
      break;
    case OPT_MYSQL_PROTOCOL:
      opt_protocol =
          find_type_or_exit(argument, &sql_protocol_typelib, opt->name);
      break;
  }

  if (orig_what_to_do && (what_to_do != orig_what_to_do)) {
    fprintf(stderr,
            "Error:  %s doesn't support multiple contradicting commands.\n",
            my_progname);
    return 1;
  }
  return 0;
}
}

static int get_options(int *argc, char ***argv, MEM_ROOT *alloc) {
  int ho_error;

  if (*argc == 1) {
    usage();
    exit(0);
  }

  my_getopt_use_args_separator = true;
  if ((ho_error =
           load_defaults("my", load_default_groups, argc, argv, alloc)) ||
      (ho_error = handle_options(argc, argv, my_long_options, get_one_option)))
    exit(ho_error);
  my_getopt_use_args_separator = false;

  if (!what_to_do) {
    size_t pnlen = strlen(my_progname);

    if (pnlen < 6) /* name too short */
      what_to_do = DO_CHECK;
    else if (!strcmp("repair", my_progname + pnlen - 6))
      what_to_do = DO_REPAIR;
    else if (!strcmp("analyze", my_progname + pnlen - 7))
      what_to_do = DO_ANALYZE;
    else if (!strcmp("optimize", my_progname + pnlen - 8))
      what_to_do = DO_OPTIMIZE;
    else
      what_to_do = DO_CHECK;
  }

  /*
    If there's no --default-character-set option given with
    --fix-table-name or --fix-db-name set the default character set to
    "utf8mb4".
  */
  if (!default_charset) {
    if (opt_fix_db_names || opt_fix_table_names)
      default_charset = (char *)"utf8mb4";
    else
      default_charset = (char *)MYSQL_AUTODETECT_CHARSET_NAME;
  }
  if (strcmp(default_charset, MYSQL_AUTODETECT_CHARSET_NAME) &&
      !get_charset_by_csname(default_charset, MY_CS_PRIMARY, MYF(MY_WME))) {
    printf("Unsupported character set: %s\n", default_charset);
    return 1;
  }
  if (*argc > 0 && opt_alldbs) {
    printf("You should give only options, no arguments at all, with option\n");
    printf("--all-databases. Please see %s --help for more information.\n",
           my_progname);
    return 1;
  }

  if (*argc < 1 && !opt_alldbs) {
    printf("You forgot to give the arguments! Please see %s --help\n",
           my_progname);
    printf("for more information.\n");
    return 1;
  }
  if (tty_password) opt_password = get_tty_password(NullS);
  if (debug_info_flag) my_end_arg = MY_CHECK_ERROR | MY_GIVE_INFO;
  if (debug_check_flag) my_end_arg = MY_CHECK_ERROR;
  return (0);
} /* get_options */

static int dbConnect(char *host, char *user, char *passwd) {
  DBUG_ENTER("dbConnect");
  if (verbose) {
    fprintf(stderr, "# Connecting to %s...\n", host ? host : "localhost");
  }
  mysql_init(&mysql_connection);
  if (opt_compress) mysql_options(&mysql_connection, MYSQL_OPT_COMPRESS, NullS);
  if (SSL_SET_OPTIONS(&mysql_connection)) {
    fprintf(stderr, "%s", SSL_SET_OPTIONS_ERROR);
    DBUG_RETURN(1);
  }
  if (opt_protocol)
    mysql_options(&mysql_connection, MYSQL_OPT_PROTOCOL, (char *)&opt_protocol);
  if (opt_bind_addr)
    mysql_options(&mysql_connection, MYSQL_OPT_BIND, opt_bind_addr);
#if defined(_WIN32)
  if (shared_memory_base_name)
    mysql_options(&mysql_connection, MYSQL_SHARED_MEMORY_BASE_NAME,
                  shared_memory_base_name);
#endif

  if (opt_plugin_dir && *opt_plugin_dir)
    mysql_options(&mysql_connection, MYSQL_PLUGIN_DIR, opt_plugin_dir);

  if (opt_default_auth && *opt_default_auth)
    mysql_options(&mysql_connection, MYSQL_DEFAULT_AUTH, opt_default_auth);

  if (using_opt_enable_cleartext_plugin)
    mysql_options(&mysql_connection, MYSQL_ENABLE_CLEARTEXT_PLUGIN,
                  (char *)&opt_enable_cleartext_plugin);

  mysql_options(&mysql_connection, MYSQL_SET_CHARSET_NAME, default_charset);
  mysql_options(&mysql_connection, MYSQL_OPT_CONNECT_ATTR_RESET, 0);
  mysql_options4(&mysql_connection, MYSQL_OPT_CONNECT_ATTR_ADD, "program_name",
                 "mysqlcheck");
  set_server_public_key(&mysql_connection);
  set_get_server_public_key_option(&mysql_connection);
  if (!(sock = mysql_real_connect(&mysql_connection, host, user, passwd, NULL,
                                  opt_mysql_port, opt_mysql_unix_port, 0))) {
    DBerror(&mysql_connection, "when trying to connect");
    DBUG_RETURN(1);
  }
  mysql_connection.reconnect = 1;
  DBUG_RETURN(0);
} /* dbConnect */

static void dbDisconnect(char *host) {
  if (verbose)
    fprintf(stderr, "# Disconnecting from %s...\n", host ? host : "localhost");
  mysql_close(sock);
} /* dbDisconnect */

static void DBerror(MYSQL *mysql, string when) {
  DBUG_ENTER("DBerror");
  my_printf_error(0, "Got error: %d: %s %s", MYF(0), mysql_errno(mysql),
                  mysql_error(mysql), when.c_str());
  safe_exit(EX_MYSQLERR);
  DBUG_VOID_RETURN;
} /* DBerror */

static void safe_exit(int error) {
  if (!first_error) first_error = error;
  if (ignore_errors) return;
  if (sock) mysql_close(sock);
  exit(error);
}

int main(int argc, char **argv) {
  MY_INIT(argv[0]);
  /*
  ** Check out the args
  */
  MEM_ROOT alloc(PSI_NOT_INSTRUMENTED, 512);
  if (get_options(&argc, &argv, &alloc)) {
    my_end(my_end_arg);
    exit(EX_USAGE);
  }
  if (dbConnect(current_host, current_user, opt_password)) exit(EX_MYSQLERR);

  // Sun Studio does not work with range constructor from char** to string.
  vector<string> conv;
  conv.reserve(argc);
  for (int i = 0; i < argc; i++) conv.push_back(argv[i]);

  mysql_check(sock, what_to_do, opt_alldbs, opt_check_only_changed,
              opt_extended, opt_databases, opt_fast, opt_medium_check,
              opt_quick, opt_all_in_1, opt_silent, opt_auto_repair,
              ignore_errors, opt_frm, opt_fix_table_names, opt_fix_db_names,
              opt_upgrade, opt_write_binlog, verbose, opt_skip_database, conv,
              DBerror);

  dbDisconnect(current_host);
  my_free(opt_password);
#if defined(_WIN32)
  my_free(shared_memory_base_name);
#endif
  free_root(&alloc, MYF(0));
  my_end(my_end_arg);
  return (first_error != 0);
} /* main */
