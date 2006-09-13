/* Copyright (C) 2000-2003 MySQL AB

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


/* compare and test functions */

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

extern Item_result item_cmp_type(Item_result a,Item_result b);
class Item_bool_func2;
class Arg_comparator;

typedef int (Arg_comparator::*arg_cmp_func)();

typedef int (*Item_field_cmpfunc)(Item_field *f1, Item_field *f2, void *arg); 

class Arg_comparator: public Sql_alloc
{
  Item **a, **b;
  arg_cmp_func func;
  Item_bool_func2 *owner;
  Arg_comparator *comparators;   // used only for compare_row()

public:
  DTCollation cmp_collation;

  Arg_comparator() {};
  Arg_comparator(Item **a1, Item **a2): a(a1), b(a2) {};

  int set_compare_func(Item_bool_func2 *owner, Item_result type);
  inline int set_compare_func(Item_bool_func2 *owner_arg)
  {
    return set_compare_func(owner_arg, item_cmp_type((*a)->result_type(),
                                                     (*b)->result_type()));
  }
  inline int set_cmp_func(Item_bool_func2 *owner_arg,
			  Item **a1, Item **a2,
			  Item_result type)
  {
    a= a1;
    b= a2;
    return set_compare_func(owner_arg, type);
  }
  inline int set_cmp_func(Item_bool_func2 *owner_arg,
			  Item **a1, Item **a2)
  {
    return set_cmp_func(owner_arg, a1, a2,
                        item_cmp_type((*a1)->result_type(),
                                      (*a2)->result_type()));
  }
  inline int compare() { return (this->*func)(); }

  int compare_string();		 // compare args[0] & args[1]
  int compare_binary_string();	 // compare args[0] & args[1]
  int compare_real();            // compare args[0] & args[1]
  int compare_decimal();         // compare args[0] & args[1]
  int compare_int_signed();      // compare args[0] & args[1]
  int compare_int_signed_unsigned();
  int compare_int_unsigned_signed();
  int compare_int_unsigned();
  int compare_row();             // compare args[0] & args[1]
  int compare_e_string();	 // compare args[0] & args[1]
  int compare_e_binary_string(); // compare args[0] & args[1]
  int compare_e_real();          // compare args[0] & args[1]
  int compare_e_decimal();       // compare args[0] & args[1]
  int compare_e_int();           // compare args[0] & args[1]
  int compare_e_int_diff_signedness();
  int compare_e_row();           // compare args[0] & args[1]

  static arg_cmp_func comparator_matrix [5][2];

  friend class Item_func;
};

class Item_bool_func :public Item_int_func
{
public:
  Item_bool_func() :Item_int_func() {}
  Item_bool_func(Item *a) :Item_int_func(a) {}
  Item_bool_func(Item *a,Item *b) :Item_int_func(a,b) {}
  Item_bool_func(THD *thd, Item_bool_func *item) :Item_int_func(thd, item) {}
  bool is_bool_func() { return 1; }
  void fix_length_and_dec() { decimals=0; max_length=1; }
  uint decimal_precision() const { return 1; }
};

class Item_cache;
class Item_in_optimizer: public Item_bool_func
{
protected:
  Item_cache *cache;
  bool save_cache;
public:
  Item_in_optimizer(Item *a, Item_in_subselect *b):
    Item_bool_func(a, my_reinterpret_cast(Item *)(b)), cache(0), save_cache(0)
  {}
  bool fix_fields(THD *, Item **);
  bool fix_left(THD *thd, Item **ref);
  bool is_null();
  /*
    Item_in_optimizer item is special boolean function. On value request 
    (one of val, val_int or val_str methods) it evaluate left expression 
    of IN by storing it value in cache item (one of Item_cache* items), 
    then it test cache is it NULL. If left expression (cache) is NULL then
    Item_in_optimizer return NULL, else it evaluate Item_in_subselect.
  */
  longlong val_int();
  void cleanup();
  const char *func_name() const { return "<in_optimizer>"; }
  Item_cache **get_cache() { return &cache; }
  void keep_top_level_cache();
};

class Comp_creator
{
public:
  Comp_creator() {}                           /* Remove gcc warning */
  virtual ~Comp_creator() {}                  /* Remove gcc warning */
  virtual Item_bool_func2* create(Item *a, Item *b) const = 0;
  virtual const char* symbol(bool invert) const = 0;
  virtual bool eqne_op() const = 0;
  virtual bool l_op() const = 0;
};

class Eq_creator :public Comp_creator
{
public:
  Eq_creator() {}                             /* Remove gcc warning */
  virtual ~Eq_creator() {}                    /* Remove gcc warning */
  virtual Item_bool_func2* create(Item *a, Item *b) const;
  virtual const char* symbol(bool invert) const { return invert? "<>" : "="; }
  virtual bool eqne_op() const { return 1; }
  virtual bool l_op() const { return 0; }
};

class Ne_creator :public Comp_creator
{
public:
  Ne_creator() {}                             /* Remove gcc warning */
  virtual ~Ne_creator() {}                    /* Remove gcc warning */
  virtual Item_bool_func2* create(Item *a, Item *b) const;
  virtual const char* symbol(bool invert) const { return invert? "=" : "<>"; }
  virtual bool eqne_op() const { return 1; }
  virtual bool l_op() const { return 0; }
};

class Gt_creator :public Comp_creator
{
public:
  Gt_creator() {}                             /* Remove gcc warning */
  virtual ~Gt_creator() {}                    /* Remove gcc warning */
  virtual Item_bool_func2* create(Item *a, Item *b) const;
  virtual const char* symbol(bool invert) const { return invert? "<=" : ">"; }
  virtual bool eqne_op() const { return 0; }
  virtual bool l_op() const { return 0; }
};

class Lt_creator :public Comp_creator
{
public:
  Lt_creator() {}                             /* Remove gcc warning */
  virtual ~Lt_creator() {}                    /* Remove gcc warning */
  virtual Item_bool_func2* create(Item *a, Item *b) const;
  virtual const char* symbol(bool invert) const { return invert? ">=" : "<"; }
  virtual bool eqne_op() const { return 0; }
  virtual bool l_op() const { return 1; }
};

class Ge_creator :public Comp_creator
{
public:
  Ge_creator() {}                             /* Remove gcc warning */
  virtual ~Ge_creator() {}                    /* Remove gcc warning */
  virtual Item_bool_func2* create(Item *a, Item *b) const;
  virtual const char* symbol(bool invert) const { return invert? "<" : ">="; }
  virtual bool eqne_op() const { return 0; }
  virtual bool l_op() const { return 0; }
};

class Le_creator :public Comp_creator
{
public:
  Le_creator() {}                             /* Remove gcc warning */
  virtual ~Le_creator() {}                    /* Remove gcc warning */
  virtual Item_bool_func2* create(Item *a, Item *b) const;
  virtual const char* symbol(bool invert) const { return invert? ">" : "<="; }
  virtual bool eqne_op() const { return 0; }
  virtual bool l_op() const { return 1; }
};

class Item_bool_func2 :public Item_int_func
{						/* Bool with 2 string args */
protected:
  Arg_comparator cmp;
  String tmp_value1,tmp_value2;
  bool abort_on_null;

public:
  Item_bool_func2(Item *a,Item *b)
    :Item_int_func(a,b), cmp(tmp_arg, tmp_arg+1), abort_on_null(FALSE) {}
  void fix_length_and_dec();
  void set_cmp_func()
  {
    cmp.set_cmp_func(this, tmp_arg, tmp_arg+1);
  }
  optimize_type select_optimize() const { return OPTIMIZE_OP; }
  virtual enum Functype rev_functype() const { return UNKNOWN_FUNC; }
  bool have_rev_func() const { return rev_functype() != UNKNOWN_FUNC; }
  void print(String *str) { Item_func::print_op(str); }
  bool is_null() { return test(args[0]->is_null() || args[1]->is_null()); }
  bool is_bool_func() { return 1; }
  CHARSET_INFO *compare_collation() { return cmp.cmp_collation.collation; }
  uint decimal_precision() const { return 1; }
  void top_level_item() { abort_on_null=1; }

  friend class  Arg_comparator;
};

class Item_bool_rowready_func2 :public Item_bool_func2
{
public:
  Item_bool_rowready_func2(Item *a, Item *b) :Item_bool_func2(a, b)
  {
    allowed_arg_cols= 0;  // Fetch this value from first argument
  }
  Item *neg_transformer(THD *thd);
  virtual Item *negated_item();
};

class Item_func_not :public Item_bool_func
{
public:
  Item_func_not(Item *a) :Item_bool_func(a) {}
  longlong val_int();
  enum Functype functype() const { return NOT_FUNC; }
  const char *func_name() const { return "not"; }
  Item *neg_transformer(THD *thd);
};

class Item_maxmin_subselect;

/*
  The class Item_func_trig_cond is used for guarded predicates 
  which are employed only for internal purposes.
  A guarded predicates is an object consisting of an a regular or
  a guarded predicate P and a pointer to a boolean guard variable g. 
  A guarded predicate P/g is evaluated to true if the value of the
  guard g is false, otherwise it is evaluated to the same value that
  the predicate P: val(P/g)= g ? val(P):true.
  Guarded predicates allow us to include predicates into a conjunction
  conditionally. Currently they are utilized for pushed down predicates
  in queries with outer join operations.

  In the future, probably, it makes sense to extend this class to
  the objects consisting of three elements: a predicate P, a pointer
  to a variable g and a firing value s with following evaluation
  rule: val(P/g,s)= g==s? val(P) : true. It will allow us to build only
  one item for the objects of the form P/g1/g2... 

  Objects of this class are built only for query execution after
  the execution plan has been already selected. That's why this
  class needs only val_int out of generic methods. 
*/

class Item_func_trig_cond: public Item_bool_func
{
  bool *trig_var;
public:
  Item_func_trig_cond(Item *a, bool *f) : Item_bool_func(a) { trig_var= f; }
  longlong val_int() { return *trig_var ? args[0]->val_int() : 1; }
  enum Functype functype() const { return TRIG_COND_FUNC; };
  const char *func_name() const { return "trigcond"; };
  bool const_item() const { return FALSE; }
};

class Item_func_not_all :public Item_func_not
{
  /* allow to check presence of values in max/min optimization */
  Item_sum_hybrid *test_sum_item;
  Item_maxmin_subselect *test_sub_item;

  bool abort_on_null;
public:
  bool show;

  Item_func_not_all(Item *a)
    :Item_func_not(a), test_sum_item(0), test_sub_item(0), abort_on_null(0),
     show(0)
    {}
  virtual void top_level_item() { abort_on_null= 1; }
  bool top_level() { return abort_on_null; }
  longlong val_int();
  enum Functype functype() const { return NOT_ALL_FUNC; }
  const char *func_name() const { return "<not>"; }
  void print(String *str);
  void set_sum_test(Item_sum_hybrid *item) { test_sum_item= item; };
  void set_sub_test(Item_maxmin_subselect *item) { test_sub_item= item; };
  bool empty_underlying_subquery();
  Item *neg_transformer(THD *thd);
};


class Item_func_nop_all :public Item_func_not_all
{
public:

  Item_func_nop_all(Item *a) :Item_func_not_all(a) {}
  longlong val_int();
  const char *func_name() const { return "<nop>"; }
  Item *neg_transformer(THD *thd);
};


class Item_func_eq :public Item_bool_rowready_func2
{
public:
  Item_func_eq(Item *a,Item *b) :Item_bool_rowready_func2(a,b) {}
  longlong val_int();
  enum Functype functype() const { return EQ_FUNC; }
  enum Functype rev_functype() const { return EQ_FUNC; }
  cond_result eq_cmp_result() const { return COND_TRUE; }
  const char *func_name() const { return "="; }
  Item *negated_item();
};

class Item_func_equal :public Item_bool_rowready_func2
{
public:
  Item_func_equal(Item *a,Item *b) :Item_bool_rowready_func2(a,b) {};
  longlong val_int();
  void fix_length_and_dec();
  table_map not_null_tables() const { return 0; }
  enum Functype functype() const { return EQUAL_FUNC; }
  enum Functype rev_functype() const { return EQUAL_FUNC; }
  cond_result eq_cmp_result() const { return COND_TRUE; }
  const char *func_name() const { return "<=>"; }
  Item *neg_transformer(THD *thd) { return 0; }
};


class Item_func_ge :public Item_bool_rowready_func2
{
public:
  Item_func_ge(Item *a,Item *b) :Item_bool_rowready_func2(a,b) {};
  longlong val_int();
  enum Functype functype() const { return GE_FUNC; }
  enum Functype rev_functype() const { return LE_FUNC; }
  cond_result eq_cmp_result() const { return COND_TRUE; }
  const char *func_name() const { return ">="; }
  Item *negated_item();
};


class Item_func_gt :public Item_bool_rowready_func2
{
public:
  Item_func_gt(Item *a,Item *b) :Item_bool_rowready_func2(a,b) {};
  longlong val_int();
  enum Functype functype() const { return GT_FUNC; }
  enum Functype rev_functype() const { return LT_FUNC; }
  cond_result eq_cmp_result() const { return COND_FALSE; }
  const char *func_name() const { return ">"; }
  Item *negated_item();
};


class Item_func_le :public Item_bool_rowready_func2
{
public:
  Item_func_le(Item *a,Item *b) :Item_bool_rowready_func2(a,b) {};
  longlong val_int();
  enum Functype functype() const { return LE_FUNC; }
  enum Functype rev_functype() const { return GE_FUNC; }
  cond_result eq_cmp_result() const { return COND_TRUE; }
  const char *func_name() const { return "<="; }
  Item *negated_item();
};


class Item_func_lt :public Item_bool_rowready_func2
{
public:
  Item_func_lt(Item *a,Item *b) :Item_bool_rowready_func2(a,b) {}
  longlong val_int();
  enum Functype functype() const { return LT_FUNC; }
  enum Functype rev_functype() const { return GT_FUNC; }
  cond_result eq_cmp_result() const { return COND_FALSE; }
  const char *func_name() const { return "<"; }
  Item *negated_item();
};


class Item_func_ne :public Item_bool_rowready_func2
{
public:
  Item_func_ne(Item *a,Item *b) :Item_bool_rowready_func2(a,b) {}
  longlong val_int();
  enum Functype functype() const { return NE_FUNC; }
  cond_result eq_cmp_result() const { return COND_FALSE; }
  optimize_type select_optimize() const { return OPTIMIZE_KEY; } 
  const char *func_name() const { return "<>"; }
  Item *negated_item();
};


/*
  The class Item_func_opt_neg is defined to factor out the functionality
  common for the classes Item_func_between and Item_func_in. The objects
  of these classes can express predicates or there negations.
  The alternative approach would be to create pairs Item_func_between,
  Item_func_notbetween and Item_func_in, Item_func_notin.

*/

class Item_func_opt_neg :public Item_int_func
{
public:
  bool negated;     /* <=> the item represents NOT <func> */
  bool pred_level;  /* <=> [NOT] <func> is used on a predicate level */
public:
  Item_func_opt_neg(Item *a, Item *b, Item *c)
    :Item_int_func(a, b, c), negated(0), pred_level(0) {}
  Item_func_opt_neg(List<Item> &list)
    :Item_int_func(list), negated(0), pred_level(0) {}
public:
  inline void negate() { negated= !negated; }
  inline void top_level_item() { pred_level= 1; }
  Item *neg_transformer(THD *thd)
  {
    negated= !negated;
    return this;
  }
};


class Item_func_between :public Item_func_opt_neg
{
  DTCollation cmp_collation;
public:
  Item_result cmp_type;
  String value0,value1,value2;
  Item_func_between(Item *a, Item *b, Item *c)
    :Item_func_opt_neg(a, b, c) {}
  longlong val_int();
  optimize_type select_optimize() const { return OPTIMIZE_KEY; }
  enum Functype functype() const   { return BETWEEN; }
  const char *func_name() const { return "between"; }
  bool fix_fields(THD *, Item **);
  void fix_length_and_dec();
  void print(String *str);
  bool is_bool_func() { return 1; }
  CHARSET_INFO *compare_collation() { return cmp_collation.collation; }
  uint decimal_precision() const { return 1; }
};


class Item_func_strcmp :public Item_bool_func2
{
public:
  Item_func_strcmp(Item *a,Item *b) :Item_bool_func2(a,b) {}
  longlong val_int();
  optimize_type select_optimize() const { return OPTIMIZE_NONE; }
  const char *func_name() const { return "strcmp"; }
  void print(String *str) { Item_func::print(str); }
};


struct interval_range
{
  Item_result type;
  double dbl;
  my_decimal dec;
};

class Item_func_interval :public Item_int_func
{
  Item_row *row;
  my_bool use_decimal_comparison;
  interval_range *intervals;
public:
  Item_func_interval(Item_row *a)
    :Item_int_func(a),row(a),intervals(0)
  {
    allowed_arg_cols= 0;    // Fetch this value from first argument
  }
  longlong val_int();
  void fix_length_and_dec();
  const char *func_name() const { return "interval"; }
  uint decimal_precision() const { return 2; }
};


class Item_func_coalesce :public Item_func_numhybrid
{
protected:
  Item_func_coalesce(Item *a, Item *b) :Item_func_numhybrid(a, b) {}
public:
  Item_func_coalesce(List<Item> &list) :Item_func_numhybrid(list) {}
  double real_op();
  longlong int_op();
  String *str_op(String *);
  my_decimal *decimal_op(my_decimal *);
  void fix_length_and_dec();
  void find_num_type() {}
  enum Item_result result_type () const { return hybrid_type; }
  const char *func_name() const { return "coalesce"; }
  table_map not_null_tables() const { return 0; }
};


class Item_func_ifnull :public Item_func_coalesce
{
protected:
  enum_field_types cached_field_type;
  bool field_type_defined;
public:
  Item_func_ifnull(Item *a, Item *b) :Item_func_coalesce(a,b) {}
  double real_op();
  longlong int_op();
  String *str_op(String *str);
  my_decimal *decimal_op(my_decimal *);
  enum_field_types field_type() const;
  void fix_length_and_dec();
  const char *func_name() const { return "ifnull"; }
  Field *tmp_table_field(TABLE *table);
  uint decimal_precision() const;
};


class Item_func_if :public Item_func
{
  enum Item_result cached_result_type;
public:
  Item_func_if(Item *a,Item *b,Item *c)
    :Item_func(a,b,c), cached_result_type(INT_RESULT)
  {}
  double val_real();
  longlong val_int();
  String *val_str(String *str);
  my_decimal *val_decimal(my_decimal *);
  enum Item_result result_type () const { return cached_result_type; }
  bool fix_fields(THD *, Item **);
  void fix_length_and_dec();
  uint decimal_precision() const;
  const char *func_name() const { return "if"; }
};


class Item_func_nullif :public Item_bool_func2
{
  enum Item_result cached_result_type;
public:
  Item_func_nullif(Item *a,Item *b)
    :Item_bool_func2(a,b), cached_result_type(INT_RESULT)
  {}
  double val_real();
  longlong val_int();
  String *val_str(String *str);
  my_decimal *val_decimal(my_decimal *);
  enum Item_result result_type () const { return cached_result_type; }
  void fix_length_and_dec();
  uint decimal_precision() const { return args[0]->decimal_precision(); }
  const char *func_name() const { return "nullif"; }
  void print(String *str) { Item_func::print(str); }
  table_map not_null_tables() const { return 0; }
  bool is_null();
};


class Item_func_case :public Item_func
{
  int first_expr_num, else_expr_num;
  enum Item_result cached_result_type;
  String tmp_value;
  uint ncases;
  Item_result cmp_type;
  DTCollation cmp_collation;
public:
  Item_func_case(List<Item> &list, Item *first_expr_arg, Item *else_expr_arg)
    :Item_func(), first_expr_num(-1), else_expr_num(-1),
    cached_result_type(INT_RESULT)
  {
    ncases= list.elements;
    if (first_expr_arg)
    {
      first_expr_num= list.elements;
      list.push_back(first_expr_arg);
    }
    if (else_expr_arg)
    {
      else_expr_num= list.elements;
      list.push_back(else_expr_arg);
    }
    set_arguments(list);
  }
  double val_real();
  longlong val_int();
  String *val_str(String *);
  my_decimal *val_decimal(my_decimal *);
  bool fix_fields(THD *thd, Item **ref);
  void fix_length_and_dec();
  uint decimal_precision() const;
  table_map not_null_tables() const { return 0; }
  enum Item_result result_type () const { return cached_result_type; }
  const char *func_name() const { return "case"; }
  void print(String *str);
  Item *find_item(String *str);
  CHARSET_INFO *compare_collation() { return cmp_collation.collation; }
};


/* Functions to handle the optimized IN */


/* A vector of values of some type  */

class in_vector :public Sql_alloc
{
public:
  char *base;
  uint size;
  qsort2_cmp compare;
  CHARSET_INFO *collation;
  uint count;
  uint used_count;
  in_vector() {}
  in_vector(uint elements,uint element_length,qsort2_cmp cmp_func, 
  	    CHARSET_INFO *cmp_coll)
    :base((char*) sql_calloc(elements*element_length)),
     size(element_length), compare(cmp_func), collation(cmp_coll),
     count(elements), used_count(elements) {}
  virtual ~in_vector() {}
  virtual void set(uint pos,Item *item)=0;
  virtual byte *get_value(Item *item)=0;
  void sort()
  {
    qsort2(base,used_count,size,compare,collation);
  }
  int find(Item *item);
  
  /* 
    Create an instance of Item_{type} (e.g. Item_decimal) constant object
    which type allows it to hold an element of this vector without any
    conversions.
    The purpose of this function is to be able to get elements of this
    vector in form of Item_xxx constants without creating Item_xxx object
    for every array element you get (i.e. this implements "FlyWeight" pattern)
  */
  virtual Item* create_item() { return NULL; }
  
  /*
    Store the value at position #pos into provided item object
    SYNOPSIS
      value_to_item()
        pos   Index of value to store
        item  Constant item to store value into. The item must be of the same
              type that create_item() returns.
  */
  virtual void value_to_item(uint pos, Item *item) { }
  
  /* Compare values number pos1 and pos2 for equality */
  bool compare_elems(uint pos1, uint pos2)
  {
    return test(compare(collation, base + pos1*size, base + pos2*size));
  }
};

class in_string :public in_vector
{
  char buff[STRING_BUFFER_USUAL_SIZE];
  String tmp;
public:
  in_string(uint elements,qsort2_cmp cmp_func, CHARSET_INFO *cs);
  ~in_string();
  void set(uint pos,Item *item);
  byte *get_value(Item *item);
  Item* create_item()
  { 
    return new Item_string(collation);
  }
  void value_to_item(uint pos, Item *item)
  {    
    String *str=((String*) base)+pos;
    Item_string *to= (Item_string*)item;
    to->str_value= *str;
  }
};

class in_longlong :public in_vector
{
  longlong tmp;
public:
  in_longlong(uint elements);
  void set(uint pos,Item *item);
  byte *get_value(Item *item);
  
  Item* create_item()
  { 
    /* 
      We're created a signed INT, this may not be correct in 
      general case (see BUG#19342).
    */
    return new Item_int((longlong)0);
  }
  void value_to_item(uint pos, Item *item)
  {
    ((Item_int*)item)->value= ((longlong*)base)[pos];
  }
};

class in_double :public in_vector
{
  double tmp;
public:
  in_double(uint elements);
  void set(uint pos,Item *item);
  byte *get_value(Item *item);
  Item *create_item()
  { 
    return new Item_float(0.0);
  }
  void value_to_item(uint pos, Item *item)
  {
    ((Item_float*)item)->value= ((double*) base)[pos];
  }

};

class in_decimal :public in_vector
{
  my_decimal val;
public:
  in_decimal(uint elements);
  void set(uint pos, Item *item);
  byte *get_value(Item *item);
  Item *create_item()
  { 
    return new Item_decimal(0, FALSE);
  }
  void value_to_item(uint pos, Item *item)
  {
    my_decimal *dec= ((my_decimal *)base) + pos;
    Item_decimal *item_dec= (Item_decimal*)item;
    item_dec->set_decimal_value(dec);
  }
};


/*
** Classes for easy comparing of non const items
*/

class cmp_item :public Sql_alloc
{
public:
  CHARSET_INFO *cmp_charset;
  cmp_item() { cmp_charset= &my_charset_bin; }
  virtual ~cmp_item() {}
  virtual void store_value(Item *item)= 0;
  virtual int cmp(Item *item)= 0;
  // for optimized IN with row
  virtual int compare(cmp_item *item)= 0;
  static cmp_item* get_comparator(Item_result type, CHARSET_INFO *cs);
  virtual cmp_item *make_same()= 0;
  virtual void store_value_by_template(cmp_item *tmpl, Item *item)
  {
    store_value(item);
  }
};

class cmp_item_string :public cmp_item 
{
protected:
  String *value_res;
public:
  cmp_item_string (CHARSET_INFO *cs) { cmp_charset= cs; }
  friend class cmp_item_sort_string;
  friend class cmp_item_sort_string_in_static;
};

class cmp_item_sort_string :public cmp_item_string
{
protected:
  char value_buff[STRING_BUFFER_USUAL_SIZE];
  String value;
public:
  cmp_item_sort_string(CHARSET_INFO *cs):
    cmp_item_string(cs),
    value(value_buff, sizeof(value_buff), cs) {}
  void store_value(Item *item)
  {
    value_res= item->val_str(&value);
  }
  int cmp(Item *arg)
  {
    char buff[STRING_BUFFER_USUAL_SIZE];
    String tmp(buff, sizeof(buff), cmp_charset), *res;
    res= arg->val_str(&tmp);
    return (value_res ? (res ? sortcmp(value_res, res, cmp_charset) : 1) :
            (res ? -1 : 0));
  }
  int compare(cmp_item *c)
  {
    cmp_item_string *cmp= (cmp_item_string *)c;
    return sortcmp(value_res, cmp->value_res, cmp_charset);
  } 
  cmp_item *make_same();
};

class cmp_item_int :public cmp_item
{
  longlong value;
public:
  cmp_item_int() {}                           /* Remove gcc warning */
  void store_value(Item *item)
  {
    value= item->val_int();
  }
  int cmp(Item *arg)
  {
    return value != arg->val_int();
  }
  int compare(cmp_item *c)
  {
    cmp_item_int *cmp= (cmp_item_int *)c;
    return (value < cmp->value) ? -1 : ((value == cmp->value) ? 0 : 1);
  }
  cmp_item *make_same();
};

class cmp_item_real :public cmp_item
{
  double value;
public:
  cmp_item_real() {}                          /* Remove gcc warning */
  void store_value(Item *item)
  {
    value= item->val_real();
  }
  int cmp(Item *arg)
  {
    return value != arg->val_real();
  }
  int compare(cmp_item *c)
  {
    cmp_item_real *cmp= (cmp_item_real *)c;
    return (value < cmp->value)? -1 : ((value == cmp->value) ? 0 : 1);
  }
  cmp_item *make_same();
};


class cmp_item_decimal :public cmp_item
{
  my_decimal value;
public:
  cmp_item_decimal() {}                       /* Remove gcc warning */
  void store_value(Item *item);
  int cmp(Item *arg);
  int compare(cmp_item *c);
  cmp_item *make_same();
};


class cmp_item_row :public cmp_item
{
  cmp_item **comparators;
  uint n;
public:
  cmp_item_row(): comparators(0), n(0) {}
  ~cmp_item_row();
  void store_value(Item *item);
  int cmp(Item *arg);
  int compare(cmp_item *arg);
  cmp_item *make_same();
  void store_value_by_template(cmp_item *tmpl, Item *);
};


class in_row :public in_vector
{
  cmp_item_row tmp;
public:
  in_row(uint elements, Item *);
  ~in_row();
  void set(uint pos,Item *item);
  byte *get_value(Item *item);
};

/* 
   cmp_item for optimized IN with row (right part string, which never
   be changed)
*/

class cmp_item_sort_string_in_static :public cmp_item_string
{
 protected:
  String value;
public:
  cmp_item_sort_string_in_static(CHARSET_INFO *cs):
    cmp_item_string(cs) {}
  void store_value(Item *item)
  {
    value_res= item->val_str(&value);
  }
  int cmp(Item *item)
  {
    // Should never be called
    DBUG_ASSERT(0);
    return 1;
  }
  int compare(cmp_item *c)
  {
    cmp_item_string *cmp= (cmp_item_string *)c;
    return sortcmp(value_res, cmp->value_res, cmp_charset);
  }
  cmp_item *make_same()
  {
    return new cmp_item_sort_string_in_static(cmp_charset);
  }
};

class Item_func_in :public Item_func_opt_neg
{
public:
  Item_result cmp_type;
  in_vector *array;
  cmp_item *in_item;
  bool have_null;
  DTCollation cmp_collation;

  Item_func_in(List<Item> &list)
    :Item_func_opt_neg(list), array(0), in_item(0), have_null(0)
  {
    allowed_arg_cols= 0;  // Fetch this value from first argument
  }
  longlong val_int();
  bool fix_fields(THD *, Item **);
  void fix_length_and_dec();
  uint decimal_precision() const { return 1; }
  void cleanup()
  {
    DBUG_ENTER("Item_func_in::cleanup");
    Item_int_func::cleanup();
    delete array;
    delete in_item;
    array= 0;
    in_item= 0;
    DBUG_VOID_RETURN;
  }
  optimize_type select_optimize() const
    { return array ? OPTIMIZE_KEY : OPTIMIZE_NONE; }
  void print(String *str);
  enum Functype functype() const { return IN_FUNC; }
  const char *func_name() const { return " IN "; }
  bool nulls_in_row();
  bool is_bool_func() { return 1; }
  CHARSET_INFO *compare_collation() { return cmp_collation.collation; }
};

/* Functions used by where clause */

class Item_func_isnull :public Item_bool_func
{
protected:
  longlong cached_value;
public:
  Item_func_isnull(Item *a) :Item_bool_func(a) {}
  longlong val_int();
  enum Functype functype() const { return ISNULL_FUNC; }
  void fix_length_and_dec()
  {
    decimals=0; max_length=1; maybe_null=0;
    update_used_tables();
  }
  const char *func_name() const { return "isnull"; }
  /* Optimize case of not_null_column IS NULL */
  virtual void update_used_tables()
  {
    if (!args[0]->maybe_null)
    {
      used_tables_cache= 0;			/* is always false */
      const_item_cache= 1;
      cached_value= (longlong) 0;
    }
    else
    {
      args[0]->update_used_tables();
      if ((const_item_cache= !(used_tables_cache= args[0]->used_tables())))
      {
	/* Remember if the value is always NULL or never NULL */
	cached_value= (longlong) args[0]->is_null();
      }
    }
  }
  table_map not_null_tables() const { return 0; }
  optimize_type select_optimize() const { return OPTIMIZE_NULL; }
  Item *neg_transformer(THD *thd);
  CHARSET_INFO *compare_collation() { return args[0]->collation.collation; }
};

/* Functions used by HAVING for rewriting IN subquery */

class Item_in_subselect;
class Item_is_not_null_test :public Item_func_isnull
{
  Item_in_subselect* owner;
public:
  Item_is_not_null_test(Item_in_subselect* ow, Item *a)
    :Item_func_isnull(a), owner(ow)
  {}
  enum Functype functype() const { return ISNOTNULLTEST_FUNC; }
  longlong val_int();
  const char *func_name() const { return "<is_not_null_test>"; }
  void update_used_tables();
  /*
    we add RAND_TABLE_BIT to prevent moving this item from HAVING to WHERE
  */
  table_map used_tables() const
    { return used_tables_cache | RAND_TABLE_BIT; }
};


class Item_func_isnotnull :public Item_bool_func
{
  bool abort_on_null;
public:
  Item_func_isnotnull(Item *a) :Item_bool_func(a), abort_on_null(0) {}
  longlong val_int();
  enum Functype functype() const { return ISNOTNULL_FUNC; }
  void fix_length_and_dec()
  {
    decimals=0; max_length=1; maybe_null=0;
  }
  const char *func_name() const { return "isnotnull"; }
  optimize_type select_optimize() const { return OPTIMIZE_NULL; }
  table_map not_null_tables() const
  { return abort_on_null ? not_null_tables_cache : 0; }
  Item *neg_transformer(THD *thd);
  void print(String *str);
  CHARSET_INFO *compare_collation() { return args[0]->collation.collation; }
  void top_level_item() { abort_on_null=1; }
};


class Item_func_like :public Item_bool_func2
{
  // Turbo Boyer-Moore data
  bool        canDoTurboBM;	// pattern is '%abcd%' case
  const char* pattern;
  int         pattern_len;

  // TurboBM buffers, *this is owner
  int* bmGs; //   good suffix shift table, size is pattern_len + 1
  int* bmBc; // bad character shift table, size is alphabet_size

  void turboBM_compute_suffixes(int* suff);
  void turboBM_compute_good_suffix_shifts(int* suff);
  void turboBM_compute_bad_character_shifts();
  bool turboBM_matches(const char* text, int text_len) const;
  enum { alphabet_size = 256 };

  Item *escape_item;
  
  bool escape_used_in_parsing;

public:
  int escape;

  Item_func_like(Item *a,Item *b, Item *escape_arg, bool escape_used)
    :Item_bool_func2(a,b), canDoTurboBM(FALSE), pattern(0), pattern_len(0), 
     bmGs(0), bmBc(0), escape_item(escape_arg),
     escape_used_in_parsing(escape_used) {}
  longlong val_int();
  enum Functype functype() const { return LIKE_FUNC; }
  optimize_type select_optimize() const;
  cond_result eq_cmp_result() const { return COND_TRUE; }
  const char *func_name() const { return "like"; }
  bool fix_fields(THD *thd, Item **ref);
  void cleanup();
};

#ifdef USE_REGEX

#include "my_regex.h"

class Item_func_regex :public Item_bool_func
{
  my_regex_t preg;
  bool regex_compiled;
  bool regex_is_const;
  String prev_regexp;
  DTCollation cmp_collation;
public:
  Item_func_regex(Item *a,Item *b) :Item_bool_func(a,b),
    regex_compiled(0),regex_is_const(0) {}
  void cleanup();
  longlong val_int();
  bool fix_fields(THD *thd, Item **ref);
  const char *func_name() const { return "regexp"; }
  void print(String *str) { print_op(str); }
  CHARSET_INFO *compare_collation() { return cmp_collation.collation; }
};

#else

class Item_func_regex :public Item_bool_func
{
public:
  Item_func_regex(Item *a,Item *b) :Item_bool_func(a,b) {}
  longlong val_int() { return 0;}
  const char *func_name() const { return "regex"; }
  void print(String *str) { print_op(str); }
};

#endif /* USE_REGEX */


typedef class Item COND;

class Item_cond :public Item_bool_func
{
protected:
  List<Item> list;
  bool abort_on_null;
  table_map and_tables_cache;

public:
  /* Item_cond() is only used to create top level items */
  Item_cond(): Item_bool_func(), abort_on_null(1)
  { const_item_cache=0; }
  Item_cond(Item *i1,Item *i2)
    :Item_bool_func(), abort_on_null(0)
  {
    list.push_back(i1);
    list.push_back(i2);
  }
  Item_cond(THD *thd, Item_cond *item);
  Item_cond(List<Item> &nlist)
    :Item_bool_func(), list(nlist), abort_on_null(0) {}
  bool add(Item *item) { return list.push_back(item); }
  void add_at_head(List<Item> *nlist) { list.prepand(nlist); }
  bool fix_fields(THD *, Item **ref);

  enum Type type() const { return COND_ITEM; }
  List<Item>* argument_list() { return &list; }
  table_map used_tables() const;
  void update_used_tables();
  void print(String *str);
  void split_sum_func(THD *thd, Item **ref_pointer_array, List<Item> &fields);
  friend int setup_conds(THD *thd, TABLE_LIST *tables, TABLE_LIST *leaves,
                         COND **conds);
  void top_level_item() { abort_on_null=1; }
  void copy_andor_arguments(THD *thd, Item_cond *item);
  bool walk(Item_processor processor, byte *arg);
  Item *transform(Item_transformer transformer, byte *arg);
  void traverse_cond(Cond_traverser, void *arg, traverse_order order);
  void neg_arguments(THD *thd);
};


/*
  The class Item_equal is used to represent conjunctions of equality
  predicates of the form field1 = field2, and field=const in where
  conditions and on expressions.

  All equality predicates of the form field1=field2 contained in a
  conjunction are substituted for a sequence of items of this class.
  An item of this class Item_equal(f1,f2,...fk) represents a
  multiple equality f1=f2=...=fk.

  If a conjunction contains predicates f1=f2 and f2=f3, a new item of
  this class is created Item_equal(f1,f2,f3) representing the multiple
  equality f1=f2=f3 that substitutes the above equality predicates in
  the conjunction.
  A conjunction of the predicates f2=f1 and f3=f1 and f3=f2 will be
  substituted for the item representing the same multiple equality
  f1=f2=f3.
  An item Item_equal(f1,f2) can appear instead of a conjunction of 
  f2=f1 and f1=f2, or instead of just the predicate f1=f2.

  An item of the class Item_equal inherits equalities from outer 
  conjunctive levels.

  Suppose we have a where condition of the following form:
  WHERE f1=f2 AND f3=f4 AND f3=f5 AND ... AND (...OR (f1=f3 AND ...)).
  In this case:
    f1=f2 will be substituted for Item_equal(f1,f2);
    f3=f4 and f3=f5  will be substituted for Item_equal(f3,f4,f5);
    f1=f3 will be substituted for Item_equal(f1,f2,f3,f4,f5);

  An object of the class Item_equal can contain an optional constant
  item c. Then it represents a multiple equality of the form 
  c=f1=...=fk.

  Objects of the class Item_equal are used for the following:

  1. An object Item_equal(t1.f1,...,tk.fk) allows us to consider any
  pair of tables ti and tj as joined by an equi-condition.
  Thus it provide us with additional access paths from table to table.

  2. An object Item_equal(t1.f1,...,tk.fk) is applied to deduce new
  SARGable predicates:
    f1=...=fk AND P(fi) => f1=...=fk AND P(fi) AND P(fj).
  It also can give us additional index scans and can allow us to
  improve selectivity estimates.

  3. An object Item_equal(t1.f1,...,tk.fk) is used to optimize the 
  selected execution plan for the query: if table ti is accessed 
  before the table tj then in any predicate P in the where condition
  the occurrence of tj.fj is substituted for ti.fi. This can allow
  an evaluation of the predicate at an earlier step.

  When feature 1 is supported they say that join transitive closure 
  is employed.
  When feature 2 is supported they say that search argument transitive
  closure is employed.
  Both features are usually supported by preprocessing original query and
  adding additional predicates.
  We do not just add predicates, we rather dynamically replace some
  predicates that can not be used to access tables in the investigated
  plan for those, obtained by substitution of some fields for equal fields,
  that can be used.     

  Prepared Statements/Stored Procedures note: instances of class
  Item_equal are created only at the time a PS/SP is executed and
  are deleted in the end of execution. All changes made to these
  objects need not be registered in the list of changes of the parse
  tree and do not harm PS/SP re-execution.

  Item equal objects are employed only at the optimize phase. Usually they are
  not supposed to be evaluated.  Yet in some cases we call the method val_int()
  for them. We have to take care of restricting the predicate such an
  object represents f1=f2= ...=fn to the projection of known fields fi1=...=fik.
*/

class Item_equal: public Item_bool_func
{
  List<Item_field> fields; /* list of equal field items                    */
  Item *const_item;        /* optional constant item equal to fields items */
  cmp_item *eval_item;
  bool cond_false;
  DTCollation cmp_collation;
public:
  inline Item_equal()
    : Item_bool_func(), const_item(0), eval_item(0), cond_false(0)
  { const_item_cache=0 ;}
  Item_equal(Item_field *f1, Item_field *f2);
  Item_equal(Item *c, Item_field *f);
  Item_equal(Item_equal *item_equal);
  inline Item* get_const() { return const_item; }
  void add(Item *c);
  void add(Item_field *f);
  uint members();
  bool contains(Field *field);
  Item_field* get_first() { return fields.head(); }
  void merge(Item_equal *item);
  void update_const();
  enum Functype functype() const { return MULT_EQUAL_FUNC; }
  longlong val_int(); 
  const char *func_name() const { return "multiple equal"; }
  optimize_type select_optimize() const { return OPTIMIZE_EQUAL; }
  void sort(Item_field_cmpfunc cmp, void *arg);
  friend class Item_equal_iterator;
  void fix_length_and_dec();
  bool fix_fields(THD *thd, Item **ref);
  void update_used_tables();
  bool walk(Item_processor processor, byte *arg);
  Item *transform(Item_transformer transformer, byte *arg);
  void print(String *str);
  CHARSET_INFO *compare_collation() 
  { return fields.head()->collation.collation; }
}; 

class COND_EQUAL: public Sql_alloc
{
public:
  uint max_members;               /* max number of members the current level
                                     list and all lower level lists */ 
  COND_EQUAL *upper_levels;       /* multiple equalities of upper and levels */
  List<Item_equal> current_level; /* list of multiple equalities of 
                                     the current and level           */
  COND_EQUAL()
  { 
    max_members= 0;
    upper_levels= 0;
  }
};


class Item_equal_iterator : public List_iterator_fast<Item_field>
{
public:
  inline Item_equal_iterator(Item_equal &item_equal) 
    :List_iterator_fast<Item_field> (item_equal.fields)
  {}
  inline Item_field* operator++(int)
  { 
    Item_field *item= (*(List_iterator_fast<Item_field> *) this)++;
    return  item;
  }
  inline void rewind(void) 
  { 
    List_iterator_fast<Item_field>::rewind();
  }
};

class Item_cond_and :public Item_cond
{
public:
  COND_EQUAL cond_equal;  /* contains list of Item_equal objects for 
                             the current and level and reference
                             to multiple equalities of upper and levels */  
  Item_cond_and() :Item_cond() {}
  Item_cond_and(Item *i1,Item *i2) :Item_cond(i1,i2) {}
  Item_cond_and(THD *thd, Item_cond_and *item) :Item_cond(thd, item) {}
  Item_cond_and(List<Item> &list): Item_cond(list) {}
  enum Functype functype() const { return COND_AND_FUNC; }
  longlong val_int();
  const char *func_name() const { return "and"; }
  table_map not_null_tables() const
  { return abort_on_null ? not_null_tables_cache: and_tables_cache; }
  Item* copy_andor_structure(THD *thd)
  {
    Item_cond_and *item;
    if ((item= new Item_cond_and(thd, this)))
       item->copy_andor_arguments(thd, this);
    return item;
  }
  Item *neg_transformer(THD *thd);
};

class Item_cond_or :public Item_cond
{
public:
  Item_cond_or() :Item_cond() {}
  Item_cond_or(Item *i1,Item *i2) :Item_cond(i1,i2) {}
  Item_cond_or(THD *thd, Item_cond_or *item) :Item_cond(thd, item) {}
  Item_cond_or(List<Item> &list): Item_cond(list) {}
  enum Functype functype() const { return COND_OR_FUNC; }
  longlong val_int();
  const char *func_name() const { return "or"; }
  table_map not_null_tables() const { return and_tables_cache; }
  Item* copy_andor_structure(THD *thd)
  {
    Item_cond_or *item;
    if ((item= new Item_cond_or(thd, this)))
      item->copy_andor_arguments(thd, this);
    return item;
  }
  Item *neg_transformer(THD *thd);
};


/*
  XOR is Item_cond, not an Item_int_func because we could like to
  optimize (a XOR b) later on. It's low prio, though
*/

class Item_cond_xor :public Item_cond
{
public:
  Item_cond_xor() :Item_cond() {}
  Item_cond_xor(Item *i1,Item *i2) :Item_cond(i1,i2) {}
  enum Functype functype() const { return COND_XOR_FUNC; }
  /* TODO: remove the next line when implementing XOR optimization */
  enum Type type() const { return FUNC_ITEM; }
  longlong val_int();
  const char *func_name() const { return "xor"; }
  void top_level_item() {}
};


/* Some useful inline functions */

inline Item *and_conds(Item *a, Item *b)
{
  if (!b) return a;
  if (!a) return b;
  return new Item_cond_and(a, b);
}

Item *and_expressions(Item *a, Item *b, Item **org_item);
