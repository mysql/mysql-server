#ifndef SQL_UDF_INCLUDED
#define SQL_UDF_INCLUDED

/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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

/* This file defines structures needed by udf functions */

#include <stddef.h>
#include <sys/types.h>

#include "lex_string.h"
#include "my_inttypes.h"
#include "my_table_map.h"
#include "mysql/udf_registration_types.h"

class Item;
class Item_result_field;
class String;
class THD;
class my_decimal;

struct udf_func {
  LEX_STRING name;
  Item_result returns;
  Item_udftype type;
  char *dl;
  void *dlhandle;
  Udf_func_any func;
  Udf_func_init func_init;
  Udf_func_deinit func_deinit;
  Udf_func_clear func_clear;
  Udf_func_add func_add;
  ulong usage_count;
};

class udf_handler {
 protected:
  udf_func *u_d;
  String *buffers;
  UDF_ARGS f_args;
  UDF_INIT initid;
  char *num_buffer;
  uchar error, is_null;
  bool initialized;
  Item **args;

 public:
  table_map used_tables_cache;
  bool not_original;
  udf_handler(udf_func *udf_arg)
      : u_d(udf_arg),
        buffers(0),
        error(0),
        is_null(0),
        initialized(0),
        not_original(0) {}
  ~udf_handler();
  const char *name() const { return u_d ? u_d->name.str : "?"; }
  Item_result result_type() const {
    return (Item_result)(u_d ? (u_d->returns) : STRING_RESULT);
  }
  bool get_arguments();
  bool fix_fields(THD *thd, Item_result_field *item, uint arg_count,
                  Item **args);
  void cleanup();
  double val(bool *null_value) {
    is_null = 0;
    if (get_arguments()) {
      *null_value = 1;
      return 0.0;
    }
    Udf_func_double func = (Udf_func_double)u_d->func;
    double tmp = func(&initid, &f_args, &is_null, &error);
    if (is_null || error) {
      *null_value = 1;
      return 0.0;
    }
    *null_value = 0;
    return tmp;
  }
  longlong val_int(bool *null_value) {
    is_null = 0;
    if (get_arguments()) {
      *null_value = 1;
      return 0LL;
    }
    Udf_func_longlong func = (Udf_func_longlong)u_d->func;
    longlong tmp = func(&initid, &f_args, &is_null, &error);
    if (is_null || error) {
      *null_value = 1;
      return 0LL;
    }
    *null_value = 0;
    return tmp;
  }
  my_decimal *val_decimal(bool *null_value, my_decimal *dec_buf);
  void clear() {
    is_null = 0;
    Udf_func_clear func = u_d->func_clear;
    func(&initid, &is_null, &error);
  }
  void add(bool *null_value) {
    if (get_arguments()) {
      *null_value = 1;
      return;
    }
    Udf_func_add func = u_d->func_add;
    func(&initid, &f_args, &is_null, &error);
    *null_value = (bool)(is_null || error);
  }
  String *val_str(String *str, String *save_str);
};

void udf_init_globals();
void udf_read_functions_table();
void udf_unload_udfs();
void udf_deinit_globals();
udf_func *find_udf(const char *name, size_t len = 0, bool mark_used = 0);
void free_udf(udf_func *udf);
bool mysql_create_function(THD *thd, udf_func *udf);
bool mysql_drop_function(THD *thd, const LEX_STRING *name);
ulong udf_hash_size(void);
void udf_hash_rlock(void);
void udf_hash_unlock(void);
typedef void udf_hash_for_each_func_t(udf_func *, void *);
void udf_hash_for_each(udf_hash_for_each_func_t *func, void *arg);
#endif /* SQL_UDF_INCLUDED */
