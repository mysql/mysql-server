/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef QUERY_RESULT_INCLUDED
#define QUERY_RESULT_INCLUDED

#include "my_global.h"
#include "mysqld_error.h"       // ER_*
#include "sql_lex.h"            // SELECT_LEX_UNIT

class JOIN;
class THD;


/*
  This is used to get result from a query
*/

class Query_result :public Sql_alloc
{
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
    : thd(thd_arg), unit(NULL), estimated_rowcount(0)
  {}
  virtual ~Query_result() {};

  /**
    Change wrapped Query_result.

    Replace the wrapped query result object with new_result and call
    prepare() and prepare2() on new_result.

    This base class implementation doesn't wrap other Query_results.

    @param new_result The new query result object to wrap around

    @retval false Success
    @retval true  Error
  */
  virtual bool change_query_result(Query_result *new_result)
  {
    return false;
  }
  /// @return true if an interceptor object is needed for EXPLAIN
  virtual bool need_explain_interceptor() const { return false; }

  virtual int prepare(List<Item> &list, SELECT_LEX_UNIT *u)
  {
    unit= u;
    return 0;
  }
  virtual int prepare2(void) { return 0; }
  /*
    Because of peculiarities of prepared statements protocol
    we need to know number of columns in the result set (if
    there is a result set) apart from sending columns metadata.
  */
  virtual uint field_count(List<Item> &fields) const
  { return fields.elements; }
  virtual bool send_result_set_metadata(List<Item> &list, uint flags)=0;
  virtual bool send_data(List<Item> &items)=0;
  virtual bool initialize_tables (JOIN *join=0) { return false; }
  virtual void send_error(uint errcode,const char *err)
  { my_message(errcode, err, MYF(0)); }
  virtual bool send_eof()=0;
  /**
    Check if this query returns a result set and therefore is allowed in
    cursors and set an error message if it is not the case.

    @retval false     success
    @retval true      error, an error message is set
  */
  virtual bool check_simple_select() const
  {
    my_error(ER_SP_BAD_CURSOR_QUERY, MYF(0));
    return true;
  }
  virtual void abort_result_set() {}
  /*
    Cleanup instance of this class for next execution of a prepared
    statement/stored procedure.
  */
  virtual void cleanup()
  {
    /* do nothing */
  }
  void set_thd(THD *thd_arg) { thd= thd_arg; }

  /**
    If we execute EXPLAIN SELECT ... LIMIT (or any other EXPLAIN query)
    we have to ignore offset value sending EXPLAIN output rows since
    offset value belongs to the underlying query, not to the whole EXPLAIN.
  */
  void reset_offset_limit_cnt() { unit->offset_limit_cnt= 0; }

#ifdef EMBEDDED_LIBRARY
  virtual void begin_dataset() {}
#else
  void begin_dataset() {}
#endif
};


/*
  Base class for Query_result descendands which intercept and
  transform result set rows. As the rows are not sent to the client,
  sending of result set metadata should be suppressed as well.
*/

class Query_result_interceptor: public Query_result
{
public:
  Query_result_interceptor(THD *thd) : Query_result(thd) {}
  uint field_count(List<Item> &fields) const { return 0; }
  bool send_result_set_metadata(List<Item> &fields, uint flag) { return false; }
};


class Query_result_send :public Query_result {
  /**
    True if we have sent result set metadata to the client.
    In this case the client always expects us to end the result
    set with an eof or error packet
  */
  bool is_result_set_started;
public:
  Query_result_send(THD *thd)
    : Query_result(thd), is_result_set_started(false) {}
  bool send_result_set_metadata(List<Item> &list, uint flags);
  bool send_data(List<Item> &items);
  bool send_eof();
  virtual bool check_simple_select() const { return false; }
  void abort_result_set();
  /**
    Cleanup an instance of this class for re-use
    at next execution of a prepared statement/
    stored procedure statement.
  */
  virtual void cleanup()
  {
    is_result_set_started= false;
  }
};


/*
  Used to hold information about file and file structure in exchange
  via non-DB file (...INTO OUTFILE..., ...LOAD DATA...)
  XXX: We never call destructor for objects of this class.
*/

class sql_exchange :public Sql_alloc
{
public:
  Field_separators field;
  Line_separators line;
  enum enum_filetype filetype; /* load XML, Added by Arnold & Erik */
  const char *file_name;
  bool dumpfile;
  ulong skip_lines;
  const CHARSET_INFO *cs;
  sql_exchange(const char *name, bool dumpfile_flag,
               enum_filetype filetype_arg= FILETYPE_CSV);
  bool escaped_given(void);
};


class Query_result_to_file :public Query_result_interceptor {
protected:
  sql_exchange *exchange;
  File file;
  IO_CACHE cache;
  ha_rows row_count;
  char path[FN_REFLEN];

public:
  Query_result_to_file(THD *thd, sql_exchange *ex)
    : Query_result_interceptor(thd), exchange(ex), file(-1),row_count(0L)
  { path[0]=0; }
  ~Query_result_to_file();
  void send_error(uint errcode,const char *err);
  bool send_eof();
  void cleanup();
};


#define ESCAPE_CHARS "ntrb0ZN" // keep synchronous with READ_INFO::unescape


/*
 List of all possible characters of a numeric value text representation.
*/
#define NUMERIC_CHARS ".0123456789e+-"


class Query_result_export :public Query_result_to_file {
  size_t field_term_length;
  int field_sep_char,escape_char,line_sep_char;
  int field_term_char; // first char of FIELDS TERMINATED BY or MAX_INT
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
  const CHARSET_INFO *write_cs; // output charset
public:
  Query_result_export(THD *thd, sql_exchange *ex)
    : Query_result_to_file(thd, ex) {}
  ~Query_result_export();
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  bool send_data(List<Item> &items);
};


class Query_result_dump :public Query_result_to_file {
public:
  Query_result_dump(THD *thd, sql_exchange *ex)
    : Query_result_to_file(thd, ex) {}
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  bool send_data(List<Item> &items);
};


class Query_dumpvar :public Query_result_interceptor {
  ha_rows row_count;
public:
  List<PT_select_var> var_list;
  Query_dumpvar(THD *thd)
    : Query_result_interceptor(thd), row_count(0) { var_list.empty(); }
  ~Query_dumpvar() {}
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  bool send_data(List<Item> &items);
  bool send_eof();
  virtual bool check_simple_select() const;
  void cleanup()
  {
    row_count= 0;
  }
};


/**
  Base class for result from a subquery.
*/

class Query_result_subquery :public Query_result_interceptor
{
protected:
  Item_subselect *item;
public:
  Query_result_subquery(THD *thd, Item_subselect *item_arg)
    : Query_result_interceptor(thd), item(item_arg) { }
  bool send_data(List<Item> &items)=0;
  bool send_eof() { return false; };
};

#endif // QUERY_RESULT_INCLUDED
