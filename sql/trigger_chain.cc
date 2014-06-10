/*
   Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "my_global.h"
#include "sql_class.h"
#include "my_bitmap.h"

#include "trigger_chain.h"
#include "table_trigger_dispatcher.h"
#include "table.h"
#include "sql_list.h"
#include "trigger_loader.h"
#include "trigger.h"
#include "sp_head.h"                  // sp_head::mark_used_trigger_fields


/**
  Add a new trigger into the list of triggers with the same
  ACTION/TIMING value combination. This method is called during
  handling of statement CREATE TRIGGER.

  @param mem_root                 memory root for allocations
  @param new_trigger              pointer to the Trigger to add into the list
  @param ordering_clause          trigger ordering clause (FOLLOWS/PRECEDES)
  @param referenced_trigger_name  trigger name referenced by clause
                                  FOLLOWS/PRECEDES in the CREATE TRIGGER
                                  statement

  @return Operation status.
    @retval false Success.
    @retval true  Failure: either trigger not found or OOM. Set error in
    Diagnostic_area in case when trigger not found.
*/

bool Trigger_chain::add_trigger(MEM_ROOT *mem_root,
                                Trigger *new_trigger,
                                enum_trigger_order_type ordering_clause,
                                const LEX_STRING &referenced_trigger_name)
{
  switch (ordering_clause)
  {
  case TRG_ORDER_NONE:
    return add_trigger(mem_root, new_trigger);

  case TRG_ORDER_FOLLOWS:
  case TRG_ORDER_PRECEDES:
    {
      DBUG_ASSERT(referenced_trigger_name.str);

      List_iterator<Trigger> it(m_triggers);
      List_iterator<Trigger> it2= it;

      while (true)
      {
        Trigger *t= it2++;

        if (!t)
        {
          my_error(ER_REFERENCED_TRG_DOES_NOT_EXIST,
                   MYF(0), referenced_trigger_name.str);
          return true;
        }

        if (my_strcasecmp(table_alias_charset,
                          t->get_trigger_name().str,
                          referenced_trigger_name.str) == 0)
        {
          break;
        }

        it= it2;
      }

      if (ordering_clause == TRG_ORDER_FOLLOWS)
        it= it2;

      bool rc;

      if (it.is_before_first())
        rc= m_triggers.push_front(new_trigger, mem_root);
      else
        rc= it.after(new_trigger, mem_root);

      return rc;
    }
  }

  DBUG_ASSERT(false);
  return true;
}


/**
  Add a new trigger into the list of triggers with the same
  ACTION/TIMING value combination. This method is called when a trigger
  is loaded from Data Dictionary.

  @param mem_root        memory root for allocations
  @param new_trigger     pointer to the Trigger to add into the list

  @return Operation status.
    @retval false Success.
    @retval true  Failure.
*/

bool Trigger_chain::add_trigger(MEM_ROOT *mem_root, Trigger *new_trigger)
{
  return m_triggers.push_back(new_trigger, mem_root);
}


/**
  Run every trigger in the list of triggers.

  @param thd  Thread context
  @return  Result of trigger execution
    @retval false  all triggers in the list were executed successfully.
    @retval true   some trigger was failed. We stop triggers execution
                   on the first failed trigger and don't attempt to finish
                   the rest of triggers located after the failed one.
*/

bool Trigger_chain::execute_triggers(THD *thd)
{
  List_iterator_fast<Trigger> it(m_triggers);
  Trigger *t;

  while ((t= it++))
  {
    if (t->execute(thd))
      return true;
  }

  return false;
}


/**
  Iterate over the list of triggers and add tables and routines used by trigger
  to the set of elements used by statement.

  @param [in]     thd               thread context
  @param [in out] prelocking_ctx    prelocking context of the statement
  @param [in]     table_list        TABLE_LIST for the table
*/

void Trigger_chain::add_tables_and_routines(THD *thd,
                                            Query_tables_list *prelocking_ctx,
                                            TABLE_LIST *table_list)
{
  List_iterator_fast<Trigger> it(m_triggers);
  Trigger *t;

  while ((t= it++))
    t->add_tables_and_routines(thd, prelocking_ctx, table_list);
}


/**
  Iterate over the list of triggers and mark fields of subject table
  which we read/set in every trigger.

  @param [in] subject_table trigger subject table
*/

void Trigger_chain::mark_fields(TABLE *subject_table)
{
  List_iterator_fast<Trigger> it(m_triggers);
  Trigger *t;

  while ((t= it++))
    t->get_sp()->mark_used_trigger_fields(subject_table);
}


/**
  Iterate over the list of triggers and check whether some
  table's fields are used in any trigger.

  @param [in] used_fields       bitmap of fields to check

  @return Check result
    @retval true   Some table fields are used in trigger
    @retval false  None of table fields are used in trigger
*/

bool Trigger_chain::has_updated_trigger_fields(const MY_BITMAP *used_fields)
{
  List_iterator_fast<Trigger> it(m_triggers);
  Trigger *t;

  while ((t= it++))
  {
    // Even if one trigger is unparseable, the whole thing is not usable.

    if (t->has_parse_error())
      return false;

    if (t->get_sp()->has_updated_trigger_fields(used_fields))
      return true;
  }

  return false;
}


/**
  Recalculate action_order value for every trigger in the list.
*/

void Trigger_chain::renumerate_triggers()
{
  ulonglong action_order= 1;

  List_iterator_fast<Trigger> it(m_triggers);
  Trigger *t;

  while ((t= it++))
  {
    t->set_action_order(action_order);
    ++action_order;
  }
}
