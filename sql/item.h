/* Copyright (C) 2000-2003 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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


#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

class Protocol;
struct st_table_list;
void item_init(void);			/* Init item functions */
class Item_field;

/*
   "Declared Type Collation"
   A combination of collation and its derivation.
*/

enum Derivation
{
  DERIVATION_IGNORABLE= 5,
  DERIVATION_COERCIBLE= 4,
  DERIVATION_SYSCONST= 3,
  DERIVATION_IMPLICIT= 2,
  DERIVATION_NONE= 1,
  DERIVATION_EXPLICIT= 0
};

/*
  Flags for collation aggregation modes:
  MY_COLL_ALLOW_SUPERSET_CONV  - allow conversion to a superset
  MY_COLL_ALLOW_COERCIBLE_CONV - allow conversion of a coercible value
                                 (i.e. constant).
  MY_COLL_ALLOW_CONV           - allow any kind of conversion
                                 (combination of the above two)
  MY_COLL_DISALLOW_NONE        - don't allow return DERIVATION_NONE
                                 (e.g. when aggregating for comparison)
  MY_COLL_CMP_CONV             - combination of MY_COLL_ALLOW_CONV
                                 and MY_COLL_DISALLOW_NONE
*/

#define MY_COLL_ALLOW_SUPERSET_CONV   1
#define MY_COLL_ALLOW_COERCIBLE_CONV  2
#define MY_COLL_ALLOW_CONV            3
#define MY_COLL_DISALLOW_NONE         4
#define MY_COLL_CMP_CONV              7

class DTCollation {
public:
  CHARSET_INFO     *collation;
  enum Derivation derivation;
  
  DTCollation()
  {
    collation= &my_charset_bin;
    derivation= DERIVATION_NONE;
  }
  DTCollation(CHARSET_INFO *collation_arg, Derivation derivation_arg)
  {
    collation= collation_arg;
    derivation= derivation_arg;
  }
  void set(DTCollation &dt)
  { 
    collation= dt.collation;
    derivation= dt.derivation;
  }
  void set(CHARSET_INFO *collation_arg, Derivation derivation_arg)
  {
    collation= collation_arg;
    derivation= derivation_arg;
  }
  void set(CHARSET_INFO *collation_arg)
  { collation= collation_arg; }
  void set(Derivation derivation_arg)
  { derivation= derivation_arg; }
  bool aggregate(DTCollation &dt, uint flags= 0);
  bool set(DTCollation &dt1, DTCollation &dt2, uint flags= 0)
  { set(dt1); return aggregate(dt2, flags); }
  const char *derivation_name() const
  {
    switch(derivation)
    {
      case DERIVATION_IGNORABLE: return "IGNORABLE";
      case DERIVATION_COERCIBLE: return "COERCIBLE";
      case DERIVATION_IMPLICIT:  return "IMPLICIT";
      case DERIVATION_SYSCONST:  return "SYSCONST";
      case DERIVATION_EXPLICIT:  return "EXPLICIT";
      case DERIVATION_NONE:      return "NONE";
      default: return "UNKNOWN";
    }
  }
};


/*************************************************************************/
/*
  A framework to easily handle different return types for hybrid items
  (hybrid item is an item whose operand can be of any type, e.g. integer,
  real, decimal).
*/

struct Hybrid_type_traits;

struct Hybrid_type
{
  longlong integer;

  double real;
  /*
    Use two decimal buffers interchangeably to speed up += operation
    which has no native support in decimal library.
    Hybrid_type+= arg is implemented as dec_buf[1]= dec_buf[0] + arg.
    The third decimal is used as a handy temporary storage.
  */
  my_decimal dec_buf[3];
  int used_dec_buf_no;

  /*
    Traits moved to a separate class to
      a) be able to easily change object traits in runtime
      b) they work as a differentiator for the union above
  */
  const Hybrid_type_traits *traits;

  Hybrid_type() {}
  /* XXX: add traits->copy() when needed */
  Hybrid_type(const Hybrid_type &rhs) :traits(rhs.traits) {}
};


/* Hybryd_type_traits interface + default implementation for REAL_RESULT */

struct Hybrid_type_traits
{
  virtual Item_result type() const { return REAL_RESULT; }

  virtual void
  fix_length_and_dec(Item *item, Item *arg) const;

  /* Hybrid_type operations. */
  virtual void set_zero(Hybrid_type *val) const { val->real= 0.0; }
  virtual void add(Hybrid_type *val, Field *f) const
  { val->real+= f->val_real(); }
  virtual void div(Hybrid_type *val, ulonglong u) const
  { val->real/= ulonglong2double(u); }

  virtual longlong val_int(Hybrid_type *val, bool unsigned_flag) const
  { return (longlong) rint(val->real); }
  virtual double val_real(Hybrid_type *val) const { return val->real; }
  virtual my_decimal *val_decimal(Hybrid_type *val, my_decimal *buf) const;
  virtual String *val_str(Hybrid_type *val, String *buf, uint8 decimals) const;
  static const Hybrid_type_traits *instance();
  Hybrid_type_traits() {}
  virtual ~Hybrid_type_traits() {}
};


struct Hybrid_type_traits_decimal: public Hybrid_type_traits
{
  virtual Item_result type() const { return DECIMAL_RESULT; }

  virtual void
  fix_length_and_dec(Item *arg, Item *item) const;

  /* Hybrid_type operations. */
  virtual void set_zero(Hybrid_type *val) const;
  virtual void add(Hybrid_type *val, Field *f) const;
  virtual void div(Hybrid_type *val, ulonglong u) const;

  virtual longlong val_int(Hybrid_type *val, bool unsigned_flag) const;
  virtual double val_real(Hybrid_type *val) const;
  virtual my_decimal *val_decimal(Hybrid_type *val, my_decimal *buf) const
  { return &val->dec_buf[val->used_dec_buf_no]; }
  virtual String *val_str(Hybrid_type *val, String *buf, uint8 decimals) const;
  static const Hybrid_type_traits_decimal *instance();
  Hybrid_type_traits_decimal() {};
};


struct Hybrid_type_traits_integer: public Hybrid_type_traits
{
  virtual Item_result type() const { return INT_RESULT; }

  virtual void
  fix_length_and_dec(Item *arg, Item *item) const;

  /* Hybrid_type operations. */
  virtual void set_zero(Hybrid_type *val) const
  { val->integer= 0; }
  virtual void add(Hybrid_type *val, Field *f) const
  { val->integer+= f->val_int(); }
  virtual void div(Hybrid_type *val, ulonglong u) const
  { val->integer/= (longlong) u; }

  virtual longlong val_int(Hybrid_type *val, bool unsigned_flag) const
  { return val->integer; }
  virtual double val_real(Hybrid_type *val) const
  { return (double) val->integer; }
  virtual my_decimal *val_decimal(Hybrid_type *val, my_decimal *buf) const
  {
    int2my_decimal(E_DEC_FATAL_ERROR, val->integer, 0, &val->dec_buf[2]);
    return &val->dec_buf[2];
  }
  virtual String *val_str(Hybrid_type *val, String *buf, uint8 decimals) const
  { buf->set(val->integer, &my_charset_bin); return buf;}
  static const Hybrid_type_traits_integer *instance();
  Hybrid_type_traits_integer() {};
};


void dummy_error_processor(THD *thd, void *data);

void view_error_processor(THD *thd, void *data);

/*
  Instances of Name_resolution_context store the information necesary for
  name resolution of Items and other context analysis of a query made in
  fix_fields().

  This structure is a part of SELECT_LEX, a pointer to this structure is
  assigned when an item is created (which happens mostly during  parsing
  (sql_yacc.yy)), but the structure itself will be initialized after parsing
  is complete

  TODO: move subquery of INSERT ... SELECT and CREATE ... SELECT to
  separate SELECT_LEX which allow to remove tricks of changing this
  structure before and after INSERT/CREATE and its SELECT to make correct
  field name resolution.
*/
struct Name_resolution_context: Sql_alloc
{
  /*
    The name resolution context to search in when an Item cannot be
    resolved in this context (the context of an outer select)
  */
  Name_resolution_context *outer_context;

  /*
    List of tables used to resolve the items of this context.  Usually these
    are tables from the FROM clause of SELECT statement.  The exceptions are
    INSERT ... SELECT and CREATE ... SELECT statements, where SELECT
    subquery is not moved to a separate SELECT_LEX.  For these types of
    statements we have to change this member dynamically to ensure correct
    name resolution of different parts of the statement.
  */
  TABLE_LIST *table_list;
  /*
    In most cases the two table references below replace 'table_list' above
    for the purpose of name resolution. The first and last name resolution
    table references allow us to search only in a sub-tree of the nested
    join tree in a FROM clause. This is needed for NATURAL JOIN, JOIN ... USING
    and JOIN ... ON. 
  */
  TABLE_LIST *first_name_resolution_table;
  /*
    Last table to search in the list of leaf table references that begins
    with first_name_resolution_table.
  */
  TABLE_LIST *last_name_resolution_table;

  /*
    SELECT_LEX item belong to, in case of merged VIEW it can differ from
    SELECT_LEX where item was created, so we can't use table_list/field_list
    from there
  */
  st_select_lex *select_lex;

  /*
    Processor of errors caused during Item name resolving, now used only to
    hide underlying tables in errors about views (i.e. it substitute some
    errors for views)
  */
  void (*error_processor)(THD *, void *);
  void *error_processor_data;

  /*
    When TRUE items are resolved in this context both against the
    SELECT list and this->table_list. If FALSE, items are resolved
    only against this->table_list.
  */
  bool resolve_in_select_list;

  /*
    Security context of this name resolution context. It's used for views
    and is non-zero only if the view is defined with SQL SECURITY DEFINER.
  */
  Security_context *security_ctx;

  Name_resolution_context()
    :outer_context(0), table_list(0), select_lex(0),
    error_processor_data(0),
    security_ctx(0)
    {}

  void init()
  {
    resolve_in_select_list= FALSE;
    error_processor= &dummy_error_processor;
    first_name_resolution_table= NULL;
    last_name_resolution_table= NULL;
  }

  void resolve_in_table_list_only(TABLE_LIST *tables)
  {
    table_list= first_name_resolution_table= tables;
    resolve_in_select_list= FALSE;
  }

  void process_error(THD *thd)
  {
    (*error_processor)(thd, error_processor_data);
  }
};


/*
  Store and restore the current state of a name resolution context.
*/

class Name_resolution_context_state
{
private:
  TABLE_LIST *save_table_list;
  TABLE_LIST *save_first_name_resolution_table;
  TABLE_LIST *save_next_name_resolution_table;
  bool        save_resolve_in_select_list;

public:
  Name_resolution_context_state() {}          /* Remove gcc warning */
  TABLE_LIST *save_next_local;

public:
  /* Save the state of a name resolution context. */
  void save_state(Name_resolution_context *context, TABLE_LIST *table_list)
  {
    save_table_list=                  context->table_list;
    save_first_name_resolution_table= context->first_name_resolution_table;
    save_next_name_resolution_table=  (context->first_name_resolution_table) ?
                                      context->first_name_resolution_table->
                                               next_name_resolution_table :
                                      NULL;
    save_resolve_in_select_list=      context->resolve_in_select_list;
    save_next_local=                  table_list->next_local;
  }

  /* Restore a name resolution context from saved state. */
  void restore_state(Name_resolution_context *context, TABLE_LIST *table_list)
  {
    table_list->next_local=                save_next_local;
    context->table_list=                   save_table_list;
    context->first_name_resolution_table=  save_first_name_resolution_table;
    if (context->first_name_resolution_table)
      context->first_name_resolution_table->
               next_name_resolution_table= save_next_name_resolution_table;
    context->resolve_in_select_list=       save_resolve_in_select_list;
  }
};

/*************************************************************************/

class sp_rcontext;


class Settable_routine_parameter
{
public:
  /*
    Set required privileges for accessing the parameter.

    SYNOPSIS
      set_required_privilege()
        rw        if 'rw' is true then we are going to read and set the
                  parameter, so SELECT and UPDATE privileges might be
                  required, otherwise we only reading it and SELECT
                  privilege might be required.
  */
  Settable_routine_parameter() {}
  virtual ~Settable_routine_parameter() {}
  virtual void set_required_privilege(bool rw) {};

  /*
    Set parameter value.

    SYNOPSIS
      set_value()
        thd       thread handle
        ctx       context to which parameter belongs (if it is local
                  variable).
        it        item which represents new value

    RETURN
      FALSE if parameter value has been set,
      TRUE if error has occured.
  */
  virtual bool set_value(THD *thd, sp_rcontext *ctx, Item **it)= 0;
};


typedef bool (Item::*Item_processor)(byte *arg);
typedef Item* (Item::*Item_transformer) (byte *arg);
typedef void (*Cond_traverser) (const Item *item, void *arg);


class Item {
  Item(const Item &);			/* Prevent use of these */
  void operator=(Item &);
public:
  static void *operator new(size_t size)
  { return (void*) sql_alloc((uint) size); }
  static void *operator new(size_t size, MEM_ROOT *mem_root)
  { return (void*) alloc_root(mem_root, (uint) size); }
  static void operator delete(void *ptr,size_t size) { TRASH(ptr, size); }
  static void operator delete(void *ptr, MEM_ROOT *mem_root) {}

  enum Type {FIELD_ITEM= 0, FUNC_ITEM, SUM_FUNC_ITEM, STRING_ITEM,
	     INT_ITEM, REAL_ITEM, NULL_ITEM, VARBIN_ITEM,
	     COPY_STR_ITEM, FIELD_AVG_ITEM, DEFAULT_VALUE_ITEM,
	     PROC_ITEM,COND_ITEM, REF_ITEM, FIELD_STD_ITEM,
	     FIELD_VARIANCE_ITEM, INSERT_VALUE_ITEM,
             SUBSELECT_ITEM, ROW_ITEM, CACHE_ITEM, TYPE_HOLDER,
             PARAM_ITEM, TRIGGER_FIELD_ITEM, DECIMAL_ITEM,
             VIEW_FIXER_ITEM};

  enum cond_result { COND_UNDEF,COND_OK,COND_TRUE,COND_FALSE };

  enum traverse_order { POSTFIX, PREFIX };
  
  /* Reuse size, only used by SP local variable assignment, otherwize 0 */
  uint rsize;

  /*
    str_values's main purpose is to be used to cache the value in
    save_in_field
  */
  String str_value;
  my_string name;			/* Name from select */
  /* Original item name (if it was renamed)*/
  my_string orig_name;
  Item *next;
  uint32 max_length;
  uint name_length;                     /* Length of name */
  uint8 marker, decimals;
  my_bool maybe_null;			/* If item may be null */
  my_bool null_value;			/* if item is null */
  my_bool unsigned_flag;
  my_bool with_sum_func;
  my_bool fixed;                        /* If item fixed with fix_fields */
  my_bool is_autogenerated_name;        /* indicate was name of this Item
                                           autogenerated or set by user */
  DTCollation collation;
  my_bool with_subselect;               /* If this item is a subselect or some
                                           of its arguments is or contains a
                                           subselect */

  // alloc & destruct is done as start of select using sql_alloc
  Item();
  /*
     Constructor used by Item_field, Item_ref & aggregate (sum) functions.
     Used for duplicating lists in processing queries with temporary
     tables
     Also it used for Item_cond_and/Item_cond_or for creating
     top AND/OR structure of WHERE clause to protect it of
     optimisation changes in prepared statements
  */
  Item(THD *thd, Item *item);
  virtual ~Item()
  {
#ifdef EXTRA_DEBUG
    name=0;
#endif
  }		/*lint -e1509 */
  void set_name(const char *str, uint length, CHARSET_INFO *cs);
  void rename(char *new_name);
  void init_make_field(Send_field *tmp_field,enum enum_field_types type);
  virtual void cleanup();
  virtual void make_field(Send_field *field);
  Field *make_string_field(TABLE *table);
  virtual bool fix_fields(THD *, Item **);
  /*
    should be used in case where we are sure that we do not need
    complete fix_fields() procedure.
  */
  inline void quick_fix_field() { fixed= 1; }
  /* Function returns 1 on overflow and -1 on fatal errors */
  int save_in_field_no_warnings(Field *field, bool no_conversions);
  virtual int save_in_field(Field *field, bool no_conversions);
  virtual void save_org_in_field(Field *field)
  { (void) save_in_field(field, 1); }
  virtual int save_safe_in_field(Field *field)
  { return save_in_field(field, 1); }
  virtual bool send(Protocol *protocol, String *str);
  virtual bool eq(const Item *, bool binary_cmp) const;
  virtual Item_result result_type() const { return REAL_RESULT; }
  virtual Item_result cast_to_int_type() const { return result_type(); }
  virtual enum_field_types field_type() const;
  virtual enum Type type() const =0;
  /* valXXX methods must return NULL or 0 or 0.0 if null_value is set. */
  /*
    Return double precision floating point representation of item.

    SYNOPSIS
      val_real()

    RETURN
      In case of NULL value return 0.0 and set null_value flag to TRUE.
      If value is not null null_value flag will be reset to FALSE.
  */
  virtual double val_real()=0;
  /*
    Return integer representation of item.

    SYNOPSIS
      val_int()

    RETURN
      In case of NULL value return 0 and set null_value flag to TRUE.
      If value is not null null_value flag will be reset to FALSE.
  */
  virtual longlong val_int()=0;
  /*
    This is just a shortcut to avoid the cast. You should still use
    unsigned_flag to check the sign of the item.
  */
  inline ulonglong val_uint() { return (ulonglong) val_int(); }
  /*
    Return string representation of this item object.

    SYNOPSIS
      val_str()
      str   an allocated buffer this or any nested Item object can use to
            store return value of this method.

    NOTE
      Buffer passed via argument  should only be used if the item itself
      doesn't have an own String buffer. In case when the item maintains
      it's own string buffer, it's preferable to return it instead to
      minimize number of mallocs/memcpys.
      The caller of this method can modify returned string, but only in case
      when it was allocated on heap, (is_alloced() is true).  This allows
      the caller to efficiently use a buffer allocated by a child without
      having to allocate a buffer of it's own. The buffer, given to
      val_str() as argument, belongs to the caller and is later used by the
      caller at it's own choosing.
      A few implications from the above:
      - unless you return a string object which only points to your buffer
        but doesn't manages it you should be ready that it will be
        modified.
      - even for not allocated strings (is_alloced() == false) the caller
        can change charset (see Item_func_{typecast/binary}. XXX: is this
        a bug?
      - still you should try to minimize data copying and return internal
        object whenever possible.

    RETURN
      In case of NULL value return 0 (NULL pointer) and set null_value flag
      to TRUE.
      If value is not null null_value flag will be reset to FALSE.
  */
  virtual String *val_str(String *str)=0;
  /*
    Return decimal representation of item with fixed point.

    SYNOPSIS
      val_decimal()
      decimal_buffer  buffer which can be used by Item for returning value
                      (but can be not)

    NOTE
      Returned value should not be changed if it is not the same which was
      passed via argument.

    RETURN
      Return pointer on my_decimal (it can be other then passed via argument)
        if value is not NULL (null_value flag will be reset to FALSE).
      In case of NULL value it return 0 pointer and set null_value flag
        to TRUE.
  */
  virtual my_decimal *val_decimal(my_decimal *decimal_buffer)= 0;
  /*
    Return boolean value of item.

    RETURN
      FALSE value is false or NULL
      TRUE value is true (not equal to 0)
  */
  virtual bool val_bool();
  /* Helper functions, see item_sum.cc */
  String *val_string_from_real(String *str);
  String *val_string_from_int(String *str);
  String *val_string_from_decimal(String *str);
  my_decimal *val_decimal_from_real(my_decimal *decimal_value);
  my_decimal *val_decimal_from_int(my_decimal *decimal_value);
  my_decimal *val_decimal_from_string(my_decimal *decimal_value);
  longlong val_int_from_decimal();
  double val_real_from_decimal();

  virtual Field *get_tmp_table_field() { return 0; }
  /* This is also used to create fields in CREATE ... SELECT: */
  virtual Field *tmp_table_field(TABLE *t_arg) { return 0; }
  virtual const char *full_name() const { return name ? name : "???"; }

  /*
    *result* family of methods is analog of *val* family (see above) but
    return value of result_field of item if it is present. If Item have not
    result field, it return val(). This methods set null_value flag in same
    way as *val* methods do it.
  */
  virtual double  val_result() { return val_real(); }
  virtual longlong val_int_result() { return val_int(); }
  virtual String *str_result(String* tmp) { return val_str(tmp); }
  virtual my_decimal *val_decimal_result(my_decimal *val)
  { return val_decimal(val); }
  virtual bool val_bool_result() { return val_bool(); }

  /* bit map of tables used by item */
  virtual table_map used_tables() const { return (table_map) 0L; }
  /*
    Return table map of tables that can't be NULL tables (tables that are
    used in a context where if they would contain a NULL row generated
    by a LEFT or RIGHT join, the item would not be true).
    This expression is used on WHERE item to determinate if a LEFT JOIN can be
    converted to a normal join.
    Generally this function should return used_tables() if the function
    would return null if any of the arguments are null
    As this is only used in the beginning of optimization, the value don't
    have to be updated in update_used_tables()
  */
  virtual table_map not_null_tables() const { return used_tables(); }
  /*
    Returns true if this is a simple constant item like an integer, not
    a constant expression. Used in the optimizer to propagate basic constants.
  */
  virtual bool basic_const_item() const { return 0; }
  /* cloning of constant items (0 if it is not const) */
  virtual Item *new_item() { return 0; }
  virtual cond_result eq_cmp_result() const { return COND_OK; }
  inline uint float_length(uint decimals_par) const
  { return decimals != NOT_FIXED_DEC ? (DBL_DIG+2+decimals_par) : DBL_DIG+8;}
  virtual uint decimal_precision() const;
  inline int decimal_int_part() const
  { return my_decimal_int_part(decimal_precision(), decimals); }
  /* 
    Returns true if this is constant (during query execution, i.e. its value
    will not change until next fix_fields) and its value is known.
  */
  virtual bool const_item() const { return used_tables() == 0; }
  /* 
    Returns true if this is constant but its value may be not known yet.
    (Can be used for parameters of prep. stmts or of stored procedures.)
  */
  virtual bool const_during_execution() const 
  { return (used_tables() & ~PARAM_TABLE_BIT) == 0; }
  /*
    This is an essential method for correct functioning of VIEWS.
    To save a view in an .frm file we need its unequivocal
    definition in SQL that takes into account sql_mode and
    environmental settings.  Currently such definition is restored
    by traversing through the parsed tree of a view and
    print()'ing SQL syntax of every node to a String buffer. This
    method is used to print the SQL definition of an item. The
    second use of this method is for EXPLAIN EXTENDED, to print
    the SQL of a query after all optimizations of the parsed tree
    have been done.
  */
  virtual void print(String *str_arg) { str_arg->append(full_name()); }
  void print_item_w_name(String *);
  virtual void update_used_tables() {}
  virtual void split_sum_func(THD *thd, Item **ref_pointer_array,
                              List<Item> &fields) {}
  /* Called for items that really have to be split */
  void split_sum_func2(THD *thd, Item **ref_pointer_array, List<Item> &fields,
                       Item **ref, bool skip_registered);
  virtual bool get_date(TIME *ltime,uint fuzzydate);
  virtual bool get_time(TIME *ltime);
  virtual bool get_date_result(TIME *ltime,uint fuzzydate)
  { return get_date(ltime,fuzzydate); }
  /*
    This function is used only in Item_func_isnull/Item_func_isnotnull
    (implementations of IS NULL/IS NOT NULL clauses). Item_func_is{not}null
    calls this method instead of one of val/result*() methods, which
    normally will set null_value. This allows to determine nullness of
    a complex expression without fully evaluating it.
    Any new item which can be NULL must implement this call.
  */
  virtual bool is_null() { return 0; }

  /*
    Inform the item that there will be no distinction between its result
    being FALSE or NULL.

    NOTE
      This function will be called for eg. Items that are top-level AND-parts
      of the WHERE clause. Items implementing this function (currently
      Item_cond_and and subquery-related item) enable special optimizations
      when they are "top level".
  */
  virtual void top_level_item() {}
  /*
    set field of temporary table for Item which can be switched on temporary
    table during query processing (grouping and so on)
  */
  virtual void set_result_field(Field *field) {}
  virtual bool is_result_field() { return 0; }
  virtual bool is_bool_func() { return 0; }
  virtual void save_in_result_field(bool no_conversions) {}
  /*
    set value of aggregate function in case of no rows for grouping were found
  */
  virtual void no_rows_in_result() {}
  virtual Item *copy_or_same(THD *thd) { return this; }
  virtual Item *copy_andor_structure(THD *thd) { return this; }
  virtual Item *real_item() { return this; }
  virtual Item *get_tmp_table_item(THD *thd) { return copy_or_same(thd); }

  static CHARSET_INFO *default_charset();
  virtual CHARSET_INFO *compare_collation() { return NULL; }

  virtual bool walk(Item_processor processor, byte *arg)
  {
    return (this->*processor)(arg);
  }

  virtual Item* transform(Item_transformer transformer, byte *arg);

   virtual void traverse_cond(Cond_traverser traverser,
                              void *arg, traverse_order order)
   {
     (*traverser)(this, arg);
   }

  virtual bool remove_dependence_processor(byte * arg) { return 0; }
  virtual bool remove_fixed(byte * arg) { fixed= 0; return 0; }
  virtual bool cleanup_processor(byte *arg);
  virtual bool collect_item_field_processor(byte * arg) { return 0; }
  virtual bool find_item_in_field_list_processor(byte *arg) { return 0; }
  virtual bool change_context_processor(byte *context) { return 0; }
  virtual bool reset_query_id_processor(byte *query_id) { return 0; }
  virtual bool is_expensive_processor(byte *arg) { return 0; }

  virtual Item *equal_fields_propagator(byte * arg) { return this; }
  virtual bool set_no_const_sub(byte *arg) { return FALSE; }
  virtual Item *replace_equal_field(byte * arg) { return this; }

  /*
    For SP local variable returns pointer to Item representing its
    current value and pointer to current Item otherwise.
  */
  virtual Item *this_item() { return this; }
  virtual const Item *this_item() const { return this; }

  /*
    For SP local variable returns address of pointer to Item representing its
    current value and pointer passed via parameter otherwise.
  */
  virtual Item **this_item_addr(THD *thd, Item **addr) { return addr; }

  // Row emulation
  virtual uint cols() { return 1; }
  virtual Item* el(uint i) { return this; }
  virtual Item** addr(uint i) { return 0; }
  virtual bool check_cols(uint c);
  // It is not row => null inside is impossible
  virtual bool null_inside() { return 0; }
  // used in row subselects to get value of elements
  virtual void bring_value() {}

  Field *tmp_table_field_from_field_type(TABLE *table);
  virtual Item_field *filed_for_view_update() { return 0; }

  virtual Item *neg_transformer(THD *thd) { return NULL; }
  virtual Item *safe_charset_converter(CHARSET_INFO *tocs);
  void delete_self()
  {
    cleanup();
    delete this;
  }

  virtual bool is_splocal() { return 0; } /* Needed for error checking */

  /*
    Return Settable_routine_parameter interface of the Item.  Return 0
    if this Item is not Settable_routine_parameter.
  */
  virtual Settable_routine_parameter *get_settable_routine_parameter()
  {
    return 0;
  }
  /*
    result_as_longlong() must return TRUE for Items representing DATE/TIME
    functions and DATE/TIME table fields.
    Those Items have result_type()==STRING_RESULT (and not INT_RESULT), but
    their values should be compared as integers (because the integer
    representation is more precise than the string one).
  */
  virtual bool result_as_longlong() { return FALSE; }
};


class sp_head;


/*****************************************************************************
  The class is a base class for representation of stored routine variables in
  the Item-hierarchy. There are the following kinds of SP-vars:
    - local variables (Item_splocal);
    - CASE expression (Item_case_expr);
*****************************************************************************/

class Item_sp_variable :public Item
{
protected:
  /*
    THD, which is stored in fix_fields() and is used in this_item() to avoid
    current_thd use.
  */
  THD *m_thd;

public:
  LEX_STRING m_name;

public:
#ifndef DBUG_OFF
  /*
    Routine to which this Item_splocal belongs. Used for checking if correct
    runtime context is used for variable handling.
  */
  sp_head *m_sp;
#endif

public:
  Item_sp_variable(char *sp_var_name_str, uint sp_var_name_length);

public:
  bool fix_fields(THD *thd, Item **);

  double val_real();
  longlong val_int();
  String *val_str(String *sp);
  my_decimal *val_decimal(my_decimal *decimal_value);
  bool is_null();

public:
  inline void make_field(Send_field *field);
  
  inline bool const_item() const;
  
  inline int save_in_field(Field *field, bool no_conversions);
  inline bool send(Protocol *protocol, String *str);
}; 

/*****************************************************************************
  Item_sp_variable inline implementation.
*****************************************************************************/

inline void Item_sp_variable::make_field(Send_field *field)
{
  Item *it= this_item();

  if (name)
    it->set_name(name, (uint) strlen(name), system_charset_info);
  else
    it->set_name(m_name.str, m_name.length, system_charset_info);
  it->make_field(field);
}

inline bool Item_sp_variable::const_item() const
{
  return TRUE;
}

inline int Item_sp_variable::save_in_field(Field *field, bool no_conversions)
{
  return this_item()->save_in_field(field, no_conversions);
}

inline bool Item_sp_variable::send(Protocol *protocol, String *str)
{
  return this_item()->send(protocol, str);
}


/*****************************************************************************
  A reference to local SP variable (incl. reference to SP parameter), used in
  runtime.
*****************************************************************************/

class Item_splocal :public Item_sp_variable,
                    private Settable_routine_parameter
{
  uint m_var_idx;

  Type m_type;
  Item_result m_result_type;

public:
  /* 
    Position of this reference to SP variable in the statement (the
    statement itself is in sp_instr_stmt::m_query).
    This is valid only for references to SP variables in statements,
    excluding DECLARE CURSOR statement. It is used to replace references to SP
    variables with NAME_CONST calls when putting statements into the binary
    log.
    Value of 0 means that this object doesn't corresponding to reference to
    SP variable in query text.
  */
  uint pos_in_query;

  Item_splocal(const LEX_STRING &sp_var_name, uint sp_var_idx,
               enum_field_types sp_var_type, uint pos_in_q= 0);

  bool is_splocal() { return 1; } /* Needed for error checking */

  Item *this_item();
  const Item *this_item() const;
  Item **this_item_addr(THD *thd, Item **);

  void print(String *str);

public:
  inline const LEX_STRING *my_name() const;

  inline uint get_var_idx() const;

  inline enum Type type() const;
  inline Item_result result_type() const;

private:
  bool set_value(THD *thd, sp_rcontext *ctx, Item **it);

public:
  Settable_routine_parameter *get_settable_routine_parameter()
  {
    return this;
  }
};

/*****************************************************************************
  Item_splocal inline implementation.
*****************************************************************************/

inline const LEX_STRING *Item_splocal::my_name() const
{
  return &m_name;
}

inline uint Item_splocal::get_var_idx() const
{
  return m_var_idx;
}

inline enum Item::Type Item_splocal::type() const
{
  return m_type;
}

inline Item_result Item_splocal::result_type() const
{
  return m_result_type;
}


/*****************************************************************************
  A reference to case expression in SP, used in runtime.
*****************************************************************************/

class Item_case_expr :public Item_sp_variable
{
public:
  Item_case_expr(int case_expr_id);

public:
  Item *this_item();
  const Item *this_item() const;
  Item **this_item_addr(THD *thd, Item **);

  inline enum Type type() const;
  inline Item_result result_type() const;

public:
  /*
    NOTE: print() is intended to be used from views and for debug.
    Item_case_expr can not occur in views, so here it is only for debug
    purposes.
  */
  void print(String *str);

private:
  int m_case_expr_id;
};

/*****************************************************************************
  Item_case_expr inline implementation.
*****************************************************************************/

inline enum Item::Type Item_case_expr::type() const
{
  return this_item()->type();
}

inline Item_result Item_case_expr::result_type() const
{
  return this_item()->result_type();
}


/*
  NAME_CONST(given_name, const_value). 
  This 'function' has all properties of the supplied const_value (which is 
  assumed to be a literal constant), and the name given_name. 

  This is used to replace references to SP variables when we write PROCEDURE
  statements into the binary log.

  TODO
    Together with Item_splocal and Item::this_item() we can actually extract
    common a base of this class and Item_splocal. Maybe it is possible to
    extract a common base with class Item_ref, too.
*/

class Item_name_const : public Item
{
  Item *value_item;
  Item *name_item;
public:
  Item_name_const(Item *name, Item *val): value_item(val), name_item(name)
  {
    Item::maybe_null= TRUE;
  }

  bool fix_fields(THD *, Item **);

  enum Type type() const;
  double val_real();
  longlong val_int();
  String *val_str(String *sp);
  my_decimal *val_decimal(my_decimal *);
  bool is_null();
  void print(String *str);

  Item_result result_type() const
  {
    return value_item->result_type();
  }

  bool const_item() const
  {
    return TRUE;
  }

  int save_in_field(Field *field, bool no_conversions)
  {
    return  value_item->save_in_field(field, no_conversions);
  }

  bool send(Protocol *protocol, String *str)
  {
    return value_item->send(protocol, str);
  }
};

bool agg_item_collations(DTCollation &c, const char *name,
                         Item **items, uint nitems, uint flags, int item_sep);
bool agg_item_collations_for_comparison(DTCollation &c, const char *name,
                                        Item **items, uint nitems, uint flags);
bool agg_item_charsets(DTCollation &c, const char *name,
                       Item **items, uint nitems, uint flags, int item_sep);


class Item_num: public Item
{
public:
  Item_num() {}                               /* Remove gcc warning */
  virtual Item_num *neg()= 0;
  Item *safe_charset_converter(CHARSET_INFO *tocs);
};

#define NO_CACHED_FIELD_INDEX ((uint)(-1))

class st_select_lex;
class Item_ident :public Item
{
protected:
  /* 
    We have to store initial values of db_name, table_name and field_name
    to be able to restore them during cleanup() because they can be 
    updated during fix_fields() to values from Field object and life-time 
    of those is shorter than life-time of Item_field.
  */
  const char *orig_db_name;
  const char *orig_table_name;
  const char *orig_field_name;

public:
  Name_resolution_context *context;
  const char *db_name;
  const char *table_name;
  const char *field_name;
  bool alias_name_used; /* true if item was resolved against alias */
  /* 
    Cached value of index for this field in table->field array, used by prep. 
    stmts for speeding up their re-execution. Holds NO_CACHED_FIELD_INDEX 
    if index value is not known.
  */
  uint cached_field_index;
  /*
    Cached pointer to table which contains this field, used for the same reason
    by prep. stmt. too in case then we have not-fully qualified field.
    0 - means no cached value.
  */
  TABLE_LIST *cached_table;
  st_select_lex *depended_from;
  Item_ident(Name_resolution_context *context_arg,
             const char *db_name_arg, const char *table_name_arg,
             const char *field_name_arg);
  Item_ident(THD *thd, Item_ident *item);
  const char *full_name() const;
  void cleanup();
  bool remove_dependence_processor(byte * arg);
  void print(String *str);
  virtual bool change_context_processor(byte *cntx)
    { context= (Name_resolution_context *)cntx; return FALSE; }
  friend bool insert_fields(THD *thd, Name_resolution_context *context,
                            const char *db_name,
                            const char *table_name, List_iterator<Item> *it,
                            bool any_privileges);
};


class Item_ident_for_show :public Item
{
public:
  Field *field;
  const char *db_name;
  const char *table_name;

  Item_ident_for_show(Field *par_field, const char *db_arg,
                      const char *table_name_arg)
    :field(par_field), db_name(db_arg), table_name(table_name_arg)
  {}

  enum Type type() const { return FIELD_ITEM; }
  double val_real() { return field->val_real(); }
  longlong val_int() { return field->val_int(); }
  String *val_str(String *str) { return field->val_str(str); }
  my_decimal *val_decimal(my_decimal *dec) { return field->val_decimal(dec); }
  void make_field(Send_field *tmp_field);
};


class Item_equal;
class COND_EQUAL;

class Item_field :public Item_ident
{
protected:
  void set_field(Field *field);
public:
  Field *field,*result_field;
  Item_equal *item_equal;
  bool no_const_subst;
  /*
    if any_privileges set to TRUE then here real effective privileges will
    be stored
  */
  uint have_privileges;
  /* field need any privileges (for VIEW creation) */
  bool any_privileges;

  Item_field(Name_resolution_context *context_arg,
             const char *db_arg,const char *table_name_arg,
	     const char *field_name_arg);
  /*
    Constructor needed to process subselect with temporary tables (see Item)
  */
  Item_field(THD *thd, Item_field *item);
  /*
    Constructor used inside setup_wild(), ensures that field, table,
    and database names will live as long as Item_field (this is important
    in prepared statements).
  */
  Item_field(THD *thd, Name_resolution_context *context_arg, Field *field);
  /*
    If this constructor is used, fix_fields() won't work, because
    db_name, table_name and column_name are unknown. It's necessary to call
    reset_field() before fix_fields() for all fields created this way.
  */
  Item_field(Field *field);
  enum Type type() const { return FIELD_ITEM; }
  bool eq(const Item *item, bool binary_cmp) const;
  double val_real();
  longlong val_int();
  my_decimal *val_decimal(my_decimal *);
  String *val_str(String*);
  double val_result();
  longlong val_int_result();
  String *str_result(String* tmp);
  my_decimal *val_decimal_result(my_decimal *);
  bool val_bool_result();
  bool send(Protocol *protocol, String *str_arg);
  void reset_field(Field *f);
  bool fix_fields(THD *, Item **);
  void make_field(Send_field *tmp_field);
  int save_in_field(Field *field,bool no_conversions);
  void save_org_in_field(Field *field);
  table_map used_tables() const;
  enum Item_result result_type () const
  {
    return field->result_type();
  }
  Item_result cast_to_int_type() const
  {
    return field->cast_to_int_type();
  }
  enum_field_types field_type() const
  {
    return field->type();
  }
  Field *get_tmp_table_field() { return result_field; }
  Field *tmp_table_field(TABLE *t_arg) { return result_field; }
  bool get_date(TIME *ltime,uint fuzzydate);
  bool get_date_result(TIME *ltime,uint fuzzydate);
  bool get_time(TIME *ltime);
  bool is_null() { return field->is_null(); }
  Item *get_tmp_table_item(THD *thd);
  bool collect_item_field_processor(byte * arg);
  bool find_item_in_field_list_processor(byte *arg);
  bool reset_query_id_processor(byte *arg)
  {
    field->query_id= *((query_id_t *) arg);
    if (result_field)
      result_field->query_id= field->query_id;
    return 0;
  }
  void cleanup();
  bool result_as_longlong()
  {
    return field->can_be_compared_as_longlong();
  }
  Item_equal *find_item_equal(COND_EQUAL *cond_equal);
  Item *equal_fields_propagator(byte *arg);
  bool set_no_const_sub(byte *arg);
  Item *replace_equal_field(byte *arg);
  inline uint32 max_disp_length() { return field->max_length(); }
  Item_field *filed_for_view_update() { return this; }
  Item *safe_charset_converter(CHARSET_INFO *tocs);
  int fix_outer_field(THD *thd, Field **field, Item **reference);
  friend class Item_default_value;
  friend class Item_insert_value;
  friend class st_select_lex_unit;
};

class Item_null :public Item
{
public:
  Item_null(char *name_par=0)
  {
    maybe_null= null_value= TRUE;
    max_length= 0;
    name= name_par ? name_par : (char*) "NULL";
    fixed= 1;
    collation.set(&my_charset_bin, DERIVATION_IGNORABLE);
  }
  enum Type type() const { return NULL_ITEM; }
  bool eq(const Item *item, bool binary_cmp) const;
  double val_real();
  longlong val_int();
  String *val_str(String *str);
  my_decimal *val_decimal(my_decimal *);
  int save_in_field(Field *field, bool no_conversions);
  int save_safe_in_field(Field *field);
  bool send(Protocol *protocol, String *str);
  enum Item_result result_type () const { return STRING_RESULT; }
  enum_field_types field_type() const   { return MYSQL_TYPE_NULL; }
  /* to prevent drop fixed flag (no need parent cleanup call) */
  void cleanup() {}
  bool basic_const_item() const { return 1; }
  Item *new_item() { return new Item_null(name); }
  bool is_null() { return 1; }
  void print(String *str) { str->append(STRING_WITH_LEN("NULL")); }
  Item *safe_charset_converter(CHARSET_INFO *tocs);
};

class Item_null_result :public Item_null
{
public:
  Field *result_field;
  Item_null_result() : Item_null(), result_field(0) {}
  bool is_result_field() { return result_field != 0; }
  void save_in_result_field(bool no_conversions)
  {
    save_in_field(result_field, no_conversions);
  }
};  

/* Item represents one placeholder ('?') of prepared statement */

class Item_param :public Item
{
  char cnvbuf[MAX_FIELD_WIDTH];
  String cnvstr;
  Item *cnvitem;
public:

  enum enum_item_param_state
  {
    NO_VALUE, NULL_VALUE, INT_VALUE, REAL_VALUE,
    STRING_VALUE, TIME_VALUE, LONG_DATA_VALUE,
    DECIMAL_VALUE
  } state;

  /*
    A buffer for string and long data values. Historically all allocated
    values returned from val_str() were treated as eligible to
    modification. I. e. in some cases Item_func_concat can append it's
    second argument to return value of the first one. Because of that we
    can't return the original buffer holding string data from val_str(),
    and have to have one buffer for data and another just pointing to
    the data. This is the latter one and it's returned from val_str().
    Can not be declared inside the union as it's not a POD type.
  */
  String str_value_ptr;
  my_decimal decimal_value;
  union
  {
    longlong integer;
    double   real;
    /*
      Character sets conversion info for string values.
      Character sets of client and connection defined at bind time are used
      for all conversions, even if one of them is later changed (i.e.
      between subsequent calls to mysql_stmt_execute).
    */
    struct CONVERSION_INFO
    {
      CHARSET_INFO *character_set_client;
      CHARSET_INFO *character_set_of_placeholder;
      /*
        This points at character set of connection if conversion
        to it is required (i. e. if placeholder typecode is not BLOB).
        Otherwise it's equal to character_set_client (to simplify
        check in convert_str_value()).
      */
      CHARSET_INFO *final_character_set_of_str_value;
    } cs_info;
    TIME     time;
  } value;

  /* Cached values for virtual methods to save us one switch.  */
  enum Item_result item_result_type;
  enum Type item_type;

  /*
    Used when this item is used in a temporary table.
    This is NOT placeholder metadata sent to client, as this value
    is assigned after sending metadata (in setup_one_conversion_function).
    For example in case of 'SELECT ?' you'll get MYSQL_TYPE_STRING both
    in result set and placeholders metadata, no matter what type you will
    supply for this placeholder in mysql_stmt_execute.
  */
  enum enum_field_types param_type;
  /*
    Offset of placeholder inside statement text. Used to create
    no-placeholders version of this statement for the binary log.
  */
  uint pos_in_query;

  Item_param(uint pos_in_query_arg);

  enum Item_result result_type () const { return item_result_type; }
  enum Type type() const { return item_type; }
  enum_field_types field_type() const { return param_type; }

  double val_real();
  longlong val_int();
  my_decimal *val_decimal(my_decimal*);
  String *val_str(String*);
  bool get_time(TIME *tm);
  bool get_date(TIME *tm, uint fuzzydate);
  int  save_in_field(Field *field, bool no_conversions);

  void set_null();
  void set_int(longlong i, uint32 max_length_arg);
  void set_double(double i);
  void set_decimal(const char *str, ulong length);
  bool set_str(const char *str, ulong length);
  bool set_longdata(const char *str, ulong length);
  void set_time(TIME *tm, timestamp_type type, uint32 max_length_arg);
  bool set_from_user_var(THD *thd, const user_var_entry *entry);
  void reset();
  /*
    Assign placeholder value from bind data.
    Note, that 'len' has different semantics in embedded library (as we
    don't need to check that packet is not broken there). See
    sql_prepare.cc for details.
  */
  void (*set_param_func)(Item_param *param, uchar **pos, ulong len);

  const String *query_val_str(String *str) const;

  bool convert_str_value(THD *thd);

  /*
    If value for parameter was not set we treat it as non-const
    so noone will use parameters value in fix_fields still
    parameter is constant during execution.
  */
  virtual table_map used_tables() const
  { return state != NO_VALUE ? (table_map)0 : PARAM_TABLE_BIT; }
  void print(String *str);
  bool is_null()
  { DBUG_ASSERT(state != NO_VALUE); return state == NULL_VALUE; }
  bool basic_const_item() const;
  /*
    This method is used to make a copy of a basic constant item when
    propagating constants in the optimizer. The reason to create a new
    item and not use the existing one is not precisely known (2005/04/16).
    Probably we are trying to preserve tree structure of items, in other
    words, avoid pointing at one item from two different nodes of the tree.
    Return a new basic constant item if parameter value is a basic
    constant, assert otherwise. This method is called only if
    basic_const_item returned TRUE.
  */
  Item *safe_charset_converter(CHARSET_INFO *tocs);
  Item *new_item();
  /*
    Implement by-value equality evaluation if parameter value
    is set and is a basic constant (integer, real or string).
    Otherwise return FALSE.
  */
  bool eq(const Item *item, bool binary_cmp) const;
};


class Item_int :public Item_num
{
public:
  longlong value;
  Item_int(int32 i,uint length=11) :value((longlong) i)
    { max_length=length; fixed= 1; }
  Item_int(longlong i,uint length=21) :value(i)
    { max_length=length; fixed= 1; }
  Item_int(ulonglong i, uint length= 21) :value((longlong)i)
    { max_length=length; fixed= 1; unsigned_flag= 1; }
  Item_int(const char *str_arg,longlong i,uint length) :value(i)
    { max_length=length; name=(char*) str_arg; fixed= 1; }
  Item_int(const char *str_arg, uint length=64);
  enum Type type() const { return INT_ITEM; }
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types field_type() const { return MYSQL_TYPE_LONGLONG; }
  longlong val_int() { DBUG_ASSERT(fixed == 1); return value; }
  double val_real() { DBUG_ASSERT(fixed == 1); return (double) value; }
  my_decimal *val_decimal(my_decimal *);
  String *val_str(String*);
  int save_in_field(Field *field, bool no_conversions);
  bool basic_const_item() const { return 1; }
  Item *new_item() { return new Item_int(name,value,max_length); }
  // to prevent drop fixed flag (no need parent cleanup call)
  void cleanup() {}
  void print(String *str);
  Item_num *neg() { value= -value; return this; }
  uint decimal_precision() const
  { return (uint)(max_length - test(value < 0)); }
  bool eq(const Item *, bool binary_cmp) const;
};


class Item_uint :public Item_int
{
public:
  Item_uint(const char *str_arg, uint length);
  Item_uint(ulonglong i) :Item_int((ulonglong) i, 10) {}
  Item_uint(const char *str_arg, longlong i, uint length);
  double val_real()
    { DBUG_ASSERT(fixed == 1); return ulonglong2double((ulonglong)value); }
  String *val_str(String*);
  Item *new_item() { return new Item_uint(name,max_length); }
  int save_in_field(Field *field, bool no_conversions);
  void print(String *str);
  Item_num *neg ();
  uint decimal_precision() const { return max_length; }
};


/* decimal (fixed point) constant */
class Item_decimal :public Item_num
{
protected:
  my_decimal decimal_value;
public:
  Item_decimal(const char *str_arg, uint length, CHARSET_INFO *charset);
  Item_decimal(const char *str, const my_decimal *val_arg,
               uint decimal_par, uint length);
  Item_decimal(my_decimal *value_par);
  Item_decimal(longlong val, bool unsig);
  Item_decimal(double val, int precision, int scale);
  Item_decimal(const char *bin, int precision, int scale);

  enum Type type() const { return DECIMAL_ITEM; }
  enum Item_result result_type () const { return DECIMAL_RESULT; }
  enum_field_types field_type() const { return MYSQL_TYPE_NEWDECIMAL; }
  longlong val_int();
  double val_real();
  String *val_str(String*);
  my_decimal *val_decimal(my_decimal *val) { return &decimal_value; }
  int save_in_field(Field *field, bool no_conversions);
  bool basic_const_item() const { return 1; }
  Item *new_item()
  {
    return new Item_decimal(name, &decimal_value, decimals, max_length);
  }
  // to prevent drop fixed flag (no need parent cleanup call)
  void cleanup() {}
  void print(String *str);
  Item_num *neg()
  {
    my_decimal_neg(&decimal_value);
    unsigned_flag= !decimal_value.sign();
    return this;
  }
  uint decimal_precision() const { return decimal_value.precision(); }
  bool eq(const Item *, bool binary_cmp) const;
  void set_decimal_value(my_decimal *value_par);
};


class Item_float :public Item_num
{
  char *presentation;
public:
  double value;
  // Item_real() :value(0) {}
  Item_float(const char *str_arg, uint length);
  Item_float(const char *str,double val_arg,uint decimal_par,uint length)
    :value(val_arg)
  {
    presentation= name=(char*) str;
    decimals=(uint8) decimal_par;
    max_length=length;
    fixed= 1;
  }
  Item_float(double value_par) :presentation(0), value(value_par) { fixed= 1; }

  int save_in_field(Field *field, bool no_conversions);
  enum Type type() const { return REAL_ITEM; }
  enum_field_types field_type() const { return MYSQL_TYPE_DOUBLE; }
  double val_real() { DBUG_ASSERT(fixed == 1); return value; }
  longlong val_int()
  {
    DBUG_ASSERT(fixed == 1);
    if (value <= (double) LONGLONG_MIN)
    {
       return LONGLONG_MIN;
    }
    else if (value >= (double) (ulonglong) LONGLONG_MAX)
    {
      return LONGLONG_MAX;
    }
    return (longlong) rint(value);
  }
  String *val_str(String*);
  my_decimal *val_decimal(my_decimal *);
  bool basic_const_item() const { return 1; }
  // to prevent drop fixed flag (no need parent cleanup call)
  void cleanup() {}
  Item *new_item()
  { return new Item_float(name, value, decimals, max_length); }
  Item_num *neg() { value= -value; return this; }
  void print(String *str);
  bool eq(const Item *, bool binary_cmp) const;
};


class Item_static_float_func :public Item_float
{
  const char *func_name;
public:
  Item_static_float_func(const char *str, double val_arg, uint decimal_par,
                        uint length)
    :Item_float(NullS, val_arg, decimal_par, length), func_name(str)
  {}
  void print(String *str) { str->append(func_name); }
  Item *safe_charset_converter(CHARSET_INFO *tocs);
};


class Item_string :public Item
{
public:
  Item_string(const char *str,uint length,
  	      CHARSET_INFO *cs, Derivation dv= DERIVATION_COERCIBLE)
  {
    collation.set(cs, dv);
    str_value.set_or_copy_aligned(str,length,cs);
    /*
      We have to have a different max_length than 'length' here to
      ensure that we get the right length if we do use the item
      to create a new table. In this case max_length must be the maximum
      number of chars for a string of this type because we in create_field::
      divide the max_length with mbmaxlen).
    */
    max_length= str_value.numchars()*cs->mbmaxlen;
    set_name(str, length, cs);
    decimals=NOT_FIXED_DEC;
    // it is constant => can be used without fix_fields (and frequently used)
    fixed= 1;
  }
  /* Just create an item and do not fill string representation */
  Item_string(CHARSET_INFO *cs, Derivation dv= DERIVATION_COERCIBLE)
  {
    collation.set(cs, dv);
    max_length= 0;
    set_name(NULL, 0, cs);
    decimals= NOT_FIXED_DEC;
    fixed= 1;
  }
  Item_string(const char *name_par, const char *str, uint length,
	      CHARSET_INFO *cs, Derivation dv= DERIVATION_COERCIBLE)
  {
    collation.set(cs, dv);
    str_value.set_or_copy_aligned(str,length,cs);
    max_length= str_value.numchars()*cs->mbmaxlen;
    set_name(name_par, 0, cs);
    decimals=NOT_FIXED_DEC;
    // it is constant => can be used without fix_fields (and frequently used)
    fixed= 1;
  }
  /*
    This is used in stored procedures to avoid memory leaks and
    does a deep copy of its argument.
  */
  void set_str_with_copy(const char *str_arg, uint length_arg)
  {
    str_value.copy(str_arg, length_arg, collation.collation);
    max_length= str_value.numchars() * collation.collation->mbmaxlen;
  }
  enum Type type() const { return STRING_ITEM; }
  double val_real();
  longlong val_int();
  String *val_str(String*)
  {
    DBUG_ASSERT(fixed == 1);
    return (String*) &str_value;
  }
  my_decimal *val_decimal(my_decimal *);
  int save_in_field(Field *field, bool no_conversions);
  enum Item_result result_type () const { return STRING_RESULT; }
  enum_field_types field_type() const { return MYSQL_TYPE_VARCHAR; }
  bool basic_const_item() const { return 1; }
  bool eq(const Item *item, bool binary_cmp) const;
  Item *new_item() 
  {
    return new Item_string(name, str_value.ptr(), 
    			   str_value.length(), collation.collation);
  }
  Item *safe_charset_converter(CHARSET_INFO *tocs);
  inline void append(char *str, uint length) { str_value.append(str, length); }
  void print(String *str);
  // to prevent drop fixed flag (no need parent cleanup call)
  void cleanup() {}
};


class Item_static_string_func :public Item_string
{
  const char *func_name;
public:
  Item_static_string_func(const char *name_par, const char *str, uint length,
                          CHARSET_INFO *cs,
                          Derivation dv= DERIVATION_COERCIBLE)
    :Item_string(NullS, str, length, cs, dv), func_name(name_par)
  {}
  Item *safe_charset_converter(CHARSET_INFO *tocs);
  void print(String *str) { str->append(func_name); }
};


/* for show tables */

class Item_datetime :public Item_string
{
public:
  Item_datetime(const char *item_name): Item_string(item_name,"",0,
                                                    &my_charset_bin)
  { max_length=19;}
  enum_field_types field_type() const { return MYSQL_TYPE_DATETIME; }
};

class Item_empty_string :public Item_string
{
public:
  Item_empty_string(const char *header,uint length, CHARSET_INFO *cs= NULL) :
    Item_string("",0, cs ? cs : &my_charset_bin)
    { name=(char*) header; max_length= cs ? length * cs->mbmaxlen : length; }
  void make_field(Send_field *field);
};

class Item_return_int :public Item_int
{
  enum_field_types int_field_type;
public:
  Item_return_int(const char *name, uint length,
		  enum_field_types field_type_arg)
    :Item_int(name, 0, length), int_field_type(field_type_arg)
  {
    unsigned_flag=1;
  }
  enum_field_types field_type() const { return int_field_type; }
};


class Item_hex_string: public Item
{
public:
  Item_hex_string(): Item() {}
  Item_hex_string(const char *str,uint str_length);
  enum Type type() const { return VARBIN_ITEM; }
  double val_real()
    { DBUG_ASSERT(fixed == 1); return (double) Item_hex_string::val_int(); }
  longlong val_int();
  bool basic_const_item() const { return 1; }
  String *val_str(String*) { DBUG_ASSERT(fixed == 1); return &str_value; }
  my_decimal *val_decimal(my_decimal *);
  int save_in_field(Field *field, bool no_conversions);
  enum Item_result result_type () const { return STRING_RESULT; }
  enum Item_result cast_to_int_type() const { return INT_RESULT; }
  enum_field_types field_type() const { return MYSQL_TYPE_VARCHAR; }
  // to prevent drop fixed flag (no need parent cleanup call)
  void cleanup() {}
  bool eq(const Item *item, bool binary_cmp) const;
  virtual Item *safe_charset_converter(CHARSET_INFO *tocs);
};


class Item_bin_string: public Item_hex_string
{
public:
  Item_bin_string(const char *str,uint str_length);
};

class Item_result_field :public Item	/* Item with result field */
{
public:
  Field *result_field;				/* Save result here */
  Item_result_field() :result_field(0) {}
  // Constructor used for Item_sum/Item_cond_and/or (see Item comment)
  Item_result_field(THD *thd, Item_result_field *item):
    Item(thd, item), result_field(item->result_field)
  {}
  ~Item_result_field() {}			/* Required with gcc 2.95 */
  Field *get_tmp_table_field() { return result_field; }
  Field *tmp_table_field(TABLE *t_arg) { return result_field; }
  table_map used_tables() const { return 1; }
  virtual void fix_length_and_dec()=0;
  void set_result_field(Field *field) { result_field= field; }
  bool is_result_field() { return 1; }
  void save_in_result_field(bool no_conversions)
  {
    save_in_field(result_field, no_conversions);
  }
  void cleanup();
};


class Item_ref :public Item_ident
{
protected:
  void set_properties();
public:
  enum Ref_Type { REF, DIRECT_REF, VIEW_REF };
  Field *result_field;			 /* Save result here */
  Item **ref;
  Item_ref(Name_resolution_context *context_arg,
           const char *db_arg, const char *table_name_arg,
           const char *field_name_arg)
    :Item_ident(context_arg, db_arg, table_name_arg, field_name_arg),
     result_field(0), ref(0) {}
  /*
    This constructor is used in two scenarios:
    A) *item = NULL
      No initialization is performed, fix_fields() call will be necessary.
      
    B) *item points to an Item this Item_ref will refer to. This is 
      used for GROUP BY. fix_fields() will not be called in this case,
      so we call set_properties to make this item "fixed". set_properties
      performs a subset of action Item_ref::fix_fields does, and this subset
      is enough for Item_ref's used in GROUP BY.
    
    TODO we probably fix a superset of problems like in BUG#6658. Check this 
         with Bar, and if we have a more broader set of problems like this.
  */
  Item_ref(Name_resolution_context *context_arg, Item **item,
           const char *table_name_arg, const char *field_name_arg);

  /* Constructor need to process subselect with temporary tables (see Item) */
  Item_ref(THD *thd, Item_ref *item)
    :Item_ident(thd, item), result_field(item->result_field), ref(item->ref) {}
  enum Type type() const		{ return REF_ITEM; }
  bool eq(const Item *item, bool binary_cmp) const
  { 
    Item *it= ((Item *) item)->real_item();
    return ref && (*ref)->eq(it, binary_cmp);
  }
  double val_real();
  longlong val_int();
  my_decimal *val_decimal(my_decimal *);
  bool val_bool();
  String *val_str(String* tmp);
  bool is_null();
  bool get_date(TIME *ltime,uint fuzzydate);
  double val_result();
  longlong val_int_result();
  String *str_result(String* tmp);
  my_decimal *val_decimal_result(my_decimal *);
  bool val_bool_result();
  bool send(Protocol *prot, String *tmp);
  void make_field(Send_field *field);
  bool fix_fields(THD *, Item **);
  int save_in_field(Field *field, bool no_conversions);
  void save_org_in_field(Field *field);
  enum Item_result result_type () const { return (*ref)->result_type(); }
  enum_field_types field_type() const   { return (*ref)->field_type(); }
  Field *get_tmp_table_field()
  { return result_field ? result_field : (*ref)->get_tmp_table_field(); }
  Item *get_tmp_table_item(THD *thd)
  { 
    return (result_field ? new Item_field(result_field) :
                          (*ref)->get_tmp_table_item(thd));
  }
  table_map used_tables() const		
  { 
    return depended_from ? OUTER_REF_TABLE_BIT : (*ref)->used_tables(); 
  }
  table_map not_null_tables() const { return (*ref)->not_null_tables(); }
  void set_result_field(Field *field)	{ result_field= field; }
  bool is_result_field() { return 1; }
  void save_in_result_field(bool no_conversions)
  {
    (*ref)->save_in_field(result_field, no_conversions);
  }
  Item *real_item()
  {
    return ref ? (*ref)->real_item() : this;
  }
  bool walk(Item_processor processor, byte *arg)
  { return (*ref)->walk(processor, arg); }
  void print(String *str);
  bool result_as_longlong()
  {
    return (*ref)->result_as_longlong();
  }
  void cleanup();
  Item_field *filed_for_view_update()
    { return (*ref)->filed_for_view_update(); }
  virtual Ref_Type ref_type() { return REF; }
};


/*
  The same as Item_ref, but get value from val_* family of method to get
  value of item on which it referred instead of result* family.
*/
class Item_direct_ref :public Item_ref
{
public:
  Item_direct_ref(Name_resolution_context *context_arg, Item **item,
                  const char *table_name_arg,
                  const char *field_name_arg)
    :Item_ref(context_arg, item, table_name_arg, field_name_arg) {}
  /* Constructor need to process subselect with temporary tables (see Item) */
  Item_direct_ref(THD *thd, Item_direct_ref *item) : Item_ref(thd, item) {}

  double val_real();
  longlong val_int();
  String *val_str(String* tmp);
  my_decimal *val_decimal(my_decimal *);
  bool val_bool();
  bool is_null();
  bool get_date(TIME *ltime,uint fuzzydate);
  virtual Ref_Type ref_type() { return DIRECT_REF; }
};

/*
  Class for view fields, the same as Item_direct_ref, but call fix_fields
  of reference if it is not called yet
*/
class Item_direct_view_ref :public Item_direct_ref
{
public:
  Item_direct_view_ref(Name_resolution_context *context_arg, Item **item,
                  const char *table_name_arg,
                  const char *field_name_arg)
    :Item_direct_ref(context_arg, item, table_name_arg, field_name_arg) {}
  /* Constructor need to process subselect with temporary tables (see Item) */
  Item_direct_view_ref(THD *thd, Item_direct_ref *item)
    :Item_direct_ref(thd, item) {}

  bool fix_fields(THD *, Item **);
  bool eq(const Item *item, bool binary_cmp) const;
  virtual Ref_Type ref_type() { return VIEW_REF; }
};


class Item_in_subselect;

class Item_ref_null_helper: public Item_ref
{
protected:
  Item_in_subselect* owner;
public:
  Item_ref_null_helper(Name_resolution_context *context_arg,
                       Item_in_subselect* master, Item **item,
		       const char *table_name_arg, const char *field_name_arg)
    :Item_ref(context_arg, item, table_name_arg, field_name_arg),
     owner(master) {}
  double val_real();
  longlong val_int();
  String* val_str(String* s);
  my_decimal *val_decimal(my_decimal *);
  bool val_bool();
  bool get_date(TIME *ltime, uint fuzzydate);
  void print(String *str);
  /*
    we add RAND_TABLE_BIT to prevent moving this item from HAVING to WHERE
  */
  table_map used_tables() const
  {
    return (depended_from ?
            OUTER_REF_TABLE_BIT :
            (*ref)->used_tables() | RAND_TABLE_BIT);
  }
};

/*
  The following class is used to optimize comparing of date and bigint columns
  We need to save the original item ('ref') to be able to call
  ref->save_in_field(). This is used to create index search keys.
  
  An instance of Item_int_with_ref may have signed or unsigned integer value.
  
*/

class Item_int_with_ref :public Item_int
{
  Item *ref;
public:
  Item_int_with_ref(longlong i, Item *ref_arg, my_bool unsigned_arg) :
    Item_int(i), ref(ref_arg)
  {
    unsigned_flag= unsigned_arg;
  }
  int save_in_field(Field *field, bool no_conversions)
  {
    return ref->save_in_field(field, no_conversions);
  }
  Item *new_item();
  virtual Item *real_item() { return ref; }
};


#include "gstream.h"
#include "spatial.h"
#include "item_sum.h"
#include "item_func.h"
#include "item_row.h"
#include "item_cmpfunc.h"
#include "item_strfunc.h"
#include "item_geofunc.h"
#include "item_timefunc.h"
#include "item_uniq.h"
#include "item_subselect.h"

class Item_copy_string :public Item
{
  enum enum_field_types cached_field_type;
public:
  Item *item;
  Item_copy_string(Item *i) :item(i)
  {
    null_value=maybe_null=item->maybe_null;
    decimals=item->decimals;
    max_length=item->max_length;
    name=item->name;
    cached_field_type= item->field_type();
  }
  enum Type type() const { return COPY_STR_ITEM; }
  enum Item_result result_type () const { return STRING_RESULT; }
  enum_field_types field_type() const { return cached_field_type; }
  double val_real()
  {
    int err_not_used;
    char *end_not_used;
    return (null_value ? 0.0 :
	    my_strntod(str_value.charset(), (char*) str_value.ptr(),
		       str_value.length(), &end_not_used, &err_not_used));
  }
  longlong val_int()
  {
    int err;
    return null_value ? LL(0) : my_strntoll(str_value.charset(),str_value.ptr(),
                                            str_value.length(),10, (char**) 0,
                                            &err); 
  }
  String *val_str(String*);
  my_decimal *val_decimal(my_decimal *);
  void make_field(Send_field *field) { item->make_field(field); }
  void copy();
  int save_in_field(Field *field, bool no_conversions);
  table_map used_tables() const { return (table_map) 1L; }
  bool const_item() const { return 0; }
  bool is_null() { return null_value; }
};


class Cached_item :public Sql_alloc
{
public:
  my_bool null_value;
  Cached_item() :null_value(0) {}
  virtual bool cmp(void)=0;
  virtual ~Cached_item(); /*line -e1509 */
};

class Cached_item_str :public Cached_item
{
  Item *item;
  String value,tmp_value;
public:
  Cached_item_str(THD *thd, Item *arg);
  bool cmp(void);
  ~Cached_item_str();                           // Deallocate String:s
};


class Cached_item_real :public Cached_item
{
  Item *item;
  double value;
public:
  Cached_item_real(Item *item_par) :item(item_par),value(0.0) {}
  bool cmp(void);
};

class Cached_item_int :public Cached_item
{
  Item *item;
  longlong value;
public:
  Cached_item_int(Item *item_par) :item(item_par),value(0) {}
  bool cmp(void);
};


class Cached_item_decimal :public Cached_item
{
  Item *item;
  my_decimal value;
public:
  Cached_item_decimal(Item *item_par);
  bool cmp(void);
};

class Cached_item_field :public Cached_item
{
  char *buff;
  Field *field;
  uint length;

public:
  Cached_item_field(Item_field *item)
  {
    field= item->field;
    buff= (char*) sql_calloc(length=field->pack_length());
  }
  bool cmp(void);
};

class Item_default_value : public Item_field
{
public:
  Item *arg;
  Item_default_value(Name_resolution_context *context_arg)
    :Item_field(context_arg, (const char *)NULL, (const char *)NULL,
               (const char *)NULL),
     arg(NULL) {}
  Item_default_value(Name_resolution_context *context_arg, Item *a)
    :Item_field(context_arg, (const char *)NULL, (const char *)NULL,
                (const char *)NULL),
     arg(a) {}
  enum Type type() const { return DEFAULT_VALUE_ITEM; }
  bool eq(const Item *item, bool binary_cmp) const;
  bool fix_fields(THD *, Item **);
  void print(String *str);
  int save_in_field(Field *field_arg, bool no_conversions);
  table_map used_tables() const { return (table_map)0L; }

  bool walk(Item_processor processor, byte *args)
  {
    return arg->walk(processor, args) ||
      (this->*processor)(args);
  }

  Item *transform(Item_transformer transformer, byte *args);
};

/*
  Item_insert_value -- an implementation of VALUES() function.
  You can use the VALUES(col_name) function in the UPDATE clause
  to refer to column values from the INSERT portion of the INSERT
  ... UPDATE statement. In other words, VALUES(col_name) in the
  UPDATE clause refers to the value of col_name that would be
  inserted, had no duplicate-key conflict occurred.
  In all other places this function returns NULL.
*/

class Item_insert_value : public Item_field
{
public:
  Item *arg;
  Item_insert_value(Name_resolution_context *context_arg, Item *a)
    :Item_field(context_arg, (const char *)NULL, (const char *)NULL,
               (const char *)NULL),
     arg(a) {}
  bool eq(const Item *item, bool binary_cmp) const;
  bool fix_fields(THD *, Item **);
  void print(String *str);
  int save_in_field(Field *field_arg, bool no_conversions)
  {
    return Item_field::save_in_field(field_arg, no_conversions);
  }
  table_map used_tables() const { return (table_map)0L; }

  bool walk(Item_processor processor, byte *args)
  {
    return arg->walk(processor, args) ||
	    (this->*processor)(args);
  }
};


/*
  We need this two enums here instead of sql_lex.h because
  at least one of them is used by Item_trigger_field interface.

  Time when trigger is invoked (i.e. before or after row actually
  inserted/updated/deleted).
*/
enum trg_action_time_type
{
  TRG_ACTION_BEFORE= 0, TRG_ACTION_AFTER= 1, TRG_ACTION_MAX
};

/*
  Event on which trigger is invoked.
*/
enum trg_event_type
{
  TRG_EVENT_INSERT= 0 , TRG_EVENT_UPDATE= 1, TRG_EVENT_DELETE= 2, TRG_EVENT_MAX
};

class Table_triggers_list;

/*
  Represents NEW/OLD version of field of row which is
  changed/read in trigger.

  Note: For this item main part of actual binding to Field object happens
        not during fix_fields() call (like for Item_field) but right after
        parsing of trigger definition, when table is opened, with special
        setup_field() call. On fix_fields() stage we simply choose one of
        two Field instances representing either OLD or NEW version of this
        field.
*/
class Item_trigger_field : public Item_field,
                           private Settable_routine_parameter
{
public:
  /* Is this item represents row from NEW or OLD row ? */
  enum row_version_type {OLD_ROW, NEW_ROW};
  row_version_type row_version;
  /* Next in list of all Item_trigger_field's in trigger */
  Item_trigger_field *next_trg_field;
  /* Index of the field in the TABLE::field array */
  uint field_idx;
  /* Pointer to Table_trigger_list object for table of this trigger */
  Table_triggers_list *triggers;

  Item_trigger_field(Name_resolution_context *context_arg,
                     row_version_type row_ver_arg,
                     const char *field_name_arg,
                     ulong priv, const bool ro)
    :Item_field(context_arg,
               (const char *)NULL, (const char *)NULL, field_name_arg),
     row_version(row_ver_arg), field_idx((uint)-1), original_privilege(priv),
     want_privilege(priv), table_grants(NULL), read_only (ro)
  {}
  void setup_field(THD *thd, TABLE *table, GRANT_INFO *table_grant_info);
  enum Type type() const { return TRIGGER_FIELD_ITEM; }
  bool eq(const Item *item, bool binary_cmp) const;
  bool fix_fields(THD *, Item **);
  void print(String *str);
  table_map used_tables() const { return (table_map)0L; }
  void cleanup();

private:
  void set_required_privilege(bool rw);
  bool set_value(THD *thd, sp_rcontext *ctx, Item **it);

public:
  Settable_routine_parameter *get_settable_routine_parameter()
  {
    return (read_only ? 0 : this);
  }

  bool set_value(THD *thd, Item **it)
  {
    return set_value(thd, NULL, it);
  }

private:
  /*
    'want_privilege' holds privileges required to perform operation on
    this trigger field (SELECT_ACL if we are going to read it and
    UPDATE_ACL if we are going to update it).  It is initialized at
    parse time but can be updated later if this trigger field is used
    as OUT or INOUT parameter of stored routine (in this case
    set_required_privilege() is called to appropriately update
    want_privilege and cleanup() is responsible for restoring of
    original want_privilege once parameter's value is updated).
  */
  ulong original_privilege;
  ulong want_privilege;
  GRANT_INFO *table_grants;
  /*
    Trigger field is read-only unless it belongs to the NEW row in a
    BEFORE INSERT of BEFORE UPDATE trigger.
  */
  bool read_only;
};


class Item_cache: public Item
{
protected:
  Item *example;
  table_map used_table_map;
public:
  Item_cache(): example(0), used_table_map(0) {fixed= 1; null_value= 1;}

  void set_used_tables(table_map map) { used_table_map= map; }

  virtual bool allocate(uint i) { return 0; }
  virtual bool setup(Item *item)
  {
    example= item;
    max_length= item->max_length;
    decimals= item->decimals;
    collation.set(item->collation);
    unsigned_flag= item->unsigned_flag;
    return 0;
  };
  virtual void store(Item *)= 0;
  enum Type type() const { return CACHE_ITEM; }
  static Item_cache* get_cache(Item_result type);
  table_map used_tables() const { return used_table_map; }
  virtual void keep_array() {}
  // to prevent drop fixed flag (no need parent cleanup call)
  void cleanup() {}
  void print(String *str);
};


class Item_cache_int: public Item_cache
{
protected:
  longlong value;
public:
  Item_cache_int(): Item_cache(), value(0) {}

  void store(Item *item);
  double val_real() { DBUG_ASSERT(fixed == 1); return (double) value; }
  longlong val_int() { DBUG_ASSERT(fixed == 1); return value; }
  String* val_str(String *str);
  my_decimal *val_decimal(my_decimal *);
  enum Item_result result_type() const { return INT_RESULT; }
};


class Item_cache_real: public Item_cache
{
  double value;
public:
  Item_cache_real(): Item_cache(), value(0) {}

  void store(Item *item);
  double val_real() { DBUG_ASSERT(fixed == 1); return value; }
  longlong val_int();
  String* val_str(String *str);
  my_decimal *val_decimal(my_decimal *);
  enum Item_result result_type() const { return REAL_RESULT; }
};


class Item_cache_decimal: public Item_cache
{
protected:
  my_decimal decimal_value;
public:
  Item_cache_decimal(): Item_cache() {}

  void store(Item *item);
  double val_real();
  longlong val_int();
  String* val_str(String *str);
  my_decimal *val_decimal(my_decimal *);
  enum Item_result result_type() const { return DECIMAL_RESULT; }
};


class Item_cache_str: public Item_cache
{
  char buffer[STRING_BUFFER_USUAL_SIZE];
  String *value, value_buff;
public:
  Item_cache_str(): Item_cache(), value(0) { }

  void store(Item *item);
  double val_real();
  longlong val_int();
  String* val_str(String *) { DBUG_ASSERT(fixed == 1); return value; }
  my_decimal *val_decimal(my_decimal *);
  enum Item_result result_type() const { return STRING_RESULT; }
  CHARSET_INFO *charset() const { return value->charset(); };
};

class Item_cache_row: public Item_cache
{
  Item_cache  **values;
  uint item_count;
  bool save_array;
public:
  Item_cache_row()
    :Item_cache(), values(0), item_count(2), save_array(0) {}
  
  /*
    'allocate' used only in row transformer, to preallocate space for row 
    cache.
  */
  bool allocate(uint num);
  /*
    'setup' is needed only by row => it not called by simple row subselect
    (only by IN subselect (in subselect optimizer))
  */
  bool setup(Item *item);
  void store(Item *item);
  void illegal_method_call(const char *);
  void make_field(Send_field *)
  {
    illegal_method_call((const char*)"make_field");
  };
  double val_real()
  {
    illegal_method_call((const char*)"val");
    return 0;
  };
  longlong val_int()
  {
    illegal_method_call((const char*)"val_int");
    return 0;
  };
  String *val_str(String *)
  {
    illegal_method_call((const char*)"val_str");
    return 0;
  };
  my_decimal *val_decimal(my_decimal *val)
  {
    illegal_method_call((const char*)"val_decimal");
    return 0;
  };

  enum Item_result result_type() const { return ROW_RESULT; }
  
  uint cols() { return item_count; }
  Item* el(uint i) { return values[i]; }
  Item** addr(uint i) { return (Item **) (values + i); }
  bool check_cols(uint c);
  bool null_inside();
  void bring_value();
  void keep_array() { save_array= 1; }
  void cleanup()
  {
    DBUG_ENTER("Item_cache_row::cleanup");
    Item_cache::cleanup();
    if (save_array)
      bzero(values, item_count*sizeof(Item**));
    else
      values= 0;
    DBUG_VOID_RETURN;
  }
};


/*
  Item_type_holder used to store type. name, length of Item for UNIONS &
  derived tables.

  Item_type_holder do not need cleanup() because its time of live limited by
  single SP/PS execution.
*/
class Item_type_holder: public Item
{
protected:
  TYPELIB *enum_set_typelib;
  enum_field_types fld_type;

  void get_full_info(Item *item);

  /* It is used to count decimal precision in join_types */
  int prev_decimal_int_part;
public:
  Item_type_holder(THD*, Item*);

  Item_result result_type() const;
  enum_field_types field_type() const { return fld_type; };
  enum Type type() const { return TYPE_HOLDER; }
  double val_real();
  longlong val_int();
  my_decimal *val_decimal(my_decimal *);
  String *val_str(String*);
  bool join_types(THD *thd, Item *);
  Field *make_field_by_type(TABLE *table);
  static uint32 display_length(Item *item);
  static enum_field_types get_real_type(Item *);
};


class st_select_lex;
void mark_select_range_as_dependent(THD *thd,
                                    st_select_lex *last_select,
                                    st_select_lex *current_sel,
                                    Field *found_field, Item *found_item,
                                    Item_ident *resolved_item);

extern Cached_item *new_Cached_item(THD *thd, Item *item);
extern Item_result item_cmp_type(Item_result a,Item_result b);
extern void resolve_const_item(THD *thd, Item **ref, Item *cmp_item);
extern bool field_is_equal_to_item(Field *field,Item *item);
