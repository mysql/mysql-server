/*
   Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"
#include <stddef.h>
#include "sql_const.h"                          // MAX_FIELD_WIDTH
#include "field.h"                              // Field
#include "log.h"                                // sql_print_warning
#include "m_string.h"                           // LEX_CSTRING
#include "my_dbug.h"                            // DBUG_ASSERT
#include "opt_costconstants.h"
#include "opt_costconstantcache.h"
#include "template_utils.h"                     // pointer_cast
#include "records.h"                            // READ_RECORD
#include "sql_base.h"                           // open_and_lock_tables
#include "sql_class.h"                          // THD
#include "sql_lex.h"                      // lex_start/lex_end
#include "sql_string.h"                         // String
#include "table.h"                              // TABLE
#include "thr_lock.h"                           // TL_READ
#include "transaction.h"
#include "sql_tmp_table.h"                // init_cache_tmp_engine_properties

Cost_constant_cache *cost_constant_cache= NULL;


static void read_cost_constants(Cost_model_constants* cost_constants);


/**
  Minimal initialization of the object. The main initialization is done
  by calling init().
*/

Cost_constant_cache::Cost_constant_cache()
  : current_cost_constants(NULL), m_inited(false)
{
}


Cost_constant_cache::~Cost_constant_cache()
{
  // Verify that close has been called
  DBUG_ASSERT(current_cost_constants == NULL);
  DBUG_ASSERT(m_inited == false);
}


void Cost_constant_cache::init()
{
  DBUG_ENTER("Cost_constant_cache::init");

  DBUG_ASSERT(m_inited == false);

  // Initialize the mutex that is used for protecting the cost constants
  mysql_mutex_init(key_LOCK_cost_const, &LOCK_cost_const,
                   MY_MUTEX_INIT_FAST);

  // Create cost constants from constants found in the source code
  Cost_model_constants *cost_constants= create_defaults();

  // Set this to be the current set of cost constants
  update_current_cost_constants(cost_constants);

  m_inited= true;

  DBUG_VOID_RETURN;
}


void Cost_constant_cache::close()
{
  DBUG_ENTER("Cost_constant_cache::close");

  DBUG_ASSERT(m_inited);

  if (m_inited == false)
    DBUG_VOID_RETURN;                           /* purecov: inspected */

  // Release the current cost constant set
  if (current_cost_constants)
  {
    release_cost_constants(current_cost_constants);
    current_cost_constants= NULL;
  }

  // To ensure none is holding the mutex when deleting it, lock/unlock it.
  mysql_mutex_lock(&LOCK_cost_const);
  mysql_mutex_unlock(&LOCK_cost_const);

  mysql_mutex_destroy(&LOCK_cost_const);

  m_inited= false;

  DBUG_VOID_RETURN;
}


void Cost_constant_cache::reload()
{
  DBUG_ENTER("Cost_constant_cache::reload");
  DBUG_ASSERT(m_inited= true);

  // Create cost constants from the constants defined in the source code
  Cost_model_constants *cost_constants= create_defaults();

  // Update the cost constants from the database tables
  read_cost_constants(cost_constants);

  // Set this to be the current set of cost constants
  update_current_cost_constants(cost_constants);

  DBUG_VOID_RETURN;
}



Cost_model_constants *Cost_constant_cache::create_defaults() const
{
  // Create default cost constants
  Cost_model_constants *cost_constants= new Cost_model_constants();

  return cost_constants;
}


void
Cost_constant_cache::update_current_cost_constants(Cost_model_constants *new_cost_constants)
{
  /*
    Increase the ref counter to ensure that the new cost constants
    are not deleted until next time we have a new set of cost constants.
  */
  new_cost_constants->inc_ref_count();

  /*
    The mutex needs to be held for the entire period for removing the
    current cost constants and adding the new cost constants to ensure
    that no user of this class can access the object when there is no
    current cost constants.
  */
  mysql_mutex_lock(&LOCK_cost_const);

  // Release the current cost constants by decrementing the ref counter
  if (current_cost_constants)
  {
    const unsigned int ref_count= current_cost_constants->dec_ref_count();

    // If there is none using the current cost constants then delete them
    if (ref_count == 0)
      delete current_cost_constants;
  }

  // Start to use the new cost constants
  current_cost_constants= new_cost_constants;

  mysql_mutex_unlock(&LOCK_cost_const);
}


/**
  Write warnings about illegal entries in the server_cost table

  The warnings are written to the MySQL error log.

  @param cost_name  name of the cost constant
  @param value      value it was attempted set to
  @param err        error status
*/

static void report_server_cost_warnings(const LEX_CSTRING &cost_name,
                                        double value,
                                        cost_constant_error error)
{
  switch(error)
  {
  case UNKNOWN_COST_NAME:
    sql_print_warning("Unknown cost constant \"%s\" in mysql.server_cost table\n",
                      cost_name.str);
    break;
  case INVALID_COST_VALUE:
    sql_print_warning("Invalid value for cost constant \"%s\" in mysql.server_cost table: %.1f\n",
                      cost_name.str, value);
    break;
  default:
    DBUG_ASSERT(false);                         /* purecov: inspected */
  }
}


/**
  Write warnings about illegal entries in the engine_cost table

  The warnings are written to the MySQL error log.

  @param se_name          name of storage engine
  @param storage_category device type
  @param cost_name        name of the cost constant
  @param value            value it was attempted set to
  @param err              error status
*/

static void report_engine_cost_warnings(const LEX_CSTRING &se_name,
                                        int storage_category,
                                        const LEX_CSTRING &cost_name,
                                        double value,
                                        cost_constant_error error)
{
  switch(error)
  {
  case UNKNOWN_COST_NAME:
    sql_print_warning("Unknown cost constant \"%s\" in mysql.engine_cost table\n",
                      cost_name.str);
    break;
  case UNKNOWN_ENGINE_NAME:
    sql_print_warning("Unknown storage engine \"%s\" in mysql.engine_cost table\n",
                      se_name.str);
    break;
  case INVALID_DEVICE_TYPE:
    sql_print_warning("Invalid device type %d for \"%s\" storage engine for cost constant \"%s\" in mysql.engine_cost table\n",
                      storage_category, se_name.str, cost_name.str);
    break;
  case INVALID_COST_VALUE:
    sql_print_warning("Invalid value for cost constant \"%s\" for \"%s\" storage engine and device type %d in mysql.engine_cost table: %.1f\n",
                      cost_name.str, se_name.str, storage_category, value);
    break;
  default:
    DBUG_ASSERT(false);                         /* purecov: inspected */
  }
}


/**
  Read the table that contains the cost constants for the server.

  The table must already be opened. The cost constant object is updated
  with cost constants found in the configuration table.

  @param thd                    the THD
  @param table                  the table to read from
  @param cost_constants[in,out] cost constant object
*/

static void read_server_cost_constants(THD *thd, TABLE *table,
                                       Cost_model_constants* cost_constants)
{
  DBUG_ENTER("read_server_cost_constants");

  /*
    The server constant table has the following columns:

    cost_name   VARCHAR(64) NOT NULL COLLATE utf8_general_ci
    cost_value  FLOAT DEFAULT NULL
    last_update TIMESTAMP
    comment     VARCHAR(1024) DEFAULT NULL
  */

  READ_RECORD read_record_info;

  // Prepare to read from the table
  const bool ret= init_read_record(&read_record_info, thd, table, NULL, true,
                                   true, false);
  if (!ret)
  {
    table->use_all_columns();

    // Read one record
    while (!read_record_info.read_record(&read_record_info))
    {
      /*
        Check if a non-default value has been configured for this cost
        constant.
      */
      if (!table->field[1]->is_null())
      {
        char cost_name_buf[MAX_FIELD_WIDTH];
        String cost_name(cost_name_buf, sizeof(cost_name_buf),
                         &my_charset_utf8_general_ci);

        // Read the name of the cost constant
        table->field[0]->val_str(&cost_name);
        cost_name[cost_name.length()]= 0; // Null-terminate

        // Read the value this cost constant should have
        const float value= static_cast<float>(table->field[1]->val_real());

        // Update the cost model with this cost constant
        const LEX_CSTRING cost_constant= cost_name.lex_cstring();
        const cost_constant_error err=
          cost_constants->update_server_cost_constant(cost_constant, value);

        if (err != COST_CONSTANT_OK)
          report_server_cost_warnings(cost_constant, value, err);
      }
    }

    end_read_record(&read_record_info);
  }
  else
  {
    sql_print_warning("init_read_record returned error when reading from mysql.server_cost table.\n");
  }

  DBUG_VOID_RETURN;
}


/**
  Read the table that contains the cost constants for the storage engines.

  The table must already be opened. The cost constant object is updated
  with cost constants found in the configuration table.

  @param thd                    the THD
  @param table                  the table to read from
  @param cost_constants[in,out] cost constant object
*/

static void read_engine_cost_constants(THD *thd, TABLE *table,
                                       Cost_model_constants* cost_constants)
{
  DBUG_ENTER("read_engine_cost_constants");

  /*
    The engine constant table has the following columns:

    engine_name VARCHAR(64) NOT NULL COLLATE utf8_general_ci,
    device_type INTEGER NOT NULL,
    cost_name   VARCHAR(64) NOT NULL COLLATE utf8_general_ci,
    cost_value  FLOAT DEFAULT NULL,
    last_update TIMESTAMP
    comment     VARCHAR(1024) DEFAULT NULL,
  */

  READ_RECORD read_record_info;

  // Prepare to read from the table
  const bool ret= init_read_record(&read_record_info, thd, table, NULL, true,
                                   true, false);
  if (!ret)
  {
    table->use_all_columns();

    // Read one record
    while (!read_record_info.read_record(&read_record_info))
    {
      /*
        Check if a non-default value has been configured for this cost
        constant.
      */
      if (!table->field[3]->is_null())
      {
        char engine_name_buf[MAX_FIELD_WIDTH];
        String engine_name(engine_name_buf, sizeof(engine_name_buf),
                           &my_charset_utf8_general_ci);
        char cost_name_buf[MAX_FIELD_WIDTH];
        String cost_name(cost_name_buf, sizeof(cost_name_buf),
                         &my_charset_utf8_general_ci);

        // Read the name of the storage engine
        table->field[0]->val_str(&engine_name);
        engine_name[engine_name.length()]= 0; // Null-terminate

        // Read the device type
        const int device_type= static_cast<int>(table->field[1]->val_int());

        // Read the name of the cost constant
        table->field[2]->val_str(&cost_name);
        cost_name[cost_name.length()]= 0; // Null-terminate

        // Read the value this cost constant should have
        const float value= static_cast<float>(table->field[3]->val_real());

        // Update the cost model with this cost constant
        const LEX_CSTRING engine= engine_name.lex_cstring();
        const LEX_CSTRING cost_constant= cost_name.lex_cstring();
        const cost_constant_error err=
          cost_constants->update_engine_cost_constant(thd, engine,
                                                      device_type,
                                                      cost_constant, value);
        if (err != COST_CONSTANT_OK)
          report_engine_cost_warnings(engine, device_type, cost_constant,
                                      value, err);
      }
    }

    end_read_record(&read_record_info);
  }
  else
  {
    sql_print_warning("init_read_record returned error when reading from mysql.engine_cost table.\n");
  }
    
  DBUG_VOID_RETURN;
}


/**
  Read the cost configuration tables and update the cost constant set.

  The const constant set must be initialized with default values when
  calling this function.

  @param cost_constants set with cost constants
*/

static void read_cost_constants(Cost_model_constants* cost_constants)
{
  DBUG_ENTER("read_cost_constants");

  /*
    This function creates its own THD. If there exists a current THD this needs
    to be restored at the end of this function. The reason the current THD
    can not be used is that this might already have opened and closed tables
    and thus opening new tables will fail.
  */
  THD *orig_thd= current_thd;

  // Create and initialize a new THD.
  THD *thd= new THD;
  DBUG_ASSERT(thd);
  thd->thread_stack= pointer_cast<char*>(&thd);
  thd->store_globals();
  lex_start(thd);

  TABLE_LIST tables[2];
  tables[0].init_one_table(C_STRING_WITH_LEN("mysql"),
                           C_STRING_WITH_LEN("server_cost"),
                           "server_cost", TL_READ);
  tables[1].init_one_table(C_STRING_WITH_LEN("mysql"),
                           C_STRING_WITH_LEN("engine_cost"),
                           "engine_cost", TL_READ);
  tables[0].next_global= tables[0].next_local=
    tables[0].next_name_resolution_table= &tables[1];

  if (!open_and_lock_tables(thd, tables, MYSQL_LOCK_IGNORE_TIMEOUT))
  {
    DBUG_ASSERT(tables[0].table != NULL);
    DBUG_ASSERT(tables[1].table != NULL);

    // Read the server constants table
    read_server_cost_constants(thd, tables[0].table, cost_constants);
    // Read the storage engine table
    read_engine_cost_constants(thd, tables[1].table, cost_constants);
  }
  else
  {
    sql_print_warning("Failed to open optimizer cost constant tables\n");
  }

  trans_commit_stmt(thd);
  close_thread_tables(thd);
  lex_end(thd->lex);

  // Delete the locally created THD
  delete thd;

  // If the caller already had a THD, this must be restored
  if(orig_thd)
    orig_thd->store_globals();

  DBUG_VOID_RETURN;
}


void init_optimizer_cost_module(bool enable_plugins)
{
  DBUG_ASSERT(cost_constant_cache == NULL);
  cost_constant_cache= new Cost_constant_cache();
  cost_constant_cache->init();
  /*
    Initialize max_key_length and max_key_part_length for internal temporary
    table engines.
  */
  if (enable_plugins)
    init_cache_tmp_engine_properties();
}


void delete_optimizer_cost_module()
{
  if (cost_constant_cache)
  {
    cost_constant_cache->close();
    delete cost_constant_cache;
    cost_constant_cache= NULL;
  }
}


void reload_optimizer_cost_constants()
{
  if (cost_constant_cache)
    cost_constant_cache->reload();
}
