/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/dd/impl/tables/dd_properties.h"

#include <string>

#include "m_ctype.h"
#include "my_base.h"
#include "my_bitmap.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/udf_registration_types.h"
#include "mysqld_error.h"
#include "sql/auth/sql_security_ctx.h"
#include "sql/dd/impl/raw/raw_table.h"
#include "sql/dd/impl/transaction_impl.h"
#include "sql/dd/impl/types/object_table_definition_impl.h"
#include "sql/dd/properties.h"
#include "sql/dd/string_type.h"       // dd::String_type, dd::Stringstream_type
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/sql_const.h"
#include "sql/stateless_allocator.h"
#include "sql/table.h"
#include "sql_string.h"

namespace dd {
namespace tables {

const DD_properties &DD_properties::instance()
{
  static DD_properties *s_instance= new DD_properties();
  return *s_instance;
}


// Setup the initial definition of mysql.dd_properties table.
DD_properties::DD_properties()
{
  m_target_def.table_name(table_name());
  m_target_def.dd_version(0);

  m_target_def.add_field(FIELD_PROPERTIES,
                         "FIELD_PROPERTIES",
                         "properties MEDIUMTEXT");

  // Insert the target dictionary version
  m_target_def.add_populate_statement(
                 "INSERT INTO dd_properties (properties)"
                 "VALUES ('" + get_target_versions() + "')");
}


// Read property for the given key.
uint DD_properties::get_property(THD *thd,
                                 String_type key, bool *exists) const
{
  /*
    Start a DD transaction to get the version number.
    Please note that we must do this read using isolation
    level ISO_READ_UNCOMMITTED because the SE undo logs may not yet
    be available.
  */
  Transaction_ro trx(thd, ISO_READ_UNCOMMITTED);
  uint version= 0;
  *exists= false;

  trx.otx.add_table<DD_properties>();

  if (!trx.otx.open_tables())
  {
    /*
      This code accesses the handler interface directly. It could be
      generalized and added to the raw module, but if this is the only use
      case, it may as well be kept here.
    */
    Raw_table *raw_t= trx.otx.get_table(table_name());
    DBUG_ASSERT(raw_t);
    TABLE *t= raw_t->get_table();
    DBUG_ASSERT(t);
    t->use_all_columns();
    if (!t->file->ha_rnd_init(true) &&
        !t->file->ha_rnd_next(t->record[0]))
    {
      char buff[MAX_FIELD_WIDTH];
      String val(buff, sizeof(buff), &my_charset_bin);

      t->field[FIELD_PROPERTIES]->val_str(&val);

      dd::Properties *p=
        dd::Properties_impl::parse_properties(val.c_ptr_safe());
      if (p)
      {
        if (p->exists(key))
          p->get_uint32(key, &version);
        delete p;
      }
    }

    t->file->ha_rnd_end();
    *exists= true;
  }
  return version;
}


// Set property for the given key.
bool DD_properties::set_property(THD *thd, String_type key, uint value)
{
  Update_dictionary_tables_ctx ctx(thd);
  ctx.otx.add_table<DD_properties>();
  int rc= 0;

  if (!ctx.otx.open_tables())
  {
    /*
      This code accesses the handler interface directly. It could be
      generalized and added to the raw module, but as it is rarely used
      case, it may as well be kept here.
    */
    Raw_table *raw_t= ctx.otx.get_table(table_name());
    DBUG_ASSERT(raw_t);
    TABLE *t= raw_t->get_table();
    DBUG_ASSERT(t);
    t->use_all_columns();
    bitmap_set_all(t->write_set);
    bitmap_set_all(t->read_set);

    if ((rc= t->file->ha_rnd_init(true)))
    {
      t->file->print_error(rc, MYF(0));
      return(true);
    }

    if (!t->file->ha_rnd_next(t->record[0]))
    {
      // Get old properties and set new value for the given key.
      char buff[MAX_FIELD_WIDTH];
      String val(buff, sizeof(buff), &my_charset_bin);
      t->field[FIELD_PROPERTIES]->val_str(&val);
      dd::Properties *p=
        dd::Properties_impl::parse_properties(val.c_ptr_safe());
      if (p == nullptr)
      {
        my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR),
                 static_cast<int>(sizeof(dd::Properties_impl)));
        t->file->ha_rnd_end();
        return true;
      }

      store_record(t, record[1]);

      p->set_uint32(key, value);

      // Update the table.
      const String_type &s= p->raw_string();
      t->field[FIELD_PROPERTIES]->store(s.c_str(), s.length(),
                                        system_charset_info);
      delete p;
      rc= t->file->ha_update_row(t->record[1], t->record[0]);
      if (rc && rc != HA_ERR_RECORD_IS_THE_SAME)
      {
        t->file->print_error(rc, MYF(0));
        t->file->ha_rnd_end();
        return true;
      }
    }
    else
    {
      // mysql.dd_properties should contain atleast one row.
      DBUG_ASSERT(0);
      t->file->ha_rnd_end();
      return true;
    }

    t->file->ha_rnd_end();
  }

  return false;
}


// Get property value for key 'DD_version'
uint DD_properties::get_actual_dd_version(THD *thd, bool *exists) const
{
  return get_property(thd, "DD_version", exists);
}


// Get property value for key 'IS_version'
uint DD_properties::get_actual_I_S_version(THD *thd) const
{
  bool not_used;
  return get_property(thd, "IS_version", &not_used);
}


// Get property value for key 'PS_version'
uint DD_properties::get_actual_P_S_version(THD *thd) const
{
  bool not_used;
  return get_property(thd, "PS_version", &not_used);
}


// Set property value for key 'IS_version'
bool DD_properties::set_I_S_version(THD *thd, uint version)
{
  return set_property(thd, "IS_version", version);
}


// Set property value for key 'PS_version'
bool DD_properties::set_P_S_version(THD *thd, uint version)
{
  return set_property(thd, "PS_version", version);
}

}
}
