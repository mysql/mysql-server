/*
   Copyright (c) 2006, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "../client_priv.h"
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <memory>
#include <iostream>
#include "sql_string.h"
#include "mysqld_error.h"
#include "my_default.h"
#include "check/mysqlcheck.h"
#include "../scripts/mysql_fix_privilege_tables_sql.c"
#include "../scripts/sql_commands_sys_schema.h"

#include "base/abstract_connection_program.h"
#include "base/abstract_options_provider.h"
#include "show_variable_query_extractor.h"

using std::string;
using std::vector;
using std::stringstream;

int mysql_check_errors;

const int SYS_TABLE_COUNT = 1;
const int SYS_VIEW_COUNT = 91;
const int SYS_TRIGGER_COUNT = 2;
const int SYS_FUNCTION_COUNT = 14;
const int SYS_PROCEDURE_COUNT = 22;

/**
  Error callback to be called from mysql_check functionality.
 */
static void mysql_check_error_callback(MYSQL *mysql, string when)
{
  mysql_check_errors= 1;
}

const char *load_default_groups[]=
{
  "client", /* Read settings how to connect to server */
  "mysql_upgrade", /* Read special settings for mysql_upgrade*/
  0
};

namespace Mysql{
namespace Tools{
namespace Upgrade{

using std::vector;
using std::string;
using std::stringstream;

enum exit_codes
{
  EXIT_INIT_ERROR = 1,
  EXIT_ALREADY_UPGRADED = 2,
  EXIT_BAD_VERSION = 3,
  EXIT_MYSQL_CHECK_ERROR = 4,
  EXIT_UPGRADING_QUERIES_ERROR = 5,
};

class Program : public Base::Abstract_connection_program
{
public:
  Program()
    : Abstract_connection_program(),
    m_mysql_connection(NULL),
    m_temporary_verbose(false)
  {}

  string get_version()
  {
    return "2.0";
  }

  int get_first_release_year()
  {
    return 2000;
  }

  string get_description()
  {
    return "MySQL utility for upgrading databases to new MySQL versions.";
  }

  /**
    Error codes:
    EXIT_INIT_ERROR - Initialization error.
    EXIT_ALREADY_UPGRADED - Server already upgraded.
    EXIT_BAD_VERSION - Bad server version.
    EXIT_MYSQL_CHECK_ERROR - Error during calling mysql_check functionality.
    EXIT_UPGRADING_QUERIES_ERROR - Error during execution of upgrading queries.
   */
  int execute(vector<string> positional_options)
  {
    /*
      Disables output buffering to make printing to stdout and stderr order
      deterministic.
    */
    setbuf(stdout, NULL);

    this->m_mysql_connection= this->create_connection();
    this->m_query_runner= new Mysql_query_runner(this->m_mysql_connection);
    this->m_query_runner->add_message_callback(new Instance_callback
      <int, Mysql_message, Program>(this, &Program::print_error));

    /*
      Master and slave should be upgraded separately. All statements executed
      by mysql_upgrade will not be binlogged.
      'SET SQL_LOG_BIN=0' is executed before any other statements.
     */
    if (this->m_upgrade_systables_only)
    {
      printf("The --upgrade-system-tables option was used, databases won't be "
        "touched.\n");
    }
    if (!this->m_write_binlog)
    {
      if (mysql_query(this->m_mysql_connection, "SET SQL_LOG_BIN=0;") != 0)
      {
        return this->print_error(1, "Cannot setup server variables.");
      }
    }

    if (mysql_query(this->m_mysql_connection, "USE mysql;") != 0)
    {
      return this->print_error(1, "Cannot select database.");
    }

    /*
      Read the mysql_upgrade_info file to check if mysql_upgrade
      already has been run for this installation of MySQL
    */
    if (this->m_ignore_errors == false && this->is_upgrade_already_done())
    {
      printf("This installation of MySQL is already upgraded to %s, "
             "use --force if you still need to run mysql_upgrade\n",
             MYSQL_SERVER_VERSION);
      return EXIT_ALREADY_UPGRADED;
    }

    if (this->m_check_version && this->is_version_matching() == false)
      return EXIT_BAD_VERSION;

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
    if (this->run_sql_fix_privilege_tables() != 0)
    {
      return EXIT_UPGRADING_QUERIES_ERROR;
    }

    if (this->m_upgrade_systables_only == false)
    {
      this->print_verbose_message("Checking system database.");

      if (this->run_mysqlcheck_mysql_db_upgrade() != 0)
      {
        return this->print_error(EXIT_MYSQL_CHECK_ERROR, "Error during call to mysql_check.");
      }
    }

    if (this->m_skip_sys_schema == false)
    {
      /*
        If the sys schema does not exist, then create it
        Otherwise, try to select from sys.version, if this does not
        exist but the schema does, then raise an error rather than
        overwriting/adding to the existing schema
      */
      if (mysql_query(this->m_mysql_connection, "USE sys") != 0)
      {
        if (this->run_sys_schema_upgrade() != 0)
        {
          return EXIT_UPGRADING_QUERIES_ERROR;
        }
      } else {
        /* If the version is smaller, upgrade */
        if (mysql_query(this->m_mysql_connection, "SELECT * FROM sys.version") != 0)
        {
          return this->print_error(EXIT_UPGRADING_QUERIES_ERROR,
              "A sys schema exists with no sys.version view. "
              "If you have a user created sys schema, this must be "
              "renamed for the upgrade to succeed.");
        } else {
          MYSQL_RES *result = mysql_store_result(this->m_mysql_connection);
          if (result)
          {
            MYSQL_ROW row;

            while ((row = mysql_fetch_row(result)))
            {
                ulong sys_version = calc_server_version(row[0]);
                if (sys_version >= calc_server_version(SYS_SCHEMA_VERSION))
                {
                  stringstream ss;
                  ss << "The sys schema is already up to date (version " << row[0] << ").";
                  this->print_verbose_message(ss.str());
                } else {
                  stringstream ss;
                  ss << "Found outdated sys schema version " << row[0] << ".";
                  this->print_verbose_message(ss.str());
                  if (this->run_sys_schema_upgrade() != 0)
                  {
                    return EXIT_UPGRADING_QUERIES_ERROR;
                  }
                }
            }
            mysql_free_result(result);
          } else {
            return this->print_error(EXIT_UPGRADING_QUERIES_ERROR,
                "A sys schema exists with a sys.version view, but it returns no results.");
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
          "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'sys' AND TABLE_TYPE = 'BASE TABLE'") != 0)
        {
          return this->print_error(EXIT_UPGRADING_QUERIES_ERROR,
              "Query against INFORMATION_SCHEMA.TABLES failed when checking the sys schema.");
        } else {
          MYSQL_RES *result = mysql_store_result(this->m_mysql_connection);
          if (result)
          {
            MYSQL_ROW row;

            while ((row = mysql_fetch_row(result)))
            {
                if (SYS_TABLE_COUNT != atoi(row[0]))
                {
                  stringstream ss;
                  ss << "Found "  << row[0] <<  " sys tables, but expected " << SYS_TABLE_COUNT << "."
                  " Re-installing the sys schema.";
                  this->print_verbose_message(ss.str());
                  if (this->run_sys_schema_upgrade() != 0)
                  {
                    return EXIT_UPGRADING_QUERIES_ERROR;
                  }
                }
            }

            mysql_free_result(result);
          }
        }

        if (mysql_query(this->m_mysql_connection, 
          "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'sys' AND TABLE_TYPE = 'VIEW'") != 0)
        {
          return this->print_error(EXIT_UPGRADING_QUERIES_ERROR,
              "Query against INFORMATION_SCHEMA.TABLES failed when checking the sys schema.");
        } else {
          MYSQL_RES *result = mysql_store_result(this->m_mysql_connection);
          if (result)
          {
            MYSQL_ROW row;

            while ((row = mysql_fetch_row(result)))
            {
                if (SYS_VIEW_COUNT != atoi(row[0]))
                {
                  stringstream ss;
                  ss << "Found "  << row[0] <<  " sys views, but expected " << SYS_VIEW_COUNT << "."
                  " Re-installing the sys schema.";
                  this->print_verbose_message(ss.str());
                  if (this->run_sys_schema_upgrade() != 0)
                  {
                    return EXIT_UPGRADING_QUERIES_ERROR;
                  }
                }
            }

            mysql_free_result(result);
          }
        }

        if (mysql_query(this->m_mysql_connection, 
          "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA = 'sys'") != 0)
        {
          return this->print_error(EXIT_UPGRADING_QUERIES_ERROR,
              "Query against INFORMATION_SCHEMA.TABLES failed when checking the sys schema.");
        } else {
          MYSQL_RES *result = mysql_store_result(this->m_mysql_connection);
          if (result)
          {
            MYSQL_ROW row;

            while ((row = mysql_fetch_row(result)))
            {
                if (SYS_TRIGGER_COUNT != atoi(row[0]))
                {
                  stringstream ss;
                  ss << "Found "  << row[0] <<  " sys triggers, but expected " << SYS_TRIGGER_COUNT << "."
                  " Re-installing the sys schema.";
                  this->print_verbose_message(ss.str());
                  if (this->run_sys_schema_upgrade() != 0)
                  {
                    return EXIT_UPGRADING_QUERIES_ERROR;
                  }
                }
            }

            mysql_free_result(result);
          }
        }

        if (mysql_query(this->m_mysql_connection, 
          "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ROUTINES WHERE ROUTINE_SCHEMA = 'sys' AND ROUTINE_TYPE = 'FUNCTION'") != 0)
        {
          return this->print_error(EXIT_UPGRADING_QUERIES_ERROR,
              "Query against INFORMATION_SCHEMA.TABLES failed when checking the sys schema.");
        } else {
          MYSQL_RES *result = mysql_store_result(this->m_mysql_connection);
          if (result)
          {
            MYSQL_ROW row;

            while ((row = mysql_fetch_row(result)))
            {
                if (SYS_FUNCTION_COUNT != atoi(row[0]))
                {
                  stringstream ss;
                  ss << "Found "  << row[0] <<  " sys functions, but expected " << SYS_FUNCTION_COUNT << "."
                  " Re-installing the sys schema.";
                  this->print_verbose_message(ss.str());
                  if (this->run_sys_schema_upgrade() != 0)
                  {
                    return EXIT_UPGRADING_QUERIES_ERROR;
                  }
                }
            }

            mysql_free_result(result);
          }
        }

        if (mysql_query(this->m_mysql_connection, 
          "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ROUTINES WHERE ROUTINE_SCHEMA = 'sys' AND ROUTINE_TYPE = 'PROCEDURE'") != 0)
        {
          return this->print_error(EXIT_UPGRADING_QUERIES_ERROR,
              "Query against INFORMATION_SCHEMA.TABLES failed when checking the sys schema.");
        } else {
          MYSQL_RES *result = mysql_store_result(this->m_mysql_connection);
          if (result)
          {
            MYSQL_ROW row;

            while ((row = mysql_fetch_row(result)))
            {
                if (SYS_PROCEDURE_COUNT != atoi(row[0]))
                {
                  stringstream ss;
                  ss << "Found "  << row[0] <<  " sys procedures, but expected " << SYS_PROCEDURE_COUNT << "."
                  " Re-installing the sys schema.";
                  this->print_verbose_message(ss.str());
                  if (this->run_sys_schema_upgrade() != 0)
                  {
                    return EXIT_UPGRADING_QUERIES_ERROR;
                  }
                }
            }

            mysql_free_result(result);
          }
        }

      }
      if (mysql_query(this->m_mysql_connection, "USE mysql") != 0)
      {
        return this->print_error(1, "Cannot select mysql database.");
      }
    } else {
      this->print_verbose_message("Skipping installation/upgrade of the sys schema.");
    }

    if (!this->m_upgrade_systables_only)
    {
      this->print_verbose_message("Checking databases.");

      if (this->run_mysqlcheck_upgrade() != 0)
      {
        return this->print_error(EXIT_MYSQL_CHECK_ERROR, "Error during call to mysql_check.");
      }
    }

    this->print_verbose_message("Upgrade process completed successfully.");

    /* Create a file indicating upgrade has been performed */
    this->create_mysql_upgrade_info_file();

    mysql_close(this->m_mysql_connection);

    return 0;
  }

  void create_options()
  {
    this->create_new_option(&this->m_check_version, "version-check",
        "Run this program only if its \'server version\' "
        "matches the version of the server to which it's connecting, "
        "(enabled by default); use --skip-version-check to avoid this check. "
        "Note: the \'server version\' of the program is the version of the "
        "MySQL server with which it was built/distributed.")
      ->set_short_character('k')
      ->set_value(true);

    this->create_new_option(&this->m_upgrade_systables_only, "upgrade-system-tables",
        "Only upgrade the system tables, do not try to upgrade the data.")
      ->set_short_character('s');

    this->create_new_option(&this->m_verbose, "verbose",
        "Display more output about the process.")
      ->set_short_character('v')
      ->set_value(true);

    this->create_new_option(&this->m_write_binlog, "write-binlog",
      "Write all executed SQL statements to binary log. Disabled by default; "
      "use when statements should be sent to replication slaves.");

    this->create_new_option(&this->m_ignore_errors, "force",
        "Force execution of SQL statements even if mysql_upgrade has already "
        "been executed for the current version of MySQL.")
      ->set_short_character('f');

    this->create_new_option(&this->m_skip_sys_schema, "skip-sys-schema",
        "Do not upgrade/install the sys schema.")
      ->set_value(false);
  }

  void error(int error_code)
  {
    std::cerr << "Upgrade process encountered error and will not continue."
      << std::endl;

    exit(error_code);
  }

private:
  /**
    Prints errors, warnings and notes to standard error.
   */
  int print_error(Mysql_message message)
  {
    String error;
    uint dummy_errors;
    error.copy("error", 5, &my_charset_latin1,
      this->m_mysql_connection->charset, &dummy_errors);

    bool is_error = my_strcasecmp(this->m_mysql_connection->charset,
      message.severity.c_str(), error.c_ptr()) == 0;
    if (this->m_temporary_verbose || is_error)
    {
      std::cerr << this->get_name() << ": [" << message.severity << "] "
        << message.code << ": " << message.message << std::endl;
    }
    if (this->m_ignore_errors == false && is_error)
    {
      return message.code;
    }
    return 0;
  }

  /**
    Prints error occurred in main routine.
   */
  int print_error(int exit_code, string message)
  {
    std::cout << "Error occurred: " << message << std::endl;
    return exit_code;
  }

  /**
    Update all system tables in MySQL Server to current
    version executing all the SQL commands
    compiled into the mysql_fix_privilege_tables array
   */
  int run_sql_fix_privilege_tables()
  {
    const char **query_ptr;
    int result;

    Mysql_query_runner runner(*this->m_query_runner);
    runner.add_result_callback(
      new Instance_callback<int, vector<string>, Program>(
      this, &Program::result_callback));
    runner.add_message_callback(
      new Instance_callback<int, Mysql_message, Program>(
      this, &Program::fix_privilage_tables_error));

    this->print_verbose_message("Running queries to upgrade MySQL server.");

    for ( query_ptr= &mysql_fix_privilege_tables[0];
      *query_ptr != NULL;
      query_ptr++
      )
    {
      /*
       Check if next query is SHOW WARNINGS, if so enable temporarily
       verbosity of server messages.
       */
      this->m_temporary_verbose= (*(query_ptr+1) != NULL
        && strcmp(*(query_ptr+1), "SHOW WARNINGS;\n") == 0);

      result= runner.run_query(*query_ptr);
      if (!this->m_ignore_errors && result != 0)
      {
        return result;
      }
    }

    return 0;
  }

  /**
    Update the sys schema
   */
  int run_sys_schema_upgrade()
  {
    const char **query_ptr;
    int result;

    Mysql_query_runner runner(*this->m_query_runner);
    runner.add_result_callback(
      new Instance_callback<int, vector<string>, Program>(
      this, &Program::result_callback));
    runner.add_message_callback(
      new Instance_callback<int, Mysql_message, Program>(
      this, &Program::fix_privilage_tables_error));

    this->print_verbose_message("Upgrading the sys schema.");

    for ( query_ptr= &mysql_sys_schema[0];
      *query_ptr != NULL;
      query_ptr++
      )
    {
      result= runner.run_query(*query_ptr);
      if (!this->m_ignore_errors && result != 0)
      {
        return result;
      }
    }

    return 0;
  }

  /**
    Gets path to file to write upgrade info into. Path is based on datadir of
    server.
   */
  int get_upgrade_info_file_name(char* name)
  {
    string datadir;
    int res= Show_variable_query_extractor::get_variable_value(
      this->m_query_runner, "datadir", &datadir);
    if (res != 0)
    {
      return res;
    }

    fn_format(name, "mysql_upgrade_info", datadir.c_str(), "", MYF(0));

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
  bool is_upgrade_already_done()
  {
    FILE *in;
    char upgrade_info_file[FN_REFLEN]= {0};
    char buf[sizeof(MYSQL_SERVER_VERSION)+1];
    char *res;

    this->print_verbose_message("Checking if update is needed.");

    if (this->get_upgrade_info_file_name(upgrade_info_file) != 0)
      return false; /* Could not get filename => not sure */

    if (!(in= my_fopen(upgrade_info_file, O_RDONLY, MYF(0))))
      return false; /* Could not open file => not sure */

    /*
      Read from file, don't care if it fails since it
      will be detected by the strncmp
    */
    memset(buf, 0, sizeof(buf));
    res= fgets(buf, sizeof(buf), in);

    my_fclose(in, MYF(0));

    if (!res)
      return false; /* Could not read from file => not sure */

    return (strncmp(res, MYSQL_SERVER_VERSION,
                    sizeof(MYSQL_SERVER_VERSION)-1)==0);
  }

  /**
    Write mysql_upgrade_info file in servers data dir indicating that
    upgrade has been done for this version

    NOTE
    This might very well fail but since it's just an optimization
    to run mysql_upgrade only when necessary the error can be
    ignored.
   */
  void create_mysql_upgrade_info_file()
  {
    FILE *out;
    char upgrade_info_file[FN_REFLEN]= {0};

    if (this->get_upgrade_info_file_name(upgrade_info_file) != 0)
      return; /* Could not get filename => skip */

    if (!(out= my_fopen(upgrade_info_file, O_TRUNC | O_WRONLY, MYF(0))))
    {
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
  bool is_version_matching()
  {
    string version;

    this->print_verbose_message("Checking server version.");

    if (Show_variable_query_extractor::get_variable_value(
      this->m_query_runner, "version", &version) != 0)
    {
      return false;
    }

    if (this->calc_server_version(version.c_str()) != MYSQL_VERSION_ID)
    {
      fprintf(stderr, "Error: Server version (%s) does not match with the "
              "version of\nthe server (%s) with which this program was built/"
              "distributed. You can\nuse --skip-version-check to skip this "
              "check.\n", version.c_str(), MYSQL_SERVER_VERSION);
      return false;
    }
    else
      return true;
  }

  /**
    Convert the specified version string into the numeric format.
   */
  ulong calc_server_version(const char *some_version)
  {
    uint major, minor, version;
    const char *point= some_version, *end_point;
    major=   (uint) strtoul(point, (char**)&end_point, 10);  point=end_point+1;
    minor=   (uint) strtoul(point, (char**)&end_point, 10);  point=end_point+1;
    version= (uint) strtoul(point, (char**)&end_point, 10);
    return (ulong) major * 10000L + (ulong)(minor * 100 + version);
  }

  /**
    Server message callback to be called during execution of upgrade queries.
   */
  int fix_privilage_tables_error(Mysql_message message)
  {
    String error;
    uint dummy_errors;
    error.copy("error", 5, &my_charset_latin1,
      this->m_mysql_connection->charset, &dummy_errors);

    bool is_error = my_strcasecmp(this->m_mysql_connection->charset,
      message.severity.c_str(), error.c_ptr()) == 0;

    // This if it is error message and if it is not expected one.
    if (this->m_temporary_verbose ||
      (is_error && is_expected_error(message.code) == false))
    {
      // Pass this message to other callbacks, i.e. print_error to be printed out.
      return 0;
    }
    // Do not pass filtered out messages to other callbacks, i.e. print_error.
    return -1;
  }

  int result_callback(vector<string> result_row)
  {
    /*
     This is an old hacky way used in upgrade queries to show warnings from
     executed queries in fix_privilege_tables. It is not result from
     "SHOW WARNINGS;" query.
     */
    for (vector<string>::iterator it= result_row.begin();
      it != result_row.end();
      it++)
    {
      String error;
      uint dummy_errors;
      error.copy("warning:", 8, &my_charset_latin1,
        this->m_mysql_connection->charset, &dummy_errors);
      String result((*it).c_str(), (*it).length(),
        this->m_mysql_connection->charset);
      result= result.substr(0, 8);

      if (my_strcasecmp(this->m_mysql_connection->charset,
        result.c_ptr(), error.c_ptr()) == 0)
      {
        std::cerr << *it << std::endl;
      }
    }
    return 0;
  }

  /**
    Checks if given error code is expected during upgrade queries execution.
   */
  bool is_expected_error(int error_no)
  {
    static const int expected_errors[]=
    {
      ER_DUP_FIELDNAME, /* Duplicate column name */
      ER_DUP_KEYNAME, /* Duplicate key name */
      ER_BAD_FIELD_ERROR, /* Unknown column */
      0
    };

    const int* expected_error= expected_errors;
    while (*expected_error)
    {
      if (*expected_error == error_no)
      {
        return true; /* Found expected error */
      }
      expected_error++;
    }
    return false;
  }

  /**
    Prepares mysqlcheck program instance to be used by mysql_upgrade.
   */
  Mysql::Tools::Check::Program* prepare_mysqlcheck(
    Mysql::Tools::Check::Program& mysql_check)
  {
    mysql_check_errors= 0;

    return (&mysql_check)
      ->set_ignore_errors(this->m_ignore_errors)
      ->enable_writing_binlog(this->m_write_binlog)
      ->enable_verbosity(this->m_verbose)
      ->set_error_callback(::mysql_check_error_callback);
  }

  /**
    Check and upgrade(if necessary) all tables in the server using mysqlcheck.
   */
  int run_mysqlcheck_upgrade()
  {
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
  int run_mysqlcheck_mysql_db_upgrade()
  {
    vector<string> databases;
    Mysql::Tools::Check::Program mysql_check;

    databases.push_back("mysql");
    this->prepare_mysqlcheck(mysql_check)
      ->enable_auto_repair(true)
      ->enable_upgrade(true)
      ->check_databases(this->m_mysql_connection, databases);
    return mysql_check_errors;
  }

  void print_verbose_message(string message)
  {
    if (!this->m_verbose)
      return;

    std::cout << message << std::endl;
  }

  MYSQL* m_mysql_connection;
  Mysql_query_runner* m_query_runner;
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

}
}
}

static ::Mysql::Tools::Upgrade::Program program;

int main(int argc, char **argv)
{
  program.run(argc, argv);
  return 0;
}
