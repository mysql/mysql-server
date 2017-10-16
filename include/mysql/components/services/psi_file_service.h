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
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef COMPONENTS_SERVICES_PSI_FILE_SERVICE_H
#define COMPONENTS_SERVICES_PSI_FILE_SERVICE_H

#include <mysql/components/services/psi_file_bits.h>
#include <mysql/components/service.h>

BEGIN_SERVICE_DEFINITION(psi_file_v1)
  /** @sa register_file_v1_t. */
  register_file_v1_t register_file;
  /** @sa create_file_v1_t. */
  create_file_v1_t create_file;
  /** @sa get_thread_file_name_locker_v1_t. */
  get_thread_file_name_locker_v1_t get_thread_file_name_locker;
  /** @sa get_thread_file_stream_locker_v1_t. */
  get_thread_file_stream_locker_v1_t get_thread_file_stream_locker;
  /** @sa get_thread_file_descriptor_locker_v1_t. */
  get_thread_file_descriptor_locker_v1_t get_thread_file_descriptor_locker;
  /** @sa start_file_open_wait_v1_t. */
  start_file_open_wait_v1_t start_file_open_wait;
  /** @sa end_file_open_wait_v1_t. */
  end_file_open_wait_v1_t end_file_open_wait;
  /** @sa end_file_open_wait_and_bind_to_descriptor_v1_t. */
  end_file_open_wait_and_bind_to_descriptor_v1_t
    end_file_open_wait_and_bind_to_descriptor;
  /** @sa end_temp_file_open_wait_and_bind_to_descriptor_v1_t. */
  end_temp_file_open_wait_and_bind_to_descriptor_v1_t
    end_temp_file_open_wait_and_bind_to_descriptor;
  /** @sa start_file_wait_v1_t. */
  start_file_wait_v1_t start_file_wait;
  /** @sa end_file_wait_v1_t. */
  end_file_wait_v1_t end_file_wait;
  /** @sa start_file_close_wait_v1_t. */
  start_file_close_wait_v1_t start_file_close_wait;
  /** @sa end_file_close_wait_v1_t. */
  end_file_close_wait_v1_t end_file_close_wait;
  /** @sa rename_file_close_wait_v1_t. */
  end_file_rename_wait_v1_t end_file_rename_wait;
END_SERVICE_DEFINITION(psi_file_v1)

#define REQUIRES_PSI_FILE_SERVICE REQUIRES_SERVICE(psi_file_v1)

#endif /* COMPONENTS_SERVICES_PSI_FILE_SERVICE_H */

