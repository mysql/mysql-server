/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/* This file defines structures needed by udf functions */

#ifdef __GNUC__
#pragma interface
#endif

enum Item_udftype {UDFTYPE_FUNCTION=1,UDFTYPE_AGGREGATE};

typedef struct st_udf_func
{
  LEX_STRING name;
  Item_result returns;
  Item_udftype type;
  char *dl;
  void *dlhandle;
  void *func;
  void *func_init;
  void *func_deinit;
  void *func_clear;
  void *func_add;
  ulong usage_count;
} udf_func;

class Item_result_field;
struct st_table_list;

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
  bool fix_fields(THD *thd,struct st_table_list *tlist,Item_result_field *item,
		  uint arg_count,Item **args);
  double val(my_bool *null_value)
  {
    if (get_arguments())
    {
      *null_value=1;
      return 0.0;
    }
    double (*func)(UDF_INIT *, UDF_ARGS *, uchar *, uchar *)=
      (double (*)(UDF_INIT *, UDF_ARGS *, uchar *, uchar *)) u_d->func;
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
    if (get_arguments())
    {
      *null_value=1;
      return LL(0);
    }
    longlong (*func)(UDF_INIT *, UDF_ARGS *, uchar *, uchar *)=
      (longlong (*)(UDF_INIT *, UDF_ARGS *, uchar *, uchar *)) u_d->func;
    longlong tmp=func(&initid, &f_args, &is_null, &error);
    if (is_null || error)
    {
      *null_value=1;
      return LL(0);
    }
    *null_value=0;
    return tmp;
  }
  void clear()
  {
    is_null= 0;
    void (*func)(UDF_INIT *, uchar *, uchar *)=
    (void (*)(UDF_INIT *, uchar *, uchar *)) u_d->func_clear;
    func(&initid, &is_null, &error);
  }
  void add(my_bool *null_value)
  {
    if (get_arguments())
    {
      *null_value=1;
      return;
    }
    void (*func)(UDF_INIT *, UDF_ARGS *, uchar *, uchar *)=
    (void (*)(UDF_INIT *, UDF_ARGS *, uchar *, uchar *)) u_d->func_add;
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
