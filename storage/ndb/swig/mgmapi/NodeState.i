/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 MySQL, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 *   Status of a node in the cluster.
 *
 *   Sub-structure in enum ndb_mgm_cluster_state
 *   returned by ndb_mgm_get_status().
 *
 *   @note <var>node_status</var>, <var>start_phase</var>,
 *         <var>dynamic_id</var>
 *         and <var>node_group</var> are relevant only for database nodes,
 *         i.e. <var>node_type</var> == @ref NDB_MGM_NODE_TYPE_NDB.
 */
%rename ndb_mgm_node_state NodeState;
struct ndb_mgm_node_state {
  /** NDB Cluster node ID*/
private:
  ndb_mgm_node_state();
  ~ndb_mgm_node_state();
  int node_id;
  /** Type of NDB Cluster node*/
  enum ndb_mgm_node_type   node_type;
  /** State of node*/
  enum ndb_mgm_node_status node_status;
  /** Start phase.
   *
   *  @note Start phase is only valid if the <var>node_type</var> is
   *        NDB_MGM_NODE_TYPE_NDB and the <var>node_status</var> is
   *        NDB_MGM_NODE_STATUS_STARTING
   */
  int start_phase;
  /** ID for heartbeats and master take-over (only valid for DB nodes)
   */
  int dynamic_id;
  /** Node group of node (only valid for DB nodes)*/
  int node_group;
  /** Internal version number*/
  int version;
  /** Number of times node has connected or disconnected to the
   *  management server
   */
  int connect_count;
  /** IP address of node when it connected to the management server.
   *  @note This value will be empty if the management server has restarted
   *        since the node last connected.
   */
  %immutable;
  char connect_address[];
  %mutable;

};

%extend ndb_mgm_node_state {

public:
  char * getConnectAddress(void) {
    return $self->connect_address;
  }

  int getConnectCount() {
    return $self->connect_count;
  }

  int getDynamicID() {
    return $self->dynamic_id;
  }

  int getNodeGroup() {
    return $self->node_group;
  }

  int getNodeID() {
    return $self->node_id;
  }

  ndb_mgm_node_status getNodeStatus() {
    return $self->node_status;
  }

  ndb_mgm_node_type getNodeType() {
    return $self->node_type;
  }

  int getStartPhase() {
    return $self->start_phase;
  }

  int getVersion() {
    return $self->version;
  }

  const char * getStartupPhase() {
    const char * startPhaseStr;

    switch ($self->start_phase) {
    case 0: startPhaseStr="Startup Phase 0. Clearing the filesystem, --initial was specified as startup option."; break;
    case 1: startPhaseStr="Startup Phase 1. Establishing inter-node connections in cluster."; break;
    case 2: startPhaseStr="Startup Phase 2. The arbitrator node is elected. If this is a system restart, the cluster determines the latest restorable global checkpoint."; break;
    case 3: startPhaseStr="Startup Phase 3. This stage initializes a number of internal cluster variables."; break;
    case 4: startPhaseStr="Startup Phase 4. If initial (re)start: redo log files are created.  If system restart: read schemas, LCP, redo logs up to last GCP. Node restart: Find tail of redo log."; break;
    case 5: startPhaseStr="Startup Phase 5. If initial start: creating internal system tables. If node restart or initial restart: include node in transactions, synchronise with node group and master."; break;
    case 6: startPhaseStr="Startup Phase 6. Update Internal variables."; break;
    case 7: startPhaseStr="Startup Phase 7. Update Internal variables."; break;
    case 8: startPhaseStr="Startup Phase 8. If system restart: rebuild all indexes."; break;
    case 9: startPhaseStr="Startup Phase 9. Update Internal variables."; break;
    case 10: startPhaseStr="Startup Phase 10. If node restart or initial node restart: API nodes may connect and send events."; break;
    case 11: startPhaseStr="Startup Phase 11. If node restart or initial node restart: taking responsibility for new transactions. After this pahse, the new node can now act as transaction coordinator."; break;
    default: startPhaseStr="Undefined Startup Phase"; break;
    }
    return startPhaseStr;
  }


};

