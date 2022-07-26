/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#ifndef PFS_TLS_CHANNEL_H
#define PFS_TLS_CHANNEL_H

/**
  @file storage/perfschema/pfs_tls_channel.h
  Performance schema instrumentation (declarations).
*/
#include <vector>

#include <mysql/psi/psi_tls_channel.h>
#include "mysql/components/services/bits/mysql_rwlock_bits.h"

/* A convenience wrapper */
using tls_channels = std::vector<TLS_channel_property_iterator *>;

/**
  Global structure to store all instrumented TLS channels registered with PFS
*/
extern tls_channels g_instrumented_tls_channels;

/**
  RW lock that protects list of instrumented TLS channels.
  @sa g_instrumented_tls_channels
*/
extern mysql_rwlock_t LOCK_pfs_tls_channels;

/**
  Register a TLS channel for instrumentation with PFS

  @param [in] provider Iterator implementation to fetch all properties
*/
void pfs_register_tls_channel_v1(TLS_channel_property_iterator *provider);

/**
  Un-register a TLS channel for instrumentation with PFS

  @param [in] provider Iterator implementation
*/
void pfs_unregister_tls_channel_v1(TLS_channel_property_iterator *provider);

/**
  Initialize internal data structures to instrument TLS channels
*/
void init_pfs_tls_channels_instrumentation();

/**
  Deinitialize internal data structures to instrument TLS channels
*/
void cleanup_pfs_tls_channels_instrumentation();

#endif  // !PFS_TLS_CHANNEL_H
