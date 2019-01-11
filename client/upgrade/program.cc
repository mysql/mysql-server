/*
   Copyright (c) 2006, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <algorithm>
#include <cctype>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "client/base/abstract_connection_program.h"
#include "client/base/abstract_options_provider.h"
#include "client/base/mysql_query_runner.h"
#include "client/base/show_variable_query_extractor.h"
#include "client/check/mysqlcheck.h"
#include "client/client_priv.h"
#include "my_default.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "mysqld_error.h"
#include "scripts/mysql_fix_privilege_tables_sql.c"
#include "scripts/sql_commands_sys_schema.h"
#include "scripts/sql_commands_system_tables_data_fix.h"
#include "sql_string.h"

using namespace Mysql::Tools::Base;
using std::placeholders::_1;
using std::string;
using std::stringstream;
using std::vector;

int mysql_check_errors;

const int SYS_TABLE_COUNT = 1;
const int SYS_VIEW_COUNT = 100;
const int SYS_TRIGGER_COUNT = 2;
const int SYS_FUNCTION_COUNT = 22;
const int SYS_PROCEDURE_COUNT = 26;

const char *load_default_groups[] = {
    "client",        /* Read settings how to connect to server */
    "mysql_upgrade", /* Read special settings for mysql_upgrade*/
    0};

namespace Mysql {
namespace Tools {
namespace Upgrade {

using std::string;
using std::stringstream;
using std::vector;

string deprecation_msg =
    "The mysql_upgrade client is now deprecated. The actions executed by the "
    "upgrade client is now done by the server.\nTo upgrade, please start the "
    "new MySQL binary with the older data directory. Repairing user tables is "
    "done automatically. Restart is not required after upgrade.\nThe upgrade "
    "process automatically starts on running a new MySQL binary with an older "
    "data directory. To avoid accidental upgrades, please use the --no-upgrade "
    "option with the MySQL binary. The option --force-upgrade is also provided "
    "to run the server upgrade sequence on demand.\nIt may be possible that "
    "the server upgrade fails due to a number of reasons. In that case, the "
    "upgrade sequence will run again during the next MySQL server start. If "
    "the server upgrade fails repeatedly, the server can be started with the "
    "--minimal-upgrade option to start the server without executing the "
    "upgrade sequence, thus allowing users to manually rectify the problem.";

enum exit_codes {
  EXIT_INIT_ERROR = 1,
  EXIT_ALREADY_UPGRADED = 2,
  EXIT_BAD_VERSION = 3,
  EXIT_MYSQL_CHECK_ERROR = 4,
  EXIT_UPGRADING_QUERIES_ERROR = 5
};

class Mysql_connection_holder {
  MYSQL *m_mysql_connection;

 public:
  explicit Mysql_connection_holder(MYSQL *mysql_connection)
      : m_mysql_connection(mysql_connection) {}

  ~Mysql_connection_holder() { mysql_close(this->m_mysql_connection); }
};

class Program : public Base::Abstract_connection_program {
 public:
  Program()
      : Abstract_connection_program(),
        m_mysql_connection(NULL),
        m_temporary_verbose(false) {}

  string get_version() { return "2.0"; }

  int get_first_release_year() { return 2000; }

  string get_description() {
    return "MySQL utility for upgrading databases to new MySQL versions "
           "(deprecated).\n" +
           deprecation_msg;
  }

  void short_usage() {
    std::cout << "Usage: " << get_name() << " [OPTIONS]" << std::endl;
  }

  int get_error_code() { return 0; }

  /**
    @param query             Query to execute
    @param value_to_compare  The value the query should output
    @return -1 if error, 1 if equal to value, 0 if different from value
  */
  int execute_conditional_query(const char *query,
                                const char *value_to_compare) {
    int condition_result = -1;
    if (!mysql_query(this->m_mysql_connection, query)) {
      MYSQL_RES *result = mysql_store_result(this->m_mysql_connection);
      if (result) {
        MYSQL_ROW row;
        if ((row = mysql_fetch_row(result))) {
          condition_result = (strcmp(row[0], value_to_compare) == 0);
        }
        mysql_free_result(result);
      }
    }
    return condition_result;
  }

  /**
    @return -1 if error, 1 if user is not there, 0 if it is
  */
  int check_session_user_absence() {
    int no_session_user = execute_conditional_query(
        "SELECT COUNT(*) FROM mysql.user WHERE user = 'mysql.session'", "0");
    return no_session_user;
  }

  /**
    @return -1 if error, 1 if the user is correctly configured, 0 if not
  */
  int check_session_user_configuration() {
    int is_user_configured = 0;
    is_user_configured = execute_conditional_query(
        "SELECT SUM(count)=3 FROM ( "
        "SELECT COUNT(*) as count FROM mysql.tables_priv WHERE "
        "Table_priv='Select' and User='mysql.session' and Db='mysql' and "
        "Table_name='user' "
        "UNION ALL "
        "SELECT COUNT(*) as count FROM mysql.db WHERE "
        "Select_priv='Y' and User='mysql.session' and Db in "
        "('performance\\_schema', 'performance_schema') "
        "UNION ALL "
        "SELECT COUNT(*) as count FROM mysql.user WHERE "
        "Super_priv='Y' and User='mysql.session') as user_priv;",
        "1");
    return is_user_configured;
  }
  /**
    Error codes:
    EXIT_INIT_ERROR - Initialization error.
    EXIT_ALREADY_UPGRADED - Server already upgraded.
    EXIT_BAD_VERSION - Bad server version.
    EXIT_MYSQL_CHECK_ERROR - Error during calling mysql_check functionality.
    EXIT_UPGRADING_QUERIES_ERROR - Error during execution of upgrading queries.
   */
  int execute(vector<string> positional_options MY_ATTRIBUTE((unused))) {
    return this->print_message(
        this->m_ignore_errors ? 0 : EXIT_ALREADY_UPGRADED, deprecation_msg);
  }

  void create_options() {
    this->create_new_option(
            &this->m_check_version, "version-check",
            "Run this program only if its \'server version\' "
            "matches the version of the server to which it's connecting, "
            "(enabled by default); use --skip-version-check to avoid this "
            "check. "
            "Note: the \'server version\' of the program is the version of the "
            "MySQL server with which it was built/distributed.")
        ->set_short_character('k')
        ->set_value(true);

    this->create_new_option(
            &this->m_upgrade_systables_only, "upgrade-system-tables",
            "Only upgrade the system tables, do not try to upgrade the data.")
        ->set_short_character('s');

    this->create_new_option(&this->m_verbose, "verbose",
                            "Display more output about the process.")
        ->set_short_character('v')
        ->set_value(true);

    this->create_new_option(
        &this->m_write_binlog, "write-binlog",
        "Write all executed SQL statements to binary log. Disabled by default; "
        "use when statements should be sent to replication slaves.");

    this->create_new_option(&this->m_ignore_errors, "force",
                            "Force execution of SQL statements even if "
                            "mysql_upgrade has already "
                            "been executed for the current version of MySQL.")
        ->set_short_character('f');

    this->create_new_option(&this->m_skip_sys_schema, "skip-sys-schema",
                            "Do not upgrade/install the sys schema.")
        ->set_value(false);
  }

  void error(const Message_data &message) {
    std::cerr << "Upgrade process encountered error and will not continue."
              << std::endl;

    exit(message.get_code());
  }

 private:
  /**
    Prints error occurred in main routine.
   */
  int print_error(int exit_code, string message) {
    std::cout << "Error occurred: " << message << std::endl;
    return exit_code;
  }

  // Print message and exit
  int print_message(int exit_code, string message) {
    std::cout << message << std::endl;
    return exit_code;
  }

  /**
    Gets path to file to write upgrade info into. Path is based on datadir of
    server.
   */
  int64 get_upgrade_info_file_name(char *name) {
    bool exists;

    if (m_datadir.empty()) {
      int64 res = Show_variable_query_extractor::get_variable_value(
          this->m_query_runner, "datadir", m_datadir, exists);

      res |= !exists;
      if (res != 0) {
        return res;
      }
    }

    fn_format(name, "mysql_upgrade_info", m_datadir.c_str(), "", MYF(0));

    return 0;
  }
  /**
    Read the content of mysql_upgrade_info file and
    compare the version number form file against
    version number which mysql_upgrade was compiled for.

    NOTE
    This is an optimization to avoid running mysql_upgrade
    when it's already been performed for the particular
    version of MySQL.

    In case the MySQL server can't return the upgrade info
    file it's always better to report that the upgrade hasn't
    been performed.
   */
  bool is_upgrade_already_done() {
    FILE *in;
    char upgrade_info_file[FN_REFLEN] = {0};
    char buf[sizeof(MYSQL_SERVER_VERSION) + 1];
    char *res;

    this->print_verbose_message("Checking if update is needed.");

    if (this->get_upgrade_info_file_name(upgrade_info_file) != 0)
      return false; /* Could not get filename => not sure */

    if (!(in = my_fopen(upgrade_info_file, O_RDONLY, MYF(0))))
      return false; /* Could not open file => not sure */

    /*
      Read from file, don't care if it fails since it
      will be detected by the strncmp
    */
    memset(buf, 0, sizeof(buf));
    res = fgets(buf, sizeof(buf), in);

    my_fclose(in, MYF(0));

    if (!res) return false; /* Could not read from file => not sure */

    return (strncmp(res, MYSQL_SERVER_VERSION,
                    sizeof(MYSQL_SERVER_VERSION) - 1) == 0);
  }

  /**
    Write mysql_upgrade_info file in servers data dir indicating that
    upgrade has been done for this version

    NOTE
    This might very well fail but since it's just an optimization
    to run mysql_upgrade only when necessary the error can be
    ignored.
   */
  void create_mysql_upgrade_info_file() {
    FILE *out;
    char upgrade_info_file[FN_REFLEN] = {0};

    if (this->get_upgrade_info_file_name(upgrade_info_file) != 0)
      return; /* Could not get filename => skip */

    if (!(out = my_fopen(upgrade_info_file, O_TRUNC | O_WRONLY, MYF(0)))) {
      fprintf(stderr,
              "Could not create the upgrade info file '%s' in "
              "the MySQL Servers datadir, errno: %d\n",
              upgrade_info_file, errno);
      return;
    }

    /* Write new version to file */
    fputs(MYSQL_SERVER_VERSION, out);
    my_fclose(out, MYF(0));

    /*
      Check if the upgrade_info_file was properly created/updated
      It's not a fatal error -> just print a message if it fails.
    */
    if (!this->is_upgrade_already_done())
      fprintf(stderr,
              "Could not write to the upgrade info file '%s' in "
              "the MySQL Servers datadir, errno: %d\n",
              upgrade_info_file, errno);
    return;
  }

  /**
    Check if the server version matches with the server version mysql_upgrade
    was compiled with.

    @return true match successful
            false failed
   */
  bool is_version_matching() {
    string version;
    bool exists;

    this->print_verbose_message("Checking server version.");

    if (Show_variable_query_extractor::get_variable_value(
            this->m_query_runner, "version", version, exists) != 0) {
      return false;
    }

    if (this->calc_server_version(version.c_str()) != MYSQL_VERSION_ID) {
      fprintf(stderr,
              "Error: Server version (%s) does not match with the "
              "version of\nthe server (%s) with which this program was built/"
              "distributed. You can\nuse --skip-version-check to skip this "
              "check.\n",
              version.c_str(), MYSQL_SERVER_VERSION);
      return false;
    } else
      return true;
  }

  /**
    Convert the specified version string into the numeric format.
   */
  ulong calc_server_version(const char *some_version) {
    uint major, minor, version;
    const char *point = some_version, *end_point;
    major = (uint)strtoul(point, (char **)&end_point, 10);
    point = end_point + 1;
    minor = (uint)strtoul(point, (char **)&end_point, 10);
    point = end_point + 1;
    version = (uint)strtoul(point, (char **)&end_point, 10);
    return (ulong)major * 10000L + (ulong)(minor * 100 + version);
  }

  void print_verbose_message(string message) {
    if (!this->m_verbose) return;

    std::cout << message << std::endl;
  }

  MYSQL *m_mysql_connection;
  Mysql_query_runner *m_query_runner;
  string m_datadir;
  bool m_write_binlog;
  bool m_upgrade_systables_only;
  bool m_skip_sys_schema;
  bool m_check_version;
  bool m_ignore_errors;
  bool m_verbose;
  /**
    Enabled during some queries execution to force printing all notes and
    warnings regardless "verbose" option.
   */
  bool m_temporary_verbose;
};

}  // namespace Upgrade
}  // namespace Tools
}  // namespace Mysql

int main(int argc, char **argv) {
  ::Mysql::Tools::Upgrade::Program program;
  program.run(argc, argv);
  return 0;
}
