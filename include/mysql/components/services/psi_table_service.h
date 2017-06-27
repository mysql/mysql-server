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

#ifndef COMPONENTS_SERVICES_PSI_TABLE_SERVICE_H
#define COMPONENTS_SERVICES_PSI_TABLE_SERVICE_H

#include <mysql/components/services/psi_table_bits.h>
#include <mysql/components/service.h>

BEGIN_SERVICE_DEFINITION(psi_table_v1)
  /** @sa get_table_share_v1_t. */
  get_table_share_v1_t get_table_share;
  /** @sa release_table_share_v1_t. */
  release_table_share_v1_t release_table_share;
  /** @sa drop_table_share_v1_t. */
  drop_table_share_v1_t drop_table_share;
  /** @sa open_table_v1_t. */
  open_table_v1_t open_table;
  /** @sa unbind_table_v1_t. */
  unbind_table_v1_t unbind_table;
  /** @sa rebind_table_v1_t. */
  rebind_table_v1_t rebind_table;
  /** @sa close_table_v1_t. */
  close_table_v1_t close_table;
  /** @sa start_table_io_wait_v1_t. */
  start_table_io_wait_v1_t start_table_io_wait;
  /** @sa end_table_io_wait_v1_t. */
  end_table_io_wait_v1_t end_table_io_wait;
  /** @sa start_table_lock_wait_v1_t. */
  start_table_lock_wait_v1_t start_table_lock_wait;
  /** @sa end_table_lock_wait_v1_t. */
  end_table_lock_wait_v1_t end_table_lock_wait;
  /** @sa end_table_lock_wait_v1_t. */
  unlock_table_v1_t unlock_table;
END_SERVICE_DEFINITION(psi_table_v1)

#define REQUIRES_PSI_TABLE_SERVICE REQUIRES_SERVICE(psi_table_v1)

#endif /* COMPONENTS_SERVICES_PSI_TABLE_SERVICE_H */

