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

#ifndef MYSQL_TLS_CHANNEL_H
#define MYSQL_TLS_CHANNEL_H

/**
  @file include/mysql/psi/mysql_tls_channel.h
  Instrumentation helpers for TLS channel info.
*/

/* HAVE_PSI_*_INTERFACE */
#include "my_psi_config.h"  // IWYU pragma: keep

#include "mysql/psi/psi_tls_channel.h"

#if defined(MYSQL_SERVER) || defined(PFS_DIRECT_CALL)
/* PSI_TLS_CHANNEL_CALL() as direct call. */
#include "pfs_tls_channel_provider.h"  // IWYU pragma: keep
#endif

#ifndef PSI_TLS_CHANNEL_CALL
#define PSI_TLS_CHANNEL_CALL(M) psi_tls_channel_service->M
#endif  // !PSI_TLS_CHANNEL_CALL

/**
  @defgroup psi_api_tls_channel TLS Channel Instrumentation (API)
  @ingroup psi_api
  @{
*/

#define mysql_tls_channel_register(I) inline_mysql_tls_channel_register(I)

static void inline_mysql_tls_channel_register(
#ifdef HAVE_PSI_TLS_CHANNEL_INTERFACE
    TLS_channel_property_iterator *i
#else
    TLS_channel_property_iterator *i [[maybe_unused]]
#endif /* HAVE_PSI_TLS_CHANNEL_INTERFACE */
) {
#ifdef HAVE_PSI_TLS_CHANNEL_INTERFACE
  PSI_TLS_CHANNEL_CALL(register_tls_channel)(i);
#endif  // HAVE_PSI_TLS_CHANNEL_INTERFACE
}

#define mysql_tls_channel_unregister(I) inline_mysql_tls_channel_unregister(I)

static void inline_mysql_tls_channel_unregister(
#ifdef HAVE_PSI_TLS_CHANNEL_INTERFACE
    TLS_channel_property_iterator *i
#else
    TLS_channel_property_iterator *i [[maybe_unused]]
#endif /* HAVE_PSI_TLS_CHANNEL_INTERFACE */
) {
#ifdef HAVE_PSI_TLS_CHANNEL_INTERFACE
  PSI_TLS_CHANNEL_CALL(unregister_tls_channel)(i);
#endif  // HAVE_PSI_TLS_CHANNEL_INTERFACE
}

/** @} (end of group psi_api_tls_channel) */

#endif  // !MYSQL_TLS_CHANNEL_H
