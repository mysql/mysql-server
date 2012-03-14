/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NODE_STATE_HPP
#define NODE_STATE_HPP

#include <NdbOut.hpp>
#include <NodeBitmask.hpp>

struct NodeStatePOD
{
  enum StartLevel {
    /**
     * SL_NOTHING 
     *   Nothing is started
     */
    SL_NOTHING    = 0,

    /**
     * SL_CMVMI
     *   CMVMI is started
     *   Listening to management server
     *   Qmgr knows nothing...
     */
    SL_CMVMI = 1,
    
    /**
     * SL_STARTING
     *   All blocks are starting
     *   Initial or restart
     *   During this phase is <b>startPhase</b> valid
     */
    SL_STARTING = 2,
    
    /**
     * The database is started open for connections
     */
    SL_STARTED = 3,

    SL_SINGLEUSER = 4,

    /**
     * SL_STOPPING_1 - Inform API
     *   API is informed not to start transactions on node
     *	 The database is about to close
     *
     *   New TcSeize(s) are refused (TcSeizeRef)
     */
    SL_STOPPING_1 = 5,
    
    /**
     * SL_STOPPING_2 - Close TC
     *   New transactions(TC) are refused
     */
    SL_STOPPING_2 = 6,



    
    /**
     * SL_STOPPING_3 - Wait for reads in LQH
     *   No transactions are running in TC
     *   New scans(s) and read(s) are refused in LQH
     *   NS: The node is not Primary for any fragment
     *   NS: No node is allow to start
     */
    SL_STOPPING_3 = 7,
    
    /**
     * SL_STOPPING_4 - Close LQH
     *   Node is out of DIGETNODES
     *   Insert/Update/Delete can still be running in LQH
     *   GCP is refused
     *   Node is not startable w.o Node Recovery
     */
    SL_STOPPING_4 = 8
  };

  enum StartType {
    ST_INITIAL_START = 0,
    ST_SYSTEM_RESTART = 1,
    ST_NODE_RESTART = 2,
    ST_INITIAL_NODE_RESTART = 3,
    ST_ILLEGAL_TYPE = 4
  };
  
  /**
   * Length in 32-bit words
   */
  STATIC_CONST( DataLength = 8 + NodeBitmask::Size );
  
  /**
   * Constructor(s)
   */
  void init();
 
  /**
   * Current start level
   */
  Uint32 startLevel;

  /**
   * Node group 
   */
  Uint32 nodeGroup;  // valid when startLevel == SL_STARTING

  /**
   * Dynamic id
   */ 
  union {
    Uint32 dynamicId;    // valid when startLevel == SL_STARTING to API
    Uint32 masterNodeId; // When from cntr
  };
    
  /**
   * 
   */
  union {
    struct {
      Uint32 startPhase;     // valid when startLevel == SL_STARTING
      Uint32 restartType;    // valid when startLevel == SL_STARTING
    } starting;
    struct {
      Uint32 systemShutdown; // valid when startLevel == SL_STOPPING_{X}
      Uint32 timeout;
      Uint32 alarmTime;
    } stopping;

    
  };
  Uint32 singleUserMode;
  Uint32 singleUserApi;          //the single user node

  BitmaskPOD<NodeBitmask::Size> m_connected_nodes;

  void setDynamicId(Uint32 dynamic);
  void setNodeGroup(Uint32 group);
  void setSingleUser(Uint32 s);
  void setSingleUserApi(Uint32 n);
  

  /**
   * Is a node restart in progress (ordinary or initial)
   */
  bool getNodeRestartInProgress() const;

  /**
   * Is a system restart ongoing
   */
  bool getSystemRestartInProgress() const;

  /**
   * Are we started
   */
  bool getStarted() const {
    return startLevel == SL_STARTED || startLevel == SL_SINGLEUSER;
  }

  /**
   * Is in single user mode?
   */
  bool getSingleUserMode() const;

  /**
   * Is in single user mode
   */
  Uint32 getSingleUserApi() const;
};

class NodeState : public NodeStatePOD
{
public:
  NodeState();
  NodeState(StartLevel);
  NodeState(StartLevel, bool systemShutdown);
  NodeState(StartLevel, Uint32 startPhase, StartType);

  NodeState& operator=(const NodeStatePOD&);
};

inline
NodeState::NodeState(){
  init();
}

inline
void
NodeStatePOD::init(){
  startLevel = SL_CMVMI;
  nodeGroup = 0xFFFFFFFF;
  dynamicId = 0xFFFFFFFF;
  singleUserMode = 0;
  singleUserApi = 0xFFFFFFFF;
  m_connected_nodes.clear();
}

inline
NodeState::NodeState(StartLevel sl){
  init();
  startLevel = sl;
  singleUserMode = 0;
  singleUserApi = 0xFFFFFFFF;
}

inline
NodeState::NodeState(StartLevel sl, Uint32 sp, StartType typeOfStart){
  init();
  startLevel = sl;
  starting.startPhase = sp;
  starting.restartType = typeOfStart;
  singleUserMode = 0;
  singleUserApi = 0xFFFFFFFF;
}

inline
NodeState::NodeState(StartLevel sl, bool sys){
  init();
  startLevel = sl;
  stopping.systemShutdown = sys;
  singleUserMode = 0;
  singleUserApi = 0xFFFFFFFF;
}

inline
void NodeStatePOD::setDynamicId(Uint32 dynamic){
  dynamicId = dynamic;
}
  
inline
void NodeStatePOD::setNodeGroup(Uint32 group){
  nodeGroup = group;
}

inline 
void NodeStatePOD::setSingleUser(Uint32 s) {
  singleUserMode = s;
}

inline 
void NodeStatePOD::setSingleUserApi(Uint32 n) {
  singleUserApi = n;
}
inline 
bool NodeStatePOD::getNodeRestartInProgress() const {
  return startLevel == SL_STARTING && 
    (starting.restartType == ST_NODE_RESTART || 
     starting.restartType == ST_INITIAL_NODE_RESTART);
}

inline 
bool NodeStatePOD::getSingleUserMode() const {
  return singleUserMode;
}

inline 
Uint32 NodeStatePOD::getSingleUserApi() const {
  return singleUserApi;
}

inline 
bool NodeStatePOD::getSystemRestartInProgress() const {
  return startLevel == SL_STARTING && starting.restartType == ST_SYSTEM_RESTART;
}

inline
NdbOut &
operator<<(NdbOut& ndbout, const NodeStatePOD & state){
  ndbout << "[NodeState: startLevel: ";
  switch(state.startLevel){
  case NodeState::SL_NOTHING:
    ndbout << "<NOTHING> ]";
    break;
  case NodeState::SL_CMVMI:
    ndbout << "<CMVMI> ]";
    break;
  case NodeState::SL_STARTING:
    ndbout << "<STARTING type: ";
    switch(state.starting.restartType){
    case NodeState::ST_INITIAL_START:
      ndbout << " INITIAL START";
      break;
    case NodeState::ST_SYSTEM_RESTART:
      ndbout << " SYSTEM RESTART ";
      break;
    case NodeState::ST_NODE_RESTART:
      ndbout << " NODE RESTART ";
      break;
    case NodeState::ST_INITIAL_NODE_RESTART:
      ndbout << " INITIAL NODE RESTART ";
      break;
    case NodeState::ST_ILLEGAL_TYPE:
    default:
      ndbout << " UNKNOWN " << state.starting.restartType;
    }
    ndbout << " phase: " << state.starting.startPhase << "> ]";
    break;
  case NodeState::SL_STARTED:
    ndbout << "<STARTED> ]";
    break;
  case NodeState::SL_STOPPING_1:
    ndbout << "<STOPPING 1 sys: " << state.stopping.systemShutdown << "> ]";
    break;
  case NodeState::SL_STOPPING_2:
    ndbout << "<STOPPING 2 sys: " << state.stopping.systemShutdown << "> ]";
    break;
  case NodeState::SL_STOPPING_3:
    ndbout << "<STOPPING 3 sys: " << state.stopping.systemShutdown << "> ]";
    break;
  case NodeState::SL_STOPPING_4: 
    ndbout << "<STOPPING 4 sys: " << state.stopping.systemShutdown << "> ]";
    break;
  default:
    ndbout << "<UNKNOWN " << state.startLevel << "> ]";
  }
  return ndbout;
}

inline
NodeState&
NodeState::operator=(const NodeStatePOD& ns)
{
  startLevel = ns.startLevel;
  nodeGroup  = ns.nodeGroup;
  dynamicId  = ns.dynamicId;
  // masterNodeId is union with dynamicId
  starting.startPhase = ns.starting.startPhase;
  starting.restartType = ns.starting.restartType;
  // stopping.systemShutdown is union with starting.startPhase
  // stopping.timeout is union with starting.restartType
  stopping.alarmTime = ns.stopping.alarmTime;
  singleUserMode = ns.singleUserMode;
  singleUserApi  = ns.singleUserApi;
  m_connected_nodes.assign(ns.m_connected_nodes);
  return * this;
}
#endif
