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

#include "TransSS.hpp"

#include <signaldata/SumaImpl.hpp>
#include <GrepError.hpp>

/*****************************************************************************
 * CREATE SUBSCRIPTION ID
 *****************************************************************************/

void 
TransSS::execGREP_CREATE_SUBID_CONF(NdbApiSignal* signal) 
{
  CreateSubscriptionIdConf const * conf = 
    (CreateSubscriptionIdConf *)signal->getDataPtr();
  Uint32 subId  = conf->subscriptionId;
  Uint32 subKey = conf->subscriptionKey;

  /** @todo Fix this */
#if 0
  signal->theData[0] = EventReport::GrepSubscriptionInfo;
  signal->theData[1] = GrepEvent::GrepSS_CreateSubIdConf;
  signal->theData[2] = subId;
  signal->theData[3] = subKey;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4 ,JBB);
#endif
  m_repState->eventSubscriptionIdCreated(subId, subKey);
}

void 
TransSS::execGREP_CREATE_SUBID_REF(NdbApiSignal* signal) 
{
  CreateSubscriptionIdRef const * ref = 
    (CreateSubscriptionIdRef *)signal->getDataPtr();
  Uint32          subId  = ref->subscriptionId;
  Uint32          subKey = ref->subscriptionKey;
  GrepError::Code err    = (GrepError::Code) ref->err;

#if 0
  signal->theData[0] = EventReport::GrepSubscriptionAlert;
  signal->theData[1] = GrepEvent::GrepSS_CreateSubIdRef;
  signal->theData[2] = subId;
  signal->theData[3] = subKey;
  signal->theData[4] = err;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 5 ,JBB);
#endif
  m_repState->eventSubscriptionIdCreateFailed(subId, subKey, err);
}

/*****************************************************************************
 * CREATE SUBSCRIPTION
 *****************************************************************************/

void
TransSS::execGREP_SUB_CREATE_CONF(NdbApiSignal* signal) 
{
  GrepSubCreateConf * const conf = (GrepSubCreateConf *)signal->getDataPtr(); 
  Uint32 noOfNodeGroups   = conf->noOfNodeGroups;
  Uint32 subId            = conf->subscriptionId;
  Uint32 subKey           = conf->subscriptionKey;

  m_repState->setNoOfNodeGroups(noOfNodeGroups);

#if 0
  signal->theData[0] = EventReport::GrepSubscriptionInfo;
  signal->theData[1] = GrepEvent::GrepSS_SubCreateConf;
  signal->theData[2] = subId;
  signal->theData[3] = subKey;
  signal->theData[4] = noOfNodeGroups;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 5, JBB);
#endif 

  m_repState->eventSubscriptionCreated(subId, subKey);
}

void
TransSS::execGREP_SUB_CREATE_REF(NdbApiSignal* signal) 
{
  GrepSubCreateRef * const ref = (GrepSubCreateRef *)signal->getDataPtr(); 
  Uint32 subId           = ref->subscriptionId;
  Uint32 subKey          = ref->subscriptionKey;
  GrepError::Code  err   = (GrepError::Code)ref->err;
#if 0
  signal->theData[0] = EventReport::GrepSubscriptionAlert;
  signal->theData[1] = GrepEvent::GrepSS_SubCreateRef;
  signal->theData[2] = subId;
  signal->theData[3] = subKey;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);
#endif

  m_repState->eventSubscriptionCreateFailed(subId, subKey, err);
}

/*****************************************************************************
 * START SUBSCRIPTION
 *****************************************************************************/

void
TransSS::execGREP_SUB_START_CONF(NdbApiSignal* signal) 
{
  GrepSubStartConf * const conf = (GrepSubStartConf *)signal->getDataPtr();
  Uint32 subId                  = conf->subscriptionId;
  Uint32 subKey                 = conf->subscriptionKey;
  SubscriptionData::Part part   = (SubscriptionData::Part) conf->part;
  
  switch(part) {
  case SubscriptionData::MetaData:
    RLOG(("Metalog started. Subscription %d-%d", subId, subKey));
    m_repState->eventMetaLogStarted(signal, subId, subKey);
#if 0
    signal->theData[0] = EventReport::GrepSubscriptionInfo;
    signal->theData[1] = GrepEvent::GrepSS_SubStartMetaConf;
    signal->theData[2] = m_requestor.getSubId(); 
    signal->theData[3] = m_requestor.getSubKey();
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);
#endif
    break;
  case SubscriptionData::TableData:
    RLOG(("Datalog started. Subscription %d-%d", subId, subKey));
    m_repState->eventDataLogStarted(signal, subId, subKey);
#if 0
    signal->theData[0] = EventReport::GrepSubscriptionInfo;
    signal->theData[1] = GrepEvent::GrepSS_SubStartDataConf;
    signal->theData[2] = m_requestor.getSubId(); 
    signal->theData[3] = m_requestor.getSubKey(); 
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);
#endif
    break;
  default:
    REPABORT("Illegal type of subscription");
  }
}

void
TransSS::execGREP_SUB_START_REF(NdbApiSignal* signal) 
{
  GrepSubStartRef * const ref = (GrepSubStartRef *)signal->getDataPtr();
  Uint32                 subId  = ref->subscriptionId;
  Uint32                 subKey = ref->subscriptionKey;
  GrepError::Code        err    = (GrepError::Code)ref->err;
  SubscriptionData::Part part   = (SubscriptionData::Part) ref->part;

  switch(part) {
  case SubscriptionData::MetaData:
    m_repState->eventMetaLogStartFailed(subId, subKey, err);
#if 1
    ndbout_c("Requestor: Subscription FAILED to start on Meta Data");
    ndbout_c("Error code : %d. Error message: %s",
	     err,  GrepError::getErrorDesc(err));
#endif
#if 0
    signal->theData[0] = EventReport::GrepSubscriptionAlert;
    signal->theData[1] = GrepEvent::GrepSS_SubStartMetaRef;
    signal->theData[2] = subId; //@todo. manage subscriptions.
    signal->theData[3] = subKey;  //@todo. manage subscriptions.
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);
#endif
    break;
  case SubscriptionData::TableData:
    m_repState->eventDataLogStartFailed(subId, subKey, err);
#if 0
    signal->theData[0] = EventReport::GrepSubscriptionAlert;
    signal->theData[1] = GrepEvent::GrepSS_SubStartDataRef;
    signal->theData[2] = subId; //@todo. manage subscriptions.
    signal->theData[3] = subKey;  //@todo. manage subscriptions.
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);
#endif
#if 1
    ndbout_c("Requestor: Subscription FAILED to start on Table Data");
#endif
    ndbout_c("Error code : %d. Error message: %s",
	     err,  GrepError::getErrorDesc(err));

    break;
  default:
    REPABORT("Illegal type of subscription");
  }
}
