/* Copyright (c) 2014, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef DD__ABSTRACT_TABLE_IMPL_INCLUDED
#define DD__ABSTRACT_TABLE_IMPL_INCLUDED

#include <stddef.h>
#include <sys/types.h>
#include <memory>  // std::unique_ptr

#include "my_dbug.h"
#include "my_inttypes.h"
#include "sql/dd/impl/properties_impl.h"  // Properties_impl
#include "sql/dd/impl/raw/raw_record.h"
#include "sql/dd/impl/types/entity_object_impl.h"  // dd::Entity_object_impl
#include "sql/dd/impl/types/weak_object_impl.h"
#include "sql/dd/object_id.h"
#include "sql/dd/properties.h"
#include "sql/dd/sdi_fwd.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/abstract_table.h"  // dd::Abstract_table
#include "sql/dd/types/column.h"          // IWYU pragma: keep
#include "sql/sql_time.h"                 // gmt_time_to_local_time

class Time_zone;

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Object_table;
class Open_dictionary_tables_ctx;
class Sdi_rcontext;
class Sdi_wcontext;
class Weak_object;

class Abstract_table_impl : public Entity_object_impl,
                            virtual public Abstract_table {
 public:
  const Object_table &object_table() const override;

  static void register_tables(Open_dictionary_tables_ctx *otx);

  bool validate() const override;

  bool restore_children(Open_dictionary_tables_ctx *otx) override;

  bool store_children(Open_dictionary_tables_ctx *otx) override;

  bool drop_children(Open_dictionary_tables_ctx *otx) const override;

  bool restore_attributes(const Raw_record &r) override;

  bool store_attributes(Raw_record *r) override;

 protected:
  void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const;

  bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val);

 public:
  void debug_print(String_type &outb) const override;

 public:
  /////////////////////////////////////////////////////////////////////////
  // schema.
  /////////////////////////////////////////////////////////////////////////

  Object_id schema_id() const override { return m_schema_id; }

  void set_schema_id(Object_id schema_id) override { m_schema_id = schema_id; }

  /////////////////////////////////////////////////////////////////////////
  // mysql_version_id.
  // Primarily intended for debugging, but can be used as a last-resort
  // version check for SE data and other items, but in general other
  // mechanisms should be preferred.
  /////////////////////////////////////////////////////////////////////////

  uint mysql_version_id() const override { return m_mysql_version_id; }

  // TODO: Commented out as it is not needed as we either use the value
  // assigned by the constructor, or restore a value from the TABLES
  // table. It may be necessary when implementing upgrade.
  // virtual void set_mysql_version_id(uint mysql_version_id)
  //{ m_mysql_version_id= mysql_version_id; }

  /////////////////////////////////////////////////////////////////////////
  // options.
  /////////////////////////////////////////////////////////////////////////

  const Properties &options() const override { return m_options; }

  Properties &options() override { return m_options; }

  bool set_options(const Properties &options) override {
    return m_options.insert_values(options);
  }

  bool set_options(const String_type &options_raw) override {
    return m_options.insert_values(options_raw);
  }

  /////////////////////////////////////////////////////////////////////////
  // created.
  /////////////////////////////////////////////////////////////////////////

  ulonglong created(bool convert_time) const override {
    return convert_time ? gmt_time_to_local_time(m_created) : m_created;
  }

  void set_created(ulonglong created) override { m_created = created; }

  /////////////////////////////////////////////////////////////////////////
  // last altered.
  /////////////////////////////////////////////////////////////////////////

  ulonglong last_altered(bool convert_time) const override {
    return convert_time ? gmt_time_to_local_time(m_last_altered)
                        : m_last_altered;
  }

  void set_last_altered(ulonglong last_altered) override {
    m_last_altered = last_altered;
  }

  /////////////////////////////////////////////////////////////////////////
  // hidden.
  /////////////////////////////////////////////////////////////////////////

  enum_hidden_type hidden() const override { return m_hidden; }

  void set_hidden(enum_hidden_type hidden) override { m_hidden = hidden; }

  /////////////////////////////////////////////////////////////////////////
  // Column collection.
  /////////////////////////////////////////////////////////////////////////

  Column *add_column() override;
  bool drop_column(const String_type &name) override;

  const Column_collection &columns() const override { return m_columns; }

  Column_collection *columns() override { return &m_columns; }

  const Column *get_column(Object_id column_id) const;

  Column *get_column(Object_id column_id);

  const Column *get_column(const String_type &name) const override;

  Column *get_column(const String_type &name);

  // Fix "inherits ... via dominance" warnings
  Entity_object_impl *impl() override { return Entity_object_impl::impl(); }
  const Entity_object_impl *impl() const override {
    return Entity_object_impl::impl();
  }
  Object_id id() const override { return Entity_object_impl::id(); }
  bool is_persistent() const override {
    return Entity_object_impl::is_persistent();
  }
  const String_type &name() const override {
    return Entity_object_impl::name();
  }
  void set_name(const String_type &name) override {
    Entity_object_impl::set_name(name);
  }

 protected:
  Abstract_table_impl();

  ~Abstract_table_impl() override = default;

 private:
  // Fields.

  uint m_mysql_version_id;

  // TODO-POST-MERGE-TO-TRUNK:
  // Add new field m_last_checked_for_upgrade

  ulonglong m_created;
  ulonglong m_last_altered;

  enum_hidden_type m_hidden;

  Properties_impl m_options;

  // References to tightly-coupled objects.

  Column_collection m_columns;

  // References to other objects.

  Object_id m_schema_id;

 protected:
  Abstract_table_impl(const Abstract_table_impl &src);
};

///////////////////////////////////////////////////////////////////////////

}  // namespace dd

#endif  // DD__ABSTRACT_TABLE_IMPL_INCLUDED
