/* Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PFS_FILE_PROVIDER_H
#define PFS_FILE_PROVIDER_H

/**
  @file include/pfs_file_provider.h
  Performance schema instrumentation (declarations).
*/

#ifdef HAVE_PSI_FILE_INTERFACE
#ifdef MYSQL_SERVER
#ifndef EMBEDDED_LIBRARY
#ifndef MYSQL_DYNAMIC_PLUGIN

#include "mysql/psi/psi.h"

#define PSI_FILE_CALL(M) pfs_ ## M ## _v1

C_MODE_START

void pfs_register_file_v1(const char *category,
                          PSI_file_info_v1 *info,
                          int count);

void pfs_create_file_v1(PSI_file_key key, const char *name, File file);

PSI_file_locker*
pfs_get_thread_file_name_locker_v1(PSI_file_locker_state *state,
                                   PSI_file_key key,
                                   PSI_file_operation op,
                                   const char *name, const void *identity);

PSI_file_locker*
pfs_get_thread_file_stream_locker_v1(PSI_file_locker_state *state,
                                     PSI_file *file, PSI_file_operation op);

PSI_file_locker*
pfs_get_thread_file_descriptor_locker_v1(PSI_file_locker_state *state,
                                         File file, PSI_file_operation op);

void pfs_start_file_open_wait_v1(PSI_file_locker *locker,
                                 const char *src_file,
                                 uint src_line);

PSI_file* pfs_end_file_open_wait_v1(PSI_file_locker *locker, void *result);

void pfs_end_file_open_wait_and_bind_to_descriptor_v1
  (PSI_file_locker *locker, File file);

void pfs_end_temp_file_open_wait_and_bind_to_descriptor_v1
  (PSI_file_locker *locker, File file, const char *filename);

void pfs_start_file_wait_v1(PSI_file_locker *locker,
                            size_t count,
                            const char *src_file,
                            uint src_line);

void pfs_end_file_wait_v1(PSI_file_locker *locker,
                          size_t byte_count);

void pfs_start_file_close_wait_v1(PSI_file_locker *locker,
                                  const char *src_file,
                                  uint src_line);

void pfs_end_file_close_wait_v1(PSI_file_locker *locker, int rc);

void pfs_end_file_rename_wait_v1(PSI_file_locker *locker, const char *old_name,
                                 const char *new_name, int rc);

C_MODE_END

#endif /* EMBEDDED_LIBRARY */
#endif /* MYSQL_DYNAMIC_PLUGIN */
#endif /* MYSQL_SERVER */
#endif /* HAVE_PSI_FILE_INTERFACE */

#endif

