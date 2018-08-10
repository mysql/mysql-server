/*
  Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef METADATA_CACHE_METADATA_FACTORY_INCLUDED
#define METADATA_CACHE_METADATA_FACTORY_INCLUDED

#include <memory>

#include "metadata.h"
#include "tcp_address.h"

// This provides a factory method that returns a pluggable instance
// to the underlying transport layer implementation. The transport
// layer provides the means from which the metadata is
// fetched.

std::shared_ptr<MetaData> get_instance(
    const std::string &user, const std::string &password, int connect_timeout,
    int read_timeout, int connection_attempts, std::chrono::milliseconds ttl,
    const mysqlrouter::SSLOptions &ssl_options);

#endif  // METADATA_CACHE_METADATA_FACTORY_INCLUDED
