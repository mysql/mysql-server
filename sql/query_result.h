/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef QUERY_RESULT_INCLUDED
#define QUERY_RESULT_INCLUDED

#include <stddef.h>
#include <sys/types.h>

#include "m_ctype.h"
#include "my_base.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_sys.h"
#include "mysqld_error.h"  // ER_*
#include "sql/sql_list.h"

class Item;
class Item_subselect;
class PT_select_var;
class SELECT_LEX_UNIT;
class THD;

/*
  This is used to get result from a query
*/

class Query_result {
 protected:
  THD *thd;
  SELECT_LEX_UNIT *unit;

 public:
  /**
    Number of records estimated in this result.
    Valid only for materialized derived tables/views.
  */
  ha_rows estimated_rowcount;

  Query_result(THD *thd_arg)
      : thd(thd_arg), unit(NULL), estimated_rowcount(0) {}
  virtual ~Query_result() {}

  virtual bool needs_file_privilege() const { return false; }

  /**
    Change wrapped Query_result.

    Replace the wrapped query result object with new_result and call
    prepare() on new_result.

    This base class implementation doesn't wrap other Query_results.

    @retval false Success
    @retval true  Error
  */
  virtual bool change_query_result(Query_result *) { return false; }
  /// @return true if an interceptor object is needed for EXPLAIN
  virtual bool need_explain_interceptor() const { return false; }

  /**
    Perform preparation specific to the query expression or DML statement.

    @returns false if success, true if error
  */
  virtual bool prepare(List<Item> &, SELECT_LEX_UNIT *u) {
    unit = u;
    return false;
  }

  /**
    Optimize the result processing of a query expression, applicable to
    data change operation (not simple select queries).

    @returns false if success, true if error
  */
  virtual bool optimize() { return false; }

  /**
    Prepare for execution of the query expression or DML statement.

    Generally, this will have an implementation only for outer-most
    SELECT_LEX objects, such as data change statements (for preparation
    of the target table(s)) or dump statements (for preparation of target file).

    @returns false if success, true if error
  */
  virtual bool start_execution() { return false; }

  /*
    Because of peculiarities of prepared statements protocol
    we need to know number of columns in the result set (if
    there is a result set) apart from sending columns metadata.
  */
  virtual uint field_count(List<Item> &fields) const { return fields.elements; }
  virtual bool send_result_set_metadata(List<Item> &list, uint flags) = 0;
  virtual bool send_data(List<Item> &items) = 0;
  virtual void send_error(uint errcode, const char *err) {
    my_message(errcode, err, MYF(0));
  }
  virtual bool send_eof() = 0;
  /**
    Check if this query returns a result set and therefore is allowed in
    cursors and set an error message if it is not the case.

    @retval false     success
    @retval true      error, an error message is set
  */
  virtual bool check_simple_select() const {
    my_error(ER_SP_BAD_CURSOR_QUERY, MYF(0));
    return true;
  }
  virtual void abort_result_set() {}
  /**
    Cleanup after this execution. Completes the execution and resets object
    before next execution of a prepared statement/stored procedure.

    @todo add thd argument
  */
  virtual void cleanup() { /* do nothing */
  }
  void set_thd(THD *thd_arg) { thd = thd_arg; }

  void begin_dataset() {}

  /// @returns Pointer to count of rows retained by this result.
  virtual const ha_rows *row_count() const /* purecov: inspected */
  {
    DBUG_ASSERT(false);
    return nullptr;
  } /* purecov: inspected */
};

/*
  Base class for Query_result descendands which intercept and
  transform result set rows. As the rows are not sent to the client,
  sending of result set metadata should be suppressed as well.
*/

class Query_result_interceptor : public Query_result {
 public:
  Query_result_interceptor(THD *thd) : Query_result(thd) {}
  uint field_count(List<Item> &) const override { return 0; }
  bool send_result_set_metadata(List<Item> &, uint) override { return false; }
};

class Query_result_send : public Query_result {
  /**
    True if we have sent result set metadata to the client.
    In this case the client always expects us to end the result
    set with an eof or error packet
  */
  bool is_result_set_started;

 public:
  Query_result_send(THD *thd)
      : Query_result(thd), is_result_set_started(false) {}
  bool send_result_set_metadata(List<Item> &list, uint flags) override;
  bool send_data(List<Item> &items) override;
  bool send_eof() override;
  bool check_simple_select() const override { return false; }
  void abort_result_set() override;
  void cleanup() override { is_result_set_started = false; }
};

class sql_exchange;

class Query_result_to_file : public Query_result_interceptor {
 protected:
  sql_exchange *exchange;
  File file;
  IO_CACHE cache;
  ha_rows row_count;
  char path[FN_REFLEN];

 public:
  Query_result_to_file(THD *thd, sql_exchange *ex)
      : Query_result_interceptor(thd), exchange(ex), file(-1), row_count(0L) {
    path[0] = 0;
  }
  ~Query_result_to_file() { DBUG_ASSERT(file < 0); }

  bool needs_file_privilege() const override { return true; }

  void send_error(uint errcode, const char *err) override;
  bool send_eof() override;
  void cleanup() override;
};

#define ESCAPE_CHARS "ntrb0ZN"  // keep synchronous with READ_INFO::unescape

/*
 List of all possible characters of a numeric value text representation.
*/
#define NUMERIC_CHARS ".0123456789e+-"

class Query_result_export final : public Query_result_to_file {
  size_t field_term_length;
  int field_sep_char, escape_char, line_sep_char;
  int field_term_char;  // first char of FIELDS TERMINATED BY or MAX_INT
  /*
    The is_ambiguous_field_sep field is true if a value of the field_sep_char
    field is one of the 'n', 't', 'r' etc characters
    (see the READ_INFO::unescape method and the ESCAPE_CHARS constant value).
  */
  bool is_ambiguous_field_sep;
  /*
     The is_ambiguous_field_term is true if field_sep_char contains the first
     char of the FIELDS TERMINATED BY (ENCLOSED BY is empty), and items can
     contain this character.
  */
  bool is_ambiguous_field_term;
  /*
    The is_unsafe_field_sep field is true if a value of the field_sep_char
    field is one of the '0'..'9', '+', '-', '.' and 'e' characters
    (see the NUMERIC_CHARS constant value).
  */
  bool is_unsafe_field_sep;
  bool fixed_row_size;
  const CHARSET_INFO *write_cs;  // output charset
 public:
  Query_result_export(THD *thd, sql_exchange *ex)
      : Query_result_to_file(thd, ex) {}
  ~Query_result_export() {}
  bool prepare(List<Item> &list, SELECT_LEX_UNIT *u) override;
  bool start_execution() override;
  bool send_data(List<Item> &items) override;
  void cleanup() override;
};

class Query_result_dump : public Query_result_to_file {
 public:
  Query_result_dump(THD *thd, sql_exchange *ex)
      : Query_result_to_file(thd, ex) {}
  bool prepare(List<Item> &list, SELECT_LEX_UNIT *u) override;
  bool start_execution() override;
  bool send_data(List<Item> &items) override;
};

class Query_dumpvar final : public Query_result_interceptor {
  ha_rows row_count;

 public:
  List<PT_select_var> var_list;
  Query_dumpvar(THD *thd) : Query_result_interceptor(thd), row_count(0) {
    var_list.empty();
  }
  ~Query_dumpvar() {}
  bool prepare(List<Item> &list, SELECT_LEX_UNIT *u) override;
  bool send_data(List<Item> &items) override;
  bool send_eof() override;
  bool check_simple_select() const override;
  void cleanup() override { row_count = 0; }
};

/**
  Base class for result from a subquery.
*/

class Query_result_subquery : public Query_result_interceptor {
 protected:
  Item_subselect *item;

 public:
  Query_result_subquery(THD *thd, Item_subselect *item_arg)
      : Query_result_interceptor(thd), item(item_arg) {}
  bool send_data(List<Item> &items) override = 0;
  bool send_eof() override { return false; }
};

#endif  // QUERY_RESULT_INCLUDED
