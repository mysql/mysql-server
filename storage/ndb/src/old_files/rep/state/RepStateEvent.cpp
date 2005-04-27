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

/****************************************************************************
 * Public Event Handlers : CREATE SUBSCRIPTION ID
 ****************************************************************************/

void 
RepState::eventSubscriptionIdCreated(Uint32 subId, Uint32 subKey)
{
  if (m_channel.getStateSub() == Channel::CREATING_SUBSCRIPTION_ID)
  {
    m_channel.setSubId(subId);
    m_channel.setSubKey(subKey);
    m_channel.setStateSub(Channel::SUBSCRIPTION_ID_CREATED);
  }
  else 
  {
    REPABORT("Illegal state for create subscription id conf");
  }
}

void 
RepState::eventSubscriptionIdCreateFailed(Uint32 subId, Uint32 subKey, 
					  GrepError::Code error)
{
  ndbout_c("\nSubscription id creation failed");
  ndbout_c("Error %d: %s", error, GrepError::getErrorDesc(error));
  ndbout_c("Subscription Id: %d, Key: %d", subId, subKey);
}

/****************************************************************************
 * Public Event Handlers : CREATE SUBSCRIPTION
 ****************************************************************************/

void
RepState::eventSubscriptionCreated(Uint32 subId, Uint32 subKey)
{
  if (m_channel.getStateSub() == Channel::STARTING_SUBSCRIPTION)
  {
    m_channel.setStateSub(Channel::SUBSCRIPTION_STARTED);
  }
  else 
  {
    REPABORT("Illegal state for create subscription conf");
  }
}

void
RepState::eventSubscriptionCreateFailed(Uint32 subId, Uint32 subKey,
					GrepError::Code error)
{
  ndbout_c("\nSubscription creation failed");
  ndbout_c("Error %d: %s", error, GrepError::getErrorDesc(error));
  ndbout_c("Subscription Id: %d, Key: %d", subId, subKey);
}


/****************************************************************************
 * Public Event Handlers : META LOG
 ****************************************************************************/

void 
RepState::eventMetaLogStarted(NdbApiSignal* signal, 
			      Uint32 subId, Uint32 subKey)
{
  if (m_channel.getState() != Channel::METALOG_STARTING) 
  {
    RLOG(("WARNING! Metalog started in state %d, should be %d",
	  m_channel.getState(), Channel::METALOG_STARTING));
  }
  
  if (!isAutoStartEnabled())
  {
    m_channel.setState(Channel::METALOG_STARTED);
  }
  else 
  {
    m_channel.setState(Channel::METALOG_STARTED);
    m_channel.setState(Channel::METASCAN_STARTING);
    m_funcRequestStartMetaScan(m_extSender, signal, subId, subKey);
  }
}

void 
RepState::eventMetaLogStartFailed(Uint32 subId, Uint32 subKey, 
				  GrepError::Code error)
{
  ndbout_c("\nMetalog start failed");
  ndbout_c("Error %d: %s", error, GrepError::getErrorDesc(error));
  ndbout_c("Subscription Id: %d, Key: %d", subId, subKey);
}

/****************************************************************************
 * Public Event Handlers : META SCAN
 ****************************************************************************/

void 
RepState::eventMetaScanCompleted(NdbApiSignal* signal, 
				 Uint32 subId, Uint32 subKey, Interval epochs) 
{
  if (m_channel.getState() != Channel::METASCAN_STARTING) 
  {
    RLOG(("WARNING! Metascan completed in state %d, should be %d",
	  m_channel.getState(), Channel::METASCAN_STARTING));
  }
  RLOG(("Metascan completed. Subscription %d-%d, Epochs [%d-%d]",
	subId, subKey, epochs.first(), epochs.last()));

  m_channel.setState(Channel::METASCAN_COMPLETED);
  
  if (isAutoStartEnabled())
  {
    m_channel.setState(Channel::DATALOG_STARTING);
    m_funcRequestStartDataLog(m_extSender, signal, subId, subKey);
  }
  m_channel.setMetaScanEpochs(epochs);
}

/****************************************************************************
 * Public Event Handlers : DATA LOG
 ****************************************************************************/

void 
RepState::eventDataLogStarted(NdbApiSignal* signal,
			      Uint32 subId, Uint32 subKey)
{
  if (m_channel.getState() != Channel::DATALOG_STARTING) 
  {
    RLOG(("WARNING! Datalog started in state %d, should be %d",
	  m_channel.getState(), Channel::DATALOG_STARTING));
  }

  m_channel.setState(Channel::DATALOG_STARTED);

  if (isAutoStartEnabled())
  {
    m_channel.setState(Channel::DATASCAN_STARTING);
    m_funcRequestStartDataScan(m_extSender, signal, subId, subKey);
  }
}

void 
RepState::eventDataLogStartFailed(Uint32 subId, Uint32 subKey, 
				  GrepError::Code error)
{
  ndbout_c("\nDatalog start failed");
  ndbout_c("Error %d: %s", error, GrepError::getErrorDesc(error));
  ndbout_c("Subscription Id: %d, Key: %d", subId, subKey);
}

/****************************************************************************
 * Public Event Handlers : DATA SCAN
 ****************************************************************************/

void 
RepState::eventDataScanCompleted(NdbApiSignal* signal, 
				 Uint32 subId, Uint32 subKey,
				 Interval epochs) 
{
  if (m_channel.getState() != Channel::DATASCAN_STARTING) 
  {
    RLOG(("WARNING! Datascan completed in state %d, should be %d",
	  m_channel.getState(), Channel::DATASCAN_STARTING));
  }
  RLOG(("Datascan completed. Subscription %d-%d, Epochs [%d-%d]",
	subId, subKey, epochs.first(), epochs.last()));
  
  m_channel.setState(Channel::DATASCAN_COMPLETED);
  m_channel.setDataScanEpochs(epochs);
}

/****************************************************************************
 * Public Event Handlers : FAILURES
 ****************************************************************************/

void
RepState::eventMetaScanFailed(Uint32 subId, Uint32 subKey,
			     GrepError::Code error)
{
  ndbout_c("\nMetascan failed");
  ndbout_c("Error %d: %s", error, GrepError::getErrorDesc(error));
  ndbout_c("Subscription Id: %d, Key: %d", subId, subKey);
}

void 
RepState::eventDataScanFailed(Uint32 subId, Uint32 subKey, 
			     GrepError::Code error)
{
  ndbout_c("\nDatascan failed");
  ndbout_c("Error %d: %s", error, GrepError::getErrorDesc(error));
  ndbout_c("Subscription Id: %d, Key: %d", subId, subKey);
}

/****************************************************************************
 * Public Event Handlers : APPLY
 ****************************************************************************/

void
RepState::eventInsertConf(Uint32 gci, Uint32 nodeGrp) 
{
  Interval app(gci, gci);
  add(Channel::App, nodeGrp, app);
  clear(Channel::AppReq, nodeGrp, app);

#ifdef DEBUG_GREP
  ndbout_c("RepState: GCI Buffer %d:[%d] applied", nodeGrp, gci);
#endif
}

void
RepState::eventInsertRef(Uint32 gci, Uint32 nodeGrp, Uint32 tableId, 
			 GrepError::Code err) 
{
  ndbout_c("\nTable %d, used in replication, did not exist");
  RLOG(("ERROR %d:%s. Apply failed (%d[%d] in table %d)", 
	err, GrepError::getErrorDesc(err), nodeGrp, gci, tableId));
}


void
RepState::eventCreateTableRef(Uint32 gci,			  
			      Uint32 tableId, 
			      const char * tableName,
			      GrepError::Code err) 
{
  ndbout_c("\nFailed to create table %s with source site table id %d",
	   tableName,
	   tableId);

  RLOG(("ERROR %d:%s. Failed to create table %s with source site table id %d!",
	err, GrepError::getErrorDesc(err), tableName, tableId));
}

/****************************************************************************
 * Public Event Handlers : Connected/Disconnected
 ****************************************************************************/

void 
RepState::eventNodeConnected(Uint32 nodeId)
{
  m_repConnected = CONNECTED;
}
 
void 
RepState::eventNodeDisconnected(Uint32 nodeId)
{
  m_repConnected = DISCONNECTED;
}

void 
RepState::eventNodeConnectable(Uint32 nodeId)
{
  m_repConnected = CONNECTABLE;
}

/****************************************************************************
 * Public Event Handlers : Connected/Disconnected
 ****************************************************************************/

void 
RepState::eventSubscriptionDeleted(Uint32 subId, Uint32 subKey) 
{
  m_gciContainer->reset();
  m_channel.setState(Channel::CONSISTENT);
  m_channel.reset();
  m_subIdToRemove = 0;
  m_subKeyToRemove = 0;
}
