/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef PFS_TLS_CHANNEL_PROVIDER_H
#define PFS_TLS_CHANNEL_PROVIDER_H

/* HAVE_PSI_*_INTERFACE */
#include "my_psi_config.h"  // IWYU pragma: keep

/**
  @file include/pfs_tls_channel_provider.h
  Performance schema instrumentation (declarations).
*/

#ifdef HAVE_PSI_TLS_CHANNEL_INTERFACE
#if defined(MYSQL_SERVER) || defined(PFS_DIRECT_CALL)
#ifndef MYSQL_DYNAMIC_PLUGIN
#ifndef WITH_LOCK_ORDER

#include <stddef.h>

#include "my_macros.h"
#include "mysql/psi/mysql_tls_channel.h"

#define PSI_TLS_CHANNEL_CALL(M) pfs_##M##_v1

void pfs_register_tls_channel_v1(TLS_channel_property_iterator *provider);

void pfs_unregister_tls_channel_v1(TLS_channel_property_iterator *provider);

#endif /* WITH_LOCK_ORDER */
#endif /* MYSQL_DYNAMIC_PLUGIN */
#endif /* MYSQL_SERVER || PFS_DIRECT_CALL */
#endif /* HAVE_PSI_TLS_CHANNEL_INTERFACE */

#endif
