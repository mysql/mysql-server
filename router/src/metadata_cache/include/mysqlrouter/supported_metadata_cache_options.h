/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef METADATA_CACHE_SUPPORTED_OPTIONS_INCLUDED
#define METADATA_CACHE_SUPPORTED_OPTIONS_INCLUDED

#include <array>

static constexpr std::array<const char *, 19> metadata_cache_supported_options{{
    "user",
    "ttl",
    "auth_cache_ttl",
    "auth_cache_refresh_interval",
    "metadata_cluster",
    "connect_timeout",
    "read_timeout",
    "router_id",
    "thread_stack_size",
    "use_gr_notifications",
    "cluster_type",
    "bootstrap_server_addresses",
    "ssl_mode",
    "ssl_cipher",
    "tls_version",
    "ssl_ca",
    "ssl_capath",
    "ssl_crl",
    "ssl_crlpath",
}};

#endif /* METADATA_CACHE_SUPPORTED_OPTIONS_INCLUDED */
