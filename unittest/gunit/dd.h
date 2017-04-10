/* Copyright (c) 2014, 2016 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef DD_INCLUDED
#define DD_INCLUDED

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "base_mock_handler.h"
#include "base_mock_field.h"
#include "fake_table.h"

#include "dd/types/charset.h"
#include "dd/types/collation.h"
#include "dd/types/column.h"
#include "dd/types/column_type_element.h"
#include "dd/types/foreign_key.h"
#include "dd/types/foreign_key_element.h"
#include "dd/types/index.h"
#include "dd/types/index_element.h"
#include "dd/types/partition.h"
#include "dd/types/partition_value.h"
#include "dd/types/partition_index.h"

#include "dd/types/schema.h"
#include "dd/types/table.h"
#include "dd/types/tablespace.h"
#include "dd/types/tablespace_file.h"
#include "dd/types/view.h"
#include "dd/types/view_table.h"
#include "dd/types/event.h"
#include "dd/types/procedure.h"
#include "dd/types/parameter.h"
#include "dd/types/trigger.h"


namespace dd_unittest {

using ::testing::_;
using ::testing::Return;
using ::testing::NiceMock;
using ::testing::StrictMock;

/**
  Mock handler for dictionary operations.
*/
class Mock_dd_HANDLER : public Base_mock_HANDLER
{
public:
  // Mock method used indirectly by find_record
  MOCK_METHOD5(index_read_idx_map,
               int(::uchar*, ::uint, const ::uchar*,
                   key_part_map, enum ha_rkey_function));

  // Handler method used for inserts
  MOCK_METHOD1(write_row, int(::uchar*));

  // Handler method used for updates
  MOCK_METHOD2(update_row, int(const ::uchar*, ::uchar*));

  Mock_dd_HANDLER(handlerton *ht, TABLE_SHARE *share):
    Base_mock_HANDLER(ht, share)
  { }

  virtual ~Mock_dd_HANDLER()
  { }
};

/**
  Mock class for Field_longlong. Mock store and val_int, provide fake
  storage to set contents to be returned or to capture values stored.
*/
class Mock_dd_field_longlong: public Base_mock_field_longlong
{
  longlong m_fake_val;
  bool m_val_is_unsigned;

public:
  // Avoid warning about hiding other overloaded versions of store().
  using Field_longlong::store;

  /*
    Mock the store and val_int methods.
    Note: Sun Studio needs a little help in resolving longlong.
  */
  MOCK_METHOD2(store, type_conversion_status(::longlong, bool));
  MOCK_METHOD0(val_int, ::longlong(void));
  MOCK_METHOD0(val_uint, ::ulonglong(void));

  /*
    Add fake methods to set and get expected contents.
  */
  type_conversion_status fake_store(longlong val, bool unsigned_val)
  {
    m_val_is_unsigned= unsigned_val;
    m_fake_val= val;
    return TYPE_OK;
  }

  longlong fake_val_int()
  { return m_fake_val; }

  ulonglong fake_val_uint()
  { return static_cast<ulonglong>(m_fake_val); }

  Mock_dd_field_longlong(): Base_mock_field_longlong()
  { }

  virtual ~Mock_dd_field_longlong()
  {}
};

/**
  Mock class for Field_varstring. Mock store and val_str, provide fake
  storage to set contents to be returned or to capture values stored.
*/
class Mock_dd_field_varstring: public Base_mock_field_varstring
{
  const char* m_fake_val;

public:
  // Avoid warning about hiding other overloaded versions of store().
  using Field_varstring::store;

  /*
    Mock the store and var_str methods.
  */
  MOCK_METHOD3(store, type_conversion_status(const char*, size_t length,
                                             const CHARSET_INFO*));
  MOCK_METHOD2(val_str, String*(String*, String*));

  /*
    Add fake methods to set and get expected contents.
  */
  type_conversion_status fake_store(const char *str)
  {
    m_fake_val= str;
    return TYPE_OK;
  }

  String* fake_val_str(String *str)
  {
    str->set((const char*)m_fake_val, strlen(m_fake_val), &my_charset_latin1);
    return str;
  }

  const char* fake_val_c_str()
  {
    return m_fake_val;
  }

  Mock_dd_field_varstring(uint32 length, TABLE_SHARE *share):
    Base_mock_field_varstring(length, share)
  { }

  virtual ~Mock_dd_field_varstring()
  { }
};

/**
  Create a Fake_TABLE object representing the schemata table.
*/
inline Fake_TABLE *get_schema_table(THD *thd, handlerton *hton)
{
  List<Field> m_field_list;
  Fake_TABLE *table= NULL;
  Fake_TABLE_SHARE dummy_share(1); // Keep Field_varstring constructor happy.

  // Add fields
  m_field_list.push_back(new Mock_dd_field_longlong());  // id
  m_field_list.push_back(new Mock_dd_field_longlong());  // catalog_id
  m_field_list.push_back(new Mock_dd_field_varstring(64, &dummy_share)); // name
  m_field_list.push_back(new Mock_dd_field_longlong());  // collation_id
  m_field_list.push_back(new Mock_dd_field_longlong());  // created
  m_field_list.push_back(new Mock_dd_field_longlong());  // last_altered

  // Create table object (and table share implicitly).
  table= new Fake_TABLE(m_field_list);

  // Create a strict mock handler for the share.
  StrictMock<Mock_dd_HANDLER> *ha= new StrictMock<Mock_dd_HANDLER>(hton, table->s);

  // Set current open table.
  ha->change_table_ptr(table, table->s);

  // Assign handler and thd.
  table->set_handler(ha);
  table->in_use= thd;

  // Setup fake key.
  table->key_info[0].key_length= 1;
  table->key_info[0].key_part[0].field= table->field[0];
  table->key_info[0].key_part[0].length= 1;
  table->key_info[0].key_part[0].null_offset= 0;

  table->key_info[1].key_length= 64;
  table->key_info[1].key_part[0].field= table->field[1];
  table->key_info[1].key_part[0].length= 64;
  table->key_info[1].key_part[0].null_offset= 0;

  // Setup the fake share to avoid asserts failing in the handler API.
  table->s->varchar_fields= 1;
  table->s->cached_row_logging_check= 0;
  table->s->reclength= 512;
  table->s->default_values= new uchar[table->s->reclength];
  table->s->tmp_table= NON_TRANSACTIONAL_TMP_TABLE;

  // Allocate dummy records to avoid failures in the handler functions.
  table->record[0]= new uchar[table->s->reclength];
  table->record[1]= new uchar[table->s->reclength];

  return table;
}


// Overloaded functions for populating DD objects

inline void set_attributes(dd::Schema *obj, const dd::String_type &name)
{
  obj->set_name(name);
  obj->set_default_collation_id(1);
}


inline void set_attributes(dd::Tablespace *obj, const dd::String_type &name)
{
  //
  // Create a new tablespacefile
  //
  dd::Tablespace_file *tsf_obj= obj->add_file();
  tsf_obj->set_filename(name + "file1");
  obj->set_engine("innodb");
  obj->set_name(name);
}


inline void set_attributes(dd::Table *obj, const dd::String_type &name,
                           const dd::Schema &schema)
{
  obj->set_name(name);

  obj->set_schema_id(schema.id());
  obj->set_collation_id(1);
  obj->set_tablespace_id(1);
  obj->set_engine("innodb");

  //
  // Create a new column
  //
  dd::Column *col_obj1= obj->add_column();
  col_obj1->set_name(name + "col2");
  col_obj1->set_default_value_null(true);
  col_obj1->set_collation_id(1);

  // New column of type enum/set
  dd::Column *col_obj2= obj->add_column();
  col_obj2->set_name(name + "col3");
  col_obj2->set_default_value_null(true);
  col_obj2->set_type(dd::enum_column_types::ENUM);
  col_obj2->set_collation_id(1);

  dd::Column_type_element *elem_obj= col_obj2->add_element();
  elem_obj->set_name("enum elem1");

  //
  // Create a new indexes
  //
  dd::Index *idx_obj= obj->add_index();
  idx_obj->set_name(name + "idx2");
  idx_obj->set_comment("Index2 comment");
  idx_obj->set_engine("innodb");
  idx_obj->set_tablespace_id(1);
  idx_obj->add_element(col_obj2);

  // Add hidden index element
  dd::Index_element *ie= idx_obj->add_element(col_obj1);
  ie->set_hidden(true);

  // Create one more index on table.
  dd::Index *idx2_obj= obj->add_index();
  idx2_obj->set_name(name + "idx3");
  idx2_obj->set_engine("innodb");
  idx2_obj->set_comment("Index3 comment");

  // Copy Index elements from first index.
  for (const dd::Index_element *e : idx_obj->elements())
    idx2_obj->add_element(const_cast<dd::Column*>(&e->column()));

  //
  // Store table partition information
  //

  dd::Partition *part_obj= obj->add_partition();
  part_obj->set_name("table_part1");
  part_obj->set_level(1);
  part_obj->set_number(2);
  part_obj->set_comment("Partition comment");
  part_obj->set_tablespace_id(1);

  dd::Partition_value *part_value_obj= part_obj->add_value();
  part_value_obj->set_list_num(1);
  part_value_obj->set_column_num(2);
  part_value_obj->set_value_utf8("part value");

  dd::Partition_index *part_index_obj= part_obj->add_index(idx_obj);
  part_index_obj->set_tablespace_id(1);

  //
  // Store table trigger information
  //

  dd::Trigger *trig_obj=
    obj->add_trigger(dd::Trigger::enum_action_timing::AT_BEFORE,
                     dd::Trigger::enum_event_type::ET_INSERT);
  trig_obj->set_name("newtrigger0");
  trig_obj->set_definer("definer_username", "definer_hostname");
  trig_obj->set_client_collation_id(1);
  trig_obj->set_connection_collation_id(1);
  trig_obj->set_schema_collation_id(1);
}


inline void set_attributes(dd::View *obj, const dd::String_type &name,
                           const dd::Schema &schema)
{
  obj->set_name(name);
  obj->set_definer("definer_username", "definer_hostname");
  obj->set_schema_id(schema.id());
  obj->set_client_collation_id(1);
  obj->set_connection_collation_id(1);

  //
  // Create a new column of type ENUM
  //

  dd::Column *col_obj= obj->add_column();
  col_obj->set_name(name + "viewcol2");
  col_obj->set_collation_id(1);
  col_obj->set_default_value_null(true);

  //
  // Add tables using which view is defined.
  //

  dd::View_table *vt_obj= obj->add_table();
  vt_obj->set_table_catalog("def");
  vt_obj->set_table_schema("test");
  vt_obj->set_table_name("t1");
}

inline void set_attributes(dd::Event *obj,
                           const dd::String_type &name,
                           const dd::Schema &schema)
{
  obj->set_name(name);
  obj->set_definer("definer_username", "definer_hostname");
  obj->set_schema_id(schema.id());
  obj->set_client_collation_id(1);
  obj->set_connection_collation_id(1);
  obj->set_schema_collation_id(1);

}

inline void set_attributes(dd::Procedure *obj,
                           const dd::String_type &name,
                           const dd::Schema &schema)
{
  obj->set_name(name);
  obj->set_definer("definer_username", "definer_hostname");
  obj->set_schema_id(schema.id());
  obj->set_client_collation_id(1);
  obj->set_connection_collation_id(1);
  obj->set_schema_collation_id(1);

  //
  // Create a new parameter
  //

  dd::Parameter *param_obj= obj->add_parameter();
  param_obj->set_name(name + "param1");
  param_obj->set_collation_id(1);

}


inline void set_attributes(dd::Foreign_key *obj, const dd::String_type &name)
{
  obj->set_name(name);
  obj->set_match_option(dd::Foreign_key::OPTION_FULL);
  obj->set_update_rule(dd::Foreign_key::RULE_SET_DEFAULT);
  obj->set_delete_rule(dd::Foreign_key::RULE_CASCADE);
  obj->referenced_table_schema_name("mysql");
  obj->referenced_table_name("dual");

  // Create Foreign key column
  dd::Foreign_key_element *fke= obj->add_element();
  fke->referenced_column_name("EMPLOYEE");
}

inline void set_attributes(dd::Charset *obj, const dd::String_type &name)
{
  obj->set_name(name);
  obj->set_default_collation_id(42);
}

inline void set_attributes(dd::Collation *obj, const dd::String_type &name)
{
  obj->set_name(name);
  obj->set_charset_id(42);
}

template <typename T>
T *nullp()
{
  return NULL;
}

} // namespace dd_unittest

#endif // DD_INCLUDED
