/*
   Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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
#include "trigger.h"

#include "pfs_file_provider.h"
#include "mysql/psi/mysql_file.h"

#include "mysql/psi/mysql_sp.h"

///////////////////////////////////////////////////////////////////////////

const char * const TRN_EXT= ".TRN";
const char * const TRG_EXT= ".TRG";

///////////////////////////////////////////////////////////////////////////

/**
  This must be kept up to date whenever a new option is added to the list
  above, as it specifies the number of required parameters of the trigger in
  .trg file.
*/

static const int TRG_NUM_REQUIRED_PARAMETERS= 8;

const LEX_STRING trg_file_type= { C_STRING_WITH_LEN("TRIGGERS") };

const LEX_STRING trn_file_type= { C_STRING_WITH_LEN("TRIGGERNAME") };

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

/*
  Structure representing contents of .TRN file which are used to support
  database wide trigger namespace.
*/

struct Trn_file_data
{
  LEX_STRING trigger_table;
};

///////////////////////////////////////////////////////////////////////////

static File_option trn_file_parameters[]=
{
  {
    { C_STRING_WITH_LEN("trigger_table")},
    offsetof(struct Trn_file_data, trigger_table),
    FILE_OPTIONS_ESTRING
  },
  { { 0, 0 }, 0, FILE_OPTIONS_STRING }
};

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

/**
  Structure representing contents of .TRG file.
*/

struct Trg_file_data
{
  /// List of CREATE TRIGGER statements.
  List<LEX_STRING>  definitions;

  /// List of 'sql mode' values.
  List<ulonglong> sql_modes;

  /// List of 'definer' values.
  List<LEX_STRING>  definers_list;

  /// List of client character set names.
  List<LEX_STRING> client_cs_names;

  /// List of connection collation names.
  List<LEX_STRING> connection_cl_names;

  /// List of database collation names.
  List<LEX_STRING> db_cl_names;

  /// List of trigger creation time stamps
  List<longlong> created_timestamps;

  bool append_trigger(Trigger *t, MEM_ROOT *m)
  {
    return
      definitions.push_back(t->get_definition_ptr(), m) ||
      sql_modes.push_back(t->get_sql_mode_ptr(), m) ||
      definers_list.push_back(t->get_definer_ptr(), m) ||
      client_cs_names.push_back(t->get_client_cs_name_ptr(), m) ||
      connection_cl_names.push_back(t->get_connection_cl_name_ptr(), m) ||
      db_cl_names.push_back(t->get_db_cl_name_ptr(), m) ||
      created_timestamps.push_back(t->get_created_timestamp_ptr(), m);
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
    my_offsetof(struct Trg_file_data, definitions),
    FILE_OPTIONS_STRLIST
  },
  {
    { C_STRING_WITH_LEN("sql_modes") },
    my_offsetof(struct Trg_file_data, sql_modes),
    FILE_OPTIONS_ULLLIST
  },
  {
    { C_STRING_WITH_LEN("definers") },
    my_offsetof(struct Trg_file_data, definers_list),
    FILE_OPTIONS_STRLIST
  },
  {
    { C_STRING_WITH_LEN("client_cs_names") },
    my_offsetof(struct Trg_file_data, client_cs_names),
    FILE_OPTIONS_STRLIST
  },
  {
    { C_STRING_WITH_LEN("connection_cl_names") },
    my_offsetof(struct Trg_file_data, connection_cl_names),
    FILE_OPTIONS_STRLIST
  },
  {
    { C_STRING_WITH_LEN("db_cl_names") },
    my_offsetof(struct Trg_file_data, db_cl_names),
    FILE_OPTIONS_STRLIST
  },
  {
    { C_STRING_WITH_LEN("created") },
    my_offsetof(struct Trg_file_data, created_timestamps),
    FILE_OPTIONS_ULLLIST
  },
  { { 0, 0 }, 0, FILE_OPTIONS_STRING }
};

///////////////////////////////////////////////////////////////////////////

static File_option sql_modes_parameters=
{
  { C_STRING_WITH_LEN("sql_modes") },
  my_offsetof(struct Trg_file_data, sql_modes),
  FILE_OPTIONS_ULLLIST
};

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

LEX_STRING Trigger_loader::build_trn_path(char *trn_file_name_buffer,
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

  trn_file_name= NULL_STR;

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
                          const Trg_file_data *trg)
{
  char trg_file_name_buffer[FN_REFLEN];
  LEX_STRING trg_file_name;
  bool was_truncated= false;

  trg_file_name.length= build_table_filename(trg_file_name_buffer,
                                             FN_REFLEN - 1,
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

static bool fill_trg_data(Trg_file_data *trg,
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

  @param db_name         Old database of subject table
  @param new_db_name         New database of subject table
  @param new_table_name      New subject table's name
  @param stopper             Pointer to a trigger_name for
                             which we should stop updating.

  @retval NULL      Success
  @retval not-NULL  Failure, pointer to Table_trigger_dispatcher::names_list
                    element for which update failed.
*/

static Trigger *change_table_name_in_trn_files(
  List<Trigger> *triggers,
  const char *db_name,
  const char *new_db_name,
  const LEX_STRING *new_table_name,
  const Trigger *stopper)
{
  List_iterator_fast<Trigger> it(*triggers);
  Trigger *t;

  while ((t= it++))
  {
    if (t == stopper)
      break;

    // Get TRN file name.

    char trn_file_name_buffer[FN_REFLEN];

    LEX_STRING trn_file_name=
      Trigger_loader::build_trn_path(trn_file_name_buffer, FN_REFLEN,
                                     new_db_name, t->get_trigger_name().str);

    if (!trn_file_name.str)
      return NULL; // FIXME: OOM

    // Prepare TRN data.

    Trn_file_data trn;
    trn.trigger_table= *new_table_name;

    // Create new TRN file.

    if (sql_create_definition_file(NULL, &trn_file_name, &trn_file_type,
                                   (uchar *) &trn, trn_file_parameters))
    {
      return t;
    }

    // Remove stale .TRN file in case of database upgrade.

    if (db_name)
    {
      if (rm_trn_file(db_name, t->get_trigger_name().str))
      {
        rm_trn_file(new_db_name, t->get_trigger_name().str);
        return t;
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
  virtual bool process_unknown_string(const char *&unknown_key, uchar *base,
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
  virtual bool process_unknown_string(const char *&unknown_key, uchar *base,
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
  uchar *base,
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
                        path, "TRIGGER");
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
  uchar *base,
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
                        path, "TRIGGER");

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

/*
  Module private variables to be used in Trigger_loader::load_triggers().
*/

static LEX_STRING default_definer= EMPTY_STR;

static LEX_STRING default_client_cs_name= NULL_STR;
static LEX_STRING default_connection_cl_name= NULL_STR;
static LEX_STRING default_db_cl_name= NULL_STR;

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

/**
  Check if TRN-file exists.

  @param trn_path path to TRN-file

  @return true if TRN-file does not exists, false otherwise.
*/

bool Trigger_loader::check_trn_exists(const LEX_STRING &trn_path)
{
  return access(trn_path.str, F_OK) != 0;
}

///////////////////////////////////////////////////////////////////////////

/**
  Check if the TRG-file for the given table exists.

  @param db_name    name of schema
  @param table_name name of trigger

  @return true if TRG-file exists, false otherwise.
*/
bool Trigger_loader::trg_file_exists(const char *db_name,
                                     const char *table_name)
{
  char path[FN_REFLEN];
  build_table_filename(path, FN_REFLEN - 1, db_name, table_name, TRG_EXT, 0);

  if (access(path, F_OK))
  {
    if (errno == ENOENT)
      return false;
  }

  return true;
}


/**
  Load table triggers from the data dictionary.

  @param [in]  thd                thread handle
  @param [in]  db_name            name of schema
  @param [in]  table_name         subject table name
  @param [out] triggers           pointer to the list where new Trigger
                                  objects will be inserted

  @return Operation status
    @retval true   Failure
    @retval false  Success
*/

bool Trigger_loader::load_triggers(THD *thd,
                                   MEM_ROOT *mem_root,
                                   const char *db_name,
                                   const char *table_name,
                                   List<Trigger> *triggers)
{
  DBUG_ENTER("Trigger_loader::load_triggers");

  // Construct TRG-file name.

  char trg_file_path_buffer[FN_REFLEN];
  LEX_STRING trg_file_path;

  trg_file_path.length= build_table_filename(trg_file_path_buffer,
                                             FN_REFLEN - 1,
                                             db_name, table_name, TRG_EXT, 0);
  trg_file_path.str= trg_file_path_buffer;

  // The TRG-file exists so we got to load triggers.

  File_parser *parser=
    sql_parse_prepare(&trg_file_path, mem_root, true);

  if (!parser)
    DBUG_RETURN(true);

  if (!is_equal(&trg_file_type, parser->type()))
  {
    my_error(ER_WRONG_OBJECT, MYF(0), table_name, TRG_EXT + 1, "TRIGGER");
    DBUG_RETURN(true);
  }

  Handle_old_incorrect_sql_modes_hook sql_modes_hook(trg_file_path.str);

  Trg_file_data trg;

  if (parser->parse((uchar*) &trg,
                    mem_root,
                    trg_file_parameters,
                    TRG_NUM_REQUIRED_PARAMETERS,
                    &sql_modes_hook))
    DBUG_RETURN(true);

  if (trg.definitions.is_empty())
  {
    DBUG_ASSERT(trg.sql_modes.is_empty());
    DBUG_ASSERT(trg.definers_list.is_empty());
    DBUG_ASSERT(trg.client_cs_names.is_empty());
    DBUG_ASSERT(trg.connection_cl_names.is_empty());
    DBUG_ASSERT(trg.db_cl_names.is_empty());
    DBUG_RETURN(false);
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
               db_name,
               table_name);

      DBUG_RETURN(true);
    }

    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_TRG_NO_CREATION_CTX,
                        ER(ER_TRG_NO_CREATION_CTX),
                        db_name,
                        table_name);


    /*
      Backward compatibility: assume that the query is in the current
      character set.
    */

    lex_string_set(&default_client_cs_name,
                   thd->variables.character_set_client->csname);

    lex_string_set(&default_connection_cl_name,
                   thd->variables.collation_connection->name);

    lex_string_set(&default_db_cl_name,
                   thd->variables.collation_database->name);
  }

  LEX_CSTRING db_name_str= {db_name, strlen(db_name)};

  LEX_CSTRING table_name_str= {table_name, strlen(table_name)};

  List_iterator_fast<LEX_STRING> it_definition(trg.definitions);
  List_iterator_fast<sql_mode_t> it_sql_mode(trg.sql_modes);
  List_iterator_fast<LEX_STRING> it_definer(trg.definers_list);
  List_iterator_fast<LEX_STRING> it_client_cs_name(trg.client_cs_names);
  List_iterator_fast<LEX_STRING> it_connect_cl_name(trg.connection_cl_names);
  List_iterator_fast<LEX_STRING> it_db_cl_name(trg.db_cl_names);
  List_iterator_fast<longlong>  it_created_timestamps(trg.created_timestamps);

  while (true)
  {
    const LEX_STRING *definition= it_definition++;

    if (!definition)
      break;

    const sql_mode_t *sql_mode= it_sql_mode++;
    const LEX_STRING *definer= it_definer++;
    const LEX_STRING *client_cs_name= it_client_cs_name++;
    const LEX_STRING *connection_cl_name= it_connect_cl_name++;
    const LEX_STRING *db_cl_name= it_db_cl_name++;
    const longlong *created_timestamp= it_created_timestamps++;

    // Backward compatibility: use default settings if attributes are missing.

    if (!sql_mode)
      sql_mode= &global_system_variables.sql_mode;

    if (!definer)
      definer= &default_definer;

    if (!client_cs_name)
      client_cs_name= &default_client_cs_name;

    if (!connection_cl_name)
      connection_cl_name= &default_connection_cl_name;

    if (!db_cl_name)
      db_cl_name= &default_db_cl_name;

    // Create a new trigger instance.

    Trigger *t= Trigger::create_from_dd(mem_root,
                                        db_name_str,
                                        table_name_str,
                                        *definition,
                                        *sql_mode,
                                        *definer,
                                        *client_cs_name,
                                        *connection_cl_name,
                                        *db_cl_name,
                                        created_timestamp);

    // NOTE: new trigger object is not fully initialized here.

    if (triggers->push_back(t, mem_root))
    {
      delete t;
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

bool Trigger_loader::store_trigger(const LEX_STRING &db_name,
                                   const LEX_STRING &table_name,
                                   MEM_ROOT *mem_root,
                                   Trigger *new_trigger,
                                   List<Trigger> *triggers)
{
  // Fill TRN-data structure.

  Trn_file_data trn;

  trn.trigger_table= table_name;

  // Fill TRG-data structure.

  Trg_file_data trg;

  if (fill_trg_data(&trg, mem_root, triggers))
    return true;

  // Get TRN file name.

  char trn_file_name_buffer[FN_REFLEN];

  LEX_STRING trn_file_name=
    Trigger_loader::build_trn_path(trn_file_name_buffer, FN_REFLEN,
                                   db_name.str,
                                   new_trigger->get_trigger_name().str);

  if (!trn_file_name.str)
    return true; // my_error() has already been called.

  // Save TRN file.

  if (sql_create_definition_file(NULL, &trn_file_name, &trn_file_type,
                                 (uchar *) &trn, trn_file_parameters))
  {
    return true; // my_error() has already been called.
  }

  // Save TRG file.

  if (save_trg_file(db_name.str, table_name.str, &trg))
  {
    mysql_file_delete(key_file_trn, trn_file_name.str, MYF(MY_WME));
    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////

/**
  Drop trigger in the data dictionary.

  @param [in]  tables         pointer to trigger's table
  @param [in]  trigger_name   name of the trigger to drop
  @param [in]  triggers       list of all table triggers
  @param [out] trigger_found  flag to store a result whether
                              the named trigger was found

  @return Operation status.
    @retval true   Failure
    @retval false  Success
*/

bool Trigger_loader::drop_trigger(const LEX_STRING &db_name,
                                  const LEX_STRING &table_name,
                                  const LEX_STRING &trigger_name,
                                  MEM_ROOT *mem_root,
                                  List<Trigger> *triggers,
                                  bool *trigger_found)
{
  // Create TRG-data with all table triggers but the trigger to drop.

  Trg_file_data trg;
  *trigger_found= false;

  {
    List_iterator<Trigger> it(*triggers);
    Trigger *t;

    while ((t= it++))
    {
      if (my_strcasecmp(table_alias_charset,
                        t->get_trigger_name().str,
                        trigger_name.str) == 0)
      {
        delete t;
        it.remove(); // Remove trigger from the list.
        *trigger_found= true;
        continue;
      }

      if (trg.append_trigger(t, mem_root))
        return true;
    }
  }

  // Remove TRN file.

  if (rm_trn_file(db_name.str, trigger_name.str))
    return true;

  // If we've just dropped the last trigger, remove TRG file. Otherwise, save
  // new TRG file.

  return triggers->is_empty() ?
         rm_trg_file(db_name.str, table_name.str) :
         save_trg_file(db_name.str, table_name.str, &trg);
}

///////////////////////////////////////////////////////////////////////////

/**
  Load trigger table name from TRN-file.

  @param [in]  thd              thread handle
  @param [in]  trigger_name     name of trigger
  @param [in]  trn_path         path to the corresponding TRN-file
  @param [out] tbl_name         variable to store retrieved table name

  @return Operation status
    @retval true   Failure.
    @retval false  Success.
*/

bool Trigger_loader::load_trn_file(THD *thd,
                                   const LEX_STRING &trigger_name,
                                   const LEX_STRING &trn_path,
                                   LEX_STRING *tbl_name)
{
  DBUG_ENTER("Trigger_loader::get_table_name_for_trigger()");

  /* Prepare the File_parser to parse the TRN-file. */

  File_parser *parser= sql_parse_prepare(&trn_path, thd->mem_root, true);

  if (!parser)
    DBUG_RETURN(true);

  if (!is_equal(&trn_file_type, parser->type()))
  {
    my_error(ER_WRONG_OBJECT, MYF(0),
             trigger_name.str,
             TRN_EXT + 1,
             "TRIGGERNAME");

    DBUG_RETURN(true);
  }

  /* Parse the TRN-file. */

  Trn_file_data trn;

  Handle_old_incorrect_trigger_table_hook trigger_table_hook(
                                          trn_path.str,
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
  Trigger *t;

  while ((t= it++))
  {
    LEX_STRING trigger_name= t->get_trigger_name();
    if (rm_trn_file(db_name, trigger_name.str))
    {
      rc= true;
      continue;
    }
#ifdef HAVE_PSI_SP_INTERFACE                                                    
    LEX_CSTRING db_name= t->get_db_name();
    /* Drop statistics for this stored program from performance schema. */      
    MYSQL_DROP_SP(SP_TYPE_TRIGGER,                                       
                  db_name.str, db_name.length,    
                  trigger_name.str, trigger_name.length);
#endif
  }

  return rm_trg_file(db_name, table_name) || rc;
}

///////////////////////////////////////////////////////////////////////////

bool Trigger_loader::rename_subject_table(MEM_ROOT *mem_root,
                                          List<Trigger> *triggers,
                                          const char *db_name,
                                          LEX_STRING *table_name,
                                          const char *new_db_name,
                                          LEX_STRING *new_table_name,
                                          bool upgrading50to51)
{
  // Prepare TRG-data. Do it here so that OOM-error will not cause data
  // inconsistency.

  Trg_file_data trg;

  if (fill_trg_data(&trg, mem_root, triggers))
    return true;

  // Change the subject table name in TRN files for all triggers.

  Trigger *err_trigger=
    change_table_name_in_trn_files(triggers,
                                   upgrading50to51 ? db_name : NULL,
                                   new_db_name, new_table_name,
                                   NULL);

  if (err_trigger)
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
      db_name, table_name,
      err_trigger);
    return true;
  }

  // Save new TRG file.

  if (save_trg_file(new_db_name, new_table_name->str, &trg))
    return true;

  // Remove old TRG file.

  if (rm_trg_file(db_name, table_name->str))
  {
    rm_trg_file(new_db_name, new_table_name->str);
    return true;
  }

  return false;
}
