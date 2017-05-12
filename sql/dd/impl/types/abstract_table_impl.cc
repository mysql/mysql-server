/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "dd/impl/types/abstract_table_impl.h"

#include <new>
#include <sstream>

#include "dd/string_type.h"                 // dd::String_type
#include "dd/impl/properties_impl.h"        // Properties_impl
#include "dd/impl/raw/object_keys.h"        // Primary_id_key
#include "dd/impl/raw/raw_record.h"         // Raw_record
#include "dd/impl/sdi_impl.h"               // sdi read/write functions
#include "dd/impl/tables/columns.h"         // Columns
#include "dd/impl/tables/tables.h"          // Tables
#include "dd/impl/transaction_impl.h"       // Open_dictionary_tables_ctx
#include "dd/impl/types/column_impl.h"      // Column_impl
#include "dd/types/column.h"
#include "dd/types/dictionary_object_table.h"
#include "dd/types/table.h"
#include "dd/types/view.h"                  // View
#include "dd/types/weak_object.h"
#include "m_ctype.h"
#include "m_string.h"
#include "my_sys.h"
#include "mysql_version.h"                  // MYSQL_VERSION_ID
#include "mysqld.h"
#include "mysqld_error.h"                   // ER_*
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"

using dd::tables::Columns;
using dd::tables::Tables;

namespace dd {

class Sdi_rcontext;
class Sdi_wcontext;

///////////////////////////////////////////////////////////////////////////
// Abstract_table implementation.
///////////////////////////////////////////////////////////////////////////

const Object_type &Abstract_table::TYPE()
{
  static Abstract_table_type s_instance;
  return s_instance;
}

const Dictionary_object_table &Abstract_table::OBJECT_TABLE()
{
  return Tables::instance();
}

///////////////////////////////////////////////////////////////////////////
// Abstract_table_impl implementation.
///////////////////////////////////////////////////////////////////////////

Abstract_table_impl::Abstract_table_impl()
 :m_mysql_version_id(MYSQL_VERSION_ID),
  m_created(0),
  m_last_altered(0),
  m_hidden(false),
  m_options(new Properties_impl()),
  m_columns(),
  m_schema_id(INVALID_OBJECT_ID)
{
}

///////////////////////////////////////////////////////////////////////////

bool Abstract_table_impl::set_options_raw(const String_type &options_raw)
{
  Properties *properties=
    Properties_impl::parse_properties(options_raw);

  if (!properties)
    return true; // Error status, current values has not changed.

  m_options.reset(properties);
  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Abstract_table_impl::validate() const
{
  if (schema_id() == INVALID_OBJECT_ID)
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Abstract_table_impl::OBJECT_TABLE().name().c_str(),
             "Schema ID is not set");
    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Abstract_table_impl::restore_children(Open_dictionary_tables_ctx *otx)
{
  return m_columns.restore_items(
    this,
    otx,
    otx->get_table<Column>(),
    Columns::create_key_by_table_id(this->id()));
}

///////////////////////////////////////////////////////////////////////////

bool Abstract_table_impl::store_children(Open_dictionary_tables_ctx *otx)
{
  return m_columns.store_items(otx);
}

///////////////////////////////////////////////////////////////////////////

bool Abstract_table_impl::drop_children(Open_dictionary_tables_ctx *otx) const
{
  return m_columns.drop_items(
    otx,
    otx->get_table<Column>(),
    Columns::create_key_by_table_id(this->id()));
}

/////////////////////////////////////////////////////////////////////////

bool Abstract_table_impl::restore_attributes(const Raw_record &r)
{
  restore_id(r, Tables::FIELD_ID);
  restore_name(r, Tables::FIELD_NAME);

  m_created= r.read_int(Tables::FIELD_CREATED);
  m_last_altered= r.read_int(Tables::FIELD_LAST_ALTERED);
  m_hidden= r.read_bool(Tables::FIELD_HIDDEN);
  m_schema_id= r.read_ref_id(Tables::FIELD_SCHEMA_ID);
  m_mysql_version_id= r.read_uint(Tables::FIELD_MYSQL_VERSION_ID);

  // Special cases dealing with NULL values for nullable fields

  set_options_raw(r.read_str(Tables::FIELD_OPTIONS, ""));

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Abstract_table_impl::store_attributes(Raw_record *r)
{
  //
  // Special cases dealing with NULL values for nullable fields
  //   - Store NULL if version is not set
  //     Eg: USER_VIEW or SYSTEM_VIEW may not have version set
  //   - Store NULL if se_private_id is not set
  //     Eg: A non-innodb table may not have se_private_id
  //   - Store NULL if collation id is not set
  //     Eg: USER_VIEW will not have collation id set.
  //   - Store NULL if tablespace id is not set
  //     Eg: A non-innodb table may not have tablespace
  //   - Store NULL in options if there are no key=value pairs
  //   - Store NULL in se_private_data if there are no key=value pairs
  //   - Store NULL in engine if it is not set.
  //   - Store NULL in partition expression
  //   - Store NULL in subpartition expression
  //

  // Store field values
  return
    store_id(r, Tables::FIELD_ID) ||
    store_name(r, Tables::FIELD_NAME) ||
    r->store_ref_id(Tables::FIELD_SCHEMA_ID, m_schema_id) ||
    r->store(Tables::FIELD_TYPE, static_cast<int>(type())) ||
    r->store(Tables::FIELD_MYSQL_VERSION_ID, m_mysql_version_id) ||
    r->store(Tables::FIELD_OPTIONS, *m_options) ||
    r->store(Tables::FIELD_CREATED, m_created) ||
    r->store(Tables::FIELD_LAST_ALTERED, m_last_altered) ||
    r->store(Tables::FIELD_HIDDEN, m_hidden);
}

///////////////////////////////////////////////////////////////////////////

bool Abstract_table::update_id_key(id_key_type *key, Object_id id)
{
  key->update(id);
  return false;
}

///////////////////////////////////////////////////////////////////////////

static_assert(Tables::FIELD_VIEW_DEFINITION == 22,
              "Tables definition has changed, review (de)ser member function"
              "s (also in derived classes");

void Abstract_table_impl::serialize(Sdi_wcontext *wctx, Sdi_writer *w) const
{
  Entity_object_impl::serialize(wctx, w);

  write(w, m_mysql_version_id, STRING_WITH_LEN("mysql_version_id"));
  write(w, m_created, STRING_WITH_LEN("created"));
  write(w, m_last_altered, STRING_WITH_LEN("last_altered"));
  write(w, m_hidden, STRING_WITH_LEN("hidden"));
  write_properties(w, m_options, STRING_WITH_LEN("options"));
  serialize_each(wctx, w, m_columns, STRING_WITH_LEN("columns"));
  write(w, lookup_schema_name(wctx),
        STRING_WITH_LEN("schema_ref"));
}

///////////////////////////////////////////////////////////////////////////

bool Abstract_table_impl::deserialize(Sdi_rcontext *rctx,
                                      const RJ_Value &val)
{
  Entity_object_impl::deserialize(rctx, val);

  read(&m_mysql_version_id, val, "mysql_version_id");
  read(&m_created, val, "created");
  read(&m_last_altered, val, "last_altered");
  read(&m_hidden, val, "hidden");
  read_properties(&m_options, val, "options");
  deserialize_each(rctx, [this] () { return add_column(); },
                   val, "columns");
  return deserialize_schema_ref(rctx, &m_schema_id, val, "schema_ref");
}

///////////////////////////////////////////////////////////////////////////

bool Abstract_table::update_name_key(name_key_type *key,
                                     Object_id schema_id,
                                     const String_type &name)
{ return Tables::update_object_key(key, schema_id, name); }

///////////////////////////////////////////////////////////////////////////

void Abstract_table_impl::debug_print(String_type &outb) const
{
  dd::Stringstream_type ss;
  ss
    << "ABSTRACT TABLE OBJECT: { "
    << "id: {OID: " << id() << "}; "
    << "m_schema: {OID: " << m_schema_id << "}; "
    << "m_name: " << name() << "; "
    << "m_mysql_version_id: " << m_mysql_version_id << "; "
    << "m_options " << m_options->raw_string() << "; "
    << "m_created: " << m_created << "; "
    << "m_last_altered: " << m_last_altered << "; "
    << "m_hidden: " << m_hidden << "; "
    << "m_columns: " << m_columns.size() << " [ ";

  {
    for (const Column *c : m_columns)
    {
      String_type s;
      c->debug_print(s);
      ss << s << " | ";
    }
  }

  ss << "] ";
  ss << " }";

  outb= ss.str();
}

///////////////////////////////////////////////////////////////////////////
// Column collection.
///////////////////////////////////////////////////////////////////////////

Column *Abstract_table_impl::add_column()
{
  Column_impl *c= new (std::nothrow) Column_impl(this);
  m_columns.push_back(c);
  return c;
}

///////////////////////////////////////////////////////////////////////////

Column *Abstract_table_impl::get_column(Object_id column_id)
{
  for (Column *c : m_columns)
  {
    if (c->id() == column_id)
      return c;
  }

  return NULL;
}

///////////////////////////////////////////////////////////////////////////

const Column *Abstract_table_impl::get_column(Object_id column_id) const
{
  for (const Column *c : m_columns)
  {
    if (c->id() == column_id)
      return c;
  }

  return NULL;
}

///////////////////////////////////////////////////////////////////////////

Column *Abstract_table_impl::get_column(const String_type name)
{
  for (Column *c : m_columns)
  {
    // Column names are case-insensitive
    if (my_strcasecmp(system_charset_info,
                      name.c_str(),
                      c->name().c_str()) == 0)
      return c;
  }

  return NULL;
}

///////////////////////////////////////////////////////////////////////////

const Column *Abstract_table_impl::get_column(const String_type name) const
{
  for (const Column *c : m_columns)
  {
    // Column names are case-insensitive
    if (my_strcasecmp(system_charset_info,
                      name.c_str(),
                      c->name().c_str()) == 0)
      return c;
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////
// Table_type implementation.
///////////////////////////////////////////////////////////////////////////

void Abstract_table_type::register_tables(Open_dictionary_tables_ctx *otx) const
{
  otx->register_tables<Table>();
  otx->register_tables<View>();
}

///////////////////////////////////////////////////////////////////////////

Abstract_table_impl::Abstract_table_impl(const Abstract_table_impl &src)
  : Weak_object(src), Entity_object_impl(src),
    m_mysql_version_id(src.m_mysql_version_id),
    m_created(src.m_created),
    m_last_altered(src.m_last_altered),
    m_hidden(src.m_hidden),
    m_options(Properties_impl::parse_properties(src.m_options->raw_string())),
    m_columns(),
    m_schema_id(src.m_schema_id)
{
  m_columns.deep_copy(src.m_columns, this);
}
}
