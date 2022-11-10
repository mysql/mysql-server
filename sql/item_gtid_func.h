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

/* This file defines all string functions */
#ifndef ITEM_GTID_INCLUDED
#define ITEM_GTID_INCLUDED

#include "item_func.h"
#include "item_strfunc.h"
#include "sql/parse_location.h"  // POS
#include "sql_string.h"

class Item;
class THD;
struct Parse_context;

/**
  This class is used for implementing the new wait_for_executed_gtid_set
  function and the functions related to them. This new function is independent
  of the slave threads.
*/
class Item_wait_for_executed_gtid_set final : public Item_int_func {
  typedef Item_int_func super;

  String value;

 public:
  Item_wait_for_executed_gtid_set(const POS &pos, Item *a)
      : Item_int_func(pos, a) {
    null_on_null = false;
  }
  Item_wait_for_executed_gtid_set(const POS &pos, Item *a, Item *b)
      : Item_int_func(pos, a, b) {
    null_on_null = false;
  }

  bool itemize(Parse_context *pc, Item **res) override;
  longlong val_int() override;
  const char *func_name() const override {
    return "wait_for_executed_gtid_set";
  }
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, 1)) return true;
    if (param_type_is_default(thd, 1, 2, MYSQL_TYPE_DOUBLE)) return true;
    set_nullable(true);
    return false;
  }
};

class Item_master_gtid_set_wait final : public Item_int_func {
  typedef Item_int_func super;

  String gtid_value;
  String channel_value;

 public:
  Item_master_gtid_set_wait(const POS &pos, Item *a);
  Item_master_gtid_set_wait(const POS &pos, Item *a, Item *b);
  Item_master_gtid_set_wait(const POS &pos, Item *a, Item *b, Item *c);

  bool itemize(Parse_context *pc, Item **res) override;
  longlong val_int() override;
  const char *func_name() const override {
    return "wait_until_sql_thread_after_gtids";
  }
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, 1)) return true;
    if (param_type_is_default(thd, 1, 2, MYSQL_TYPE_DOUBLE)) return true;
    if (param_type_is_default(thd, 2, 3)) return true;
    set_nullable(true);
    return false;
  }
};

class Item_func_gtid_subset final : public Item_int_func {
  String buf1;
  String buf2;

 public:
  Item_func_gtid_subset(const POS &pos, Item *a, Item *b)
      : Item_int_func(pos, a, b) {}
  longlong val_int() override;
  const char *func_name() const override { return "gtid_subset"; }
  bool resolve_type(THD *thd) override {
    if (param_type_is_default(thd, 0, ~0U)) return true;
    return false;
  }
  bool is_bool_func() const override { return true; }
};

class Item_func_gtid_subtract final : public Item_str_ascii_func {
  String buf1, buf2;

 public:
  Item_func_gtid_subtract(const POS &pos, Item *a, Item *b)
      : Item_str_ascii_func(pos, a, b) {}
  bool resolve_type(THD *) override;
  const char *func_name() const override { return "gtid_subtract"; }
  String *val_str_ascii(String *) override;
};

#endif /* ITEM_GTID_INCLUDED */
