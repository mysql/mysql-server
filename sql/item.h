#ifndef ITEM_INCLUDED
#define ITEM_INCLUDED

/* Copyright (c) 2000, 2013, Oracle and/or its affiliates. All rights reserved.

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


#include "sql_priv.h"                /* STRING_BUFFER_USUAL_SIZE */
#include "unireg.h"
#include "sql_const.h"                 /* RAND_TABLE_BIT, MAX_FIELD_NAME */
#include "unireg.h"                    // REQUIRED: for other includes
#include "thr_malloc.h"                         /* sql_calloc */
#include "field.h"                              /* Derivation */
#include "sql_array.h"

class Protocol;
struct TABLE_LIST;
void item_init(void);			/* Init item functions */
class Item_field;
class user_var_entry;

typedef Bounds_checked_array<Item*> Ref_ptr_array;

static inline uint32
char_to_byte_length_safe(uint32 char_length_arg, uint32 mbmaxlen_arg)
{
   ulonglong tmp= ((ulonglong) char_length_arg) * mbmaxlen_arg;
   return (tmp > UINT_MAX32) ? (uint32) UINT_MAX32 : (uint32) tmp;
}


/*
   "Declared Type Collation"
   A combination of collation and its derivation.

  Flags for collation aggregation modes:
  MY_COLL_ALLOW_SUPERSET_CONV  - allow conversion to a superset
  MY_COLL_ALLOW_COERCIBLE_CONV - allow conversion of a coercible value
                                 (i.e. constant).
  MY_COLL_ALLOW_CONV           - allow any kind of conversion
                                 (combination of the above two)
  MY_COLL_ALLOW_NUMERIC_CONV   - if all items were numbers, convert to
                                 @@character_set_connection
  MY_COLL_DISALLOW_NONE        - don't allow return DERIVATION_NONE
                                 (e.g. when aggregating for comparison)
  MY_COLL_CMP_CONV             - combination of MY_COLL_ALLOW_CONV
                                 and MY_COLL_DISALLOW_NONE
*/

#define MY_COLL_ALLOW_SUPERSET_CONV   1
#define MY_COLL_ALLOW_COERCIBLE_CONV  2
#define MY_COLL_DISALLOW_NONE         4
#define MY_COLL_ALLOW_NUMERIC_CONV    8

#define MY_COLL_ALLOW_CONV (MY_COLL_ALLOW_SUPERSET_CONV | MY_COLL_ALLOW_COERCIBLE_CONV)
#define MY_COLL_CMP_CONV   (MY_COLL_ALLOW_CONV | MY_COLL_DISALLOW_NONE)

class DTCollation {
public:
  const CHARSET_INFO *collation;
  enum Derivation derivation;
  uint repertoire;
  
  void set_repertoire_from_charset(const CHARSET_INFO *cs)
  {
    repertoire= cs->state & MY_CS_PUREASCII ?
                MY_REPERTOIRE_ASCII : MY_REPERTOIRE_UNICODE30;
  }
  DTCollation()
  {
    collation= &my_charset_bin;
    derivation= DERIVATION_NONE;
    repertoire= MY_REPERTOIRE_UNICODE30;
  }
  DTCollation(const CHARSET_INFO *collation_arg, Derivation derivation_arg)
  {
    collation= collation_arg;
    derivation= derivation_arg;
    set_repertoire_from_charset(collation_arg);
  }
  void set(DTCollation &dt)
  { 
    collation= dt.collation;
    derivation= dt.derivation;
    repertoire= dt.repertoire;
  }
  void set(const CHARSET_INFO *collation_arg, Derivation derivation_arg)
  {
    collation= collation_arg;
    derivation= derivation_arg;
    set_repertoire_from_charset(collation_arg);
  }
  void set(const CHARSET_INFO *collation_arg,
           Derivation derivation_arg,
           uint repertoire_arg)
  {
    collation= collation_arg;
    derivation= derivation_arg;
    repertoire= repertoire_arg;
  }
  void set_numeric()
  {
    collation= &my_charset_numeric;
    derivation= DERIVATION_NUMERIC;
    repertoire= MY_REPERTOIRE_NUMERIC;
  }
  void set(const CHARSET_INFO *collation_arg)
  {
    collation= collation_arg;
    set_repertoire_from_charset(collation_arg);
  }
  void set(Derivation derivation_arg)
  { derivation= derivation_arg; }
  void set_repertoire(uint repertoire_arg)
  { repertoire= repertoire_arg; }
  bool aggregate(DTCollation &dt, uint flags= 0);
  bool set(DTCollation &dt1, DTCollation &dt2, uint flags= 0)
  { set(dt1); return aggregate(dt2, flags); }
  const char *derivation_name() const
  {
    switch(derivation)
    {
      case DERIVATION_NUMERIC:   return "NUMERIC";
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

/**
  Storage for name strings.
  Enpowers Simple_cstring with allocation routines from the sql_strmake family.

  This class must stay as small as possible as we often 
  pass it into functions using call-by-value evaluation.

  Don't add new members or virual methods into this class!
*/
class Name_string: public Simple_cstring
{
private:
  void set_or_copy(const char *str, size_t length, bool is_null_terminated)
  {
    if (is_null_terminated)
      set(str, length);
    else
      copy(str, length);  
  }
public:
  Name_string(): Simple_cstring() {}
  /*
    Please do NOT add constructor Name_string(const char *str) !
    It will involve hidden strlen() call, which can affect
    performance negatively. Use Name_string(str, len) instead.
  */
  Name_string(const char *str, size_t length):
    Simple_cstring(str, length) {}
  Name_string(const LEX_STRING str): Simple_cstring(str) {}
  Name_string(const char *str, size_t length, bool is_null_terminated):
    Simple_cstring()
  {
    set_or_copy(str, length, is_null_terminated);
  }
  Name_string(const LEX_STRING str, bool is_null_terminated):
    Simple_cstring()
  {
    set_or_copy(str.str, str.length, is_null_terminated);
  }
  /**
    Allocate space using sql_strmake() or sql_strmake_with_convert().
  */
  void copy(const char *str, size_t length, const CHARSET_INFO *cs);
  /**
    Variants for copy(), for various argument combinations.
  */
  void copy(const char *str, size_t length)
  {
    copy(str, length, system_charset_info);
  }
  void copy(const char *str)
  {
    copy(str, (str ? strlen(str) : 0), system_charset_info);
  }
  void copy(const LEX_STRING lex)
  {
    copy(lex.str, lex.length);
  }
  void copy(const LEX_STRING *lex)
  {
    copy(lex->str, lex->length);
  }
  void copy(const Name_string str)
  {
    copy(str.ptr(), str.length());
  }
  /**
    Compare name to another name in C string, case insensitively.
  */
  bool eq(const char *str) const
  {
    DBUG_ASSERT(str && ptr());
    return my_strcasecmp(system_charset_info, ptr(), str) == 0;
  }
  bool eq_safe(const char *str) const
  {
    return is_set() && str && eq(str);
  }
  /**
    Compare name to another name in Name_string, case insensitively.
  */
  bool eq(const Name_string name) const
  {
    return eq(name.ptr());
  }
  bool eq_safe(const Name_string name) const
  {
    return is_set() && name.is_set() && eq(name);
  }
};


#define NAME_STRING(x)  Name_string(C_STRING_WITH_LEN(x))


extern const Name_string null_name_string;


/**
  Storage for Item names.
  Adds "autogenerated" flag and warning functionality to Name_string.
*/
class Item_name_string: public Name_string
{
private:
  bool m_is_autogenerated; /* indicates if name of this Item
                              was autogenerated or set by user */
public:
  Item_name_string(): Name_string(), m_is_autogenerated(true)
  { }
  Item_name_string(const Name_string name)
    :Name_string(name), m_is_autogenerated(true)
  { }
  /**
    Set m_is_autogenerated flag to the given value.
  */
  void set_autogenerated(bool is_autogenerated)
  {
    m_is_autogenerated= is_autogenerated;
  }
  /**
    Return the auto-generated flag.
  */
  bool is_autogenerated() const { return m_is_autogenerated; }
  using Name_string::copy;
  /**
    Copy name together with autogenerated flag.
    Produce a warning if name was cut.
  */
  void copy(const char *str_arg, size_t length_arg, const CHARSET_INFO *cs_arg,
           bool is_autogenerated_arg);
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

  /**
    When TRUE, items are resolved in this context against
    SELECT_LEX::item_list, SELECT_lex::group_list and
    this->table_list. If FALSE, items are resolved only against
    this->table_list.

    @see st_select_lex::item_list, st_select_lex::group_list
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
  TABLE_LIST *save_next_local;

public:
  Name_resolution_context_state() {}          /* Remove gcc warning */

public:
  /* Save the state of a name resolution context. */
  void save_state(Name_resolution_context *context, TABLE_LIST *table_list)
  {
    save_table_list=                  context->table_list;
    save_first_name_resolution_table= context->first_name_resolution_table;
    save_resolve_in_select_list=      context->resolve_in_select_list;
    save_next_local=                  table_list->next_local;
    save_next_name_resolution_table=  table_list->next_name_resolution_table;
  }

  /* Restore a name resolution context from saved state. */
  void restore_state(Name_resolution_context *context, TABLE_LIST *table_list)
  {
    table_list->next_local=                save_next_local;
    table_list->next_name_resolution_table= save_next_name_resolution_table;
    context->table_list=                   save_table_list;
    context->first_name_resolution_table=  save_first_name_resolution_table;
    context->resolve_in_select_list=       save_resolve_in_select_list;
  }

  TABLE_LIST *get_first_name_resolution_table()
  {
    return save_first_name_resolution_table;
  }
};


/*
  This enum is used to report information about monotonicity of function
  represented by Item* tree.
  Monotonicity is defined only for Item* trees that represent table
  partitioning expressions (i.e. have no subselects/user vars/PS parameters
  etc etc). An Item* tree is assumed to have the same monotonicity properties
  as its correspoinding function F:

  [signed] longlong F(field1, field2, ...) {
    put values of field_i into table record buffer;
    return item->val_int(); 
  }

  NOTE
  At the moment function monotonicity is not well defined (and so may be
  incorrect) for Item trees with parameters/return types that are different
  from INT_RESULT, may be NULL, or are unsigned.
  It will be possible to address this issue once the related partitioning bugs
  (BUG#16002, BUG#15447, BUG#13436) are fixed.

  The NOT_NULL enums are used in TO_DAYS, since TO_DAYS('2001-00-00') returns
  NULL which puts those rows into the NULL partition, but
  '2000-12-31' < '2001-00-00' < '2001-01-01'. So special handling is needed
  for this (see Bug#20577).
*/

typedef enum monotonicity_info 
{
   NON_MONOTONIC,              /* none of the below holds */
   MONOTONIC_INCREASING,       /* F() is unary and (x < y) => (F(x) <= F(y)) */
   MONOTONIC_INCREASING_NOT_NULL,  /* But only for valid/real x and y */
   MONOTONIC_STRICT_INCREASING,/* F() is unary and (x < y) => (F(x) <  F(y)) */
   MONOTONIC_STRICT_INCREASING_NOT_NULL  /* But only for valid/real x and y */
} enum_monotonicity_info;

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

  virtual void set_out_param_info(Send_field *info) {}

  virtual const Send_field *get_out_param_info() const
  { return NULL; }
};


typedef bool (Item::*Item_processor) (uchar *arg);
/*
  Analyzer function
    SYNOPSIS
      argp   in/out IN:  Analysis parameter
                    OUT: Parameter to be passed to the transformer

    RETURN 
      TRUE   Invoke the transformer
      FALSE  Don't do it

*/
typedef bool (Item::*Item_analyzer) (uchar **argp);
typedef Item* (Item::*Item_transformer) (uchar *arg);
typedef void (*Cond_traverser) (const Item *item, void *arg);


class Item
{
  Item(const Item &);			/* Prevent use of these */
  void operator=(Item &);
  /* Cache of the result of is_expensive(). */
  int8 is_expensive_cache;
  virtual bool is_expensive_processor(uchar *arg) { return 0; }

public:
  static void *operator new(size_t size) throw ()
  { return sql_alloc(size); }
  static void *operator new(size_t size, MEM_ROOT *mem_root) throw ()
  { return alloc_root(mem_root, size); }
  static void operator delete(void *ptr,size_t size) { TRASH(ptr, size); }
  static void operator delete(void *ptr, MEM_ROOT *mem_root) {}

  enum Type {FIELD_ITEM= 0, FUNC_ITEM, SUM_FUNC_ITEM, STRING_ITEM,
	     INT_ITEM, REAL_ITEM, NULL_ITEM, VARBIN_ITEM,
	     COPY_STR_ITEM, FIELD_AVG_ITEM, DEFAULT_VALUE_ITEM,
	     PROC_ITEM,COND_ITEM, REF_ITEM, FIELD_STD_ITEM,
	     FIELD_VARIANCE_ITEM, INSERT_VALUE_ITEM,
             SUBSELECT_ITEM, ROW_ITEM, CACHE_ITEM, TYPE_HOLDER,
             PARAM_ITEM, TRIGGER_FIELD_ITEM, DECIMAL_ITEM,
             XPATH_NODESET, XPATH_NODESET_CMP,
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

  Item_name_string item_name;  /* Name from select */
  Item_name_string orig_name;  /* Original item name (if it was renamed)*/

  /**
     Intrusive list pointer for free list. If not null, points to the next
     Item on some Query_arena's free list. For instance, stored procedures
     have their own Query_arena's.

     @see Query_arena::free_list
   */
  Item *next;
  uint32 max_length;                    /* Maximum length, in bytes */
  /**
     This member has several successive meanings, depending on the phase we're
     in:
     - during field resolution: it contains the index, in the "all_fields"
     list, of the expression to which this field belongs; or a special
     constant UNDEF_POS; see st_select_lex::cur_pos_in_all_fields and
     match_exprs_for_only_full_group_by().
     - when attaching conditions to tables: it says whether some condition
     needs to be attached or can be omitted (for example because it is already
     implemented by 'ref' access)
     - when pushing index conditions: it says whether a condition uses only
     indexed columns
     - when creating an internal temporary table: it says how to store BIT
     fields
     - when we change DISTINCT to GROUP BY: it is used for book-keeping of
     fields.
  */
  int marker;
  uint8 decimals;
  my_bool maybe_null;			/* If item may be null */
  my_bool null_value;			/* if item is null */
  my_bool unsigned_flag;
  my_bool with_sum_func;
  my_bool fixed;                        /* If item fixed with fix_fields */
  DTCollation collation;
  Item_result cmp_context;              /* Comparison context */
 protected:
  my_bool with_subselect;               /* If this item is a subselect or some
                                           of its arguments is or contains a
                                           subselect. Computed by fix_fields
                                           and updated by update_used_tables. */
  my_bool with_stored_program;          /* If this item is a stored program
                                           or some of its arguments is or
                                           contains a stored program.
                                           Computed by fix_fields and updated
                                           by update_used_tables. */

  /**
    This variable is a cache of 'Needed tables are locked'. True if either
    'No tables locks is needed' or 'Needed tables are locked'.
    If tables are used, then it will be set to
    current_thd->lex->is_query_tables_locked().

    It is used when checking const_item()/can_be_evaluated_now().
  */
  bool tables_locked_cache;
 public:
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
    item_name.set(0);
#endif
  }		/*lint -e1509 */
  void rename(char *new_name);
  void init_make_field(Send_field *tmp_field,enum enum_field_types type);
  virtual void cleanup();
  virtual void make_field(Send_field *field);
  virtual Field *make_string_field(TABLE *table);
  virtual bool fix_fields(THD *, Item **);
  /**
    Fix after tables have been moved from one select_lex level to the parent
    level, e.g by semijoin conversion.
    Basically re-calculate all attributes dependent on the tables.

    @param parent_select  select_lex that tables are moved to.
    @param removed_select select_lex that tables are moved away from,
                          child of parent_select.
  */
  virtual void fix_after_pullout(st_select_lex *parent_select,
                                 st_select_lex *removed_select)
  {};
  /*
    should be used in case where we are sure that we do not need
    complete fix_fields() procedure.
  */
  inline void quick_fix_field() { fixed= 1; }
  /* Function returns 1 on overflow and -1 on fatal errors */
  type_conversion_status save_in_field_no_warnings(Field *field,
                                                   bool no_conversions);
  /**
    Save a temporal value in packed longlong format into a Field.
    Used in optimizer.
    @param OUT field  The field to set the value to.
    @retval 0         On success.
    @retval >0        In error.
  */
  virtual type_conversion_status save_in_field(Field *field,
                                               bool no_conversions);
  virtual void save_org_in_field(Field *field)
  { (void) save_in_field(field, 1); }
  virtual type_conversion_status save_safe_in_field(Field *field)
  { return save_in_field(field, 1); }
  virtual bool send(Protocol *protocol, String *str);
  virtual bool eq(const Item *, bool binary_cmp) const;
  virtual Item_result result_type() const { return REAL_RESULT; }
  /**
    Result type when an item appear in a numeric context.
    See Field::numeric_context_result_type() for more comments.
  */
  virtual enum Item_result numeric_context_result_type() const
  {
    if (is_temporal())
      return decimals ? DECIMAL_RESULT : INT_RESULT;
    if (result_type() == STRING_RESULT)
      return REAL_RESULT; 
    return result_type();
  }
  /**
    Similar to result_type() but makes DATE, DATETIME, TIMESTAMP
    pretend to be numbers rather than strings.
  */
  inline enum Item_result temporal_with_date_as_number_result_type() const
  {
    return is_temporal_with_date() ? 
           (decimals ? DECIMAL_RESULT : INT_RESULT) : result_type();
  }
  virtual Item_result cast_to_int_type() const { return result_type(); }
  virtual enum_field_types string_field_type() const;
  virtual enum_field_types field_type() const;
  virtual enum Type type() const =0;
  
  /*
    Return information about function monotonicity. See comment for
    enum_monotonicity_info for details. This function can only be called
    after fix_fields() call.
  */
  virtual enum_monotonicity_info get_monotonicity_info() const
  { return NON_MONOTONIC; }

  /*
    Convert "func_arg $CMP$ const" half-interval into "FUNC(func_arg) $CMP2$ const2"

    SYNOPSIS
      val_int_endpoint()
        left_endp  FALSE  <=> The interval is "x < const" or "x <= const"
                   TRUE   <=> The interval is "x > const" or "x >= const"

        incl_endp  IN   FALSE <=> the comparison is '<' or '>'
                        TRUE  <=> the comparison is '<=' or '>='
                   OUT  The same but for the "F(x) $CMP$ F(const)" comparison

    DESCRIPTION
      This function is defined only for unary monotonic functions. The caller
      supplies the source half-interval

         x $CMP$ const

      The value of const is supplied implicitly as the value this item's
      argument, the form of $CMP$ comparison is specified through the
      function's arguments. The calle returns the result interval
         
         F(x) $CMP2$ F(const)
      
      passing back F(const) as the return value, and the form of $CMP2$ 
      through the out parameter. NULL values are assumed to be comparable and
      be less than any non-NULL values.

    RETURN
      The output range bound, which equal to the value of val_int()
        - If the value of the function is NULL then the bound is the 
          smallest possible value of LONGLONG_MIN 
  */
  virtual longlong val_int_endpoint(bool left_endp, bool *incl_endp)
  { DBUG_ASSERT(0); return 0; }


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
  /**
    Return date value of item in packed longlong format.
  */
  virtual longlong val_date_temporal();
  /**
    Return time value of item in packed longlong format.
  */
  virtual longlong val_time_temporal();
  /**
    Return date or time value of item in packed longlong format,
    depending on item field type.
  */
  longlong val_temporal_by_field_type()
  {
    if (field_type() == MYSQL_TYPE_TIME)
      return val_time_temporal();
    DBUG_ASSERT(is_temporal_with_date());
    return val_date_temporal();
  }
  /**
    Get date or time value in packed longlong format.
    Before conversion from MYSQL_TIME to packed format,
    the MYSQL_TIME value is rounded to "dec" fractional digits.
  */
  longlong val_temporal_with_round(enum_field_types type, uint8 dec);

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
    Returns string representation of this item in ASCII format.

    SYNOPSIS
      val_str_ascii()
      str - similar to val_str();

    NOTE
      This method is introduced for performance optimization purposes.

      1. val_str() result of some Items in string context
      depends on @@character_set_results.
      @@character_set_results can be set to a "real multibyte" character
      set like UCS2, UTF16, UTF32. (We'll use only UTF32 in the examples
      below for convenience.)

      So the default string result of such functions
      in these circumstances is real multi-byte character set, like UTF32.

      For example, all numbers in string context
      return result in @@character_set_results:

      SELECT CONCAT(20010101); -> UTF32

      We do sprintf() first (to get ASCII representation)
      and then convert to UTF32;
      
      So these kind "data sources" can use ASCII representation
      internally, but return multi-byte data only because
      @@character_set_results wants so.
      Therefore, conversion from ASCII to UTF32 is applied internally.


      2. Some other functions need in fact ASCII input.

      For example,
        inet_aton(), GeometryFromText(), Convert_TZ(), GET_FORMAT().

      Similar, fields of certain type, like DATE, TIME,
      when you insert string data into them, expect in fact ASCII input.
      If they get non-ASCII input, for example UTF32, they
      convert input from UTF32 to ASCII, and then use ASCII
      representation to do further processing.


      3. Now imagine we pass result of a data source of the first type
         to a data destination of the second type.

      What happens:
        a. data source converts data from ASCII to UTF32, because
           @@character_set_results wants so and passes the result to
           data destination.
        b. data destination gets UTF32 string.
        c. data destination converts UTF32 string to ASCII,
           because it needs ASCII representation to be able to handle data
           correctly.

      As a result we get two steps of unnecessary conversion:
      From ASCII to UTF32, then from UTF32 to ASCII.

      A better way to handle these situations is to pass ASCII
      representation directly from the source to the destination.

      This is why val_str_ascii() introduced.

    RETURN
      Similar to val_str()
  */
  virtual String *val_str_ascii(String *str);
  
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
  virtual String *val_nodeset(String*) { return 0; }

protected:
  /* Helper functions, see item_sum.cc */
  String *val_string_from_real(String *str);
  String *val_string_from_int(String *str);
  String *val_string_from_decimal(String *str);
  String *val_string_from_date(String *str);
  String *val_string_from_datetime(String *str);
  String *val_string_from_time(String *str);
  my_decimal *val_decimal_from_real(my_decimal *decimal_value);
  my_decimal *val_decimal_from_int(my_decimal *decimal_value);
  my_decimal *val_decimal_from_string(my_decimal *decimal_value);
  my_decimal *val_decimal_from_date(my_decimal *decimal_value);
  my_decimal *val_decimal_from_time(my_decimal *decimal_value);
  longlong val_int_from_decimal();
  longlong val_int_from_date();
  longlong val_int_from_time();
  longlong val_int_from_datetime();
  double val_real_from_decimal();

  /**
    Convert val_str() to date in MYSQL_TIME
  */
  bool get_date_from_string(MYSQL_TIME *ltime, uint flags);
  /**
    Convert val_real() to date in MYSQL_TIME
  */
  bool get_date_from_real(MYSQL_TIME *ltime, uint flags);
  /**
    Convert val_decimal() to date in MYSQL_TIME
  */
  bool get_date_from_decimal(MYSQL_TIME *ltime, uint flags);
  /**
    Convert val_int() to date in MYSQL_TIME
  */
  bool get_date_from_int(MYSQL_TIME *ltime, uint flags);
  /**
    Convert get_time() from time to date in MYSQL_TIME
  */
  bool get_date_from_time(MYSQL_TIME *ltime);

  /**
    Convert a numeric type to date
  */
  bool get_date_from_numeric(MYSQL_TIME *ltime, uint fuzzydate);

  /**
    Convert a non-temporal type to date
  */
  bool get_date_from_non_temporal(MYSQL_TIME *ltime, uint fuzzydate);

  /**
    Convert val_str() to time in MYSQL_TIME
  */
  bool get_time_from_string(MYSQL_TIME *ltime);
  /**
    Convert val_real() to time in MYSQL_TIME
  */
  bool get_time_from_real(MYSQL_TIME *ltime);
  /**
    Convert val_decimal() to time in MYSQL_TIME
  */
  bool get_time_from_decimal(MYSQL_TIME *ltime);
  /**
    Convert val_int() to time in MYSQL_TIME
  */
  bool get_time_from_int(MYSQL_TIME *ltime);
  /**
    Convert date to time
  */
  bool get_time_from_date(MYSQL_TIME *ltime);
  /**
    Convert datetime to time
  */
  bool get_time_from_datetime(MYSQL_TIME *ltime);

  /**
    Convert a numeric type to time
  */
  bool get_time_from_numeric(MYSQL_TIME *ltime);

  /**
    Convert a non-temporal type to time
  */
  bool get_time_from_non_temporal(MYSQL_TIME *ltime);


public:

  type_conversion_status save_time_in_field(Field *field);
  type_conversion_status save_date_in_field(Field *field);
  type_conversion_status save_str_value_in_field(Field *field, String *result);

  virtual Field *get_tmp_table_field() { return 0; }
  /* This is also used to create fields in CREATE ... SELECT: */
  virtual Field *tmp_table_field(TABLE *t_arg) { return 0; }
  virtual const char *full_name() const
  {
    return item_name.is_set() ? item_name.ptr() : "???";
  }

  /*
    *result* family of methods is analog of *val* family (see above) but
    return value of result_field of item if it is present. If Item have not
    result field, it return val(). This methods set null_value flag in same
    way as *val* methods do it.
  */
  virtual double  val_result() { return val_real(); }
  virtual longlong val_int_result() { return val_int(); }
  /**
    Get time value in packed longlong format. NULL is converted to 0.
  */
  virtual longlong val_time_temporal_result() { return val_time_temporal(); }
  /**
    Get date value in packed longlong format. NULL is converted to 0.
  */
  virtual longlong val_date_temporal_result() { return val_date_temporal(); }
  virtual String *str_result(String* tmp) { return val_str(tmp); }
  virtual my_decimal *val_decimal_result(my_decimal *val)
  { return val_decimal(val); }
  virtual bool val_bool_result() { return val_bool(); }
  virtual bool is_null_result() { return is_null(); }

  /* bit map of tables used by item */
  virtual table_map used_tables() const { return (table_map) 0L; }
  /**
    Return used table information for the level this item is resolved on.
     - For fields, this returns the table the item is resolved from.
     - For all other items, this behaves like used_tables().

    @note: Use this function with caution. External calls to this function
           should only be made for class objects derived from Item_ident.
           Item::resolved_used_tables is for internal use only, in order to
           process fields underlying a view column reference.
  */
  virtual table_map resolved_used_tables() const
  {
    // As this is the level this item was resolved on, it cannot be outer:
    DBUG_ASSERT(!(used_tables() & OUTER_REF_TABLE_BIT));

    return used_tables();
  }
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
  virtual Item *clone_item() { return 0; }
  virtual cond_result eq_cmp_result() const { return COND_OK; }
  inline uint float_length(uint decimals_par) const
  { return decimals != NOT_FIXED_DEC ? (DBL_DIG+2+decimals_par) : DBL_DIG+8;}
  virtual uint decimal_precision() const;
  inline int decimal_int_part() const
  { return my_decimal_int_part(decimal_precision(), decimals); }
  /**
    TIME precision of the item: 0..6
  */
  virtual uint time_precision();
  /**
    DATETIME precision of the item: 0..6
  */
  virtual uint datetime_precision();
  /* 
    Returns true if this is constant (during query execution, i.e. its value
    will not change until next fix_fields) and its value is known.
    When the default implementation of used_tables() is effective, this
    function will always return true (because used_tables() is empty).
  */
  virtual bool const_item() const
  {
    if (used_tables() == 0)
      return can_be_evaluated_now();
    return false;
  }
  /* 
    Returns true if this is constant but its value may be not known yet.
    (Can be used for parameters of prep. stmts or of stored procedures.)
  */
  virtual bool const_during_execution() const 
  { return (used_tables() & ~PARAM_TABLE_BIT) == 0; }

  /**
    This method is used for to:
      - to generate a view definition query (SELECT-statement);
      - to generate a SQL-query for EXPLAIN EXTENDED;
      - to generate a SQL-query to be shown in INFORMATION_SCHEMA;
      - debug.

    For more information about view definition query, INFORMATION_SCHEMA
    query and why they should be generated from the Item-tree, @see
    mysql_register_view().
  */
  virtual inline void print(String *str, enum_query_type query_type)
  {
    str->append(full_name());
  }

  void print_item_w_name(String *, enum_query_type query_type);
  /**
     Prints the item when it's part of ORDER BY and GROUP BY.
     @param  str            String to print to
     @param  query_type     How to format the item
     @param  used_alias     Whether item was referenced with alias.
  */
  void print_for_order(String *str, enum_query_type query_type,
                       bool used_alias);

  virtual void update_used_tables() {}
  virtual void split_sum_func(THD *thd, Ref_ptr_array ref_pointer_array,
                              List<Item> &fields) {}
  /* Called for items that really have to be split */
  void split_sum_func2(THD *thd, Ref_ptr_array ref_pointer_array,
                       List<Item> &fields,
                       Item **ref, bool skip_registered);
  virtual bool get_date(MYSQL_TIME *ltime,uint fuzzydate)= 0;
  virtual bool get_time(MYSQL_TIME *ltime)= 0;
  /**
    Get timestamp in "struct timeval" format.
    @retval  false on success
    @retval  true  on error
  */
  virtual bool get_timeval(struct timeval *tm, int *warnings);
  virtual bool get_date_result(MYSQL_TIME *ltime,uint fuzzydate)
  { return get_date(ltime,fuzzydate); }
  /*
    The method allows to determine nullness of a complex expression 
    without fully evaluating it, instead of calling val/result*() then 
    checking null_value. Used in Item_func_isnull/Item_func_isnotnull
    and Item_sum_count/Item_sum_count_distinct.
    Any new item which can be NULL must implement this method.
  */
  virtual bool is_null() { return 0; }

  /*
   Make sure the null_value member has a correct value.
  */
  virtual void update_null_value () { (void) val_int(); }

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
    Set value of aggregate function in case of no rows for grouping were found.
    Also used for subqueries with outer references in SELECT list.
  */
  virtual void no_rows_in_result() {}
  virtual Item *copy_or_same(THD *thd) { return this; }
  /**
     @param real_items  True <=> in the copy, replace any Item_ref with its
     real_item()
     @todo this argument should be always false and removed in WL#7082.
  */
  virtual Item *copy_andor_structure(THD *thd, bool real_items= false)
  { return real_items ? real_item() : this; }
  virtual Item *real_item() { return this; }
  virtual Item *get_tmp_table_item(THD *thd) { return copy_or_same(thd); }

  static const CHARSET_INFO *default_charset();
  virtual const CHARSET_INFO *compare_collation() { return NULL; }

  /*
    For backward compatibility, to make numeric
    data types return "binary" charset in client-side metadata.
  */
  virtual const CHARSET_INFO *charset_for_protocol(void) const
  {
    return result_type() == STRING_RESULT ? collation.collation :
                                            &my_charset_bin;
  };

  virtual bool walk(Item_processor processor, bool walk_subquery, uchar *arg)
  {
    return (this->*processor)(arg);
  }

  virtual Item* transform(Item_transformer transformer, uchar *arg);

  /*
    This function performs a generic "compilation" of the Item tree.
    The process of compilation is assumed to go as follows: 
    
    compile()
    { 
      if (this->*some_analyzer(...))
      {
        compile children if any;
        return this->*some_transformer(...);
      }
      else
        return this;
    }

    i.e. analysis is performed top-down while transformation is done
    bottom-up. If no transformation is applied, the item is returned unchanged.
    A transformation error is indicated by returning a NULL pointer. Notice
    that the analyzer function should never cause an error.
  */
  virtual Item* compile(Item_analyzer analyzer, uchar **arg_p,
                        Item_transformer transformer, uchar *arg_t)
  {
    if ((this->*analyzer) (arg_p))
      return ((this->*transformer) (arg_t));
    return this;
  }

   virtual void traverse_cond(Cond_traverser traverser,
                              void *arg, traverse_order order)
   {
     (*traverser)(this, arg);
   }

  /*
    This is used to get the most recent version of any function in
    an item tree. The version is the version where a MySQL function
    was introduced in. So any function which is added should use
    this function and set the int_arg to maximum of the input data
    and their own version info.
  */
  virtual bool intro_version(uchar *int_arg) { return 0; }

  virtual bool remove_dependence_processor(uchar * arg) { return 0; }
  virtual bool remove_fixed(uchar * arg) { fixed= 0; return 0; }
  virtual bool cleanup_processor(uchar *arg);
  virtual bool collect_item_field_processor(uchar * arg) { return 0; }
  virtual bool add_field_to_set_processor(uchar * arg) { return 0; }

  /**
     Visitor interface for removing all column expressions (Item_field) in
     this expression tree from a bitmap. @See walk()

     @param arg  A MY_BITMAP* cast to unsigned char*, where the bits represent
                 Field::field_index values.
   */
  virtual bool remove_column_from_bitmap(uchar *arg) { return false; }
  virtual bool find_item_in_field_list_processor(uchar *arg) { return 0; }
  virtual bool change_context_processor(uchar *context) { return 0; }
  virtual bool reset_query_id_processor(uchar *query_id_arg) { return 0; }
  virtual bool find_item_processor(uchar *arg) { return this == (void *) arg; }
  virtual bool register_field_in_read_map(uchar *arg) { return 0; }
  virtual bool inform_item_in_cond_of_tab(uchar *join_tab_index) { return false; }
  /**
     Clean up after removing the item from the item tree.

     @param arg Pointer to the st_select_lex from which the walk started, i.e.,
                the st_select_lex that contained the clause that was removed.
  */
  virtual bool clean_up_after_removal(uchar *arg) { return false; }

  virtual bool cache_const_expr_analyzer(uchar **arg);
  virtual Item* cache_const_expr_transformer(uchar *arg);

  /**
     Analyzer for finding Item_field by name
     
     @param arg  Field name to search for
     
     @return TRUE Go deeper in item tree.  (Found Item or not an Item_field)
     @return FALSE Don't go deeper in item tree. (Item_field with other name)
  */
  virtual bool item_field_by_name_analyzer(uchar **arg) { return true; };

  /**
     Simple transformer that returns the argument if this is an Item_field.
     The new item will inherit it's name to maintain aliases.

     @param arg Item to replace Item_field

     @return argument if this is an Item_field
     @return this otherwise.
  */
  virtual Item* item_field_by_name_transformer(uchar *arg) { return this; }

  virtual bool equality_substitution_analyzer(uchar **arg) { return false; }

  virtual Item* equality_substitution_transformer(uchar *arg) { return this; }

  /*
    Check if a partition function is allowed
    SYNOPSIS
      check_partition_func_processor()
      int_arg                        Ignored
    RETURN VALUE
      TRUE                           Partition function not accepted
      FALSE                          Partition function accepted

    DESCRIPTION
    check_partition_func_processor is used to check if a partition function
    uses an allowed function. An allowed function will always ensure that
    X=Y guarantees that also part_function(X)=part_function(Y) where X is
    a set of partition fields and so is Y. The problems comes mainly from
    character sets where two equal strings can be quite unequal. E.g. the
    german character for double s is equal to 2 s.

    The default is that an item is not allowed
    in a partition function. Allowed functions
    can never depend on server version, they cannot depend on anything
    related to the environment. They can also only depend on a set of
    fields in the table itself. They cannot depend on other tables and
    cannot contain any queries and cannot contain udf's or similar.
    If a new Item class is defined and it inherits from a class that is
    allowed in a partition function then it is very important to consider
    whether this should be inherited to the new class. If not the function
    below should be defined in the new Item class.

    The general behaviour is that most integer functions are allowed.
    If the partition function contains any multi-byte collations then
    the function check_part_func_fields will report an error on the
    partition function independent of what functions are used. So the
    only character sets allowed are single character collation and
    even for those only a limited set of functions are allowed. The
    problem with multi-byte collations is that almost every string
    function has the ability to change things such that two strings
    that are equal will not be equal after manipulated by a string
    function. E.g. two strings one contains a double s, there is a
    special german character that is equal to two s. Now assume a
    string function removes one character at this place, then in
    one the double s will be removed and in the other there will
    still be one s remaining and the strings are no longer equal
    and thus the partition function will not sort equal strings into
    the same partitions.

    So the check if a partition function is valid is two steps. First
    check that the field types are valid, next check that the partition
    function is valid. The current set of partition functions valid
    assumes that there are no multi-byte collations amongst the partition
    fields.
  */
  virtual bool check_partition_func_processor(uchar *bool_arg) { return TRUE;}
  virtual bool subst_argument_checker(uchar **arg)
  { 
    if (*arg)
      *arg= NULL; 
    return TRUE;     
  }
  virtual bool explain_subquery_checker(uchar **arg) { return true; }
  virtual Item *explain_subquery_propagator(uchar *arg) { return this; }

  virtual Item *equal_fields_propagator(uchar * arg) { return this; }
  virtual bool set_no_const_sub(uchar *arg) { return FALSE; }
  virtual Item *replace_equal_field(uchar * arg) { return this; }
  /*
    Check if an expression value has allowed arguments, like DATE/DATETIME
    for date functions. Also used by partitioning code to reject
    timezone-dependent expressions in a (sub)partitioning function.
  */
  virtual bool check_valid_arguments_processor(uchar *bool_arg)
  {
    return FALSE;
  }

  /**
    Find a function of a given type

    @param   arg     the function type to search (enum Item_func::Functype)
    @return
      @retval TRUE   the function type we're searching for is found
      @retval FALSE  the function type wasn't found

    @description
      This function can be used (together with Item::walk()) to find functions
      in an item tree fragment.
  */
  virtual bool find_function_processor (uchar *arg)
  {
    return FALSE;
  }

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
  virtual Item **this_item_addr(THD *thd, Item **addr_arg) { return addr_arg; }

  // Row emulation
  virtual uint cols() { return 1; }
  virtual Item* element_index(uint i) { return this; }
  virtual Item** addr(uint i) { return 0; }
  virtual bool check_cols(uint c);
  // It is not row => null inside is impossible
  virtual bool null_inside() { return 0; }
  // used in row subselects to get value of elements
  virtual void bring_value() {}

  Field *tmp_table_field_from_field_type(TABLE *table, bool fixed_length);
  virtual Item_field *field_for_view_update() { return 0; }

  virtual Item *neg_transformer(THD *thd) { return NULL; }
  virtual Item *update_value_transformer(uchar *select_arg) { return this; }
  virtual Item *safe_charset_converter(const CHARSET_INFO *tocs);
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
  inline bool is_temporal_with_date() const
  {
    return is_temporal_type_with_date(field_type());
  }
  inline bool is_temporal_with_date_and_time() const
  {
    return is_temporal_type_with_date_and_time(field_type());
  }
  inline bool is_temporal_with_time() const
  {
    return is_temporal_type_with_time(field_type());
  }
  inline bool is_temporal() const
  {
    return is_temporal_type(field_type());
  }
  /**
    Check whether this and the given item has compatible comparison context.
    Used by the equality propagation. See Item_field::equal_fields_propagator.

    @return
      TRUE  if the context is the same or if fields could be
            compared as DATETIME values by the Arg_comparator.
      FALSE otherwise.
  */
  inline bool has_compatible_context(Item *item) const
  {
    /* Same context. */
    if (cmp_context == (Item_result)-1 || item->cmp_context == cmp_context)
      return TRUE;
    /* DATETIME comparison context. */
    if (is_temporal_with_date())
      return item->is_temporal_with_date() ||
             item->cmp_context == STRING_RESULT;
    if (item->is_temporal_with_date())
      return is_temporal_with_date() || cmp_context == STRING_RESULT;
    return FALSE;
  }
  virtual Field::geometry_type get_geometry_type() const
    { return Field::GEOM_GEOMETRY; };
  String *check_well_formed_result(String *str, bool send_error= 0);
  bool eq_by_collation(Item *item, bool binary_cmp, const CHARSET_INFO *cs); 

  /*
    Test whether an expression is expensive to compute. Used during
    optimization to avoid computing expensive expressions during this
    phase. Also used to force temp tables when sorting on expensive
    functions.
    TODO:
    Normally we should have a method:
      cost Item::execution_cost(),
    where 'cost' is either 'double' or some structure of various cost
    parameters.
  */
  virtual bool is_expensive()
  {
    if (is_expensive_cache < 0)
      is_expensive_cache= walk(&Item::is_expensive_processor, 0, (uchar*)0);
    return MY_TEST(is_expensive_cache);
  }
  virtual bool can_be_evaluated_now() const;
  uint32 max_char_length() const
  { return max_length / collation.collation->mbmaxlen; }
  void fix_length_and_charset(uint32 max_char_length_arg,
                              const CHARSET_INFO *cs)
  {
    max_length= char_to_byte_length_safe(max_char_length_arg, cs->mbmaxlen);
    collation.collation= cs;
  }
  void fix_char_length(uint32 max_char_length_arg)
  {
    max_length= char_to_byte_length_safe(max_char_length_arg,
                                         collation.collation->mbmaxlen);
  }
  void fix_char_length_ulonglong(ulonglong max_char_length_arg)
  {
    ulonglong max_result_length= max_char_length_arg *
                                 collation.collation->mbmaxlen;
    if (max_result_length >= MAX_BLOB_WIDTH)
    {
      max_length= MAX_BLOB_WIDTH;
      maybe_null= 1;
    }
    else
      max_length= (uint32) max_result_length;
  }
  void fix_length_and_charset_datetime(uint32 max_char_length_arg)
  {
    collation.set(&my_charset_numeric, DERIVATION_NUMERIC, MY_REPERTOIRE_ASCII);
    fix_char_length(max_char_length_arg);
  }
  void fix_length_and_dec_and_charset_datetime(uint32 max_char_length_arg,
                                               uint8 dec_arg)
  {
    decimals= dec_arg;
    fix_length_and_charset_datetime(max_char_length_arg +
                                    (dec_arg ? dec_arg + 1 : 0));
  }
  /*
    Return TRUE if the item points to a column of an outer-joined table.
  */
  virtual bool is_outer_field() const { DBUG_ASSERT(fixed); return FALSE; }

  /**
     Check if an item either is a blob field, or will be represented as a BLOB
     field if a field is created based on this item.

     @retval TRUE  If a field based on this item will be a BLOB field,
     @retval FALSE Otherwise.
  */
  bool is_blob_field() const;

  /**
    Checks if this item or any of its decendents contains a subquery.
  */
  virtual bool has_subquery() const { return with_subselect; }
  virtual bool has_stored_program() const { return with_stored_program; }
  /// Whether this Item was created by the IN->EXISTS subquery transformation
  virtual bool created_by_in2exists() const { return false; }
};


class sp_head;


class Item_basic_constant :public Item
{
  table_map used_table_map;
public:
  Item_basic_constant(): Item(), used_table_map(0) {};
  void set_used_tables(table_map map) { used_table_map= map; }
  table_map used_tables() const { return used_table_map; }
  /* to prevent drop fixed flag (no need parent cleanup call) */
  void cleanup()
  {
    /*
      Restore the original field name as it might not have been allocated
      in the statement memory. If the name is auto generated, it must be
      done again between subsequent executions of a prepared statement.
    */
    if (orig_name.is_set())
      item_name= orig_name;
  }
};


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
  Name_string m_name;

public:
#ifndef DBUG_OFF
  /*
    Routine to which this Item_splocal belongs. Used for checking if correct
    runtime context is used for variable handling.
  */
  sp_head *m_sp;
#endif

public:
  Item_sp_variable(const Name_string sp_var_name);

public:
  bool fix_fields(THD *thd, Item **);

  double val_real();
  longlong val_int();
  String *val_str(String *sp);
  my_decimal *val_decimal(my_decimal *decimal_value);
  bool get_date(MYSQL_TIME *ltime, uint fuzzydate);
  bool get_time(MYSQL_TIME *ltime);
  bool is_null();

public:
  inline void make_field(Send_field *field);  
  inline type_conversion_status save_in_field(Field *field,
                                              bool no_conversions);
  inline bool send(Protocol *protocol, String *str);
}; 

/*****************************************************************************
  Item_sp_variable inline implementation.
*****************************************************************************/

inline void Item_sp_variable::make_field(Send_field *field)
{
  Item *it= this_item();
  it->item_name.copy(item_name.is_set() ? item_name : m_name);
  it->make_field(field);
}

inline type_conversion_status
Item_sp_variable::save_in_field(Field *field, bool no_conversions)
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
  enum_field_types m_field_type;
public:
  /*
    If this variable is a parameter in LIMIT clause.
    Used only during NAME_CONST substitution, to not append
    NAME_CONST to the resulting query and thus not break
    the slave.
  */
  bool limit_clause_param;
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
  /*
    Byte length of SP variable name in the statement (see pos_in_query).
    The value of this field may differ from the name_length value because
    name_length contains byte length of UTF8-encoded item name, but
    the query string (see sp_instr_stmt::m_query) is currently stored with
    a charset from the SET NAMES statement.
  */
  uint len_in_query;

  Item_splocal(const Name_string sp_var_name, uint sp_var_idx,
               enum_field_types sp_var_type,
               uint pos_in_q= 0, uint len_in_q= 0);

  bool is_splocal() { return 1; } /* Needed for error checking */

  Item *this_item();
  const Item *this_item() const;
  Item **this_item_addr(THD *thd, Item **);

  virtual void print(String *str, enum_query_type query_type);

public:
  inline uint get_var_idx() const;

  inline enum Type type() const;
  inline Item_result result_type() const;
  inline enum_field_types field_type() const { return m_field_type; }

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
  Item_case_expr(uint case_expr_id);

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
  virtual void print(String *str, enum_query_type query_type);

private:
  uint m_case_expr_id;
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
  bool valid_args;
public:
  Item_name_const(Item *name_arg, Item *val);

  bool fix_fields(THD *, Item **);

  enum Type type() const;
  double val_real();
  longlong val_int();
  String *val_str(String *sp);
  my_decimal *val_decimal(my_decimal *);
  bool get_date(MYSQL_TIME *ltime, uint fuzzydate);
  bool get_time(MYSQL_TIME *ltime);
  bool is_null();
  virtual void print(String *str, enum_query_type query_type);

  Item_result result_type() const
  {
    return value_item->result_type();
  }

  type_conversion_status save_in_field(Field *field, bool no_conversions)
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
bool agg_item_set_converter(DTCollation &coll, const char *fname,
                            Item **args, uint nargs, uint flags, int item_sep);
bool agg_item_charsets(DTCollation &c, const char *name,
                       Item **items, uint nitems, uint flags, int item_sep);
inline bool
agg_item_charsets_for_string_result(DTCollation &c, const char *name,
                                    Item **items, uint nitems,
                                    int item_sep= 1)
{
  uint flags= MY_COLL_ALLOW_SUPERSET_CONV |
              MY_COLL_ALLOW_COERCIBLE_CONV |
              MY_COLL_ALLOW_NUMERIC_CONV;
  return agg_item_charsets(c, name, items, nitems, flags, item_sep);
}
inline bool
agg_item_charsets_for_comparison(DTCollation &c, const char *name,
                                 Item **items, uint nitems,
                                 int item_sep= 1)
{
  uint flags= MY_COLL_ALLOW_SUPERSET_CONV |
              MY_COLL_ALLOW_COERCIBLE_CONV |
              MY_COLL_DISALLOW_NONE;
  return agg_item_charsets(c, name, items, nitems, flags, item_sep);
}
inline bool
agg_item_charsets_for_string_result_with_comparison(DTCollation &c,
                                                    const char *name,
                                                    Item **items, uint nitems,
                                                    int item_sep= 1)
{
  uint flags= MY_COLL_ALLOW_SUPERSET_CONV |
              MY_COLL_ALLOW_COERCIBLE_CONV |
              MY_COLL_ALLOW_NUMERIC_CONV |
              MY_COLL_DISALLOW_NONE;
  return agg_item_charsets(c, name, items, nitems, flags, item_sep);
}


class Item_num: public Item_basic_constant
{
public:
  Item_num() { collation.set_numeric(); } /* Remove gcc warning */
  virtual Item_num *neg()= 0;
  Item *safe_charset_converter(const CHARSET_INFO *tocs);
  bool check_partition_func_processor(uchar *int_arg) { return FALSE;}
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
  virtual void fix_after_pullout(st_select_lex *parent_select,
                                 st_select_lex *removed_select);
  void cleanup();
  bool remove_dependence_processor(uchar * arg);
  virtual void print(String *str, enum_query_type query_type);
  virtual bool change_context_processor(uchar *cntx)
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
  bool get_date(MYSQL_TIME *ltime, uint fuzzydate)
  {
    return field->get_date(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return field->get_time(ltime);
  }
  void make_field(Send_field *tmp_field);
  CHARSET_INFO *charset_for_protocol(void) const
  { return (CHARSET_INFO *)field->charset_for_protocol(); }
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
    Constructor needed to process subquery with temporary tables (see Item).
    Notice that it will have no name resolution context.
  */
  Item_field(THD *thd, Item_field *item);
  /*
    Ensures that field, table, and database names will live as long as
    Item_field (this is important in prepared statements).
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
  longlong val_time_temporal();
  longlong val_date_temporal();
  my_decimal *val_decimal(my_decimal *);
  String *val_str(String*);
  double val_result();
  longlong val_int_result();
  longlong val_time_temporal_result();
  longlong val_date_temporal_result();
  String *str_result(String* tmp);
  my_decimal *val_decimal_result(my_decimal *);
  bool val_bool_result();
  bool is_null_result();
  bool send(Protocol *protocol, String *str_arg);
  void reset_field(Field *f);
  bool fix_fields(THD *, Item **);
  void make_field(Send_field *tmp_field);
  type_conversion_status save_in_field(Field *field,bool no_conversions);
  void save_org_in_field(Field *field);
  table_map used_tables() const;
  virtual table_map resolved_used_tables() const;
  enum Item_result result_type () const
  {
    return field->result_type();
  }
  enum Item_result numeric_context_result_type() const
  {
    return field->numeric_context_result_type();
  }
  Item_result cast_to_int_type() const
  {
    return field->cast_to_int_type();
  }
  enum_field_types field_type() const
  {
    return field->type();
  }
  enum_monotonicity_info get_monotonicity_info() const
  {
    return MONOTONIC_STRICT_INCREASING;
  }
  longlong val_int_endpoint(bool left_endp, bool *incl_endp);
  Field *get_tmp_table_field() { return result_field; }
  Field *tmp_table_field(TABLE *t_arg) { return result_field; }
  bool get_date(MYSQL_TIME *ltime,uint fuzzydate);
  bool get_date_result(MYSQL_TIME *ltime,uint fuzzydate);
  bool get_time(MYSQL_TIME *ltime);
  bool get_timeval(struct timeval *tm, int *warnings);
  bool is_null() { return field->is_null(); }
  void update_null_value();
  Item *get_tmp_table_item(THD *thd);
  bool collect_item_field_processor(uchar * arg);
  bool add_field_to_set_processor(uchar * arg);
  bool remove_column_from_bitmap(uchar * arg);
  bool find_item_in_field_list_processor(uchar *arg);
  bool register_field_in_read_map(uchar *arg);
  bool check_partition_func_processor(uchar *int_arg) {return FALSE;}
  void cleanup();
  Item_equal *find_item_equal(COND_EQUAL *cond_equal);
  bool subst_argument_checker(uchar **arg);
  Item *equal_fields_propagator(uchar *arg);
  bool set_no_const_sub(uchar *arg);
  Item *replace_equal_field(uchar *arg);
  inline uint32 max_disp_length() { return field->max_display_length(); }
  Item_field *field_for_view_update() { return this; }
  Item *safe_charset_converter(const CHARSET_INFO *tocs);
  int fix_outer_field(THD *thd, Field **field, Item **reference);
  virtual Item *update_value_transformer(uchar *select_arg);
  virtual bool item_field_by_name_analyzer(uchar **arg);
  virtual Item* item_field_by_name_transformer(uchar *arg);
  virtual void print(String *str, enum_query_type query_type);
  bool is_outer_field() const
  {
    DBUG_ASSERT(fixed);
    return field->table->pos_in_table_list->outer_join ||
           field->table->pos_in_table_list->outer_join_nest();
  }
  Field::geometry_type get_geometry_type() const
  {
    DBUG_ASSERT(field_type() == MYSQL_TYPE_GEOMETRY);
    return field->get_geometry_type();
  }
  const CHARSET_INFO *charset_for_protocol(void) const
  { return field->charset_for_protocol(); }

#ifndef DBUG_OFF
  void dbug_print()
  {
    fprintf(DBUG_FILE, "<field ");
    if (field)
    {
      fprintf(DBUG_FILE, "'%s.%s': ", field->table->alias, field->field_name);
      field->dbug_print();
    }
    else
      fprintf(DBUG_FILE, "NULL");

    fprintf(DBUG_FILE, ", result_field: ");
    if (result_field)
    {
      fprintf(DBUG_FILE, "'%s.%s': ",
              result_field->table->alias, result_field->field_name);
      result_field->dbug_print();
    }
    else
      fprintf(DBUG_FILE, "NULL");
    fprintf(DBUG_FILE, ">\n");
  }
#endif

  /// Pushes the item to select_lex.non_agg_fields() and updates its marker.
  bool push_to_non_agg_fields(st_select_lex *select_lex);

  friend class Item_default_value;
  friend class Item_insert_value;
  friend class st_select_lex_unit;
};

class Item_null :public Item_basic_constant
{
  void init()
  {
    maybe_null= null_value= TRUE;
    max_length= 0;
    fixed= 1;
    collation.set(&my_charset_bin, DERIVATION_IGNORABLE);
  }
public:
  Item_null()
  {
    init();
    item_name= NAME_STRING("NULL");
  }
  Item_null(const Name_string &name_par)
  {
    init();
    item_name= name_par;
  }
  enum Type type() const { return NULL_ITEM; }
  bool eq(const Item *item, bool binary_cmp) const;
  double val_real();
  longlong val_int();
  longlong val_time_temporal() { return val_int(); }
  longlong val_date_temporal() { return val_int(); }
  String *val_str(String *str);
  my_decimal *val_decimal(my_decimal *);
  bool get_date(MYSQL_TIME *ltime, uint fuzzydate)
  {
    return true;
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return true;
  }
  type_conversion_status save_in_field(Field *field, bool no_conversions);
  type_conversion_status save_safe_in_field(Field *field);
  bool send(Protocol *protocol, String *str);
  enum Item_result result_type () const { return STRING_RESULT; }
  enum_field_types field_type() const   { return MYSQL_TYPE_NULL; }
  bool basic_const_item() const { return 1; }
  Item *clone_item() { return new Item_null(item_name); }
  bool is_null() { return 1; }

  virtual inline void print(String *str, enum_query_type query_type)
  {
    str->append(STRING_WITH_LEN("NULL"));
  }

  Item *safe_charset_converter(const CHARSET_INFO *tocs);
  bool check_partition_func_processor(uchar *int_arg) {return FALSE;}
};

/**
  An item representing NULL values for use with ROLLUP.

  When grouping WITH ROLLUP, Item_null_result items are created to
  represent NULL values in the grouping columns of the ROLLUP rows. To
  avoid type problems during execution, these objects are created with
  the same field and result types as the fields of the columns they
  belong to.
 */
class Item_null_result :public Item_null
{
  /** Field type for this NULL value */
  enum_field_types fld_type;
  /** Result type for this NULL value */
  Item_result res_type;

public:
  Field *result_field;
  Item_null_result(enum_field_types fld_type, Item_result res_type)
    : Item_null(), fld_type(fld_type), res_type(res_type), result_field(0) {}
  bool is_result_field() { return result_field != 0; }
  void save_in_result_field(bool no_conversions)
  {
    save_in_field(result_field, no_conversions);
  }
  bool check_partition_func_processor(uchar *int_arg) {return TRUE;}
  enum_field_types field_type() const { return fld_type; }
  Item_result result_type() const { return res_type; }
};  

/* Item represents one placeholder ('?') of prepared statement */

class Item_param :public Item,
                  private Settable_routine_parameter
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
      const CHARSET_INFO *character_set_client;
      const CHARSET_INFO *character_set_of_placeholder;
      /*
        This points at character set of connection if conversion
        to it is required (i. e. if placeholder typecode is not BLOB).
        Otherwise it's equal to character_set_client (to simplify
        check in convert_str_value()).
      */
      const CHARSET_INFO *final_character_set_of_str_value;
    } cs_info;
    MYSQL_TIME     time;
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
  bool get_time(MYSQL_TIME *tm);
  bool get_date(MYSQL_TIME *tm, uint fuzzydate);
  type_conversion_status save_in_field(Field *field, bool no_conversions);

  void set_null();
  void set_int(longlong i, uint32 max_length_arg);
  void set_double(double i);
  void set_decimal(const char *str, ulong length);
  void set_decimal(const my_decimal *dv);
  bool set_str(const char *str, ulong length);
  bool set_longdata(const char *str, ulong length);
  void set_time(MYSQL_TIME *tm, timestamp_type type, uint32 max_length_arg);
  bool set_from_user_var(THD *thd, const user_var_entry *entry);
  void reset();
  /*
    Assign placeholder value from bind data.
    Note, that 'len' has different semantics in embedded library (as we
    don't need to check that packet is not broken there). See
    sql_prepare.cc for details.
  */
  void (*set_param_func)(Item_param *param, uchar **pos, ulong len);

  const String *query_val_str(THD *thd, String *str) const;

  bool convert_str_value(THD *thd);

  /*
    If value for parameter was not set we treat it as non-const
    so noone will use parameters value in fix_fields still
    parameter is constant during execution.
  */
  virtual table_map used_tables() const
  { return state != NO_VALUE ? (table_map)0 : PARAM_TABLE_BIT; }
  virtual void print(String *str, enum_query_type query_type);
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
  Item *safe_charset_converter(const CHARSET_INFO *tocs);
  Item *clone_item();
  /*
    Implement by-value equality evaluation if parameter value
    is set and is a basic constant (integer, real or string).
    Otherwise return FALSE.
  */
  bool eq(const Item *item, bool binary_cmp) const;
  /** Item is a argument to a limit clause. */
  bool limit_clause_param;
  void set_param_type_and_swap_value(Item_param *from);

private:
  virtual inline Settable_routine_parameter *
    get_settable_routine_parameter()
  {
    return this;
  }

  virtual bool set_value(THD *thd, sp_rcontext *ctx, Item **it);

  virtual void set_out_param_info(Send_field *info);

public:
  virtual const Send_field *get_out_param_info() const;

  virtual void make_field(Send_field *field);

private:
  Send_field *m_out_param_info;
};


class Item_int :public Item_num
{
public:
  longlong value;
  Item_int(int32 i,uint length= MY_INT32_NUM_DECIMAL_DIGITS)
    :value((longlong) i)
    { max_length=length; fixed= 1; }
  Item_int(longlong i,uint length= MY_INT64_NUM_DECIMAL_DIGITS)
    :value(i)
    { max_length=length; fixed= 1; }
  Item_int(ulonglong i, uint length= MY_INT64_NUM_DECIMAL_DIGITS)
    :value((longlong)i)
    { max_length=length; fixed= 1; unsigned_flag= 1; }
  Item_int(Item_int *item_arg)
  {
    value= item_arg->value;
    item_name= item_arg->item_name;
    max_length= item_arg->max_length;
    fixed= 1;
  }
  Item_int(const Name_string &name_arg, longlong i, uint length) :value(i)
  {
    max_length= length;
    item_name= name_arg;
    fixed= 1;
  }
  Item_int(const char *str_arg, uint length);
  enum Type type() const { return INT_ITEM; }
  enum Item_result result_type () const { return INT_RESULT; }
  enum_field_types field_type() const { return MYSQL_TYPE_LONGLONG; }
  longlong val_int() { DBUG_ASSERT(fixed == 1); return value; }
  double val_real() { DBUG_ASSERT(fixed == 1); return (double) value; }
  my_decimal *val_decimal(my_decimal *);
  String *val_str(String*);
  bool get_date(MYSQL_TIME *ltime, uint fuzzydate)
  {
    return get_date_from_int(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return get_time_from_int(ltime);
  }
  type_conversion_status save_in_field(Field *field, bool no_conversions);
  bool basic_const_item() const { return 1; }
  Item *clone_item() { return new Item_int(this); }
  virtual void print(String *str, enum_query_type query_type);
  Item_num *neg() { value= -value; return this; }
  uint decimal_precision() const
  { return (uint)(max_length - MY_TEST(value < 0)); }
  bool eq(const Item *, bool binary_cmp) const;
  bool check_partition_func_processor(uchar *bool_arg) { return FALSE;}
};


/**
  Item_int with value==0 and length==1
*/
class Item_int_0 :public Item_int
{
public:
  Item_int_0() :Item_int(NAME_STRING("0"), 0, 1) {}
};


/*
  Item_temporal is used to store numeric representation
  of time/date/datetime values for queries like:

     WHERE datetime_column NOT IN
     ('2006-04-25 10:00:00','2006-04-25 10:02:00', ...);

  and for SHOW/INFORMATION_SCHEMA purposes (see sql_show.cc)

  TS-TODO: Can't we use Item_time_literal, Item_date_literal,
  TS-TODO: and Item_datetime_literal for this purpose?
*/
class Item_temporal :public Item_int
{
  enum_field_types cached_field_type;
public:
  Item_temporal(enum_field_types field_type_arg, longlong i): Item_int(i),
    cached_field_type(field_type_arg)
  {
    DBUG_ASSERT(is_temporal_type(field_type_arg));
  }
  Item_temporal(enum_field_types field_type_arg, const Name_string &name_arg,
                longlong i, uint length): Item_int(i),
    cached_field_type(field_type_arg)
  {
    DBUG_ASSERT(is_temporal_type(field_type_arg));
    max_length= length;
    item_name= name_arg;
    fixed= 1;
  }
  Item *clone_item() { return new Item_temporal(field_type(), value); }
  type_conversion_status save_in_field(Field *field, bool no_conversions);
  longlong val_time_temporal() { return val_int(); }
  longlong val_date_temporal() { return val_int(); }
  bool get_date(MYSQL_TIME *ltime, uint fuzzydate)
  {
    DBUG_ASSERT(0);
    return false;
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    DBUG_ASSERT(0);
    return false;
  }
  enum_field_types field_type() const
  {
    return cached_field_type;
  }
};


class Item_uint :public Item_int
{
public:
  Item_uint(const char *str_arg, uint length)
    :Item_int(str_arg, length) { unsigned_flag= 1; }
  Item_uint(ulonglong i) :Item_int((ulonglong) i, 10) {}
  Item_uint(const Name_string &name_arg, longlong i, uint length)
    :Item_int(name_arg, i, length) { unsigned_flag= 1; }
  double val_real()
    { DBUG_ASSERT(fixed == 1); return ulonglong2double((ulonglong)value); }
  String *val_str(String*);

  Item *clone_item() { return new Item_uint(item_name, value, max_length); }
  type_conversion_status save_in_field(Field *field, bool no_conversions);
  virtual void print(String *str, enum_query_type query_type);
  Item_num *neg ();
  uint decimal_precision() const { return max_length; }
  bool check_partition_func_processor(uchar *bool_arg) { return FALSE;}
};


/* decimal (fixed point) constant */
class Item_decimal :public Item_num
{
protected:
  my_decimal decimal_value;
public:
  Item_decimal(const char *str_arg, uint length, const CHARSET_INFO *charset);
  Item_decimal(const Name_string &name_arg,
               const my_decimal *val_arg, uint decimal_par, uint length);
  Item_decimal(my_decimal *value_par);
  Item_decimal(longlong val, bool unsig);
  Item_decimal(double val, int precision, int scale);
  Item_decimal(const uchar *bin, int precision, int scale);

  enum Type type() const { return DECIMAL_ITEM; }
  enum Item_result result_type () const { return DECIMAL_RESULT; }
  enum_field_types field_type() const { return MYSQL_TYPE_NEWDECIMAL; }
  longlong val_int();
  double val_real();
  String *val_str(String*);
  my_decimal *val_decimal(my_decimal *val) { return &decimal_value; }
  bool get_date(MYSQL_TIME *ltime, uint fuzzydate)
  {
    return get_date_from_decimal(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return get_time_from_decimal(ltime);
  }
  type_conversion_status save_in_field(Field *field, bool no_conversions);
  bool basic_const_item() const { return 1; }
  Item *clone_item()
  {
    return new Item_decimal(item_name, &decimal_value, decimals, max_length);
  }
  virtual void print(String *str, enum_query_type query_type);
  Item_num *neg()
  {
    my_decimal_neg(&decimal_value);
    unsigned_flag= !decimal_value.sign();
    return this;
  }
  uint decimal_precision() const { return decimal_value.precision(); }
  bool eq(const Item *, bool binary_cmp) const;
  void set_decimal_value(my_decimal *value_par);
  bool check_partition_func_processor(uchar *bool_arg) { return FALSE;}
};


class Item_float :public Item_num
{
  Name_string presentation;
public:
  double value;
  // Item_real() :value(0) {}
  Item_float(const char *str_arg, uint length);
  Item_float(const Name_string name_arg,
             double val_arg, uint decimal_par, uint length)
    :value(val_arg)
  {
    presentation= name_arg;
    item_name= name_arg;
    decimals= (uint8) decimal_par;
    max_length= length;
    fixed= 1;
  }
  Item_float(double value_par, uint decimal_par) :value(value_par)
  {
    decimals= (uint8) decimal_par;
    fixed= 1;
  }
  type_conversion_status save_in_field(Field *field, bool no_conversions);
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
  bool get_date(MYSQL_TIME *ltime, uint fuzzydate)
  {
    return get_date_from_real(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return get_time_from_real(ltime);
  }
  bool basic_const_item() const { return 1; }
  Item *clone_item()
  { return new Item_float(item_name, value, decimals, max_length); }
  Item_num *neg() { value= -value; return this; }
  virtual void print(String *str, enum_query_type query_type);
  bool eq(const Item *, bool binary_cmp) const;
};


class Item_static_float_func :public Item_float
{
  const Name_string func_name;
public:
  Item_static_float_func(const Name_string &name_arg,
                         double val_arg, uint decimal_par, uint length)
    :Item_float(null_name_string,
                val_arg, decimal_par, length), func_name(name_arg)
  {}

  virtual inline void print(String *str, enum_query_type query_type)
  {
    str->append(func_name);
  }

  Item *safe_charset_converter(const CHARSET_INFO *tocs);
};


class Item_string :public Item_basic_constant
{
public:
  /* Create from a string, set name from the string itself. */
  Item_string(const char *str,uint length,
              const CHARSET_INFO *cs, Derivation dv= DERIVATION_COERCIBLE,
              uint repertoire= MY_REPERTOIRE_UNICODE30)
    : m_cs_specified(FALSE)
  {
    str_value.set_or_copy_aligned(str, length, cs);
    collation.set(cs, dv, repertoire);
    /*
      We have to have a different max_length than 'length' here to
      ensure that we get the right length if we do use the item
      to create a new table. In this case max_length must be the maximum
      number of chars for a string of this type because we in Create_field::
      divide the max_length with mbmaxlen).
    */
    max_length= str_value.numchars()*cs->mbmaxlen;
    item_name.copy(str, length, cs);
    decimals=NOT_FIXED_DEC;
    // it is constant => can be used without fix_fields (and frequently used)
    fixed= 1;
  }
  /* Just create an item and do not fill string representation */
  Item_string(const CHARSET_INFO *cs, Derivation dv= DERIVATION_COERCIBLE)
    : m_cs_specified(FALSE)
  {
    collation.set(cs, dv);
    max_length= 0;
    decimals= NOT_FIXED_DEC;
    fixed= 1;
  }
  /* Create from the given name and string. */
  Item_string(const Name_string name_par, const char *str, uint length,
              const CHARSET_INFO *cs, Derivation dv= DERIVATION_COERCIBLE,
              uint repertoire= MY_REPERTOIRE_UNICODE30)
    : m_cs_specified(FALSE)
  {
    str_value.set_or_copy_aligned(str, length, cs);
    collation.set(cs, dv, repertoire);
    max_length= str_value.numchars()*cs->mbmaxlen;
    item_name= name_par;
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
  void set_repertoire_from_value()
  {
    collation.repertoire= my_string_repertoire(str_value.charset(),
                                               str_value.ptr(),
                                               str_value.length());
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
  bool get_date(MYSQL_TIME *ltime, uint fuzzydate)
  {
    return get_date_from_string(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return get_time_from_string(ltime);
  }
  type_conversion_status save_in_field(Field *field, bool no_conversions);
  enum Item_result result_type () const { return STRING_RESULT; }
  enum_field_types field_type() const { return MYSQL_TYPE_VARCHAR; }
  bool basic_const_item() const { return 1; }
  bool eq(const Item *item, bool binary_cmp) const;
  Item *clone_item() 
  {
    return new Item_string(static_cast<Name_string>(item_name), str_value.ptr(), 
    			   str_value.length(), collation.collation);
  }
  Item *safe_charset_converter(const CHARSET_INFO *tocs);
  Item *charset_converter(const CHARSET_INFO *tocs, bool lossless);
  inline void append(char *str, uint length)
  {
    str_value.append(str, length);
    max_length= str_value.numchars() * collation.collation->mbmaxlen;
  }
  virtual void print(String *str, enum_query_type query_type);
  bool check_partition_func_processor(uchar *int_arg) {return FALSE;}

  /**
    Return TRUE if character-set-introducer was explicitly specified in the
    original query for this item (text literal).

    This operation is to be called from Item_string::print(). The idea is
    that when a query is generated (re-constructed) from the Item-tree,
    character-set-introducers should appear only for those literals, where
    they were explicitly specified by the user. Otherwise, that may lead to
    loss collation information (character set introducers implies default
    collation for the literal).

    Basically, that makes sense only for views and hopefully will be gone
    one day when we start using original query as a view definition.

    @return This operation returns the value of m_cs_specified attribute.
      @retval TRUE if character set introducer was explicitly specified in
      the original query.
      @retval FALSE otherwise.
  */
  inline bool is_cs_specified() const
  {
    return m_cs_specified;
  }

  /**
    Set the value of m_cs_specified attribute.

    m_cs_specified attribute shows whether character-set-introducer was
    explicitly specified in the original query for this text literal or
    not. The attribute makes sense (is used) only for views.

    This operation is to be called from the parser during parsing an input
    query.
  */
  inline void set_cs_specified(bool cs_specified)
  {
    m_cs_specified= cs_specified;
  }

private:
  bool m_cs_specified;
};


longlong 
longlong_from_string_with_check (const CHARSET_INFO *cs,
                                 const char *cptr, char *end);
double 
double_from_string_with_check (const CHARSET_INFO *cs,
                               const char *cptr, char *end);

class Item_static_string_func :public Item_string
{
  const Name_string func_name;
public:
  Item_static_string_func(const Name_string &name_par,
                          const char *str, uint length, const CHARSET_INFO *cs,
                          Derivation dv= DERIVATION_COERCIBLE)
    :Item_string(null_name_string, str, length, cs, dv), func_name(name_par)
  {}
  Item *safe_charset_converter(const CHARSET_INFO *tocs);

  virtual inline void print(String *str, enum_query_type query_type)
  {
    str->append(func_name);
  }

  bool check_partition_func_processor(uchar *int_arg) {return TRUE;}
};


/* for show tables */
class Item_partition_func_safe_string: public Item_string
{
public:
  Item_partition_func_safe_string(const Name_string name, uint length,
                                  const CHARSET_INFO *cs= NULL):
    Item_string(name, NullS, 0, cs)
  {
    max_length= length;
  }
};


class Item_blob :public Item_partition_func_safe_string
{
public:
  Item_blob(const char *name, uint length) :
    Item_partition_func_safe_string(Name_string(name, strlen(name)),
                                    length, &my_charset_bin)
  { }
  enum Type type() const { return TYPE_HOLDER; }
  enum_field_types field_type() const { return MYSQL_TYPE_BLOB; }
};


/**
  Item_empty_string -- is a utility class to put an item into List<Item>
  which is then used in protocol.send_result_set_metadata() when sending SHOW output to
  the client.
*/

class Item_empty_string :public Item_partition_func_safe_string
{
public:
  Item_empty_string(const char *header, uint length,
                    const CHARSET_INFO *cs= NULL) :
    Item_partition_func_safe_string(Name_string(header, strlen(header)),
                                    0, cs ? cs : &my_charset_utf8_general_ci)
    {
      max_length= length * collation.collation->mbmaxlen;
    }
  void make_field(Send_field *field);
};


class Item_return_int :public Item_int
{
  enum_field_types int_field_type;
public:
  Item_return_int(const char *name_arg, uint length,
		  enum_field_types field_type_arg, longlong value= 0)
    :Item_int(Name_string(name_arg, name_arg ? strlen(name_arg) : 0),
              value, length), int_field_type(field_type_arg)
  {
    unsigned_flag=1;
  }
  enum_field_types field_type() const { return int_field_type; }
};


class Item_hex_string: public Item_basic_constant
{
public:
  Item_hex_string();
  Item_hex_string(const char *str,uint str_length);
  enum Type type() const { return VARBIN_ITEM; }
  double val_real()
  { 
    DBUG_ASSERT(fixed == 1); 
    return (double) (ulonglong) Item_hex_string::val_int();
  }
  longlong val_int();
  bool basic_const_item() const { return 1; }
  String *val_str(String*) { DBUG_ASSERT(fixed == 1); return &str_value; }
  my_decimal *val_decimal(my_decimal *);
  bool get_date(MYSQL_TIME *ltime, uint fuzzydate)
  {
    return get_date_from_string(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return get_time_from_string(ltime);
  }
  type_conversion_status save_in_field(Field *field, bool no_conversions);
  enum Item_result result_type () const { return STRING_RESULT; }
  enum Item_result cast_to_int_type() const { return INT_RESULT; }
  enum_field_types field_type() const { return MYSQL_TYPE_VARCHAR; }
  virtual void print(String *str, enum_query_type query_type);
  bool eq(const Item *item, bool binary_cmp) const;
  virtual Item *safe_charset_converter(const CHARSET_INFO *tocs);
  bool check_partition_func_processor(uchar *int_arg) {return FALSE;}
private:
  void hex_string_init(const char *str, uint str_length);
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
  /*
    This method is used for debug purposes to print the name of an
    item to the debug log. The second use of this method is as
    a helper function of print() and error messages, where it is
    applicable. To suit both goals it should return a meaningful,
    distinguishable and sintactically correct string. This method
    should not be used for runtime type identification, use enum
    {Sum}Functype and Item_func::functype()/Item_sum::sum_func()
    instead.
    Added here, to the parent class of both Item_func and Item_sum_func.

    NOTE: for Items inherited from Item_sum, func_name() return part of
    function name till first argument (including '(') to make difference in
    names for functions with 'distinct' clause and without 'distinct' and
    also to make printing of items inherited from Item_sum uniform.
  */
  virtual const char *func_name() const= 0;
};


class Item_ref :public Item_ident
{
protected:
  void set_properties();
public:
  enum Ref_Type { REF, DIRECT_REF, VIEW_REF, OUTER_REF, AGGREGATE_REF };
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
           const char *table_name_arg, const char *field_name_arg,
           bool alias_name_used_arg= FALSE);

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
  longlong val_time_temporal();
  longlong val_date_temporal();
  my_decimal *val_decimal(my_decimal *);
  bool val_bool();
  String *val_str(String* tmp);
  bool is_null();
  bool get_date(MYSQL_TIME *ltime,uint fuzzydate);
  double val_result();
  longlong val_int_result();
  String *str_result(String* tmp);
  my_decimal *val_decimal_result(my_decimal *);
  bool val_bool_result();
  bool is_null_result();
  bool send(Protocol *prot, String *tmp);
  void make_field(Send_field *field);
  bool fix_fields(THD *, Item **);
  void fix_after_pullout(st_select_lex *parent_select,
                         st_select_lex *removed_select);
  type_conversion_status save_in_field(Field *field, bool no_conversions);
  void save_org_in_field(Field *field);
  enum Item_result result_type () const { return (*ref)->result_type(); }
  enum_field_types field_type() const   { return (*ref)->field_type(); }
  Field *get_tmp_table_field()
  { return result_field ? result_field : (*ref)->get_tmp_table_field(); }
  Item *get_tmp_table_item(THD *thd);
  bool const_item() const
  {
    return (*ref)->const_item() && (used_tables() == 0);
  }
  table_map used_tables() const		
  {
    return depended_from ? OUTER_REF_TABLE_BIT : (*ref)->used_tables(); 
  }
  void update_used_tables() 
  { 
    if (!depended_from) 
      (*ref)->update_used_tables(); 
  }

  virtual table_map resolved_used_tables() const
  { return (*ref)->resolved_used_tables(); }

  table_map not_null_tables() const
  {
    /*
      It can happen that our 'depended_from' member is set but the
      'depended_from' member of the referenced item is not (example: if a
      field in a subquery belongs to an outer merged view), so we first test
      ours:
    */
    return depended_from ? OUTER_REF_TABLE_BIT : (*ref)->not_null_tables();
  }
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
  bool walk(Item_processor processor, bool walk_subquery, uchar *arg)
  {
    return (*ref)->walk(processor, walk_subquery, arg) ||
           (this->*processor)(arg);
  }
  virtual Item* transform(Item_transformer, uchar *arg);
  virtual Item* compile(Item_analyzer analyzer, uchar **arg_p,
                        Item_transformer transformer, uchar *arg_t);
  virtual bool explain_subquery_checker(uchar **arg)
  {
    /*
      Always return false: we don't need to go deeper into referenced
      expression tree since we have to mark aliased subqueries at
      their original places (select list, derived tables), not by
      references from other expression (order by etc).
    */
    return false;
  }
  virtual void print(String *str, enum_query_type query_type);
  void cleanup();
  Item_field *field_for_view_update()
    { return (*ref)->field_for_view_update(); }
  virtual Ref_Type ref_type() { return REF; }

  // Row emulation: forwarding of ROW-related calls to ref
  uint cols()
  {
    return ref && result_type() == ROW_RESULT ? (*ref)->cols() : 1;
  }
  Item* element_index(uint i)
  {
    return ref && result_type() == ROW_RESULT ? (*ref)->element_index(i) : this;
  }
  Item** addr(uint i)
  {
    return ref && result_type() == ROW_RESULT ? (*ref)->addr(i) : 0;
  }
  bool check_cols(uint c)
  {
    return ref && result_type() == ROW_RESULT ? (*ref)->check_cols(c) 
                                              : Item::check_cols(c);
  }
  bool null_inside()
  {
    return ref && result_type() == ROW_RESULT ? (*ref)->null_inside() : 0;
  }
  void bring_value()
  { 
    if (ref && result_type() == ROW_RESULT)
      (*ref)->bring_value();
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    DBUG_ASSERT(fixed);
    return (*ref)->get_time(ltime);
  }
  virtual bool basic_const_item() const { return ref && (*ref)->basic_const_item(); }
  bool is_outer_field() const
  {
    DBUG_ASSERT(fixed);
    DBUG_ASSERT(ref);
    return (*ref)->is_outer_field();
  }

  /**
    Checks if the item tree that ref points to contains a subquery.
  */
  virtual bool has_subquery() const 
  { 
    DBUG_ASSERT(ref);
    return (*ref)->has_subquery();
  }

  /**
    Checks if the item tree that ref points to contains a stored program.
  */
  virtual bool has_stored_program() const 
  { 
    DBUG_ASSERT(ref);
    return (*ref)->has_stored_program();
  }

  virtual bool created_by_in2exists() const
  {
    return (*ref)->created_by_in2exists();
  }
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
                  const char *field_name_arg,
                  bool alias_name_used_arg= FALSE)
    :Item_ref(context_arg, item, table_name_arg,
              field_name_arg, alias_name_used_arg)
  {}
  /* Constructor need to process subselect with temporary tables (see Item) */
  Item_direct_ref(THD *thd, Item_direct_ref *item) : Item_ref(thd, item) {}

  double val_real();
  longlong val_int();
  longlong val_time_temporal();
  longlong val_date_temporal();
  String *val_str(String* tmp);
  my_decimal *val_decimal(my_decimal *);
  bool val_bool();
  bool is_null();
  bool get_date(MYSQL_TIME *ltime,uint fuzzydate);
  virtual Ref_Type ref_type() { return DIRECT_REF; }
};

/*
  Class for view fields, the same as Item_direct_ref, but call fix_fields
  of reference if it is not called yet
*/
class Item_direct_view_ref :public Item_direct_ref
{
public:
  Item_direct_view_ref(Name_resolution_context *context_arg,
                       Item **item,
                       const char *alias_name_arg,
                       const char *table_name_arg,
                       const char *field_name_arg)
    : Item_direct_ref(context_arg, item, alias_name_arg, field_name_arg)
  {
    orig_table_name= table_name_arg;
  }

  /* Constructor need to process subselect with temporary tables (see Item) */
  Item_direct_view_ref(THD *thd, Item_direct_ref *item)
    :Item_direct_ref(thd, item) {}

  /*
    We share one underlying Item_field, so we have to disable
    build_equal_items_for_cond().
    TODO: Implement multiple equality optimization for views.
  */
  virtual bool subst_argument_checker(uchar **arg)
  {
    return false;
  }

  bool fix_fields(THD *, Item **);
  bool eq(const Item *item, bool binary_cmp) const;
  Item *get_tmp_table_item(THD *thd)
  {
    Item *item= Item_ref::get_tmp_table_item(thd);
    item->item_name= item_name;
    return item;
  }
  virtual Ref_Type ref_type() { return VIEW_REF; }
};


/*
  Class for outer fields.
  An object of this class is created when the select where the outer field was
  resolved is a grouping one. After it has been fixed the ref field will point
  to either an Item_ref or an Item_direct_ref object which will be used to
  access the field.
  The ref field may also point to an Item_field instance.
  See also comments for the fix_inner_refs() and the
  Item_field::fix_outer_field() functions.
*/

class Item_sum;
class Item_outer_ref :public Item_direct_ref
{
public:
  Item *outer_ref;
  /* The aggregate function under which this outer ref is used, if any. */
  Item_sum *in_sum_func;
  /*
    TRUE <=> that the outer_ref is already present in the select list
    of the outer select.
  */
  bool found_in_select_list;
  Item_outer_ref(Name_resolution_context *context_arg,
                 Item_field *outer_field_arg)
    :Item_direct_ref(context_arg, 0, outer_field_arg->table_name,
                     outer_field_arg->field_name),
    outer_ref(outer_field_arg), in_sum_func(0),
    found_in_select_list(0)
  {
    ref= &outer_ref;
    set_properties();
    fixed= 0;
  }
  Item_outer_ref(Name_resolution_context *context_arg, Item **item,
                 const char *table_name_arg, const char *field_name_arg,
                 bool alias_name_used_arg)
    :Item_direct_ref(context_arg, item, table_name_arg, field_name_arg,
                     alias_name_used_arg),
    outer_ref(0), in_sum_func(0), found_in_select_list(1)
  {}
  void save_in_result_field(bool no_conversions)
  {
    outer_ref->save_org_in_field(result_field);
  }
  bool fix_fields(THD *, Item **);
  void fix_after_pullout(st_select_lex *parent_select,
                         st_select_lex *removed_select);
  table_map used_tables() const
  {
    return (*ref)->const_item() ? 0 : OUTER_REF_TABLE_BIT;
  }
  table_map not_null_tables() const { return 0; }

  virtual Ref_Type ref_type() { return OUTER_REF; }
};


class Item_in_subselect;


/*
  An object of this class:
   - Converts val_XXX() calls to ref->val_XXX_result() calls, like Item_ref.
   - Sets owner->was_null=TRUE if it has returned a NULL value from any
     val_XXX() function. This allows to inject an Item_ref_null_helper
     object into subquery and then check if the subquery has produced a row
     with NULL value.
*/

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
  longlong val_time_temporal();
  longlong val_date_temporal();
  String* val_str(String* s);
  my_decimal *val_decimal(my_decimal *);
  bool val_bool();
  bool get_date(MYSQL_TIME *ltime, uint fuzzydate);
  virtual void print(String *str, enum_query_type query_type);
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
  The following class is used to optimize comparing of bigint columns.
  We need to save the original item ('ref') to be able to call
  ref->save_in_field(). This is used to create index search keys.
  
  An instance of Item_int_with_ref may have signed or unsigned integer value.
  
*/

class Item_int_with_ref :public Item_int
{
protected:
  Item *ref;
public:
  Item_int_with_ref(longlong i, Item *ref_arg, my_bool unsigned_arg) :
    Item_int(i), ref(ref_arg)
  {
    unsigned_flag= unsigned_arg;
  }
  type_conversion_status save_in_field(Field *field, bool no_conversions)
  {
    return ref->save_in_field(field, no_conversions);
  }
  Item *clone_item();
  virtual Item *real_item() { return ref; }
};


/*
  Similar to Item_int_with_ref, but to optimize comparing of temporal columns.
*/
class Item_temporal_with_ref :public Item_int_with_ref
{
private:
  enum_field_types cached_field_type;
public:
  Item_temporal_with_ref(enum_field_types field_type_arg,
                         uint8 decimals_arg, longlong i, Item *ref_arg,
                         bool unsigned_flag):
    Item_int_with_ref(i, ref_arg, unsigned_flag),
    cached_field_type(field_type_arg)
  {
    decimals= decimals_arg;
  }
  enum_field_types field_type() const { return cached_field_type; }
  void print(String *str, enum_query_type query_type);
  bool get_date(MYSQL_TIME *ltime, uint fuzzydate)
  {
    DBUG_ASSERT(0);
    return true;
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    DBUG_ASSERT(0);
    return true;
  }

};


/*
  Item_datetime_with_ref is used to optimize queries like:
    SELECT ... FROM t1 WHERE date_or_datetime_column = 20110101101010;
  The numeric constant is replaced to Item_datetime_with_ref
  by convert_constant_item().
*/
class Item_datetime_with_ref :public Item_temporal_with_ref
{
private:
  enum_field_types cached_field_type;
public:
  /**
    Constructor for Item_datetime_with_ref.
    @param    field_type_arg Data type: MYSQL_TYPE_DATE or MYSQL_TYPE_DATETIME
    @param    decimals_arg   Number of fractional digits.
    @param    i              Temporal value in packed format.
    @param    ref_arg        Pointer to the original numeric Item.
  */
  Item_datetime_with_ref(enum_field_types field_type_arg,
                         uint8 decimals_arg, longlong i, Item *ref_arg):
    Item_temporal_with_ref(field_type_arg, decimals_arg, i, ref_arg, true),
    cached_field_type(field_type_arg)
  {
  }
  Item *clone_item();
  longlong val_date_temporal() { return val_int(); }
  longlong val_time_temporal()
  {
    DBUG_ASSERT(0);
    return val_int();
  }
};


/*
  Item_time_with_ref is used to optimize queries like:
    SELECT ... FROM t1 WHERE time_column = 20110101101010;
  The numeric constant is replaced to Item_time_with_ref
  by convert_constant_item().
*/
class Item_time_with_ref :public Item_temporal_with_ref
{
public:
  /**
    Constructor for Item_time_with_ref.
    @param    decimals_arg   Number of fractional digits.
    @param    i              Temporal value in packed format.
    @param    ref_arg        Pointer to the original numeric Item.
  */
  Item_time_with_ref(uint8 decimals_arg, longlong i, Item *ref_arg):
    Item_temporal_with_ref(MYSQL_TYPE_TIME, decimals_arg, i, ref_arg, 0)
  {
  }
  Item *clone_item();
  longlong val_time_temporal() { return val_int(); }
  longlong val_date_temporal()
  {
    DBUG_ASSERT(0);
    return val_int();
  }
};


#ifdef MYSQL_SERVER
#include "gstream.h"
#include "spatial.h"
#include "item_sum.h"
#include "item_func.h"
#include "item_row.h"
#include "item_cmpfunc.h"
#include "item_strfunc.h"
#include "item_geofunc.h"
#include "item_timefunc.h"
#include "item_subselect.h"
#include "item_xmlfunc.h"
#include "item_create.h"
#endif

/**
  Base class to implement typed value caching Item classes

  Item_copy_ classes are very similar to the corresponding Item_
  classes (e.g. Item_copy_int is similar to Item_int) but they add
  the following additional functionality to Item_ :
    1. Nullability
    2. Possibility to store the value not only on instantiation time,
       but also later.
  Item_copy_ classes are a functionality subset of Item_cache_ 
  classes, as e.g. they don't support comparisons with the original Item
  as Item_cache_ classes do.
  Item_copy_ classes are used in GROUP BY calculation.
  TODO: Item_copy should be made an abstract interface and Item_copy_
  classes should inherit both the respective Item_ class and the interface.
  Ideally we should drop Item_copy_ classes altogether and merge 
  their functionality to Item_cache_ (and these should be made to inherit
  from Item_).
*/

class Item_copy :public Item
{
protected:  

  /**
    Stores the type of the resulting field that would be used to store the data
    in the cache. This is to avoid calls to the original item.
  */
  enum enum_field_types cached_field_type;

  /** The original item that is copied */
  Item *item;

  /**
    Stores the result type of the original item, so it can be returned
    without calling the original item's method
  */
  Item_result cached_result_type;

  /**
    Constructor of the Item_copy class

    stores metadata information about the original class as well as a 
    pointer to it.
  */
  Item_copy(Item *i)
  {
    item= i;
    null_value=maybe_null=item->maybe_null;
    decimals=item->decimals;
    max_length=item->max_length;
    item_name= item->item_name;
    cached_field_type= item->field_type();
    cached_result_type= item->result_type();
    unsigned_flag= item->unsigned_flag;
    fixed= item->fixed;
    collation.set(item->collation);
  }

public:
  /** 
    Factory method to create the appropriate subclass dependent on the type of 
    the original item.

    @param item      the original item.
  */  
  static Item_copy *create (Item *item);

  /** 
    Update the cache with the value of the original item
   
    This is the method that updates the cached value.
    It must be explicitly called by the user of this class to store the value 
    of the orginal item in the cache.
  */  
  virtual void copy() = 0;

  Item *get_item() { return item; }
  /** All of the subclasses should have the same type tag */
  enum Type type() const { return COPY_STR_ITEM; }
  enum_field_types field_type() const { return cached_field_type; }
  enum Item_result result_type () const { return cached_result_type; }

  void make_field(Send_field *field) { item->make_field(field); }
  table_map used_tables() const { return (table_map) 1L; }
  bool const_item() const { return 0; }
  bool is_null() { return null_value; }

  virtual void no_rows_in_result()
  {
    item->no_rows_in_result();
  }

  /*  
    Override the methods below as pure virtual to make sure all the 
    sub-classes implement them.
  */  

  virtual String *val_str(String*) = 0;
  virtual my_decimal *val_decimal(my_decimal *) = 0;
  virtual double val_real() = 0;
  virtual longlong val_int() = 0;
  virtual bool get_date(MYSQL_TIME *ltime, uint fuzzydate)= 0;
  virtual bool get_time(MYSQL_TIME *ltime)= 0;
  virtual type_conversion_status save_in_field(Field *field,
                                               bool no_conversions) = 0;
};

/**
 Implementation of a string cache.
 
 Uses Item::str_value for storage
*/ 
class Item_copy_string : public Item_copy
{
public:
  Item_copy_string (Item *item) : Item_copy(item) {}

  String *val_str(String*);
  my_decimal *val_decimal(my_decimal *);
  double val_real();
  longlong val_int();
  bool get_date(MYSQL_TIME *ltime, uint fuzzydate);
  bool get_time(MYSQL_TIME *ltime);
  void copy();
  type_conversion_status save_in_field(Field *field, bool no_conversions);
};


class Item_copy_int : public Item_copy
{
protected:  
  longlong cached_value; 
public:
  Item_copy_int (Item *i) : Item_copy(i) {}
  type_conversion_status save_in_field(Field *field, bool no_conversions);

  virtual String *val_str(String*);
  virtual my_decimal *val_decimal(my_decimal *);
  virtual double val_real()
  {
    return null_value ? 0.0 : (double) cached_value;
  }
  virtual longlong val_int()
  {
    return null_value ? LL(0) : cached_value;
  }
  bool get_date(MYSQL_TIME *ltime, uint fuzzydate)
  {
    return get_date_from_int(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return get_time_from_int(ltime);
  }
  virtual void copy();
};


class Item_copy_uint : public Item_copy_int
{
public:
  Item_copy_uint (Item *item) : Item_copy_int(item) 
  {
    unsigned_flag= 1;
  }

  String *val_str(String*);
  double val_real()
  {
    return null_value ? 0.0 : (double) (ulonglong) cached_value;
  }
};


class Item_copy_float : public Item_copy
{
protected:  
  double cached_value; 
public:
  Item_copy_float (Item *i) : Item_copy(i) {}
  type_conversion_status save_in_field(Field *field, bool no_conversions);

  String *val_str(String*);
  my_decimal *val_decimal(my_decimal *);
  double val_real()
  {
    return null_value ? 0.0 : cached_value;
  }
  longlong val_int()
  {
    return (longlong) rint(val_real());
  }
  bool get_date(MYSQL_TIME *ltime, uint fuzzydate)
  {
    return get_date_from_real(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return get_time_from_real(ltime);
  }
  void copy()
  {
    cached_value= item->val_real();
    null_value= item->null_value;
  }
};


class Item_copy_decimal : public Item_copy
{
protected:  
  my_decimal cached_value;
public:
  Item_copy_decimal (Item *i) : Item_copy(i) {}
  type_conversion_status save_in_field(Field *field, bool no_conversions);

  String *val_str(String*);
  my_decimal *val_decimal(my_decimal *) 
  { 
    return null_value ? NULL: &cached_value; 
  }
  double val_real();
  longlong val_int();
  bool get_date(MYSQL_TIME *ltime, uint fuzzydate)
  {
    return get_date_from_decimal(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return get_time_from_decimal(ltime);
  }
  void copy();
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
  uint32 value_max_length;
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

class Cached_item_temporal :public Cached_item
{
  Item *item;
  longlong value;
public:
  Cached_item_temporal(Item *item_par) :item(item_par), value(0) {}
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
  uchar *buff;
  Field *field;
  uint length;

public:
#ifndef DBUG_OFF
  void dbug_print()
  {
    uchar *org_ptr;
    org_ptr= field->ptr;
    fprintf(DBUG_FILE, "new: ");
    field->dbug_print();
    field->ptr= buff;
    fprintf(DBUG_FILE, ", old: ");
    field->dbug_print();
    field->ptr= org_ptr;
    fprintf(DBUG_FILE, "\n");
  }
#endif
  Cached_item_field(Field *arg_field) : field(arg_field)
  {
    field= arg_field;
    /* TODO: take the memory allocation below out of the constructor. */
    buff= (uchar*) sql_calloc(length=field->pack_length());
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
  virtual void print(String *str, enum_query_type query_type);
  type_conversion_status save_in_field(Field *field_arg, bool no_conversions);
  table_map used_tables() const { return (table_map)0L; }
  Item *get_tmp_table_item(THD *thd) { return copy_or_same(thd); }

  bool walk(Item_processor processor, bool walk_subquery, uchar *args)
  {
    if (arg && arg->walk(processor, walk_subquery, args))
      return true;

    return (this->*processor)(args);
  }

  Item *transform(Item_transformer transformer, uchar *args);
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
  virtual void print(String *str, enum_query_type query_type);
  type_conversion_status save_in_field(Field *field_arg, bool no_conversions)
  {
    return Item_field::save_in_field(field_arg, no_conversions);
  }
  /* 
   We use RAND_TABLE_BIT to prevent Item_insert_value from
   being treated as a constant and precalculated before execution
  */
  table_map used_tables() const { return RAND_TABLE_BIT; }

  bool walk(Item_processor processor, bool walk_subquery, uchar *args)
  {
    return arg->walk(processor, walk_subquery, args) ||
	    (this->*processor)(args);
  }
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
  virtual void print(String *str, enum_query_type query_type);
  table_map used_tables() const { return (table_map)0L; }
  Field *get_tmp_table_field() { return 0; }
  Item *copy_or_same(THD *thd) { return this; }
  Item *get_tmp_table_item(THD *thd) { return copy_or_same(thd); }
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


class Item_cache: public Item_basic_constant
{
protected:
  Item *example;
  table_map used_table_map;
  /**
    Field that this object will get value from. This is used by 
    index-based subquery engines to detect and remove the equality injected 
    by IN->EXISTS transformation.
  */  
  Field *cached_field;
  enum enum_field_types cached_field_type;
  /*
    TRUE <=> cache holds value of the last stored item (i.e actual value).
    store() stores item to be cached and sets this flag to FALSE.
    On the first call of val_xxx function if this flag is set to FALSE the 
    cache_value() will be called to actually cache value of saved item.
    cache_value() will set this flag to TRUE.
  */
  bool value_cached;
public:
  Item_cache():
    example(0), used_table_map(0), cached_field(0),
    cached_field_type(MYSQL_TYPE_STRING),
    value_cached(0)
  {
    fixed= 1; 
    null_value= 1;
  }
  Item_cache(enum_field_types field_type_arg):
    example(0), used_table_map(0), cached_field(0),
    cached_field_type(field_type_arg),
    value_cached(0)
  {
    fixed= 1;
    null_value= 1;
  }

  void set_used_tables(table_map map) { used_table_map= map; }

  virtual table_map resolved_used_tables() const
  {
    return example ? example->resolved_used_tables() : used_table_map;
  }

  virtual bool allocate(uint i) { return 0; }
  virtual bool setup(Item *item)
  {
    example= item;
    max_length= item->max_length;
    decimals= item->decimals;
    collation.set(item->collation);
    unsigned_flag= item->unsigned_flag;
    if (item->type() == FIELD_ITEM)
      cached_field= ((Item_field *)item)->field;
    return 0;
  };
  enum Type type() const { return CACHE_ITEM; }
  enum_field_types field_type() const { return cached_field_type; }
  static Item_cache* get_cache(const Item *item);
  static Item_cache* get_cache(const Item* item, const Item_result type);
  table_map used_tables() const { return used_table_map; }
  virtual void keep_array() {}
  virtual void print(String *str, enum_query_type query_type);
  bool eq_def(Field *field) 
  { 
    return cached_field ? cached_field->eq_def (field) : FALSE;
  }
  bool eq(const Item *item, bool binary_cmp) const
  {
    return this == item;
  }
  /**
     Check if saved item has a non-NULL value.
     Will cache value of saved item if not already done. 
     @return TRUE if cached value is non-NULL.
   */
  bool has_value()
  {
    return (value_cached || cache_value()) && !null_value;
  }

  /** 
    If this item caches a field value, return pointer to underlying field.

    @return Pointer to field, or NULL if this is not a cache for a field value.
  */
  Field* field() { return cached_field; }

  virtual void store(Item *item);
  virtual bool cache_value()= 0;
  bool basic_const_item() const
  { return MY_TEST(example && example->basic_const_item());}
  bool walk (Item_processor processor, bool walk_subquery, uchar *argument);
  virtual void clear() { null_value= TRUE; value_cached= FALSE; }
  bool is_null() { return value_cached ? null_value : example->is_null(); }
  Item_result result_type() const
  {
    if (!example)
      return INT_RESULT;
    return Field::result_merge_type(example->field_type());
  }
};


class Item_cache_int: public Item_cache
{
protected:
  longlong value;
public:
  Item_cache_int(): Item_cache(),
    value(0) {}
  Item_cache_int(enum_field_types field_type_arg):
    Item_cache(field_type_arg), value(0) {}

  virtual void store(Item *item){ Item_cache::store(item); }
  void store(Item *item, longlong val_arg);
  double val_real();
  longlong val_int();
  longlong val_time_temporal() { return val_int(); }
  longlong val_date_temporal() { return val_int(); }
  String* val_str(String *str);
  my_decimal *val_decimal(my_decimal *);
  bool get_date(MYSQL_TIME *ltime, uint fuzzydate)
  {
    return get_date_from_int(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return get_time_from_int(ltime);
  }
  enum Item_result result_type() const { return INT_RESULT; }
  bool cache_value();
};


class Item_cache_real: public Item_cache
{
  double value;
public:
  Item_cache_real(): Item_cache(),
    value(0) {}

  double val_real();
  longlong val_int();
  String* val_str(String *str);
  my_decimal *val_decimal(my_decimal *);
  bool get_date(MYSQL_TIME *ltime, uint fuzzydate)
  {
    return get_date_from_real(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return get_time_from_real(ltime);
  }
  enum Item_result result_type() const { return REAL_RESULT; }
  bool cache_value();
};


class Item_cache_decimal: public Item_cache
{
protected:
  my_decimal decimal_value;
public:
  Item_cache_decimal(): Item_cache() {}

  double val_real();
  longlong val_int();
  String* val_str(String *str);
  my_decimal *val_decimal(my_decimal *);
  bool get_date(MYSQL_TIME *ltime, uint fuzzydate)
  {
    return get_date_from_decimal(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return get_time_from_decimal(ltime);
  }
  enum Item_result result_type() const { return DECIMAL_RESULT; }
  bool cache_value();
};


class Item_cache_str: public Item_cache
{
  char buffer[STRING_BUFFER_USUAL_SIZE];
  String *value, value_buff;
  bool is_varbinary;
  
public:
  Item_cache_str(const Item *item) :
    Item_cache(item->field_type()), value(0),
    is_varbinary(item->type() == FIELD_ITEM &&
                 cached_field_type == MYSQL_TYPE_VARCHAR &&
                 !((const Item_field *) item)->field->has_charset())
  {
    collation.set(const_cast<DTCollation&>(item->collation));
  }
  double val_real();
  longlong val_int();
  String* val_str(String *);
  my_decimal *val_decimal(my_decimal *);
  bool get_date(MYSQL_TIME *ltime, uint fuzzydate)
  {
    return get_date_from_string(ltime, fuzzydate);
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    return get_time_from_string(ltime);
  }
  enum Item_result result_type() const { return STRING_RESULT; }
  const CHARSET_INFO *charset() const { return value->charset(); };
  type_conversion_status save_in_field(Field *field, bool no_conversions);
  bool cache_value();
};

class Item_cache_row: public Item_cache
{
  Item_cache  **values;
  uint item_count;
  bool save_array;
public:
  Item_cache_row()
    :Item_cache(), values(0), item_count(2),
    save_array(0) {}
  
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
  bool get_date(MYSQL_TIME *ltime, uint fuzzydate)
  {
    illegal_method_call((const char *) "get_date");
    return true;
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    illegal_method_call((const char *) "get_time");
    return true;
  }

  enum Item_result result_type() const { return ROW_RESULT; }
  
  uint cols() { return item_count; }
  Item *element_index(uint i) { return values[i]; }
  Item **addr(uint i) { return (Item **) (values + i); }
  bool check_cols(uint c);
  bool null_inside();
  void bring_value();
  void keep_array() { save_array= 1; }
  void cleanup()
  {
    DBUG_ENTER("Item_cache_row::cleanup");
    Item_cache::cleanup();
    if (save_array)
      memset(values, 0, item_count*sizeof(Item**));
    else
      values= 0;
    DBUG_VOID_RETURN;
  }
  bool cache_value();
};


class Item_cache_datetime: public Item_cache
{
protected:
  String str_value;
  longlong int_value;
  bool str_value_cached;
public:
  Item_cache_datetime(enum_field_types field_type_arg):
    Item_cache(field_type_arg), int_value(0), str_value_cached(0)
  {
    cmp_context= STRING_RESULT;
  }

  void store(Item *item, longlong val_arg);
  void store(Item *item);
  double val_real();
  longlong val_int();
  longlong val_time_temporal();
  longlong val_date_temporal();
  String* val_str(String *str);
  my_decimal *val_decimal(my_decimal *);
  bool get_date(MYSQL_TIME *ltime, uint fuzzydate);
  bool get_time(MYSQL_TIME *ltime);
  enum Item_result result_type() const { return STRING_RESULT; }
  /*
    In order to avoid INT <-> STRING conversion of a DATETIME value
    two cache_value functions are introduced. One (cache_value) caches STRING
    value, another (cache_value_int) - INT value. Thus this cache item
    completely relies on the ability of the underlying item to do the
    correct conversion.
  */
  bool cache_value_int();
  bool cache_value();
  void clear() { Item_cache::clear(); str_value_cached= FALSE; }
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
  Field::geometry_type geometry_type;

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
  bool get_date(MYSQL_TIME *ltime, uint fuzzydate)
  {
    DBUG_ASSERT(0);
    return true;
  }
  bool get_time(MYSQL_TIME *ltime)
  {
    DBUG_ASSERT(0);
    return true;
  }
  bool join_types(THD *thd, Item *);
  Field *make_field_by_type(TABLE *table);
  static uint32 display_length(Item *item);
  static enum_field_types get_real_type(Item *);
  Field::geometry_type get_geometry_type() const { return geometry_type; };
};


class st_select_lex;
void mark_select_range_as_dependent(THD *thd,
                                    st_select_lex *last_select,
                                    st_select_lex *current_sel,
                                    Field *found_field, Item *found_item,
                                    Item_ident *resolved_item);

extern Cached_item *new_Cached_item(THD *thd, Item *item,
                                    bool use_result_field);
extern Item_result item_cmp_type(Item_result a,Item_result b);
extern void resolve_const_item(THD *thd, Item **ref, Item *cmp_item);
extern int stored_field_cmp_to_item(THD *thd, Field *field, Item *item);

extern const String my_null_string;

#endif /* ITEM_INCLUDED */
