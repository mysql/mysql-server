/* Copyright (c) 2011, 2018, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file sql/sql_tmp_table.cc
  Temporary tables implementation.
*/

#include "sql/sql_tmp_table.h"

#include <fcntl.h>
#include <stdio.h>
#include <algorithm>
#include <cstring>
#include <new>

#include "binary_log_types.h"
#include "lex_string.h"
#include "m_ctype.h"
#include "m_string.h"
#include "my_alloc.h"
#include "my_bitmap.h"
#include "my_compare.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_macros.h"
#include "my_pointer_arithmetic.h"
#include "my_sys.h"
#include "myisam.h"  // MI_COLUMNDEF
#include "mysql/plugin.h"
#include "mysql/udf_registration_types.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "sql/current_thd.h"
#include "sql/debug_sync.h"  // DEBUG_SYNC
#include "sql/field.h"
#include "sql/filesort.h"  // filesort_free_buffers
#include "sql/gis/srid.h"
#include "sql/handler.h"
#include "sql/item_func.h"  // Item_func
#include "sql/item_sum.h"   // Item_sum
#include "sql/key.h"
#include "sql/mem_root_array.h"     // Mem_root_array
#include "sql/mysqld.h"             // heap_hton
#include "sql/opt_range.h"          // QUICK_SELECT_I
#include "sql/opt_trace.h"          // Opt_trace_object
#include "sql/opt_trace_context.h"  // Opt_trace_context
#include "sql/psi_memory_key.h"
#include "sql/query_options.h"
#include "sql/sql_base.h"   // free_io_cache
#include "sql/sql_class.h"  // THD
#include "sql/sql_const.h"
#include "sql/sql_executor.h"  // SJ_TMP_TABLE
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_opt_exec_shared.h"
#include "sql/sql_plugin.h"  // plugin_unlock
#include "sql/sql_plugin_ref.h"
#include "sql/sql_select.h"
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/temp_table_param.h"
#include "sql/thr_malloc.h"
#include "sql/window.h"
#include "template_utils.h"

using std::max;
using std::min;
static bool setup_tmp_table_handler(TABLE *table, ulonglong select_options,
                                    bool force_disk_table, bool schema_table);
static bool alloc_record_buffers(TABLE *table);

/****************************************************************************
  Create internal temporary table
****************************************************************************/

/**
  Create field for temporary table from given field.

  @param thd	       Thread handler
  @param org_field    field from which new field will be created
  @param name         New field name
  @param table	       Temporary table
  @param item	       !=NULL if item->result_field should point to new field.
                      This is relevant for how fill_record() is going to work:
                      If item != NULL then fill_record() will update
                      the record in the original table.
                      If item == NULL then fill_record() will update
                      the temporary table

  @retval
    NULL		on error
  @retval
    new_created field
*/

Field *create_tmp_field_from_field(THD *thd, Field *org_field, const char *name,
                                   TABLE *table, Item_field *item) {
  Field *new_field;

  new_field =
      org_field->new_field(thd->mem_root, table, table == org_field->table);
  if (new_field) {
    new_field->init(table);
    new_field->orig_table = org_field->table;
    if (item)
      item->result_field = new_field;
    else
      new_field->field_name = name;
    new_field->flags |= (org_field->flags & NO_DEFAULT_VALUE_FLAG);
    if (org_field->maybe_null() || (item && item->maybe_null))
      new_field->flags &= ~NOT_NULL_FLAG;  // Because of outer join
    if (org_field->type() == FIELD_TYPE_DOUBLE)
      ((Field_double *)new_field)->not_fixed = true;
    /*
      This field will belong to an internal temporary table, it cannot be
      generated.
    */
    new_field->gcol_info = NULL;
    new_field->stored_in_db = true;
  }
  return new_field;
}

/**
  Create field for temporary table using type of given item.

  @param item                  Item to create a field for
  @param table                 Temporary table
  @param copy_func             If set and item is a function, store copy of
                               item in this array
  @param modify_item           1 if item->result_field should point to new
                               item. This is relevent for how fill_record()
                               is going to work:
                               If modify_item is 1 then fill_record() will
                               update the record in the original table.
                               If modify_item is 0 then fill_record() will
                               update the temporary table

  @retval
    0  on error
  @retval
    new_created field
*/

static Field *create_tmp_field_from_item(Item *item, TABLE *table,
                                         Func_ptr_array *copy_func,
                                         bool modify_item) {
  bool maybe_null = item->maybe_null;
  Field *new_field = NULL;

  switch (item->result_type()) {
    case REAL_RESULT:
      new_field = new (*THR_MALLOC)
          Field_double(item->max_length, maybe_null, item->item_name.ptr(),
                       item->decimals, true);
      break;
    case INT_RESULT:
      /*
        Select an integer type with the minimal fit precision.
        MY_INT32_NUM_DECIMAL_DIGITS is sign inclusive, don't consider the sign.
        Values with MY_INT32_NUM_DECIMAL_DIGITS digits may or may not fit into
        Field_long : make them Field_longlong.
      */
      if (item->max_length >= (MY_INT32_NUM_DECIMAL_DIGITS - 1))
        new_field = new (*THR_MALLOC)
            Field_longlong(item->max_length, maybe_null, item->item_name.ptr(),
                           item->unsigned_flag);
      else
        new_field = new (*THR_MALLOC)
            Field_long(item->max_length, maybe_null, item->item_name.ptr(),
                       item->unsigned_flag);
      break;
    case STRING_RESULT:
      DBUG_ASSERT(item->collation.collation);

      /*
        DATE/TIME, GEOMETRY and JSON fields have STRING_RESULT result type.
        To preserve type they needed to be handled separately.
      */
      if (item->is_temporal() || item->data_type() == MYSQL_TYPE_GEOMETRY ||
          item->data_type() == MYSQL_TYPE_JSON) {
        new_field = item->tmp_table_field_from_field_type(table, 1);
      } else {
        new_field = item->make_string_field(table);
      }
      new_field->set_derivation(item->collation.derivation);
      break;
    case DECIMAL_RESULT:
      new_field = Field_new_decimal::create_from_item(item);
      break;
    case ROW_RESULT:
    default:
      // This case should never be choosen
      DBUG_ASSERT(0);
      new_field = 0;
      break;
  }
  if (new_field) new_field->init(table);

  /*
    If the item is a function, a pointer to the item is stored in
    copy_func. We separate fields from functions by checking if the
    item is a result field item. The real_item() must be checked to
    avoid falsely identifying Item_ref and its subclasses as functions
    when they refer to field-like items, such as Item_copy and
    subclasses. References to true fields have already been untangled
    in the beginning of create_tmp_field().
   */
  if (copy_func && item->real_item()->is_result_field())
    copy_func->push_back(item);
  if (modify_item) item->set_result_field(new_field);
  if (item->type() == Item::NULL_ITEM)
    new_field->is_created_from_null_item = true;
  return new_field;
}

/**
  Create field for information schema table.

  @param table		Temporary table
  @param item		Item to create a field for

  @retval
    0			on error
  @retval
    new_created field
*/

static Field *create_tmp_field_for_schema(Item *item, TABLE *table) {
  if (item->data_type() == MYSQL_TYPE_VARCHAR) {
    Field *field;
    if (item->max_length > MAX_FIELD_VARCHARLENGTH)
      field = new (*THR_MALLOC)
          Field_blob(item->max_length, item->maybe_null, item->item_name.ptr(),
                     item->collation.collation, false);
    else {
      field = new (*THR_MALLOC) Field_varstring(
          item->max_length, item->maybe_null, item->item_name.ptr(), table->s,
          item->collation.collation);
      table->s->db_create_options |= HA_OPTION_PACK_RECORD;
    }
    if (field) field->init(table);
    return field;
  }
  return item->tmp_table_field_from_field_type(table, 0);
}

/**
  Create field for temporary table.

  @param thd		Thread handler
  @param table		Temporary table
  @param item		Item to create a field for
  @param type		Type of item (normally item->type)
  @param copy_func	If set and item is a function, store copy of item
                       in this array
  @param from_field    if field will be created using other field as example,
                       pointer example field will be written here
  @param default_field	If field has a default value field, store it here
  @param group		1 if we are going to do a relative group by on result
  @param modify_item	1 if item->result_field should point to new item.
                       This is relevent for how fill_record() is going to
                       work:
                       If modify_item is 1 then fill_record() will update
                       the record in the original table.
                       If modify_item is 0 then fill_record() will update
                       the temporary table
  @param table_cant_handle_bit_fields
  @param make_copy_field
  @param copy_result_field true <=> save item's result_field in the from_field
                       arg, before changing it. This is used for a window's
                       OUT table when window uses frame buffer to copy a
                       function's result field from OUT table to frame buffer
                       (and back). @note that the goals of 'from_field' when
                       this argument is true and when it is false, are
                       different.

  @retval NULL On error.

  @retval new_created field
*/

Field *create_tmp_field(THD *thd, TABLE *table, Item *item, Item::Type type,
                        Func_ptr_array *copy_func, Field **from_field,
                        Field **default_field, bool group, bool modify_item,
                        bool table_cant_handle_bit_fields, bool make_copy_field,
                        bool copy_result_field) {
  DBUG_ENTER("create_tmp_field");
  Field *result = NULL;
  Item::Type orig_type = type;
  Item *orig_item = 0;

  if (type != Item::FIELD_ITEM &&
      item->real_item()->type() == Item::FIELD_ITEM) {
    orig_item = item;
    item = item->real_item();
    type = Item::FIELD_ITEM;
  }

  bool is_wf =
      type == Item::SUM_FUNC_ITEM && item->real_item()->m_is_window_function;

  switch (type) {
    case Item::FIELD_ITEM:
    case Item::DEFAULT_VALUE_ITEM:
    case Item::TRIGGER_FIELD_ITEM: {
      Item_field *field = (Item_field *)item;
      bool orig_modify = modify_item;
      if (orig_type == Item::REF_ITEM) modify_item = 0;
      /*
        If item have to be able to store NULLs but underlaid field can't do it,
        create_tmp_field_from_field() can't be used for tmp field creation.
      */
      if (field->maybe_null && !field->field->maybe_null()) {
        result = create_tmp_field_from_item(item, table, NULL, modify_item);
        if (!result) break;
        *from_field = field->field;
        if (modify_item) field->result_field = result;
      } else if (table_cant_handle_bit_fields &&
                 field->field->type() == MYSQL_TYPE_BIT) {
        *from_field = field->field;
        result =
            create_tmp_field_from_item(item, table, copy_func, modify_item);
        if (!result) break;
        if (modify_item) field->result_field = result;
      } else {
        result = create_tmp_field_from_field(
            thd, (*from_field = field->field),
            orig_item ? orig_item->item_name.ptr() : item->item_name.ptr(),
            table, modify_item ? field : NULL);
        if (!result) break;
      }
      if (orig_type == Item::REF_ITEM && orig_modify)
        ((Item_ref *)orig_item)->set_result_field(result);
      /*
        Fields that are used as arguments to the DEFAULT() function already have
        their data pointers set to the default value during name resulotion. See
        Item_default_value::fix_fields.
      */
      if (orig_type != Item::DEFAULT_VALUE_ITEM && field->field->eq_def(result))
        *default_field = field->field;
      break;
    }
    /* Fall through */
    case Item::FUNC_ITEM:
      if (((Item_func *)item)->functype() == Item_func::FUNC_SP) {
        Item_func_sp *item_func_sp = (Item_func_sp *)item;
        Field *sp_result_field = item_func_sp->get_sp_result_field();

        if (make_copy_field) {
          DBUG_ASSERT(item_func_sp->result_field);
          *from_field = item_func_sp->result_field;
        } else {
          copy_func->push_back(item);
        }

        result = create_tmp_field_from_field(
            thd, sp_result_field, item_func_sp->item_name.ptr(), table, NULL);
        if (!result) break;
        if (modify_item) item->set_result_field(result);
        break;
      }

      /* Fall through */
    case Item::COND_ITEM:
    case Item::FIELD_AVG_ITEM:
    case Item::FIELD_BIT_ITEM:
    case Item::FIELD_STD_ITEM:
    case Item::FIELD_VARIANCE_ITEM:
    case Item::SUBSELECT_ITEM:
      /* The following can only happen with 'CREATE TABLE ... SELECT' */
    case Item::PROC_ITEM:
    case Item::INT_ITEM:
    case Item::REAL_ITEM:
    case Item::DECIMAL_ITEM:
    case Item::STRING_ITEM:
    case Item::REF_ITEM:
    case Item::NULL_ITEM:
    case Item::VARBIN_ITEM:
    case Item::PARAM_ITEM:
    case Item::SUM_FUNC_ITEM:
      if (type == Item::SUM_FUNC_ITEM && !is_wf) {
        Item_sum *item_sum = (Item_sum *)item;
        result = item_sum->create_tmp_field(group, table);
        if (!result) my_error(ER_OUT_OF_RESOURCES, MYF(ME_FATALERROR));
      } else {
        /*
          (2) we're windowing. The Item doesn't contain any not-yet-calculated
          window function (per logic in our caller create_tmp_table()). So it
          is an ordinary function or can be considered as such. We're creating
          the OUT table using IN table as source, and we have previously
          created a frame buffer (FB) using IN table as source. That previous
          creation has set IN's item's result_field to be the FB field. Here
          we save that FB field in from_field. Right after that,
          create_tmp_field_from_item() sets IN's item's result_field to the
          OUT field (which OUT field is the 'result' variable). We mark the
          OUT field with FIELD_IS_MARKED. Later we detect the mark, and create
          a Copy_field to from_field (FB) from the marked field (OUT). The end
          situation is: IN's item's result_field is in OUT, enabling the
          initial function evaluation and saving of its result in OUT; the
          Copy_field from OUT to FB and back will allow buffering/restoration
          of that result.
        */
        if (make_copy_field || (copy_result_field && !is_wf))  // (2)
        {
          *from_field = item->get_tmp_table_field();
          DBUG_ASSERT(*from_field);
        }

        result = create_tmp_field_from_item(
            item, table, (make_copy_field ? NULL : copy_func), modify_item);
        result->flags |= copy_result_field ? FIELD_IS_MARKED : 0;
      }
      break;
    case Item::TYPE_HOLDER:
      result = ((Item_type_holder *)item)
                   ->make_field_by_type(table, thd->is_strict_mode());
      if (!result) break;
      result->set_derivation(item->collation.derivation);
      break;
    default:  // Dosen't have to be stored
      DBUG_ASSERT(false);
      break;
  }
  DBUG_RETURN(result);
}

/*
  Set up column usage bitmaps for a temporary table

  IMPLEMENTATION
    For temporary tables, we need one bitmap with all columns set and
    a tmp_set bitmap to be used by things like filesort.
*/

static void setup_tmp_table_column_bitmaps(TABLE *table, uchar *bitmaps) {
  uint field_count = table->s->fields;
  bitmap_init(&table->def_read_set, (my_bitmap_map *)bitmaps, field_count,
              false);
  bitmap_init(&table->tmp_set,
              (my_bitmap_map *)(bitmaps + bitmap_buffer_size(field_count)),
              field_count, false);
  bitmap_init(&table->cond_set,
              (my_bitmap_map *)(bitmaps + bitmap_buffer_size(field_count) * 2),
              field_count, false);
  /* write_set and all_set are copies of read_set */
  table->def_write_set = table->def_read_set;
  table->s->all_set = table->def_read_set;
  bitmap_set_all(&table->s->all_set);
  table->default_column_bitmaps();
  table->s->column_bitmap_size = bitmap_buffer_size(field_count);
}

/**
  Cache for the storage engine properties for the alternative temporary table
  storage engines. This cache is initialized during startup of the server by
  asking the storage engines for the values properties.
*/

class Cache_temp_engine_properties {
 public:
  static uint HEAP_MAX_KEY_LENGTH;
  static uint TEMPTABLE_MAX_KEY_LENGTH;
  static uint MYISAM_MAX_KEY_LENGTH;
  static uint INNODB_MAX_KEY_LENGTH;
  static uint HEAP_MAX_KEY_PART_LENGTH;
  static uint TEMPTABLE_MAX_KEY_PART_LENGTH;
  static uint MYISAM_MAX_KEY_PART_LENGTH;
  static uint INNODB_MAX_KEY_PART_LENGTH;
  static uint HEAP_MAX_KEY_PARTS;
  static uint TEMPTABLE_MAX_KEY_PARTS;
  static uint MYISAM_MAX_KEY_PARTS;
  static uint INNODB_MAX_KEY_PARTS;

  static void init(THD *thd);
};

void Cache_temp_engine_properties::init(THD *thd) {
  handler *handler;
  plugin_ref db_plugin;

  // Cache HEAP engine's
  db_plugin = ha_lock_engine(0, heap_hton);
  handler = get_new_handler((TABLE_SHARE *)0, false, thd->mem_root, heap_hton);
  HEAP_MAX_KEY_LENGTH = handler->max_key_length();
  HEAP_MAX_KEY_PART_LENGTH = handler->max_key_part_length();
  HEAP_MAX_KEY_PARTS = handler->max_key_parts();
  destroy(handler);
  plugin_unlock(0, db_plugin);
  // Cache TempTable engine's
  db_plugin = ha_lock_engine(0, temptable_hton);
  handler =
      get_new_handler((TABLE_SHARE *)0, false, thd->mem_root, temptable_hton);
  TEMPTABLE_MAX_KEY_LENGTH = handler->max_key_length();
  TEMPTABLE_MAX_KEY_PART_LENGTH = handler->max_key_part_length();
  TEMPTABLE_MAX_KEY_PARTS = handler->max_key_parts();
  destroy(handler);
  plugin_unlock(0, db_plugin);
  // Cache MYISAM engine's
  db_plugin = ha_lock_engine(0, myisam_hton);
  handler =
      get_new_handler((TABLE_SHARE *)0, false, thd->mem_root, myisam_hton);
  MYISAM_MAX_KEY_LENGTH = handler->max_key_length();
  MYISAM_MAX_KEY_PART_LENGTH = handler->max_key_part_length();
  MYISAM_MAX_KEY_PARTS = handler->max_key_parts();
  destroy(handler);
  plugin_unlock(0, db_plugin);
  // Cache INNODB engine's
  db_plugin = ha_lock_engine(0, innodb_hton);
  handler =
      get_new_handler((TABLE_SHARE *)0, false, thd->mem_root, innodb_hton);
  INNODB_MAX_KEY_LENGTH = handler->max_key_length();
  /*
    For ha_innobase::max_supported_key_part_length(), the returned value
    is constant. However, in innodb itself, the limitation
    on key_part length is up to the ROW_FORMAT. In current trunk, internal
    temp table's ROW_FORMAT is DYNAMIC. In order to keep the consistence
    between server and innodb, here we hard-coded 3072 as the maximum of
    key_part length supported by innodb until bug#20629014 is fixed.

    TODO: Remove the hard-code here after bug#20629014 is fixed.
  */
  INNODB_MAX_KEY_PART_LENGTH = 3072;
  INNODB_MAX_KEY_PARTS = handler->max_key_parts();
  destroy(handler);
  plugin_unlock(0, db_plugin);
}

uint Cache_temp_engine_properties::HEAP_MAX_KEY_LENGTH = 0;
uint Cache_temp_engine_properties::TEMPTABLE_MAX_KEY_LENGTH = 0;
uint Cache_temp_engine_properties::MYISAM_MAX_KEY_LENGTH = 0;
uint Cache_temp_engine_properties::INNODB_MAX_KEY_LENGTH = 0;
uint Cache_temp_engine_properties::HEAP_MAX_KEY_PART_LENGTH = 0;
uint Cache_temp_engine_properties::TEMPTABLE_MAX_KEY_PART_LENGTH = 0;
uint Cache_temp_engine_properties::MYISAM_MAX_KEY_PART_LENGTH = 0;
uint Cache_temp_engine_properties::INNODB_MAX_KEY_PART_LENGTH = 0;
uint Cache_temp_engine_properties::HEAP_MAX_KEY_PARTS = 0;
uint Cache_temp_engine_properties::TEMPTABLE_MAX_KEY_PARTS = 0;
uint Cache_temp_engine_properties::MYISAM_MAX_KEY_PARTS = 0;
uint Cache_temp_engine_properties::INNODB_MAX_KEY_PARTS = 0;

/**
  Initialize the storage engine properties for the alternative temporary table
  storage engines.
*/
void init_cache_tmp_engine_properties() {
  DBUG_ASSERT(!current_thd);
  THD *thd = new THD();
  thd->thread_stack = pointer_cast<char *>(&thd);
  thd->store_globals();
  Cache_temp_engine_properties::init(thd);
  delete thd;
}

/**
  Get the minimum of max_key_length/part_length/parts.
  The minimum is between HEAP engine and internal_tmp_disk_storage_engine.

  @param[out] max_key_length Minimum of max_key_length
  @param[out] max_key_part_length Minimum of max_key_part_length
  @param[out] max_key_parts  Minimum of max_key_parts
*/

void get_max_key_and_part_length(uint *max_key_length,
                                 uint *max_key_part_length,
                                 uint *max_key_parts) {
  // Make sure these cached properties are initialized.
  DBUG_ASSERT(Cache_temp_engine_properties::HEAP_MAX_KEY_LENGTH);

  switch (internal_tmp_disk_storage_engine) {
    case TMP_TABLE_MYISAM:
      *max_key_length =
          std::min(Cache_temp_engine_properties::HEAP_MAX_KEY_LENGTH,
                   Cache_temp_engine_properties::MYISAM_MAX_KEY_LENGTH);
      *max_key_part_length =
          std::min(Cache_temp_engine_properties::HEAP_MAX_KEY_PART_LENGTH,
                   Cache_temp_engine_properties::MYISAM_MAX_KEY_PART_LENGTH);
      *max_key_parts =
          std::min(Cache_temp_engine_properties::HEAP_MAX_KEY_PARTS,
                   Cache_temp_engine_properties::MYISAM_MAX_KEY_PARTS);
      break;
    case TMP_TABLE_INNODB:
    default:
      *max_key_length =
          std::min(Cache_temp_engine_properties::HEAP_MAX_KEY_LENGTH,
                   Cache_temp_engine_properties::INNODB_MAX_KEY_LENGTH);
      *max_key_part_length =
          std::min(Cache_temp_engine_properties::HEAP_MAX_KEY_PART_LENGTH,
                   Cache_temp_engine_properties::INNODB_MAX_KEY_PART_LENGTH);
      *max_key_parts =
          std::min(Cache_temp_engine_properties::HEAP_MAX_KEY_PARTS,
                   Cache_temp_engine_properties::INNODB_MAX_KEY_PARTS);
      break;
  }
}

/**
  Create a temporary name for one field if the field_name is empty.

  @param thd          Thread handle
  @param field_index  Index of this field in table->field
*/

static const char *create_tmp_table_field_tmp_name(THD *thd, int field_index) {
  char buf[64];
  snprintf(buf, 64, "tmp_field_%d", field_index);
  return thd->mem_strdup(buf);
}

/**
  Helper function for create_tmp_table().

  Insert a field at the head of the hidden field area.

  @param table            Temporary table
  @param default_field    Default value array pointer
  @param from_field       Original field array pointer
  @param blob_field       Array pointer to record fields index of blob type
  @param field            The registed hidden field
 */

static void register_hidden_field(TABLE *table, Field **default_field,
                                  Field **from_field, uint *blob_field,
                                  Field *field) {
  uint i;
  Field **tmp_field = table->field;

  /* Increase all of registed fields index */
  for (i = 0; i < table->s->fields; i++) tmp_field[i]->field_index++;

  // Increase the field_index of visible blob field
  for (i = 0; i < table->s->blob_fields; i++) blob_field[i]++;
  // Insert field
  table->field[-1] = field;
  default_field[-1] = NULL;
  from_field[-1] = NULL;
  field->table = field->orig_table = table;
  field->field_index = 0;
}

/**
  Helper function which evaluates correct TABLE_SHARE::real_row_type
  for the temporary table.
*/
static void set_real_row_type(TABLE *table) {
  HA_CREATE_INFO create_info;
  create_info.row_type = table->s->row_type;
  create_info.options |=
      HA_LEX_CREATE_TMP_TABLE | HA_LEX_CREATE_INTERNAL_TMP_TABLE;
  create_info.table_options = table->s->db_create_options;
  table->s->real_row_type = table->file->get_real_row_type(&create_info);
}

bool Func_ptr::set_contains_alias_of_expr(const SELECT_LEX *select) {
  // We cast 'const' away, but the walker will not modify '*select'.
  uchar *walk_arg =
      const_cast<uchar *>(reinterpret_cast<const uchar *>(select));
  return m_contains_alias_of_expr =
             m_func->walk(&Item::contains_alias_of_expr,
                          Item::enum_walk(Item::WALK_PREFIX |
                                          // ref to alias might be in a subquery
                                          Item::WALK_SUBQUERY),
                          walk_arg);
}

/**
  Moves to the end of the 'copy_func' array the elements which contain a
  reference to an alias of an expression of the SELECT list of 'select'.
  @param[in,out]  copy_func  array to sort
  @param          select     query block to search in.
*/
void sort_copy_func(const SELECT_LEX *select, Func_ptr_array *copy_func) {
  /*
    In the select->all_fields list, there are hidden elements first, then
    non-hidden. Non-hidden are those of the SELECT list. Hidden ones are:
    (a) those of GROUP BY, HAVING, ORDER BY
    (b) those which have been extracted from higher-level elements (of the
    SELECT, GROUP BY, etc) by split_sum_func() (when aggregates are
    involved).
    Note that the clauses in (a) are allowed to reference a non-hidden
    expression through an alias (e.g. "SELECT a+2 AS x GROUP BY x+3"),

    Let's go through the process of writing to the tmp table
    (e.g. end_write(), end_write_group()). We also include here the
    "pseudo-tmp table" embedded into REF_ITEM_SLICE3, used by
    end_send_group().
    (1) we switch to the REF_SLICE used to read from that tmp table
    (2.1) we (copy_fields() part 1) copy some columns from the
    output of the previous step of execution (e.g. the join's output) to the
    tmp table
    (2.2) (specifically for REF_SLICE_TMP3 in end_send_group()) we
    (copy_fields() part 2) evaluate some expressions from the same previous
    step of execution, with Item_copy::copy(). The mechanism of Item_copy is:
    * copy() evaluates the expression and caches its value in memory
    * val_*() returns the cached value;
    so Item_copy::copy() for "a+2" evaluates "a+2" (using the join's value
    of "a") and caches the value; then Item_copy::copy() for "x+3" evaluates
    "x", through Item_ref (because of the alias), that Item_ref points to
    the Item_copy for "a+2" (does not point to the "a+2" Item_func_plus
    expression, as we advanced the REF_SLICE to TMP3); copy() on
    "x+3" thus evaluates the Item_copy for "a+2" which returns the cached value.
    This way, if "a+2" were rather some non-deterministic expression
    (e.g. rand()), the logic above does only one evaluation of rand(), which is
    correct (the two objects "x" and "a+2" in 'fields' thus have equal
    values).
    For this to work, the Item_copy for "x" must be copy()d after that
    of "a+2", so it can use the value cached for "a+2". setup_copy_fields()
    ensures this by putting Item_copy-s of hidden elements last.
    (3) We are now done with copy_fields(). Next is copy_funcs(). It
    is meant to evaluate expressions and store their values into the tmp table.
    [ note that we could replace Item_copy in (2) with a real one-row tmp
    table; then end_send_group() could just use copy_funcs() instead of
    Item_copy: copy_funcs() would store into the tmp table's column which
    would thus be the storage for the cached value ].
    Because we advanced the REF_SLICE, when copy_funcs() evaluates an
    expression which uses Item_ref, that Item_ref may point to a column of
    the tmp table. It is thus important that this column has been filled
    already. So the order of evaluation of expressions by copy_funcs() must
    respect "dependencies".
    It is correct to evaluate elements of (b) first, as they are inner to
    others. But it is incorrect to evaluate elements of (a) first if they
    refer to non-hidden elements through aliases.
    So, we sort elements below, putting to the end the ones which use
    aliases. We use a stable sort, to not disturb any dependence already
    reflected in the order.

    A simpler and more robust solution would be to break the design that
    hidden elements are always first in SELECT_LEX::all_fields: references
    using aliases (in GROUP BY, HAVING, ORDER BY) would be added to
    all_fields last (after the SELECT list); an inner element (split by
    split_sum_func) would be added right before its containing element. That
    would reflect dependencies naturally. But it is hard to implement, as
    some code relies on the fact that non-hidden elements are last, and
    other code relies on the fact that SELECT::fields is just a part of
    SELECT::all_fields (i.e. they share 'next' pointers, in the
    implementation).

    You may wonder why setup_copy_fields() can solve the dependency problem
    by putting all hidden elements last, while for the copy_func array we
    have a (more complex) sort. It's because setup_copy_fields() is for
    end_send_group() which handles only queries with GROUP BY without ORDER
    BY, window functions or DISTINCT. So the hidden elements produced by
    split_sum_func are only group aggregates (not anything from WFs), which
    setup_copy_fields() ignores: these aggregates are thus not cached
    (neither in Item_copy, nor in a further tmp table's row as there's no tmp
    table); so any parent item which references them,
    if evaluated, will reach to the aggregate, not to any cache
    materializing the aggregate, so will get an up-to-date value.
    Whereas with window functions, it's possible to have a hidden element be an
    aggregate (produced by split_sum_func) _and_ be materialized (into a
    further tmp table), so we cannot ignore such Item anymore: we have to
    leave it at the beginning of the copy_func array. Except if it contains
    an alias to an expression of the SELECT list: in that case, the sorting
    will move it to the end, but will also move the aliased expression, and
    their relative order will remain unchanged thanks to stable sort, so
    their evaluation will be in the right order.

    So we walk each item to copy, and record if it uses an alias. If we
    found such items, we sort.
  */
  bool need_sort = false;
  for (uint i = 0; i < copy_func->size(); i++)
    need_sort |= copy_func->at(i).set_contains_alias_of_expr(select);
  if (need_sort)
    std::stable_sort(copy_func->begin(), copy_func->end(),
                     [](const Func_ptr &lhs, const Func_ptr &rhs) {
                       return !lhs.contains_alias_of_expr() &&
                              rhs.contains_alias_of_expr();
                     });
}

/**
  Helper function for create_tmp_table_* family for setting tmp table fields
  to their place in record buffer

  @param field      field to set
  @param pos        field's position in table's record buffer
  @param null_flags beginning of table's null bits buffer
  @param null_count  field's null bit in null bits buffer
*/

inline void relocate_field(Field *field, uchar *pos, uchar *null_flags,
                           uint *null_count) {
  if (!(field->flags & NOT_NULL_FLAG)) {
    field->move_field(pos, null_flags + *null_count / 8,
                      (uint8)1 << (*null_count & 7));
    (*null_count)++;
  } else
    field->move_field(pos, (uchar *)0, 0);
  if (field->type() == MYSQL_TYPE_BIT) {
    /* We have to reserve place for extra bits among null bits */
    ((Field_bit *)field)
        ->set_bit_ptr(null_flags + *null_count / 8, *null_count & 7);
    (*null_count) += (field->field_length & 7);
  }
  field->reset();
}

  /**
    Create a temp table according to a field list.

    Given field pointers are changed to point at tmp_table for
    send_result_set_metadata. The table object is self contained: it's
    allocated in its own memory root, as well as Field objects
    created for table columns. Those Field objects are common to TABLE and
    TABLE_SHARE.
    This function will replace Item_sum items in 'fields' list with
    corresponding Item_field items, pointing at the fields in the
    temporary table, unless this was prohibited by true
    value of argument save_sum_fields. The Item_field objects
    are created in THD memory root.

    @param thd                  thread handle
    @param param                a description used as input to create the table
    @param fields               list of items that will be used to define
                                column types of the table (also see NOTES)
    @param group                Group key to use for temporary table, NULL if
    none
    @param distinct             should table rows be distinct
    @param save_sum_fields      see NOTES
    @param select_options
    @param rows_limit
    @param table_alias          possible name of the temporary table that can
                                be used for name resolving; can be "".

    @remark mysql_create_view() checks that views have less than
            MAX_FIELDS columns. This prevents any MyISAM temp table
            made when materializing the view from hitting the 64k
            MyISAM header size limit.

    @remark We may actually end up with a table without any columns at all.
            See comment below: We don't have to store this.
  */

#define STRING_TOTAL_LENGTH_TO_PACK_ROWS 128
#define AVG_STRING_LENGTH_TO_PACK_ROWS 64
#define RATIO_TO_PACK_ROWS 2
#define MIN_STRING_LENGTH_TO_PACK_ROWS 10

TABLE *create_tmp_table(THD *thd, Temp_table_param *param, List<Item> &fields,
                        ORDER *group, bool distinct, bool save_sum_fields,
                        ulonglong select_options, ha_rows rows_limit,
                        const char *table_alias) {
  MEM_ROOT *mem_root_save, own_root;
  TABLE *table;
  TABLE_SHARE *share;
  uint i, field_count, null_count, null_pack_length;
  uint copy_func_count = param->func_count;
  uint hidden_null_count, hidden_null_pack_length;
  long hidden_field_count;
  uint blob_count, group_null_items, string_count;
  uint fieldnr = 0;
  ulong reclength, string_total_length, distinct_key_length = 0;
  /**
    When true, enforces unique constraint (by adding a hidden hash_field and
    creating a key over this field) when:
    (1) unique key is too long or
    (2) number of key parts in distinct key is too big.
  */
  bool using_unique_constraint = false;
  bool use_packed_rows = false;
  const bool not_all_columns = !(select_options & TMP_TABLE_ALL_COLUMNS);
  uchar *pos, *group_buff, *bitmaps;
  uchar *null_flags;
  Field **reg_field, **from_field, **default_field;
  uint *blob_field;
  Copy_field *copy = 0;
  KEY *keyinfo;
  KEY_PART_INFO *key_part_info;
  MI_COLUMNDEF *recinfo;
  /*
    total_uneven_bit_length is uneven bit length for visible fields
    hidden_uneven_bit_length is uneven bit length for hidden fields
  */
  uint total_uneven_bit_length = 0, hidden_uneven_bit_length = 0;
  bool force_copy_fields = param->force_copy_fields;

  uint max_key_length, max_key_part_length, max_key_parts;
  /* Treat sum functions as normal ones when loose index scan is used. */
  save_sum_fields |= param->precomputed_group_by;
  DBUG_ENTER("create_tmp_table");
  DBUG_PRINT("enter",
             ("distinct: %d  save_sum_fields: %d  rows_limit: %lu  group: %d",
              (int)distinct, (int)save_sum_fields, (ulong)rows_limit,
              static_cast<bool>(group)));
  if (group) {
    if (!param->quick_group)
      group = 0;  // Can't use group key
    else
      for (ORDER *tmp = group; tmp; tmp = tmp->next) {
        /*
          marker == MARKER_BIT means two things:
          - store NULLs in the key, and
          - convert BIT fields to 64-bit long, needed because MEMORY tables
            can't index BIT fields.
        */
        (*tmp->item)->marker = Item::MARKER_BIT;
        const uint char_len = (*tmp->item)->max_length /
                              (*tmp->item)->collation.collation->mbmaxlen;
        if (char_len > CONVERT_IF_BIGGER_TO_BLOB)
          using_unique_constraint = true;
      }
    if (group) {
      if (param->group_length >= MAX_BLOB_WIDTH) using_unique_constraint = true;
      distinct = 0;  // Can't use distinct
    }
  }

  field_count = param->field_count + param->func_count + param->sum_func_count;
  hidden_field_count = param->hidden_field_count;

  /*
    When loose index scan is employed as access method, it already
    computes all groups and the result of all aggregate functions. We
    make space for the items of the aggregate function in the list of
    functions Temp_table_param::items_to_copy, so that the values of
    these items are stored in the temporary table.
  */
  if (param->precomputed_group_by) copy_func_count += param->sum_func_count;

  init_sql_alloc(key_memory_TABLE, &own_root, TABLE_ALLOC_BLOCK_SIZE, 0);

  void *rawmem = alloc_root(&own_root, sizeof(Func_ptr_array));
  if (!rawmem) DBUG_RETURN(NULL); /* purecov: inspected */
  Func_ptr_array *copy_func = new (rawmem) Func_ptr_array(&own_root);
  copy_func->reserve(copy_func_count);

  if (!multi_alloc_root(
          &own_root, &table, sizeof(*table), &share, sizeof(*share), &reg_field,
          sizeof(Field *) * (field_count + 2), &default_field,
          sizeof(Field *) * (field_count + 1), &blob_field,
          sizeof(uint) * (field_count + 2), &from_field,
          sizeof(Field *) * (field_count + 1), &param->keyinfo,
          sizeof(*param->keyinfo), &key_part_info,
          sizeof(*key_part_info) * (param->group_parts + 1),
          &param->start_recinfo,
          sizeof(*param->recinfo) * (field_count * 2 + 4), &group_buff,
          (group && !using_unique_constraint ? param->group_length : 0),
          &bitmaps, bitmap_buffer_size(field_count + 1) * 3, NullS)) {
    DBUG_RETURN(NULL); /* purecov: inspected */
  }
  /* Copy_field belongs to Temp_table_param, allocate it in THD mem_root */
  if (!(param->copy_field = copy =
            new (thd->mem_root) Copy_field[field_count])) {
    free_root(&own_root, MYF(0)); /* purecov: inspected */
    DBUG_RETURN(NULL);            /* purecov: inspected */
  }
  param->items_to_copy = copy_func;
  /* make table according to fields */

  new (table) TABLE;
  memset(reg_field, 0, sizeof(Field *) * (field_count + 2));
  memset(default_field, 0, sizeof(Field *) * (field_count + 1));
  memset(from_field, 0, sizeof(Field *) * (field_count + 1));

  // Leave the first place to be prepared for hash_field
  reg_field++;
  default_field++;
  from_field++;
  table->init_tmp_table(thd, share, &own_root, param->table_charset,
                        table_alias, reg_field, blob_field, false);
  /*
    We will use TABLE_SHARE's MEM_ROOT for all allocations, so TABLE's
    MEM_ROOT remains uninitialized.
    TABLE_SHARE's MEM_ROOT is a copy of own_root, upon error free_tmp_table()
    will free it.
  */
  mem_root_save = thd->mem_root;
  thd->mem_root = &share->mem_root;
  copy_func->set_mem_root(&share->mem_root);

  if (param->schema_table) share->db = INFORMATION_SCHEMA_NAME;

  /* Calculate which type of fields we will store in the temporary table */

  reclength = string_total_length = 0;
  blob_count = string_count = null_count = hidden_null_count =
      group_null_items = 0;
  param->using_outer_summary_function = 0;

  List_iterator_fast<Item> li(fields);
  Item *item;
  Field **tmp_from_field = from_field;
  while ((item = li++)) {
    Field *new_field = NULL;
    Item::Type type = item->type();
    const bool is_sum_func =
        type == Item::SUM_FUNC_ITEM && !item->m_is_window_function;

    if (type == Item::COPY_STR_ITEM) {
      item = ((Item_copy *)item)->get_item();
      type = item->type();
    }
    if (not_all_columns) {
      if (item->has_aggregation() && type != Item::SUM_FUNC_ITEM) {
        if (item->used_tables() & OUTER_REF_TABLE_BIT)
          item->update_used_tables();
        if (type == Item::SUBSELECT_ITEM ||
            (item->used_tables() & ~OUTER_REF_TABLE_BIT)) {
          /*
            Mark that we have ignored an item that refers to a summary
            function. We need to know this if someone is going to use
            DISTINCT on the result.
          */
          param->using_outer_summary_function = 1;
          goto update_hidden;
        }
      }
      if (item->m_is_window_function) {
        if (!param->m_window) {
          /*
            A pre-windowing table; no point in storing WF.
            Or a window's frame buffer:
            - the window's WFs cannot be calculated yet
            - same for later windows' WFs
            - previous windows' WFs are already replaced with Item_field (so
            don't come here).
          */
          goto update_hidden;
        }
        if (param->m_window != down_cast<Item_sum *>(item)->window()) {
          // A later window's WF: no point in storing it in this table.
          goto update_hidden;
        }
      } else if (item->has_wf()) {
        /*
          A non-WF expression containing a WF conservatively requires all
          windows to have been processed, and is not stored in any of
          windowing tables. Note that if the tmp table belongs to an even
          later step - materialization of a query's result - then
          not_all_columns==false and we store the expression.
        */
        goto update_hidden;
      }
      if (item->const_item() && (int)hidden_field_count <= 0)
        continue;  // We don't have to store this
    }

    if (is_sum_func && !group && !save_sum_fields) { /* Can't calc group yet */
      Item_sum *sum_item = down_cast<Item_sum *>(item);
      uint arg_count = sum_item->get_arg_count();
      for (i = 0; i < arg_count; i++) {
        Item *arg = sum_item->get_arg(i);
        if (is_sum_func && !arg->const_item()) {
          new_field =
              create_tmp_field(thd, table, arg, arg->type(), copy_func,
                               tmp_from_field, &default_field[fieldnr],
                               group != 0, not_all_columns, distinct, false);
          if (!new_field) goto err;  // Should be OOM
          tmp_from_field++;
          reclength += new_field->pack_length();
          if (new_field->flags & BLOB_FLAG) {
            *blob_field++ = fieldnr;
            blob_count++;
          }
          if (new_field->type() == MYSQL_TYPE_BIT)
            total_uneven_bit_length += new_field->field_length & 7;
          *(reg_field++) = new_field;
          if (new_field->real_type() == MYSQL_TYPE_STRING ||
              new_field->real_type() == MYSQL_TYPE_VARCHAR) {
            string_count++;
            string_total_length += new_field->pack_length();
          }
          thd->mem_root = mem_root_save;

          arg = sum_item->set_arg(i, thd, new Item_field(new_field));
          thd->mem_root = &share->mem_root;
          if (!(new_field->flags & NOT_NULL_FLAG)) {
            null_count++;
            /*
              new_field->maybe_null() is still false, it will be
              changed below. But we have to setup Item_field correctly
            */
            arg->maybe_null = 1;
          }
          new_field->field_index = fieldnr++;
          /* InnoDB temp table doesn't allow field with empty_name */
          if (!new_field->field_name)
            new_field->field_name =
                create_tmp_table_field_tmp_name(thd, new_field->field_index);
        }
      }
    } else {
      /*
        Parameters of create_tmp_field():

        (1) is a bit tricky:
        We need to set it to 0 in union, to get fill_record() to modify the
        temporary table.
        We need to set it to 1 on multi-table-update and in select to
        write rows to the temporary table.
        We here distinguish between UNION and multi-table-updates by the fact
        that in the later case group is set to the row pointer.
        (2) If item->marker == MARKER_BIT then we force create_tmp_field
        to create a 64-bit longs for BIT fields because HEAP
        tables can't index BIT fields directly. We do the same
        for distinct, as we want the distinct index to be
        usable in this case too.
        (3) This is the OUT table of windowing, there is a frame buffer, and
        the item is an expression which can store its value in a result_field
        (e.g. it is Item_func). In that case we pass copy_result_field=true.
      */
      new_field =
          (param->schema_table)
              ? create_tmp_field_for_schema(item, table)
              : create_tmp_field(
                    thd, table, item, type, copy_func, tmp_from_field,
                    &default_field[fieldnr],
                    group != 0,  // (1)
                    !force_copy_fields && (not_all_columns || group != 0),
                    item->marker == Item::MARKER_BIT ||
                        param->bit_fields_as_long,  //(2)
                    force_copy_fields,
                    (param->m_window &&  // (3)
                     param->m_window->frame_buffer_param() &&
                     item->is_result_field()));

      if (!new_field) {
        DBUG_ASSERT(thd->is_fatal_error);
        goto err;  // Got OOM
      }
      /*
        Some group aggregate function use result_field to maintain their
        current value (e.g. Item_avg_field stores both count and sum there).
        But only for the group-by table. So do not set result_field if this is
        a tmp table for UNION or derived table materialization.
      */
      if (not_all_columns && type == Item::SUM_FUNC_ITEM)
        ((Item_sum *)item)->result_field = new_field;
      tmp_from_field++;
      reclength += new_field->pack_length();
      if (!(new_field->flags & NOT_NULL_FLAG)) null_count++;
      if (new_field->type() == MYSQL_TYPE_BIT)
        total_uneven_bit_length += new_field->field_length & 7;
      if (new_field->flags & BLOB_FLAG) {
        *blob_field++ = fieldnr;
        blob_count++;
      }

      if (new_field->real_type() == MYSQL_TYPE_STRING ||
          new_field->real_type() == MYSQL_TYPE_VARCHAR) {
        string_count++;
        string_total_length += new_field->pack_length();
      }
      // In order to reduce footprint ask SE to pack variable-length fields.
      if (new_field->type() == MYSQL_TYPE_VAR_STRING ||
          new_field->type() == MYSQL_TYPE_VARCHAR)
        table->s->db_create_options |= HA_OPTION_PACK_RECORD;

      if (item->marker == Item::MARKER_BIT && item->maybe_null) {
        group_null_items++;
        new_field->flags |= GROUP_FLAG;
      }
      new_field->field_index = fieldnr++;
      *(reg_field++) = new_field;
      /* InnoDB temp table doesn't allow field with empty_name */
      if (!new_field->field_name)
        new_field->field_name =
            create_tmp_table_field_tmp_name(thd, new_field->field_index);
    }

  update_hidden:
    /*
      Calculate length of distinct key. The goal is to decide what to use -
      key or unique constraint. As blobs force unique constraint on their
      own due to their length, they aren't taken into account.
    */
    if (distinct && !using_unique_constraint && hidden_field_count <= 0 &&
        new_field) {
      if (new_field->flags & BLOB_FLAG)
        using_unique_constraint = true;
      else
        distinct_key_length += new_field->pack_length();
    }

    if (!--hidden_field_count) {
      /*
        This was the last hidden field; Remember how many hidden fields could
        have null
      */
      hidden_null_count = null_count;
      /*
        We need to update hidden_field_count as we may have stored group
        functions with constant arguments
      */
      param->hidden_field_count = fieldnr;
      null_count = 0;
      /*
        On last hidden field we store uneven bit length in
        hidden_uneven_bit_length and proceed calculation of
        uneven bits for visible fields into
        total_uneven_bit_length variable.
      */
      hidden_uneven_bit_length = total_uneven_bit_length;
      total_uneven_bit_length = 0;
    }
  }  // end of while ((item=li++)).

  DBUG_ASSERT(fieldnr == (uint)(reg_field - table->field));
  DBUG_ASSERT(field_count >= (uint)(reg_field - table->field));
  field_count = fieldnr;

  *reg_field = 0;
  *blob_field = 0;  // End marker
  share->fields = field_count;
  share->blob_fields = blob_count;

  /*
    Different temp table engine supports different max_key_length
    and max_key_part_lengthi. If HEAP engine is selected, it can be
    possible to convert into on-disk engine later. We must choose
    the minimal of max_key_length and max_key_part_length between
    HEAP engine and possible on-disk engine to verify whether unique
    constraint is needed so that the convertion goes well.
   */
  get_max_key_and_part_length(&max_key_length, &max_key_part_length,
                              &max_key_parts);

  if (group && (param->group_parts > max_key_parts ||
                param->group_length > max_key_length))
    using_unique_constraint = true;
  keyinfo = param->keyinfo;
  keyinfo->table = table;
  keyinfo->is_visible = true;

  if (group) {
    DBUG_PRINT("info", ("Creating group key in temporary table"));
    table->group = group; /* Table is grouped by key */
    param->group_buff = group_buff;
    share->keys = 1;
    // Let each group expression know the column which materializes its value
    for (ORDER *cur_group = group; cur_group; cur_group = cur_group->next) {
      Field *field = (*cur_group->item)->get_tmp_table_field();
      DBUG_ASSERT(field->table == table);
      cur_group->field_in_tmp_table = field;
    }
    // Use key definition created below only if the key isn't too long.
    // Otherwise a dedicated key over a hash value will be created and this
    // definition will be used by server to calc hash.
    if (!using_unique_constraint) {
      table->key_info = share->key_info = keyinfo;
      keyinfo->key_part = key_part_info;
      keyinfo->flags = HA_NOSAME;
      keyinfo->usable_key_parts = keyinfo->user_defined_key_parts =
          param->group_parts;
      keyinfo->actual_key_parts = keyinfo->user_defined_key_parts;
      keyinfo->rec_per_key = 0;
      // keyinfo->algorithm is set later, when storage engine is known
      keyinfo->set_rec_per_key_array(NULL, NULL);
      keyinfo->set_in_memory_estimate(IN_MEMORY_ESTIMATE_UNKNOWN);
      keyinfo->name = (char *)"<group_key>";
      ORDER *cur_group = group;
      for (; cur_group; cur_group = cur_group->next, key_part_info++) {
        Field *field = cur_group->field_in_tmp_table;
        key_part_info->init_from_field(field);

        /* In GROUP BY 'a' and 'a ' are equal for VARCHAR fields */
        key_part_info->key_part_flag |= HA_END_SPACE_ARE_EQUAL;

        if (key_part_info->store_length > max_key_part_length) {
          using_unique_constraint = true;
          break;
        }
      }
      keyinfo->actual_flags = keyinfo->flags;
    }
  }

  if (distinct && field_count != param->hidden_field_count) {
    /*
      Create an unique key or an unique constraint over all columns
      that should be in the result.  In the temporary table, there are
      'param->hidden_field_count' extra columns, whose null bits are stored
      in the first 'hidden_null_pack_length' bytes of the row.
    */
    DBUG_PRINT("info", ("hidden_field_count: %d", param->hidden_field_count));
    share->keys = 1;
    table->is_distinct = true;
    if (!using_unique_constraint) {
      Field **reg_field;
      keyinfo->user_defined_key_parts = field_count - param->hidden_field_count;
      keyinfo->actual_key_parts = keyinfo->user_defined_key_parts;
      if (!(key_part_info = new (&share->mem_root)
                KEY_PART_INFO[keyinfo->user_defined_key_parts]))
        goto err;
      table->key_info = share->key_info = keyinfo;
      keyinfo->key_part = key_part_info;
      keyinfo->actual_flags = keyinfo->flags = HA_NOSAME | HA_NULL_ARE_EQUAL;
      // TODO rename to <distinct_key>
      keyinfo->name = (char *)"<auto_key>";
      // keyinfo->algorithm is set later, when storage engine is known
      keyinfo->set_rec_per_key_array(NULL, NULL);
      keyinfo->set_in_memory_estimate(IN_MEMORY_ESTIMATE_UNKNOWN);
      /* Create a distinct key over the columns we are going to return */
      for (i = param->hidden_field_count, reg_field = table->field + i;
           i < field_count; i++, reg_field++, key_part_info++) {
        key_part_info->init_from_field(*reg_field);
        if (key_part_info->store_length > max_key_part_length) {
          using_unique_constraint = true;
          break;
        }
      }
    }
  }

  /*
    To enforce unique constraint we need to add a field to hold key's hash
    A1) already detected unique constraint
    A2) distinct key is too long
    A3) number of keyparts in distinct key is too big
  */
  if (using_unique_constraint ||               // 1
      distinct_key_length > max_key_length ||  // 2
      (distinct &&                             // 3
       (fieldnr - param->hidden_field_count) > max_key_parts)) {
    using_unique_constraint = true;
  }

  if (setup_tmp_table_handler(table, select_options, false,
                              param->schema_table))
    goto err; /* purecov: inspected */

  if (table->s->keys == 1 && table->key_info)
    table->key_info->algorithm = table->file->get_default_index_algorithm();

  if (using_unique_constraint) {
    Field_longlong *field = new (&share->mem_root)
        Field_longlong(sizeof(ulonglong), false, "<hash_field>", true);
    if (!field) {
      /* purecov: begin inspected */
      DBUG_ASSERT(thd->is_fatal_error);
      goto err;  // Got OOM
      /* purecov: end */
    }

    // Mark hash_field as NOT NULL
    field->flags &= NOT_NULL_FLAG;
    // Register hash_field as a hidden field.
    register_hidden_field(table, default_field, from_field, share->blob_field,
                          field);
    // Repoint arrays
    table->field--;
    default_field--;
    from_field--;
    reclength += field->pack_length();
    field_count = ++fieldnr;
    param->hidden_field_count++;
    share->fields = field_count;
    share->field--;
    table->hash_field = field;
  }

  table->hidden_field_count = param->hidden_field_count;

  if (!using_unique_constraint)
    reclength += group_null_items;  // null flag is stored separately

  if (blob_count == 0) {
    /* We need to ensure that first byte is not 0 for the delete link */
    if (param->hidden_field_count)
      hidden_null_count++;
    else
      null_count++;
  }
  hidden_null_pack_length =
      (hidden_null_count + 7 + hidden_uneven_bit_length) / 8;
  null_pack_length = (hidden_null_pack_length +
                      (null_count + total_uneven_bit_length + 7) / 8);
  reclength += null_pack_length;
  if (!reclength) reclength = 1;  // Dummy select
  /* Use packed rows if there is blobs or a lot of space to gain */
  if (blob_count ||
      (string_total_length >= STRING_TOTAL_LENGTH_TO_PACK_ROWS &&
       (reclength / string_total_length <= RATIO_TO_PACK_ROWS ||
        string_total_length / string_count >= AVG_STRING_LENGTH_TO_PACK_ROWS)))
    use_packed_rows = true;

  if (!use_packed_rows) share->db_create_options &= ~HA_OPTION_PACK_RECORD;

  share->reclength = reclength;
  share->null_bytes = null_pack_length;
  share->null_fields = null_count + hidden_null_count;

  if (alloc_record_buffers(table)) goto err;
  param->func_count = copy_func->size();
  DBUG_ASSERT(param->func_count <= copy_func_count);  // Used <= allocated
  sort_copy_func(thd->lex->current_select(), copy_func);
  setup_tmp_table_column_bitmaps(table, bitmaps);

  recinfo = param->start_recinfo;
  null_flags = table->record[0];
  pos = table->record[0] + null_pack_length;
  if (null_pack_length) {
    memset(recinfo, 0, sizeof(*recinfo));
    recinfo->type = FIELD_NORMAL;
    recinfo->length = null_pack_length;
    recinfo++;
  }
  null_count = (blob_count == 0) ? 1 : 0;
  hidden_field_count = param->hidden_field_count;
  DBUG_ASSERT((uint)hidden_field_count <= field_count);
  for (i = 0, reg_field = table->field; i < field_count;
       i++, reg_field++, recinfo++) {
    Field *field = *reg_field;
    uint length;
    memset(recinfo, 0, sizeof(*recinfo));

    if (!(field->flags & NOT_NULL_FLAG)) {
      if (field->flags & GROUP_FLAG && !using_unique_constraint) {
        /*
          We have to reserve one byte here for NULL bits,
          as this is updated by 'end_update()'
        */
        *pos++ = 0;  // Null is stored here
        recinfo->length = 1;
        recinfo->type = FIELD_NORMAL;
        recinfo++;
        memset(recinfo, 0, sizeof(*recinfo));
      } else {
        recinfo->null_bit = (uint8)1 << (null_count & 7);
        recinfo->null_pos = null_count / 8;
      }
    }
    relocate_field(field, pos, null_flags, &null_count);

    /*
      Test if there is a default field value. The test for ->ptr is to skip
      'offset' fields generated by initalize_tables
    */
    if (default_field[i] && default_field[i]->ptr) {
      /*
         default_field[i] is set only in the cases  when 'field' can
         inherit the default value that is defined for the field referred
         by the Item_field object from which 'field' has been created.
      */
      Field *orig_field = default_field[i];
      /*
        Get the value from default_values. Note that orig_field->ptr might not
        point into record[0] if previous step is REF_SLICE_TMP3 and we are
        creating a tmp table to materialize the query's result.
      */
      my_ptrdiff_t diff = orig_field->table->default_values_offset();
      Field *f_in_record0 = orig_field->table->field[orig_field->field_index];
      f_in_record0->move_field_offset(diff);  // Points now at default_values
      if (f_in_record0->is_real_null())
        field->set_null();
      else {
        field->set_notnull();
        memcpy(field->ptr, f_in_record0->ptr, field->pack_length());
      }
      f_in_record0->move_field_offset(-diff);  // Back to record[0]
    }

    if (from_field[i]) { /* Not a table Item */
      if (param->m_window && param->m_window->frame_buffer_param() &&
          field->flags & FIELD_IS_MARKED) {
        Temp_table_param *window_fb = param->m_window->frame_buffer_param();
        // Grep for FIELD_IS_MARKED in this file.
        field->flags ^= FIELD_IS_MARKED;
        window_fb->copy_field_end->set(from_field[i], field, save_sum_fields);
        window_fb->copy_field_end++;
      } else {
        copy->set(field, from_field[i], save_sum_fields);
        copy++;
      }
    }
    length = field->pack_length();
    pos += length;

    /* Make entry for create table */
    recinfo->length = length;
    if (field->flags & BLOB_FLAG)
      recinfo->type = (int)FIELD_BLOB;
    else if (use_packed_rows && field->real_type() == MYSQL_TYPE_STRING &&
             length >= MIN_STRING_LENGTH_TO_PACK_ROWS)
      recinfo->type = FIELD_SKIP_ENDSPACE;
    else if (use_packed_rows && field->real_type() == MYSQL_TYPE_VARCHAR &&
             length >= MIN_STRING_LENGTH_TO_PACK_ROWS)
      recinfo->type = FIELD_VARCHAR;
    else
      recinfo->type = FIELD_NORMAL;
    if (!--hidden_field_count)
      null_count = (null_count + 7) & ~7;  // move to next byte

    // fix table name in field entry
    field->table_name = &table->alias;
  }

  param->copy_field_end = copy;
  param->recinfo = recinfo;
  store_record(table, s->default_values);  // Make empty default record

  /*
    Push the LIMIT clause to the temporary table creation, so that we
    materialize only up to 'rows_limit' records instead of all result records.
  */
  set_if_smaller(share->max_rows, rows_limit);
  param->end_write_records = rows_limit;

  if (group && !using_unique_constraint) {
    ORDER *cur_group = group;
    key_part_info = keyinfo->key_part;
    if (param->can_use_pk_for_unique) share->primary_key = 0;
    keyinfo->key_length = 0;  // Will compute the sum of the parts below.
    /*
      Here, we have to make the group fields point to the right record
      position.
    */
    for (; cur_group; cur_group = cur_group->next, key_part_info++) {
      Field *field = cur_group->field_in_tmp_table;
      bool maybe_null = (*cur_group->item)->maybe_null;
      key_part_info->init_from_field(key_part_info->field);
      keyinfo->key_length += key_part_info->store_length;

      cur_group->buff = (char *)group_buff;
      cur_group->field_in_tmp_table =
          field->new_key_field(thd->mem_root, table, group_buff + maybe_null);

      if (!cur_group->field_in_tmp_table) goto err; /* purecov: inspected */

      if (maybe_null) {
        /*
          To be able to group on NULL, we reserved place in group_buff
          for the NULL flag just before the column. (see above).
          The field data is after this flag.
          The NULL flag is updated in 'end_update()' and 'end_write()'
        */
        keyinfo->flags |= HA_NULL_ARE_EQUAL;  // def. that NULL == NULL
        cur_group->buff++;                    // Pointer to field data
        group_buff++;                         // Skipp null flag
      }
      group_buff += cur_group->field_in_tmp_table->pack_length();
    }
  }

  if (distinct && field_count != param->hidden_field_count &&
      !using_unique_constraint) {
    null_pack_length -= hidden_null_pack_length;
    key_part_info = keyinfo->key_part;
    if (param->can_use_pk_for_unique) share->primary_key = 0;
    keyinfo->key_length = 0;  // Will compute the sum of the parts below.
    /*
      Here, we have to make the key fields point to the right record
      position.
    */
    for (i = param->hidden_field_count, reg_field = table->field + i;
         i < field_count; i++, reg_field++, key_part_info++) {
      key_part_info->init_from_field(*reg_field);
      keyinfo->key_length += key_part_info->store_length;
    }
  }

  // Create a key over hash_field to enforce unique constraint
  if (using_unique_constraint) {
    KEY *hash_key;
    KEY_PART_INFO *hash_kpi;

    if (!multi_alloc_root(&share->mem_root, &hash_key, sizeof(*hash_key),
                          &hash_kpi, sizeof(*hash_kpi),  // Only one key part
                          NullS))
      goto err;
    table->key_info = share->key_info = hash_key;
    hash_key->table = table;
    hash_key->key_part = hash_kpi;
    hash_key->actual_flags = hash_key->flags = HA_NULL_ARE_EQUAL;
    hash_key->actual_key_parts = hash_key->usable_key_parts = 1;
    hash_key->user_defined_key_parts = 1;
    hash_key->set_rec_per_key_array(NULL, NULL);
    hash_key->algorithm = table->file->get_default_index_algorithm();
    hash_key->set_in_memory_estimate(IN_MEMORY_ESTIMATE_UNKNOWN);
    if (distinct)
      hash_key->name = (char *)"<hash_distinct_key>";
    else
      hash_key->name = (char *)"<hash_group_key>";
    hash_kpi->init_from_field(table->hash_field);
    hash_key->key_length = hash_kpi->store_length;
    param->keyinfo = hash_key;
  }

  if (thd->is_fatal_error)  // If end of memory
    goto err;               /* purecov: inspected */

  set_real_row_type(table);

  if (!param->skip_create_table) {
    if (instantiate_tmp_table(thd, table, param->keyinfo, param->start_recinfo,
                              &param->recinfo, select_options,
                              thd->variables.big_tables))
      goto err;
  }

  thd->mem_root = mem_root_save;

  DEBUG_SYNC(thd, "tmp_table_created");

  DBUG_RETURN(table);

err:
  thd->mem_root = mem_root_save;
  free_tmp_table(thd, table); /* purecov: inspected */
  DBUG_RETURN(NULL);          /* purecov: inspected */
}

/*
  Create a temporary table to weed out duplicate rowid combinations

  SYNOPSIS

    create_duplicate_weedout_tmp_table()
      thd                    Thread handle
      uniq_tuple_length_arg  Length of the table's column
      sjtbl                  Update sjtbl->[start_]recinfo values which
                             will be needed if we'll need to convert the
                             created temptable from HEAP to MyISAM/Maria.

  DESCRIPTION
    Create a temporary table to weed out duplicate rowid combinations. The
    table has a single column that is a concatenation of all rowids in the
    combination.

    Depending on the needed length, there are two cases:

    1. When the length of the column < max_key_length:

      CREATE TABLE tmp (col VARBINARY(n) NOT NULL, UNIQUE KEY(col));

    2. Otherwise (not a valid SQL syntax but internally supported):

      CREATE TABLE tmp (col VARBINARY NOT NULL, UNIQUE CONSTRAINT(col));

    The code in this function was produced by extraction of relevant parts
    from create_tmp_table().

  RETURN
    created table
    NULL on error
*/

TABLE *create_duplicate_weedout_tmp_table(THD *thd, uint uniq_tuple_length_arg,
                                          SJ_TMP_TABLE *sjtbl) {
  MEM_ROOT *mem_root_save, own_root;
  TABLE *table;
  TABLE_SHARE *share;
  Field **reg_field;
  KEY_PART_INFO *key_part_info;
  KEY *keyinfo;
  uchar *group_buff;
  uchar *bitmaps;
  uint *blob_field;
  MI_COLUMNDEF *recinfo, *start_recinfo;
  bool using_unique_constraint = false;
  Field *field, *key_field;
  uint null_pack_length;
  uchar *null_flags;
  uchar *pos;
  uint i;

  DBUG_ENTER("create_duplicate_weedout_tmp_table");
  DBUG_ASSERT(!sjtbl->is_confluent);

  DBUG_EXECUTE_IF("create_duplicate_weedout_tmp_table_error", {
    my_error(ER_UNKNOWN_ERROR, MYF(0));
    DBUG_RETURN(nullptr);
  });

  /* STEP 1: Figure if we'll be using a key or blob+constraint */
  if (uniq_tuple_length_arg > CONVERT_IF_BIGGER_TO_BLOB)
    using_unique_constraint = true;

  /* STEP 2: Allocate memory for temptable description */
  init_sql_alloc(key_memory_TABLE, &own_root, TABLE_ALLOC_BLOCK_SIZE, 0);
  if (!multi_alloc_root(
          &own_root, &table, sizeof(*table), &share, sizeof(*share), &reg_field,
          sizeof(Field *) * (1 + 2), &blob_field, sizeof(uint) * 3, &keyinfo,
          sizeof(*keyinfo), &key_part_info, sizeof(*key_part_info) * 2,
          &start_recinfo, sizeof(*recinfo) * (1 * 2 + 2), &group_buff,
          (!using_unique_constraint ? uniq_tuple_length_arg : 0), &bitmaps,
          bitmap_buffer_size(1) * 3, NullS)) {
    DBUG_RETURN(NULL);
  }

  /* STEP 3: Create TABLE description */
  new (table) TABLE;
  memset(reg_field, 0, sizeof(Field *) * 3);
  table->init_tmp_table(thd, share, &own_root, NULL, "weedout-tmp", reg_field,
                        blob_field, false);

  mem_root_save = thd->mem_root;
  thd->mem_root = &share->mem_root;

  uint reclength = 0;
  uint null_count = 0;

  /* Create the field */
  if (using_unique_constraint) {
    Field_longlong *field = new (&share->mem_root)
        Field_longlong(sizeof(ulonglong), false, "<hash_field>", true);
    if (!field) {
      DBUG_ASSERT(thd->is_fatal_error);
      goto err;  // Got OOM
    }
    // Mark hash_field as NOT NULL
    field->flags = NOT_NULL_FLAG;
    *(reg_field++) = sjtbl->hash_field = field;
    table->hash_field = field;
    field->table = field->orig_table = table;
    share->fields++;
    field->field_index = 0;
    reclength = field->pack_length();
    table->hidden_field_count++;
  }
  {
    /*
      For the sake of uniformity, always use Field_varstring (altough we could
      use Field_string for shorter keys)
    */
    field = new (*THR_MALLOC) Field_varstring(uniq_tuple_length_arg, false,
                                              "rowids", share, &my_charset_bin);
    if (!field) DBUG_RETURN(0);
    field->table = table;
    field->auto_flags = Field::NONE;
    field->flags = (NOT_NULL_FLAG | BINARY_FLAG | NO_DEFAULT_VALUE_FLAG);
    field->reset_fields();
    field->init(table);
    field->orig_table = NULL;
    *(reg_field++) = field;
    *blob_field = 0;
    *reg_field = 0;

    field->field_index = share->fields;
    share->fields++;
    share->blob_fields = 0;
    reclength += field->pack_length();
    null_count++;
  }

  /* See also create_tmp_table() */
  if (setup_tmp_table_handler(table, 0LL, using_unique_constraint, false))
    goto err;

  null_pack_length = 1;
  reclength += null_pack_length;

  share->reclength = reclength;
  share->null_bytes = null_pack_length;
  share->null_fields = null_count;

  if (alloc_record_buffers(table)) goto err;
  setup_tmp_table_column_bitmaps(table, bitmaps);

  recinfo = start_recinfo;
  null_flags = table->record[0];

  pos = table->record[0] + null_pack_length;
  if (null_pack_length) {
    memset(recinfo, 0, sizeof(*recinfo));
    recinfo->type = FIELD_NORMAL;
    recinfo->length = null_pack_length;
    recinfo++;
  }
  null_count = 1;
  for (i = 0, reg_field = table->field; i < share->fields;
       i++, reg_field++, recinfo++) {
    Field *field = *reg_field;
    uint length;
    /* Table description for the concatenated rowid column */
    memset(recinfo, 0, sizeof(*recinfo));

    relocate_field(field, pos, null_flags, &null_count);
    length = field->pack_length();
    pos += length;

    /*
      Don't care about packing the VARCHAR since it's only a
      concatenation of rowids. @see create_tmp_table() for how
      packed VARCHARs can be achieved
    */
    recinfo->length = length;
    recinfo->type = FIELD_NORMAL;

    // fix table name in field entry
    field->table_name = &table->alias;
  }

  // Create a key over param->hash_field to enforce unique constraint
  if (using_unique_constraint) {
    KEY *hash_key = keyinfo;
    KEY_PART_INFO *hash_kpi = key_part_info;

    share->keys = 1;
    table->key_info = share->key_info = hash_key;
    hash_key->table = table;
    hash_key->key_part = hash_kpi;
    hash_key->actual_flags = hash_key->flags = HA_NULL_ARE_EQUAL;
    hash_kpi->init_from_field(sjtbl->hash_field);
    hash_key->key_length = hash_kpi->store_length;
  } else {
    DBUG_PRINT("info", ("Creating group key in temporary table"));
    share->keys = 1;
    table->key_info = table->s->key_info = keyinfo;
    keyinfo->key_part = key_part_info;
    keyinfo->actual_flags = keyinfo->flags = HA_NOSAME;
    keyinfo->key_length = 0;
    {
      key_part_info->init_from_field(field);
      key_part_info->bin_cmp = true;

      key_field = field->new_key_field(thd->mem_root, table, group_buff);
      if (!key_field) goto err;
      key_part_info->key_part_flag |= HA_END_SPACE_ARE_EQUAL;  // todo need
                                                               // this?
      keyinfo->key_length += key_part_info->length;
    }
  }
  {
    table->key_info->user_defined_key_parts = 1;
    table->key_info->usable_key_parts = 1;
    table->key_info->actual_key_parts = table->key_info->user_defined_key_parts;
    table->key_info->set_rec_per_key_array(NULL, NULL);
    table->key_info->algorithm = table->file->get_default_index_algorithm();
    table->key_info->set_in_memory_estimate(IN_MEMORY_ESTIMATE_UNKNOWN);
    table->key_info->name = (char *)"weedout_key";
  }

  if (thd->is_fatal_error)  // If end of memory
    goto err;

  set_real_row_type(table);

  if (instantiate_tmp_table(thd, table, table->key_info, start_recinfo,
                            &recinfo, 0, 0))
    goto err;

  sjtbl->start_recinfo = start_recinfo;
  sjtbl->recinfo = recinfo;

  thd->mem_root = mem_root_save;
  DBUG_RETURN(table);

err:
  thd->mem_root = mem_root_save;
  table->file->ha_index_or_rnd_end();
  free_tmp_table(thd, table); /* purecov: inspected */
  DBUG_RETURN(NULL);          /* purecov: inspected */
}

/****************************************************************************/

/**
  Create an, optionally reduced, TABLE object with properly set up Field list
  from a list of field definitions.

  @details
  When is_virtual arg is true:
    The created table doesn't have a table handler associated with
    it, has no keys, no group/distinct, no copy_funcs array.
    The sole purpose of this TABLE object is to use the power of Field
    class to read/write data to/from table->record[0]. Then one can store
    the record in any container (RB tree, hash, etc).
    The table is created in THD mem_root, so are the table's fields.
    Consequently, if you don't BLOB fields, you don't need to free it.
  When is_virtual is false:
    This function creates a normal tmp table out of fields' definitions,
    rather than from lst of items. This is the main difference with
    create_tmp_table. Also the table created here doesn't do grouping,
    doesn't have indexes and copy_funcs/fields. The purpose is to be able to
    create result table for table functions out of fields' definitions
    without need in intermediate list of items.

  @param thd         connection handle
  @param field_list  list of column definitions
  @param is_virtual  if true, then it's effectively only a record buffer
                       with wrapper, used e.g to store vars in SP
                     if false, then a normal table, which can hold
                       records, is created
  @param select_options options for non-virtual tmp table
  @param alias       table's alias

  @return
    0 if out of memory, TABLE object in case of success
*/

TABLE *create_tmp_table_from_fields(THD *thd, List<Create_field> &field_list,
                                    bool is_virtual, ulonglong select_options,
                                    const char *alias) {
  uint field_count = field_list.elements;
  uint blob_count = 0;
  Field **reg_field;
  Create_field *cdef; /* column definition */
  uint record_length = 0;
  uint null_count = 0;   /* number of columns which may be null */
  uint null_pack_length; /* NULL representation array length */
  uint *blob_field;
  uchar *bitmaps;
  TABLE *table;
  TABLE_SHARE *share;
  MEM_ROOT own_root, *m_root;
  /*
    total_uneven_bit_length is uneven bit length for BIT fields
  */
  uint total_uneven_bit_length = 0;

  if (!is_virtual) {
    init_sql_alloc(key_memory_TABLE, &own_root, TABLE_ALLOC_BLOCK_SIZE, 0);
    m_root = &own_root;
  } else
    m_root = thd->mem_root;

  if (!multi_alloc_root(m_root, &table, sizeof(*table), &share, sizeof(*share),
                        &reg_field, (field_count + 1) * sizeof(Field *),
                        &blob_field, (field_count + 1) * sizeof(uint), &bitmaps,
                        bitmap_buffer_size(field_count) * 3, NullS))
    return 0;

  new (table) TABLE;
  new (share) TABLE_SHARE;
  table->init_tmp_table(thd, share, m_root, NULL, alias, reg_field, blob_field,
                        is_virtual);

  /* Create all fields and calculate the total length of record */
  List_iterator_fast<Create_field> it(field_list);
  uint idx = 0;
  while ((cdef = it++)) {
    *reg_field = make_field(
        share, 0, cdef->length, (uchar *)(cdef->maybe_null ? "" : 0),
        cdef->maybe_null ? 1 : 0, cdef->sql_type, cdef->charset,
        cdef->geom_type, cdef->auto_flags, cdef->interval, cdef->field_name,
        cdef->maybe_null, cdef->is_zerofill, cdef->is_unsigned, cdef->decimals,
        cdef->treat_bit_as_char, cdef->pack_length_override, cdef->m_srid);
    if (!*reg_field) goto error;
    (*reg_field)->init(table);
    record_length += (*reg_field)->pack_length();
    if (!((*reg_field)->flags & NOT_NULL_FLAG)) null_count++;
    (*reg_field)->field_index = idx++;
    if ((*reg_field)->type() == MYSQL_TYPE_BIT)
      total_uneven_bit_length += (*reg_field)->field_length & 7;

    if ((*reg_field)->flags & BLOB_FLAG)
      share->blob_field[blob_count++] = (uint)(reg_field - table->field);

    reg_field++;
  }
  *reg_field = NULL;                 /* mark the end of the list */
  share->blob_field[blob_count] = 0; /* mark the end of the list */
  share->blob_fields = blob_count;

  null_pack_length = (null_count + total_uneven_bit_length + 7) / 8;
  share->reclength = record_length + null_pack_length;
  share->null_bytes = null_pack_length;
  share->null_fields = null_count;
  share->fields = field_count;

  if (is_virtual) {
    /*
      When the table is virtual, updates won't be done on the table and
      default values won't be stored. Thus no need to allocate buffers for
      that.
    */
    share->rec_buff_length = ALIGN_SIZE(share->reclength + 1);
    table->record[0] = (uchar *)thd->alloc(share->rec_buff_length);
    if (!table->record[0]) goto error;
    if (null_pack_length) {
      table->null_flags = table->record[0];
      memset(table->record[0], 255, null_pack_length);  // Set null fields
    }
  } else if (alloc_record_buffers(table))
    goto error;

  setup_tmp_table_column_bitmaps(table, bitmaps);

  {
    /* Set up field pointers */
    uchar *null_flags = table->record[0];
    uchar *pos = null_flags + share->null_bytes;
    uint null_count = 0;

    for (reg_field = table->field; *reg_field; ++reg_field) {
      Field *field = *reg_field;
      relocate_field(field, pos, null_flags, &null_count);
      pos += field->pack_length();
    }
  }

  if (is_virtual) return table;

  store_record(table, s->default_values);  // Make empty default record

  if (setup_tmp_table_handler(table, select_options, false, false)) goto error;

  return table;
error:
  for (reg_field = table->field; *reg_field; ++reg_field) destroy(*reg_field);
  return 0;
}

/**
  Helper function to create_tmp_table_* family for setting up table's SE

  @param table            table to allocate SE for
  @param select_options   current select's options
  @param force_disk_table true <=> Use MyISAM or InnoDB
  @param schema_table     whether the table is a schema table

  @returns
    false on success
    true  otherwise
*/

static bool setup_tmp_table_handler(TABLE *table, ulonglong select_options,
                                    bool force_disk_table, bool schema_table) {
  THD *thd = table->in_use;
  TABLE_SHARE *share = table->s;
  if (select_options & TMP_TABLE_FORCE_MYISAM)
    share->db_plugin = ha_lock_engine(0, myisam_hton);
  else if (share->blob_fields ||          // 1
           (thd->variables.big_tables &&  // 2
            !(select_options & SELECT_SMALL_RESULT)) ||
           force_disk_table ||  // 3
           opt_initialize)      // 4
  {
    /*
      1: MEMORY and TempTable do not support BLOBs
      2: User said the result would be big, so may not fit in memory
      3: Caller needs SE to be disk-based (@see create_tmp_table())
      4: During bootstrap, the heap engine is not available, so we force using
      InnoDB. This is especially hit when creating a I_S system view
      definition with a UNION in it.

      Except for special conditions, tmp table engine will be chosen by user.
    */
    switch (internal_tmp_disk_storage_engine) {
      case TMP_TABLE_MYISAM:
        share->db_plugin = ha_lock_engine(0, myisam_hton);
        break;
      case TMP_TABLE_INNODB:
        share->db_plugin = ha_lock_engine(0, innodb_hton);
        break;
      default:
        DBUG_ASSERT(0); /* purecov: deadcode */
        share->db_plugin = ha_lock_engine(0, innodb_hton);
    }
  } else {
    share->db_plugin = nullptr;
    switch ((enum_internal_tmp_mem_storage_engine)
                thd->variables.internal_tmp_mem_storage_engine) {
      case TMP_TABLE_TEMPTABLE:
        if (!schema_table) {
          share->db_plugin = ha_lock_engine(0, temptable_hton);
          break;
        }
        /* For information_schema tables we use the Heap engine because we do
        not allow user-created TempTable tables and even though
        information_schema tables are not user-created, an ingenious user may
        execute: CREATE TABLE myowntemptabletable LIKE information_schema.some;
      */
        /* Fall-through. */
      case TMP_TABLE_MEMORY:
        share->db_plugin = ha_lock_engine(0, heap_hton);
        break;
    }
    DBUG_ASSERT(share->db_plugin != nullptr);
  }

  if (!(table->file =
            get_new_handler(share, false, &share->mem_root, share->db_type())))
    return true;
  // Update the handler with information about the table object
  table->file->change_table_ptr(table, share);
  if (table->file->set_ha_share_ref(&share->ha_share)) {
    destroy(table->file);
    return true;
  }

  // Initialize cost model for this table
  table->init_cost_model(thd->cost_model());

  return false;
}

/**
  Helper function for create_tmp_table_* family for allocating record buffers

  @note Caller must initialize TABLE_SHARE::reclength and
  TABLE_SHARE::null_bytes before calling this function.

  @param table  table to allocate record buffers for

  @returns
    false  on success
    true   otherwise
*/

static bool alloc_record_buffers(TABLE *table) {
  TABLE_SHARE *share = table->s;
  THD *thd = table->in_use;
  uint alloc_length = ALIGN_SIZE(share->reclength + MI_UNIQUE_HASH_LENGTH + 1);
  share->rec_buff_length = alloc_length;
  /*
    Note that code in open_table_from_share() relies on the fact that
    for optimizer-created temporary tables TABLE_SHARE::default_values
    is allocated in a single chuck with TABLE::record[0] for the first
    TABLE instance.
  */
  if (!(table->record[0] = (uchar *)alloc_root(
            &share->mem_root, (alloc_length * 3 + share->null_bytes))))
    return true;
  table->record[1] = table->record[0] + alloc_length;
  share->default_values = table->record[1] + alloc_length;
  table->null_flags_saved = share->default_values + alloc_length;
  if (share->null_bytes) {
    table->null_flags = table->record[0];
    memset(table->record[0], 255, share->null_bytes);  // Set null fields
  }

  if (thd->variables.tmp_table_size == ~(ulonglong)0)  // No limit
    share->max_rows = ~(ha_rows)0;
  else
    share->max_rows = (ha_rows)(((share->db_type() == heap_hton)
                                     ? min(thd->variables.tmp_table_size,
                                           thd->variables.max_heap_table_size)
                                     : thd->variables.tmp_table_size) /
                                share->reclength);
  set_if_bigger(share->max_rows, 1);  // For dummy start options

  return false;
}

bool open_tmp_table(TABLE *table) {
  DBUG_ASSERT(table->s->ref_count == 1 ||          // not shared, or:
              table->s->db_type() == heap_hton ||  // using right engines
              table->s->db_type() == temptable_hton ||
              table->s->db_type() == innodb_hton);

  int error;
  if ((error = table->file->ha_open(table, table->s->table_name.str, O_RDWR,
                                    HA_OPEN_TMP_TABLE | HA_OPEN_INTERNAL_TABLE,
                                    nullptr))) {
    table->file->print_error(error, MYF(0)); /* purecov: inspected */
    table->db_stat = 0;
    return (1);
  }
  (void)table->file->extra(HA_EXTRA_QUICK); /* Faster */

  table->set_created();
  table->s->tmp_handler_count++;
  return false;
}

/*
  Create MyISAM temporary table

  SYNOPSIS
    create_myisam_tmp_table()
      table           Table object that descrimes the table to be created
      keyinfo         Description of the index (there is always one index)
      start_recinfo   MyISAM's column descriptions
      recinfo INOUT   End of MyISAM's column descriptions
      options         Option bits

  DESCRIPTION
    Create a MyISAM temporary table according to passed description. The is
    assumed to have one unique index or constraint.

    The passed array or MI_COLUMNDEF structures must have this form:

      1. 1-byte column (afaiu for 'deleted' flag) (note maybe not 1-byte
         when there are many nullable columns)
      2. Table columns
      3. One free MI_COLUMNDEF element (*recinfo points here)

    This function may use the free element to create hash column for unique
    constraint.

   RETURN
     false - OK
     true  - Error
*/

static bool create_myisam_tmp_table(TABLE *table, KEY *keyinfo,
                                    MI_COLUMNDEF *start_recinfo,
                                    MI_COLUMNDEF **recinfo, ulonglong options,
                                    bool big_tables) {
  int error;
  MI_KEYDEF keydef;
  MI_UNIQUEDEF uniquedef;
  TABLE_SHARE *share = table->s;
  DBUG_ENTER("create_myisam_tmp_table");

  if (share->keys) {  // Get keys for ni_create
    if (share->keys > 1) {
      DBUG_ASSERT(0);  // This code can't handle more than 1 key
      share->keys = 1;
    }
    HA_KEYSEG *seg = (HA_KEYSEG *)alloc_root(
        &table->s->mem_root, sizeof(*seg) * keyinfo->user_defined_key_parts);
    if (!seg) goto err;

    memset(seg, 0, sizeof(*seg) * keyinfo->user_defined_key_parts);

    /* Create an unique key */
    memset(&keydef, 0, sizeof(keydef));
    keydef.flag = static_cast<uint16>(keyinfo->flags);
    keydef.keysegs = keyinfo->user_defined_key_parts;
    keydef.seg = seg;

    for (uint i = 0; i < keyinfo->user_defined_key_parts; i++, seg++) {
      Field *field = keyinfo->key_part[i].field;
      seg->flag = 0;
      seg->language = field->charset()->number;
      seg->length = keyinfo->key_part[i].length;
      seg->start = keyinfo->key_part[i].offset;
      if (field->flags & BLOB_FLAG) {
        seg->type = ((keyinfo->key_part[i].bin_cmp) ? HA_KEYTYPE_VARBINARY2
                                                    : HA_KEYTYPE_VARTEXT2);
        seg->bit_start =
            (uint8)(field->pack_length() - portable_sizeof_char_ptr);
        seg->flag = HA_BLOB_PART;
        seg->length = 0;  // Whole blob in unique constraint
      } else {
        seg->type = keyinfo->key_part[i].type;
        /* Tell handler if it can do suffic space compression */
        if (field->real_type() == MYSQL_TYPE_STRING &&
            keyinfo->key_part[i].length > 4)
          seg->flag |= HA_SPACE_PACK;
      }
      if (!(field->flags & NOT_NULL_FLAG)) {
        seg->null_bit = field->null_bit;
        seg->null_pos = field->null_offset();
      }
    }
  }
  MI_CREATE_INFO create_info;
  memset(&create_info, 0, sizeof(create_info));

  if (big_tables && !(options & SELECT_SMALL_RESULT))
    create_info.data_file_length = ~(ulonglong)0;

  if ((error = mi_create(share->table_name.str, share->keys, &keydef,
                         (uint)(*recinfo - start_recinfo), start_recinfo, 0,
                         &uniquedef, &create_info,
                         HA_CREATE_TMP_TABLE | HA_CREATE_INTERNAL_TABLE |
                             ((share->db_create_options & HA_OPTION_PACK_RECORD)
                                  ? HA_PACK_RECORD
                                  : 0)))) {
    table->file->print_error(error, MYF(0)); /* purecov: inspected */
    table->db_stat = 0;
    goto err;
  }
  table->in_use->inc_status_created_tmp_disk_tables();
  DBUG_RETURN(0);
err:
  DBUG_RETURN(1);
}

/**
  Try to create an in-memory temporary table and if not enough space, then
  try to create an on-disk one.

  Create a temporary table according to passed description.

  The passed array or MI_COLUMNDEF structures must have this form:

    1. 1-byte column (afaiu for 'deleted' flag) (note maybe not 1-byte
       when there are many nullable columns)
    2. Table columns
    3. One free MI_COLUMNDEF element (*recinfo points here)

  This function may use the free element to create hash column for unique
  constraint.

  @param[in,out] table Table object that describes the table to be created

  @retval false OK
  @retval true Error
*/
static bool create_tmp_table_with_fallback(TABLE *table) {
  TABLE_SHARE *share = table->s;

  DBUG_ENTER("create_tmp_table_with_fallback");

  HA_CREATE_INFO create_info;

  create_info.db_type = table->s->db_type();
  create_info.row_type = table->s->row_type;
  create_info.options |=
      HA_LEX_CREATE_TMP_TABLE | HA_LEX_CREATE_INTERNAL_TMP_TABLE;

  /*
    INNODB's fixed length column size is restricted to 1024. Exceeding this can
    result in incorrect behavior.
  */
  if (table->s->db_type() == innodb_hton) {
    for (Field **field = table->field; *field; ++field) {
      if ((*field)->type() == MYSQL_TYPE_STRING &&
          (*field)->key_length() > 1024) {
        my_error(ER_TOO_LONG_KEY, MYF(0), 1024);
        DBUG_RETURN(true);
      }
    }
  }

  int error =
      table->file->create(share->table_name.str, table, &create_info, nullptr);
  if (error == HA_ERR_RECORD_FILE_FULL &&
      table->s->db_type() == temptable_hton) {
    auto &disk_hton = internal_tmp_disk_storage_engine == TMP_TABLE_INNODB
                          ? innodb_hton
                          : myisam_hton;
    table->file =
        get_new_handler(table->s, false, &table->s->mem_root, disk_hton);
    error = table->file->create(share->table_name.str, table, &create_info,
                                nullptr);
  }

  if (error) {
    table->file->print_error(error, MYF(0)); /* purecov: inspected */
    table->db_stat = 0;
    DBUG_RETURN(true);
  } else {
    if (table->s->db_type() != temptable_hton) {
      table->in_use->inc_status_created_tmp_disk_tables();
    }
    DBUG_RETURN(false);
  }
}

static void trace_tmp_table(Opt_trace_context *trace, const TABLE *table) {
  TABLE_SHARE *s = table->s;
  Opt_trace_object trace_tmp(trace, "tmp_table_info");
  if (strlen(table->alias) != 0)
    trace_tmp.add_utf8_table(table->pos_in_table_list);
  else
    trace_tmp.add_alnum("table", "intermediate_tmp_table");
  QEP_TAB *tab = table->reginfo.qep_tab;
  if (tab) trace_tmp.add("in_plan_at_position", tab->idx());
  trace_tmp.add("columns", s->fields)
      .add("row_length", s->reclength)
      .add("key_length", table->key_info ? table->key_info->key_length : 0)
      .add("unique_constraint", table->hash_field ? true : false)
      .add("makes_grouped_rows", table->group != nullptr)
      .add("cannot_insert_duplicates", table->is_distinct);

  if (s->db_type() == myisam_hton) {
    trace_tmp.add_alnum("location", "disk (MyISAM)");
    if (s->db_create_options & HA_OPTION_PACK_RECORD)
      trace_tmp.add_alnum("record_format", "packed");
    else
      trace_tmp.add_alnum("record_format", "fixed");
  } else if (s->db_type() == innodb_hton) {
    trace_tmp.add_alnum("location", "disk (InnoDB)");
    if (s->db_create_options & HA_OPTION_PACK_RECORD)
      trace_tmp.add_alnum("record_format", "packed");
    else
      trace_tmp.add_alnum("record_format", "fixed");
  } else if (table->s->db_type() == temptable_hton) {
    trace_tmp.add_alnum("location", "TempTable");
  } else {
    DBUG_ASSERT(s->db_type() == heap_hton);
    trace_tmp.add_alnum("location", "memory (heap)")
        .add("row_limit_estimate", s->max_rows);
  }
}

/**
  @brief
  Instantiates temporary table

  @param  thd             Thread handler
  @param  table           Table object that describes the table to be
                          instantiated
  @param  keyinfo         Description of the index (there is always one index)
  @param  start_recinfo   Column descriptions
  @param[in,out]  recinfo End of column descriptions
  @param  options         Option bits
  @param  big_tables

  @details
    Creates tmp table and opens it.

  @return
     false - OK
     true  - Error
*/

bool instantiate_tmp_table(THD *thd, TABLE *table, KEY *keyinfo,
                           MI_COLUMNDEF *start_recinfo, MI_COLUMNDEF **recinfo,
                           ulonglong options, bool big_tables) {
  TABLE_SHARE *const share = table->s;
#ifndef DBUG_OFF
  for (uint i = 0; i < share->fields; i++)
    DBUG_ASSERT(table->field[i]->gcol_info == NULL &&
                table->field[i]->stored_in_db);
#endif
  thd->inc_status_created_tmp_tables();

  if (share->db_type() == temptable_hton) {
    if (create_tmp_table_with_fallback(table)) return true;
  } else if (share->db_type() == innodb_hton) {
    if (create_tmp_table_with_fallback(table)) return true;
    // Make empty record so random data is not written to disk
    empty_record(table);
  } else if (share->db_type() == myisam_hton) {
    DBUG_ASSERT(start_recinfo && recinfo);
    if (create_myisam_tmp_table(table, keyinfo, start_recinfo, recinfo, options,
                                big_tables))
      return true;
    // Make empty record so random data is not written to disk
    empty_record(table);
  }

  // If a heap table, it's created by open_tmp_table().
  if (open_tmp_table(table)) {
    /*
      Delete table immediately if we fail to open it, so
      TABLE::is_created() also implies that table is open.
    */
    table->file->ha_delete_table(share->table_name.str,
                                 nullptr); /* purecov: inspected */
    return true;
  }

  if (share->first_unused_tmp_key < share->keys) {
    /*
      Some other clone of this materialized temporary table has defined
      "possible" keys; as we are here creating the table in the engine, we must
      decide here what to do with them: drop them now, or make them "existing"
      now. As the other clone assumes they will be available if the Optimizer
      chooses them, we make them existing.
    */
    share->find_first_unused_tmp_key(Key_map(share->keys));
  }

  Opt_trace_context *const trace = &thd->opt_trace;
  if (unlikely(trace->is_started())) {
    Opt_trace_object wrapper(trace);
    Opt_trace_object convert(trace, "creating_tmp_table");
    trace_tmp_table(trace, table);
  }
  return false;
}

/**
  Free TABLE object and release associated resources for
  internal temporary table.
*/
void free_tmp_table(THD *thd, TABLE *entry) {
  const char *save_proc_info;
  DBUG_ENTER("free_tmp_table");
  DBUG_PRINT("enter", ("table: %s", entry->alias));

  save_proc_info = thd->proc_info;
  THD_STAGE_INFO(thd, stage_removing_tmp_table);

  filesort_free_buffers(entry, true);

  DBUG_ASSERT(entry->s->tmp_handler_count <= entry->s->ref_count);

  if (entry->is_created()) {
    DBUG_ASSERT(entry->s->tmp_handler_count >= 1);
    // Table is marked as created only if was successfully opened.
    if (--entry->s->tmp_handler_count)
      entry->file->ha_close();
    else  // no more open 'handler' objects
      entry->file->ha_drop_table(entry->s->table_name.str);
    entry->set_deleted();
  }

  destroy(entry->file);
  entry->file = NULL;

  /* free blobs */
  for (Field **ptr = entry->field; *ptr; ptr++) (*ptr)->mem_free();
  free_io_cache(entry);

  DBUG_ASSERT(entry->mem_root.allocated_size() == 0);

  DBUG_ASSERT(entry->s->ref_count >= 1);
  if (--entry->s->ref_count == 0)  // no more TABLE objects
  {
    plugin_unlock(0, entry->s->db_plugin);
    /*
      In create_tmp_table(), the share's memroot is allocated inside own_root
      and is then made a copy of own_root, so it is inside its memory blocks,
      so as soon as we free a memory block the memroot becomes unreadable.
      So we need a copy to free it.
    */
    MEM_ROOT own_root = std::move(entry->s->mem_root);
    free_root(&own_root, MYF(0));
  }

  thd_proc_info(thd, save_proc_info);

  DBUG_VOID_RETURN;
}

/**
  If a MEMORY table gets full, create a disk-based table and copy all rows
  to this.

  @param thd             THD reference
  @param wtable          Table reference being written to
  @param start_recinfo   Engine's column descriptions
  @param [in,out] recinfo End of engine's column descriptions
  @param error           Reason why inserting into MEMORY table failed.
  @param ignore_last_dup If true, ignore duplicate key error for last
                         inserted key (see detailed description below).
  @param [out] is_duplicate if non-NULL and ignore_last_dup is true,
                         return true if last key was a duplicate,
                         and false otherwise.

  @details
    Function can be called with any error code, but only HA_ERR_RECORD_FILE_FULL
    will be handled, all other errors cause a fatal error to be thrown.
    The function creates a disk-based temporary table, copies all records
    from the MEMORY table into this new table, deletes the old table and
    switches to use the new table within the table handle.
    The function uses table->record[1] as a temporary buffer while copying.

    The function assumes that table->record[0] contains the row that caused
    the error when inserting into the MEMORY table (the "last row").
    After all existing rows have been copied to the new table, the last row
    is attempted to be inserted as well. If ignore_last_dup is true,
    this row can be a duplicate of an existing row without throwing an error.
    If is_duplicate is non-NULL, an indication of whether the last row was
    a duplicate is returned.

  @note that any index/scan access initialized on the MEMORY 'wtable' is not
  replicated to the on-disk table - it's the caller's responsibility.
  However, access initialized on other TABLEs, is replicated.

  If 'wtable' has other TABLE clones (example: a multi-referenced or a
  recursive CTE), we convert all clones; if an error happens during conversion
  of clone B after successfully converting clone A, clone A and B will exit
  from the function with a TABLE_SHARE corresponding to the pre-conversion
  table ("old" TABLE_SHARE). So A will be inconsistent (for example
  s->db_type() will say "MEMORY" while A->file will be a disk-based engine).
  However, as all callers bail out, it is reasonable to think that they won't
  be using the TABLE_SHARE except in free_tmp_table(); and free_tmp_table()
  only uses properties of TABLE_SHARE which are common to the old and new
  object (reference counts, MEM_ROOT), so that should work.
  Solutions to fix this cleanly:
  - allocate new TABLE_SHARE on heap instead of on stack, to be able to
  exit with two TABLE_SHAREs (drawback: more heap memory consumption, and need
  to verify all exit paths are safe),
  - close all TABLEs if error (but then callers and cleanup code may be
  surprised to find already-closed tables so they would need fixing).
  To lower the risk of error between A and B: we expect most errors will
  happen when copying rows (e.g. read or write errors); so we convert 'wtable'
  (which does the row copying) first; if it fails, the A-B situation is
  avoided and we can properly exit with the old TABLE_SHARE.

  @returns true if error.
*/

bool create_ondisk_from_heap(THD *thd, TABLE *wtable,
                             MI_COLUMNDEF *start_recinfo,
                             MI_COLUMNDEF **recinfo, int error,
                             bool ignore_last_dup, bool *is_duplicate) {
  int write_err = 0;
#ifndef DBUG_OFF
  const uint initial_handler_count = wtable->s->tmp_handler_count;
  bool rows_on_disk = false;
#endif
  bool table_on_disk = false;
  DBUG_ENTER("create_ondisk_from_heap");

  if ((wtable->s->db_type() != heap_hton) ||
      (error != HA_ERR_RECORD_FILE_FULL)) {
    /*
      We don't want this error to be converted to a warning, e.g. in case of
      INSERT IGNORE ... SELECT.
    */
    wtable->file->print_error(error, MYF(ME_FATALERROR));
    DBUG_RETURN(1);
  }

  const char *save_proc_info = thd->proc_info;
  THD_STAGE_INFO(thd, stage_converting_heap_to_ondisk);

  TABLE_SHARE *const old_share = wtable->s;
  const plugin_ref old_plugin = old_share->db_plugin;
  TABLE_SHARE share = std::move(*old_share);
  DBUG_ASSERT(share.ha_share == nullptr);

  switch (internal_tmp_disk_storage_engine) {
    case TMP_TABLE_MYISAM:
      if (!start_recinfo) {
        my_error(ER_NOT_SUPPORTED_YET, MYF(0), "MyISAM for table functions");
        DBUG_RETURN(true);
      }
      share.db_plugin = ha_lock_engine(thd, myisam_hton);
      break;
    case TMP_TABLE_INNODB:
      share.db_plugin = ha_lock_engine(thd, innodb_hton);
      break;
    default:
      DBUG_ASSERT(0); /* purecov: deadcode */
      share.db_plugin = ha_lock_engine(thd, innodb_hton);
  }

  TABLE_LIST *const wtable_list = wtable->pos_in_table_list;
  Derived_refs_iterator ref_it(wtable_list);

  if (wtable_list) {
    Common_table_expr *cte = wtable_list->common_table_expr();
    if (cte) {
      int i = 0, found = -1;
      TABLE *t;
      while ((t = ref_it.get_next())) {
        if (t == wtable) {
          found = i;
          break;
        }
        ++i;
      }
      DBUG_ASSERT(found >= 0);
      if (found > 0)
        // 'wtable' is at position 'found', move it to 0 to convert it first
        std::swap(cte->tmp_tables[0], cte->tmp_tables[found]);
      ref_it.rewind();
    }
  }

  TABLE new_table, *table = nullptr;

  while (true) {
    if (wtable_list)  // Possibly there are clones
    {
      table = ref_it.get_next();
      if (table == nullptr) break;
    } else  // No clones
    {
      if (table == wtable)  // Already processed
        break;
      table = wtable;
    }

    table->mem_root.Clear();

    // Set up a partial copy of the table.
    new_table.record[0] = table->record[0];
    new_table.record[1] = table->record[1];
    new_table.field = table->field;
    new_table.key_info = table->key_info;
    new_table.in_use = table->in_use;
    new_table.db_stat = table->db_stat;
    new_table.key_info = table->key_info;
    new_table.hash_field = table->hash_field;
    new_table.group = table->group;
    new_table.is_distinct = table->is_distinct;
    new_table.alias = table->alias;
    new_table.pos_in_table_list = table->pos_in_table_list;
    new_table.reginfo = table->reginfo;
    new_table.read_set = table->read_set;
    new_table.write_set = table->write_set;

    new_table.s = &share;  // New table points to new share

    if (!(new_table.file = get_new_handler(
              &share, false, &new_table.s->mem_root, new_table.s->db_type())))
      goto err_after_proc_info; /* purecov: inspected */
    if (new_table.file->set_ha_share_ref(&share.ha_share))
      goto err_after_alloc; /* purecov: inspected */

    /* Fix row type which might have changed with SE change. */
    set_real_row_type(&new_table);

    if (!table_on_disk) {
      if (share.db_type() == myisam_hton) {
        if (create_myisam_tmp_table(&new_table, share.key_info, start_recinfo,
                                    recinfo,
                                    thd->lex->select_lex->active_options(),
                                    thd->variables.big_tables))
          goto err_after_alloc; /* purecov: inspected */
      } else if (share.db_type() == innodb_hton) {
        if (create_tmp_table_with_fallback(&new_table))
          goto err_after_alloc; /* purecov: inspected */
      }
      table_on_disk = true;
    }

    bool rec_ref_w_open_cursor = false, psi_batch_started = false;

    if (table->is_created()) {
      // Close it, drop it, and open a new one in the disk-based engine.

      if (open_tmp_table(&new_table))
        goto err_after_create; /* purecov: inspected */

      if (table->file->indexes_are_disabled())
        new_table.file->ha_disable_indexes(HA_KEY_SWITCH_ALL);

      if (table == wtable) {
        // The table receiving writes; migrate rows before closing/dropping.

        if (unlikely(thd->opt_trace.is_started())) {
          Opt_trace_context *trace = &thd->opt_trace;
          Opt_trace_object wrapper(trace);
          Opt_trace_object convert(trace, "converting_tmp_table_to_ondisk");
          DBUG_ASSERT(error == HA_ERR_RECORD_FILE_FULL);
          convert.add_alnum("cause", "memory_table_size_exceeded");
          trace_tmp_table(trace, &new_table);
        }

        table->file->ha_index_or_rnd_end();

        if ((write_err = table->file->ha_rnd_init(1))) {
          /* purecov: begin inspected */
          table->file->print_error(write_err, MYF(ME_FATALERROR));
          write_err = 0;
          goto err_after_open;
          /* purecov: end */
        }

        if (table->no_rows) {
          new_table.file->extra(HA_EXTRA_NO_ROWS);
          new_table.no_rows = 1;
        }

        /*
          copy all old rows from heap table to on-disk table
          This is the only code that uses record[1] to read/write but this
          is safe as this is a temporary on-disk table without timestamp/
          autoincrement or partitioning.
        */
        while (!table->file->ha_rnd_next(new_table.record[1])) {
          write_err = new_table.file->ha_write_row(new_table.record[1]);
          DBUG_EXECUTE_IF("raise_error", write_err = HA_ERR_FOUND_DUPP_KEY;);
          if (write_err) goto err_after_open;
        }
        /* copy row that filled HEAP table */
        if ((write_err = new_table.file->ha_write_row(table->record[0]))) {
          if (!new_table.file->is_ignorable_error(write_err) ||
              !ignore_last_dup)
            goto err_after_open;
          if (is_duplicate) *is_duplicate = true;
        } else {
          if (is_duplicate) *is_duplicate = false;
        }

        (void)table->file->ha_rnd_end();
#ifndef DBUG_OFF
        rows_on_disk = true;
#endif
      }

      /* remove heap table and change to use on-disk table */

      if (table->pos_in_table_list &&
          table->pos_in_table_list->is_recursive_reference() &&
          table->file->inited) {
        /*
          Due to the last condition, this is guaranteed to be a recursive
          reference belonging to the unit which 'wtable' materializes, and not
          to the unit of another non-recursive reference (indeed, this other
          reference will re-use the rows of 'wtable', i.e. not execute its
          unit).
          This reference has opened a cursor.
          In the 'tmp_tables' list, 'wtable' is always before such recursive
          reference, as setup_materialized_derived_tmp_table() runs before
          substitute_recursive_reference(). So, we know the disk-based rows
          already exist at this point.
        */
        DBUG_ASSERT(rows_on_disk);
        (void)table->file->ha_rnd_end();
        rec_ref_w_open_cursor = true;
        psi_batch_started = table->file->end_psi_batch_mode_if_started();
      }

      // Closing the MEMORY table drops it if its ref count is down to zero.
      (void)table->file->ha_close();
      share.tmp_handler_count--;
    }

    /*
      Replace the guts of the old table with the new one, although keeping
      most members.
    */
    destroy(table->file);
    table->s = new_table.s;
    table->file = new_table.file;
    table->db_stat = new_table.db_stat;
    table->in_use = new_table.in_use;
    table->no_rows = new_table.no_rows;
    table->record[0] = new_table.record[0];
    table->record[1] = new_table.record[1];
    table->mem_root = std::move(new_table.mem_root);

    /*
      Depending on if this TABLE clone is early/late in optimization, or in
      execution, it has a JOIN_TAB or a QEP_TAB or none.
    */
    QEP_TAB *qep_tab = table->reginfo.qep_tab;
    QEP_shared_owner *tab;
    if (qep_tab)
      tab = qep_tab;
    else
      tab = table->reginfo.join_tab;

    /* Update quick select, if any. */
    if (tab && tab->quick()) {
      DBUG_ASSERT(table->pos_in_table_list->uses_materialization());
      tab->quick()->set_handler(table->file);
    }

    if (rec_ref_w_open_cursor) {
      /*
        The table just changed from MEMORY to INNODB. 'table' is a reader and
        had an open cursor to the MEMORY table. We closed the cursor, now need
        to open it to InnoDB and re-position it at the same row as before.
        Row positions (returned by handler::position()) are different in
        MEMORY and InnoDB - so the MEMORY row and InnoDB row have differing
        positions.
        We had read N rows of the MEMORY table, need to re-position our
        cursor after the same N rows in the InnoDB table.
      */
      if (psi_batch_started) table->file->start_psi_batch_mode();
      if (reposition_innodb_cursor(table, qep_tab->m_fetched_rows))
        goto err_after_proc_info; /* purecov: inspected */
    }

    // Point 'table' back to old_share; *old_share will be updated after loop.
    table->s = old_share;
    /*
      Update share-dependent pointers cached in 'table->file' and in
      read_set/write_set.
    */
    table->file->change_table_ptr(table, table->s);
    table->file->set_ha_share_ref(&table->s->ha_share);
    table->use_all_columns();

  }  // End of tables-processing loop

  plugin_unlock(0, old_plugin);
  share.db_plugin = my_plugin_lock(0, &share.db_plugin);
  *old_share = std::move(share);

  /*
    Now old_share is new, and all TABLEs in Derived_refs_iterator point to
    it, and so do their table->file: everything is consistent.
  */

  DBUG_ASSERT(initial_handler_count == wtable->s->tmp_handler_count);

  if (save_proc_info)
    thd_proc_info(thd, (!strcmp(save_proc_info, "Copying to tmp table")
                            ? "Copying to tmp table on disk"
                            : save_proc_info));
  DBUG_RETURN(0);

err_after_open:
  if (write_err) {
    DBUG_PRINT("error", ("Got error: %d", write_err));
    new_table.file->print_error(write_err, MYF(0));
  }
  if (table->file->inited) (void)table->file->ha_rnd_end();
  (void)new_table.file->ha_close();
err_after_create:
  new_table.file->ha_delete_table(new_table.s->table_name.str, nullptr);
err_after_alloc:
  destroy(new_table.file);
err_after_proc_info:
  thd_proc_info(thd, save_proc_info);
  // New share took control of old share mem_root; regain control:
  old_share->mem_root = std::move(share.mem_root);
  DBUG_RETURN(1);
}

/**
  Encode an InnoDB PK in 6 bytes, high-byte first; like
  InnoDB's dict_sys_write_row_id() does.
  @param rowid_bytes  where to store the result
  @param length       how many available bytes in rowid_bytes
  @param row_num      PK to encode
*/
void encode_innodb_position(uchar *rowid_bytes,
                            uint length MY_ATTRIBUTE((unused)),
                            ha_rows row_num) {
  DBUG_ASSERT(length == 6);
  for (int i = 0; i < 6; i++)
    rowid_bytes[i] = (uchar)(row_num >> ((5 - i) * 8));
}

/**
  Helper function for create_ondisk_from_heap().

  Our InnoDB on-disk intrinsic table uses an autogenerated
  auto-incrementing primary key:
  - first inserted row has pk=1 (see
  dict_table_get_next_table_sess_row_id()), second has pk=2, etc
  - ha_rnd_next uses a PK index scan so returns rows in PK order
  - position() returns the PK
  - ha_rnd_pos() takes the PK in input.

  @param table   table read by cursor
  @param row_num function should position on the row_num'th row in insertion
  order.
*/
bool reposition_innodb_cursor(TABLE *table, ha_rows row_num) {
  DBUG_ASSERT(table->s->db_type() == innodb_hton);
  if (table->file->ha_rnd_init(false)) return true; /* purecov: inspected */
  // Per the explanation above, the wanted InnoDB row has PK=row_num.
  uchar rowid_bytes[6];
  encode_innodb_position(rowid_bytes, sizeof(rowid_bytes), row_num);
  /*
    Go to the row, and discard the row. That places the cursor at
    the same row as before the engine conversion, so that rnd_next() will
    read the (row_num+1)th row.
  */
  return table->file->ha_rnd_pos(table->record[0], rowid_bytes);
}
