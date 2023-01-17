/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#define DBTUP_C
#define DBTUP_INDEX_CPP
#include <cstring>
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

#include <EventLogger.hpp>

#if (defined(VM_TRACE) || defined(ERROR_INSERT))
//#define DEBUG_INDEX_BUILD 1
#endif

#ifdef DEBUG_INDEX_BUILD
#define DEB_INDEX_BUILD(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_INDEX_BUILD(arglist) do { } while (0)
#endif

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
  ndbrequire(c_page_pool.getPtr(pagePtr, pageId));
  lkey1 = pagePtr.p->frag_page_id;
  lkey2 = pageIndex;
}

/**
 * Can be called from MT-build of ordered indexes.
 */
int
Dbtup::tuxAllocNode(EmulatedJamBuffer * jamBuf,
                    Uint32 *fragPtrP_input,
                    Uint32 *tablePtrP_input,
                    Uint32& pageId,
                    Uint32& pageOffset,
                    Uint32*& node)
{
  thrjamEntry(jamBuf);
  Tablerec* tablePtrP = (Tablerec*)tablePtrP_input;
  Fragrecord* fragPtrP = (Fragrecord*)fragPtrP_input;

  Local_key key;
  Uint32* ptr, frag_page_id, err;
  c_allow_alloc_spare_page=true;
  if ((ptr = alloc_fix_rec(jamBuf,
                           &err,
                           fragPtrP,
                           tablePtrP,
                           &key, 
                           &frag_page_id)) == 0)
  {
    c_allow_alloc_spare_page=false;
    thrjam(jamBuf);
    return err;
  }
  release_frag_mutex(fragPtrP, frag_page_id);
  c_allow_alloc_spare_page=false;
  pageId= key.m_page_no;
  pageOffset= key.m_page_idx;
  Uint32 attrDescIndex= tablePtrP->tabDescriptor + (0 << ZAD_LOG_SIZE);
  Uint32 attrDataOffset= AttributeOffset::getOffset(
                              tableDescriptor[attrDescIndex + 1].tabDescr);
  node= ptr + attrDataOffset;
  return 0;
}

void
Dbtup::tuxFreeNode(Uint32* fragPtrP_input,
                   Uint32* tablePtrP_input,
                   Uint32 pageId,
                   Uint32 pageOffset,
                   Uint32* node)
{
  jamEntry();
  Tablerec* tablePtrP = (Tablerec*)tablePtrP_input;
  Fragrecord* fragPtrP = (Fragrecord*)fragPtrP_input;

  Local_key key;
  key.m_page_no = pageId;
  key.m_page_idx = pageOffset;
  PagePtr pagePtr;
  Tuple_header* ptr = (Tuple_header*)get_ptr(&pagePtr, &key, tablePtrP);

  Uint32 attrDescIndex= tablePtrP->tabDescriptor + (0 << ZAD_LOG_SIZE);
  Uint32 attrDataOffset= AttributeOffset::getOffset(tableDescriptor[attrDescIndex + 1].tabDescr);
  ndbrequire(node == (Uint32*)ptr + attrDataOffset);

  free_fix_rec(fragPtrP, tablePtrP, &key, (Fix_page*)pagePtr.p);
}

int
Dbtup::tuxReadAttrsCurr(EmulatedJamBuffer *jamBuf,
                        const Uint32* attrIds,
                        Uint32 numAttrs,
                        Uint32* dataOut,
                        bool xfrmFlag,
                        Uint32 tupVersion)
{
  thrjamEntryDebug(jamBuf);
  // use own variables instead of globals
  Fragrecord *fragPtrP = prepare_fragptr.p;
  Tablerec *tablePtrP = prepare_tabptr.p;

  // search for tuple version if not original
  Operationrec tmpOp;
  KeyReqStruct req_struct(jamBuf);
  req_struct.tablePtrP = tablePtrP;
  req_struct.fragPtrP = fragPtrP;

  tmpOp.op_type = ZREAD; // valgrind
  setup_fixed_tuple_ref_opt(&req_struct);
  setup_fixed_part(&req_struct, &tmpOp, tablePtrP);

  return tuxReadAttrsCommon(req_struct,
                            attrIds,
                            numAttrs,
                            dataOut,
                            xfrmFlag,
                            tupVersion);
}

/**
 * This method can be called from MT-build of
 * ordered indexes.
 */
int
Dbtup::tuxReadAttrsOpt(EmulatedJamBuffer * jamBuf,
                       Uint32* fragPtrP,
                       Uint32* tablePtrP,
                       Uint32 pageId,
                       Uint32 pageIndex,
                       Uint32 tupVersion,
                       const Uint32* attrIds,
                       Uint32 numAttrs,
                       Uint32* dataOut,
                       bool xfrmFlag)
{
  thrjamEntryDebug(jamBuf);
  // search for tuple version if not original

  Operationrec tmpOp;
  KeyReqStruct req_struct(jamBuf);
  req_struct.tablePtrP = (Tablerec*)tablePtrP;
  req_struct.fragPtrP = (Fragrecord*)fragPtrP;

  tmpOp.m_tuple_location.m_page_no= pageId;
  tmpOp.m_tuple_location.m_page_idx= pageIndex;
  tmpOp.op_type = ZREAD; // valgrind
  setup_fixed_tuple_ref(&req_struct,
                        &tmpOp,
                        (Tablerec*)tablePtrP);
  setup_fixed_part(&req_struct,
                   &tmpOp,
                   (Tablerec*)tablePtrP);
  return tuxReadAttrsCommon(req_struct,
                            attrIds,
                            numAttrs,
                            dataOut,
                            xfrmFlag,
                            tupVersion);
}

int
Dbtup::tuxReadAttrsCommon(KeyReqStruct &req_struct,
                          const Uint32* attrIds,
                          Uint32 numAttrs,
                          Uint32* dataOut,
                          bool xfrmFlag,
                          Uint32 tupVersion)
{
  /**
   * This function can be called from both LDM threads and from Query
   * threads. However the list of operations can only be operations
   * created in the LDM thread.
   *
   * To handle this we call getOperationPtrP on the TUP block in the
   * LDM thread.
   */
  Tuple_header *tuple_ptr = req_struct.m_tuple_ptr;
  if (tuple_ptr->get_tuple_version() != tupVersion)
  {
    thrjamDebug(req_struct.jamBuffer);
    OperationrecPtr opPtr;
    opPtr.i = tuple_ptr->m_operation_ptr_i;
    Uint32 loopGuard= 0;
    while (opPtr.i != RNIL)
    {
      opPtr.p = m_ldm_instance_used->getOperationPtrP(opPtr.i);
      if (opPtr.p->op_struct.bit_field.tupVersion == tupVersion)
      {
        thrjamDebug(req_struct.jamBuffer);
	if (!opPtr.p->m_copy_tuple_location.isNull())
        {
	  req_struct.m_tuple_ptr=
            get_copy_tuple(&opPtr.p->m_copy_tuple_location);
        }
	break;
      }
      thrjamDebug(req_struct.jamBuffer);
      opPtr.i= opPtr.p->prevActiveOp;
      ndbrequire(++loopGuard < (1 << ZTUP_VERSION_BITS));
    }
  }
  // read key attributes from found tuple version
  // save globals
  prepare_read(&req_struct, req_struct.tablePtrP, false); 

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
Dbtup::tuxReadPk(Uint32* fragPtrP_input,
                 Uint32* tablePtrP_input,
                 Uint32 pageId,
                 Uint32 pageIndex,
                 Uint32* dataOut,
                 bool xfrmFlag)
{
  jamEntryDebug();
  Fragrecord* fragPtrP = (Fragrecord*)fragPtrP_input;
  Tablerec* tablePtrP = (Tablerec*)tablePtrP_input;
  
  Operationrec tmpOp;
  tmpOp.m_tuple_location.m_page_no= pageId;
  tmpOp.m_tuple_location.m_page_idx= pageIndex;
  
  KeyReqStruct req_struct(this);
  req_struct.tablePtrP = tablePtrP;
  req_struct.fragPtrP = fragPtrP;
 
  PagePtr page_ptr;
  Uint32* ptr= get_ptr(&page_ptr, &tmpOp.m_tuple_location, tablePtrP);
  req_struct.m_page_ptr = page_ptr;
  req_struct.m_tuple_ptr = (Tuple_header*)ptr;
  
  int ret = 0;
  /**
   * Coming here from ACC means that we hold the page map mutex, and it
   * also means that the entry is in DBACC and the local key have been
   * set. In this state only a commit of a delete can change the state to
   * be FREE or an abort of an INSERT. This state change is in both cases
   * performed using exclusive fragment access. Thus we can rely on this
   * bit even without using the TUP fragment mutex.
   *
   * When accessing from TUX we know that the entry has been inserted
   * into the ordered index and not yet removed. Thus the FREE cannot
   * be set, this also requires no mutex protection since adding and
   * removing index entries cannot happen in parallel with index scans.
   *
   * Coming from DBTUP in a TUP scan we did check that the state wasn't
   * FREE before we arrived here and that read was done with mutex
   * protection. Thus only an exclusive access can set it back to FREE
   * again and this cannot happen while we are performing a TUP scan.
   */
  if (unlikely(req_struct.m_tuple_ptr->m_header_bits & Tuple_header::FREE))
  {
    /**
     * The tuple has been deleted and committed to be deleted already.
     * If we come here from DBTUX and DBTUP we will crash. If we come here
     * from DBACC we have to do a deeper analysis before we decide on what
     * to do.
     */
    jam();
    return -ZTUPLE_DELETED_ERROR; /* Leads to crash in DBTUX and DBTUP */
  }
  req_struct.check_offset[MM]= tablePtrP->get_check_offset(MM);
  req_struct.check_offset[DD]= tablePtrP->get_check_offset(DD);
    
  Uint32 num_attr= tablePtrP->m_no_of_attributes;
  Uint32 descr_start= tablePtrP->tabDescriptor;
  TableDescriptor *tab_descr= &tableDescriptor[descr_start];
  ndbrequire(descr_start + (num_attr << ZAD_LOG_SIZE) <= cnoOfTabDescrRec);
  req_struct.attr_descr= tab_descr; 

  /**
   * Resetting the ALLOC bit can only happen in exclusive access from
   * abort or commit.
   *
   * The ALLOC bit is set during INSERT when the row didn't previously
   * exist. This happens before the row is inserted into any ordered index
   * and thus it is safe to read this bit without mutex when coming from
   * TUX to read the row.
   *
   * When coming from ACC the ALLOC bit is set before the local key is
   * updated and we can only arrive here if the local key has been updated.
   * Thus it is safe to read this also when coming from ACC without mutex.
   *
   * When arriving here from a TUP scan we read the header bits with mutex
   * protection and found a row where the FREE bit wasn't set. During the
   * initial INSERT of a row we hold the mutex during the time that we
   * update the ALLOC bit and set the operation pointer in the record.
   * Thus when we arrive here from a TUP scan the row cannot change the
   * ALLOC bit. Either the bit was set when reading the header bits in the
   * TUP scan, if so they will remain set until we get exclusive access to
   * the fragment. Otherwise the ALLOC bit wasn't set, but also the FREE
   * bit wasn't set and thus the row contained a proper row that can be
   * read and thus the ALLOC bit cannot change after reading it in the
   * TUP scan and we can trust it to be the same here without using a
   * mutex to protect the read.
   */
  if (unlikely(req_struct.m_tuple_ptr->m_header_bits & Tuple_header::ALLOC))
  {
    jam();
    OperationrecPtr opPtr;
    opPtr.i = req_struct.m_tuple_ptr->m_operation_ptr_i;
    /**
     * The operation pointer is in the LDM thread, we need to get the memory
     * address of it from the owning LDM thread, we cannot access it from this
     * query thread directly.
     */
    opPtr.p = m_ldm_instance_used->getOperationPtrP(opPtr.i);
    ndbrequire(!opPtr.p->m_copy_tuple_location.isNull());
    req_struct.m_tuple_ptr=
      get_copy_tuple(&opPtr.p->m_copy_tuple_location);
  }
  prepare_read(&req_struct, tablePtrP, false);
    
  const Uint32* attrIds= &tableDescriptor[tablePtrP->readKeyArray].tabDescr;
  const Uint32 numAttrs= tablePtrP->noOfKeyAttr;
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
    while (n < numAttrs)
    {
      const AttributeHeader ah(dataOut[i]);
      Uint32 size= ah.getDataSize();
      ndbrequire(size != 0);
      for (Uint32 j= 0; j < size; j++)
      {
        dataOut[i + j - n]= dataOut[i + j + 1];
      }
      n+= 1;
      i+= 1 + size;
    }
    ndbrequire((int)i == ret);
    ret -= numAttrs;
  }
  else
  {
    jam();
    return ret;
  }
  if (likely(tablePtrP->m_bits & Tablerec::TR_RowGCI))
  {
    dataOut[ret] = *req_struct.m_tuple_ptr->get_mm_gci(tablePtrP);
  }
  else
  {
    dataOut[ret] = 0;
  }
  return ret;
}

int
Dbtup::accReadPk(Uint32 fragPageId,
                 Uint32 pageIndex,
                 Uint32* dataOut,
                 bool xfrmFlag)
{
  jamEntryDebug();
  // get real page id and tuple offset
  Uint32 pageId = getRealpid(prepare_fragptr.p, fragPageId);
  // use TUX routine - optimize later
  int ret = tuxReadPk((Uint32*)prepare_fragptr.p,
                      (Uint32*)prepare_tabptr.p,
                      pageId,
                      pageIndex,
                      dataOut,
                      xfrmFlag);
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
 *
 * This method can be called from a query thread, thus all accesses
 * to fetch operation records must refer to the blocks in the LDM
 * owning the fragment since only the LDM thread is allowed to
 * insert operation records into the linked list of operations
 * found in the row header.
 *
 */
bool
Dbtup::tuxQueryTh(Uint32 opPtrI,
                  Uint32 tupVersion,
                  Uint32 transId1,
                  Uint32 transId2,
                  bool dirty,
                  Uint32 savepointId)
{
  jamEntryDebug();

  OperationrecPtr currOpPtr;
  currOpPtr.i = opPtrI;
  currOpPtr.p = m_ldm_instance_used->getOperationPtrP(currOpPtr.i);

  const bool sameTrans =
    c_lqh->m_ldm_instance_used->is_same_trans(currOpPtr.p->userpointer,
                                              transId1,
                                              transId2);

  bool res = false;
  OperationrecPtr loopOpPtr = currOpPtr;

  if (!sameTrans)
  {
    jamDebug();
    if (!dirty)
    {
      jamDebug();
      if (currOpPtr.p->nextActiveOp == RNIL)
      {
        jamDebug();
        // last op - TUX makes ACC lock request in same timeslice
        res = true;
      }
    }
    else
    {
      // loop to first op (returns false)
      m_ldm_instance_used->find_savepoint(loopOpPtr,
                                          0,
                                          jamBuffer());
      const Uint32 op_type = loopOpPtr.p->op_type;

      if (op_type != ZINSERT)
      {
        jamDebug();
        // read committed version
        Tuple_header *tuple_ptr = (Tuple_header*)prepare_tuple_ptr;
        const Uint32 origVersion = tuple_ptr->get_tuple_version();
        if (origVersion == tupVersion)
        {
          jamDebug();
          res = true;
        }
      }
    }
  }
  else
  {
    jamDebug();
    // for own trans, ignore dirty flag

    if (m_ldm_instance_used->find_savepoint(loopOpPtr,
                                            savepointId,
                                            jamBuffer()))
    {
      jamDebug();
      const Uint32 op_type = loopOpPtr.p->op_type;

      if (op_type != ZDELETE)
      {
        jamDebug();
        // check if this op has produced the scanned version
        Uint32 loopVersion = loopOpPtr.p->op_struct.bit_field.tupVersion;
        if (loopVersion == tupVersion)
        {
          jamDebug();
          res = true;
        }
      }
    }
  }
  return res;
}

/**
 * This method is still used by index statistics and debug code.
 */
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
  return tuxReadAttrsCommon(req_struct,
                            attrIds,
                            numAttrs,
                            dataOut,
                            xfrmFlag,
                            tupVersion);
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
  ndbassert(!m_is_query_block);
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

    ndbrequire(c_page_pool.getPtr(pagePtr, realPageId));

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
      preferable to store in TUX.
      */
      jam();

      /**
       * Since copy tuples now can't be found on real pages.
       *   we will here build all copies of the tuple
       *
       * Note only "real" tupVersion's should be added 
       *      i.e delete's shouldn't be added 
       *      (unless it's the first op, when "original" should be added)
       */

      /*
       * Start from first operation.  This is only to make things more
       * clear.  It is not required by ordered index implementation.
       */
      ndbrequire(c_operation_pool.getValidPtr(pageOperPtr));
      while (pageOperPtr.p->prevActiveOp != RNIL)
      {
        jam();
        pageOperPtr.i = pageOperPtr.p->prevActiveOp;
        ndbrequire(c_operation_pool.getValidPtr(pageOperPtr));
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
        ndbrequire(c_operation_pool.getValidPtr(pageOperPtr));
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
        ndbabort();
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
  ndbassert(!m_is_query_block);
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
  std::memset(req, 0, sizeof(*req));
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
  ndbassert(!m_is_query_block);
  BuildIndexPtr buildPtr;
  buildPtr.i= buildPtrI;
  c_buildIndexList.getPtr(buildPtr);
  const BuildIndxImplReq* buildReq = 
    (const BuildIndxImplReq*)&buildPtr.p->m_request;
  // get table
  TablerecPtr tablePtr;
  tablePtr.i= buildReq->tableId;
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);

  DEB_INDEX_BUILD(("(%u) Starting index build of primary table %u"
                   ", ordered index table %u",
                   instance(),
                   tablePtr.i,
                   buildPtr.p->m_indexId));
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
    std::memset(&req, 0, sizeof(req));
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
    req.buffer_size = 32*32768; // thread-local-buffer

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
    std::memset(req, 0, sizeof(*req));
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
    /**
     * This code is executed in NDBFS threads at two occasions, during
     * restart with parallel index builds and as an offline index build
     * process.
     *
     * In the restart case there is no other activity ongoing so there
     * is no risk of concurrent access to the fragment map in DBTUP.
     * We don't need to initialise page map entries that are missing
     * since we are only interested if the page exists and if so its
     * page id.
     *
     * The second case happens only when the node has been started and
     * the table is in read only. The node being restarted means that
     * a local checkpoint has the scanned table. The table scan performed
     * by LCPs use TUP scans and this scan will ensure that no holes
     * are left in the fragment page map. Thus after a restart we have
     * no holes in the fragment page map.
     *
     * In addition for offline index builds the table is in read only,
     * thus no new pages will be added to the fragment page map while
     * we are scanning.
     */
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
  ndbrequire(c_page_pool.getPtr(pagePtr, pos->m_page_no));

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
    ndbrequire(c_page_pool.getPtr(pagePtr, pos->m_page_no));
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
