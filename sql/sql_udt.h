/* Copyright (c) 2000, 2026, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SQL_UDT_INCLUDED
#define SQL_UDT_INCLUDED

#include "sql/create_field.h"
#include "sql/item_func.h"

struct udt_function_record;

class Create_udt_func {
 public:
  static Item *create(THD *thd, const POS &pos,
                      udt_function_record *udt_function,
                      PT_item_list *item_list);
};

class Item_udt_func : public Item_func {
  typedef Item_func super;

 public:
  Item_udt_func(const POS &pos, udt_function_record *udt_function,
                PT_item_list *opt_list);

  bool do_itemize(Parse_context *pc, Item **res) override;

  bool resolve_type_inner(THD *thd) override;

  double val_real() override;
  longlong val_int() override;
  String *val_str(String *str) override;
  bool val_date(Date_val *date, my_time_flags_t flags) override;
  bool val_time(Time_val *time) override;
  bool val_datetime(Datetime_val *dt, my_time_flags_t flags) override;
  const char *func_name() const override;

  Field *create_result_field(THD *thd);

 protected:
  type_conversion_status save_in_field_inner(Field *field,
                                             bool no_conversions) override;

 private:
  bool execute();
  bool init_result_field(THD *thd);
  bool evaluate_to_field(Field *field);

  udt_function_record *m_udt_function;

  // Fake table to hold the result field.
  TABLE m_table;
  TABLE_SHARE m_share;

  Create_field m_return_field_def;

  Field *m_return_field{nullptr};
};

void udt_init_globals();
void udt_deinit_globals();

udt_function_record *acquire_udt_function(const char *name);
void release_udt_function(udt_function_record *record);

#endif /* SQL_UDT_INCLUDED */
