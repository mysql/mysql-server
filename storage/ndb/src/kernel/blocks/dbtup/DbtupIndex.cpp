/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#define DBTUP_C
#define DBTUP_INDEX_CPP
#include <Dblqh.hpp>
#include "Dbtup.hpp"
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <pc.hpp>
#include <AttributeDescriptor.hpp>
#include "AttributeOffset.hpp"
#include <AttributeHeader.hpp>
#include <signaldata/TuxMaint.hpp>

// methods used by ordered index

void
Dbtup::tuxGetTupAddr(Uint32 fragPtrI,
                     Uint32 pageId,
                     Uint32 pageIndex,
                     Uint32& tupAddr)
{
  jamEntry();
  PagePtr pagePtr;
  c_page_pool.getPtr(pagePtr, pageId);
  Uint32 fragPageId= pagePtr.p->frag_page_id;
  tupAddr= (fragPageId << MAX_TUPLES_BITS) | pageIndex;
}

int
Dbtup::tuxAllocNode(Signal* signal,
                    Uint32 fragPtrI,
                    Uint32& pageId,
                    Uint32& pageOffset,
                    Uint32*& node)
{
  jamEntry();
  FragrecordPtr fragPtr;
  fragPtr.i= fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
  TablerecPtr tablePtr;
  tablePtr.i= fragPtr.p->fragTableId;
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);
  terrorCode= 0;

  Local_key key;
  Uint32* ptr, frag_page_id;
  if ((ptr= alloc_fix_rec(fragPtr.p, tablePtr.p, &key, &frag_page_id)) == 0)
  {
    jam();
    terrorCode = ZMEM_NOMEM_ERROR; // caller sets error
    return terrorCode;
  }
  pageId= key.m_page_no;
  pageOffset= key.m_page_idx;
  Uint32 attrDescIndex= tablePtr.p->tabDescriptor + (0 << ZAD_LOG_SIZE);
  Uint32 attrDataOffset= AttributeOffset::getOffset(
                              tableDescriptor[attrDescIndex + 1].tabDescr);
  node= ptr + attrDataOffset;
  return 0;
}

#if 0
void
Dbtup::tuxFreeNode(Signal* signal,
                   Uint32 fragPtrI,
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
  PagePtr pagePtr;
  pagePtr.i= pageId;
  ptrCheckGuard(pagePtr, cnoOfPage, cpage);
  Uint32 attrDescIndex= tablePtr.p->tabDescriptor + (0 << ZAD_LOG_SIZE);
  Uint32 attrDataOffset= AttributeOffset::getOffset(tableDescriptor[attrDescIndex + 1].tabDescr);
  ndbrequire(node == &pagePtr.p->pageWord[pageOffset] + attrDataOffset);
  freeTh(fragPtr.p, tablePtr.p, signal, pagePtr.p, pageOffset);
}
#endif

void
Dbtup::tuxGetNode(Uint32 fragPtrI,
                  Uint32 pageId,
                  Uint32 pageOffset,
                  Uint32*& node)
{
  jamEntry();
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
Dbtup::tuxReadAttrs(Uint32 fragPtrI,
                    Uint32 pageId,
                    Uint32 pageIndex,
                    Uint32 tupVersion,
                    const Uint32* attrIds,
                    Uint32 numAttrs,
                    Uint32* dataOut)
{
  jamEntry();
  // use own variables instead of globals
  FragrecordPtr fragPtr;
  fragPtr.i= fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
  TablerecPtr tablePtr;
  tablePtr.i= fragPtr.p->fragTableId;
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);

  // search for tuple version if not original

  Operationrec tmpOp;
  KeyReqStruct req_struct;
  tmpOp.m_tuple_location.m_page_no= pageId;
  tmpOp.m_tuple_location.m_page_idx= pageIndex;

  setup_fixed_part(&req_struct, &tmpOp, tablePtr.p);
  Tuple_header *tuple_ptr= req_struct.m_tuple_ptr;
  if (tuple_ptr->get_tuple_version() != tupVersion)
  {
    jam();
    OperationrecPtr opPtr;
    opPtr.i= tuple_ptr->m_operation_ptr_i;
    Uint32 loopGuard= 0;
    while (opPtr.i != RNIL) {
      c_operation_pool.getPtr(opPtr);
      if (opPtr.p->tupVersion == tupVersion) {
	jam();
	if (!opPtr.p->m_copy_tuple_location.isNull()) {
	  req_struct.m_tuple_ptr= (Tuple_header*)
	    c_undo_buffer.get_ptr(&opPtr.p->m_copy_tuple_location);
        }
	break;
      }
      jam();
      opPtr.i= opPtr.p->prevActiveOp;
      ndbrequire(++loopGuard < (1 << ZTUP_VERSION_BITS));
    }
  }
  // read key attributes from found tuple version
  // save globals
  TablerecPtr tabptr_old= tabptr;
  FragrecordPtr fragptr_old= fragptr;
  OperationrecPtr operPtr_old= operPtr;
  // new globals
  tabptr= tablePtr;
  fragptr= fragPtr;
  operPtr.i= RNIL;
  operPtr.p= NULL;
  prepare_read(&req_struct, tablePtr.p, false); 

  // do it
  int ret = readAttributes(&req_struct,
                           attrIds,
                           numAttrs,
                           dataOut,
                           ZNIL,
                           true);

  // restore globals
  tabptr= tabptr_old;
  fragptr= fragptr_old;
  operPtr= operPtr_old;
  // done
  if (ret == -1) {
    ret = terrorCode ? (-(int)terrorCode) : -1;
  }
  return ret;
}
int
Dbtup::tuxReadPk(Uint32 fragPtrI, Uint32 pageId, Uint32 pageIndex, Uint32* dataOut, bool xfrmFlag)
{
  jamEntry();
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
  
  KeyReqStruct req_struct;
 
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
      req_struct.m_tuple_ptr= (Tuple_header*)
	c_undo_buffer.get_ptr(&opPtrP->m_copy_tuple_location);
    }
    prepare_read(&req_struct, tablePtr.p, false);
    
    const Uint32* attrIds= &tableDescriptor[tablePtr.p->readKeyArray].tabDescr;
    const Uint32 numAttrs= tablePtr.p->noOfKeyAttr;
    // read pk attributes from original tuple
    
    // save globals
    TablerecPtr tabptr_old= tabptr;
    FragrecordPtr fragptr_old= fragptr;
    OperationrecPtr operPtr_old= operPtr;
    
    // new globals
    tabptr= tablePtr;
    fragptr= fragPtr;
    operPtr.i= RNIL;
    operPtr.p= NULL;
    
    // do it
    ret = readAttributes(&req_struct,
			 attrIds,
			 numAttrs,
			 dataOut,
			 ZNIL,
			 xfrmFlag);
    // restore globals
    tabptr= tabptr_old;
    fragptr= fragptr_old;
    operPtr= operPtr_old;
    // done
    if (ret != -1) {
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
      ret= terrorCode ? (-(int)terrorCode) : -1;
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
  jamEntry();
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
  jamEntry();
  FragrecordPtr fragPtr;
  fragPtr.i= fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
  TablerecPtr tablePtr;
  tablePtr.i= fragPtr.p->fragTableId;
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);
  PagePtr pagePtr;
  pagePtr.i = pageId;
  c_page_pool.getPtr(pagePtr);

  KeyReqStruct req_struct;

  {
    Operationrec tmpOp;
    tmpOp.m_tuple_location.m_page_no = pageId;
    tmpOp.m_tuple_location.m_page_idx = pageIndex;
    setup_fixed_part(&req_struct, &tmpOp, tablePtr.p);
  }

  Tuple_header* tuple_ptr = req_struct.m_tuple_ptr;

  OperationrecPtr currOpPtr;
  currOpPtr.i = tuple_ptr->m_operation_ptr_i;
  if (currOpPtr.i == RNIL) {
    jam();
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
      const Uint32 op_type = loopOpPtr.p->op_struct.op_type;

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
      const Uint32 op_type = loopOpPtr.p->op_struct.op_type;

      if (op_type != ZDELETE) {
        jam();
        // check if this op has produced the scanned version
        Uint32 loopVersion = loopOpPtr.p->tupVersion;
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
  NDB_TICKS tot_time_passed;
  Uint32 number_events;
#endif
void
Dbtup::execBUILDINDXREQ(Signal* signal)
{
  jamEntry();
#ifdef TIME_MEASUREMENT
  time_events= 0;
  tot_time_passed= 0;
  number_events= 1;
#endif
  // get new operation
  BuildIndexPtr buildPtr;
  if (! c_buildIndexList.seize(buildPtr)) {
    jam();
    BuildIndexRec buildRec;
    memcpy(buildRec.m_request, signal->theData, sizeof(buildRec.m_request));
    buildRec.m_errorCode= BuildIndxRef::Busy;
    buildIndexReply(signal, &buildRec);
    return;
  }
  memcpy(buildPtr.p->m_request,
         signal->theData,
         sizeof(buildPtr.p->m_request));
  // check
  buildPtr.p->m_errorCode= BuildIndxRef::NoError;
  do {
    const BuildIndxReq* buildReq= (const BuildIndxReq*)buildPtr.p->m_request;
    if (buildReq->getTableId() >= cnoOfTablerec) {
      jam();
      buildPtr.p->m_errorCode= BuildIndxRef::InvalidPrimaryTable;
      break;
    }
    TablerecPtr tablePtr;
    tablePtr.i= buildReq->getTableId();
    ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);
    if (tablePtr.p->tableStatus != DEFINED) {
      jam();
      buildPtr.p->m_errorCode= BuildIndxRef::InvalidPrimaryTable;
      break;
    }
    // memory page format
    buildPtr.p->m_build_vs =
      (tablePtr.p->m_attributes[MM].m_no_of_varsize +
       tablePtr.p->m_attributes[MM].m_no_of_dynamic) > 0;
    if (DictTabInfo::isOrderedIndex(buildReq->getIndexType())) {
      jam();
      const DLList<TupTriggerData>& triggerList = 
	tablePtr.p->tuxCustomTriggers;

      TriggerPtr triggerPtr;
      triggerList.first(triggerPtr);
      while (triggerPtr.i != RNIL) {
	if (triggerPtr.p->indexId == buildReq->getIndexId()) {
	  jam();
	  break;
	}
	triggerList.next(triggerPtr);
      }
      if (triggerPtr.i == RNIL) {
	jam();
	// trigger was not created
	buildPtr.p->m_errorCode = BuildIndxRef::InternalError;
	break;
      }
      buildPtr.p->m_indexId = buildReq->getIndexId();
      buildPtr.p->m_buildRef = DBTUX;
    } else if(buildReq->getIndexId() == RNIL) {
      jam();
      // REBUILD of acc
      buildPtr.p->m_indexId = RNIL;
      buildPtr.p->m_buildRef = DBACC;
    } else {
      jam();
      buildPtr.p->m_errorCode = BuildIndxRef::InvalidIndexType;
      break;
    }

    // set to first tuple position
    const Uint32 firstTupleNo = 0;
    buildPtr.p->m_fragNo= 0;
    buildPtr.p->m_pageId= 0;
    buildPtr.p->m_tupleNo= firstTupleNo;
    // start build
    buildIndex(signal, buildPtr.i);
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
  const BuildIndxReq* buildReq= (const BuildIndxReq*)buildPtr.p->m_request;
  // get table
  TablerecPtr tablePtr;
  tablePtr.i= buildReq->getTableId();
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);

  const Uint32 firstTupleNo = 0;
  const Uint32 tupheadsize = tablePtr.p->m_offsets[MM].m_fix_header_size;

#ifdef TIME_MEASUREMENT
  MicroSecondTimer start;
  MicroSecondTimer stop;
  NDB_TICKS time_passed;
#endif
  do {
    // get fragment
    FragrecordPtr fragPtr;
    if (buildPtr.p->m_fragNo == MAX_FRAG_PER_NODE) {
      jam();
      // build ready
      buildIndexReply(signal, buildPtr.p);
      c_buildIndexList.release(buildPtr);
      return;
    }
    ndbrequire(buildPtr.p->m_fragNo < MAX_FRAG_PER_NODE);
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
    if (buildPtr.p->m_pageId >= fragPtr.p->noOfPages) {
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
      buildPtr.p->m_pageId++;
      buildPtr.p->m_tupleNo= firstTupleNo;
      break;
    }

    c_page_pool.getPtr(pagePtr, realPageId);
    Uint32 pageState= pagePtr.p->page_state;
    // skip empty page
    if (pageState == ZEMPTY_MM) {
      jam();
      buildPtr.p->m_pageId++;
      buildPtr.p->m_tupleNo= firstTupleNo;
      break;
    }
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
    NdbTick_getMicroTimer(&start);
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
      do 
      {
	c_operation_pool.getPtr(pageOperPtr);
	if(pageOperPtr.p->op_struct.op_type != ZDELETE ||
	   pageOperPtr.p->is_first_operation())
	{
	  req->errorCode = RNIL;
	  req->tupVersion= pageOperPtr.p->tupVersion;
	  EXECUTE_DIRECT(buildPtr.p->m_buildRef, GSN_TUX_MAINT_REQ,
			 signal, TuxMaintReq::SignalLength+2);
	}
	else
	{
	  req->errorCode= 0;
	}
	pageOperPtr.i= pageOperPtr.p->prevActiveOp;
      } while(req->errorCode == 0 && pageOperPtr.i != RNIL);
    } 
    
    jamEntry();
    if (req->errorCode != 0) {
      switch (req->errorCode) {
      case TuxMaintReq::NoMemError:
        jam();
        buildPtr.p->m_errorCode= BuildIndxRef::AllocationFailure;
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
    NdbTick_getMicroTimer(&stop);
    time_passed= NdbTick_getMicrosPassed(start, stop);
    if (time_passed < 1000) {
      time_events++;
      tot_time_passed += time_passed;
      if (time_events == number_events) {
        NDB_TICKS mean_time_passed= tot_time_passed /
                                     (NDB_TICKS)number_events;
        ndbout << "Number of events= " << number_events;
        ndbout << " Mean time passed= " << mean_time_passed << endl;
        number_events <<= 1;
        tot_time_passed= (NDB_TICKS)0;
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

void
Dbtup::buildIndexReply(Signal* signal, const BuildIndexRec* buildPtrP)
{
  const BuildIndxReq* const buildReq=
                    (const BuildIndxReq*)buildPtrP->m_request;
  // conf is subset of ref
  BuildIndxRef* rep= (BuildIndxRef*)signal->getDataPtr();
  rep->setUserRef(buildReq->getUserRef());
  rep->setConnectionPtr(buildReq->getConnectionPtr());
  rep->setRequestType(buildReq->getRequestType());
  rep->setTableId(buildReq->getTableId());
  rep->setIndexType(buildReq->getIndexType());
  rep->setIndexId(buildReq->getIndexId());
  // conf
  if (buildPtrP->m_errorCode == BuildIndxRef::NoError) {
    jam();
    sendSignal(rep->getUserRef(), GSN_BUILDINDXCONF,
        signal, BuildIndxConf::SignalLength, JBB);
    return;
  }
  // ref
  rep->setErrorCode(buildPtrP->m_errorCode);
  sendSignal(rep->getUserRef(), GSN_BUILDINDXREF,
      signal, BuildIndxRef::SignalLength, JBB);
}
