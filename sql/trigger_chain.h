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

#ifndef TRIGGER_CHAIN_H_INCLUDED
#define TRIGGER_CHAIN_H_INCLUDED

class THD;
class Trigger;

class Trigger_chain : public Sql_alloc
{
public:
  Trigger_chain()
  { }

  /**
    @return a reference to the list of triggers with the same
    EVENT/ACTION_TIME assigned to the table.
  */
  List<Trigger> &get_trigger_list()
  { return m_triggers; }

  bool add_trigger(MEM_ROOT *mem_root,
                   Trigger *new_trigger,
                   enum_trigger_order_type ordering_clause,
                   const LEX_STRING &referenced_trigger_name);

  bool add_trigger(MEM_ROOT *mem_root,
                   Trigger *new_trigger);

  bool execute_triggers(THD *thd);

  void add_tables_and_routines(THD *thd,
                               Query_tables_list *prelocking_ctx,
                               TABLE_LIST *table_list);

  void mark_fields(TABLE *subject_table);

  bool has_updated_trigger_fields(const MY_BITMAP *used_fields);

  void renumerate_triggers();

private:
  /// List of triggers of this chain.
  List<Trigger> m_triggers;
};

#endif
