/* Copyright (C) 2004-2005 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA */


/*
  This class holds all information about triggers of table.

  QQ: Will it be merged into TABLE in future ?
*/

class Table_triggers_list: public Sql_alloc
{
  /* Triggers as SPs grouped by event, action_time */
  sp_head           *bodies[TRG_EVENT_MAX][TRG_ACTION_MAX];
  /*
    Copy of TABLE::Field array with field pointers set to TABLE::record[1]
    buffer instead of TABLE::record[0] (used for OLD values in on UPDATE
    trigger and DELETE trigger when it is called for REPLACE).
  */
  Field             **record1_field;
  /*
    During execution of trigger new_field and old_field should point to the
    array of fields representing new or old version of row correspondingly
    (so it can point to TABLE::field or to Tale_triggers_list::record1_field)
  */
  Field             **new_field;
  Field             **old_field;
  /* TABLE instance for which this triggers list object was created */
  TABLE *table;
  /*
    Names of triggers.
    Should correspond to order of triggers on definitions_list,
    used in CREATE/DROP TRIGGER for looking up trigger by name.
  */
  List<LEX_STRING>  names_list;
  /*
    Key representing triggers for this table in set of all stored
    routines used by statement.
    TODO: We won't need this member once triggers namespace will be
    database-wide instead of table-wide because then we will be able
    to use key based on sp_name as for other stored routines.
  */
  LEX_STRING        sroutines_key;

public:
  /*
    Field responsible for storing triggers definitions in file.
    It have to be public because we are using it directly from parser.
  */
  List<LEX_STRING>  definitions_list;
  /*
    List of sql modes for triggers
  */
  List<ulonglong> definition_modes_list;

  Table_triggers_list(TABLE *table_arg):
    record1_field(0), table(table_arg)
  {
    bzero((char *)bodies, sizeof(bodies));
  }
  ~Table_triggers_list();

  bool create_trigger(THD *thd, TABLE_LIST *table);
  bool drop_trigger(THD *thd, TABLE_LIST *table);
  bool process_triggers(THD *thd, trg_event_type event,
                        trg_action_time_type time_type,
                        bool old_row_is_record1);
  bool get_trigger_info(THD *thd, trg_event_type event,
                        trg_action_time_type time_type,
                        LEX_STRING *trigger_name, LEX_STRING *trigger_stmt,
                        ulong *sql_mode);

  static bool check_n_load(THD *thd, const char *db, const char *table_name,
                           TABLE *table, bool names_only);
  static bool drop_all_triggers(THD *thd, char *db, char *table_name);

  bool has_delete_triggers()
  {
    return (bodies[TRG_EVENT_DELETE][TRG_ACTION_BEFORE] ||
            bodies[TRG_EVENT_DELETE][TRG_ACTION_AFTER]);
  }

  bool has_before_update_triggers()
  {
    return test(bodies[TRG_EVENT_UPDATE][TRG_ACTION_BEFORE]);
  }

  void set_table(TABLE *new_table);

  friend class Item_trigger_field;
  friend int sp_cache_routines_and_add_tables_for_triggers(THD *thd, LEX *lex,
                Table_triggers_list *triggers);

private:
  bool prepare_record1_accessors(TABLE *table);
};

extern const LEX_STRING trg_action_time_type_names[];
extern const LEX_STRING trg_event_type_names[];
