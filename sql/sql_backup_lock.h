/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SQL_LOCK_INCLUDED
#define SQL_LOCK_INCLUDED

#include <sys/types.h>

#include "my_sqlcommand.h"  // SQLCOM_LOCK_INSTANCE, SQLCOM_UNLOCK_INSTANCE
#include "sql_cmd.h"        // Sql_cmd

class THD;


/**
  Sql_cmd_lock_instance represents statement LOCK INSTANCE FOR BACKUP.
*/

class Sql_cmd_lock_instance : public Sql_cmd
{
public:
  /**
    Execute LOCK INSTANCE statement once.

    @param thd Thread handler

    @returns false on success, true on error
  */

  virtual bool execute(THD *thd);


  virtual enum_sql_command sql_command_code() const
  {
    return SQLCOM_LOCK_INSTANCE;
  }
};


/**
  Sql_cmd_unlock_instance represents statement UNLOCK INSTANCE.
*/

class Sql_cmd_unlock_instance : public Sql_cmd
{
public:
  /**
    Execute UNLOCK INSTANCE statement once.

    @param thd Thread handler

    @returns false on success, true on error
  */

  virtual bool execute(THD *thd);


  virtual enum_sql_command sql_command_code() const
  {
    return SQLCOM_UNLOCK_INSTANCE;
  }
};


/**
  Acquire exclusive Backup Lock.

  @param[in] thd                Current thread context
  @param[in] lock_wait_timeout  How many seconds to wait before timeout.

  @return Operation status.
    @retval false Success
    @retval true  Failure
*/

bool acquire_exclusive_backup_lock(THD *thd, unsigned long lock_wait_timeout);


/**
  Acquire shared Backup Lock.

  @param[in] thd                Current thread context
  @param[in] lock_wait_timeout  How many seconds to wait before timeout.

  @return Operation status.
    @retval false Success
    @retval true  Failure
*/

bool acquire_shared_backup_lock(THD *thd, unsigned long lock_wait_timeout);


/**
  Release Backup Lock if it was acquired.

  @param[in] thd         Current thread context
*/

void release_backup_lock(THD *thd);

#endif /* SQL_LOCK_INCLUDED */
