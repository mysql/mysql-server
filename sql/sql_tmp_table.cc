/* Copyright (c) 2011, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/** @file Temporary tables implementation */

#include "sql_tmp_table.h"

#include "myisam.h"               // MI_COLUMNDEF
#include "debug_sync.h"           // DEBUG_SYNC
#include "filesort.h"             // filesort_free_buffers
#include "item_func.h"            // Item_func
#include "item_sum.h"             // Item_sum
#include "mem_root_array.h"       // Mem_root_array
#include "opt_range.h"            // QUICK_SELECT_I
#include "opt_trace.h"            // Opt_trace_object
#include "opt_trace_context.h"    // Opt_trace_context
#include "sql_base.h"             // free_io_cache
#include "sql_class.h"            // THD
#include "sql_executor.h"         // SJ_TMP_TABLE
#include "sql_plugin.h"           // plugin_unlock

#include <algorithm>

using std::max;
using std::min;

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

Field *create_tmp_field_from_field(THD *thd, Field *org_field,
                                   const char *name, TABLE *table,
                                   Item_field *item)
{
  Field *new_field;

  new_field= org_field->new_field(thd->mem_root, table,
                                  table == org_field->table);
  if (new_field)
  {
    new_field->init(table);
    new_field->orig_table= org_field->table;
    if (item)
      item->result_field= new_field;
    else
      new_field->field_name= name;
    new_field->flags|= (org_field->flags & NO_DEFAULT_VALUE_FLAG);
    if (org_field->maybe_null() || (item && item->maybe_null))
      new_field->flags&= ~NOT_NULL_FLAG;	// Because of outer join
    if (org_field->type() == FIELD_TYPE_DOUBLE)
      ((Field_double *) new_field)->not_fixed= TRUE;
    /*
      This field will belong to an internal temporary table, it cannot be
      generated.
    */
    new_field->gcol_info= NULL;
    new_field->stored_in_db= true;
  }
  return new_field;
}

/**
  Create field for temporary table using type of given item.

  @param thd                   Thread handler
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

static Field *create_tmp_field_from_item(THD *thd, Item *item, TABLE *table,
                                         Func_ptr_array *copy_func,
                                         bool modify_item)
{
  bool maybe_null= item->maybe_null;
  Field *new_field= NULL;

  switch (item->result_type()) {
  case REAL_RESULT:
    new_field= new Field_double(item->max_length, maybe_null,
                                item->item_name.ptr(), item->decimals, TRUE);
    break;
  case INT_RESULT:
    /* 
      Select an integer type with the minimal fit precision.
      MY_INT32_NUM_DECIMAL_DIGITS is sign inclusive, don't consider the sign.
      Values with MY_INT32_NUM_DECIMAL_DIGITS digits may or may not fit into 
      Field_long : make them Field_longlong.  
    */
    if (item->max_length >= (MY_INT32_NUM_DECIMAL_DIGITS - 1))
      new_field=new Field_longlong(item->max_length, maybe_null,
                                   item->item_name.ptr(), item->unsigned_flag);
    else
      new_field=new Field_long(item->max_length, maybe_null,
                               item->item_name.ptr(), item->unsigned_flag);
    break;
  case STRING_RESULT:
    DBUG_ASSERT(item->collation.collation);
  
    /*
      DATE/TIME, GEOMETRY and JSON fields have STRING_RESULT result type.
      To preserve type they needed to be handled separately.
    */
    if (item->is_temporal() ||
        item->field_type() == MYSQL_TYPE_GEOMETRY ||
        item->field_type() == MYSQL_TYPE_JSON)
    {
      new_field= item->tmp_table_field_from_field_type(table, 1);
    }
    else
    {
      new_field= item->make_string_field(table);
    }
    new_field->set_derivation(item->collation.derivation);
    break;
  case DECIMAL_RESULT:
    new_field= Field_new_decimal::create_from_item(item);
    break;
  case ROW_RESULT:
  default:
    // This case should never be choosen
    DBUG_ASSERT(0);
    new_field= 0;
    break;
  }
  if (new_field)
    new_field->init(table);

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
  if (modify_item)
    item->set_result_field(new_field);
  if (item->type() == Item::NULL_ITEM)
    new_field->is_created_from_null_item= TRUE;
  return new_field;
}


/**
  Create field for information schema table.

  @param thd		Thread handler
  @param table		Temporary table
  @param item		Item to create a field for

  @retval
    0			on error
  @retval
    new_created field
*/

static Field *create_tmp_field_for_schema(THD *thd, Item *item, TABLE *table)
{
  if (item->field_type() == MYSQL_TYPE_VARCHAR)
  {
    Field *field;
    if (item->max_length > MAX_FIELD_VARCHARLENGTH)
      field= new Field_blob(item->max_length, item->maybe_null,
                            item->item_name.ptr(),
                            item->collation.collation, false);
    else
    {
      field= new Field_varstring(item->max_length, item->maybe_null,
                                 item->item_name.ptr(),
                                 table->s, item->collation.collation);
      table->s->db_create_options|= HA_OPTION_PACK_RECORD;
    }
    if (field)
      field->init(table);
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

  @retval NULL On error.

  @retval new_created field
*/

Field *create_tmp_field(THD *thd, TABLE *table,Item *item, Item::Type type,
                        Func_ptr_array *copy_func, Field **from_field,
                        Field **default_field,
                        bool group, bool modify_item,
                        bool table_cant_handle_bit_fields,
                        bool make_copy_field)
{
  Field *result= NULL;
  Item::Type orig_type= type;
  Item *orig_item= 0;

  if (type != Item::FIELD_ITEM &&
      item->real_item()->type() == Item::FIELD_ITEM)
  {
    orig_item= item;
    item= item->real_item();
    type= Item::FIELD_ITEM;
  }

  switch (type) {
  case Item::SUM_FUNC_ITEM:
  {
    Item_sum *item_sum=(Item_sum*) item;
    result= item_sum->create_tmp_field(group, table);
    if (!result)
      my_error(ER_OUT_OF_RESOURCES, MYF(ME_FATALERROR));
    break;
  }
  case Item::FIELD_ITEM:
  case Item::DEFAULT_VALUE_ITEM:
  case Item::TRIGGER_FIELD_ITEM:
  {
    Item_field *field= (Item_field*) item;
    bool orig_modify= modify_item;
    if (orig_type == Item::REF_ITEM)
      modify_item= 0;
    /*
      If item have to be able to store NULLs but underlaid field can't do it,
      create_tmp_field_from_field() can't be used for tmp field creation.
    */
    if (field->maybe_null && !field->field->maybe_null())
    {
      result= create_tmp_field_from_item(thd, item, table, NULL,
                                         modify_item);
      if (!result)
        break;
      *from_field= field->field;
      if (modify_item)
        field->result_field= result;
    } 
    else if (table_cant_handle_bit_fields && field->field->type() ==
             MYSQL_TYPE_BIT)
    {
      *from_field= field->field;
      result= create_tmp_field_from_item(thd, item, table, copy_func,
                                         modify_item);
      if (!result)
        break;
      if (modify_item)
        field->result_field= result;
    }
    else
    {
      result= create_tmp_field_from_field(thd, (*from_field= field->field),
                                          orig_item ? orig_item->item_name.ptr() :
                                          item->item_name.ptr(),
                                          table,
                                          modify_item ? field :
                                          NULL);
      if (!result)
        break;
    }
    if (orig_type == Item::REF_ITEM && orig_modify)
      ((Item_ref*)orig_item)->set_result_field(result);
    /*
      Fields that are used as arguments to the DEFAULT() function already have
      their data pointers set to the default value during name resulotion. See
      Item_default_value::fix_fields.
    */
    if (orig_type != Item::DEFAULT_VALUE_ITEM && field->field->eq_def(result))
      *default_field= field->field;
    break;
  }
  /* Fall through */
  case Item::FUNC_ITEM:
    if (((Item_func *) item)->functype() == Item_func::FUNC_SP)
    {
      Item_func_sp *item_func_sp= (Item_func_sp *) item;
      Field *sp_result_field= item_func_sp->get_sp_result_field();

      if (make_copy_field)
      {
        DBUG_ASSERT(item_func_sp->result_field);
        *from_field= item_func_sp->result_field;
      }
      else
      {
        copy_func->push_back(item);
      }

      result= create_tmp_field_from_field(thd,
                                          sp_result_field,
                                          item_func_sp->item_name.ptr(),
                                          table,
                                          NULL);
      if (!result)
        break;
      if (modify_item)
        item->set_result_field(result);
      break;
    }

    /* Fall through */
  case Item::COND_ITEM:
  case Item::FIELD_AVG_ITEM:
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
    if (make_copy_field)
    {
      DBUG_ASSERT(((Item_result_field*)item)->result_field);
      *from_field= ((Item_result_field*)item)->result_field;
    }
    result= create_tmp_field_from_item(thd, item, table,
                                       (make_copy_field ? NULL : copy_func),
                                       modify_item);
    break;
  case Item::TYPE_HOLDER:  
    result= ((Item_type_holder *)item)->make_field_by_type(table,
                                                           thd->is_strict_mode());
    if (!result)
      break;
    result->set_derivation(item->collation.derivation);
    break;
  default:					// Dosen't have to be stored
    DBUG_ASSERT(false);
    break;
  }
  return result;
}

/*
  Set up column usage bitmaps for a temporary table

  IMPLEMENTATION
    For temporary tables, we need one bitmap with all columns set and
    a tmp_set bitmap to be used by things like filesort.
*/

static void setup_tmp_table_column_bitmaps(TABLE *table, uchar *bitmaps)
{
  uint field_count= table->s->fields;
  bitmap_init(&table->def_read_set, (my_bitmap_map*) bitmaps, field_count,
              FALSE);
  bitmap_init(&table->tmp_set,
              (my_bitmap_map*) (bitmaps + bitmap_buffer_size(field_count)),
              field_count, FALSE);
  bitmap_init(&table->cond_set,
              (my_bitmap_map*) (bitmaps + bitmap_buffer_size(field_count) * 2),
              field_count, FALSE);
  /* write_set and all_set are copies of read_set */
  table->def_write_set= table->def_read_set;
  table->s->all_set= table->def_read_set;
  bitmap_set_all(&table->s->all_set);
  table->default_column_bitmaps();
  table->s->column_bitmap_size= bitmap_buffer_size(field_count);
}

/**
  Cache for the storage engine properties for the alternative temporary table
  storage engines. This cache is initialized during startup of the server by
  asking the storage engines for the values properties.
*/

class Cache_temp_engine_properties
{
public:
  static uint HEAP_MAX_KEY_LENGTH;
  static uint MYISAM_MAX_KEY_LENGTH;
  static uint INNODB_MAX_KEY_LENGTH;
  static uint HEAP_MAX_KEY_PART_LENGTH;
  static uint MYISAM_MAX_KEY_PART_LENGTH;
  static uint INNODB_MAX_KEY_PART_LENGTH;
  static uint HEAP_MAX_KEY_PARTS;
  static uint MYISAM_MAX_KEY_PARTS;
  static uint INNODB_MAX_KEY_PARTS;

  static void init(THD *thd);
};

void Cache_temp_engine_properties::init(THD *thd)
{
  handler *handler;
  plugin_ref db_plugin;

  // Cache HEAP engine's
  db_plugin= ha_lock_engine(0, heap_hton);
  handler= get_new_handler((TABLE_SHARE *)0, thd->mem_root, heap_hton);
  HEAP_MAX_KEY_LENGTH= handler->max_key_length();
  HEAP_MAX_KEY_PART_LENGTH= handler->max_key_part_length();
  HEAP_MAX_KEY_PARTS= handler->max_key_parts();
  delete handler;
  plugin_unlock(0, db_plugin);
  // Cache MYISAM engine's
  db_plugin= ha_lock_engine(0, myisam_hton);
  handler= get_new_handler((TABLE_SHARE *)0, thd->mem_root, myisam_hton);
  MYISAM_MAX_KEY_LENGTH= handler->max_key_length();
  MYISAM_MAX_KEY_PART_LENGTH= handler->max_key_part_length();
  MYISAM_MAX_KEY_PARTS= handler->max_key_parts();
  delete handler;
  plugin_unlock(0, db_plugin);
  // Cache INNODB engine's
  db_plugin= ha_lock_engine(0, innodb_hton);
  handler= get_new_handler((TABLE_SHARE *)0, thd->mem_root, innodb_hton);
  INNODB_MAX_KEY_LENGTH= handler->max_key_length();
  /*
    For ha_innobase::max_supported_key_part_length(), the returned value
    relies on innodb_large_prefix. However, in innodb itself, the limitation
    on key_part length is up to the ROW_FORMAT. In current trunk, internal
    temp table's ROW_FORMAT is DYNAMIC. In order to keep the consistence
    between server and innodb, here we hard-coded 3072 as the maximum of
    key_part length supported by innodb until bug#20629014 is fixed.

    TODO: Remove the hard-code here after bug#20629014 is fixed.
  */
  INNODB_MAX_KEY_PART_LENGTH= 3072;
  INNODB_MAX_KEY_PARTS= handler->max_key_parts();
  delete handler;
  plugin_unlock(0, db_plugin);
}

uint Cache_temp_engine_properties::HEAP_MAX_KEY_LENGTH= 0;
uint Cache_temp_engine_properties::MYISAM_MAX_KEY_LENGTH= 0;
uint Cache_temp_engine_properties::INNODB_MAX_KEY_LENGTH= 0;
uint Cache_temp_engine_properties::HEAP_MAX_KEY_PART_LENGTH= 0;
uint Cache_temp_engine_properties::MYISAM_MAX_KEY_PART_LENGTH= 0;
uint Cache_temp_engine_properties::INNODB_MAX_KEY_PART_LENGTH= 0;
uint Cache_temp_engine_properties::HEAP_MAX_KEY_PARTS= 0;
uint Cache_temp_engine_properties::MYISAM_MAX_KEY_PARTS= 0;
uint Cache_temp_engine_properties::INNODB_MAX_KEY_PARTS= 0;

void init_cache_tmp_engine_properties()
{
  DBUG_ASSERT(!current_thd);
  THD *thd= new THD();
  thd->thread_stack= pointer_cast<char *>(&thd);
  thd->store_globals();
  Cache_temp_engine_properties::init(thd);
  delete thd;
}

/**
  Get the minimum of max_key_length and max_key_part_length.
  The minimum is between HEAP engine and internal_tmp_disk_storage_engine.

  @param[out] max_key_length Minimum of max_key_length
  @param[out] max_key_part_length Minimum of max_key_part_length
*/

void get_max_key_and_part_length(uint *max_key_length,
                                 uint *max_key_part_length)
{
  // Make sure these cached properties are initialized.
  DBUG_ASSERT(Cache_temp_engine_properties::HEAP_MAX_KEY_LENGTH);

  switch (internal_tmp_disk_storage_engine)
  {
  case TMP_TABLE_MYISAM:
    *max_key_length=
      std::min(Cache_temp_engine_properties::HEAP_MAX_KEY_LENGTH,
               Cache_temp_engine_properties::MYISAM_MAX_KEY_LENGTH);
    *max_key_part_length=
      std::min(Cache_temp_engine_properties::HEAP_MAX_KEY_PART_LENGTH,
               Cache_temp_engine_properties::MYISAM_MAX_KEY_PART_LENGTH);
    /*
      create_tmp_table() tests tmp_se->max_key_parts() too, not only HEAP's.
      It is correct as long as HEAP'S not bigger than on-disk temp table
      engine's, which we check here.
    */
    DBUG_ASSERT(Cache_temp_engine_properties::HEAP_MAX_KEY_PARTS <=
                Cache_temp_engine_properties::MYISAM_MAX_KEY_PARTS);
    break;
  case TMP_TABLE_INNODB:
  default:
    *max_key_length=
      std::min(Cache_temp_engine_properties::HEAP_MAX_KEY_LENGTH,
               Cache_temp_engine_properties::INNODB_MAX_KEY_LENGTH);
    *max_key_part_length=
      std::min(Cache_temp_engine_properties::HEAP_MAX_KEY_PART_LENGTH,
               Cache_temp_engine_properties::INNODB_MAX_KEY_PART_LENGTH);
    DBUG_ASSERT(Cache_temp_engine_properties::HEAP_MAX_KEY_PARTS <=
                Cache_temp_engine_properties::INNODB_MAX_KEY_PARTS);
    break;
  }
}

/**
  Create a temporary name for one field if the field_name is empty.

  @param thd          Thread handle
  @param field_index  Index of this field in table->field
*/

static const char *create_tmp_table_field_tmp_name(THD *thd, int field_index)
{
  char buf[64];
  my_snprintf(buf, 64, "tmp_field_%d", field_index);
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

static void register_hidden_field(TABLE *table, 
                              Field **default_field,
                              Field **from_field,
                              uint *blob_field,
                              Field *field)
{
  uint i;
  Field **tmp_field= table->field;

  /* Increase all of registed fields index */
  for (i= 0; i < table->s->fields; i++)
    tmp_field[i]->field_index++;

  // Increase the field_index of visible blob field
  for (i= 0; i < table->s->blob_fields; i++)
    blob_field[i]++;
  // Insert field
  table->field[-1]= field;
  default_field[-1]= NULL;
  from_field[-1]= NULL;
  field->table= field->orig_table= table;
  field->field_index= 0;
}

/**
  Create a temp table according to a field list.

  Given field pointers are changed to point at tmp_table for
  send_result_set_metadata. The table object is self contained: it's
  allocated in its own memory root, as well as Field objects
  created for table columns.
  This function will replace Item_sum items in 'fields' list with
  corresponding Item_field items, pointing at the fields in the
  temporary table, unless this was prohibited by TRUE
  value of argument save_sum_fields. The Item_field objects
  are created in THD memory root.

  @param thd                  thread handle
  @param param                a description used as input to create the table
  @param fields               list of items that will be used to define
                              column types of the table (also see NOTES)
  @param group                Group key to use for temporary table, NULL if none
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
#define AVG_STRING_LENGTH_TO_PACK_ROWS   64
#define RATIO_TO_PACK_ROWS	       2
#define MIN_STRING_LENGTH_TO_PACK_ROWS   10

TABLE *
create_tmp_table(THD *thd, Temp_table_param *param, List<Item> &fields,
		 ORDER *group, bool distinct, bool save_sum_fields,
		 ulonglong select_options, ha_rows rows_limit,
		 const char *table_alias)
{
  MEM_ROOT *mem_root_save, own_root;
  TABLE *table;
  TABLE_SHARE *share;
  uint	i,field_count,null_count,null_pack_length;
  uint  copy_func_count= param->func_count;
  uint  hidden_null_count, hidden_null_pack_length;
  long hidden_field_count;
  uint  blob_count,group_null_items, string_count;
  uint  temp_pool_slot=MY_BIT_NONE;
  uint fieldnr= 0;
  ulong reclength, string_total_length, distinct_key_length= 0;
  /**
    When true, enforces unique constraint (by adding a hidden hash_field and
    creating a key over this field) when:
    (1) unique key is too long or
    (2) number of key parts in distinct key is too big.
  */
  bool  using_unique_constraint= false;
  bool  use_packed_rows= false;
  bool  not_all_columns= !(select_options & TMP_TABLE_ALL_COLUMNS);
  char  *tmpname,path[FN_REFLEN];
  uchar	*pos, *group_buff, *bitmaps;
  uchar *null_flags;
  Field **reg_field, **from_field, **default_field;
  uint *blob_field;
  Copy_field *copy=0;
  KEY *keyinfo;
  KEY_PART_INFO *key_part_info;
  MI_COLUMNDEF *recinfo;
  /*
    total_uneven_bit_length is uneven bit length for visible fields
    hidden_uneven_bit_length is uneven bit length for hidden fields
  */
  uint total_uneven_bit_length= 0, hidden_uneven_bit_length= 0;
  bool force_copy_fields= param->force_copy_fields;

  uint max_key_length;
  uint max_key_part_length;
  /* Treat sum functions as normal ones when loose index scan is used. */
  save_sum_fields|= param->precomputed_group_by;
  DBUG_ENTER("create_tmp_table");
  DBUG_PRINT("enter",
             ("distinct: %d  save_sum_fields: %d  rows_limit: %lu  group: %d",
              (int) distinct, (int) save_sum_fields,
              (ulong) rows_limit, MY_TEST(group)));

  thd->inc_status_created_tmp_tables();

  if (use_temp_pool && !(test_flags & TEST_KEEP_TMP_TABLES))
    temp_pool_slot = bitmap_lock_set_next(&temp_pool);

  if (temp_pool_slot != MY_BIT_NONE) // we got a slot
    sprintf(path, "%s_%lx_%i", tmp_file_prefix,
            current_pid, temp_pool_slot);
  else
  {
    /* if we run out of slots or we are not using tempool */
    DBUG_ASSERT(sizeof(my_thread_id) == 4);
    sprintf(path,"%s%lx_%x_%x", tmp_file_prefix, current_pid,
            thd->thread_id(), thd->tmp_table++);
  }

  /*
    No need to change table name to lower case as we are only creating
    MyISAM or HEAP tables here
  */
  fn_format(path, path, mysql_tmpdir, "", MY_REPLACE_EXT|MY_UNPACK_FILENAME);


  if (group)
  {
    if (!param->quick_group)
      group=0;					// Can't use group key
    else for (ORDER *tmp=group ; tmp ; tmp=tmp->next)
    {
      /*
        marker == 4 means two things:
        - store NULLs in the key, and
        - convert BIT fields to 64-bit long, needed because MEMORY tables
          can't index BIT fields.
      */
      (*tmp->item)->marker= 4;
      const uint char_len=
        (*tmp->item)->max_length / (*tmp->item)->collation.collation->mbmaxlen;
      if (char_len > CONVERT_IF_BIGGER_TO_BLOB)
        using_unique_constraint= true;
    }
    if (group)
    {
      if (param->group_length >= MAX_BLOB_WIDTH)
        using_unique_constraint= true;
      distinct=0;				// Can't use distinct
    }
  }

  field_count=param->field_count+param->func_count+param->sum_func_count;
  hidden_field_count=param->hidden_field_count;

  /*
    When loose index scan is employed as access method, it already
    computes all groups and the result of all aggregate functions. We
    make space for the items of the aggregate function in the list of
    functions Temp_table_param::items_to_copy, so that the values of
    these items are stored in the temporary table.
  */
  if (param->precomputed_group_by)
    copy_func_count+= param->sum_func_count;
  
  init_sql_alloc(key_memory_TABLE, &own_root, TABLE_ALLOC_BLOCK_SIZE, 0);

  void *rawmem= alloc_root(&own_root, sizeof(Func_ptr_array));
  if (!rawmem)
    DBUG_RETURN(NULL);                        /* purecov: inspected */
  Func_ptr_array *copy_func= new (rawmem) Func_ptr_array(&own_root);
  copy_func->reserve(copy_func_count);

  if (!multi_alloc_root(&own_root,
                        &table, sizeof(*table),
                        &share, sizeof(*share),
                        &reg_field, sizeof(Field*) * (field_count + 2),
                        &default_field, sizeof(Field*) * (field_count + 1),
                        &blob_field, sizeof(uint)*(field_count+2),
                        &from_field, sizeof(Field*)*(field_count + 1),
                        &param->keyinfo, sizeof(*param->keyinfo),
                        &key_part_info,
                        sizeof(*key_part_info)*(param->group_parts+1),
                        &param->start_recinfo,
                        sizeof(*param->recinfo)*(field_count*2+4),
                        &tmpname, strlen(path)+1,
                        &group_buff, (group && !using_unique_constraint ?
                                      param->group_length : 0),
                        &bitmaps, bitmap_buffer_size(field_count + 1) * 3,
                        NullS))
  {
    if (temp_pool_slot != MY_BIT_NONE)
      bitmap_lock_clear_bit(&temp_pool, temp_pool_slot);
    DBUG_RETURN(NULL);				/* purecov: inspected */
  }
  /* Copy_field belongs to Temp_table_param, allocate it in THD mem_root */
  if (!(param->copy_field= copy= new (thd->mem_root) Copy_field[field_count]))
  {
    if (temp_pool_slot != MY_BIT_NONE)
      bitmap_lock_clear_bit(&temp_pool, temp_pool_slot);
    free_root(&own_root, MYF(0));               /* purecov: inspected */
    DBUG_RETURN(NULL);				/* purecov: inspected */
  }
  param->items_to_copy= copy_func;
  my_stpcpy(tmpname,path);
  /* make table according to fields */

  new (table) TABLE;
  memset(reg_field, 0, sizeof(Field*)*(field_count + 2));
  memset(default_field, 0, sizeof(Field*) * (field_count + 1));
  memset(from_field, 0, sizeof(Field*)*(field_count + 1));

  // This invokes (the synthesized) st_mem_root &operator=(const st_mem_root&)
  table->mem_root= own_root;
  mem_root_save= thd->mem_root;
  thd->mem_root= &table->mem_root;
  copy_func->set_mem_root(&table->mem_root);

  // Leave the first place to be prepared for hash_field
  reg_field++;
  default_field++;
  from_field++;
  table->field=reg_field;
  table->alias= table_alias;
  table->reginfo.lock_type=TL_WRITE;	/* Will be updated */
  table->db_stat=HA_OPEN_KEYFILE+HA_OPEN_RNDFILE;
  table->temp_pool_slot = temp_pool_slot;
  table->copy_blobs= 1;
  table->in_use= thd;
  table->quick_keys.init();
  table->possible_quick_keys.init();
  table->covering_keys.init();
  table->merge_keys.init();
  table->keys_in_use_for_query.init();
  table->keys_in_use_for_group_by.init();
  table->keys_in_use_for_order_by.init();

  table->s= share;
  init_tmp_table_share(thd, share, "", 0, tmpname, tmpname);
  share->blob_field= blob_field;
  share->db_low_byte_first=1;                // True for HEAP and MyISAM
  share->table_charset= param->table_charset;
  share->primary_key= MAX_KEY;               // Indicate no primary key
  share->keys_for_keyread.init();
  share->keys_in_use.init();
  share->keys= 0;
  if (param->schema_table)
    share->db= INFORMATION_SCHEMA_NAME;

  /* Calculate which type of fields we will store in the temporary table */

  reclength= string_total_length= 0;
  blob_count= string_count= null_count= hidden_null_count= group_null_items= 0;
  param->using_outer_summary_function= 0;

  List_iterator_fast<Item> li(fields);
  Item *item;
  Field **tmp_from_field=from_field;
  while ((item=li++))
  {
    Field *new_field= NULL;
    Item::Type type= item->type();
    if (type == Item::COPY_STR_ITEM)
    {
      item= ((Item_copy *)item)->get_item();
      type= item->type();
    }
    if (not_all_columns)
    {
      if (item->with_sum_func && type != Item::SUM_FUNC_ITEM)
      {
        if (item->used_tables() & OUTER_REF_TABLE_BIT)
          item->update_used_tables();
        if (type == Item::SUBSELECT_ITEM ||
            (item->used_tables() & ~OUTER_REF_TABLE_BIT))
        {
	  /*
	    Mark that we have ignored an item that refers to a summary
	    function. We need to know this if someone is going to use
	    DISTINCT on the result.
	  */
	  param->using_outer_summary_function= 1;
          goto update_hidden;
        }
      }

      if (item->const_item() && (int)hidden_field_count <= 0)
        continue; // We don't have to store this
    }
    if (type == Item::SUM_FUNC_ITEM && !group && !save_sum_fields)
    {						/* Can't calc group yet */
      Item_sum *sum_item= (Item_sum *) item;
      for (i=0 ; i < sum_item->get_arg_count() ; i++)
      {
        Item *arg= sum_item->get_arg(i);
        if (!arg->const_item())
        {
          new_field=
            create_tmp_field(thd, table, arg, arg->type(), copy_func,
                             tmp_from_field, &default_field[fieldnr],
                             group != 0, not_all_columns,
                             distinct, false);
          if (!new_field)
            goto err;					// Should be OOM
          tmp_from_field++;
          reclength+= new_field->pack_length();
          if (new_field->flags & BLOB_FLAG)
          {
            *blob_field++= fieldnr;
            blob_count++;
          }
          if (new_field->type() == MYSQL_TYPE_BIT)
            total_uneven_bit_length+= new_field->field_length & 7;
          *(reg_field++)= new_field;
          if (new_field->real_type() == MYSQL_TYPE_STRING ||
              new_field->real_type() == MYSQL_TYPE_VARCHAR)
          {
            string_count++;
            string_total_length+= new_field->pack_length();
          }
          thd->mem_root= mem_root_save;
          arg= sum_item->set_arg(i, thd, new Item_field(new_field));
          thd->mem_root= &table->mem_root;
          if (!(new_field->flags & NOT_NULL_FLAG))
          {
            null_count++;
            /*
              new_field->maybe_null() is still false, it will be
              changed below. But we have to setup Item_field correctly
            */
            arg->maybe_null=1;
          }
          new_field->field_index= fieldnr++;
          /* InnoDB temp table doesn't allow field with empty_name */
          if (!new_field->field_name)
            new_field->field_name= create_tmp_table_field_tmp_name(thd,
                                                        new_field->field_index);
        }
      }
    }
    else
    {
      /*
        The last parameter to create_tmp_field() is a bit tricky:

        We need to set it to 0 in union, to get fill_record() to modify the
        temporary table.
        We need to set it to 1 on multi-table-update and in select to
        write rows to the temporary table.
        We here distinguish between UNION and multi-table-updates by the fact
        that in the later case group is set to the row pointer.
      */
      new_field= (param->schema_table) ?
        create_tmp_field_for_schema(thd, item, table) :
        create_tmp_field(thd, table, item, type, copy_func,
                         tmp_from_field, &default_field[fieldnr],
                         group != 0,
                         !force_copy_fields &&
                           (not_all_columns || group !=0),
                         /*
                           If item->marker == 4 then we force create_tmp_field
                           to create a 64-bit longs for BIT fields because HEAP
                           tables can't index BIT fields directly. We do the same
                           for distinct, as we want the distinct index to be
                           usable in this case too.
                         */
                         item->marker == 4 || param->bit_fields_as_long,
                         force_copy_fields);

      if (!new_field)
      {
        DBUG_ASSERT(thd->is_fatal_error);
        goto err;				// Got OOM
      }
      if (type == Item::SUM_FUNC_ITEM)
	((Item_sum *) item)->result_field= new_field;
      tmp_from_field++;
      reclength+=new_field->pack_length();
      if (!(new_field->flags & NOT_NULL_FLAG))
	null_count++;
      if (new_field->type() == MYSQL_TYPE_BIT)
        total_uneven_bit_length+= new_field->field_length & 7;
      if (new_field->flags & BLOB_FLAG)
      {
        *blob_field++= fieldnr;
	blob_count++;
      }

      if (new_field->real_type() == MYSQL_TYPE_STRING ||
          new_field->real_type() == MYSQL_TYPE_VARCHAR)
      {
        string_count++;
        string_total_length+= new_field->pack_length();
      }
      // In order to reduce footprint ask SE to pack variable-length fields.
      if (new_field->type() == MYSQL_TYPE_VAR_STRING ||
          new_field->type() == MYSQL_TYPE_VARCHAR)
        table->s->db_create_options|= HA_OPTION_PACK_RECORD;

      if (item->marker == 4 && item->maybe_null)
      {
	group_null_items++;
	new_field->flags|= GROUP_FLAG;
      }
      new_field->field_index= fieldnr++;
      *(reg_field++)= new_field;
      /* InnoDB temp table doesn't allow field with empty_name */
      if (!new_field->field_name)
        new_field->field_name= create_tmp_table_field_tmp_name(thd, new_field->field_index);
    }

update_hidden:
    /*
      Calculate length of distinct key. The goal is to decide what to use -
      key or unique constraint. As blobs force unique constraint on their
      own due to their length, they aren't taken into account.
    */
    if (distinct && !using_unique_constraint && hidden_field_count <= 0 &&
        new_field)
    {
      if (new_field->flags & BLOB_FLAG)
        using_unique_constraint= true;
      else
        distinct_key_length+= new_field->pack_length();
    }
    if (!--hidden_field_count)
    {
      /*
        This was the last hidden field; Remember how many hidden fields could
        have null
      */
      hidden_null_count=null_count;
      /*
	We need to update hidden_field_count as we may have stored group
	functions with constant arguments
      */
      param->hidden_field_count= fieldnr;
      null_count= 0;
      /*
        On last hidden field we store uneven bit length in
        hidden_uneven_bit_length and proceed calculation of
        uneven bits for visible fields into
        total_uneven_bit_length variable.
      */
      hidden_uneven_bit_length= total_uneven_bit_length;
      total_uneven_bit_length= 0;
    }
  }
  DBUG_ASSERT(fieldnr == (uint) (reg_field - table->field));
  DBUG_ASSERT(field_count >= (uint) (reg_field - table->field));
  field_count= fieldnr;
  *reg_field= 0;
  *blob_field= 0;				// End marker
  share->fields= field_count;
  share->blob_fields= blob_count;

  /* If result table is small; use a heap */
  if (select_options & TMP_TABLE_FORCE_MYISAM)
  {
    share->db_plugin= ha_lock_engine(0, myisam_hton);
    table->file= get_new_handler(share, &table->mem_root,
                                 share->db_type());
  }
  else if (blob_count ||
           (thd->variables.big_tables &&
            !(select_options & SELECT_SMALL_RESULT)))
  {
    /*
     * Except for special conditions, tmp table engine will be choosen by user.
     */
    switch (internal_tmp_disk_storage_engine)
    {
    case TMP_TABLE_MYISAM:
      share->db_plugin= ha_lock_engine(0, myisam_hton);
      break;
    case TMP_TABLE_INNODB:
      share->db_plugin= ha_lock_engine(0, innodb_hton);
      break;
    default:
      DBUG_ASSERT(0);
      share->db_plugin= ha_lock_engine(0, innodb_hton);
    }

    table->file= get_new_handler(share, &table->mem_root,
                                 share->db_type());
  }
  else
  {
    share->db_plugin= ha_lock_engine(0, heap_hton);
    table->file= get_new_handler(share, &table->mem_root,
                                 share->db_type());
  }

  /*
    Different temp table engine supports different max_key_length
    and max_key_part_lengthi. If HEAP engine is selected, it can be
    possible to convert into on-disk engine later. We must choose
    the minimal of max_key_length and max_key_part_length between
    HEAP engine and possible on-disk engine to verify whether unique
    constraint is needed so that the convertion goes well.
   */
  get_max_key_and_part_length(&max_key_length,
                              &max_key_part_length);

  if (!table->file)
    goto err;
  if (group &&
      (param->group_parts > table->file->max_key_parts() ||
       param->group_length > max_key_length))
    using_unique_constraint= true;
  keyinfo= param->keyinfo;
  keyinfo->table= table;

  if (group)
  {
    DBUG_PRINT("info",("Creating group key in temporary table"));
    table->group= group;				/* Table is grouped by key */
    param->group_buff= group_buff;
    share->keys= 1;
    // Use key definition created below only if the key isn't too long.
    // Otherwise a dedicated key over a hash value will be created and this
    // definition will be used by server to calc hash.
    if (!using_unique_constraint)
    {
      table->key_info= share->key_info= keyinfo;
      keyinfo->key_part= key_part_info;
      keyinfo->flags= HA_NOSAME;
      keyinfo->usable_key_parts= keyinfo->user_defined_key_parts=
        param->group_parts;
      keyinfo->actual_key_parts= keyinfo->user_defined_key_parts;
      keyinfo->rec_per_key= 0;
      keyinfo->algorithm= HA_KEY_ALG_UNDEF;
      keyinfo->set_rec_per_key_array(NULL, NULL);
      keyinfo->set_in_memory_estimate(IN_MEMORY_ESTIMATE_UNKNOWN);
      keyinfo->name= (char*) "<group_key>";
      ORDER *cur_group= group;
      for (; cur_group ; cur_group= cur_group->next, key_part_info++)
      {
        Field *field= (*cur_group->item)->get_tmp_table_field();
        DBUG_ASSERT(field->table == table);
        key_part_info->init_from_field(field);

        /* In GROUP BY 'a' and 'a ' are equal for VARCHAR fields */
        key_part_info->key_part_flag|= HA_END_SPACE_ARE_EQUAL;

        if (key_part_info->store_length > max_key_part_length)
        {
          using_unique_constraint= true;
          break;
        }
      }
      keyinfo->actual_flags= keyinfo->flags;
    }
  }

  if (distinct && field_count != param->hidden_field_count)
  {
    /*
      Create an unique key or an unique constraint over all columns
      that should be in the result.  In the temporary table, there are
      'param->hidden_field_count' extra columns, whose null bits are stored
      in the first 'hidden_null_pack_length' bytes of the row.
    */
    DBUG_PRINT("info",("hidden_field_count: %d", param->hidden_field_count));
    share->keys= 1;
    table->distinct= 1;
    if (!using_unique_constraint)
    {
      Field **reg_field;
      keyinfo->user_defined_key_parts= field_count-param->hidden_field_count;
      keyinfo->actual_key_parts= keyinfo->user_defined_key_parts;
      if (!(key_part_info= (KEY_PART_INFO*)
            alloc_root(&table->mem_root,
                       keyinfo->user_defined_key_parts * sizeof(KEY_PART_INFO))))
        goto err;
      memset(key_part_info, 0, keyinfo->user_defined_key_parts *
             sizeof(KEY_PART_INFO));
      table->key_info= share->key_info= keyinfo;
      keyinfo->key_part= key_part_info;
      keyinfo->actual_flags= keyinfo->flags= HA_NOSAME | HA_NULL_ARE_EQUAL;
      // TODO rename to <distinct_key>
      keyinfo->name= (char*) "<auto_key>";
      keyinfo->algorithm= HA_KEY_ALG_UNDEF;
      keyinfo->set_rec_per_key_array(NULL, NULL);
      keyinfo->set_in_memory_estimate(IN_MEMORY_ESTIMATE_UNKNOWN);
      /* Create a distinct key over the columns we are going to return */
      for (i=param->hidden_field_count, reg_field=table->field + i ;
           i < field_count;
           i++, reg_field++, key_part_info++)
      {
        key_part_info->init_from_field(*reg_field);
        if (key_part_info->store_length > max_key_part_length)
        {
          using_unique_constraint= true;
          break;
        }
      }
    }
  }

  /*
    To enforce unique constraint we need to add a field to hold key's hash
    1) already detected unique constraint
    2) distinct key is too long
    3) number of keyparts in distinct key is too big
  */
  if (using_unique_constraint ||                              // 1
      distinct_key_length > max_key_length ||                 // 2
      (distinct &&                                            // 3
       (fieldnr - param->hidden_field_count) > table->file->max_key_parts()))
  {
    using_unique_constraint= true;
    Field_longlong *field= new(&table->mem_root) 
          Field_longlong(sizeof(ulonglong), false, "<hash_field>", true);
    if (!field)
    {
      DBUG_ASSERT(thd->is_fatal_error);
      goto err;   // Got OOM
    }

    // Mark hash_field as NOT NULL
    field->flags &= NOT_NULL_FLAG;
    // Register hash_field as a hidden field.
    register_hidden_field(table, default_field,
                          from_field, share->blob_field, field);
    // Repoint arrays
    table->field--;
    default_field--;
    from_field--;
    reclength+= field->pack_length();
    field_count= ++fieldnr;
    param->hidden_field_count++;
    share->fields= field_count;
    table->hash_field= field;
  }

  // Update the handler with information about the table object
  table->file->change_table_ptr(table, share);
  table->hidden_field_count= param->hidden_field_count;

  if (table->file->set_ha_share_ref(&share->ha_share))
  {
    delete table->file;
    goto err;
  }

  // Initialize cost model for this table
  table->init_cost_model(thd->cost_model());

  if (!using_unique_constraint)
    reclength+= group_null_items;	// null flag is stored separately

  if (blob_count == 0)
  {
    /* We need to ensure that first byte is not 0 for the delete link */
    if (param->hidden_field_count)
      hidden_null_count++;
    else
      null_count++;
  }
  hidden_null_pack_length= (hidden_null_count + 7 +
                            hidden_uneven_bit_length) / 8;
  null_pack_length= (hidden_null_pack_length +
                     (null_count + total_uneven_bit_length + 7) / 8);
  reclength+=null_pack_length;
  if (!reclength)
    reclength=1;				// Dummy select
  /* Use packed rows if there is blobs or a lot of space to gain */
  if (blob_count ||
      (string_total_length >= STRING_TOTAL_LENGTH_TO_PACK_ROWS &&
      (reclength / string_total_length <= RATIO_TO_PACK_ROWS ||
       string_total_length / string_count >= AVG_STRING_LENGTH_TO_PACK_ROWS)))
    use_packed_rows= true;

  if (!use_packed_rows)
    share->db_create_options&= ~HA_OPTION_PACK_RECORD;

  share->reclength= reclength;
  {
    uint alloc_length=ALIGN_SIZE(reclength+MI_UNIQUE_HASH_LENGTH+1);
    share->rec_buff_length= alloc_length;
    if (!(table->record[0]= (uchar*)
          alloc_root(&table->mem_root, alloc_length*3)))
      goto err;
    table->record[1]= table->record[0]+alloc_length;
    share->default_values= table->record[1]+alloc_length;
  }
  param->func_count= copy_func->size();
  DBUG_ASSERT(param->func_count <= copy_func_count); // Used <= allocated

  setup_tmp_table_column_bitmaps(table, bitmaps);

  recinfo=param->start_recinfo;
  null_flags= table->record[0];
  pos= table->record[0] + null_pack_length;
  if (null_pack_length)
  {
    memset(recinfo, 0, sizeof(*recinfo));
    recinfo->type=FIELD_NORMAL;
    recinfo->length=null_pack_length;
    recinfo++;
    memset(null_flags, 255, null_pack_length);	// Set null fields

    table->null_flags= table->record[0];
    share->null_fields= null_count+ hidden_null_count;
    share->null_bytes= null_pack_length;
  }
  null_count= (blob_count == 0) ? 1 : 0;
  hidden_field_count=param->hidden_field_count;
  DBUG_ASSERT((uint)hidden_field_count <= field_count);
  for (i=0,reg_field=table->field; i < field_count; i++,reg_field++,recinfo++)
  {
    Field *field= *reg_field;
    uint length;
    memset(recinfo, 0, sizeof(*recinfo));

    if (!(field->flags & NOT_NULL_FLAG))
    {
      if (field->flags & GROUP_FLAG && !using_unique_constraint)
      {
	/*
	  We have to reserve one byte here for NULL bits,
	  as this is updated by 'end_update()'
	*/
	*pos++=0;				// Null is stored here
	recinfo->length=1;
	recinfo->type=FIELD_NORMAL;
	recinfo++;
	memset(recinfo, 0, sizeof(*recinfo));
      }
      else
      {
	recinfo->null_bit= (uint8)1 << (null_count & 7);
	recinfo->null_pos= null_count/8;
      }
      field->move_field(pos,null_flags+null_count/8,
			(uint8)1 << (null_count & 7));
      null_count++;
    }
    else
      field->move_field(pos,(uchar*) 0,0);
    if (field->type() == MYSQL_TYPE_BIT)
    {
      /* We have to reserve place for extra bits among null bits */
      ((Field_bit*) field)->set_bit_ptr(null_flags + null_count / 8,
                                        null_count & 7);
      null_count+= (field->field_length & 7);
    }
    field->reset();

    /*
      Test if there is a default field value. The test for ->ptr is to skip
      'offset' fields generated by initalize_tables
    */
    if (default_field[i] && default_field[i]->ptr)
    {
      /* 
         default_field[i] is set only in the cases  when 'field' can
         inherit the default value that is defined for the field referred
         by the Item_field object from which 'field' has been created.
      */
      Field *orig_field= default_field[i];
      /* Get the value from default_values */
      my_ptrdiff_t diff= orig_field->table->default_values_offset();
      orig_field->move_field_offset(diff);      // Points now at default_values
      if (orig_field->is_real_null())
        field->set_null();
      else
      {
        field->set_notnull();
        memcpy(field->ptr, orig_field->ptr, field->pack_length());
      }
      orig_field->move_field_offset(-diff);     // Back to record[0]
    } 

    if (from_field[i])
    {						/* Not a table Item */
      copy->set(field,from_field[i],save_sum_fields);
      copy++;
    }
    length=field->pack_length();
    pos+= length;

    /* Make entry for create table */
    recinfo->length=length;
    if (field->flags & BLOB_FLAG)
      recinfo->type= (int) FIELD_BLOB;
    else if (use_packed_rows &&
             field->real_type() == MYSQL_TYPE_STRING &&
	     length >= MIN_STRING_LENGTH_TO_PACK_ROWS)
      recinfo->type=FIELD_SKIP_ENDSPACE;
    else if (use_packed_rows && 
             field->real_type() == MYSQL_TYPE_VARCHAR &&
             length >= MIN_STRING_LENGTH_TO_PACK_ROWS)
      recinfo->type= FIELD_VARCHAR;
    else
      recinfo->type=FIELD_NORMAL;
    if (!--hidden_field_count)
      null_count=(null_count+7) & ~7;		// move to next byte

    // fix table name in field entry
    field->table_name= &table->alias;
  }

  param->copy_field_end=copy;
  param->recinfo=recinfo;
  store_record(table,s->default_values);        // Make empty default record

  if (thd->variables.tmp_table_size == ~ (ulonglong) 0)		// No limit
    share->max_rows= ~(ha_rows) 0;
  else
    share->max_rows= (ha_rows) (((share->db_type() == heap_hton) ?
                                 min(thd->variables.tmp_table_size,
                                     thd->variables.max_heap_table_size) :
                                 thd->variables.tmp_table_size) /
			         share->reclength);
  set_if_bigger(share->max_rows,1);		// For dummy start options
  /*
    Push the LIMIT clause to the temporary table creation, so that we
    materialize only up to 'rows_limit' records instead of all result records.
  */
  set_if_smaller(share->max_rows, rows_limit);
  param->end_write_records= rows_limit;

  if (group && !using_unique_constraint)
  {
    ORDER *cur_group= group;
    key_part_info= keyinfo->key_part;
    if (param->can_use_pk_for_unique)
      share->primary_key= 0;
    keyinfo->key_length= 0;  // Will compute the sum of the parts below.
    /*
      Here, we have to make the group fields point to the right record
      position.
    */
    for (; cur_group ; cur_group= cur_group->next, key_part_info++)
    {
      Field *field= (*cur_group->item)->get_tmp_table_field();
      DBUG_ASSERT(field->table == table);
      bool maybe_null= (*cur_group->item)->maybe_null;
      key_part_info->init_from_field(key_part_info->field);
      keyinfo->key_length+= key_part_info->store_length;

      cur_group->buff= (char*) group_buff;
      cur_group->field= field->new_key_field(thd->mem_root, table,
                                             group_buff + MY_TEST(maybe_null));

      if (!cur_group->field)
        goto err; /* purecov: inspected */

      if (maybe_null)
      {
        /*
          To be able to group on NULL, we reserved place in group_buff
          for the NULL flag just before the column. (see above).
          The field data is after this flag.
          The NULL flag is updated in 'end_update()' and 'end_write()'
        */
        keyinfo->flags|= HA_NULL_ARE_EQUAL;	// def. that NULL == NULL
        cur_group->buff++;                        // Pointer to field data
        group_buff++;                         // Skipp null flag
      }
      group_buff+= cur_group->field->pack_length();
    }
  }

  if (distinct && field_count != param->hidden_field_count &&
      !using_unique_constraint)
  {
    null_pack_length-=hidden_null_pack_length;
    key_part_info= keyinfo->key_part;
    if (param->can_use_pk_for_unique)
      share->primary_key= 0;
    keyinfo->key_length= 0;  // Will compute the sum of the parts below.
    /*
      Here, we have to make the key fields point to the right record
      position.
    */
    for (i=param->hidden_field_count, reg_field=table->field + i ;
         i < field_count;
         i++, reg_field++, key_part_info++)
    {
      key_part_info->init_from_field(*reg_field);
      keyinfo->key_length+= key_part_info->store_length;
    }
  }

  // Create a key over hash_field to enforce unique constraint
  if (using_unique_constraint)
  {
    KEY *hash_key;
    KEY_PART_INFO *hash_kpi;

    if (!multi_alloc_root(&table->mem_root,
                          &hash_key, sizeof(*hash_key),
                          &hash_kpi, sizeof(*hash_kpi), // Only one key part
                          NullS))
      goto err;
    table->key_info= share->key_info= hash_key;
    hash_key->table= table;
    hash_key->key_part= hash_kpi;
    hash_key->actual_flags= hash_key->flags= HA_NULL_ARE_EQUAL;
    hash_key->actual_key_parts= hash_key->usable_key_parts= 1;
    hash_key->user_defined_key_parts= 1;
    hash_key->set_rec_per_key_array(NULL, NULL);
    hash_key->set_in_memory_estimate(IN_MEMORY_ESTIMATE_UNKNOWN);
    hash_key->algorithm= HA_KEY_ALG_UNDEF;
    if (distinct)
      hash_key->name= (char*) "<hash_distinct_key>";
    else
      hash_key->name= (char*) "<hash_group_key>";
    hash_kpi->init_from_field(table->hash_field);
    hash_key->key_length= hash_kpi->store_length;
    param->keyinfo= hash_key;
  }

  if (thd->is_fatal_error)				// If end of memory
    goto err;					 /* purecov: inspected */
  share->db_record_offset= 1;
  if (!param->skip_create_table)
  {
    if (instantiate_tmp_table(table, param->keyinfo, param->start_recinfo,
                              &param->recinfo, select_options,
                              thd->variables.big_tables, &thd->opt_trace))
      goto err;
  }

  thd->mem_root= mem_root_save;

  DEBUG_SYNC(thd, "tmp_table_created");

  DBUG_RETURN(table);

err:
  thd->mem_root= mem_root_save;
  free_tmp_table(thd,table);                    /* purecov: inspected */
  DBUG_RETURN(NULL);				/* purecov: inspected */
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

TABLE *create_duplicate_weedout_tmp_table(THD *thd, 
                                          uint uniq_tuple_length_arg,
                                          SJ_TMP_TABLE *sjtbl)
{
  MEM_ROOT *mem_root_save, own_root;
  TABLE *table;
  TABLE_SHARE *share;
  uint  temp_pool_slot=MY_BIT_NONE;
  char	*tmpname,path[FN_REFLEN];
  Field **reg_field;
  KEY_PART_INFO *key_part_info;
  KEY *keyinfo;
  uchar *group_buff;
  uchar *bitmaps;
  uint *blob_field;
  MI_COLUMNDEF *recinfo, *start_recinfo;
  bool using_unique_constraint=false;
  Field *field, *key_field;
  uint null_pack_length;
  uchar *null_flags;
  uchar	*pos;
  uint i;

  DBUG_ENTER("create_duplicate_weedout_tmp_table");
  DBUG_ASSERT(!sjtbl->is_confluent);
  /*
    STEP 1: Get temporary table name
  */
  thd->inc_status_created_tmp_tables();
  if (use_temp_pool && !(test_flags & TEST_KEEP_TMP_TABLES))
    temp_pool_slot = bitmap_lock_set_next(&temp_pool);

  if (temp_pool_slot != MY_BIT_NONE) // we got a slot
    sprintf(path, "%s_%lx_%i", tmp_file_prefix,
	    current_pid, temp_pool_slot);
  else
  {
    /* if we run out of slots or we are not using tempool */
    DBUG_ASSERT(sizeof(my_thread_id) == 4);
    sprintf(path,"%s%lx_%x_%x", tmp_file_prefix,current_pid,
            thd->thread_id(), thd->tmp_table++);
  }
  fn_format(path, path, mysql_tmpdir, "", MY_REPLACE_EXT|MY_UNPACK_FILENAME);

  /* STEP 2: Figure if we'll be using a key or blob+constraint */
  if (uniq_tuple_length_arg > CONVERT_IF_BIGGER_TO_BLOB)
    using_unique_constraint= true;

  /* STEP 3: Allocate memory for temptable description */
  init_sql_alloc(key_memory_TABLE, &own_root, TABLE_ALLOC_BLOCK_SIZE, 0);
  if (!multi_alloc_root(&own_root,
                        &table, sizeof(*table),
                        &share, sizeof(*share),
                        &reg_field, sizeof(Field*) * (1+2),
                        &blob_field, sizeof(uint)*3,
                        &keyinfo, sizeof(*keyinfo),
                        &key_part_info, sizeof(*key_part_info) * 2,
                        &start_recinfo,
                        sizeof(*recinfo)*(1*2+2),
                        &tmpname, strlen(path)+1,
                        &group_buff, (!using_unique_constraint ?
                                      uniq_tuple_length_arg : 0),
                        &bitmaps, bitmap_buffer_size(1) * 3,
                        NullS))
  {
    if (temp_pool_slot != MY_BIT_NONE)
      bitmap_lock_clear_bit(&temp_pool, temp_pool_slot);
    DBUG_RETURN(NULL);
  }
  my_stpcpy(tmpname,path);

  /* STEP 4: Create TABLE description */
  new (table) TABLE;
  memset(reg_field, 0, sizeof(Field*) * 3);

  table->mem_root= own_root;
  mem_root_save= thd->mem_root;
  thd->mem_root= &table->mem_root;

  table->field=reg_field;
  table->alias= "weedout-tmp";
  table->reginfo.lock_type=TL_WRITE;	/* Will be updated */
  table->db_stat=HA_OPEN_KEYFILE+HA_OPEN_RNDFILE;
  table->temp_pool_slot = temp_pool_slot;
  table->copy_blobs= 1;
  table->in_use= thd;
  table->quick_keys.init();
  table->possible_quick_keys.init();
  table->covering_keys.init();
  table->keys_in_use_for_query.init();

  table->s= share;
  init_tmp_table_share(thd, share, "", 0, tmpname, tmpname);
  share->blob_field= blob_field;
  share->db_low_byte_first=1;                // True for HEAP and MyISAM
  share->table_charset= NULL;
  share->primary_key= MAX_KEY;               // Indicate no primary key
  share->keys_for_keyread.init();
  share->keys_in_use.init();

  uint reclength= 0;
  uint null_count= 0;

  /* Create the field */
  if (using_unique_constraint)
  {
    Field_longlong *field= new(&table->mem_root)
          Field_longlong(sizeof(ulonglong), false, "<hash_field>", true);
    if (!field)
    {
      DBUG_ASSERT(thd->is_fatal_error);
      goto err;				// Got OOM
    }
    // Mark hash_field as NOT NULL
    field->flags &= NOT_NULL_FLAG;
    *(reg_field++)= sjtbl->hash_field= field;
    table->hash_field= field;
    field->table= field->orig_table= table;
    share->fields++;
    field->field_index= 0;
    reclength= field->pack_length();
    table->hidden_field_count++;
  }
  {
    /*
      For the sake of uniformity, always use Field_varstring (altough we could
      use Field_string for shorter keys)
    */
    field= new Field_varstring(uniq_tuple_length_arg, FALSE, "rowids", share,
                               &my_charset_bin);
    if (!field)
      DBUG_RETURN(0);
    field->table= table;
    field->unireg_check= Field::NONE;
    field->flags= (NOT_NULL_FLAG | BINARY_FLAG | NO_DEFAULT_VALUE_FLAG);
    field->reset_fields();
    field->init(table);
    field->orig_table= NULL;
    *(reg_field++)= field;
    *blob_field= 0;
    *reg_field= 0;

    field->field_index= share->fields;
    share->fields++;
    share->blob_fields= 0;
    reclength+= field->pack_length();
    null_count++;
  }

  if (using_unique_constraint)
  {
    switch (internal_tmp_disk_storage_engine)
    {
    case TMP_TABLE_MYISAM:
      share->db_plugin= ha_lock_engine(0, myisam_hton);
      break;
    case TMP_TABLE_INNODB:
      share->db_plugin= ha_lock_engine(0, innodb_hton);
      break;
    default:
      DBUG_ASSERT(0);
      share->db_plugin= ha_lock_engine(0, innodb_hton);
    }
    table->file= get_new_handler(share, &table->mem_root,
                                 share->db_type());
  }
  else
  {
    share->db_plugin= ha_lock_engine(0, heap_hton);
    table->file= get_new_handler(share, &table->mem_root,
                                 share->db_type());
  }


  if (!table->file)
    goto err;

  if (table->file->set_ha_share_ref(&share->ha_share))
  {
    delete table->file;
    goto err;
  }

  null_pack_length= 1;
  reclength+= null_pack_length;

  share->reclength= reclength;
  {
    uint alloc_length= ALIGN_SIZE(share->reclength + MI_UNIQUE_HASH_LENGTH+1);
    share->rec_buff_length= alloc_length;
    if (!(table->record[0]= (uchar*)
                            alloc_root(&table->mem_root, alloc_length * 3)))
      goto err;
    table->record[1]= table->record[0] + alloc_length;
    share->default_values= table->record[1] + alloc_length;
  }
  setup_tmp_table_column_bitmaps(table, bitmaps);

  recinfo= start_recinfo;
  null_flags= table->record[0];

  pos= table->record[0] + null_pack_length;
  if (null_pack_length)
  {
    memset(recinfo, 0, sizeof(*recinfo));
    recinfo->type= FIELD_NORMAL;
    recinfo->length= null_pack_length;
    recinfo++;
    memset(null_flags, 255, null_pack_length);	// Set null fields

    table->null_flags= table->record[0];
    share->null_fields= null_count;
    share->null_bytes= null_pack_length;
  }
  null_count=1;
  for (i=0, reg_field=table->field; i < share->fields;
       i++, reg_field++, recinfo++)
  {
    Field *field= *reg_field;
    uint length;
    /* Table description for the concatenated rowid column */
    memset(recinfo, 0, sizeof(*recinfo));

    if (!(field->flags & NOT_NULL_FLAG))
    {
      if (field->flags & GROUP_FLAG && !using_unique_constraint)
      {
        /*
          We have to reserve one byte here for NULL bits,
          as this is updated by 'end_update()'
        */
        *pos++= 0;				// Null is stored here
        recinfo->length= 1;
        recinfo->type= FIELD_NORMAL;
        recinfo++;
        memset(recinfo, 0, sizeof(*recinfo));
      }
      else
      {
        recinfo->null_bit= (uint8)1 << (null_count & 7);
        recinfo->null_pos= null_count/8;
      }
      field->move_field(pos,null_flags+null_count/8,
                        (uint8)1 << (null_count & 7));
      null_count++;
    }
    else
      field->move_field(pos,(uchar*) 0, 0);
    if (field->type() == MYSQL_TYPE_BIT)
    {
      /* We have to reserve place for extra bits among null bits */
      ((Field_bit*) field)->set_bit_ptr(null_flags + null_count / 8,
                                        null_count & 7);
      null_count+= (field->field_length & 7);
    }
    field->reset();

    length= field->pack_length();
    pos+= length;

    /* 
      Don't care about packing the VARCHAR since it's only a
      concatenation of rowids. @see create_tmp_table() for how
      packed VARCHARs can be achieved
    */
    recinfo->length= length;
    recinfo->type= FIELD_NORMAL;

    // fix table name in field entry
    field->table_name= &table->alias;
  }

  if (thd->variables.tmp_table_size == ~ (ulonglong) 0)		// No limit
    share->max_rows= ~(ha_rows) 0;
  else
    share->max_rows= (ha_rows) (((share->db_type() == heap_hton) ?
                                 min(thd->variables.tmp_table_size,
                                     thd->variables.max_heap_table_size) :
                                 thd->variables.tmp_table_size) /
			         share->reclength);
  set_if_bigger(share->max_rows,1);		// For dummy start options


  // Create a key over param->hash_field to enforce unique constraint
  if (using_unique_constraint)
  {
    KEY *hash_key= keyinfo;
    KEY_PART_INFO *hash_kpi= key_part_info;

    share->keys= 1;
    table->key_info= share->key_info= hash_key;
    hash_key->table= table;
    hash_key->key_part= hash_kpi;
    hash_key->actual_flags= hash_key->flags= HA_NULL_ARE_EQUAL;
    hash_kpi->init_from_field(sjtbl->hash_field);
    hash_key->key_length= hash_kpi->store_length;
  }
  else
  {
    DBUG_PRINT("info",("Creating group key in temporary table"));
    share->keys=1;
    table->key_info= table->s->key_info= keyinfo;
    keyinfo->key_part=key_part_info;
    keyinfo->actual_flags= keyinfo->flags= HA_NOSAME;
    keyinfo->key_length=0;
    {
      key_part_info->init_from_field(field);
      DBUG_ASSERT(key_part_info->key_type == FIELDFLAG_BINARY);

      key_field= field->new_key_field(thd->mem_root, table, group_buff);
      if (!key_field)
        goto err;
      key_part_info->key_part_flag|= HA_END_SPACE_ARE_EQUAL; //todo need this?
      keyinfo->key_length+=  key_part_info->length;
    }
  }
  {
    table->key_info->user_defined_key_parts= 1;
    table->key_info->usable_key_parts= 1;
    table->key_info->actual_key_parts= table->key_info->user_defined_key_parts;
    table->key_info->set_rec_per_key_array(NULL, NULL);
    table->key_info->set_in_memory_estimate(IN_MEMORY_ESTIMATE_UNKNOWN);
    table->key_info->algorithm= HA_KEY_ALG_UNDEF;
    table->key_info->name= (char*) "weedout_key";
  }

  if (thd->is_fatal_error)				// If end of memory
    goto err;
  share->db_record_offset= 1;
  if (instantiate_tmp_table(table, table->key_info, start_recinfo, &recinfo,
                            0, 0, &thd->opt_trace))
    goto err;

  sjtbl->start_recinfo= start_recinfo;
  sjtbl->recinfo=       recinfo;

  thd->mem_root= mem_root_save;
  DBUG_RETURN(table);

err:
  thd->mem_root= mem_root_save;
  table->file->ha_index_or_rnd_end();
  free_tmp_table(thd,table);                    /* purecov: inspected */
  DBUG_RETURN(NULL);				/* purecov: inspected */
}


/****************************************************************************/

/**
  Create a reduced TABLE object with properly set up Field list from a
  list of field definitions.

    The created table doesn't have a table handler associated with
    it, has no keys, no group/distinct, no copy_funcs array.
    The sole purpose of this TABLE object is to use the power of Field
    class to read/write data to/from table->record[0]. Then one can store
    the record in any container (RB tree, hash, etc).
    The table is created in THD mem_root, so are the table's fields.
    Consequently, if you don't BLOB fields, you don't need to free it.

  @param thd         connection handle
  @param field_list  list of column definitions

  @return
    0 if out of memory, TABLE object in case of success
*/

TABLE *create_virtual_tmp_table(THD *thd, List<Create_field> &field_list)
{
  uint field_count= field_list.elements;
  uint blob_count= 0;
  Field **field;
  Create_field *cdef;                           /* column definition */
  uint record_length= 0;
  uint null_count= 0;                 /* number of columns which may be null */
  uint null_pack_length;              /* NULL representation array length */
  uint *blob_field;
  uchar *bitmaps;
  TABLE *table;
  TABLE_SHARE *share;

  if (!multi_alloc_root(thd->mem_root,
                        &table, sizeof(*table),
                        &share, sizeof(*share),
                        &field, (field_count + 1) * sizeof(Field*),
                        &blob_field, (field_count+1) *sizeof(uint),
                        &bitmaps, bitmap_buffer_size(field_count) * 3,
                        NullS))
    return 0;

  new (table) TABLE;
  new (share) TABLE_SHARE;
  table->field= field;
  table->s= share;
  table->temp_pool_slot= MY_BIT_NONE;
  share->blob_field= blob_field;
  share->fields= field_count;
  share->db_low_byte_first=1;                // True for HEAP and MyISAM
  setup_tmp_table_column_bitmaps(table, bitmaps);

  /* Create all fields and calculate the total length of record */
  List_iterator_fast<Create_field> it(field_list);
  while ((cdef= it++))
  {
    *field= make_field(share, 0, cdef->length,
                       (uchar*) (f_maybe_null(cdef->pack_flag) ? "" : 0),
                       f_maybe_null(cdef->pack_flag) ? 1 : 0,
                       cdef->pack_flag, cdef->sql_type, cdef->charset,
                       cdef->geom_type, cdef->unireg_check,
                       cdef->interval, cdef->field_name);
    if (!*field)
      goto error;
    (*field)->init(table);
    record_length+= (*field)->pack_length();
    if (! ((*field)->flags & NOT_NULL_FLAG))
      null_count++;

    if ((*field)->flags & BLOB_FLAG)
      share->blob_field[blob_count++]= (uint) (field - table->field);

    field++;
  }
  *field= NULL;                             /* mark the end of the list */
  share->blob_field[blob_count]= 0;            /* mark the end of the list */
  share->blob_fields= blob_count;

  null_pack_length= (null_count + 7)/8;
  share->reclength= record_length + null_pack_length;
  share->rec_buff_length= ALIGN_SIZE(share->reclength + 1);
  table->record[0]= (uchar*) thd->alloc(share->rec_buff_length);
  if (!table->record[0])
    goto error;

  if (null_pack_length)
  {
    table->null_flags= table->record[0];
    share->null_fields= null_count;
    share->null_bytes= null_pack_length;
  }

  table->in_use= thd;           /* field->reset() may access table->in_use */
  {
    /* Set up field pointers */
    uchar *null_pos= table->record[0];
    uchar *field_pos= null_pos + share->null_bytes;
    uint null_bit= 1;

    for (field= table->field; *field; ++field)
    {
      Field *cur_field= *field;
      if ((cur_field->flags & NOT_NULL_FLAG))
        cur_field->move_field(field_pos);
      else
      {
        cur_field->move_field(field_pos, null_pos, null_bit);
        null_bit<<= 1;
        if (null_bit == (uint8)1 << 8)
        {
          ++null_pos;
          null_bit= 1;
        }
      }
      if (cur_field->type() == MYSQL_TYPE_BIT &&
          cur_field->key_type() == HA_KEYTYPE_BIT)
      {
        /* This is a Field_bit since key_type is HA_KEYTYPE_BIT */
        static_cast<Field_bit*>(cur_field)->set_bit_ptr(null_pos, null_bit);
        null_bit+= cur_field->field_length & 7;
        if (null_bit > 7)
        {
          null_pos++;
          null_bit-= 8;
        }
      }
      cur_field->reset();

      field_pos+= cur_field->pack_length();
    }
  }
  return table;
error:
  for (field= table->field; *field; ++field)
    delete *field;                         /* just invokes field destructor */
  return 0;
}


bool open_tmp_table(TABLE *table)
{
  int error;
  if ((error=table->file->ha_open(table, table->s->table_name.str,O_RDWR,
                                  HA_OPEN_TMP_TABLE | HA_OPEN_INTERNAL_TABLE)))
  {
    table->file->print_error(error,MYF(0)); /* purecov: inspected */
    table->db_stat=0;
    return(1);
  }
  (void) table->file->extra(HA_EXTRA_QUICK);		/* Faster */

  table->set_created();

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
     FALSE - OK
     TRUE  - Error
*/

bool create_myisam_tmp_table(TABLE *table, KEY *keyinfo, 
                             MI_COLUMNDEF *start_recinfo,
                             MI_COLUMNDEF **recinfo, 
                             ulonglong options, my_bool big_tables)
{
  int error;
  MI_KEYDEF keydef;
  MI_UNIQUEDEF uniquedef;
  TABLE_SHARE *share= table->s;
  DBUG_ENTER("create_myisam_tmp_table");

  if (share->keys)
  {						// Get keys for ni_create
    if (share->keys > 1)
    {
      DBUG_ASSERT(0); // This code can't handle more than 1 key
      share->keys= 1;
    }
    HA_KEYSEG *seg= (HA_KEYSEG*) alloc_root(&table->mem_root,
                                            sizeof(*seg) *
                                            keyinfo->user_defined_key_parts);
    if (!seg)
      goto err;

    memset(seg, 0, sizeof(*seg) * keyinfo->user_defined_key_parts);

    /* Create an unique key */
    memset(&keydef, 0, sizeof(keydef));
    keydef.flag= static_cast<uint16>(keyinfo->flags);
    keydef.keysegs=  keyinfo->user_defined_key_parts;
    keydef.seg= seg;

    for (uint i=0; i < keyinfo->user_defined_key_parts ; i++,seg++)
    {
      Field *field=keyinfo->key_part[i].field;
      seg->flag=     0;
      seg->language= field->charset()->number;
      seg->length=   keyinfo->key_part[i].length;
      seg->start=    keyinfo->key_part[i].offset;
      if (field->flags & BLOB_FLAG)
      {
	seg->type=
	((keyinfo->key_part[i].key_type & FIELDFLAG_BINARY) ?
	 HA_KEYTYPE_VARBINARY2 : HA_KEYTYPE_VARTEXT2);
	seg->bit_start= (uint8)(field->pack_length() -
                                portable_sizeof_char_ptr);
	seg->flag= HA_BLOB_PART;
	seg->length=0;			// Whole blob in unique constraint
      }
      else
      {
	seg->type= keyinfo->key_part[i].type;
        /* Tell handler if it can do suffic space compression */
	if (field->real_type() == MYSQL_TYPE_STRING &&
	    keyinfo->key_part[i].length > 4)
	  seg->flag|= HA_SPACE_PACK;
      }
      if (!(field->flags & NOT_NULL_FLAG))
      {
	seg->null_bit= field->null_bit;
	seg->null_pos= field->null_offset();
      }
    }
  }
  MI_CREATE_INFO create_info;
  memset(&create_info, 0, sizeof(create_info));

  if (big_tables && !(options & SELECT_SMALL_RESULT))
    create_info.data_file_length= ~(ulonglong) 0;

  if ((error=mi_create(share->table_name.str, share->keys, &keydef,
                       (uint) (*recinfo - start_recinfo),
                       start_recinfo,
                       0, &uniquedef,
                       &create_info,
                       HA_CREATE_TMP_TABLE | HA_CREATE_INTERNAL_TABLE |
                       ((share->db_create_options & HA_OPTION_PACK_RECORD) ?
                        HA_PACK_RECORD : 0)
                       )))
  {
    table->file->print_error(error,MYF(0));	/* purecov: inspected */
    /*
      Table name which was allocated from temp-pool is already occupied
      in SE. Probably we hit a bug in server or some problem with system
      configuration. Prevent problem from re-occurring by marking temp-pool
      slot for this name as permanently busy, to do this we only need to set
      TABLE::temp_pool_slot to MY_BIT_NONE in order to avoid freeing it
      in free_tmp_table().
    */
    if (error == EEXIST)
      table->temp_pool_slot= MY_BIT_NONE;

    table->db_stat=0;
    goto err;
  }
  table->in_use->inc_status_created_tmp_disk_tables();
  share->db_record_offset= 1;
  DBUG_RETURN(0);
 err:
  DBUG_RETURN(1);
}

/*
  Create InnoDB temporary table

  SYNOPSIS
    create_innodb_tmp_table()
      table           Table object that describes the table to be created
      keyinfo         Description of the index (there is always one index)

  DESCRIPTION
    Create an InnoDB temporary table according to passed description. It is
    assumed to have one unique index or constraint.

    The passed array or MI_COLUMNDEF structures must have this form:

      1. 1-byte column (afaiu for 'deleted' flag) (note maybe not 1-byte
         when there are many nullable columns)
      2. Table columns
      3. One free MI_COLUMNDEF element (*recinfo points here)

    This function may use the free element to create hash column for unique
    constraint.

   RETURN
     FALSE - OK
     TRUE  - Error
*/
bool create_innodb_tmp_table(TABLE *table, KEY *keyinfo)
{
  TABLE_SHARE *share= table->s;

  DBUG_ENTER("create_innodb_tmp_table");

  HA_CREATE_INFO create_info;

  create_info.db_type= table->s->db_type();
  create_info.row_type= table->s->row_type;
  create_info.options|= HA_LEX_CREATE_TMP_TABLE |
                        HA_LEX_CREATE_INTERNAL_TMP_TABLE;
  /*
    INNODB's fixed length column size is restricted to 1024. Exceeding this can
    result in incorrect behavior.
  */
  if (table->s->db_type() == innodb_hton)
  {
    for (Field **field= table->field; *field; ++field)
    {
      if ((*field)->type() == MYSQL_TYPE_STRING &&
          (*field)->key_length() > 1024)
      {
        my_error(ER_TOO_LONG_KEY, MYF(0), 1024);
        DBUG_RETURN(true);
      }
    }
  }

  int error;
  if ((error= table->file->create(share->table_name.str, table, &create_info)))
  {
    table->file->print_error(error,MYF(0));    /* purecov: inspected */
    /*
      Table name which was allocated from temp-pool is already occupied
      in SE. Probably we hit a bug in server or some problem with system
      configuration. Prevent problem from re-occurring by marking temp-pool
      slot for this name as permanently busy, to do this we only need to set
      TABLE::temp_pool_slot to MY_BIT_NONE in order to avoid freeing it
      in free_tmp_table().

      Note that currently InnoDB never reports an error in this case but
      instead aborts on failed assertion. So the below if-statement is here
      mostly to make code future-proof and consistent with MyISAM case.
   */

   if (error == HA_ERR_FOUND_DUPP_KEY || error == HA_ERR_TABLESPACE_EXISTS ||
       error == HA_ERR_TABLE_EXIST)
     table->temp_pool_slot= MY_BIT_NONE;
    table->db_stat= 0;
    DBUG_RETURN(true);
  }
  else
  {
    table->in_use->inc_status_created_tmp_disk_tables();
    share->db_record_offset= 1;
    DBUG_RETURN(false);
  }
}

static void trace_tmp_table(Opt_trace_context *trace, const TABLE *table)
{
  Opt_trace_object trace_tmp(trace, "tmp_table_info");
  if (strlen(table->alias) != 0)
    trace_tmp.add_utf8_table(table->pos_in_table_list);
  else
    trace_tmp.add_alnum("table", "intermediate_tmp_table");

  trace_tmp.add("row_length",table->s->reclength).
    add("key_length", table->s->key_info ? 
        table->s->key_info->key_length : 0).
    add("unique_constraint", table->hash_field ? true : false);

  if (table->s->db_type() == myisam_hton)
  {
    trace_tmp.add_alnum("location", "disk (MyISAM)");
    if (table->s->db_create_options & HA_OPTION_PACK_RECORD)
      trace_tmp.add_alnum("record_format", "packed");
    else
      trace_tmp.add_alnum("record_format", "fixed");
  }
  else if(table->s->db_type() == innodb_hton)
  {
    trace_tmp.add_alnum("location", "disk (InnoDB)");
    if (table->s->db_create_options & HA_OPTION_PACK_RECORD)
      trace_tmp.add_alnum("record_format", "packed");
    else
      trace_tmp.add_alnum("record_format", "fixed");
  }
  else
  {
    DBUG_ASSERT(table->s->db_type() == heap_hton);
    trace_tmp.add_alnum("location", "memory (heap)").
      add("row_limit_estimate", table->s->max_rows);
  }
}

/**
  @brief
  Instantiates temporary table

  @param  table           Table object that describes the table to be
                          instantiated
  @param  keyinfo         Description of the index (there is always one index)
  @param  start_recinfo   Column descriptions
  @param  recinfo INOUT   End of column descriptions
  @param  options         Option bits
  @param  trace           Optimizer trace to write info to

  @details
    Creates tmp table and opens it.

  @return
     FALSE - OK
     TRUE  - Error
*/

bool instantiate_tmp_table(TABLE *table, KEY *keyinfo, 
                           MI_COLUMNDEF *start_recinfo,
                           MI_COLUMNDEF **recinfo, 
                           ulonglong options, my_bool big_tables,
                           Opt_trace_context *trace)
{
#ifndef DBUG_OFF
  for (uint i= 0; i < table->s->fields; i++)
    DBUG_ASSERT(table->field[i]->gcol_info== NULL && table->field[i]->stored_in_db);
#endif

  if (table->s->db_type() == innodb_hton)
  {
    if (create_innodb_tmp_table(table, keyinfo))
      return TRUE;
    // Make empty record so random data is not written to disk
    empty_record(table);
  }
  else if (table->s->db_type() == myisam_hton)
  {
    if (create_myisam_tmp_table(table, keyinfo, start_recinfo, recinfo,
                                options, big_tables))
      return TRUE;
    // Make empty record so random data is not written to disk
    empty_record(table);
  }

  if (open_tmp_table(table))
  {
    table->file->ha_delete_table(table->s->table_name.str);
    return TRUE;
  }

  if (unlikely(trace->is_started()))
  {
    Opt_trace_object wrapper(trace);
    Opt_trace_object convert(trace, "creating_tmp_table");
    trace_tmp_table(trace, table);
  }
  return FALSE;
}

void
free_tmp_table(THD *thd, TABLE *entry)
{
  MEM_ROOT own_root= entry->mem_root;
  const char *save_proc_info;
  DBUG_ENTER("free_tmp_table");
  DBUG_PRINT("enter",("table: %s",entry->alias));

  save_proc_info=thd->proc_info;
  THD_STAGE_INFO(thd, stage_removing_tmp_table);

  // Release latches since this can take a long time
  ha_release_temporary_latches(thd);

  filesort_free_buffers(entry, true);

  if (entry->is_created())
  {
    if (entry->db_stat)
      entry->file->ha_drop_table(entry->s->table_name.str);
    else
      entry->file->ha_delete_table(entry->s->table_name.str);
    delete entry->file;
    entry->file= NULL;

    entry->set_deleted();
  }
  /* free blobs */
  for (Field **ptr=entry->field ; *ptr ; ptr++)
    (*ptr)->mem_free();
  free_io_cache(entry);

  if (entry->temp_pool_slot != MY_BIT_NONE)
    bitmap_lock_clear_bit(&temp_pool, entry->temp_pool_slot);

  plugin_unlock(0, entry->s->db_plugin);

  free_root(&own_root, MYF(0)); /* the table is allocated in its own root */
  thd_proc_info(thd, save_proc_info);

  DBUG_VOID_RETURN;
}

/**
  If a MEMORY table gets full, create a disk-based table and copy all rows
  to this.

  @param thd             THD reference
  @param table           Table reference
  @param start_recinfo   Engine's column descriptions
  @param recinfo[in,out] End of engine's column descriptions
  @param error           Reason why inserting into MEMORY table failed. 
  @param ignore_last_dup If true, ignore duplicate key error for last
                         inserted key (see detailed description below).
  @param is_duplicate[out] if non-NULL and ignore_last_dup is TRUE,
                         return TRUE if last key was a duplicate,
                         and FALSE otherwise.

  @detail
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

  @note that any index/scan access initialized on the MEMORY table is not
  replicated to the on-disk table - it's the caller's responsibility.
*/

bool create_ondisk_from_heap(THD *thd, TABLE *table,
                             MI_COLUMNDEF *start_recinfo,
                             MI_COLUMNDEF **recinfo, 
			     int error, bool ignore_last_dup,
                             bool *is_duplicate)
{
  TABLE new_table;
  TABLE_SHARE share;
  const char *save_proc_info;
  int write_err;
  DBUG_ENTER("create_ondisk_from_heap");

  if (table->s->db_type() != heap_hton || 
      error != HA_ERR_RECORD_FILE_FULL)
  {
    /*
      We don't want this error to be converted to a warning, e.g. in case of
      INSERT IGNORE ... SELECT.
    */
    table->file->print_error(error, MYF(ME_FATALERROR));
    DBUG_RETURN(1);
  }

  // Release latches since this can take a long time
  ha_release_temporary_latches(thd);

  new_table= *table;
  share= *table->s;
  share.ha_share= NULL;
  new_table.s= &share;
  switch (internal_tmp_disk_storage_engine)
  {
  case TMP_TABLE_MYISAM:
    new_table.s->db_plugin= ha_lock_engine(thd, myisam_hton);
    break;
  case TMP_TABLE_INNODB:
    new_table.s->db_plugin= ha_lock_engine(thd, innodb_hton);
    break;
  default:
    DBUG_ASSERT(0);
    new_table.s->db_plugin= ha_lock_engine(thd, innodb_hton);
  }

  if (!(new_table.file= get_new_handler(&share, &new_table.mem_root,
                                        new_table.s->db_type())))
    DBUG_RETURN(1);				// End of memory
  if (new_table.file->set_ha_share_ref(&share.ha_share))
  {
    delete new_table.file;
    DBUG_RETURN(1);
  }
  save_proc_info=thd->proc_info;
  THD_STAGE_INFO(thd, stage_converting_heap_to_ondisk);

  if (share.db_type() == myisam_hton)
  {
    if (create_myisam_tmp_table(&new_table, table->s->key_info,
                                start_recinfo, recinfo,
                                thd->lex->select_lex->active_options(),
                                thd->variables.big_tables))
      goto err2;
  }
  else if (share.db_type() == innodb_hton)
  {
    if (create_innodb_tmp_table(&new_table, table->s->key_info))
      goto err2;
  }

  if (open_tmp_table(&new_table))
    goto err1;


  if (unlikely(thd->opt_trace.is_started()))
  {
    Opt_trace_context * trace= &thd->opt_trace;
    Opt_trace_object wrapper(trace);
    Opt_trace_object convert(trace, "converting_tmp_table_to_ondisk");
    DBUG_ASSERT(error == HA_ERR_RECORD_FILE_FULL);
    convert.add_alnum("cause", "memory_table_size_exceeded");
    trace_tmp_table(trace, &new_table);
  }

  if (table->file->indexes_are_disabled())
    new_table.file->ha_disable_indexes(HA_KEY_SWITCH_ALL);
  table->file->ha_index_or_rnd_end();
  if ((write_err= table->file->ha_rnd_init(1)))
  {
    table->file->print_error(write_err, MYF(ME_FATALERROR));
    write_err= 0;
    goto err;
  }
  if (table->no_rows)
  {
    new_table.file->extra(HA_EXTRA_NO_ROWS);
    new_table.no_rows=1;
  }

  /* HA_EXTRA_WRITE_CACHE can stay until close, no need to disable it */
  new_table.file->extra(HA_EXTRA_WRITE_CACHE);

  /*
    copy all old rows from heap table to on-disk table
    This is the only code that uses record[1] to read/write but this
    is safe as this is a temporary on-disk table without timestamp/
    autoincrement or partitioning.
  */
  while (!table->file->ha_rnd_next(new_table.record[1]))
  {
    write_err= new_table.file->ha_write_row(new_table.record[1]);
    DBUG_EXECUTE_IF("raise_error", write_err= HA_ERR_FOUND_DUPP_KEY ;);
    if (write_err)
      goto err;
  }
  /* copy row that filled HEAP table */
  if ((write_err=new_table.file->ha_write_row(table->record[0])))
  {
    if (!new_table.file->is_ignorable_error(write_err) ||
	!ignore_last_dup)
      goto err;
    if (is_duplicate)
      *is_duplicate= TRUE;
  }
  else
  {
    if (is_duplicate)
      *is_duplicate= FALSE;
  }

  /* remove heap table and change to use on-disk table */
  (void) table->file->ha_rnd_end();
  (void) table->file->ha_close();              // This deletes the table !
  delete table->file;
  table->file=0;
  plugin_unlock(0, table->s->db_plugin);
  share.db_plugin= my_plugin_lock(0, &share.db_plugin);
  new_table.s= table->s;                       // Keep old share
  *table= new_table;
  *table->s= share;
  /* Update quick select, if any. */
  {
    QEP_TAB *tab= table->reginfo.qep_tab;
    DBUG_ASSERT(tab || !table->reginfo.join_tab);
    if (tab && tab->quick())
    {
      /*
        This could happen only with result of derived table/view
        materialization.
      */
      DBUG_ASSERT(tab->table_ref && tab->table_ref->uses_materialization());
      tab->quick()->set_handler(table->file);
    }
  }
  table->file->change_table_ptr(table, table->s);
  table->use_all_columns();
  if (save_proc_info)
    thd_proc_info(thd, (!strcmp(save_proc_info,"Copying to tmp table") ?
                  "Copying to tmp table on disk" : save_proc_info));
  DBUG_RETURN(0);

 err:
  if (write_err)
  {
    DBUG_PRINT("error",("Got error: %d",write_err));
    new_table.file->print_error(write_err, MYF(0));
  }
  if (table->file->inited)
    (void) table->file->ha_rnd_end();
  (void) new_table.file->ha_close();
 err1:
  new_table.file->ha_delete_table(new_table.s->table_name.str);
 err2:
  delete new_table.file;
  thd_proc_info(thd, save_proc_info);
  table->mem_root= new_table.mem_root;
  DBUG_RETURN(1);
}
