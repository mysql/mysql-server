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

#include "RepState.hpp"

#include <signaldata/SumaImpl.hpp>
#include <NdbApiSignal.hpp>
#include <Properties.hpp>
//#define DBUG_REQUESTOR

#ifdef DBUG_REQUESTOR
#define DBUG_REQUESTOR_PRINT(X) ndbout_c(X);
#else
#define DBUG_REQUESTOR_PRINT(X)
#endif

/****************************************************************************
 * Constructor / Destructor / Init
 ****************************************************************************/
RepState::RepState() 
{
  m_connected = UNKNOWN;
  m_repConnected = UNKNOWN;
  m_mutex = NdbMutex_Create();
  m_stopEpoch = 0;
  m_subIdToRemove = 0;
  m_subKeyToRemove = 0;
}

RepState::~RepState() 
{
  NdbMutex_Destroy(m_mutex);
}

void
RepState::setSubscriptionRequests(FuncRequestCreateSubscriptionId f1,
				  FuncRequestCreateSubscription f2,
				  FuncRequestRemoveSubscription f3)
{
  m_funcRequestCreateSubscriptionId = f1;
  m_funcRequestCreateSubscription = f2;
  m_funcRequestRemoveSubscription = f3;
}

void
RepState::setIntervalRequests(FuncRequestTransfer f1, 
			      FuncRequestApply f2,
			      FuncRequestDeleteSS f3, 
			      FuncRequestDeletePS f4)
{
  m_funcRequestTransfer = f1;
  m_funcRequestApply = f2;
  m_funcRequestDeleteSS = f3;
  m_funcRequestDeletePS = f4;
}

void
RepState::setStartRequests(FuncRequestStartMetaLog * f5,
			   FuncRequestStartDataLog * f6,
			   FuncRequestStartMetaScan * f7,
			   FuncRequestStartDataScan * f8, 
			   FuncRequestEpochInfo * f9) 
{
  m_funcRequestStartMetaLog = f5;
  m_funcRequestStartDataLog = f6;
  m_funcRequestStartMetaScan = f7;
  m_funcRequestStartDataScan = f8;
  m_funcRequestEpochInfo = f9;
}


/****************************************************************************
 * Private Helper functions
 ****************************************************************************/

void 
RepState::requestTransfer(NdbApiSignal * signal) 
{
  DBUG_REQUESTOR_PRINT("RepState: Transfer calculations started");
  for(Uint32 nodeGrp=0; nodeGrp<m_channel.getNoOfNodeGroups(); nodeGrp++) {
    DBUG_REQUESTOR_PRINT("RepState: Transfer calc for node grp");
    Interval i;
    if (m_channel.requestTransfer(nodeGrp, &i)) {
      m_funcRequestTransfer(m_extSender, signal, nodeGrp, i.first(), i.last());
    }
  }
}

void
RepState::requestApply(NdbApiSignal * signal) 
{
  DBUG_REQUESTOR_PRINT("RepState: Apply calculations started");
  for(Uint32 nodeGrp=0; nodeGrp<m_channel.getNoOfNodeGroups(); nodeGrp++) {
    DBUG_REQUESTOR_PRINT("RepState: Apply calc for node grp");
    Uint32 gci;
    if (m_channel.requestApply(nodeGrp, &gci)) {
      Uint32 force = (m_channel.getState() == Channel::LOG) ? 0 : 1;
      m_funcRequestApply(m_applier, signal, nodeGrp, gci, gci, force);
    }
  }
}

void
RepState::requestDelete(NdbApiSignal * signal) 
{
  DBUG_REQUESTOR_PRINT("RepState: Delete calculations started");
  for(Uint32 nodeGrp=0; nodeGrp<m_channel.getNoOfNodeGroups(); nodeGrp++) {
    DBUG_REQUESTOR_PRINT("RepState: Delete calc for node grp");
    Interval i;
    if (m_channel.requestDelete(nodeGrp, &i)){
      m_funcRequestDeleteSS(m_gciContainer, signal, nodeGrp, 
			    i.first(), i.last());
      m_funcRequestDeletePS(m_extSender, signal, nodeGrp, i.first(), i.last());
    }
  }
}

void
RepState::requestEpochInfo(NdbApiSignal * signal) 
{
  DBUG_REQUESTOR_PRINT("RepState: Epoch Info calculations");
  for(Uint32 nodeGrp=0; nodeGrp<m_channel.getNoOfNodeGroups(); nodeGrp++) {
    m_funcRequestEpochInfo(m_extSender, signal, nodeGrp);
  }
}

/****************************************************************************
 * Public 
 ****************************************************************************/

GrepError::Code
RepState::add(Channel::Position s, Uint32 nodeGrp, const Interval i) 
{
  m_channel.add(s, nodeGrp, i);

  if(s == Channel::PS) 
  {
    m_connected = CONNECTED;
    m_connected_counter = 0;
  }

  Interval fullEpochs;
  m_channel.getFullyAppliedEpochs(&fullEpochs);
  if(s == Channel::App &&
     m_channel.getState() == Channel::DATASCAN_COMPLETED && 
     fullEpochs.last() >= m_channel.getDataScanEpochs().last() &&
     fullEpochs.last() >= m_channel.getMetaScanEpochs().last())
  {
    RLOG(("[%d-%d] fully applied. Channel state changed to LOG",
	  fullEpochs.first(), fullEpochs.last()));
    m_channel.setState(Channel::LOG);
    disableAutoStart();
  }

  return GrepError::NO_ERROR;
}

GrepError::Code 
RepState::clear(Channel::Position s, Uint32 nodeGrp, const Interval i) 
{
  m_channel.clear(s, nodeGrp, i);
  return GrepError::NO_ERROR;
}

/****************************************************************************
 * Execute 
 * 
 * This method should only be called from Requestor!
 ****************************************************************************/

GrepError::Code 
RepState::protectedExecute()
{
  GrepError::Code err;
  
  NdbMutex_Lock(m_mutex);
  
  NdbApiSignal* signal = m_extSender->getSignal();
  if (signal == NULL) {
    err = GrepError::COULD_NOT_ALLOCATE_MEM_FOR_SIGNAL;
  } else {
    err = execute(signal);
  }
  NdbMutex_Unlock(m_mutex);
  return err;
}

GrepError::Code 
RepState::execute(NdbApiSignal* signal)
{
  Uint32 subId = m_channel.getSubId();
  Uint32 subKey = m_channel.getSubKey();

  if (!m_channel.m_requestorEnabled) 
    return GrepError::NO_ERROR;

  /**
   * @todo Should have subscriptions in here
   */
  requestEpochInfo(signal);

  /**
   * Update connected counter (Silence time)
   */
  m_connected_counter++;
  if (m_connected_counter > REQUESTOR_EXECUTES_NEEDED_FOR_UNKNOWN_CONNECTION) {
    m_connected = UNKNOWN;
  }

  switch (m_channel.getState()) 
  {
  case Channel::CONSISTENT:
    if (isAutoStartEnabled()) {
      switch (m_channel.getStateSub()) 
      {
      case Channel::NO_SUBSCRIPTION_EXISTS:
	m_funcRequestCreateSubscriptionId(m_extSender, signal);  
	m_channel.setStateSub(Channel::CREATING_SUBSCRIPTION_ID);
	break;

      case Channel::CREATING_SUBSCRIPTION_ID:
	break;

      case Channel::SUBSCRIPTION_ID_CREATED:
	if(m_channel.isSelective())
          m_funcRequestCreateSubscription(m_extSender, signal,
                                          m_channel.getSubId(),
                                          m_channel.getSubKey(),
                                          m_channel.getSelectedTables());
        else
          m_funcRequestCreateSubscription(m_extSender, signal,
                                          m_channel.getSubId(),
                                          m_channel.getSubKey(),
                                          0);
	m_channel.setStateSub(Channel::STARTING_SUBSCRIPTION);
	break;

      case Channel::STARTING_SUBSCRIPTION:
	break;

      case Channel::SUBSCRIPTION_STARTED:
	m_funcRequestStartMetaLog(m_extSender, signal, 
				  m_channel.getSubId(),
				  m_channel.getSubKey());
	m_channel.setState(Channel::METALOG_STARTING);
	break;
      } 
    }
    break;

  case Channel::METALOG_STARTING:
    break;

  case Channel::METALOG_STARTED:
    if (isAutoStartEnabled()) {
      m_funcRequestStartMetaScan(m_extSender, signal, subId, subKey);
      m_channel.setState(Channel::METASCAN_STARTING);
    }
    break;

  case Channel::METASCAN_STARTING:
    break;

  case Channel::METASCAN_COMPLETED:
    if (isAutoStartEnabled()) {
      m_funcRequestStartDataLog(m_extSender, signal, subId, subKey);
      m_channel.setState(Channel::DATALOG_STARTING);
    }
    break;

  case Channel::DATALOG_STARTING:
    break;

  case Channel::DATALOG_STARTED:
    if (isAutoStartEnabled()) {
      m_funcRequestStartDataScan(m_extSender, signal, subId, subKey);
      m_channel.setState(Channel::DATASCAN_STARTING);
    }
    break;

  case Channel::DATASCAN_STARTING:
    break;

  case Channel::DATASCAN_COMPLETED:
    break;

  case Channel::LOG:
    if (m_channel.shouldStop()) {
      disableTransfer();
      m_channel.setState(Channel::STOPPING);
    }
    break;

  case Channel::STOPPING:
    if (m_channel.m_transferEnabled) 
    {
      REPABORT("Illegal stopping state while transfer is still enabled");
    }
    /**
     * check if channel has a subscription, if not,
     * check if we have marked a subscription that we want to remove
     * and remove it. This is used to clean up "dangling subscriptions"
     * after various crashes
     */
    if(!m_channel.subscriptionExists())
    { 
      if(m_subIdToRemove && m_subKeyToRemove) 
      {
	m_funcRequestRemoveSubscription(m_extSender, signal, 
					m_subIdToRemove, 
					m_subKeyToRemove);
	eventSubscriptionDeleted( m_subIdToRemove, 
				  m_subKeyToRemove);
	return GrepError::NO_ERROR;
      }  
      else {
	return GrepError::SUBSCRIPTION_ID_NOT_FOUND;
      }
    } else {
      if (m_channel.isStoppable())
	{
	  
	  m_funcRequestRemoveSubscription(m_extSender, signal, 
					  m_channel.getSubId(), 
					  m_channel.getSubKey());
	  eventSubscriptionDeleted(m_channel.getSubId(), 
				   m_channel.getSubKey());  
	}
      else 
	return GrepError::CHANNEL_NOT_STOPPABLE;
      
    }
    break;

  default:
    REPABORT("Illegal replication state");
  }
  if (m_channel.m_transferEnabled)  requestTransfer(signal);
  if (m_channel.m_applyEnabled)     requestApply(signal);
  if (m_channel.m_deleteEnabled)    requestDelete(signal); 
  return GrepError::NO_ERROR;
}

/****************************************************************************
 * Request
 * 
 * This method should only be called from Main Thread!
 ****************************************************************************/

GrepError::Code 
RepState::protectedRequest(GrepReq::Request req, Uint32 arg)
{
  return protectedRequest(req, arg, 0);
}

GrepError::Code 
RepState::protectedRequest(GrepReq::Request req, Uint32 arg1, Uint32 arg2)
{
  GrepError::Code code;
  NdbMutex_Lock(m_mutex);

  NdbApiSignal* signal = m_extSender->getSignal();
  if (signal == NULL) {
    code = GrepError::COULD_NOT_ALLOCATE_MEM_FOR_SIGNAL;
  } else {
    code = request(req, arg1, arg2, signal);
  }

  NdbMutex_Unlock(m_mutex);
  return code;
}

GrepError::Code
RepState::protectedAddTable(const char * fullTableName)
{
  GrepError::Code code;
  NdbMutex_Lock(m_mutex);
  code  =  m_channel.addTable(fullTableName);
  NdbMutex_Unlock(m_mutex);
  return code;
}

GrepError::Code
RepState::protectedRemoveTable(const char * fullTableName)
{
  GrepError::Code code;
  if(m_channel.getStateSub() !=  Channel::NO_SUBSCRIPTION_EXISTS)
    return GrepError::START_ALREADY_IN_PROGRESS;
  NdbMutex_Lock(m_mutex);
  code  =  m_channel.removeTable(fullTableName);
  NdbMutex_Unlock(m_mutex);
  return code;
}

GrepError::Code 
RepState::request(GrepReq::Request request, Uint32 arg1, Uint32 arg2, 
		  NdbApiSignal* signal) 
{
  switch (request) 
  {
    /*************************************************************************
     * STATUS etc
     *************************************************************************/

  case GrepReq::STATUS:
    printStatus();
    break;

  case GrepReq::REMOVE_BUFFERS:
    return GrepError::NOT_YET_IMPLEMENTED;
    
    /*************************************************************************
     * START
     *************************************************************************/

  case GrepReq::CREATE_SUBSCR:
    if (m_channel.getStateSub() != Channel::NO_SUBSCRIPTION_EXISTS) 
      return GrepError::SUBSCRIPTION_ID_ALREADY_EXIST;

    m_funcRequestCreateSubscriptionId(m_extSender, signal);  
    m_channel.setStateSub(Channel::CREATING_SUBSCRIPTION_ID);
    return GrepError::NO_ERROR;

  case GrepReq::START_SUBSCR:
    if (m_channel.getState() == Channel::STOPPING)
      return GrepError::ILLEGAL_ACTION_WHEN_STOPPING;
    if (m_channel.getStateSub() != Channel::SUBSCRIPTION_ID_CREATED)
      return GrepError::SUBSCRIPTION_ID_NOT_FOUND;
    if(m_channel.isSelective())
      m_funcRequestCreateSubscription(m_extSender, signal,
                                      m_channel.getSubId(),
                                      m_channel.getSubKey(),
                                      m_channel.getSelectedTables());
    else
      m_funcRequestCreateSubscription(m_extSender, signal,
                                      m_channel.getSubId(),
                                      m_channel.getSubKey(),
                                      0);
    m_channel.setStateSub(Channel::STARTING_SUBSCRIPTION);
    return GrepError::NO_ERROR;

  case GrepReq::START_METALOG:
    if (m_channel.getState() == Channel::STOPPING)
      return GrepError::ILLEGAL_ACTION_WHEN_STOPPING;
    if (m_channel.getStateSub() != Channel::SUBSCRIPTION_STARTED)
      return GrepError::SUBSCRIPTION_NOT_STARTED;
    if (m_channel.getState() != Channel::CONSISTENT)
      return GrepError::START_OF_COMPONENT_IN_WRONG_STATE;
    
    m_funcRequestStartMetaLog(m_extSender, signal, 
			      m_channel.getSubId(), 
			      m_channel.getSubKey());  
    m_channel.setState(Channel::METALOG_STARTING);
    return GrepError::NO_ERROR;

  case GrepReq::START_METASCAN:
    if (m_channel.getState() == Channel::STOPPING)
      return GrepError::ILLEGAL_ACTION_WHEN_STOPPING;
    if (m_channel.getStateSub() != Channel::SUBSCRIPTION_STARTED)
      return GrepError::SUBSCRIPTION_NOT_STARTED;
    if (m_channel.getState() != Channel::METALOG_STARTED)
      return GrepError::START_OF_COMPONENT_IN_WRONG_STATE;
 
    m_funcRequestStartMetaScan(m_extSender, signal,
			       m_channel.getSubId(), 
			       m_channel.getSubKey());  
    m_channel.setState(Channel::METASCAN_STARTING);
    return GrepError::NO_ERROR;

  case GrepReq::START_DATALOG:
    if (m_channel.getState() == Channel::STOPPING)
      return GrepError::ILLEGAL_ACTION_WHEN_STOPPING;
    if (m_channel.getStateSub() != Channel::SUBSCRIPTION_STARTED)
      return GrepError::SUBSCRIPTION_NOT_STARTED;
    if (m_channel.getState() != Channel::METASCAN_COMPLETED)
      return GrepError::START_OF_COMPONENT_IN_WRONG_STATE;

    m_funcRequestStartDataLog(m_extSender, signal,
			      m_channel.getSubId(), 
			      m_channel.getSubKey());  
    m_channel.setState(Channel::DATALOG_STARTING);
    return GrepError::NO_ERROR;

  case GrepReq::START_DATASCAN:
    if (m_channel.getState() == Channel::STOPPING)
      return GrepError::ILLEGAL_ACTION_WHEN_STOPPING;
    if (m_channel.getStateSub() != Channel::SUBSCRIPTION_STARTED)
      return GrepError::SUBSCRIPTION_NOT_STARTED;
    if (m_channel.getState() != Channel::DATALOG_STARTED)
      return GrepError::START_OF_COMPONENT_IN_WRONG_STATE;

    m_funcRequestStartDataScan(m_extSender, signal,
			      m_channel.getSubId(), 
			      m_channel.getSubKey());  
    m_channel.setState(Channel::DATASCAN_STARTING);
    return GrepError::NO_ERROR;

  case GrepReq::START_REQUESTOR:
    enable();
    return GrepError::NO_ERROR;
    
  case GrepReq::START_TRANSFER:
    if (m_channel.getState() == Channel::STOPPING)
      return GrepError::ILLEGAL_ACTION_WHEN_STOPPING;
    enableTransfer();
    return GrepError::NO_ERROR;

  case GrepReq::START_APPLY:
    enableApply();
    return GrepError::NO_ERROR;

  case GrepReq::START_DELETE:
    enableDelete();
    return GrepError::NO_ERROR;

  case GrepReq::START:
    if (isAutoStartEnabled())
      return GrepError::START_ALREADY_IN_PROGRESS;

    enableAutoStart();
    return GrepError::NO_ERROR;
    
    /*************************************************************************
     * STOP
     *************************************************************************/

  case GrepReq::STOP:
    if (m_channel.getStateSub() == Channel::NO_SUBSCRIPTION_EXISTS)
      return GrepError::SUBSCRIPTION_NOT_STARTED;
    if (m_channel.getState() == Channel::STOPPING)
      return GrepError::ILLEGAL_ACTION_WHEN_STOPPING;

    if (arg1 == 0) { 
      /**
       * Stop immediately
       */
      disableTransfer();
      m_channel.setState(Channel::STOPPING);
      m_channel.setStopEpochId(0);
      return GrepError::NO_ERROR;
    } else {
      /**
       * Set future stop epoch
       */
      return m_channel.setStopEpochId(arg1);
    }

  case GrepReq::STOP_SUBSCR:
    {
      if(m_subIdToRemove == 0 && m_subKeyToRemove == 0) {
	  m_subIdToRemove   = arg1;
	  m_subKeyToRemove  = arg2;
      } else {
	return GrepError::ILLEGAL_ACTION_WHEN_STOPPING;
      }
      
      if(m_channel.getSubId() != 0 && m_channel.getSubKey() != 0)
	return GrepError::ILLEGAL_USE_OF_COMMAND;
      if (m_channel.getState() == Channel::STOPPING)
	return GrepError::ILLEGAL_ACTION_WHEN_STOPPING;
      disableTransfer();
      m_channel.setState(Channel::STOPPING);
      return GrepError::NO_ERROR;
    }
  case GrepReq::STOP_METALOG:
  case GrepReq::STOP_METASCAN:
  case GrepReq::STOP_DATALOG:
  case GrepReq::STOP_DATASCAN:
    return GrepError::NOT_YET_IMPLEMENTED;

  case GrepReq::STOP_REQUESTOR:
    disable();
    return GrepError::NO_ERROR;
    
  case GrepReq::STOP_TRANSFER:
    disableTransfer();
    return GrepError::NO_ERROR;

  case GrepReq::STOP_APPLY:
    disableApply();
    return GrepError::NO_ERROR;

  case GrepReq::STOP_DELETE:
    disableDelete();
    return GrepError::NO_ERROR;

  default:
    ndbout_c("RepCommandInterpreter: Illegal request received");
    return GrepError::NOT_YET_IMPLEMENTED;
  }
  return GrepError::NOT_YET_IMPLEMENTED;
}

/****************************************************************************
 * 
 ****************************************************************************/

/*
GrepError::Code
RepState::slowStop() 
{
  switch(m_channel.getState()) 
  {
  case Channel::LOG:
    m_channel.setState(Channel::LOG_SLOW_STOP);
    return GrepError::NO_ERROR;
  default:
    return GrepError::REQUESTOR_ILLEGAL_STATE_FOR_SLOWSTOP;
  }
}

GrepError::Code
RepState::fastStop() 
{
  switch(m_channel.getState()) 
  {
  case Channel::LOG:
    m_channel.setState(Channel::LOG_FAST_STOP);
    return GrepError::NO_ERROR;
  default:
    return GrepError::REQUESTOR_ILLEGAL_STATE_FOR_FASTSTOP;
  }
}
*/

/****************************************************************************
 * Print Status
 ****************************************************************************/

static const char* 
headerText =
"+-------------------------------------------------------------------------+\n"
"|                         MySQL Replication Server                        |\n"
"+-------------------------------------------------------------------------+\n"
;

static const char* 
channelHeaderText =
"+-------------------------------------------------------------------------+\n"
"|                   Applier Channel 1 Replication Status                  |\n"
"+-------------------------------------------------------------------------+\n"
;

static const char* 
line =
"+-------------------------------------------------------------------------+\n"
;


Properties *
RepState::getStatus() 
{
  Properties * prop = new Properties();
  if(prop == NULL)
    return NULL;
  NdbMutex_Lock(m_mutex);

  prop->put("nodegroups", (int)m_channel.getNoOfNodeGroups());
//  prop->put("epoch_state", m_channel.getEpochState());
  NdbMutex_Unlock(m_mutex);
  return prop;
}


Properties * RepState::query(QueryCounter counter, Uint32 replicationId) 
{
  Properties * prop = new Properties();
  if(prop == NULL)
    return NULL;
  NdbMutex_Lock(m_mutex);
  if(counter != ~(Uint32)0)
    getEpochState((Channel::Position)counter, prop );
  prop->put("no_of_nodegroups", m_channel.getNoOfNodeGroups());
  prop->put("subid", m_channel.getNoOfNodeGroups());
  prop->put("subkey", m_channel.getSubKey());
  prop->put("connected_db", m_connected);
  prop->put("connected_rep", m_repConnected);
  prop->put("state_sub", (int)m_channel.getStateSub());
  prop->put("state", (int)m_channel.getState());    
  
  NdbMutex_Unlock(m_mutex);
  return prop;
  
}

void
RepState::getEpochState(Channel::Position pos, Properties * p)
{
  char first_buf[20];
  char last_buf[20];
  int pos_first = 0, pos_last = 0;
  Uint32 first = 0, last = 0;
  for(Uint32 i = 0; i < m_channel.getNoOfNodeGroups() ; i++) 
  {    
    m_channel.getEpochState(pos, i, &first, &last);
    pos_first += sprintf(first_buf+pos_first,"%d%s",first,",");
    pos_last  += sprintf(last_buf+pos_last,"%d%s",last,",");    
  }
/**
 * remove trailing comma
 */
  pos_first--;
  pos_last--;
  first_buf[pos_first]= '\0';
  last_buf[pos_last]= '\0';
#if 0
  sprintf(first_buf+pos_first,"","");
  sprintf(last_buf + pos_last,"","");    
#endif

  p->put("first", first_buf);
  p->put("last", last_buf);
  
}


void
RepState::printStatus() 
{
  /***************************************************************************
   * Global Status
   ***************************************************************************/
  ndbout << headerText;
  switch (m_connected)
  {
  case CONNECTED: 
    ndbout << "| Source:        Connected    "; break;
  case DISCONNECTED:
    ndbout << "| Source:        Disconnected "; break;
  case CONNECTABLE:
    ndbout << "| Source:        Disconnected "; break;
  default:
    ndbout << "| Source:        Unknown      "; break;
  }
  switch (m_repConnected)
  {
  case CONNECTED:
    ndbout << "(Rep: Connected)       "; break;
  case DISCONNECTED:
    ndbout << "(Rep: Disconnected)    "; break;
  case CONNECTABLE:
    ndbout << "(Rep: Disconnected)    "; break;
  default:
    ndbout << "(Rep: Unknown)         "; break;
  }
  ndbout << "                     |" << endl;
  ndbout << "| Autostart:     " << (isAutoStartEnabled() ? "On " : "Off")
	 << "                          ";
  ndbout_c("  Silence time:  %10u |", m_connected_counter);

  /***************************************************************************
   * Channel Status
   ***************************************************************************/
  ndbout << channelHeaderText;
  switch(m_channel.getStateSub()) {
  case Channel::NO_SUBSCRIPTION_EXISTS:
    ndbout_c("| Subscription:  Non-existing                      "
	     "                       |");
    break;
  case Channel::CREATING_SUBSCRIPTION_ID:
    ndbout_c("| Subscription:  Non-existing (Id is being created)"
	     "                       |");
    break;
  case Channel::SUBSCRIPTION_ID_CREATED:
    ndbout_c("| Subscription:  %-3d-%-6d in state: Not yet started     "
	     "                |", 
	     m_channel.getSubId(), m_channel.getSubKey());
    break;
  case Channel::STARTING_SUBSCRIPTION:
    ndbout_c("| Subscription:  %-3d-%-6d in state: Being started       "
	     "                |", 
	     m_channel.getSubId(), m_channel.getSubKey());
    break;
  case Channel::SUBSCRIPTION_STARTED:
    ndbout_c("| Subscription:  %-3d-%-6d in state: Started             "
	     "                |", 
	     m_channel.getSubId(), m_channel.getSubKey());
    break;
  default:
    REPABORT("Illegal subscription state");
  }
  ndbout << "| Stop epoch:    ";
  if (m_channel.getStopEpochId() == intervalMax) {
    ndbout << "No stop defined                             "; 
  } else {
    ndbout.print("%-10d                                  ",
		 m_channel.getStopEpochId());
  }
  ndbout << "             |" << endl;
  
  ndbout << "| State:         ";
  switch(m_channel.getState()) 
  {
  case Channel::CONSISTENT:     
    ndbout << "Local database is subscription consistent   "; 
    break;
  case Channel::METALOG_STARTING:
    ndbout << "Starting (Phase 1: Metalog starting)        "; 
    break;
  case Channel::METALOG_STARTED:
    ndbout << "Starting (Phase 2: Metalog started)         "; 
    break;
  case Channel::METASCAN_STARTING:
    ndbout << "Starting (Phase 3: Metascan starting)       "; 
    break;
  case Channel::METASCAN_COMPLETED:
    ndbout << "Starting (Phase 4: Metascan completed)      "; 
    break;
  case Channel::DATALOG_STARTING:
    ndbout << "Starting (Phase 5: Datalog starting)        "; 
    break;
  case Channel::DATALOG_STARTED:
    ndbout << "Starting (Phase 6: Datalog started)         "; 
    break;
  case Channel::DATASCAN_STARTING:
    ndbout << "Starting (Phase 7: Datascan completed)      "; 
    break;
  case Channel::DATASCAN_COMPLETED:
    ndbout << "Starting (Phase 8: Datascan completed)      "; 
    break;
  case Channel::LOG:            
    ndbout << "Logging                                     "; 
    break;
  case Channel::STOPPING:
    ndbout << "Stopping (Stopped when all epochs applied)  ";
    break;
  }
  ndbout << "             |" << endl;

/* @todo
  ndbout_c("| Syncable:      Yes/Scan/No/Unknown (Not implemented)"
	   "                    |");
*/
  ndbout << "| Requestor:     " << (isEnabled() ? "On " : "Off") 
	 << " (Transfer: " << (isTransferEnabled() ? "On,  " : "Off, ")
	 << "Apply: " << (isApplyEnabled() ? "On,  " : "Off, ")
	 << "Delete: " << (isDeleteEnabled() ? "On) " : "Off)")
	 << "             |" << endl;
  ndbout_c("| Tables being replicated using this channel:         "
	   "                    |");
  m_channel.printTables();

  /**
   * Print node groups
   */
  if (getNoOfNodeGroups() == 0) 
  {
    ndbout_c("| No node groups are known.           "
	     "                                    |");
  } 
  else 
  {
    m_channel.print();
  }
  ndbout << line;
}
