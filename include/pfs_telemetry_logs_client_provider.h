/* Copyright (c) 2024 Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PFS_TELEMETRY_LOGS_CLIENT_PROVIDER_H
#define PFS_TELEMETRY_LOGS_CLIENT_PROVIDER_H

/**
  @file include/pfs_telemetry_logs_client_provider.h
  Performance schema instrumentation (declarations).
*/

#include <mysql/components/services/bits/server_telemetry_logs_bits.h>
#include <sys/types.h>

/* HAVE_PSI_*_INTERFACE */
#include "my_psi_config.h"  // IWYU pragma: keep

#ifdef HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE
#if defined(MYSQL_SERVER) || defined(PFS_DIRECT_CALL)
#ifndef MYSQL_DYNAMIC_PLUGIN

#include "my_inttypes.h"
#include "my_macros.h"

#define PSI_LOGS_CLIENT_CALL(M) pfs_##M##_v1

void pfs_register_logger_client_v1(PSI_logger_info_v1 *info, size_t count,
                                   const char *category);
void pfs_unregister_logger_client_v1(PSI_logger_info_v1 *info, size_t count);
PSI_logger *pfs_check_enabled_v1(PSI_logger_key key, OTELLogLevel level);
void pfs_log_emit_v1(PSI_logger *logger, OTELLogLevel level,
                     const char *message, time_t timestamp,
                     const log_attribute_t *attr_array, size_t attr_count);

#endif /* MYSQL_DYNAMIC_PLUGIN */
#endif /* MYSQL_SERVER || PFS_DIRECT_CALL */
#endif /* HAVE_PSI_SERVER_TELEMETRY_LOGS_INTERFACE */

#endif
