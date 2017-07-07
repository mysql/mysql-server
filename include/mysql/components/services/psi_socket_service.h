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

#ifndef COMPONENTS_SERVICES_PSI_SOCKET_SERVICE_H
#define COMPONENTS_SERVICES_PSI_SOCKET_SERVICE_H

#include <mysql/components/services/psi_socket_bits.h>
#include <mysql/components/service.h>

BEGIN_SERVICE_DEFINITION(psi_socket_v1)
  /** @sa register_socket_v1_t. */
  register_socket_v1_t register_socket;
  /** @sa init_socket_v1_t. */
  init_socket_v1_t init_socket;
  /** @sa destroy_socket_v1_t. */
  destroy_socket_v1_t destroy_socket;
  /** @sa start_socket_wait_v1_t. */
  start_socket_wait_v1_t start_socket_wait;
  /** @sa end_socket_wait_v1_t. */
  end_socket_wait_v1_t end_socket_wait;
  /** @sa set_socket_state_v1_t. */
  set_socket_state_v1_t set_socket_state;
  /** @sa set_socket_info_v1_t. */
  set_socket_info_v1_t set_socket_info;
  /** @sa set_socket_thread_owner_v1_t. */
  set_socket_thread_owner_v1_t set_socket_thread_owner;
END_SERVICE_DEFINITION(psi_socket_v1)

#define REQUIRES_PSI_SOCKET_SERVICE REQUIRES_SERVICE(psi_socket_v1)

#endif /* COMPONENTS_SERVICES_PSI_SOCKET_SERVICE_H */

