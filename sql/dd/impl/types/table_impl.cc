/* Copyright (c) 2014, 2017 Oracle and/or its affiliates. All rights reserved.

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

#include "dd/impl/types/table_impl.h"

#include <string.h>
#include <sstream>

#include "current_thd.h"                             // current_thd
#include "dd/impl/object_key.h"                      // Needed for destructor
#include "dd/impl/properties_impl.h"                 // Properties_impl
#include "dd/impl/raw/raw_record.h"                  // Raw_record
#include "dd/impl/sdi_impl.h"                        // sdi read/write functions
#include "dd/impl/tables/foreign_keys.h"             // Foreign_keys
#include "dd/impl/tables/indexes.h"                  // Indexes
#include "dd/impl/tables/table_partitions.h"         // Table_partitions
#include "dd/impl/tables/tables.h"                   // Tables
#include "dd/impl/tables/triggers.h"                 // Triggers
#include "dd/impl/transaction_impl.h"                // Open_dictionary_tables_ctx
#include "dd/impl/types/foreign_key_impl.h"          // Foreign_key_impl
#include "dd/impl/types/index_impl.h"                // Index_impl
#include "dd/impl/types/partition_impl.h"            // Partition_impl
#include "dd/impl/types/trigger_impl.h"              // Trigger_impl
#include "dd/properties.h"
#include "dd/string_type.h"                          // dd::String_type
#include "dd/types/column.h"                         // Column
#include "dd/types/foreign_key.h"
#include "dd/types/index.h"
#include "dd/types/partition.h"
#include "dd/types/weak_object.h"
#include "m_string.h"
#include "my_dbug.h"
#include "mysqld_error.h"                            // ER_*
#include "my_sys.h"
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "sql_class.h"

using dd::tables::Foreign_keys;
using dd::tables::Indexes;
using dd::tables::Tables;
using dd::tables::Table_partitions;
using dd::tables::Triggers;

namespace dd {

class Sdi_rcontext;
class Sdi_wcontext;

///////////////////////////////////////////////////////////////////////////
// Table implementation.
///////////////////////////////////////////////////////////////////////////

const Object_type &Table::TYPE()
{
  static Table_type s_instance;
  return s_instance;
}

///////////////////////////////////////////////////////////////////////////
// Table_impl implementation.
///////////////////////////////////////////////////////////////////////////

Table_impl::Table_impl()
 :m_se_private_id(INVALID_OBJECT_ID),
  m_se_private_data(new Properties_impl()),
  m_row_format(RF_FIXED),
  m_partition_type(PT_NONE),
  m_default_partitioning(DP_NONE),
  m_subpartition_type(ST_NONE),
  m_default_subpartitioning(DP_NONE),
  m_indexes(),
  m_foreign_keys(),
  m_partitions(),
  m_triggers(),
  m_collation_id(INVALID_OBJECT_ID),
  m_tablespace_id(INVALID_OBJECT_ID)
{
}

Table_impl::~Table_impl()
{ }

///////////////////////////////////////////////////////////////////////////

bool Table_impl::set_se_private_data_raw(const String_type &se_private_data_raw)
{
  Properties *properties=
    Properties_impl::parse_properties(se_private_data_raw);

  if (!properties)
    return true; // Error status, current values has not changed.

  m_se_private_data.reset(properties);
  return false;
}

///////////////////////////////////////////////////////////////////////////

void Table_impl::set_se_private_data(const Properties &se_private_data)
{ m_se_private_data->assign(se_private_data); }

///////////////////////////////////////////////////////////////////////////

bool Table_impl::validate() const
{
  if (Abstract_table_impl::validate())
    return true;

  if (m_collation_id == INVALID_OBJECT_ID)
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Table_impl::OBJECT_TABLE().name().c_str(),
             "Collation ID not set.");
    return true;
  }

  if (m_engine.empty())
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Table_impl::OBJECT_TABLE().name().c_str(),
             "Engine name is not set.");
    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Table_impl::restore_children(Open_dictionary_tables_ctx *otx)
{
  // NOTE: the order of restoring collections is important because:
  //   - Index-objects reference Column-objects
  //     (thus, Column-objects must be loaded before Index-objects).
  //   - Foreign_key-objects reference both Index-objects and Column-objects.
  //     (thus, both Indexes and Columns must be loaded before FKs).
  //   - Partitions should be loaded at the end, as it refers to
  //     indexes.

  bool ret=
    Abstract_table_impl::restore_children(otx)
    ||
    m_indexes.restore_items(
      this,
      otx,
      otx->get_table<Index>(),
      Indexes::create_key_by_table_id(this->id()))
    ||
    m_foreign_keys.restore_items(
      this,
      otx,
      otx->get_table<Foreign_key>(),
      Foreign_keys::create_key_by_table_id(this->id()),
      Foreign_key_order_comparator())
    ||
    m_partitions.restore_items(
      this,
      otx,
      otx->get_table<Partition>(),
      Table_partitions::create_key_by_table_id(this->id()),
      // Sort partitions first on level and then on number.
      Partition_order_comparator())
    ||
    m_triggers.restore_items(
      this,
      otx,
      otx->get_table<Trigger>(),
      Triggers::create_key_by_table_id(this->id()),
      Trigger_order_comparator());

  if (!ret)
    fix_partitions();

  return ret;
}

///////////////////////////////////////////////////////////////////////////

bool Table_impl::store_triggers(Open_dictionary_tables_ctx *otx)
{
  /*
    There is a requirement to keep the collection items in
    following order.  The reason is,

    Suppose we are updating a dd::Table object with,
      a) We already have a trigger 't1' with ID 1.
      b) We added a new trigger 't2' added preceding to 't1'.
    We have a row for a) in (DD) disk with action_order=1.

    The expectation is that row b) should have action_order=1
    and row a) should have action_order=2.

    If we try to store row b) first with action_order=1, then
    there is possibility violating the constraint
      "UNIQUE KEY (table_id, event_type,
                   action_timing, action_order)"
    because row a) might also contain the same event_type and
    action_timing as that of b). And we would fail inserting
    row b).

    This demands us to drop all the triggers which are already
    present on disk and then store any new triggers.  This
    would not violate the above unique constraint.

    However we should avoid trying to drop triggers if no triggers
    existed before. Such an attempt will lead to index lookup which
    might cause acquisition of gap lock on index supremum in InnoDB.
    This might lead to deadlock if two independent CREATE TRIGGER
    are executed concurrently and both acquire gap locks on index
    supremum first and then try to insert their records into this gap.
  */
  bool needs_delete= m_triggers.has_removed_items();

  if (!needs_delete)
  {
    /* Check if there are any non-new Trigger objects. */
    for (const Trigger *trigger : *triggers())
    {
      if (trigger->id() != INVALID_OBJECT_ID)
      {
        needs_delete= true;
        break;
      }
    }
  }

  if (needs_delete)
  {
    if (m_triggers.drop_items(otx,
                              otx->get_table<Trigger>(),
                              Triggers::create_key_by_table_id(this->id())))
      return true;

    /*
      In-case a trigger is dropped, we need to avoid dropping it
      second time. So clear all the removed items.
    */
    m_triggers.clear_removed_items();
  }

  // Store the items.
  return m_triggers.store_items(otx);
}

///////////////////////////////////////////////////////////////////////////

bool Table_impl::store_children(Open_dictionary_tables_ctx *otx)
{
  return Abstract_table_impl::store_children(otx) ||
    // Note that indexes has to be stored first, as
    // partitions refer indexes.
    m_indexes.store_items(otx) ||
    m_foreign_keys.store_items(otx) ||
    m_partitions.store_items(otx) ||
    store_triggers(otx);
}

///////////////////////////////////////////////////////////////////////////

bool Table_impl::drop_children(Open_dictionary_tables_ctx *otx) const
{
  // Note that partition collection has to be dropped first
  // as it has foreign key to indexes.

  return
    m_triggers.drop_items(otx,
      otx->get_table<Trigger>(),
      Triggers::create_key_by_table_id(this->id()))
    ||
    m_partitions.drop_items(otx,
      otx->get_table<Partition>(),
      Table_partitions::create_key_by_table_id(this->id()))
    ||
    m_foreign_keys.drop_items(otx,
      otx->get_table<Foreign_key>(),
      Foreign_keys::create_key_by_table_id(this->id()))
    ||
    m_indexes.drop_items(otx,
      otx->get_table<Index>(),
      Indexes::create_key_by_table_id(this->id()))
    ||
    Abstract_table_impl::drop_children(otx);
}

/////////////////////////////////////////////////////////////////////////

bool Table_impl::restore_attributes(const Raw_record &r)
{
  {
    enum_table_type table_type=
      static_cast<enum_table_type>(r.read_int(Tables::FIELD_TYPE));

    if (table_type != enum_table_type::BASE_TABLE)
      return true;
  }

  if (Abstract_table_impl::restore_attributes(r))
    return true;

  m_comment=         r.read_str(Tables::FIELD_COMMENT);
  m_row_format=      (enum_row_format) r.read_int(Tables::FIELD_ROW_FORMAT);

  // Partitioning related fields (NULL -> enum value 0!)

  m_partition_type=
    (enum_partition_type) r.read_int(Tables::FIELD_PARTITION_TYPE, 0);

  m_default_partitioning=
    (enum_default_partitioning) r.read_int(Tables::FIELD_DEFAULT_PARTITIONING,
                                           0);

  m_subpartition_type=
    (enum_subpartition_type) r.read_int(Tables::FIELD_SUBPARTITION_TYPE, 0);

  m_default_subpartitioning=
    (enum_default_partitioning)
      r.read_int(Tables::FIELD_DEFAULT_SUBPARTITIONING, 0);

  // Special cases dealing with NULL values for nullable fields

  m_se_private_id= dd::tables::Tables::read_se_private_id(r);

  m_collation_id= r.read_ref_id(Tables::FIELD_COLLATION_ID);
  m_tablespace_id= r.read_ref_id(Tables::FIELD_TABLESPACE_ID);

  set_se_private_data_raw(r.read_str(Tables::FIELD_SE_PRIVATE_DATA, ""));

  m_engine= r.read_str(Tables::FIELD_ENGINE);

  m_partition_expression= r.read_str(Tables::FIELD_PARTITION_EXPRESSION, "");
  m_subpartition_expression= r.read_str(Tables::FIELD_SUBPARTITION_EXPRESSION, "");

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Table_impl::store_attributes(Raw_record *r)
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
  //   - Store NULL in partition type if not set.
  //   - Store NULL in partition expression if not set.
  //   - Store NULL in default partitioning if not set.
  //   - Store NULL in subpartition type if not set.
  //   - Store NULL in subpartition expression if not set.
  //   - Store NULL in default subpartitioning if not set.
  //

  // Store field values
  return
    Abstract_table_impl::store_attributes(r) ||
    r->store(Tables::FIELD_ENGINE, m_engine) ||
    r->store_ref_id(Tables::FIELD_COLLATION_ID, m_collation_id) ||
    r->store(Tables::FIELD_COMMENT, m_comment) ||
    r->store(Tables::FIELD_SE_PRIVATE_DATA, *m_se_private_data) ||
    r->store(Tables::FIELD_SE_PRIVATE_ID,
             m_se_private_id,
             m_se_private_id == (Object_id) -1) ||
    r->store(Tables::FIELD_ROW_FORMAT, m_row_format) ||
    r->store_ref_id(Tables::FIELD_TABLESPACE_ID, m_tablespace_id) ||
    r->store(Tables::FIELD_PARTITION_TYPE,
             m_partition_type,
             m_partition_type == PT_NONE) ||
    r->store(Tables::FIELD_PARTITION_EXPRESSION,
             m_partition_expression,
             m_partition_expression.empty()) ||
    r->store(Tables::FIELD_DEFAULT_PARTITIONING,
             m_default_partitioning,
             m_default_partitioning == DP_NONE) ||
    r->store(Tables::FIELD_SUBPARTITION_TYPE,
             m_subpartition_type,
             m_subpartition_type == ST_NONE) ||
    r->store(Tables::FIELD_SUBPARTITION_EXPRESSION,
             m_subpartition_expression,
             m_subpartition_expression.empty()) ||
    r->store(Tables::FIELD_DEFAULT_SUBPARTITIONING,
             m_default_subpartitioning,
             m_default_subpartitioning == DP_NONE);
}

///////////////////////////////////////////////////////////////////////////

void
Table_impl::serialize(Sdi_wcontext *wctx, Sdi_writer *w) const
{
  w->StartObject();
  Abstract_table_impl::serialize(wctx, w);
  write(w, m_se_private_id, STRING_WITH_LEN("se_private_id"));
  write(w, m_engine, STRING_WITH_LEN("engine"));
  write(w, m_comment, STRING_WITH_LEN("comment"));
  write_properties(w, m_se_private_data, STRING_WITH_LEN("se_private_data"));
  write_enum(w, m_row_format, STRING_WITH_LEN("row_format"));
  write_enum(w, m_partition_type, STRING_WITH_LEN("partition_type"));
  write(w, m_partition_expression, STRING_WITH_LEN("partition_expression"));
  write_enum(w, m_default_partitioning, STRING_WITH_LEN("default_partitioning"));
  write_enum(w, m_subpartition_type, STRING_WITH_LEN("subpartition_type"));
  write(w, m_subpartition_expression, STRING_WITH_LEN("subpartition_expression"));
  write_enum(w, m_default_subpartitioning, STRING_WITH_LEN("default_subpartitioning"));
  serialize_each(wctx, w, m_indexes, STRING_WITH_LEN("indexes"));
  serialize_each(wctx, w, m_foreign_keys, STRING_WITH_LEN("foreign_keys"));
  serialize_each(wctx, w, m_partitions, STRING_WITH_LEN("partitions"));
  write(w, m_collation_id, STRING_WITH_LEN("collation_id"));
  serialize_tablespace_ref(wctx, w, m_tablespace_id,
                           STRING_WITH_LEN("tablespace_ref"));
  w->EndObject();
}

///////////////////////////////////////////////////////////////////////////

bool
Table_impl::deserialize(Sdi_rcontext *rctx, const RJ_Value &val)
{
  Abstract_table_impl::deserialize(rctx, val);
  read(&m_se_private_id, val, "se_private_id");
  read(&m_engine, val, "engine");
  read(&m_comment, val, "comment");
  read_properties(&m_se_private_data, val, "se_private_data");
  read_enum(&m_row_format, val, "row_format");
  read_enum(&m_partition_type, val, "partition_type");
  read(&m_partition_expression, val, "partition_expression");
  read_enum(&m_default_partitioning, val, "default_partitioning");
  read_enum(&m_subpartition_type, val, "subpartition_type");
  read(&m_subpartition_expression, val, "subpartition_expression");
  read_enum(&m_default_subpartitioning, val, "default_subpartitioning");

  // Note! Deserialization of ordinal position cross-referenced
  // objects (i.e. Index and Column) must happen before deserializing
  // objects which refrence these objects:
  // Foreign_key_element -> Column,
  // Foreign_key         -> Index,
  // Index_element       -> Column,
  // Partition_index     -> Index
  // Otherwise the cross-references will not be deserialized correctly
  // (as we don't know the address of the referenced Column or Index
  // object).

  deserialize_each(rctx, [this] () { return add_index(); }, val,
                   "indexes");

  deserialize_each(rctx, [this] () { return add_foreign_key(); },
                   val, "foreign_keys");
  deserialize_each(rctx, [this] () { return add_partition(); }, val,
                   "partitions");
  fix_partitions();
  read(&m_collation_id, val, "collation_id");
  return deserialize_tablespace_ref(rctx, &m_tablespace_id, val, "tablespace_id");
}

///////////////////////////////////////////////////////////////////////////

void Table_impl::debug_print(String_type &outb) const
{
  String_type s;
  Abstract_table_impl::debug_print(s);

  dd::Stringstream_type ss;
  ss
    << "TABLE OBJECT: { "
    << s
    << "m_engine: " << m_engine << "; "
    << "m_collation: {OID: " << m_collation_id << "}; "
    << "m_comment: " << m_comment << "; "
    << "m_se_private_data " << m_se_private_data->raw_string() << "; "
    << "m_se_private_id: {OID: " << m_se_private_id << "}; "
    << "m_row_format: " << m_row_format << "; "
    << "m_tablespace: {OID: " << m_tablespace_id << "}; "
    << "m_partition_type " << m_partition_type << "; "
    << "m_default_partitioning " << m_default_partitioning << "; "
    << "m_partition_expression " << m_partition_expression << "; "
    << "m_subpartition_type " << m_subpartition_type << "; "
    << "m_default_subpartitioning " << m_default_subpartitioning << "; "
    << "m_subpartition_expression " << m_subpartition_expression << "; "
    << "m_partitions: " << m_partitions.size() << " [ ";

  {
    for (const Partition *i : partitions())
    {
      String_type s;
      i->debug_print(s);
      ss << s << " | ";
    }
  }

  ss << "] m_indexes: " << m_indexes.size() << " [ ";

  {
    for (const Index *i : indexes())
    {
      String_type s;
      i->debug_print(s);
      ss << s << " | ";
    }
  }

  ss << "] m_foreign_keys: " << m_foreign_keys.size() << " [ ";

  {
    for (const Foreign_key *fk : foreign_keys())
    {
      String_type s;
      fk->debug_print(s);
      ss << s << " | ";
    }
  }

  ss << "] m_triggers: " << m_triggers.size() << " [ ";

  {
    for (const Trigger *trig : triggers())
    {
      String_type s;
      trig->debug_print(s);
      ss << s << " | ";
    }
  }
  ss << "] ";

  ss << " }";

  outb= ss.str();
}

///////////////////////////////////////////////////////////////////////////
// Index collection.
///////////////////////////////////////////////////////////////////////////

Index *Table_impl::add_index()
{
  Index_impl *i= new (std::nothrow) Index_impl(this);
  m_indexes.push_back(i);
  return i;
}

///////////////////////////////////////////////////////////////////////////

Index *Table_impl::add_first_index()
{
  Index_impl *i= new (std::nothrow) Index_impl(this);
  m_indexes.push_front(i);
  return i;
}

///////////////////////////////////////////////////////////////////////////

Index *Table_impl::get_index(Object_id index_id)
{
  for (Index *i : m_indexes)
  {
    if (i->id() == index_id)
      return i;
  }

  return NULL;
}

///////////////////////////////////////////////////////////////////////////
// Foreign key collection.
///////////////////////////////////////////////////////////////////////////

Foreign_key *Table_impl::add_foreign_key()
{
  Foreign_key_impl *fk= new (std::nothrow) Foreign_key_impl(this);
  m_foreign_keys.push_back(fk);
  return fk;
}

///////////////////////////////////////////////////////////////////////////
// Partition collection.
///////////////////////////////////////////////////////////////////////////

Partition *Table_impl::add_partition()
{
  Partition_impl *i= new (std::nothrow) Partition_impl(this);
  m_partitions.push_back(i);
  return i;
}

///////////////////////////////////////////////////////////////////////////

Partition *Table_impl::get_partition(Object_id partition_id)
{
  for (Partition *i : m_partitions)
  {
    if (i->id() == partition_id)
      return i;
  }

  return NULL;
}


///////////////////////////////////////////////////////////////////////////
// Trigger collection.
///////////////////////////////////////////////////////////////////////////

uint Table_impl::get_max_action_order(Trigger::enum_action_timing at,
                                      Trigger::enum_event_type et) const
{
  uint max_order= 0;
  for (const Trigger *trig : triggers())
  {
    if (trig->action_timing() == at &&
        trig->event_type() == et)
      max_order++;
  }

  return max_order;
}

///////////////////////////////////////////////////////////////////////////

void Table_impl::reorder_action_order(Trigger::enum_action_timing at,
                                      Trigger::enum_event_type et) {

  uint new_order= 1;
  for (Trigger *trigger : *triggers())
  {
    if (trigger->action_timing() == at &&
        trigger->event_type() == et)
      trigger->set_action_order(new_order++);
  }
}

///////////////////////////////////////////////////////////////////////////

Trigger_impl *Table_impl::create_trigger() {

  Trigger_impl *trigger= new (std::nothrow) Trigger_impl(this);
  if (trigger == nullptr)
    return nullptr;

  THD *thd= current_thd;
  trigger->set_created(thd->query_start_timeval_trunc(2));
  trigger->set_last_altered(thd->query_start_timeval_trunc(2));

  return trigger;
}

///////////////////////////////////////////////////////////////////////////

Trigger *Table_impl::add_trigger(Trigger::enum_action_timing at,
                                 Trigger::enum_event_type et) {

  Trigger_impl *trigger= create_trigger();
  if (trigger == nullptr)
    return nullptr;

  m_triggers.push_back(trigger);
  trigger->set_action_timing(at);
  trigger->set_event_type(et);
  trigger->set_action_order(get_max_action_order(at, et));

  return trigger;
}

///////////////////////////////////////////////////////////////////////////

const Trigger *Table_impl::get_trigger(const char *name) const
{
  for (const Trigger *trigger : triggers())
  {
    if (!strcmp(name, trigger->name().c_str()))
      return trigger;
  }

  return nullptr;
}

///////////////////////////////////////////////////////////////////////////

Trigger *Table_impl::add_trigger_following(const Trigger *trigger,
                                           Trigger::enum_action_timing at,
                                           Trigger::enum_event_type et)
{
  DBUG_ASSERT(trigger != nullptr &&
              trigger->action_timing() == at &&
              trigger->event_type() == et);

  int new_pos= dynamic_cast<const Trigger_impl*>(trigger)->ordinal_position();

  // Allocate new Trigger object.
  Trigger_impl *new_trigger= create_trigger();
  if (new_trigger == nullptr)
    return nullptr;

  m_triggers.push_back(new_trigger);
  new_trigger->set_action_timing(at);
  new_trigger->set_event_type(et);

  int last_pos= dynamic_cast<Trigger_impl*>(new_trigger)->ordinal_position();
  if (last_pos > (new_pos + 1))
    m_triggers.move(last_pos - 1, new_pos);

  reorder_action_order(at, et);

  return new_trigger;
}

///////////////////////////////////////////////////////////////////////////

Trigger *Table_impl::add_trigger_preceding(const Trigger *trigger,
                                           Trigger::enum_action_timing at,
                                           Trigger::enum_event_type et)
{
  DBUG_ASSERT(trigger != nullptr &&
              trigger->action_timing() == at
              && trigger->event_type() == et);

  Trigger_impl *new_trigger= create_trigger();
  if (new_trigger == nullptr)
    return nullptr;

  int new_pos= dynamic_cast<const Trigger_impl*>(trigger)->ordinal_position();
  m_triggers.push_back(new_trigger);
  new_trigger->set_action_timing(at);
  new_trigger->set_event_type(et);

  int last_pos= dynamic_cast<Trigger_impl*>(new_trigger)->ordinal_position();
  m_triggers.move(last_pos-1, new_pos-1);

  reorder_action_order(at, et);

  return new_trigger;
}

///////////////////////////////////////////////////////////////////////////

void Table_impl::copy_triggers(const Table *tab_obj)
{
  DBUG_ASSERT(tab_obj != nullptr);

  for (const Trigger *trig : tab_obj->triggers())
  {
    /*
      Reset the trigger primary key ID, so that a new row is
      created for them, when the object is stored. Following is
      the issue if we don't do that.

      * When the triggers are copied by dd::Table::copy_triggers(),
        it retained the old trigger ID's. This is fine in theory
        to re-use ID. But see below points.

      * thd->dd_client()->update() updates the dd::Table object which
        contains the moved triggers. The DD framework would insert
        these triggers with same old trigger ID in mysql.triggers.id.
        This too is fine.

      * After inserting a row, we set dd::Trigger_impl::m_id
        only if a new id m_table->file->insert_id_for_cur_row was
        generated. The problem here is that there was no new row ID
        generated as we did retain old mysql.triggers.id. Hence we
        end-up marking the dd::Trigger_impl::m_id as INVALID_OBJECT_ID.
        Note that the value stored in DD is now difference than the
        value in in-memory dd::Trigger_impl object.

      * Later if the same object is updated (may be rename operation)
        then as the dd::Trigger_impl::m_id is INVALID_OBJECT_ID, we
        end-up creating a duplicate row which already exists.

      So, It is not necessary to retain the old trigger ID's, the
      dd::Table::copy_triggers() API now sets the ID's of cloned
      trigger objects to INVALID_OBJECT_ID. This will work fine as the
      m_table->file->insert_id_for_cur_row gets generated as expected
      and the trigger metadata on DD table mysql.triggers and in-memory
      DD object dd::Trigger_impl would both be same.
    */
    Trigger_impl *new_trigger=
      new Trigger_impl(*dynamic_cast<const Trigger_impl*>(trig), this);
    DBUG_ASSERT(new_trigger != nullptr);

    new_trigger->set_id(INVALID_OBJECT_ID);

    m_triggers.push_back(new_trigger);
  }
}

///////////////////////////////////////////////////////////////////////////

void Table_impl::drop_all_triggers()
{
  m_triggers.remove_all();
}

///////////////////////////////////////////////////////////////////////////

void Table_impl::drop_trigger(const Trigger *trigger)
{
  DBUG_ASSERT(trigger != nullptr);
  dd::Trigger::enum_action_timing at= trigger->action_timing();
  dd::Trigger::enum_event_type et= trigger->event_type();

  m_triggers.remove(dynamic_cast<Trigger_impl*>(const_cast<Trigger*>(trigger)));

  reorder_action_order(at, et);
}

///////////////////////////////////////////////////////////////////////////

Partition *Table_impl::get_partition(const String_type &name)
{
  for (Partition *i : m_partitions)
  {
    if (i->name() == name)
      return i;
  }

  return NULL;
}

///////////////////////////////////////////////////////////////////////////

void Table_impl::fix_partitions()
{
  size_t part_num= 0;
  size_t subpart_num= 0;
  size_t subpart_processed= 0;

  Partition_collection::iterator part_it= m_partitions.begin();
  for (Partition *part : m_partitions)
  {
    if (part->level() == 0)
      ++part_num;
    else
    {
      if (!subpart_num)
      {
        // First subpartition.
        subpart_num= (m_partitions.size() - part_num) / part_num;
      }
      part->set_parent(*part_it);
      ++subpart_processed;
      if (subpart_processed % subpart_num == 0)
        ++part_it;
    }
  }
}

///////////////////////////////////////////////////////////////////////////

bool Table::update_aux_key(aux_key_type *key,
                           const String_type &engine,
                           Object_id se_private_id)
{
  if (se_private_id != INVALID_OBJECT_ID)
    return Tables::update_aux_key(key, engine, se_private_id);

  return true;
}

////////////////////////////////////////////////////////////////////////////
// Table_type implementation.
///////////////////////////////////////////////////////////////////////////

void Table_type::register_tables(Open_dictionary_tables_ctx *otx) const
{
  otx->add_table<Tables>();

  otx->register_tables<Column>();
  otx->register_tables<Index>();
  otx->register_tables<Foreign_key>();
  otx->register_tables<Partition>();
  otx->register_tables<Trigger>();
}

///////////////////////////////////////////////////////////////////////////

Table_impl::Table_impl(const Table_impl &src)
  : Weak_object(src), Abstract_table_impl(src),
    m_se_private_id(src.m_se_private_id),
    m_engine(src.m_engine),
    m_comment(src.m_comment),
    m_se_private_data(Properties_impl::
                      parse_properties(src.m_se_private_data->raw_string())),
    m_row_format(src.m_row_format),
    m_partition_type(src.m_partition_type),
    m_partition_expression(src.m_partition_expression),
    m_default_partitioning(src.m_default_partitioning),
    m_subpartition_type(src.m_subpartition_type),
    m_subpartition_expression(src.m_subpartition_expression),
    m_default_subpartitioning(src.m_default_subpartitioning),
    m_indexes(),
    m_foreign_keys(),
    m_partitions(),
    m_triggers(),
    m_collation_id(src.m_collation_id), m_tablespace_id(src.m_tablespace_id)
{
  m_indexes.deep_copy(src.m_indexes, this);
  m_foreign_keys.deep_copy(src.m_foreign_keys, this);
  m_partitions.deep_copy(src.m_partitions, this);
  m_triggers.deep_copy(src.m_triggers, this);
}
}
