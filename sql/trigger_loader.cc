/*
   Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"
#include "trigger_loader.h"
#include "sql_class.h"
#include "sp_head.h"      // sp_name
#include "sql_base.h"     // is_equal(LEX_STRING, LEX_STRING)
#include "sql_table.h"    // build_table_filename()
#include <mysys_err.h>    // EE_OUTOFMEMORY
#include "parse_file.h"   // File_option

///////////////////////////////////////////////////////////////////////////

/**
  This must be kept up to date whenever a new option is added to the list
  above, as it specifies the number of required parameters of the trigger in
  .trg file.
*/

static const int TRG_NUM_REQUIRED_PARAMETERS= 6;

const LEX_STRING trg_file_type= { C_STRING_WITH_LEN("TRIGGERS") };

const LEX_STRING trn_file_type= { C_STRING_WITH_LEN("TRIGGERNAME") };

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

/*
  Structure representing contents of .TRN file which are used to support
  database wide trigger namespace.
*/

struct Trigger_name_data
{
  LEX_STRING trigger_table;
};

///////////////////////////////////////////////////////////////////////////

static File_option trn_file_parameters[]=
{
  {
    { C_STRING_WITH_LEN("trigger_table")},
    offsetof(struct Trigger_name_data, trigger_table),
    FILE_OPTIONS_ESTRING
  },
  { { 0, 0 }, 0, FILE_OPTIONS_STRING }
};

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

/**
  Structure representing contents of .TRG file.
*/

struct Trigger_data
{
  /// List of CREATE TRIGGER statements.
  List<LEX_STRING>  definitions_list;

  /// List of 'sql mode' values.
  List<ulonglong> definition_modes_list;

  /// List of 'definer' values.
  List<LEX_STRING>  definers_list;

  /// List of client character set names.
  List<LEX_STRING> client_cs_names;

  /// List of connection collation names.
  List<LEX_STRING> connection_cl_names;

  /// List of database collation names.
  List<LEX_STRING> db_cl_names;

  bool append_trigger(Trigger *t, MEM_ROOT *mem_root)
  {
    return
      definitions_list.push_back(t->get_definition(), mem_root) ||
      definition_modes_list.push_back(t->get_sql_mode(), mem_root) ||
      definers_list.push_back(t->get_definer(), mem_root) ||
      client_cs_names.push_back(t->get_client_cs_name(), mem_root) ||
      connection_cl_names.push_back(t->get_connection_cl_name(), mem_root) ||
      db_cl_names.push_back(t->get_db_cl_name(), mem_root);
  }
};

///////////////////////////////////////////////////////////////////////////

/**
  Table of .TRG file field descriptors.
*/

static File_option trg_file_parameters[]=
{
  {
    { C_STRING_WITH_LEN("triggers") },
    my_offsetof(struct Trigger_data, definitions_list),
    FILE_OPTIONS_STRLIST
  },
  {
    { C_STRING_WITH_LEN("sql_modes") },
    my_offsetof(struct Trigger_data, definition_modes_list),
    FILE_OPTIONS_ULLLIST
  },
  {
    { C_STRING_WITH_LEN("definers") },
    my_offsetof(struct Trigger_data, definers_list),
    FILE_OPTIONS_STRLIST
  },
  {
    { C_STRING_WITH_LEN("client_cs_names") },
    my_offsetof(struct Trigger_data, client_cs_names),
    FILE_OPTIONS_STRLIST
  },
  {
    { C_STRING_WITH_LEN("connection_cl_names") },
    my_offsetof(struct Trigger_data, connection_cl_names),
    FILE_OPTIONS_STRLIST
  },
  {
    { C_STRING_WITH_LEN("db_cl_names") },
    my_offsetof(struct Trigger_data, db_cl_names),
    FILE_OPTIONS_STRLIST
  },
  { { 0, 0 }, 0, FILE_OPTIONS_STRING }
};

///////////////////////////////////////////////////////////////////////////

static File_option sql_modes_parameters=
{
  { C_STRING_WITH_LEN("sql_modes") },
  my_offsetof(struct Trigger_data, definition_modes_list),
  FILE_OPTIONS_ULLLIST
};

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

static LEX_STRING get_trn_file_name(char *trn_file_name_buffer,
                                    int trn_file_name_buffer_size,
                                    const char *db_name,
                                    const char *trigger_name)
{
  bool was_truncated= false;
  LEX_STRING trn_file_name;

  trn_file_name.str= trn_file_name_buffer;
  trn_file_name.length= build_table_filename(trn_file_name_buffer,
                                             trn_file_name_buffer_size - 1,
                                             db_name, trigger_name,
                                             TRN_EXT, 0, &was_truncated);

  if (!was_truncated)
    return trn_file_name;

  my_error(ER_IDENT_CAUSES_TOO_LONG_PATH, MYF(0),
           sizeof (trn_file_name_buffer) - 1,
           trn_file_name_buffer);

  trn_file_name.str= NULL;
  trn_file_name.length= 0;

  return trn_file_name;
}

/**
  This method saves .TRG file for the table specified by arguments.

  @param db_name     Name of database for subject table
  @param table_name  Name of subject table

  @return Operation status.
  @retval false  Success
  @retval true   Failure
*/

static bool save_trg_file(const char *db_name,
                          const char *table_name,
                          const Trigger_data *trg)
{
  char trg_file_name_buffer[FN_REFLEN];
  LEX_STRING trg_file_name;
  bool was_truncated= false;

  trg_file_name.length= build_table_filename(trg_file_name_buffer, FN_REFLEN - 1,
                                             db_name, table_name,
                                             TRG_EXT, 0, &was_truncated);

  if (was_truncated)
  {
    my_error(ER_IDENT_CAUSES_TOO_LONG_PATH, MYF(0),
             sizeof (trg_file_name_buffer) - 1,
             trg_file_name_buffer);
    return true;
  }

  trg_file_name.str= trg_file_name_buffer;
  return sql_create_definition_file(NULL, &trg_file_name, &trg_file_type,
                                    (uchar*) trg, trg_file_parameters);
}


/**
  Deletes the .TRN file for a trigger.

  @param [in] db_name        trigger's database name
  @param [in] trigger_name   trigger's name

  @return Operation status.
    @retval true   Failure
    @retval false  Success
*/

static bool rm_trn_file(const char *db_name, const char *trigger_name)
{
  char path[FN_REFLEN];

  build_table_filename(path, FN_REFLEN - 1, db_name, trigger_name, TRN_EXT, 0);
  return mysql_file_delete(key_file_trn, path, MYF(MY_WME));
}


/**
  Deletes the .TRG file for a table.

  @param db_name      table's database name
  @param table_name   table's name

  @return Operation status.
    @retval true   Failure
    @retval false  Success
*/

static bool rm_trg_file(const char *db_name, const char *table_name)
{
  char path[FN_REFLEN];

  build_table_filename(path, FN_REFLEN - 1, db_name, table_name, TRG_EXT, 0);
  return mysql_file_delete(key_file_trg, path, MYF(MY_WME));
}

static bool fill_trg_data(Trigger_data *trg,
                          MEM_ROOT *mem_root,
                          List<Trigger> *triggers)
{
  List_iterator<Trigger> it(*triggers);

  Trigger *t;
  while ((t= it++))
  {
    if (trg->append_trigger(t, mem_root))
      return true;
  }

  return false;
}

/**
  Change the subject table in the given list of triggers.

  @param old_db_name         Old database of subject table
  @param new_db_name         New database of subject table
  @param new_table_name      New subject table's name
  @param stopper             Pointer to a trigger_name for
                             which we should stop updating.

  @retval NULL      Success
  @retval not-NULL  Failure, pointer to Table_trigger_dispatcher::names_list element
                    for which update failed.
*/

static LEX_STRING *change_table_name_in_trn_files(
  List<Trigger> *triggers,
  const char *old_db_name,
  const char *new_db_name,
  const LEX_STRING *new_table_name,
  const LEX_STRING *stopper)
{
  Trigger *trigger;
  List_iterator_fast<Trigger> it(*triggers);

  while ((trigger= it++))
  {
    // TODO: FIXME: just a pointer check here! is it enough?
    if (trigger->get_trigger_name() == stopper)
      break;

    // Get TRN file name.

    char trn_file_name_buffer[FN_REFLEN];

    LEX_STRING trn_file_name= get_trn_file_name(trn_file_name_buffer, FN_REFLEN,
                                                new_db_name,
                                                trigger->get_trigger_name()->str);
    if (!trn_file_name.str)
      return NULL; // FIXME: OOM

    // Prepare TRN data.

    Trigger_name_data trn;
    trn.trigger_table= *new_table_name;

    // Create new TRN file.

    if (sql_create_definition_file(NULL, &trn_file_name, &trn_file_type,
                                   (uchar *) &trn, trn_file_parameters))
    {
      return trigger->get_trigger_name();
    }

    // Remove stale .TRN file in case of database upgrade.

    if (old_db_name)
    {
      if (rm_trn_file(old_db_name, trigger->get_trigger_name()->str))
      {
        rm_trn_file(new_db_name, trigger->get_trigger_name()->str);
        return trigger->get_trigger_name();
      }
    }
  }

  return NULL;
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

class Handle_old_incorrect_sql_modes_hook: public Unknown_key_hook
{
private:
  char *path;
public:
  Handle_old_incorrect_sql_modes_hook(char *file_path)
    :path(file_path)
  {};
  virtual bool process_unknown_string(const char *&unknown_key, uchar* base,
                                      MEM_ROOT *mem_root, const char *end);
};

///////////////////////////////////////////////////////////////////////////

class Handle_old_incorrect_trigger_table_hook: public Unknown_key_hook
{
public:
  Handle_old_incorrect_trigger_table_hook(char *file_path,
                                          LEX_STRING *trigger_table_arg)
    :path(file_path), trigger_table_value(trigger_table_arg)
  {};
  virtual bool process_unknown_string(const char *&unknown_key, uchar* base,
                                      MEM_ROOT *mem_root, const char *end);
private:
  char *path;
  LEX_STRING *trigger_table_value;
};

///////////////////////////////////////////////////////////////////////////

/**
  Trigger BUG#14090 compatibility hook.

  @param[in,out] unknown_key       reference on the line with unknown
    parameter and the parsing point
  @param[in]     base              base address for parameter writing
    (structure like TABLE)
  @param[in]     mem_root          MEM_ROOT for parameters allocation
  @param[in]     end               the end of the configuration

  @note
    NOTE: this hook process back compatibility for incorrectly written
    sql_modes parameter (see BUG#14090).

  @retval
    false OK
  @retval
    true  Error
*/

#define INVALID_SQL_MODES_LENGTH 13

bool Handle_old_incorrect_sql_modes_hook::process_unknown_string(
  const char *&unknown_key,
  uchar* base,
  MEM_ROOT *mem_root,
  const char *end)
{
  DBUG_ENTER("Handle_old_incorrect_sql_modes_hook::process_unknown_string");
  DBUG_PRINT("info", ("unknown key: %60s", unknown_key));

  if (unknown_key + INVALID_SQL_MODES_LENGTH + 1 < end &&
      unknown_key[INVALID_SQL_MODES_LENGTH] == '=' &&
      !memcmp(unknown_key, STRING_WITH_LEN("sql_modes")))
  {
    const char *ptr= unknown_key + INVALID_SQL_MODES_LENGTH + 1;

    DBUG_PRINT("info", ("sql_modes affected by BUG#14090 detected"));
    push_warning_printf(current_thd,
                        Sql_condition::SL_NOTE,
                        ER_OLD_FILE_FORMAT,
                        ER(ER_OLD_FILE_FORMAT),
                        (char *)path, "TRIGGER");
    if (get_file_options_ulllist(ptr, end, unknown_key, base,
                                 &sql_modes_parameters, mem_root))
    {
      DBUG_RETURN(true);
    }
    /*
      Set parsing pointer to the last symbol of string (\n)
      1) to avoid problem with \0 in the junk after sql_modes
      2) to speed up skipping this line by parser.
    */
    unknown_key= ptr-1;
  }
  DBUG_RETURN(false);
}

///////////////////////////////////////////////////////////////////////////

#define INVALID_TRIGGER_TABLE_LENGTH 15

/**
  Trigger BUG#15921 compatibility hook. For details see
  Handle_old_incorrect_sql_modes_hook::process_unknown_string().
*/

bool Handle_old_incorrect_trigger_table_hook::process_unknown_string(
  const char *&unknown_key,
  uchar* base,
  MEM_ROOT *mem_root,
  const char *end)
{
  DBUG_ENTER("Handle_old_incorrect_trigger_table_hook::process_unknown_string");
  DBUG_PRINT("info", ("unknown key: %60s", unknown_key));

  if (unknown_key + INVALID_TRIGGER_TABLE_LENGTH + 1 < end &&
      unknown_key[INVALID_TRIGGER_TABLE_LENGTH] == '=' &&
      !memcmp(unknown_key, STRING_WITH_LEN("trigger_table")))
  {
    const char *ptr= unknown_key + INVALID_TRIGGER_TABLE_LENGTH + 1;

    DBUG_PRINT("info", ("trigger_table affected by BUG#15921 detected"));
    push_warning_printf(current_thd,
                        Sql_condition::SL_NOTE,
                        ER_OLD_FILE_FORMAT,
                        ER(ER_OLD_FILE_FORMAT),
                        (char *)path, "TRIGGER");

    if (!(ptr= parse_escaped_string(ptr, end, mem_root, trigger_table_value)))
    {
      my_error(ER_FPARSER_ERROR_IN_PARAMETER, MYF(0), "trigger_table",
               unknown_key);
      DBUG_RETURN(true);
    }

    /* Set parsing pointer to the last symbol of string (\n). */
    unknown_key= ptr-1;
  }
  DBUG_RETURN(false);
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

/**
  Load table triggers from the data dictionary.

  @param [in]  thd                thread handle
  @param [in]  db_name            name of schema
  @param [in]  db_name            name of table
  @param [in]  table              pointer to the trigger's table
  @param [out] triggers           pointer to the list where new Trigger
                                  objects will be inserted
  @param [out] trigger_not_found  true if trigger was not found by name,
                                  else false

  @return Operation status
    @retval true   Failure
    @retval false  Success
*/

bool Trigger_loader::load_triggers(THD *thd,
                                   const char *db_name,
                                   const char *table_name,
                                   TABLE *table,
                                   List<Trigger> *triggers,
                                   bool *trigger_not_found)
{
  DBUG_ENTER("Trigger_loader::load_triggers");

  // Check that TRG-file exists.

  char trg_file_path_buffer[FN_REFLEN];
  LEX_STRING trg_file_path;

  trg_file_path.length= build_table_filename(trg_file_path_buffer,
                                             FN_REFLEN - 1,
                                             db_name, table_name, TRG_EXT, 0);
  trg_file_path.str= trg_file_path_buffer;

  *trigger_not_found= false;

  if (access(trg_file_path_buffer, F_OK))
  {
    if (errno == ENOENT)
      *trigger_not_found= true;

    DBUG_RETURN(true);
  }

  // The TRG-file exists so we got to load triggers.

  File_parser *parser=
    sql_parse_prepare(&trg_file_path, &table->mem_root, true);

  if (!parser)
    DBUG_RETURN(true);

  if (!is_equal(&trg_file_type, parser->type()))
  {
    my_error(ER_WRONG_OBJECT, MYF(0), table_name, TRG_EXT + 1, "TRIGGER");
    DBUG_RETURN(true);
  }

  Handle_old_incorrect_sql_modes_hook sql_modes_hook(trg_file_path.str);

  Trigger_data trg;

  if (parser->parse((uchar*) &trg,
                    &table->mem_root,
                    trg_file_parameters,
                    TRG_NUM_REQUIRED_PARAMETERS,
                    &sql_modes_hook))
    DBUG_RETURN(true);

  if (trg.definitions_list.is_empty())
  {
    DBUG_ASSERT(trg.definition_modes_list.is_empty());
    DBUG_ASSERT(trg.definers_list.is_empty());
    DBUG_ASSERT(trg.client_cs_names.is_empty());
    DBUG_ASSERT(trg.connection_cl_names.is_empty());
    DBUG_ASSERT(trg.db_cl_names.is_empty());
    DBUG_RETURN(false);
  }

  List_iterator_fast<LEX_STRING> it(trg.definitions_list);

  // Make sure sql_mode list is filled.

  if (trg.definition_modes_list.is_empty())
  {
    /*
      It is old file format => we should fill list of sql_modes.

      We use one mode (current) for all triggers, because we have not
      information about mode in old format.
    */
    sql_mode_t *sql_mode= alloc_type<sql_mode_t>(&table->mem_root);

    if (!sql_mode)
      DBUG_RETURN(true); // EOM

    *sql_mode= global_system_variables.sql_mode;

    while (it++)
    {
      if (trg.definition_modes_list.push_back(sql_mode, &table->mem_root))
        DBUG_RETURN(true); // EOM
    }

    it.rewind();
  }

  // Make sure definer list is filled.

  if (trg.definers_list.is_empty())
  {
    /*
      It is old file format => we should fill list of definers.

      If there is no definer information, we should not switch context to
      definer when checking privileges. I.e. privileges for such triggers
      are checked for "invoker" rather than for "definer".
    */

    LEX_STRING *definer= alloc_lex_string(&table->mem_root);

    if (!definer)
      DBUG_RETURN(true); // EOM

    definer->str= (char*) "";
    definer->length= 0;

    while (it++)
    {
      if (trg.definers_list.push_back(definer, &table->mem_root))
        DBUG_RETURN(true); // EOM
    }

    it.rewind();
  }

  // Make sure character set properties are filled.

  if (trg.client_cs_names.is_empty() ||
      trg.connection_cl_names.is_empty() ||
      trg.db_cl_names.is_empty())
  {
    if (!trg.client_cs_names.is_empty() ||
        !trg.connection_cl_names.is_empty() ||
        !trg.db_cl_names.is_empty())
    {
      my_error(ER_TRG_CORRUPTED_FILE, MYF(0),
               (const char *) db_name,
               (const char *) table_name);

      DBUG_RETURN(true);
    }

    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_TRG_NO_CREATION_CTX,
                        ER(ER_TRG_NO_CREATION_CTX),
                        (const char*) db_name,
                        (const char*) table_name);

    LEX_STRING *trg_client_cs_name= alloc_lex_string(&table->mem_root);
    LEX_STRING *trg_connection_cl_name= alloc_lex_string(&table->mem_root);
    LEX_STRING *trg_db_cl_name= alloc_lex_string(&table->mem_root);

    if (!trg_client_cs_name || !trg_connection_cl_name || !trg_db_cl_name)
      DBUG_RETURN(true); // EOM

    /*
      Backward compatibility: assume that the query is in the current
      character set.
    */

    lex_string_set(trg_client_cs_name,
                   thd->variables.character_set_client->csname);

    lex_string_set(trg_connection_cl_name,
                   thd->variables.collation_connection->name);

    lex_string_set(trg_db_cl_name,
                   thd->variables.collation_database->name);

    while (it++)
    {
      if (trg.client_cs_names.push_back(trg_client_cs_name, &table->mem_root) ||
          trg.connection_cl_names.push_back(trg_connection_cl_name, &table->mem_root) ||
          trg.db_cl_names.push_back(trg_db_cl_name, &table->mem_root))
      {
        DBUG_RETURN(true); // EOM
      }
    }

    it.rewind();
  }

  DBUG_ASSERT(trg.definition_modes_list.elements == trg.definitions_list.elements);
  DBUG_ASSERT(trg.definers_list.elements == trg.definitions_list.elements);
  DBUG_ASSERT(trg.client_cs_names.elements == trg.definitions_list.elements);
  DBUG_ASSERT(trg.connection_cl_names.elements == trg.definitions_list.elements);
  DBUG_ASSERT(trg.db_cl_names.elements == trg.definitions_list.elements);

  LEX_STRING *current_db_name= alloc_lex_string(&table->mem_root);
  lex_string_set(current_db_name, db_name);

  LEX_STRING *table_name_str= alloc_lex_string(&table->mem_root);
  lex_string_set(table_name_str, table_name);

  LEX_STRING *trg_create_str;

  List_iterator_fast<sql_mode_t> itm(trg.definition_modes_list);
  List_iterator_fast<LEX_STRING> it_definer(trg.definers_list);
  List_iterator_fast<LEX_STRING> it_client_cs_name(trg.client_cs_names);
  List_iterator_fast<LEX_STRING> it_connection_cl_name(trg.connection_cl_names);
  List_iterator_fast<LEX_STRING> it_db_cl_name(trg.db_cl_names);

  while ((trg_create_str= it++))
  {
    sql_mode_t *sql_mode= itm++;
    LEX_STRING *definer= it_definer++;
    LEX_STRING *client_cs_name= it_client_cs_name++;
    LEX_STRING *connection_cl_name= it_connection_cl_name++;
    LEX_STRING *db_cl_name= it_db_cl_name++;

    // Create an new trigger instance

    Trigger* trigger=
        new (&table->mem_root) Trigger(current_db_name,
                                       table_name_str, trg_create_str,
                                       *sql_mode, definer,
                                       client_cs_name, connection_cl_name,
                                       db_cl_name);

    if (triggers->push_back(trigger, &table->mem_root))
    {
      delete trigger;
      DBUG_RETURN(true);
    }
  }

  DBUG_RETURN(false);
}

///////////////////////////////////////////////////////////////////////////

/**
  Store a table trigger into the data dictionary.

  @param [in]  tables       pointer to trigger's table
  @param [in]  new_trigger  trigger to save
  @param [in]  triggers     pointer to the list where new trigger object has to
                            be added

  @return Operation status
    @retval true   Failure
    @retval false  Success
*/

bool Trigger_loader::store_trigger(TABLE_LIST *tables,
                                   Trigger *new_trigger,
                                   List<Trigger> *triggers)
{
  // Fill TRN-data structure.

  Trigger_name_data trn;

  trn.trigger_table.str= tables->table_name;
  trn.trigger_table.length= tables->table_name_length;

  // Fill TRG-data structure.

  Trigger_data trg;

  if (fill_trg_data(&trg, &tables->table->mem_root, triggers))
    return true;

  // Get TRN file name.

  char trn_file_name_buffer[FN_REFLEN];

  LEX_STRING trn_file_name= get_trn_file_name(trn_file_name_buffer, FN_REFLEN,
                                              tables->db,
                                              new_trigger->get_trigger_name()->str);
  if (!trn_file_name.str)
    return true; // my_error() has already been called.

  // Save TRN file.

  if (sql_create_definition_file(NULL, &trn_file_name, &trn_file_type,
                                 (uchar *) &trn, trn_file_parameters))
  {
    return true; // my_error() has already been called.
  }

  // Save TRG file.

  if (save_trg_file(tables->db, tables->table_name, &trg))
  {
    mysql_file_delete(key_file_trn, trn_file_name.str, MYF(MY_WME));
    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////

/**
  Drop trigger in the data dictionary.

  @param [in]  tables       pointer to trigger's table
  @param [in]  trigger_name name of the trigger to drop
  @param [in]  triggers     list of all table triggers

  @return Operation status.
    @retval true   Failure
    @retval false  Success
*/

bool Trigger_loader::drop_trigger(TABLE_LIST *tables,
                                  const char *trigger_name,
                                  List<Trigger> *triggers)
{
  // Create TRG-data with all table triggers but the trigger to drop.

  Trigger_data trg;
  bool trigger_found= false;

  {
    List_iterator<Trigger> it(*triggers);

    Trigger *t;
    while ((t= it++))
    {
      if (my_strcasecmp(table_alias_charset,
                        t->get_trigger_name()->str,
                        trigger_name) == 0)
      {
        it.remove(); // Remove trigger from the list.
        trigger_found= true;
        continue;
      }

      if (trg.append_trigger(t, &tables->table->mem_root))
        return true;
    }
  }

  // If no trigger found, report an error.

  if (!trigger_found)
  {
    my_message(ER_TRG_DOES_NOT_EXIST, ER(ER_TRG_DOES_NOT_EXIST), MYF(0));
    return true;
  }

  // Remove TRN file.

  if (rm_trn_file(tables->db, trigger_name))
    return true;

  // If we've just dropped the last trigger, remove TRG file. Otherwise, save
  // new TRG file.

  return triggers->is_empty() ?
         rm_trg_file(tables->db, tables->table_name) :
         save_trg_file(tables->db, tables->table_name, &trg);
}

///////////////////////////////////////////////////////////////////////////

/**
  Check whether the trigger specified by schema and name does exist
  in the data dictionary.

  @param [in]  db_name              name of schema
  @param [in]  trigger_name         name of trigger

  @return Operation status
    @retval true   Trigger exists. Set error in Diagnostic_area as
                   a side effect
    @retval false  Trigger doesn't exist
*/

bool Trigger_loader::check_for_uniqueness(const char *db_name,
                                          const char *trigger_name)
{
  char trn_file_name_buffer[FN_REFLEN];

  LEX_STRING trn_file_name= get_trn_file_name(trn_file_name_buffer, FN_REFLEN,
                                              db_name, trigger_name);

  if (!trn_file_name.str)
    return true;

  if (!access(trn_file_name.str, F_OK))
  {
    my_error(ER_TRG_ALREADY_EXISTS, MYF(0));
    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////

/**
  Get name of table by trigger name.

  @param [in]  thd              thread handle
  @param [in]  trigger_name     name of trigger
  @param [in]  trn_path         path to the corresponding TRN-file
  @param [out] tbl_name         variable to store retrieved table name

  @return Operation status
    @retval true   Failure.
    @retval false  Success.
*/

bool Trigger_loader::get_table_name_for_trigger(THD *thd,
                                                const LEX_STRING *trigger_name,
                                                const LEX_STRING *trn_path,
                                                LEX_STRING *tbl_name)
{
  DBUG_ENTER("Trigger_loader::get_table_name_for_trigger()");

  /* Prepare the File_parser to parse the TRN-file. */

  File_parser *parser= sql_parse_prepare(trn_path, thd->mem_root, true);

  if (!parser)
    DBUG_RETURN(true);

  if (!is_equal(&trn_file_type, parser->type()))
  {
    my_error(ER_WRONG_OBJECT, MYF(0),
             trigger_name->str,
             TRN_EXT + 1,
             "TRIGGERNAME");

    DBUG_RETURN(true);
  }

  /* Parse the TRN-file. */

  Trigger_name_data trn;

  Handle_old_incorrect_trigger_table_hook trigger_table_hook(
                                          trn_path->str,
                                          &trn.trigger_table);


  if (parser->parse((uchar *) &trn, thd->mem_root,
                    trn_file_parameters, 1,
                    &trigger_table_hook))
    DBUG_RETURN(true);

  /* Copy trigger table name. */

  *tbl_name= trn.trigger_table;

  /* That's all. */

  DBUG_RETURN(false);
}

///////////////////////////////////////////////////////////////////////////

/**
  Drop all triggers for the given table.
*/

bool Trigger_loader::drop_all_triggers(const char *db_name,
                                       const char *table_name,
                                       List<Trigger> *triggers)
{
  bool rc= false;

  List_iterator_fast<Trigger> it(*triggers);

  {
    Trigger *t;
    while ((t= it++))
    {
      if (rm_trn_file(db_name, t->get_trigger_name()->str))
      {
        rc= true;
        continue;
      }
    }
  }

  return rm_trg_file(db_name, table_name) || rc;
}

///////////////////////////////////////////////////////////////////////////

bool Trigger_loader::rename_table_in_trigger(TABLE *table,
                                             List<Trigger> *triggers,
                                             const char *old_db_name,
                                             LEX_STRING *old_table_name,
                                             const char *new_db_name,
                                             LEX_STRING *new_table_name,
                                             bool upgrading50to51)
{
  // Prepare TRG-data. Do it here so that OOM-error will not cause data
  // inconsistency.

  Trigger_data trg;

  if (fill_trg_data(&trg, &table->mem_root, triggers))
    return true;

  // Change the subject table name in TRN files for all triggers.

  LEX_STRING *err_trigname=
    change_table_name_in_trn_files(triggers,
                                   upgrading50to51 ? old_db_name : NULL,
                                   new_db_name, new_table_name,
                                   NULL);

  if (err_trigname)
  {
    /*
      If we were unable to update one of .TRN files properly we will
      revert all changes that we have done and report about error.
      We assume that we will be able to undo our changes without errors
      (we can't do much if there will be an error anyway).
    */
    change_table_name_in_trn_files(
      triggers,
      upgrading50to51 ? new_db_name : NULL,
      old_db_name, old_table_name,
      err_trigname);
    return true;
  }

  // Save new TRG file.

  if (save_trg_file(new_db_name, new_table_name->str, &trg))
    return true;

  // Remove old TRG file.

  if (rm_trg_file(old_db_name, old_table_name->str))
  {
    rm_trg_file(new_db_name, new_table_name->str);
    return true;
  }

  return false;
}
