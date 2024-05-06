/*
   Copyright (c) 2011, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_SCHEMA_OBJECT_H
#define NDB_SCHEMA_OBJECT_H

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "my_inttypes.h"

/*
  Used for communication between the schema distribution Client(which often is
  in a user thread) performing a schema operation and the schema distribution
  Coordinator(which is running as part of the binlog thread).

  The schema distribution Client creates a NDB_SCHEMA_OBJECT before writing
  the schema operation to NDB, then it waits on the NDB_SCHEMA_OBJECT to be
  woken up when the schema operation is completed.

  The schema distribution Coordinator receives new events for the schema
  operation and will update the NDB_SCHEMA_OBJECT with replies and results from
  the other nodes in the cluster. Finally, all other MySQL Servers have replied
  and the schema distribution Client can continue.
*/
class NDB_SCHEMA_OBJECT {
  // String used when storing the NDB_SCHEMA_OBJECT in the list of
  // active NDB_SCHEMA_OBJECTs
  const std::string m_key;

  // The first part of key, normally used for db
  const std::string m_db;
  // The second part of key, normally used for name
  const std::string m_name;
  // The third part of key, normally used for id
  const uint32 m_id;
  // The fourth part of key, normally used for version
  const uint32 m_version;

  // Unique identifier giving each NDB_SCHEMA_OBJECT(and thus each schema
  // operation) a global id in combination with the nodeid of the node who
  // starts the schema operation
  const uint32 m_schema_op_id;

  // Point in time when schema operation started
  const std::chrono::steady_clock::time_point m_started;

  // State variables for the coordinator and client
  mutable struct State {
    // Mutex protecting state
    std::mutex m_lock;
    // Condition for communication between client and coordinator
    std::condition_variable m_cond;

    // Use counter controlling lifecycle of the NDB_SCHEMA_OBJECT
    // Normally there are only two users(the Client and the Coordinator)
    // but functions in the coordinator will also increment use count while
    // working with the NDB_SCHEMA_OBJECT
    uint m_use_count{1};

    // List of participant nodes in schema operation.
    // Used like this:
    // 1) When coordinator receives the schema op event it adds all the
    //    nodes currently subscribed as participants
    // 2) When coordinator receives reply or failure from a participant it will
    //    be removed from the list
    // 3) When list of participants is empty the coordinator will
    //    send the final ack, clearing all slock bits(thus releasing also any
    //    old version nodes)
    // 4) When final ack is received, client will be woken up
    struct Participant {
      bool m_completed{false};
      uint32 m_result{0};
      std::string m_message;
      Participant() = default;
      Participant(const Participant &) = delete;
      Participant &operator=(const Participant &) = delete;
    };
    std::unordered_map<uint32, Participant> m_participants;

    // Set after coordinator has received replies from all participants and
    // received the final ack which cleared all the slock bits
    bool m_coordinator_completed{false};

    bool dbug_lock() { return m_lock.try_lock(); }
    bool dbug_unlock() { return m_lock.unlock(), true; }
  } state;

  uint increment_use_count() const;
  uint decremement_use_count() const;

  NDB_SCHEMA_OBJECT() = delete;
  NDB_SCHEMA_OBJECT(const NDB_SCHEMA_OBJECT &) = delete;
  NDB_SCHEMA_OBJECT(const char *key, const char *db, const char *name,
                    uint32 id, uint32 version);
  ~NDB_SCHEMA_OBJECT();

  void fail_participants_not_in_list(const std::unordered_set<uint32> &nodes,
                                     uint32 result, const char *message) const;

  size_t count_completed_participants() const;

 public:
  const char *db() const { return m_db.c_str(); }
  const char *name() const { return m_name.c_str(); }
  uint32 id() const { return m_id; }
  uint32 version() const { return m_version; }

  // Return the schema operation id
  uint32 schema_op_id() const { return m_schema_op_id; }

  // Return current list of waiting participants as human readable string
  std::string waiting_participants_to_string() const;

  std::string to_string(const char *line_separator = "\n") const;

  /**
     @brief Initialize the NDB_SCHEMA_OBJECT facility

     @param nodeid The nodeid of this node
   */
  static void init(uint32 nodeid);

  /**
    @brief Get NDB_SCHEMA_OBJECT to be used for communication between Client
           and Coordinator. The Client is usually the one to create an instance
           while the Coordinator simple uses it.

           The parameters "db", "table_name", "id" and "version" identifies
           which object the communication is about

    @param db          Part 1 of key, normally used for database
    @param table_name  Part 2 of key, normally used for table
    @param id          Part 3 of key, normally used for id
    @param version     Part 4 of key, normally used for version
    @param create      Create a new NDB_SCHEMA_OBJECT and check that one
                       doesn't already exist.

    @return pointer to NDB_SCHEMA_OBJECT if it existed already or was created
    @return nullptr if NDB_SCHEMA_OBJECT didn't exist
  */
  static NDB_SCHEMA_OBJECT *get(const char *db, const char *table_name,
                                uint32 id, uint32 version, bool create = false);

  /**
    @brief Get NDB_SCHEMA_OBJECT by schema operation id

    @param nodeid Nodeid of the node which started the schema operation
    @param schema_op_id  Id of the schema operation in the node which started
                         the schema operation

    @note This function should only be used on the Coordinator(i.e where the
    nodeid in the schema operation matches own nodeid)

    @return pointer to NDB_SCHEMA_OBJECT if it existed
    @return nullptr if NDB_SCHEMA_OBJECT didn't exist
  */
  static NDB_SCHEMA_OBJECT *get(uint32 nodeid, uint32 schema_op_id);

  /**
    @brief Get NDB_SCHEMA_OBJECT by pointer to existing. Used to acquire another
    reference.

    @param schema_object Pointer to existing NDB_SCHEMA_OBHECT

    @return pointer to existing NDB_SCHEMA_OBJECT
  */
  static NDB_SCHEMA_OBJECT *get(NDB_SCHEMA_OBJECT *schema_object);

  /**
     @brief Release NDB_SCHEMA_OBJECT which has been acquired with get()

     @param ndb_schema_object pointer to NDB_SCHEMA_OBJECT to release
   */
  static void release(NDB_SCHEMA_OBJECT *ndb_schema_object);

  /**
     @brief Count number of NDB_SCHEMA_OBJECTs registered

     @return Number of NDB_SCHEMA_OBJECTs
   */
  static size_t count_active_schema_ops();

  /**
     @brief Register participants taking part in schema operation.
     The function will check the schema operation doesn't already contain
     participants, in such case the participants can't be registered.

     @param nodes List of nodes to add

     @return true Could not register participants
   */
  bool register_participants(const std::unordered_set<uint32> &nodes);

  /**
     @brief Save the result received from a node
     @param participant_node_id The nodeid of the node who reported result
     @param result The result received
     @param message The message describing the result if != 0

     @return true if node was registered as participant, false otherwise
   */
  bool result_received_from_node(uint32 participant_node_id, uint32 result,
                                 const std::string &message) const;

  /**
     @brief Save the acks received from several nodes
     @note Used when using the old protocol, no result is provided
     @param nodes The list of nodes which have been acked
   */
  void result_received_from_nodes(
      const std::unordered_set<uint32> &nodes) const;

  /**
     @brief Check if all participants has completed.
     @return true all participants completed
   */
  bool check_all_participants_completed() const;

  /**
     @brief Check if all participants have completed and notify waiter. This is
     the last step in the normal path when participants reply. Requires that all
     participants has completed.

     @return true if coordinator has completed
   */
  bool check_coordinator_completed() const;

  /**
     @brief Check if any client should wakeup after subscribers have changed.
     This happens when node unsubscribes(one subscriber shutdown or fail) or
     when cluster connection is lost(all subscribers are removed)
     @param new_subscribers Current set of subscribers
     @param result The result to set on the participant
     @param message The message to set on the participant
     @return true if all participants have completed
   */
  bool check_for_failed_subscribers(
      const std::unordered_set<uint32> &new_subscribers, uint32 result,
      const char *message) const;

  /**
     @brief Check if schema operation has timed out and in such case mark all
     participants which haven't already completed as timedout.
     @param is_client Indicates if it is the client checking timeout
     @param result The result to set on the participant
     @param message The message to set on the participant
     @return true if timeout occurred (and all participants have completed)
   */
  bool check_timeout(bool is_client, int timeout_seconds, uint32 result,
                     const char *message) const;

  /**
     @brief Set schema operation as failed and mark all participants which
     haven't already completed as failed.
     @param result The result to set on the participant
     @param message The message to set on the participant
   */
  void fail_schema_op(uint32 result, const char *message) const;

  /**
     @brief Fail all schema operations.
     @param result The result to set on the participant
     @param message The message to set on the participant
   */
  static void fail_all_schema_ops(uint32 result, const char *message);

  /**
     @brief Wait until coordinator indicates that all participants has completed
     or timeout occurs
     @param max_wait_seconds Max time to wait
     @return true if all participants has completed
   */
  bool client_wait_completed(uint max_wait_seconds) const;

  struct Result {
    uint32 nodeid;
    uint32 result;
    std::string message;
  };
  /**
     @brief Return list of schema operation results consisting of nodeid,
     result and message
   */
  void client_get_schema_op_results(std::vector<Result> &results) const;
};

#endif
