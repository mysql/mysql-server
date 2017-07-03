/* Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef MYSQL_PSI_FILE_H
#define MYSQL_PSI_FILE_H

/**
  @file include/mysql/psi/psi_file.h
  Performance schema instrumentation interface.

  @defgroup psi_abi_file File Instrumentation (ABI)
  @ingroup psi_abi
  @{
*/

#include "my_inttypes.h"
#include "my_io.h"
#include "my_macros.h"
#include "my_psi_config.h"  // IWYU pragma: keep
#include "my_sharedlib.h"
#include "mysql/components/services/psi_file_bits.h"

C_MODE_START

/**
  @def PSI_FILE_VERSION_1
  Performance Schema File Interface number for version 1.
  This version is supported.
*/
#define PSI_FILE_VERSION_1 1

/**
  @def PSI_CURRENT_FILE_VERSION
  Performance Schema File Interface number for the most recent version.
  The most current version is @c PSI_FILE_VERSION_1
*/
#define PSI_CURRENT_FILE_VERSION 1

/** Entry point for the performance schema interface. */
struct PSI_file_bootstrap
{
  /**
    ABI interface finder.
    Calling this method with an interface version number returns either
    an instance of the ABI for this version, or NULL.
    @sa PSI_FILE_VERSION_1
    @sa PSI_FILE_VERSION_2
    @sa PSI_CURRENT_FILE_VERSION
  */
  void *(*get_interface)(int version);
};
typedef struct PSI_file_bootstrap PSI_file_bootstrap;

#ifdef HAVE_PSI_FILE_INTERFACE

/**
  Performance Schema file Interface, version 1.
  @since PSI_FILE_VERSION_1
*/
struct PSI_file_service_v1
{
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
};

typedef struct PSI_file_service_v1 PSI_file_service_t;

extern MYSQL_PLUGIN_IMPORT PSI_file_service_t *psi_file_service;

#endif /* HAVE_PSI_FILE_INTERFACE */

/** @} (end of group psi_abi_file) */

C_MODE_END

#endif /* MYSQL_PSI_FILE_H */
