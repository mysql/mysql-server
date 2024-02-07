/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQL_PSI_TLS_CHANNEL_H
#define MYSQL_PSI_TLS_CHANNEL_H

/**
  @file include/mysql/psi/psi_tls_channel.h
  Performance schema instrumentation interface.

  @defgroup psi_abi_tls_channel TLS Channel Instrumentation (ABI)
  @ingroup psi_abi
  @{
*/

#include "my_inttypes.h"
#include "my_macros.h"

/* HAVE_PSI_*_INTERFACE */
#include "my_psi_config.h"  // IWYU pragma: keep

#include "my_sharedlib.h"
#include "mysql/components/services/bits/psi_tls_channel_bits.h"

/** Entry point for the performance schema interface. */
struct PSI_tls_channel_bootstrap {
  /**
    ABI interface finder.
    Calling this method with an interface version number returns either
    an instance of the ABI for this version, or NULL.
    @sa PSI_TLS_CHANNEL_VERSION_1
    @sa PSI_CURRENT_TLS_CHANNEL_VERSION
  */
  void *(*get_interface)(int version);
};
typedef struct PSI_tls_channel_bootstrap PSI_tls_channel_bootstrap;

#ifdef HAVE_PSI_TLS_CHANNEL_INTERFACE

/**
  Performance schema TLS channel interface, version 1.
  @since PSI_TLS_CHANNEL_VERSION_1
*/
struct PSI_tls_channel_service_v1 {
  register_tls_channel_v1_t register_tls_channel;
  unregister_tls_channel_v1_t unregister_tls_channel;
};

typedef struct PSI_tls_channel_service_v1 PSI_tls_channel_service_t;

extern MYSQL_PLUGIN_IMPORT PSI_tls_channel_service_t *psi_tls_channel_service;

#endif  // HAVE_PSI_TLS_CHANNEL_INTERFACE

/** @} (end of group psi_abi_tls_channel) */

#endif  // !MYSQL_PSI_TLS_CHANNEL_H
