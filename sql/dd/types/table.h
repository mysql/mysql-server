/* Copyright (c) 2014, 2017 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free Software
   Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef DD__TABLE_INCLUDED
#define DD__TABLE_INCLUDED

#include "my_global.h"
#include "prealloced_array.h"

#include "dd/types/abstract_table.h"   // dd::Abstract_table
#include "dd/types/trigger.h"          // dd::Trigger::enum_*
#include "dd/sdi_fwd.h"                // Sdi_wcontext

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Foreign_key;
class Index;
class Object_type;
class Partition;
class Trigger;

///////////////////////////////////////////////////////////////////////////

class Table : virtual public Abstract_table
{
public:
  static const Object_type &TYPE();
  typedef Collection<Index*> Index_collection;
  typedef Collection<Foreign_key*> Foreign_key_collection;
  typedef Collection<Partition*> Partition_collection;
  typedef Collection<Trigger*> Trigger_collection;

  // We need a set of functions to update a preallocated se private id key,
  // which requires special handling for table objects.
  virtual bool update_aux_key(aux_key_type *key) const
  { return update_aux_key(key, engine(), se_private_id()); }

  static bool update_aux_key(aux_key_type *key,
                             const String_type &engine,
                             Object_id se_private_id);

public:
  virtual ~Table()
  { };

public:
  enum enum_row_format
  {
    RF_FIXED= 1,
    RF_DYNAMIC,
    RF_COMPRESSED,
    RF_REDUNDANT,
    RF_COMPACT,
    RF_PAGED
  };

  /* Keep in sync with subpartition type for forward compatibility.*/
  enum enum_partition_type
  {
    PT_NONE= 0,
    PT_HASH,
    PT_KEY_51,
    PT_KEY_55,
    PT_LINEAR_HASH,
    PT_LINEAR_KEY_51,
    PT_LINEAR_KEY_55,
    PT_RANGE,
    PT_LIST,
    PT_RANGE_COLUMNS,
    PT_LIST_COLUMNS,
    PT_AUTO,
    PT_AUTO_LINEAR,
  };

  enum enum_subpartition_type
  {
    ST_NONE= 0,
    ST_HASH,
    ST_KEY_51,
    ST_KEY_55,
    ST_LINEAR_HASH,
    ST_LINEAR_KEY_51,
    ST_LINEAR_KEY_55
  };

  /* Also used for default subpartitioning. */
  enum enum_default_partitioning
  {
    DP_NONE= 0,
    DP_NO,
    DP_YES,
    DP_NUMBER
  };

public:
  /////////////////////////////////////////////////////////////////////////
  //collation.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id collation_id() const = 0;
  virtual void set_collation_id(Object_id collation_id) = 0;

  /////////////////////////////////////////////////////////////////////////
  // tablespace.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id tablespace_id() const = 0;
  virtual void set_tablespace_id(Object_id tablespace_id) = 0;

  /////////////////////////////////////////////////////////////////////////
  // engine.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &engine() const = 0;
  virtual void set_engine(const String_type &engine) = 0;

  /////////////////////////////////////////////////////////////////////////
  // row_format
  /////////////////////////////////////////////////////////////////////////
  virtual enum_row_format row_format() const = 0;
  virtual void set_row_format(enum_row_format row_format) = 0;

  /////////////////////////////////////////////////////////////////////////
  // comment
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &comment() const = 0;
  virtual void set_comment(const String_type &comment) = 0;

  /////////////////////////////////////////////////////////////////////////
  // se_private_data.
  /////////////////////////////////////////////////////////////////////////

  virtual const Properties &se_private_data() const = 0;

  virtual Properties &se_private_data() = 0;
  virtual bool set_se_private_data_raw(const String_type &se_private_data_raw) = 0;
  virtual void set_se_private_data(const Properties &se_private_data)= 0;

  /////////////////////////////////////////////////////////////////////////
  // se_private_id.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id se_private_id() const = 0;
  virtual void set_se_private_id(Object_id se_private_id) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Partition related.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_partition_type partition_type() const = 0;
  virtual void set_partition_type(enum_partition_type partition_type) = 0;

  virtual enum_default_partitioning default_partitioning() const = 0;
  virtual void set_default_partitioning(
    enum_default_partitioning default_partitioning) = 0;

  virtual const String_type &partition_expression() const = 0;
  virtual void set_partition_expression(
    const String_type &partition_expression) = 0;

  virtual enum_subpartition_type subpartition_type() const = 0;
  virtual void set_subpartition_type(
    enum_subpartition_type subpartition_type) = 0;

  virtual enum_default_partitioning default_subpartitioning() const = 0;
  virtual void set_default_subpartitioning(
    enum_default_partitioning default_subpartitioning) = 0;

  virtual const String_type &subpartition_expression() const = 0;
  virtual void set_subpartition_expression(
    const String_type &subpartition_expression) = 0;

  /** Dummy method to be able to use Partition and Table interchangeably
  in templates. */
  const Table &table() const
  { return *this; }
  Table &table()
  { return *this; }

  /////////////////////////////////////////////////////////////////////////
  //Index collection.
  /////////////////////////////////////////////////////////////////////////

  virtual Index *add_index() = 0;

  virtual Index *add_first_index() = 0;

  virtual const Index_collection &indexes() const = 0;

  virtual Index_collection *indexes() = 0;

  /////////////////////////////////////////////////////////////////////////
  // Foreign key collection.
  /////////////////////////////////////////////////////////////////////////

  virtual Foreign_key *add_foreign_key() = 0;

  virtual const Foreign_key_collection &foreign_keys() const = 0;

  /////////////////////////////////////////////////////////////////////////
  // Partition collection.
  /////////////////////////////////////////////////////////////////////////

  virtual Partition *add_partition() = 0;

  virtual const Partition_collection &partitions() const = 0;

  virtual Partition_collection *partitions() = 0;

  /**
    Find and set parent partitions for subpartitions.

    TODO: Adjust API and code to avoid need for this method.
  */
  virtual void fix_partitions() = 0;

  /////////////////////////////////////////////////////////////////////////
  // Trigger collection.
  /////////////////////////////////////////////////////////////////////////

  /**
    Check if table has any triggers.

    @return true  - if there are triggers on the table installed.
    @return false - if not.
  */

  virtual bool has_trigger() const = 0;


  /**
    Get const reference to Trigger_collection.

    @return Trigger_collection& - Const reference to a collection of triggers.
  */

  virtual const Trigger_collection &triggers() const = 0;


  /**
    Get non-const pointer to Trigger_collection.

    @return Trigger_collection* - Pointer to collection of triggers.
  */

  virtual Trigger_collection *triggers() = 0;


  /**
    Clone all the triggers from a dd::Table object into an array.

    @param [out] triggers - Pointer to trigger array to clone into.
  */

  virtual void clone_triggers(Prealloced_array<Trigger*, 1> *triggers) const= 0;


  /**
    Move all the triggers from an array into the table object.

    @param triggers       Pointer to trigger array to move triggers from.
  */

  virtual void move_triggers(Prealloced_array<Trigger*, 1> *triggers)= 0;


  /**
    Copy all the triggers from another dd::Table object.

    @param tab_obj* - Pointer to Table from which the triggers
                      are copied.
  */

  virtual void copy_triggers(const Table *tab_obj) = 0;


  /**
    Add new trigger to the table.

    @param at      - Action timing of the trigger to be added.
    @param et      - Event type of the trigger to be added.

    @return Trigger* - Pointer to new Trigger that is added to table.
  */

  virtual Trigger *add_trigger(Trigger::enum_action_timing at,
                               Trigger::enum_event_type et) = 0;


  /**
    Get dd::Trigger object for the given trigger name.

    @return Trigger* - Pointer to Trigger.
  */

  virtual const Trigger *get_trigger(const char *name) const = 0;


  /**
    Add new trigger just after the trigger specified in argument.

    @param trigger - dd::Trigger object after which the new
                     trigger should be created.
    @param at      - Action timing of the trigger to be added.
    @param et      - Event type of the trigger to be added.

    @return Trigger* - Pointer to newly created dd::Trigger object.
  */

  virtual Trigger *add_trigger_following(const Trigger *trigger,
                                         Trigger::enum_action_timing at,
                                         Trigger::enum_event_type et) = 0;


  /**
    Add new trigger just before the trigger specified in argument.

    @param trigger - dd::Trigger object before which the new
                     trigger should be created.
    @param at      - Action timing of the trigger to be added.
    @param et      - Event type of the trigger to be added.

    @return Trigger* - Pointer to newly created dd::Trigger object.
  */

  virtual Trigger *add_trigger_preceding(const Trigger *trigger,
                                         Trigger::enum_action_timing at,
                                         Trigger::enum_event_type et) = 0;


  /**
    Drop the given trigger object.

    The method returns void, as we just remove the trigger from
    dd::Collection (dd::Table_impl::m_triggers) in memory. The
    trigger will be removed from mysql.triggers DD table when the
    dd::Table object is stored/updated.

    @param trigger - dd::Trigger object to be dropped.
  */

  virtual void drop_trigger(const Trigger *trigger) = 0;


  /**
    Drop all the trigger on this dd::Table object.

    The method returns void, as we just remove the trigger from
    dd::Collection (dd::Table_impl::m_triggers) in memory. The
    trigger will be removed from mysql.triggers DD table when the
    dd::Table object is stored/updated.
  */

  virtual void drop_all_triggers() = 0;


public:

  /**
    Allocate a new object graph and invoke the copy contructor for
    each object.

    @return pointer to dynamically allocated copy
  */
  virtual Table *clone() const = 0;


  /**
    Converts *this into json.

    Converts all member variables that are to be included in the sdi
    into json by transforming them appropriately and passing them to
    the rapidjson writer provided.

    @param wctx opaque context for data needed by serialization
    @param w rapidjson writer which will perform conversion to json

  */

  virtual void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const = 0;


  /**
    Re-establishes the state of *this by reading sdi information from
    the rapidjson DOM subobject provided.

    Cross-references encountered within this object are tracked in
    sdictx, so that they can be updated when the entire object graph
    has been established.

    @param rctx stores book-keeping information for the
    deserialization process
    @param val subobject of rapidjson DOM containing json
    representation of this object
  */

  virtual bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val) = 0;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__TABLE_INCLUDED
