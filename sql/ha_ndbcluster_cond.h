/*
   Copyright (C) 2000-2007 MySQL AB
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*
  This file defines the data structures used by engine condition pushdown in
  the NDB Cluster handler
*/

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
  Item::Type value_type; 
  enum_field_types field_type; // Instead of Item::FIELD_ITEM
  NDB_FUNC_TYPE function_type; // Instead of Item::FUNC_ITEM
} NDB_ITEM_QUALIFICATION;

typedef struct ndb_item_field_value {
  Field* field;
  int column_no;
} NDB_ITEM_FIELD_VALUE;

typedef union ndb_item_value {
  const Item *item;
  NDB_ITEM_FIELD_VALUE *field_value;
  uint arg_count;
} NDB_ITEM_VALUE;

struct negated_function_mapping
{
  NDB_FUNC_TYPE pos_fun;
  NDB_FUNC_TYPE neg_fun;
};

/*
  Define what functions can be negated in condition pushdown.
  Note, these HAVE to be in the same order as in definition enum
*/
static const negated_function_mapping neg_map[]= 
{
  {NDB_EQ_FUNC, NDB_NE_FUNC},
  {NDB_NE_FUNC, NDB_EQ_FUNC},
  {NDB_LT_FUNC, NDB_GE_FUNC},
  {NDB_LE_FUNC, NDB_GT_FUNC},
  {NDB_GT_FUNC, NDB_LE_FUNC},
  {NDB_GE_FUNC, NDB_LT_FUNC},
  {NDB_ISNULL_FUNC, NDB_ISNOTNULL_FUNC},
  {NDB_ISNOTNULL_FUNC, NDB_ISNULL_FUNC},
  {NDB_LIKE_FUNC, NDB_NOTLIKE_FUNC},
  {NDB_NOTLIKE_FUNC, NDB_LIKE_FUNC},
  {NDB_NOT_FUNC, NDB_UNSUPPORTED_FUNC},
  {NDB_UNKNOWN_FUNC, NDB_UNSUPPORTED_FUNC},
  {NDB_COND_AND_FUNC, NDB_UNSUPPORTED_FUNC},
  {NDB_COND_OR_FUNC, NDB_UNSUPPORTED_FUNC},
  {NDB_UNSUPPORTED_FUNC, NDB_UNSUPPORTED_FUNC}
};
  
/*
  This class is the construction element for serialization of Item tree 
  in condition pushdown.
  An instance of Ndb_Item represents a constant, table field reference,
  unary or binary comparison predicate, and start/end of AND/OR.
  Instances of Ndb_Item are stored in a linked list implemented by Ndb_cond
  class.
  The order of elements produced by Ndb_cond::next corresponds to
  breadth-first traversal of the Item (i.e. expression) tree in prefix order.
  AND and OR have arbitrary arity, so the end of AND/OR group is marked with  
  Ndb_item with type == NDB_END_COND.
  NOT items represent negated conditions and generate NAND/NOR groups.
*/
class Ndb_item : public Sql_alloc
{
public:
  Ndb_item(NDB_ITEM_TYPE item_type) : type(item_type) {};
  Ndb_item(NDB_ITEM_TYPE item_type, 
           NDB_ITEM_QUALIFICATION item_qualification,
           const Item *item_value)
    : type(item_type), qualification(item_qualification)
  { 
    switch(item_type) {
    case(NDB_VALUE):
      value.item= item_value;
      break;
    case(NDB_FIELD): {
      NDB_ITEM_FIELD_VALUE *field_value= new NDB_ITEM_FIELD_VALUE();
      Item_field *field_item= (Item_field *) item_value;
      field_value->field= field_item->field;
      field_value->column_no= -1; // Will be fetched at scan filter generation
      value.field_value= field_value;
      break;
    }
    case(NDB_FUNCTION):
      value.item= item_value;
      value.arg_count= ((Item_func *) item_value)->argument_count();
      break;
    case(NDB_END_COND):
      break;
    }
  };
  Ndb_item(Field *field, int column_no) : type(NDB_FIELD)
  {
    NDB_ITEM_FIELD_VALUE *field_value= new NDB_ITEM_FIELD_VALUE();
    qualification.field_type= field->real_type();
    field_value->field= field;
    field_value->column_no= column_no;
    value.field_value= field_value;
  };
  Ndb_item(Item_func::Functype func_type, const Item *item_value) 
    : type(NDB_FUNCTION)
  {
    qualification.function_type= item_func_to_ndb_func(func_type);
    value.item= item_value;
    value.arg_count= ((Item_func *) item_value)->argument_count();
  };
  Ndb_item(Item_func::Functype func_type, uint no_args) 
    : type(NDB_FUNCTION)
  {
    qualification.function_type= item_func_to_ndb_func(func_type);
    value.arg_count= no_args;
  };
  ~Ndb_item()
  { 
    if (type == NDB_FIELD)
      {
        delete value.field_value;
        value.field_value= NULL;
      }
  };

  uint32 pack_length() 
  { 
    switch(type) {
    case(NDB_VALUE):
      if(qualification.value_type == Item::STRING_ITEM)
        return value.item->str_value.length();
      break;
    case(NDB_FIELD):
      return value.field_value->field->pack_length(); 
    default:
      break;
    }
    
    return 0;
  };

  Field * get_field() { return value.field_value->field; };

  int get_field_no() { return value.field_value->column_no; };

  int argument_count() 
  { 
    return value.arg_count;
  };

  const char* get_val() 
  {  
    switch(type) {
    case(NDB_VALUE):
      if(qualification.value_type == Item::STRING_ITEM)
        return value.item->str_value.ptr();
      break;
    case(NDB_FIELD):
      return (char*) value.field_value->field->ptr; 
    default:
      break;
    }
    
    return NULL;
  };

  void save_in_field(Ndb_item *field_item)
  {
    DBUG_ENTER("save_in_field");
    Field *field = field_item->value.field_value->field;
    const Item *item= value.item;
    if (item && field)
    {
      DBUG_PRINT("info", ("item length %u, field length %u",
                          item->max_length, field->field_length));
      if (item->max_length > field->field_length)
      {
        DBUG_PRINT("info", ("Comparing field with longer value"));
        DBUG_PRINT("info", ("Field can store %u", field->field_length));
      }
      my_bitmap_map *old_map=
        dbug_tmp_use_all_columns(field->table, field->table->write_set);
      ((Item *)item)->save_in_field(field, FALSE);
      dbug_tmp_restore_column_map(field->table->write_set, old_map);
    }
    DBUG_VOID_RETURN;
  };

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
    case (Item_func::UNKNOWN_FUNC): { return NDB_UNKNOWN_FUNC; }
    case (Item_func::COND_AND_FUNC): { return NDB_COND_AND_FUNC; }
    case (Item_func::COND_OR_FUNC): { return NDB_COND_OR_FUNC; }
    default: { return NDB_UNSUPPORTED_FUNC; }
    }
  };

  static NDB_FUNC_TYPE negate(NDB_FUNC_TYPE fun)
  {
    uint i= (uint) fun;
    DBUG_ASSERT(fun == neg_map[i].pos_fun);
    return  neg_map[i].neg_fun;
  };

  NDB_ITEM_TYPE type;
  NDB_ITEM_QUALIFICATION qualification;
 private:
  NDB_ITEM_VALUE value;
};

/*
  This class implements a linked list used for storing a
  serialization of the Item tree for condition pushdown.
 */
class Ndb_cond : public Sql_alloc
{
 public:
  Ndb_cond() : ndb_item(NULL), next(NULL), prev(NULL) {};
  ~Ndb_cond() 
  { 
    if (ndb_item) delete ndb_item; 
    ndb_item= NULL;
    /*
      First item in the linked list deletes all in a loop
      Note - doing it recursively causes stack issues for
      big IN clauses
    */
    Ndb_cond *n= next;
    while (n)
    {
      Ndb_cond *tmp= n;
      n= n->next;
      tmp->next= NULL;
      delete tmp;
    }
    next= prev= NULL; 
  };
  Ndb_item *ndb_item;
  Ndb_cond *next;
  Ndb_cond *prev;
};

/*
  This class implements a stack for storing several conditions
  for pushdown (represented as serialized Item trees using Ndb_cond).
  The current implementation only pushes one condition, but is
  prepared for handling several (C1 AND C2 ...) if the logic for 
  pushing conditions is extended in sql_select.
*/
class Ndb_cond_stack : public Sql_alloc
{
 public:
  Ndb_cond_stack() : ndb_cond(NULL), next(NULL) {};
  ~Ndb_cond_stack() 
  { 
    if (ndb_cond) delete ndb_cond; 
    ndb_cond= NULL; 
    if (next) delete next;
    next= NULL; 
  };
  Ndb_cond *ndb_cond;
  Ndb_cond_stack *next;
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
class Ndb_expect_stack : public Sql_alloc
{
  static const uint MAX_EXPECT_ITEMS = Item::VIEW_FIXER_ITEM + 1;
  static const uint MAX_EXPECT_FIELD_TYPES = MYSQL_TYPE_GEOMETRY + 1;
  static const uint MAX_EXPECT_FIELD_RESULTS = DECIMAL_RESULT + 1;
 public:
Ndb_expect_stack(): collation(NULL), length(0), max_length(0), next(NULL) 
  {
    // Allocate type checking bitmaps using fixed size buffers
    // since max size is known at compile time
    bitmap_init(&expect_mask, m_expect_buf,
                MAX_EXPECT_ITEMS, FALSE);
    bitmap_init(&expect_field_type_mask, m_expect_field_type_buf,
                MAX_EXPECT_FIELD_TYPES, FALSE);
    bitmap_init(&expect_field_result_mask, m_expect_field_result_buf,
                MAX_EXPECT_FIELD_RESULTS, FALSE);
  };
  ~Ndb_expect_stack()
  {
    if (next)
      delete next;
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
      next= next->next;
      delete expect_next;
    }
  }
  void expect(Item::Type type)
  {
    bitmap_set_bit(&expect_mask, (uint) type);
    if (type == Item::FIELD_ITEM)
      expect_all_field_types();
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

  void expect_field_type(enum_field_types type)
  {
    bitmap_set_bit(&expect_field_type_mask, (uint) type);
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

class Ndb_rewrite_context : public Sql_alloc
{
public:
  Ndb_rewrite_context(Item_func *func) 
    : func_item(func), left_hand_item(NULL), count(0) {};
  ~Ndb_rewrite_context()
  {
    if (next) delete next;
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
class Ndb_cond_traverse_context : public Sql_alloc
{
 public:
   Ndb_cond_traverse_context(TABLE *tab, const NdbDictionary::Table *ndb_tab,
			     Ndb_cond_stack* stack)
    : table(tab), ndb_table(ndb_tab), 
    supported(TRUE), cond_stack(stack), cond_ptr(NULL),
    skip(0), rewrite_stack(NULL)
  { 
   if (stack)
      cond_ptr= stack->ndb_cond;
  }
  ~Ndb_cond_traverse_context()
  {
    if (rewrite_stack) delete rewrite_stack;
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
  

  TABLE* table;
  const NdbDictionary::Table *ndb_table;
  bool supported;
  Ndb_cond_stack* cond_stack;
  Ndb_cond* cond_ptr;
  Ndb_expect_stack expect_stack;
  uint skip;
  Ndb_rewrite_context *rewrite_stack;
};

class ha_ndbcluster;

class ha_ndbcluster_cond
{
public:
  ha_ndbcluster_cond() 
  : m_cond_stack(NULL)
  {}
  ~ha_ndbcluster_cond() 
  { if (m_cond_stack) delete m_cond_stack; }
  const Item *cond_push(const Item *cond, 
                        TABLE *table, const NdbDictionary::Table *ndb_table);
  void cond_pop();
  void cond_clear();
  int generate_scan_filter(NdbInterpretedCode* code, 
                           NdbScanOperation::ScanOptions* options);
  int generate_scan_filter_from_cond(NdbScanFilter& filter);
  int generate_scan_filter_from_key(NdbInterpretedCode* code,
                                    NdbScanOperation::ScanOptions* options,
                                    const KEY* key_info, 
                                    const key_range *start_key,
                                    const key_range *end_key,
                                    uchar *buf);
private:
  bool serialize_cond(const Item *cond, Ndb_cond_stack *ndb_cond,
		      TABLE *table, const NdbDictionary::Table *ndb_table);
  int build_scan_filter_predicate(Ndb_cond* &cond, 
                                  NdbScanFilter* filter,
                                  bool negated= false);
  int build_scan_filter_group(Ndb_cond* &cond, 
                              NdbScanFilter* filter);
  int build_scan_filter(Ndb_cond* &cond, NdbScanFilter* filter);

  Ndb_cond_stack *m_cond_stack;
};
