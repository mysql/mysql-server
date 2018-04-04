/*
   Copyright (c) 2006, 2018, Oracle and/or its affiliates. All rights reserved.

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

/**
  Error callback to be called from mysql_check functionality.
 */
static void mysql_check_error_callback(MYSQL *, string) {
  mysql_check_errors = 1;
}

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

enum exit_codes {
  EXIT_INIT_ERROR = 1,
  EXIT_ALREADY_UPGRADED = 2,
  EXIT_BAD_VERSION = 3,
  EXIT_MYSQL_CHECK_ERROR = 4,
  EXIT_UPGRADING_QUERIES_ERROR = 5,
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
    return "MySQL utility for upgrading databases to new MySQL versions.";
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
        "SELECT SUM(count)=5 FROM ( "
        "SELECT COUNT(*) as count FROM mysql.tables_priv WHERE "
        "Table_priv='Select' and User='mysql.session' and Db='mysql' and "
        "Table_name='user' "
        "UNION ALL "
        "SELECT COUNT(*) as count FROM mysql.db WHERE "
        "Select_priv='Y' and User='mysql.session' and Db='performance_schema' "
        "UNION ALL "
        "SELECT COUNT(*) as count FROM mysql.user WHERE "
        "Super_priv='Y' and User='mysql.session' "
        "UNION ALL "
        "SELECT COUNT(*) as count FROM mysql.global_grants WHERE "
        "Priv='PERSIST_RO_VARIABLES_ADMIN' and User='mysql.session' "
        "UNION ALL "
        "SELECT COUNT(*) as count FROM mysql.global_grants WHERE "
        "Priv='SYSTEM_VARIABLES_ADMIN' and User='mysql.session') "
        "as user_priv;",
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
    /*
      Disables output buffering to make printing to stdout and stderr order
      deterministic.
    */
    setbuf(stdout, NULL);

    this->m_mysql_connection = this->create_connection();
    // Remember to call mysql_close()
    Mysql_connection_holder connection_holder(m_mysql_connection);
    this->m_query_runner = new Mysql_query_runner(this->m_mysql_connection);
    this->m_query_runner->add_message_callback(
        new std::function<int64(const Message_data &)>(
            std::bind(&Program::process_error, this, _1)));

    /*
      Master and slave should be upgraded separately. All statements executed
      by mysql_upgrade will not be binlogged.
      'SET SQL_LOG_BIN=0' is executed before any other statements.
     */
    if (this->m_upgrade_systables_only) {
      printf(
          "The --upgrade-system-tables option was used, databases won't be "
          "touched.\n");
    }
    if (!this->m_write_binlog) {
      if (mysql_query(this->m_mysql_connection, "SET SQL_LOG_BIN=0") != 0) {
        return this->print_error(1, "Cannot setup server variables.");
      }
    }

    if (mysql_query(this->m_mysql_connection, "USE mysql") != 0) {
      return this->print_error(1, "Cannot select database.");
    }

    /*
      Read the mysql_upgrade_info file to check if mysql_upgrade
      already has been run for this installation of MySQL
    */
    if (this->m_ignore_errors == false && this->is_upgrade_already_done()) {
      printf(
          "This installation of MySQL is already upgraded to %s, "
          "use --force if you still need to run mysql_upgrade\n",
          MYSQL_SERVER_VERSION);
      return EXIT_ALREADY_UPGRADED;
    }

    if (this->m_check_version && this->is_version_matching() == false)
      return EXIT_BAD_VERSION;

    /*
      Check and see if the Server Session Service default user exists
    */

    int user_is_not_there = check_session_user_absence();
    int is_user_correctly_configured = 1;

    if (!user_is_not_there) {
      is_user_correctly_configured = check_session_user_configuration();
    }

    if (!is_user_correctly_configured) {
      return this->print_error(
          EXIT_UPGRADING_QUERIES_ERROR,
          "The mysql.session exists but is not correctly configured."
          " The mysql.session needs SELECT privileges in the"
          " performance_schema database and the mysql.db table and also"
          " SUPER, SYSTEM_VARIABLES_ADMIN and PERSIST_RO_VARIABLES_ADMIN"
          " privileges.");
    }

    if (user_is_not_there < 0 || is_user_correctly_configured < 0) {
      return this->print_error(EXIT_UPGRADING_QUERIES_ERROR,
                               "Query against mysql.user table failed "
                               "when checking the mysql.session.");
    }

    /*
      Run "mysql_fix_privilege_tables.sql" and "mysqlcheck".

      First, upgrade all tables in the system database and then check
      them.

      The order is important here because we might encounter really old
      log tables in CSV engine which have NULLable columns and old TIMESTAMPs.
      Trying to do REPAIR TABLE on such table prior to upgrading it will fail,
      because REPAIR will detect old TIMESTAMPs and try to upgrade them to
      the new ones. In the process it will attempt to create a table with
      NULLable columns which is not supported by CSV engine nowadays.

      After that, run mysqlcheck on all tables.
    */
    if (this->run_sql_fix_privilege_tables() != 0) {
      return EXIT_UPGRADING_QUERIES_ERROR;
    }

    if (this->run_commands_system_tables_data_fix() != 0) {
      return EXIT_UPGRADING_QUERIES_ERROR;
    }

    if (this->m_upgrade_systables_only == false) {
      this->print_verbose_message("Checking system database.");

      if (this->run_mysqlcheck_mysql_db_upgrade() != 0) {
        return this->print_error(EXIT_MYSQL_CHECK_ERROR,
                                 "Error during call to mysql_check.");
      }
    }

    if (this->m_skip_sys_schema == false) {
      /*
        If the sys schema does not exist, then create it
        Otherwise, try to select from sys.version, if this does not
        exist but the schema does, then raise an error rather than
        overwriting/adding to the existing schema
      */
      if (mysql_query(this->m_mysql_connection, "USE sys") != 0) {
        if (this->run_sys_schema_upgrade() != 0) {
          return EXIT_UPGRADING_QUERIES_ERROR;
        }
      } else {
        /* If the database is empty, upgrade */
        if (!mysql_query(this->m_mysql_connection,
                         "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES WHERE "
                         "TABLE_SCHEMA = 'sys'")) {
          MYSQL_RES *result = mysql_store_result(this->m_mysql_connection);
          if (result) {
            MYSQL_ROW row;
            if ((row = mysql_fetch_row(result))) {
              if (strcmp(row[0], "0") == 0) {
                // The sys database contained nothing
                stringstream ss;
                ss << "Found empty sys database. Installing the sys schema.";
                this->print_verbose_message(ss.str());
                if (this->run_sys_schema_upgrade() != 0) {
                  return EXIT_UPGRADING_QUERIES_ERROR;
                }
              }
            }
            mysql_free_result(result);
          }
        }

        /* If the version is smaller, upgrade */
        if (mysql_query(this->m_mysql_connection,
                        "SELECT * FROM sys.version") != 0) {
          return this->print_error(
              EXIT_UPGRADING_QUERIES_ERROR,
              "A sys schema exists with no sys.version view. "
              "If you have a user created sys schema, this must be "
              "renamed for the upgrade to succeed.");
        } else {
          MYSQL_RES *result = mysql_store_result(this->m_mysql_connection);
          if (result) {
            MYSQL_ROW row;

            while ((row = mysql_fetch_row(result))) {
              stringstream ss;
              ulong installed_sys_version = calc_server_version(row[0]);
              ulong expected_sys_version =
                  calc_server_version(SYS_SCHEMA_VERSION);
              if (installed_sys_version >= expected_sys_version) {
                ss << "The sys schema is already up to date (version " << row[0]
                   << ").";
                this->print_verbose_message(ss.str());
              } else {
                ss << "Found outdated sys schema version " << row[0] << ".";
                this->print_verbose_message(ss.str());
                if (this->run_sys_schema_upgrade() != 0) {
                  return EXIT_UPGRADING_QUERIES_ERROR;
                }
              }
            }
            mysql_free_result(result);
          } else {
            return this->print_error(EXIT_UPGRADING_QUERIES_ERROR,
                                     "A sys schema exists with a sys.version "
                                     "view, but it returns no results.");
          }
        }
        /*
           The version may be the same, but in some upgrade scenarios
           such as importing a 5.6 dump in to a fresh 5.7 install that
           includes the mysql schema, and then running mysql_upgrade,
           the functions/procedures will be removed.

           In this case, we check for the expected counts of objects,
           and if those do not match, we just re-install the schema.
        */
        if (mysql_query(this->m_mysql_connection,
                        "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES WHERE "
                        "TABLE_SCHEMA = 'sys' AND TABLE_TYPE = 'BASE TABLE'") !=
            0) {
          return this->print_error(EXIT_UPGRADING_QUERIES_ERROR,
                                   "Query against INFORMATION_SCHEMA.TABLES "
                                   "failed when checking the sys schema.");
        } else {
          MYSQL_RES *result = mysql_store_result(this->m_mysql_connection);
          if (result) {
            MYSQL_ROW row;

            while ((row = mysql_fetch_row(result))) {
              if (SYS_TABLE_COUNT > atoi(row[0])) {
                stringstream ss;
                ss << "Found " << row[0] << " sys tables, but expected "
                   << SYS_TABLE_COUNT
                   << "."
                      " Re-installing the sys schema.";
                this->print_verbose_message(ss.str());
                if (this->run_sys_schema_upgrade() != 0) {
                  return EXIT_UPGRADING_QUERIES_ERROR;
                }
              }
            }

            mysql_free_result(result);
          }
        }

        if (mysql_query(this->m_mysql_connection,
                        "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES WHERE "
                        "TABLE_SCHEMA = 'sys' AND TABLE_TYPE = 'VIEW'") != 0) {
          return this->print_error(EXIT_UPGRADING_QUERIES_ERROR,
                                   "Query against INFORMATION_SCHEMA.TABLES "
                                   "failed when checking the sys schema.");
        } else {
          MYSQL_RES *result = mysql_store_result(this->m_mysql_connection);
          if (result) {
            MYSQL_ROW row;

            while ((row = mysql_fetch_row(result))) {
              if (SYS_VIEW_COUNT > atoi(row[0])) {
                stringstream ss;
                ss << "Found " << row[0] << " sys views, but expected "
                   << SYS_VIEW_COUNT
                   << "."
                      " Re-installing the sys schema.";
                this->print_verbose_message(ss.str());
                if (this->run_sys_schema_upgrade() != 0) {
                  return EXIT_UPGRADING_QUERIES_ERROR;
                }
              }
            }

            mysql_free_result(result);
          }
        }

        if (mysql_query(this->m_mysql_connection,
                        "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TRIGGERS "
                        "WHERE TRIGGER_SCHEMA = 'sys'") != 0) {
          return this->print_error(EXIT_UPGRADING_QUERIES_ERROR,
                                   "Query against INFORMATION_SCHEMA.TRIGGERS "
                                   "failed when checking the sys schema.");
        } else {
          MYSQL_RES *result = mysql_store_result(this->m_mysql_connection);
          if (result) {
            MYSQL_ROW row;

            while ((row = mysql_fetch_row(result))) {
              if (SYS_TRIGGER_COUNT > atoi(row[0])) {
                stringstream ss;
                ss << "Found " << row[0] << " sys triggers, but expected "
                   << SYS_TRIGGER_COUNT
                   << "."
                      " Re-installing the sys schema.";
                this->print_verbose_message(ss.str());
                if (this->run_sys_schema_upgrade() != 0) {
                  return EXIT_UPGRADING_QUERIES_ERROR;
                }
              }
            }

            mysql_free_result(result);
          }
        }

        if (mysql_query(this->m_mysql_connection,
                        "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ROUTINES "
                        "WHERE ROUTINE_SCHEMA = 'sys' AND ROUTINE_TYPE = "
                        "'FUNCTION'") != 0) {
          return this->print_error(EXIT_UPGRADING_QUERIES_ERROR,
                                   "Query against INFORMATION_SCHEMA.ROUTINES "
                                   "failed when checking the sys schema.");
        } else {
          MYSQL_RES *result = mysql_store_result(this->m_mysql_connection);
          if (result) {
            MYSQL_ROW row;

            while ((row = mysql_fetch_row(result))) {
              if (SYS_FUNCTION_COUNT > atoi(row[0])) {
                stringstream ss;
                ss << "Found " << row[0] << " sys functions, but expected "
                   << SYS_FUNCTION_COUNT
                   << "."
                      " Re-installing the sys schema.";
                this->print_verbose_message(ss.str());
                if (this->run_sys_schema_upgrade() != 0) {
                  return EXIT_UPGRADING_QUERIES_ERROR;
                }
              }
            }

            mysql_free_result(result);
          }
        }

        if (mysql_query(this->m_mysql_connection,
                        "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ROUTINES "
                        "WHERE ROUTINE_SCHEMA = 'sys' AND ROUTINE_TYPE = "
                        "'PROCEDURE'") != 0) {
          return this->print_error(EXIT_UPGRADING_QUERIES_ERROR,
                                   "Query against INFORMATION_SCHEMA.ROUTINES "
                                   "failed when checking the sys schema.");
        } else {
          MYSQL_RES *result = mysql_store_result(this->m_mysql_connection);
          if (result) {
            MYSQL_ROW row;

            while ((row = mysql_fetch_row(result))) {
              if (SYS_PROCEDURE_COUNT > atoi(row[0])) {
                stringstream ss;
                ss << "Found " << row[0] << " sys procedures, but expected "
                   << SYS_PROCEDURE_COUNT
                   << "."
                      " Re-installing the sys schema.";
                this->print_verbose_message(ss.str());
                if (this->run_sys_schema_upgrade() != 0) {
                  return EXIT_UPGRADING_QUERIES_ERROR;
                }
              }
            }

            mysql_free_result(result);
          }
        }
      }
      if (mysql_query(this->m_mysql_connection, "USE mysql") != 0) {
        return this->print_error(1, "Cannot select mysql database.");
      }
    } else {
      this->print_verbose_message(
          "Skipping installation/upgrade of the sys schema.");
    }

    if (!this->m_upgrade_systables_only) {
      this->print_verbose_message("Checking databases.");

      if (this->run_mysqlcheck_upgrade() != 0) {
        return this->print_error(EXIT_MYSQL_CHECK_ERROR,
                                 "Error during call to mysql_check.");
      }
    }

    this->print_verbose_message("Upgrade process completed successfully.");

    /* Create a file indicating upgrade has been performed */
    this->create_mysql_upgrade_info_file();

    return 0;
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
    Process messages and decides if to prints them.
   */
  int64 process_error(const Message_data &message) {
    if (this->m_temporary_verbose ||
        message.get_message_type() == Message_type_error) {
      message.print_error(this->get_name());
    }
    if (this->m_ignore_errors == false &&
        message.get_message_type() == Message_type_error) {
      return message.get_code();
    }
    return 0;
  }

  /**
    Process warning messages during upgrades.
   */
  void process_warning(const Message_data &message) {
    if (this->m_temporary_verbose &&
        message.get_message_type() == Message_type_warning) {
      message.print_error(this->get_name());
    }
  }
  /**
    Prints error occurred in main routine.
   */
  int print_error(int exit_code, string message) {
    std::cout << "Error occurred: " << message << std::endl;
    return exit_code;
  }

  /**
    Update all system tables in MySQL Server to current
    version executing all the SQL commands
    compiled into the mysql_fix_privilege_tables array
   */
  int64 run_sql_fix_privilege_tables() {
    const char **query_ptr;
    int64 result;

    Mysql_query_runner runner(*this->m_query_runner);
    std::function<int64(const Mysql_query_runner::Row &)> result_cb(
        std::bind(&Program::result_callback, this, _1));
    std::function<int64(const Message_data &)> message_cb(
        std::bind(&Program::fix_privilage_tables_error, this, _1));

    runner.add_result_callback(&result_cb);
    runner.add_message_callback(&message_cb);

    this->print_verbose_message("Running queries to upgrade MySQL server.");

    for (query_ptr = &mysql_fix_privilege_tables[0]; *query_ptr != NULL;
         query_ptr++) {
      /*
       Check if next query is SHOW WARNINGS, if so enable temporarily
       verbosity of server messages.
       */
      this->m_temporary_verbose =
          (*(query_ptr + 1) != NULL &&
           strcmp(*(query_ptr + 1), "SHOW WARNINGS;\n") == 0);

      result = runner.run_query(*query_ptr);
      if (!this->m_ignore_errors && result != 0) {
        return result;
      }
    }

    return 0;
  }

  /**
    Update system table data

    @retval 0 Success
    @retval non-zero Error
  */
  int run_commands_system_tables_data_fix() {
    const char **query_ptr;
    int result;

    Mysql_query_runner runner(*this->m_query_runner);
    std::function<int64(const Mysql_query_runner::Row &)> result_cb(
        std::bind(&Program::result_callback, this, _1));
    std::function<int64(const Message_data &)> message_cb(
        std::bind(&Program::fix_privilage_tables_error, this, _1));

    runner.add_result_callback(&result_cb);
    runner.add_message_callback(&message_cb);

    this->print_verbose_message("Upgrading system table data.");

    for (query_ptr = &mysql_system_tables_data_fix[0]; *query_ptr != NULL;
         query_ptr++) {
      result = runner.run_query(*query_ptr);
      if (!this->m_ignore_errors && result != 0) {
        return result;
      }
    }

    return 0;
  }

  /**
    Update the sys schema
   */
  int run_sys_schema_upgrade() {
    const char **query_ptr;
    int result;

    Mysql_query_runner runner(*this->m_query_runner);
    std::function<int64(const Mysql_query_runner::Row &)> result_cb(
        std::bind(&Program::result_callback, this, _1));
    std::function<int64(const Message_data &)> message_cb(
        std::bind(&Program::fix_privilage_tables_error, this, _1));

    runner.add_result_callback(&result_cb);
    runner.add_message_callback(&message_cb);

    this->print_verbose_message("Upgrading the sys schema.");

    for (query_ptr = &mysql_sys_schema[0]; *query_ptr != NULL; query_ptr++) {
      result = runner.run_query(*query_ptr);
      if (!this->m_ignore_errors && result != 0) {
        return result;
      }
    }

    return 0;
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

  /**
    Server message callback to be called during execution of upgrade queries.
   */
  int64 fix_privilage_tables_error(const Message_data &message) {
    // This if it is error message and if it is not expected one.
    if (message.get_message_type() == Message_type_error &&
        is_expected_error(message.get_code()) == false) {
      // Pass this message to other callbacks, i.e. print_error to be printed
      // out.
      return 0;
    }
    process_warning(message);
    // Do not pass filtered out messages to other callbacks, i.e. print_error.
    return -1;
  }

  int64 result_callback(const Mysql_query_runner::Row &result_row) {
    /*
     This is an old hacky way used in upgrade queries to show warnings from
     executed queries in fix_privilege_tables. It is not result from
     "SHOW WARNINGS" query.
     */
    if (result_row.size() == 1 && !(result_row.is_value_null(0))) {
      String error;
      uint dummy_errors;
      error.copy("warning:", 8, &my_charset_latin1,
                 this->m_mysql_connection->charset, &dummy_errors);
      std::string result = result_row[0];
      result = result.substr(0, 8);

      if (my_strcasecmp(this->m_mysql_connection->charset, result.c_str(),
                        error.c_ptr()) == 0) {
        std::cerr << result_row[0] << std::endl;
      }
    }
    Mysql_query_runner::cleanup_result(result_row);
    return 0;
  }

  /**
    Checks if given error code is expected during upgrade queries execution.
   */
  bool is_expected_error(int64 error_no) {
    static const int64 expected_errors[] = {
        ER_DUP_FIELDNAME,   /* Duplicate column name */
        ER_DUP_KEYNAME,     /* Duplicate key name */
        ER_BAD_FIELD_ERROR, /* Unknown column */
        0};

    const int64 *expected_error = expected_errors;
    while (*expected_error) {
      if (*expected_error == error_no) {
        return true; /* Found expected error */
      }
      expected_error++;
    }
    return false;
  }

  /**
    Prepares mysqlcheck program instance to be used by mysql_upgrade.
   */
  Mysql::Tools::Check::Program *prepare_mysqlcheck(
      Mysql::Tools::Check::Program &mysql_check) {
    mysql_check_errors = 0;

    return (&mysql_check)
        ->set_ignore_errors(this->m_ignore_errors)
        ->enable_writing_binlog(this->m_write_binlog)
        ->enable_verbosity(this->m_verbose)
        ->set_error_callback(::mysql_check_error_callback);
  }

  /**
    Check and upgrade(if necessary) all tables in the server using mysqlcheck.
   */
  int run_mysqlcheck_upgrade() {
    Mysql::Tools::Check::Program mysql_check;
    this->prepare_mysqlcheck(mysql_check)
        ->enable_auto_repair(true)
        ->enable_upgrade(true)
        ->set_skip_database("mysql")
        ->check_all_databases(this->m_mysql_connection);
    return mysql_check_errors;
  }

  /**
    Check and upgrade(if necessary) all system tables in the server using
    mysqlcheck.
   */
  int run_mysqlcheck_mysql_db_upgrade() {
    vector<string> databases;
    Mysql::Tools::Check::Program mysql_check;

    databases.push_back("mysql");
    this->prepare_mysqlcheck(mysql_check)
        ->enable_auto_repair(true)
        ->enable_upgrade(true)
        ->check_databases(this->m_mysql_connection, databases);
    return mysql_check_errors;
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
