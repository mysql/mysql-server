#ifndef PROCEDURE_INCLUDED
#define PROCEDURE_INCLUDED

/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

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

/* When using sql procedures */

#include <string.h>
#include <sys/types.h>

#include "m_ctype.h"
#include "my_inttypes.h"
#include "my_time.h"
#include "mysql/udf_registration_types.h"
#include "mysql_time.h"
#include "sql/item.h"
#include "sql_string.h"

class my_decimal;

/* Procedure items used by procedures to store values for
 * send_result_set_metadata */

class Item_proc : public Item {
 public:
  Item_proc(const char *name_par) : Item() { item_name.set(name_par); }
  enum Type type() const override { return Item::PROC_ITEM; }
  virtual void set(const char *str, size_t length, const CHARSET_INFO *cs) = 0;
  virtual void set(longlong nr) = 0;
  void set(const char *str) { set(str, strlen(str), default_charset()); }
  bool check_function_as_value_generator(uchar *args) override {
    Check_function_as_value_generator_parameters *func_arg =
        pointer_cast<Check_function_as_value_generator_parameters *>(args);
    func_arg->err_code = func_arg->is_gen_col
                             ? ER_GENERATED_COLUMN_FUNCTION_IS_NOT_ALLOWED
                             : ER_DEFAULT_VAL_GENERATED_FUNCTION_IS_NOT_ALLOWED;
    return true;
  }
};

class Item_proc_int : public Item_proc {
  longlong value;

 public:
  Item_proc_int(const char *name_par) : Item_proc(name_par) {
    set_data_type_longlong();
    max_length = 11;
  }
  enum Item_result result_type() const override { return INT_RESULT; }
  void set(longlong nr) override { value = nr; }
  void set(const char *str, size_t length, const CHARSET_INFO *cs) override {
    int err;
    value = my_strntoll(cs, str, length, 10, NULL, &err);
  }
  double val_real() override { return (double)value; }
  longlong val_int() override { return value; }
  String *val_str(String *s) override {
    s->set(value, default_charset());
    return s;
  }
  my_decimal *val_decimal(my_decimal *) override;
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) override {
    return get_date_from_int(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime) override { return get_time_from_int(ltime); }
  bool check_function_as_value_generator(uchar *args) override {
    Check_function_as_value_generator_parameters *func_arg =
        pointer_cast<Check_function_as_value_generator_parameters *>(args);
    func_arg->err_code = func_arg->is_gen_col
                             ? ER_GENERATED_COLUMN_FUNCTION_IS_NOT_ALLOWED
                             : ER_DEFAULT_VAL_GENERATED_FUNCTION_IS_NOT_ALLOWED;
    return true;
  }
};

class Item_proc_string : public Item_proc {
 public:
  Item_proc_string(const char *name_par, uint length) : Item_proc(name_par) {
    set_data_type_string(length);
  }
  enum Item_result result_type() const override { return STRING_RESULT; }
  void set(longlong nr) override { str_value.set(nr, default_charset()); }
  void set(const char *str, size_t length, const CHARSET_INFO *cs) override {
    str_value.copy(str, length, cs);
  }
  double val_real() override {
    int err_not_used;
    char *end_not_used;
    const CHARSET_INFO *cs = str_value.charset();
    return my_strntod(cs, (char *)str_value.ptr(), str_value.length(),
                      &end_not_used, &err_not_used);
  }
  longlong val_int() override {
    int err;
    const CHARSET_INFO *cs = str_value.charset();
    return my_strntoll(cs, str_value.ptr(), str_value.length(), 10, NULL, &err);
  }
  bool get_date(MYSQL_TIME *ltime, my_time_flags_t fuzzydate) override {
    return get_date_from_string(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime) override {
    return get_time_from_string(ltime);
  }
  String *val_str(String *) override {
    return null_value ? (String *)0 : &str_value;
  }
  my_decimal *val_decimal(my_decimal *) override;
};

/* The procedure class definitions */

#endif /* PROCEDURE_INCLUDED */
