/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef DD__TABLE_IMPL_INCLUDED
#define DD__TABLE_IMPL_INCLUDED

#include <sys/types.h>
#include <memory>
#include <new>
#include <string>

#include "dd/impl/raw/raw_record.h"
#include "dd/impl/types/abstract_table_impl.h" // dd::Abstract_table_impl
#include "dd/impl/types/entity_object_impl.h"
#include "dd/impl/types/weak_object_impl.h"
#include "dd/object_id.h"
#include "dd/sdi_fwd.h"
#include "dd/types/abstract_table.h"
#include "dd/types/entity_object_table.h"      // dd::Entity_object_table
#include "dd/types/foreign_key.h"              // dd::Foreign_key
#include "dd/types/index.h"                    // dd::Index
#include "dd/types/object_type.h"
#include "dd/types/partition.h"                // dd::Partition
#include "dd/types/table.h"                    // dd:Table
#include "dd/types/trigger.h"                  // dd::Trigger
#include "my_inttypes.h"

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Column;
class Foreign_key;
class Index;
class Open_dictionary_tables_ctx;
class Partition;
class Properties;
class Sdi_rcontext;
class Sdi_wcontext;
class Trigger_impl;
class Weak_object;

class Table_impl : public Abstract_table_impl,
                   virtual public Table
{
public:
  Table_impl();

  virtual ~Table_impl();

public:
  /////////////////////////////////////////////////////////////////////////
  // enum_table_type.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_table_type type() const
  { return enum_table_type::BASE_TABLE; }

public:
  virtual const Object_table &object_table() const
  { return Table::OBJECT_TABLE(); }

  virtual bool validate() const;

  virtual bool restore_children(Open_dictionary_tables_ctx *otx);

  virtual bool store_children(Open_dictionary_tables_ctx *otx);

  virtual bool drop_children(Open_dictionary_tables_ctx *otx) const;

  virtual bool restore_attributes(const Raw_record &r);

  virtual bool store_attributes(Raw_record *r);

  void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const;

  bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val);

  virtual void debug_print(String_type &outb) const;

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
  // collation.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id collation_id() const
  { return m_collation_id; }

  virtual void set_collation_id(Object_id collation_id)
  { m_collation_id= collation_id; }

  /////////////////////////////////////////////////////////////////////////
  // tablespace.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id tablespace_id() const
  { return m_tablespace_id; }

  virtual void set_tablespace_id(Object_id tablespace_id)
  { m_tablespace_id= tablespace_id; }

  /////////////////////////////////////////////////////////////////////////
  // engine.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &engine() const
  { return m_engine; }

  virtual void set_engine(const String_type &engine)
  { m_engine= engine; }

  /////////////////////////////////////////////////////////////////////////
  // row_format
  /////////////////////////////////////////////////////////////////////////

  virtual enum_row_format row_format() const
  { return m_row_format; }

  virtual void set_row_format(enum_row_format row_format)
  { m_row_format= row_format; }

  /////////////////////////////////////////////////////////////////////////
  // comment
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &comment() const
  { return m_comment; }

  virtual void set_comment(const String_type &comment)
  { m_comment= comment; }

  /////////////////////////////////////////////////////////////////////////
  //se_private_data.
  /////////////////////////////////////////////////////////////////////////

  virtual const Properties &se_private_data() const
  { return *m_se_private_data; }

  virtual Properties &se_private_data()
  { return *m_se_private_data; }

  virtual bool set_se_private_data_raw(const String_type &se_private_data_raw);
  virtual void set_se_private_data(const Properties &se_private_data);

  /////////////////////////////////////////////////////////////////////////
  //se_private_id.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id se_private_id() const
  { return m_se_private_id; }

  virtual void set_se_private_id(Object_id se_private_id)
  { m_se_private_id= se_private_id; }

  /////////////////////////////////////////////////////////////////////////
  // Partition type
  /////////////////////////////////////////////////////////////////////////

  virtual enum_partition_type partition_type() const
  { return m_partition_type; }

  virtual void set_partition_type(
    enum_partition_type partition_type)
  { m_partition_type= partition_type; }

  /////////////////////////////////////////////////////////////////////////
  // default_partitioning
  /////////////////////////////////////////////////////////////////////////

  virtual enum_default_partitioning default_partitioning() const
  { return m_default_partitioning; }

  virtual void set_default_partitioning(
    enum_default_partitioning default_partitioning)
  { m_default_partitioning= default_partitioning; }

  /////////////////////////////////////////////////////////////////////////
  // partition_expression
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &partition_expression() const
  { return m_partition_expression; }

  virtual void set_partition_expression(
    const String_type &partition_expression)
  { m_partition_expression= partition_expression; }

  /////////////////////////////////////////////////////////////////////////
  // subpartition_type
  /////////////////////////////////////////////////////////////////////////

  virtual enum_subpartition_type subpartition_type() const
  { return m_subpartition_type; }

  virtual void set_subpartition_type(
    enum_subpartition_type subpartition_type)
  { m_subpartition_type= subpartition_type; }

  /////////////////////////////////////////////////////////////////////////
  // default_subpartitioning
  /////////////////////////////////////////////////////////////////////////

  virtual enum_default_partitioning default_subpartitioning() const
  { return m_default_subpartitioning; }

  virtual void set_default_subpartitioning(
    enum_default_partitioning default_subpartitioning)
  { m_default_subpartitioning= default_subpartitioning; }

  /////////////////////////////////////////////////////////////////////////
  // subpartition_expression
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &subpartition_expression() const
  { return m_subpartition_expression; }

  virtual void set_subpartition_expression(
    const String_type &subpartition_expression)
  { m_subpartition_expression= subpartition_expression; }

  /////////////////////////////////////////////////////////////////////////
  // Index collection.
  /////////////////////////////////////////////////////////////////////////

  virtual Index *add_index();

  virtual Index *add_first_index();

  virtual const Index_collection &indexes() const
  { return m_indexes; }

  virtual Index_collection *indexes()
  { return &m_indexes; }

  const Index *get_index(Object_id index_id) const
  { return const_cast<Table_impl *> (this)->get_index(index_id); }

  Index *get_index(Object_id index_id);

  /////////////////////////////////////////////////////////////////////////
  // Foreign key collection.
  /////////////////////////////////////////////////////////////////////////

  virtual Foreign_key *add_foreign_key();

  virtual const Foreign_key_collection &foreign_keys() const
  { return m_foreign_keys; }

  virtual Foreign_key_collection *foreign_keys()
  { return &m_foreign_keys; }

  /////////////////////////////////////////////////////////////////////////
  // Partition collection.
  /////////////////////////////////////////////////////////////////////////

  virtual Partition *add_partition();

  virtual const Partition_collection &partitions() const
  { return m_partitions; }

  virtual Partition_collection *partitions()
  { return &m_partitions; }

  const Partition *get_partition(Object_id partition_id) const
  { return const_cast<Table_impl *> (this)->get_partition(partition_id); }

  Partition *get_partition(Object_id partition_id);

  Partition *get_partition(const String_type &name);

  /** Find and set parent partitions for subpartitions. */
  virtual void fix_partitions();


  // Fix "inherits ... via dominance" warnings
  virtual Entity_object_impl *impl()
  { return Entity_object_impl::impl(); }
  virtual const Entity_object_impl *impl() const
  { return Entity_object_impl::impl(); }
  virtual Object_id id() const
  { return Entity_object_impl::id(); }
  virtual bool is_persistent() const
  { return Entity_object_impl::is_persistent(); }
  virtual const String_type &name() const
  { return Entity_object_impl::name(); }
  virtual void set_name(const String_type &name)
  { Entity_object_impl::set_name(name); }
  virtual Object_id schema_id() const
  { return Abstract_table_impl::schema_id(); }
  virtual void set_schema_id(Object_id schema_id)
  { Abstract_table_impl::set_schema_id(schema_id); }
  virtual uint mysql_version_id() const
  { return Abstract_table_impl::mysql_version_id(); }
  virtual const Properties &options() const
  { return Abstract_table_impl::options(); }
  virtual Properties &options()
  { return Abstract_table_impl::options(); }
  virtual bool set_options_raw(const String_type &options_raw)
  { return Abstract_table_impl::set_options_raw(options_raw); }
  virtual ulonglong created() const
  { return Abstract_table_impl::created(); }
  virtual void set_created(ulonglong created)
  { Abstract_table_impl::set_created(created); }
  virtual ulonglong last_altered() const
  { return Abstract_table_impl::last_altered(); }
  virtual void set_last_altered(ulonglong last_altered)
  { Abstract_table_impl::set_last_altered(last_altered); }
  virtual Column *add_column()
  { return Abstract_table_impl::add_column(); }
  virtual const Column_collection &columns() const
  { return Abstract_table_impl::columns(); }
  virtual Column_collection *columns()
  { return Abstract_table_impl::columns(); }
  const Column *get_column(Object_id column_id) const
  { return Abstract_table_impl::get_column(column_id); }
  Column *get_column(Object_id column_id)
  { return Abstract_table_impl::get_column(column_id); }
  const Column *get_column(const String_type name) const
  { return Abstract_table_impl::get_column(name); }
  Column *get_column(const String_type name)
  { return Abstract_table_impl::get_column(name); }
  virtual bool update_aux_key(aux_key_type *key) const
  { return Table::update_aux_key(key); }
  virtual enum_hidden_type hidden() const
  { return Abstract_table_impl::hidden(); }
  virtual void set_hidden(enum_hidden_type hidden)
  { Abstract_table_impl::set_hidden(hidden); }

  /////////////////////////////////////////////////////////////////////////
  // Trigger collection.
  /////////////////////////////////////////////////////////////////////////

  virtual bool has_trigger() const
  {
    return (m_triggers.size() > 0);
  }

  virtual const Trigger_collection &triggers() const
  { return m_triggers; }

  virtual Trigger_collection *triggers()
  { return &m_triggers; }

  virtual void copy_triggers(const Table *tab_obj);

  virtual Trigger *add_trigger(Trigger::enum_action_timing at,
                               Trigger::enum_event_type et);

  virtual const Trigger *get_trigger(const char *name) const;

  virtual Trigger *add_trigger_following(const Trigger *trigger,
                                         Trigger::enum_action_timing at,
                                         Trigger::enum_event_type et);

  virtual Trigger *add_trigger_preceding(const Trigger *trigger,
                                         Trigger::enum_action_timing at,
                                         Trigger::enum_event_type et);

  virtual void drop_trigger(const Trigger *trigger);

  virtual void drop_all_triggers();

private:

  uint get_max_action_order(Trigger::enum_action_timing at,
                            Trigger::enum_event_type et) const;

  void reorder_action_order(Trigger::enum_action_timing at,
                            Trigger::enum_event_type et);

  Trigger_impl *create_trigger();

private:
  // Fields.

  Object_id m_se_private_id;

  String_type m_engine;
  String_type m_comment;
  std::unique_ptr<Properties> m_se_private_data;
  enum_row_format m_row_format;

  // - Partitioning related fields.

  enum_partition_type           m_partition_type;
  String_type                   m_partition_expression;
  enum_default_partitioning     m_default_partitioning;

  enum_subpartition_type        m_subpartition_type;
  String_type                   m_subpartition_expression;
  enum_default_partitioning     m_default_subpartitioning;

  // References to tightly-coupled objects.

  Index_collection m_indexes;
  Foreign_key_collection m_foreign_keys;
  Partition_collection m_partitions;
  Trigger_collection m_triggers;

  // References to other objects.

  Object_id m_collation_id;
  Object_id m_tablespace_id;

  Table_impl(const Table_impl &src);
  Table_impl *clone() const
  {
    return new Table_impl(*this);
  }
};

///////////////////////////////////////////////////////////////////////////

class Table_type : public Object_type
{
public:
  virtual void register_tables(Open_dictionary_tables_ctx *otx) const;

  virtual Weak_object *create_object() const
  { return new (std::nothrow) Table_impl(); }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__TABLE_IMPL_INCLUDED
