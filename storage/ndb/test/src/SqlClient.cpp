/*
   Copyright (c) 2008, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/* Implementation of the SQL client class. */

#include "SqlClient.hpp"

#include <memory>
#include <mutex>

#include "util/require.h"

#include <mysql.h>

#include <NdbSleep.h>
#include <NdbOut.hpp>
#include "NDBT_Output.hpp"

static std::once_flag mysql_library_initialized;

// Release resources at program exit
static void sqlclient_atexit() {
  // Release MySQL library
  mysql_library_end();
}

SqlClient::SqlClient(const char *dbname, const char *suffix)
    : m_user("root"), m_pass(""), m_dbname(dbname) {
  // Initialize MySQL library and setup to release it when program exits
  std::call_once(mysql_library_initialized, mysql_library_init, 0, nullptr,
                 nullptr);
  std::atexit(sqlclient_atexit);

  // Usage of SqlClient initializes the MySQL library and allocates resources in
  // the thread that need to be released.
  thread_local struct EndThreadGuard {
    bool enabled{false};
    ~EndThreadGuard() {
      if (enabled) mysql_thread_end();
    }
  } end_thread_guard;
  end_thread_guard.enabled = true;

  /**
   The settings for how SqlClient connects to a MySQL Server is configured by
   reading from a given section of my.cnf (aka. --defaults-file). This makes it
   possible for different SqlClient instances in same test program to connect to
   different MySQL Servers. The location of my.cnf is normally provided with
   --defaults-file= argument when starting the test binary, if not the MySQL
   Client library will "search" for a my.cnf according to it's rules.

   For example:

     $> testNDBT --defaults-file=/home/user/trunk/mysql-test/var/my.cnf
     # NDBT_DEFAULTS_FILE=/home/user/trunk/mysql-test/var/my.cnf

     // Connect using default section, i.e [client]
     SqlClient sql("test");
     // Connect using section ending in .1.1, i.e [client.1.1]
     SqlClient sql("test", ".1.1");
  */

  // When parsing arguments NDBT will setup location of --defaults-file in
  // environment variable
  const char *env = getenv("NDBT_DEFAULTS_FILE");
  if (env != nullptr && strlen(env) > 0) {
    m_default_file.assign(env);
  } else {
    // Legacy read from my.cnf in MYSQL_HOME
    env = getenv("MYSQL_HOME");
    if (env != nullptr && strlen(env) > 0) {
      m_default_file.assfmt("%s/my.cnf", env);
    }
  }

  // By default the MySQL Client library reads from the [client] section
  m_default_group.assign("client");
  // Using a suffix makes it read from a [client$suffix] section, if no such
  // section is found it will still read from [client]
  if (suffix != nullptr) {
    m_default_group.append(suffix);
  }
}

SqlClient::SqlClient(MYSQL *mysql)
    : m_mysql(mysql),
      m_owns_mysql(false)  // The passed MYSQL object is NOT owned by this class
{}

bool SqlClient::isConnected() {
  if (m_owns_mysql == false) {
    // Using a passed in MYSQL object not owned by this class, the external
    // MYSQL objects is assumed to be connected already
    require(m_mysql);
    return true;
  }
  if (m_mysql) {
    return true;  // Already connected
  }
  return connect();
}

bool SqlClient::waitConnected(int timeout) {
  timeout *= 10;
  while (!isConnected()) {
    if (timeout-- == 0) return false;
    NdbSleep_MilliSleep(100);
  }
  return true;
}

void SqlClient::disconnect() {
  if (m_mysql == nullptr) {
    return;
  }

  // Only disconnect/close when MYSQL object is owned by this class
  if (m_owns_mysql) {
    mysql_close(m_mysql);
    m_mysql = nullptr;
  }
}

SqlClient::~SqlClient() { disconnect(); }

bool SqlClient::connect() {
  // Only allow connect() when the MYSQL object is owned by this class
  require(m_owns_mysql);

  // Only allow connect() when the MYSQL object isn't already allocated
  require(m_mysql == nullptr);

  if (!(m_mysql = mysql_init(NULL))) {
    printError("DB connect-> mysql_init() failed");
    return false;
  }

  g_info << "Connect to MySQL using "
         << (m_default_file.c_str() != nullptr ? m_default_file.c_str()
                                               : " default my.cnf ")
         << " [" << m_default_group.c_str() << "]" << endl;

  // Tell MySQL client library ro read connection parameters from specific file
  // and group
  if (mysql_options(m_mysql, MYSQL_READ_DEFAULT_FILE, m_default_file.c_str()) ||
      mysql_options(m_mysql, MYSQL_READ_DEFAULT_GROUP,
                    m_default_group.c_str())) {
    printError("DB Connect -> mysql_options failed");
    disconnect();
    return false;
  }

  /*
    Connect, read settings from my.cnf
    NOTE! user and password can be stored there as well
  */
  if (mysql_real_connect(m_mysql, NULL, m_user.c_str(), m_pass.c_str(),
                         m_dbname.c_str(), 0, NULL, 0) == NULL) {
    printError("connection failed");
    disconnect();
    return false;
  }
  require(m_mysql);
  return true;
}

/* Error Printing */

void SqlClient::printError(const char *msg) const {
  if (m_mysql && mysql_errno(m_mysql)) {
    if (m_mysql->server_version)
      printf("\n [MySQL-%s]", m_mysql->server_version);
    else
      printf("\n [MySQL]");
    printf("[%d] %s\n", mysql_errno(m_mysql), mysql_error(m_mysql));
  } else if (msg)
    printf(" [MySQL] %s\n", msg);
}

/* Count Table Rows */

unsigned long long SqlClient::selectCountTable(const char *table) {
  BaseString query;
  SqlResultSet result;

  query.assfmt("select count(*) as count from %s", table);
  if (!doQuery(query, result)) {
    printError("select count(*) failed");
    return -1;
  }
  return result.columnAsLong("count");
}

bool SqlClient::runQuery(const char *sql, const Properties &args,
                         SqlResultSet &rows) {
  rows.clear();
  if (!isConnected()) return false;
  require(m_mysql);

  g_debug << "runQuery: " << endl << " sql: '" << sql << "'" << endl;

  MYSQL_STMT *stmt = mysql_stmt_init(m_mysql);
  if (mysql_stmt_prepare(stmt, sql, (unsigned long)strlen(sql))) {
    mysql_stmt_close(stmt);
    report_error("Failed to prepare");
    return false;
  }

  const uint params = mysql_stmt_param_count(stmt);
  std::unique_ptr<MYSQL_BIND[]> bind_param =
      std::make_unique<MYSQL_BIND[]>(params);
  memset(bind_param.get(), 0, params * sizeof(MYSQL_BIND));
  std::unique_ptr<Uint32[]> val_i = std::make_unique<Uint32[]>(params);

  for (uint i = 0; i < params; i++) {
    BaseString name;
    name.assfmt("%d", i);
    // Parameters are named 0, 1, 2...
    if (!args.contains(name.c_str())) {
      g_err << "param " << i << " missing" << endl;
      require(false);
    }
    PropertiesType t;
    const char *val_s;
    args.getTypeOf(name.c_str(), &t);
    switch (t) {
      case PropertiesType_Uint32:
        args.get(name.c_str(), &val_i[i]);
        bind_param[i].buffer_type = MYSQL_TYPE_LONG;
        bind_param[i].buffer = (char *)&val_i[i];
        g_debug << " param" << name.c_str() << ": " << val_i[i] << endl;
        break;
      case PropertiesType_char:
        args.get(name.c_str(), &val_s);
        bind_param[i].buffer_type = MYSQL_TYPE_STRING;
        bind_param[i].buffer = (char *)val_s;
        bind_param[i].buffer_length = (unsigned long)strlen(val_s);
        g_debug << " param" << name.c_str() << ": " << val_s << endl;
        break;
      default:
        require(false);
        break;
    }
  }
  if (mysql_stmt_bind_named_param(stmt, bind_param.get(), params, nullptr)) {
    report_error("Failed to bind param");
    mysql_stmt_close(stmt);
    return false;
  }

  if (mysql_stmt_execute(stmt)) {
    report_error("Failed to execute");
    mysql_stmt_close(stmt);
    return false;
  }

  /*
    Update max_length, making it possible to know how big
    buffers to allocate
  */
  bool one = 1;
  mysql_stmt_attr_set(stmt, STMT_ATTR_UPDATE_MAX_LENGTH, (void *)&one);

  if (mysql_stmt_store_result(stmt)) {
    report_error("Failed to store result");
    mysql_stmt_close(stmt);
    return false;
  }

  uint row = 0;
  MYSQL_RES *res = mysql_stmt_result_metadata(stmt);
  if (res != NULL) {
    MYSQL_FIELD *fields = mysql_fetch_fields(res);
    const uint num_fields = mysql_num_fields(res);
    std::unique_ptr<MYSQL_BIND[]> bind_result =
        std::make_unique<MYSQL_BIND[]>(num_fields);
    memset(bind_result.get(), 0, num_fields * sizeof(MYSQL_BIND));

    for (uint i = 0; i < num_fields; i++) {
      unsigned long buf_len = sizeof(int);

      switch (fields[i].type) {
        case MYSQL_TYPE_STRING:
          buf_len = fields[i].length + 1;
          break;
        case MYSQL_TYPE_VARCHAR:
        case MYSQL_TYPE_VAR_STRING:
          buf_len = fields[i].max_length + 1;
          break;
        case MYSQL_TYPE_LONGLONG:
          buf_len = sizeof(long long);
          break;
        case MYSQL_TYPE_LONG:
          buf_len = sizeof(long);
          break;
        case MYSQL_TYPE_TIMESTAMP:
        case MYSQL_TYPE_DATE:
        case MYSQL_TYPE_TIME:
        case MYSQL_TYPE_DATETIME:
          buf_len = sizeof(MYSQL_TIME);
          break;
        default:
          break;
      }

      bind_result[i].buffer_type = fields[i].type;
      bind_result[i].buffer = malloc(buf_len);
      if (bind_result[i].buffer == NULL) {
        report_error("Unable to allocate memory for bind_result[].buffer");
        mysql_stmt_close(stmt);
        return false;
      }

      bind_result[i].buffer_length = buf_len;
      bind_result[i].is_null = (bool *)malloc(sizeof(bool));
      if (bind_result[i].is_null == NULL) {
        free(bind_result[i].buffer);
        report_error("Unable to allocate memory for bind_result[].is_null");
        mysql_stmt_close(stmt);
        return false;
      }

      *bind_result[i].is_null = 0;
    }

    if (mysql_stmt_bind_result(stmt, bind_result.get())) {
      report_error("Failed to bind result");
      mysql_stmt_close(stmt);
      return false;
    }

    while (mysql_stmt_fetch(stmt) != MYSQL_NO_DATA) {
      Properties curr(true);
      for (uint i = 0; i < num_fields; i++) {
        if (*bind_result[i].is_null) continue;
        switch (fields[i].type) {
          case MYSQL_TYPE_STRING:
            ((char *)bind_result[i].buffer)[fields[i].max_length] = 0;
            [[fallthrough]];
          case MYSQL_TYPE_VARCHAR:
          case MYSQL_TYPE_VAR_STRING:
            curr.put(fields[i].name, (char *)bind_result[i].buffer);
            break;

          case MYSQL_TYPE_LONGLONG:
            curr.put64(fields[i].name,
                       *(unsigned long long *)bind_result[i].buffer);
            break;

          case MYSQL_TYPE_TIMESTAMP:
          case MYSQL_TYPE_DATE:
          case MYSQL_TYPE_TIME:
          case MYSQL_TYPE_DATETIME: {
            MYSQL_TIME *ts = (MYSQL_TIME *)bind_result[i].buffer;
            std::string str(
                std::to_string(ts->year) + "-" + std::to_string(ts->month) +
                "-" + std::to_string(ts->day) + " " + std::to_string(ts->hour) +
                ":" + std::to_string(ts->minute) + ":" +
                std::to_string(ts->second));
            curr.put(str.c_str(), str.length());
            break;
          }

          default:
            curr.put(fields[i].name, *(int *)bind_result[i].buffer);
            break;
        }
      }
      rows.put("row", row++, &curr);
    }

    mysql_free_result(res);

    for (uint i = 0; i < num_fields; i++) {
      free(bind_result[i].buffer);
      free(bind_result[i].is_null);
    }
  }

  // Save stats in result set
  rows.put("rows", row);
  rows.put64("affected_rows", mysql_affected_rows(m_mysql));
  rows.put("mysql_errno", mysql_errno(m_mysql));
  rows.put("mysql_error", mysql_error(m_mysql));
  rows.put("mysql_sqlstate", mysql_sqlstate(m_mysql));
  rows.put64("insert_id", mysql_insert_id(m_mysql));

  mysql_stmt_close(stmt);
  return true;
}

bool SqlClient::runQueryBasic(const char *sql, SqlResultSet &rows) {
  rows.clear();
  if (!isConnected()) return false;
  require(m_mysql);

  g_debug << "runQueryBasic: " << endl << " sql: '" << sql << "'" << endl;

  if (mysql_query(m_mysql, sql) != 0) {
    report_error("Failed to run query");
    return false;
  }

  uint row_count = 0;
  MYSQL_RES *res = mysql_store_result(m_mysql);
  if (res != nullptr) {
    MYSQL_FIELD *const fields = mysql_fetch_fields(res);
    const uint num_fields = mysql_num_fields(res);
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != nullptr) {
      Properties curr(true);
      for (uint i = 0; i < num_fields; i++) {
        char *const field_data = row[i];
        if (field_data == nullptr) {
          // field is NULL
          continue;
        }

        switch (fields[i].type) {
          case MYSQL_TYPE_LONGLONG:
            // Save as Uint64 in result
            curr.put64(fields[i].name, std::stoull(field_data));
            break;

          case MYSQL_TYPE_TINY:
          case MYSQL_TYPE_SHORT:
          case MYSQL_TYPE_INT24:
          case MYSQL_TYPE_LONG:
            // Save as Uint32 in result
            curr.put(fields[i].name, std::stoul(field_data));
            break;

          default:
            // Save as string in result
            curr.put(fields[i].name, field_data);
            break;
        }
      }
      rows.put("row", row_count++, &curr);
    }
    mysql_free_result(res);
  }

  // Save stats in result set
  rows.put("rows", row_count);
  rows.put64("affected_rows", mysql_affected_rows(m_mysql));
  rows.put("mysql_errno", mysql_errno(m_mysql));
  rows.put("mysql_error", mysql_error(m_mysql));
  rows.put("mysql_sqlstate", mysql_sqlstate(m_mysql));
  rows.put64("insert_id", mysql_insert_id(m_mysql));
  return true;
}

bool SqlClient::doQuery(const char *query) {
  SqlResultSet result;
  return doQuery(query, result);
}

bool SqlClient::doQuery(const char *query, SqlResultSet &result) {
  if (!runQueryBasic(query, result)) return false;
  result.get_row(0);  // Load first row
  return true;
}

bool SqlClient::doQuery(const char *query, const Properties &args,
                        SqlResultSet &result) {
  if (!runQuery(query, args, result)) return false;
  result.get_row(0);  // Load first row
  return true;
}

bool SqlClient::doQuery(const char *query, const Properties &args) {
  SqlResultSet result;
  return doQuery(query, args, result);
}

bool SqlClient::doQuery(BaseString &str) { return doQuery(str.c_str()); }

bool SqlClient::doQuery(BaseString &str, SqlResultSet &result) {
  return doQuery(str.c_str(), result);
}

bool SqlClient::doQuery(BaseString &str, const Properties &args,
                        SqlResultSet &result) {
  return doQuery(str.c_str(), args, result);
}

bool SqlClient::doQuery(BaseString &str, const Properties &args) {
  return doQuery(str.c_str(), args);
}

void SqlClient::report_error(const char *message) const {
  g_err << "ERROR: " << message << ", mysql_errno: " << mysql_errno(m_mysql)
        << ", mysql_error: '" << mysql_error(m_mysql) << "'" << endl;
}

/* SqlResultSet */

bool SqlResultSet::get_row(int row_num) {
  if (!get("row", row_num, &m_curr_row)) {
    return false;
  }
  return true;
}

bool SqlResultSet::next() { return get_row(++m_curr_row_num); }

// Reset iterator
void SqlResultSet::reset() {
  m_curr_row_num = -1;
  m_curr_row = 0;
}

// Remove row from resultset
void SqlResultSet::remove() {
  BaseString row_name;
  row_name.assfmt("row_%d", m_curr_row_num);
  Properties::remove(row_name.c_str());
}

// Clear all rows and reset iterator
void SqlResultSet::clear() {
  reset();
  Properties::clear();
}

SqlResultSet::SqlResultSet() : m_curr_row(0), m_curr_row_num(-1) {}

SqlResultSet::~SqlResultSet() {}

const char *SqlResultSet::column(const char *col_name) {
  const char *value;
  if (!m_curr_row) {
    g_err << "ERROR: SqlResultSet::column(" << col_name << ")" << endl
          << "There is no row loaded, call next() before "
          << "acessing the column values" << endl;
    require(m_curr_row);
  }
  if (!m_curr_row->get(col_name, &value)) return NULL;
  return value;
}

std::string_view SqlResultSet::columnAsString(const char *col_name) {
  return {column(col_name)};
}

uint SqlResultSet::columnAsInt(const char *col_name) {
  uint value;
  if (!m_curr_row) {
    g_err << "ERROR: SqlResultSet::columnAsInt(" << col_name << ")" << endl
          << "There is no row loaded, call next() before "
          << "acessing the column values" << endl;
    require(m_curr_row);
  }
  if (!m_curr_row->get(col_name, &value)) return (uint)-1;
  return value;
}

unsigned long long SqlResultSet::columnAsLong(const char *col_name) {
  unsigned long long value;
  if (!m_curr_row) {
    g_err << "ERROR: SqlResultSet::columnAsLong(" << col_name << ")" << endl
          << "There is no row loaded, call next() before "
          << "acessing the column values" << endl;
    require(m_curr_row);
  }
  if (!m_curr_row->get(col_name, &value)) return (uint)-1;
  return value;
}

unsigned long long SqlResultSet::insertId() { return get_long("insert_id"); }

unsigned long long SqlResultSet::affectedRows() {
  return get_long("affected_rows");
}

uint SqlResultSet::numRows() { return get_int("rows"); }

uint SqlResultSet::mysqlErrno() { return get_int("mysql_errno"); }

const char *SqlResultSet::mysqlError() { return get_string("mysql_error"); }

const char *SqlResultSet::mysqlSqlstate() {
  return get_string("mysql_sqlstate");
}

uint SqlResultSet::get_int(const char *name) {
  uint value;
  get(name, &value);
  return value;
}

unsigned long long SqlResultSet::get_long(const char *name) {
  unsigned long long value;
  get(name, &value);
  return value;
}

const char *SqlResultSet::get_string(const char *name) {
  const char *value;
  get(name, &value);
  return value;
}
