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

#include "storage/ndb/plugin/ha_ndbcluster_cond.h"

#include "my_dbug.h"
#include "sql/current_thd.h"
#include "sql/item.h"          // Item
#include "sql/item_cmpfunc.h"  // Item_func_like etc.
#include "sql/item_func.h"     // Item_func
#include "storage/ndb/plugin/ha_ndbcluster.h"
#include "storage/ndb/plugin/ndb_log.h"
#include "storage/ndb/plugin/ndb_thd.h"

// Typedefs for long names
typedef NdbDictionary::Column NDBCOL;
typedef NdbDictionary::Table NDBTAB;

typedef enum ndb_item_type {
  NDB_VALUE = 0,     // Qualified more with Item::Type
  NDB_FIELD = 1,     // Qualified from table definition
  NDB_FUNCTION = 2,  // Qualified from Item_func::Functype
  NDB_END_COND = 3   // End marker for condition group
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
  NDB_COND_AND_FUNC = 11,
  NDB_COND_OR_FUNC = 12,
  NDB_UNSUPPORTED_FUNC = 13
} NDB_FUNC_TYPE;

typedef union ndb_item_value {
  const Item *item;  // NDB_VALUE
  struct {           // NDB_FIELD
    Field *field;
    int column_no;
  };
  struct {  // NDB_FUNCTION
    NDB_FUNC_TYPE func_type;
    uint arg_count;
  };
} NDB_ITEM_VALUE;

/*
  Mapping defining the negated and swapped function equivalent
   - 'not op1 func op2' -> 'op1 neg_func op2'
   - 'op1 func op2' -> ''op2 swap_func op1'
*/
struct function_mapping {
  NDB_FUNC_TYPE func;
  NDB_FUNC_TYPE neg_func;
  NDB_FUNC_TYPE swap_func;
};

/*
  Define what functions can be negated in condition pushdown.
  Note, these HAVE to be in the same order as in definition enum
*/
static const function_mapping func_map[] = {
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
    {NDB_COND_AND_FUNC, NDB_UNSUPPORTED_FUNC, NDB_UNSUPPORTED_FUNC},
    {NDB_COND_OR_FUNC, NDB_UNSUPPORTED_FUNC, NDB_UNSUPPORTED_FUNC},
    {NDB_UNSUPPORTED_FUNC, NDB_UNSUPPORTED_FUNC, NDB_UNSUPPORTED_FUNC}};

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
class Ndb_item {
 public:
  Ndb_item(NDB_ITEM_TYPE item_type) : type(item_type) {}
  // A Ndb_Item where an Item expression defines the value (a const)
  Ndb_item(const Item *item_value) : type(NDB_VALUE) {
    value.item = item_value;
  }
  // A Ndb_Item refering a Field from 'this' table
  Ndb_item(Field *field, int column_no) : type(NDB_FIELD) {
    value.field = field;
    value.column_no = column_no;
  }
  Ndb_item(Item_func::Functype func_type, const Item_func *item_func)
      : type(NDB_FUNCTION) {
    value.func_type = item_func_to_ndb_func(func_type);
    value.arg_count = item_func->argument_count();
  }
  Ndb_item(Item_func::Functype func_type, uint no_args) : type(NDB_FUNCTION) {
    value.func_type = item_func_to_ndb_func(func_type);
    value.arg_count = no_args;
  }
  ~Ndb_item() {}

  Field *get_field() const {
    DBUG_ASSERT(type == NDB_FIELD);
    return value.field;
  }

  int get_field_no() const {
    DBUG_ASSERT(type == NDB_FIELD);
    return value.column_no;
  }

  NDB_FUNC_TYPE get_func_type() const {
    DBUG_ASSERT(type == NDB_FUNCTION);
    return value.func_type;
  }

  int get_argument_count() const {
    DBUG_ASSERT(type == NDB_FUNCTION);
    return value.arg_count;
  }

  uint32 pack_length() const { return get_field()->pack_length(); }

  const uchar *get_val() const { return get_field()->ptr; }

  const CHARSET_INFO *get_field_charset() const {
    const Field *field = get_field();
    if (field) return field->charset();

    return NULL;
  }

  const Item *get_item() const {
    DBUG_ASSERT(this->type == NDB_VALUE);
    return value.item;
  }

  int save_in_field(const Ndb_item *field_item) const {
    DBUG_TRACE;
    Field *field = field_item->get_field();
    const Item *item = get_item();
    if (unlikely(item == nullptr || field == nullptr)) return -1;

    my_bitmap_map *old_map =
        dbug_tmp_use_all_columns(field->table, field->table->write_set);
    const type_conversion_status status =
        const_cast<Item *>(item)->save_in_field(field, false);
    dbug_tmp_restore_column_map(field->table->write_set, old_map);

    if (unlikely(status != TYPE_OK)) return -1;

    return 0;  // OK
  }

  static NDB_FUNC_TYPE item_func_to_ndb_func(Item_func::Functype fun) {
    switch (fun) {
      case (Item_func::EQ_FUNC): {
        return NDB_EQ_FUNC;
      }
      case (Item_func::NE_FUNC): {
        return NDB_NE_FUNC;
      }
      case (Item_func::LT_FUNC): {
        return NDB_LT_FUNC;
      }
      case (Item_func::LE_FUNC): {
        return NDB_LE_FUNC;
      }
      case (Item_func::GT_FUNC): {
        return NDB_GT_FUNC;
      }
      case (Item_func::GE_FUNC): {
        return NDB_GE_FUNC;
      }
      case (Item_func::ISNULL_FUNC): {
        return NDB_ISNULL_FUNC;
      }
      case (Item_func::ISNOTNULL_FUNC): {
        return NDB_ISNOTNULL_FUNC;
      }
      case (Item_func::LIKE_FUNC): {
        return NDB_LIKE_FUNC;
      }
      case (Item_func::NOT_FUNC): {
        return NDB_NOT_FUNC;
      }
      case (Item_func::COND_AND_FUNC): {
        return NDB_COND_AND_FUNC;
      }
      case (Item_func::COND_OR_FUNC): {
        return NDB_COND_OR_FUNC;
      }
      default: {
        return NDB_UNSUPPORTED_FUNC;
      }
    }
  }

  static NDB_FUNC_TYPE negate(NDB_FUNC_TYPE fun) {
    uint i = (uint)fun;
    DBUG_ASSERT(fun == func_map[i].func);
    return func_map[i].neg_func;
  }

  static NDB_FUNC_TYPE swap(NDB_FUNC_TYPE fun) {
    uint i = (uint)fun;
    DBUG_ASSERT(fun == func_map[i].func);
    return func_map[i].swap_func;
  }

  const NDB_ITEM_TYPE type;

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
class Ndb_expect_stack {
  static const uint MAX_EXPECT_ITEMS = Item::VIEW_FIXER_ITEM + 1;
  static const uint MAX_EXPECT_FIELD_TYPES = MYSQL_TYPE_GEOMETRY + 1;
  static const uint MAX_EXPECT_FIELD_RESULTS = DECIMAL_RESULT + 1;

 public:
  Ndb_expect_stack()
      : other_field(nullptr),
        collation(nullptr),
        length(0),
        max_length(0),
        next(nullptr) {
    // Allocate type checking bitmaps using fixed size buffers
    // since max size is known at compile time
    bitmap_init(&expect_mask, m_expect_buf, MAX_EXPECT_ITEMS);
    bitmap_init(&expect_field_type_mask, m_expect_field_type_buf,
                MAX_EXPECT_FIELD_TYPES);
    bitmap_init(&expect_field_result_mask, m_expect_field_result_buf,
                MAX_EXPECT_FIELD_RESULTS);
  }
  ~Ndb_expect_stack() {
    if (next) destroy(next);
    next = NULL;
  }
  void push(Ndb_expect_stack *expect_next) { next = expect_next; }
  void pop() {
    if (next) {
      Ndb_expect_stack *expect_next = next;
      bitmap_copy(&expect_mask, &next->expect_mask);
      bitmap_copy(&expect_field_type_mask, &next->expect_field_type_mask);
      bitmap_copy(&expect_field_result_mask, &next->expect_field_result_mask);
      other_field = next->other_field;
      collation = next->collation;
      next = next->next;
      destroy(expect_next);
    }
  }

  void expect(Item::Type type) { bitmap_set_bit(&expect_mask, (uint)type); }
  void dont_expect(Item::Type type) {
    bitmap_clear_bit(&expect_mask, (uint)type);
  }
  bool expecting(Item::Type type) {
    if (unlikely((uint)type > MAX_EXPECT_ITEMS)) {
      // Unknown type, can't be expected
      return false;
    }
    return bitmap_is_set(&expect_mask, (uint)type);
  }
  void expect_nothing() { bitmap_clear_all(&expect_mask); }
  bool expecting_nothing() { return bitmap_is_clear_all(&expect_mask); }
  void expect_only(Item::Type type) {
    expect_nothing();
    expect(type);
  }
  bool expecting_only(Item::Type type) {
    return (expecting(type) && bitmap_bits_set(&expect_mask) == 1);
  }

  void expect_field_type(enum_field_types type) {
    bitmap_set_bit(&expect_field_type_mask, (uint)type);
  }
  void dont_expect_field_type(enum_field_types type) {
    bitmap_clear_bit(&expect_field_type_mask, (uint)type);
  }
  void expect_all_field_types() { bitmap_set_all(&expect_field_type_mask); }
  bool expecting_field_type(enum_field_types type) {
    if (unlikely((uint)type > MAX_EXPECT_FIELD_TYPES)) {
      // Unknown type, can't be expected
      return false;
    }
    return bitmap_is_set(&expect_field_type_mask, (uint)type);
  }
  void expect_only_field_type(enum_field_types type) {
    bitmap_clear_all(&expect_field_type_mask);
    expect_field_type(type);
  }

  void expect_comparable_field(const Field *field) { other_field = field; }
  bool expecting_comparable_field(const Field *field) {
    if (other_field == nullptr)
      // No Field to be comparable with
      return true;

    // Fields need to have equal definition
    return other_field->eq_def(field);
  }

  void expect_field_result(Item_result result) {
    bitmap_set_bit(&expect_field_result_mask, (uint)result);
  }
  bool expecting_field_result(Item_result result) {
    if (unlikely((uint)result > MAX_EXPECT_FIELD_RESULTS)) {
      // Unknown result, can't be expected
      return false;
    }
    return bitmap_is_set(&expect_field_result_mask, (uint)result);
  }
  void expect_no_field_result() { bitmap_clear_all(&expect_field_result_mask); }
  bool expecting_no_field_result() {
    return bitmap_is_clear_all(&expect_field_result_mask);
  }
  void expect_collation(const CHARSET_INFO *col) { collation = col; }
  bool expecting_collation(const CHARSET_INFO *col) {
    bool matching = (!collation) ? true : (collation == col);
    collation = NULL;

    return matching;
  }
  void expect_length(Uint32 len) { length = len; }
  void expect_max_length(Uint32 max) { max_length = max; }
  bool expecting_length(Uint32 len) {
    return max_length == 0 || len <= max_length;
  }
  bool expecting_max_length(Uint32 max) { return max >= length; }
  void expect_no_length() { length = max_length = 0; }

 private:
  my_bitmap_map m_expect_buf[bitmap_buffer_size(MAX_EXPECT_ITEMS)];
  my_bitmap_map
      m_expect_field_type_buf[bitmap_buffer_size(MAX_EXPECT_FIELD_TYPES)];
  my_bitmap_map
      m_expect_field_result_buf[bitmap_buffer_size(MAX_EXPECT_FIELD_RESULTS)];
  MY_BITMAP expect_mask;
  MY_BITMAP expect_field_type_mask;
  MY_BITMAP expect_field_result_mask;
  const Field *other_field;
  const CHARSET_INFO *collation;
  Uint32 length;
  Uint32 max_length;
  Ndb_expect_stack *next;
};

class Ndb_rewrite_context {
 public:
  Ndb_rewrite_context(const Item_func *func)
      : func_item(func), left_hand_item(NULL), count(0) {}
  ~Ndb_rewrite_context() {
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
class Ndb_cond_traverse_context {
 public:
  Ndb_cond_traverse_context(TABLE *tab, const NdbDictionary::Table *ndb_tab)
      : table(tab),
        ndb_table(ndb_tab),
        supported(true),
        skip(0),
        rewrite_stack(NULL) {}
  ~Ndb_cond_traverse_context() {
    if (rewrite_stack) destroy(rewrite_stack);
  }

  inline void expect_field_from_table() {
    expect_stack.expect(Item::FIELD_ITEM);
    expect_stack.expect_all_field_types();
    expect_stack.expect_comparable_field(nullptr);
  }
  inline void expect_only_field_from_table() {
    expect_stack.expect_nothing();
    expect_field_from_table();
  }

  inline void expect(Item::Type type) { expect_stack.expect(type); }
  inline void dont_expect(Item::Type type) { expect_stack.dont_expect(type); }
  inline bool expecting(Item::Type type) {
    return expect_stack.expecting(type);
  }
  inline void expect_nothing() { expect_stack.expect_nothing(); }
  inline bool expecting_nothing() { return expect_stack.expecting_nothing(); }
  inline void expect_only(Item::Type type) { expect_stack.expect_only(type); }

  inline void expect_field_type(enum_field_types type) {
    expect_stack.expect_field_type(type);
  }
  inline void dont_expect_field_type(enum_field_types type) {
    expect_stack.dont_expect_field_type(type);
  }
  inline void expect_only_field_type(enum_field_types result) {
    expect_stack.expect_only_field_type(result);
  }

  inline void expect_comparable_field(const Field *field) {
    expect_stack.expect_only_field_type(field->real_type());
    expect_stack.expect_comparable_field(field);
  }
  inline bool expecting_comparable_field(const Field *field) {
    return expect_stack.expecting_field_type(field->real_type()) &&
           expect_stack.expecting_comparable_field(field);
  }

  inline void expect_field_result(Item_result result) {
    expect_stack.expect_field_result(result);
  }
  inline bool expecting_field_result(Item_result result) {
    return expect_stack.expecting_field_result(result);
  }
  inline void expect_no_field_result() {
    expect_stack.expect_no_field_result();
  }
  inline bool expecting_no_field_result() {
    return expect_stack.expecting_no_field_result();
  }
  inline void expect_collation(const CHARSET_INFO *col) {
    expect_stack.expect_collation(col);
  }
  inline bool expecting_collation(const CHARSET_INFO *col) {
    return expect_stack.expecting_collation(col);
  }
  inline void expect_length(Uint32 length) {
    expect_stack.expect_length(length);
  }
  inline void expect_max_length(Uint32 max) {
    expect_stack.expect_max_length(max);
  }
  inline bool expecting_length(Uint32 length) {
    return expect_stack.expecting_length(length);
  }
  inline bool expecting_max_length(Uint32 max) {
    return expect_stack.expecting_max_length(max);
  }
  inline void expect_no_length() { expect_stack.expect_no_length(); }

  TABLE *const table;
  const NdbDictionary::Table *const ndb_table;
  bool supported;
  List<const Ndb_item> items;
  Ndb_expect_stack expect_stack;
  uint skip;
  Ndb_rewrite_context *rewrite_stack;
};

static bool is_supported_temporal_type(enum_field_types type) {
  switch (type) {
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
  operand_count() reflect the traverse_cond() operand traversal.
  Note that traverse_cond() only traverse any operands for FUNC_ITEM
  and COND_ITEM, which is reflected by operand_count().
*/
static uint operand_count(const Item *item) {
  switch (item->type()) {
    case Item::FUNC_ITEM: {
      const Item_func *func_item = static_cast<const Item_func *>(item);
      return func_item->argument_count();
    }
    case Item::COND_ITEM: {
      Item_cond *cond_item =
          const_cast<Item_cond *>(static_cast<const Item_cond *>(item));
      List<Item> *arguments = cond_item->argument_list();
      // A COND_ITEM (And/or) is visited both infix and postfix, so need '+1'
      return arguments->elements + 1;
    }
    default:
      return 0;
  }
}

/*
  Serialize the item tree into a List of Ndb_item objecs
  for fast generation of NbdScanFilter. Adds information such as
  position of fields that is not directly available in the Item tree.
  Also checks if condition is supported.
*/
static void ndb_serialize_cond(const Item *item, void *arg) {
  Ndb_cond_traverse_context *context = (Ndb_cond_traverse_context *)arg;
  DBUG_TRACE;

  // Check if we are skipping arguments to a function to be evaluated
  if (context->skip) {
    DBUG_PRINT("info", ("Skipping argument %d", context->skip));
    context->skip--;
    if (item != nullptr) {
      context->skip += operand_count(item);
    }
    return;
  }

  if (context->supported) {
    Ndb_rewrite_context *rewrite_context = context->rewrite_stack;
    // Check if we are rewriting some unsupported function call
    if (rewrite_context) {
      rewrite_context->count++;
      if (rewrite_context->count == 1) {
        // This is the <left_hand_item>, save it in the rewrite context
        rewrite_context->left_hand_item = item;
      } else {
        // Has already seen the 'left_hand_item', this 'item' is one of
        // the right hand items in the in/between predicate to be rewritten.
        Item *cmp_func = nullptr;
        const Item_func *rewrite_func_item = rewrite_context->func_item;
        switch (rewrite_func_item->functype()) {
          case Item_func::BETWEEN: {
            /*
              Rewrite <left_hand_item> BETWEEN <item1> AND <item2>
              to <left_hand_item> >= <item1> AND
                 <left_hand_item> <= <item2>
            */
            if (rewrite_context->count == 2)  // Lower 'between-limit'
            {
              // Lower limit of BETWEEN
              DBUG_PRINT("info", ("GE_FUNC"));
              cmp_func = new (*THR_MALLOC) Item_func_ge(
                  const_cast<Item *>(rewrite_context->left_hand_item),
                  const_cast<Item *>(item));
            } else if (rewrite_context->count == 3)  // Upper 'between-limit'
            {
              // Upper limit of BETWEEN
              DBUG_PRINT("info", ("LE_FUNC"));
              cmp_func = new (*THR_MALLOC) Item_func_le(
                  const_cast<Item *>(rewrite_context->left_hand_item),
                  const_cast<Item *>(item));
            } else {
              // Illegal BETWEEN expression
              DBUG_PRINT("info", ("Illegal BETWEEN expression"));
              context->supported = false;
              return;
            }
            break;
          }
          case Item_func::IN_FUNC: {
            /*
              Rewrite <left_hand_item> IN(<item1>, <item2>,..)
              to <left_hand_item> = <item1> OR
                 <left_hand_item> = <item2> ...
            */
            DBUG_PRINT("info", ("EQ_FUNC"));
            cmp_func = new (*THR_MALLOC) Item_func_eq(
                const_cast<Item *>(rewrite_context->left_hand_item),
                const_cast<Item *>(item));
            break;
          }
          default:
            // Only BETWEEN/IN can be rewritten.
            // If we add support for rewrite of others, handling must be added
            // above
            DBUG_ASSERT(false);
            context->supported = false;
            return;
        }
        cmp_func->fix_fields(current_thd, &cmp_func);
        cmp_func->update_used_tables();

        // Traverse and serialize the rewritten predicate
        context->rewrite_stack = NULL;  // Disable rewrite mode
        context->expect_only(Item::FUNC_ITEM);
        cmp_func->traverse_cond(&ndb_serialize_cond, context, Item::PREFIX);
        context->rewrite_stack = rewrite_context;  // Re-enable rewrite mode

        // Possibly terminate the rewrite_context
        if (context->supported &&
            rewrite_context->count ==
                rewrite_context->func_item->argument_count()) {
          // Rewrite is done, wrap an END() at the end
          DBUG_PRINT("info", ("End of rewrite condition group"));
          context->items.push_back(new (*THR_MALLOC) Ndb_item(NDB_END_COND));
          // Pop rewrite stack
          context->rewrite_stack = rewrite_context->next;
          rewrite_context->next = NULL;
          destroy(rewrite_context);
        }
      }
      DBUG_PRINT("info",
                 ("Skip 'item' (to be) handled in rewritten predicate"));
      context->skip = operand_count(item);
      return;
    } else  // not in a 'rewrite_context'
    {
      const Ndb_item *ndb_item = nullptr;
      // Check for end of AND/OR expression
      if (!item) {
        // End marker for condition group
        DBUG_PRINT("info", ("End of condition group"));
        context->expect_no_length();
        ndb_item = new (*THR_MALLOC) Ndb_item(NDB_END_COND);
      } else {
        bool pop = true;
        /*
          Based on which tables being used from an item expression,
          we might be able to evaluate its value immediately.
          Generally any tables prior to 'this' table has values known by
          now, same is true for expressions being entirely 'const'.
        */
        const table_map this_table = context->table->pos_in_table_list->map();
        if (!(item->used_tables() & this_table)) {
          /*
            Item value can be evaluated right away, and its value used in the
            condition, instead of the Item-expression. Note that this will
            also catch the INT_, STRING_, REAL_, DECIMAL_ and VARBIN_ITEM,
            as well as any CACHE_ITEM and FIELD_ITEM referring 'other' tables.
          */
#ifndef DBUG_OFF
          String str;
          item->print(current_thd, &str, QT_ORDINARY);
#endif
          if (item->is_bool_func()) {
            // Item is a boolean func, (e.g. an EQ_FUNC)
            DBUG_ASSERT(item->result_type() == INT_RESULT);
            DBUG_PRINT("info",
                       ("BOOLEAN 'VALUE' expression: '%s'", str.c_ptr_safe()));
            ndb_item = new (*THR_MALLOC) Ndb_item(item);

            // Expect another logical expression
            context->expect_only(Item::FUNC_ITEM);
            context->expect(Item::COND_ITEM);
          } else if (item->type() == Item::VARBIN_ITEM) {
            // VARBIN_ITEM is special as no similar VARBIN_RESULT type is
            // defined, so it need to be explicitely handled here.
            DBUG_PRINT("info", ("VARBIN_ITEM 'VALUE' expression: '%s'",
                                str.c_ptr_safe()));
            if (context->expecting(Item::VARBIN_ITEM)) {
              ndb_item = new (*THR_MALLOC) Ndb_item(item);
              if (context->expecting_no_field_result()) {
                // We have not seen the field argument referring this table yet
                context->expect_only_field_from_table();
                context->expect_field_result(STRING_RESULT);
              } else {
                // Expect another logical expression
                context->expect_only(Item::FUNC_ITEM);
                context->expect(Item::COND_ITEM);
              }
            } else
              context->supported = false;
          } else {
            // For the INT, REAL, DECIMAL and STRING Item type, we use
            // the similar result_type() as a 'catch it all' synonym to
            // handle both an Item and any expression of the specific type.
            //
            // Assert that any such Items are of the expected RESULT_ type:
            DBUG_ASSERT(item->type() != Item::INT_ITEM ||
                        item->result_type() == INT_RESULT);
            DBUG_ASSERT(item->type() != Item::REAL_ITEM ||
                        item->result_type() == REAL_RESULT);
            DBUG_ASSERT(item->type() != Item::DECIMAL_ITEM ||
                        item->result_type() == DECIMAL_RESULT);
            DBUG_ASSERT(item->type() != Item::STRING_ITEM ||
                        item->result_type() == STRING_RESULT);

            switch (item->result_type()) {
              case INT_RESULT:
                DBUG_PRINT("info", ("INTEGER 'VALUE' expression: '%s'",
                                    str.c_ptr_safe()));
                if (context->expecting(Item::INT_ITEM)) {
                  ndb_item = new (*THR_MALLOC) Ndb_item(item);
                  if (context->expecting_no_field_result()) {
                    // We have not seen the field argument yet
                    context->expect_only_field_from_table();
                    context->expect_field_result(INT_RESULT);
                    context->expect_field_result(REAL_RESULT);
                    context->expect_field_result(DECIMAL_RESULT);
                  } else {
                    // Expect another logical expression
                    context->expect_only(Item::FUNC_ITEM);
                    context->expect(Item::COND_ITEM);
                  }
                } else
                  context->supported = false;
                break;

              case REAL_RESULT:
                DBUG_PRINT("info",
                           ("REAL 'VALUE' expression: '%s'", str.c_ptr_safe()));
                if (context->expecting(Item::REAL_ITEM)) {
                  ndb_item = new (*THR_MALLOC) Ndb_item(item);
                  if (context->expecting_no_field_result()) {
                    // We have not seen the field argument yet
                    context->expect_only_field_from_table();
                    context->expect_field_result(REAL_RESULT);
                  } else {
                    // Expect another logical expression
                    context->expect_only(Item::FUNC_ITEM);
                    context->expect(Item::COND_ITEM);
                  }
                } else
                  context->supported = false;
                break;

              case DECIMAL_RESULT:
                DBUG_PRINT("info", ("DECIMAL 'VALUE' expression: '%s'",
                                    str.c_ptr_safe()));
                if (context->expecting(Item::DECIMAL_ITEM)) {
                  ndb_item = new (*THR_MALLOC) Ndb_item(item);
                  if (context->expecting_no_field_result()) {
                    // We have not seen the field argument yet
                    context->expect_only_field_from_table();
                    context->expect_field_result(REAL_RESULT);
                    context->expect_field_result(DECIMAL_RESULT);
                  } else {
                    // Expect another logical expression
                    context->expect_only(Item::FUNC_ITEM);
                    context->expect(Item::COND_ITEM);
                  }
                } else
                  context->supported = false;
                break;

              case STRING_RESULT:
                DBUG_PRINT("info", ("STRING 'VALUE' expression: '%s'",
                                    str.c_ptr_safe()));
                // Check that we do support pushing the item value length
                if (context->expecting(Item::STRING_ITEM) &&
                    context->expecting_length(item->max_length)) {
                  ndb_item = new (*THR_MALLOC) Ndb_item(item);
                  if (context->expecting_no_field_result()) {
                    // We have not seen the field argument yet
                    context->expect_only_field_from_table();
                    context->expect_field_result(STRING_RESULT);
                    context->expect_collation(item->collation.collation);
                    context->expect_length(item->max_length);
                  } else {
                    // Expect another logical expression
                    context->expect_only(Item::FUNC_ITEM);
                    context->expect(Item::COND_ITEM);
                    context->expect_no_length();
                    // Check that we are comparing with a field with same
                    // collation
                    if (!context->expecting_collation(
                            item->collation.collation)) {
                      DBUG_PRINT("info", ("Found non-matching collation %s",
                                          item->collation.collation->name));
                      context->supported = false;
                    }
                  }
                } else
                  context->supported = false;
                break;

              default:
                DBUG_ASSERT(false);
                context->supported = false;
                break;
            }
          }
          if (context->supported) {
            DBUG_ASSERT(ndb_item != nullptr);
            context->items.push_back(ndb_item);
          }

          // Skip any arguments since we will evaluate this expression instead
          context->skip = operand_count(item);
          DBUG_PRINT("info", ("Skip until end of arguments marker, operands:%d",
                              context->skip));
          return;
        }

        switch (item->type()) {
          case Item::FIELD_ITEM: {
            const Item_field *field_item = down_cast<const Item_field *>(item);
            Field *field = field_item->field;
            const enum_field_types type = field->real_type();

            /* Check whether field is computed at MySQL layer */
            if (field->is_virtual_gcol()) {
              context->supported = false;
              break;
            }

            DBUG_PRINT("info", ("FIELD_ITEM"));
            DBUG_PRINT("info", ("table %s", field->table->alias));
            DBUG_PRINT("info", ("column %s", field->field_name));
            DBUG_PRINT("info", ("column length %u", field->field_length));
            DBUG_PRINT("info", ("type %d", type));
            DBUG_PRINT("info", ("result type %d", field->result_type()));

            // Check that we are expecting a field with the correct
            // type, and possibly being 'comparable' with a previous Field.
            if (context->expecting(Item::FIELD_ITEM) &&
                context->expecting_comparable_field(field) &&
                // Bit fields not yet supported in scan filter
                type != MYSQL_TYPE_BIT &&
                /* Char(0) field is treated as Bit fields inside NDB
                   Hence not supported in scan filter */
                (!(type == MYSQL_TYPE_STRING && field->pack_length() == 0)) &&
                // No BLOB support in scan filter
                type != MYSQL_TYPE_TINY_BLOB &&
                type != MYSQL_TYPE_MEDIUM_BLOB &&
                type != MYSQL_TYPE_LONG_BLOB && type != MYSQL_TYPE_BLOB &&
                type != MYSQL_TYPE_JSON && type != MYSQL_TYPE_GEOMETRY) {
              // Found a Field_item of a supported type. and from 'this' table
              DBUG_ASSERT(context->table == field->table);

              const NDBCOL *col =
                  context->ndb_table->getColumn(field->field_name);
              DBUG_ASSERT(col);
              ndb_item = new (*THR_MALLOC) Ndb_item(field, col->getColumnNo());

              /*
                Check, or set, further expectations for the operand(s).
                For an operation taking multiple operands, the first operand
                sets the requirement for the next to be compatible.
                'expecting_*_field_result' is used to check if this is the
                first operand or not: If there are no 'field_result'
                expectations set yet, this is the first operand, and it is used
                to set expectations for the next one(s).
              */
              if (!context->expecting_no_field_result()) {
                // Have some result type expectations to check.
                // Note that STRING and INT(Year) are always allowed
                // to be used together with temporal data types.
                if (!(context->expecting_field_result(field->result_type()) ||
                      // Date and year can be written as string or int
                      (is_supported_temporal_type(type) &&
                       (context->expecting_field_result(STRING_RESULT) ||
                        context->expecting_field_result(INT_RESULT))))) {
                  DBUG_PRINT("info",
                             ("Was not expecting field of result_type %u(%u)",
                              field->result_type(), type));
                  context->supported = false;
                  break;
                }

                // STRING results has to be checked for correct 'length' and
                // collation, except if it is a result from a temporal data
                // type.
                if (field->result_type() == STRING_RESULT &&
                    !is_supported_temporal_type(type)) {
                  if (!context->expecting_max_length(field->field_length)) {
                    DBUG_PRINT("info", ("Found non-matching string length %s",
                                        field->field_name));
                    context->supported = false;
                    break;
                  }
                  // Check that field and string constant collations are the
                  // same
                  if (!context->expecting_collation(
                          item->collation.collation)) {
                    DBUG_PRINT("info", ("Found non-matching collation %s",
                                        item->collation.collation->name));
                    context->supported = false;
                    break;
                  }
                }

                // Seen expected arguments, expect another logical expression
                context->expect_only(Item::FUNC_ITEM);
                context->expect(Item::COND_ITEM);
              } else  // is not 'expecting_field_result'
              {
                // This is the first operand, it decides expectations for
                // the next operand, required to be compatible with this one.
                if (is_supported_temporal_type(type)) {
                  context->expect_only(Item::STRING_ITEM);
                  context->expect(Item::INT_ITEM);
                } else {
                  switch (field->result_type()) {
                    case STRING_RESULT:
                      // Expect char string or binary string
                      context->expect_only(Item::STRING_ITEM);
                      context->expect(Item::VARBIN_ITEM);
                      context->expect_collation(
                          field_item->collation.collation);
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
                      DBUG_ASSERT(false);
                      break;
                  }
                }
                const Ndb *ndb = get_thd_ndb(current_thd)->ndb;
                if (ndbd_support_column_cmp(ndb->getMinDbNodeVersion())) {
                  // Since WL#13120: Two columns may be compared in
                  // NdbScanFilter:
                  // -> Second argument can also be a FIELD_ITEM, referring
                  // another Field from this table. Need to ensure that these
                  // Fields are of identical type, length, precision etc.
                  context->expect(Item::FIELD_ITEM);
                  context->expect_comparable_field(field);
                }
                context->expect_field_result(field->result_type());
              }
            } else {
              DBUG_PRINT("info", ("Was not expecting field of type %u(%u)",
                                  field->result_type(), type));
              context->supported = false;
            }
            break;
          }
          case Item::FUNC_ITEM: {
            // Check that we expect a function here
            if (!context->expecting(Item::FUNC_ITEM)) {
              context->supported = false;
              break;
            }

            context->expect_nothing();
            context->expect_no_length();

            const Item_func *func_item = static_cast<const Item_func *>(item);
            switch (func_item->functype()) {
              case Item_func::EQ_FUNC: {
                DBUG_PRINT("info", ("EQ_FUNC"));
                ndb_item = new (*THR_MALLOC)
                    Ndb_item(func_item->functype(), func_item);
                context->expect(Item::STRING_ITEM);
                context->expect(Item::INT_ITEM);
                context->expect(Item::REAL_ITEM);
                context->expect(Item::DECIMAL_ITEM);
                context->expect(Item::VARBIN_ITEM);
                context->expect_field_from_table();
                context->expect_no_field_result();
                break;
              }
              case Item_func::NE_FUNC: {
                DBUG_PRINT("info", ("NE_FUNC"));
                ndb_item = new (*THR_MALLOC)
                    Ndb_item(func_item->functype(), func_item);
                context->expect(Item::STRING_ITEM);
                context->expect(Item::INT_ITEM);
                context->expect(Item::REAL_ITEM);
                context->expect(Item::DECIMAL_ITEM);
                context->expect(Item::VARBIN_ITEM);
                context->expect_field_from_table();
                context->expect_no_field_result();
                break;
              }
              case Item_func::LT_FUNC: {
                DBUG_PRINT("info", ("LT_FUNC"));
                ndb_item = new (*THR_MALLOC)
                    Ndb_item(func_item->functype(), func_item);
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
              case Item_func::LE_FUNC: {
                DBUG_PRINT("info", ("LE_FUNC"));
                ndb_item = new (*THR_MALLOC)
                    Ndb_item(func_item->functype(), func_item);
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
              case Item_func::GE_FUNC: {
                DBUG_PRINT("info", ("GE_FUNC"));
                ndb_item = new (*THR_MALLOC)
                    Ndb_item(func_item->functype(), func_item);
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
              case Item_func::GT_FUNC: {
                DBUG_PRINT("info", ("GT_FUNC"));
                ndb_item = new (*THR_MALLOC)
                    Ndb_item(func_item->functype(), func_item);
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
              case Item_func::LIKE_FUNC: {
                Ndb_expect_stack *expect_next =
                    new (*THR_MALLOC) Ndb_expect_stack();
                DBUG_PRINT("info", ("LIKE_FUNC"));

                const Item_func_like *like_func =
                    static_cast<const Item_func_like *>(func_item);
                if (like_func->escape_was_used_in_parsing()) {
                  DBUG_PRINT("info",
                             ("LIKE expressions with ESCAPE not supported"));
                  context->supported = false;
                }
                ndb_item = new (*THR_MALLOC)
                    Ndb_item(func_item->functype(), func_item);

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
                pop = false;
                break;
              }
              case Item_func::ISNULL_FUNC: {
                DBUG_PRINT("info", ("ISNULL_FUNC"));
                ndb_item = new (*THR_MALLOC)
                    Ndb_item(func_item->functype(), func_item);
                context->expect_field_from_table();
                context->expect_field_result(STRING_RESULT);
                context->expect_field_result(REAL_RESULT);
                context->expect_field_result(INT_RESULT);
                context->expect_field_result(DECIMAL_RESULT);
                break;
              }
              case Item_func::ISNOTNULL_FUNC: {
                DBUG_PRINT("info", ("ISNOTNULL_FUNC"));
                ndb_item = new (*THR_MALLOC)
                    Ndb_item(func_item->functype(), func_item);
                context->expect_field_from_table();
                context->expect_field_result(STRING_RESULT);
                context->expect_field_result(REAL_RESULT);
                context->expect_field_result(INT_RESULT);
                context->expect_field_result(DECIMAL_RESULT);
                break;
              }
              case Item_func::NOT_FUNC: {
                DBUG_PRINT("info", ("NOT_FUNC"));
                ndb_item = new (*THR_MALLOC)
                    Ndb_item(func_item->functype(), func_item);
                context->expect(Item::FUNC_ITEM);
                context->expect(Item::COND_ITEM);
                break;
              }
              case Item_func::BETWEEN: {
                DBUG_PRINT("info", ("BETWEEN, rewriting using AND"));
                const Item_func_between *between_func =
                    static_cast<const Item_func_between *>(func_item);
                Ndb_rewrite_context *rewrite_context =
                    new (*THR_MALLOC) Ndb_rewrite_context(func_item);
                rewrite_context->next = context->rewrite_stack;
                context->rewrite_stack = rewrite_context;
                if (between_func->negated) {
                  DBUG_PRINT("info", ("NOT_FUNC"));
                  context->items.push_back(
                      new (*THR_MALLOC) Ndb_item(Item_func::NOT_FUNC, 1));
                }
                DBUG_PRINT("info", ("COND_AND_FUNC"));
                ndb_item = new (*THR_MALLOC) Ndb_item(
                    Item_func::COND_AND_FUNC, func_item->argument_count() - 1);
                // We do not 'expect' anything yet, added later as part of
                // rewrite,
                break;
              }
              case Item_func::IN_FUNC: {
                DBUG_PRINT("info", ("IN_FUNC, rewriting using OR"));
                const Item_func_in *in_func =
                    static_cast<const Item_func_in *>(func_item);
                Ndb_rewrite_context *rewrite_context =
                    new (*THR_MALLOC) Ndb_rewrite_context(func_item);
                rewrite_context->next = context->rewrite_stack;
                context->rewrite_stack = rewrite_context;
                if (in_func->negated) {
                  DBUG_PRINT("info", ("NOT_FUNC"));
                  context->items.push_back(
                      new (*THR_MALLOC) Ndb_item(Item_func::NOT_FUNC, 1));
                }
                DBUG_PRINT("info", ("COND_OR_FUNC"));
                ndb_item = new (*THR_MALLOC) Ndb_item(
                    Item_func::COND_OR_FUNC, func_item->argument_count() - 1);
                // We do not 'expect' anything yet, added later as part of
                // rewrite,
                break;
              }
              default: {
                DBUG_PRINT("info", ("Found func_item of type %d",
                                    func_item->functype()));
                context->supported = false;
              }
            }
            break;
          }

          case Item::COND_ITEM: {
            const Item_cond *cond_item = static_cast<const Item_cond *>(item);
            if (context->expecting(Item::COND_ITEM)) {
              switch (cond_item->functype()) {
                case Item_func::COND_AND_FUNC:
                  DBUG_PRINT("info", ("COND_AND_FUNC"));
                  ndb_item = new (*THR_MALLOC)
                      Ndb_item(cond_item->functype(), cond_item);
                  break;
                case Item_func::COND_OR_FUNC:
                  DBUG_PRINT("info", ("COND_OR_FUNC"));
                  ndb_item = new (*THR_MALLOC)
                      Ndb_item(cond_item->functype(), cond_item);
                  break;
                default:
                  DBUG_PRINT("info", ("COND_ITEM %d", cond_item->functype()));
                  context->supported = false;
                  break;
              }
            } else {
              /* Did not expect condition */
              context->supported = false;
            }
            break;
          }
          case Item::STRING_ITEM:
          case Item::INT_ITEM:
          case Item::REAL_ITEM:
          case Item::VARBIN_ITEM:
          case Item::DECIMAL_ITEM:
          case Item::CACHE_ITEM:
            DBUG_ASSERT(false);  // Expression folded under 'used_tables'
            // Fall through
          default:
            DBUG_PRINT("info",
                       ("Found unsupported item of type %d", item->type()));
            context->supported = false;
        }
        if (pop) context->expect_stack.pop();
      }

      if (context->supported) {
        DBUG_ASSERT(ndb_item != nullptr);
        context->items.push_back(ndb_item);
      }
    }
  }
}

ha_ndbcluster_cond::ha_ndbcluster_cond()
    : m_ndb_cond(), m_scan_filter_code(nullptr), m_unpushed_cond(nullptr) {}

ha_ndbcluster_cond::~ha_ndbcluster_cond() { m_ndb_cond.destroy_elements(); }

/*
  Clear the condition stack
*/
void ha_ndbcluster_cond::cond_clear() {
  DBUG_TRACE;
  m_ndb_cond.destroy_elements();
  m_scan_filter_code.reset();
  m_unpushed_cond = nullptr;
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
static int create_and_conditions(Item_cond *cond, List<Item> pushed_list,
                                 List<Item> remainder_list, Item *&pushed_cond,
                                 Item *&remainder_cond) {
  if (remainder_list.is_empty()) {
    // Entire cond pushed, no remainder
    pushed_cond = cond;
    remainder_cond = nullptr;
    return 0;
  }
  if (pushed_list.is_empty()) {
    // Nothing pushed, entire 'cond' is remainder
    pushed_cond = nullptr;
    remainder_cond = cond;
    return 0;
  }

  // Condition was partly pushed, with some remainder
  if (pushed_list.elements == 1) {
    // Single boolean term pushed, return it
    pushed_cond = pushed_list.head();
  } else {
    // Construct an AND'ed condition of pushed boolean terms
    pushed_cond = new Item_cond_and(pushed_list);
    if (unlikely(pushed_cond == nullptr)) return 1;
    pushed_cond->quick_fix_field();
    pushed_cond->update_used_tables();
  }

  if (remainder_list.elements == 1) {
    // A single boolean term as remainder, return it
    remainder_cond = remainder_list.head();
  } else {
    // Construct a remainder as an AND'ed condition of the boolean terms
    remainder_cond = new Item_cond_and(remainder_list);
    if (unlikely(remainder_cond == nullptr)) return 1;
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
static int create_or_conditions(Item_cond *cond, List<Item> pushed_list,
                                List<Item> remainder_list, Item *&pushed_cond,
                                Item *&remainder_cond) {
  DBUG_ASSERT(pushed_list.elements == cond->argument_list()->elements);

  if (remainder_list.is_empty()) {
    // Entire cond pushed, no remainder
    pushed_cond = cond;
    remainder_cond = nullptr;
  } else {
    // When condition was partially pushed, we need to reevaluate
    // original OR-cond on the server side:
    remainder_cond = cond;

    // Construct an OR'ed condition of pushed boolean terms
    pushed_cond = new Item_cond_or(pushed_list);
    if (unlikely(pushed_cond == nullptr)) return 1;
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
static List<const Ndb_item> cond_push_boolean_term(Item *term, TABLE *table,
                                                   const NDBTAB *ndb_table,
                                                   bool other_tbls_ok,
                                                   Item *&pushed_cond,
                                                   Item *&remainder_cond)

{
  DBUG_TRACE;
  static const List<const Ndb_item> empty_list;

  if (term->type() == Item::COND_ITEM) {
    // Build lists of the boolean terms either 'pushed', or being a 'remainder'
    List<Item> pushed_list;
    List<Item> remainder_list;
    List<const Ndb_item> code;

    Item_cond *cond = (Item_cond *)term;
    if (cond->functype() == Item_func::COND_AND_FUNC) {
      DBUG_PRINT("info", ("COND_AND_FUNC"));

      List_iterator<Item> li(*cond->argument_list());
      Item *boolean_term;
      while ((boolean_term = li++)) {
        Item *pushed = nullptr, *remainder = nullptr;
        List<const Ndb_item> code_stub = cond_push_boolean_term(
            boolean_term, table, ndb_table, other_tbls_ok, pushed, remainder);

        // Collect all bits we pushed, and its leftovers.
        if (!code_stub.is_empty()) code.concat(&code_stub);
        if (pushed != nullptr) pushed_list.push_back(pushed);
        if (remainder != nullptr) remainder_list.push_back(remainder);
      }

      // Transform the list of pushed and the remainder conditions
      // into its respective AND'ed conditions.
      if (create_and_conditions(cond, pushed_list, remainder_list, pushed_cond,
                                remainder_cond)) {
        // Failed, discard pushed conditions and generated code.
        pushed_cond = nullptr;
        remainder_cond = cond;
        code.destroy_elements();
        return empty_list;
      }
      // Serialized code has to be embedded in an AND-group
      if (!code.is_empty()) {
        code.push_front(new (*THR_MALLOC)
                            Ndb_item(Item_func::COND_AND_FUNC, cond));
        code.push_back(new (*THR_MALLOC) Ndb_item(NDB_END_COND));
      }
      DBUG_PRINT("info", ("COND_AND_FUNC, end"));
    } else {
      DBUG_ASSERT(cond->functype() == Item_func::COND_OR_FUNC);
      DBUG_PRINT("info", ("COND_OR_FUNC"));

      List_iterator<Item> li(*cond->argument_list());
      Item *boolean_term;
      while ((boolean_term = li++)) {
        Item *pushed = nullptr, *remainder = nullptr;
        List<const Ndb_item> code_stub = cond_push_boolean_term(
            boolean_term, table, ndb_table, other_tbls_ok, pushed, remainder);

        if (pushed == nullptr) {
          // Failure of pushing one of the OR-terms fails entire OR'ed cond
          //(Else the rows matching that term would be missing in result set)
          // Also see comments in create_or_conditions().
          pushed_cond = nullptr;
          remainder_cond = cond;
          code.destroy_elements();
          return empty_list;
        }

        // Collect all bits we pushed, and its leftovers.
        if (!code_stub.is_empty()) code.concat(&code_stub);
        if (pushed != nullptr) pushed_list.push_back(pushed);
        if (remainder != nullptr) remainder_list.push_back(remainder);
      }

      // Transform the list of pushed and the remainder conditions
      // into its respective OR'ed conditions.
      if (create_or_conditions(cond, pushed_list, remainder_list, pushed_cond,
                               remainder_cond)) {
        // Failed, discard pushed conditions and generated code.
        pushed_cond = nullptr;
        remainder_cond = cond;
        code.destroy_elements();
        return empty_list;
      }
      // Serialized code has to be embedded in an OR-group
      if (!code.is_empty()) {
        code.push_front(new (*THR_MALLOC)
                            Ndb_item(Item_func::COND_OR_FUNC, cond));
        code.push_back(new (*THR_MALLOC) Ndb_item(NDB_END_COND));
      }
      DBUG_PRINT("info", ("COND_OR_FUNC, end"));
    }
    return code;
  } else if (term->type() == Item::FUNC_ITEM) {
    const Item_func *item_func = static_cast<const Item_func *>(term);
    if (item_func->functype() == Item_func::TRIG_COND_FUNC) {
      const Item_func_trig_cond *func_trig =
          static_cast<const Item_func_trig_cond *>(item_func);

      if (func_trig->get_trig_type() ==
          Item_func_trig_cond::IS_NOT_NULL_COMPL) {
        DBUG_ASSERT(item_func->argument_count() == 1);
        Item *cond_arg = item_func->arguments()[0];
        Item *remainder = nullptr;
        List<const Ndb_item> code = cond_push_boolean_term(
            cond_arg, table, ndb_table, other_tbls_ok, pushed_cond, remainder);
        if (remainder != nullptr) {
          item_func->arguments()[0] = remainder;
          remainder_cond = term;
        }
        return code;
      }
    }
  }
  /*
    There are some used_tables() 'out of reach', tagged with special
    *_TABLE_BIT. These can not be referred from a pushed condition.
  */
  const table_map dont_use_tables =
      INNER_TABLE_BIT |  // Condition contain a subquery
      RAND_TABLE_BIT;    // 'non-stable' value

  if (term->used_tables() & dont_use_tables) {
  } else if (other_tbls_ok ||
             !(term->used_tables() & ~table->pos_in_table_list->map())) {
    // Has broken down the condition into predicate terms, or sub conditions,
    // which either has to be accepted or rejected for pushdown
    Ndb_cond_traverse_context context(table, ndb_table);
    context.expect(Item::FUNC_ITEM);
    context.expect(Item::COND_ITEM);
    term->traverse_cond(&ndb_serialize_cond, &context, Item::PREFIX);

    if (context.supported)  // 'term' was pushed
    {
      pushed_cond = term;
      remainder_cond = nullptr;
      DBUG_ASSERT(!context.items.is_empty());
      return context.items;
    }
    context.items.destroy_elements();
  }
  // Failed to push
  pushed_cond = nullptr;
  remainder_cond = term;
  return empty_list;  // Discard any generated Ndb_cond's
}

/*
  Push a condition, return any remainder condition
 */
const Item *ha_ndbcluster_cond::cond_push(const Item *cond, TABLE *table,
                                          const NDBTAB *ndb_table,
                                          bool other_tbls_ok,
                                          Item *&pushed_cond) {
  DBUG_TRACE;

  // Build lists of the boolean terms either 'pushed', or being a 'remainder'
  Item *item = const_cast<Item *>(cond);
  Item *remainder = nullptr;
  List<const Ndb_item> code = cond_push_boolean_term(
      item, table, ndb_table, other_tbls_ok, pushed_cond, remainder);

  // Save the serialized representation of the code
  m_ndb_cond = code;

  if (pushed_cond != nullptr &&
      !(pushed_cond->used_tables() & ~table->pos_in_table_list->map())) {
    /**
     * pushed_cond had no dependencies outside of this 'table'.
     * Code for pushed condition can be generated now, and reused
     * for all later API requests to 'table'
     */
    NdbInterpretedCode code(ndb_table);
    NdbScanFilter filter(&code);
    const int ret = generate_scan_filter_from_cond(filter);
    if (unlikely(ret != 0)) {
      // Failed to 'generate' the pushed code.
      pushed_cond = nullptr;
      m_ndb_cond.destroy_elements();
      remainder = item;
    } else {
      // Success, save the generated code.
      DBUG_ASSERT(code.getWordsUsed() > 0);
      m_scan_filter_code.copy(code);
    }
  }
  return remainder;
}

int ha_ndbcluster_cond::build_scan_filter_predicate(
    List_iterator<const Ndb_item> &cond, NdbScanFilter *filter,
    bool negated) const {
  DBUG_TRACE;
  const Ndb_item *ndb_item = *cond.ref();
  switch (ndb_item->type) {
    case NDB_FUNCTION: {
      const Ndb_item *b = nullptr;
      const Ndb_item *field1, *field2 = nullptr, *value = nullptr;
      const Ndb_item *a = cond++;
      if (a == nullptr) break;

      enum ndb_func_type function_type =
          (negated) ? Ndb_item::negate(ndb_item->get_func_type())
                    : ndb_item->get_func_type();

      switch (ndb_item->get_argument_count()) {
        case 1:
          field1 = (a->type == NDB_FIELD) ? a : NULL;
          break;
        case 2:
          b = cond++;
          if (b == nullptr) {
            field1 = nullptr;
            break;
          }
          if (a->type == NDB_FIELD) {
            field1 = a;
            if (b->type == NDB_VALUE)
              value = b;
            else if (b->type == NDB_FIELD)
              field2 = b;
          } else {
            DBUG_ASSERT(a->type == NDB_VALUE);
            DBUG_ASSERT(b->type == NDB_FIELD);
            field1 = b;
            value = a;
          }
          if (value == a) function_type = Ndb_item::swap(function_type);
          break;
        default:
          DBUG_PRINT("info", ("condition had unexpected number of arguments"));
          return 1;
      }
      if (field1 == nullptr) {
        DBUG_PRINT("info", ("condition missing 'field' argument"));
        return 1;
      }

      if (value != nullptr) {
        const Item *item = value->get_item();
#ifndef DBUG_OFF
        if (!item->basic_const_item()) {
          String expr;
          String buf, *val = const_cast<Item *>(item)->val_str(&buf);
          item->print(current_thd, &expr, QT_ORDINARY);
          DBUG_PRINT("info",
                     ("Value evaluated to: '%s', expression '%s'",
                      val ? val->c_ptr_safe() : "NULL", expr.c_ptr_safe()));
        }
#endif

        /*
          The NdbInterpreter handles a NULL value as being less than any
          non-NULL value. However, MySQL server (and SQL std spec) specifies
          that a NULL-value in a comparison predicate should result in an
          UNKNOWN boolean result, which is 'not TRUE' -> the row being
          eliminated.

          Thus, extra checks for both 'field' and 'value' being a
          NULL-value has to be added to mitigate this semantic difference.
        */
        if (const_cast<Item *>(item)->is_null()) {
          /*
            'value' known to be a NULL-value.
            Condition will be 'not TRUE' -> false, independent of the 'field'
            value. Encapsulate in own group, as only this predicate become
            'false', not entire group it is part of.
          */
          if (filter->begin() == -1 || filter->isfalse() == -1 ||
              filter->end() == -1)
            return 1;
          return 0;
        }
      }

      const bool field1_maybe_null = field1->get_field()->maybe_null();
      const bool field2_maybe_null =
          field2 && field2->get_field()->maybe_null();
      bool added_null_check = false;

      if (field1_maybe_null || field2_maybe_null) {
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
            a '<NULL> < <any value>' check in the ScanFilter.
          */
          case NDB_LT_FUNC:
          case NDB_LE_FUNC:
            // NdbInterpreter incorrectly compare '<NULL> < f2' as 'true'
            // -> NULL filter f1

          case NDB_LIKE_FUNC:
          case NDB_NOTLIKE_FUNC:
            // NdbInterpreter incorrectly compare '<NULL> [not] like <value>' as
            // 'true'
            // -> NULL filter f1
            if (field1_maybe_null) {
              DBUG_PRINT("info", ("Appending extra field1 ISNOTNULL check"));
              if (filter->begin(NdbScanFilter::AND) == -1 ||
                  filter->isnotnull(field1->get_field_no()) == -1)
                return 1;
              added_null_check = true;
            }
            break;

          case NDB_EQ_FUNC:
            // NdbInterpreter incorrectly compare <NULL> = <NULL> as 'true'
            // -> At least either f1 or f2 need a NULL filter to ensure
            //    not both are NULL.
            if (!field1_maybe_null) break;
            // Fall through to check 'field2_maybe_null'

          case NDB_GE_FUNC:
          case NDB_GT_FUNC:
            // NdbInterpreter incorrectly compare f1 > <NULL> as true -> NULL
            // filter f2
            if (field2_maybe_null) {
              DBUG_PRINT("info", ("Appending extra field2 ISNOTNULL check"));
              if (filter->begin(NdbScanFilter::AND) == -1 ||
                  filter->isnotnull(field2->get_field_no()) == -1)
                return 1;
              added_null_check = true;
            }
            break;

          case NDB_NE_FUNC:
            // f1 '<>' f2 -> f1 < f2 or f2 < f1: Both f1 and f2 need NULL
            // filters
            DBUG_PRINT("info",
                       ("Appending extra field1 & field2 ISNOTNULL check"));
            if (filter->begin(NdbScanFilter::AND) == -1 ||
                (field1_maybe_null &&
                 filter->isnotnull(field1->get_field_no()) == -1) ||
                (field2_maybe_null &&
                 filter->isnotnull(field2->get_field_no()) == -1))
              return 1;
            added_null_check = true;
            break;

          default:
            break;
        }
      }

      NdbScanFilter::BinaryCondition cond;
      switch (function_type) {
        case NDB_EQ_FUNC: {
          DBUG_PRINT("info", ("Generating EQ filter"));
          cond = NdbScanFilter::COND_EQ;
          break;
        }
        case NDB_NE_FUNC: {
          DBUG_PRINT("info", ("Generating NE filter"));
          cond = NdbScanFilter::COND_NE;
          break;
        }
        case NDB_LT_FUNC: {
          DBUG_PRINT("info", ("Generating LT filter"));
          cond = NdbScanFilter::COND_LT;
          break;
        }
        case NDB_LE_FUNC: {
          DBUG_PRINT("info", ("Generating LE filter"));
          cond = NdbScanFilter::COND_LE;
          break;
        }
        case NDB_GE_FUNC: {
          DBUG_PRINT("info", ("Generating GE filter"));
          cond = NdbScanFilter::COND_GE;
          break;
        }
        case NDB_GT_FUNC: {
          DBUG_PRINT("info", ("Generating GT filter"));
          cond = NdbScanFilter::COND_GT;
          break;
        }
        case NDB_LIKE_FUNC: {
          DBUG_PRINT("info", ("Generating LIKE filter"));
          cond = NdbScanFilter::COND_LIKE;
          break;
        }
        case NDB_NOTLIKE_FUNC: {
          DBUG_PRINT("info", ("Generating NOT LIKE filter"));
          cond = NdbScanFilter::COND_NOT_LIKE;
          break;
        }
        case NDB_ISNULL_FUNC: {
          DBUG_PRINT("info", ("Generating ISNULL filter"));
          if (filter->isnull(field1->get_field_no()) == -1) return 1;
          return 0;
        }
        case NDB_ISNOTNULL_FUNC: {
          DBUG_PRINT("info", ("Generating ISNOTNULL filter"));
          if (filter->isnotnull(field1->get_field_no()) == -1) return 1;
          return 0;
        }
        default:
          DBUG_ASSERT(false);
          return 1;
      }

      if (cond <= NdbScanFilter::COND_NE) {
        if (value != nullptr) {
          // Save value in right format for the field type
          if (unlikely(value->save_in_field(field1) == -1)) return 1;
          if (filter->cmp(cond, field1->get_field_no(), field1->get_val(),
                          field1->pack_length()) == -1)
            return 1;
        } else {
          DBUG_ASSERT(field2 != nullptr);
          DBUG_ASSERT(ndbd_support_column_cmp(
              get_thd_ndb(current_thd)->ndb->getMinDbNodeVersion()));
          if (filter->cmp(cond, field1->get_field_no(),
                          field2->get_field_no()) == -1)
            return 1;
        }
      } else  // [NOT] LIKE
      {
        DBUG_ASSERT(cond == NdbScanFilter::COND_LIKE ||
                    cond == NdbScanFilter::COND_NOT_LIKE);
        DBUG_ASSERT(field1 == a && value == b);

        char buff[MAX_FIELD_WIDTH];
        String str(buff, sizeof(buff), field1->get_field_charset());
        Item *value_item = const_cast<Item *>(value->get_item());
        const String *pattern = value_item->val_str(&str);

        if (filter->cmp(cond, field1->get_field_no(), pattern->ptr(),
                        pattern->length()) == -1)
          return 1;
      }

      if (added_null_check && filter->end() == -1)  // Local AND group
        return 1;
      return 0;
    }
    default:
      break;
  }
  DBUG_PRINT("info", ("Found illegal condition"));
  return 1;
}

int ha_ndbcluster_cond::build_scan_filter_group(
    List_iterator<const Ndb_item> &cond, NdbScanFilter *filter,
    const bool negated) const {
  uint level = 0;
  DBUG_TRACE;

  do {
    const Ndb_item *ndb_item = cond++;
    if (ndb_item == nullptr) return 1;
    switch (ndb_item->type) {
      case NDB_FUNCTION: {
        switch (ndb_item->get_func_type()) {
          case NDB_COND_AND_FUNC: {
            level++;
            DBUG_PRINT("info", ("Generating %s group %u",
                                (negated) ? "OR" : "AND", level));
            if ((negated) ? filter->begin(NdbScanFilter::OR)
                          : filter->begin(NdbScanFilter::AND) == -1)
              return 1;
            break;
          }
          case NDB_COND_OR_FUNC: {
            level++;
            DBUG_PRINT("info", ("Generating %s group %u",
                                (negated) ? "AND" : "OR", level));
            if ((negated) ? filter->begin(NdbScanFilter::AND)
                          : filter->begin(NdbScanFilter::OR) == -1)
              return 1;
            break;
          }
          case NDB_NOT_FUNC: {
            DBUG_PRINT("info", ("Generating negated query"));
            if (build_scan_filter_group(cond, filter, !negated)) return 1;
            break;
          }
          default:
            if (build_scan_filter_predicate(cond, filter, negated)) return 1;
            break;
        }
        break;
      }
      case NDB_VALUE: {
        // (Boolean-)VALUE known at generate
        const Item *item = ndb_item->get_item();
#ifndef DBUG_OFF
        String str;
        item->print(current_thd, &str, QT_ORDINARY);
#endif
        if (const_cast<Item *>(item)->is_null()) {
          // Note that boolean 'unknown' -> 'not true'
          DBUG_PRINT("info", ("BOOLEAN value 'UNKNOWN', expression '%s'",
                              str.c_ptr_safe()));
          if (filter->begin(NdbScanFilter::AND) == -1 ||
              filter->isfalse() == -1 || filter->end() == -1)
            return 1;
        } else if (const_cast<Item *>(item)->val_bool() == !negated) {
          DBUG_PRINT("info", ("BOOLEAN value 'TRUE', expression '%s'",
                              str.c_ptr_safe()));
          if (filter->begin(NdbScanFilter::OR) == -1 ||
              filter->istrue() == -1 || filter->end() == -1)
            return 1;
        } else {
          DBUG_PRINT("info", ("BOOLEAN value 'FALSE', expression '%s'",
                              str.c_ptr_safe()));
          if (filter->begin(NdbScanFilter::AND) == -1 ||
              filter->isfalse() == -1 || filter->end() == -1)
            return 1;
        }
        break;
      }
      case NDB_END_COND:
        DBUG_PRINT("info", ("End of group %u", level));
        level--;
        if (filter->end() == -1) return 1;
        break;
      default: {
        DBUG_PRINT("info", ("Illegal scan filter"));
        DBUG_ASSERT(false);
        return 1;
      }
    }
  } while (level > 0);

  return 0;
}

int ha_ndbcluster_cond::generate_scan_filter_from_cond(NdbScanFilter &filter) {
  bool need_group = true;
  DBUG_TRACE;

  // Determine if we need to wrap an AND group around condition(s)
  const Ndb_item *ndb_item = m_ndb_cond.head();
  if (ndb_item->type == NDB_FUNCTION) {
    switch (ndb_item->get_func_type()) {
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

  if (need_group && filter.begin() == -1) return 1;

  List_iterator<const Ndb_item> cond(m_ndb_cond);
  if (build_scan_filter_group(cond, &filter, false)) {
    DBUG_PRINT("info", ("build_scan_filter_group failed"));

    const NdbError &err = filter.getNdbError();
    if (err.code == NdbScanFilter::FilterTooLarge) {
      DBUG_PRINT("info", ("%s", err.message));
      push_warning(current_thd, Sql_condition::SL_WARNING, err.code,
                   err.message);
    }
    return 1;
  }
  if (need_group && filter.end() == -1) return 1;

  return 0;
}

/*
  Optimizer sometimes does hash index lookup of a key where some
  key parts are null.  The set of cases where this happens makes
  no sense but cannot be ignored since optimizer may expect the result
  to be filtered accordingly.  The scan is actually on the table and
  the index bounds are pushed down.
*/
int ha_ndbcluster_cond::generate_scan_filter_from_key(
    NdbScanFilter &filter, const KEY *key_info, const key_range *start_key,
    const key_range *end_key) {
  DBUG_TRACE;

#ifndef DBUG_OFF
  {
    DBUG_PRINT("info",
               ("key parts:%u length:%u", key_info->user_defined_key_parts,
                key_info->key_length));
    const key_range *keylist[2] = {start_key, end_key};
    for (uint j = 0; j <= 1; j++) {
      char buf[8192];
      const key_range *key = keylist[j];
      if (key == 0) {
        sprintf(buf, "key range %u: none", j);
      } else {
        sprintf(buf, "key range %u: flag:%u part", j, key->flag);
        const KEY_PART_INFO *key_part = key_info->key_part;
        const uchar *ptr = key->key;
        for (uint i = 0; i < key_info->user_defined_key_parts; i++) {
          sprintf(buf + strlen(buf), " %u:", i);
          for (uint k = 0; k < key_part->store_length; k++) {
            sprintf(buf + strlen(buf), " %02x", ptr[k]);
          }
          ptr += key_part->store_length;
          if (ptr - key->key >= (ptrdiff_t)key->length) {
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

  do {
    /*
      Case "x is not null".
      Seen with index(x) where it becomes range "null < x".
      Not seen with index(x,y) for any combination of bounds
      which include "is not null".
    */
    if (start_key != 0 && start_key->flag == HA_READ_AFTER_KEY &&
        end_key == 0 && key_info->user_defined_key_parts == 1) {
      const KEY_PART_INFO *key_part = key_info->key_part;
      if (key_part->null_bit != 0)  // nullable (must be)
      {
        const uchar *ptr = start_key->key;
        if (ptr[0] != 0)  // null (in "null < x")
        {
          DBUG_PRINT("info", ("Generating ISNOTNULL filter for nullable %s",
                              key_part->field->field_name));
          if (filter.isnotnull(key_part->fieldnr - 1) == -1) return 1;
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
    if (start_key != 0 && start_key->flag == HA_READ_KEY_EXACT &&
        end_key != 0 && end_key->flag == HA_READ_AFTER_KEY &&
        start_key->length == end_key->length &&
        memcmp(start_key->key, end_key->key, start_key->length) == 0) {
      const KEY_PART_INFO *key_part = key_info->key_part;
      const uchar *ptr = start_key->key;
      for (uint i = 0; i < key_info->user_defined_key_parts; i++) {
        const Field *field = key_part->field;
        if (key_part->null_bit)  // nullable
        {
          if (ptr[0] != 0)  // null
          {
            DBUG_PRINT("info", ("Generating ISNULL filter for nullable %s",
                                field->field_name));
            if (filter.isnull(key_part->fieldnr - 1) == -1) return 1;
          } else {
            DBUG_PRINT("info", ("Generating EQ filter for nullable %s",
                                field->field_name));
            if (filter.cmp(NdbScanFilter::COND_EQ, key_part->fieldnr - 1,
                           ptr + 1,  // skip null-indicator byte
                           field->pack_length()) == -1)
              return 1;
          }
        } else {
          DBUG_PRINT("info", ("Generating EQ filter for non-nullable %s",
                              field->field_name));
          if (filter.cmp(NdbScanFilter::COND_EQ, key_part->fieldnr - 1, ptr,
                         field->pack_length()) == -1)
            return 1;
        }
        ptr += key_part->store_length;
        if (ptr - start_key->key >= (ptrdiff_t)start_key->length) {
          break;
        }
        key_part++;
      }
      break;
    }

    DBUG_PRINT("info", ("Unknown hash index scan"));
    // Catch new cases when optimizer changes
    DBUG_ASSERT(false);
  } while (0);

  return 0;
}

/**
  In case we failed to 'generate' a scan filter accepted by 'cond_push',
  or we later choose to ignore it, set_condition() will set the condition
  to be evaluated by the handler.

  @param cond     The condition to be evaluated by the handler
*/
void ha_ndbcluster_cond::set_condition(const Item *cond) {
  m_unpushed_cond = cond;
}

/**
  Return the boolean value of a condition previously set by 'set_condition',
  evaluated on the current row.

  @return    true if the condition is evaluated to true.
*/
bool ha_ndbcluster_cond::eval_condition() const {
  return const_cast<Item *>(m_unpushed_cond)->val_int() == 1;
}

/**
  Add any columns referred by 'cond' to the read_set of the table.

  @param table  The table to update the read_set for.
  @param cond   The condition referring columns in 'table'
*/
void ha_ndbcluster_cond::add_read_set(TABLE *table, const Item *cond) {
  if (cond != nullptr) {
    Mark_field mf(table, MARK_COLUMNS_READ);
    const_cast<Item *>(cond)->walk(&Item::mark_field_in_map, enum_walk::PREFIX,
                                   (uchar *)&mf);
  }
}

/*
  Interface layer between ha_ndbcluster and ha_ndbcluster_cond
*/
void ha_ndbcluster::generate_scan_filter(
    NdbInterpretedCode *code, NdbScanOperation::ScanOptions *options) {
  DBUG_TRACE;

  if (pushed_cond == nullptr) {
    DBUG_PRINT("info", ("Empty stack"));
    return;
  }

  if (m_cond.get_interpreter_code().getWordsUsed() > 0) {
    /**
     * We had already generated the NdbInterpreterCode for the scan_filter.
     * Just use what we had.
     */
    if (options != nullptr) {
      options->interpretedCode = &m_cond.get_interpreter_code();
      options->optionsPresent |= NdbScanOperation::ScanOptions::SO_INTERPRETED;
    } else {
      code->copy(m_cond.get_interpreter_code());
    }
    return;
  }

  // Generate the scan_filter from previously 'serialized' condition code
  NdbScanFilter filter(code);
  const int ret = m_cond.generate_scan_filter_from_cond(filter);
  if (unlikely(ret != 0)) {
    /**
     * Failed to generate a scan filter, fallback to let
     * ha_ndbcluster evaluate the condition.
     */
    m_cond.set_condition(pushed_cond);
  } else if (options != nullptr) {
    options->interpretedCode = code;
    options->optionsPresent |= NdbScanOperation::ScanOptions::SO_INTERPRETED;
  }
}

int ha_ndbcluster::generate_scan_filter_with_key(
    NdbInterpretedCode *code, NdbScanOperation::ScanOptions *options,
    const KEY *key_info, const key_range *start_key, const key_range *end_key) {
  DBUG_TRACE;

  NdbScanFilter filter(code);
  if (filter.begin(NdbScanFilter::AND) == -1) return 1;

  // Generate a scanFilter from a prepared pushed conditions
  if (pushed_cond != nullptr) {
    /**
     * Note, that in this case we can not use the pre-generated scan_filter,
     * as it does not contain the code for the additional 'key'.
     */
    const int ret = m_cond.generate_scan_filter_from_cond(filter);
    if (unlikely(ret != 0)) {
      /**
       * Failed to generate a scan filter, fallback to let
       * ha_ndbcluster evaluate the condition.
       */
      m_cond.set_condition(pushed_cond);

      // Discard the failed scanFilter and prepare for 'key'
      filter.reset();
      if (filter.begin(NdbScanFilter::AND) == -1) return 1;
    }
  }

  // Generate a scanFilter from the key definition
  if (key_info != nullptr) {
    const int ret = ha_ndbcluster_cond::generate_scan_filter_from_key(
        filter, key_info, start_key, end_key);
    if (unlikely(ret != 0)) return ret;
  }

  if (filter.end() == -1) return 1;

  if (options != nullptr) {
    options->interpretedCode = code;
    options->optionsPresent |= NdbScanOperation::ScanOptions::SO_INTERPRETED;
  }

  return 0;
}
