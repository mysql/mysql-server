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


/* classes for sum functions */

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

#include <my_tree.h>

class Item_sum :public Item_result_field
{
public:
  enum Sumfunctype {COUNT_FUNC,COUNT_DISTINCT_FUNC,SUM_FUNC,AVG_FUNC,MIN_FUNC,
		    MAX_FUNC, UNIQUE_USERS_FUNC,STD_FUNC,SUM_BIT_FUNC,
		    UDF_SUM_FUNC };

  Item **args,*tmp_args[2];
  uint arg_count;
  bool quick_group;			/* If incremental update of fields */

  Item_sum() : arg_count(0),quick_group(1) { with_sum_func=1; }
  Item_sum(Item *a) :quick_group(1)
  {
    arg_count=1;
    args=tmp_args;
    args[0]=a;
    with_sum_func = 1;
  }
  Item_sum( Item *a, Item *b ) :quick_group(1)
  {
    arg_count=2;
    args=tmp_args;
    args[0]=a; args[1]=b;
    with_sum_func=1;
  }
  Item_sum(List<Item> &list);
  ~Item_sum() { result_field=0; }
  enum Type type() const { return SUM_FUNC_ITEM; }
  virtual enum Sumfunctype sum_func () const=0;
  virtual void reset()=0;
  virtual bool add()=0;
  virtual void reset_field()=0;
  virtual void update_field(int offset)=0;
  virtual bool keep_field_type(void) const { return 0; }
  virtual void fix_length_and_dec() { maybe_null=1; null_value=1; }
  virtual const char *func_name() const { return "?"; }
  virtual Item *result_item(Field *field)
  { return new Item_field(field);}
  table_map used_tables() const { return ~(table_map) 0; } /* Not used */
  bool const_item() const { return 0; }
  void update_used_tables() { }
  void make_field(Send_field *field);
  void print(String *str);
  void fix_num_length_and_dec();
  virtual bool setup(THD *thd) {return 0;}
};


class Item_sum_num :public Item_sum
{
public:
  Item_sum_num() :Item_sum() {}
  Item_sum_num(Item *item_par) :Item_sum(item_par) {}
  Item_sum_num(Item *a, Item* b) :Item_sum(a,b) {}
  Item_sum_num(List<Item> &list) :Item_sum(list) {}
  bool fix_fields(THD *,struct st_table_list *);
  longlong val_int() { return (longlong) val(); } /* Real as default */
  String *val_str(String*str);
  void reset_field();
};


class Item_sum_int :public Item_sum_num
{
  void fix_length_and_dec()
    { decimals=0; max_length=21; maybe_null=null_value=0; }

public:
  Item_sum_int(Item *item_par) :Item_sum_num(item_par) {}
  Item_sum_int(List<Item> &list) :Item_sum_num(list) {}
  double val() { return (double) val_int(); }
  String *val_str(String*str);
  enum Item_result result_type () const { return INT_RESULT; }
};


class Item_sum_sum :public Item_sum_num
{
  double sum;
  void fix_length_and_dec() { maybe_null=null_value=1; }

  public:
  Item_sum_sum(Item *item_par) :Item_sum_num(item_par),sum(0.0) {}
  enum Sumfunctype sum_func () const {return SUM_FUNC;}
  void reset();
  bool add();
  double val();
  void reset_field();
  void update_field(int offset);
  const char *func_name() const { return "sum"; }
};


class Item_sum_count :public Item_sum_int
{
  longlong count;
  table_map used_table_cache;

  public:
  Item_sum_count(Item *item_par)
    :Item_sum_int(item_par),count(0),used_table_cache(~(table_map) 0)
  {}
  table_map used_tables() const { return used_table_cache; }
  bool const_item() const { return !used_table_cache; }
  enum Sumfunctype sum_func () const { return COUNT_FUNC; }
  void reset();
  bool add();
  void make_const(longlong count_arg) { count=count_arg; used_table_cache=0; }
  longlong val_int();
  void reset_field();
  void update_field(int offset);
  const char *func_name() const { return "count"; }
};


class TMP_TABLE_PARAM;

class Item_sum_count_distinct :public Item_sum_int
{
  TABLE *table;
  table_map used_table_cache;
  bool fix_fields(THD *thd,TABLE_LIST *tables);
  TMP_TABLE_PARAM *tmp_table_param;
  TREE tree;
  uint max_elements_in_tree;
  // calculated based on max_heap_table_size. If reached,
  // walk the tree and dump it into MyISAM table
  
  bool use_tree;
  // If there are no blobs, we can use a tree, which
  // is faster than heap table. In that case, we still use the table
  // to help get things set up, but we insert nothing in it
  
  int rec_offset;
  // the first few bytes of record ( at least one)
  // are just markers for deleted and NULLs. We want to skip them since
  // they will just bloat the tree without providing any valuable info

  int tree_to_myisam();
  
  friend int composite_key_cmp(void* arg, byte* key1, byte* key2);
  friend int dump_leaf(byte* key, uint32 count __attribute__((unused)),
		Item_sum_count_distinct* item);

  public:
  Item_sum_count_distinct(List<Item> &list)
    :Item_sum_int(list),table(0),used_table_cache(~(table_map) 0),
     tmp_table_param(0),use_tree(0)
  { quick_group=0; }
  ~Item_sum_count_distinct();
  table_map used_tables() const { return used_table_cache; }
  enum Sumfunctype sum_func () const { return COUNT_DISTINCT_FUNC; }
  void reset();
  bool add();
  longlong val_int();
  void reset_field() { return ;}		// Never called
  void update_field(int offset) { return ; }	// Never called
  const char *func_name() const { return "count_distinct"; }
  bool setup(THD *thd);
};


/* Item to get the value of a stored sum function */

class Item_sum_avg;

class Item_avg_field :public Item_result_field
{
public:
  Field *field;
  Item_avg_field(Item_sum_avg *item);
  enum Type type() const { return FIELD_AVG_ITEM; }
  double val();
  longlong val_int() { return (longlong) val(); }
  String *val_str(String*);
  void make_field(Send_field *field);
  void fix_length_and_dec() {}
};


class Item_sum_avg :public Item_sum_num
{
  void fix_length_and_dec() { decimals+=4; maybe_null=1; }

  double sum;
  ulonglong count;

  public:
  Item_sum_avg(Item *item_par) :Item_sum_num(item_par),count(0) {}
  enum Sumfunctype sum_func () const {return AVG_FUNC;}
  void reset();
  bool add();
  double val();
  void reset_field();
  void update_field(int offset);
  Item *result_item(Field *field)
  { return new Item_avg_field(this); }
  const char *func_name() const { return "avg"; }
};

class Item_sum_std;

class Item_std_field :public Item_result_field
{
public:
  Field *field;
  Item_std_field(Item_sum_std *item);
  enum Type type() const { return FIELD_STD_ITEM; }
  double val();
  longlong val_int() { return (longlong) val(); }
  String *val_str(String*);
  void make_field(Send_field *field);
  void fix_length_and_dec() {}
};

class Item_sum_std :public Item_sum_num
{
  double sum;
  double sum_sqr;
  ulonglong count;
  void fix_length_and_dec() { decimals+=4; maybe_null=1; }

  public:
  Item_sum_std(Item *item_par) :Item_sum_num(item_par),count(0) {}
  enum Sumfunctype sum_func () const { return STD_FUNC; }
  void reset();
  bool add();
  double val();
  void reset_field();
  void update_field(int offset);
  Item *result_item(Field *field)
  { return new Item_std_field(this); }
  const char *func_name() const { return "std"; }
};


// This class is a string or number function depending on num_func

class Item_sum_hybrid :public Item_sum
{
 protected:
  String value,tmp_value;
  double sum;
  Item_result hybrid_type;
  int cmp_sign;
  table_map used_table_cache;

  public:
  Item_sum_hybrid(Item *item_par,int sign) :Item_sum(item_par),cmp_sign(sign),
    used_table_cache(~(table_map) 0)
  {}
  bool fix_fields(THD *,struct st_table_list *);
  table_map used_tables() const { return used_table_cache; }
  bool const_item() const { return !used_table_cache; }

  void reset()
  {
    sum=0.0;
    value.length(0);
    null_value=1;
    add();
  }
  double val();
  longlong val_int() { return (longlong) val(); } /* Real as default */
  void reset_field();
  String *val_str(String *);
  void make_const() { used_table_cache=0; }
  bool keep_field_type(void) const { return 1; }
  enum Item_result result_type () const { return hybrid_type; }
  void update_field(int offset);
  void min_max_update_str_field(int offset);
  void min_max_update_real_field(int offset);
  void min_max_update_int_field(int offset);
};


class Item_sum_min :public Item_sum_hybrid
{
public:
  Item_sum_min(Item *item_par) :Item_sum_hybrid(item_par,1) {}
  enum Sumfunctype sum_func () const {return MIN_FUNC;}

  bool add();
  const char *func_name() const { return "min"; }
};


class Item_sum_max :public Item_sum_hybrid
{
public:
  Item_sum_max(Item *item_par) :Item_sum_hybrid(item_par,-1) {}
  enum Sumfunctype sum_func () const {return MAX_FUNC;}

  bool add();
  const char *func_name() const { return "max"; }
};


class Item_sum_bit :public Item_sum_int
{
 protected:
  ulonglong reset_bits,bits;

  public:
  Item_sum_bit(Item *item_par,ulonglong reset_arg)
    :Item_sum_int(item_par),reset_bits(reset_arg),bits(reset_arg) {}
  enum Sumfunctype sum_func () const {return SUM_BIT_FUNC;}
  void reset();
  longlong val_int();
  void reset_field();
};


class Item_sum_or :public Item_sum_bit
{
  public:
  Item_sum_or(Item *item_par) :Item_sum_bit(item_par,LL(0)) {}
  bool add();
  void update_field(int offset);
  const char *func_name() const { return "bit_or"; }
};


class Item_sum_and :public Item_sum_bit
{
  public:
  Item_sum_and(Item *item_par) :Item_sum_bit(item_par, ~(ulonglong) LL(0)) {}
  bool add();
  void update_field(int offset);
  const char *func_name() const { return "bit_and"; }
};

/*
**	user defined aggregates
*/
#ifdef HAVE_DLOPEN

class Item_udf_sum : public Item_sum
{
protected:
  udf_handler udf;

public:
  Item_udf_sum(udf_func *udf_arg) :Item_sum(), udf(udf_arg) { quick_group=0;}
  Item_udf_sum( udf_func *udf_arg, List<Item> &list )
    :Item_sum( list ), udf(udf_arg)
  { quick_group=0;}
  ~Item_udf_sum() {}
  const char *func_name() const { return udf.name(); }
  bool fix_fields(THD *thd,struct st_table_list *tables)
  {
    return udf.fix_fields(thd,tables,this,this->arg_count,this->args);
  }
  enum Sumfunctype sum_func () const { return UDF_SUM_FUNC; }
  virtual bool have_field_update(void) const { return 0; }

  void reset();
  bool add();
  void reset_field() {};
  void update_field(int offset_arg) {};
};


class Item_sum_udf_float :public Item_udf_sum
{
 public:
  Item_sum_udf_float(udf_func *udf_arg) :Item_udf_sum(udf_arg) {}
  Item_sum_udf_float(udf_func *udf_arg, List<Item> &list)
    :Item_udf_sum(udf_arg,list) {}
  ~Item_sum_udf_float() {}
  longlong val_int() { return (longlong) Item_sum_udf_float::val(); }
  double val();
  String *val_str(String*str);
  void fix_length_and_dec() { fix_num_length_and_dec(); }
};


class Item_sum_udf_int :public Item_udf_sum
{
public:
  Item_sum_udf_int(udf_func *udf_arg) :Item_udf_sum(udf_arg) {}
  Item_sum_udf_int(udf_func *udf_arg, List<Item> &list)
    :Item_udf_sum(udf_arg,list) {}
  ~Item_sum_udf_int() {}
  longlong val_int();
  double val() { return (double) Item_sum_udf_int::val_int(); }
  String *val_str(String*str);
  enum Item_result result_type () const { return INT_RESULT; }
  void fix_length_and_dec() { decimals=0; max_length=21; }
};


class Item_sum_udf_str :public Item_udf_sum
{
public:
  Item_sum_udf_str(udf_func *udf_arg) :Item_udf_sum(udf_arg) {}
  Item_sum_udf_str(udf_func *udf_arg, List<Item> &list)
    :Item_udf_sum(udf_arg,list) {}
  ~Item_sum_udf_str() {}
  String *val_str(String *);
  double val()
  {
    String *res;  res=val_str(&str_value);
    return res ? atof(res->c_ptr()) : 0.0;
  }
  longlong val_int()
  {
    String *res;  res=val_str(&str_value);
    return res ? strtoll(res->c_ptr(),(char**) 0,10) : (longlong) 0;
  }
  enum Item_result result_type () const { return STRING_RESULT; }
  void fix_length_and_dec();
};

#else /* Dummy functions to get sql_yacc.cc compiled */

class Item_sum_udf_float :public Item_sum_num
{
 public:
  Item_sum_udf_float(udf_func *udf_arg) :Item_sum_num() {}
  Item_sum_udf_float(udf_func *udf_arg, List<Item> &list) :Item_sum_num() {}
  ~Item_sum_udf_float() {}
  enum Sumfunctype sum_func () const { return UDF_SUM_FUNC; }
  double val() { return 0.0; }
  void reset() {}
  bool add() { return 0; }  
  void update_field(int offset) {}
};


class Item_sum_udf_int :public Item_sum_num
{
public:
  Item_sum_udf_int(udf_func *udf_arg) :Item_sum_num() {}
  Item_sum_udf_int(udf_func *udf_arg, List<Item> &list) :Item_sum_num() {}
  ~Item_sum_udf_int() {}
  enum Sumfunctype sum_func () const { return UDF_SUM_FUNC; }
  longlong val_int() { return 0; }
  double val() { return 0; }
  void reset() {}
  bool add() { return 0; }  
  void update_field(int offset) {}
};


class Item_sum_udf_str :public Item_sum_num
{
public:
  Item_sum_udf_str(udf_func *udf_arg) :Item_sum_num() {}
  Item_sum_udf_str(udf_func *udf_arg, List<Item> &list)  :Item_sum_num() {}
  ~Item_sum_udf_str() {}
  String *val_str(String *) { null_value=1; return 0; }
  double val() { null_value=1; return 0.0; }
  longlong val_int() { null_value=1; return 0; }
  enum Item_result result_type () const { return STRING_RESULT; }
  void fix_length_and_dec() { maybe_null=1; max_length=0; }
  enum Sumfunctype sum_func () const { return UDF_SUM_FUNC; }
  void reset() {}
  bool add() { return 0; }  
  void update_field(int offset) {}
};

#endif /* HAVE_DLOPEN */
