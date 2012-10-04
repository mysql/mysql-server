#ifndef SQL_TRIGGER_INCLUDED
#define SQL_TRIGGER_INCLUDED

/*
   Copyright (c) 2004, 2012, Oracle and/or its affiliates. All rights reserved.

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

/* Forward declarations */

class Item_trigger_field;
class sp_head;
class sp_name;
class Query_tables_list;
struct TABLE_LIST;
class Query_tables_list;

/** Event on which trigger is invoked. */
enum trg_event_type
{
  TRG_EVENT_INSERT= 0,
  TRG_EVENT_UPDATE= 1,
  TRG_EVENT_DELETE= 2,
  TRG_EVENT_MAX
};

#include "table.h"                              /* GRANT_INFO */

/*
  We need this two enums here instead of sql_lex.h because
  at least one of them is used by Item_trigger_field interface.

  Time when trigger is invoked (i.e. before or after row actually
  inserted/updated/deleted).
*/
enum trg_action_time_type
{
  TRG_ACTION_BEFORE= 0, TRG_ACTION_AFTER= 1, TRG_ACTION_MAX
};


/**
  This class holds all information about triggers of table.

  QQ: Will it be merged into TABLE in the future ?
*/

class Table_triggers_list: public Sql_alloc
{
  /** Triggers as SPs grouped by event, action_time */
  sp_head *bodies[TRG_EVENT_MAX][TRG_ACTION_MAX];

  /**
    Copy of TABLE::Field array with field pointers set to TABLE::record[1]
    buffer instead of TABLE::record[0] (used for OLD values in on UPDATE
    trigger and DELETE trigger when it is called for REPLACE).
  */
  Field             **record1_field;

  /**
    During execution of trigger new_field and old_field should point to the
    array of fields representing new or old version of row correspondingly
    (so it can point to TABLE::field or to Tale_triggers_list::record1_field)
  */
  Field             **new_field;
  Field             **old_field;

public:
  /** TABLE instance for which this triggers list object was created. */
  TABLE *trigger_table;

private:
  /**
    Names of triggers.
    Should correspond to order of triggers on definitions_list,
    used in CREATE/DROP TRIGGER for looking up trigger by name.
  */
  List<LEX_STRING>  names_list;

  /**
    List of "ON table_name" parts in trigger definitions, used for
    updating trigger definitions during RENAME TABLE.
  */
  List<LEX_STRING>  on_table_names_list;

public:
  /**
    Grant information for each trigger (pair: subject table, trigger definer).
  */
  GRANT_INFO        subject_table_grants[TRG_EVENT_MAX][TRG_ACTION_MAX];

private:
  /**
     This flag indicates that one of the triggers was not parsed successfully,
     and as a precaution the object has entered a state where all trigger
     access results in errors until all such triggers are dropped. It is not
     safe to add triggers since we don't know if the broken trigger has the
     same name or event type. Nor is it safe to invoke any trigger for the
     aforementioned reasons. The only safe operations are drop_trigger and
     drop_all_triggers.

     @see Table_triggers_list::set_parse_error
   */
  bool m_has_unparseable_trigger;

  /**
    This error will be displayed when the user tries to manipulate or invoke
    triggers on a table that has broken triggers. It will get set only once
    per statement and thus will contain the first parse error encountered in
    the trigger file.
   */
  char m_parse_error_message[MYSQL_ERRMSG_SIZE];

public:
  /**
    Field responsible for storing triggers definitions in file.
    It have to be public because we are using it directly from parser.
  */
  List<LEX_STRING>  definitions_list;

  /**
    List of sql modes for triggers
  */
  List<ulonglong> definition_modes_list;

  List<LEX_STRING>  definers_list;

  /* Character set context, used for parsing and executing triggers. */

  List<LEX_STRING> client_cs_names;
  List<LEX_STRING> connection_cl_names;
  List<LEX_STRING> db_cl_names;

  /* End of character ser context. */

  Table_triggers_list(TABLE *table_arg)
    :record1_field(0), trigger_table(table_arg),
    m_has_unparseable_trigger(false)
  {
    memset(bodies, 0, sizeof(bodies));
    memset(&subject_table_grants, 0, sizeof(subject_table_grants));
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
                        sql_mode_t *sql_mode,
                        LEX_STRING *definer,
                        LEX_STRING *client_cs_name,
                        LEX_STRING *connection_cl_name,
                        LEX_STRING *db_cl_name);

  void get_trigger_info(THD *thd,
                        int trigger_idx,
                        LEX_STRING *trigger_name,
                        sql_mode_t *sql_mode,
                        LEX_STRING *sql_original_stmt,
                        LEX_STRING *client_cs_name,
                        LEX_STRING *connection_cl_name,
                        LEX_STRING *db_cl_name);

  int find_trigger_by_name(const LEX_STRING *trigger_name);

  static bool check_n_load(THD *thd, const char *db, const char *table_name,
                           TABLE *table, bool names_only);
  static bool drop_all_triggers(THD *thd, char *db, char *table_name);
  static bool change_table_name(THD *thd, const char *db,
                                const char *old_alias,
                                const char *old_table,
                                const char *new_db,
                                const char *new_table);
  bool has_triggers(trg_event_type event_type, 
                    trg_action_time_type action_time)
  {
    return (bodies[event_type][action_time] != NULL);
  }
  bool has_delete_triggers()
  {
    return (bodies[TRG_EVENT_DELETE][TRG_ACTION_BEFORE] ||
            bodies[TRG_EVENT_DELETE][TRG_ACTION_AFTER]);
  }

  void set_table(TABLE *new_table);

  void mark_fields_used(trg_event_type event);

  void set_parse_error_message(char *error_message);

  friend class Item_trigger_field;

  bool add_tables_and_routines_for_triggers(THD *thd,
                                            Query_tables_list *prelocking_ctx,
                                            TABLE_LIST *table_list);
  bool is_fields_updated_in_trigger(MY_BITMAP *used_fields,
                                    trg_event_type event_type,
                                    trg_action_time_type action_time);
private:
  bool prepare_record1_accessors();
  LEX_STRING* change_table_name_in_trignames(const char *old_db_name,
                                             const char *new_db_name,
                                             LEX_STRING *new_table_name,
                                             LEX_STRING *stopper);
  bool change_table_name_in_triggers(THD *thd,
                                     const char *old_db_name,
                                     const char *new_db_name,
                                     LEX_STRING *old_table_name,
                                     LEX_STRING *new_table_name);

  bool check_for_broken_triggers() 
  {
    if (m_has_unparseable_trigger)
    {
      my_message(ER_PARSE_ERROR, m_parse_error_message, MYF(0));
      return true;
    }
    return false;
  }
};

extern const LEX_STRING trg_action_time_type_names[];
extern const LEX_STRING trg_event_type_names[];

bool add_table_for_trigger(THD *thd,
                           const sp_name *trg_name,
                           bool continue_if_not_exist,
                           TABLE_LIST **table);

void build_trn_path(THD *thd, const sp_name *trg_name, LEX_STRING *trn_path);

bool check_trn_exists(const LEX_STRING *trn_path);

bool load_table_name_for_trigger(THD *thd,
                                 const sp_name *trg_name,
                                 const LEX_STRING *trn_path,
                                 LEX_STRING *tbl_name);
bool mysql_create_or_drop_trigger(THD *thd, TABLE_LIST *tables, bool create);

extern const char * const TRG_EXT;
extern const char * const TRN_EXT;

#endif /* SQL_TRIGGER_INCLUDED */
