/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "rpl_transaction_ctx.h"

#include "mysqld_thd_manager.h" // Global_THD_manager
#include "rpl_gtid.h"           // rpl_sidno
#include "sql_class.h"          // THD
#include "sql_parse.h"          // Find_thd_with_id


Rpl_transaction_ctx::Rpl_transaction_ctx()
{
  DBUG_ENTER("Rpl_transaction_ctx::Rpl_transaction_ctx");
  cleanup();
  DBUG_VOID_RETURN;
}

void Rpl_transaction_ctx::cleanup()
{
  DBUG_ENTER("Rpl_transaction_ctx::cleanup");
  m_transaction_ctx.m_thread_id= 0;
  m_transaction_ctx.m_flags= 0;
  m_transaction_ctx.m_rollback_transaction= FALSE;
  m_transaction_ctx.m_generated_gtid= FALSE;
  m_transaction_ctx.m_sidno= 0;
  m_transaction_ctx.m_gno= 0;
  DBUG_VOID_RETURN;
}

int Rpl_transaction_ctx::set_rpl_transaction_ctx(Transaction_termination_ctx transaction_termination_ctx)
{
  DBUG_ENTER("Rpl_transaction_ctx::set_verdict");

  if (transaction_termination_ctx.m_generated_gtid)
  {
    if (transaction_termination_ctx.m_rollback_transaction ||
        transaction_termination_ctx.m_sidno <= 0 ||
        transaction_termination_ctx.m_gno <= 0)
      DBUG_RETURN(1);
  }

  m_transaction_ctx= transaction_termination_ctx;
  DBUG_RETURN(0);
}

bool Rpl_transaction_ctx::is_transaction_rollback()
{
  DBUG_ENTER("Rpl_transaction_ctx::is_transaction_rollback");
  DBUG_RETURN(m_transaction_ctx.m_rollback_transaction);
}

bool Rpl_transaction_ctx::is_generated_gtid()
{
  DBUG_ENTER("Rpl_transaction_ctx::is_generated_gtid");
  DBUG_RETURN(m_transaction_ctx.m_generated_gtid);
}

rpl_sidno Rpl_transaction_ctx::get_sidno()
{
  DBUG_ENTER("Rpl_transaction_ctx::get_sidno");
  DBUG_RETURN(m_transaction_ctx.m_sidno);
}

rpl_gno Rpl_transaction_ctx::get_gno()
{
  DBUG_ENTER("Rpl_transaction_ctx::get_gno");
  DBUG_RETURN(m_transaction_ctx.m_gno);
}

/**
   Implementation of service_transaction_veredict, see
   @file include/mysql/service_rpl_transaction_ctx.h
*/
int set_transaction_ctx(Transaction_termination_ctx transaction_termination_ctx)
{
  DBUG_ENTER("set_transaction_ctx");
  DBUG_PRINT("enter", ("thread_id=%lu, rollback_transaction=%d, "
                       "generated_gtid=%d, sidno=%d, gno=%lld",
                       transaction_termination_ctx.m_thread_id,
                       transaction_termination_ctx.m_rollback_transaction,
                       transaction_termination_ctx.m_generated_gtid,
                       transaction_termination_ctx.m_sidno,
                       transaction_termination_ctx.m_gno));

  THD *thd= NULL;
  uint error=ER_NO_SUCH_THREAD;
  Find_thd_with_id find_thd_with_id(transaction_termination_ctx.m_thread_id);

  thd= Global_THD_manager::get_instance()->find_thd(&find_thd_with_id);
  if (thd)
  {
    error= thd->get_transaction()->get_rpl_transaction_ctx()->set_rpl_transaction_ctx(transaction_termination_ctx);
    mysql_mutex_unlock(&thd->LOCK_thd_data);
  }
  DBUG_RETURN(error);
}
