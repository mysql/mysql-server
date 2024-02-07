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

#ifndef DD__TABLE_IMPL_INCLUDED
#define DD__TABLE_IMPL_INCLUDED

#include <sys/types.h>
#include <memory>
#include <new>
#include <string>

#include "my_inttypes.h"
#include "mysql_version.h"  // MYSQL_VERSION_ID
#include "sql/dd/impl/properties_impl.h"
#include "sql/dd/impl/raw/raw_record.h"
#include "sql/dd/impl/types/abstract_table_impl.h"  // dd::Abstract_table_impl
#include "sql/dd/impl/types/entity_object_impl.h"
#include "sql/dd/impl/types/weak_object_impl.h"
#include "sql/dd/object_id.h"
#include "sql/dd/sdi_fwd.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/abstract_table.h"
#include "sql/dd/types/check_constraint.h"  // dd::Check_constraint
#include "sql/dd/types/foreign_key.h"       // dd::Foreign_key
#include "sql/dd/types/index.h"             // dd::Index
#include "sql/dd/types/partition.h"         // dd::Partition
#include "sql/dd/types/table.h"             // dd:Table
#include "sql/dd/types/trigger.h"           // dd::Trigger
#include "sql/strfunc.h"

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Column;
class Foreign_key;
class Index;
class Object_table;
class Open_dictionary_tables_ctx;
class Partition;
class Properties;
class Sdi_rcontext;
class Sdi_wcontext;
class Trigger_impl;
class Weak_object;
class Object_table;

class Table_impl : public Abstract_table_impl, virtual public Table {
 public:
  Table_impl();

  ~Table_impl() override;

 public:
  /////////////////////////////////////////////////////////////////////////
  // enum_table_type.
  /////////////////////////////////////////////////////////////////////////

  enum_table_type type() const override { return enum_table_type::BASE_TABLE; }

 public:
  static void register_tables(Open_dictionary_tables_ctx *otx);

  bool validate() const override;

  bool restore_children(Open_dictionary_tables_ctx *otx) override;

  bool store_children(Open_dictionary_tables_ctx *otx) override;

  bool drop_children(Open_dictionary_tables_ctx *otx) const override;

  bool restore_attributes(const Raw_record &r) override;

  bool store_attributes(Raw_record *r) override;

  void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const override;

  bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val) override;

  void debug_print(String_type &outb) const override;

 private:
  /**
    Store the trigger object in DD table.

    @param otx  current Open_dictionary_tables_ctx

    @returns
     false on success.
     true on failure.
  */
  bool store_triggers(Open_dictionary_tables_ctx *otx);

 public:
  /////////////////////////////////////////////////////////////////////////
  // is_temporary.
  /////////////////////////////////////////////////////////////////////////

  bool is_temporary() const override { return m_is_temporary; }
  void set_is_temporary(bool is_temporary) override {
    m_is_temporary = is_temporary;
  }

  /////////////////////////////////////////////////////////////////////////
  // collation.
  /////////////////////////////////////////////////////////////////////////

  Object_id collation_id() const override { return m_collation_id; }

  void set_collation_id(Object_id collation_id) override {
    m_collation_id = collation_id;
  }

  /////////////////////////////////////////////////////////////////////////
  // tablespace.
  /////////////////////////////////////////////////////////////////////////

  Object_id tablespace_id() const override { return m_tablespace_id; }

  void set_tablespace_id(Object_id tablespace_id) override {
    m_tablespace_id = tablespace_id;
  }

  bool is_explicit_tablespace() const override {
    bool is_explicit = false;
    if (options().exists("explicit_tablespace"))
      options().get("explicit_tablespace", &is_explicit);
    return is_explicit;
  }

  /////////////////////////////////////////////////////////////////////////
  // engine.
  /////////////////////////////////////////////////////////////////////////

  const String_type &engine() const override { return m_engine; }

  void set_engine(const String_type &engine) override { m_engine = engine; }

  /////////////////////////////////////////////////////////////////////////
  // row_format
  /////////////////////////////////////////////////////////////////////////

  enum_row_format row_format() const override { return m_row_format; }

  void set_row_format(enum_row_format row_format) override {
    m_row_format = row_format;
  }

  /////////////////////////////////////////////////////////////////////////
  // comment
  /////////////////////////////////////////////////////////////////////////

  const String_type &comment() const override { return m_comment; }

  void set_comment(const String_type &comment) override { m_comment = comment; }

  /////////////////////////////////////////////////////////////////////////
  // last_checked_for_upgrade_version_id
  /////////////////////////////////////////////////////////////////////////
  uint last_checked_for_upgrade_version_id() const override {
    return m_last_checked_for_upgrade_version_id;
  }

  void mark_as_checked_for_upgrade() override {
    m_last_checked_for_upgrade_version_id = MYSQL_VERSION_ID;
  }

  /////////////////////////////////////////////////////////////////////////
  // se_private_data.
  /////////////////////////////////////////////////////////////////////////

  const Properties &se_private_data() const override {
    return m_se_private_data;
  }

  Properties &se_private_data() override { return m_se_private_data; }

  bool set_se_private_data(const String_type &se_private_data_raw) override {
    return m_se_private_data.insert_values(se_private_data_raw);
  }

  bool set_se_private_data(const Properties &se_private_data) override {
    return m_se_private_data.insert_values(se_private_data);
  }

  /////////////////////////////////////////////////////////////////////////
  // se_private_id.
  /////////////////////////////////////////////////////////////////////////

  Object_id se_private_id() const override { return m_se_private_id; }

  void set_se_private_id(Object_id se_private_id) override {
    m_se_private_id = se_private_id;
  }

  /////////////////////////////////////////////////////////////////////////
  // Storage engine attributes
  /////////////////////////////////////////////////////////////////////////

  LEX_CSTRING engine_attribute() const override {
    return lex_cstring_handle(m_engine_attribute);
  }

  void set_engine_attribute(LEX_CSTRING a) override {
    m_engine_attribute.assign(a.str, a.length);
  }

  LEX_CSTRING secondary_engine_attribute() const override {
    return lex_cstring_handle(m_secondary_engine_attribute);
  }

  void set_secondary_engine_attribute(LEX_CSTRING a) override {
    m_secondary_engine_attribute.assign(a.str, a.length);
  }

  /////////////////////////////////////////////////////////////////////////
  // Partition type
  /////////////////////////////////////////////////////////////////////////

  enum_partition_type partition_type() const override {
    return m_partition_type;
  }

  void set_partition_type(enum_partition_type partition_type) override {
    m_partition_type = partition_type;
  }

  /////////////////////////////////////////////////////////////////////////
  // default_partitioning
  /////////////////////////////////////////////////////////////////////////

  enum_default_partitioning default_partitioning() const override {
    return m_default_partitioning;
  }

  void set_default_partitioning(
      enum_default_partitioning default_partitioning) override {
    m_default_partitioning = default_partitioning;
  }

  /////////////////////////////////////////////////////////////////////////
  // partition_expression
  /////////////////////////////////////////////////////////////////////////

  const String_type &partition_expression() const override {
    return m_partition_expression;
  }

  void set_partition_expression(
      const String_type &partition_expression) override {
    m_partition_expression = partition_expression;
  }

  /////////////////////////////////////////////////////////////////////////
  // partition_expression_utf8
  /////////////////////////////////////////////////////////////////////////

  const String_type &partition_expression_utf8() const override {
    return m_partition_expression_utf8;
  }

  void set_partition_expression_utf8(
      const String_type &partition_expression_utf8) override {
    m_partition_expression_utf8 = partition_expression_utf8;
  }

  /////////////////////////////////////////////////////////////////////////
  // subpartition_type
  /////////////////////////////////////////////////////////////////////////

  enum_subpartition_type subpartition_type() const override {
    return m_subpartition_type;
  }

  void set_subpartition_type(
      enum_subpartition_type subpartition_type) override {
    m_subpartition_type = subpartition_type;
  }

  /////////////////////////////////////////////////////////////////////////
  // default_subpartitioning
  /////////////////////////////////////////////////////////////////////////

  enum_default_partitioning default_subpartitioning() const override {
    return m_default_subpartitioning;
  }

  void set_default_subpartitioning(
      enum_default_partitioning default_subpartitioning) override {
    m_default_subpartitioning = default_subpartitioning;
  }

  /////////////////////////////////////////////////////////////////////////
  // subpartition_expression
  /////////////////////////////////////////////////////////////////////////

  const String_type &subpartition_expression() const override {
    return m_subpartition_expression;
  }

  void set_subpartition_expression(
      const String_type &subpartition_expression) override {
    m_subpartition_expression = subpartition_expression;
  }

  /////////////////////////////////////////////////////////////////////////
  // subpartition_expression_utf8
  /////////////////////////////////////////////////////////////////////////

  const String_type &subpartition_expression_utf8() const override {
    return m_subpartition_expression_utf8;
  }

  void set_subpartition_expression_utf8(
      const String_type &subpartition_expression_utf8) override {
    m_subpartition_expression_utf8 = subpartition_expression_utf8;
  }

  /////////////////////////////////////////////////////////////////////////
  // Index collection.
  /////////////////////////////////////////////////////////////////////////

  Index *add_index() override;

  Index *add_first_index() override;

  const Index_collection &indexes() const override { return m_indexes; }

  Index_collection *indexes() override { return &m_indexes; }

  const Index *get_index(Object_id index_id) const {
    return const_cast<Table_impl *>(this)->get_index(index_id);
  }

  Index *get_index(Object_id index_id);

  /////////////////////////////////////////////////////////////////////////
  // Foreign key collection.
  /////////////////////////////////////////////////////////////////////////

  Foreign_key *add_foreign_key() override;

  const Foreign_key_collection &foreign_keys() const override {
    return m_foreign_keys;
  }

  Foreign_key_collection *foreign_keys() override { return &m_foreign_keys; }

  /////////////////////////////////////////////////////////////////////////
  // Foreign key parent collection.
  /////////////////////////////////////////////////////////////////////////

  virtual Foreign_key_parent *add_foreign_key_parent();

 private:
  bool load_foreign_key_parents(Open_dictionary_tables_ctx *otx);

 public:
  bool reload_foreign_key_parents(THD *thd) override;

  const Foreign_key_parent_collection &foreign_key_parents() const override {
    return m_foreign_key_parents;
  }

  /////////////////////////////////////////////////////////////////////////
  // Partition collection.
  /////////////////////////////////////////////////////////////////////////

  Partition *add_partition() override;

  const Partition_collection &partitions() const override {
    return m_partitions;
  }

  Partition_collection *partitions() override { return &m_partitions; }

  const Partition_leaf_vector &leaf_partitions() const override {
    return m_leaf_partitions;
  }

  Partition_leaf_vector *leaf_partitions() override {
    return &m_leaf_partitions;
  }

  // non-virtual
  void add_leaf_partition(Partition *p) { m_leaf_partitions.push_back(p); }

  const Partition *get_partition(Object_id partition_id) const {
    return const_cast<Table_impl *>(this)->get_partition(partition_id);
  }

  Partition *get_partition(Object_id partition_id);

  Partition *get_partition(const String_type &name);

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
  Object_id schema_id() const override {
    return Abstract_table_impl::schema_id();
  }
  void set_schema_id(Object_id schema_id) override {
    Abstract_table_impl::set_schema_id(schema_id);
  }
  uint mysql_version_id() const override {
    return Abstract_table_impl::mysql_version_id();
  }
  const Properties &options() const override {
    return Abstract_table_impl::options();
  }
  Properties &options() override { return Abstract_table_impl::options(); }
  bool set_options(const Properties &options) override {
    return Abstract_table_impl::set_options(options);
  }
  bool set_options(const String_type &options_raw) override {
    return Abstract_table_impl::set_options(options_raw);
  }
  ulonglong created(bool convert_time) const override {
    return Abstract_table_impl::created(convert_time);
  }
  void set_created(ulonglong created) override {
    Abstract_table_impl::set_created(created);
  }
  ulonglong last_altered(bool convert_time) const override {
    return Abstract_table_impl::last_altered(convert_time);
  }
  void set_last_altered(ulonglong last_altered) override {
    Abstract_table_impl::set_last_altered(last_altered);
  }
  Column *add_column() override { return Abstract_table_impl::add_column(); }
  bool drop_column(const String_type &name) override {
    return Abstract_table_impl::drop_column(name);
  }
  const Column_collection &columns() const override {
    return Abstract_table_impl::columns();
  }
  Column_collection *columns() override {
    return Abstract_table_impl::columns();
  }
  const Column *get_column(Object_id column_id) const {
    return Abstract_table_impl::get_column(column_id);
  }
  Column *get_column(Object_id column_id) {
    return Abstract_table_impl::get_column(column_id);
  }
  const Column *get_column(const String_type &name) const override {
    return Abstract_table_impl::get_column(name);
  }
  Column *get_column(const String_type &name) {
    return Abstract_table_impl::get_column(name);
  }
  bool update_aux_key(Aux_key *key) const override {
    return Table::update_aux_key(key);
  }
  enum_hidden_type hidden() const override {
    return Abstract_table_impl::hidden();
  }
  void set_hidden(enum_hidden_type hidden) override {
    Abstract_table_impl::set_hidden(hidden);
  }

  /////////////////////////////////////////////////////////////////////////
  // Trigger collection.
  /////////////////////////////////////////////////////////////////////////

  bool has_trigger() const override { return (m_triggers.size() > 0); }

  const Trigger_collection &triggers() const override { return m_triggers; }

  Trigger_collection *triggers() override { return &m_triggers; }

  void copy_triggers(const Table *tab_obj) override;

  Trigger *add_trigger(Trigger::enum_action_timing at,
                       Trigger::enum_event_type et) override;

  const Trigger *get_trigger(const char *name) const override;

  Trigger *add_trigger_following(const Trigger *trigger,
                                 Trigger::enum_action_timing at,
                                 Trigger::enum_event_type et) override;

  Trigger *add_trigger_preceding(const Trigger *trigger,
                                 Trigger::enum_action_timing at,
                                 Trigger::enum_event_type et) override;

  void drop_trigger(const Trigger *trigger) override;

  void drop_all_triggers() override;

 private:
  uint get_max_action_order(Trigger::enum_action_timing at,
                            Trigger::enum_event_type et) const;

  void reorder_action_order(Trigger::enum_action_timing at,
                            Trigger::enum_event_type et);

  Trigger_impl *create_trigger();

 public:
  /////////////////////////////////////////////////////////////////////////
  // Check constraints.
  /////////////////////////////////////////////////////////////////////////

  Check_constraint *add_check_constraint() override;

  const Check_constraint_collection &check_constraints() const override {
    return m_check_constraints;
  }

  Check_constraint_collection *check_constraints() override {
    return &m_check_constraints;
  }

 private:
  // Fields.

  Object_id m_se_private_id;

  String_type m_engine;
  String_type m_comment;

  // Setting this to 0 means that every table will be checked by CHECK
  // TABLE FOR UPGRADE once, even if it was created in this version.
  // If we instead initialize to MYSQL_VERSION_ID, it will only run
  // CHECK TABLE FOR UPGRADE after a real upgrade.
  uint m_last_checked_for_upgrade_version_id = 0;
  Properties_impl m_se_private_data;

  // SE-specific json attributes
  dd::String_type m_engine_attribute;
  dd::String_type m_secondary_engine_attribute;

  enum_row_format m_row_format;
  bool m_is_temporary;

  // - Partitioning related fields.

  enum_partition_type m_partition_type;
  String_type m_partition_expression;
  String_type m_partition_expression_utf8;
  enum_default_partitioning m_default_partitioning;

  enum_subpartition_type m_subpartition_type;
  String_type m_subpartition_expression;
  String_type m_subpartition_expression_utf8;
  enum_default_partitioning m_default_subpartitioning;

  // References to tightly-coupled objects.

  Index_collection m_indexes;
  Foreign_key_collection m_foreign_keys;
  Foreign_key_parent_collection m_foreign_key_parents;
  Partition_collection m_partitions;
  Partition_leaf_vector m_leaf_partitions;
  Trigger_collection m_triggers;
  Check_constraint_collection m_check_constraints;

  // References to other objects.

  Object_id m_collation_id;
  Object_id m_tablespace_id;

  Table_impl(const Table_impl &src);
  Table_impl *clone() const override { return new Table_impl(*this); }

  // N.B.: returning dd::Table from this function might confuse MSVC
  // compiler thanks to diamond inheritance.
  Table_impl *clone_dropped_object_placeholder() const override {
    /*
      TODO: In future we might want to save even more memory and use separate
            placeholder class implementing dd::Table interface instead of
            Table_impl. Instances of such class can be several times smaller
            than an empty Table_impl. It might make sense to do the same for
            for some of other types as well.
    */
    Table_impl *placeholder = new Table_impl();
    placeholder->set_id(id());
    placeholder->set_schema_id(schema_id());
    placeholder->set_name(name());
    placeholder->set_engine(engine());
    placeholder->set_se_private_id(se_private_id());
    return placeholder;
  }
};

///////////////////////////////////////////////////////////////////////////

}  // namespace dd

#endif  // DD__TABLE_IMPL_INCLUDED
