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
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02111-1307  USA */

#ifndef COMPONENTS_SERVICES_PSI_TRANSACTION_SERVICE_H
#define COMPONENTS_SERVICES_PSI_TRANSACTION_SERVICE_H

#include <mysql/components/services/psi_transaction_bits.h>
#include <mysql/components/service.h>

BEGIN_SERVICE_DEFINITION(psi_transaction_v1)
  /** @sa get_thread_transaction_locker_v1_t. */
  get_thread_transaction_locker_v1_t get_thread_transaction_locker;
  /** @sa start_transaction_v1_t. */
  start_transaction_v1_t start_transaction;
  /** @sa set_transaction_xid_v1_t. */
  set_transaction_xid_v1_t set_transaction_xid;
  /** @sa set_transaction_xa_state_v1_t. */
  set_transaction_xa_state_v1_t set_transaction_xa_state;
  /** @sa set_transaction_gtid_v1_t. */
  set_transaction_gtid_v1_t set_transaction_gtid;
  /** @sa set_transaction_trxid_v1_t. */
  set_transaction_trxid_v1_t set_transaction_trxid;
  /** @sa inc_transaction_savepoints_v1_t. */
  inc_transaction_savepoints_v1_t inc_transaction_savepoints;
  /** @sa inc_transaction_rollback_to_savepoint_v1_t. */
  inc_transaction_rollback_to_savepoint_v1_t
    inc_transaction_rollback_to_savepoint;
  /** @sa inc_transaction_release_savepoint_v1_t. */
  inc_transaction_release_savepoint_v1_t inc_transaction_release_savepoint;
  /** @sa end_transaction_v1_t. */
  end_transaction_v1_t end_transaction;
END_SERVICE_DEFINITION(psi_transaction_v1)

#define REQUIRES_PSI_TRANSACTION_SERVICE REQUIRES_SERVICE(psi_transaction_v1)

#endif /* COMPONENTS_SERVICES_PSI_TRANSACTION_SERVICE_H */

