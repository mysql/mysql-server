/*
   Copyright (c) 2011, 2018, Oracle and/or its affiliates. All rights reserved.

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

// Implements the functions declared in ndb_schema_object.h
#include "sql/ndb_schema_object.h"

#include <mutex>
#include <string>
#include <unordered_map>

#include "sql/ndb_bitmap.h"

// List keeping track of active NDB_SCHEMA_OBJECTs. The list is used
// by the schema distribution coordinator to find the correct NDB_SCHEMA_OBJECT
// in order to communicate with the schema dist client.
class Ndb_schema_objects
{
public:
  // Mutex protecting the unordered map
  std::mutex m_lock;
  std::unordered_map<std::string, NDB_SCHEMA_OBJECT *> m_hash;
  Ndb_schema_objects() {}

  NDB_SCHEMA_OBJECT *find(std::string key) const {
    const auto it = m_hash.find(key);
    if (it == m_hash.end()) return nullptr;
    return it->second;
  }
} active_schema_clients;

NDB_SCHEMA_OBJECT::NDB_SCHEMA_OBJECT(const char *key, uint slock_bits)
    : m_key(key) {
  // Check legacy min limit for number of bits
  DBUG_ASSERT(slock_bits >= 256);

  // Initialize bitmap, clears all bits.
  bitmap_init(&slock_bitmap, nullptr, slock_bits, false);

  // Set all bits in order to expect answer from all other nodes by
  // default(those who are not subscribed will be filtered away by the
  // Coordinator which keep track of such stuff)
  bitmap_set_all(&slock_bitmap);

  mysql_mutex_init(PSI_INSTRUMENT_ME, &mutex, MY_MUTEX_INIT_FAST);
  mysql_cond_init(PSI_INSTRUMENT_ME, &cond);
}

NDB_SCHEMA_OBJECT::~NDB_SCHEMA_OBJECT() {
  DBUG_ASSERT(m_use_count == 0);

  mysql_cond_destroy(&cond);
  mysql_mutex_destroy(&mutex);

  bitmap_free(&slock_bitmap);
}

NDB_SCHEMA_OBJECT *NDB_SCHEMA_OBJECT::get(const char *db,
                                          const char *table_name, uint32 id,
                                          uint32 version, uint participants,
                                          bool create_if_not_exists) {
  DBUG_ENTER("NDB_SCHEMA_OBJECT::get");
  DBUG_PRINT("enter", ("db: '%s', table_name: '%s', id: %u, version: %u",
                       db, table_name, id, version));

  // Number of partipcipants must be provided when allowing a new instance to be
  // created
  DBUG_ASSERT((create_if_not_exists && participants) || !create_if_not_exists);

  // Build a key on the form "./<db>/<name>_<id>_<version>"
  const std::string key = std::string("./") + db + "/" + table_name + "_" +
                          std::to_string(id) + "_" + std::to_string(version);
  DBUG_PRINT("info", ("key: '%s'", key.c_str()));

  std::lock_guard<std::mutex> lock_hash(active_schema_clients.m_lock);

  NDB_SCHEMA_OBJECT *ndb_schema_object = active_schema_clients.find(key);
  if (ndb_schema_object)
  {
    ndb_schema_object->m_use_count++;
    DBUG_PRINT("info", ("use_count: %d", ndb_schema_object->m_use_count));
    DBUG_RETURN(ndb_schema_object);
  }

  if (!create_if_not_exists) {
    DBUG_PRINT("info", ("does not exist"));
    DBUG_RETURN(nullptr);
  }

  ndb_schema_object =
      new (std::nothrow) NDB_SCHEMA_OBJECT(key.c_str(), participants);
  if (!ndb_schema_object) {
    DBUG_PRINT("info", ("failed to allocate"));
    DBUG_RETURN(nullptr);
  }

  // Add to list of NDB_SCHEMA_OBJECTs
  active_schema_clients.m_hash.emplace(key, ndb_schema_object);
  ndb_schema_object->m_use_count++;
  DBUG_PRINT("info", ("use_count: %d", ndb_schema_object->m_use_count));
  DBUG_RETURN(ndb_schema_object);
}

void
NDB_SCHEMA_OBJECT::release(NDB_SCHEMA_OBJECT *ndb_schema_object)
{
  DBUG_ENTER("NDB_SCHEMA_OBJECT::release");
  DBUG_PRINT("enter", ("key: '%s'", ndb_schema_object->m_key.c_str()));

  std::lock_guard<std::mutex> lock_hash(active_schema_clients.m_lock);

  ndb_schema_object->m_use_count--;
  DBUG_PRINT("info", ("use_count: %d", ndb_schema_object->m_use_count));

  if (ndb_schema_object->m_use_count != 0)
    DBUG_VOID_RETURN;

  // Remove from list of NDB_SCHEMA_OBJECTS
  active_schema_clients.m_hash.erase(ndb_schema_object->m_key);
  delete ndb_schema_object;
  DBUG_VOID_RETURN;
}


void NDB_SCHEMA_OBJECT::check_waiters(const MY_BITMAP &new_participants)
{
  std::lock_guard<std::mutex> lock_hash(active_schema_clients.m_lock);

  for (const auto &key_and_value : active_schema_clients.m_hash)
  {
    NDB_SCHEMA_OBJECT *schema_object = key_and_value.second;
    schema_object->check_waiter(new_participants);
  }
}

void
NDB_SCHEMA_OBJECT::check_waiter(const MY_BITMAP &new_participants)
{
  mysql_mutex_lock(&mutex);
  bitmap_intersect(&slock_bitmap, &new_participants);
  mysql_mutex_unlock(&mutex);

  // Wakeup waiting Client
  mysql_cond_signal(&cond);
}

std::string NDB_SCHEMA_OBJECT::slock_bitmap_to_string() const {
  return ndb_bitmap_to_hex_string(&slock_bitmap);
}
