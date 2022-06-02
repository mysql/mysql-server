/* Copyright (c) 2000, 2022, Oracle and/or its affiliates.

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

/* Functions to create an item. Used by sql/sql_yacc.yy */

#ifndef ITEM_CREATE_H
#define ITEM_CREATE_H

/**
  @file sql/item_create.h
  Builder for SQL functions.
*/

#include <cstddef>

#include "field_types.h"
#include "lex_string.h"
#include "my_inttypes.h"         // uint
#include "sql/parse_location.h"  // POS

/**
  @addtogroup GROUP_PARSER
  @{
*/

class Item;
class PT_item_list;
class THD;
struct Cast_type;
struct CHARSET_INFO;
struct udf_func;
enum class Json_on_response_type : uint16;

/* For type casts */

enum Cast_target : unsigned char {
  ITEM_CAST_SIGNED_INT,
  ITEM_CAST_UNSIGNED_INT,
  ITEM_CAST_DATE,
  ITEM_CAST_TIME,
  ITEM_CAST_DATETIME,
  ITEM_CAST_YEAR,
  ITEM_CAST_CHAR,
  ITEM_CAST_DECIMAL,
  ITEM_CAST_JSON,
  ITEM_CAST_FLOAT,
  ITEM_CAST_DOUBLE,
  ITEM_CAST_POINT,
  ITEM_CAST_LINESTRING,
  ITEM_CAST_POLYGON,
  ITEM_CAST_MULTIPOINT,
  ITEM_CAST_MULTILINESTRING,
  ITEM_CAST_MULTIPOLYGON,
  ITEM_CAST_GEOMETRYCOLLECTION
};

/**
  Public function builder interface.
  The parser (sql/sql_yacc.yy) uses a factory / builder pattern to
  construct an <code>Item</code> object for each function call.
  All the concrete function builders implements this interface,
  either directly or indirectly with some adapter helpers.
  Keeping the function creation separated from the bison grammar allows
  to simplify the parser, and avoid the need to introduce a new token
  for each function, which has undesirable side effects in the grammar.
*/

class Create_func {
 public:
  /**
    The builder create method.
    Given the function name and list or arguments, this method creates
    an <code>Item</code> that represents the function call.
    In case or errors, a NULL item is returned, and an error is reported.
    Note that the <code>thd</code> object may be modified by the builder.
    In particular, the following members/methods can be set/called,
    depending on the function called and the function possible side effects.
    <ul>
      <li><code>thd->lex->binlog_row_based_if_mixed</code></li>
      <li><code>thd->lex->current_context()</code></li>
      <li><code>thd->lex->safe_to_cache_query</code></li>
      <li><code>thd->lex->uncacheable(UNCACHEABLE_SIDEEFFECT)</code></li>
      <li><code>thd->lex->uncacheable(UNCACHEABLE_RAND)</code></li>
      <li><code>thd->lex->add_time_zone_tables_to_query_tables(thd)</code></li>
    </ul>
    @param thd The current thread
    @param name The function name
    @param item_list The list of arguments to the function, can be NULL
    @return An item representing the parsed function call, or NULL
  */
  virtual Item *create_func(THD *thd, LEX_STRING name,
                            PT_item_list *item_list) = 0;

 protected:
  Create_func() = default;
  virtual ~Create_func() = default;
};

/**
  Function builder for qualified functions.
  This builder is used with functions call using a qualified function name
  syntax, as in <code>db.func(expr, expr, ...)</code>.
*/

class Create_qfunc : public Create_func {
 public:
  /**
    The builder create method, for unqualified functions.
    This builder will use the current database for the database name.
    @param thd The current thread
    @param name The function name
    @param item_list The list of arguments to the function, can be NULL
    @return An item representing the parsed function call
  */
  Item *create_func(THD *thd, LEX_STRING name,
                    PT_item_list *item_list) override;

  /**
    The builder create method, for qualified functions.
    @param thd The current thread
    @param db The database name or NULL_STR to use the default db name
    @param name The function name
    @param use_explicit_name Should the function be represented as 'db.name'?
    @param item_list The list of arguments to the function, can be NULL
    @return An item representing the parsed function call
  */
  virtual Item *create(THD *thd, LEX_STRING db, LEX_STRING name,
                       bool use_explicit_name, PT_item_list *item_list) = 0;

 protected:
  /** Constructor. */
  Create_qfunc() = default;
  /** Destructor. */
  ~Create_qfunc() override = default;
};

/**
  Find the native function builder associated with a given function name.

  @param name The native function name
  @return The native function builder associated with the name, or NULL
*/
extern Create_func *find_native_function_builder(const LEX_STRING &name);

/**
  Find the function builder for qualified functions.
  @param thd The current thread
  @return A function builder for qualified functions
*/
extern Create_qfunc *find_qualified_function_builder(THD *thd);

/**
  Function builder for User Defined Functions.
*/

class Create_udf_func : public Create_func {
 public:
  Item *create_func(THD *thd, LEX_STRING name,
                    PT_item_list *item_list) override;

  /**
    The builder create method, for User Defined Functions.
    @param thd The current thread
    @param fct The User Defined Function metadata
    @param item_list The list of arguments to the function, can be NULL
    @return An item representing the parsed function call
  */
  Item *create(THD *thd, udf_func *fct, PT_item_list *item_list);

  /** Singleton. */
  static Create_udf_func s_singleton;

 protected:
  /** Constructor. */
  Create_udf_func() = default;
  /** Destructor. */
  ~Create_udf_func() override = default;
};

/**
  Builder for cast expressions.
  @param thd The current thread
  @param pos Location of casting expression
  @param arg The item to cast
  @param type the type casted into
  @param as_array Cast to array
*/
Item *create_func_cast(THD *thd, const POS &pos, Item *arg,
                       const Cast_type &type, bool as_array);

Item *create_func_cast(THD *thd, const POS &pos, Item *a,
                       Cast_target cast_target, const CHARSET_INFO *cs_arg);

/**
  Creates an Item that represents a JSON_VALUE expression.

  @param thd        thread handler
  @param pos        the location of the expression
  @param arg        the JSON input argument to the JSON_VALUE expression
  @param path       the path to extract from the JSON document
  @param type       the target type of the JSON_VALUE expression
  @param on_empty_type     the type of the ON EMPTY clause
  @param on_empty_default  the default value specified in ON EMPTY, if any
  @param on_error_type     the type of the ON ERROR clause
  @param on_error_default  the default value specified in ON ERROR, if any
  @return an Item on success, or nullptr on error
*/
Item *create_func_json_value(THD *thd, const POS &pos, Item *arg, Item *path,
                             const Cast_type &type,
                             Json_on_response_type on_empty_type,
                             Item *on_empty_default,
                             Json_on_response_type on_error_type,
                             Item *on_error_default);

Item *create_temporal_literal(THD *thd, const char *str, size_t length,
                              const CHARSET_INFO *cs, enum_field_types type,
                              bool send_error);

/**
  Load the hash table for native functions.
  Note: this code is not thread safe, and is intended to be used at server
  startup only (before going multi-threaded)

  @retval false OK.
  @retval true An exception was caught.
*/
bool item_create_init();

/**
  Empty the hash table for native functions.
  Note: this code is not thread safe, and is intended to be used at server
  shutdown only (after thread requests have been executed).
*/
void item_create_cleanup();

/**
  @} (end of group GROUP_PARSER)
*/

#endif
