/*
   Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "DnsCache.hpp"

#include "NdbTCP.h"
#include "portlib/ndb_sockaddr.h"

LocalDnsCache::~LocalDnsCache() {
  for (const auto& pair : m_resolver_cache) {
    delete pair.second;
  }
}

bool LocalDnsCache::getCachedOrResolveAddress(ndb_sockaddr *result,
                                              const char *hostname) {
  const auto pair = m_resolver_cache.find(hostname);

  if (pair != m_resolver_cache.end()) {
    *result = *pair->second;
    return true; /* Usable cache hit */
  }

  if (Ndb_getAddr(result, hostname) != 0) {
    return false;  // hostname not found in DNS
  }

  // Hostname found, create a cache entry
  m_resolver_cache[hostname] = new ndb_sockaddr(*result);
  return true;
}

int LocalDnsCache::getAddress(ndb_sockaddr *result_address, const char *hostname) {
  if (m_failed_lookups.count(hostname) != 0) {
    // Lookup failed earlier, same result now
    return -1;
  }

  const bool result = getCachedOrResolveAddress(result_address, hostname);
  if (!result) {
    // Not valid address, save for later
    m_failed_lookups.insert(hostname);
  }
  return result ? 0 : -1;
}
