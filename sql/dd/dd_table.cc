/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "dd_table.h"

#include "debug_sync.h"                       // DEBUG_SYNC
#include "default_values.h"                   // max_pack_length
#include "log.h"                              // sql_print_error
#include "partition_info.h"                   // partition_info
#include "psi_memory_key.h"                   // key_memory_frm
#include "sql_class.h"                        // THD
#include "sql_partition.h"                    // expr_to_string
#include "sql_table.h"                        // primary_key_name
#include "transaction.h"                      // trans_commit

#include "dd/dd.h"                            // dd::get_dictionary
#include "dd/dd_schema.h"                     // dd::Schema_MDL_locker
#include "dd/dictionary.h"                    // dd::Dictionary
#include "dd/iterator.h"                      // dd::Iterator
#include "dd/properties.h"                    // dd::Properties
#include "dd/cache/dictionary_client.h"       // dd::cache::Dictionary_client
#include "dd/types/column.h"                  // dd::Column
#include "dd/types/column_type_element.h"     // dd::Column_type_element
#include "dd/types/index.h"                   // dd::Index
#include "dd/types/index_element.h"           // dd::Index_element
#include "dd/types/object_table.h"            // dd::Object_table
#include "dd/types/object_table_definition.h" // dd::Object_table_definition
#include "dd/types/partition.h"               // dd::Partition
#include "dd/types/partition_value.h"         // dd::Partition_value
#include "dd/types/schema.h"                  // dd::Schema
#include "dd/types/table.h"                   // dd::Table
#include "dd/types/tablespace.h"              // dd::Tablespace
#include "dd/types/view.h"                    // dd::View

// TODO: Avoid exposing dd/impl headers in public files.
#include "dd/impl/utils.h"                    // dd::escape

#include <memory>                             // unique_ptr


// Explicit instanciation of some template functions
template bool dd::drop_table<dd::Abstract_table>(THD *thd,
                                                 const char *schema_name,
                                                 const char *name);
template bool dd::drop_table<dd::Table>(THD *thd,
                                        const char *schema_name,
                                        const char *name);
template bool dd::drop_table<dd::View>(THD *thd,
                                       const char *schema_name,
                                       const char *name);

template bool dd::table_exists<dd::Abstract_table>(
                                      dd::cache::Dictionary_client *client,
                                      const char *schema_name,
                                      const char *name,
                                      bool *exists);
template bool dd::table_exists<dd::Table>(
                                      dd::cache::Dictionary_client *client,
                                      const char *schema_name,
                                      const char *name,
                                      bool *exists);
template bool dd::table_exists<dd::View>(
                                      dd::cache::Dictionary_client *client,
                                      const char *schema_name,
                                      const char *name,
                                      bool *exists);

template bool dd::rename_table<dd::Table>(THD *thd,
                                      const char *from_schema_name,
                                      const char *from_name,
                                      const char *to_schema_name,
                                      const char *to_name,
                                      bool no_foreign_key_check);
template bool dd::rename_table<dd::View>(THD *thd,
                                      const char *from_schema_name,
                                      const char *from_name,
                                      const char *to_schema_name,
                                      const char *to_name,
                                      bool no_foreign_key_check);

namespace dd {

/**
  Convert to and from new enum types in DD framework to current MySQL
  server enum types. We have plans to retain both old and new enum
  values in DD tables so as to handle client compatibility and
  information schema requirements.
*/

static dd::Column::enum_column_types dd_get_new_field_type(enum_field_types type)
{
  switch (type)
  {
  case MYSQL_TYPE_DECIMAL:
    return dd::Column::TYPE_DECIMAL;

  case MYSQL_TYPE_TINY:
    return dd::Column::TYPE_TINY;

  case MYSQL_TYPE_SHORT:
    return dd::Column::TYPE_SHORT;

  case MYSQL_TYPE_LONG:
    return dd::Column::TYPE_LONG;

  case MYSQL_TYPE_FLOAT:
    return dd::Column::TYPE_FLOAT;

  case MYSQL_TYPE_DOUBLE:
    return dd::Column::TYPE_DOUBLE;

  case MYSQL_TYPE_NULL:
    return dd::Column::TYPE_NULL;

  case MYSQL_TYPE_TIMESTAMP:
    return dd::Column::TYPE_TIMESTAMP;

  case MYSQL_TYPE_LONGLONG:
    return dd::Column::TYPE_LONGLONG;

  case MYSQL_TYPE_INT24:
    return dd::Column::TYPE_INT24;

  case MYSQL_TYPE_DATE:
    return dd::Column::TYPE_DATE;

  case MYSQL_TYPE_TIME:
    return dd::Column::TYPE_TIME;

  case MYSQL_TYPE_DATETIME:
    return dd::Column::TYPE_DATETIME;

  case MYSQL_TYPE_YEAR:
    return dd::Column::TYPE_YEAR;

  case MYSQL_TYPE_NEWDATE:
    return dd::Column::TYPE_NEWDATE;

  case MYSQL_TYPE_VARCHAR:
    return dd::Column::TYPE_VARCHAR;

  case MYSQL_TYPE_BIT:
    return dd::Column::TYPE_BIT;

  case MYSQL_TYPE_TIMESTAMP2:
    return dd::Column::TYPE_TIMESTAMP2;

  case MYSQL_TYPE_DATETIME2:
    return dd::Column::TYPE_DATETIME2;

  case MYSQL_TYPE_TIME2:
    return dd::Column::TYPE_TIME2;

  case MYSQL_TYPE_NEWDECIMAL:
    return dd::Column::TYPE_NEWDECIMAL;

  case MYSQL_TYPE_ENUM:
    return dd::Column::TYPE_ENUM;

  case MYSQL_TYPE_SET:
    return dd::Column::TYPE_SET;

  case MYSQL_TYPE_TINY_BLOB:
    return dd::Column::TYPE_TINY_BLOB;

  case MYSQL_TYPE_MEDIUM_BLOB:
    return dd::Column::TYPE_MEDIUM_BLOB;

  case MYSQL_TYPE_LONG_BLOB:
    return dd::Column::TYPE_LONG_BLOB;

  case MYSQL_TYPE_BLOB:
    return dd::Column::TYPE_BLOB;

  case MYSQL_TYPE_VAR_STRING:
    return dd::Column::TYPE_VAR_STRING;

  case MYSQL_TYPE_STRING:
    return dd::Column::TYPE_STRING;

  case MYSQL_TYPE_GEOMETRY:
    return dd::Column::TYPE_GEOMETRY;

  case MYSQL_TYPE_JSON:
    return dd::Column::TYPE_JSON;

  }

  /* purecov: begin deadcode */
  sql_print_error("Error: Invalid field type.");
  DBUG_ASSERT(false);

  return dd::Column::TYPE_LONG;
  /* purecov: end */
}


static std::string now_with_opt_decimals(uint decimals)
{
  char buff[17 + 1 + 1 + 1 + 1];
  String val(buff, sizeof(buff), &my_charset_bin);
  val.length(0);
  val.append("CURRENT_TIMESTAMP");
  if (decimals > 0)
    val.append_parenthesized(decimals);
  return std::string(val.ptr(), val.length());
}


/**
  Add column objects to dd::Table according to list of Create_field objects.
*/

static bool
fill_dd_columns_from_create_fields(THD *thd,
                                   dd::Table *tab_obj,
                                   const List<Create_field> &create_fields,
                                   handler *file,
                                   sql_mode_t user_trans_sql_mode)
{
  // Local class to handle the thd context, which must be manipulated since
  // we may be in the middle of a DD transaction, which sets sql mode= 0.
  // The user transaction may have a different sql mode, which must be used
  // here to handle default values correctly, e.g. allowing invalid dates.
  // When this function returns, the old (i.e., DD transaction's)  sql mode
  // must be restored. This class also takes care of freeing the dynamically
  // allocated buffer used to prepare default values.
  class Context_handler
  {
  private:
    THD *m_thd;
    uchar *m_buf;
    enum_check_fields m_count_cuted_fields;
    sql_mode_t m_sql_mode;
  public:
    Context_handler(THD *thd, uchar *buf, sql_mode_t user_trans_sql_mode):
                    m_thd(thd), m_buf(buf),
                    m_count_cuted_fields(m_thd->count_cuted_fields),
                    m_sql_mode(m_thd->variables.sql_mode)
    {
      // Set to warn about wrong default values.
      m_thd->count_cuted_fields= CHECK_FIELD_WARN;
      // Temporarily restore user SQL mode, as we are now in a DD transaction.
      m_thd->variables.sql_mode= user_trans_sql_mode;
    }
    ~Context_handler()
    {
      // Delete buffer and restore context.
      my_free(m_buf);
      m_thd->count_cuted_fields= m_count_cuted_fields;
      m_thd->variables.sql_mode= m_sql_mode;
    }
  };

  // Allocate buffer large enough to hold the largest field. Add one byte
  // of potential null bit and leftover bits.
  size_t bufsize= 1 + max_pack_length(create_fields);

  // When accessing leftover bits in the preamble while preparing default
  // values, the get_rec_buf() function applied will assume the buffer
  // size to be at least two bytes.
  bufsize= std::max<size_t>(2, bufsize);
  uchar *buf= reinterpret_cast<uchar*>(my_malloc(key_memory_frm,
                                                 bufsize, MYF(MY_WME)));

  if (!buf)
    return true; /* purecov: inspected */

  // Use RAII to save old context and restore at function return.
  Context_handler save_and_restore_thd_context(thd, buf, user_trans_sql_mode);

  // We need a fake table and share to generate the default values.
  // We prepare these once, and reuse them for all fields.
  TABLE table;
  TABLE_SHARE share;
  memset(&table, 0, sizeof(table));
  memset(&share, 0, sizeof(share));
  table.s= &share;
  table.in_use= thd;
  table.s->db_low_byte_first= file->low_byte_first();

  //
  // Iterate through all the table columns
  //
  Create_field *field;
  List_iterator<Create_field> it(const_cast
                                   <List<Create_field>&>(create_fields));
  while ((field=it++))
  {
    //
    // Add new DD column
    //

    dd::Column *col_obj= tab_obj->add_column();

    col_obj->set_name(field->field_name);

    col_obj->set_type(dd_get_new_field_type(field->sql_type));

    col_obj->set_char_length(field->length);

    col_obj->set_nullable(field->maybe_null);

    col_obj->set_unsigned(field->is_unsigned);

    col_obj->set_zerofill(field->is_zerofill);

    /*
      AUTO_INCREMENT, DEFAULT/ON UPDATE CURRENT_TIMESTAMP properties are
      stored in Create_field::auto_flags.
    */
    if (field->auto_flags & Field::DEFAULT_NOW)
      col_obj->set_default_option(now_with_opt_decimals(field->decimals));

    if (field->auto_flags & Field::ON_UPDATE_NOW)
      col_obj->set_update_option(now_with_opt_decimals(field->decimals));

    col_obj->set_auto_increment((field->auto_flags & Field::NEXT_NUMBER) != 0);

    // Handle generated columns
    if (field->gcol_info)
    {
      col_obj->set_virtual(!field->stored_in_db);
      sql_mode_t sql_mode= thd->variables.sql_mode;
      thd->variables.sql_mode&= ~MODE_ANSI_QUOTES;

      /*
        It is important to normalize the expression's text into the FRM, to
        make it independent from sql_mode. For example, 'a||b' means 'a OR b'
        or 'CONCAT(a,b)', depending on if PIPES_AS_CONCAT is on. Using
        Item::print(), we get self-sufficient text containing 'OR' or
        'CONCAT'. If sql_mode later changes, it will not affect the column.
       */
      String s;
      // Printing db and table name is useless
      field->gcol_info->expr_item->
        print(&s, enum_query_type(QT_NO_DB | QT_NO_TABLE));

      thd->variables.sql_mode= sql_mode;
      /*
        The new text must have exactly the same lifetime as the old text, it's
        a replacement for it. So the same MEM_ROOT must be used: pass NULL.
      */
      field->gcol_info->dup_expr_str(NULL, s.ptr(), s.length());

      col_obj->set_generation_expression(
                 std::string(field->gcol_info->expr_str.str,
                             field->gcol_info->expr_str.length));

      // Prepare UTF expression for IS.
      String gc_expr(field->gcol_info->expr_str.str,
                     field->gcol_info->expr_str.length,
                     thd->charset());
      String gc_expr_for_IS;
      convert_and_print(&gc_expr, &gc_expr_for_IS, system_charset_info);

      col_obj->set_generation_expression_utf8(
                 std::string(gc_expr_for_IS.ptr(), gc_expr_for_IS.length()));
    }

    if (field->comment.str)
      col_obj->set_comment(std::string(field->comment.str,
                                       field->comment.length));

    // Collation ID
    col_obj->set_collation_id(field->charset->number);

    /*
      Store numeric scale for types relying on this info (old and new decimal
      and floating point types). Also store 0 for integer types to simplify I_S
      implementation.
    */
    switch (field->sql_type) {
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
      /* For these types we show NULL in I_S if scale was not given. */
      if (field->decimals != NOT_FIXED_DEC)
        col_obj->set_numeric_scale(field->decimals);
      else
      {
        DBUG_ASSERT(col_obj->is_numeric_scale_null());
      }
      break;
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_DECIMAL:
      col_obj->set_numeric_scale(field->decimals);
      break;
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONGLONG:
      DBUG_ASSERT(field->decimals == 0);
      col_obj->set_numeric_scale(0);
      break;
    default:
      DBUG_ASSERT(col_obj->is_numeric_scale_null());
      break;
    }

    //
    // Set options
    //

    dd::Properties *col_options= &col_obj->options();


    /*
      Store flag indicating whether BIT type storage optimized or not.
      We need to store this flag in DD to correctly handle the case
      when SE starts supporting optimized BIT storage but still needs
      to handle correctly columns which were created before this change.
    */
    if (field->sql_type == MYSQL_TYPE_BIT)
      col_options->set_bool("treat_bit_as_char", field->treat_bit_as_char);

    // Store geometry sub type
    if (field->sql_type == MYSQL_TYPE_GEOMETRY)
    {
      col_options->set_uint32("geom_type", field->geom_type);
    }

    // Field storage media and column format options
    if (field->field_storage_type() != HA_SM_DEFAULT)
      col_options->set_uint32("storage",
                              static_cast<uint32>(field->field_storage_type()));

    if (field->column_format() != COLUMN_FORMAT_TYPE_DEFAULT)
      col_options->set_uint32("column_format",
                              static_cast<uint32>(field->column_format()));

    //
    // Write intervals
    //
    uint i= 0;
    if (field->interval)
    {
      uchar buff[MAX_FIELD_WIDTH];
      String tmp((char*) buff,sizeof(buff), &my_charset_bin);
      tmp.length(0);

      for (const char **pos=field->interval->type_names ; *pos ; pos++)
      {
        //
        // Create enum/set object
        //
        dd::Column_type_element *elem_obj= NULL;

        DBUG_ASSERT(col_obj->type() == dd::Column::TYPE_SET ||
                    col_obj->type() == dd::Column::TYPE_ENUM);

        if (col_obj->type() == dd::Column::TYPE_SET)
          elem_obj= col_obj->add_set_element();
        else if (col_obj->type() == dd::Column::TYPE_ENUM)
          elem_obj= col_obj->add_enum_element();

        //  Copy type_lengths[i] bytes including '\0'
        //  This helps store typelib names that are of different charsets.
        std::string interval_name(*pos, field->interval->type_lengths[i]);
        elem_obj->set_name(interval_name);

        i++;
      }

    }

    // Reset the buffer and assign the column's default value.
    memset(buf, 0, bufsize);
    if (prepare_default_value(thd, buf, table, *field, col_obj))
      return true;
  }

  return false;
}


static dd::Index::enum_index_algorithm dd_get_new_index_algorithm_type(enum ha_key_alg type)
{
  switch (type)
  {
  case HA_KEY_ALG_SE_SPECIFIC:
    return dd::Index::IA_SE_SPECIFIC;

  case HA_KEY_ALG_BTREE:
    return dd::Index::IA_BTREE;

  case HA_KEY_ALG_RTREE:
    return dd::Index::IA_RTREE;

  case HA_KEY_ALG_HASH:
    return dd::Index::IA_HASH;

  case HA_KEY_ALG_FULLTEXT:
    return dd::Index::IA_FULLTEXT;

  }

  /* purecov: begin deadcode */
  sql_print_error("Error: Invalid index algorithm.");
  DBUG_ASSERT(false);

  return dd::Index::IA_SE_SPECIFIC;
  /* purecov: end */
}


static dd::Index::enum_index_type dd_get_new_index_type(const KEY *key)
{
  if (key->flags & HA_FULLTEXT)
    return dd::Index::IT_FULLTEXT;

  if (key->flags & HA_SPATIAL)
    return dd::Index::IT_SPATIAL;

  if (key->flags & HA_NOSAME)
  {
    /*
      mysql_prepare_create_table() marks PRIMARY KEY by assigning
      KEY::name special value. We rely on this here and in several
      other places in server (e.g. in sort_keys()).
    */
    if (key->name == primary_key_name)
      return dd::Index::IT_PRIMARY;
    else
      return dd::Index::IT_UNIQUE;
  }

  return dd::Index::IT_MULTIPLE;
}


/**
  Add dd::Index_element objects to dd::Index/Table according to
  KEY_PART_INFO array for the index.
*/

static void
fill_dd_index_elements_from_key_parts(const dd::Table *tab_obj,
                                      dd::Index *idx_obj,
                                      uint key_part_count,
                                      const KEY_PART_INFO *key_parts)
{
  //
  // Iterate through all the index element
  //

  const KEY_PART_INFO *key_part= key_parts;
  const KEY_PART_INFO *key_part_end= key_parts + key_part_count;

  for ( ; key_part != key_part_end; ++key_part)
  {
    //
    // Get reference to column object
    //

    const dd::Column *key_col_obj;

    {
      std::unique_ptr<dd::Column_const_iterator> it(tab_obj->user_columns());
      int i= 0;

      while ((key_col_obj= it->next()) != NULL && i < key_part->fieldnr)
        i++;
    }
    DBUG_ASSERT(key_col_obj);

    //
    // Create new index element object
    //

    dd::Index_element *idx_elem=
      idx_obj->add_element(const_cast<dd::Column*>(key_col_obj));

    idx_elem->set_length(key_part->length);

    // Note: Sort order is always 0 in FRM file
    // May be this is part of key_part_flag ? need to study
  }
}


/** Add index objects to dd::Table according to array of KEY structures. */
static void fill_dd_indexes_from_keyinfo(dd::Table *tab_obj,
                                         uint key_count,
                                         const KEY *keyinfo)
{
  //
  // Iterate through all the indexes
  //

  const KEY *key= keyinfo;
  const KEY *end= keyinfo + key_count;

  for (int key_nr= 1; key != end; ++key, ++key_nr)
  {
    //
    // Add new DD index
    //

    dd::Index *idx_obj= tab_obj->add_index();

    idx_obj->set_name(key->name);

    idx_obj->set_algorithm(dd_get_new_index_algorithm_type(key->algorithm));
    idx_obj->set_algorithm_explicit(key->is_algorithm_explicit);

    idx_obj->set_type(dd_get_new_index_type(key));

    idx_obj->set_ordinal_position(key_nr);

    idx_obj->set_generated(key->flags & HA_GENERATED_KEY);

    if (key->comment.str)
      idx_obj->set_comment(std::string(key->comment.str,
                                       key->comment.length));

    idx_obj->set_engine(tab_obj->engine());

    //
    // Set options
    //

    dd::Properties *idx_options= &idx_obj->options();

    /*
      Most of flags in KEY::flags bitmap can be easily calculated from other
      attributes of Index, Index_element or Column objects, so we avoid
      storing this redundant information in DD.

      HA_PACK_KEY and HA_BINARY_PACK_KEY are special in this respect. Even
      though we calculate them on the basis of key part attributes, they,
      unlike other flags, do not reflect immanent property of key or its
      parts, but rather reflect our decision to apply certain optimization
      in the specific case. So it is better to store these flags explicitly
      in DD in order to avoid problems with binary compatibility if we decide
      to change conditions in which optimization is applied in future releases.
    */
    idx_options->set_uint32("flags", (key->flags & (HA_PACK_KEY |
                                                    HA_BINARY_PACK_KEY)));

    if (key->block_size)
      idx_options->set_uint32("block_size", key->block_size);

    if (key->parser_name && key->parser_name->str)
      idx_options->set("parser_name", key->parser_name->str);

    // Add Index elements
    fill_dd_index_elements_from_key_parts(tab_obj,
                                          idx_obj,
                                          key->user_defined_key_parts,
                                          key->key_part);
  }
}


/**
  Set dd::Tablespace object id for dd::Table and dd::Partition
  object during CREATE TABLE.

  @param thd                 - Thread handle.
  @param obj                 - dd::Table or dd::Partition.
  @param hton                - handlerton of table or a partition.
  @param tablespace_name     - Tablespace name to be associated
                               with Table or partition.
  @param is_temporary_table  - Is this temporary table ?

  @return true  - On failure.
  @return false - On success.
*/

template <typename T>
static bool fill_dd_tablespace_id_or_name(THD *thd,
                                          T *obj,
                                          handlerton *hton,
                                          const char *tablespace_name,
                                          bool is_temporary_table)
{
  DBUG_ENTER("fill_dd_tablespace_id_or_name");

  if (!(tablespace_name && strlen(tablespace_name)))
    DBUG_RETURN(false);

  /*
    Tablespace metadata can be stored in new DD for following cases.

    1) For engines NDB and InnoDB

    2) A temporary table cannot be assigned with non-temporary tablespace.
       And meta data of temporary tablespace is not captured by new DD.
       Hence it is not necessary to look up tablespaces for temporary
       tables. We store the tablespace name in 'tablespace' table
       option.

    3) Innodb uses predefined/reserved tablespace names started with
       'innodb_'. New DD does not contain metadata for these tablespaces.
       WL7141 can decide if they are really needed to be visible to server.
       So we just store these name in dd::Table::option so as to support
       old behavior and make SHOW CREATE to display the tablespace name.

    4) Note that we store tablespace name for non-tablespace-capable SEs
       for compatibility reasons.
  */
  const char *innodb_prefix= "innodb_";

  if (hton->alter_tablespace &&
      !is_temporary_table &&
      strncmp(tablespace_name, innodb_prefix, strlen(innodb_prefix)) != 0)
  {
    /*
      Make sure we have at least an IX lock on the tablespace name,
      unless this is a temporary table. For temporary tables, the
      tablespace name is not IX locked. When setting tablespace id
      for dd::Partition, we acquire IX lock here.
    */
    DBUG_ASSERT(thd->mdl_context.owns_equal_or_stronger_lock(
                         MDL_key::TABLESPACE,
                         "", tablespace_name,
                         MDL_INTENTION_EXCLUSIVE));

    // Acquire tablespace.
    dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
    const dd::Tablespace* ts_obj= NULL;
    DEBUG_SYNC(thd, "before_acquire_in_fill_dd_tablespace_id_or_name");
    if (thd->dd_client()->acquire<dd::Tablespace>(tablespace_name, &ts_obj))
    {
      // acquire() always fails with a error being reported.
      DBUG_RETURN(true);
    }

    if (!ts_obj)
    {
      my_error(ER_TABLESPACE_MISSING_WITH_NAME, MYF(0), tablespace_name);
      DBUG_RETURN(true);
    }

    // We found valid tablespace so store the ID with dd::Table now.
    obj->set_tablespace_id(ts_obj->id());
  }
  else
  {
    /*
      Persist the tablespace name for non-ndb/non-innodb engines
      This is the current behavior to retain them. The SHOW CREATE
      is suppose to show the options that are provided in CREATE
      TABLE, even though the tablespaces are not supported by
      the engine.
    */
    dd::Properties *options= &obj->options();
    options->set("tablespace", tablespace_name);
  }

  DBUG_RETURN(false);
}


/**
  Get a string of fields to be stores as partition_expression.

  Must be in sync with set_field_list()!

  @param[in,out] str       String to add the field name list to.
  @param[in]     name_list Field name list.

  @return false on success, else true.
*/

static bool get_field_list_str(std::string &str, List<char> *name_list)
{
  List_iterator<char> it(*name_list);
  const char *name;
  uint i= 0, elements= name_list->elements;
  while ((name= it++))
  {
    str.append(dd::escape(name));
    if (++i < elements)
      str.push_back(FIELD_NAME_SEPARATOR_CHAR);
  }
  DBUG_ASSERT(i == name_list->elements);
  return false;
}


/** Helper function to set partition options. */
static void set_partition_options(partition_element *part_elem,
                                  dd::Properties *part_options)
{
  if (part_elem->part_max_rows)
    part_options->set_uint64("max_rows", part_elem->part_max_rows);
  if (part_elem->part_min_rows)
    part_options->set_uint64("min_rows", part_elem->part_min_rows);
  if (part_elem->data_file_name && part_elem->data_file_name[0])
    part_options->set("data_file_name", part_elem->data_file_name);
  if (part_elem->index_file_name && part_elem->index_file_name[0])
    part_options->set("index_file_name", part_elem->index_file_name);
  if (part_elem->nodegroup_id != UNDEF_NODEGROUP)
    part_options->set_uint32("nodegroup_id", part_elem->nodegroup_id);
}


/** Helper function to add partition column values. */
static bool add_part_col_vals(partition_info *part_info,
                              part_elem_value *list_value,
                              uint list_index,
                              dd::Partition *part_obj,
                              const HA_CREATE_INFO *create_info,
                              const List<Create_field> &create_fields)
{
  uint i;
  List_iterator<char> it(part_info->part_field_list);
  uint num_elements= part_info->part_field_list.elements;

  for (i= 0; i < num_elements; i++)
  {
    dd::Partition_value *val_obj= part_obj->add_value();
    part_column_list_val *col_val= &list_value->col_val_array[i];
    char *field_name= it++;
    val_obj->set_column_num(i);
    val_obj->set_list_num(list_index);
    if (col_val->max_value)
    {
      val_obj->set_max_value(true);
    }
    else if (col_val->null_value)
    {
      val_obj->set_value_null(true);
    }
    else
    {
      //  Store in value in utf8 string format.
      String val_str;
      DBUG_ASSERT(!col_val->item_expression->null_value);
      if (expr_to_string(&val_str,
                         col_val->item_expression,
                         col_val,
                         NULL,
                         field_name,
                         create_info,
                         const_cast<List<Create_field>*>(&create_fields)))
      {
        return true;
      }
      std::string std_str(val_str.ptr(), val_str.length());
      val_obj->set_value_utf8(std_str);
    }
  }
  return false;
}


/**
  Fill in partitioning meta data form create_info
  to the table object.

  @param[in]     thd            Thread handle.
  @param[in,out] tab_obj        Table object where to store the info.
  @param[in]     create_info    Create info.
  @param[in]     create_fields  List of fiels in the new table.
  @param[in]     part_info      Partition info object.

  @return false on success, else true.
*/

static bool fill_dd_partition_from_create_info(THD *thd,
                                               dd::Table *tab_obj,
                                               const HA_CREATE_INFO *create_info,
                                               const List<Create_field> &create_fields,
                                               partition_info *part_info)
{
  // TODO-PARTITION: move into partitioning service, WL#4827
  // TODO-PARTITION: Change partition_info, partition_element, part_column_list_val
  //       and p_elem_val to be more similar with
  //       the DD counterparts to ease conversions!
  if (part_info)
  {
    switch (part_info->part_type) {
    case RANGE_PARTITION:
      if (part_info->column_list)
        tab_obj->set_partition_type(dd::Table::PT_RANGE_COLUMNS);
      else
        tab_obj->set_partition_type(dd::Table::PT_RANGE);
      break;
    case LIST_PARTITION:
      if (part_info->column_list)
        tab_obj->set_partition_type(dd::Table::PT_LIST_COLUMNS);
      else
        tab_obj->set_partition_type(dd::Table::PT_LIST);
      break;
    case HASH_PARTITION:
      if (part_info->list_of_part_fields)
      {
        /* KEY partitioning */
        if (part_info->linear_hash_ind)
        {
          if (part_info->key_algorithm == partition_info::KEY_ALGORITHM_51)
            tab_obj->set_partition_type(dd::Table::PT_LINEAR_KEY_51);
          else
            tab_obj->set_partition_type(dd::Table::PT_LINEAR_KEY_55);
        }
        else
        {
          if (part_info->key_algorithm == partition_info::KEY_ALGORITHM_51)
            tab_obj->set_partition_type(dd::Table::PT_KEY_51);
          else
            tab_obj->set_partition_type(dd::Table::PT_KEY_55);
        }
      }
      else
      {
        if (part_info->linear_hash_ind)
          tab_obj->set_partition_type(dd::Table::PT_LINEAR_HASH);
        else
          tab_obj->set_partition_type(dd::Table::PT_HASH);
      }
      break;
    default:
      DBUG_ASSERT(0); /* purecov: deadcode */
    }

    if (part_info->is_auto_partitioned)
    {
      if (tab_obj->partition_type() == dd::Table::PT_KEY_55)
      {
        tab_obj->set_partition_type(dd::Table::PT_AUTO);
      }
      else if (tab_obj->partition_type() == dd::Table::PT_LINEAR_KEY_55)
      {
        tab_obj->set_partition_type(dd::Table::PT_AUTO_LINEAR);
      }
      else
      {
        /*
          Currently only [LINEAR] KEY partitioning is used for auto partitioning.
        */
        DBUG_ASSERT(0); /* purecov: deadcode */
      }
    }

    /* Set partition_expression */
    if (part_info->list_of_part_fields)
    {
      std::string str;
      if (get_field_list_str(str, &part_info->part_field_list))
        return true;
      tab_obj->set_partition_expression(str);
    }
    else
    {
      /* column_list also has list_of_part_fields set! */
      DBUG_ASSERT(!part_info->column_list);
      /* TODO-PARTITION: use part_info->part_expr->print() instead! */
      std::string str(part_info->part_func_string,
                      part_info->part_func_len);
      tab_obj->set_partition_expression(str);
    }

    if (part_info->use_default_partitions)
    {
      if (!part_info->use_default_num_partitions)
        tab_obj->set_default_partitioning(dd::Table::DP_NUMBER);
      else
        tab_obj->set_default_partitioning(dd::Table::DP_YES);
    }
    else
      tab_obj->set_default_partitioning(dd::Table::DP_NO);

    /* Set up subpartitioning. */
    if (part_info->is_sub_partitioned())
    {
      if (part_info->list_of_subpart_fields)
      {
        /* KEY partitioning */
        if (part_info->linear_hash_ind)
        {
          if (part_info->key_algorithm == partition_info::KEY_ALGORITHM_51)
            tab_obj->set_subpartition_type(dd::Table::ST_LINEAR_KEY_51);
          else
            tab_obj->set_subpartition_type(dd::Table::ST_LINEAR_KEY_55);
        }
        else
        {
          if (part_info->key_algorithm == partition_info::KEY_ALGORITHM_51)
            tab_obj->set_subpartition_type(dd::Table::ST_KEY_51);
          else
            tab_obj->set_subpartition_type(dd::Table::ST_KEY_55);
        }
      }
      else
      {
        if (part_info->linear_hash_ind)
          tab_obj->set_subpartition_type(dd::Table::ST_LINEAR_HASH);
        else
          tab_obj->set_subpartition_type(dd::Table::ST_HASH);
      }

      /* Set subpartition_expression */
      if (part_info->list_of_subpart_fields)
      {
        std::string str;
        if (get_field_list_str(str, &part_info->subpart_field_list))
          return true;
        tab_obj->set_subpartition_expression(str);
      }
      else
      {
        /* TODO-PARTITION: use part_info->subpart_expr->print() instead! */
        std::string str(part_info->subpart_func_string,
                        part_info->subpart_func_len);
        tab_obj->set_subpartition_expression(str);
      }
      if (part_info->use_default_subpartitions)
      {
        if (!part_info->use_default_num_subpartitions)
          tab_obj->set_default_subpartitioning(dd::Table::DP_NUMBER);
        else
          tab_obj->set_default_subpartitioning(dd::Table::DP_YES);
      }
      else
        tab_obj->set_default_subpartitioning(dd::Table::DP_NO);
    }

    /* Add partitions and subpartitions. */
    {
      List_iterator<partition_element> part_it(part_info->partitions);
      partition_element *part_elem;
      uint part_num= 0;
      while ((part_elem= part_it++))
      {
        if (part_elem->part_state == PART_TO_BE_DROPPED ||
            part_elem->part_state == PART_REORGED_DROPPED)
        {
          /* These should not be included in the new table definition. */
          continue;
        }

        dd::Partition *part_obj= tab_obj->add_partition();

        part_obj->set_level(0);
        part_obj->set_name(part_elem->partition_name);
        part_obj->set_engine(tab_obj->engine());
        if (part_elem->part_comment)
          part_obj->set_comment(part_elem->part_comment);
        part_obj->set_number(part_num);
        dd::Properties *part_options= &part_obj->options();
        set_partition_options(part_elem, part_options);

        // Set partition tablespace
        if (fill_dd_tablespace_id_or_name<dd::Partition>(
              thd,
              part_obj,
              create_info->db_type,
              part_elem->tablespace_name,
              create_info->options & HA_LEX_CREATE_TMP_TABLE))
          return true;

        /* Fill in partition values if not KEY/HASH. */
        if (part_info->part_type == RANGE_PARTITION)
        {
          if (part_info->column_list)
          {
            List_iterator<part_elem_value> list_it(part_elem->list_val_list);
            part_elem_value *list_value= list_it++;
            if (add_part_col_vals(part_info,
                                  list_value,
                                  0,
                                  part_obj,
                                  create_info,
                                  create_fields))
            {
              return true;
            }
            DBUG_ASSERT(list_it++ == NULL);
          }
          else
          {
            dd::Partition_value *val_obj= part_obj->add_value();
            if (part_elem->max_value)
            {
              val_obj->set_max_value(true);
            }
            else
            {
              if (part_elem->signed_flag)
              {
                val_obj->set_value_utf8(dd::Properties::from_int64(
                                        part_elem->range_value));
              }
              else
              {
                val_obj->set_value_utf8(dd::Properties::from_uint64(
                                        (ulonglong) part_elem->range_value));
              }
            }
          }
        }
        else if (part_info->part_type == LIST_PARTITION)
        {
          uint list_index= 0;
          List_iterator<part_elem_value> list_val_it(part_elem->list_val_list);
          if (part_elem->has_null_value)
          {
            DBUG_ASSERT(!part_info->column_list);
            dd::Partition_value *val_obj= part_obj->add_value();
            val_obj->set_value_null(true);
            val_obj->set_list_num(list_index++);
          }
          part_elem_value *list_value;
          while ((list_value= list_val_it++))
          {
            if (part_info->column_list)
            {
              if (add_part_col_vals(part_info,
                                    list_value,
                                    list_index,
                                    part_obj,
                                    create_info,
                                    create_fields))
              {
                return true;
              }
            }
            else
            {
              dd::Partition_value *val_obj= part_obj->add_value();
              val_obj->set_list_num(list_index);
              if (list_value->unsigned_flag)
              {
                val_obj->set_value_utf8(dd::Properties::from_uint64(
                                        (ulonglong) list_value->value));
              }
              else
              {
                val_obj->set_value_utf8(dd::Properties::from_int64(
                                        list_value->value));
              }
            }
            list_index++;
          }
        }
        else
        {
          // HASH/KEY partition, nothing to fill in?
          DBUG_ASSERT(part_info->part_type == HASH_PARTITION);
        }


        /* Set up all subpartitions in this partition. */
        if (part_info->is_sub_partitioned())
        {
          uint sub_part_num= 0;
          List_iterator<partition_element> sub_it(part_elem->subpartitions);
          partition_element *sub_elem;
          while ((sub_elem= sub_it++))
          {
            dd::Partition *sub_obj= tab_obj->add_partition();
            sub_obj->set_level(1);
            if (sub_elem->part_comment)
              sub_obj->set_comment(sub_elem->part_comment);
            sub_obj->set_name(sub_elem->partition_name);
            sub_obj->set_number(sub_part_num + part_num * part_info->num_subparts);
            dd::Properties *sub_options= &sub_obj->options();
            set_partition_options(sub_elem, sub_options);

            // Set partition tablespace
            if (fill_dd_tablespace_id_or_name<dd::Partition>(
                  thd,
                  sub_obj,
                  create_info->db_type,
                  sub_elem->tablespace_name,
                  create_info->options & HA_LEX_CREATE_TMP_TABLE))
              return true;

            /*
              If table is subpartitioned for each subpartition, index pair
              we need to create Partition_index object.
            */
            std::unique_ptr<dd::Index_iterator> idx_it(tab_obj->indexes());
            dd::Index *idx;

            while ((idx= idx_it->next()) != NULL)
              sub_obj->add_index(idx);

            sub_part_num++;
          }
        }
        else
        {
          /*
            If table is not subpartitioned then Partition_index object is
            required for each partition, index pair.
            */
          std::unique_ptr<dd::Index_iterator> idx_it(tab_obj->indexes());
          dd::Index *idx;

          while ((idx= idx_it->next()) != NULL)
            part_obj->add_index(idx);
        }

        part_num++;
      }
    }
  }
  else
  {
    tab_obj->set_partition_type(dd::Table::PT_NONE);
  }
  return false;
}


/** Fill dd::Table object from mysql_prepare_create_table() output. */
static bool fill_dd_table_from_create_info(THD *thd,
                                           dd::Table *tab_obj,
                                           const std::string &table_name,
                                           const HA_CREATE_INFO *create_info,
                                           const List<Create_field> &create_fields,
                                           const KEY *keyinfo,
                                           uint keys,
                                           handler *file,
                                           sql_mode_t user_trans_sql_mode)
{
  // Table name must be set with the correct case depending on l_c_t_n
  tab_obj->set_name(table_case_name(create_info, table_name.c_str()));

  // TODO-POST-MERGE-TO-TRUNK:
  // Initialize new field tab_obj->last_checked_for_upgrade

  // No need set tab_obj->m_mysql_version_id here. It is always
  // initialized to MYSQL_VERSION_ID by the dd::Abstract_table_impl
  // constructor.

  // Engine
  {
    // Storing real storage engine name in tab_obj.

    handlerton *hton=
      thd->work_part_info ? thd->work_part_info->default_engine_type :
                            create_info->db_type;

    DBUG_ASSERT(hton && ha_storage_engine_is_enabled(hton));

    tab_obj->set_engine(ha_resolve_storage_engine_name(hton));
  }

  // Comments
  if (create_info->comment.str)
    tab_obj->set_comment(std::string(create_info->comment.str,
                                     create_info->comment.length));

  //
  // Set options
  //
  dd::Properties *table_options= &tab_obj->options();

  if (create_info->max_rows)
    table_options->set_uint64("max_rows", create_info->max_rows);

  if (create_info->min_rows)
    table_options->set_uint64("min_rows", create_info->min_rows);

  //
  // Options encoded in HA_CREATE_INFO::table_options.
  //

  /* We should not get any unexpected flags which are not handled below. */
  DBUG_ASSERT(!(create_info->table_options &
                ~(HA_OPTION_PACK_RECORD|
                  HA_OPTION_PACK_KEYS|HA_OPTION_NO_PACK_KEYS|
                  HA_OPTION_CHECKSUM|HA_OPTION_NO_CHECKSUM|
                  HA_OPTION_DELAY_KEY_WRITE|HA_OPTION_NO_DELAY_KEY_WRITE|
                  HA_OPTION_STATS_PERSISTENT|HA_OPTION_NO_STATS_PERSISTENT)));

  /*
    Even though we calculate HA_OPTION_PACK_RECORD flag from the value of
    ROW_FORMAT option and column types, it doesn't really reflect property
    of table, but rather our decision to apply optimization in some cases.
    So it is better to store this flag explicitly in DD in order to avoid
    problems with binary compatibility if we decide to change rules for
    applying this optimization in future releases.
  */
  table_options->set_bool("pack_record",
                          create_info->table_options & HA_OPTION_PACK_RECORD);

  /*
    PACK_KEYS=# clause. Absence of PACK_KEYS option/PACK_KEYS=DEFAULT is
    represented by absence of "pack_keys" property.
  */
  if (create_info->table_options & (HA_OPTION_PACK_KEYS|HA_OPTION_NO_PACK_KEYS))
  {
    DBUG_ASSERT((create_info->table_options &
                 (HA_OPTION_PACK_KEYS|HA_OPTION_NO_PACK_KEYS)) !=
                (HA_OPTION_PACK_KEYS|HA_OPTION_NO_PACK_KEYS));

    table_options->set_bool("pack_keys",
                            create_info->table_options & HA_OPTION_PACK_KEYS);
  }

  /*
    CHECKSUM=# clause. CHECKSUM=DEFAULT doesn't have special meaning and
    is equivalent to CHECKSUM=0.
  */
  DBUG_ASSERT(!((create_info->table_options & HA_OPTION_CHECKSUM) &&
                (create_info->table_options & HA_OPTION_NO_CHECKSUM)));
  table_options->set_bool("checksum",
                          create_info->table_options & HA_OPTION_CHECKSUM);

  /* DELAY_KEY_WRITE=# clause. Same situation as for CHECKSUM option. */
  DBUG_ASSERT(!((create_info->table_options & HA_OPTION_DELAY_KEY_WRITE) &&
                (create_info->table_options & HA_OPTION_NO_DELAY_KEY_WRITE)));
  table_options->set_bool("delay_key_write", create_info->table_options &
                                             HA_OPTION_DELAY_KEY_WRITE);

  /*
    STATS_PERSISTENT=# clause. Absence option in dd::Properties represents
    STATS_PERSIST=DEFAULT value (which means that global server default
    should be used).
  */
  if (create_info->table_options & (HA_OPTION_STATS_PERSISTENT|
                                    HA_OPTION_NO_STATS_PERSISTENT))
  {
    DBUG_ASSERT((create_info->table_options &
                 (HA_OPTION_STATS_PERSISTENT|HA_OPTION_NO_STATS_PERSISTENT)) !=
                (HA_OPTION_STATS_PERSISTENT|HA_OPTION_NO_STATS_PERSISTENT));

    table_options->set_bool("stats_persistent",
                            (create_info->table_options &
                             HA_OPTION_STATS_PERSISTENT));
  }

  //
  // Set other table options.
  //

  table_options->set_uint32("avg_row_length", create_info->avg_row_length);

  table_options->set_uint32("row_type", create_info->row_type);

  table_options->set_uint32("stats_sample_pages",
                     create_info->stats_sample_pages & 0xffff);

  table_options->set_uint32("stats_auto_recalc",
                            create_info->stats_auto_recalc);

  table_options->set_uint32("key_block_size", create_info->key_block_size);

  if (create_info->connect_string.str)
    table_options->set("connection_string", create_info->connect_string.str);

  if (create_info->compress.str)
    table_options->set("compress", create_info->compress.str);

  // Storage media
  if (create_info->storage_media > HA_SM_DEFAULT)
    table_options->set_uint32("storage", create_info->storage_media);

  // Collation ID
  DBUG_ASSERT(create_info->default_table_charset);
  tab_obj->set_collation_id(create_info->default_table_charset->number);

  // TODO-MYSQL_VERSION: We decided not to store MYSQL_VERSION_ID ?
  //
  //       If we are to introduce this version we need to explain when
  //       it can be useful (e.g. informational and for backward
  //       compatibility reasons, to handle rare cases when meaning of
  //       some option values changed like it happened for partitioning
  //       by KEY, to optimize CHECK FOR UPGRADE). Note that in practice
  //       we can't use this version ID as a robust binary format version
  //       number, because our shows that we often must be able to create
  //       tables in old binary format even in newer versions to avoid
  //       expensive table rebuilds by ALTER TABLE.

  // Add field definitions
  if (fill_dd_columns_from_create_fields(thd, tab_obj,
                                         create_fields,
                                         file,
                                         user_trans_sql_mode))
    return true;

  // Add index definitions
  fill_dd_indexes_from_keyinfo(tab_obj, keys, keyinfo);

  // Add tablespace definition.
  if (fill_dd_tablespace_id_or_name<dd::Table>(
                              thd,
                              tab_obj,
                              create_info->db_type,
                              create_info->tablespace,
                              create_info->options & HA_LEX_CREATE_TMP_TABLE))
    return true;

  // Add partition definitions
  return fill_dd_partition_from_create_info(thd,
                                            tab_obj,
                                            create_info,
                                            create_fields,
                                            thd->work_part_info);
}


static bool create_dd_system_table(THD *thd,
                                   const std::string &table_name,
                                   HA_CREATE_INFO *create_info,
                                   const List<Create_field> &create_fields,
                                   const KEY *keyinfo,
                                   uint keys,
                                   handler *file,
                                   const dd::Object_table &dd_table)
{
  // For DD tables: create system schema using special DD operation.
  std::unique_ptr<dd::Schema> sch_obj(dd::create_dd_schema());

  //
  // Create dd::Table object
  //
  std::unique_ptr<dd::Table> tab_obj(sch_obj->create_table());

  if (fill_dd_table_from_create_info(thd, tab_obj.get(), table_name,
                                     create_info, create_fields,
                                     keyinfo, keys, file,
                                     thd->variables.sql_mode))
    return true;

  //
  // Store info in DD tables now, unless this is a DD table
  //

  dd::Object_table_definition &td=
    const_cast<dd::Object_table_definition &> (dd_table.table_definition());

  // For DD tables, the dd::Table object is now owned by the table
  // definition object.
  td.meta_data(tab_obj.release());

  // Set correct schema id explicitly.
  td.meta_data()->set_schema_id(sch_obj->id());

  return false;
}


static bool create_dd_user_table(THD *thd,
                                 const std::string &schema_name,
                                 const std::string &table_name,
                                 HA_CREATE_INFO *create_info,
                                 const List<Create_field> &create_fields,
                                 const KEY *keyinfo,
                                 uint keys,
                                 handler *file)
{
  // Save SQL mode, needed temporarily while processing default values.
  // The SQL mode must be saved here because starting the dictionary
  // transaction, which is done below, will reset the SQL mode in the THD,
  // but while preparing the default values, we need to obey the SQL mode
  // of the user transaction, e.g. for correct error handling.
  sql_mode_t user_trans_sql_mode= thd->variables.sql_mode;

  // Verify that this is not a dd table.
  // TODO-WL6394 Refactor.
  dd::Dictionary *dict __attribute__((unused));
  dict= dd::get_dictionary();
  DBUG_ASSERT(dict->get_dd_table(schema_name, table_name) == NULL);

  // Check if the schema exists. We must make sure the schema is released
  // and unlocked in the right order.
  dd::Schema_MDL_locker mdl_locker(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  const dd::Schema *sch_obj= NULL;
  if (mdl_locker.ensure_locked(schema_name.c_str()) ||
      thd->dd_client()->acquire<dd::Schema>(schema_name, &sch_obj))
  {
    // Error is reported by the dictionary subsystem.
    return true;
  }

  if (!sch_obj)
  {
    my_error(ER_BAD_DB_ERROR, MYF(0), schema_name.c_str());
    return true;
  }

  // Create dd::Table object.
  std::unique_ptr<dd::Table> tab_obj(
    const_cast<dd::Schema *>(sch_obj)->create_table());

  if (fill_dd_table_from_create_info(thd, tab_obj.get(), table_name,
                                     create_info, create_fields,
                                     keyinfo, keys, file,
                                     user_trans_sql_mode))
    return true;

  Disable_gtid_state_update_guard disabler(thd);

  // Store info in DD tables.
  if (thd->dd_client()->store(tab_obj.get()))
  {
    trans_rollback_stmt(thd);
    // Full rollback in case we have THD::transaction_rollback_request.
    trans_rollback(thd);
    return true;
  }

  return trans_commit_stmt(thd) ||
         trans_commit(thd);
}


bool create_table(THD *thd,
                  const std::string &schema_name,
                  const std::string &table_name,
                  HA_CREATE_INFO *create_info,
                  const List<Create_field> &create_fields,
                  const KEY *keyinfo,
                  uint keys,
                  handler *file)
{
  dd::Dictionary *dict= dd::get_dictionary();
  const dd::Object_table *dd_table= dict->get_dd_table(schema_name, table_name);

  return dd_table ?
    create_dd_system_table(thd, table_name, create_info, create_fields,
                           keyinfo, keys, file, *dd_table) :
    create_dd_user_table(thd, schema_name, table_name, create_info,
                         create_fields, keyinfo, keys, file);
}


dd::Table *create_tmp_table(THD *thd,
                            const std::string &schema_name,
                            const std::string &table_name,
                            HA_CREATE_INFO *create_info,
                            const List<Create_field> &create_fields,
                            const KEY *keyinfo,
                            uint keys,
                            handler *file)
{
  // Save SQL mode, needed temporarily while processing default values.
  // The SQL mode must be saved here because starting the dictionary
  // transaction, which is done below, will reset the SQL mode in the THD,
  // but while preparing the default values, we need to obey the SQL mode
  // of the user transaction, e.g. for correct error handling.
  sql_mode_t user_trans_sql_mode= thd->variables.sql_mode;

  // Check if the schema exists. We must make sure the schema is released
  // and unlocked in the right order.
  dd::Schema_MDL_locker mdl_locker(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  const dd::Schema *sch_obj= NULL;
  if (mdl_locker.ensure_locked(schema_name.c_str()) ||
      thd->dd_client()->acquire<dd::Schema>(schema_name, &sch_obj))
  {
    // Error is reported by the dictionary subsystem.
    return NULL;
  }

  if (!sch_obj)
  {
    my_error(ER_BAD_DB_ERROR, MYF(0), schema_name.c_str());
    return NULL;
  }

  // Create dd::Table object.
  std::unique_ptr<dd::Table> tab_obj(
    const_cast<dd::Schema *>(sch_obj)->create_table());

  if (fill_dd_table_from_create_info(thd, tab_obj.get(), table_name,
                                     create_info, create_fields,
                                     keyinfo, keys, file,
                                     user_trans_sql_mode))
    return NULL;

  return tab_obj.release();
}


template <typename T>
bool drop_table(THD *thd, const char *schema_name, const char *name)
{
  dd::cache::Dictionary_client *client= thd->dd_client();


  // Verify that the schema exists. We must make sure the schema is released
  // and unlocked in the right order.
  dd::Schema_MDL_locker mdl_locker(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(client);
  const dd::Schema *sch= NULL;
  if (mdl_locker.ensure_locked(schema_name) ||
      client->acquire<dd::Schema>(schema_name, &sch))
  {
    // Error is reported by the dictionary subsystem.
    return true;
  }

  if (!sch)
  {
    my_error(ER_BAD_DB_ERROR, MYF(0), schema_name);
    return true;
  }

  const T *at= NULL;
  if (client->acquire<T>(schema_name, name, &at))
  {
    // Error is reported by the dictionary subsystem.
    return true;
  }

  // A non-existing object is a legitimate scenario.
  if (!at)
    return false;

  Disable_gtid_state_update_guard disabler(thd);

  // Drop the table/view
  if (client->drop(const_cast<T*>(at)))
  {
    trans_rollback_stmt(thd);
    // Full rollback in case we have THD::transaction_rollback_request.
    trans_rollback(thd);
    return true;
  }

  return trans_commit_stmt(thd) ||
         trans_commit(thd);
}


template <typename T>
bool table_exists(dd::cache::Dictionary_client *client,
                  const char *schema_name, const char *name,
                  bool *exists)
{
  DBUG_ENTER("dd::table_exists");
  DBUG_ASSERT(exists);
  // A DD table exists (has been created) if the meta data exists.
  // TODO-WL6394.
  dd::Dictionary *dict= dd::get_dictionary();
  const dd::Object_table *dd_table= dict->get_dd_table(schema_name, name);
  if (dd_table)
  {
    *exists= dd_table->table_definition().meta_data() != NULL;
  }
  else
  {
    dd::cache::Dictionary_client::Auto_releaser releaser(client);
    const T* tab_obj= NULL;
    if (client->acquire<T>(schema_name, name, &tab_obj))
    {
      // Error is reported by the dictionary subsystem.
      DBUG_RETURN(true);
    }
    *exists= (tab_obj != NULL);
  }

  DBUG_RETURN(false);
}


template <typename T>
bool rename_table(THD *thd,
                  const char *from_schema_name,
                  const char *from_table_name,
                  const char *to_schema_name,
                  const char *to_table_name,
                  bool no_foreign_key_check)
{
  // Disable foreign key checks temporarily
  class Disable_foreign_key_check
  {
  public:
    Disable_foreign_key_check(THD *thd, bool check)
      :m_thd(thd)
    {
      if (check &&
          !(m_thd->variables.option_bits & OPTION_NO_FOREIGN_KEY_CHECKS))
        m_thd->variables.option_bits|= OPTION_NO_FOREIGN_KEY_CHECKS;
      else
        m_thd= NULL;
    }
    ~Disable_foreign_key_check()
    {
      if (m_thd)
        m_thd->variables.option_bits&= ~OPTION_NO_FOREIGN_KEY_CHECKS;
    }
    THD *m_thd;
  };
  Disable_foreign_key_check dfkc(thd, no_foreign_key_check);

  // We must make sure the schema is released and unlocked in the right order.
  dd::Schema_MDL_locker from_mdl_locker(thd);
  dd::Schema_MDL_locker to_mdl_locker(thd);

  // Check if source and destination schemas exist.
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  const dd::Schema *from_sch= NULL;
  const dd::Schema *to_sch= NULL;
  const T *from_tab= NULL;
  const T *to_tab= NULL;

  // Acquire all objects.
  if (from_mdl_locker.ensure_locked(from_schema_name) ||
      to_mdl_locker.ensure_locked(to_schema_name) ||
      thd->dd_client()->acquire<dd::Schema>(from_schema_name, &from_sch) ||
      thd->dd_client()->acquire<dd::Schema>(to_schema_name, &to_sch) ||
      thd->dd_client()->acquire<T>(to_schema_name, to_table_name, &to_tab) ||
      thd->dd_client()->acquire<T>(from_schema_name, from_table_name,
                                   &from_tab))
  {
    // Error is reported by the dictionary subsystem.
    return true;
  }

  // Report error if missing objects. Missing 'to_tab' is not an error.
  if (!from_sch)
  {
    my_error(ER_BAD_DB_ERROR, MYF(0), from_schema_name);
    return true;
  }

  if (!to_sch)
  {
    my_error(ER_BAD_DB_ERROR, MYF(0), to_schema_name);
    return true;
  }

  if (!from_tab)
  {
    my_error(ER_NO_SUCH_TABLE, MYF(0), from_schema_name, from_table_name);
    return true;
  }

  Disable_gtid_state_update_guard disabler(thd);

  // If 'to_tab' exists (which it may not), get the 'to' table stickiness.
  // Set it unsticky to allow dropping it.
  bool is_sticky= false;
  if (to_tab)
  {
    is_sticky= thd->dd_client()->is_sticky(to_tab);
    if (is_sticky)
      thd->dd_client()->set_sticky(to_tab, false);
    if (thd->dd_client()->drop(const_cast<T*>(to_tab)))
    {
      // Error is reported by the dictionary subsystem.
      trans_rollback_stmt(thd);
      // Full rollback in case we have THD::transaction_rollback_request.
      trans_rollback(thd);
      return true;
    }
  }

  // Set schema id and table name.
  const_cast<T*>(from_tab)->set_schema_id(to_sch->id());
  const_cast<T*>(from_tab)->set_name(to_table_name);

  // Preserve stickiness by setting 'from_tab' to sticky if 'to_tab'
  // was sticky, and update the changes.
  if (is_sticky)
    thd->dd_client()->set_sticky(from_tab, true);

  // Do the update. Errors will be reported by the dictionary subsystem.
  if (thd->dd_client()->update(const_cast<T*>(from_tab)))
  {
    trans_rollback_stmt(thd);
    // Full rollback in case we have THD::transaction_rollback_request.
    trans_rollback(thd);
    return true;
  }

  return trans_commit_stmt(thd) ||
         trans_commit(thd);
}


bool abstract_table_type(dd::cache::Dictionary_client *client,
                         const char *schema_name,
                         const char *table_name,
                         dd::Abstract_table::enum_table_type *table_type)
{
  DBUG_ENTER("dd::abstract_table_type");

  dd::cache::Dictionary_client::Auto_releaser releaser(client);
  // Get hold of the dd::Table object.
  const dd::Abstract_table *table= NULL;
  if (client->acquire<dd::Abstract_table>(schema_name, table_name, &table))
  {
    // Error is reported by the dictionary subsystem.
    DBUG_RETURN(true);
  }

  if (table == NULL)
  {
    my_error(ER_NO_SUCH_TABLE, MYF(0), schema_name, table_name);
    DBUG_RETURN(true);
  }

  // Assign the table type out parameter.
  DBUG_ASSERT(table_type);
  *table_type= table->type();

  DBUG_RETURN(false);
}


// Only used by NDB
/* purecov: begin deadcode */
bool table_legacy_db_type(THD *thd, const char *schema_name,
                          const char *table_name,
                          enum legacy_db_type *db_type)
{
  DBUG_ENTER("dd::table_legacy_db_type");

  // TODO-NOW: Getting DD objects without getting MDL lock on them
  //       is likely to cause problems. We need to revisit
  //       this function at some point.
  // Sivert: Can you please elaborate the problem ?
  // Sivert: Not much to add. Without an MDL lock, we can risk that
  //         the object is modified while we're using it. The global
  //         cache guard does not apply to the new cache (wl#8150).
  // If we are talking about 'problems' point to DD cache issue,
  // probably we can solve now, as we have a DD cache guard
  // introduced already to solve one of similar problem with
  // Innodb.
  // Dlenev: Yes. I guess cache guard can help in this case as a temporary
  // workaround.
  // However long-term we need some better solution. Perhaps this function
  // might turn out unnecessary after discussions with Cluster team.

  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  // Get hold of the dd::Table object.
  const dd::Table *table= NULL;
  if (thd->dd_client()->acquire<dd::Table>(schema_name, table_name, &table))
  {
    // Error is reported by the dictionary subsystem.
    DBUG_RETURN(true);
  }

  if (table == NULL)
  {
    my_error(ER_NO_SUCH_TABLE, MYF(0), schema_name, table_name);
    DBUG_RETURN(true);
  }

  // Get engine by name
  LEX_CSTRING se_name;
  se_name.str= table->engine().c_str();
  se_name.length= table->engine().length();
  plugin_ref tmp_plugin= ha_resolve_by_name_raw(thd, se_name);

  // Return DB_TYPE_UNKNOWN and no error if engine is not loaded.
  *db_type= ha_legacy_type(tmp_plugin ? plugin_data<handlerton*>(tmp_plugin) :
                                        NULL);

  DBUG_RETURN(false);
}
/* purecov: end */


bool table_storage_engine(THD *thd, const TABLE_LIST *table_list,
                          handlerton **hton)
{
  DBUG_ENTER("dd::table_storage_engine");

  // Define pointers to schema- and table name
  DBUG_ASSERT(table_list);
  const char *schema_name= table_list->db;
  const char *table_name= table_list->table_name;

  // There should be at least some lock on the table
  DBUG_ASSERT(thd->mdl_context.owns_equal_or_stronger_lock(MDL_key::TABLE,
                                                           schema_name,
                                                           table_name,
                                                           MDL_SHARED));

  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  // Get hold of the dd::Table object.
  const dd::Table *table= NULL;
  if (thd->dd_client()->acquire<dd::Table>(schema_name, table_name, &table))
  {
    // Error is reported by the dictionary subsystem.
    DBUG_RETURN(true);
  }

  if (table == NULL)
  {
    my_error(ER_NO_SUCH_TABLE, MYF(0), schema_name, table_name);
    DBUG_RETURN(true);
  }

  DBUG_ASSERT(hton);

  // Get engine by name
  plugin_ref tmp_plugin= ha_resolve_by_name_raw(thd, table->engine());
  if (!tmp_plugin)
  {
    my_error(ER_STORAGE_ENGINE_NOT_LOADED, MYF(0), schema_name, table_name);
    DBUG_RETURN(true);
  }

  *hton= plugin_data<handlerton*>(tmp_plugin);
  DBUG_ASSERT(*hton);

  /*
    In DD tables, real storage engine name is stored for the tables.
    So if real storage engine does *not* support partitioning then
    using "ha_partition" storage engine.
  */
  if (table->partition_type() != dd::Table::PT_NONE &&
      !(*hton)->partition_flags)
  {
    tmp_plugin= ha_resolve_by_name_raw(thd, std::string("partition"));
    if (!tmp_plugin)
    {
      /*
        In cases when partitioning SE is disabled we need to produce custom
        error message.
      */
      my_error(ER_FEATURE_NOT_AVAILABLE, MYF(0), "partitioning",
               "--skip-partition", "-DWITH_PARTITION_STORAGE_ENGINE=1");
      DBUG_RETURN(true);
    }
    *hton= plugin_data<handlerton*>(tmp_plugin);
  }
  DBUG_ASSERT(*hton && ha_storage_engine_is_enabled(*hton));

  DBUG_RETURN(false);
}


bool check_storage_engine_flag(THD *thd, const TABLE_LIST *table_list,
                               uint32 flag, bool *yes_no)
{
  DBUG_ASSERT(table_list);

  // Get the handlerton for the table.
  handlerton *hton= NULL;
  if (dd::table_storage_engine(thd, table_list, &hton))
    return true;

  DBUG_ASSERT(yes_no && hton);
  *yes_no= ha_check_storage_engine_flag(hton, flag);

  return false;
}


bool recreate_table(THD *thd, const char *schema_name,
                    const char *table_name)
{
  // There should be an exclusive metadata lock on the table
  DBUG_ASSERT(thd->mdl_context.owns_equal_or_stronger_lock(MDL_key::TABLE,
              schema_name, table_name, MDL_EXCLUSIVE));

  HA_CREATE_INFO create_info;

  // Create a path to the table, but without a extension
  char path[FN_REFLEN + 1];
  build_table_filename(path, sizeof(path) - 1, schema_name, table_name, "", 0);

  // Attempt to reconstruct the table
  return ha_create_table(thd, path, schema_name, table_name, &create_info,
                         true, false, NULL);
}

} // namespace dd
