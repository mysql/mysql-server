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

#ifndef REP_STATE_HPP
#define REP_STATE_HPP

#include <GrepError.hpp>
#include <signaldata/GrepImpl.hpp>
#include <rep/repapi/repapi.h>
#include <rep/ExtSender.hpp>
#include <rep/adapters/AppNDB.hpp>
#include <Properties.hpp>

#include "Channel.hpp"
#include "Interval.hpp"

#define REQUESTOR_EXECUTES_NEEDED_FOR_UNKNOWN_CONNECTION 5

class NdbApiSignal;


/**
 * @class RepState
 * @brief The main information about the replication
 */
class RepState
{
public:
  RepState();
  ~RepState();
  void init(ExtSender * extSender) { m_extSender = extSender; }

  /***************************************************************************
   * Callback functions 
   *
   * These are used when RepState wants to do something
   ***************************************************************************/

  typedef void (FuncRequestCreateSubscriptionId) 
    (void * cbObj, NdbApiSignal* signal);
  
  typedef void (FuncRequestCreateSubscription) 
    (void * cbObj, NdbApiSignal* signal, Uint32 subId, 
     Uint32 subKey , 
     Vector <struct table *> * selectedTables);
  
  typedef void (FuncRequestRemoveSubscription) 
    (void * cbObj, NdbApiSignal* signal, Uint32 subId, Uint32 subKey);
    
  typedef void (FuncRequestTransfer) 
    (void * cbObj, NdbApiSignal* signal, 
     Uint32 nodeGrp, Uint32 first, Uint32 last);

  typedef void (FuncRequestApply)
    (void * cbObj, NdbApiSignal* signal, 
     Uint32 nodeGrp, Uint32 first, Uint32 last, Uint32 force); 

  typedef void (FuncRequestDeleteSS)
    (void * cbObj, NdbApiSignal* signal, 
     Uint32 nodeGrp, Uint32 first, Uint32 last); 

  typedef void (FuncRequestDeletePS)
    (void * cbObj, NdbApiSignal* signal, 
     Uint32 nodeGrp, Uint32 first, Uint32 last); 

  typedef void (FuncRequestStartMetaLog)
    (void * cbObj, NdbApiSignal * signal, Uint32 subId, Uint32 subKey);
  
  typedef void (FuncRequestStartDataLog)
    (void * cbObj, NdbApiSignal * signal, Uint32 subId, Uint32 subKey);

  typedef void (FuncRequestStartMetaScan)
    (void * cbObj, NdbApiSignal * signal, Uint32 subId, Uint32 subKey);    

  typedef void (FuncRequestStartDataScan)
    (void * cbObj, NdbApiSignal * signal, Uint32 subId, Uint32 subKey);

  typedef void (FuncRequestEpochInfo)
    (void * cbObj, NdbApiSignal * signal, Uint32 nodeGrp);

  /***************************************************************************
   *
   ***************************************************************************/
  void setSubscriptionRequests(FuncRequestCreateSubscriptionId f1,
			       FuncRequestCreateSubscription f2,
			       FuncRequestRemoveSubscription f3);
  void setIntervalRequests(FuncRequestTransfer * f1, 
			   FuncRequestApply * f2,
			   FuncRequestDeleteSS * f3, 
			   FuncRequestDeletePS * f4);
  void setStartRequests(FuncRequestStartMetaLog * f5,
			FuncRequestStartDataLog * f6,
			FuncRequestStartMetaScan * f7,
			FuncRequestStartDataScan * f8,
			FuncRequestEpochInfo * f9);

  /***************************************************************************
   * Enablings 
   ***************************************************************************/
  bool isEnabled()          { return m_channel.m_requestorEnabled; }
  bool isTransferEnabled()  { return m_channel.m_transferEnabled; }
  bool isApplyEnabled()     { return m_channel.m_applyEnabled; }
  bool isDeleteEnabled()    { return m_channel.m_deleteEnabled; }
  bool isAutoStartEnabled() { return m_channel.m_autoStartEnabled; }

  void enable()             { m_channel.m_requestorEnabled = true; }
  void enableTransfer()     { m_channel.m_transferEnabled = true; }
  void enableApply()        { m_channel.m_applyEnabled = true; }
  void enableDelete()       { m_channel.m_deleteEnabled = true; }
  void enableAutoStart()    { m_channel.m_autoStartEnabled = true; }

  void disable()            { m_channel.m_requestorEnabled = false; }
  void disableTransfer()    { m_channel.m_transferEnabled = false; }
  void disableApply()       { m_channel.m_applyEnabled = false;}
  void disableDelete()      { m_channel.m_deleteEnabled = false; }
  void disableAutoStart()   { m_channel.m_autoStartEnabled = false; }

  /***************************************************************************
   * Node groups
   ***************************************************************************/
  void   setNoOfNodeGroups(Uint32 n) { m_channel.setNoOfNodeGroups(n); }
  Uint32 getNoOfNodeGroups()         { return m_channel.getNoOfNodeGroups(); }

  /***************************************************************************
   * Event reporting to RepState
   *
   * These are used to update the state of the Requestor when something
   * has happend.
   ***************************************************************************/
  void request(GrepReq::Request request);

  //GrepError::Code  createSubscription(Uint32 subId, Uint32 subKey);
  GrepError::Code protectedExecute();
  GrepError::Code protectedRequest(GrepReq::Request request, Uint32 arg);
  GrepError::Code protectedRequest(GrepReq::Request request, 
				   Uint32 arg1, 
				   Uint32 arg2);
  GrepError::Code protectedAddTable(const char * fullTableName);
  GrepError::Code protectedRemoveTable(const char * fullTableName);
  GrepError::Code add(Channel::Position s, Uint32 nodeGrp, const Interval i);
  GrepError::Code clear(Channel::Position s, Uint32 nodeGrp, const Interval i);
  
  void eventSubscriptionDeleted(Uint32 subId, Uint32 subKey);

  void eventMetaLogStarted(NdbApiSignal*, Uint32 subId, Uint32 subKey);
  void eventDataLogStarted(NdbApiSignal*, Uint32 subId, Uint32 subKey);
  void eventMetaScanCompleted(NdbApiSignal*, Uint32 subId, Uint32 subKey,
			      Interval epochs);
  void eventDataScanCompleted(NdbApiSignal*, Uint32 subId, Uint32 subKey,
			      Interval epochs);
  void eventMetaScanFailed(Uint32 subId, Uint32 subKey, GrepError::Code error);
  void eventDataScanFailed(Uint32 subId, Uint32 subKey, GrepError::Code error);
                  
  /**
   * @fn sendInsertConf
   * @param	gci - the gci of the applied GCIBuffer.
   * @param	nodeGrp - the nodeGrp of the applied GCIBuffer.
   */
  void eventInsertConf(Uint32 gci, Uint32 nodeGrp);

  /**
   * @fn sendInsertRef
   * @param	gci - the gci of the applied GCIBuffer.
   * @param	nodeGrp - the nodeGrp of the applied GCIBuffer.
   * @param	tableId - the table of the applied GCIBuffer.
   */
  void eventInsertRef(Uint32 gci, Uint32 nodeGrp, Uint32 tableId,
		      GrepError::Code err);  
  void eventCreateTableRef(Uint32 gci,			  
			   Uint32 tableId, 
			   const char * tableName,
			   GrepError::Code err) ;

  void eventSubscriptionIdCreated(Uint32 subId, Uint32 subKey);
  void eventSubscriptionIdCreateFailed(Uint32 subId, Uint32 subKey, 
				       GrepError::Code error);

  void eventSubscriptionCreated(Uint32 subId, Uint32 subKey);
  void eventSubscriptionCreateFailed(Uint32 subId, Uint32 subKey,
				     GrepError::Code error);

  void eventMetaLogStartFailed(Uint32 subId, Uint32 subKey,
			       GrepError::Code error);
  void eventDataLogStartFailed(Uint32 subId, Uint32 subKey,
			       GrepError::Code error);
    
  void eventNodeConnected(Uint32 nodeId);
  void eventNodeDisconnected(Uint32 nodeId);
  void eventNodeConnectable(Uint32 nodeId);

  void printStatus();
  Properties * getStatus();
  Properties * query(QueryCounter counter, Uint32 replicationId);
  Uint32 getSubId()   { return m_channel.getSubId(); }
  Uint32 getSubKey () { return m_channel.getSubKey(); }

  void setApplier(class AppNDB * app) { m_applier = app; }
  void setGCIContainer(class GCIContainer * c) { m_gciContainer = c; }

  /* @todo should be private */
  Channel                  m_channel;

private:
  /***************************************************************************
   * PRIVATE ATTRIBUTES
   ***************************************************************************/
  ExtSender *              m_extSender;
  AppNDB *                 m_applier;
  GCIContainer *           m_gciContainer;

  Uint32                   m_subIdToRemove;
  Uint32                   m_subKeyToRemove;


  enum Connected 
  {
    UNKNOWN,           ///< 
    CONNECTED,         ///< Recently received info from (all needed) PS REP
    DISCONNECTED,      ///< Received disconnect info from (some needed) PS REP
    CONNECTABLE        ///< Received disconnect info from (some needed) PS REP
  };
  Connected                m_connected;
  Connected                m_repConnected;
  Uint32                   m_connected_counter;

  NdbMutex *               m_mutex;

  /** @todo Should be channel-specific */
  Uint32                   m_stopEpoch;

  /***************************************************************************
   * PRIVATE METHODS
   ***************************************************************************/
  GrepError::Code execute(NdbApiSignal*);
  GrepError::Code request(GrepReq::Request req, 
			  Uint32 arg1,
			  Uint32 arg2,
			  NdbApiSignal*);

  FuncRequestCreateSubscriptionId  * m_funcRequestCreateSubscriptionId;
  FuncRequestCreateSubscription    * m_funcRequestCreateSubscription;
  FuncRequestRemoveSubscription    * m_funcRequestRemoveSubscription;

  FuncRequestTransfer              * m_funcRequestTransfer;
  FuncRequestApply                 * m_funcRequestApply;
  FuncRequestDeleteSS              * m_funcRequestDeleteSS;
  FuncRequestDeletePS              * m_funcRequestDeletePS;

  FuncRequestStartMetaLog          * m_funcRequestStartMetaLog;
  FuncRequestStartDataLog          * m_funcRequestStartDataLog;
  FuncRequestStartMetaScan         * m_funcRequestStartMetaScan;
  FuncRequestStartDataScan         * m_funcRequestStartDataScan;
  FuncRequestEpochInfo             * m_funcRequestEpochInfo;

  void requestTransfer(NdbApiSignal * signal);
  void requestApply(NdbApiSignal * signal);
  void requestDelete(NdbApiSignal * signal);
  void requestEpochInfo(NdbApiSignal * signal);
  void getEpochState(Channel::Position pos, Properties * p);
  friend void testRepState();
};

#endif
