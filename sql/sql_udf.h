#ifndef SQL_UDF_INCLUDED
#define SQL_UDF_INCLUDED

/* Copyright (c) 2000, 2003-2007 MySQL AB, 2009 Sun Microsystems, Inc.
   Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */


/* This file defines structures needed by udf functions */

#ifdef USE_PRAGMA_INTERFACE
#pragma interface
#endif

enum Item_udftype {UDFTYPE_FUNCTION=1,UDFTYPE_AGGREGATE};

typedef void (*Udf_func_clear)(UDF_INIT *, uchar *, uchar *);
typedef void (*Udf_func_add)(UDF_INIT *, UDF_ARGS *, uchar *, uchar *);
typedef void (*Udf_func_deinit)(UDF_INIT*);
typedef my_bool (*Udf_func_init)(UDF_INIT *, UDF_ARGS *,  char *);
typedef void (*Udf_func_any)();
typedef double (*Udf_func_double)(UDF_INIT *, UDF_ARGS *, uchar *, uchar *);
typedef longlong (*Udf_func_longlong)(UDF_INIT *, UDF_ARGS *, uchar *,
                                      uchar *);

typedef struct st_udf_func
{
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
} udf_func;

class Item_result_field;

class udf_handler :public Sql_alloc
{
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
  bool const_item_cache;
  bool not_original;
  udf_handler(udf_func *udf_arg) :u_d(udf_arg), buffers(0), error(0),
    is_null(0), initialized(0), not_original(0)
  {}
  ~udf_handler();
  const char *name() const { return u_d ? u_d->name.str : "?"; }
  Item_result result_type () const
  { return u_d	? u_d->returns : STRING_RESULT;}
  bool get_arguments();
  bool fix_fields(THD *thd, Item_result_field *item,
		  uint arg_count, Item **args);
  void cleanup();
  double val(my_bool *null_value)
  {
    is_null= 0;
    if (get_arguments())
    {
      *null_value=1;
      return 0.0;
    }
    Udf_func_double func= (Udf_func_double) u_d->func;
    double tmp=func(&initid, &f_args, &is_null, &error);
    if (is_null || error)
    {
      *null_value=1;
      return 0.0;
    }
    *null_value=0;
    return tmp;
  }
  longlong val_int(my_bool *null_value)
  {
    is_null= 0;
    if (get_arguments())
    {
      *null_value=1;
      return LL(0);
    }
    Udf_func_longlong func= (Udf_func_longlong) u_d->func;
    longlong tmp=func(&initid, &f_args, &is_null, &error);
    if (is_null || error)
    {
      *null_value=1;
      return LL(0);
    }
    *null_value=0;
    return tmp;
  }
  my_decimal *val_decimal(my_bool *null_value, my_decimal *dec_buf);
  void clear()
  {
    is_null= 0;
    Udf_func_clear func= u_d->func_clear;
    func(&initid, &is_null, &error);
  }
  void add(my_bool *null_value)
  {
    if (get_arguments())
    {
      *null_value=1;
      return;
    }
    Udf_func_add func= u_d->func_add;
    func(&initid, &f_args, &is_null, &error);
    *null_value= (my_bool) (is_null || error);
  }
  String *val_str(String *str,String *save_str);
};


#ifdef HAVE_DLOPEN
void udf_init(void),udf_free(void);
udf_func *find_udf(const char *name, uint len=0,bool mark_used=0);
void free_udf(udf_func *udf);
int mysql_create_function(THD *thd,udf_func *udf);
int mysql_drop_function(THD *thd,const LEX_STRING *name);
#endif
#endif /* SQL_UDF_INCLUDED */
