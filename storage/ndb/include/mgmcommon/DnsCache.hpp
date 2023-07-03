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

#ifndef DnsCache_H
#define DnsCache_H

#include <string>
#include <unordered_set>
#include <unordered_map>

/*
  Local DNS cache used to speed up subsequent DNS lookups where same hostname is
  potentially resolved many times.

  Entries in the cache live only as long as the LocalDnsCache object
  itself. The user should only create stack-based LocalDnsCache objects, and
  these should not be long-lived. Heap allocation of LocalDnsCache is not
  allowed.
*/

struct in6_addr;

class LocalDnsCache {
public:
  ~LocalDnsCache();

  int getAddress(in6_addr * result, const char *hostname);
protected:
  /* no heap allocation */
  static void * operator new(std::size_t) = delete;
  static void * operator new[](std::size_t) = delete;

private:
  // Negative cache of DNS misses
  std::unordered_set<std::string> m_failed_lookups;
  // Positive cache of DNS lookups
  std::unordered_map<std::string, in6_addr*> m_resolver_cache;

  bool getCachedOrResolveAddress(in6_addr* result, const char* hostname);
};

#endif
