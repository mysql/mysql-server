/*
   Copyright (c) 2020 Oracle and/or its affiliates.

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
#include <unordered_map>

#include "NdbTCP.h"
#include "NdbMutex.h"
#include "NdbTick.h"

#include "DnsCache.hpp"

static constexpr unsigned int default_expire_time = 300;  // 5 minutes

class CacheEntry {
public:
  CacheEntry(struct in6_addr _addr, NDB_TICKS _time ) :
    create_time(_time),
    address(_addr)                                       {}
  Uint64 age(NDB_TICKS current_time);

  NDB_TICKS create_time;
  struct in6_addr address;
};

Uint64 CacheEntry::age(NDB_TICKS t) {
  return NdbTick_Elapsed(create_time, t).seconds();
}

class GlobalDnsCache : public NdbLockable {
public:
  GlobalDnsCache() : NdbLockable() {}
  ~GlobalDnsCache();
  GlobalDnsCache(const GlobalDnsCache &)            = delete;
  GlobalDnsCache& operator=(const GlobalDnsCache &) = delete;
  GlobalDnsCache(GlobalDnsCache &&)                 = delete;
  GlobalDnsCache& operator=(GlobalDnsCache &&)      = delete;

  bool getAddress(struct in6_addr *result, const char *hostname);

private:
  std::unordered_map<std::string, CacheEntry *> m_resolver_cache;
};

GlobalDnsCache::~GlobalDnsCache() {
  for(auto pair : m_resolver_cache)
    delete pair.second;
 }

bool GlobalDnsCache::getAddress(struct in6_addr *result, const char *hostname) {
  Guard locked(m_mutex);
  NDB_TICKS current_time = NdbTick_getCurrentTicks();
  auto pair = m_resolver_cache.find(hostname);

  if (pair != m_resolver_cache.end()) {
    CacheEntry * record = pair->second;
    if(record->age(current_time) < default_expire_time) {
      *result = record->address;
      return true;   /* Usable cache hit */
    }
    /* Expired entry */
    m_resolver_cache.erase(pair);
    delete record;
  }

  if (Ndb_getInAddr6(result, hostname) != 0)
    return false;   // hostname not found in DNS

  /* Hostname found; create a cache entry*/
  m_resolver_cache[hostname] = new CacheEntry(*result, current_time);
  return true;
}

/* User interface: LocalDnsCache
 *
 * Contains its own negative cache of DNS misses, so that they are limited
 * to the scope of the local cache
 */
static GlobalDnsCache theGlobalCache;

int LocalDnsCache::getAddress(struct in6_addr * result, const char *hostname) {
  if(m_failed_lookups.count(hostname)) return -1;

  bool globalResult = theGlobalCache.getAddress(result, hostname);

  if(! globalResult) m_failed_lookups.insert(hostname);
  return globalResult ? 0 : -1;
}



