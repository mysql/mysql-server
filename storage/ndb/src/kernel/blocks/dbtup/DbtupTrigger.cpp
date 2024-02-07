/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

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

#define DBTUP_C
#define DBTUP_TRIGGER_CPP
#include <ndb_limits.h>
#include <AttributeDescriptor.hpp>
#include <AttributeHeader.hpp>
#include <RefConvert.hpp>
#include <pc.hpp>
#include <signaldata/AlterIndxImpl.hpp>
#include <signaldata/CreateTrig.hpp>
#include <signaldata/CreateTrigImpl.hpp>
#include <signaldata/DropTrig.hpp>
#include <signaldata/DropTrigImpl.hpp>
#include <signaldata/FireTrigOrd.hpp>
#include <signaldata/ScanFrag.hpp>
#include <signaldata/TuxMaint.hpp>
#include "../dblqh/Dblqh.hpp"
#include "../dbtux/Dbtux.hpp"
#include "AttributeOffset.hpp"
#include "Dbtup.hpp"

#define JAM_FILE_ID 423

/* **************************************************************** */
/* ---------------------------------------------------------------- */
/* ----------------------- TRIGGER HANDLING ----------------------- */
/* ---------------------------------------------------------------- */
/* **************************************************************** */

Dbtup::TupTriggerData_list *Dbtup::findTriggerList(
    Tablerec *table, TriggerType::Value ttype, TriggerActionTime::Value ttime,
    TriggerEvent::Value tevent) {
  TupTriggerData_list *tlist = NULL;
  switch (ttype) {
    case TriggerType::SUBSCRIPTION:
    case TriggerType::SUBSCRIPTION_BEFORE:
      switch (tevent) {
        case TriggerEvent::TE_INSERT:
          jam();
          if (ttime == TriggerActionTime::TA_DETACHED)
            tlist = &table->subscriptionInsertTriggers;
          break;
        case TriggerEvent::TE_UPDATE:
          jam();
          if (ttime == TriggerActionTime::TA_DETACHED)
            tlist = &table->subscriptionUpdateTriggers;
          break;
        case TriggerEvent::TE_DELETE:
          jam();
          if (ttime == TriggerActionTime::TA_DETACHED)
            tlist = &table->subscriptionDeleteTriggers;
          break;
        default:
          break;
      }
      break;
    case TriggerType::SECONDARY_INDEX:
    case TriggerType::REORG_TRIGGER:
    case TriggerType::FULLY_REPLICATED_TRIGGER:
      switch (tevent) {
        case TriggerEvent::TE_INSERT:
          jam();
          if (ttime == TriggerActionTime::TA_AFTER)
            tlist = &table->afterInsertTriggers;
          break;
        case TriggerEvent::TE_UPDATE:
          jam();
          if (ttime == TriggerActionTime::TA_AFTER)
            tlist = &table->afterUpdateTriggers;
          break;
        case TriggerEvent::TE_DELETE:
          jam();
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
          jam();
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
          jam();
          if (ttime == TriggerActionTime::TA_AFTER)
            tlist = &table->constraintUpdateTriggers;
          break;
        default:
          break;
      }
      break;
    case TriggerType::FK_PARENT:
    case TriggerType::FK_CHILD:
      switch (tevent) {
        case TriggerEvent::TE_INSERT:
          jam();
          if (ttime == TriggerActionTime::TA_DEFERRED)
            tlist = &table->deferredInsertTriggers;
          else if (ttime == TriggerActionTime::TA_AFTER)
            tlist = &table->afterInsertTriggers;
          break;
        case TriggerEvent::TE_UPDATE:
          jam();
          if (ttime == TriggerActionTime::TA_DEFERRED)
            tlist = &table->deferredUpdateTriggers;
          else if (ttime == TriggerActionTime::TA_AFTER)
            tlist = &table->afterUpdateTriggers;
          break;
        case TriggerEvent::TE_DELETE:
          jam();
          if (ttime == TriggerActionTime::TA_DEFERRED)
            tlist = &table->deferredDeleteTriggers;
          else if (ttime == TriggerActionTime::TA_AFTER)
            tlist = &table->afterDeleteTriggers;
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
void Dbtup::execCREATE_TRIG_IMPL_REQ(Signal *signal) {
  jamEntry();
  if (!assembleFragments(signal)) {
    jam();
    return;
  }

  const CreateTrigImplReq *req =
      (const CreateTrigImplReq *)signal->getDataPtr();
  const Uint32 senderRef = req->senderRef;
  const Uint32 senderData = req->senderData;
  const Uint32 tableId = req->tableId;
  const Uint32 triggerId = req->triggerId;
  const Uint32 triggerInfo = req->triggerInfo;

  CreateTrigRef::ErrorCode error = CreateTrigRef::NoError;

  AttributeMask mask;
  SectionHandle handle(this, signal);
  if (handle.m_cnt <= CreateTrigImplReq::ATTRIBUTE_MASK_SECTION) {
    jam();
    ndbassert(false);
    error = CreateTrigRef::BadRequestType;
  } else {
    SegmentedSectionPtr ptr;
    ndbrequire(
        handle.getSection(ptr, CreateTrigImplReq::ATTRIBUTE_MASK_SECTION));
    ndbrequire(ptr.sz == mask.getSizeInWords());
    ::copy(mask.rep.data, ptr);
  }

  releaseSections(handle);

  if (error != CreateTrigRef::NoError) {
    goto err;
  }

  {
    // Find table
    TablerecPtr tabPtr;
    tabPtr.i = req->tableId;
    ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);

    if (tabPtr.p->tableStatus != DEFINED) {
      jam();
      error = CreateTrigRef::InvalidTable;
    }
    // Create trigger and associate it with the table
    else if (createTrigger(tabPtr.p, req, mask)) {
      jam();
      // Send conf
      CreateTrigImplConf *conf = (CreateTrigImplConf *)signal->getDataPtrSend();
      conf->senderRef = reference();
      conf->senderData = senderData;
      conf->tableId = tableId;
      conf->triggerId = triggerId;
      conf->triggerInfo = triggerInfo;

      sendSignal(senderRef, GSN_CREATE_TRIG_IMPL_CONF, signal,
                 CreateTrigImplConf::SignalLength, JBB);
      return;
    } else {
      jam();
      error = CreateTrigRef::TooManyTriggers;
    }
  }

err:
  ndbassert(error != CreateTrigRef::NoError);
  // Send ref
  CreateTrigImplRef *ref = (CreateTrigImplRef *)signal->getDataPtrSend();
  ref->senderRef = reference();
  ref->senderData = senderData;
  ref->tableId = tableId;
  ref->triggerId = triggerId;
  ref->triggerInfo = triggerInfo;
  ref->errorCode = error;

  sendSignal(senderRef, GSN_CREATE_TRIG_IMPL_REF, signal,
             CreateTrigImplRef::SignalLength, JBB);
}

void Dbtup::execDROP_TRIG_IMPL_REQ(Signal *signal) {
  jamEntry();
  ndbassert(!m_is_query_block);
  const DropTrigImplReq *req = (const DropTrigImplReq *)signal->getDataPtr();
  const Uint32 senderRef = req->senderRef;
  const Uint32 senderData = req->senderData;
  const Uint32 tableId = req->tableId;
  const Uint32 indexId = req->indexId;
  const Uint32 triggerId = req->triggerId;
  const Uint32 triggerInfo = req->triggerInfo;
  const Uint32 receiverRef = req->receiverRef;

  // Find table
  TablerecPtr tabPtr;
  tabPtr.i = req->tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);

  // Drop trigger
  Uint32 r = dropTrigger(tabPtr.p, req, refToBlock(receiverRef));
  if (r == 0) {
    /**
     * make sure that any trigger data is sent before DROP_TRIG_CONF
     *   NOTE: This is only needed for SUMA triggers
     *         (which are the only buffered ones) but it shouldn't
     *         be too bad to do it for all triggers...
     */
    flush_ndbmtd_suma_buffer(signal);

    // Send conf
    DropTrigImplConf *conf = (DropTrigImplConf *)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = senderData;
    conf->tableId = tableId;
    conf->triggerId = triggerId;

    sendSignal(senderRef, GSN_DROP_TRIG_IMPL_CONF, signal,
               DropTrigImplConf::SignalLength, JBB);

    // Set ordered index to Dropping in same timeslice
    TriggerType::Value ttype = TriggerInfo::getTriggerType(triggerInfo);
    if (ttype == TriggerType::ORDERED_INDEX) {
      jam();
      AlterIndxImplReq *areq = (AlterIndxImplReq *)signal->getDataPtrSend();
      areq->senderRef = 0;  // no CONF
      areq->senderData = 0;
      areq->requestType = AlterIndxImplReq::AlterIndexOffline;
      areq->tableId = tableId;
      areq->tableVersion = 0;
      areq->indexId = indexId;  // index id
      areq->indexVersion = 0;
      areq->indexType = DictTabInfo::OrderedIndex;
      EXECUTE_DIRECT(DBTUX, GSN_ALTER_INDX_IMPL_REQ, signal,
                     AlterIndxImplReq::SignalLength);
    }
  } else {
    // Send ref
    DropTrigImplRef *ref = (DropTrigImplRef *)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = senderData;
    ref->tableId = tableId;
    ref->triggerId = triggerId;
    ref->errorCode = r;
    sendSignal(senderRef, GSN_DROP_TRIG_IMPL_REF, signal,
               DropTrigImplRef::SignalLength, JBB);
  }
}

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
bool Dbtup::createTrigger(Tablerec *table, const CreateTrigImplReq *req,
                          const AttributeMask &mask) {
  if (ERROR_INSERTED(4003)) {
    CLEAR_ERROR_INSERT_VALUE;
    return false;
  }

  const Uint32 tinfo = req->triggerInfo;
  TriggerType::Value ttype = TriggerInfo::getTriggerType(tinfo);
  TriggerActionTime::Value ttime = TriggerInfo::getTriggerActionTime(tinfo);
  TriggerEvent::Value tevent = TriggerInfo::getTriggerEvent(tinfo);

  int cnt;
  struct {
    TriggerEvent::Value event;
    TupTriggerData_list *list;
    TriggerPtr ptr;
  } tmp[3];

  if (ttype == TriggerType::SECONDARY_INDEX ||
      ttype == TriggerType::REORG_TRIGGER ||
      ttype == TriggerType::FULLY_REPLICATED_TRIGGER) {
    jam();
    cnt = 3;
    tmp[0].event = TriggerEvent::TE_INSERT;
    tmp[1].event = TriggerEvent::TE_UPDATE;
    tmp[2].event = TriggerEvent::TE_DELETE;
  } else if (ttype == TriggerType::FK_PARENT) {
    jam();
    cnt = 2;
    tmp[0].event = TriggerEvent::TE_UPDATE;
    tmp[1].event = TriggerEvent::TE_DELETE;
  } else if (ttype == TriggerType::FK_CHILD) {
    jam();
    cnt = 2;
    tmp[0].event = TriggerEvent::TE_INSERT;
    tmp[1].event = TriggerEvent::TE_UPDATE;
  } else {
    jam();
    cnt = 1;
    tmp[0].event = tevent;
  }

  int i = 0;
  for (i = 0; i < cnt; i++) {
    tmp[i].list = findTriggerList(table, ttype, ttime, tmp[i].event);
    ndbrequire(tmp[i].list != NULL);

    TriggerPtr tptr;
    bool inserted;
    /**
     * FK constraints has to be checked after any SECONDARY_INDEX triggers
     * which updates the indexes possible referred by the constraints. So
     * we always insert the FK-constraint last in the list of triggers.
     */
    if (ttype == TriggerType::FK_CHILD || ttype == TriggerType::FK_PARENT)
      inserted = tmp[i].list->seizeLast(tptr);
    else
      inserted = tmp[i].list->seizeFirst(tptr);

    if (!inserted) {
      jam();
      goto err;
    }

    tmp[i].ptr = tptr;

    // Set trigger id
    tptr.p->triggerId = req->triggerId;
    tptr.p->oldTriggerIds[0] = req->upgradeExtra[0];
    tptr.p->oldTriggerIds[1] = req->upgradeExtra[1];
    tptr.p->oldTriggerIds[2] = req->upgradeExtra[2];

    // Set index id
    tptr.p->indexId = req->indexId;

    // Set trigger type etc
    tptr.p->triggerType = ttype;
    tptr.p->triggerActionTime = ttime;
    tptr.p->triggerEvent = tevent;

    tptr.p->sendBeforeValues = true;
    if ((tptr.p->triggerType == TriggerType::SUBSCRIPTION) &&
        ((tptr.p->triggerEvent == TriggerEvent::TE_UPDATE) ||
         (tptr.p->triggerEvent == TriggerEvent::TE_DELETE))) {
      jam();
      tptr.p->sendBeforeValues = false;
    }

    if (ttype == TriggerType::REORG_TRIGGER ||
        ttype == TriggerType::FULLY_REPLICATED_TRIGGER) {
      jam();
      tptr.p->sendBeforeValues = false;
    }

    /*
      tptr.p->sendOnlyChangedAttributes = false;
      if (((tptr.p->triggerType == TriggerType::SUBSCRIPTION) ||
      (tptr.p->triggerType == TriggerType::SUBSCRIPTION_BEFORE)) &&
      (tptr.p->triggerEvent == TriggerEvent::TE_UPDATE)) {
      jam();
      tptr.p->sendOnlyChangedAttributes = true;
      }
    */
    tptr.p->sendOnlyChangedAttributes =
        !TriggerInfo::getReportAllMonitoredAttributes(tinfo);

    tptr.p->monitorAllAttributes = TriggerInfo::getMonitorAllAttributes(tinfo);
    tptr.p->monitorReplicas = TriggerInfo::getMonitorReplicas(tinfo);
    tptr.p->m_receiverRef = req->receiverRef;

    if (tptr.p->monitorAllAttributes) {
      /**
       * Note that SUMA does not set up new triggers, with updated 'mask',
       * in case of a column being added to a monitored table.
       * In such cases monitorAllAttributes -> 'include those added later'
       */
      jam();
      /**
       * Set *all* attributes, including attrs possibly added later....
       * Exclude any non-character primary key attributes as they will
       * identical BEFORE & AFTER values in an UPDATE. OTOH, a char-pk
       * can be updated to an equal-by-collation-compare value.
       */
      tptr.p->attributeMask.set();
      tptr.p->attributeMask.bitANDC(table->nonCharPkAttributeMask);
    } else {
      jam();
      // Set attribute mask
      tptr.p->attributeMask = mask;
    }
  }
  return true;

err:
  for (--i; i >= 0; i--) {
    jam();
    tmp[i].list->release(tmp[i].ptr);
  }
  return false;
}  // Dbtup::createTrigger()

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
Uint32 Dbtup::dropTrigger(Tablerec *table, const DropTrigImplReq *req,
                          BlockNumber receiver) {
  if (ERROR_INSERTED(4004)) {
    CLEAR_ERROR_INSERT_VALUE;
    return 9999;
  }
  Uint32 triggerId = req->triggerId;

  const Uint32 tinfo = req->triggerInfo;
  TriggerType::Value ttype = TriggerInfo::getTriggerType(tinfo);
  TriggerActionTime::Value ttime = TriggerInfo::getTriggerActionTime(tinfo);
  TriggerEvent::Value tevent = TriggerInfo::getTriggerEvent(tinfo);

  int cnt;
  struct {
    TriggerEvent::Value event;
    TupTriggerData_list *list;
    TriggerPtr ptr;
  } tmp[3];

  if (ttype == TriggerType::SECONDARY_INDEX ||
      ttype == TriggerType::REORG_TRIGGER ||
      ttype == TriggerType::FULLY_REPLICATED_TRIGGER) {
    jam();
    cnt = 3;
    tmp[0].event = TriggerEvent::TE_INSERT;
    tmp[1].event = TriggerEvent::TE_UPDATE;
    tmp[2].event = TriggerEvent::TE_DELETE;
  } else if (ttype == TriggerType::FK_PARENT) {
    jam();
    cnt = 2;
    tmp[0].event = TriggerEvent::TE_UPDATE;
    tmp[1].event = TriggerEvent::TE_DELETE;
  } else if (ttype == TriggerType::FK_CHILD) {
    jam();
    cnt = 2;
    tmp[0].event = TriggerEvent::TE_INSERT;
    tmp[1].event = TriggerEvent::TE_UPDATE;
  } else {
    jam();
    cnt = 1;
    tmp[0].event = tevent;
  }

  int i = 0;
  for (i = 0; i < cnt; i++) {
    tmp[i].list = findTriggerList(table, ttype, ttime, tmp[i].event);
    ndbrequire(tmp[i].list != NULL);

    Ptr<TupTriggerData> ptr;
    tmp[i].ptr.setNull();
    for (tmp[i].list->first(ptr); !ptr.isNull(); tmp[i].list->next(ptr)) {
      jam();
      if (ptr.p->triggerId == triggerId) {
        if (ttype == TriggerType::SUBSCRIPTION &&
            receiver != refToBlock(ptr.p->m_receiverRef)) {
          /**
           * You can only drop your own triggers for subscription triggers.
           * Trigger IDs are private for each block.
           *
           * SUMA encodes information in the triggerId
           *
           * Backup doesn't really care about the Ids though.
           */
          jam();
          continue;
        }
        jam();
        tmp[i].ptr = ptr;
      }
    }
    if (tmp[i].ptr.isNull()) {
      jam();
      return DropTrigRef::TriggerNotFound;
    }
  }

  for (i = 0; i < cnt; i++) {
    jam();
    tmp[i].list->release(tmp[i].ptr);
  }
  return 0;
}  // Dbtup::dropTrigger()

void Dbtup::execFIRE_TRIG_REQ(Signal *signal) {
  jam();
  Uint32 opPtrI = signal->theData[0];
  Uint32 pass = signal->theData[5];

  FragrecordPtr regFragPtr;
  OperationrecPtr regOperPtr;
  TablerecPtr regTabPtr;
  KeyReqStruct req_struct(this,
                          (When)(KRS_PRE_COMMIT_BASE +
                                 (pass & TriggerPreCommitPass::TPCP_PASS_MAX)));

  regOperPtr.i = opPtrI;

  jamEntry();

  ndbrequire(c_operation_pool.getValidPtr(regOperPtr));

  regFragPtr.i = regOperPtr.p->fragmentPtr;
  Uint32 no_of_fragrec = cnoOfFragrec;
  ptrCheckGuard(regFragPtr, no_of_fragrec, fragrecord);

  TransState trans_state = get_trans_state(regOperPtr.p);
  ndbrequire(trans_state == TRANS_STARTED);

  Uint32 no_of_tablerec = cnoOfTablerec;
  regTabPtr.i = regFragPtr.p->fragTableId;
  ptrCheckGuard(regTabPtr, no_of_tablerec, tablerec);

  req_struct.signal = signal;
  req_struct.TC_ref = signal->theData[1];
  req_struct.TC_index = signal->theData[2];
  req_struct.trans_id1 = signal->theData[3];
  req_struct.trans_id2 = signal->theData[4];
  req_struct.m_reorg = regOperPtr.p->op_struct.bit_field.m_reorg;
  req_struct.m_deferred_constraints =
      regOperPtr.p->op_struct.bit_field.m_deferred_constraints;

  PagePtr page;
  Tuple_header *tuple_ptr = (Tuple_header *)get_ptr(
      &page, &regOperPtr.p->m_tuple_location, regTabPtr.p);
  req_struct.m_tuple_ptr = tuple_ptr;

  OperationrecPtr lastOperPtr;
  lastOperPtr.i = tuple_ptr->m_operation_ptr_i;
  ndbrequire(c_operation_pool.getValidPtr(lastOperPtr));
  ndbassert(regOperPtr.p->op_struct.bit_field.m_reorg ==
            lastOperPtr.p->op_struct.bit_field.m_reorg);

  /**
   * Deferred triggers should fire only once per primary key (per pass)
   *   regardless of no of DML on that primary key
   *
   * We keep track of this on *last* operation (which btw, implies that
   *   a trigger can't update "own" tuple...i.e first op would be better...)
   */
  if (!c_lqh->check_fire_trig_pass(lastOperPtr.p->userpointer, pass)) {
    jam();
    signal->theData[0] = 0;
    signal->theData[1] = 0;
    return;
  }

  /**
   * This is deferred triggers...
   *   which is basically the same as detached,
   *     i.e before value is <before transaction>
   *     and after values is <after transaction>
   *   with the difference that they execute (fire) while
   *   still having a transaction context...
   *   i.e can abort transactions, modify transaction
   */
  req_struct.num_fired_triggers = 0;

  /**
   * See DbtupCommit re "Setting the op-list has this effect"
   */
  Uint32 save[2] = {lastOperPtr.p->nextActiveOp, lastOperPtr.p->prevActiveOp};
  lastOperPtr.p->nextActiveOp = RNIL;
  lastOperPtr.p->prevActiveOp = RNIL;

  checkDeferredTriggers(&req_struct, lastOperPtr.p, regTabPtr.p, false);

  lastOperPtr.p->nextActiveOp = save[0];
  lastOperPtr.p->prevActiveOp = save[1];

  signal->theData[0] = 0;
  signal->theData[1] = req_struct.num_fired_triggers;
}

/* ---------------------------------------------------------------- */
/* -------------- checkImmediateTriggersAfterOp ------------------ */
/*                                                                  */
/* Called after an insert, delete, or update operation takes        */
/* place. Fetches before tuple for deletes and updates and          */
/* after tuple for inserts and updates.                             */
/* Executes immediate triggers by sending FIRETRIGORD               */
/*                                                                  */
/* ---------------------------------------------------------------- */
void Dbtup::checkImmediateTriggersAfterInsert(KeyReqStruct *req_struct,
                                              Operationrec *regOperPtr,
                                              Tablerec *regTablePtr,
                                              bool disk) {
  if (refToMain(req_struct->TC_ref) != DBTC) {
    return;
  }

  if (regOperPtr->op_struct.bit_field.m_triggers ==
      TupKeyReq::OP_PRIMARY_REPLICA) {
    if (!regTablePtr->afterInsertTriggers.isEmpty()) {
      jam();
      fireImmediateTriggers(req_struct, regTablePtr->afterInsertTriggers,
                            regOperPtr, disk);
    }

    if (!regTablePtr->deferredInsertTriggers.isEmpty()) {
      checkDeferredTriggersDuringPrepare(
          req_struct, regTablePtr->deferredInsertTriggers, regOperPtr, disk);
    }
  }
}

void Dbtup::checkImmediateTriggersAfterUpdate(KeyReqStruct *req_struct,
                                              Operationrec *regOperPtr,
                                              Tablerec *regTablePtr,
                                              bool disk) {
  if (refToMain(req_struct->TC_ref) != DBTC) {
    return;
  }

  if (regOperPtr->op_struct.bit_field.m_triggers ==
      TupKeyReq::OP_PRIMARY_REPLICA) {
    if (!regTablePtr->afterUpdateTriggers.isEmpty()) {
      jam();
      fireImmediateTriggers(req_struct, regTablePtr->afterUpdateTriggers,
                            regOperPtr, disk);
    }

    if (!regTablePtr->constraintUpdateTriggers.isEmpty()) {
      jam();
      fireImmediateTriggers(req_struct, regTablePtr->constraintUpdateTriggers,
                            regOperPtr, disk);
    }

    if (!regTablePtr->deferredUpdateTriggers.isEmpty()) {
      jam();
      checkDeferredTriggersDuringPrepare(
          req_struct, regTablePtr->deferredUpdateTriggers, regOperPtr, disk);
    }
  }
}

void Dbtup::checkImmediateTriggersAfterDelete(KeyReqStruct *req_struct,
                                              Operationrec *regOperPtr,
                                              Tablerec *regTablePtr,
                                              bool disk) {
  if (refToMain(req_struct->TC_ref) != DBTC) {
    return;
  }

  if (regOperPtr->op_struct.bit_field.m_triggers ==
      TupKeyReq::OP_PRIMARY_REPLICA) {
    if (!regTablePtr->afterDeleteTriggers.isEmpty()) {
      fireImmediateTriggers(req_struct, regTablePtr->afterDeleteTriggers,
                            regOperPtr, disk);
    }

    if (!regTablePtr->deferredDeleteTriggers.isEmpty()) {
      checkDeferredTriggersDuringPrepare(
          req_struct, regTablePtr->deferredDeleteTriggers, regOperPtr, disk);
    }
  }
}

void Dbtup::checkDeferredTriggersDuringPrepare(KeyReqStruct *req_struct,
                                               TupTriggerData_list &triggerList,
                                               Operationrec *const regOperPtr,
                                               bool disk) {
  jam();
  TriggerPtr trigPtr;
  triggerList.first(trigPtr);
  while (trigPtr.i != RNIL) {
    jam();
    if (trigPtr.p->monitorAllAttributes ||
        trigPtr.p->attributeMask.overlaps(req_struct->changeMask)) {
      jam();
      switch (trigPtr.p->triggerType) {
        case TriggerType::SECONDARY_INDEX:
          jam();
          NoOfFiredTriggers::setDeferredUKBit(req_struct->num_fired_triggers);
          break;
        case TriggerType::FK_PARENT:
        case TriggerType::FK_CHILD:
          jam();
          NoOfFiredTriggers::setDeferredFKBit(req_struct->num_fired_triggers);
          break;
        default:
          jam();
          ndbassert(false);
      }
      if (NoOfFiredTriggers::getDeferredAllSet(
              req_struct->num_fired_triggers)) {
        jam();
        return;
      }
    }
    triggerList.next(trigPtr);
  }
}

/* ---------------------------------------------------------------- */
/* --------------------- checkDeferredTriggers -------------------- */
/*                                                                  */
/* Called before commit after an insert, delete, or update          */
/* operation. Fetches before tuple for deletes and updates and      */
/* after tuple for inserts and updates.                             */
/* Executes deferred triggers by sending FIRETRIGORD                */
/*                                                                  */
/* ---------------------------------------------------------------- */
void Dbtup::checkDeferredTriggers(KeyReqStruct *req_struct,
                                  Operationrec *regOperPtr,
                                  Tablerec *regTablePtr, bool disk) {
  jam();
  Uint32 save_type = regOperPtr->op_type;
  Tuple_header *save_ptr = req_struct->m_tuple_ptr;
  TupTriggerData_list *deferred_list = 0;
  TupTriggerData_list *constraint_list = 0;

  switch (save_type) {
    case ZUPDATE:
    case ZINSERT:
      jam();
      req_struct->m_tuple_ptr =
          get_copy_tuple(&regOperPtr->m_copy_tuple_location);
      break;
  }

  /**
   * Set correct operation type and fix change mask
   * Note ALLOC is set in "orig" tuple
   */
  if (save_ptr->m_header_bits & Tuple_header::ALLOC) {
    if (save_type == ZDELETE) {
      // insert + delete = nothing
      jam();
      return;
      goto end;
    }
    jam();
    regOperPtr->op_type = ZINSERT;
  } else if (save_type == ZINSERT) {
    /**
     * Tuple was not created but last op is INSERT.
     * This is possible only on DELETE + INSERT
     */
    jam();
    regOperPtr->op_type = ZUPDATE;
  }

  switch (regOperPtr->op_type) {
    case (ZINSERT):
      jam();
      deferred_list = &regTablePtr->deferredInsertTriggers;
      constraint_list = &regTablePtr->afterInsertTriggers;
      break;
    case (ZDELETE):
      jam();
      deferred_list = &regTablePtr->deferredDeleteTriggers;
      constraint_list = &regTablePtr->afterDeleteTriggers;
      break;
    case (ZUPDATE):
      jam();
      deferred_list = &regTablePtr->deferredUpdateTriggers;
      constraint_list = &regTablePtr->afterUpdateTriggers;
      break;
    default:
      ndbabort();
  }

  if (deferred_list->isEmpty() &&
      (!req_struct->m_deferred_constraints || constraint_list->isEmpty())) {
    jam();
    goto end;
  }

  /**
   * Compute change-mask
   */
  set_commit_change_mask_info(regTablePtr, req_struct, regOperPtr);

  /**
   * Note that there are two variants of deferred trigger/constraints:
   * 1) Triggers created by a 'NO ACTION' foreign key are deferred by
   *    declaration, and managed by deferred<Op>Triggers list.
   *    These are always fired at commit time (below)
   * 2) Any 'immediate' constraints in after<Op>Triggers may be
   *    deferred by setting 'TupKeyReq::deferred_constraints'.
   *    These should be conditionally fired here only if not
   *    already handled 'immediate'.
   */
  if (!deferred_list->isEmpty()) {
    jam();
    fireDeferredTriggers(req_struct, *deferred_list, regOperPtr, disk);
  }

  if (req_struct->m_deferred_constraints && !constraint_list->isEmpty()) {
    jam();
    fireDeferredConstraints(req_struct, *constraint_list, regOperPtr, disk);
  }

end:
  regOperPtr->op_type = save_type;
  req_struct->m_tuple_ptr = save_ptr;
}  // Dbtup::checkDeferredTriggers()

/* ---------------------------------------------------------------- */
/* --------------------- checkDetachedTriggers -------------------- */
/*                                                                  */
/* Called at commit after an insert, delete, or update operation.   */
/* Fetches before tuple for deletes and updates and                 */
/* after tuple for inserts and updates.                             */
/* Executes detached triggers by sending FIRETRIGORD                */
/*                                                                  */
/* ---------------------------------------------------------------- */
void Dbtup::checkDetachedTriggers(KeyReqStruct *req_struct,
                                  Operationrec *regOperPtr,
                                  Tablerec *regTablePtr, bool disk,
                                  Uint32 diskPagePtrI) {
  Uint32 save_type = regOperPtr->op_type;
  Tuple_header *save_ptr = req_struct->m_tuple_ptr;

  switch (save_type) {
    case ZUPDATE:
    case ZINSERT:
    case ZREFRESH:
      req_struct->m_tuple_ptr =
          get_copy_tuple(&regOperPtr->m_copy_tuple_location);
      break;
  }

  /**
   * Set correct operation type and fix change mask
   * Note ALLOC is set in "orig" tuple
   */
  if (save_ptr->m_header_bits & Tuple_header::ALLOC) {
    if (save_type == ZDELETE) {
      // insert + delete = nothing
      jam();
      return;
    } else if (save_type != ZREFRESH) {
      regOperPtr->op_type = ZINSERT;
    }
  } else if (save_type == ZINSERT) {
    /**
     * Tuple was not created but last op is INSERT.
     * This is possible only on DELETE + INSERT
     */
    regOperPtr->op_type = ZUPDATE;
  }

  switch (regOperPtr->op_type) {
    case (ZINSERT):
      jam();
      if (regTablePtr->subscriptionInsertTriggers.isEmpty()) {
        // Table has no active triggers monitoring inserts at commit
        jam();
        goto end;
      }

      // If any fired immediate insert trigger then fetch after tuple
      fireDetachedTriggers(req_struct, regTablePtr->subscriptionInsertTriggers,
                           regOperPtr, disk, diskPagePtrI);
      break;
    case (ZDELETE):
      jam();
      if (regTablePtr->subscriptionDeleteTriggers.isEmpty()) {
        // Table has no active triggers monitoring deletes at commit
        jam();
        goto end;
      }

      // Execute any after delete triggers by sending
      // FIRETRIGORD with the before tuple
      fireDetachedTriggers(req_struct, regTablePtr->subscriptionDeleteTriggers,
                           regOperPtr, disk, diskPagePtrI);
      break;
    case (ZUPDATE):
      jam();
      if (regTablePtr->subscriptionUpdateTriggers.isEmpty()) {
        // Table has no active triggers monitoring updates at commit
        jam();
        goto end;
      }

      // If any fired immediate update trigger then fetch after tuple
      // and send two FIRETRIGORD one with before tuple and one with after tuple
      fireDetachedTriggers(req_struct, regTablePtr->subscriptionUpdateTriggers,
                           regOperPtr, disk, diskPagePtrI);
      break;
    case ZREFRESH:
      jam();
      /* Depending on the Refresh scenario, fire Delete or Insert
       * triggers to simulate the effect of arriving at the tuple's
       * current state.
       */
      switch (regOperPtr->m_copy_tuple_location.m_file_no) {
        case Operationrec::RF_SINGLE_NOT_EXIST:
        case Operationrec::RF_MULTI_NOT_EXIST:
          fireDetachedTriggers(req_struct,
                               regTablePtr->subscriptionDeleteTriggers,
                               regOperPtr, disk, diskPagePtrI);
          break;
        case Operationrec::RF_SINGLE_EXIST:
        case Operationrec::RF_MULTI_EXIST:
          fireDetachedTriggers(req_struct,
                               regTablePtr->subscriptionInsertTriggers,
                               regOperPtr, disk, diskPagePtrI);
          break;
        default:
          ndbabort();
      }
      break;
    default:
      ndbabort();
  }

end:
  regOperPtr->op_type = save_type;
  req_struct->m_tuple_ptr = save_ptr;
}

static bool is_constraint(const Dbtup::TupTriggerData *trigPtr) {
  return (trigPtr->triggerType == TriggerType::SECONDARY_INDEX) ||
         (trigPtr->triggerType == TriggerType::FK_PARENT) ||
         (trigPtr->triggerType == TriggerType::FK_CHILD);
}

void Dbtup::fireImmediateTriggers(KeyReqStruct *req_struct,
                                  TupTriggerData_list &triggerList,
                                  Operationrec *const regOperPtr, bool disk) {
  TriggerPtr trigPtr;
  triggerList.first(trigPtr);
  while (trigPtr.i != RNIL) {
    jam();
    if (trigPtr.p->monitorAllAttributes ||
        trigPtr.p->attributeMask.overlaps(req_struct->changeMask)) {
      jam();

      if (req_struct->m_when == KRS_PREPARE &&
          req_struct->m_deferred_constraints && is_constraint(trigPtr.p)) {
        switch (trigPtr.p->triggerType) {
          case TriggerType::SECONDARY_INDEX:
            NoOfFiredTriggers::setDeferredUKBit(req_struct->num_fired_triggers);
            break;
          case TriggerType::FK_PARENT:
          case TriggerType::FK_CHILD:
            NoOfFiredTriggers::setDeferredFKBit(req_struct->num_fired_triggers);
            break;
          default:
            ndbassert(false);
        }
      } else {
        executeTrigger(req_struct, trigPtr.p, regOperPtr, disk);
      }
    }
    triggerList.next(trigPtr);
  }  // while
}  // Dbtup::fireImmediateTriggers()

void Dbtup::fireDeferredConstraints(KeyReqStruct *req_struct,
                                    TupTriggerData_list &triggerList,
                                    Operationrec *const regOperPtr, bool disk) {
  TriggerPtr trigPtr;
  triggerList.first(trigPtr);
  while (trigPtr.i != RNIL) {
    jam();

    if (trigPtr.p->monitorAllAttributes ||
        trigPtr.p->attributeMask.overlaps(req_struct->changeMask)) {
      jam();
      switch (trigPtr.p->triggerType) {
        case TriggerType::SECONDARY_INDEX:
        case TriggerType::FK_PARENT:
        case TriggerType::FK_CHILD:
          jam();
          /**
           * Unique index triggers have to do pre-commit checks when
           * running in a slave cluster.
           * Also foreign key triggers are handled in pre-commit stage.
           */
          executeTrigger(req_struct, trigPtr.p, regOperPtr, disk);
          break;
        case TriggerType::FULLY_REPLICATED_TRIGGER:
        case TriggerType::REORG_TRIGGER:
          /**
           * Fully replicated triggers and reorg triggers should not be
           * executed in pre-commit phase since they are about replicating
           * writes and not about pre-commit checks.
           */
          jam();
          break;
        default:
          ndbabort();
      }
    }  // if
    triggerList.next(trigPtr);
  }  // while
}  // Dbtup::fireDeferredConstraints()

void Dbtup::fireDeferredTriggers(KeyReqStruct *req_struct,
                                 TupTriggerData_list &triggerList,
                                 Operationrec *const regOperPtr, bool disk) {
  TriggerPtr trigPtr;
  triggerList.first(trigPtr);
  while (trigPtr.i != RNIL) {
    jam();
    if (trigPtr.p->monitorAllAttributes ||
        trigPtr.p->attributeMask.overlaps(req_struct->changeMask)) {
      jam();
      executeTrigger(req_struct, trigPtr.p, regOperPtr, disk);
    }  // if
    triggerList.next(trigPtr);
  }  // while
}  // Dbtup::fireDeferredTriggers()

void Dbtup::fireDetachedTriggers(KeyReqStruct *req_struct,
                                 TupTriggerData_list &triggerList,
                                 Operationrec *const regOperPtr, bool disk,
                                 Uint32 diskPagePtrI) {
  TriggerPtr trigPtr;

  /**
   * Set disk page
   */
  req_struct->m_disk_page_ptr.i = diskPagePtrI;

  ndbrequire(regOperPtr->is_first_operation());
  triggerList.first(trigPtr);
  while (trigPtr.i != RNIL) {
    jam();
    if ((trigPtr.p->monitorReplicas ||
         regOperPtr->op_struct.bit_field.m_triggers ==
             TupKeyReq::OP_PRIMARY_REPLICA) &&
        (trigPtr.p->monitorAllAttributes ||
         trigPtr.p->attributeMask.overlaps(req_struct->changeMask))) {
      jam();
      executeTrigger(req_struct, trigPtr.p, regOperPtr, disk);
    }
    triggerList.next(trigPtr);
  }
}

bool Dbtup::check_fire_trigger(const Fragrecord *fragPtrP,
                               const TupTriggerData *trigPtrP,
                               const KeyReqStruct *req_struct,
                               const Operationrec *regOperPtr) const {
  jam();

  if (trigPtrP->triggerType == TriggerType::SUBSCRIPTION_BEFORE) {
    if (!check_fire_suma(req_struct, regOperPtr, fragPtrP)) return false;
    return true;
  }

  switch (fragPtrP->fragStatus) {
    case Fragrecord::FS_REORG_NEW:
      jam();
      return false;
    case Fragrecord::FS_REORG_COMMIT:
    case Fragrecord::FS_REORG_COMPLETE:
      return req_struct->m_reorg == ScanFragReq::REORG_ALL;
    default:
      return true;
  }
}

bool Dbtup::check_fire_fully_replicated(const KeyReqStruct *req_struct,
                                        Fragrecord::FragState state) const {
  switch (state) {
    case Fragrecord::FS_ONLINE:
    case Fragrecord::FS_REORG_COMMIT:
    case Fragrecord::FS_REORG_COMPLETE: {
      jam();
      /**
       * This is the normal operations that come through the main
       * fragment, it should not happen on non-main fragments.
       */
      return true;
    }
    case Fragrecord::FS_REORG_NEW:
      jam();
      /**
       * This is the special fully replicated trigger which fires on
       * the first new fragment in an ALTER TABLE reorg (first new
       * fragment is always on the first new node group).
       * This only happens in the copy phase of the ALTER TABLE reorg
       * for fully replicated tables.
       */
      return true;
    case Fragrecord::FS_REORG_COMMIT_NEW:
    case Fragrecord::FS_REORG_COMPLETE_NEW:
      jam();
      /**
       * Reorg scan is done, so no more triggers should fire
       * here, we're kept up-to-date by the fully replicated
       * trigger firing from the main fragment from here and
       * onwards.
       */
      ndbabort();
      return true;
    default:
      break;
  }
  ndbabort();
  return false;
}

bool Dbtup::check_fire_reorg(const KeyReqStruct *req_struct,
                             Fragrecord::FragState state) const {
  Uint32 flag = req_struct->m_reorg;
  switch (state) {
    case Fragrecord::FS_ONLINE:
    case Fragrecord::FS_REORG_COMMIT_NEW:
    case Fragrecord::FS_REORG_COMPLETE_NEW:
      jam();
      if ((flag == ScanFragReq::REORG_MOVED) ||
          (flag == ScanFragReq::REORG_MOVED_COPY)) {
        jam();
        return true;
      }
      return false;
    case Fragrecord::FS_REORG_NEW:
    case Fragrecord::FS_REORG_COMMIT:
    case Fragrecord::FS_REORG_COMPLETE:
    default:
      jam();
      return false;
  }
}

bool Dbtup::check_fire_suma(const KeyReqStruct *req_struct,
                            const Operationrec *opPtrP,
                            const Fragrecord *regFragPtrP) const {
  Ptr<Tablerec> tablePtr;
  tablePtr.i = regFragPtrP->fragTableId;
  Fragrecord::FragState state = regFragPtrP->fragStatus;
  Uint32 gci_hi = req_struct->gci_hi;
  Uint32 flag = opPtrP->op_struct.bit_field.m_reorg;

  switch (state) {
    case Fragrecord::FS_FREE:
      ndbassert(false);
      return false;
    case Fragrecord::FS_ONLINE:
      jam();
      if (flag == ScanFragReq::REORG_MOVED_COPY) {
        /* Don't fire SUMA triggers */
        return false;
      }

      return true;
    case Fragrecord::FS_REORG_NEW:
      jam();
      return false;
    case Fragrecord::FS_REORG_COMMIT_NEW:
      jam();
      return false;
    case Fragrecord::FS_REORG_COMPLETE_NEW:
      jam();
      return true;
    case Fragrecord::FS_REORG_COMMIT:
      jam();
      return true;
    case Fragrecord::FS_REORG_COMPLETE:
      jam();
      if (flag != ScanFragReq::REORG_NOT_MOVED) {
        jam();
        return true;
      }
      break;
  }

  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);
  if (gci_hi < tablePtr.p->m_reorg_suma_filter.m_gci_hi) {
    jam();
    return true;
  }

  return false;
}

Uint32 Dbtup::getOldTriggerId(const TupTriggerData *trigPtrP, Uint32 op_type) {
  switch (op_type) {
    case ZINSERT:
      return trigPtrP->oldTriggerIds[0];
    case ZUPDATE:
      return trigPtrP->oldTriggerIds[1];
    case ZDELETE:
      return trigPtrP->oldTriggerIds[2];
  }
  ndbabort();
  return RNIL;
}

void Dbtup::sendBatchedFIRE_TRIG_ORD(Signal *signal, Uint32 ref, Uint32 siglen,
                                     SectionHandle *handle) {
  jam();
  const Uint32 version = getNodeInfo(refToNode(ref)).m_version;
  if (ndbd_frag_fire_trig_ord(version)) {
    jam();
    sendBatchedFragmentedSignal(ref, GSN_FIRE_TRIG_ORD, signal, siglen, JBB,
                                handle, false);
  } else {
    jam();
    sendSignal(ref, GSN_FIRE_TRIG_ORD, signal, siglen, JBB, handle);
  }
}

void Dbtup::sendBatchedFIRE_TRIG_ORD(Signal *signal, Uint32 ref, Uint32 siglen,
                                     LinearSectionPtr ptr[], Uint32 nptr) {
  const Uint32 version = getNodeInfo(refToNode(ref)).m_version;
  if (ndbd_frag_fire_trig_ord(version)) {
    jam();
    sendBatchedFragmentedSignal(ref, GSN_FIRE_TRIG_ORD, signal, siglen, JBB,
                                ptr, nptr);
  } else {
    jam();
    sendSignal(ref, GSN_FIRE_TRIG_ORD, signal, siglen, JBB, ptr, nptr);
  }
}

#define ZOUT_OF_LONG_SIGNAL_MEMORY_IN_TRIGGER 312

void Dbtup::executeTrigger(KeyReqStruct *req_struct,
                           TupTriggerData *const trigPtr,
                           Operationrec *const regOperPtr, bool disk) {
  Signal *signal = req_struct->signal;
  BlockReference ref = trigPtr->m_receiverRef;
  Uint32 triggerType = trigPtr->triggerType;

  if ((triggerType == TriggerType::FK_PARENT ||
       triggerType == TriggerType::FK_CHILD) &&
      regOperPtr->op_struct.bit_field.m_disable_fk_checks) {
    jam();
    return;
  }

  FragrecordPtr regFragPtr;
  regFragPtr.i = regOperPtr->fragmentPtr;
  ptrCheckGuard(regFragPtr, cnoOfFragrec, fragrecord);
  Fragrecord::FragState fragstatus = regFragPtr.p->fragStatus;

  if (refToMain(ref) == getBACKUP()) {
    jam();
    if (isNdbMtLqh()) {
      goto out;
    }

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
    signal->theData[1] = regFragPtr.p->fragmentId;
    EXECUTE_DIRECT(getBACKUP(), GSN_BACKUP_TRIG_REQ, signal, 2);
    jamEntry();
    if (signal->theData[0] == 0) {
      jam();
      return;
    }
  out:
    (void)1;
  } else if (unlikely(triggerType == TriggerType::REORG_TRIGGER)) {
    if (!check_fire_reorg(req_struct, fragstatus)) {
      jam();
      return;
    }
    jam();
  } else if (unlikely(triggerType == TriggerType::FULLY_REPLICATED_TRIGGER)) {
    if (!check_fire_fully_replicated(req_struct, fragstatus)) {
      jam();
      return;
    }
    jam();
  } else if (unlikely(regFragPtr.p->fragStatus != Fragrecord::FS_ONLINE ||
                      req_struct->m_reorg == ScanFragReq::REORG_MOVED_COPY)) {
    if (!check_fire_trigger(regFragPtr.p, trigPtr, req_struct, regOperPtr)) {
      jam();
      return;
    }
    jam();
  } else {
    jam();
    jamLine((Uint16)triggerType);
  }

  Uint32 noPrimKey, noAfterWords, noBeforeWords;
  Uint32 *const keyBuffer = &cinBuffer[0];
  Uint32 *const afterBuffer = &coutBuffer[0];
  Uint32 *const beforeBuffer = &clogMemBuffer[0];
  if (!readTriggerInfo(trigPtr, regOperPtr, req_struct, regFragPtr.p, keyBuffer,
                       noPrimKey, afterBuffer, noAfterWords, beforeBuffer,
                       noBeforeWords, disk)) {
    jam();
    return;
  }

  //--------------------------------------------------------------------
  // Now all data for this trigger has been read. It is now time to send
  // the trigger information consisting of two or three sets of TRIG_
  // ATTRINFO signals and one FIRE_TRIG_ORD signal.
  // We start by setting common header info for all TRIG_ATTRINFO signals.
  //--------------------------------------------------------------------
  bool executeDirect;
  bool longsignal = false;
  bool detached = false;
  Uint32 triggerId = trigPtr->triggerId;
  TrigAttrInfo *const trigAttrInfo = (TrigAttrInfo *)signal->getDataPtrSend();
  trigAttrInfo->setConnectionPtr(req_struct->TC_index);
  trigAttrInfo->setTriggerId(trigPtr->triggerId);

  switch (triggerType) {
    case (TriggerType::SECONDARY_INDEX): {
      jam();
      /**
       * Handle stupid 6.3 which uses one triggerId per operation type
       */
      Uint32 node = refToNode(req_struct->TC_ref);
      if (unlikely(node &&
                   getNodeInfo(node).m_version < MAKE_VERSION(6, 4, 0))) {
        jam();
        triggerId = getOldTriggerId(trigPtr, regOperPtr->op_type);
        trigAttrInfo->setTriggerId(triggerId);
      }
    }
      [[fallthrough]];
    case (TriggerType::REORG_TRIGGER):
    case (TriggerType::FK_PARENT):
    case (TriggerType::FK_CHILD):
    case (TriggerType::FULLY_REPLICATED_TRIGGER):
      jam();
      ref = req_struct->TC_ref;
      executeDirect = false;
      longsignal = true;
      break;
    case (TriggerType::SUBSCRIPTION):
    case (TriggerType::SUBSCRIPTION_BEFORE):
      jam();
      // Since only backup uses subscription triggers we send to backup directly
      // for now
      ref = trigPtr->m_receiverRef;
      // executeDirect = !isNdbMtLqh() || (refToMain(ref) != SUMA);
      executeDirect = refToInstance(ref) == instance();

      // If we can do execute direct, lets do that, else do long signal (only
      // local node)
      longsignal = !executeDirect;
      ndbassert(refToNode(ref) == 0 || refToNode(ref) == getOwnNodeId());
      detached = true;
      break;
    case (TriggerType::READ_ONLY_CONSTRAINT):
      terrorCode = ZREAD_ONLY_CONSTRAINT_VIOLATION;
      // XXX should return status and abort the rest
      return;
    default:
      ndbabort();
      executeDirect = false;  // remove warning
  }                           // switch

  if (ERROR_INSERTED(4030)) {
    terrorCode = ZREAD_ONLY_CONSTRAINT_VIOLATION;
    // XXX should return status and abort the rest
    return;
  }

  if (triggerType == TriggerType::SECONDARY_INDEX &&
      req_struct->m_when != KRS_PREPARE) {
    ndbrequire(req_struct->m_deferred_constraints);
    if (req_struct->m_when == KRS_UK_PRE_COMMIT0) {
      switch (regOperPtr->op_type) {
        case ZINSERT:
          NoOfFiredTriggers::setDeferredUKBit(req_struct->num_fired_triggers);
          return;
          break;
        case ZUPDATE:
          NoOfFiredTriggers::setDeferredUKBit(req_struct->num_fired_triggers);
          noAfterWords = 0;
          break;
        case ZDELETE:
          break;
        default:
          ndbabort();
      }
    } else if (req_struct->m_when == KRS_UK_PRE_COMMIT1) {
      switch (regOperPtr->op_type) {
        case ZINSERT:
          break;
        case ZUPDATE:
          noBeforeWords = 0;
          break;
        case ZDELETE:
          return;
        default:
          ndbabort();
      }
    } else {
      ndbassert(req_struct->m_when == KRS_FK_PRE_COMMIT);
      return;
    }
  }

  if ((triggerType == TriggerType::FK_PARENT ||
       triggerType == TriggerType::FK_CHILD) &&
      req_struct->m_when != KRS_PREPARE) {
    if (req_struct->m_when != KRS_FK_PRE_COMMIT) {
      return;
    }
  }

  req_struct->num_fired_triggers++;

  if (longsignal == false) {
    jam();

    trigAttrInfo->setAttrInfoType(TrigAttrInfo::PRIMARY_KEY);
    sendTrigAttrInfo(signal, keyBuffer, noPrimKey, executeDirect, ref);

    switch (regOperPtr->op_type) {
      case (ZINSERT):
      is_insert:
        jam();
        // Send AttrInfo signals with new attribute values
        trigAttrInfo->setAttrInfoType(TrigAttrInfo::AFTER_VALUES);
        sendTrigAttrInfo(signal, afterBuffer, noAfterWords, executeDirect, ref);
        break;
      case (ZDELETE):
      is_delete:
        if (trigPtr->sendBeforeValues) {
          jam();
          trigAttrInfo->setAttrInfoType(TrigAttrInfo::BEFORE_VALUES);
          sendTrigAttrInfo(signal, beforeBuffer, noBeforeWords, executeDirect,
                           ref);
        }
        break;
      case (ZUPDATE):
        jam();
        if (trigPtr->sendBeforeValues) {
          jam();
          trigAttrInfo->setAttrInfoType(TrigAttrInfo::BEFORE_VALUES);
          sendTrigAttrInfo(signal, beforeBuffer, noBeforeWords, executeDirect,
                           ref);
        }
        trigAttrInfo->setAttrInfoType(TrigAttrInfo::AFTER_VALUES);
        sendTrigAttrInfo(signal, afterBuffer, noAfterWords, executeDirect, ref);
        break;
      case ZREFRESH:
        jam();
        /* Reuse Insert/Delete trigger firing code as necessary */
        switch (regOperPtr->m_copy_tuple_location.m_file_no) {
          case Operationrec::RF_SINGLE_NOT_EXIST:
            jam();
            [[fallthrough]];
          case Operationrec::RF_MULTI_NOT_EXIST:
            jam();
            goto is_delete;
          case Operationrec::RF_SINGLE_EXIST:
            jam();
            [[fallthrough]];
          case Operationrec::RF_MULTI_EXIST:
            jam();
            goto is_insert;
          default:
            ndbabort();
        }
      default:
        ndbabort();
    }
  }

  /**
   * sendFireTrigOrd
   */
  FireTrigOrd *const fireTrigOrd = (FireTrigOrd *)signal->getDataPtrSend();

  fireTrigOrd->setConnectionPtr(req_struct->TC_index);
  fireTrigOrd->setTriggerId(triggerId);
  fireTrigOrd->fragId = regFragPtr.p->fragmentId;
  fireTrigOrd->setUserRef(reference());

  switch (regOperPtr->op_type) {
    case (ZINSERT):
      jam();
      fireTrigOrd->m_triggerEvent = TriggerEvent::TE_INSERT;
      break;
    case (ZUPDATE):
      jam();
      fireTrigOrd->m_triggerEvent = TriggerEvent::TE_UPDATE;
      break;
    case (ZDELETE):
      jam();
      fireTrigOrd->m_triggerEvent = TriggerEvent::TE_DELETE;
      break;
    case ZREFRESH:
      jam();
      switch (regOperPtr->m_copy_tuple_location.m_file_no) {
        case Operationrec::RF_SINGLE_NOT_EXIST:
          jam();
          [[fallthrough]];
        case Operationrec::RF_MULTI_NOT_EXIST:
          jam();
          fireTrigOrd->m_triggerEvent = TriggerEvent::TE_DELETE;
          break;
        case Operationrec::RF_SINGLE_EXIST:
          jam();
          [[fallthrough]];
        case Operationrec::RF_MULTI_EXIST:
          jam();
          fireTrigOrd->m_triggerEvent = TriggerEvent::TE_INSERT;
          break;
        default:
          ndbabort();
      }
      break;
    default:
      ndbabort();
  }

  fireTrigOrd->setNoOfPrimaryKeyWords(noPrimKey);
  fireTrigOrd->setNoOfBeforeValueWords(noBeforeWords);
  fireTrigOrd->setNoOfAfterValueWords(noAfterWords);

  LinearSectionPtr ptr[3];
  ptr[0].p = keyBuffer;
  ptr[0].sz = noPrimKey;
  ptr[1].p = beforeBuffer;
  ptr[1].sz = noBeforeWords;
  ptr[2].p = afterBuffer;
  ptr[2].sz = noAfterWords;

  SectionHandle handle(this);
  if (longsignal && !detached && !import(&handle, ptr, 3)) {
    jam();
    terrorCode = ZOUT_OF_LONG_SIGNAL_MEMORY_IN_TRIGGER;
    return;
  }

  switch (trigPtr->triggerType) {
    case (TriggerType::SECONDARY_INDEX):
    case (TriggerType::REORG_TRIGGER):
    case (TriggerType::FULLY_REPLICATED_TRIGGER):
    case (TriggerType::FK_PARENT):
    case (TriggerType::FK_CHILD):
      jam();
      fireTrigOrd->m_triggerType = trigPtr->triggerType;
      fireTrigOrd->m_transId1 = req_struct->trans_id1;
      fireTrigOrd->m_transId2 = req_struct->trans_id2;
      sendBatchedFIRE_TRIG_ORD(signal, req_struct->TC_ref,
                               FireTrigOrd::SignalLength, &handle);
      break;
    case (TriggerType::SUBSCRIPTION_BEFORE):
      jam();
      fireTrigOrd->m_transId1 = req_struct->trans_id1;
      fireTrigOrd->m_transId2 = req_struct->trans_id2;
      fireTrigOrd->setGCI(req_struct->gci_hi);
      fireTrigOrd->setHashValue(req_struct->hash_value);
      fireTrigOrd->m_any_value = regOperPtr->m_any_value;
      fireTrigOrd->m_gci_lo = req_struct->gci_lo;
      if (executeDirect) {
        jam();
        EXECUTE_DIRECT(refToMain(ref), GSN_FIRE_TRIG_ORD, signal,
                       FireTrigOrd::SignalLengthSuma);
        jamEntry();
      } else {
        ndbassert(longsignal);
        LinearSectionPtr ptr[3];
        ptr[0].p = keyBuffer;
        ptr[0].sz = noPrimKey;
        ptr[1].p = beforeBuffer;
        ptr[1].sz = noBeforeWords;
        ptr[2].p = afterBuffer;
        ptr[2].sz = noAfterWords;
        if (refToMain(ref) == SUMA && (refToInstance(ref) != instance())) {
          jam();
          ndbmtd_buffer_suma_trigger(signal, FireTrigOrd::SignalLengthSuma,
                                     ptr);
        } else {
          jam();
          sendBatchedFIRE_TRIG_ORD(signal, ref, FireTrigOrd::SignalLengthSuma,
                                   ptr, 3);
        }
      }
      break;
    case (TriggerType::SUBSCRIPTION):
      jam();
      // Since only backup uses subscription triggers we
      // send to backup directly for now
      fireTrigOrd->setGCI(req_struct->gci_hi);

      if (executeDirect) {
        jam();
        EXECUTE_DIRECT(refToMain(ref), GSN_FIRE_TRIG_ORD, signal,
                       FireTrigOrd::SignalWithGCILength);
        jamEntry();
      } else {
        jam();
        // Todo send only before/after depending on BACKUP REDO/UNDO
        ndbassert(longsignal);
        LinearSectionPtr ptr[3];
        ptr[0].p = keyBuffer;
        ptr[0].sz = noPrimKey;
        ptr[1].p = beforeBuffer;
        ptr[1].sz = noBeforeWords;
        ptr[2].p = afterBuffer;
        ptr[2].sz = noAfterWords;
        sendBatchedFIRE_TRIG_ORD(signal, ref, FireTrigOrd::SignalWithGCILength,
                                 ptr, 3);
      }
      break;
    default:
      ndbabort();
  }
}

Uint32 Dbtup::setAttrIds(const AttributeMask &attributeMask,
                         Uint32 no_of_attributes, Uint32 *inBuffer) {
  Uint32 bufIndx = 0;
  jam();
  for (Uint32 i = 0; i < no_of_attributes; i++) {
    if (attributeMask.get(i)) {
      jamLine(i);
      AttributeHeader::init(&inBuffer[bufIndx++], i, 0);
    }
  }
  jam();
  return bufIndx;
}

bool Dbtup::readTriggerInfo(TupTriggerData *const trigPtr,
                            Operationrec *const regOperPtr,
                            KeyReqStruct *req_struct,
                            Fragrecord *const regFragPtr,
                            Uint32 *const keyBuffer, Uint32 &noPrimKey,
                            Uint32 *const afterBuffer, Uint32 &noAfterWords,
                            Uint32 *const beforeBuffer, Uint32 &noBeforeWords,
                            bool disk) {
  noAfterWords = 0;
  noBeforeWords = 0;
  Uint32 readBuffer[MAX_ATTRIBUTES_IN_TABLE];

  //---------------------------------------------------------------------------
  // Set-up variables needed by readAttributes operPtr.p, tabptr.p
  //---------------------------------------------------------------------------
  Ptr<Tablerec> tabptr;
  Ptr<Operationrec> operPtr;
  operPtr.p = regOperPtr;
  tabptr.i = regFragPtr->fragTableId;
  ptrCheckGuard(tabptr, cnoOfTablerec, tablerec);

  Tablerec *const regTabPtr = tabptr.p;
  Uint32 num_attr = regTabPtr->m_no_of_attributes;
  Uint32 descr_start = regTabPtr->tabDescriptor;
  ndbrequire(descr_start + (num_attr << ZAD_LOG_SIZE) <= cnoOfTabDescrRec);

  req_struct->tablePtrP = regTabPtr;
  req_struct->operPtrP = regOperPtr;
  req_struct->check_offset[MM] = regTabPtr->get_check_offset(MM);
  req_struct->check_offset[DD] = regTabPtr->get_check_offset(DD);
  req_struct->attr_descr = &tableDescriptor[descr_start];

  if ((regOperPtr->op_struct.bit_field.m_triggers ==
       TupKeyReq::OP_NO_TRIGGERS) &&
      (refToMain(trigPtr->m_receiverRef) == SUMA ||
       refToMain(trigPtr->m_receiverRef) == getBACKUP())) {
    /* Operations that have no logical effect need not be backed up
     * or sent as an event. Eg. OPTIMIZE TABLE is performed as a
     * ZUPDATE operation on table records, moving the varpart
     * column-values between pages, to be storage-effective.
     */
    return false;
  }

  //--------------------------------------------------------------------
  // Read Primary Key Values
  //--------------------------------------------------------------------
  Tuple_header *save0 = req_struct->m_tuple_ptr;
  if (regOperPtr->op_type == ZDELETE && !regOperPtr->is_first_operation()) {
    jam();
    req_struct->m_tuple_ptr =
        get_copy_tuple(&req_struct->prevOpPtr.p->m_copy_tuple_location);
  }

  if (regTabPtr->need_expand(disk)) prepare_read(req_struct, regTabPtr, disk);

  // Read Primary key into the keyBuffer
  int ret = readAttributes(
      req_struct, &tableDescriptor[regTabPtr->readKeyArray].tabDescr,
      regTabPtr->noOfKeyAttr, keyBuffer, ZATTR_BUFFER_SIZE);
  ndbrequire(ret >= 0);
  noPrimKey = ret;

  req_struct->m_tuple_ptr = save0;

  AttributeMask attributeMask;
  if ((regOperPtr->op_type == ZUPDATE) &&
      (trigPtr->sendOnlyChangedAttributes)) {
    jam();
    //--------------------------------------------------------------------
    // Update that sends only changed information (Among those monitored)
    //--------------------------------------------------------------------
    attributeMask = trigPtr->attributeMask;
    attributeMask.bitAND(req_struct->changeMask);
  } else if ((regOperPtr->op_type == ZDELETE) && (!trigPtr->sendBeforeValues)) {
    jam();
    //--------------------------------------------------------------------
    // Delete without sending before values only read Primary Key
    //--------------------------------------------------------------------
    return true;
  } else if (regOperPtr->op_type != ZREFRESH) {
    jam();
    //--------------------------------------------------------------------
    // All others send all attributes that are monitored, except:
    // Omit unchanged blob inlines on update i.e.
    // attributeMask & ~ (blobAttributeMask & ~ changeMask)
    //--------------------------------------------------------------------
    attributeMask = trigPtr->attributeMask;
    if (regOperPtr->op_type == ZUPDATE) {
      AttributeMask tmpMask = regTabPtr->blobAttributeMask;
      tmpMask.bitANDC(req_struct->changeMask);
      attributeMask.bitANDC(tmpMask);
    }
  } else {
    jam();
    ndbassert(regOperPtr->op_type == ZREFRESH);
    /* Refresh specific before/after value hacks */
    switch (regOperPtr->m_copy_tuple_location.m_file_no) {
      case Operationrec::RF_SINGLE_NOT_EXIST:
      case Operationrec::RF_MULTI_NOT_EXIST:
        return true;  // generate ZDELETE...no before values
      case Operationrec::RF_SINGLE_EXIST:
      case Operationrec::RF_MULTI_EXIST:
        // generate ZINSERT...all after values
        attributeMask = trigPtr->attributeMask;
        break;
      default:
        ndbabort();
        return false;  // Never reached
    }
  }

  // PK attributes are already part of Key, exclude them from AFTER values.
  // Keep the FullMask as an BEFORE-UPDATE may need it
  const AttributeMask attributeFullMask(attributeMask);
  if (trigPtr->monitorAllAttributes) {
    attributeMask.bitANDC(regTabPtr->allPkAttributeMask);
  }

  Uint32 numAttrsToRead =
      setAttrIds(attributeMask, regTabPtr->m_no_of_attributes, &readBuffer[0]);
  ndbrequire(numAttrsToRead <= MAX_ATTRIBUTES_IN_TABLE);
  //--------------------------------------------------------------------
  // Read Main tuple 'AFTER' values
  //--------------------------------------------------------------------
  if (regOperPtr->op_type != ZDELETE) {
    jam();
    int ret = readAttributes(req_struct, &readBuffer[0], numAttrsToRead,
                             afterBuffer, ZATTR_BUFFER_SIZE);
    ndbrequire(ret >= 0);
    noAfterWords = ret;
  } else {
    jam();
    noAfterWords = 0;
  }

  //--------------------------------------------------------------------
  // Read Copy tuple 'BEFORE' values for UPDATE and DELETE's
  //--------------------------------------------------------------------
  // Initialise pagep and tuple offset for read of copy tuple
  //--------------------------------------------------------------------
  if ((regOperPtr->op_type == ZUPDATE || regOperPtr->op_type == ZDELETE) &&
      (trigPtr->sendBeforeValues)) {
    jam();

    // Locate the before tuple
    Tuple_header *save = req_struct->m_tuple_ptr;
    PagePtr tmp;
    if (regOperPtr->is_first_operation()) {
      Uint32 *ptr = get_ptr(&tmp, &regOperPtr->m_tuple_location, regTabPtr);
      req_struct->m_tuple_ptr = (Tuple_header *)ptr;
    } else {
      req_struct->m_tuple_ptr =
          get_copy_tuple(&req_struct->prevOpPtr.p->m_copy_tuple_location);
    }

    if (regTabPtr->need_expand(disk)) prepare_read(req_struct, regTabPtr, disk);

    /**
     * Check if an UPDATE:
     * 1) Changed the key value,  and
     * 2) We want the key value included in the before values
     */
    bool keys_equal = true;
    if (regOperPtr->op_type == ZUPDATE &&
        req_struct->changeMask.overlaps(regTabPtr->allPkAttributeMask) &&  // 1)
        !attributeMask.equal(attributeFullMask)) {                         // 2)

      // Read BEFORE-PK, use beforeBuffer as temp storage, not kept
      Uint32 *beforeKey = beforeBuffer;
      const Uint32 keyWords = readAttributes(
          req_struct, &tableDescriptor[regTabPtr->readKeyArray].tabDescr,
          regTabPtr->noOfKeyAttr, beforeKey, ZATTR_BUFFER_SIZE);

      // If 'beforeKey != afterKey' we need it in the update trigger as well
      if (keyWords != noPrimKey ||
          memcmp(beforeKey, keyBuffer, keyWords * 4) != 0) {
        // Include the FullMask set of attributes in the BEFORE-value
        jam();
        keys_equal = false;
        numAttrsToRead = setAttrIds(
            attributeFullMask, regTabPtr->m_no_of_attributes, &readBuffer[0]);
      }
    }

    int ret = readAttributes(req_struct, &readBuffer[0], numAttrsToRead,
                             beforeBuffer, ZATTR_BUFFER_SIZE);
    req_struct->m_tuple_ptr = save;
    ndbrequire(ret >= 0);
    noBeforeWords = ret;

    //--------------------------------------------------------------------
    // Except for SUMA, which may need to 'AllowEmptyUpdate' events, we
    // supress the trigger if BEFORE and AFTER values are exactly the same.
    // Note that we need to do a binary compare: We can not suppress
    // the trigger in cases where character field comparing as equal
    // had a change in their binary representation (eg: 'xyz' -> 'XYZ').
    // Such changes may need to be replicated, included in backup logs, etc.
    //--------------------------------------------------------------------
    if (regOperPtr->op_type == ZUPDATE &&
        refToMain(trigPtr->m_receiverRef) != SUMA) {
      if (keys_equal && noAfterWords == noBeforeWords &&
          memcmp(afterBuffer, beforeBuffer, noAfterWords * 4) == 0) {
        jam();
        return false;
      }
    }
  }
  return true;
}

void Dbtup::sendTrigAttrInfo(Signal *signal, Uint32 *data, Uint32 dataLen,
                             bool executeDirect,
                             BlockReference receiverReference) {
  TrigAttrInfo *const trigAttrInfo = (TrigAttrInfo *)signal->getDataPtrSend();
  Uint32 sigLen;
  Uint32 dataIndex = 0;
  do {
    sigLen = dataLen - dataIndex;
    if (sigLen > TrigAttrInfo::DataLength) {
      jam();
      sigLen = TrigAttrInfo::DataLength;
    }
    MEMCOPY_NO_WORDS(trigAttrInfo->getData(), data + dataIndex, sigLen);
    if (executeDirect) {
      jam();
      EXECUTE_DIRECT(refToMain(receiverReference), GSN_TRIG_ATTRINFO, signal,
                     TrigAttrInfo::StaticLength + sigLen);
      jamEntry();
    } else {
      jam();
      sendSignal(receiverReference, GSN_TRIG_ATTRINFO, signal,
                 TrigAttrInfo::StaticLength + sigLen, JBB);
    }
    dataIndex += sigLen;
  } while (dataLen != dataIndex);
}

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

int Dbtup::executeTuxInsertTriggers(Signal *signal, Operationrec *regOperPtr,
                                    Fragrecord *regFragPtr,
                                    Tablerec *regTabPtr) {
  TuxMaintReq *const req = (TuxMaintReq *)signal->getDataPtrSend();
  // fill in constant part
  req->tableId = regFragPtr->fragTableId;
  req->fragId = regFragPtr->fragmentId;
  req->pageId = regOperPtr->m_tuple_location.m_page_no;
  req->pageIndex = regOperPtr->m_tuple_location.m_page_idx;
  req->tupVersion = regOperPtr->op_struct.bit_field.tupVersion;
  req->opInfo = TuxMaintReq::OpAdd;
  return addTuxEntries(signal, regOperPtr, regTabPtr);
}

int Dbtup::executeTuxUpdateTriggers(Signal *signal, Operationrec *regOperPtr,
                                    Fragrecord *regFragPtr,
                                    Tablerec *regTabPtr) {
  TuxMaintReq *const req = (TuxMaintReq *)signal->getDataPtrSend();
  // fill in constant part
  req->tableId = regFragPtr->fragTableId;
  req->fragId = regFragPtr->fragmentId;
  req->pageId = regOperPtr->m_tuple_location.m_page_no;
  req->pageIndex = regOperPtr->m_tuple_location.m_page_idx;
  req->tupVersion = regOperPtr->op_struct.bit_field.tupVersion;
  req->opInfo = TuxMaintReq::OpAdd;
  return addTuxEntries(signal, regOperPtr, regTabPtr);
}

int Dbtup::addTuxEntries(Signal *signal, Operationrec *regOperPtr,
                         Tablerec *regTabPtr) {
  if (ERROR_INSERTED(4022)) {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    terrorCode = 9999;
    return -1;
  }
  TuxMaintReq *const req = (TuxMaintReq *)signal->getDataPtrSend();
  const TupTriggerData_list &triggerList = regTabPtr->tuxCustomTriggers;
  TriggerPtr triggerPtr;
  Uint32 failPtrI;
  triggerList.first(triggerPtr);
  while (triggerPtr.i != RNIL) {
    jamDebug();
    req->indexId = triggerPtr.p->indexId;
    req->errorCode = RNIL;
    if (ERROR_INSERTED(4023) && !triggerList.hasNext(triggerPtr)) {
      jam();
      CLEAR_ERROR_INSERT_VALUE;
      terrorCode = 9999;
      failPtrI = triggerPtr.i;
      goto fail;
    }
    c_tux->execTUX_MAINT_REQ(signal);
    jamEntryDebug();
    if (unlikely(req->errorCode != 0)) {
      jam();
      terrorCode = req->errorCode;
      failPtrI = triggerPtr.i;
      goto fail;
    }
    triggerList.next(triggerPtr);
  }
  return 0;
fail:
  req->opInfo = TuxMaintReq::OpRemove;
  triggerList.first(triggerPtr);
  while (triggerPtr.i != failPtrI) {
    jamDebug();
    req->indexId = triggerPtr.p->indexId;
    req->errorCode = RNIL;
    c_tux->execTUX_MAINT_REQ(signal);
    jamEntryDebug();
    ndbrequire(req->errorCode == 0);
    triggerList.next(triggerPtr);
  }
#ifdef VM_TRACE
  ndbout << "aborted partial tux update: op " << hex << regOperPtr << endl;
#endif
  return -1;
}

int Dbtup::executeTuxDeleteTriggers(Signal *signal,
                                    Operationrec *const regOperPtr,
                                    Fragrecord *const regFragPtr,
                                    Tablerec *const regTabPtr) {
  // do nothing
  return 0;
}

void Dbtup::executeTuxCommitTriggers(Signal *signal, Operationrec *regOperPtr,
                                     Fragrecord *regFragPtr,
                                     Tablerec *regTabPtr) {
  TuxMaintReq *const req = (TuxMaintReq *)signal->getDataPtrSend();
  Uint32 tupVersion;
  if (regOperPtr->op_type == ZINSERT) {
    if (!regOperPtr->op_struct.bit_field.delete_insert_flag) return;
    jam();
    tupVersion = decr_tup_version(regOperPtr->op_struct.bit_field.tupVersion);
  } else if (regOperPtr->op_type == ZUPDATE) {
    jam();
    tupVersion = decr_tup_version(regOperPtr->op_struct.bit_field.tupVersion);
  } else if (regOperPtr->op_type == ZDELETE) {
    if (regOperPtr->op_struct.bit_field.delete_insert_flag) return;
    jam();
    tupVersion = regOperPtr->op_struct.bit_field.tupVersion;
  } else if (regOperPtr->op_type == ZREFRESH) {
    /* Refresh should not affect TUX */
    return;
  } else {
    ndbabort();
    tupVersion = 0;  // remove warning
  }
  // fill in constant part
  req->tableId = regFragPtr->fragTableId;
  req->fragId = regFragPtr->fragmentId;
  req->pageId = regOperPtr->m_tuple_location.m_page_no;
  req->pageIndex = regOperPtr->m_tuple_location.m_page_idx;
  req->tupVersion = tupVersion;
  req->opInfo = TuxMaintReq::OpRemove;
  removeTuxEntries(signal, regTabPtr);
}

void Dbtup::executeTuxAbortTriggers(Signal *signal, Operationrec *regOperPtr,
                                    Fragrecord *regFragPtr,
                                    Tablerec *regTabPtr) {
  TuxMaintReq *const req = (TuxMaintReq *)signal->getDataPtrSend();
  // get version
  Uint32 tupVersion;
  if (regOperPtr->op_type == ZINSERT) {
    jam();
    tupVersion = regOperPtr->op_struct.bit_field.tupVersion;
  } else if (regOperPtr->op_type == ZUPDATE) {
    jam();
    tupVersion = regOperPtr->op_struct.bit_field.tupVersion;
  } else if (regOperPtr->op_type == ZDELETE) {
    jam();
    return;
  } else if (regOperPtr->op_type == ZREFRESH) {
    jam();
    /* Refresh should not affect TUX */
    return;
  } else {
    ndbabort();
    tupVersion = 0;  // remove warning
  }
  // fill in constant part
  req->tableId = regFragPtr->fragTableId;
  req->fragId = regFragPtr->fragmentId;
  req->pageId = regOperPtr->m_tuple_location.m_page_no;
  req->pageIndex = regOperPtr->m_tuple_location.m_page_idx;
  req->tupVersion = tupVersion;
  req->opInfo = TuxMaintReq::OpRemove;
  removeTuxEntries(signal, regTabPtr);
}

void Dbtup::removeTuxEntries(Signal *signal, Tablerec *regTabPtr) {
  TuxMaintReq *const req = (TuxMaintReq *)signal->getDataPtrSend();
  const TupTriggerData_list &triggerList = regTabPtr->tuxCustomTriggers;
  TriggerPtr triggerPtr;
  triggerList.first(triggerPtr);
  while (triggerPtr.i != RNIL) {
    jamDebug();
    req->indexId = triggerPtr.p->indexId;
    req->errorCode = RNIL;
    c_tux->execTUX_MAINT_REQ(signal);
    jamEntryDebug();
    // must succeed
    ndbrequire(req->errorCode == 0);
    triggerList.next(triggerPtr);
  }
}

void Dbtup::ndbmtd_buffer_suma_trigger(Signal *signal, Uint32 len,
                                       LinearSectionPtr sec[3]) {
  jam();
  Uint32 tot = len + 5;
  for (Uint32 i = 0; i < 3; i++) tot += sec[i].sz;

  Uint32 *ptr = 0;
  Uint32 used = m_suma_trigger_buffer.m_usedWords;
  Uint32 free = m_suma_trigger_buffer.m_freeWords;
  Uint32 pageId = m_suma_trigger_buffer.m_pageId;
  Uint32 oom = m_suma_trigger_buffer.m_out_of_memory;
  if (free < tot) {
    jam();
    if (pageId != RNIL) {
      jam();
      flush_ndbmtd_suma_buffer(signal);
      used = 0;
      free = 0;
      pageId = RNIL;
    }
    if (oom == 0) {
      jam();
      ndbassert(m_suma_trigger_buffer.m_pageId == RNIL);
      Uint32 page_count = (tot - 1) / GLOBAL_PAGE_SIZE_WORDS + 1;
      Uint32 count = page_count;
      m_ctx.m_mm.alloc_pages(RT_SUMA_TRIGGER_BUFFER,
                             &m_suma_trigger_buffer.m_pageId, &count,
                             page_count);
      pageId = m_suma_trigger_buffer.m_pageId;
      if (count == 0) {
        jam();
        ptr = 0;
      } else {
        jam();
        ptr = reinterpret_cast<Uint32 *>(c_page_pool.getPtr(pageId));
        free = count * GLOBAL_PAGE_SIZE_WORDS - tot;
      }
    }
  } else {
    jam();
    ptr = reinterpret_cast<Uint32 *>(c_page_pool.getPtr(pageId));
    ptr += used;
    free -= tot;
  }

  if (likely(ptr != 0)) {
    jam();
    *ptr++ = tot;
    *ptr++ = len;
    *ptr++ = sec[0].sz;
    *ptr++ = sec[1].sz;
    *ptr++ = sec[2].sz;
    memcpy(ptr, signal->getDataPtrSend(), 4 * len);
    ptr += len;
    for (Uint32 i = 0; i < 3; i++) {
      jam();
      memcpy(ptr, sec[i].p, 4 * sec[i].sz);
      ptr += sec[i].sz;
    }

    used += tot;

    m_suma_trigger_buffer.m_usedWords = used;
    m_suma_trigger_buffer.m_freeWords = free;
    if (free < (len + 5)) {
      jam();
      flush_ndbmtd_suma_buffer(signal);
    }
  } else {
    jam();
    m_suma_trigger_buffer.m_out_of_memory = 1;
  }
}

void Dbtup::flush_ndbmtd_suma_buffer(Signal *signal) {
  jam();

  Uint32 pageId = m_suma_trigger_buffer.m_pageId;
  Uint32 used = m_suma_trigger_buffer.m_usedWords;
  Uint32 oom = m_suma_trigger_buffer.m_out_of_memory;

  if (pageId != RNIL) {
    jam();
    Uint32 save[2];
    save[0] = signal->theData[0];
    save[1] = signal->theData[1];
    signal->theData[0] = pageId;
    signal->theData[1] = used;
    sendSignal(SUMA_REF, GSN_FIRE_TRIG_ORD_L, signal, 2, JBB);

    signal->theData[0] = save[0];
    signal->theData[1] = save[1];
  } else if (oom) {
    jam();
    Uint32 save[2];
    save[0] = signal->theData[0];
    save[1] = signal->theData[1];
    signal->theData[0] = RNIL;
    signal->theData[1] = 0;
    sendSignal(SUMA_REF, GSN_FIRE_TRIG_ORD_L, signal, 2, JBB);

    signal->theData[0] = save[0];
    signal->theData[1] = save[1];
  }

  m_suma_trigger_buffer.m_pageId = RNIL;
  m_suma_trigger_buffer.m_usedWords = 0;
  m_suma_trigger_buffer.m_freeWords = 0;
  m_suma_trigger_buffer.m_out_of_memory = 0;
}

void Dbtup::execSUB_GCP_COMPLETE_REP(Signal *signal) {
  flush_ndbmtd_suma_buffer(signal);
}
