/* Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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

#include "sql_check_constraint.h"

#include "binlog_event.h"  // UNDEFINED_SERVER_VERSION
#include "item_func.h"     // print
#include "mysqld_error.h"  // ER_*
#include "sql_parse.h"     // check_string_char_length
#include "sql_string.h"    // String
#include "strfunc.h"       // make_lex_string_root
#include "thd_raii.h"      // Sql_mode_parse_guard

bool Sql_check_constraint_spec::pre_validate() {
  /*
    Validate check constraint expression name. If name is not specified for the
    check constraint then name is generated before calling this method.
  */
  if (check_string_char_length(to_lex_cstring(name), "", NAME_CHAR_LEN,
                               system_charset_info, 1)) {
    my_error(ER_TOO_LONG_IDENT, MYF(0), name.str);
    return true;
  }

  /*
    If this is a column check constraint then expression should refer only its
    column.
  */
  if (column_name.length != 0) {
    if (!expr_refers_to_only_column(column_name.str)) {
      my_error(ER_COLUMN_CHECK_CONSTRAINT_REFERENCES_OTHER_COLUMN, MYF(0),
               name.str);
      return true;
    }
  }

  // Check constraint expression must be a boolean expression.
  if (!check_expr->is_bool_func()) {
    my_error(ER_NON_BOOLEAN_EXPR_FOR_CHECK_CONSTRAINT, MYF(0), name.str);
    return true;
  }

  /*
    Pre-validate expression to determine if it is allowed for the check
    constraint.
  */
  if (pre_validate_value_generator_expr(check_expr, name.str,
                                        VGS_CHECK_CONSTRAINT))
    return true;

  return false;
}

void Sql_check_constraint_spec::print_expr(THD *thd, String &out) {
  out.length(0);
  Sql_mode_parse_guard parse_guard(thd);
  auto flags = enum_query_type(QT_NO_DB | QT_NO_TABLE | QT_FORCE_INTRODUCERS);
  check_expr->print(thd, &out, flags);
}

bool Sql_check_constraint_spec::expr_refers_column(const char *column_name) {
  List<Item_field> fields;
  check_expr->walk(&Item::collect_item_field_processor, enum_walk::POSTFIX,
                   (uchar *)&fields);

  Item_field *cur_item;
  List_iterator<Item_field> fields_it(fields);
  while ((cur_item = fields_it++)) {
    if (cur_item->type() == Item::FIELD_ITEM &&
        !my_strcasecmp(system_charset_info, cur_item->field_name, column_name))
      return true;
  }
  return false;
}

bool Sql_check_constraint_spec::expr_refers_to_only_column(
    const char *column_name) {
  List<Item_field> fields;
  check_expr->walk(&Item::collect_item_field_processor, enum_walk::POSTFIX,
                   (uchar *)&fields);

  // Expression does not refer to any columns.
  if (fields.elements == 0) return false;

  Item_field *cur_item;
  List_iterator<Item_field> fields_it(fields);
  while ((cur_item = fields_it++)) {
    // Expression refers to some other column.
    if (cur_item->type() == Item::FIELD_ITEM &&
        my_strcasecmp(system_charset_info, cur_item->field_name, column_name))
      return false;
  }
  return true;
}

Sql_table_check_constraint::~Sql_table_check_constraint() {
  if (m_val_gen != nullptr) delete m_val_gen;
}

bool is_slave_with_master_without_check_constraints_support(THD *thd) {
  return ((thd->system_thread &
           (SYSTEM_THREAD_SLAVE_SQL | SYSTEM_THREAD_SLAVE_WORKER)) &&
          (thd->variables.original_server_version == UNDEFINED_SERVER_VERSION ||
           thd->variables.original_server_version < 80016));
}
