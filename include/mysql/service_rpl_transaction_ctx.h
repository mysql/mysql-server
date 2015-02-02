/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_SERVICE_RPL_TRANSACTION_CTX_INCLUDED

/**
  @file include/mysql/service_rpl_transaction_ctx.h
  This service provides a function for plugins to report if a transaction of a
  given THD should continue or be aborted.

  SYNOPSIS
  set_transaction_ctx()
    should be called during RUN_HOOK macro, on which we know that thread is
    on plugin context and it is before
    Rpl_transaction_ctx::is_transaction_rollback() check.
*/

#ifndef MYSQL_ABI_CHECK
#include <stdlib.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct st_transaction_termination_ctx
{
  unsigned long m_thread_id;
  unsigned int m_flags; // reserved

  /*
    If the instruction is to rollback the transaction,
    then this flag is set to false.
    Note: type is char like on my_bool.
   */
  char m_rollback_transaction;

  /*
    If the plugin has generated a GTID, then the follwoing
    fields MUST be set.
    Note: type is char like on my_bool.
   */
  char m_generated_gtid;
  int m_sidno;
  long long int m_gno;
};
typedef struct st_transaction_termination_ctx Transaction_termination_ctx;

extern struct rpl_transaction_ctx_service_st {
  int (*set_transaction_ctx)(Transaction_termination_ctx transaction_termination_ctx);
} *rpl_transaction_ctx_service;

#ifdef MYSQL_DYNAMIC_PLUGIN

#define set_transaction_ctx(transaction_termination_ctx) \
  (rpl_transaction_ctx_service->set_transaction_ctx((transaction_termination_ctx)))

#else

int set_transaction_ctx(Transaction_termination_ctx transaction_termination_ctx);

#endif

#ifdef __cplusplus
}
#endif

#define MYSQL_SERVICE_RPL_TRANSACTION_CTX_INCLUDED
#endif
