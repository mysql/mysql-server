/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/dd/dd_upgrade.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <memory>
#include <string>
#include <vector>

#include "dd/cache/dictionary_client.h"       // dd::cache::Dictionary_client
#include "dd/dd.h"                            // dd::get_dictionary
#include "dd/dd_event.h"                      // create_event
#include "dd/dd_schema.h"                     // create_schema
#include "dd/dd_table.h"                      // create_dd_user_table
#include "dd/dd_tablespace.h"                 // create_tablespace
#include "dd/dd_trigger.h"                    // dd::create_trigger
#include "dd/dd_view.h"                       // create_view
#include "dd/impl/bootstrapper.h"             // execute_query
#include "dd/impl/dictionary_impl.h"          // dd::Dictionary_impl
#include "dd/types/object_type.h"             // dd::Object_type
#include "dd/types/table.h"                   // dd::Table
#include "dd/types/tablespace.h"              // dd::Tablespace
#include "derror.h"                           // ER_DEFAULT
#include "event_db_repository.h"              // Events
#include "event_parse_data.h"                 // Event_parse_data
#include "handler.h"                          // legacy_db_type
#include "lock.h"                             // Tablespace_hash_set
#include "log.h"                              // sql_print_warning
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_user.h"                          // parse_user
#include "mysql/psi/mysql_file.h"             // mysql_file_open
#include "mysqld.h"                           // mysql_real_data_home
#include "parse_file.h"                       // File_option
#include "partition_info.h"                   // partition_info
#include "psi_memory_key.h"                   // key_memory_TABLE
#include "sp.h"                               // db_load_routine
#include "sp_head.h"                          // sp_head
#include "sql_base.h"                         // open_tables
#include "sql_lex.h"                          // new_empty_query_block
#include "sql_parse.h"                        // check_string_char_length
#include "sql_partition.h"                    // mysql_unpack_partition
#include "sql_plugin.h"                       // plugin_unlock
#include "sql_prepare.h"                      // Ed_connection
#include "sql_show.h"                         // view_store_options
#include "sql_table.h"                        // build_tablename
#include "sql_time.h"                         // interval_type_to_name
#include "sql_view.h"                         // mysql_create_view
#include "table.h"                            // Table_check_intact
#include "table_trigger_dispatcher.h"         // Table_trigger_dispatcher
#include "transaction.h"                      // trans_commit
#include "trigger.h"                          // Trigger
#include "tztime.h"                           // my_tz_find

namespace dd {

const String_type ISL_EXT= ".isl";
const String_type PAR_EXT= ".par";
const String_type OPT_EXT= ".opt";
const String_type SDI_EXT= ".SDI";
const char *TRN_EXT= ".TRN";
const char *TRG_EXT= ".TRG";


/*
  Custom version of standard offsetof() macro which can be used to get
  offsets of members in class for non-POD types (according to the current
  version of C++ standard offsetof() macro can't be used in such cases and
  attempt to do so causes warnings to be emitted, OTOH in many cases it is
  still OK to assume that all instances of the class has the same offsets
  for the same members).

  This is temporary solution which should be removed once File_parser class
  and related routines are refactored.
*/

#define my_offsetof_upgrade(TYPE, MEMBER) \
        ((size_t)((char *)&(((TYPE *)0x10)->MEMBER) - (char*)0x10))


/**
  Class to check the system tables we are using from 5.7 are
  not corrupted before migrating the information to new DD.
*/
class Check_table_intact : public Table_check_intact
{
protected:
  void report_error(uint, const char *fmt, ...)
  {
    va_list args;
    char buff[MYSQL_ERRMSG_SIZE];
    va_start(args, fmt);
    my_vsnprintf(buff, sizeof(buff), fmt, args);
    va_end(args);

    sql_print_error("%s", buff);
  }
};

static Check_table_intact table_intact;


/**
  Column definitions for 5.7 mysql.proc table (5.7.13 and up).
*/
static const
TABLE_FIELD_TYPE proc_table_fields[MYSQL_PROC_FIELD_COUNT] =
{
  {
    { C_STRING_WITH_LEN("db") },
    { C_STRING_WITH_LEN("char(64)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("name") },
    { C_STRING_WITH_LEN("char(64)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("type") },
    { C_STRING_WITH_LEN("enum('FUNCTION','PROCEDURE')") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("specific_name") },
    { C_STRING_WITH_LEN("char(64)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("language") },
    { C_STRING_WITH_LEN("enum('SQL')") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("sql_data_access") },
    { C_STRING_WITH_LEN("enum('CONTAINS_SQL','NO_SQL','READS_SQL_DATA','MODIFIES_SQL_DATA')") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("is_deterministic") },
    { C_STRING_WITH_LEN("enum('YES','NO')") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("security_type") },
    { C_STRING_WITH_LEN("enum('INVOKER','DEFINER')") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("param_list") },
    { C_STRING_WITH_LEN("blob") },
    { NULL, 0 }
  },

  {
    { C_STRING_WITH_LEN("returns") },
    { C_STRING_WITH_LEN("longblob") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("body") },
    { C_STRING_WITH_LEN("longblob") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("definer") },
    { C_STRING_WITH_LEN("char(93)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("created") },
    { C_STRING_WITH_LEN("timestamp") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("modified") },
    { C_STRING_WITH_LEN("timestamp") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("sql_mode") },
        { C_STRING_WITH_LEN("set('REAL_AS_FLOAT','PIPES_AS_CONCAT','ANSI_QUOTES',"
    "'IGNORE_SPACE','NOT_USED','ONLY_FULL_GROUP_BY','NO_UNSIGNED_SUBTRACTION',"
    "'NO_DIR_IN_CREATE','POSTGRESQL','ORACLE','MSSQL','DB2','MAXDB',"
    "'NO_KEY_OPTIONS','NO_TABLE_OPTIONS','NO_FIELD_OPTIONS','MYSQL323','MYSQL40',"
    "'ANSI','NO_AUTO_VALUE_ON_ZERO','NO_BACKSLASH_ESCAPES','STRICT_TRANS_TABLES',"
    "'STRICT_ALL_TABLES','NO_ZERO_IN_DATE','NO_ZERO_DATE','INVALID_DATES',"
    "'ERROR_FOR_DIVISION_BY_ZERO','TRADITIONAL','NO_AUTO_CREATE_USER',"
    "'HIGH_NOT_PRECEDENCE','NO_ENGINE_SUBSTITUTION','PAD_CHAR_TO_FULL_LENGTH')") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("comment") },
    { C_STRING_WITH_LEN("text") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("character_set_client") },
    { C_STRING_WITH_LEN("char(32)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("collation_connection") },
    { C_STRING_WITH_LEN("char(32)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("db_collation") },
    { C_STRING_WITH_LEN("char(32)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("body_utf8") },
    { C_STRING_WITH_LEN("longblob") },
    { NULL, 0 }
  }
};

static const TABLE_FIELD_DEF
  proc_table_def= {MYSQL_PROC_FIELD_COUNT, proc_table_fields};


/**
  Column definitions for 5.7 mysql.proc table (before 5.7.13).
*/

static const
TABLE_FIELD_TYPE proc_table_fields_old[MYSQL_PROC_FIELD_COUNT] =
{
  {
    { C_STRING_WITH_LEN("db") },
    { C_STRING_WITH_LEN("char(64)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("name") },
    { C_STRING_WITH_LEN("char(64)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("type") },
    { C_STRING_WITH_LEN("enum('FUNCTION','PROCEDURE')") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("specific_name") },
    { C_STRING_WITH_LEN("char(64)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("language") },
    { C_STRING_WITH_LEN("enum('SQL')") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("sql_data_access") },
    { C_STRING_WITH_LEN("enum('CONTAINS_SQL','NO_SQL','READS_SQL_DATA','MODIFIES_SQL_DATA')") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("is_deterministic") },
    { C_STRING_WITH_LEN("enum('YES','NO')") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("security_type") },
    { C_STRING_WITH_LEN("enum('INVOKER','DEFINER')") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("param_list") },
    { C_STRING_WITH_LEN("blob") },
    { NULL, 0 }
  },

  {
    { C_STRING_WITH_LEN("returns") },
    { C_STRING_WITH_LEN("longblob") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("body") },
    { C_STRING_WITH_LEN("longblob") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("definer") },
    { C_STRING_WITH_LEN("char(77)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("created") },
    { C_STRING_WITH_LEN("timestamp") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("modified") },
    { C_STRING_WITH_LEN("timestamp") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("sql_mode") },
    { C_STRING_WITH_LEN("set('REAL_AS_FLOAT','PIPES_AS_CONCAT','ANSI_QUOTES',"
    "'IGNORE_SPACE','NOT_USED','ONLY_FULL_GROUP_BY','NO_UNSIGNED_SUBTRACTION',"
    "'NO_DIR_IN_CREATE','POSTGRESQL','ORACLE','MSSQL','DB2','MAXDB',"
    "'NO_KEY_OPTIONS','NO_TABLE_OPTIONS','NO_FIELD_OPTIONS','MYSQL323','MYSQL40',"
    "'ANSI','NO_AUTO_VALUE_ON_ZERO','NO_BACKSLASH_ESCAPES','STRICT_TRANS_TABLES',"
    "'STRICT_ALL_TABLES','NO_ZERO_IN_DATE','NO_ZERO_DATE','INVALID_DATES',"
    "'ERROR_FOR_DIVISION_BY_ZERO','TRADITIONAL','NO_AUTO_CREATE_USER',"
    "'HIGH_NOT_PRECEDENCE','NO_ENGINE_SUBSTITUTION','PAD_CHAR_TO_FULL_LENGTH')") },
    { NULL, 0 }
  },
  {
    { C_STRING_WITH_LEN("comment") },
    { C_STRING_WITH_LEN("text") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("character_set_client") },
    { C_STRING_WITH_LEN("char(32)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("collation_connection") },
    { C_STRING_WITH_LEN("char(32)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("db_collation") },
    { C_STRING_WITH_LEN("char(32)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("body_utf8") },
    { C_STRING_WITH_LEN("longblob") },
    { NULL, 0 }
  }
};

static const TABLE_FIELD_DEF
  proc_table_def_old= {MYSQL_PROC_FIELD_COUNT, proc_table_fields_old};


/**
  Class to handle loading and parsing of Triggers.
  This class is nececssary for loading triggers in
  case of upgrade from 5.7
*/

class Trigger_loader
{
public:
  static bool trg_file_exists(const char *db_name,
                              const char *table_name);

  static bool load_triggers(THD *thd,
                            MEM_ROOT *mem_root,
                            const char *db_name,
                            const char *table_name,
                            List<::Trigger> *triggers);
};


static const int TRG_NUM_REQUIRED_PARAMETERS= 8;

const LEX_STRING trg_file_type= { C_STRING_WITH_LEN("TRIGGERS") };

/**
  Structure representing contents of .TRG file.
*/

struct Trg_file_data
{
  // List of CREATE TRIGGER statements.
  List<LEX_STRING>  definitions;

  // List of 'sql mode' values.
  List<ulonglong> sql_modes;

  // List of 'definer' values.
  List<LEX_STRING>  definers_list;

  // List of client character set names.
  List<LEX_STRING> client_cs_names;

  // List of connection collation names.
  List<LEX_STRING> connection_cl_names;

  // List of database collation names.
  List<LEX_STRING> db_cl_names;

  // List of trigger creation time stamps
  List<longlong> created_timestamps;
};


/**
  Table of .TRG file field descriptors.
*/

static File_option trg_file_parameters[]=
{
  {
    { C_STRING_WITH_LEN("triggers") },
    my_offsetof_upgrade(struct Trg_file_data, definitions),
    FILE_OPTIONS_STRLIST
  },
  {
    { C_STRING_WITH_LEN("sql_modes") },
    my_offsetof_upgrade(struct Trg_file_data, sql_modes),
    FILE_OPTIONS_ULLLIST
  },
  {
    { C_STRING_WITH_LEN("definers") },
    my_offsetof_upgrade(struct Trg_file_data, definers_list),
    FILE_OPTIONS_STRLIST
  },
  {
    { C_STRING_WITH_LEN("client_cs_names") },
    my_offsetof_upgrade(struct Trg_file_data, client_cs_names),
    FILE_OPTIONS_STRLIST
  },
  {
    { C_STRING_WITH_LEN("connection_cl_names") },
    my_offsetof_upgrade(struct Trg_file_data, connection_cl_names),
    FILE_OPTIONS_STRLIST
  },
  {
    { C_STRING_WITH_LEN("db_cl_names") },
    my_offsetof_upgrade(struct Trg_file_data, db_cl_names),
    FILE_OPTIONS_STRLIST
  },
  {
    { C_STRING_WITH_LEN("created") },
    my_offsetof_upgrade(struct Trg_file_data, created_timestamps),
    FILE_OPTIONS_ULLLIST
  },
  { { 0, 0 }, 0, FILE_OPTIONS_STRING }
};


static File_option sql_modes_parameters=
{
  { C_STRING_WITH_LEN("sql_modes") },
  my_offsetof_upgrade(struct Trg_file_data, sql_modes),
  FILE_OPTIONS_ULLLIST
};


/*
  Module private variables to be used in Trigger_loader::load_triggers().
*/

static LEX_STRING default_client_cs_name= NULL_STR;
static LEX_STRING default_connection_cl_name= NULL_STR;
static LEX_STRING default_db_cl_name= NULL_STR;


class Handle_old_incorrect_sql_modes_hook: public Unknown_key_hook
{
private:
  char *m_path;
public:
  Handle_old_incorrect_sql_modes_hook(char *file_path)
    : m_path(file_path)
  {};
  virtual bool process_unknown_string(const char *&unknown_key, uchar *base,
                                      MEM_ROOT *mem_root, const char *end);
};


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
  Load table triggers from .TRG file.

  @param [in]  thd                thread handle
  @param [in]  mem_root           MEM_ROOT pointer
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
                                   List<::Trigger> *triggers)
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

    sql_print_warning(ER_DEFAULT(ER_TRG_NO_CREATION_CTX),
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
    {
      // We dont know trigger name yet.
      sql_print_error("Definer clause is missing in Trigger of Table %s. "
                      "Rebuild Trigger to fix definer.", table_name);
      return true;
    }

    if (!client_cs_name)
    {
      sql_print_warning("Client character set is missing for trigger of table "
                        "%s. Using default character set.", table_name);
      client_cs_name= &default_client_cs_name;
    }

    if (!connection_cl_name)
    {
      sql_print_warning("Connection collation is missing for trigger of table "
                        "%s. Using default connection collation.", table_name);
      connection_cl_name= &default_connection_cl_name;
    }

    if (!db_cl_name)
    {
      sql_print_warning("Database collation is missing for trigger of table "
                        "%s. Using Default character set.", table_name);
      db_cl_name= &default_db_cl_name;
    }

    char tmp_body_utf8[]= "temp_utf8_definition";
    LEX_CSTRING body_utf8= { tmp_body_utf8, strlen(tmp_body_utf8) };


    // Allocate space to hold username and hostname.
    char *pos= NULL;
    if (!(pos= static_cast<char*>(alloc_root(mem_root, USERNAME_LENGTH + 1))))
    {
      sql_print_error("Error in Memory allocation for "
                      "Definer User for Trigger.");
      return true;
    }

    LEX_STRING definer_user{pos, 0};

    if (!(pos= static_cast<char*>(alloc_root(mem_root, USERNAME_LENGTH + 1))))
    {
      sql_print_error("Error in Memory allocation for "
                      "Definer Host for Trigger.");
      return true;
    }

    LEX_STRING definer_host{pos, 0};


    // Parse user string to separate user name and host
    parse_user(definer->str, definer->length,
               definer_user.str,
               &definer_user.length,
               definer_host.str,
               &definer_host.length);

    LEX_CSTRING definer_user_name{ definer_user.str, definer_user.length };
    LEX_CSTRING definer_host_name{ definer_host.str, definer_host.length };

    // Set timeval to use for Created field.
    timeval timestamp_value;
    timestamp_value.tv_sec= static_cast<long>(*created_timestamp / 100);
    timestamp_value.tv_usec= (*created_timestamp % 100) * 10000;

    // Create temporary Trigger name to be fixed while parsing.
    // parse_triggers() will fix this.
    char temp_trigger_name[]= "temporary_trigger_name";
    LEX_CSTRING tmp_name= { temp_trigger_name, strlen(temp_trigger_name) };

    // Create definition as LEX_CSTRING
    LEX_CSTRING orig_definition= { definition->str, definition->length };

    // Create client_character_set as LEX_CSTRING
    LEX_CSTRING client_cs= { client_cs_name->str, client_cs_name->length };

    // Create connection_collation as LEX_CSTRING
    LEX_CSTRING cn_cl= { connection_cl_name->str, connection_cl_name->length };

    // Create database_collation as LEX_CSTRING
    LEX_CSTRING db_cl= { db_cl_name->str, db_cl_name->length };

    // Create a new trigger instance.
    ::Trigger *t= ::Trigger::create_from_dd(
            mem_root,
            tmp_name,
            db_name_str,
            table_name_str,
            orig_definition,
            body_utf8,
            *sql_mode,
            definer_user_name,
            definer_host_name,
            client_cs,
            cn_cl,
            db_cl,
            enum_trigger_event_type::TRG_EVENT_MAX,
            enum_trigger_action_time_type::TRG_ACTION_MAX,
            0,
            timestamp_value);

    /*
      NOTE: new trigger object is not fully initialized here.
      Initialization of definer, trigger name, action time, action event
      will be done in parse_triggers().
    */
    if (triggers->push_back(t, mem_root))
    {
      delete t;
      DBUG_RETURN(true);
    }
  }

  DBUG_RETURN(false);
}


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
    sql_print_warning(ER_DEFAULT(ER_OLD_FILE_FORMAT),
                      m_path, "TRIGGER");
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
    unknown_key= ptr - 1;
  }
  DBUG_RETURN(false);
}


/**
  RAII to handle MDL locks while upgrading.
*/

class Upgrade_MDL_guard
{
  MDL_ticket *m_mdl_ticket_schema;
  MDL_ticket *m_mdl_ticket_table;
  bool m_tablespace_lock;

  THD *m_thd;
public:
  bool acquire_lock(const String_type &db_name, const String_type &table_name)
  {
    return dd::acquire_exclusive_schema_mdl(m_thd, db_name.c_str(),
                                            false, &m_mdl_ticket_schema) ||
           dd::acquire_exclusive_table_mdl(m_thd, db_name.c_str(),
                                           table_name.c_str(),
                                           false, &m_mdl_ticket_table);
  }
  bool acquire_lock_tablespace(Tablespace_hash_set *tablespace_names)
  {
    m_tablespace_lock= true;
    return lock_tablespace_names(m_thd, tablespace_names,
                                 m_thd->variables.lock_wait_timeout);
  }

  Upgrade_MDL_guard(THD *thd)
    : m_mdl_ticket_schema(nullptr), m_mdl_ticket_table(nullptr),
      m_tablespace_lock(false), m_thd(thd)
  {}
  ~Upgrade_MDL_guard()
  {
    if (m_mdl_ticket_schema != nullptr)
      dd::release_mdl(m_thd, m_mdl_ticket_schema);
    if (m_mdl_ticket_table != nullptr)
      dd::release_mdl(m_thd, m_mdl_ticket_table);
    // Release transactional locks acquired.
    if (m_tablespace_lock)
      m_thd->mdl_context.release_transactional_locks();
  }
};


/**
   RAII for handling open and close of event and proc tables.
*/

class System_table_close_guard
{
  THD* m_thd;
  TABLE *m_table;
  MEM_ROOT *m_mem_root;

public:
  System_table_close_guard(THD *thd, TABLE *table)
    : m_thd(thd), m_table(table)
  {
    m_mem_root= m_thd->mem_root;
  }
  ~System_table_close_guard()
  {
    m_thd->mem_root= m_mem_root;
    if (m_table->file->inited)
      (void) m_table->file->ha_index_end();
    close_thread_tables(m_thd);
  }
};


/**
   RAII for handling creation context of Events and
   Stored routines.
*/

class Routine_event_context_guard
{
  THD *m_thd;
  sql_mode_t m_sql_mode;
  Time_zone *m_saved_time_zone;
  const CHARSET_INFO *m_client_cs;
  const CHARSET_INFO *m_connection_cl;

public:
  Routine_event_context_guard(THD *thd): m_thd(thd)
  {
    m_sql_mode= m_thd->variables.sql_mode;
    m_client_cs= m_thd->variables.character_set_client;
    m_connection_cl= m_thd->variables.collation_connection;
    m_saved_time_zone= m_thd->variables.time_zone;
  }
  ~Routine_event_context_guard()
  {
    m_thd->variables.sql_mode= m_sql_mode;
    m_thd->variables.character_set_client= m_client_cs;
    m_thd->variables.collation_connection= m_connection_cl;
    m_thd->variables.time_zone= m_saved_time_zone;
  }
};


/**
  RAII to handle cleanup after table upgrading.
*/

class Table_upgrade_guard
{
  THD *m_thd;
  TABLE *m_table;
  MEM_ROOT *m_mem_root;
  sql_mode_t m_sql_mode;
  handler *m_handler;
  bool m_is_table_open;
  LEX *m_lex_saved;
public:

  void update_mem_root(MEM_ROOT *mem_root)
  {
    m_mem_root= mem_root;
  }

  void update_handler(handler *handler)
  {
    m_handler= handler;
  }

  void update_lex(LEX *lex)
  {
    m_lex_saved= lex;
  }

  void set_is_table_open(bool param)
  {
    m_is_table_open= param;
  }

  Table_upgrade_guard(THD *thd, TABLE *table, MEM_ROOT *mem_root)
    :  m_thd(thd), m_table(table), m_mem_root(mem_root), m_handler(nullptr),
       m_is_table_open(false), m_lex_saved(nullptr)
  {
    m_sql_mode= m_thd->variables.sql_mode;
    m_thd->variables.sql_mode= m_sql_mode;
  }

  ~Table_upgrade_guard()
  {
    m_thd->variables.sql_mode= m_sql_mode;
    m_thd->work_part_info= 0;

    // Free item list for partitions
    if (m_table->s->m_part_info)
      free_items(m_table->s->m_part_info->item_free_list);

    // Restore thread lex
    if (m_lex_saved != nullptr)
    {
      lex_end(m_thd->lex);
      m_thd->lex= m_lex_saved;
    }

    /*
      Free item list for generated columns
      Items being freed were allocated by fix_generated_columns_for_upgrade(),
      and TABLE instance might have its own items allocated which will be freed
      by closefrm() call.
    */
    if (m_table->s->field)
    {
      for (Field **ptr=m_table->s->field ; *ptr ; ptr++)
      {
        if ((*ptr)->gcol_info)
          free_items((*ptr)->gcol_info->item_free_list);
      }
    }

    // Close the table. It was opened using ha_open for FK information.
    if (m_is_table_open)
    {
      (void) closefrm(m_table, false);
    }

    free_table_share(m_table->s);

    delete m_handler;
    /*
      Make a copy of mem_root as TABLE object is allocated within its
      own mem_root and free_root() updates its argument.
    */
    MEM_ROOT m_root= std::move(*m_mem_root);
    free_root(&m_root, MYF(0));
  }
};


/**
  Fill HA_CREATE_INFO from TABLE_SHARE.
*/

static void  fill_create_info_for_upgrade(HA_CREATE_INFO *create_info,
                                          const TABLE *table)
{
  /*
    Storage Engine names will be resolved when reading .frm file.
    We can assume here that SE is present and initialized.
  */
  create_info->db_type= table->s->db_type();

  create_info->init_create_options_from_share(table->s, 0);

  create_info->row_type= table->s->row_type;

  // DD framework handles only these options
  uint db_create_options= table->s->db_create_options;
  db_create_options &= (HA_OPTION_PACK_RECORD|
                        HA_OPTION_PACK_KEYS|HA_OPTION_NO_PACK_KEYS|
                        HA_OPTION_CHECKSUM|HA_OPTION_NO_CHECKSUM|
                        HA_OPTION_DELAY_KEY_WRITE|HA_OPTION_NO_DELAY_KEY_WRITE|
                        HA_OPTION_STATS_PERSISTENT|HA_OPTION_NO_STATS_PERSISTENT);
  create_info->table_options= db_create_options;
}


static const int REQUIRED_VIEW_PARAMETERS= 12;

/*
  Table of VIEW .frm field descriptors

  Note that one should NOT change the order for this,
  as it's used by parse().
*/
static File_option view_parameters[]=
{{{ C_STRING_WITH_LEN("query")},
  my_offsetof_upgrade(TABLE_LIST, select_stmt),
  FILE_OPTIONS_ESTRING},
 {{ C_STRING_WITH_LEN("updatable")},
  my_offsetof_upgrade(TABLE_LIST, updatable_view),
  FILE_OPTIONS_ULONGLONG},
 {{ C_STRING_WITH_LEN("algorithm")},
  my_offsetof_upgrade(TABLE_LIST, algorithm),
  FILE_OPTIONS_ULONGLONG},
 {{ C_STRING_WITH_LEN("definer_user")},
  my_offsetof_upgrade(TABLE_LIST, definer.user),
  FILE_OPTIONS_STRING},
 {{ C_STRING_WITH_LEN("definer_host")},
  my_offsetof_upgrade(TABLE_LIST, definer.host),
  FILE_OPTIONS_STRING},
 {{ C_STRING_WITH_LEN("suid")},
  my_offsetof_upgrade(TABLE_LIST, view_suid),
  FILE_OPTIONS_ULONGLONG},
 {{ C_STRING_WITH_LEN("with_check_option")},
  my_offsetof_upgrade(TABLE_LIST, with_check),
  FILE_OPTIONS_ULONGLONG},
 {{ C_STRING_WITH_LEN("timestamp")},
  my_offsetof_upgrade(TABLE_LIST, timestamp),
  FILE_OPTIONS_TIMESTAMP},
 {{ C_STRING_WITH_LEN("source")},
  my_offsetof_upgrade(TABLE_LIST, source),
  FILE_OPTIONS_ESTRING},
 {{(char*) STRING_WITH_LEN("client_cs_name")},
  my_offsetof_upgrade(TABLE_LIST, view_client_cs_name),
  FILE_OPTIONS_STRING},
 {{(char*) STRING_WITH_LEN("connection_cl_name")},
  my_offsetof_upgrade(TABLE_LIST, view_connection_cl_name),
  FILE_OPTIONS_STRING},
 {{(char*) STRING_WITH_LEN("view_body_utf8")},
  my_offsetof_upgrade(TABLE_LIST, view_body_utf8),
  FILE_OPTIONS_ESTRING},
 {{NullS, 0},                   0,
  FILE_OPTIONS_STRING}
};


/**
  Create the view in DD without its column and dependency
  information.

  @param[in] thd       Thread handle.
  @param[in] view_ref  TABLE_LIST to store view data.

  @retval false  ON SUCCESS
  @retval true   ON FAILURE
*/
static bool create_unlinked_view(THD *thd,
                                 TABLE_LIST *view_ref)
{
  SELECT_LEX *backup_select= thd->lex->select_lex;
  TABLE_LIST *saved_query_tables= thd->lex->query_tables;
  SQL_I_List<Sroutine_hash_entry> saved_sroutines_list;
  // For creation of view without column information.
  SELECT_LEX select(nullptr, nullptr);

  // Backup
  thd->lex->select_lex= &select;
  thd->lex->query_tables= NULL;
  thd->lex->sroutines_list.save_and_clear(&saved_sroutines_list);

  const dd::Schema *schema= nullptr;
  if (thd->dd_client()->acquire(view_ref->db, &schema))
    return true;
  DBUG_ASSERT(schema != nullptr); // Should be impossible during upgrade.

  // Disable autocommit option in thd variable
  Disable_autocommit_guard autocommit_guard(thd);

  Disable_gtid_state_update_guard disabler(thd);

  bool result= dd::create_view(thd, *schema, view_ref);

  if (result)
  {
    trans_rollback_stmt(thd);
    // Full rollback in case we have THD::transaction_rollback_request.
    trans_rollback(thd);
  }
  else
    result= trans_commit_stmt(thd) || trans_commit(thd);

  // Restore
  thd->lex->select_lex= backup_select;
  thd->lex->sroutines_list.push_front(&saved_sroutines_list);
  thd->lex->query_tables= saved_query_tables;

  return result;
}


/**
  Construct ALTER VIEW statement to fix the column list
  and dependency information but retains the previous
  view defintion entry in DD.

  @param[in]  thd       Thread handle.
  @param[in]  view_ref  TABLE_LIST to store view data.
  @param[out] str       String object to store view definition.
  @param[in]  db_name   database name.
  @param[in]  view_name view name.
  @param[in]  cs        Charset Information.
*/
static void create_alter_view_stmt(THD *thd,
                                   TABLE_LIST *view_ref,
                                   String *str,
                                   const String_type &db_name,
                                   const String_type &view_name,
                                   const CHARSET_INFO *cs)
{
  str->append(STRING_WITH_LEN("ALTER "));
  view_store_options(thd, view_ref, str);
  str->append(STRING_WITH_LEN("VIEW "));
  append_identifier(thd, str, db_name.c_str(), db_name.length());
  str->append('.');
  append_identifier(thd, str, view_name.c_str(), view_name.length());
  str->append(STRING_WITH_LEN(" AS "));
  str->append(view_ref->select_stmt.str, view_ref->select_stmt.length, cs);
  if (view_ref->with_check != VIEW_CHECK_NONE)
  {
    if (view_ref->with_check == VIEW_CHECK_LOCAL)
      str->append(STRING_WITH_LEN(" WITH LOCAL CHECK OPTION"));
    else
      str->append(STRING_WITH_LEN(" WITH CASCADED CHECK OPTION"));
  }
}


/**
  Finalize upgrading view by fixing column data, table and routines
  dependency.
  View will be marked invalid if ALTER VIEW statement fails on the view.

  @param[in] thd                     Thread handle.
  @param[in] view_ref                TABLE_LIST with view data.
  @param[in] db_name                 database name.
  @param[in] view_name               view name.
  @param[in] mem_root                MEM_ROOT to handle memory allocations.

  @retval false  ON SUCCESS
  @retval true   ON FAILURE

*/
static bool fix_view_cols_and_deps(THD *thd, TABLE_LIST *view_ref,
                                   const String_type &db_name,
                                   const String_type &view_name,
                                   MEM_ROOT *mem_root)
{
  bool error= false;

  const CHARSET_INFO *client_cs= thd->variables.character_set_client;
  const CHARSET_INFO *cs= thd->variables.collation_connection;
  const CHARSET_INFO *m_client_cs, *m_connection_cl;

  /*
    Charset has beed fixed in migrate_view_to_dd().
    resolve functions should never fail here.
  */
  resolve_charset(view_ref->view_client_cs_name.str,
                  system_charset_info,
                  &m_client_cs);

  resolve_collation(view_ref->view_connection_cl_name.str,
                    system_charset_info,
                    &m_connection_cl);

  thd->variables.character_set_client= m_client_cs;
  thd->variables.collation_connection= m_connection_cl;
  thd->update_charset();

  MEM_ROOT *m_mem_root= thd->mem_root;
  thd->mem_root= mem_root;

  const sql_mode_t saved_mode= thd->variables.sql_mode;
  // Switch off modes which can prevent normal parsing of VIEW.
  thd->variables.sql_mode&= ~(MODE_PIPES_AS_CONCAT | MODE_ANSI_QUOTES |
                              MODE_IGNORE_SPACE | MODE_NO_BACKSLASH_ESCAPES);

  String full_view_definition((char *)0, 0, m_connection_cl);
  create_alter_view_stmt(thd, view_ref, &full_view_definition,
                         db_name, view_name, m_connection_cl);

  String db_query;
  db_query.append(STRING_WITH_LEN("USE "));
  append_identifier(thd, &db_query, db_name.c_str(), db_name.length());

  String_type change_db_query(db_query.ptr(), db_query.length());
  error= execute_query(thd, change_db_query);

  // Execute ALTER view statement to create the view dependency entry in DD.
  String_type query(full_view_definition.ptr(), full_view_definition.length());
  if (!error)
    error= execute_query(thd, query);

  // Disable autocommit option in thd variable
  Disable_autocommit_guard autocommit_guard(thd);

  /*
    If there is an error in ALTERing view, mark it as invalid and proceed
    with upgrade.
  */
  if (error)
  {
    sql_print_warning("Resolving dependency for the view '%s.%s' failed. "
                      "View is no more valid to use", db_name.c_str(),
                      view_name.c_str());
    update_view_status(thd, db_name.c_str(), view_name.c_str(), false, true);
    error= false;
  }

  // Restore variables
  thd->variables.character_set_client= client_cs;
  thd->variables.collation_connection= cs;
  thd->update_charset();
  thd->mem_root= m_mem_root;
  thd->variables.sql_mode= saved_mode;

  return error;
}


/**
  Create an entry in the DD for the view.

  @param[in]  thd                       Thread handle.
  @param[in]  frm_context               Structure to hold view definition
                                        read from frm file.
  @param[in]  db_name                   database name.
  @param[in]  view_name                 view name.
  @param[in]  mem_root                  MEM_ROOT to handle memory allocations.
  @param [in] is_fix_view_cols_and_deps Flag to create view
                                        with dependency information.
  @retval false  ON SUCCESS
  @retval true   ON FAILURE
*/

static bool migrate_view_to_dd(THD *thd,
                               const FRM_context &frm_context,
                               const String_type &db_name,
                               const String_type &view_name,
                               MEM_ROOT *mem_root,
                               bool is_fix_view_cols_and_deps)
{
  TABLE_LIST table_list;

  table_list.init_one_table(db_name.c_str(), db_name.length(),
                            view_name.c_str(), view_name.length(),
                            view_name.c_str(), TL_READ);

  // Initialize timestamp
  table_list.timestamp.str= table_list.timestamp_buffer;

  // Prepare default values for old format
  table_list.view_suid= TRUE;
  table_list.definer.user.str= table_list.definer.host.str= 0;
  table_list.definer.user.length= table_list.definer.host.length= 0;

  if (frm_context.view_def->parse(reinterpret_cast<uchar*>(&table_list),
                               mem_root,
                               view_parameters, REQUIRED_VIEW_PARAMETERS,
                               &file_parser_dummy_hook))
  {
    sql_print_error("Error in parsing view %s.%s",
                    db_name.c_str(), view_name.c_str());
    return true;
  }

  // Check old format view .frm file
  if (!table_list.definer.user.str)
  {
    sql_print_warning("%s.%s has no definer(maybe an old view format. "
                      "Current user is used as definer. Please recreate "
                      "the view.", db_name.c_str(),  view_name.c_str());
    get_default_definer(thd, &table_list.definer);
  }


  /*
    Check client character_set and connection collation.
    Throw a warning if there is no or unknown cs name.
    Print warning in error log only once.
  */
  bool invalid_ctx= false;

  // Check for blank creation context
  if (table_list.view_client_cs_name.str == nullptr ||
      table_list.view_connection_cl_name.str == nullptr)
  {
    // Print warning only once in the error log.
    if (!is_fix_view_cols_and_deps)
      sql_print_warning(ER_DEFAULT(ER_VIEW_NO_CREATION_CTX),
                        db_name.c_str(), view_name.c_str());
    invalid_ctx= true;
  }

  // Check for valid character set.
  const CHARSET_INFO *cs= nullptr;
  if (!invalid_ctx)
  {
    invalid_ctx = resolve_charset(table_list.view_client_cs_name.str,
                                  system_charset_info,
                                  &cs);

    invalid_ctx |= resolve_collation(table_list.view_connection_cl_name.str,
                                     system_charset_info,
                                     &cs);

    // Print warning only once in the error log.
    if (!is_fix_view_cols_and_deps && invalid_ctx)
      sql_print_warning("View '%s'.'%s': there is unknown charset/collation "
                        "names (client: '%s'; connection: '%s').",
                        db_name.c_str(),
                        view_name.c_str(),
                        table_list.view_client_cs_name.str,
                        table_list.view_connection_cl_name.str);
  }

  // Set system_charset_info for view.
  if (invalid_ctx)
  {
    cs= system_charset_info;
    size_t cs_length= strlen(cs->csname);
    size_t length= strlen(cs->name);

    table_list.view_client_cs_name.str= strmake_root(mem_root,
                                                      cs->csname,
                                                      cs_length);
    table_list.view_client_cs_name.length= cs_length;

    table_list.view_connection_cl_name.str= strmake_root(mem_root,
                                                          cs->name,
                                                          length);
    table_list.view_connection_cl_name.length= length;

    if (table_list.view_client_cs_name.str == nullptr ||
        table_list.view_connection_cl_name.str == nullptr)
    {
      sql_print_error("Error in allocating memory for character set name for "
                      "view %s.%s.", db_name.c_str(), view_name.c_str());
      return true;
    }
  }

  // View is already created, we are recreating it now.
  if (is_fix_view_cols_and_deps)
  {
    if (fix_view_cols_and_deps(thd, &table_list, db_name, view_name, mem_root))
    {
      sql_print_error("Error in Creating View %s.%s",
                      db_name.c_str(), view_name.c_str());
      return true;
    }
  }
  else
  {
    /*
      Create view without making entry in mysql.columns,
      mysql.view_table_usage and mysql.view_routine_usage.
    */
    if (create_unlinked_view(thd, &table_list))
    {
      sql_print_error("Error in parsing view %s.%s",
      db_name.c_str(), view_name.c_str());
      return true;
    }
  }
  return false;
}


/**
  Create an entry in mysql.tablespaces for tablespace entry
  found in .frm files for InnoDB and NDB.

  @param[in]  thd        Thread handle.
  @param[in]  name       Tablespace name.
  @param[in]  hton       handlerton object

  @retval false  ON SUCCESS
  @retval true   ON FAILURE
*/

static bool migrate_tablespace_to_dd(THD *thd, const char *name,
                                     handlerton *hton)
{
  st_alter_tablespace ts_info;
  const dd::Tablespace* ts_obj= nullptr;

  Disable_gtid_state_update_guard disabler(thd);
  Disable_autocommit_guard autocommit_guard(thd);

  // If engine does not support tablespaces, return
  if (!hton->alter_tablespace)
    return false;

  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  if (thd->dd_client()->acquire(name, &ts_obj))
    return true;

  // Tablespace object found in the DD, return.
  if (ts_obj != nullptr)
    return false;

  ts_info.tablespace_name= name;
  /*
    TODO : When upgrading, server does not know path of the tablespace file.
           It should be handled later when doing InnoDB dictionary upgrade.
  */
  ts_info.data_file_name= name;

  if (dd::create_tablespace(thd, &ts_info, hton))
  {
    trans_rollback_stmt(thd);
    // Full rollback in case we have THD::transaction_rollback_request.
    trans_rollback(thd);
    return true;
  }

  return trans_commit_stmt(thd) || trans_commit(thd);
}


/**
  Function to convert string to fk_option enum.
*/

static fk_option get_ref_opt(const char *str)
{
  if (strcmp(str, "RESTRICT") == 0)
    return FK_OPTION_RESTRICT;
  if (strcmp(str, "CASCADE") == 0)
    return FK_OPTION_CASCADE;
  if (strcmp(str, "SET NULL") == 0)
    return FK_OPTION_SET_NULL;
  if (strcmp(str, "NO ACTION") == 0)
    return FK_OPTION_NO_ACTION;

  return FK_OPTION_UNDEF;
}


/**
  Prepare Foreign key data to store in DD.
*/

static bool prepare_foreign_key_upgrade(FOREIGN_KEY_INFO *fk_key_info,
                                        FOREIGN_KEY *fk_key,
                                        MEM_ROOT *mem_root)
{
  fk_key->name= fk_key_info->foreign_id->str;
  LEX_CSTRING name{ fk_key_info->foreign_id->str,
                    fk_key_info->foreign_id->length };

  if (check_string_char_length(name,
                               "", NAME_CHAR_LEN,
                               system_charset_info, 1))
  {
    sql_print_error(ER_DEFAULT(ER_TOO_LONG_IDENT), fk_key->name);
    return true;
  }

  fk_key->ref_db.str= fk_key_info->referenced_db->str;
  fk_key->ref_db.length= fk_key_info->referenced_db->length;
  fk_key->ref_table.str= fk_key_info->referenced_table->str;
  fk_key->ref_table.length= fk_key_info->referenced_table->length;
  fk_key->delete_opt= get_ref_opt(fk_key_info->delete_method->str);
  fk_key->update_opt= get_ref_opt(fk_key_info->update_method->str);
  fk_key->match_opt= FK_MATCH_UNDEF;
  fk_key->key_parts= fk_key_info->foreign_fields.elements;

  fk_key->key_part=
    reinterpret_cast<LEX_CSTRING*>(alloc_root(mem_root,(sizeof(LEX_CSTRING) *
                                   fk_key_info->foreign_fields.elements)));
  if (!fk_key->key_part)
  {
    sql_print_error("Error in Memory allocation for Foreign key list.");
    return true;
  }

  fk_key->fk_key_part=
    reinterpret_cast<LEX_CSTRING*>(alloc_root(mem_root,
                                    (sizeof(LEX_CSTRING) *
                                     fk_key_info->foreign_fields.elements)));
  if (!fk_key->fk_key_part)
  {
    sql_print_error("Error in Memory allocation for Foreign key list.");
    return true;
  }


  LEX_STRING *f_info;
  LEX_STRING *r_info;
  List_iterator_fast<LEX_STRING> foreign_fields(fk_key_info->foreign_fields);
  List_iterator_fast<LEX_STRING> ref_fields(fk_key_info->referenced_fields);

  for (size_t column_nr= 0;
       column_nr < fk_key->key_parts;
       column_nr++)
  {
    f_info= foreign_fields++;
    r_info= ref_fields++;
    fk_key->key_part[column_nr]= { f_info->str, f_info->length };
    fk_key->fk_key_part[column_nr]= { r_info->str, r_info->length };
  }

  return false;
}


/**
   Create partition information for upgrade.
   This function uses the same method to create partition information
   as done by open_table_from_share().
*/

static bool fill_partition_info_for_upgrade(THD *thd,
                                            TABLE_SHARE *share,
                                            const FRM_context *frm_context,
                                            TABLE *table)
{
  thd->work_part_info= nullptr;

  // If partition information is present in TABLE_SHARE
  if (share->partition_info_str_len && table->file)
  {
    // Parse partition expression and create Items.
    if (unpack_partition_info(thd, table, share,
        frm_context->default_part_db_type, false))
      return true;

    // dd::create_dd_user_table() uses thd->part_info to get partition values.
    thd->work_part_info= table->part_info;
    // This assignment is necessary to free the partition_info
    share->m_part_info= table->part_info;
    /*
      For normal TABLE instances, free_items() is called by closefrm().
      For this scenario, free_items() will be called by destructor of
      Table_upgrade_guard.
    */
    share->m_part_info->item_free_list= table->part_info->item_free_list;
  }
  return false;
}


/**
  Add triggers to table
*/

static bool add_triggers_to_table(THD *thd,
                                  TABLE *table,
                                  const String_type &schema_name,
                                  const String_type &table_name)
{
  List<::Trigger> m_triggers;
  if (Trigger_loader::trg_file_exists(schema_name.c_str(),
                                      table_name.c_str()))
  {
    if (Trigger_loader::load_triggers(thd,
                                      &table->mem_root,
                                      schema_name.c_str(),
                                      table_name.c_str(),
                                      &m_triggers))
    {
      sql_print_warning("Error in reading %s.TRG file.", table_name.c_str());
      return true;
    }
    Table_trigger_dispatcher *d= Table_trigger_dispatcher::create(table);

    d->parse_triggers(thd, &m_triggers, true);
    if (d->check_for_broken_triggers())
    {
      sql_print_warning("Error in parsing Triggers from %s.TRG file.",
                        table_name.c_str());
      return true;
    }


    List_iterator<::Trigger> it(m_triggers);
    /*
      Fix the order column for the execution of Triggers with
      same action event and same action timing. .TRG filed used to handle
      this by storing the triggers in the order of their execution.
    */

    // Get 1st Trigger
    ::Trigger *t= it++;

    // If no Trigger found, return
    if (!t)
      return false;

    ulonglong order= 1;
    enum_trigger_event_type t_type= t->get_event();
    enum_trigger_action_time_type t_time= t->get_action_time();

    // Set order for 1st Trigger as 1.
    t->set_action_order(order);
    order= order + 1;

    // Set action order for rest of the Triggers.
    while (true)
    {
      ::Trigger *t= it++;

      if (!t)
        break;

      /*
        events of the same type and timing always go in one group according
        to their action order.
      */
      assert(t->get_event() >= t_type && (t->get_event() > t_type ||
                                          t->get_action_time() >= t_time));

      // We found next trigger with same action event and same action time.
      if (t->get_event() == t_type &&
          t->get_action_time() == t_time)
      {
        // Set action order for Trigger
        t->set_action_order(order);
        // Increment the value of action order
        order= order + 1;
        continue;
      }
      // If action event OR action time OR both changes for the next trigger.
      else
      {
        // Reset action order value to 1.
        order= 1;
        // Set "1" as the action order.
        t->set_action_order(order);
        // Increment the value of action order
        order= order + 1;
        // Reset values of t_type and t_time
        t_type= t->get_event();
        t_time= t->get_action_time();
        continue;
      }
    } // End of while loop

    // Set Iterator to the beginning
    it.rewind();

    // Create entry in DD table for each trigger.
    while (true)
    {
      ::Trigger *t= it++;

      if (!t)
        break;

      Disable_gtid_state_update_guard disabler(thd);

      // Ordering of Triggers is taken care above, pass dummy arguments here.
      LEX_CSTRING anchor_trigger_name{0, 0};
      if (dd::create_trigger(thd, t,
                             enum_trigger_order_type::TRG_ORDER_NONE,
                             anchor_trigger_name))
      {
        trans_rollback_stmt(thd);
        // Full rollback in case we have THD::transaction_rollback_request.
        trans_rollback(thd);
        return true;
      }
      // dd::create_trigger() does not commit transaction
      if (trans_commit_stmt(thd) || trans_commit(thd))
      {
        sql_print_error("Error in creating DD entry for Trigger %s.%s",
                        t->get_db_name().str, t->get_trigger_name().str);
        return true;
      }

      //Cleanup for Trigger
      sp_head *sp= t->get_sp();
      sp_head *saved_sphead= thd->lex->sphead;
      thd->lex->sphead= sp;
      sp->m_parser_data.finish_parsing_sp_body(thd);
      thd->lex->sphead= saved_sphead;
      sp_head::destroy(sp);

    } //End of while loop
  } //End of If condition to check Trigger existance
  return false;
}


/**
  Open table in SE to get FK information.

  @param[in]  thd          Thread handle.
  @param[in]  schema_name  Database name
  @param[in]  table_name   Table name.
  @param[in]  share        TABLE_SHARE object
  @param[out] table        TABLE object
  @param[in]  table_guard  Table_upgrade_guard object
  @param[in]  mem_root     MEM_ROOT object

  @retval false  ON SUCCESS
  @retval true   ON FAILURE
*/

static bool open_table_for_fk_info(THD *thd,
                                   const String_type &schema_name,
                                   const String_type &table_name,
                                   TABLE_SHARE *share,
                                   TABLE *table,
                                   Table_upgrade_guard *table_guard,
                                   MEM_ROOT *mem_root)
{
  uint i= 0;
  KEY *key_info= nullptr;
  /*
    Open only tables which support Foreign key to retrieve FK information.
    This is temporary workaround until we move to reading data directly from
    InnoDB sys tables.
  */
  if (ha_check_storage_engine_flag(share->db_type(),
                                   HTON_SUPPORTS_FOREIGN_KEYS))
  {
    // Fix the index information for opening table
    key_info= share->key_info;
    for (i=0 ; i < share->keys ; i++,key_info++)
    {
      if (!my_strcasecmp(system_charset_info, key_info->name, primary_key_name))
        key_info->name= primary_key_name;
      // The algorithm was HA_KEY_ALG_UNDEF in 5.7.
      if (key_info->algorithm == HA_KEY_ALG_SE_SPECIFIC)
      {
        // FULLTEXT indexes are marked as HA_KEY_ALG_FULLTEXT.
        if (key_info->flags & HA_SPATIAL)
          key_info->algorithm= HA_KEY_ALG_RTREE;
        else
          key_info->algorithm= table->file->get_default_index_algorithm();
      }
      else if (!(key_info->flags & HA_FULLTEXT))
      {
        /*
          If explicit algorithm is not supported by SE, replace it with
          default one. Don't mark key algorithm as explicitly specified
          in this case.
        */
        if (table->file->is_index_algorithm_supported(key_info->algorithm))
        {
          // Mark key algorithm as explicitly specified
          key_info->is_algorithm_explicit= true;
        }
        else
        {
          key_info->algorithm= table->file->get_default_index_algorithm();
        }
      }
    }

    /*
      open_table_from_share() will memset 0 for the table object.
      Copy mem_root object as TABLE object is allocated with its own mem_root.
    */
    memcpy((char *)mem_root, (char*) &table->mem_root, sizeof(*mem_root));
    table_guard->update_mem_root(mem_root);

    if (open_table_from_share(thd, share, share->table_name.str,
                              (uint) (HA_OPEN_KEYFILE |
                                      HA_OPEN_RNDFILE |
                                      HA_GET_INDEX |
                                      HA_TRY_READ_ONLY),
                                      EXTRA_RECORD | OPEN_NO_DD_TABLE,
                              thd->open_options, table, FALSE,
                              NULL))
    {
      sql_print_error("Error in opening table %s.%s",
                      schema_name.c_str(), table_name.c_str());
      return true;
    }
    table_guard->set_is_table_open(true);
  }
  return false;
}


/**
  Fix generated columns.

  @param[in]  thd            Thread handle.
  @param[in]  table          TABLE object.
  @param[in]  create_fields  List of Create_fields

  @retval false  ON SUCCESS
  @retval true   ON FAILURE

*/

static bool fix_generated_columns_for_upgrade(THD *thd,
                                              TABLE *table,
                                              List<Create_field> &create_fields)
{
  Create_field *sql_field;
  bool error_reported= FALSE;
  bool error= false;

  if (table->s->vfields)
  {
    List_iterator<Create_field> itc(create_fields);
    Field **field_ptr;

    for (field_ptr= table->s->field;
         (sql_field=itc++);
         field_ptr++)
    {
      // Field has generated col information.
      if (sql_field->gcol_info && (*field_ptr)->gcol_info)
      {
        if (unpack_gcol_info(thd, table, *field_ptr,
                             FALSE, &error_reported))
        {
          error= true;
          break;
        }
        sql_field->gcol_info->expr_item= (*field_ptr)->gcol_info->expr_item;
      }
    }
  }

  return error;
}


/**
  Read .frm files and enter metadata for tables/views.
*/

static bool migrate_table_to_dd(THD *thd,
                                const String_type &schema_name,
                                const String_type &table_name,
                                bool is_fix_view_cols_and_deps)
{
  int error= 0;
  FRM_context frm_context;
  TABLE_SHARE share;
  TABLE *table= nullptr;
  Field **ptr,*field;
  handler *file= nullptr;
  MEM_ROOT mem_root;

  char path[FN_REFLEN + 1];
  bool was_truncated= false;
  build_table_filename(path, sizeof(path) - 1 - reg_ext_length,
                       schema_name.c_str(),
                       table_name.c_str(), "", 0,
                       &was_truncated);

  if (was_truncated)
  {
    sql_print_error(ER_DEFAULT(ER_IDENT_CAUSES_TOO_LONG_PATH),
                    sizeof(path) - 1, path);
    return true;
  }

  // Create table share for tables and views.
  if (create_table_share_for_upgrade(thd,
                                     path,
                                     &share,
                                     &frm_context,
                                     schema_name.c_str(),
                                     table_name.c_str(),
                                     is_fix_view_cols_and_deps))
  {
    sql_print_error("Error in creating TABLE_SHARE from %s.frm file.",
                     table_name.c_str());
    return true;
  }

  /*
     Acquire mdl lock before upgrading.
     Don't acquire mdl lock if fixing dummy views.
  */
  Upgrade_MDL_guard mdl_guard(thd);
  if (mdl_guard.acquire_lock(schema_name, table_name))
  {
    free_table_share(&share);
    sql_print_error("Unable to acquire lock on %s.%s",
                    schema_name.c_str(), table_name.c_str());
    return true;
  }

  // Initialize TABLE mem_root
  init_sql_alloc(key_memory_TABLE,
                 &mem_root, TABLE_ALLOC_BLOCK_SIZE, 0);

  // Make a new TABLE object
  if (!(table= static_cast<TABLE *>(alloc_root(&mem_root, sizeof(*table)))))
  {
    free_table_share(&share);
    sql_print_error("Error in allocation memory for TABLE object.");
    return true;
  }

  // Fix pointers in TABLE, TABLE_SHARE
  memset(table, 0, sizeof(*table));
  table->s= &share;
  table->in_use= thd;
  memcpy((char*) &table->mem_root, (char*) &mem_root, sizeof(mem_root));

  // Object to handle cleanup.
  Table_upgrade_guard table_guard(thd, table, &table->mem_root);

  // Dont upgrade tables, we are fixing dependency for views.
  if (!share.is_view && is_fix_view_cols_and_deps)
    return false;

  if (share.is_view)
    return (migrate_view_to_dd(thd, frm_context, schema_name,
                               table_name, &table->mem_root,
                               is_fix_view_cols_and_deps));

  // Get the handler
  if (!(file= get_new_handler(&share,
                              share.partition_info_str_len != 0,
                              &table->mem_root,
                              share.db_type())))
  {
    sql_print_error("Error in creating handler object for table %s.%s",
                    schema_name.c_str(), table_name.c_str());
    return true;
  }
  table->file= file;
  table_guard.update_handler(file);

  if (table->file->set_ha_share_ref(&share.ha_share))
  {
    sql_print_error("Error in setting handler reference for table %s.%s",
                    table_name.c_str(), schema_name.c_str());
    return true;
  }

  /*
    Fix pointers in TABLE, TABLE_SHARE and fields.
    These steps are necessary for correct handling of
    default values by Create_field constructor.
  */
  table->s->db_low_byte_first= table->file->low_byte_first();
  table->use_all_columns();
  table->record[0]= table->record[1]= share.default_values;
  table->null_row= 0;
  table->field= share.field;
  table->key_info= share.key_info;

  //Set table_name variable and table in fields
  const char *alias= "";
  for (ptr= share.field ; (field= *ptr); ptr++)
  {
    field->table= table;
    field->table_name= &alias;
  }

  // Check presence of old data types
  bool avoid_temporal_upgrade_saved= avoid_temporal_upgrade;
  avoid_temporal_upgrade= false;
  error= check_table_for_old_types(table);
  avoid_temporal_upgrade= avoid_temporal_upgrade_saved;

  if (error)
  {
    sql_print_error(ER_DEFAULT(ER_TABLE_NEEDS_UPGRADE), table_name.c_str());
    return true;
  }

  uint i= 0;
  KEY *key_info= share.key_info;

  /*
    Mark all the keys visible and supported algorithm explicit.
    Unsupported algorithms will get fixed by prepare_key() call.
  */
  key_info= share.key_info;
  for (i=0 ; i < share.keys ; i++,key_info++)
  {
    key_info->is_visible= true;
    /*
      Fulltext and Spatical indexes will get fixed by
      mysql_prepare_create_table()
    */
    if (key_info->algorithm != HA_KEY_ALG_SE_SPECIFIC &&
        !(key_info->flags & HA_FULLTEXT) &&
        !(key_info->flags & HA_SPATIAL) &&
        table->file->is_index_algorithm_supported(key_info->algorithm))
          key_info->is_algorithm_explicit= true;
  }

  // Fill create_info to be passed to the DD framework.
  HA_CREATE_INFO create_info;
  Alter_info alter_info;
  alter_info.reset();
  Alter_table_ctx alter_ctx;

  /*
    Replace thd->mem_root as prepare_fields_and_keys() and
    mysql_prepare_create_table() allocates memory in thd->mem_root.
  */
  MEM_ROOT *mem_root_backup= thd->mem_root;
  thd->mem_root= &table->mem_root;

  fill_create_info_for_upgrade(&create_info, table);

  if (prepare_fields_and_keys(thd, table, &create_info,
                              &alter_info, &alter_ctx,
                              create_info.used_fields, true))
  {
    thd->mem_root= mem_root_backup;
    return true;
  }

  // Fix keys and indexes.
  KEY *key_info_buffer;
  uint key_count;
  FOREIGN_KEY *dummy_fk_key_info= NULL;
  uint fk_key_count= 0;

  if (mysql_prepare_create_table(thd, schema_name.c_str(), table_name.c_str(),
                                 &create_info, &alter_info,
                                 file, &key_info_buffer, &key_count,
                                 &dummy_fk_key_info, &fk_key_count,
                                 alter_ctx.fk_info, alter_ctx.fk_count,
                                 0))
  {
    thd->mem_root= mem_root_backup;
    return true;
  }

  // Restore thd mem_root
  thd->mem_root= mem_root_backup;

  int select_field_pos= alter_info.create_list.elements;
  create_info.null_bits= 0;
  Create_field *sql_field;
  List_iterator<Create_field> it_create(alter_info.create_list);

  for (int field_no= 0; (sql_field=it_create++) ; field_no++)
  {
    if (prepare_create_field(thd, &create_info, &alter_info.create_list,
                             &select_field_pos, table->file, sql_field, field_no))
      return true;
  }

  // open_table_from_share and partition expression parsing needs a
  // valid SELECT_LEX to parse generated columns
  LEX *lex_saved= thd->lex;
  LEX lex;
  thd->lex= &lex;
  lex_start(thd);
  table_guard.update_lex(lex_saved);

  if (fill_partition_info_for_upgrade(thd, &share, &frm_context, table))
    return true;

  // Add name of all tablespaces used by partitions to the hash set.
  Tablespace_hash_set tablespace_name_set(PSI_INSTRUMENT_ME);
  if (thd->work_part_info != nullptr)
  {
    List_iterator<partition_element>
      partition_iter(thd->work_part_info->partitions);
    partition_element *partition_elem;

    while ((partition_elem= partition_iter++) )
    {
      if (partition_elem->tablespace_name != nullptr)
      {
        tablespace_name_set.insert(
          const_cast<char*>(partition_elem->tablespace_name));
      }
    }
  }

  // Add name of the tablespace used by table to the hash set.
  if (share.tablespace != nullptr)
    tablespace_name_set.insert(
          const_cast<char*>(share.tablespace));

  /*
    Acquire lock on tablespace names

    No lock is needed when creating DD objects from system thread
    handling server bootstrap/initialization.
    And in cases when lock is required it is X MDL and not IX lock
    the code acquires.

    However since IX locks on tablespaces used for table creation we
    still have to acquire locks. IX locks are acquired on tablespaces
    to satisfy asserts in dd::create_table()).
  */
  if ((tablespace_name_set.size() != 0) &&
    mdl_guard.acquire_lock_tablespace(&tablespace_name_set))
  {
     sql_print_error("Unable to acquire lock on tablespace name %s",
                     share.tablespace);
     return true;
  }

  Tablespace_hash_set::Iterator it_tablespace(tablespace_name_set);
  char *tablespace= NULL;
  while ((tablespace= it_tablespace++))
  {
    if (migrate_tablespace_to_dd(thd, tablespace, share.db_type()))
    {
      sql_print_error("Error in creating entry for %s tablespace in DD tables.",
                      tablespace);
      return true;
    }
  }

  // Open table to get Foreign key information
  if (open_table_for_fk_info(thd, schema_name, table_name,
                             &share, table, &table_guard,
                             &mem_root))
    return true;

  /*
    Generated columns are fixed here as open_table_from_share()
    asserts that Field objects in TABLE_SHARE doesn't have
    expressions assigned.
  */
  if (fix_generated_columns_for_upgrade(thd, table, alter_info.create_list))
  {
    sql_print_error("Error in processing generated columns");
    return true;
  }

  List<FOREIGN_KEY_INFO> f_key_list;
  table->file->get_foreign_key_list(thd, &f_key_list);

  FOREIGN_KEY_INFO *f_key_info;
  List_iterator_fast<FOREIGN_KEY_INFO> it(f_key_list);


  FOREIGN_KEY *fk_key_info_buffer= NULL;
  uint fk_number= 0;

  // Allocate memory for foreign key information
  FOREIGN_KEY *fk_key_info;
  (fk_key_info_buffer)= fk_key_info=
    (FOREIGN_KEY*) alloc_root(&table->mem_root,
                              (sizeof(FOREIGN_KEY) * (f_key_list.elements)));

  if (!fk_key_info_buffer)
  {
    sql_print_error("Error in Memory allocation for Foreign key Information.");
    return true;
  }

  // Create Foreign key List
  while ((f_key_info= it++))
  {
    if (prepare_foreign_key_upgrade(f_key_info, fk_key_info, &table->mem_root))
      return true;
    fk_key_info++;
    fk_number++;
  }

  // Set sql_mode=0 for handling default values, it will be restored vai RAII.
  thd->variables.sql_mode= 0;
  // Disable autocommit option in thd variable
  Disable_autocommit_guard autocommit_guard(thd);

  if (dd::create_dd_user_table(thd,
                               schema_name,
                               table_name,
                               &create_info,
                               alter_info.create_list,
                               key_info_buffer,
                               key_count,
                               Alter_info::ENABLE,
                               fk_key_info_buffer,
                               fk_number,
                               table->file,
                               true))
  {
    sql_print_error("Error in Creating DD entry for %s.%s",
                    schema_name.c_str(), table_name.c_str());
    return true;
  }

  // Set row type for InnoDB tables. This needs to be done after ha_open().
  enum row_type se_row_type= table->file->get_row_type_for_upgrade();

  if (se_row_type != ROW_TYPE_NOT_USED)
  {
    if (dd::fix_row_type(thd, &share, se_row_type))
    {
      sql_print_error("Error in fixing row type in DD for %s.%s",
                      schema_name.c_str(), table_name.c_str());
      return true;
    }
  }

  MEM_ROOT *thd_mem_root= thd->mem_root;
  thd->mem_root= &table->mem_root;
  error= add_triggers_to_table(thd, table, schema_name, table_name);
  thd->mem_root= thd_mem_root;

  return error;
}


/**
  Upgrade mysql.plugin table. This is required to initialize the plugins.
  User tables will be upgraded after all the plugins are initialized.
*/

bool migrate_plugin_table_to_dd(THD *thd)
{
  return migrate_table_to_dd(thd, "mysql", "plugin", false);
}


/**
  Returns the collation id for the database specified.

  @param[in]  thd                        Thread handle.
  @param[in]  db_opt_path                Path for database.
  @param[out] schema_charset             Character set of database.

  @retval false  ON SUCCESS
  @retval true   ON FAILURE

*/
static bool load_db_schema_collation(THD *thd,
                                     const LEX_STRING *db_opt_path,
                                     const CHARSET_INFO **schema_charset)
{
  IO_CACHE cache;
  File file;
  char buf[256];
  uint nbytes;

  if ((file= mysql_file_open(key_file_dbopt, db_opt_path->str,
                             O_RDONLY, MYF(0))) < 0)
  {
    sql_print_warning("Unable to open db.opt file %s. "
                      "Using default Character set.", db_opt_path->str);
    return false;
  }

  if (init_io_cache(&cache, file, IO_SIZE, READ_CACHE, 0, 0, MYF(0)))
  {
    sql_print_error("Unable to intialize IO cache to open db.opt file %s. ",
                     db_opt_path->str);
    goto err;
  }

  while ((int) (nbytes= my_b_gets(&cache, (char*) buf, sizeof(buf))) > 0)
  {
    char *pos= buf + nbytes - 1;

    /* Remove end space and control characters */
    while (pos > buf && !my_isgraph(&my_charset_latin1, pos[-1]))
      pos--;

    *pos=0;
    if ((pos= strrchr(buf, '=')))
    {
      if (!strncmp(buf,"default-character-set", (pos-buf)))
      {
        /*
           Try character set name, and if it fails try collation name, probably
           it's an old 4.1.0 db.opt file, which didn't have separate
           default-character-set and default-collation commands.
        */
        if (!(*schema_charset= get_charset_by_csname(pos + 1,
                                                    MY_CS_PRIMARY, MYF(0))) &&
            !(*schema_charset= get_charset_by_name(pos + 1, MYF(0))))
        {
          sql_print_warning("Unable to identify the charset in %s. "
                            "Using default character set.", db_opt_path->str);

          *schema_charset= thd->variables.collation_server;
        }
      }
      else if (!strncmp(buf, "default-collation", (pos - buf)))
      {
        if (!(*schema_charset= get_charset_by_name(pos + 1, MYF(0))) )
        {
          sql_print_warning("Unable to identify the charset in %s. "
                            "Using default character set.", db_opt_path->str);
          *schema_charset= thd->variables.collation_server;
        }
      }
    }
  }

  end_io_cache(&cache);
  mysql_file_close(file, MYF(0));
  return false;

err:
  mysql_file_close(file, MYF(0));
  return true;
}


/**
   Update the Schemata:DD for every database present
   in the data directory.
*/

bool migrate_schema_to_dd(THD *thd, const char *dbname)
{
  char dbopt_path_buff[FN_REFLEN + 1];
  char schema_name[NAME_LEN + 1];
  LEX_STRING dbopt_file_name;
  const CHARSET_INFO *schema_charset= thd->variables.collation_server;

  // Construct the schema name from its canonical format.
  filename_to_tablename(dbname, schema_name, sizeof(schema_name));

  dbopt_file_name.str= dbopt_path_buff;
  dbopt_file_name.length= build_table_filename(dbopt_path_buff, FN_REFLEN - 1,
                                               schema_name, "db", ".opt", 0);

  if (!my_access(dbopt_file_name.str, F_OK))
  {
    // Get the collation id for the database.
    if (load_db_schema_collation(thd, &dbopt_file_name, &schema_charset))
      return true;
  }
  else
  {
    sql_print_warning("db.opt file not found for %s database. "
                      "Using default Character set.", dbname);
  }

  // Disable autocommit option
  Disable_autocommit_guard autocommit_guard(thd);

  if (dd::create_schema(thd, schema_name, schema_charset))
  {
    trans_rollback_stmt(thd);
    // Full rollback in case we have THD::transaction_rollback_request.
    trans_rollback(thd);
    return true;
  }

  if (trans_commit_stmt(thd) || trans_commit(thd))
    return true;

  return false;
}


/**
  Column definitions for 5.7 mysql.event table (5.7.13 and up).
*/
const TABLE_FIELD_TYPE event_table_fields[ET_FIELD_COUNT] =
{
  {
    { C_STRING_WITH_LEN("db") },
    { C_STRING_WITH_LEN("char(64)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("name") },
    { C_STRING_WITH_LEN("char(64)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("body") },
    { C_STRING_WITH_LEN("longblob") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("definer") },
    { C_STRING_WITH_LEN("char(93)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("execute_at") },
    { C_STRING_WITH_LEN("datetime") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("interval_value") },
    { C_STRING_WITH_LEN("int(11)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("interval_field") },
    { C_STRING_WITH_LEN("enum('YEAR','QUARTER','MONTH','DAY',"
    "'HOUR','MINUTE','WEEK','SECOND','MICROSECOND','YEAR_MONTH','DAY_HOUR',"
    "'DAY_MINUTE','DAY_SECOND','HOUR_MINUTE','HOUR_SECOND','MINUTE_SECOND',"
    "'DAY_MICROSECOND','HOUR_MICROSECOND','MINUTE_MICROSECOND',"
    "'SECOND_MICROSECOND')") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("created") },
    { C_STRING_WITH_LEN("timestamp") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("modified") },
    { C_STRING_WITH_LEN("timestamp") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("last_executed") },
    { C_STRING_WITH_LEN("datetime") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("starts") },
    { C_STRING_WITH_LEN("datetime") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("ends") },
    { C_STRING_WITH_LEN("datetime") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("status") },
    { C_STRING_WITH_LEN("enum('ENABLED','DISABLED','SLAVESIDE_DISABLED')") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("on_completion") },
    { C_STRING_WITH_LEN("enum('DROP','PRESERVE')") },
    {NULL, 0}
   },
  {
    { C_STRING_WITH_LEN("sql_mode") },
    { C_STRING_WITH_LEN("set('REAL_AS_FLOAT','PIPES_AS_CONCAT','ANSI_QUOTES',"
    "'IGNORE_SPACE','NOT_USED','ONLY_FULL_GROUP_BY','NO_UNSIGNED_SUBTRACTION',"
    "'NO_DIR_IN_CREATE','POSTGRESQL','ORACLE','MSSQL','DB2','MAXDB',"
    "'NO_KEY_OPTIONS','NO_TABLE_OPTIONS','NO_FIELD_OPTIONS','MYSQL323','MYSQL40',"
    "'ANSI','NO_AUTO_VALUE_ON_ZERO','NO_BACKSLASH_ESCAPES','STRICT_TRANS_TABLES',"
    "'STRICT_ALL_TABLES','NO_ZERO_IN_DATE','NO_ZERO_DATE','INVALID_DATES',"
    "'ERROR_FOR_DIVISION_BY_ZERO','TRADITIONAL','NO_AUTO_CREATE_USER',"
    "'HIGH_NOT_PRECEDENCE','NO_ENGINE_SUBSTITUTION','PAD_CHAR_TO_FULL_LENGTH')") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("comment") },
    { C_STRING_WITH_LEN("char(64)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("originator") },
    { C_STRING_WITH_LEN("int(10)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("time_zone") },
    { C_STRING_WITH_LEN("char(64)") },
    { C_STRING_WITH_LEN("latin1") }
  },
  {
    { C_STRING_WITH_LEN("character_set_client") },
    { C_STRING_WITH_LEN("char(32)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("collation_connection") },
    { C_STRING_WITH_LEN("char(32)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("db_collation") },
    { C_STRING_WITH_LEN("char(32)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("body_utf8") },
    { C_STRING_WITH_LEN("longblob") },
    { NULL, 0 }
  }
};

static const TABLE_FIELD_DEF
  event_table_def= {ET_FIELD_COUNT, event_table_fields};


/**
  Column definitions for 5.7 mysql.event table (before 5.7.13).
*/

static
const TABLE_FIELD_TYPE event_table_fields_old[ET_FIELD_COUNT] =
{
  {
    { C_STRING_WITH_LEN("db") },
    { C_STRING_WITH_LEN("char(64)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("name") },
    { C_STRING_WITH_LEN("char(64)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("body") },
    { C_STRING_WITH_LEN("longblob") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("definer") },
    { C_STRING_WITH_LEN("char(77)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("execute_at") },
    { C_STRING_WITH_LEN("datetime") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("interval_value") },
    { C_STRING_WITH_LEN("int(11)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("interval_field") },
    { C_STRING_WITH_LEN("enum('YEAR','QUARTER','MONTH','DAY',"
    "'HOUR','MINUTE','WEEK','SECOND','MICROSECOND','YEAR_MONTH','DAY_HOUR',"
    "'DAY_MINUTE','DAY_SECOND','HOUR_MINUTE','HOUR_SECOND','MINUTE_SECOND',"
    "'DAY_MICROSECOND','HOUR_MICROSECOND','MINUTE_MICROSECOND',"
    "'SECOND_MICROSECOND')") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("created") },
    { C_STRING_WITH_LEN("timestamp") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("modified") },
    { C_STRING_WITH_LEN("timestamp") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("last_executed") },
    { C_STRING_WITH_LEN("datetime") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("starts") },
    { C_STRING_WITH_LEN("datetime") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("ends") },
    { C_STRING_WITH_LEN("datetime") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("status") },
    { C_STRING_WITH_LEN("enum('ENABLED','DISABLED','SLAVESIDE_DISABLED')") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("on_completion") },
    { C_STRING_WITH_LEN("enum('DROP','PRESERVE')") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("sql_mode") },
    { C_STRING_WITH_LEN("set('REAL_AS_FLOAT','PIPES_AS_CONCAT','ANSI_QUOTES',"
    "'IGNORE_SPACE','NOT_USED','ONLY_FULL_GROUP_BY','NO_UNSIGNED_SUBTRACTION',"
    "'NO_DIR_IN_CREATE','POSTGRESQL','ORACLE','MSSQL','DB2','MAXDB',"
    "'NO_KEY_OPTIONS','NO_TABLE_OPTIONS','NO_FIELD_OPTIONS','MYSQL323','MYSQL40',"
    "'ANSI','NO_AUTO_VALUE_ON_ZERO','NO_BACKSLASH_ESCAPES','STRICT_TRANS_TABLES',"
    "'STRICT_ALL_TABLES','NO_ZERO_IN_DATE','NO_ZERO_DATE','INVALID_DATES',"
    "'ERROR_FOR_DIVISION_BY_ZERO','TRADITIONAL','NO_AUTO_CREATE_USER',"
    "'HIGH_NOT_PRECEDENCE','NO_ENGINE_SUBSTITUTION','PAD_CHAR_TO_FULL_LENGTH')") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("comment") },
    { C_STRING_WITH_LEN("char(64)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("originator") },
    { C_STRING_WITH_LEN("int(10)") },
    {NULL, 0}
  },
  {
    { C_STRING_WITH_LEN("time_zone") },
    { C_STRING_WITH_LEN("char(64)") },
    { C_STRING_WITH_LEN("latin1") }
  },
  {
    { C_STRING_WITH_LEN("character_set_client") },
    { C_STRING_WITH_LEN("char(32)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("collation_connection") },
    { C_STRING_WITH_LEN("char(32)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("db_collation") },
    { C_STRING_WITH_LEN("char(32)") },
    { C_STRING_WITH_LEN("utf8") }
  },
  {
    { C_STRING_WITH_LEN("body_utf8") },
    { C_STRING_WITH_LEN("longblob") },
    { NULL, 0 }
  }
};


static const TABLE_FIELD_DEF
  event_table_def_old= {ET_FIELD_COUNT, event_table_fields_old};


/**
   Load the charset and time zone information for an event.
*/
static void load_event_creation_context(THD *thd, TABLE *table,
                                        Event_parse_data *et_parse_data)
{
  LEX_STRING tz_name;
  const CHARSET_INFO *client_cs;
  const CHARSET_INFO *connection_cl;
  thd->variables.time_zone= my_tz_SYSTEM;

  if ((tz_name.str=
       get_field(thd->mem_root, table->field[ET_FIELD_TIME_ZONE])) == NULL)
  {
    sql_print_warning("Event '%s'.'%s': invalid value "
                      "in column mysql.event.time_zone.",
                      et_parse_data->dbname.str, et_parse_data->name.str);
  }
  else
  {
    tz_name.length= strlen(tz_name.str);
    String tz_str(tz_name.str, &my_charset_latin1);
    if ((thd->variables.time_zone= my_tz_find(thd, &tz_str)) == NULL)
    {
      thd->variables.time_zone= my_tz_SYSTEM;
      sql_print_warning("Event '%s'.'%s': has invalid time zone value ",
                        et_parse_data->dbname.str, et_parse_data->name.str);
    }
  }

  if (load_charset(thd->mem_root,
                   table->field[ET_FIELD_CHARACTER_SET_CLIENT],
                   thd->variables.character_set_client,
                   &client_cs))
  {
    sql_print_warning("Event '%s'.'%s': invalid value "
                      "in column mysql.event.character_set_client.",
                      et_parse_data->dbname.str, et_parse_data->name.str);
  }

  if (load_collation(thd->mem_root,
                     table->field[ET_FIELD_COLLATION_CONNECTION],
                     thd->variables.collation_connection,
                     &connection_cl))
  {
    sql_print_warning("Event '%s'.'%s': invalid value "
                      "in column mysql.event.collation_connection.",
                      et_parse_data->dbname.str, et_parse_data->name.str);
  }

  thd->variables.character_set_client= client_cs;
  thd->variables.collation_connection= connection_cl;
}


/**
   Update the created, last modified and last executed
   time for the event with the values read from the old
   data dir.
*/

static bool update_event_timing_fields(THD *thd, TABLE *table,
                                       char *event_db_name,
                                       char *event_name)
{
  dd::Event *new_event= nullptr;
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  if (thd->dd_client()->acquire_for_modification(event_db_name,
                                                 event_name,
                                                 &new_event))
    return true;
  if (new_event == nullptr)
    return true;

  if (!table->field[ET_FIELD_LAST_EXECUTED]->is_null())
  {
    MYSQL_TIME time;
    my_time_t last_executed;
    bool not_used= FALSE;
    table->field[ET_FIELD_LAST_EXECUTED]->get_date(&time,
                                                   TIME_NO_ZERO_DATE);
    last_executed= my_tz_OFFSET0->TIME_to_gmt_sec(&time, &not_used);
    new_event->set_last_executed(last_executed);
  }

  new_event->set_created(table->field[ET_FIELD_CREATED]->val_int());
  new_event->set_last_altered(table->field[ET_FIELD_MODIFIED]->val_int());

  if (thd->dd_client()->update(new_event))
  {
    trans_rollback_stmt(thd);
    return true;
  }

  return trans_commit_stmt(thd) || trans_commit(thd);
}


/**
  Searches for a LEX_STRING in an LEX_STRING array.

  @param[in] haystack  The array.
  @param[in] needle    The string to search for.
  @param[in] cs        Charset info.

  @note The last LEX_STRING in the array should have str member set to NULL.

  @retval -1   Not found.
  @retval >=0  Ordinal position.
*/

static int find_string_in_array(LEX_STRING * const haystack,
                                LEX_STRING * const needle,
                                CHARSET_INFO * const cs)
{
  const LEX_STRING *pos;
  for (pos= haystack; pos->str; pos++)
    if (!cs->coll->strnncollsp(cs, (uchar *) pos->str, pos->length,
                               (uchar *) needle->str, needle->length))
    {
      return static_cast<int>(pos - haystack);
    }
  return -1;
}


/**
  Update the event's interval and status information in the DD.
*/

static bool set_status_and_interval_for_event(THD *thd, TABLE *table,
                                              Event_parse_data *et_parse_data)
{
  char *ptr;
  bool not_used= FALSE;
  MYSQL_TIME time;

  if (!table->field[ET_FIELD_INTERVAL_EXPR]->is_null())
    et_parse_data->expression= table->field[ET_FIELD_INTERVAL_EXPR]->val_int();
  else
    et_parse_data->expression= 0;

  /*
    If neither STARTS and ENDS is set, then both fields are empty.
    Hence, if ET_FIELD_EXECUTE_AT is empty there is an error.
  */
  et_parse_data->execute_at_null= table->field[ET_FIELD_EXECUTE_AT]->is_null();
  if (!et_parse_data->expression && !et_parse_data->execute_at_null)
  {
    if (table->field[ET_FIELD_EXECUTE_AT]->get_date(&time,
                                                    TIME_NO_ZERO_DATE))
      return true;
    et_parse_data->execute_at= my_tz_OFFSET0->TIME_to_gmt_sec(&time, &not_used);
  }

  /*
    We load the interval type from disk as string and then map it to
    an integer. This decouples the values of enum interval_type
    and values actually stored on disk. Therefore the type can be
    reordered without risking incompatibilities of data between versions.
  */
  if (!table->field[ET_FIELD_TRANSIENT_INTERVAL]->is_null())
  {
    int i;
    char buff[MAX_FIELD_WIDTH];
    String str(buff, sizeof(buff), &my_charset_bin);
    LEX_STRING tmp;

    table->field[ET_FIELD_TRANSIENT_INTERVAL]->val_str(&str);
    if (!(tmp.length= str.length()))
      return true;

    tmp.str= str.c_ptr_safe();

    i= find_string_in_array(interval_type_to_name, &tmp, system_charset_info);
    if (i < 0)
      return true;
    et_parse_data->interval= (interval_type) i;
  }

  if ((ptr= get_field(thd->mem_root,
                      table->field[ET_FIELD_STATUS])) == NULL)
    return true;

  switch (ptr[0])
  {
  case 'E' :
    et_parse_data->status= Event_parse_data::ENABLED;
    break;
  case 'S' :
    et_parse_data->status= Event_parse_data::SLAVESIDE_DISABLED;
    break;
  case 'D' :
  default:
    et_parse_data->status= Event_parse_data::DISABLED;
    break;
  }
  return false;
}


/**
   Create an entry in the DD for the event by reading all the
   event attributes stored in 'mysql.event' table.
*/

static bool migrate_event_to_dd(THD *thd, TABLE *event_table)
{
  char *ptr;
  MYSQL_TIME time;
  LEX_USER user_info;
  Event_parse_data et_parse_data;
  LEX_STRING event_body, event_body_utf8;

  et_parse_data.interval= INTERVAL_LAST;
  et_parse_data.identifier= NULL;

  if ((et_parse_data.definer.str=
       get_field(thd->mem_root, event_table->field[ET_FIELD_DEFINER])) == NULL)
    return true;
  et_parse_data.definer.length= strlen(et_parse_data.definer.str);

  if ((et_parse_data.name.str=
       get_field(thd->mem_root, event_table->field[ET_FIELD_NAME])) == NULL)
    return true;
  et_parse_data.name.length= strlen(et_parse_data.name.str);

  if ((et_parse_data.dbname.str=
       get_field(thd->mem_root, event_table->field[ET_FIELD_DB])) == NULL)
    return true;
  et_parse_data.dbname.length=  strlen(et_parse_data.dbname.str);

  if ((et_parse_data.comment.str=
       get_field(thd->mem_root, event_table->field[ET_FIELD_COMMENT])) == NULL)
    et_parse_data.comment.length= 0;
  else
    et_parse_data.comment.length= strlen(et_parse_data.comment.str);

  bool not_used= FALSE;
  et_parse_data.starts_null= event_table->field[ET_FIELD_STARTS]->is_null();
  if (!et_parse_data.starts_null)
  {
    event_table->field[ET_FIELD_STARTS]->get_date(&time, TIME_NO_ZERO_DATE);
    et_parse_data.starts= my_tz_OFFSET0->TIME_to_gmt_sec(&time, &not_used);
  }

  et_parse_data.ends_null= event_table->field[ET_FIELD_ENDS]->is_null();
  if (!et_parse_data.ends_null)
  {
    event_table->field[ET_FIELD_ENDS]->get_date(&time, TIME_NO_ZERO_DATE);
    et_parse_data.ends= my_tz_OFFSET0->TIME_to_gmt_sec(&time, &not_used);
  }

  et_parse_data.originator = event_table->field[ET_FIELD_ORIGINATOR]->val_int();

  if (set_status_and_interval_for_event(thd, event_table, &et_parse_data))
    return true;

  if ((ptr= get_field(thd->mem_root,
                      event_table->field[ET_FIELD_ORIGINATOR])) == NULL)
    return true;

 if ((ptr= get_field(thd->mem_root,
                      event_table->field[ET_FIELD_ON_COMPLETION])) == NULL)
   return true;


  et_parse_data.on_completion= (ptr[0]=='D'?
                                Event_parse_data::ON_COMPLETION_DROP:
                                Event_parse_data::ON_COMPLETION_PRESERVE);

  // Set up the event body.
  if ((event_body.str= get_field(thd->mem_root,
                                 event_table->field[ET_FIELD_BODY])) == NULL)
    return true;
  event_body.length= strlen(event_body.str);

  if ((event_body_utf8.str=
       get_field(thd->mem_root,
                 event_table->field[ET_FIELD_BODY_UTF8])) == NULL)
    return true;
  event_body_utf8.length= strlen(event_body_utf8.str);
  et_parse_data.body_changed= true;

  Routine_event_context_guard event_ctx_guard(thd);

  thd->variables.sql_mode=
    (sql_mode_t)event_table->field[ET_FIELD_SQL_MODE]->val_int();

  // Holders for user name and host name used in parse user.
  char definer_user_name_holder[USERNAME_LENGTH + 1];
  char definer_host_name_holder[HOSTNAME_LENGTH + 1];
  memset(&user_info, 0, sizeof(LEX_USER));
  user_info.user= { definer_user_name_holder, USERNAME_LENGTH };
  user_info.host= { definer_host_name_holder, HOSTNAME_LENGTH };

  parse_user(et_parse_data.definer.str, et_parse_data.definer.length,
             definer_user_name_holder, &user_info.user.length,
             definer_host_name_holder, &user_info.host.length);

  load_event_creation_context(thd, event_table, &et_parse_data);

  // Disable autocommit option in thd variable
  Disable_autocommit_guard autocommit_guard(thd);

  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  const dd::Schema *schema= nullptr;
  if (thd->dd_client()->acquire(et_parse_data.dbname.str, &schema))
    return true;
  DBUG_ASSERT(schema != nullptr);

  if (dd::create_event(thd, *schema, et_parse_data.name.str,
                       event_body.str, event_body_utf8.str, &user_info,
                       &et_parse_data))
  {
    trans_rollback_stmt(thd);
    // Full rollback we have THD::transaction_rollback_request.
    trans_rollback(thd);
    return true;
  }

  if (trans_commit_stmt(thd) || trans_commit(thd))
    return true;

  return (update_event_timing_fields(thd, event_table, et_parse_data.dbname.str,
                                     et_parse_data.name.str));
}


/**
   Migrate all the events from 'mysql.event' to 'events' DD table.
*/

bool migrate_events_to_dd(THD *thd)
{
  TABLE *event_table;
  TABLE_LIST tables, *table_list;
  int error= 0;
  uint flags= MYSQL_LOCK_IGNORE_TIMEOUT;
  DML_prelocking_strategy prelocking_strategy;
  MEM_ROOT records_mem_root;

  tables.init_one_table("mysql", 5, "event", 5, "event", TL_READ);

  table_list= &tables;
  if (open_and_lock_tables(thd, table_list, flags, &prelocking_strategy))
  {
    close_thread_tables(thd);
    sql_print_error("Failed to open mysql.event Table.");
    return true;
  }

  event_table= tables.table;
  event_table->use_all_columns();

  if (table_intact.check(thd, event_table, &event_table_def))
  {
    // check with table format too before returning error.
    if (table_intact.check(thd, event_table, &event_table_def_old))
    {
      close_thread_tables(thd);
      return true;
    }
  }

  System_table_close_guard event_table_guard(thd, event_table);

  // Initialize time zone support infrastructure since the information
  // is not available during upgrade.
  my_tz_init(thd, default_tz_name, 0);

  if (event_table->file->ha_index_init(0, 1))
  {
    sql_print_error("Failed to read mysql.event table.");
    goto err;
  }

  // Read the first row in the 'event' table via index.
  if ((error= event_table->file->ha_index_first(event_table->record[0])))
  {
    if (error == HA_ERR_END_OF_FILE)
    {
      my_tz_free();
      return false;
    }
    sql_print_error("Failed to read mysql.event table.");
    goto err;
  }

  init_sql_alloc(PSI_NOT_INSTRUMENTED, &records_mem_root,
                 MEM_ROOT_BLOCK_SIZE, 0);
  thd->mem_root= &records_mem_root;

  if (migrate_event_to_dd(thd, event_table))
    goto err;

  // Read the next row in 'event' table via index.
  while (!(error= event_table->file->ha_index_next(event_table->record[0])))
  {
    if (migrate_event_to_dd(thd, event_table))
      goto err;
  }

  if (error != HA_ERR_END_OF_FILE)
  {
    sql_print_error("Failed to read mysql.event table.");
    goto err;
  }

  my_tz_free();
  free_root(&records_mem_root, MYF(0));
  return false;

err:
  my_tz_free();
  free_root(&records_mem_root, MYF(0));
  return true;
}


/**
  Set st_sp_chistics for routines.
*/

static bool set_st_sp_chistics(THD *thd, TABLE *proc_table,
                               st_sp_chistics *chistics)
{
  char *ptr;
  size_t length;
  char buff[65];
  String str(buff, sizeof(buff), &my_charset_bin);

  memset(chistics, 0, sizeof(st_sp_chistics));

  if ((ptr= get_field(thd->mem_root,
                      proc_table->field[MYSQL_PROC_FIELD_ACCESS])) == NULL)
    return true;

  switch (ptr[0]) {
  case 'N':
    chistics->daccess= SP_NO_SQL;
    break;
  case 'C':
    chistics->daccess= SP_CONTAINS_SQL;
    break;
  case 'R':
    chistics->daccess= SP_READS_SQL_DATA;
    break;
  case 'M':
    chistics->daccess= SP_MODIFIES_SQL_DATA;
    break;
  default:
    chistics->daccess= SP_DEFAULT_ACCESS_MAPPING;
  }

  // Deterministic
  if ((ptr=
       get_field(thd->mem_root,
                 proc_table->field[MYSQL_PROC_FIELD_DETERMINISTIC])) == NULL)
    return true;

  chistics->detistic= (ptr[0] == 'N' ? FALSE : TRUE);

  // Security type
  if ((ptr=
       get_field(thd->mem_root,
                 proc_table->field[MYSQL_PROC_FIELD_SECURITY_TYPE])) == NULL)
    return true;

  chistics->suid= (ptr[0] == 'I' ? SP_IS_NOT_SUID : SP_IS_SUID);

  // Fetch SP/SF comment
  proc_table->field[MYSQL_PROC_FIELD_COMMENT]->val_str(&str, &str);

  ptr= 0;
  if ((length= str.length()))
    ptr= strmake_root(thd->mem_root, str.ptr(), length);
  chistics->comment.str= ptr;
  chistics->comment.length= length;

  return false;
}


/**
  This function migrate one SP/SF from mysql.proc to routines DD table.
  One record in mysql.proc is metadata for one SP/SF. This function
  parses one record to extract metadata required and store it in DD table.
*/

static bool migrate_routine_to_dd(THD *thd, TABLE *proc_table)
{
  const char *params, *returns, *body, *definer;
  char *sp_db, *sp_name1;
  sp_head *sp= nullptr;
  enum_sp_type routine_type;
  LEX_USER user_info;

  // Fetch SP/SF name, datbase name, definer and type.
  if ((sp_db= get_field(thd->mem_root,
                        proc_table->field[MYSQL_PROC_FIELD_DB])) == NULL)
    return true;

  if ((sp_name1= get_field(thd->mem_root,
                           proc_table->field[MYSQL_PROC_FIELD_NAME])) == NULL)
    return true;

  if ((definer= get_field(thd->mem_root,
                          proc_table->field[MYSQL_PROC_FIELD_DEFINER])) == NULL)
    return true;

  routine_type=
      (enum_sp_type) proc_table->field[MYSQL_PROC_MYSQL_TYPE]->val_int();

  // Fetch SP/SF parameters string
  if ((params=
       get_field(thd->mem_root,
                 proc_table->field[MYSQL_PROC_FIELD_PARAM_LIST])) == NULL)
    params= "";

  // Create return type string for SF
  if (routine_type == enum_sp_type::PROCEDURE)
    returns= "";
  else if ((returns=
            get_field(thd->mem_root,
                      proc_table->field[MYSQL_PROC_FIELD_RETURNS])) == NULL)
    return true;

  st_sp_chistics chistics;
  if (set_st_sp_chistics(thd, proc_table, &chistics))
    return true;

  // Fetch SP/SF created and modified timestamp
  longlong created= proc_table->field[MYSQL_PROC_FIELD_CREATED]->val_int();
  longlong modified= proc_table->field[MYSQL_PROC_FIELD_MODIFIED]->val_int();

  // Fetch SP/SF body
  if ((body= get_field(thd->mem_root,
                       proc_table->field[MYSQL_PROC_FIELD_BODY])) == NULL)
    return true;

  Routine_event_context_guard routine_ctx_guard(thd);

  thd->variables.sql_mode=
    (sql_mode_t) proc_table->field[MYSQL_PROC_FIELD_SQL_MODE]->val_int();

  LEX_CSTRING sp_db_str;
  LEX_STRING sp_name_str;

  sp_db_str.str= sp_db;
  sp_db_str.length= strlen(sp_db);
  sp_name_str.str= sp_name1;
  sp_name_str.length= strlen(sp_name1);

  sp_name sp_name_obj= sp_name(sp_db_str, sp_name_str, true);
  sp_name_obj.init_qname(thd);

  // Create SP creation context to be used in db_load_routine()
  Stored_program_creation_ctx *creation_ctx=
  Stored_routine_creation_ctx::load_from_db(thd, &sp_name_obj, proc_table);

  // Holders for user name and host name used in parse user.
  char definer_user_name_holder[USERNAME_LENGTH + 1];
  char definer_host_name_holder[HOSTNAME_LENGTH + 1];
  memset(&user_info, 0, sizeof(LEX_USER));
  user_info.user= { definer_user_name_holder, USERNAME_LENGTH };
  user_info.host= { definer_host_name_holder, HOSTNAME_LENGTH };

  // Parse user string to separate user name and host
  parse_user(definer, strlen(definer),
             definer_user_name_holder, &user_info.user.length,
             definer_host_name_holder, &user_info.host.length);

  // Disable autocommit option in thd variable
  Disable_autocommit_guard autocommit_guard(thd);

  // This function fixes sp_head to use in sp_create_routine()
  if (db_load_routine(thd, routine_type, sp_db_str.str, sp_db_str.length,
                      sp_name_str.str, sp_name_str.length, &sp,
                      thd->variables.sql_mode, params, returns, body, &chistics,
                      definer_user_name_holder, definer_host_name_holder,
                      created, modified, creation_ctx))
  {
    /*
      Parsing of routine body failed, report a warning and use empty
      routine body.
    */
    sql_print_warning("Parsing '%s.%s' routine body failed. "
                      "Creating routine without parsing routine body",
                      sp_db_str.str, sp_name_str.str);

    LEX_CSTRING sr_body;
    if (routine_type == enum_sp_type::FUNCTION)
      sr_body= { STRING_WITH_LEN("RETURN NULL") };
    else
      sr_body= { STRING_WITH_LEN("BEGIN END") };

    if (db_load_routine(thd, routine_type, sp_db_str.str, sp_db_str.length,
                        sp_name_str.str, sp_name_str.length, &sp,
                        thd->variables.sql_mode, params, returns, sr_body.str,
                        &chistics, definer_user_name_holder,
                        definer_host_name_holder, created,
                        modified, creation_ctx))
      goto err;

    // Set actual routine body.
    sp->m_body.str= const_cast<char*>(body);
    sp->m_body.length= strlen(body);
  }

  // Create entry for SP/SF in DD table.
  if (sp_create_routine(thd, sp, &user_info))
    goto err;

  if (sp != nullptr)            // To be safe
    sp_head::destroy(sp);

  return false;

err:
   if (sp != nullptr)            // To be safe
     sp_head::destroy(sp);
   return true;
}


/**
  Migrate Stored Procedure and Functions
  from mysql.proc to routines dd table.
*/

bool migrate_routines_to_dd(THD *thd)
{
  TABLE *proc_table;
  TABLE_LIST tables, *table_list;
  int error= 0;
  uint flags= MYSQL_LOCK_IGNORE_TIMEOUT;
  DML_prelocking_strategy prelocking_strategy;
  MEM_ROOT records_mem_root;

  tables.init_one_table("mysql", 5, "proc", 4, "proc", TL_READ);

  table_list= &tables;
  if (open_and_lock_tables(thd, table_list, flags, &prelocking_strategy))
  {
    close_thread_tables(thd);
    sql_print_error("Failed to open mysql.proc Table.");
    return true;
  }

  proc_table= tables.table;
  proc_table->use_all_columns();

  if (table_intact.check(thd, proc_table, &proc_table_def))
  {
    // Check with old format too before returning error
    if (table_intact.check(thd, proc_table, &proc_table_def_old))
    {
      close_thread_tables(thd);
      return true;
    }
  }

  System_table_close_guard proc_table_guard(thd, proc_table);

  if (proc_table->file->ha_index_init(0, 1))
  {
    sql_print_error("Failed to read mysql.proc table.");
    return true;
  }

  // Read first record from mysql.proc table. Return if table is empty.
  if ((error= proc_table->file->ha_index_first(proc_table->record[0])))
  {
    if (error == HA_ERR_END_OF_FILE)
      return false;

    sql_print_error("Failed to read mysql.proc table.");
    return true;
  }

  init_sql_alloc(PSI_NOT_INSTRUMENTED, &records_mem_root,
                 MEM_ROOT_BLOCK_SIZE, 0);
  thd->mem_root= &records_mem_root;

  // Migrate first record read to dd routines table.
  if (migrate_routine_to_dd(thd, proc_table))
    goto err;

  // Read one record from mysql.proc table and
  // migrate it until all records are finished
  while (!(error= proc_table->file->ha_index_next(proc_table->record[0])))
  {
    if (migrate_routine_to_dd(thd, proc_table))
      goto err;
  }

  if (error != HA_ERR_END_OF_FILE)
  {
    sql_print_error("Failed to read mysql.proc table.");
    goto err;
  }

  free_root(&records_mem_root, MYF(0));
  return false;

err:
  free_root(&records_mem_root, MYF(0));
  return true;
}


/**
  Scan the database to identify all .frm files.
  Triggers existence will be checked only for tables found here.
*/

bool find_files_with_metadata(THD *thd, const char *dbname,
                              bool is_fix_view_cols_and_deps)
{
  uint i;
  MY_DIR *a;
  String_type path;
  bool error= false;

  path.assign(mysql_real_data_home);
  path += dbname;

  if (!(a = my_dir(path.c_str(), MYF(MY_WANT_STAT))))
  {
    sql_print_error("Error in opening directory %s", path.c_str());
    return true;
  }
  for (i = 0; i < (uint)a->number_off_files; i++)
  {
    String_type file;

    file.assign(a->dir_entry[i].name);
    if (file.at(0)  == '.')
        continue;

    if(!MY_S_ISDIR(a->dir_entry[i].mystat->st_mode))
    {
      String_type file_ext;
      char schema_name[NAME_LEN + 1];
      char table_name[NAME_LEN + 1];

      if (file.size() < 4)
        continue;

      file_ext.assign(file.c_str() + file.size() - 4);

      // Skip if it is not .frm file.
      if (file_ext.compare(reg_ext))
        continue;

      // Skip for temporary tables.
      if (is_prefix(file.c_str(), tmp_file_prefix))
        continue;

      // Get the name without the file extension.
      file.erase(file.size() - 4, 4);
      // Construct the schema name from its canonical format.
      filename_to_tablename(dbname, schema_name, sizeof(schema_name));
      filename_to_tablename(file.c_str(), table_name, sizeof(table_name));

      /*
        Skip mysql.plugin tables during upgrade of user and system tables as
        it has been upgraded already after creating DD tables.

        Skip mysql.innodb_table_stats, mysql.innodb_index_stats tables
        during upgrade. These table are part of Dictionary tables now.
        Dictionary table creation framework handles creation of these tables.
      */

      bool is_skip_table= ((strcmp(schema_name, "mysql") == 0) &&
                          ((strcmp(table_name, "plugin") == 0) ||
                           (strcmp(table_name, "innodb_table_stats") == 0) ||
                           (strcmp(table_name, "innodb_index_stats") == 0)));

      if (is_skip_table)
        continue;

      // Create an entry in the new DD.
      bool result= false;
      result= migrate_table_to_dd(thd, schema_name, table_name,
                                  is_fix_view_cols_and_deps);

      // Don't abort upgrade if error is in upgrading Performance Schema table.
      if (result && (strcmp(dbname, "performance_schema") == 0))
        result= false;

      /*
        Set error status, but don't abort upgrade
        as we want to process all tables.
      */
      error|= result;
    }
  }
  my_dirend(a);
  return error;
}


/**
  Scans datadir for databases and lists all the database names.
*/

bool find_schema_from_datadir(THD *thd, std::vector<String_type> *db_name)
{
  MY_DIR *a;
  uint i;
  FILEINFO *file;

  if (!(a = my_dir(mysql_real_data_home, MYF(MY_WANT_STAT))))
    return true;

  for (i = 0; i < (uint)a->number_off_files; i++)
  {
    file= a->dir_entry+i;

    if (file->name[0]  == '.')
      continue;

    if (MY_S_ISDIR(a->dir_entry[i].mystat->st_mode))
    {
      db_name->push_back(a->dir_entry[i].name);
      continue;
    }
  }

  my_dirend(a);
  return false;
}


/**
  Check if it is a file extension which should be moved
  to backup_metadata_57 folder upgrade upgrade is successful.
*/

static bool check_file_extension(const String_type &extn)
{
  // Check for extensions
  if (extn.size() < 4)
    return false;

  return ((extn.compare(extn.size() - 4, 4, reg_ext) == 0) ||
          (extn.compare(extn.size() - 4, 4, TRG_EXT) == 0) ||
          (extn.compare(extn.size() - 4, 4, TRN_EXT) == 0) ||
          (extn.compare(extn.size() - 4, 4, PAR_EXT) == 0) ||
          (extn.compare(extn.size() - 4, 4, OPT_EXT) == 0) ||
          (extn.compare(extn.size() - 4, 4, ISL_EXT) == 0));
}


/**
  Cleanup and create the metadata backup in a new folder
  after successful upgrade.
*/
void create_metadata_backup(THD *thd)
{
  uint i;
  MY_DIR *a, *b;
  String_type path;
  char to_path[FN_REFLEN];
  char from_path[FN_REFLEN];

  std::vector<String_type> db_name;

  (void) execute_query(thd,
                       "RENAME TABLE mysql.proc TO mysql.proc_backup_57");
  (void) execute_query(thd,
                       "RENAME TABLE mysql.event TO mysql.event_backup_57");

  path.assign(mysql_real_data_home);
  const char *backup_folder_name="backup_metadata_57";
  char backup_folder_location[FN_REFLEN];

  if (fn_format(backup_folder_location, backup_folder_name, mysql_data_home, "",
                MYF(MY_UNPACK_FILENAME | MY_SAFE_PATH)) == NULL)
    return;

  // Create 'backup_metadata_57' folder in data directory
  if (my_mkdir(backup_folder_location ,0777,MYF(0)) < 0)
  {
    sql_print_error("Error in creating folder %s",
                     backup_folder_location);
    return;
  }

  if (!(a = my_dir(path.c_str(), MYF(MY_WANT_STAT))))
  {
    sql_print_error("Error in opening the backup folder %s",
                     backup_folder_location);
    return;
  }

  // Scan all files and folders in data directory.
  for (i = 0; i < (uint) a->number_off_files; i++)
  {
    String_type file;

    file.assign(a->dir_entry[i].name);
    if (file.at(0)  == '.')
        continue;

    // If its a folder, add it to the vector.
    if(MY_S_ISDIR(a->dir_entry[i].mystat->st_mode))
    {
      if (file.compare(backup_folder_name) != 0)
        db_name.push_back(a->dir_entry[i].name);
    }
    else
    {
      String_type file_ext;

      if (file.size() < 4)
        continue;

      file_ext.assign(file.c_str() + file.size() - 4);
      // Get the name without the file extension.
      if (check_file_extension(file_ext))
      {
        if (fn_format(to_path, file.c_str(), backup_folder_location, "",
                      MYF(MY_UNPACK_FILENAME | MY_SAFE_PATH)) == NULL)
          return;

        if (fn_format(from_path, file.c_str(), mysql_real_data_home, "",
                      MYF(MY_UNPACK_FILENAME | MY_SAFE_PATH)) == NULL)
          return;

        (void) mysql_file_rename(key_file_misc, from_path,
                                 to_path, MYF(0));
      }
    }
  }

  // Iterate through the databases list
  for (String_type str: db_name)
  {
    String_type dir_name= str.c_str();
    char dir_path[FN_REFLEN];

    if (fn_format(dir_path, dir_name.c_str(), path.c_str(),  "",
                  MYF(MY_UNPACK_FILENAME | MY_SAFE_PATH)) == NULL)
      continue;

    if (!(b = my_dir(dir_path, MYF(MY_WANT_STAT))))
      continue;

    char backup_folder_dir_location[FN_REFLEN];

    if (fn_format(backup_folder_dir_location, dir_name.c_str(),
                  backup_folder_location, "",
                  MYF(MY_UNPACK_FILENAME | MY_SAFE_PATH)) == NULL)
      continue;


    if (my_mkdir(backup_folder_dir_location, 0777,MYF(0)) < 0)
    {
      sql_print_error("Error in creating folder %s",
                       backup_folder_dir_location);
      continue;
    }

    // Scan all files and folders in data directory.
    for (i = 0; i < (uint) b->number_off_files; i++)
    {
      String_type file;
      file.assign(b->dir_entry[i].name);

      if ((file.at(0)  == '.') || (file.size() < 4))
        continue;

      String_type file_ext;
      file_ext.assign(file.c_str() + file.size() - 4);

       // Get the name without the file extension.
      if (check_file_extension(file_ext))
      {
        if (fn_format(to_path, file.c_str(), backup_folder_dir_location, "",
                      MYF(MY_UNPACK_FILENAME | MY_SAFE_PATH)) == NULL)
          continue;

        if (fn_format(from_path, file.c_str(), dir_path, "",
                      MYF(MY_UNPACK_FILENAME | MY_SAFE_PATH)) == NULL)
          continue;

        (void) mysql_file_rename(key_file_misc, from_path,
                                 to_path, MYF(0));
      }
    }
    my_dirend(b);
  }

  my_dirend(a);
}


/**
  Check whether the DD tables can be created in mysql schema
  Report error if tables having the same as that of DD tables
  exists.
*/

bool check_for_dd_tables()
{
  // Iterate over DD tables, check .frm files
  for (System_tables::Const_iterator it= System_tables::instance()->begin();
       it != System_tables::instance()->end();
       ++it)
  {
    String_type table_name= (*it)->entity()->name();
    String_type schema_name(MYSQL_SCHEMA_NAME.str);

    const System_tables::Types *table_type= System_tables::instance()->
      find_type(schema_name, table_name);

    bool is_innodb_stats_table= (table_type != nullptr) &&
                                (*table_type == System_tables::Types::SUPPORT);
    is_innodb_stats_table &=
      (strcmp(table_name.c_str(), "innodb_table_stats") == 0) ||
      (strcmp(table_name.c_str(), "innodb_index_stats") == 0);

    if (is_innodb_stats_table)
      continue;

    char path[FN_REFLEN+1];
    bool not_used;
    build_table_filename(path, sizeof(path) - 1, "mysql", table_name.c_str(),
                         reg_ext, 0, &not_used);

    if (!my_access(path, F_OK))
    {
      sql_print_error("Found %s file in mysql schema. DD will create .ibd "
                       "file with same name. Please rename table and start "
                       "upgrade process again.", path);
      return true;
    }
  }
  return false;
}


/**
  Drop all Data Dictionary tables and all .SDI files created during upgrade.
*/

void drop_dd_tables_and_sdi_files(THD *thd,
       const System_tables::Const_iterator &last_table)
{
  uint i, j;
  bool error;

  error= execute_query(thd, "SET FOREIGN_KEY_CHECKS= 0");

  // Iterate over DD tables, delete tables
  for (System_tables::Const_iterator it= System_tables::instance()->begin();
       it != last_table;
       ++it)
  {
    String_type table_name= (*it)->entity()->name();
    String_type schema_name(MYSQL_SCHEMA_NAME.str);

    const System_tables::Types *table_type= System_tables::instance()->
      find_type(schema_name, table_name);

    bool is_innodb_stats_table= (table_type != nullptr) &&
                                (*table_type == System_tables::Types::SUPPORT);
    is_innodb_stats_table &=
      (strcmp(table_name.c_str(), "innodb_table_stats") == 0) ||
      (strcmp(table_name.c_str(), "innodb_index_stats") == 0);

    if (is_innodb_stats_table)
      continue;

    String_type query;
    query.assign("DROP TABLE mysql.");
    query= query + table_name;
    // Try to delete all DD tables even if error occurs.
    error|= execute_query(thd, query);
  }

  error|= execute_query(thd, "SET FOREIGN_KEY_CHECKS= 1");

  if (error)
    sql_print_error("Unable to drop the DD tables during clean "
                     "up after upgrade failure");

  // Iterate in data directory and delete all .SDI files
  MY_DIR *a, *b;
  String_type path;

  path.assign(mysql_real_data_home);

  if (!(a = my_dir(path.c_str(), MYF(MY_WANT_STAT))))
  {
    sql_print_error("Unable to open the data directory %s during "
                     "clean up after upgrade failed", path.c_str());
    return;
  }

  // Scan all files and folders in data directory.
  for (i = 0; i < (uint)a->number_off_files; i++)
  {
    String_type file;

    file.assign(a->dir_entry[i].name);
    if (file.at(0)  == '.')
       continue;

    // If its a folder, iterate it to delete all .SDI files
    if(MY_S_ISDIR(a->dir_entry[i].mystat->st_mode))
    {
       char dir_path[FN_REFLEN];
       if (fn_format(dir_path, file.c_str(), path.c_str(), "",
                     MYF(MY_UNPACK_FILENAME | MY_SAFE_PATH)) == NULL)
       {
         sql_print_error("Failed to set path %s", file.c_str());
         continue;
       }

      if (!(b = my_dir(dir_path, MYF(MY_WANT_STAT))))
      {
        sql_print_error("Failed to open to dir %s", dir_path);
        continue;
      }

      // Scan all files and folders in data directory.
      for (j = 0; j < (uint)b->number_off_files; j++)
      {
        String_type file2;
        file2.assign(b->dir_entry[j].name);

        if ((file2.at(0)  == '.') || (file2.size() < 4))
          continue;

        String_type file_ext;
        file_ext.assign(file2.c_str() + file2.size() - 4);
        if (file_ext.compare(0, 4, SDI_EXT) == 0)
        {
          char to_path[FN_REFLEN];
          if (fn_format(to_path, file2.c_str(), dir_path, "",
                        MYF(MY_UNPACK_FILENAME | MY_SAFE_PATH)) == NULL)
          {
            sql_print_error("Failed to set path %s.", file2.c_str());
            continue;
          }

          (void) mysql_file_delete(key_file_sdi, to_path, MYF(MY_WME));
        }
      }
      my_dirend(b);
    }
    else
    {
      // Delete .SDI files in data directory created for schema.
      String_type file_ext;
      if (file.size() < 4)
        continue;
      file_ext.assign(file.c_str() + file.size() - 4);
      // Get the name without the file extension.
      if (file_ext.compare(0, 4, SDI_EXT) == 0)
      {
        char to_path[FN_REFLEN];
        if (fn_format(to_path, file.c_str(), path.c_str(), "",
                      MYF(MY_UNPACK_FILENAME | MY_SAFE_PATH)) == NULL)
        {
          sql_print_error("Failed to set path %s.", file.c_str());
          continue;
        }
        (void) mysql_file_delete(key_file_sdi, to_path, MYF(MY_WME));
      }
    }
  }

  my_dirend(a);
} // drop_dd_table
} // namespace dd
