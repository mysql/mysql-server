/*
   Copyright (c) 2000, 2019, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*
  This file defines the NDB Cluster handler engine_condition_pushdown
*/

#include "sql/ha_ndbcluster_cond.h"
#include "sql/ha_ndbcluster.h"

#include "my_dbug.h"
#include "sql/current_thd.h"
#include "sql/item.h"       // Item
#include "sql/item_cmpfunc.h" // Item_func_like etc.
#include "sql/item_func.h"  // Item_func
#include "sql/ndb_log.h"

// Typedefs for long names 
typedef NdbDictionary::Column NDBCOL;
typedef NdbDictionary::Table NDBTAB;

typedef enum ndb_item_type {
  NDB_VALUE = 0,   // Qualified more with Item::Type
  NDB_FIELD = 1,   // Qualified from table definition
  NDB_FUNCTION = 2,// Qualified from Item_func::Functype
  NDB_END_COND = 3 // End marker for condition group
} NDB_ITEM_TYPE;

typedef enum ndb_func_type {
  NDB_EQ_FUNC = 0,
  NDB_NE_FUNC = 1,
  NDB_LT_FUNC = 2,
  NDB_LE_FUNC = 3,
  NDB_GT_FUNC = 4,
  NDB_GE_FUNC = 5,
  NDB_ISNULL_FUNC = 6,
  NDB_ISNOTNULL_FUNC = 7,
  NDB_LIKE_FUNC = 8,
  NDB_NOTLIKE_FUNC = 9,
  NDB_NOT_FUNC = 10,
  NDB_UNKNOWN_FUNC = 11,
  NDB_COND_AND_FUNC = 12,
  NDB_COND_OR_FUNC = 13,
  NDB_UNSUPPORTED_FUNC = 14
} NDB_FUNC_TYPE;

typedef union ndb_item_qualification {
  Item::Type value_type;       // NDB_VALUE
  enum_field_types field_type; // NDB_FIELD, instead of Item::FIELD_ITEM
  NDB_FUNC_TYPE function_type; // NDB_FUNCTION, instead of Item::FUNC_ITEM
} NDB_ITEM_QUALIFICATION;

typedef struct ndb_item_field_value {
  Field* field;
  int column_no;
} NDB_ITEM_FIELD_VALUE;

typedef union ndb_item_value {
  const Item *item;                     // NDB_VALUE
  NDB_ITEM_FIELD_VALUE *field_value;    // NDB_FIELD
  uint arg_count;                       // NDB_FUNCTION
} NDB_ITEM_VALUE;

/*
  Mapping defining the negated and swapped function equivalent
   - 'not op1 func op2' -> 'op1 neg_func op2'
   - 'op1 func op2' -> ''op2 swap_func op1'
*/
struct function_mapping
{
  NDB_FUNC_TYPE func;
  NDB_FUNC_TYPE neg_func;
  NDB_FUNC_TYPE swap_func;
};

/*
  Define what functions can be negated in condition pushdown.
  Note, these HAVE to be in the same order as in definition enum
*/
static const function_mapping func_map[]=
{
  {NDB_EQ_FUNC, NDB_NE_FUNC, NDB_EQ_FUNC},
  {NDB_NE_FUNC, NDB_EQ_FUNC, NDB_NE_FUNC},
  {NDB_LT_FUNC, NDB_GE_FUNC, NDB_GT_FUNC},
  {NDB_LE_FUNC, NDB_GT_FUNC, NDB_GE_FUNC},
  {NDB_GT_FUNC, NDB_LE_FUNC, NDB_LT_FUNC},
  {NDB_GE_FUNC, NDB_LT_FUNC, NDB_LE_FUNC},
  {NDB_ISNULL_FUNC, NDB_ISNOTNULL_FUNC, NDB_UNSUPPORTED_FUNC},
  {NDB_ISNOTNULL_FUNC, NDB_ISNULL_FUNC, NDB_UNSUPPORTED_FUNC},
  {NDB_LIKE_FUNC, NDB_NOTLIKE_FUNC, NDB_UNSUPPORTED_FUNC},
  {NDB_NOTLIKE_FUNC, NDB_LIKE_FUNC, NDB_UNSUPPORTED_FUNC},
  {NDB_NOT_FUNC, NDB_UNSUPPORTED_FUNC, NDB_UNSUPPORTED_FUNC},
  {NDB_UNKNOWN_FUNC, NDB_UNSUPPORTED_FUNC, NDB_UNSUPPORTED_FUNC},
  {NDB_COND_AND_FUNC, NDB_UNSUPPORTED_FUNC, NDB_UNSUPPORTED_FUNC},
  {NDB_COND_OR_FUNC, NDB_UNSUPPORTED_FUNC, NDB_UNSUPPORTED_FUNC},
  {NDB_UNSUPPORTED_FUNC, NDB_UNSUPPORTED_FUNC, NDB_UNSUPPORTED_FUNC}
};


/*
  This class is the construction element for serialization of Item tree
  in condition pushdown.
  An instance of Ndb_Item represents a constant, table field reference,
  unary or binary comparison predicate, and start/end of AND/OR.
  Instances of Ndb_Item are stored in a linked list using the List-template

  The order of elements produced by iterating this list corresponds to
  breadth-first traversal of the Item (i.e. expression) tree in prefix order.
  AND and OR have arbitrary arity, so the end of AND/OR group is marked with
  Ndb_item with type == NDB_END_COND.
  NOT items represent negated conditions and generate NAND/NOR groups.
*/
class Ndb_item
{
public:
  Ndb_item(NDB_ITEM_TYPE item_type) : type(item_type) {}
  // A Ndb_Item where an Item expression defines the value (a const)
  Ndb_item(const Item *item_value, Item::Type item_type) : type(NDB_VALUE)
  {
    qualification.value_type= item_type;
    value.item= item_value;
  }
  // A Ndb_Item referring a Field from a previous table
  // Handled as a NDB_VALUE, as its value is known when we 'generate'
  Ndb_item(Field *field) : type(NDB_VALUE)
  {
    const Item *item_value = new(*THR_MALLOC) Item_field(field);
    value.item= item_value;
    qualification.value_type= item_value->type();
  }
  // A Ndb_Item reffering a Field from 'this' table
  Ndb_item(Field *field, int column_no) : type(NDB_FIELD)
  {
    NDB_ITEM_FIELD_VALUE *field_value= new (*THR_MALLOC) NDB_ITEM_FIELD_VALUE();
    qualification.field_type= field->real_type();
    field_value->field= field;
    field_value->column_no= column_no;
    value.field_value= field_value;
  }
  Ndb_item(Item_func::Functype func_type, const Item *item_value)
    : type(NDB_FUNCTION)
  {
    qualification.function_type= item_func_to_ndb_func(func_type);
    value.arg_count= ((Item_func *) item_value)->argument_count();
  }
  Ndb_item(Item_func::Functype func_type, uint no_args)
    : type(NDB_FUNCTION)
  {
    qualification.function_type= item_func_to_ndb_func(func_type);
    value.arg_count= no_args;
  }
  ~Ndb_item()
  {
    if (type == NDB_FIELD)
    {
      destroy(value.field_value);
      value.field_value= NULL;
    }
  }

  uint32 pack_length()
  {
    switch(type) {
    case(NDB_VALUE):
      if(qualification.value_type == Item::STRING_ITEM && value.item->type() == Item::STRING_ITEM)
        // const_cast is safe for Item_string.
        return const_cast<Item*>(value.item)->val_str(nullptr)->length();
      break;
    case(NDB_FIELD):
      return value.field_value->field->pack_length();
    default:
      break;
    }

    return 0;
  }

  Field * get_field() { return value.field_value->field; }

  int get_field_no() { return value.field_value->column_no; }

  int argument_count()
  {
    DBUG_ASSERT(type == NDB_FUNCTION);
    return value.arg_count;
  }

  const char* get_val()
  {
    switch(type) {
    case(NDB_VALUE):
      if(qualification.value_type == Item::STRING_ITEM && value.item->type() == Item::STRING_ITEM)
        // const_cast is safe for Item_string.
        return const_cast<Item*>(value.item)->val_str(nullptr)->ptr();
      break;
    case(NDB_FIELD):
      return (char*) value.field_value->field->ptr;
    default:
      DBUG_ASSERT(false);
      break;
    }

    return NULL;
  }

  const CHARSET_INFO *get_field_charset()
  {
    Field *field= get_field();
    if (field)
      return field->charset();

    return NULL;
  }

  String *get_field_val_str(String *str)
  {
    Field *field= get_field();
    if (field)
      return field->val_str(str);

    return NULL;
  }

  const Item *get_item()
  {
    DBUG_ASSERT(this->type == NDB_VALUE);
    return value.item;
  }

  bool is_const_func()
  {
    DBUG_ASSERT(this->type == NDB_VALUE);
    const Item *item= value.item;

    if (item->type() == Item::FUNC_ITEM)
    {
      const Item_func *func_item= static_cast<const Item_func*>(item);
      if (func_item->const_item())
        return true;
    }
    return false;
  }

  bool is_cached()
  {
    DBUG_ASSERT(this->type == NDB_VALUE);
    const Item *item= value.item;

    return (item->type() == Item::CACHE_ITEM);
  }

  uint32 save_in_field(Ndb_item *field_item, bool allow_truncate=false)
  {
    uint32 length= 0;
    DBUG_ENTER("save_in_field");
    Field *field = field_item->value.field_value->field;
    const Item *item= value.item;
    if (item && field)
    {
      length= item->max_length;
      my_bitmap_map *old_map=
        dbug_tmp_use_all_columns(field->table, field->table->write_set);
      const type_conversion_status status = const_cast<Item*>(item)->save_in_field(field, false);
      dbug_tmp_restore_column_map(field->table->write_set, old_map);

      if (unlikely(status != TYPE_OK))
      {
        if (!allow_truncate)
          DBUG_RETURN(0);

        switch (status)
        {
          case TYPE_NOTE_TRUNCATED:
          case TYPE_WARN_TRUNCATED:
          case TYPE_NOTE_TIME_TRUNCATED:
            break; // -> OK
          default:
            DBUG_RETURN(0);
        }
      }
    }
    DBUG_RETURN(length);
  }

  static NDB_FUNC_TYPE item_func_to_ndb_func(Item_func::Functype fun)
  {
    switch (fun) {
    case (Item_func::EQ_FUNC): { return NDB_EQ_FUNC; }
    case (Item_func::NE_FUNC): { return NDB_NE_FUNC; }
    case (Item_func::LT_FUNC): { return NDB_LT_FUNC; }
    case (Item_func::LE_FUNC): { return NDB_LE_FUNC; }
    case (Item_func::GT_FUNC): { return NDB_GT_FUNC; }
    case (Item_func::GE_FUNC): { return NDB_GE_FUNC; }
    case (Item_func::ISNULL_FUNC): { return NDB_ISNULL_FUNC; }
    case (Item_func::ISNOTNULL_FUNC): { return NDB_ISNOTNULL_FUNC; }
    case (Item_func::LIKE_FUNC): { return NDB_LIKE_FUNC; }
    case (Item_func::NOT_FUNC): { return NDB_NOT_FUNC; }
    case (Item_func::NEG_FUNC): { return NDB_UNKNOWN_FUNC; }
    case (Item_func::DATE_FUNC): { return NDB_UNKNOWN_FUNC; }
    case (Item_func::DATETIME_LITERAL): { return NDB_UNKNOWN_FUNC; }
    case (Item_func::UNKNOWN_FUNC): { return NDB_UNKNOWN_FUNC; }
    case (Item_func::COND_AND_FUNC): { return NDB_COND_AND_FUNC; }
    case (Item_func::COND_OR_FUNC): { return NDB_COND_OR_FUNC; }
    default: { return NDB_UNSUPPORTED_FUNC; }
    }
  }

  static NDB_FUNC_TYPE negate(NDB_FUNC_TYPE fun)
  {
    uint i= (uint) fun;
    DBUG_ASSERT(fun == func_map[i].func);
    return  func_map[i].neg_func;
  }

  static NDB_FUNC_TYPE swap(NDB_FUNC_TYPE fun)
  {
    uint i= (uint) fun;
    DBUG_ASSERT(fun == func_map[i].func);
    return  func_map[i].swap_func;
  }

  const NDB_ITEM_TYPE type;
  NDB_ITEM_QUALIFICATION qualification;
 private:
  NDB_ITEM_VALUE value;
};

/*
  This class implements look-ahead during the parsing
  of the item tree. It contains bit masks for expected
  items, field types and field results. It also contains
  expected collation. The parse context (Ndb_cond_traverse_context)
  always contains one expect_stack instance (top of the stack).
  More expects (deeper look-ahead) can be pushed to the expect_stack
  to check specific order (currently used for detecting support for
  <field> LIKE <string>|<func>, but not <string>|<func> LIKE <field>).
 */
class Ndb_expect_stack
{
  static const uint MAX_EXPECT_ITEMS = Item::VIEW_FIXER_ITEM + 1;
  static const uint MAX_EXPECT_FIELD_TYPES = MYSQL_TYPE_GEOMETRY + 1;
  static const uint MAX_EXPECT_FIELD_RESULTS = DECIMAL_RESULT + 1;
 public:
  Ndb_expect_stack(): table(nullptr),
                      collation(NULL), length(0), max_length(0), next(NULL)
  {
    // Allocate type checking bitmaps using fixed size buffers
    // since max size is known at compile time
    bitmap_init(&expect_mask, m_expect_buf,
                MAX_EXPECT_ITEMS, false);
    bitmap_init(&expect_field_type_mask, m_expect_field_type_buf,
                MAX_EXPECT_FIELD_TYPES, false);
    bitmap_init(&expect_field_result_mask, m_expect_field_result_buf,
                MAX_EXPECT_FIELD_RESULTS, false);
  }
  ~Ndb_expect_stack()
  {
    if (next)
      destroy(next);
    next= NULL;
  }
  void push(Ndb_expect_stack* expect_next)
  {
    next= expect_next;
  }
  void pop()
  {
    if (next)
    {
      Ndb_expect_stack* expect_next= next;
      bitmap_clear_all(&expect_mask);
      bitmap_union(&expect_mask, &next->expect_mask);
      bitmap_clear_all(&expect_field_type_mask);
      bitmap_union(&expect_field_type_mask, &next->expect_field_type_mask);
      bitmap_clear_all(&expect_field_result_mask);
      bitmap_union(&expect_field_result_mask, &next->expect_field_result_mask);
      collation= next->collation;
      table= next->table;
      next= next->next;
      destroy(expect_next);
    }
  }

  void expect(TABLE* tab)
  {
    table = tab;
  }
  void dont_expect(TABLE *tab MY_ATTRIBUTE((unused)))
  {
    table = nullptr;
  }
  bool expecting(TABLE *tab) const
  {
    return (table == tab);
  }

  void expect(Item::Type type)
  {
    bitmap_set_bit(&expect_mask, (uint) type);
  }
  void dont_expect(Item::Type type)
  {
    bitmap_clear_bit(&expect_mask, (uint) type);
  }
  bool expecting(Item::Type type)
  {
    if (unlikely((uint)type > MAX_EXPECT_ITEMS))
    {
      // Unknown type, can't be expected
      return false;
    }
    return bitmap_is_set(&expect_mask, (uint) type);
  }
  void expect_nothing()
  {
    bitmap_clear_all(&expect_mask);
  }
  bool expecting_nothing()
  {
    return bitmap_is_clear_all(&expect_mask);
  }
  void expect_only(Item::Type type)
  {
    expect_nothing();
    expect(type);
  }
  bool expecting_only(Item::Type type)
  {
    return (expecting(type) && bitmap_bits_set(&expect_mask) == 1);
  }

  void expect_field_type(enum_field_types type)
  {
    bitmap_set_bit(&expect_field_type_mask, (uint) type);
  }
  void dont_expect_field_type(enum_field_types type)
  {
    bitmap_clear_bit(&expect_field_type_mask, (uint) type);
  }
  void expect_all_field_types()
  {
    bitmap_set_all(&expect_field_type_mask);
  }
  bool expecting_field_type(enum_field_types type)
  {
    if (unlikely((uint)type > MAX_EXPECT_FIELD_TYPES))
    {
      // Unknown type, can't be expected
      return false;
    }
    return bitmap_is_set(&expect_field_type_mask, (uint) type);
  }
  void expect_no_field_type()
  {
    bitmap_clear_all(&expect_field_type_mask);
  }
  bool expecting_no_field_type()
  {
    return bitmap_is_clear_all(&expect_field_type_mask);
  }
  void expect_only_field_type(enum_field_types result)
  {
    expect_no_field_type();
    expect_field_type(result);
  }

  void expect_field_result(Item_result result)
  {
    bitmap_set_bit(&expect_field_result_mask, (uint) result);
  }
  bool expecting_field_result(Item_result result)
  {
    if (unlikely((uint)result > MAX_EXPECT_FIELD_RESULTS))
    {
      // Unknown result, can't be expected
      return false;
    }
    return bitmap_is_set(&expect_field_result_mask,
                         (uint) result);
  }
  void expect_no_field_result()
  {
    bitmap_clear_all(&expect_field_result_mask);
  }
  bool expecting_no_field_result()
  {
    return bitmap_is_clear_all(&expect_field_result_mask);
  }
  void expect_only_field_result(Item_result result)
  {
    expect_no_field_result();
    expect_field_result(result);
  }
  void expect_collation(const CHARSET_INFO* col)
  {
    collation= col;
  }
  bool expecting_collation(const CHARSET_INFO* col)
  {
    bool matching= (!collation)
      ? true
      : (collation == col);
    collation= NULL;

    return matching;
  }
  void expect_length(Uint32 len)
  {
    length= len;
  }
  void expect_max_length(Uint32 max)
  {
    max_length= max;
  }
  bool expecting_length(Uint32 len)
  {
    return max_length == 0 || len <= max_length;
  }
  bool expecting_max_length(Uint32 max)
  {
    return max >= length;
  }
  void expect_no_length()
  {
    length= max_length= 0;
  }

private:
  TABLE* table;
  my_bitmap_map
    m_expect_buf[bitmap_buffer_size(MAX_EXPECT_ITEMS)];
  my_bitmap_map
    m_expect_field_type_buf[bitmap_buffer_size(MAX_EXPECT_FIELD_TYPES)];
  my_bitmap_map
    m_expect_field_result_buf[bitmap_buffer_size(MAX_EXPECT_FIELD_RESULTS)];
  MY_BITMAP expect_mask;
  MY_BITMAP expect_field_type_mask;
  MY_BITMAP expect_field_result_mask;
  const CHARSET_INFO* collation;
  Uint32 length;
  Uint32 max_length;
  Ndb_expect_stack* next;
};

class Ndb_rewrite_context
{
public:
  Ndb_rewrite_context(Item_func *func)
    : func_item(func), left_hand_item(NULL), count(0) {}
  ~Ndb_rewrite_context()
  {
    if (next) destroy(next);
  }
  const Item_func *func_item;
  const Item *left_hand_item;
  uint count;
  Ndb_rewrite_context *next;
};

/*
  This class is used for storing the context when traversing
  the Item tree. It stores a reference to the table the condition
  is defined on, the serialized representation being generated,
  if the condition found is supported, and information what is
  expected next in the tree inorder for the condition to be supported.
*/
class Ndb_cond_traverse_context
{
public:
  Ndb_cond_traverse_context(TABLE *tab, const NdbDictionary::Table *ndb_tab,
                            bool other_tbls_ok)
    : table(tab), ndb_table(ndb_tab), m_other_tbls_ok(other_tbls_ok),
    supported(true), skip(0), rewrite_stack(NULL)
  {}
  ~Ndb_cond_traverse_context()
  {
    if (rewrite_stack) destroy(rewrite_stack);
  }

  inline void expect_field_from_table()
  {
    expect_stack.expect(table);
    expect_stack.expect(Item::FIELD_ITEM);
    expect_stack.expect_all_field_types();
  }
  inline void expect_only_field_from_table()
  {
    expect_stack.expect_nothing();
    expect_field_from_table();
  }
  inline void dont_expect_field_from_table()
  {
    expect_stack.dont_expect(table);
    expect_stack.dont_expect(Item::FIELD_ITEM);
  }
  inline bool expecting_field_from_table()
  {
    return expect_stack.expecting(table) &&
           expect_stack.expecting(Item::FIELD_ITEM);
  }
  inline bool expecting_only_field_from_table()
  {
    return expect_stack.expecting(table) &&
           expect_stack.expecting_only(Item::FIELD_ITEM);
  }

  inline void expect(Item::Type type)
  {
    expect_stack.expect(type);
  }
  inline void dont_expect(Item::Type type)
  {
    expect_stack.dont_expect(type);
  }
  inline bool expecting(Item::Type type)
  {
    return expect_stack.expecting(type);
  }
  inline void expect_nothing()
  {
    expect_stack.expect_nothing();
  }
  inline bool expecting_nothing()
  {
    return expect_stack.expecting_nothing();
  }
  inline void expect_only(Item::Type type)
  {
    expect_stack.expect_only(type);
  }

  inline void expect_field_type(enum_field_types type)
  {
    expect_stack.expect_field_type(type);
  }
  inline void dont_expect_field_type(enum_field_types type)
  {
    expect_stack.dont_expect_field_type(type);
  }
  inline void expect_all_field_types()
  {
    expect_stack.expect_all_field_types();
  }
  inline bool expecting_field_type(enum_field_types type)
  {
    return expect_stack.expecting_field_type(type);
  }
  inline void expect_no_field_type()
  {
    expect_stack.expect_no_field_type();
  }
  inline bool expecting_no_field_type()
  {
    return expect_stack.expecting_no_field_type();
  }
  inline void expect_only_field_type(enum_field_types result)
  {
    expect_stack.expect_only_field_type(result);
  }

  inline void expect_field_result(Item_result result)
  {
    expect_stack.expect_field_result(result);
  }
  inline bool expecting_field_result(Item_result result)
  {
    return expect_stack.expecting_field_result(result);
  }
  inline void expect_no_field_result()
  {
    expect_stack.expect_no_field_result();
  }
  inline bool expecting_no_field_result()
  {
    return expect_stack.expecting_no_field_result();
  }
  inline void expect_only_field_result(Item_result result)
  {
    expect_stack.expect_only_field_result(result);
  }
  inline void expect_collation(const CHARSET_INFO* col)
  {
    expect_stack.expect_collation(col);
  }
  inline bool expecting_collation(const CHARSET_INFO* col)
  {
    return expect_stack.expecting_collation(col);
  }
  inline void expect_length(Uint32 length)
  {
    expect_stack.expect_length(length);
  }
  inline void expect_max_length(Uint32 max)
  {
    expect_stack.expect_max_length(max);
  }
  inline bool expecting_length(Uint32 length)
  {
    return expect_stack.expecting_length(length);
  }
  inline bool expecting_max_length(Uint32 max)
  {
    return expect_stack.expecting_max_length(max);
  }
  inline void expect_no_length()
  {
    expect_stack.expect_no_length();
  }

  TABLE* const table;
  const NdbDictionary::Table* const ndb_table;
  const bool m_other_tbls_ok;
  bool supported;
  List<Ndb_item> items;
  Ndb_expect_stack expect_stack;
  uint skip;
  Ndb_rewrite_context *rewrite_stack;
};

static bool
is_supported_temporal_type(enum_field_types type)
{
  switch(type) {
  case MYSQL_TYPE_TIME:
  case MYSQL_TYPE_TIME2:
  case MYSQL_TYPE_DATE:
  case MYSQL_TYPE_NEWDATE:
  case MYSQL_TYPE_YEAR:
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_DATETIME2:
    return true;
  default:
    return false;
  }
}

/*
  Serialize the item tree into a List of Ndb_item objecs
  for fast generation of NbdScanFilter. Adds information such as
  position of fields that is not directly available in the Item tree.
  Also checks if condition is supported.
*/
static void
ndb_serialize_cond(const Item *item, void *arg)
{
  Ndb_cond_traverse_context *context= (Ndb_cond_traverse_context *) arg;
  DBUG_ENTER("ndb_serialize_cond");

  // Check if we are skipping arguments to a function to be evaluated
  if (context->skip)
  {
    if (!item)
    {
      ndb_log_error("ndb_serialize_cond(), Unexpected mismatch of found and "
                    "expected number of function arguments %u", context->skip);
      context->skip= 0;
      DBUG_VOID_RETURN;
    }
    DBUG_PRINT("info", ("Skipping argument %d", context->skip));
    context->skip--;
    switch (item->type()) {
    case Item::FUNC_ITEM:
    {
      Item_func *func_item= (Item_func *) item;
      context->skip+= func_item->argument_count();
      break;
    }
    case Item::INT_ITEM:
    case Item::REAL_ITEM:
    case Item::STRING_ITEM:
    case Item::VARBIN_ITEM:
    case Item::DECIMAL_ITEM:
      break;
    default:
      context->supported= false;
      break;
    }
    
    DBUG_VOID_RETURN;
  }
  
  if (context->supported)
  {
    Ndb_rewrite_context *rewrite_context2= context->rewrite_stack;
    const Item_func *rewrite_func_item;
    // Check if we are rewriting some unsupported function call
    if (rewrite_context2 &&
        (rewrite_func_item= rewrite_context2->func_item) &&
        rewrite_context2->count++ == 0)
    {
      switch (rewrite_func_item->functype()) {
      case Item_func::BETWEEN:
        /*
          Rewrite 
          <field>|<const> BETWEEN <const1>|<field1> AND <const2>|<field2>
          to <field>|<const> > <const1>|<field1> AND 
          <field>|<const> < <const2>|<field2>
          or actually in prefix format
          BEGIN(AND) GT(<field>|<const>, <const1>|<field1>), 
          LT(<field>|<const>, <const2>|<field2>), END()
        */
      case Item_func::IN_FUNC:
      {
        /*
          Rewrite <field>|<const> IN(<const1>|<field1>, <const2>|<field2>,..)
          to <field>|<const> = <const1>|<field1> OR 
          <field> = <const2>|<field2> ...
          or actually in prefix format
          BEGIN(OR) EQ(<field>|<const>, <const1><field1>), 
          EQ(<field>|<const>, <const2>|<field2>), ... END()
          Each part of the disjunction is added for each call
          to ndb_serialize_cond and end of rewrite statement 
          is wrapped in end of ndb_serialize_cond
        */
        if (context->expecting(item->type()))
        {
          // This is the <field>|<const> item, save it in the rewrite context
          rewrite_context2->left_hand_item= item;
          if (item->type() == Item::FUNC_ITEM)
          {
            Item_func *func_item= (Item_func *) item;
            if ((func_item->functype() == Item_func::UNKNOWN_FUNC ||
                 func_item->functype() == Item_func::DATE_FUNC ||
                 func_item->functype() == Item_func::DATETIME_LITERAL ||
                 func_item->functype() == Item_func::NEG_FUNC) &&
                func_item->const_item())
            {
              // Skip any arguments since we will evaluate function instead
              DBUG_PRINT("info", ("Skip until end of arguments marker"));
              context->skip= func_item->argument_count();
            }
            else
            {
              DBUG_PRINT("info", ("Found unsupported functional expression in BETWEEN|IN"));
              context->supported= false;
              DBUG_VOID_RETURN;
            }
          }
        }
        else
        {
          // Non-supported BETWEEN|IN expression
          DBUG_PRINT("info", ("Found unexpected item of type %u in BETWEEN|IN",
                              item->type()));
          context->supported= false;
          DBUG_VOID_RETURN;
        }
        break;
      }
      default:
        context->supported= false;
        break;
      }
      DBUG_VOID_RETURN;
    }
    else
    {
      Ndb_item *ndb_item = nullptr;
      // Check if we are rewriting some unsupported function call
      if (context->rewrite_stack)
      {
        Ndb_rewrite_context *rewrite_context= context->rewrite_stack;
        const Item_func *func_item= rewrite_context->func_item;
        context->expect_only_field_from_table();
        context->expect_no_field_result();
        context->expect(Item::INT_ITEM);
        context->expect(Item::STRING_ITEM);
        context->expect(Item::VARBIN_ITEM);
        context->expect(Item::FUNC_ITEM);
        context->expect(Item::CACHE_ITEM);
        switch (func_item->functype()) {
        case Item_func::BETWEEN:
        {
          /*
            Rewrite 
            <field>|<const> BETWEEN <const1>|<field1> AND <const2>|<field2>
            to <field>|<const> > <const1>|<field1> AND 
            <field>|<const> < <const2>|<field2>
            or actually in prefix format
            BEGIN(AND) GT(<field>|<const>, <const1>|<field1>), 
            LT(<field>|<const>, <const2>|<field2>), END()
          */
          if (rewrite_context->count == 2)
          {
            // Lower limit of BETWEEN
            DBUG_PRINT("info", ("GE_FUNC"));      
            ndb_item= new (*THR_MALLOC) Ndb_item(Item_func::GE_FUNC, 2);
          }
          else if (rewrite_context->count == 3)
          {
            // Upper limit of BETWEEN
            DBUG_PRINT("info", ("LE_FUNC"));      
            ndb_item= new (*THR_MALLOC) Ndb_item(Item_func::LE_FUNC, 2);
          }
          else
          {
            // Illegal BETWEEN expression
            DBUG_PRINT("info", ("Illegal BETWEEN expression"));
            context->supported= false;
            DBUG_VOID_RETURN;
          }
          // Enum comparison can not be pushed
          context->dont_expect_field_type(MYSQL_TYPE_ENUM);
          break;
        }
        case Item_func::IN_FUNC:
        {
          /*
            Rewrite <field>|<const> IN(<const1>|<field1>, <const2>|<field2>,..)
            to <field>|<const> = <const1>|<field1> OR 
            <field> = <const2>|<field2> ...
            or actually in prefix format
            BEGIN(OR) EQ(<field>|<const>, <const1><field1>), 
            EQ(<field>|<const>, <const2>|<field2>), ... END()
            Each part of the disjunction is added for each call
            to ndb_serialize_cond and end of rewrite statement 
            is wrapped in end of ndb_serialize_cond
          */
          DBUG_PRINT("info", ("EQ_FUNC"));      
          ndb_item= new (*THR_MALLOC) Ndb_item(Item_func::EQ_FUNC, 2);
          break;
        }
        default:
          context->supported= false;
        }
        context->items.push_back(ndb_item);
        ndb_item = nullptr;

        // Handle left hand <field>|<const>
        context->rewrite_stack= NULL; // Disable rewrite mode
        ndb_serialize_cond(rewrite_context->left_hand_item, context);
        context->skip= 0; // Any FUNC_ITEM expression has already been parsed
        context->rewrite_stack= rewrite_context; // Enable rewrite mode
        if (!context->supported)
          DBUG_VOID_RETURN;
      }
      
      // Check for end of AND/OR expression
      if (!item)
      {
        // End marker for condition group
        DBUG_PRINT("info", ("End of condition group"));
        context->expect_no_length();
        ndb_item= new (*THR_MALLOC) Ndb_item(NDB_END_COND);
      }
      else
      {
        bool pop= true;

        switch (item->type()) {
        case Item::FIELD_ITEM:
        {
          Item_field *field_item= (Item_field *) item;
          Field *field= field_item->field;
          const enum_field_types type= field->real_type();

          DBUG_ASSERT(!item->is_bool_func());
          if (!context->m_other_tbls_ok &&
              (item->used_tables() & ~context->table->pos_in_table_list->map()))
          {
            /**
             * 'cond' refers fields from other tables -> reject it.
             */
            context->supported= false;
            break;
          }
          /* Check whether field is computed at MySQL layer */
          if (field->is_virtual_gcol())
          {
            context->supported= false;
            break;
          }

          DBUG_PRINT("info", ("FIELD_ITEM"));
          DBUG_PRINT("info", ("table %s", field->table->alias));
          DBUG_PRINT("info", ("column %s", field->field_name));
          DBUG_PRINT("info", ("column length %u", field->field_length));
          DBUG_PRINT("info", ("type %d", type));
          DBUG_PRINT("info", ("result type %d", field->result_type()));

	  /*
            Check that the field is part of the table of the handler
            instance and that we expect a field of this result type.
          */
          // Check that we are expecting a field and with the correct
          // result type and of length that can store the item value
          if (context->expecting(Item::FIELD_ITEM) &&
              context->expecting_field_type(type) &&
              // Bit fields not yet supported in scan filter
              type != MYSQL_TYPE_BIT &&
              /* Char(0) field is treated as Bit fields inside NDB
                 Hence not supported in scan filter */
              (!(type == MYSQL_TYPE_STRING && field->pack_length() == 0)) &&
              // No BLOB support in scan filter
              type != MYSQL_TYPE_TINY_BLOB &&
              type != MYSQL_TYPE_MEDIUM_BLOB &&
              type != MYSQL_TYPE_LONG_BLOB &&
              type != MYSQL_TYPE_BLOB &&
              type != MYSQL_TYPE_JSON &&
              type != MYSQL_TYPE_GEOMETRY)
          {
            // Found a Field_item of a supported type.

            // A Field_item could either refer 'this' table, or a previous
            // 'other' table in the query plan. Check that we expected the
            // variant we found:
            if (context->table == field->table)    //'this' table
            {
              const NDBCOL *col= context->ndb_table->getColumn(field->field_name);
              DBUG_ASSERT(col);
              ndb_item= new (*THR_MALLOC) Ndb_item(field, col->getColumnNo());

              // Field is a reference to 'this' table, was it expected?
              if (!context->expecting_field_from_table())
              {
                DBUG_PRINT("info", ("Was not expecting field from table %s",
                                    field->table->s->table_name.str));
                context->supported= false;
                break;
              }
            }
            else
            {
              // Is a field reference to another table
              ndb_item= new (*THR_MALLOC) Ndb_item(field);

              if (context->expecting_only_field_from_table())
              {
                // Have already seen a const, or a Field value from another
                // table. We only accept a field from 'this' table now.
                DBUG_PRINT("info", ("Was not expecting a field from another table %s",
                                    field->table->s->table_name.str));
                context->supported= false;
                break;
              }
	    }

	    /*
              Check, or set, further expectations for the operand(s).
              For an operation taking multiple operands, the first operand
              sets the requirement for the next to be compatible.
              'expecting_*_field_result' is used to check if this is the
              first operand or not: If there are no 'field_result' expectations
              set yet, this is the first operand, and it is used to set expectations
              for the next one(s).
            */
            if (!context->expecting_no_field_result())
            {
              // Have some result type expectations to check.
              // Note that STRING and INT(Year) are always allowed
              // to be used together with temporal data types.
              if (!(context->expecting_field_result(field->result_type()) ||
                  // Date and year can be written as string or int
		  (is_supported_temporal_type(type) &&
                    (context->expecting_field_result(STRING_RESULT) ||
                     context->expecting_field_result(INT_RESULT)))))
              {
                DBUG_PRINT("info", ("Was not expecting field of result_type %u(%u)",
                                    field->result_type(), type));
                context->supported= false;
                break;
              }

              // STRING has to be checked for correct 'length' and
              // collation, except if it is used as a temporal data type.
              if (field->result_type() == STRING_RESULT &&
	          !is_supported_temporal_type(type))
              {
                if (!context->expecting_max_length(field->field_length) ||
	            !context->expecting_length(item->max_length))
                {
                  DBUG_PRINT("info", ("Found non-matching string length %s",
                                      field->field_name));
                  context->supported= false;
                  break;
                }
                // Check that field and string constant collations are the same
                if (!context->expecting_collation(item->collation.collation))
                {
                  DBUG_PRINT("info", ("Found non-matching collation %s",
                                      item->collation.collation->name));
                  context->supported= false;
                  break;
                }
              }
              // Seen expected arguments, expect another logical expression
              context->expect_only(Item::FUNC_ITEM);
              context->expect(Item::COND_ITEM);
            }
            else  //is not 'expecting_field_result'
            {
              // This is the first operand, it decides expectations for
              // the next operand, required to be compatible with this one.
              if (context->table == field->table)
              {
                // Dont expect more Fields referring 'this' table
                context->dont_expect_field_from_table();

                if (is_supported_temporal_type(type))
                {
                  context->expect_only(Item::STRING_ITEM);
                  context->expect(Item::INT_ITEM);
                }
                else
                {
                  switch (field->result_type()) {
                  case STRING_RESULT:
                    // Expect char string or binary string
                    context->expect_only(Item::STRING_ITEM);
                    context->expect(Item::VARBIN_ITEM);
                    context->expect_collation(field_item->collation.collation);
                    context->expect_max_length(field->field_length);
                    break;
                  case REAL_RESULT:
                    context->expect_only(Item::REAL_ITEM);
                    context->expect(Item::DECIMAL_ITEM);
                    context->expect(Item::INT_ITEM);
                    break;
                  case INT_RESULT:
                    context->expect_only(Item::INT_ITEM);
                    context->expect(Item::VARBIN_ITEM);
                    break;
                  case DECIMAL_RESULT:
                    context->expect_only(Item::DECIMAL_ITEM);
                    context->expect(Item::REAL_ITEM);
                    context->expect(Item::INT_ITEM);
                    break;
                  default:
                    break;
                  }
                }
                // In addition, second argument can always be a FIELD_ITEM
                // referring a previous table in the query plan.
                context->expect(Item::FIELD_ITEM);
              }
              else  //is an 'other' table (context->table != field->table)
              {		
                // We have not seen the field argument referring this table yet
                // Expect it to refer a Field of same type as 'this' Field
                context->expect_only_field_from_table();
                if (field->result_type() == STRING_RESULT)
                {
                  context->expect_collation(field_item->collation.collation);
                  context->expect_length(item->max_length);
                }
              }
              context->expect_only_field_type(type);
              context->expect_only_field_result(field->result_type());	
            }
          }
          else
          {
            DBUG_PRINT("info", ("Was not expecting field of type %u(%u)",
                                field->result_type(), type));
            context->supported= false;
          }
          break;
        }
        case Item::FUNC_ITEM:
        {
          Item_func *func_item= (Item_func *) item;
          // Check that we expect a function or functional expression here
          if (context->expecting(Item::FUNC_ITEM) ||
              func_item->functype() == Item_func::UNKNOWN_FUNC ||
              func_item->functype() == Item_func::DATE_FUNC ||
              func_item->functype() == Item_func::DATETIME_LITERAL ||
              func_item->functype() == Item_func::NEG_FUNC)
            context->expect_nothing();
          else
          {
            // Did not expect function here
            context->supported= false;
            break;
          }
          context->expect_no_length();
          switch (func_item->functype()) {
          case Item_func::EQ_FUNC:
          {
            DBUG_PRINT("info", ("EQ_FUNC"));      
            ndb_item=
              new (*THR_MALLOC) Ndb_item(func_item->functype(), func_item);
            context->expect(Item::STRING_ITEM);
            context->expect(Item::INT_ITEM);
            context->expect(Item::REAL_ITEM);
            context->expect(Item::DECIMAL_ITEM);
            context->expect(Item::VARBIN_ITEM);
            context->expect_field_from_table();
            context->expect_no_field_result();
            break;
          }
          case Item_func::NE_FUNC:
          {
            DBUG_PRINT("info", ("NE_FUNC"));      
            ndb_item=
              new (*THR_MALLOC) Ndb_item(func_item->functype(), func_item);      
            context->expect(Item::STRING_ITEM);
            context->expect(Item::INT_ITEM);
            context->expect(Item::REAL_ITEM);
            context->expect(Item::DECIMAL_ITEM);
            context->expect(Item::VARBIN_ITEM);
            context->expect_field_from_table();
            context->expect_no_field_result();
            break;
          }
          case Item_func::LT_FUNC:
          {
            DBUG_PRINT("info", ("LT_FUNC"));      
            ndb_item=
              new (*THR_MALLOC) Ndb_item(func_item->functype(), func_item);      
            context->expect(Item::STRING_ITEM);
            context->expect(Item::INT_ITEM);
            context->expect(Item::REAL_ITEM);
            context->expect(Item::DECIMAL_ITEM);
            context->expect(Item::VARBIN_ITEM);
            context->expect_field_from_table();
            context->expect_no_field_result();
            // Enum can only be compared by equality.
            context->dont_expect_field_type(MYSQL_TYPE_ENUM);
            break;
          }
          case Item_func::LE_FUNC:
          {
            DBUG_PRINT("info", ("LE_FUNC"));      
            ndb_item=
              new (*THR_MALLOC) Ndb_item(func_item->functype(), func_item);      
            context->expect(Item::STRING_ITEM);
            context->expect(Item::INT_ITEM);
            context->expect(Item::REAL_ITEM);
            context->expect(Item::DECIMAL_ITEM);
            context->expect(Item::VARBIN_ITEM);
            context->expect_field_from_table();
            context->expect_no_field_result();
            // Enum can only be compared by equality.
            context->dont_expect_field_type(MYSQL_TYPE_ENUM);
            break;
          }
          case Item_func::GE_FUNC:
          {
            DBUG_PRINT("info", ("GE_FUNC"));      
            ndb_item=
              new (*THR_MALLOC) Ndb_item(func_item->functype(), func_item);      
            context->expect(Item::STRING_ITEM);
            context->expect(Item::INT_ITEM);
            context->expect(Item::REAL_ITEM);
            context->expect(Item::DECIMAL_ITEM);
            context->expect(Item::VARBIN_ITEM);
            context->expect_field_from_table();
            context->expect_no_field_result();
            // Enum can only be compared by equality.
            context->dont_expect_field_type(MYSQL_TYPE_ENUM);
            break;
          }
          case Item_func::GT_FUNC:
          {
            DBUG_PRINT("info", ("GT_FUNC"));      
            ndb_item=
              new (*THR_MALLOC) Ndb_item(func_item->functype(), func_item);      
            context->expect(Item::STRING_ITEM);
            context->expect(Item::REAL_ITEM);
            context->expect(Item::DECIMAL_ITEM);
            context->expect(Item::INT_ITEM);
            context->expect(Item::VARBIN_ITEM);
            context->expect_field_from_table();
            context->expect_no_field_result();
            // Enum can only be compared by equality.
            context->dont_expect_field_type(MYSQL_TYPE_ENUM);
            break;
          }
          case Item_func::LIKE_FUNC:
          {
            Ndb_expect_stack* expect_next= new (*THR_MALLOC) Ndb_expect_stack();
            DBUG_PRINT("info", ("LIKE_FUNC"));

            if (((const Item_func_like *)func_item)->escape_was_used_in_parsing())
            {
              DBUG_PRINT("info", ("LIKE expressions with ESCAPE not supported"));
              context->supported= false;
            }
            ndb_item=
              new (*THR_MALLOC) Ndb_item(func_item->functype(), func_item);

            /*
              Ndb currently only supports pushing
              <field> LIKE <string> | <func>
              we thus push <string> | <func>
              on the expect stack to catch that we
              don't support <string> LIKE <field>.
             */
            context->expect_field_from_table();
            context->expect_only_field_type(MYSQL_TYPE_STRING);
            context->expect_field_type(MYSQL_TYPE_VAR_STRING);
            context->expect_field_type(MYSQL_TYPE_VARCHAR);
            context->expect_field_result(STRING_RESULT);
            expect_next->expect(Item::STRING_ITEM);
            expect_next->expect(Item::FUNC_ITEM);
            context->expect_stack.push(expect_next);
            pop= false;
            break;
          }
          case Item_func::ISNULL_FUNC:
          {
            DBUG_PRINT("info", ("ISNULL_FUNC"));      
            ndb_item=
              new (*THR_MALLOC) Ndb_item(func_item->functype(), func_item);      
            context->expect_field_from_table();
            context->expect_field_result(STRING_RESULT);
            context->expect_field_result(REAL_RESULT);
            context->expect_field_result(INT_RESULT);
            context->expect_field_result(DECIMAL_RESULT);
            break;
          }
          case Item_func::ISNOTNULL_FUNC:
          {
            DBUG_PRINT("info", ("ISNOTNULL_FUNC"));      
            ndb_item=
              new (*THR_MALLOC) Ndb_item(func_item->functype(), func_item);     
            context->expect_field_from_table();
            context->expect_field_result(STRING_RESULT);
            context->expect_field_result(REAL_RESULT);
            context->expect_field_result(INT_RESULT);
            context->expect_field_result(DECIMAL_RESULT);
            break;
          }
          case Item_func::NOT_FUNC:
          {
            DBUG_PRINT("info", ("NOT_FUNC"));      
            ndb_item=
              new (*THR_MALLOC) Ndb_item(func_item->functype(), func_item);     
            context->expect(Item::FUNC_ITEM);
            context->expect(Item::COND_ITEM);
            break;
          }
          case Item_func::BETWEEN:
          {
            DBUG_PRINT("info", ("BETWEEN, rewriting using AND"));
            Item_func_between *between_func= (Item_func_between *) func_item;
            Ndb_rewrite_context *rewrite_context= 
              new (*THR_MALLOC) Ndb_rewrite_context(func_item);
            rewrite_context->next= context->rewrite_stack;
            context->rewrite_stack= rewrite_context;
            if (between_func->negated)
            {
              DBUG_PRINT("info", ("NOT_FUNC"));
              context->items.push_back(
                new (*THR_MALLOC) Ndb_item(Item_func::NOT_FUNC, 1));
            }
            DBUG_PRINT("info", ("COND_AND_FUNC"));
            ndb_item=
              new (*THR_MALLOC) Ndb_item(Item_func::COND_AND_FUNC, 
                           func_item->argument_count() - 1);
            context->expect_field_from_table();
            context->expect(Item::INT_ITEM);
            context->expect(Item::STRING_ITEM);
            context->expect(Item::VARBIN_ITEM);
            context->expect(Item::FUNC_ITEM);
            context->expect(Item::CACHE_ITEM);
            break;
          }
          case Item_func::IN_FUNC:
          {
            DBUG_PRINT("info", ("IN_FUNC, rewriting using OR"));
            Item_func_in *in_func= (Item_func_in *) func_item;
            Ndb_rewrite_context *rewrite_context= 
              new (*THR_MALLOC) Ndb_rewrite_context(func_item);
            rewrite_context->next= context->rewrite_stack;
            context->rewrite_stack= rewrite_context;
            if (in_func->negated)
            {
              DBUG_PRINT("info", ("NOT_FUNC"));
              context->items.push_back(
                new (*THR_MALLOC) Ndb_item(Item_func::NOT_FUNC, 1));
            }
            DBUG_PRINT("info", ("COND_OR_FUNC"));
            ndb_item= new (*THR_MALLOC) Ndb_item(
              Item_func::COND_OR_FUNC, func_item->argument_count() - 1);
            context->expect_field_from_table();
            context->expect(Item::INT_ITEM);
            context->expect(Item::STRING_ITEM);
            context->expect(Item::VARBIN_ITEM);
            context->expect(Item::FUNC_ITEM);
            context->expect(Item::CACHE_ITEM);
            break;
          }
          case Item_func::NEG_FUNC:
          case Item_func::DATE_FUNC:
          case Item_func::DATETIME_LITERAL:
          case Item_func::UNKNOWN_FUNC:
          {
            /*
              Constant expressions of the type
              -17, 1+2, concat(0xBB, '%') will
              be evaluated before pushed.
             */
            DBUG_PRINT("info", ("Function %s", 
                                func_item->const_item()?"const":""));  
            DBUG_PRINT("info", ("result type %d", func_item->result_type()));
            /*
              Check if we are rewriting queries of the type
              <const> BETWEEN|IN <func> ...
              as this is currently not supported.
             */
            if (context->rewrite_stack &&
                context->rewrite_stack->left_hand_item &&
                context->rewrite_stack->left_hand_item->type()
                != Item::FIELD_ITEM)
            {
              DBUG_PRINT("info", ("Function during rewrite not supported"));
              context->supported= false;
            }
            if (func_item->const_item())
            {
              switch (func_item->result_type()) {
              case STRING_RESULT:
              {
                ndb_item= new (*THR_MALLOC) Ndb_item(item, Item::STRING_ITEM);
                if (context->expecting_no_field_result())
                {
                  // We have not seen the field argument yet
                  context->expect_only_field_from_table();
                  context->expect_only_field_result(STRING_RESULT);
                  context->expect_collation(func_item->collation.collation);
                }
                else
                {
                  // Expect another logical expression
                  context->expect_only(Item::FUNC_ITEM);
                  context->expect(Item::COND_ITEM);
                  // Check that string result have correct collation
                  if (!context->expecting_collation(item->collation.collation))
                  {
                    DBUG_PRINT("info", ("Found non-matching collation %s",  
                                        item->collation.collation->name));
                    context->supported= false;
                  }
                }
                // Skip any arguments since we will evaluate function instead
                DBUG_PRINT("info", ("Skip until end of arguments marker"));
                context->skip= func_item->argument_count();
                break;
              }
              case REAL_RESULT:
              {
                ndb_item= new (*THR_MALLOC) Ndb_item(item, Item::REAL_ITEM);
                if (context->expecting_no_field_result())
                {
                  // We have not seen the field argument yet
                  context->expect_only_field_from_table();
                  context->expect_only_field_result(REAL_RESULT);
                }
                else
                {
                  // Expect another logical expression
                  context->expect_only(Item::FUNC_ITEM);
                  context->expect(Item::COND_ITEM);
                }
                
                // Skip any arguments since we will evaluate function instead
                DBUG_PRINT("info", ("Skip until end of arguments marker"));
                context->skip= func_item->argument_count();
                break;
              }
              case INT_RESULT:
              {
                ndb_item= new (*THR_MALLOC) Ndb_item(item, Item::INT_ITEM);
                if (context->expecting_no_field_result())
                {
                  // We have not seen the field argument yet
                  context->expect_only_field_from_table();
                  context->expect_only_field_result(INT_RESULT);
                }
                else
                {
                  // Expect another logical expression
                  context->expect_only(Item::FUNC_ITEM);
                  context->expect(Item::COND_ITEM);
                }
                
                // Skip any arguments since we will evaluate function instead
                DBUG_PRINT("info", ("Skip until end of arguments marker"));
                context->skip= func_item->argument_count();
                break;
              }
              case DECIMAL_RESULT:
              {
                ndb_item= new (*THR_MALLOC) Ndb_item(item, Item::DECIMAL_ITEM);
                if (context->expecting_no_field_result())
                {
                  // We have not seen the field argument yet
                  context->expect_only_field_from_table();
                  context->expect_only_field_result(DECIMAL_RESULT);
                }
                else
                {
                  // Expect another logical expression
                  context->expect_only(Item::FUNC_ITEM);
                  context->expect(Item::COND_ITEM);
                }
                // Skip any arguments since we will evaluate function instead
                DBUG_PRINT("info", ("Skip until end of arguments marker"));
                context->skip= func_item->argument_count();
                break;
              }
              default:
                DBUG_ASSERT(false);
                context->supported= false;
                break;
              }
            }
            else
              // Function does not return constant expression
              context->supported= false;
            break;
          }
          default:
          {
            DBUG_PRINT("info", ("Found func_item of type %d", 
                                func_item->functype()));
            context->supported= false;
          }
          }
          break;
        }
        case Item::STRING_ITEM:
          DBUG_PRINT("info", ("STRING_ITEM")); 
          // Check that we do support pushing the item value length
          if (context->expecting(Item::STRING_ITEM) &&
              context->expecting_length(item->max_length)) 
          {
#ifndef DBUG_OFF
            char buff[256];
            String str(buff, 0, system_charset_info);
            const_cast<Item*>(item)->print(current_thd, &str, QT_ORDINARY);
            DBUG_PRINT("info", ("value: '%s'", str.c_ptr_safe()));
#endif
            ndb_item= new (*THR_MALLOC) Ndb_item(item, Item::STRING_ITEM);
            if (context->expecting_no_field_result())
            {
              // We have not seen the field argument yet
              context->expect_only_field_from_table();
              context->expect_only_field_result(STRING_RESULT);
              context->expect_collation(item->collation.collation);
              context->expect_length(item->max_length);
            }
            else 
            {
              // Expect another logical expression
              context->expect_only(Item::FUNC_ITEM);
              context->expect(Item::COND_ITEM);
              context->expect_no_length();
              // Check that we are comparing with a field with same collation
              if (!context->expecting_collation(item->collation.collation))
              {
                DBUG_PRINT("info", ("Found non-matching collation %s",  
                                    item->collation.collation->name));
                context->supported= false;
              }
            }
          }
          else
            context->supported= false;
          break;
        case Item::INT_ITEM:
          DBUG_PRINT("info", ("INT_ITEM"));
          if (context->expecting(Item::INT_ITEM)) 
          {
            DBUG_PRINT("info", ("value %ld",
                                (long) ((Item_int*) item)->value));
            ndb_item= new (*THR_MALLOC) Ndb_item(item, Item::INT_ITEM);
            if (context->expecting_no_field_result())
            {
              // We have not seen the field argument yet
              context->expect_only_field_from_table();
              context->expect_only_field_result(INT_RESULT);
              context->expect_field_result(REAL_RESULT);
              context->expect_field_result(DECIMAL_RESULT);
            }
            else
            {
              // Expect another logical expression
              context->expect_only(Item::FUNC_ITEM);
              context->expect(Item::COND_ITEM);
            }
          }
          else
            context->supported= false;
          break;
        case Item::REAL_ITEM:
          DBUG_PRINT("info", ("REAL_ITEM"));
          if (context->expecting(Item::REAL_ITEM)) 
          {
            DBUG_PRINT("info", ("value %f", ((Item_float*) item)->value));
            ndb_item= new (*THR_MALLOC) Ndb_item(item, Item::REAL_ITEM);
            if (context->expecting_no_field_result())
            {
              // We have not seen the field argument yet
              context->expect_only_field_from_table();
              context->expect_only_field_result(REAL_RESULT);
            }
            else
            {
              // Expect another logical expression
              context->expect_only(Item::FUNC_ITEM);
              context->expect(Item::COND_ITEM);
            }
          }
          else
            context->supported= false;
          break;
        case Item::VARBIN_ITEM:
          DBUG_PRINT("info", ("VARBIN_ITEM"));
          if (context->expecting(Item::VARBIN_ITEM)) 
          {
            ndb_item= new (*THR_MALLOC) Ndb_item(item, Item::VARBIN_ITEM);
            if (context->expecting_no_field_result())
            {
              // We have not seen the field argument yet
              context->expect_only_field_from_table();
              context->expect_only_field_result(STRING_RESULT);
            }
            else
            {
              // Expect another logical expression
              context->expect_only(Item::FUNC_ITEM);
              context->expect(Item::COND_ITEM);
            }
          }
          else
            context->supported= false;
          break;
        case Item::DECIMAL_ITEM:
          DBUG_PRINT("info", ("DECIMAL_ITEM"));
          if (context->expecting(Item::DECIMAL_ITEM)) 
          {
            DBUG_PRINT("info", ("value %f",
                                ((Item_decimal*) item)->val_real()));
            ndb_item= new (*THR_MALLOC) Ndb_item(item, Item::DECIMAL_ITEM);
            if (context->expecting_no_field_result())
            {
              // We have not seen the field argument yet
              context->expect_only_field_from_table();
              context->expect_only_field_result(REAL_RESULT);
              context->expect_field_result(DECIMAL_RESULT);
            }
            else
            {
              // Expect another logical expression
              context->expect_only(Item::FUNC_ITEM);
              context->expect(Item::COND_ITEM);
            }
          }
          else
            context->supported= false;
          break;
        case Item::COND_ITEM:
        {
          Item_cond *cond_item= (Item_cond *) item;
          if (context->expecting(Item::COND_ITEM))
          {
            switch (cond_item->functype()) {
            case Item_func::COND_AND_FUNC:
              DBUG_PRINT("info", ("COND_AND_FUNC"));
              ndb_item= new (*THR_MALLOC) Ndb_item(cond_item->functype(),
                                                cond_item);      
              break;
            case Item_func::COND_OR_FUNC:
              DBUG_PRINT("info", ("COND_OR_FUNC"));
              ndb_item= new (*THR_MALLOC) Ndb_item(cond_item->functype(),
                                                cond_item);      
              break;
            default:
              DBUG_PRINT("info", ("COND_ITEM %d", cond_item->functype()));
              context->supported= false;
              break;
            }
          }
          else
          {
            /* Did not expect condition */
            context->supported= false;
          }
          break;
        }
        case Item::CACHE_ITEM:
        {
          DBUG_PRINT("info", ("CACHE_ITEM"));
          Item_cache* cache_item = (Item_cache*)item;
          DBUG_PRINT("info", ("result type %d", cache_item->result_type()));

          // Item_cache has cached "something", use its value
          // based on the result_type of the item
          switch(cache_item->result_type())
          {
          case INT_RESULT:
            DBUG_PRINT("info", ("INT_RESULT"));
            if (context->expecting(Item::INT_ITEM)) 
            {
              DBUG_PRINT("info", ("value %ld",
                                  (long) ((Item_int*) item)->value));
              ndb_item= new (*THR_MALLOC) Ndb_item(item, Item::INT_ITEM);
              if (context->expecting_no_field_result())
              {
                // We have not seen the field argument yet
                context->expect_only_field_from_table();
                context->expect_only_field_result(INT_RESULT);
                context->expect_field_result(REAL_RESULT);
                context->expect_field_result(DECIMAL_RESULT);
              }
              else
              {
                // Expect another logical expression
                context->expect_only(Item::FUNC_ITEM);
                context->expect(Item::COND_ITEM);
              }
            }
            else
              context->supported= false;
            break;

          case REAL_RESULT:
            DBUG_PRINT("info", ("REAL_RESULT"));
            if (context->expecting(Item::REAL_ITEM)) 
            {
              DBUG_PRINT("info", ("value %f", ((Item_float*) item)->value));
              ndb_item= new (*THR_MALLOC) Ndb_item(item, Item::REAL_ITEM);
              if (context->expecting_no_field_result())
              {
                // We have not seen the field argument yet
                context->expect_only_field_from_table();
                context->expect_only_field_result(REAL_RESULT);
              }
              else
              {
                // Expect another logical expression
                context->expect_only(Item::FUNC_ITEM);
                context->expect(Item::COND_ITEM);
              }
            }
            else
              context->supported= false;
            break;

          case DECIMAL_RESULT:
            DBUG_PRINT("info", ("DECIMAL_RESULT"));
            if (context->expecting(Item::DECIMAL_ITEM)) 
            {
              DBUG_PRINT("info", ("value %f",
                                  ((Item_decimal*) item)->val_real()));
              ndb_item= new (*THR_MALLOC) Ndb_item(item, Item::DECIMAL_ITEM);
              if (context->expecting_no_field_result())
              {
                // We have not seen the field argument yet
                context->expect_only_field_from_table();
                context->expect_only_field_result(REAL_RESULT);
                context->expect_field_result(DECIMAL_RESULT);
              }
              else
              {
                // Expect another logical expression
                context->expect_only(Item::FUNC_ITEM);
                context->expect(Item::COND_ITEM);
              }
            }
            else
              context->supported= false;
            break;

          case STRING_RESULT:
            DBUG_PRINT("info", ("STRING_RESULT")); 
            // Check that we do support pushing the item value length
            if (context->expecting(Item::STRING_ITEM) &&
                context->expecting_length(item->max_length)) 
            {
  #ifndef DBUG_OFF
              char buff[256];
              String str(buff, 0, system_charset_info);
              const_cast<Item*>(item)->print(current_thd, &str, QT_ORDINARY);
              DBUG_PRINT("info", ("value: '%s'", str.c_ptr_safe()));
  #endif
              ndb_item= new (*THR_MALLOC) Ndb_item(item, Item::STRING_ITEM);
              if (context->expecting_no_field_result())
              {
                // We have not seen the field argument yet
                context->expect_only_field_from_table();
                context->expect_only_field_result(STRING_RESULT);
                context->expect_collation(item->collation.collation);
                context->expect_length(item->max_length);
              }
              else 
              {
                // Expect another logical expression
                context->expect_only(Item::FUNC_ITEM);
                context->expect(Item::COND_ITEM);
                context->expect_no_length();
                // Check that we are comparing with a field with same collation
                if (!context->expecting_collation(item->collation.collation))
                {
                  DBUG_PRINT("info", ("Found non-matching collation %s",  
                                      item->collation.collation->name));
                  context->supported= false;
                }
              }
            }
            else
              context->supported= false;
            break;

          default:
            context->supported= false;
            break;
          }
          break;
        }

        default:
        {
          DBUG_PRINT("info", ("Found unsupported item of type %d",
                              item->type()));
          context->supported= false;
        }
        }
        if (pop)
          context->expect_stack.pop();
      }

      if (context->supported)
      {
        DBUG_ASSERT(ndb_item != nullptr);
        context->items.push_back(ndb_item);

        if (context->rewrite_stack)
        {
          Ndb_rewrite_context *rewrite_context= context->rewrite_stack;
          if (rewrite_context->count ==
              rewrite_context->func_item->argument_count())
          {
            // Rewrite is done, wrap an END() at the end
            DBUG_PRINT("info", ("End of condition group"));
            context->expect_no_length();
            context->items.push_back(new (*THR_MALLOC) Ndb_item(NDB_END_COND));
            // Pop rewrite stack
            context->rewrite_stack=  rewrite_context->next;
            rewrite_context->next= NULL;
            destroy(rewrite_context);
          }
        }
      }
    }
  }
  DBUG_VOID_RETURN;
}


ha_ndbcluster_cond::ha_ndbcluster_cond()
  : m_ndb_cond(), m_unpushed_cond(nullptr)
{
}


ha_ndbcluster_cond::~ha_ndbcluster_cond()
{
  m_ndb_cond.destroy_elements();
}


/*
  Clear the condition stack
*/
void
ha_ndbcluster_cond::cond_clear()
{
  DBUG_ENTER("cond_clear");
  m_ndb_cond.destroy_elements();
  m_unpushed_cond = nullptr;
  DBUG_VOID_RETURN;
}


/**
  Construct the AND conjunction of the pushed- and remainder
  predicate terms. If the the original condition was either
  completely pushable, or not pushable at all, it is returned
  instead of constructing new AND conditions.

  @param cond            Original condition we tried to push
  @param pushed_list     A list of predicate terms to be pushed.
  @param remainder_list  A list of predicate terms not pushable.
  @param pushed_cond     Return the resulting pushed condition.
  @param remainder_cond  Return the unpushable part of 'cond'

  @return    '1' in case of failure, else '0'.
 */
static int
create_and_conditions(Item_cond *cond,
                      List<Item> pushed_list, List<Item> remainder_list,
                      Item *&pushed_cond, Item *&remainder_cond)
{
  if (remainder_list.is_empty())
  {
    // Entire cond pushed, no remainder
    pushed_cond = cond;
    remainder_cond = nullptr;
    return 0;
  }
  if (pushed_list.is_empty())
  {
    // Nothing pushed, entire 'cond' is remainder
    pushed_cond = nullptr;
    remainder_cond = cond;
    return 0;
  }

  // Condition was partly pushed, with some remainder
  if (pushed_list.elements == 1)
  {
    // Single boolean term pushed, return it
    pushed_cond = pushed_list.head();
  }
  else
  {
    // Construct an AND'ed condition of pushed boolean terms
    pushed_cond = new Item_cond_and(pushed_list);
    if (unlikely(pushed_cond == nullptr))
      return 1;
    pushed_cond->quick_fix_field();
    pushed_cond->update_used_tables();
  }

  if (remainder_list.elements == 1)
  {
    // A single boolean term as remainder, return it
    remainder_cond = remainder_list.head();
  }
  else
  {
    // Construct a remainder as an AND'ed condition of the boolean terms
    remainder_cond = new Item_cond_and(remainder_list);
    if (unlikely(remainder_cond == nullptr))
      return 1;
    remainder_cond->quick_fix_field();
    remainder_cond->update_used_tables();
  }
  return 0;
}

/**
  Construct the OR conjunction of the pushed- and remainder
  predicate terms.

  Note that the handling of partially pushed OR conditions
  has important differences relative to AND condition:

  1) Something has to be pushed from each term in the
     OR condition (Else the rows matching that term would
     be missing from the result set)

  2) If the OR condition is not completely pushed (there is
     a remainder), the entire original condition has to be
     reevaluated on the server side, or in the AND condition
     containg this OR condition if such exists.

  @param cond            Original condition we tried to push
  @param pushed_list     A list of predicate terms to be pushed.
  @param remainder_list  A list of predicate terms not pushable.
  @param pushed_cond     Return the resulting pushed condition.
  @param remainder_cond  Return the unpushable part of 'cond'

  @return    '1' in case of failure, else '0'.
 */
static int
create_or_conditions(Item_cond *cond,
                     List<Item> pushed_list, List<Item> remainder_list,
                     Item *&pushed_cond, Item *&remainder_cond)
{
  DBUG_ASSERT(pushed_list.elements == cond->argument_list()->elements);

  if (remainder_list.is_empty())
  {
    // Entire cond pushed, no remainder
    pushed_cond = cond;
    remainder_cond = nullptr;
  }
  else
  {
    // When condition was partially pushed, we need to reevaluate
    // original OR-cond on the server side:
    remainder_cond = cond;

    // Construct an OR'ed condition of pushed boolean terms
    pushed_cond = new Item_cond_or(pushed_list);
    if (unlikely(pushed_cond == nullptr))
      return 1;
    pushed_cond->quick_fix_field();
    pushed_cond->update_used_tables();
  }
  return 0;
}

/**
  Decompose a condition into AND'ed 'boolean terms'. Add the terms
  to either the list of 'pushed' or unpushed 'remainder' terms.

  @param term          Condition to try to push down.
  @param table         The Table the conditions may be pushed to.
  @param ndb_table     The Ndb table.
  @param other_tbls_ok Are other tables allowed to be referred
                       from the condition terms pushed down.
  @param pushed_cond   The (part of) the condition term we may push
                       down to the ndbcluster storage engine.
  @param remainder     The remainder (part of) the condition term
                       still needed to be evaluated by the server.

  @return a List of Ndb_item objects representing the serialized
          form of the 'pushed_cond'.
 */
static List<Ndb_item>
cond_push_boolean_term(Item *term,
		       TABLE *table, const NDBTAB *ndb_table,
                       bool other_tbls_ok,
                       Item *&pushed_cond, Item *&remainder_cond)

{
  DBUG_ENTER("ha_ndbcluster::cond_push_boolean_term");
  static const List<Ndb_item> empty_list;

  if (term->type() == Item::COND_ITEM)
  {
    // Build lists of the boolean terms either 'pushed', or being a 'remainder'
    List<Item> pushed_list;
    List<Item> remainder_list;
    List<Ndb_item> code;

    Item_cond *cond = (Item_cond *) term;
    if (cond->functype() == Item_func::COND_AND_FUNC)
    {
      DBUG_PRINT("info", ("COND_AND_FUNC"));

      List_iterator<Item> li(*cond->argument_list());
      Item *boolean_term;
      while ((boolean_term = li++))
      {
        Item *pushed = nullptr, *remainder = nullptr;
        List<Ndb_item> code_stub =
          cond_push_boolean_term(boolean_term, table, ndb_table,
				 other_tbls_ok, pushed, remainder);

        // Collect all bits we pushed, and its leftovers.
        if (!code_stub.is_empty())
          code.concat(&code_stub);
        if (pushed != nullptr)
          pushed_list.push_back(pushed);
        if (remainder != nullptr)
          remainder_list.push_back(remainder);
      }

      // Transform the list of pushed and the remainder conditions
      // into its respective AND'ed conditions.
      if (create_and_conditions(cond, pushed_list, remainder_list,
                                pushed_cond, remainder_cond))
      {
        //Failed, discard pushed conditions and generated code.
        pushed_cond = nullptr;
        remainder_cond = cond;
        code.destroy_elements();
        DBUG_RETURN(empty_list);
      }
      // Serialized code has to be embedded in an AND-group
      if (!code.is_empty())
      {
        code.push_front(new (*THR_MALLOC) Ndb_item(Item_func::COND_AND_FUNC, cond));
        code.push_back (new (*THR_MALLOC) Ndb_item(NDB_END_COND));
      }
    }
    else
    {
      DBUG_ASSERT(cond->functype() == Item_func::COND_OR_FUNC);
      DBUG_PRINT("info", ("COND_OR_FUNC"));

      List_iterator<Item> li(*cond->argument_list());
      Item *boolean_term;
      while ((boolean_term = li++))
      {
        Item *pushed = nullptr, *remainder = nullptr;
        List<Ndb_item> code_stub =
          cond_push_boolean_term(boolean_term, table, ndb_table,
				 other_tbls_ok, pushed, remainder);

        if (pushed == nullptr)
        {
          //Failure of pushing one of the OR-terms fails entire OR'ed cond
          //(Else the rows matching that term would be missing in result set)
          // Also see comments in create_or_conditions().
          pushed_cond = nullptr;
          remainder_cond = cond;
          code.destroy_elements();
          DBUG_RETURN(empty_list);
        }
	
        // Collect all bits we pushed, and its leftovers.
        if (!code_stub.is_empty())
          code.concat(&code_stub);
        if (pushed != nullptr)
          pushed_list.push_back(pushed);
        if (remainder != nullptr)
          remainder_list.push_back(remainder);
      }

      // Transform the list of pushed and the remainder conditions
      // into its respective OR'ed conditions.
      if (create_or_conditions(cond, pushed_list, remainder_list,
                               pushed_cond, remainder_cond))
      {
        //Failed, discard pushed conditions and generated code.
        pushed_cond = nullptr;
        remainder_cond = cond;
        code.destroy_elements();
        DBUG_RETURN(empty_list);
      }
      // Serialized code has to be embedded in an OR-group
      if (!code.is_empty())
      {
        code.push_front(new (*THR_MALLOC) Ndb_item(Item_func::COND_OR_FUNC, cond));
        code.push_back (new (*THR_MALLOC) Ndb_item(NDB_END_COND));
      }
    }
    DBUG_RETURN(code);
  }

  // Has broken down the condition into predicate terms, or sub conditions,
  // which either has to be accepted or rejected for pushdown
  Ndb_cond_traverse_context context(table, ndb_table, other_tbls_ok);
  context.expect(Item::FUNC_ITEM);
  context.expect(Item::COND_ITEM);
  term->traverse_cond(&ndb_serialize_cond, &context, Item::PREFIX);
  
  if (context.supported)
  {
    pushed_cond = term;
    remainder_cond = nullptr;
    DBUG_ASSERT(!context.items.is_empty());
    DBUG_RETURN(context.items);
  }
  else  // Failed to push
  {
    pushed_cond = nullptr;
    remainder_cond = term;
    context.items.destroy_elements();
    DBUG_RETURN(empty_list);   //Discard any generated Ndb_cond's
  }
}


/*
  Push a condition, return any remainder condition
 */
const Item*
ha_ndbcluster_cond::cond_push(const Item *cond,
                              TABLE *table, const NDBTAB *ndb_table,
                              bool other_tbls_ok,
                              Item *&pushed_cond)
{
  DBUG_ENTER("ha_ndbcluster_cond::cond_push");

  // Build lists of the boolean terms either 'pushed', or being a 'remainder'
  Item *item= const_cast<Item*>(cond);
  Item *remainder = nullptr;
  List<Ndb_item> code =
    cond_push_boolean_term(item, table, ndb_table, other_tbls_ok, pushed_cond, remainder);

  // Save the serialized representation of the code
  m_ndb_cond = code;
  DBUG_RETURN(remainder);
}


int
ha_ndbcluster_cond::build_scan_filter_predicate(List_iterator<Ndb_item> &cond,
                                                NdbScanFilter *filter,
                                                bool negated) const
{
  DBUG_ENTER("build_scan_filter_predicate");  
  Ndb_item *ndb_item = *cond.ref();
  switch (ndb_item->type) {
  case NDB_FUNCTION:
  {
    Ndb_item *b, *field, *value= nullptr;
    Ndb_item *a = cond++;
    if (a == nullptr)
      break;

    enum ndb_func_type function_type = (negated)
       ? Ndb_item::negate(ndb_item->qualification.function_type)
       : ndb_item->qualification.function_type;

    switch (ndb_item->argument_count()) {
    case 1:
      field= (a->type == NDB_FIELD)? a : NULL;
      break;
    case 2:
      b = cond++;
      if (b == nullptr)
      {
        field= nullptr;
        break;
      }
      value= ((a->type == NDB_VALUE) ? a :
              (b->type == NDB_VALUE) ? b :
              NULL);
      field= ((a->type == NDB_FIELD) ? a :
              (b->type == NDB_FIELD) ? b :
              NULL);
      if (!value)
      {
        DBUG_PRINT("info", ("condition missing 'value' argument"));
        DBUG_RETURN(1);
      }
      if (field == b)
        function_type = Ndb_item::swap(function_type);
      break;
    default:
      DBUG_PRINT("info", ("condition had unexpected number of arguments"));
      DBUG_RETURN(1);
    }
    if (!field)
    {
      DBUG_PRINT("info", ("condition missing 'field' argument"));
      DBUG_RETURN(1);
    }

    /*
      The NdbInterpreter handles a NULL value as being less than any
      non-NULL value. However, MySQL server (and SQL std spec) specifies
      that a NULL-value in a comparison predicate should result in an
      UNKNOWN boolean result, which is 'not TRUE' -> the row being eliminated.

      Thus, extra checks for both 'field' and 'value' being a
      NULL-value has to be added to mitigate this semantic difference.
    */
    if (value != nullptr &&
	const_cast<Item*>(value->get_item())->is_null())
    {
      /*
        'value' known to be a NULL-value.
        Condition will be 'not TRUE' -> false, independent of the 'field'
        value. Encapsulate in own group, as only this predicate become
        'false', not entire group it is part of.
      */
      if (filter->begin() == -1   ||
	  filter->isfalse() == -1 ||
	  filter->end() == -1)
        DBUG_RETURN(1);
      DBUG_RETURN(0);
    }

    bool need_explicit_null_check = field->get_field()->maybe_null();
    if (unlikely(need_explicit_null_check))
    {
      switch (function_type) {
      /*
        The NdbInterpreter handles a NULL value as being less than any
        non-NULL value. Thus any NULL value columns will evaluate to
        'TRUE' (and pass the filter) in the predicate expression:
            <column> </ <= / <> <non-NULL value>

        This is not according to how the server expect NULL valued
        predicates to be evaluated: Any NULL values in a comparison
        predicate should result in an UNKNOWN boolean result
        and the row being eliminated.

        This is mitigated by adding an extra isnotnull-check to
        eliminate NULL valued rows which otherwise would have passed
        the ScanFilter.
      */
      case NDB_NE_FUNC:
      case NDB_LT_FUNC:
      case NDB_LE_FUNC:
      /*
        The NdbInterpreter will also let any null valued columns
        in *both* a LIKE / NOT LIKE predicate pass the filter.
      */
      case NDB_LIKE_FUNC:
      case NDB_NOTLIKE_FUNC:
      {
        DBUG_PRINT("info", ("Appending extra ISNOTNULL filter"));
        if (filter->begin(NdbScanFilter::AND) == -1 ||
	    filter->isnotnull(field->get_field_no()) == -1)
          DBUG_RETURN(1);
	break;
      }
      /*
        As the NULL value is less than any non-NULL values, they are
        correctly eliminated from a '>', '>=' and '=' comparison.
        Thus, no notnull-check needed here.
      */
      case NDB_EQ_FUNC:
      case NDB_GE_FUNC:
      case NDB_GT_FUNC:
      default:
        need_explicit_null_check = false;
        break;
      }
    }

    switch (function_type) {
    case NDB_EQ_FUNC:
    {
      // Save value in right format for the field type
      if (unlikely(value->save_in_field(field) == 0))
        DBUG_RETURN(1);
      DBUG_PRINT("info", ("Generating EQ filter"));
      if (filter->cmp(NdbScanFilter::COND_EQ, 
                      field->get_field_no(),
                      field->get_val(),
                      field->pack_length()) == -1)
        DBUG_RETURN(1);
      break;
    }
    case NDB_NE_FUNC:
    {
      // Save value in right format for the field type
      if (unlikely(value->save_in_field(field) == 0))
        DBUG_RETURN(1);
      DBUG_PRINT("info", ("Generating NE filter"));
      if (filter->cmp(NdbScanFilter::COND_NE, 
                      field->get_field_no(),
                      field->get_val(),
                      field->pack_length()) == -1)
        DBUG_RETURN(1);
      break;
    }
    case NDB_LT_FUNC:
    {
      // Save value in right format for the field type
      if (unlikely(value->save_in_field(field) == 0))
        DBUG_RETURN(1);
      DBUG_PRINT("info", ("Generating LT filter"));
      if (filter->cmp(NdbScanFilter::COND_LT,
                      field->get_field_no(),
                      field->get_val(),
                      field->pack_length()) == -1)
        DBUG_RETURN(1);
      break;
    }
    case NDB_LE_FUNC:
    {
      // Save value in right format for the field type
      if (unlikely(value->save_in_field(field) == 0))
        DBUG_RETURN(1);
      DBUG_PRINT("info", ("Generating LE filter"));
      if (filter->cmp(NdbScanFilter::COND_LE,
                      field->get_field_no(),
                      field->get_val(),
                      field->pack_length()) == -1)
        DBUG_RETURN(1);
      break;
    }
    case NDB_GE_FUNC:
    {
      // Save value in right format for the field type
      if (unlikely(value->save_in_field(field) == 0))
        DBUG_RETURN(1);
      DBUG_PRINT("info", ("Generating GE filter"));
      if (filter->cmp(NdbScanFilter::COND_GE,
                      field->get_field_no(),
                      field->get_val(),
                      field->pack_length()) == -1)
        DBUG_RETURN(1);
      break;
    }
    case NDB_GT_FUNC:
    {
      // Save value in right format for the field type
      if (unlikely(value->save_in_field(field) == 0))
        DBUG_RETURN(1);
      DBUG_PRINT("info", ("Generating GT filter"));
      if (filter->cmp(NdbScanFilter::COND_GT,
                      field->get_field_no(),
                      field->get_val(),
                      field->pack_length()) == -1)
        DBUG_RETURN(1);
      }
      break;
    case NDB_LIKE_FUNC:
    {
      const bool is_string= (value->qualification.value_type == Item::STRING_ITEM);
      // Save value in right format for the field type, allow string truncation
      const uint32 val_len= value->save_in_field(field, true);
      if (unlikely(val_len == 0))
        DBUG_RETURN(1);
      char buff[MAX_FIELD_WIDTH];
      String str(buff,sizeof(buff),field->get_field_charset());
      if (val_len > field->get_field()->field_length)
        str.set(value->get_val(), val_len, field->get_field_charset());
      else
        field->get_field_val_str(&str);
      uint32 len=
        ((value->is_const_func() || value->is_cached()) && is_string)?
        str.length():
        value->pack_length();
      const char *val=
        ((value->is_const_func() || value->is_cached()) && is_string)?
        str.ptr()
        : value->get_val();
      DBUG_PRINT("info", ("Generating LIKE filter: like(%d,%s,%d)", 
                          field->get_field_no(),
                          val,
                          len));
      if (filter->cmp(NdbScanFilter::COND_LIKE, 
                      field->get_field_no(),
                      val,
                      len) == -1)
        DBUG_RETURN(1);
      break;
    }
    case NDB_NOTLIKE_FUNC:
    {
      const bool is_string= (value->qualification.value_type == Item::STRING_ITEM);
      // Save value in right format for the field type, allow string truncation
      const uint32 val_len= value->save_in_field(field, true);
      if (unlikely(val_len == 0))
        DBUG_RETURN(1);
      char buff[MAX_FIELD_WIDTH];
      String str(buff,sizeof(buff),field->get_field_charset());
      if (val_len > field->get_field()->field_length)
        str.set(value->get_val(), val_len, field->get_field_charset());
      else
        field->get_field_val_str(&str);
      uint32 len=
        ((value->is_const_func() || value->is_cached()) && is_string)?
        str.length():
        value->pack_length();
      const char *val=
        ((value->is_const_func() || value->is_cached()) && is_string)?
        str.ptr()
        : value->get_val();
      DBUG_PRINT("info", ("Generating NOTLIKE filter: notlike(%d,%s,%d)", 
                          field->get_field_no(),
                          (value->pack_length() > len)?value->get_val():val,
                          (value->pack_length() > len)?value->pack_length():len));
      if (filter->cmp(NdbScanFilter::COND_NOT_LIKE, 
                      field->get_field_no(),
                      (value->pack_length() > len)?value->get_val():val,
                      (value->pack_length() > len)?value->pack_length():len) == -1)
        DBUG_RETURN(1);
      break;
    }
    case NDB_ISNULL_FUNC:
      DBUG_PRINT("info", ("Generating ISNULL filter"));
      if (filter->isnull(field->get_field_no()) == -1)
        DBUG_RETURN(1);
      break;
    case NDB_ISNOTNULL_FUNC:
    {
      DBUG_PRINT("info", ("Generating ISNOTNULL filter"));
      if (filter->isnotnull(field->get_field_no()) == -1)
        DBUG_RETURN(1);         
      break;
    }
    default:
      DBUG_ASSERT(false);
      break;
    }
    if (need_explicit_null_check && filter->end() == -1) //Local AND group
      DBUG_RETURN(1);
    DBUG_RETURN(0);
  }
  default:
    break;
  }
  DBUG_PRINT("info", ("Found illegal condition"));
  DBUG_RETURN(1);
}


int
ha_ndbcluster_cond::build_scan_filter_group(List_iterator<Ndb_item> &cond,
                                            NdbScanFilter *filter,
                                            const bool negated) const
{
  uint level=0;
  DBUG_ENTER("build_scan_filter_group");

  do
  {
    const Ndb_item *ndb_item = cond++;
    if (ndb_item == nullptr)
      DBUG_RETURN(1);
    switch (ndb_item->type) {
    case NDB_FUNCTION:
    {
      switch (ndb_item->qualification.function_type) {
      case NDB_COND_AND_FUNC:
      {
        level++;
        DBUG_PRINT("info", ("Generating %s group %u", (negated)?"OR":"AND",
                            level));
        if ((negated) ? filter->begin(NdbScanFilter::OR)
            : filter->begin(NdbScanFilter::AND) == -1)
          DBUG_RETURN(1);
        break;
      }
      case NDB_COND_OR_FUNC:
      {
        level++;
        DBUG_PRINT("info", ("Generating %s group %u", (negated)?"AND":"OR",
                            level));
        if ((negated) ? filter->begin(NdbScanFilter::AND)
            : filter->begin(NdbScanFilter::OR) == -1)
          DBUG_RETURN(1);
        break;
      }
      case NDB_NOT_FUNC:
      {
        DBUG_PRINT("info", ("Generating negated query"));
        if (build_scan_filter_group(cond, filter, !negated))
          DBUG_RETURN(1);
        break;
      }
      default:
        if (build_scan_filter_predicate(cond, filter, negated))
          DBUG_RETURN(1);
        break;
      }
      break;
    }
    case NDB_END_COND:
      DBUG_PRINT("info", ("End of group %u", level));
      level--;
      if (filter->end() == -1)
        DBUG_RETURN(1);
      break;
    default:
    {
      DBUG_PRINT("info", ("Illegal scan filter"));
      DBUG_ASSERT(false);
      DBUG_RETURN(1);
    }
    }
  }  while (level > 0);
  
  DBUG_RETURN(0);
}


int
ha_ndbcluster_cond::generate_scan_filter_from_cond(NdbScanFilter& filter)
{
  bool need_group = true;
  DBUG_ENTER("generate_scan_filter_from_cond");

  // Determine if we need to wrap an AND group around condition(s)
  Ndb_item *ndb_item = m_ndb_cond.head();
  if (ndb_item->type == NDB_FUNCTION)
  {
    switch (ndb_item->qualification.function_type) {
    case NDB_COND_AND_FUNC:
    case NDB_COND_OR_FUNC:
      // A single AND/OR condition has its own AND/OR-group
      // .. in all other cases we start a AND group now
      need_group = false;
      break;
    default:
      break;
    }
  }

  if (need_group && filter.begin() == -1)
    DBUG_RETURN(1);

  List_iterator<Ndb_item> cond(m_ndb_cond);
  if (build_scan_filter_group(cond, &filter, false))
  {
    DBUG_PRINT("info", ("build_scan_filter_group failed"));

    const NdbError& err= filter.getNdbError();
    if (err.code == NdbScanFilter::FilterTooLarge)
    {
      DBUG_PRINT("info", ("%s", err.message));
      push_warning(current_thd, Sql_condition::SL_WARNING,
                   err.code, err.message);
    }
    DBUG_RETURN(1);
  }
  if (need_group && filter.end() == -1)
    DBUG_RETURN(1);

  DBUG_RETURN(0);
}


/*
  Optimizer sometimes does hash index lookup of a key where some
  key parts are null.  The set of cases where this happens makes
  no sense but cannot be ignored since optimizer may expect the result
  to be filtered accordingly.  The scan is actually on the table and
  the index bounds are pushed down.
*/
int ha_ndbcluster_cond::generate_scan_filter_from_key(NdbScanFilter &filter,
                                                      const KEY *key_info, 
                                                      const key_range *start_key,
                                                      const key_range *end_key)
{
  DBUG_ENTER("generate_scan_filter_from_key");

#ifndef DBUG_OFF
  {
    DBUG_PRINT("info", ("key parts:%u length:%u",
                        key_info->user_defined_key_parts, key_info->key_length));
    const key_range* keylist[2]={ start_key, end_key };
    for (uint j=0; j <= 1; j++)
    {
      char buf[8192];
      const key_range* key=keylist[j];
      if (key == 0)
      {
        sprintf(buf, "key range %u: none", j);
      }
      else
      {
        sprintf(buf, "key range %u: flag:%u part", j, key->flag);
        const KEY_PART_INFO* key_part=key_info->key_part;
        const uchar* ptr=key->key;
        for (uint i=0; i < key_info->user_defined_key_parts; i++)
        {
          sprintf(buf+strlen(buf), " %u:", i);
          for (uint k=0; k < key_part->store_length; k++)
          {
            sprintf(buf+strlen(buf), " %02x", ptr[k]);
          }
          ptr+=key_part->store_length;
          if (ptr - key->key >= (ptrdiff_t)key->length)
          {
            /*
              key_range has no count of parts so must test byte length.
              But this is not the place for following assert.
            */
            // DBUG_ASSERT(ptr - key->key == key->length);
            break;
          }
          key_part++;
        }
      }
      DBUG_PRINT("info", ("%s", buf));
    }
  }
#endif

  do
  {
    /*
      Case "x is not null".
      Seen with index(x) where it becomes range "null < x".
      Not seen with index(x,y) for any combination of bounds
      which include "is not null".
    */
    if (start_key != 0 &&
        start_key->flag == HA_READ_AFTER_KEY &&
        end_key == 0 &&
        key_info->user_defined_key_parts == 1)
    {
      const KEY_PART_INFO* key_part=key_info->key_part;
      if (key_part->null_bit != 0) // nullable (must be)
      {
        const uchar* ptr= start_key->key;
        if (ptr[0] != 0) // null (in "null < x")
        {
          DBUG_PRINT("info", ("Generating ISNOTNULL filter for nullable %s",
                              key_part->field->field_name));
          if (filter.isnotnull(key_part->fieldnr-1) == -1)
            DBUG_RETURN(1);
          break;
        }
      }
    }

    /*
      Case "x is null" in an EQ range.
      Seen with index(x) for "x is null".
      Seen with index(x,y) for "x is null and y = 1".
      Not seen with index(x,y) for "x is null and y is null".
      Seen only when all key parts are present (but there is
      no reason to limit the code to this case).
    */
    if (start_key != 0 &&
        start_key->flag == HA_READ_KEY_EXACT &&
        end_key != 0 &&
        end_key->flag == HA_READ_AFTER_KEY &&
        start_key->length == end_key->length &&
        memcmp(start_key->key, end_key->key, start_key->length) == 0)
    {
      const KEY_PART_INFO* key_part=key_info->key_part;
      const uchar* ptr=start_key->key;
      for (uint i=0; i < key_info->user_defined_key_parts; i++)
      {
        const Field* field=key_part->field;
        if (key_part->null_bit) // nullable
        {
          if (ptr[0] != 0) // null
          {
            DBUG_PRINT("info", ("Generating ISNULL filter for nullable %s",
                                field->field_name));
            if (filter.isnull(key_part->fieldnr-1) == -1)
              DBUG_RETURN(1);
          }
          else
          {
            DBUG_PRINT("info", ("Generating EQ filter for nullable %s",
                                field->field_name));
            if (filter.cmp(NdbScanFilter::COND_EQ, 
                           key_part->fieldnr-1,
                           ptr + 1, // skip null-indicator byte
                           field->pack_length()) == -1)
              DBUG_RETURN(1);
          }
        }
        else
        {
          DBUG_PRINT("info", ("Generating EQ filter for non-nullable %s",
                              field->field_name));
          if (filter.cmp(NdbScanFilter::COND_EQ, 
                         key_part->fieldnr-1,
                         ptr,
                         field->pack_length()) == -1)
            DBUG_RETURN(1);
        }
        ptr+=key_part->store_length;
        if (ptr - start_key->key >= (ptrdiff_t)start_key->length)
        {
          break;
        }
        key_part++;
      }
      break;
    }

    DBUG_PRINT("info", ("Unknown hash index scan"));
    // Catch new cases when optimizer changes
    DBUG_ASSERT(false);
  }
  while (0);

  DBUG_RETURN(0);
}

/**
  In case we failed to 'generate' a scan filter accepted by 'cond_push',
  or we later choose to ignore it, set_condition() will set the condition
  to be evaluated by the handler.

  @param cond     The condition to be evaluated by the handler
*/
void
ha_ndbcluster_cond::set_condition(const Item *cond)
{
  m_unpushed_cond = cond;
}

/**
  Return the boolean value of a condition previously set by 'set_condition',
  evaluated on the current row.

  @return    true if the condition is evaluated to true.
*/
bool
ha_ndbcluster_cond::eval_condition() const
{
  return ((Item*)m_unpushed_cond)->val_int()==1;
}


/**
  Add any columns referred by 'cond' to the read_set of the table.

  @param table  The table to update the read_set for.
  @param cond   The condition referring columns in 'table'
*/
void
ha_ndbcluster_cond::add_read_set(TABLE *table, const Item *cond)
{
  if (cond != nullptr)
  {
    Mark_field mf(table, MARK_COLUMNS_READ);
    ((Item*)cond)->walk(&Item::mark_field_in_map, enum_walk::PREFIX,
                            (uchar *)&mf);
  }
}

/*
  Interface layer between ha_ndbcluster and ha_ndbcluster_cond
*/
void
ha_ndbcluster::generate_scan_filter(NdbInterpretedCode *code,
                                    NdbScanOperation::ScanOptions *options)
{
  DBUG_ENTER("generate_scan_filter");

  if (pushed_cond == nullptr)
  {
    DBUG_PRINT("info", ("Empty stack"));
    DBUG_VOID_RETURN;
  }

  NdbScanFilter filter(code);
  const int ret= m_cond.generate_scan_filter_from_cond(filter);
  if (unlikely(ret != 0))
  {
    /**
     * Failed to generate a scan filter, fallback to let
     * ha_ndbcluster evaluate the condition.
     */
    m_cond.set_condition(pushed_cond);
  }
  else if (options != nullptr)
  {
    options->interpretedCode= code;
    options->optionsPresent|= NdbScanOperation::ScanOptions::SO_INTERPRETED;
  }
  DBUG_VOID_RETURN;
}

int ha_ndbcluster::generate_scan_filter_with_key(NdbInterpretedCode *code,
                                                 NdbScanOperation::ScanOptions *options,
                                                 const KEY *key_info,
                                                 const key_range *start_key,
                                                 const key_range *end_key)
{
  DBUG_ENTER("generate_scan_filter_with_key");

  NdbScanFilter filter(code);
  if (filter.begin(NdbScanFilter::AND) == -1)
     DBUG_RETURN(1);

  // Generate a scanFilter from a prepared pushed conditions
  if (pushed_cond != nullptr)
  {
    const int ret = m_cond.generate_scan_filter_from_cond(filter);
    if (unlikely(ret != 0))
    {
      /**
       * Failed to generate a scan filter, fallback to let
       * ha_ndbcluster evaluate the condition.
       */
      m_cond.set_condition(pushed_cond);

      // Discard the failed scanFilter and prepare for 'key'
      filter.reset();
      if (filter.begin(NdbScanFilter::AND) == -1)
        DBUG_RETURN(1);
    }
  }

  // Generate a scanFilter from the key definition
  if (key_info != nullptr)
  {
    const int ret = ha_ndbcluster_cond::generate_scan_filter_from_key(
     filter, key_info, start_key, end_key);
    if (unlikely(ret != 0))
      DBUG_RETURN(ret);
  }

  if (filter.end() == -1)
    DBUG_RETURN(1);

  if (options != nullptr)
  {
    options->interpretedCode= code;
    options->optionsPresent|= NdbScanOperation::ScanOptions::SO_INTERPRETED;
  }

  DBUG_RETURN(0);
}
