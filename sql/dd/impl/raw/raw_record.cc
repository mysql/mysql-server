/* Copyright (c) 2014, 2015 Oracle and/or its affiliates. All rights reserved.

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

#include "dd/impl/raw/raw_record.h"

#include "field.h"                  // Field
#include "table.h"                  // TABLE

#include "dd/properties.h"          // dd::Properties

namespace dd {

///////////////////////////////////////////////////////////////////////////

Raw_record::Raw_record(TABLE *table)
 :m_table(table)
{
  bitmap_set_all(m_table->read_set);
}

///////////////////////////////////////////////////////////////////////////

/**
  @brief
    Update table record into SE

  @return true - on failure and error is reported.
  @return false - on success.
*/
bool Raw_record::update()
{
  DBUG_ENTER("Raw_record::update");

  int rc= m_table->file->ha_update_row(m_table->record[1], m_table->record[0]);

  /**
    We ignore HA_ERR_RECORD_IS_THE_SAME here for following reason.
    If in case we are updating childrens of some DD object,
    and only one of the children has really changed and other have
    not. Then we get HA_ERR_RECORD_IS_THE_SAME for children (rows)
    which has not really been modified.

    Currently DD framework creates/updates *all* childrens at once
    and we don't have machinism to update only required child.
    May be this is part of task which will implement inplace
    alter in better way, updating only the changed child (or row)
    and ignore others. Then we can remove the below check which
    ignores HA_ERR_RECORD_IS_THE_SAME.
  */

  if (rc && rc != HA_ERR_RECORD_IS_THE_SAME)
  {
    m_table->file->print_error(rc, MYF(0));
    DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
}

///////////////////////////////////////////////////////////////////////////

/**
  @brief
    Drop the record from SE

  @return true - on failure and error is reported.
  @return false - on success.
*/
bool Raw_record::drop()
{
  DBUG_ENTER("Raw_record::drop");

  int rc= m_table->file->ha_delete_row(m_table->record[1]);

  if (rc)
  {
    m_table->file->print_error(rc, MYF(0));
    DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
}

///////////////////////////////////////////////////////////////////////////

Field *Raw_record::field(int field_no) const
{
  return m_table->field[field_no];
}

///////////////////////////////////////////////////////////////////////////

bool Raw_record::store_pk_id(int field_no, Object_id id)
{
  field(field_no)->set_notnull();

  return (id == INVALID_OBJECT_ID) ? false : store(field_no, id);
}

///////////////////////////////////////////////////////////////////////////

bool Raw_record::store_ref_id(int field_no, Object_id id)
{
  if (id == INVALID_OBJECT_ID)
  {
    set_null(field_no, true);
    return false;
  }

  set_null(field_no, false);
  type_conversion_status rc= field(field_no)->store(id, true);

  DBUG_ASSERT(rc == TYPE_OK);
  return rc != TYPE_OK;
}

///////////////////////////////////////////////////////////////////////////

void Raw_record::set_null(int field_no, bool is_null)
{
  if (is_null)
    field(field_no)->set_null();
  else
    field(field_no)->set_notnull();
}

///////////////////////////////////////////////////////////////////////////

bool Raw_record::store(int field_no, const std::string &s, bool is_null)
{
  set_null(field_no, is_null);

  if (is_null)
    return false;

  type_conversion_status rc=
    field(field_no)->store(s.c_str(), s.length(), system_charset_info);

  DBUG_ASSERT(rc == TYPE_OK);
  return rc != TYPE_OK;
}

///////////////////////////////////////////////////////////////////////////

bool Raw_record::store(int field_no, ulonglong ull, bool is_null)
{
  set_null(field_no, is_null);

  if (is_null)
    return false;

  type_conversion_status rc=
    field(field_no)->store(ull, true);

  DBUG_ASSERT(rc == TYPE_OK);
  return rc != TYPE_OK;
}

///////////////////////////////////////////////////////////////////////////

bool Raw_record::store(int field_no, longlong ll, bool is_null)
{
  set_null(field_no, is_null);

  if (is_null)
    return false;

  type_conversion_status rc=
    field(field_no)->store(ll, false);

  DBUG_ASSERT(rc == TYPE_OK);
  return rc != TYPE_OK;
}

///////////////////////////////////////////////////////////////////////////

bool Raw_record::store(int field_no, const Properties &p)
{
  return store(field_no, p.raw_string(), p.empty());
}

///////////////////////////////////////////////////////////////////////////

bool Raw_record::is_null(int field_no) const
{
  return field(field_no)->is_null();
}

///////////////////////////////////////////////////////////////////////////

longlong Raw_record::read_int(int field_no) const
{
  return field(field_no)->val_int();
}

///////////////////////////////////////////////////////////////////////////

ulonglong Raw_record::read_uint(int field_no) const
{
  return static_cast<ulonglong>(field(field_no)->val_int());
}

///////////////////////////////////////////////////////////////////////////

std::string Raw_record::read_str(int field_no) const
{
  char buff[MAX_FIELD_WIDTH];
  String val(buff, sizeof(buff), &my_charset_bin);

  field(field_no)->val_str(&val);

  return std::string(val.ptr(), val.length());
}

///////////////////////////////////////////////////////////////////////////

Object_id Raw_record::read_ref_id(int field_no) const
{
  return field(field_no)->is_null() ?
         dd::INVALID_OBJECT_ID :
         read_int(field_no);
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

Raw_new_record::Raw_new_record(TABLE *table)
 :Raw_record(table)
{
  bitmap_set_all(m_table->write_set);
  bitmap_set_all(m_table->read_set);

  m_table->next_number_field= m_table->found_next_number_field;
  m_table->auto_increment_field_not_null= true;

  restore_record(m_table, s->default_values);
}

///////////////////////////////////////////////////////////////////////////

/**
  @brief
    Create new record in SE

  @return true - on failure and error is reported.
  @return false - on success.
*/
bool Raw_new_record::insert()
{
  DBUG_ENTER("Raw_new_record::insert");

  int rc= m_table->file->ha_write_row(m_table->record[0]);

  if (rc)
  {
    m_table->file->print_error(rc, MYF(0));
    DBUG_RETURN(true);
  }

  DBUG_RETURN(false);
}

///////////////////////////////////////////////////////////////////////////

Object_id Raw_new_record::get_insert_id() const
{
  Object_id id= m_table->file->insert_id_for_cur_row;

  // Objects without primary key should have still get INVALID_OBJECT_ID.
  return id ? id : dd::INVALID_OBJECT_ID;
}

///////////////////////////////////////////////////////////////////////////

void Raw_new_record::finalize()
{
  if (!m_table)
    return;

  m_table->auto_increment_field_not_null= false;
  m_table->file->ha_release_auto_increment();
  m_table->next_number_field= NULL;

  m_table= NULL;
}

///////////////////////////////////////////////////////////////////////////

}
