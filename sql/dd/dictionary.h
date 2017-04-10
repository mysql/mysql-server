/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__DICTIONARY_INCLUDED
#define DD__DICTIONARY_INCLUDED


#include "dd/string_type.h"                    // dd::String_type

class THD;
class MDL_ticket;

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Collation;
class Object_table;
class Schema;

namespace cache {
  class Dictionary_client;
}

///////////////////////////////////////////////////////////////////////////

/// Main interface class enabling users to operate on data dictionary.
class Dictionary
{
public:
  /**
    Get dictionary object for a given dictionary table name.
    If the given schema_name and table_name is not a dictionary
    table name, then the function returns NULL.

    @returns Pointer to dictionary object for the given
             dictionary table name, else NULL.
  */
  virtual const Object_table *get_dd_table(
    const String_type &schema_name,
    const String_type &table_name) const = 0;

  /**
    Store metadata of plugin's information schema tables into
    DD tables.

    @return false - On success
    @return true - On error
  */
  virtual bool install_plugin_IS_table_metadata() = 0;

public:
  /////////////////////////////////////////////////////////////////////////
  // Auxiliary operations.
  /////////////////////////////////////////////////////////////////////////

  /**
    Check if the given schema name is 'mysql', which where
    the DD tables are stored.

    @param schema_name    Schema name to check.

    @returns true - If schema_name is 'mysql'
    @returns false - If schema_name is not 'mysql'
  */
  virtual bool is_dd_schema_name(const String_type &schema_name) const = 0;

  /**
    Check if given table name is a dictionary table name.

    @param schema_name    Schema name to check.
    @param table_name     Table name to check.

    @returns true -  If given table name is a dictionary table.
    @returns false - If table name is not a dictionary table.
  */
  virtual bool is_dd_table_name(const String_type &schema_name,
                                const String_type &table_name) const = 0;

  /**
    Get the error code representing the type name string for a dictionary
    or system table.

    Necessary to support localization of error messages.

    @param schema_name    Schema name to check.
    @param table_name     Table name to check.

    @returns The error code representing the type name associated with the table,
             for being used in error messages.
  */
  virtual int table_type_error_code(const String_type &schema_name,
                                    const String_type &table_name) const = 0;

  /**
    Check if given table name can be accessed by the given thread type.

    @param is_dd_internal_thread    'true' if this is a DD internal
                                    thread.
    @param is_ddl_statement         'true' if this is a DDL statement.
    @param schema_name              Schema name to check.
    @param schema_length            Length of schema name to check.
    @param table_name               Table name to check.

    @returns true -  If given table name is accessible by the thread type.
    @returns false - If table name is not accessible.
  */
  virtual bool is_dd_table_access_allowed(bool is_dd_internal_thread,
                                          bool is_ddl_statement,
                                          const char *schema_name,
                                          size_t schema_length,
                                          const char *table_name) const= 0;

  /**
    Check if given table name is a system view name.

    @returns true -  If given table name is a system view.
    @returns false - If table name is not a system view.
  */
  virtual bool is_system_view_name(const char *schema_name,
                                   const char *table_name) const = 0;

public:
  // Destructor to cleanup data dictionary instance upon server shutdown.
  virtual ~Dictionary()
  { }
};

///////////////////////////////////////////////////////////////////////////

//
// MDL wrapper functions
//

/**
  @brief
    Acquire shared metadata lock on the given table name with
    explicit duration.

  @param thd - THD to which lock belongs to.
  @param schema_name    - Schema name
  @param table_name     - Table name
  @param no_wait        - Use try_acquire_lock() if no_wait is true.
                          else use acquire_lock() with
                          thd->variables.lock_wait_timeout timeout value.
  @param out_mdl_ticket - This is a OUT parameter, a pointer to MDL_ticket
                          upon successful lock attempt.

*/
bool acquire_shared_table_mdl(THD *thd,
                              const char *schema_name,
                              const char *table_name,
                              bool no_wait,
                              MDL_ticket **out_mdl_ticket);


/**
  Predicate to check if we have a shared meta data lock on the
  submitted schema qualified table name.

  @param    thd            Thread context.
  @param    schema_name    Schema name.
  @param    table_name     Table name.

  @retval   true           The thread context has a lock.
  @retval   false          The thread context does not have a lock.
*/

bool has_shared_table_mdl(THD *thd,
                          const char *schema_name,
                          const char *table_name);


/**
  Predicate to check if we have an exclusive meta data lock on the
  submitted schema qualified table name.

  @param    thd            Thread context.
  @param    schema_name    Schema name.
  @param    table_name     Table name.

  @retval   true           The thread context has a lock.
  @retval   false          The thread context does not have a lock.
*/

bool has_exclusive_table_mdl(THD *thd,
                             const char *schema_name,
                             const char *table_name);


/**
  Acquire an exclusive metadata lock on the given tablespace name with
  transaction duration.

  @param       thd           THD to which lock belongs.
  @param       tablespace_name  Tablespace name
  @param       no_wait        Use try_acquire_lock() if no_wait is true,
                              else use acquire_lock() with
                              thd->variables.lock_wait_timeout timeout value.

  @retval      true           Failure, e.g. a lock wait timeout.
  @retval      false          Successful lock acquisition.
*/

bool acquire_exclusive_tablespace_mdl(THD *thd,
                                      const char *tablespace_name,
                                      bool no_wait);


/**
  Acquire a shared metadata lock on the given tablespace name with
  transaction duration.

  @param       thd           THD to which lock belongs.
  @param       tablespace_name  Tablespace name
  @param       no_wait        Use try_acquire_lock() if no_wait is true,
                              else use acquire_lock() with
                              thd->variables.lock_wait_timeout timeout value.

  @retval      true           Failure, e.g. a lock wait timeout.
  @retval      false          Successful lock acquisition.
*/
bool acquire_shared_tablespace_mdl(THD *thd,
                                   const char *tablespace_name,
                                   bool no_wait);


/**
  Predicate to check if we have a shared meta data lock on the
  submitted tablespace name.

  @param    thd              Thread context.
  @param    tablespace_name  Tablespace name.

  @retval   true             The thread context has a lock.
  @retval   false            The thread context does not have a lock.
*/

bool has_shared_tablespace_mdl(THD *thd,
                               const char *tablespace_name);


/**
  Predicate to check if we have an exclusive meta data lock on the
  submitted tablespace name.

  @param    thd              Thread context.
  @param    tablespace_name  Tablespace name.

  @retval   true             The thread context has a lock.
  @retval   false            The thread context does not have a lock.
*/

bool has_exclusive_tablespace_mdl(THD *thd,
                                  const char *tablespace_name);


/**
  Acquire exclusive metadata lock on the given table name with
  explicit duration.

  @param[in]  thd              THD to which lock belongs to.
  @param[in]  schema_name      Schema name
  @param[in]  table_name       Table name
  @param[in]  no_wait          Use try_acquire_lock() if no_wait is true.
                               else use acquire_lock() with
                               thd->variables.lock_wait_timeout timeout value.
  @param[out] out_mdl_ticket   A pointer to MDL_ticket upon successful lock
                               attempt.
*/

bool acquire_exclusive_table_mdl(THD *thd,
                                 const char *schema_name,
                                 const char *table_name,
                                 bool no_wait,
                                 MDL_ticket **out_mdl_ticket);


/**
  Acquire exclusive metadata lock on the given schema name with
  explicit duration.

  @param[in]  thd              THD to which lock belongs to.
  @param[in]  schema_name      Schema name
  @param[in]  no_wait          Use try_acquire_lock() if no_wait is true.
                               else use acquire_lock() with
                               thd->variables.lock_wait_timeout timeout value.
  @param[out] out_mdl_ticket   A pointer to MDL_ticket upon successful lock
                               attempt.
*/

bool acquire_exclusive_schema_mdl(THD *thd,
                                 const char *schema_name,
                                 bool no_wait,
                                 MDL_ticket **out_mdl_ticket);

/**
  @brief
    Release MDL_EXPLICIT lock held by a ticket

  @param thd - THD to which lock belongs to.
  @param mdl_ticket - Lock ticket.

*/
void release_mdl(THD *thd, MDL_ticket *mdl_ticket);

/** Get Dictionary_client from THD object (the latter is opaque * in SEs). */
cache::Dictionary_client *get_dd_client(THD *thd);
}

#endif // DD__DICTIONARY_INCLUDED
