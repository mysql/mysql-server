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

#include "sql_backup_lock.h"

#include "mysqld_error.h"     // ER_SPECIFIC_ACCESS_DENIED_ERROR
#include "sql_class.h"        // THD
#include "sql_security_ctx.h" // Security_context

/**
  Check if a current user has the privilege BACKUP_ADMIN required to run
  the statement LOCK INSTANCE FOR BACKUP.

  @param thd    Current thread

  @retval false  A user has the privilege BACKUP_ADMIN
  @retval true   A user doesn't have the privilege BACKUP_ADMIN
*/

static bool check_backup_admin_privilege(THD *thd)
{
  Security_context *sctx= thd->security_context();

  if (!sctx->has_global_grant(STRING_WITH_LEN("BACKUP_ADMIN")).first)
  {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "BACKUP_ADMIN");
    return true;
  }

  return false;
}


bool Sql_cmd_lock_instance::execute(THD *thd)
{
  if (check_backup_admin_privilege(thd) ||
      acquire_exclusive_backup_lock(thd, thd->variables.lock_wait_timeout))
    return true;

  my_ok(thd);
  return false;
}


bool Sql_cmd_unlock_instance::execute(THD *thd)
{
  if (check_backup_admin_privilege(thd))
    return true;

  release_backup_lock(thd);

  my_ok(thd);
  return false;
}


/**
  Acquire either exclusive or shared Backup Lock.

  @param[in] thd                    Current thread context
  @param[in] mdl_type               Type of metadata lock to acquire for backup
  @param[in] mdl_duration           Duration of metadata lock
  @param[in] lock_wait_timeout      How many seconds to wait before timeout.

  @return Operation status.
    @retval false Success
    @retval true  Failure
*/

static bool acquire_mdl_for_backup(THD *thd,
                                   enum_mdl_type mdl_type,
                                   enum_mdl_duration mdl_duration,
                                   ulong lock_wait_timeout)
{
  MDL_request mdl_request;

  DBUG_ASSERT(mdl_type == MDL_SHARED ||
              mdl_type == MDL_INTENTION_EXCLUSIVE);

  MDL_REQUEST_INIT(&mdl_request,
                   MDL_key::BACKUP_LOCK, "", "", mdl_type,
                   mdl_duration);

  return thd->mdl_context.acquire_lock(&mdl_request, lock_wait_timeout);
}


/**
  MDL_release_locks_visitor subclass to release MDL for BACKUP_LOCK.
*/

class Release_all_backup_locks : public MDL_release_locks_visitor
{
public:
  virtual bool release(MDL_ticket *ticket)
  {
    return ticket->get_key()->mdl_namespace() == MDL_key::BACKUP_LOCK;
  }
};


void release_backup_lock(THD *thd)
{
  Release_all_backup_locks lock_visitor;
  thd->mdl_context.release_locks(&lock_visitor);
}


/*
  The following rationale is for justification of choice for specific lock
  types in the functions acquire_exclusive_backup_lock,
  acquire_shared_backup_lock.

  IX and S locks are mutually incompatible. On the other hand both these
  lock types are compatible with themselves. IX lock has lower priority
  than S locks. So we can use S lock for rare Backup operation which should
  not be starved by more frequent DDL operations using IX locks.

  From all listed above follows that S lock should be considered as Exclusive
  Backup Lock and IX lock should be considered as Shared Backup Lock.
*/

bool acquire_exclusive_backup_lock(THD *thd, ulong lock_wait_timeout)
{
  return acquire_mdl_for_backup(thd, MDL_SHARED, MDL_EXPLICIT,
                                lock_wait_timeout);
}


bool acquire_shared_backup_lock(THD *thd, ulong lock_wait_timeout)
{
  return acquire_mdl_for_backup(thd, MDL_INTENTION_EXCLUSIVE, MDL_TRANSACTION,
                                lock_wait_timeout);
}
