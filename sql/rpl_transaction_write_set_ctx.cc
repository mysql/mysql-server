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

#include "rpl_transaction_write_set_ctx.h"

#include "mysql/service_rpl_transaction_write_set.h" // Transaction_write_set
#include "mysqld_thd_manager.h"                      // Global_THD_manager
#include "sql_class.h"                               // THD
#include "sql_parse.h"                               // Find_thd_with_id


Rpl_transaction_write_set_ctx::Rpl_transaction_write_set_ctx()
{
  DBUG_ENTER("Rpl_transaction_write_set_ctx::Rpl_transaction_write_set_ctx");
  DBUG_VOID_RETURN;
}

void Rpl_transaction_write_set_ctx::add_write_set(uint64 hash)
{
  DBUG_ENTER("Transaction_context_log_event::add_write_set");
  write_set.push_back(hash);
  DBUG_VOID_RETURN;
}

std::vector<uint64>* Rpl_transaction_write_set_ctx::get_write_set()
{
  DBUG_ENTER("Transaction_context_log_event::add_write_set");
  DBUG_RETURN(&write_set);
}

void Rpl_transaction_write_set_ctx::clear_write_set()
{
  DBUG_ENTER("Transaction_context_log_event::clear_write_set");
  write_set.clear();
  DBUG_VOID_RETURN;
}

/**
  Implementation of service_rpl_transaction_write_set, see
  @file include/mysql/service_rpl_transaction_write_set.h
*/

Transaction_write_set* get_transaction_write_set(unsigned long m_thread_id)
{
  DBUG_ENTER("get_transaction_write_set");
  THD *thd= NULL;
  Transaction_write_set *result_set= NULL;
  Find_thd_with_id find_thd_with_id(m_thread_id);

  thd= Global_THD_manager::get_instance()->find_thd(&find_thd_with_id);
  if (thd)
  {
    Rpl_transaction_write_set_ctx *transaction_write_set_ctx=
      thd->get_transaction()->get_transaction_write_set_ctx();
    int write_set_size= transaction_write_set_ctx->get_write_set()->size();
    if (write_set_size == 0)
    {
      mysql_mutex_unlock(&thd->LOCK_thd_data);
      DBUG_RETURN(NULL);
    }

    result_set= (Transaction_write_set*)my_malloc(key_memory_write_set_extraction,
                                                  sizeof(Transaction_write_set),
                                                  MYF(0));
    result_set->write_set_size= write_set_size;
    result_set->write_set=
        (unsigned long long*)my_malloc(key_memory_write_set_extraction,
                                       write_set_size *
                                       sizeof(unsigned long long),
                                       MYF(0));
    int result_set_index= 0;
    for (std::vector<uint64>::iterator it= transaction_write_set_ctx->get_write_set()->begin();
         it!=transaction_write_set_ctx->get_write_set()->end();
         ++it)
    {
      uint64 temp= *it;
      result_set->write_set[result_set_index++]=temp;
    }
    mysql_mutex_unlock(&thd->LOCK_thd_data);
  }
  DBUG_RETURN(result_set);
}
