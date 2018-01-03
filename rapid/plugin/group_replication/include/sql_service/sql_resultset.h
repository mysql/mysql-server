/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SQL_RESULTSET_INCLUDE
#define SQL_RESULTSET_INCLUDE

#include <mysql/plugin.h>
#include <stddef.h>
#include <string>
#include <vector>

#include "my_inttypes.h"
#include "plugin/group_replication/include/plugin_log.h"

typedef struct
{
  std::string db_name;
  std::string table_name;
  std::string org_table_name;
  std::string col_name;
  std::string org_col_name;
  unsigned long length;
  unsigned int charsetnr;
  unsigned int flags;
  unsigned int decimals;
  enum_field_types type;
} Field_type;

struct Field_value
{
  Field_value();
  Field_value(const Field_value& other);
  Field_value(const longlong &num, bool unsign = false);
  Field_value(const decimal_t &decimal);
  Field_value(const double num);
  Field_value(const MYSQL_TIME &time);
  Field_value(const char *str, size_t length);
  Field_value& operator=(const Field_value& other);
  ~Field_value();
  union
  {
    longlong v_long;
    double v_double;
    decimal_t v_decimal;
    MYSQL_TIME v_time;
    char *v_string;
  } value;
  size_t v_string_length;
  bool is_unsigned;
  bool has_ptr;

private:
  void copy_string(const char *str, size_t length);
};


class Sql_resultset
{
public:
  Sql_resultset() :
    current_row(0),
    num_cols(0),
    num_rows(0),
    num_metarow(0),
    m_resultcs(NULL),
    m_server_status(0),
    m_warn_count(0),
    m_affected_rows(0),
    m_last_insert_id(0),
    m_sql_errno(0),
    m_killed(false)
  {}

  ~Sql_resultset()
  {
    clear();
  }


  /* new row started for resultset */
  void new_row();

  /*
    add new field to existing row

    @param val Field_value to be stored in resulset
  */
  void new_field(Field_value *val);

  /* truncate and free resultset rows and field values */
  void clear();

  /* move row index to next row */
  bool next();

  /*
    move row index to particular row

    @param row  row position to set
  */
  void absolute(int row) { current_row= row; }

  /* move row index to first row */
  void first() { current_row= 0; }

  /* move row index to last row */
  void last() { current_row= num_rows > 0 ? num_rows - 1 : 0; }

  /* increment number of rows in resulset */
  void increment_rows() { ++num_rows; }


  /** Set Methods **/

  /*
    set number of rows in resulset

    @param rows  number of rows in resultset
  */
  void set_rows(uint rows) { num_rows= rows; }

  /*
    set number of cols in resulset

    @param rows  number of cols in resultset
  */
  void set_cols(uint cols) { num_cols= cols; }

  /**
    set resultset charset info

    @param result_cs   charset of resulset
  */
  void set_charset(const CHARSET_INFO *result_cs)
  {
    m_resultcs= result_cs;
  }

  /**
    set server status. check mysql_com for more details

    @param server_status   server status
  */
  void set_server_status(uint server_status)
  {
    m_server_status= server_status;
  }

  /**
    set count of warning issued during command execution

    @param warn_count  number of warning
  */
  void set_warn_count(uint warn_count)
  {
    m_warn_count= warn_count;
  }

  /**
    set rows affected due to last command execution

    @param affected_rows  number of rows affected due to last operation
  */
  void set_affected_rows(ulonglong affected_rows)
  {
    m_affected_rows= affected_rows;
  }

  /**
    set value of the AUTOINCREMENT column for the last INSERT

    @param last_insert_id   last inserted value in AUTOINCREMENT column
  */
  void set_last_insert_id(ulonglong last_insert_id)
  {
    m_last_insert_id= last_insert_id;
  }

  /**
    set client message

    @param msg  client message
  */
  void set_message(std::string msg)
  {
   m_message= msg;
  }

  /**
    set sql error number saved during error in
    last command execution

    @param sql_errno  sql error number
  */
  void set_sql_errno(uint sql_errno)
  {
    m_sql_errno= sql_errno;
  }

  /**
    set sql error message saved during error in
    last command execution

    @param msg  sql error message
  */
  void set_err_msg(std::string msg)
  {
    m_err_msg= msg;
  }

  /**
    set sql error state saved during error in
    last command execution

    @param state  sql error state
  */
  void set_sqlstate(std::string state)
  {
    m_sqlstate= state;
  }

  /* Session was shutdown while command was running */
  void set_killed()
  {
    m_killed= true; /* purecov: inspected */
  }


  /** Get Methods **/

  /*
    get number of rows in resulset

    @return number of rows in resultset
  */
  uint get_rows() { return num_rows; }

  /*
    get number of cols in resulset

    @return number of cols in resultset
  */
  uint get_cols() { return num_cols; }

  /**
    get resultset charset info

    @return charset info
  */
  const CHARSET_INFO * get_charset() { return m_resultcs; }

  /**
    get server status. check mysql_com for more details

    @return server status
  */
  uint get_server_status() { return m_server_status; }

  /**
    get count of warning issued during command execution

    @return warn_count
  */
  uint get_warn_count() { return m_warn_count; }

  /**
    return rows affected dure to last command execution

    @return affected_row
  */
  ulonglong get_affected_rows() { return m_affected_rows; }

  /**
    get value of the AUTOINCREMENT column for the last INSERT

    @return the sql error number
  */
  ulonglong get_last_insert_id() { return m_last_insert_id; }

  /**
    get client message

    @return message
  */
  std::string get_message() { return m_message; }


  /** Getting error info **/
  /**
    get sql error number saved during error in last command execution

    @return the sql error number
      @retval 0      OK
      @retval !=0    SQL Error Number
  */
  uint sql_errno() { return m_sql_errno; }


  /**
    get sql error message saved during error in last command execution

    @return the sql error message
  */
  std::string err_msg() { return m_err_msg; /* purecov: inspected */ }

  /**
    get sql error state saved during error in last command execution

    @return the sql error state
  */
  std::string sqlstate() { return m_sqlstate; }


  /* get long field type column */
  longlong getLong(uint columnIndex)
  {
    return result_value[current_row][columnIndex]->value.v_long;
  }

  /* get decimal field type column */
  decimal_t getDecimal(uint columnIndex)
  {
    return result_value[current_row][columnIndex]->value.v_decimal;
  }

  /* get double field type column */
  double getDouble(uint columnIndex)
  {
    return result_value[current_row][columnIndex]->value.v_double;
  }

  /* get time field type column */
  MYSQL_TIME getTime(uint columnIndex)
  {
    return result_value[current_row][columnIndex]->value.v_time;
  }

  /* get string field type column */
  char *getString(uint columnIndex)
  {
    if (result_value[current_row][columnIndex] != NULL)
      return result_value[current_row][columnIndex]->value.v_string;
    return const_cast<char*>("");
  }

  /* resultset metadata functions */

  /* set metadata info */
  void set_metadata(Field_type ftype)
  {
    result_meta.push_back(ftype);
    ++num_metarow;
  }

  /* get database */
  std::string get_database(uint rowIndex= 0)
  {
    return result_meta[rowIndex].db_name;
  }

  /* get table alias */
  std::string get_table(uint rowIndex= 0)
  {
    return result_meta[rowIndex].table_name;
  }

  /* get original table */
  std::string get_org_table(uint rowIndex= 0)
  {
    return result_meta[rowIndex].org_table_name;
  }

  /* get column name alias */
  std::string get_column_name(uint rowIndex= 0)
  {
    return result_meta[rowIndex].col_name;
  }

  /* get original column name */
  std::string get_org_column_name(uint rowIndex= 0)
  {
    return result_meta[rowIndex].org_col_name;
  }

  /* get field width */
  unsigned long get_length(uint rowIndex= 0)
  {
    return result_meta[rowIndex].length;
  }

  /* get field charsetnr */
  unsigned int get_charsetnr(uint rowIndex= 0)
  {
    return result_meta[rowIndex].charsetnr;
  }

  /*
    get field flag.
    Check
      https://dev.mysql.com/doc/refman/5.7/en/c-api-data-structures.html
    for all flags
  */
  unsigned int get_flags(uint rowIndex= 0)
  {
    return result_meta[rowIndex].flags;
  }

  /* get the number of decimals for numeric fields */
  unsigned int get_decimals(uint rowIndex= 0)
  {
    return result_meta[rowIndex].decimals;
  }

  /* get field type. Check enum enum_field_types for whole list */
  enum_field_types get_field_type(uint rowIndex= 0)
  {
    return result_meta[rowIndex].type;
  }

  /*
    get status whether session was shutdown while command was running

    @return session shutdown status
      @retval true   session was stopped
      @retval false  session was not stopped
  */
  bool get_killed_status()
  {
    return m_killed;
  }

private:
  /* resultset store */
  std::vector< std::vector< Field_value* > > result_value;
  /* metadata store */
  std::vector< Field_type > result_meta;

  int current_row; /* current row position */
  uint num_cols; /* number of columns in resultset/metadata */
  uint num_rows; /* number of rows in resultset */
  uint num_metarow; /* number of rows in metadata */

  const CHARSET_INFO *m_resultcs; /* result charset */

  uint m_server_status; /* server status */
  uint m_warn_count;    /* warning count */

  /* rows affected mostly useful for command like update */
  ulonglong m_affected_rows;
  ulonglong m_last_insert_id; /* last auto-increment column value */
  std::string m_message; /* client message */

  uint m_sql_errno;  /* sql error number */
  std::string m_err_msg; /* sql error message */
  std::string m_sqlstate; /* sql error state */

  bool m_killed; /* session killed status */
};

#endif //SQL_RESULTSET_INCLUDE
