/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include "Interval.hpp"
#include <rep/rep_version.hpp>
#include <Vector.hpp>
#include <ndb_limits.h>
#include <GrepError.hpp>


/**
 * Max number of requested epochs from PS
 */
#define GREP_SYSTEM_TABLE_MAX_RANGE 20

#define MAX_NO_OF_NODE_GROUPS 32

/**
 * This table struct is used in m_selectedTables
 */
struct table{
  table(const char * n) {strncpy(tableName, n, MAX_TAB_NAME_SIZE);}
  char tableName[MAX_TAB_NAME_SIZE];
};

/**
 * @class Channel
 * @brief Represents location of various epochs belonging to a subscription
 */
class Channel {
public:
  enum StateSub
  {
    NO_SUBSCRIPTION_EXISTS,

    CREATING_SUBSCRIPTION_ID,
    SUBSCRIPTION_ID_CREATED,

    STARTING_SUBSCRIPTION,
    SUBSCRIPTION_STARTED
  };

  enum StateRep
  {
    CONSISTENT,        ///< Consistent database.  Grep not running.
    METALOG_STARTING,  ///< Starting. Starting METALOG subscription
    METALOG_STARTED,         
    METASCAN_STARTING, ///< Starting. Starting METASCAN subscription
    METASCAN_COMPLETED,
    DATALOG_STARTING,  ///< Starting. Starting DATALOG subscription
    DATALOG_STARTED,
    DATASCAN_STARTING, ///< Starting. Starting DATASCAN subscription
    DATASCAN_COMPLETED,
    LOG,               ///< Started. Cons/Inconsistent. Grep running.
                       ///< All scan records have been applied.
    STOPPING           ///< Channel is stopping
  };

  /**
   *   Storage "positions" of Epochs
   */
  enum Position {
    PS = 0,            ///< Stored on Primary System REP
    SSReq = 1,         ///< Requested for transfer to Standby System
    SS = 2,            ///< Stored on Standby System REP
    AppReq = 3,        ///< Requested to be applied to Standby System
    App = 4,           ///< Has been applied to Standby System
    DelReq = 5,        ///< Has been requested to be deleted on PS REP & SS REP
    NO_OF_POSITIONS = 6
  }; //DONT FORGET TO ADD STUFF in position2Name if u add somehting here,

  /***************************************************************************
   * CONSTRUCTOR / DESTRUCTOR
   ***************************************************************************/
  Channel();
  ~Channel();

  /**
   *   Get and set no of nodegroups that actually exists on PS
   */
  void	 setNoOfNodeGroups(Uint32 n) { m_noOfNodeGroups = n; };
  Uint32 getNoOfNodeGroups()         { return m_noOfNodeGroups; };
  void getEpochState(Position p, 
		     Uint32 nodeGrp, 
		     Uint32 * first, 
		     Uint32 * last);
  Uint32 getEpochState(Position p, Uint32 nodegroup);
  bool m_requestorEnabled;  
  bool m_transferEnabled;   
  bool m_applyEnabled;    
  bool m_deleteEnabled;     
  bool m_autoStartEnabled;     
  
  /***************************************************************************
   * GETTERS and SETTERS
   ***************************************************************************/
  bool requestTransfer(Uint32 nodeGrp, Interval * i);
  bool requestApply(Uint32 nodeGrp, Uint32 * epoch);
  bool requestDelete(Uint32 nodeGrp, Interval * i);

  void add(Position pos, Uint32 nodeGrp, const Interval i);
  void clear(Position pos, Uint32 nodeGrp, const Interval i);

  void   setSubId(Uint32 subId)   { m_subId=subId; };
  Uint32 getSubId()               { return m_subId; };

  Uint32 getSubKey()              { return m_subKey; };
  void   setSubKey(Uint32 subKey) { m_subKey=subKey; };

  bool isSynchable(Uint32 nodeGrp);
  GrepError::Code  addTable(const char * tableName);
  GrepError::Code  removeTable(const char * tableName);
  void             printTables();
  bool             isSelective() {return m_selectedTables.size()>0;};
  Vector<struct table *> *  getSelectedTables();

  void reset();

  StateRep  getState()                { return m_stateRep; }
  void      setState(StateRep sr)     { m_stateRep = sr; }

  StateSub  getStateSub()             { return m_stateSub; }
  void      setStateSub(StateSub ss)  { m_stateSub = ss; }

  Interval  getMetaScanEpochs()           { return m_metaScanEpochs; }
  void      setMetaScanEpochs(Interval i) { m_metaScanEpochs = i; }
  Interval  getDataScanEpochs()           { return m_dataScanEpochs; }
  void      setDataScanEpochs(Interval i) { m_dataScanEpochs = i; }

  GrepError::Code  setStopEpochId(Uint32 n);
  Uint32           getStopEpochId()         { return m_stopEpochId; };

  bool isStoppable();
  bool shouldStop();

  bool subscriptionExists() { return (m_subId != 0 && m_subKey != 0); }

  /***************************************************************************
   * GETTERS
   ***************************************************************************/
  Uint32 getFirst(Position pos, Uint32 nodeGrp) { 
    return state[nodeGrp][pos].first(); 
  }

  Uint32 getLast(Position pos, Uint32 nodeGrp) { 
    return state[nodeGrp][pos].last(); 
  }

  void           getFullyAppliedEpochs(Interval * i);

  /***************************************************************************
   * PRINT METHODS
   ***************************************************************************/
  void print();
  void print(Position pos);
  void print(Position pos, Uint32 nodeGrp);
  void print(Uint32 nodeGrp);

  /***************************************************************************
   * PUBLIC ATTRIBUTES
   ***************************************************************************/

private:  
  /***************************************************************************
   * PRIVATE ATTRIBUTES
   ***************************************************************************/
  StateRep      m_stateRep;        // Replication state
  StateSub      m_stateSub;        // Subscription state

  Uint32        m_subId;
  Uint32        m_subKey;

  Uint32        m_noOfNodeGroups;  // Number of node grps in this channel
  Uint32        m_stopEpochId;     // Epoch id to stop subscription

  Interval      state[MAX_NO_OF_NODE_GROUPS][NO_OF_POSITIONS];

  Interval      m_metaScanEpochs;  
  Interval      m_dataScanEpochs;  

  
  Vector<struct table *> m_selectedTables;
  void invariant();                // Abort if channel metadata is inconsistent
  char * position2Name(Position p);
public:
  bool copy(Position from, Position to, Uint32 range, 
	    Uint32 * f, Uint32 * l, Uint32 nodeGrp);
};

#endif
