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
#include "sql/ndb_require.h"

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

void NDB_SCHEMA_OBJECT::get_schema_op_ids(std::vector<uint32>& ids)
{
  std::lock_guard<std::mutex> lock_hash(active_schema_clients.m_lock);
  for (const auto entry : active_schema_clients.m_hash){
    NDB_SCHEMA_OBJECT* schema_object = entry.second;
    ids.push_back(schema_object->schema_op_id());
  }
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
                                     uint32 version)
    : m_key(key),
      m_db(db),
      m_name(name),
      m_id(id),
      m_version(version),
      m_schema_op_id(next_schema_op_id()) {
}

NDB_SCHEMA_OBJECT::~NDB_SCHEMA_OBJECT() {
  DBUG_ASSERT(m_use_count == 0);
  // Check that all participants have completed
  DBUG_ASSERT(state.m_participants.size() == count_completed_participants());
  // Coordinator should have completed
  DBUG_ASSERT(state.m_coordinator_completed);
}

NDB_SCHEMA_OBJECT *NDB_SCHEMA_OBJECT::get(const char *db,
                                          const char *table_name, uint32 id,
                                          uint32 version,
                                          bool create_if_not_exists) {
  DBUG_ENTER("NDB_SCHEMA_OBJECT::get");
  DBUG_PRINT("enter", ("db: '%s', table_name: '%s', id: %u, version: %u",
                       db, table_name, id, version));

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
      NDB_SCHEMA_OBJECT(key.c_str(), db, table_name, id, version);
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

std::string NDB_SCHEMA_OBJECT::waiting_participants_to_string() const {
  std::lock_guard<std::mutex> lock_participants(state.m_lock);
  const char* separator = "";
  std::string participants("[");
  for (const auto& it: state.m_participants){
    if (it.second.m_completed == true)
      continue; // Don't show completed
    participants.append(separator).append(std::to_string(it.first));
    separator = ",";
  }
  participants.append("]");
  return participants;
}

size_t NDB_SCHEMA_OBJECT::count_completed_participants() const {
  size_t count = 0;
  for (const auto& it : state.m_participants) {
    const State::Participant &participant = it.second;
    if (participant.m_completed) count++;
  }
  return count;
}

void NDB_SCHEMA_OBJECT::register_participants(
    const std::unordered_set<uint32> &nodes) const {
  std::lock_guard<std::mutex> lock_participants(state.m_lock);

  // Assume the list of participants is empty
  ndbcluster::ndbrequire(state.m_participants.size() == 0);
  // Assume coordinator have not completed
  ndbcluster::ndbrequire(!state.m_coordinator_completed);

  // Insert new participants as specified by nodes list
  for (const uint32 node: nodes)
    state.m_participants[node];

  // Double check that there are as many participants as nodes
  ndbcluster::ndbrequire(nodes.size() == state.m_participants.size());
}

void NDB_SCHEMA_OBJECT::result_received_from_node(
    uint32 participant_node_id, uint32 result,
    const std::string &message) const {
  std::lock_guard<std::mutex> lock_participants(state.m_lock);

  const auto it = state.m_participants.find(participant_node_id);
  if (it == state.m_participants.end()) {
    // Received reply from node not registered as participant, may happen
    // when a node hears the schema op but this node hasn't registered it as
    // subscriber yet.
    return;
  }

  // Mark participant as completed and save result
  State::Participant& participant = it->second;
  participant.m_completed = true;
  participant.m_result = result;
  participant.m_message = message;
}

void NDB_SCHEMA_OBJECT::result_received_from_nodes(
    const std::unordered_set<uint32> &nodes) const {
  std::unique_lock<std::mutex> lock_participants(state.m_lock);

  // Mark the listed nodes as completed
  for(auto node : nodes) {
    const auto it = state.m_participants.find(node);
    if (it == state.m_participants.end()) {
      // Received reply from node not registered as participant, may happen
      // when a node hears the schema op but this node hasn't registered it as
      // subscriber yet.
      return;
    }

    // Participant is not in list, mark it as failed
    State::Participant& participant = it->second;
    participant.m_completed = true;
    // No result or message provided in old protocol
  }
}

bool NDB_SCHEMA_OBJECT::check_all_participants_completed() const {
  std::lock_guard<std::mutex> lock_participants(state.m_lock);
  return state.m_participants.size() == count_completed_participants();
}

void NDB_SCHEMA_OBJECT::fail_participants_not_in_list(
    const std::unordered_set<uint32> &nodes, uint32 result,
    const char *message) const {
  for (auto& it : state.m_participants) {
    if (nodes.find(it.first) != nodes.end()) {
      // Participant still exist in list
      continue;
    }

    // Participant is not in list, mark it as failed
    State::Participant& participant = it.second;
    participant.m_completed = true;
    participant.m_result = result;
    participant.m_message = message;
  }
}

bool NDB_SCHEMA_OBJECT::check_for_failed_subscribers(
    const std::unordered_set<uint32> &new_subscribers, uint32 result,
    const char *message) const {
  std::unique_lock<std::mutex> lock_participants(state.m_lock);

  // Fail participants not in list of nodes
  fail_participants_not_in_list(new_subscribers, result, message);

  if (state.m_participants.size() != count_completed_participants()) {
    // Not all participants have completed yet
    return false;
  }

  // All participants have replied
  return true;
}

bool NDB_SCHEMA_OBJECT::check_coordinator_completed() const {
  std::unique_lock<std::mutex> lock_participants(state.m_lock);
  // Don't set completed unless all participants have replied
  if (state.m_participants.size() != count_completed_participants())
    return false;

  state.m_coordinator_completed = true;
  // Signal the Client
  state.m_cond.notify_one();
  return true;
}

bool NDB_SCHEMA_OBJECT::client_wait_completed(uint max_wait_seconds) const {
  const auto timeout_time = std::chrono::seconds(max_wait_seconds);
  std::unique_lock<std::mutex> lock_participants(state.m_lock);

  const bool completed =
      state.m_cond.wait_for(lock_participants, timeout_time, [this]() {
        return state.m_coordinator_completed &&
               state.m_participants.size() == count_completed_participants();
      });

  return completed;
}
