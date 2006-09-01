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
    Heads of the lists linking items for all fields used in triggers
    grouped by event and action_time.
  */
  Item_trigger_field *trigger_fields[TRG_EVENT_MAX][TRG_ACTION_MAX];
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
    List of "ON table_name" parts in trigger definitions, used for
    updating trigger definitions during RENAME TABLE.
  */
  List<LEX_STRING>  on_table_names_list;
  /*
    Key representing triggers for this table in set of all stored
    routines used by statement.
    TODO: We won't need this member once triggers namespace will be
    database-wide instead of table-wide because then we will be able
    to use key based on sp_name as for other stored routines.
  */
  LEX_STRING        sroutines_key;

  /*
    Grant information for each trigger (pair: subject table, trigger definer).
  */
  GRANT_INFO        subject_table_grants[TRG_EVENT_MAX][TRG_ACTION_MAX];

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

  List<LEX_STRING>  definers_list;

  Table_triggers_list(TABLE *table_arg):
    record1_field(0), table(table_arg)
  {
    bzero((char *)bodies, sizeof(bodies));
    bzero((char *)trigger_fields, sizeof(trigger_fields));
    bzero((char *)&subject_table_grants, sizeof(subject_table_grants));
  }
  ~Table_triggers_list();

  bool create_trigger(THD *thd, TABLE_LIST *table, String *stmt_query);
  bool drop_trigger(THD *thd, TABLE_LIST *table, String *stmt_query);
  bool process_triggers(THD *thd, trg_event_type event,
                        trg_action_time_type time_type,
                        bool old_row_is_record1);
  bool get_trigger_info(THD *thd, trg_event_type event,
                        trg_action_time_type time_type,
                        LEX_STRING *trigger_name, LEX_STRING *trigger_stmt,
                        ulong *sql_mode,
                        LEX_STRING *definer);

  static bool check_n_load(THD *thd, const char *db, const char *table_name,
                           TABLE *table, bool names_only);
  static bool drop_all_triggers(THD *thd, char *db, char *table_name);
  static bool change_table_name(THD *thd, const char *db,
                                const char *old_table,
                                const char *new_db,
                                const char *new_table);
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

  void mark_fields_used(THD *thd, trg_event_type event);

  friend class Item_trigger_field;
  friend int sp_cache_routines_and_add_tables_for_triggers(THD *thd, LEX *lex,
                                                            TABLE_LIST *table);

private:
  bool prepare_record1_accessors(TABLE *table);
  LEX_STRING* change_table_name_in_trignames(const char *db_name,
                                             LEX_STRING *new_table_name,
                                             LEX_STRING *stopper);
  bool change_table_name_in_triggers(THD *thd,
                                     const char *db_name,
                                     LEX_STRING *old_table_name,
                                     LEX_STRING *new_table_name);
};

extern const LEX_STRING trg_action_time_type_names[];
extern const LEX_STRING trg_event_type_names[];
