/* Copyright (c) 2014, 2015 Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"
#include <string>

class THD;
class MDL_ticket;

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Collation;
class Object_table;

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
    const std::string &schema_name,
    const std::string &table_name) const = 0;

public:
  /////////////////////////////////////////////////////////////////////////
  // Auxiliary operations.
  /////////////////////////////////////////////////////////////////////////

  /**
    Check if the given schema name is 'mysql', which where
    the DD tables are stored.

    @returns true - If schema_name is 'mysql'
    @returns false - If schema_name is not 'mysql'
  */
  virtual bool is_dd_schema_name(const std::string &schema_name) const = 0;

  /**
    Check if given table name is a dictionary table name.

    @returns true -  If given table name is a dictionary table.
    @returns false - If table name is not a dictionary table.
  */
  virtual bool is_dd_table_name(const std::string &schema_name,
                                const std::string &table_name) const = 0;

  /**
    Check if given table name is a system view name.

    @returns true -  If given table name is a system view.
    @returns false - If table name is not a system view.
  */
  virtual bool is_system_view_name(const std::string &schema_name,
                                   const std::string &table_name) const = 0;

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
