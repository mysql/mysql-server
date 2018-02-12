/* Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "log.h"
#include "my_default.h"                 // my_getopt_use_args_separator
#include "migrate_keyring.h"
#include "mysqld.h"
#include "mysqld_error.h"
#include "sql_plugin.h"                 // plugin_early_load_one
#include "violite.h"

/**
  Standard constructor
*/

Migrate_keyring::Migrate_keyring()
{
  m_source_plugin_handle= NULL;
  m_destination_plugin_handle= NULL;
  mysql= NULL;
}

/**
  This function does the following:
    1. Validate all keyring migration specific options.
    2. Get a connection handle by connecting to server if connection
       specific options are set.

   @param [in] argc               Pointer to argc of original program
   @param [in] argv               Pointer to argv of original program
   @param [in] source_plugin      Pointer to source plugin option
   @param [in] destination_plugin Pointer to destination plugin option
   @param [in] user               User to login to server
   @param [in] host               Host on which to connect to server
   @param [in] password           Password used to connect to server
   @param [in] socket             The socket file to use for connection
   @param [in] port               Port number to use for connection

   @return 0 Success
   @return 1 Failure

*/
bool Migrate_keyring::init(int  argc,
                           char **argv,
                           char *source_plugin,
                           char *destination_plugin,
                           char *user, char *host,
                           char *password, char *socket,
                           ulong port)
{
  DBUG_ENTER("Migrate_keyring::init");

  std::size_t found= std::string::npos;
  string equal("=");
  string so(".so");
  string dll(".dll");

  m_argc= argc;
  m_argv= argv;

  if (!source_plugin)
  {
    my_error(ER_KEYRING_MIGRATION_FAILURE, MYF(0),
             "Invalid --keyring-migration-source option.");
    DBUG_RETURN(true);
  }
  if (!destination_plugin)
  {
    my_error(ER_KEYRING_MIGRATION_FAILURE, MYF(0),
             "Invalid --keyring-migration-destination option.");
    DBUG_RETURN(true);
  }
  m_source_plugin_option= source_plugin;
  m_destination_plugin_option= destination_plugin;

  /* extract plugin name from the specified source plugin option */
  if ((found= m_source_plugin_option.find(equal)) != std::string::npos)
    m_source_plugin_name= m_source_plugin_option.substr(0, found);
  else if ((found= m_source_plugin_option.find(so)) != std::string::npos)
    m_source_plugin_name= m_source_plugin_option.substr(0, found);
  else if ((found= m_source_plugin_option.find(dll)) != std::string::npos)
    m_source_plugin_name= m_source_plugin_option.substr(0, found);
  else
  {
    my_error(ER_KEYRING_MIGRATION_FAILURE, MYF(0),
             "Invalid source plugin option value.");
    DBUG_RETURN(true);
  }

  /* extract plugin name from the specified destination plugin option */
  if ((found= m_destination_plugin_option.find(equal)) != std::string::npos)
    m_destination_plugin_name= m_destination_plugin_option.substr(0, found);
  else if ((found= m_destination_plugin_option.find(so)) != std::string::npos)
    m_destination_plugin_name= m_destination_plugin_option.substr(0, found);
  else if ((found= m_destination_plugin_option.find(dll)) != std::string::npos)
    m_destination_plugin_name= m_destination_plugin_option.substr(0, found);
  else
  {
    my_error(ER_KEYRING_MIGRATION_FAILURE, MYF(0),
             "Invalid destination plugin option value.");
    DBUG_RETURN(true);
  }

  /* if connect options are provided then initiate connection */
  if (migrate_connect_options)
  {
    ssl_start();
    /* initiate connection */
    mysql= mysql_init(NULL);

    enum mysql_ssl_mode ssl_mode= SSL_MODE_REQUIRED;
    mysql_options(mysql, MYSQL_OPT_SSL_MODE, &ssl_mode);
    mysql_options(mysql, MYSQL_OPT_CONNECT_ATTR_RESET, 0);
    mysql_options4(mysql, MYSQL_OPT_CONNECT_ATTR_ADD, "program_name",
                   "mysqld");
    mysql_options4(mysql, MYSQL_OPT_CONNECT_ATTR_ADD, "_client_role",
                   "keyring_migration_tool");

    if (!mysql_real_connect(mysql, host, user, password, "",
      port, socket, 0))
    {
      my_error(ER_KEYRING_MIGRATION_FAILURE, MYF(0),
               "Connection to server failed. Please check connection specific "
               "option values.");
      DBUG_RETURN(true);
    }
  }
  DBUG_RETURN(false);
}

/**
  This function does the following in sequence:
    1. Load source plugin.
    2. Load destination plugin.
    3. Disable access to keyring service APIs.
    4. Fetch all keys from source plugin and upon
       sucess store in destination plugin.
    5. Enable access to keyring service APIs.
    6. Unload source plugin.
    7. Unload destination plugin.

  NOTE: In case there is any error while fetching keys from source plugin,
  this function would remove all keys stored as part of fetch.

  @return 0 Success
  @return 1 Failure
*/
bool Migrate_keyring::execute()
{
  DBUG_ENTER("Migrate_keyring::execute");

  /* Load source plugin. */
  if (load_plugin(SOURCE_PLUGIN))
  {
    my_error(ER_KEYRING_MIGRATION_FAILURE, MYF(0),
             "Failed to initialize source keyring");
    DBUG_RETURN(true);
  }

  /* Load destination source plugin. */
  if (load_plugin(DESTINATION_PLUGIN))
  {
    my_error(ER_KEYRING_MIGRATION_FAILURE, MYF(0),
             "Failed to initialize destination keyring");
    DBUG_RETURN(true);
  }

  /* skip program name */
  m_argc--;
  m_argv++;
  /* check for invalid options */
  if (m_argc > 1)
  {
    struct my_option no_opts[]=
    {
      {0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
    };
    my_getopt_skip_unknown= 0;
    my_getopt_use_args_separator= true;
    if (handle_options(&m_argc, &m_argv, no_opts, NULL))
      unireg_abort(MYSQLD_ABORT_EXIT);

    if (m_argc > 1)
    {
      sql_print_error("Please specify options specific to keyring migration. Any "
                      "additional options can be ignored. NOTE: Although some options "
                      "are valid, migration tool can still report error example: plugin "
                      "variables for which plugin is not loaded yet.");
      unireg_abort(MYSQLD_ABORT_EXIT);
    }
  }
  /* Disable access to keyring service APIs */
  if (migrate_connect_options && disable_keyring_operations())
    goto error;

  /* Fetch all keys from source plugin and store into destination plugin. */
  if (fetch_and_store_keys())
    goto error;

  /* Enable access to keyring service APIs */
  if (migrate_connect_options)
    enable_keyring_operations();

  DBUG_RETURN(false);

error:
  /*
   Enable keyring_operations in case of error
  */
  if (migrate_connect_options)
    enable_keyring_operations();
  DBUG_RETURN(true);
}

/**
  Load plugin.

  @param [in] plugin_type        Indicates what plugin to be loaded

  @return 0 Success
  @return 1 Failure
*/
bool Migrate_keyring::load_plugin(enum_plugin_type plugin_type)
{
  DBUG_ENTER("Migrate_keyring::load_plugin");

  char* keyring_plugin= NULL;
  char* plugin_name= NULL;
  bool is_source_plugin= 0;

  if (plugin_type == SOURCE_PLUGIN)
    is_source_plugin= 1;

  if (is_source_plugin)
  {
    keyring_plugin= const_cast<char *>(m_source_plugin_option.c_str());
    plugin_name= const_cast<char *>(m_source_plugin_name.c_str());
  }
  else
  {
    keyring_plugin= const_cast<char *>(m_destination_plugin_option.c_str());
    plugin_name= const_cast<char *>(m_destination_plugin_name.c_str());
  }

  if (plugin_early_load_one(&m_argc, m_argv, keyring_plugin))
    goto error;
  else
  {
    /* set plugin handle */
    plugin_ref plugin;
    plugin= my_plugin_lock_by_name(0, to_lex_cstring(plugin_name),
                                   MYSQL_KEYRING_PLUGIN);
    if (plugin == NULL)
      goto error;

    if (is_source_plugin)
      m_source_plugin_handle= (st_mysql_keyring*)plugin_decl(plugin)->info;
    else
      m_destination_plugin_handle= (st_mysql_keyring*)plugin_decl(plugin)->info;

    plugin_unlock(0, plugin);
  }
  DBUG_RETURN(false);

error:
  if (is_source_plugin)
    my_error(ER_KEYRING_MIGRATION_FAILURE, MYF(0),
             "Failed to load source keyring plugin.");
  else
    my_error(ER_KEYRING_MIGRATION_FAILURE, MYF(0),
             "Failed to load destination keyring plugin.");
  DBUG_RETURN(true);
}

/**
  This function does the following in sequence:
    1. Initialize key iterator which will make iterator to position itself
       inorder to fetch a key.
    2. Using iterator get key ID and user name.
    3. Fetch the key information using key ID and user name.
    4. Store the fetched key into destination plugin.
    5. In case of errors remove keys from destination plugin.

  @return 0 Success
  @return 1 Failure
*/
bool Migrate_keyring::fetch_and_store_keys()
{
  DBUG_ENTER("Migrate_keyring::fetch_keys");

  bool error= FALSE;
  char key_id[MAX_KEY_LEN]= { 0 };
  char user_id[USERNAME_LENGTH]= { 0 };
  void *key=NULL;
  size_t key_len= 0;
  char *key_type= NULL;
  void *key_iterator= NULL;

  m_source_plugin_handle->mysql_key_iterator_init(&key_iterator);
  if (key_iterator == NULL)
  {
    my_error(ER_KEYRING_MIGRATION_FAILURE, MYF(0),
             "Initializing source keyring iterator failed.");
    DBUG_RETURN(true);
  }
  while(!error)
  {
    if (m_source_plugin_handle->
      mysql_key_iterator_get_key(key_iterator, key_id, user_id))
      break;

    /* using key_info metadata fetch the actual key */
    if (m_source_plugin_handle->mysql_key_fetch(key_id,
                                                &key_type,
                                                user_id,
                                                &key,
                                                &key_len))
    {
      /* fetch failed */
      string errmsg= "Fetching key (" + string(key_id) +
        ") from source plugin failed.";
      my_error(ER_KEYRING_MIGRATION_FAILURE, MYF(0),
               errmsg.c_str());
      error= TRUE;
    }
    else /* store the fetched key into destination plugin */
    {
      if (m_destination_plugin_handle->mysql_key_store(key_id, key_type,
          user_id, key, key_len))
      {
        string errmsg= "Storing key (" + string(key_id) +
          ") into destination plugin failed.";
        my_error(ER_KEYRING_MIGRATION_FAILURE, MYF(0),
                 errmsg.c_str());
        error= TRUE;
      }
      else
      {
        /*
         keep track of keys stored in successfully so that they can be
         removed in case of error.
        */
        Key_info ki(key_id, user_id);
        m_source_keys.push_back(ki);
      }
    }
    if (key)
      my_free((char*)key);
    if (key_type)
      my_free(key_type);
  }
  if (error)
  {
    /* something went wrong remove keys from destination plugin. */
    while (m_source_keys.size())
    {
      Key_info ki= m_source_keys.back();
      if (m_destination_plugin_handle->mysql_key_remove(
        ki.m_key_id, ki.m_user_id))
      {
        string errmsg= "Removing key (" + string(ki.m_key_id) +
          ") from destination plugin failed.";
        my_error(ER_KEYRING_MIGRATION_FAILURE, MYF(0),
                 errmsg.c_str());
      }
      m_source_keys.pop_back();
    }
  }
  m_source_plugin_handle->mysql_key_iterator_deinit(key_iterator);
  DBUG_RETURN(error);
}

/**
  Disable variable @@keyring_operations.

  @return 0 Success
  @return 1 Failure
*/
bool Migrate_keyring::disable_keyring_operations()
{
  DBUG_ENTER("Migrate_keyring::disable_keyring_operations");
  const char query[]= "SET GLOBAL KEYRING_OPERATIONS=0";
  if (mysql && mysql_real_query(mysql, query, strlen(query)))
  {
    my_error(ER_KEYRING_MIGRATION_FAILURE, MYF(0),
             "Failed to disable keyring_operations variable.");
    DBUG_RETURN(true);
  }
  DBUG_RETURN(false);
}

/**
  Enable variable @@keyring_operations.

  @return 0 Success
  @return 1 Failure
*/
bool Migrate_keyring::enable_keyring_operations()
{
  DBUG_ENTER("Migrate_keyring::enable_keyring_operations");
  const char query[]= "SET GLOBAL KEYRING_OPERATIONS=1";
  if (mysql && mysql_real_query(mysql, query, strlen(query)))
  {
    my_error(ER_KEYRING_MIGRATION_FAILURE, MYF(0),
             "Failed to enable keyring_operations variable.");
    DBUG_RETURN(true);
  }
  DBUG_RETURN(false);
}

/**
  Standard destructor to close connection handle.
*/
Migrate_keyring::~Migrate_keyring()
{
  if (mysql)
  {
    mysql_close(mysql);
    mysql= NULL;
    if (migrate_connect_options)
      vio_end();
  }
}
