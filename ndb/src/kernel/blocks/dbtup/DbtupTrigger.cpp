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


#define DBTUP_C
#include "Dbtup.hpp"
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <pc.hpp>
#include <AttributeDescriptor.hpp>
#include "AttributeOffset.hpp"
#include <AttributeHeader.hpp>
#include <signaldata/FireTrigOrd.hpp>
#include <signaldata/CreateTrig.hpp>
#include <signaldata/TuxMaint.hpp>

#define ljam() { jamLine(7000 + __LINE__); }
#define ljamEntry() { jamEntryLine(7000 + __LINE__); }

/* **************************************************************** */
/* ---------------------------------------------------------------- */
/* ----------------------- TRIGGER HANDLING ----------------------- */
/* ---------------------------------------------------------------- */
/* **************************************************************** */

ArrayList<Dbtup::TupTriggerData>*
Dbtup::findTriggerList(Tablerec* table,
                       TriggerType::Value ttype,
                       TriggerActionTime::Value ttime,
                       TriggerEvent::Value tevent)
{
  ArrayList<TupTriggerData>* tlist = NULL;
  switch (ttype) {
  case TriggerType::SUBSCRIPTION:
  case TriggerType::SUBSCRIPTION_BEFORE:
    switch (tevent) {
    case TriggerEvent::TE_INSERT:
      ljam();
      if (ttime == TriggerActionTime::TA_DETACHED)
        tlist = &table->subscriptionInsertTriggers;
      break;
    case TriggerEvent::TE_UPDATE:
      ljam();
      if (ttime == TriggerActionTime::TA_DETACHED)
        tlist = &table->subscriptionUpdateTriggers;
      break;
    case TriggerEvent::TE_DELETE:
      ljam();
      if (ttime == TriggerActionTime::TA_DETACHED)
        tlist = &table->subscriptionDeleteTriggers;
      break;
    default:
      break;
    }
    break;
  case TriggerType::SECONDARY_INDEX:
    switch (tevent) {
    case TriggerEvent::TE_INSERT:
      ljam();
      if (ttime == TriggerActionTime::TA_AFTER)
        tlist = &table->afterInsertTriggers;
      break;
    case TriggerEvent::TE_UPDATE:
      ljam();
      if (ttime == TriggerActionTime::TA_AFTER)
        tlist = &table->afterUpdateTriggers;
      break;
    case TriggerEvent::TE_DELETE:
      ljam();
      if (ttime == TriggerActionTime::TA_AFTER)
        tlist = &table->afterDeleteTriggers;
      break;
    default:
      break;
    }
    break;
  case TriggerType::ORDERED_INDEX:
    switch (tevent) {
    case TriggerEvent::TE_CUSTOM:
      ljam();
      if (ttime == TriggerActionTime::TA_CUSTOM)
        tlist = &table->tuxCustomTriggers;
      break;
    default:
      break;
    }
    break;
  case TriggerType::READ_ONLY_CONSTRAINT:
    switch (tevent) {
    case TriggerEvent::TE_UPDATE:
      ljam();
      if (ttime == TriggerActionTime::TA_AFTER)
        tlist = &table->constraintUpdateTriggers;
      break;
    default:
      break;
    }
    break;
  default:
    break;
  }
  return tlist;
}

// Trigger signals
void
Dbtup::execCREATE_TRIG_REQ(Signal* signal)
{
  ljamEntry();
  BlockReference senderRef = signal->getSendersBlockRef();
  const CreateTrigReq reqCopy = *(const CreateTrigReq*)signal->getDataPtr();
  const CreateTrigReq* const req = &reqCopy;

  // Find table
  TablerecPtr tabPtr;
  tabPtr.i = req->getTableId();
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);

  // Create trigger and associate it with the table
  if (createTrigger(tabPtr.p, req)) {
    ljam();
    // Send conf
    CreateTrigConf* const conf = (CreateTrigConf*)signal->getDataPtrSend();
    conf->setUserRef(reference());
    conf->setConnectionPtr(req->getConnectionPtr());
    conf->setRequestType(req->getRequestType());
    conf->setTableId(req->getTableId());
    conf->setIndexId(req->getIndexId());
    conf->setTriggerId(req->getTriggerId());
    conf->setTriggerInfo(req->getTriggerInfo());
    sendSignal(senderRef, GSN_CREATE_TRIG_CONF, 
               signal, CreateTrigConf::SignalLength, JBB);
  } else {
    ljam();
    // Send ref
    CreateTrigRef* const ref = (CreateTrigRef*)signal->getDataPtrSend();
    ref->setUserRef(reference());
    ref->setConnectionPtr(req->getConnectionPtr());
    ref->setRequestType(req->getRequestType());
    ref->setTableId(req->getTableId());
    ref->setIndexId(req->getIndexId());
    ref->setTriggerId(req->getTriggerId());
    ref->setTriggerInfo(req->getTriggerInfo());
    ref->setErrorCode(CreateTrigRef::TooManyTriggers);
    sendSignal(senderRef, GSN_CREATE_TRIG_REF, 
               signal, CreateTrigRef::SignalLength, JBB);
  }
}//Dbtup::execCREATE_TRIG_REQ()

void
Dbtup::execDROP_TRIG_REQ(Signal* signal)
{
  ljamEntry();
  BlockReference senderRef = signal->getSendersBlockRef();
  const DropTrigReq reqCopy = *(const DropTrigReq*)signal->getDataPtr();
  const DropTrigReq* const req = &reqCopy;

  // Find table
  TablerecPtr tabPtr;
  tabPtr.i = req->getTableId();
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);

  // Drop trigger
  Uint32 r = dropTrigger(tabPtr.p, req);
  if (r == 0){
    // Send conf
    DropTrigConf* const conf = (DropTrigConf*)signal->getDataPtrSend();
    conf->setUserRef(senderRef);
    conf->setConnectionPtr(req->getConnectionPtr());
    conf->setRequestType(req->getRequestType());
    conf->setTableId(req->getTableId());
    conf->setIndexId(req->getIndexId());
    conf->setTriggerId(req->getTriggerId());
    sendSignal(senderRef, GSN_DROP_TRIG_CONF, 
	       signal, DropTrigConf::SignalLength, JBB);
  } else {
    // Send ref
    DropTrigRef* const ref = (DropTrigRef*)signal->getDataPtrSend();
    ref->setUserRef(senderRef);
    ref->setConnectionPtr(req->getConnectionPtr());
    ref->setRequestType(req->getRequestType());
    ref->setTableId(req->getTableId());
    ref->setIndexId(req->getIndexId());
    ref->setTriggerId(req->getTriggerId());
    ref->setErrorCode((DropTrigRef::ErrorCode)r);
    ref->setErrorLine(__LINE__);
    ref->setErrorNode(refToNode(reference()));
    sendSignal(senderRef, GSN_DROP_TRIG_REF, 
	       signal, DropTrigRef::SignalLength, JBB);
  }
}//Dbtup::DROP_TRIG_REQ()

/* ---------------------------------------------------------------- */
/* ------------------------- createTrigger ------------------------ */
/*                                                                  */
/* Creates a new trigger record by fetching one from the trigger    */
/* pool and associates it with the given table.                     */
/* Trigger type can be one of secondary_index, subscription,        */
/* constraint(NYI), foreign_key(NYI), schema_upgrade(NYI),          */
/* api_trigger(NYI) or sql_trigger(NYI).                            */   
/* Note that this method only checks for total number of allowed    */
/* triggers. Checking the number of allowed triggers per table is   */
/* done by TRIX.                                                    */
/*                                                                  */
/* ---------------------------------------------------------------- */
bool
Dbtup::createTrigger(Tablerec* table, const CreateTrigReq* req)
{
  if (ERROR_INSERTED(4003)) {
    CLEAR_ERROR_INSERT_VALUE;
    return false;
  }
  TriggerType::Value ttype = req->getTriggerType();
  TriggerActionTime::Value ttime = req->getTriggerActionTime();
  TriggerEvent::Value tevent = req->getTriggerEvent();

  ArrayList<TupTriggerData>* tlist = findTriggerList(table, ttype, ttime, tevent);
  ndbrequire(tlist != NULL);

  TriggerPtr tptr;
  if (!tlist->seize(tptr))
    return false;

  // Set trigger id
  tptr.p->triggerId = req->getTriggerId();

  //  ndbout_c("Create TupTrigger %u = %u %u %u %u", tptr.p->triggerId, table, ttype, ttime, tevent);

  // Set index id
  tptr.p->indexId = req->getIndexId();

  // Set trigger type etc
  tptr.p->triggerType = ttype;
  tptr.p->triggerActionTime = ttime;
  tptr.p->triggerEvent = tevent;

  tptr.p->sendBeforeValues = true;
  if ((tptr.p->triggerType == TriggerType::SUBSCRIPTION) &&
      ((tptr.p->triggerEvent == TriggerEvent::TE_UPDATE) ||
       (tptr.p->triggerEvent == TriggerEvent::TE_DELETE))) {
    ljam();
    tptr.p->sendBeforeValues = false;
  }
  tptr.p->sendOnlyChangedAttributes = false;
  if (((tptr.p->triggerType == TriggerType::SUBSCRIPTION) ||
      (tptr.p->triggerType == TriggerType::SUBSCRIPTION_BEFORE)) &&
      (tptr.p->triggerEvent == TriggerEvent::TE_UPDATE)) {
    ljam();
    tptr.p->sendOnlyChangedAttributes = true;
  }

  // Set monitor all
  tptr.p->monitorAllAttributes = req->getMonitorAllAttributes();
  tptr.p->monitorReplicas = req->getMonitorReplicas();
  tptr.p->m_receiverBlock = refToBlock(req->getReceiverRef());

  tptr.p->attributeMask.clear();
  if (tptr.p->monitorAllAttributes) {
    ljam();
    for(Uint32 i = 0; i < table->noOfAttr; i++) {
      if (!primaryKey(table, i)) {
        ljam();
        tptr.p->attributeMask.set(i);
      }
    }
  } else {
    // Set attribute mask
    ljam();
    tptr.p->attributeMask = req->getAttributeMask();
  }
  return true;
}//Dbtup::createTrigger()

bool
Dbtup::primaryKey(Tablerec* const regTabPtr, Uint32 attrId)
{
  Uint32 attrDescriptorStart = regTabPtr->tabDescriptor;
  Uint32 attrDescriptor = getTabDescrWord(attrDescriptorStart + (attrId * ZAD_SIZE));
  return (bool)AttributeDescriptor::getPrimaryKey(attrDescriptor);
}//Dbtup::primaryKey()

/* ---------------------------------------------------------------- */
/* -------------------------- dropTrigger ------------------------- */
/*                                                                  */
/* Deletes a trigger record by disassociating it with the given     */
/* table and returning it to the trigger pool.                      */
/* Trigger type can be one of secondary_index, subscription,        */
/* constraint(NYI), foreign_key(NYI), schema_upgrade(NYI),          */
/* api_trigger(NYI) or sql_trigger(NYI).                            */   
/*                                                                  */
/* ---------------------------------------------------------------- */
Uint32
Dbtup::dropTrigger(Tablerec* table, const DropTrigReq* req)
{
  Uint32 triggerId = req->getTriggerId();

  TriggerType::Value ttype = req->getTriggerType();
  TriggerActionTime::Value ttime = req->getTriggerActionTime();
  TriggerEvent::Value tevent = req->getTriggerEvent();

  //  ndbout_c("Drop TupTrigger %u = %u %u %u %u", triggerId, table, ttype, ttime, tevent);

  ArrayList<TupTriggerData>* tlist = findTriggerList(table, ttype, ttime, tevent);
  ndbrequire(tlist != NULL);

  Ptr<TupTriggerData> ptr;
  for (tlist->first(ptr); !ptr.isNull(); tlist->next(ptr)) {
    ljam();
    if (ptr.p->triggerId == triggerId) {
      ljam();
      tlist->release(ptr.i);
      return 0;
    }
  }
  return DropTrigRef::TriggerNotFound;
}//Dbtup::dropTrigger()

/* ---------------------------------------------------------------- */
/* -------------- checkImmediateTriggersAfterOp ------------------ */
/*                                                                  */
/* Called after an insert, delete, or update operation takes        */
/* place. Fetches before tuple for deletes and updates and          */
/* after tuple for inserts and updates.                             */
/* Executes immediate triggers by sending FIRETRIGORD               */
/*                                                                  */
/* ---------------------------------------------------------------- */
void Dbtup::checkImmediateTriggersAfterInsert(Signal* signal, 
                                              Operationrec* const regOperPtr, 
                                              Tablerec* const regTablePtr)
{
  if(refToBlock(regOperPtr->coordinatorTC) == DBLQH) {
    return;
  }

  if ((regOperPtr->primaryReplica) &&
      (!(regTablePtr->afterInsertTriggers.isEmpty()))) {
    ljam();
    fireImmediateTriggers(signal,
                          regTablePtr->afterInsertTriggers,
                          regOperPtr);
  }//if
}//Dbtup::checkImmediateTriggersAfterInsert()

void Dbtup::checkImmediateTriggersAfterUpdate(Signal* signal, 
                                              Operationrec* const regOperPtr, 
                                              Tablerec* const regTablePtr)
{
  if(refToBlock(regOperPtr->coordinatorTC) == DBLQH) {
    return;
  }

  if ((regOperPtr->primaryReplica) &&
      (!(regTablePtr->afterUpdateTriggers.isEmpty()))) {
    ljam();
    fireImmediateTriggers(signal,
                          regTablePtr->afterUpdateTriggers,
                          regOperPtr);
  }//if
  if ((regOperPtr->primaryReplica) &&
      (!(regTablePtr->constraintUpdateTriggers.isEmpty()))) {
    ljam();
    fireImmediateTriggers(signal,
                          regTablePtr->constraintUpdateTriggers,
                          regOperPtr);
  }//if
}//Dbtup::checkImmediateTriggersAfterUpdate()

void Dbtup::checkImmediateTriggersAfterDelete(Signal* signal, 
                                              Operationrec* const regOperPtr, 
                                              Tablerec* const regTablePtr)
{
  if(refToBlock(regOperPtr->coordinatorTC) == DBLQH) {
    return;
  }

  if ((regOperPtr->primaryReplica) &&
      (!(regTablePtr->afterDeleteTriggers.isEmpty()))) {
    ljam();
    executeTriggers(signal,
                    regTablePtr->afterDeleteTriggers,
                    regOperPtr);
  }//if
}//Dbtup::checkImmediateTriggersAfterDelete()

#if 0
/* ---------------------------------------------------------------- */
/* --------------------- checkDeferredTriggers -------------------- */
/*                                                                  */
/* Called before commit after an insert, delete, or update          */
/* operation. Fetches before tuple for deletes and updates and      */
/* after tuple for inserts and updates.                             */
/* Executes deferred triggers by sending FIRETRIGORD                */
/*                                                                  */
/* ---------------------------------------------------------------- */
void Dbtup::checkDeferredTriggers(Signal* signal, 
                                  Operationrec* const regOperPtr,
                                  Tablerec* const regTablePtr)
{
  ljam();
  // NYI
}//Dbtup::checkDeferredTriggers()
#endif

/* ---------------------------------------------------------------- */
/* --------------------- checkDetachedTriggers -------------------- */
/*                                                                  */
/* Called at commit after an insert, delete, or update operation.   */
/* Fetches before tuple for deletes and updates and                 */
/* after tuple for inserts and updates.                             */
/* Executes detached triggers by sending FIRETRIGORD                */
/*                                                                  */
/* ---------------------------------------------------------------- */
void Dbtup::checkDetachedTriggers(Signal* signal, 
                                  Operationrec* const regOperPtr,
                                  Tablerec* const regTablePtr)
{
  switch(regOperPtr->optype) {
  case(ZINSERT):
    ljam();
    if (regTablePtr->subscriptionInsertTriggers.isEmpty()) {
      // Table has no active triggers monitoring inserts at commit
      ljam();
      return;
    }//if

    // If any fired immediate insert trigger then fetch after tuple
    fireDetachedTriggers(signal, 
                         regTablePtr->subscriptionInsertTriggers, 
                         regOperPtr);
    break;
  case(ZDELETE):
    ljam();
    if (regTablePtr->subscriptionDeleteTriggers.isEmpty()) {
      // Table has no active triggers monitoring deletes at commit
      ljam();
      return;
    }//if

    // Execute any after delete triggers by sending 
    // FIRETRIGORD with the before tuple
    executeTriggers(signal, 
                    regTablePtr->subscriptionDeleteTriggers, 
                    regOperPtr);
    break;
  case(ZUPDATE):
    ljam();
    if (regTablePtr->subscriptionUpdateTriggers.isEmpty()) {
      // Table has no active triggers monitoring updates at commit
      ljam();
      return;
    }//if

    // If any fired immediate update trigger then fetch after tuple
    // and send two FIRETRIGORD one with before tuple and one with after tuple
    fireDetachedTriggers(signal, 
                         regTablePtr->subscriptionUpdateTriggers, 
                         regOperPtr);
    break;
  default:
    ndbrequire(false);
    break;
  }//switch
}//Dbtup::CheckDetachedTriggers()

void 
Dbtup::fireImmediateTriggers(Signal* signal, 
                             ArrayList<TupTriggerData>& triggerList, 
                             Operationrec* const regOperPtr)
{
  TriggerPtr trigPtr;
  triggerList.first(trigPtr);
  while (trigPtr.i != RNIL) {
    ljam();
    if (trigPtr.p->monitorAllAttributes ||
        trigPtr.p->attributeMask.overlaps(regOperPtr->changeMask)) {
      ljam();
      executeTrigger(signal,
                     trigPtr.p,
                     regOperPtr);
    }//if
    triggerList.next(trigPtr);
  }//while
}//Dbtup::fireImmediateTriggers()

#if 0
void 
Dbtup::fireDeferredTriggers(Signal* signal, 
                            ArrayList<TupTriggerData>& triggerList, 
                            Operationrec* const regOperPtr)
{
  TriggerPtr trigPtr;
  triggerList.first(trigPtr);
  while (trigPtr.i != RNIL) {
    ljam();
    if (trigPtr.p->monitorAllAttributes ||
        trigPtr.p->attributeMask.overlaps(regOperPtr->changeMask)) {
      ljam();
      executeTrigger(signal,
                     trigPtr,
                     regOperPtr);
    }//if
    triggerList.next(trigPtr);
  }//while
}//Dbtup::fireDeferredTriggers()
#endif

void 
Dbtup::fireDetachedTriggers(Signal* signal, 
                            ArrayList<TupTriggerData>& triggerList, 
                            Operationrec* const regOperPtr)
{
  TriggerPtr trigPtr;  
  triggerList.first(trigPtr);
  while (trigPtr.i != RNIL) {
    ljam();
    if ((trigPtr.p->monitorReplicas || regOperPtr->primaryReplica) &&
        (trigPtr.p->monitorAllAttributes ||
         trigPtr.p->attributeMask.overlaps(regOperPtr->changeMask))) {
      ljam();
      executeTrigger(signal,
                     trigPtr.p,
                     regOperPtr);
    }//if
    triggerList.next(trigPtr);
  }//while
}//Dbtup::fireDetachedTriggers()

void Dbtup::executeTriggers(Signal* signal, 
                            ArrayList<TupTriggerData>& triggerList, 
                            Operationrec* regOperPtr)
{
  TriggerPtr trigPtr;
  triggerList.first(trigPtr);
  while (trigPtr.i != RNIL) {
    ljam();
    executeTrigger(signal,
                   trigPtr.p,
                   regOperPtr);
    triggerList.next(trigPtr);

  }//while
}//Dbtup::executeTriggers()

void Dbtup::executeTrigger(Signal* signal,
                           TupTriggerData* const trigPtr,
                           Operationrec* const regOperPtr)
{

  /**
   * The block below does not work together with GREP.
   * I have 2 db nodes (2 replicas) -> one node group.
   * I want to have FIRETRIG_ORD sent to all SumaParticipants,
   * from all nodes in the node group described above. However, 
   * only one of the nodes in the node group actually sends the
   *  FIRE_TRIG_ORD, and the other node enters this "hack" below.
   * I don't really know what the code snippet below does, but it
   * does not work with GREP the way Lars and I want it.
   * We need to have triggers fired from both the primary and the
   * backup replica, not only the primary as it is now.
   * 
   * Note: In Suma, I have changed triggers to be created with
   * setMonitorReplicas(true).
   * /Johan
   *
   * See RT 709
   */
  // XXX quick fix to NR, should fix in LQHKEYREQ instead
  /*  
      if (refToBlock(regOperPtr->coordinatorTC) == DBLQH) {
      jam();
      return;
      }
  */
  BlockReference ref = trigPtr->m_receiverBlock;
  Uint32* const keyBuffer = &cinBuffer[0];
  Uint32* const mainBuffer = &coutBuffer[0];
  Uint32* const copyBuffer = &clogMemBuffer[0];

  Uint32 noPrimKey, noMainWords, noCopyWords;

  if (ref == BACKUP) {
    ljam();
    /*
    In order for the implementation of BACKUP to work even when changing
    primaries in the middle of the backup we need to set the trigger on
    all replicas. This check checks whether this is the node where this
    trigger should be fired. The check should preferably have been put
    completely in the BACKUP block but it was about five times simpler
    to put it here and also much faster for the backup (small overhead
    for everybody else.
    */
    signal->theData[0] = trigPtr->triggerId;
    signal->theData[1] = regOperPtr->fragId;
    EXECUTE_DIRECT(BACKUP, GSN_BACKUP_TRIG_REQ, signal, 2);
    ljamEntry();
    if (signal->theData[0] == 0) {
      ljam();
      return;
    }//if
  }//if
  if (!readTriggerInfo(trigPtr,
                       regOperPtr,
                       keyBuffer,
                       noPrimKey,
                       mainBuffer,
                       noMainWords,
                       copyBuffer,
                       noCopyWords)) {
    ljam();
    return;
  }//if
//--------------------------------------------------------------------
// Now all data for this trigger has been read. It is now time to send
// the trigger information consisting of two or three sets of TRIG_
// ATTRINFO signals and one FIRE_TRIG_ORD signal.
// We start by setting common header info for all TRIG_ATTRINFO signals.
//--------------------------------------------------------------------
  bool executeDirect;
  TrigAttrInfo* const trigAttrInfo = (TrigAttrInfo *)signal->getDataPtrSend();
  trigAttrInfo->setConnectionPtr(regOperPtr->tcOpIndex);
  trigAttrInfo->setTriggerId(trigPtr->triggerId);

  switch(trigPtr->triggerType) {
  case (TriggerType::SECONDARY_INDEX):
    ljam();
    ref = regOperPtr->coordinatorTC;
    executeDirect = false;
    break;
  case (TriggerType::SUBSCRIPTION):
  case (TriggerType::SUBSCRIPTION_BEFORE):
    ljam();
    // Since only backup uses subscription triggers we send to backup directly for now
    ref = trigPtr->m_receiverBlock;
    executeDirect = true;
    break;
  case (TriggerType::READ_ONLY_CONSTRAINT):
    terrorCode = ZREAD_ONLY_CONSTRAINT_VIOLATION;
    // XXX should return status and abort the rest
    return;
  default:
    ndbrequire(false);
  }//switch

  regOperPtr->noFiredTriggers++;

  trigAttrInfo->setAttrInfoType(TrigAttrInfo::PRIMARY_KEY);
  sendTrigAttrInfo(signal, keyBuffer, noPrimKey, executeDirect, ref);

  Uint32 noAfter = 0;
  Uint32 noBefore = 0;
  switch(regOperPtr->optype) {
  case(ZINSERT):
    ljam();
    // Send AttrInfo signals with new attribute values
    trigAttrInfo->setAttrInfoType(TrigAttrInfo::AFTER_VALUES);
    sendTrigAttrInfo(signal, mainBuffer, noMainWords, executeDirect, ref);
    noAfter = noMainWords;
    break;
  case(ZDELETE):
    if (trigPtr->sendBeforeValues) {
      ljam();
      trigAttrInfo->setAttrInfoType(TrigAttrInfo::BEFORE_VALUES);
      sendTrigAttrInfo(signal, mainBuffer, noMainWords, executeDirect, ref);
      noBefore = noMainWords;
    }//if
    break;
  case(ZUPDATE):
    ljam();
    if (trigPtr->sendBeforeValues) {
      ljam();
      trigAttrInfo->setAttrInfoType(TrigAttrInfo::BEFORE_VALUES);
      sendTrigAttrInfo(signal, copyBuffer, noCopyWords, executeDirect, ref);
      noBefore = noCopyWords;
    }//if
    trigAttrInfo->setAttrInfoType(TrigAttrInfo::AFTER_VALUES);
    sendTrigAttrInfo(signal, mainBuffer, noMainWords, executeDirect, ref);
    noAfter = noMainWords;
    break;
  default:
    ndbrequire(false);
  }//switch
  sendFireTrigOrd(signal,
                  regOperPtr,
                  trigPtr,
                  noPrimKey,
                  noBefore,
                  noAfter);
}//Dbtup::executeTrigger()

Uint32 Dbtup::setAttrIds(Bitmask<MAXNROFATTRIBUTESINWORDS>& attributeMask, 
                         Uint32 noOfAttributes, 
                         Uint32* inBuffer)
{
  Uint32 bufIndx = 0;
  for (Uint32 i = 0; i < noOfAttributes; i++) {
    ljam();
    if (attributeMask.get(i)) {
      ljam();
      AttributeHeader::init(&inBuffer[bufIndx++], i, 0);
    }//if
  }//for
  return bufIndx;
}//Dbtup::setAttrIds()

bool Dbtup::readTriggerInfo(TupTriggerData* const trigPtr,
                            Operationrec* const regOperPtr, 
                            Uint32* const keyBuffer,
                            Uint32& noPrimKey,
                            Uint32*  const mainBuffer,
                            Uint32& noMainWords,
                            Uint32* const copyBuffer,
                            Uint32& noCopyWords)
{
  noCopyWords = 0;
  noMainWords = 0;
  Uint32 readBuffer[MAX_ATTRIBUTES_IN_TABLE];
  PagePtr pagep;

//---------------------------------------------------------------------------
// Set-up variables needed by readAttributes operPtr.p, tabptr.p
//---------------------------------------------------------------------------
  operPtr.p = regOperPtr;
  tabptr.i = regOperPtr->tableRef;
  ptrCheckGuard(tabptr, cnoOfTablerec, tablerec);
  Tablerec* const regTabPtr = tabptr.p;
//--------------------------------------------------------------------
// Initialise pagep and tuple offset for read of main tuple
//--------------------------------------------------------------------
  Uint32 tupheadoffset = regOperPtr->pageOffset;
  pagep.i = regOperPtr->realPageId;
  ptrCheckGuard(pagep, cnoOfPage, page);

//--------------------------------------------------------------------
// Read Primary Key Values
//--------------------------------------------------------------------
  noPrimKey = readAttributes(pagep.p,
                             tupheadoffset,
                             &tableDescriptor[regTabPtr->readKeyArray].tabDescr,
                             regTabPtr->noOfKeyAttr,
                             keyBuffer,
                             ZATTR_BUFFER_SIZE,
                             true);
  ndbrequire(noPrimKey != (Uint32)-1);

  Uint32 numAttrsToRead;
  if ((regOperPtr->optype == ZUPDATE) &&
      (trigPtr->sendOnlyChangedAttributes)) {
    ljam();
//--------------------------------------------------------------------
// Update that sends only changed information
//--------------------------------------------------------------------
    Bitmask<MAXNROFATTRIBUTESINWORDS> attributeMask;
    attributeMask = trigPtr->attributeMask;
    attributeMask.bitAND(regOperPtr->changeMask);
    numAttrsToRead = setAttrIds(attributeMask, regTabPtr->noOfAttr, &readBuffer[0]);

  } else if ((regOperPtr->optype == ZDELETE) &&
             (!trigPtr->sendBeforeValues)) {
    ljam();
//--------------------------------------------------------------------
// Delete without sending before values only read Primary Key
//--------------------------------------------------------------------
    return true;
  } else {
    ljam();
//--------------------------------------------------------------------
// All others send all attributes that are monitored
//--------------------------------------------------------------------
    numAttrsToRead = setAttrIds(trigPtr->attributeMask, regTabPtr->noOfAttr, &readBuffer[0]);
  }//if
  ndbrequire(numAttrsToRead < MAX_ATTRIBUTES_IN_TABLE);
//--------------------------------------------------------------------
// Read Main tuple values
//--------------------------------------------------------------------
  if ((regOperPtr->optype != ZDELETE) ||
      (trigPtr->sendBeforeValues)) {
    ljam();
    noMainWords = readAttributes(pagep.p,
                                 tupheadoffset,
                                 &readBuffer[0],
                                 numAttrsToRead,
                                 mainBuffer,
                                 ZATTR_BUFFER_SIZE,
                                 true);
    ndbrequire(noMainWords != (Uint32)-1);
  } else {
    ljam();
    noMainWords = 0;
  }//if
//--------------------------------------------------------------------
// Read Copy tuple values for UPDATE's
//--------------------------------------------------------------------
// Initialise pagep and tuple offset for read of copy tuple
//--------------------------------------------------------------------
  if ((regOperPtr->optype == ZUPDATE) &&
      (trigPtr->sendBeforeValues)) {
    ljam();

    tupheadoffset = regOperPtr->pageOffsetC;
    pagep.i = regOperPtr->realPageIdC;
    ptrCheckGuard(pagep, cnoOfPage, page);

    noCopyWords = readAttributes(pagep.p,
                                 tupheadoffset,
                                 &readBuffer[0],
                                 numAttrsToRead,
                                 copyBuffer,
                                 ZATTR_BUFFER_SIZE,
                                 true);

    ndbrequire(noCopyWords != (Uint32)-1);
    if ((noMainWords == noCopyWords) &&
        (memcmp(mainBuffer, copyBuffer, noMainWords << 2) == 0)) {
//--------------------------------------------------------------------
// Although a trigger was fired it was not necessary since the old
// value and the new value was exactly the same
//--------------------------------------------------------------------
      ljam();
      return false;
    }//if
  }//if
  return true;
}//Dbtup::readTriggerInfo()

void Dbtup::sendTrigAttrInfo(Signal* signal, 
                             Uint32* data, 
                             Uint32  dataLen,
                             bool    executeDirect,
                             BlockReference receiverReference)
{
  TrigAttrInfo* const trigAttrInfo = (TrigAttrInfo *)signal->getDataPtrSend();
  Uint32 sigLen;
  Uint32 dataIndex = 0;
  do {
    sigLen = dataLen - dataIndex;
    if (sigLen > TrigAttrInfo::DataLength) {
      ljam();
      sigLen = TrigAttrInfo::DataLength;
    }//if
    MEMCOPY_NO_WORDS(trigAttrInfo->getData(), 
                     data + dataIndex,
                     sigLen);
    if (executeDirect) {
      ljam();
      EXECUTE_DIRECT(receiverReference, 
                     GSN_TRIG_ATTRINFO,
                     signal,
		     TrigAttrInfo::StaticLength + sigLen);
      ljamEntry();
    } else {
      ljam();
      sendSignal(receiverReference, 
                 GSN_TRIG_ATTRINFO, 
                 signal, 
                 TrigAttrInfo::StaticLength + sigLen,
                 JBB);
    }//if
    dataIndex += sigLen;
  } while (dataLen != dataIndex);
}//Dbtup::sendTrigAttrInfo()

void Dbtup::sendFireTrigOrd(Signal* signal, 
                            Operationrec * const regOperPtr, 
                            TupTriggerData* const trigPtr, 
                            Uint32 noPrimKeyWords, 
                            Uint32 noBeforeValueWords, 
                            Uint32 noAfterValueWords)
{
  FireTrigOrd* const fireTrigOrd = (FireTrigOrd *)signal->getDataPtrSend();
  
  fireTrigOrd->setConnectionPtr(regOperPtr->tcOpIndex);
  fireTrigOrd->setTriggerId(trigPtr->triggerId);

  switch(regOperPtr->optype) {
  case(ZINSERT):
    ljam();
    fireTrigOrd->setTriggerEvent(TriggerEvent::TE_INSERT);
    break;
  case(ZDELETE):
    ljam();
    fireTrigOrd->setTriggerEvent(TriggerEvent::TE_DELETE);
    break;
  case(ZUPDATE):
    ljam();
    fireTrigOrd->setTriggerEvent(TriggerEvent::TE_UPDATE);
    break;
  default:
    ndbrequire(false);
    break;
  }//switch

  fireTrigOrd->setNoOfPrimaryKeyWords(noPrimKeyWords);
  fireTrigOrd->setNoOfBeforeValueWords(noBeforeValueWords);
  fireTrigOrd->setNoOfAfterValueWords(noAfterValueWords);

  switch(trigPtr->triggerType) {
  case (TriggerType::SECONDARY_INDEX):
    ljam();
    sendSignal(regOperPtr->coordinatorTC, GSN_FIRE_TRIG_ORD, 
               signal, FireTrigOrd::SignalLength, JBB);
    break;
  case (TriggerType::SUBSCRIPTION_BEFORE): // Only Suma
    ljam();
    // Since only backup uses subscription triggers we 
    // send to backup directly for now
    fireTrigOrd->setGCI(regOperPtr->gci);
    fireTrigOrd->setHashValue(regOperPtr->hashValue);
    EXECUTE_DIRECT(trigPtr->m_receiverBlock,
                   GSN_FIRE_TRIG_ORD,
                   signal,
		   FireTrigOrd::SignalWithHashValueLength);
    break;
  case (TriggerType::SUBSCRIPTION):
    ljam();
    // Since only backup uses subscription triggers we 
    // send to backup directly for now
    fireTrigOrd->setGCI(regOperPtr->gci);
    EXECUTE_DIRECT(trigPtr->m_receiverBlock,
                   GSN_FIRE_TRIG_ORD,
                   signal,
		   FireTrigOrd::SignalWithGCILength);
    break;
  default:
    ndbrequire(false);
    break;
  }//switch
}//Dbtup::sendFireTrigOrd()

/*
 * Ordered index triggers.
 *
 * Insert: add entry to index
 * Update: add entry to index, de|ay remove until commit
 * Delete: do nothing, delay remove until commit
 * Commit: remove entry delayed from update and delete
 * Abort : remove entry added by insert and update
 *
 * See Notes.txt for the details.
 */

int
Dbtup::executeTuxInsertTriggers(Signal* signal,
                                Operationrec* const regOperPtr,
                                Tablerec* const regTabPtr)
{
  TuxMaintReq* const req = (TuxMaintReq*)signal->getDataPtrSend();
  PagePtr pagePtr;
  pagePtr.i = regOperPtr->realPageId;
  ptrCheckGuard(pagePtr, cnoOfPage, page);
  Uint32 tupVersion = pagePtr.p->pageWord[regOperPtr->pageOffset + 1];
  ndbrequire(tupVersion == regOperPtr->tupVersion);
  // fill in constant part
  req->tableId = regOperPtr->tableRef;
  req->fragId = regOperPtr->fragId;
  req->pageId = regOperPtr->realPageId;
  req->pageOffset = regOperPtr->pageOffset;
  req->tupVersion = tupVersion;
  req->opInfo = TuxMaintReq::OpAdd;
  // loop over index list
  const ArrayList<TupTriggerData>& triggerList = regTabPtr->tuxCustomTriggers;
  TriggerPtr triggerPtr;
  triggerList.first(triggerPtr);
  while (triggerPtr.i != RNIL) {
    ljam();
    req->indexId = triggerPtr.p->indexId;
    req->errorCode = RNIL;
    EXECUTE_DIRECT(DBTUX, GSN_TUX_MAINT_REQ,
        signal, TuxMaintReq::SignalLength);
    ljamEntry();
    if (req->errorCode != 0) {
      ljam();
      terrorCode = req->errorCode;
      return -1;
    }
    triggerList.next(triggerPtr);
  }
  return 0;
}

int
Dbtup::executeTuxUpdateTriggers(Signal* signal,
                                Operationrec* const regOperPtr,
                                Tablerec* const regTabPtr)
{
  TuxMaintReq* const req = (TuxMaintReq*)signal->getDataPtrSend();
  PagePtr pagePtr;
  pagePtr.i = regOperPtr->realPageId;
  ptrCheckGuard(pagePtr, cnoOfPage, page);
  Uint32 tupVersion = pagePtr.p->pageWord[regOperPtr->pageOffset + 1];
  ndbrequire(tupVersion == regOperPtr->tupVersion);
  // fill in constant part
  req->tableId = regOperPtr->tableRef;
  req->fragId = regOperPtr->fragId;
  req->pageId = regOperPtr->realPageId;
  req->pageOffset = regOperPtr->pageOffset;
  req->tupVersion = tupVersion;
  req->opInfo = TuxMaintReq::OpAdd;
  // loop over index list
  const ArrayList<TupTriggerData>& triggerList = regTabPtr->tuxCustomTriggers;
  TriggerPtr triggerPtr;
  triggerList.first(triggerPtr);
  while (triggerPtr.i != RNIL) {
    ljam();
    req->indexId = triggerPtr.p->indexId;
    req->errorCode = RNIL;
    EXECUTE_DIRECT(DBTUX, GSN_TUX_MAINT_REQ,
        signal, TuxMaintReq::SignalLength);
    ljamEntry();
    if (req->errorCode != 0) {
      ljam();
      terrorCode = req->errorCode;
      return -1;
    }
    triggerList.next(triggerPtr);
  }
  return 0;
}

int
Dbtup::executeTuxDeleteTriggers(Signal* signal,
                                Operationrec* const regOperPtr,
                                Tablerec* const regTabPtr)
{
  // do nothing
  return 0;
}

void
Dbtup::executeTuxCommitTriggers(Signal* signal,
                                Operationrec* regOperPtr,
                                Tablerec* const regTabPtr)
{
  TuxMaintReq* const req = (TuxMaintReq*)signal->getDataPtrSend();
  // get version
  // XXX could add prevTupVersion to Operationrec
  Uint32 tupVersion;
  if (regOperPtr->optype == ZINSERT) {
    if (! regOperPtr->deleteInsertFlag)
      return;
    ljam();
    PagePtr pagePtr;
    pagePtr.i = regOperPtr->realPageIdC;
    ptrCheckGuard(pagePtr, cnoOfPage, page);
    tupVersion = pagePtr.p->pageWord[regOperPtr->pageOffsetC + 1];
    ndbrequire(tupVersion != regOperPtr->tupVersion);
  } else if (regOperPtr->optype == ZUPDATE) {
    ljam();
    PagePtr pagePtr;
    pagePtr.i = regOperPtr->realPageIdC;
    ptrCheckGuard(pagePtr, cnoOfPage, page);
    tupVersion = pagePtr.p->pageWord[regOperPtr->pageOffsetC + 1];
    ndbrequire(tupVersion != regOperPtr->tupVersion);
  } else if (regOperPtr->optype == ZDELETE) {
    if (regOperPtr->deleteInsertFlag)
      return;
    ljam();
    PagePtr pagePtr;
    pagePtr.i = regOperPtr->realPageId;
    ptrCheckGuard(pagePtr, cnoOfPage, page);
    tupVersion = pagePtr.p->pageWord[regOperPtr->pageOffset + 1];
    ndbrequire(tupVersion == regOperPtr->tupVersion);
  } else {
    ndbrequire(false);
  }
  // fill in constant part
  req->tableId = regOperPtr->tableRef;
  req->fragId = regOperPtr->fragId;
  req->pageId = regOperPtr->realPageId;
  req->pageOffset = regOperPtr->pageOffset;
  req->tupVersion = tupVersion;
  req->opInfo = TuxMaintReq::OpRemove;
  // loop over index list
  const ArrayList<TupTriggerData>& triggerList = regTabPtr->tuxCustomTriggers;
  TriggerPtr triggerPtr;
  triggerList.first(triggerPtr);
  while (triggerPtr.i != RNIL) {
    ljam();
    req->indexId = triggerPtr.p->indexId;
    req->errorCode = RNIL;
    EXECUTE_DIRECT(DBTUX, GSN_TUX_MAINT_REQ,
        signal, TuxMaintReq::SignalLength);
    ljamEntry();
    // commit must succeed
    ndbrequire(req->errorCode == 0);
    triggerList.next(triggerPtr);
  }
}

void
Dbtup::executeTuxAbortTriggers(Signal* signal,
                               Operationrec* regOperPtr,
                               Tablerec* const regTabPtr)
{
  TuxMaintReq* const req = (TuxMaintReq*)signal->getDataPtrSend();
  // get version
  Uint32 tupVersion;
  if (regOperPtr->optype == ZINSERT) {
    ljam();
    tupVersion = regOperPtr->tupVersion;
  } else if (regOperPtr->optype == ZUPDATE) {
    ljam();
    tupVersion = regOperPtr->tupVersion;
  } else if (regOperPtr->optype == ZDELETE) {
    ljam();
    return;
  } else {
    ndbrequire(false);
  }
  // fill in constant part
  req->tableId = regOperPtr->tableRef;
  req->fragId = regOperPtr->fragId;
  req->pageId = regOperPtr->realPageId;
  req->pageOffset = regOperPtr->pageOffset;
  req->tupVersion = tupVersion;
  req->opInfo = TuxMaintReq::OpRemove;
  // loop over index list
  const ArrayList<TupTriggerData>& triggerList = regTabPtr->tuxCustomTriggers;
  TriggerPtr triggerPtr;
  triggerList.first(triggerPtr);
  while (triggerPtr.i != RNIL) {
    ljam();
    req->indexId = triggerPtr.p->indexId;
    req->errorCode = RNIL,
    EXECUTE_DIRECT(DBTUX, GSN_TUX_MAINT_REQ,
        signal, TuxMaintReq::SignalLength);
    ljamEntry();
    // abort must succeed
    ndbrequire(req->errorCode == 0);
    triggerList.next(triggerPtr);
  }
}
