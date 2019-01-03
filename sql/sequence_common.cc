/* Copyright (c) 2000, 2017, Alibaba and/or its affiliates. All rights reserved.

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
  @file

  Implementation of SEQUENCE shared structure or function.
*/

#include "mysql_com.h"       // NOT NULL
#include "mysqld_error.h"    // ER_SEQUENCE_INVALID
#include "sql/field.h"       // Create_field
#include "sql/handler.h"     // DB_TYPE_SEQUENCE_DB
#include "sql/sql_alter.h"   // Alter_info
#include "sql/sql_plugin.h"  // plugin_unlock
#include "sql/table.h"       // bitmap

#include "sql/sequence_common.h"


/**
  @addtogroup Sequence Engine

  Sequence Engine shared structure or function implementation.

  @{
*/

PSI_memory_key key_memory_sequence_last_value;

st_sequence_field_info seq_fields[] = {
    {"currval",
     "21",
     Sequence_field::FIELD_NUM_CURRVAL,
     MYSQL_TYPE_LONGLONG,
     {C_STRING_WITH_LEN("current value")}},
    {"nextval",
     "21",
     Sequence_field::FIELD_NUM_NEXTVAL,
     MYSQL_TYPE_LONGLONG,
     {C_STRING_WITH_LEN("next value")}},
    {"minvalue",
     "21",
     Sequence_field::FIELD_NUM_MINVALUE,
     MYSQL_TYPE_LONGLONG,
     {C_STRING_WITH_LEN("min value")}},
    {"maxvalue",
     "21",
     Sequence_field::FIELD_NUM_MAXVALUE,
     MYSQL_TYPE_LONGLONG,
     {C_STRING_WITH_LEN("max value")}},
    {"start",
     "21",
     Sequence_field::FIELD_NUM_START,
     MYSQL_TYPE_LONGLONG,
     {C_STRING_WITH_LEN("start value")}},
    {"increment",
     "21",
     Sequence_field::FIELD_NUM_INCREMENT,
     MYSQL_TYPE_LONGLONG,
     {C_STRING_WITH_LEN("increment value")}},
    {"cache",
     "21",
     Sequence_field::FIELD_NUM_CACHE,
     MYSQL_TYPE_LONGLONG,
     {C_STRING_WITH_LEN("cache size")}},
    {"cycle",
     "21",
     Sequence_field::FIELD_NUM_CYCLE,
     MYSQL_TYPE_LONGLONG,
     {C_STRING_WITH_LEN("cycle state")}},
    {"round",
     "21",
     Sequence_field::FIELD_NUM_ROUND,
     MYSQL_TYPE_LONGLONG,
     {C_STRING_WITH_LEN("already how many round")}},
    {NULL,
     NULL,
     Sequence_field::FIELD_NUM_END,
     MYSQL_TYPE_LONGLONG,
     {C_STRING_WITH_LEN("")}}};
/**
   Global variables for sequence engine and sequence base engine, in order to
   get the engine plugin through these engine name.
*/
const LEX_STRING SEQUENCE_ENGINE_NAME = {C_STRING_WITH_LEN("Sequence")};
const LEX_STRING SEQUENCE_BASE_ENGINE_NAME = {C_STRING_WITH_LEN("InnoDB")};

/**
  Resolve the sequence engine plugin.

  @param[in]    thd           user connection

  @retval       plugin_ref    sequence engine plugin.
*/
plugin_ref ha_resolve_sequence(const THD *thd) {
  return ha_resolve_by_name(const_cast<THD *>(thd), &SEQUENCE_ENGINE_NAME,
                            false);
}

/**
  Resolve the sequence base engine plugin.

  @param[in]    thd           user connection

  @retval       plugin_ref    sequence base engine plugin.
*/
plugin_ref ha_resolve_sequence_base(const THD *thd) {
  return ha_resolve_by_name(const_cast<THD *>(thd), &SEQUENCE_BASE_ENGINE_NAME,
                            false);
}

/**
  Assign initial default values of sequence fields

  @retval   void
*/
void Sequence_info::init_default() {
  DBUG_ENTER("Sequence_info::init_default");
  values[FIELD_NUM_CURRVAL] = 0;
  values[FIELD_NUM_NEXTVAL] = 0;
  values[FIELD_NUM_MINVALUE] = 1;
  values[FIELD_NUM_MAXVALUE] = ULLONG_MAX;
  values[FIELD_NUM_START] = 1;
  values[FIELD_NUM_INCREMENT] = 1;
  values[FIELD_NUM_CACHE] = 10000;
  values[FIELD_NUM_CYCLE] = 0;
  values[FIELD_NUM_ROUND] = 0;

  base_db_type = NULL;
  db = NULL;
  table_name = NULL;

  DBUG_VOID_RETURN;
}

/**
  Assign the initial values for all sequence fields
*/
Sequence_info::Sequence_info() {
  init_default();
}
/**
  Sequence field setting function

  @param[in]    field_num   Sequence field number
  @param[in]    value       Sequence field value

  @retval       void
*/
void Sequence_info::init_value(const Fields field_num, const ulonglong value) {
  DBUG_ENTER("Sequence_info::init_value");
  values[field_num] = value;
  DBUG_VOID_RETURN;
}

/**
  Validate sequence values
  Require:
    1. max value >= min value
    2. start >= min value
    3. increment >= 1
    4. max value >= start

  @param[in]    item    field value

  @retval       false   valid
  @retval       true    invalid
*/
bool check_sequence_values_valid(const ulonglong *items) {
  DBUG_ENTER("check_sequence_values_valid");
  if (items[Sequence_field::FIELD_NUM_MAXVALUE] >=
          items[Sequence_field::FIELD_NUM_MINVALUE] &&
      items[Sequence_field::FIELD_NUM_START] >=
          items[Sequence_field::FIELD_NUM_MINVALUE] &&
      items[Sequence_field::FIELD_NUM_INCREMENT] >= 1 &&
      items[Sequence_field::FIELD_NUM_MAXVALUE] >
          items[Sequence_field::FIELD_NUM_START])
    DBUG_RETURN(false);

  DBUG_RETURN(true);
}

/*
  Check whether inited values are valid through
  syntax:
    CREATE SEQUENCE [IF NOT EXISTS] schema.seqName
     [START WITH <constant>]
     [MINVALUE <constant>]
     [MAXVALUE <constant>]
     [INCREMENT BY <constant>]
     [CACHE <constant> | NOCACHE]
     [CYCLE | NOCYCLE]
    ;

  @retval       true        Invalid
  @retval       false       valid
*/
bool Sequence_info::check_valid() const {
  DBUG_ENTER("Sequence_info::check_valid");

  if (check_sequence_values_valid(values)) {
    my_error(ER_SEQUENCE_INVALID, MYF(0), db, table_name);
    DBUG_RETURN(true);
  }
  DBUG_RETURN(false);
}

/**
  Sequence field getting function

  @param[in]    field_num   Sequence field number

  @retval       ulonglong   Sequence field value
*/
ulonglong Sequence_info::get_value(const Fields field_num) const {
  DBUG_ENTER("Sequence_info::get_value");
  DBUG_ASSERT(field_num < FIELD_NUM_END);
  DBUG_RETURN(values[field_num]);
}

/**
  Release the ref count if locked
*/
Sequence_property::~Sequence_property() {
  if (m_plugin) plugin_unlock(NULL, m_plugin);
}

/**
  Configure the sequence flags and base db_type when open_table_share.

  @param[in]    plugin      Storage engine plugin
*/
void Sequence_property::configure(plugin_ref plugin) {
  handlerton *hton;
  if (plugin && ((hton = plugin_data<handlerton *>(plugin))) &&
      hton->db_type == DB_TYPE_SEQUENCE_DB) {
    if ((m_plugin = ha_resolve_sequence_base(NULL))) {
      base_db_type = plugin_data<handlerton *>(m_plugin);
      m_sequence = true;
    }
  }
}
/**
  Judge the sequence iteration type according to the query string.

  @param[in]    table         TABLE object

  @retval       iteration mode
*/
Sequence_iter_mode sequence_iteration_type(TABLE *table) {
  DBUG_ENTER("sequence_iteration_type");
  if (bitmap_is_set(table->read_set, Sequence_field::FIELD_NUM_NEXTVAL))
    DBUG_RETURN(Sequence_iter_mode::IT_NEXTVAL);

  DBUG_RETURN(Sequence_iter_mode::IT_NON_NEXTVAL);
}

/**
  Check the sequence table fields validation

  @param[in]    alter info    The alter information

  @retval       true          Failure
  @retval       false         Success

*/
bool check_sequence_fields_valid(Alter_info *alter_info) {
  Create_field *field;
  List_iterator<Create_field> it(alter_info->create_list);
  size_t field_count;
  size_t field_no;
  DBUG_ENTER("check_sequence_fields_valid");

  field_count = alter_info->create_list.elements;
  field_no = 0;
  if (field_count != Sequence_field::FIELD_NUM_END ||
      alter_info->key_list.size() > 0)
    DBUG_RETURN(true);

  while ((field = it++)) {
    if (my_strcasecmp(system_charset_info, seq_fields[field_no].field_name,
                      field->field_name) ||
        (field->flags != (NOT_NULL_FLAG | NO_DEFAULT_VALUE_FLAG)) ||
        (field->sql_type != seq_fields[field_no].field_type))
      DBUG_RETURN(true);

    field_no++;
  }
  DBUG_RETURN(false);
}

/// @} (end of group Sequence Engine)
