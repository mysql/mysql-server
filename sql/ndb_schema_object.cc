/*
   Copyright (c) 2011, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include <atomic>
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
  // Nodeid of this node
  uint32 m_own_nodeid{0};

  // Mutex protecting the unordered map
  std::mutex m_lock;
  std::unordered_map<std::string, NDB_SCHEMA_OBJECT *> m_hash;
  Ndb_schema_objects() {}

  NDB_SCHEMA_OBJECT *find(std::string key) const {
    const auto it = m_hash.find(key);
    if (it == m_hash.end()) return nullptr;
    return it->second;
  }

  /**
     @brief Find NDB_SCHEMA_OBJECT with corresponding nodeid and schema_op_id

     @param nodeid Nodeid to find
     @param schema_op_id Schema operation id to find
     @return Pointer to NDB_SCHEMA_OBJECT or nullptr

     @note Searches by iterating over the list until an entry is found. This is
     ok as normally only one schema operation at a time is supported and thus
     there is only one entry in the hash.
   */
  NDB_SCHEMA_OBJECT *find(uint32 nodeid, uint32 schema_op_id) const {
    DBUG_ASSERT(nodeid);

    // Make sure that own nodeid has been set
    DBUG_ASSERT(m_own_nodeid);

    if (nodeid != m_own_nodeid) {
      // Looking for a schema operation started in another node,  the
      // schema_op_id is only valid in the node which started
      return nullptr;
    }

    for (const auto entry : m_hash){
      NDB_SCHEMA_OBJECT* schema_object = entry.second;
      if (schema_object->schema_op_id() == schema_op_id)
        return schema_object;
    }
    return nullptr;
  }
} active_schema_clients;

void NDB_SCHEMA_OBJECT::init(uint32 nodeid) {
  DBUG_ASSERT(nodeid);
  std::lock_guard<std::mutex> lock_hash(active_schema_clients.m_lock);
  // Make sure that no active schema clients exist when function is called
  DBUG_ASSERT(active_schema_clients.m_hash.size() == 0);
  active_schema_clients.m_own_nodeid = nodeid;
}

static uint32 next_schema_op_id() {
  static std::atomic<uint32> schema_op_id_sequence{1};
  uint32 id = schema_op_id_sequence++;
  // Handle wraparound
  if (id == 0) {
    id = schema_op_id_sequence++;
  }
  DBUG_ASSERT(id != 0);
  return id;
}

NDB_SCHEMA_OBJECT::NDB_SCHEMA_OBJECT(const char *key, const char *db,
                                     const char *name, uint32 id,
                                     uint32 version, uint slock_bits)
    : m_key(key),
      m_db(db),
      m_name(name),
      m_id(id),
      m_version(version),
      m_schema_op_id(next_schema_op_id()) {
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

  // Number of partipcipants must be provided when allowing a new
  // instance to be created
  DBUG_ASSERT((create_if_not_exists && participants) ||
              !create_if_not_exists);

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

  ndb_schema_object = new (std::nothrow)
      NDB_SCHEMA_OBJECT(key.c_str(), db, table_name, id, version, participants);
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

NDB_SCHEMA_OBJECT* NDB_SCHEMA_OBJECT::get(uint32 nodeid, uint32 schema_op_id) {
  DBUG_ENTER("NDB_SCHEMA_OBJECT::get");
  DBUG_PRINT("enter", ("nodeid: %d, schema_op_id: %u", nodeid, schema_op_id));

  std::lock_guard<std::mutex> lock_hash(active_schema_clients.m_lock);

  NDB_SCHEMA_OBJECT *ndb_schema_object =
      active_schema_clients.find(nodeid, schema_op_id);
  if (ndb_schema_object) {
    ndb_schema_object->m_use_count++;
    DBUG_PRINT("info", ("use_count: %d", ndb_schema_object->m_use_count));
    DBUG_RETURN(ndb_schema_object);
  }

  DBUG_PRINT("info", ("No NDB_SCHEMA_OBJECT found"));
  DBUG_RETURN(nullptr);
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
