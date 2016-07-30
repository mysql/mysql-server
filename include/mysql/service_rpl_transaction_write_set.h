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

#ifndef MYSQL_SERVICE_TRANSACTION_WRITE_SET_INCLUDED

/**
  @file include/mysql/service_rpl_transaction_write_set.h
  This service provides a function for plugins to get the write set of a given
  transaction.

  SYNOPSIS
  get_transaction_write_set()
    This service is used to fetch the write_set extracted for the currently
    executing transaction by passing the thread_id as an input parameter for
    the method.

    @param [in] - thread_id - It is the thread identifier of the currently
                              executing thread.

    In the current implementation it is being called during RUN_HOOK macro,
    on which we know that thread is on plugin context.

  Cleanup :
    The service caller must take of the memory allocated during the service
    call to prevent memory leaks.
*/

#ifndef MYSQL_ABI_CHECK
#include <stdlib.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
  This structure is used to keep the list of the hash values of the records
  changed in the transaction.
*/
struct st_trans_write_set
{
  unsigned int m_flags; // reserved
  unsigned long write_set_size; // Size of the PKE set of the transaction.
  unsigned long long* write_set;  // A pointer to the PKE set.
};
typedef struct st_trans_write_set Transaction_write_set;

extern struct transaction_write_set_service_st {
  Transaction_write_set* (*get_transaction_write_set)(unsigned long m_thread_id);
} *transaction_write_set_service;

#ifdef MYSQL_DYNAMIC_PLUGIN

#define get_transaction_write_set(m_thread_id) \
  (transaction_write_set_service->get_transaction_write_set((m_thread_id)))

#else

Transaction_write_set* get_transaction_write_set(unsigned long m_thread_id);

#endif

#ifdef __cplusplus
}
#endif

#define MYSQL_SERVICE_TRANSACTION_WRITE_SET_INCLUDED
#endif
