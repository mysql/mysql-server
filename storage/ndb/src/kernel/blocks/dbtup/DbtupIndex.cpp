/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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

#define DBTUP_C
#define DBTUP_INDEX_CPP
#include <dblqh/Dblqh.hpp>
#include "Dbtup.hpp"
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <pc.hpp>
#include <AttributeDescriptor.hpp>
#include "AttributeOffset.hpp"
#include <AttributeHeader.hpp>
#include <signaldata/TuxMaint.hpp>
#include <signaldata/AlterIndxImpl.hpp>

#define JAM_FILE_ID 418


// methods used by ordered index

void
Dbtup::tuxGetTupAddr(Uint32 fragPtrI,
                     Uint32 pageId,
                     Uint32 pageIndex,
                     Uint32& lkey1,
                     Uint32& lkey2)
{
  jamEntryDebug();
  PagePtr pagePtr;
  c_page_pool.getPtr(pagePtr, pageId);
  lkey1 = pagePtr.p->frag_page_id;
  lkey2 = pageIndex;
}

int
Dbtup::tuxAllocNode(EmulatedJamBuffer * jamBuf,
                    Uint32 fragPtrI,
                    Uint32& pageId,
                    Uint32& pageOffset,
                    Uint32*& node)
{
  thrjamEntry(jamBuf);
  FragrecordPtr fragPtr;
  fragPtr.i= fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
  TablerecPtr tablePtr;
  tablePtr.i= fragPtr.p->fragTableId;
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);

  Local_key key;
  Uint32* ptr, frag_page_id, err;
  c_allow_alloc_spare_page=true;
  if ((ptr= alloc_fix_rec(jamBuf, &err,fragPtr.p,tablePtr.p, &key, 
                          &frag_page_id)) == 0)
  {
    c_allow_alloc_spare_page=false;
    thrjam(jamBuf);
    return err;
  }
  c_allow_alloc_spare_page=false;
  pageId= key.m_page_no;
  pageOffset= key.m_page_idx;
  Uint32 attrDescIndex= tablePtr.p->tabDescriptor + (0 << ZAD_LOG_SIZE);
  Uint32 attrDataOffset= AttributeOffset::getOffset(
                              tableDescriptor[attrDescIndex + 1].tabDescr);
  node= ptr + attrDataOffset;
  return 0;
}

void
Dbtup::tuxFreeNode(Uint32 fragPtrI,
                   Uint32 pageId,
                   Uint32 pageOffset,
                   Uint32* node)
{
  jamEntry();
  FragrecordPtr fragPtr;
  fragPtr.i= fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
  TablerecPtr tablePtr;
  tablePtr.i= fragPtr.p->fragTableId;
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);

  Local_key key;
  key.m_page_no = pageId;
  key.m_page_idx = pageOffset;
  PagePtr pagePtr;
  Tuple_header* ptr = (Tuple_header*)get_ptr(&pagePtr, &key, tablePtr.p);

  Uint32 attrDescIndex= tablePtr.p->tabDescriptor + (0 << ZAD_LOG_SIZE);
  Uint32 attrDataOffset= AttributeOffset::getOffset(tableDescriptor[attrDescIndex + 1].tabDescr);
  ndbrequire(node == (Uint32*)ptr + attrDataOffset);

  free_fix_rec(fragPtr.p, tablePtr.p, &key, (Fix_page*)pagePtr.p);
}

void
Dbtup::tuxGetNode(Uint32 fragPtrI,
                  Uint32 pageId,
                  Uint32 pageOffset,
                  Uint32*& node)
{
  FragrecordPtr fragPtr;
  fragPtr.i= fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
  TablerecPtr tablePtr;
  tablePtr.i= fragPtr.p->fragTableId;
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);
  PagePtr pagePtr;
  c_page_pool.getPtr(pagePtr, pageId);
  Uint32 attrDescIndex= tablePtr.p->tabDescriptor + (0 << ZAD_LOG_SIZE);
  Uint32 attrDataOffset= AttributeOffset::getOffset(
                            tableDescriptor[attrDescIndex + 1].tabDescr);
  node= ((Fix_page*)pagePtr.p)->
    get_ptr(pageOffset, tablePtr.p->m_offsets[MM].m_fix_header_size) + 
    attrDataOffset;
}

int
Dbtup::tuxReadAttrs(EmulatedJamBuffer * jamBuf,
                    Uint32 fragPtrI,
                    Uint32 pageId,
                    Uint32 pageIndex,
                    Uint32 tupVersion,
                    const Uint32* attrIds,
                    Uint32 numAttrs,
                    Uint32* dataOut,
                    bool xfrmFlag)
{
  thrjamEntryDebug(jamBuf);
  // use own variables instead of globals
  FragrecordPtr fragPtr;
  fragPtr.i= fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
  TablerecPtr tablePtr;
  tablePtr.i= fragPtr.p->fragTableId;
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);

  // search for tuple version if not original

  Operationrec tmpOp;
  KeyReqStruct req_struct(jamBuf);
  req_struct.tablePtrP = tablePtr.p;
  req_struct.fragPtrP = fragPtr.p;

  tmpOp.m_tuple_location.m_page_no= pageId;
  tmpOp.m_tuple_location.m_page_idx= pageIndex;
  tmpOp.op_type = ZREAD; // valgrind
  setup_fixed_tuple_ref(&req_struct, &tmpOp, tablePtr.p);
  setup_fixed_part(&req_struct, &tmpOp, tablePtr.p);
  Tuple_header *tuple_ptr= req_struct.m_tuple_ptr;
  if (tuple_ptr->get_tuple_version() != tupVersion)
  {
    jamDebug();
    OperationrecPtr opPtr;
    opPtr.i= tuple_ptr->m_operation_ptr_i;
    Uint32 loopGuard= 0;
    while (opPtr.i != RNIL) {
      c_operation_pool.getPtr(opPtr);
      if (opPtr.p->op_struct.bit_field.tupVersion == tupVersion) {
	jamDebug();
	if (!opPtr.p->m_copy_tuple_location.isNull()) {
	  req_struct.m_tuple_ptr=
            get_copy_tuple(&opPtr.p->m_copy_tuple_location);
        }
	break;
      }
      jamDebug();
      opPtr.i= opPtr.p->prevActiveOp;
      ndbrequire(++loopGuard < (1 << ZTUP_VERSION_BITS));
    }
  }
  // read key attributes from found tuple version
  // save globals
  prepare_read(&req_struct, tablePtr.p, false); 

  // do it
  int ret = readAttributes(&req_struct,
                           attrIds,
                           numAttrs,
                           dataOut,
                           ZNIL,
                           xfrmFlag);

  // done
  return ret;
}
int
Dbtup::tuxReadPk(Uint32 fragPtrI, Uint32 pageId, Uint32 pageIndex, Uint32* dataOut, bool xfrmFlag)
{
  jamEntryDebug();
  // use own variables instead of globals
  FragrecordPtr fragPtr;
  fragPtr.i= fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
  TablerecPtr tablePtr;
  tablePtr.i= fragPtr.p->fragTableId;
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);
  
  Operationrec tmpOp;
  tmpOp.m_tuple_location.m_page_no= pageId;
  tmpOp.m_tuple_location.m_page_idx= pageIndex;
  
  KeyReqStruct req_struct(this);
  req_struct.tablePtrP = tablePtr.p;
  req_struct.fragPtrP = fragPtr.p;
 
  PagePtr page_ptr;
  Uint32* ptr= get_ptr(&page_ptr, &tmpOp.m_tuple_location, tablePtr.p);
  req_struct.m_page_ptr = page_ptr;
  req_struct.m_tuple_ptr = (Tuple_header*)ptr;
  
  int ret = 0;
  if (! (req_struct.m_tuple_ptr->m_header_bits & Tuple_header::FREE))
  {
    req_struct.check_offset[MM]= tablePtr.p->get_check_offset(MM);
    req_struct.check_offset[DD]= tablePtr.p->get_check_offset(DD);
    
    Uint32 num_attr= tablePtr.p->m_no_of_attributes;
    Uint32 descr_start= tablePtr.p->tabDescriptor;
    TableDescriptor *tab_descr= &tableDescriptor[descr_start];
    ndbrequire(descr_start + (num_attr << ZAD_LOG_SIZE) <= cnoOfTabDescrRec);
    req_struct.attr_descr= tab_descr; 

    if(req_struct.m_tuple_ptr->m_header_bits & Tuple_header::ALLOC)
    {
      Uint32 opPtrI= req_struct.m_tuple_ptr->m_operation_ptr_i;
      Operationrec* opPtrP= c_operation_pool.getPtr(opPtrI);
      ndbassert(!opPtrP->m_copy_tuple_location.isNull());
      req_struct.m_tuple_ptr=
	get_copy_tuple(&opPtrP->m_copy_tuple_location);
    }
    prepare_read(&req_struct, tablePtr.p, false);
    
    const Uint32* attrIds= &tableDescriptor[tablePtr.p->readKeyArray].tabDescr;
    const Uint32 numAttrs= tablePtr.p->noOfKeyAttr;
    // read pk attributes from original tuple
    
    // do it
    ret = readAttributes(&req_struct,
			 attrIds,
			 numAttrs,
			 dataOut,
			 ZNIL,
			 xfrmFlag);
    // done
    if (ret >= 0) {
      // remove headers
      Uint32 n= 0;
      Uint32 i= 0;
      while (n < numAttrs) {
	const AttributeHeader ah(dataOut[i]);
	Uint32 size= ah.getDataSize();
	ndbrequire(size != 0);
	for (Uint32 j= 0; j < size; j++) {
	  dataOut[i + j - n]= dataOut[i + j + 1];
	}
	n+= 1;
	i+= 1 + size;
      }
      ndbrequire((int)i == ret);
      ret -= numAttrs;
    } else {
      return ret;
    }
  }
  if (tablePtr.p->m_bits & Tablerec::TR_RowGCI)
  {
    dataOut[ret] = *req_struct.m_tuple_ptr->get_mm_gci(tablePtr.p);
  }
  else
  {
    dataOut[ret] = 0;
  }
  return ret;
}

int
Dbtup::accReadPk(Uint32 tableId, Uint32 fragId, Uint32 fragPageId, Uint32 pageIndex, Uint32* dataOut, bool xfrmFlag)
{
  jamEntryDebug();
  // get table
  TablerecPtr tablePtr;
  tablePtr.i = tableId;
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);
  // get fragment
  FragrecordPtr fragPtr;
  getFragmentrec(fragPtr, fragId, tablePtr.p);
  // get real page id and tuple offset

  Uint32 pageId = getRealpid(fragPtr.p, fragPageId);
  // use TUX routine - optimize later
  int ret = tuxReadPk(fragPtr.i, pageId, pageIndex, dataOut, xfrmFlag);
  return ret;
}

/*
 * TUX index contains all tuple versions.  A scan in TUX has scanned
 * one of them and asks if it can be returned as scan result.  This
 * depends on trans id, dirty read flag, and savepoint within trans.
 *
 * Previously this faked a ZREAD operation and used getPage().
 * In TUP getPage() is run after ACC locking, but TUX comes here
 * before ACC access.  Instead of modifying getPage() it is more
 * clear to do the full check here.
 */
bool
Dbtup::tuxQueryTh(Uint32 fragPtrI,
                  Uint32 pageId,
                  Uint32 pageIndex,
                  Uint32 tupVersion,
                  Uint32 transId1,
                  Uint32 transId2,
                  bool dirty,
                  Uint32 savepointId)
{
  jamEntryDebug();
  FragrecordPtr fragPtr;
  fragPtr.i= fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
  TablerecPtr tablePtr;
  tablePtr.i= fragPtr.p->fragTableId;
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);
  PagePtr pagePtr;
  pagePtr.i = pageId;
  c_page_pool.getPtr(pagePtr);

  KeyReqStruct req_struct(this);

  {
    Operationrec tmpOp;
    tmpOp.m_tuple_location.m_page_no = pageId;
    tmpOp.m_tuple_location.m_page_idx = pageIndex;
    tmpOp.op_type = ZREAD; // valgrind
    setup_fixed_tuple_ref(&req_struct, &tmpOp, tablePtr.p);
    setup_fixed_part(&req_struct, &tmpOp, tablePtr.p);
  }

  Tuple_header* tuple_ptr = req_struct.m_tuple_ptr;

  OperationrecPtr currOpPtr;
  currOpPtr.i = tuple_ptr->m_operation_ptr_i;
  if (currOpPtr.i == RNIL) {
    jamDebug();
    // tuple has no operation, any scan can see it
    return true;
  }
  c_operation_pool.getPtr(currOpPtr);

  const bool sameTrans =
    c_lqh->is_same_trans(currOpPtr.p->userpointer, transId1, transId2);

  bool res = false;
  OperationrecPtr loopOpPtr = currOpPtr;

  if (!sameTrans) {
    jam();
    if (!dirty) {
      jam();
      if (currOpPtr.p->nextActiveOp == RNIL) {
        jam();
        // last op - TUX makes ACC lock request in same timeslice
        res = true;
      }
    }
    else {
      // loop to first op (returns false)
      find_savepoint(loopOpPtr, 0);
      const Uint32 op_type = loopOpPtr.p->op_type;

      if (op_type != ZINSERT) {
        jam();
        // read committed version
        const Uint32 origVersion = tuple_ptr->get_tuple_version();
        if (origVersion == tupVersion) {
          jam();
          res = true;
        }
      }
    }
  }
  else {
    jam();
    // for own trans, ignore dirty flag

    if (find_savepoint(loopOpPtr, savepointId)) {
      jam();
      const Uint32 op_type = loopOpPtr.p->op_type;

      if (op_type != ZDELETE) {
        jam();
        // check if this op has produced the scanned version
        Uint32 loopVersion = loopOpPtr.p->op_struct.bit_field.tupVersion;
        if (loopVersion == tupVersion) {
          jam();
          res = true;
        }
      }
    }
  }

  return res;
}

// ordered index build

//#define TIME_MEASUREMENT
#ifdef TIME_MEASUREMENT
  static Uint32 time_events;
  Uint64 tot_time_passed;
  Uint32 number_events;
#endif
void
Dbtup::execBUILD_INDX_IMPL_REQ(Signal* signal)
{
  jamEntry();
#ifdef TIME_MEASUREMENT
  time_events= 0;
  tot_time_passed= 0;
  number_events= 1;
#endif
  const BuildIndxImplReq* const req =
    (const BuildIndxImplReq*)signal->getDataPtr();
  // get new operation
  BuildIndexPtr buildPtr;
  if (ERROR_INSERTED(4031) || ! c_buildIndexList.seizeFirst(buildPtr)) {
    jam();
    BuildIndexRec buildRec;
    buildRec.m_request = *req;
    buildRec.m_errorCode = BuildIndxImplRef::Busy;
    if (ERROR_INSERTED(4031))
    {
      CLEAR_ERROR_INSERT_VALUE;
    }
    buildIndexReply(signal, &buildRec);
    return;
  }
  buildPtr.p->m_request = *req;
  const BuildIndxImplReq* buildReq = &buildPtr.p->m_request;
  // check
  buildPtr.p->m_errorCode= BuildIndxImplRef::NoError;
  buildPtr.p->m_outstanding = 0;
  do {
    if (buildReq->tableId >= cnoOfTablerec) {
      jam();
      buildPtr.p->m_errorCode= BuildIndxImplRef::InvalidPrimaryTable;
      break;
    }
    TablerecPtr tablePtr;
    tablePtr.i= buildReq->tableId;
    ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);
    if (tablePtr.p->tableStatus != DEFINED) {
      jam();
      buildPtr.p->m_errorCode= BuildIndxImplRef::InvalidPrimaryTable;
      break;
    }
    // memory page format
    buildPtr.p->m_build_vs =
      (tablePtr.p->m_attributes[MM].m_no_of_varsize +
       tablePtr.p->m_attributes[MM].m_no_of_dynamic) > 0;
    if (DictTabInfo::isOrderedIndex(buildReq->indexType)) {
      jam();
      const TupTriggerData_list& triggerList =
	tablePtr.p->tuxCustomTriggers;

      TriggerPtr triggerPtr;
      triggerList.first(triggerPtr);
      while (triggerPtr.i != RNIL) {
	if (triggerPtr.p->indexId == buildReq->indexId) {
	  jam();
	  break;
	}
	triggerList.next(triggerPtr);
      }
      if (triggerPtr.i == RNIL) {
	jam();
	// trigger was not created
        ndbassert(false);
	buildPtr.p->m_errorCode = BuildIndxImplRef::InternalError;
	break;
      }
      buildPtr.p->m_indexId = buildReq->indexId;
      buildPtr.p->m_buildRef = DBTUX;
      AlterIndxImplReq* req = (AlterIndxImplReq*)signal->getDataPtrSend();
      req->indexId = buildReq->indexId;
      req->senderRef = 0;
      req->requestType = AlterIndxImplReq::AlterIndexBuilding;
      EXECUTE_DIRECT(DBTUX, GSN_ALTER_INDX_IMPL_REQ, signal, 
                     AlterIndxImplReq::SignalLength);
    } else if(buildReq->indexId == RNIL) {
      jam();
      // REBUILD of acc
      buildPtr.p->m_indexId = RNIL;
      buildPtr.p->m_buildRef = DBACC;
    } else {
      jam();
      buildPtr.p->m_errorCode = BuildIndxImplRef::InvalidIndexType;
      break;
    }

    // set to first tuple position
    const Uint32 firstTupleNo = 0;
    buildPtr.p->m_fragNo= 0;
    buildPtr.p->m_pageId= 0;
    buildPtr.p->m_tupleNo= firstTupleNo;
    // start build

    bool offline = !!(buildReq->requestType&BuildIndxImplReq::RF_BUILD_OFFLINE);
    if (offline && m_max_parallel_index_build > 1)
    {
      jam();
      buildIndexOffline(signal, buildPtr.i);
    }
    else
    {
      jam();
      buildIndex(signal, buildPtr.i);
    }
    return;
  } while (0);
  // check failed
  buildIndexReply(signal, buildPtr.p);
  c_buildIndexList.release(buildPtr);
}

void
Dbtup::buildIndex(Signal* signal, Uint32 buildPtrI)
{
  // get build record
  BuildIndexPtr buildPtr;
  buildPtr.i= buildPtrI;
  c_buildIndexList.getPtr(buildPtr);
  const BuildIndxImplReq* buildReq= &buildPtr.p->m_request;
  // get table
  TablerecPtr tablePtr;
  tablePtr.i= buildReq->tableId;
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);

  const Uint32 firstTupleNo = 0;
  const Uint32 tupheadsize = tablePtr.p->m_offsets[MM].m_fix_header_size;

#ifdef TIME_MEASUREMENT
  NDB_TICKS start;
  NDB_TICKS stop;
  Uint64 time_passed;
#endif
  do {
    // get fragment
    FragrecordPtr fragPtr;
    if (buildPtr.p->m_fragNo == NDB_ARRAY_SIZE(tablePtr.p->fragrec)) {
      jam();
      // build ready
      buildIndexReply(signal, buildPtr.p);
      c_buildIndexList.release(buildPtr);
      return;
    }
    ndbrequire(buildPtr.p->m_fragNo < NDB_ARRAY_SIZE(tablePtr.p->fragrec));
    fragPtr.i= tablePtr.p->fragrec[buildPtr.p->m_fragNo];
    if (fragPtr.i == RNIL) {
      jam();
      buildPtr.p->m_fragNo++;
      buildPtr.p->m_pageId= 0;
      buildPtr.p->m_tupleNo= firstTupleNo;
      break;
    }
    ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
    // get page
    PagePtr pagePtr;
    if (buildPtr.p->m_pageId >= fragPtr.p->m_max_page_cnt)
    {
      jam();
      buildPtr.p->m_fragNo++;
      buildPtr.p->m_pageId= 0;
      buildPtr.p->m_tupleNo= firstTupleNo;
      break;
    }
    Uint32 realPageId= getRealpidCheck(fragPtr.p, buildPtr.p->m_pageId);
    // skip empty page
    if (realPageId == RNIL) 
    {
      jam();
      goto next_tuple;
    }

    c_page_pool.getPtr(pagePtr, realPageId);

next_tuple:
    // get tuple
    Uint32 pageIndex = ~0;
    const Tuple_header* tuple_ptr = 0;
    pageIndex = buildPtr.p->m_tupleNo * tupheadsize;
    if (pageIndex + tupheadsize > Fix_page::DATA_WORDS) {
      jam();
      buildPtr.p->m_pageId++;
      buildPtr.p->m_tupleNo= firstTupleNo;
      break;
    }
    
    if (realPageId == RNIL)
    {
      jam();
      buildPtr.p->m_tupleNo++;
      break;
    }

    tuple_ptr = (Tuple_header*)&pagePtr.p->m_data[pageIndex];
    // skip over free tuple
    if (tuple_ptr->m_header_bits & Tuple_header::FREE) {
      jam();
      buildPtr.p->m_tupleNo++;
      break;
    }
    Uint32 tupVersion= tuple_ptr->get_tuple_version();
    OperationrecPtr pageOperPtr;
    pageOperPtr.i= tuple_ptr->m_operation_ptr_i;
#ifdef TIME_MEASUREMENT
    start = NdbTick_getCurrentTicks();
#endif
    // add to index
    TuxMaintReq* const req = (TuxMaintReq*)signal->getDataPtrSend();
    req->errorCode = RNIL;
    req->tableId = tablePtr.i;
    req->indexId = buildPtr.p->m_indexId;
    req->fragId = tablePtr.p->fragid[buildPtr.p->m_fragNo];
    req->pageId = realPageId;
    req->tupVersion = tupVersion;
    req->opInfo = TuxMaintReq::OpAdd;
    req->tupFragPtrI = fragPtr.i;
    req->fragPageId = buildPtr.p->m_pageId;
    req->pageIndex = pageIndex;

    if (pageOperPtr.i == RNIL)
    {
      EXECUTE_DIRECT(buildPtr.p->m_buildRef, GSN_TUX_MAINT_REQ,
		     signal, TuxMaintReq::SignalLength+2);
    }
    else
    {
      /*
      If there is an ongoing operation on the tuple then it is either a
      copy tuple or an original tuple with an ongoing transaction. In
      both cases realPageId and pageOffset refer to the original tuple.
      The tuple address stored in TUX will always be the original tuple
      but with the tuple version of the tuple we found.

      This is necessary to avoid having to update TUX at abort of
      update. If an update aborts then the copy tuple is copied to
      the original tuple. The build will however have found that
      tuple as a copy tuple. The original tuple is stable and is thus
      preferrable to store in TUX.
      */
      jam();

      /**
       * Since copy tuples now can't be found on real pages.
       *   we will here build all copies of the tuple
       *
       * Note only "real" tupVersion's should be added 
       *      i.e delete's shouldnt be added 
       *      (unless it's the first op, when "original" should be added)
       */

      /*
       * Start from first operation.  This is only to make things more
       * clear.  It is not required by ordered index implementation.
       */
      c_operation_pool.getPtr(pageOperPtr);
      while (pageOperPtr.p->prevActiveOp != RNIL)
      {
        jam();
        pageOperPtr.i = pageOperPtr.p->prevActiveOp;
        c_operation_pool.getPtr(pageOperPtr);
      }
      /*
       * Do not use req->errorCode as global control.
       */
      bool ok = true;
      /*
       * If first operation is an update, add previous version.
       * This version does not appear as the version of any operation.
       * At commit this version is removed by executeTuxCommitTriggers.
       * At abort it is preserved by executeTuxAbortTriggers.
       */
      if (pageOperPtr.p->op_type == ZUPDATE)
      {
        jam();
        req->errorCode = RNIL;
        req->tupVersion =
          decr_tup_version(pageOperPtr.p->op_struct.bit_field.tupVersion);
        EXECUTE_DIRECT(buildPtr.p->m_buildRef, GSN_TUX_MAINT_REQ,
                       signal, TuxMaintReq::SignalLength+2);
        ok = (req->errorCode == 0);
      }
      /*
       * Add versions from all operations.
       *
       * Each operation has a tuple version.  For insert and update it
       * is the newly created version.  For delete it is the version
       * deleted.  The existence of operation tuple version implies that
       * a corresponding tuple version exists for TUX to read.
       *
       * We could be in the middle of a commit.  The process here makes
       * no assumptions about operation commit order.  (It should be
       * first to last but this is not the place to assert it).
       *
       * Duplicate versions are possible e.g. a delete in the middle
       * may have same version as the previous operation.  TUX ignores
       * duplicate version errors during index build.
       */
      while (pageOperPtr.i != RNIL && ok)
      {
        jam();
        c_operation_pool.getPtr(pageOperPtr);
        req->errorCode = RNIL;
        req->tupVersion = pageOperPtr.p->op_struct.bit_field.tupVersion;
        EXECUTE_DIRECT(buildPtr.p->m_buildRef, GSN_TUX_MAINT_REQ,
                       signal, TuxMaintReq::SignalLength+2);
        pageOperPtr.i = pageOperPtr.p->nextActiveOp;
        ok = (req->errorCode == 0);
      }
    } 
    
    jamEntry();
    if (req->errorCode != 0) {
      switch (req->errorCode) {
      case TuxMaintReq::NoMemError:
      case TuxMaintReq::NoTransMemError:
        jam();
        buildPtr.p->m_errorCode= BuildIndxImplRef::AllocationFailure;
        break;
      default:
        ndbrequire(false);
        break;
      }
      buildIndexReply(signal, buildPtr.p);
      c_buildIndexList.release(buildPtr);
      return;
    }
#ifdef TIME_MEASUREMENT
    stop = NdbTick_getCurrentTicks();
    time_passed= NdbTick_Elapsed(start, stop).microSec();
    if (time_passed < 1000) {
      time_events++;
      tot_time_passed += time_passed;
      if (time_events == number_events) {
        Uint64 mean_time_passed= tot_time_passed /
                                     (Uint64)number_events;
        ndbout << "Number of events= " << number_events;
        ndbout << " Mean time passed= " << mean_time_passed << endl;
        number_events <<= 1;
        tot_time_passed= 0;
        time_events= 0;
      }
    }
#endif
    // next tuple
    buildPtr.p->m_tupleNo++;
    break;
  } while (0);
  signal->theData[0]= ZBUILD_INDEX;
  signal->theData[1]= buildPtr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
}

Uint32 Dbtux_mt_buildIndexFragment_wrapper_C(void*);

void
Dbtup::buildIndexOffline(Signal* signal, Uint32 buildPtrI)
{
  jam();
  /**
   * We need to make table read-only...as mtoib does not work otherwise
   */
  BuildIndexPtr buildPtr;
  buildPtr.i= buildPtrI;
  c_buildIndexList.getPtr(buildPtr);
  const BuildIndxImplReq* buildReq = 
    (const BuildIndxImplReq*)&buildPtr.p->m_request;

  AlterTabReq* req = (AlterTabReq*)signal->getDataPtrSend();
  /**
   * Note: before 7.3.4, 7.2.15, 7.1.30 fifth word and
   * up was undefined.
   */
  bzero(req, sizeof(*req));
  req->senderRef = reference();
  req->senderData = buildPtrI;
  req->tableId = buildReq->tableId;
  req->requestType = AlterTabReq::AlterTableReadOnly;
  sendSignal(calcInstanceBlockRef(DBLQH), GSN_ALTER_TAB_REQ, signal,
             AlterTabReq::SignalLength, JBB);
}

void
Dbtup::execALTER_TAB_CONF(Signal* signal)
{
  jamEntry();
  AlterTabConf* conf = (AlterTabConf*)signal->getDataPtr();

  BuildIndexPtr buildPtr;
  buildPtr.i = conf->senderData;
  c_buildIndexList.getPtr(buildPtr);


  if (buildPtr.p->m_fragNo == 0)
  {
    jam();
    buildIndexOffline_table_readonly(signal, conf->senderData);
    return;
  }
  else
  {
    jam();
    TablerecPtr tablePtr;
    (void)tablePtr; // hide unused warning
    ndbrequire(buildPtr.p->m_fragNo >= NDB_ARRAY_SIZE(tablePtr.p->fragid));
    buildIndexReply(signal, buildPtr.p);
    c_buildIndexList.release(buildPtr);
    return;
  }
}

void
Dbtup::buildIndexOffline_table_readonly(Signal* signal, Uint32 buildPtrI)
{
  // get build record
  BuildIndexPtr buildPtr;
  buildPtr.i= buildPtrI;
  c_buildIndexList.getPtr(buildPtr);
  const BuildIndxImplReq* buildReq = 
    (const BuildIndxImplReq*)&buildPtr.p->m_request;
  // get table
  TablerecPtr tablePtr;
  tablePtr.i= buildReq->tableId;
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);

  for (;buildPtr.p->m_fragNo < NDB_ARRAY_SIZE(tablePtr.p->fragrec);
       buildPtr.p->m_fragNo++)
  {
    jam();
    FragrecordPtr fragPtr;
    fragPtr.i = tablePtr.p->fragrec[buildPtr.p->m_fragNo];
    if (fragPtr.i == RNIL)
    {
      jam();
      continue;
    }
    ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
    mt_BuildIndxReq req;
    bzero(&req, sizeof(req));
    req.senderRef = reference();
    req.senderData = buildPtr.i;
    req.tableId = buildReq->tableId;
    req.indexId = buildPtr.p->m_indexId;
    req.fragId = tablePtr.p->fragid[buildPtr.p->m_fragNo];

    SimulatedBlock * tux = globalData.getBlock(DBTUX);
    if (instance() != 0)
    {
      tux = tux->getInstance(instance());
      ndbrequire(tux != 0);
    }
    req.tux_ptr = tux;
    req.tup_ptr = this;
    req.func_ptr = Dbtux_mt_buildIndexFragment_wrapper_C;
    req.buffer_size = 16*32768; // thread-local-buffer

    Uint32 * req_ptr = signal->getDataPtrSend();
    memcpy(req_ptr, &req, sizeof(req));

    sendSignal(NDBFS_REF, GSN_BUILD_INDX_IMPL_REQ, signal,
               (sizeof(req) + 15) / 4, JBB);

    buildPtr.p->m_outstanding++;
    if (buildPtr.p->m_outstanding >= m_max_parallel_index_build)
    {
      jam();
      return;
    }
  }

  if (buildPtr.p->m_outstanding == 0)
  {
    jam();
    AlterTabReq* req = (AlterTabReq*)signal->getDataPtrSend();
    /**
     * Note: before 7.3.4, 7.2.15, 7.1.30 fifth word and
     * up was undefined.
     */
    bzero(req, sizeof(*req));
    req->senderRef = reference();
    req->senderData = buildPtrI;
    req->tableId = buildReq->tableId;
    req->requestType = AlterTabReq::AlterTableReadWrite;
    sendSignal(calcInstanceBlockRef(DBLQH), GSN_ALTER_TAB_REQ, signal,
               AlterTabReq::SignalLength, JBB);
    return;
  }
  else
  {
    jam();
    // wait for replies
    return;
  }
}

int
Dbtup::mt_scan_init(Uint32 tableId, Uint32 fragId,
                    Local_key* pos, Uint32 * fragPtrI)
{
  TablerecPtr tablePtr;
  tablePtr.i = tableId;
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);

  FragrecordPtr fragPtr;
  fragPtr.i = RNIL;
  for (Uint32 i = 0; i<NDB_ARRAY_SIZE(tablePtr.p->fragid); i++)
  {
    if (tablePtr.p->fragid[i] == fragId)
    {
      fragPtr.i = tablePtr.p->fragrec[i];
      break;
    }
  }

  if (fragPtr.i == RNIL)
    return -1;

  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);

  Uint32 fragPageId = 0;
  while (fragPageId < fragPtr.p->m_max_page_cnt)
  {
    Uint32 realPageId= getRealpidCheck(fragPtr.p, fragPageId);
    if (realPageId != RNIL)
    {
      * fragPtrI = fragPtr.i;
      pos->m_page_no = realPageId;
      pos->m_page_idx = 0;
      pos->m_file_no = 0;
      return 0;
    }
    fragPageId++;
  }

  return 1;
}

int
Dbtup::mt_scan_next(Uint32 tableId, Uint32 fragPtrI,
                    Local_key* pos, bool moveNext)
{
  TablerecPtr tablePtr;
  tablePtr.i = tableId;
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);

  FragrecordPtr fragPtr;
  fragPtr.i = fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);

  Uint32 tupheadsize = tablePtr.p->m_offsets[MM].m_fix_header_size;
  if (moveNext)
  {
    pos->m_page_idx += tupheadsize;
  }

  PagePtr pagePtr;
  c_page_pool.getPtr(pagePtr, pos->m_page_no);

  while (1)
  {
    Tuple_header* tuple_ptr;
    while (pos->m_page_idx + tupheadsize <= Fix_page::DATA_WORDS)
    {
      tuple_ptr = (Tuple_header*)(pagePtr.p->m_data + pos->m_page_idx);
      // skip over free tuple
      if (tuple_ptr->m_header_bits & Tuple_header::FREE)
      {
        pos->m_page_idx += tupheadsize;
        continue;
      }
      pos->m_file_no = tuple_ptr->get_tuple_version();
      return 0; // Found
    }

    // End of page...move to next
    Uint32 fragPageId = pagePtr.p->frag_page_id + 1;
    while (fragPageId < fragPtr.p->m_max_page_cnt)
    {
      Uint32 realPageId = getRealpidCheck(fragPtr.p, fragPageId);
      if (realPageId != RNIL)
      {
        pos->m_page_no = realPageId;
        break;
      }
      fragPageId++;
    }

    if (fragPageId == fragPtr.p->m_max_page_cnt)
      break;

    pos->m_page_idx = 0;
    c_page_pool.getPtr(pagePtr, pos->m_page_no);
  }

  return 1;
}

void
Dbtup::execBUILD_INDX_IMPL_REF(Signal* signal)
{
  jamEntry();
  BuildIndxImplRef* ref = (BuildIndxImplRef*)signal->getDataPtrSend();
  Uint32 ptr = ref->senderData;
  Uint32 err = ref->errorCode;

  BuildIndexPtr buildPtr;
  c_buildIndexList.getPtr(buildPtr, ptr);
  ndbrequire(buildPtr.p->m_outstanding);
  buildPtr.p->m_outstanding--;

  TablerecPtr tablePtr;
  (void)tablePtr; // hide unused warning
  buildPtr.p->m_errorCode = (BuildIndxImplRef::ErrorCode)err;
  // No point in starting any more
  buildPtr.p->m_fragNo = NDB_ARRAY_SIZE(tablePtr.p->fragrec);
  buildIndexOffline_table_readonly(signal, ptr);
}

void
Dbtup::execBUILD_INDX_IMPL_CONF(Signal* signal)
{
  jamEntry();
  BuildIndxImplConf* conf = (BuildIndxImplConf*)signal->getDataPtrSend();
  Uint32 ptr = conf->senderData;

  BuildIndexPtr buildPtr;
  c_buildIndexList.getPtr(buildPtr, ptr);
  ndbrequire(buildPtr.p->m_outstanding);
  buildPtr.p->m_outstanding--;
  buildPtr.p->m_fragNo++;

  buildIndexOffline_table_readonly(signal, ptr);
}

void
Dbtup::buildIndexReply(Signal* signal, const BuildIndexRec* buildPtrP)
{
  const BuildIndxImplReq* buildReq = &buildPtrP->m_request;

  AlterIndxImplReq* req = (AlterIndxImplReq*)signal->getDataPtrSend();
  req->indexId = buildReq->indexId;
  req->senderRef = 0; //
  if (buildPtrP->m_errorCode == BuildIndxImplRef::NoError)
  {
    jam();
    req->requestType = AlterIndxImplReq::AlterIndexOnline;
  }
  else
  {
    jam();
    req->requestType = AlterIndxImplReq::AlterIndexOffline;
  }
  EXECUTE_DIRECT(DBTUX, GSN_ALTER_INDX_IMPL_REQ, signal, 
                 AlterIndxImplReq::SignalLength);

  if (buildPtrP->m_errorCode == BuildIndxImplRef::NoError) {
    jam();
    BuildIndxImplConf* conf =
      (BuildIndxImplConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = buildReq->senderData;

    sendSignal(buildReq->senderRef, GSN_BUILD_INDX_IMPL_CONF,
               signal, BuildIndxImplConf::SignalLength, JBB);
  } else {
    jam();
    BuildIndxImplRef* ref =
      (BuildIndxImplRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = buildReq->senderData;
    ref->errorCode = buildPtrP->m_errorCode;

    sendSignal(buildReq->senderRef, GSN_BUILD_INDX_IMPL_REF,
               signal, BuildIndxImplRef::SignalLength, JBB);
  }
}
