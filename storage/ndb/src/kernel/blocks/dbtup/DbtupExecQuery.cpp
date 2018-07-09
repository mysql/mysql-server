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
#include <dblqh/Dblqh.hpp>
#include "Dbtup.hpp"
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <pc.hpp>
#include <AttributeDescriptor.hpp>
#include "AttributeOffset.hpp"
#include <AttributeHeader.hpp>
#include <Interpreter.hpp>
#include <signaldata/TupKey.hpp>
#include <signaldata/AttrInfo.hpp>
#include <signaldata/TuxMaint.hpp>
#include <signaldata/ScanFrag.hpp>
#include <signaldata/TransIdAI.hpp>
#include <NdbSqlUtil.hpp>
#include <Checksum.hpp>
#include <portlib/ndb_prefetch.h>

#define JAM_FILE_ID 422

#ifdef VM_TRACE
//#define DEBUG_LCP 1
//#define DEBUG_DELETE 1
//#define DEBUG_DELETE_NR 1
//#define DEBUG_LCP_SKIP_DELETE 1
//#define DEBUG_LCP_LGMAN 1
//#define DEBUG_LCP_SKIP_DELETE 1
#endif

#ifdef DEBUG_LCP
#define DEB_LCP(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_LCP(arglist) do { } while (0)
#endif

#ifdef DEBUG_DELETE
#define DEB_DELETE(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_DELETE(arglist) do { } while (0)
#endif

#ifdef DEBUG_LCP_SKIP_DELETE
#define DEB_LCP_SKIP_DELETE(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_LCP_SKIP_DELETE(arglist) do { } while (0)
#endif

#ifdef DEBUG_DELETE_NR
#define DEB_DELETE_NR(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_DELETE_NR(arglist) do { } while (0)
#endif

#ifdef DEBUG_LCP_LGMAN
#define DEB_LCP_LGMAN(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_LCP_LGMAN(arglist) do { } while (0)
#endif

// #define TRACE_INTERPRETER

/* For debugging */
static void
dump_hex(const Uint32 *p, Uint32 len)
{
  if(len > 2560)
    len= 160;
  if(len==0)
    return;
  for(;;)
  {
    if(len>=4)
      ndbout_c("%8p %08X %08X %08X %08X", p, p[0], p[1], p[2], p[3]);
    else if(len>=3)
      ndbout_c("%8p %08X %08X %08X", p, p[0], p[1], p[2]);
    else if(len>=2)
      ndbout_c("%8p %08X %08X", p, p[0], p[1]);
    else
      ndbout_c("%8p %08X", p, p[0]);
    if(len <= 4)
      break;
    len-= 4;
    p+= 4;
  }
}

/**
 * getStoredProcAttrInfo
 *
 * Get the I-Val of the supplied stored procedure's 
 * AttrInfo section
 * Initialise the AttrInfo length in the request
 */
void Dbtup::getStoredProcAttrInfo(Uint32 storedId,
                                 KeyReqStruct* req_struct,
                                 Uint32& attrInfoIVal) 
{
  jamDebug();
  StoredProcPtr storedPtr;
  c_storedProcPool.getPtr(storedPtr, storedId);
  ndbrequire(storedPtr.i != RNIL);
  ndbrequire(((storedPtr.p->storedCode == ZSCAN_PROCEDURE) ||
               (storedPtr.p->storedCode == ZCOPY_PROCEDURE)));
  /* Setup OperationRec with stored procedure AttrInfo section */
  SegmentedSectionPtr sectionPtr;
  getSection(sectionPtr, storedPtr.p->storedProcIVal);
  Uint32 storedProcLen= sectionPtr.sz;

  attrInfoIVal= storedPtr.p->storedProcIVal;
  req_struct->attrinfo_len= storedProcLen;
}

void Dbtup::copyAttrinfo(Uint32 expectedLen,
                         Uint32 attrInfoIVal)
{
  Uint32 *inBuffer = &cinBuffer[0];
  ndbassert( expectedLen > 0 || attrInfoIVal == RNIL );

  if (expectedLen > 0)
  {
    ndbassert( attrInfoIVal != RNIL );
    
    /* Check length in section is as we expect */
    SegmentedSectionPtr sectionPtr;
    getSection(sectionPtr, attrInfoIVal);
    
    ndbrequire(sectionPtr.sz == expectedLen);
    ndbrequire(sectionPtr.sz < ZATTR_BUFFER_SIZE);
    
    /* Copy attrInfo data into linear buffer */
    // TODO : Consider operating TUP out of first segment where
    // appropriate
    copy(inBuffer, attrInfoIVal);
  }
}

Uint32 Dbtup::copyAttrinfo(Uint32 storedProcId)
{
  Uint32 attrinfoVal;
  KeyReqStruct req_struct(this);
  getStoredProcAttrInfo(storedProcId, &req_struct, attrinfoVal);
  copyAttrinfo(req_struct.attrinfo_len, attrinfoVal);
  return req_struct.attrinfo_len;
}

void
Dbtup::setInvalidChecksum(Tuple_header *tuple_ptr,
                          const Tablerec * regTabPtr)
{
  if (regTabPtr->m_bits & Tablerec::TR_Checksum)
  {
    jam();
    /**
     * Set a magic checksum when tuple isn't supposed to be read.
     */
    tuple_ptr->m_checksum = 0x87654321;
  }
}

void
Dbtup::updateChecksum(Tuple_header *tuple_ptr,
                      const Tablerec *regTabPtr,
                      Uint32 old_header,
                      Uint32 new_header)
{
  /**
   * This function is used when only updating the header bits in row.
   * We start by XOR:ing the old header, this negates the impact of the
   * old header since old_header ^ old_header = 0. Next we XOR with new
   * header to get the new checksum and finally we store the new checksum.
   */
  if (regTabPtr->m_bits & Tablerec::TR_Checksum)
  {
    Uint32 checksum = tuple_ptr->m_checksum;
    jam();
    checksum ^= old_header;
    checksum ^= new_header;
    tuple_ptr->m_checksum = checksum;
  }
}

void
Dbtup::setChecksum(Tuple_header* tuple_ptr,
                   const Tablerec* regTabPtr)
{
  if (regTabPtr->m_bits & Tablerec::TR_Checksum)
  {
    jamDebug();
    tuple_ptr->m_checksum= 0;
    tuple_ptr->m_checksum= calculateChecksum(tuple_ptr, regTabPtr);
  }
}

Uint32
Dbtup::calculateChecksum(Tuple_header* tuple_ptr,
                         const Tablerec* regTabPtr)
{
  Uint32 checksum;
  Uint32 rec_size, *tuple_header;
  rec_size= regTabPtr->m_offsets[MM].m_fix_header_size;
  tuple_header= &tuple_ptr->m_header_bits;
  // includes tupVersion
  //printf("%p - ", tuple_ptr);

  /**
   * We include every except the first word of the Tuple header
   * which is only used on copy tuples. We do however include
   * the header bits.
   */
  checksum = computeXorChecksum(
               tuple_header, (rec_size-Tuple_header::HeaderSize) + 1);
  
  //printf("-> %.8x\n", checksum);

#if 0
  if (var_sized) {
    /*
    if (! req_struct->fix_var_together) {
      jam();
      checksum ^= tuple_header[rec_size];
    }
    */
    jam();
    var_data_part= req_struct->var_data_start;
    vsize_words= calculate_total_var_size(req_struct->var_len_array,
                                          regTabPtr->no_var_attr);
    ndbassert(req_struct->var_data_end >= &var_data_part[vsize_words]);
    checksum = computeXorChecksum(var_data_part,vsize_words,checksum);
  }
#endif
  return checksum;
}

int
Dbtup::corruptedTupleDetected(KeyReqStruct *req_struct, Tablerec *regTabPtr)
{
  Uint32 checksum = calculateChecksum(req_struct->m_tuple_ptr, regTabPtr);
  Uint32 header_bits = req_struct->m_tuple_ptr->m_header_bits;
  Uint32 tableId = req_struct->fragPtrP->fragTableId;
  Uint32 fragId = req_struct->fragPtrP->fragmentId;
  Uint32 page_id = prepare_frag_page_id;
  Uint32 page_idx = prepare_page_idx;

  ndbout_c("Tuple corruption detected, checksum: 0x%x, header_bits: 0x%x"
           ", checksum word: 0x%x"
           ", tab(%u,%u), page(%u,%u)",
           checksum,
           header_bits,
           req_struct->m_tuple_ptr->m_checksum,
           tableId,
           fragId,
           page_id,
           page_idx);
  if (c_crashOnCorruptedTuple && !ERROR_INSERTED(4036))
  {
    ndbout_c(" Exiting."); 
    ndbrequire(false);
  }
  (void)ERROR_INSERTED_CLEAR(4036);
  terrorCode= ZTUPLE_CORRUPTED_ERROR;
  tupkeyErrorLab(req_struct);
  return -1;
}

/* ----------------------------------------------------------------- */
/* -----------       INSERT_ACTIVE_OP_LIST            -------------- */
/* ----------------------------------------------------------------- */
bool 
Dbtup::insertActiveOpList(OperationrecPtr regOperPtr,
			  KeyReqStruct* req_struct)
{
  OperationrecPtr prevOpPtr;
  ndbrequire(!regOperPtr.p->op_struct.bit_field.in_active_list);
  regOperPtr.p->op_struct.bit_field.in_active_list= true;
  req_struct->prevOpPtr.i= 
    prevOpPtr.i= req_struct->m_tuple_ptr->m_operation_ptr_i;
  regOperPtr.p->prevActiveOp= prevOpPtr.i;
  regOperPtr.p->nextActiveOp= RNIL;
  regOperPtr.p->m_undo_buffer_space= 0;
  req_struct->m_tuple_ptr->m_operation_ptr_i= regOperPtr.i;
  if (prevOpPtr.i == RNIL) {
    return true;
  } else {
    jam();
    req_struct->prevOpPtr.p= prevOpPtr.p= c_operation_pool.getPtr(prevOpPtr.i);
    prevOpPtr.p->nextActiveOp= regOperPtr.i;

    regOperPtr.p->op_struct.bit_field.m_wait_log_buffer= 
      prevOpPtr.p->op_struct.bit_field.m_wait_log_buffer;
    regOperPtr.p->op_struct.bit_field.m_load_diskpage_on_commit= 
      prevOpPtr.p->op_struct.bit_field.m_load_diskpage_on_commit;
    regOperPtr.p->op_struct.bit_field.m_gci_written=
      prevOpPtr.p->op_struct.bit_field.m_gci_written;
    regOperPtr.p->op_struct.bit_field.m_tuple_existed_at_start=
      prevOpPtr.p->op_struct.bit_field.m_tuple_existed_at_start;
    regOperPtr.p->m_undo_buffer_space= prevOpPtr.p->m_undo_buffer_space;
    // start with prev mask (matters only for UPD o UPD)

    regOperPtr.p->m_any_value = prevOpPtr.p->m_any_value;

    prevOpPtr.p->op_struct.bit_field.m_wait_log_buffer= 0;
    prevOpPtr.p->op_struct.bit_field.m_load_diskpage_on_commit= 0;

    if(prevOpPtr.p->tuple_state == TUPLE_PREPARED)
    {
      Uint32 op= regOperPtr.p->op_type;
      Uint32 prevOp= prevOpPtr.p->op_type;
      if (prevOp == ZDELETE)
      {
	if(op == ZINSERT)
	{
	  // mark both
	  prevOpPtr.p->op_struct.bit_field.delete_insert_flag= true;
	  regOperPtr.p->op_struct.bit_field.delete_insert_flag= true;
	  return true;
	}
        else if (op == ZREFRESH)
        {
          /* ZREFRESH after Delete - ok */
          return true;
        }
        else
        {
	  terrorCode= ZTUPLE_DELETED_ERROR;
	  return false;
	}
      } 
      else if(op == ZINSERT && prevOp != ZDELETE)
      {
	terrorCode= ZINSERT_ERROR;
	return false;
      }
      else if (prevOp == ZREFRESH)
      {
        /* No operation after a ZREFRESH */
        terrorCode= ZOP_AFTER_REFRESH_ERROR;
        return false;
      }
      return true;
    }
    else
    {
      terrorCode= ZMUST_BE_ABORTED_ERROR;
      return false;
    }
  }
}

bool
Dbtup::setup_read(KeyReqStruct *req_struct,
		  Operationrec* regOperPtr,
		  Tablerec* regTabPtr,
		  bool disk)
{
  OperationrecPtr currOpPtr;
  currOpPtr.i= req_struct->m_tuple_ptr->m_operation_ptr_i;
  const Uint32 bits = req_struct->m_tuple_ptr->m_header_bits;

  if (unlikely(req_struct->m_reorg != ScanFragReq::REORG_ALL))
  {
    const Uint32 moved = bits & Tuple_header::REORG_MOVE;
    if (! ((req_struct->m_reorg == ScanFragReq::REORG_NOT_MOVED && moved == 0) ||
           (req_struct->m_reorg == ScanFragReq::REORG_MOVED && moved != 0)))
    {
      /**
       * We're either scanning to only find moved rows (used when scanning
       * for rows to delete in reorg delete phase or we're scanning for
       * only non-moved rows and this happens also in reorg delete phase,
       * but it is done for normal scans in this phase.
       */
      terrorCode= ZTUPLE_DELETED_ERROR;
      return false;
    }
  }
  if (unlikely(bits & Tuple_header::FREE))
  {
    /**
     * The tuple could be FREE'ed due to an INSERT operation which aborted
     * while we waited for ACC to grant us access to the tuple.
     */
    terrorCode= ZTUPLE_DELETED_ERROR;
    return false;
  }
  if (likely(currOpPtr.i == RNIL))
  {
    jamDebug();
    if (regTabPtr->need_expand(disk))
    {
      jamDebug();
      prepare_read(req_struct, regTabPtr, disk);
    }
    return true;
  }

  do {
    Uint32 savepointId= regOperPtr->savepointId;
    bool dirty= req_struct->dirty_op;
    
    c_operation_pool.getPtr(currOpPtr);
    const bool sameTrans= c_lqh->is_same_trans(currOpPtr.p->userpointer,
                                               req_struct->trans_id1,
                                               req_struct->trans_id2);
    /**
     * Read committed in same trans reads latest copy
     */
    if(dirty && !sameTrans)
    {
      savepointId= 0;
    }
    else if(sameTrans)
    {
      // Use savepoint even in read committed mode
      dirty= false;
    }

    /* found == true indicates that savepoint is some state
     * within tuple's current transaction's uncommitted operations
     */
    const bool found= find_savepoint(currOpPtr, savepointId);
    
    const Uint32 currOp= currOpPtr.p->op_type;
    
    /* is_insert==true if tuple did not exist before its current
     * transaction
     */
    const bool is_insert = (bits & Tuple_header::ALLOC);

    /* If savepoint is in transaction, and post-delete-op
     *   OR
     * Tuple didn't exist before
     *      AND
     *   Read is dirty
     *           OR
     *   Savepoint is before-transaction
     *
     * Tuple does not exist in read's view
     */
    if((found && currOp == ZDELETE) || 
       ((dirty || !found) && is_insert))
    {
      /* Tuple not visible to this read operation */
      terrorCode= ZTUPLE_DELETED_ERROR;
      break;
    }
    
    if(dirty || !found)
    {
      /* Read existing committed tuple */
    }
    else
    {
      req_struct->m_tuple_ptr=
        get_copy_tuple(&currOpPtr.p->m_copy_tuple_location);
    }

    if (regTabPtr->need_expand(disk))
    {
      prepare_read(req_struct, regTabPtr, disk);
    }
    
#if 0
    ndbout_c("reading copy");
    Uint32 *var_ptr = fixed_ptr+regTabPtr->var_offset;
    req_struct->m_tuple_ptr= fixed_ptr;
    req_struct->fix_var_together= true;  
    req_struct->var_len_array= (Uint16*)var_ptr;
    req_struct->var_data_start= var_ptr+regTabPtr->var_array_wsize;
    Uint32 var_sz32= init_var_pos_array((Uint16*)var_ptr,
					req_struct->var_pos_array,
					regTabPtr->no_var_attr);
    req_struct->var_data_end= var_ptr+regTabPtr->var_array_wsize + var_sz32;
#endif
    return true;
  } while(0);
  
  return false;
}

int
Dbtup::load_diskpage(Signal* signal,
		     Uint32 opRec, Uint32 fragPtrI,
		     Uint32 lkey1, Uint32 lkey2, Uint32 flags)
{
  Ptr<Operationrec> operPtr;

  c_operation_pool.getPtr(operPtr, opRec);

  Operationrec *  regOperPtr= operPtr.p;
  Fragrecord * regFragPtr= prepare_fragptr.p;
  Tablerec* regTabPtr = prepare_tabptr.p;

  if (Local_key::isInvalid(lkey1, lkey2))
  {
    jam();
    regOperPtr->op_struct.bit_field.m_wait_log_buffer= 1;
    regOperPtr->op_struct.bit_field.m_load_diskpage_on_commit= 1;
    if (unlikely((flags & 7) == ZREFRESH))
    {
      jam();
      /* Refresh of previously nonexistant DD tuple.
       * No diskpage to load at commit time
       */
      regOperPtr->op_struct.bit_field.m_wait_log_buffer= 0;
      regOperPtr->op_struct.bit_field.m_load_diskpage_on_commit= 0;
    }

    /* In either case return 1 for 'proceed' */
    return 1;
  }
  
  jam();
  ndbassert(Uint16(lkey2) == lkey2);
  Uint16 page_idx= Uint16(lkey2);
  Uint32 frag_page_id= lkey1;
  regOperPtr->m_tuple_location.m_page_no= getRealpid(regFragPtr,
						     frag_page_id);
  regOperPtr->m_tuple_location.m_page_idx= page_idx;
  
  PagePtr page_ptr;
  Uint32* tmp= get_ptr(&page_ptr, &regOperPtr->m_tuple_location, regTabPtr);
  Tuple_header* ptr= (Tuple_header*)tmp;
  
  int res= 1;
  if(ptr->m_header_bits & Tuple_header::DISK_PART)
  {
    jam();
    Page_cache_client::Request req;
    memcpy(&req.m_page, ptr->get_disk_ref_ptr(regTabPtr), sizeof(Local_key));
    req.m_table_id = regFragPtr->fragTableId;
    req.m_fragment_id = regFragPtr->fragmentId;
    req.m_callback.m_callbackData= opRec;
    req.m_callback.m_callbackFunction= 
      safe_cast(&Dbtup::disk_page_load_callback);

#ifdef ERROR_INSERT
    if (ERROR_INSERTED(4022))
    {
      flags |= Page_cache_client::DELAY_REQ;
      const NDB_TICKS now = NdbTick_getCurrentTicks();
      req.m_delay_until_time = NdbTick_AddMilliseconds(now,(Uint64)3000);
    }
    if (ERROR_INSERTED(4035) && (rand() % 13) == 0)
    {
      // Disk access have to randomly wait max 16ms for a diskpage
      Uint64 delay = (Uint64)(rand() % 16) + 1;
      flags |= Page_cache_client::DELAY_REQ;
      const NDB_TICKS now = NdbTick_getCurrentTicks();
      req.m_delay_until_time = NdbTick_AddMilliseconds(now,delay);
    }
#endif
    
    Page_cache_client pgman(this, c_pgman);
    res= pgman.get_page(signal, req, flags);
  }

  switch(flags & 7)
  {
  case ZREAD:
  case ZREAD_EX:
    break;
  case ZDELETE:
  case ZUPDATE:
  case ZINSERT:
  case ZWRITE:
  case ZREFRESH:
    jam();
    regOperPtr->op_struct.bit_field.m_wait_log_buffer= 1;
    regOperPtr->op_struct.bit_field.m_load_diskpage_on_commit= 1;
  }
  return res;
}

void
Dbtup::disk_page_load_callback(Signal* signal, Uint32 opRec, Uint32 page_id)
{
  Ptr<Operationrec> operPtr;
  c_operation_pool.getPtr(operPtr, opRec);
  c_lqh->acckeyconf_load_diskpage_callback(signal, 
					   operPtr.p->userpointer, page_id);
}

int
Dbtup::load_diskpage_scan(Signal* signal,
			  Uint32 opRec, Uint32 fragPtrI,
			  Uint32 lkey1, Uint32 lkey2, Uint32 tux_flag)
{
  Ptr<Operationrec> operPtr;

  c_operation_pool.getPtr(operPtr, opRec);

  Operationrec *  regOperPtr= operPtr.p;
  Fragrecord * regFragPtr= prepare_fragptr.p;
  Tablerec* regTabPtr = prepare_tabptr.p;

  jam();
  Uint32 page_idx= lkey2;
  if (likely(tux_flag))
  {
    jamDebug();
    regOperPtr->m_tuple_location.m_page_no = lkey1;
  }
  else
  {
    jamDebug();
    Uint32 frag_page_id= lkey1;
    regOperPtr->m_tuple_location.m_page_no= getRealpid(regFragPtr,
						     frag_page_id);
  }
  regOperPtr->m_tuple_location.m_page_idx= page_idx;
  regOperPtr->op_struct.bit_field.m_load_diskpage_on_commit= 0;
  
  PagePtr page_ptr;
  Uint32* tmp= get_ptr(&page_ptr, &regOperPtr->m_tuple_location, regTabPtr);
  Tuple_header* ptr= (Tuple_header*)tmp;
  
  int res= 1;
  if(ptr->m_header_bits & Tuple_header::DISK_PART)
  {
    jam();
    Page_cache_client::Request req;
    memcpy(&req.m_page, ptr->get_disk_ref_ptr(regTabPtr), sizeof(Local_key));
    req.m_table_id = regFragPtr->fragTableId;
    req.m_fragment_id = regFragPtr->fragmentId;
    req.m_callback.m_callbackData= opRec;
    req.m_callback.m_callbackFunction= 
      safe_cast(&Dbtup::disk_page_load_scan_callback);
    
    Page_cache_client pgman(this, c_pgman);
    res= pgman.get_page(signal, req, 0);
  }
  return res;
}

void
Dbtup::disk_page_load_scan_callback(Signal* signal, 
				    Uint32 opRec, Uint32 page_id)
{
  Ptr<Operationrec> operPtr;
  c_operation_pool.getPtr(operPtr, opRec);
  c_lqh->next_scanconf_load_diskpage_callback(signal, 
					      operPtr.p->userpointer, page_id);
}

/**
  This method is used to prepare for faster execution of TUPKEYREQ.
  It prepares the pointers to the fragment record, the table record,
  the page for the record and the tuple pointer to the record. In
  addition it also prefetches the cache lines of the fixed size part
  of the tuple.

  The calculations performed here have to be done when we arrive in
  execTUPKEYREQ, we perform them here to enable prefetching the
  cache lines of the fixed part of the tuple storage. In order to not
  do the same work twice we store the calculated information in
  block variables. Given that we can arrive in execTUPKEYREQ from
  multiple directions, we have added debug-code that verifies that we
  have passed through prepareTUPKEYREQ always before we reach
  execTUPKEYREQ.

  The access of the fixed size part of the tuple is an almost certain
  CPU cache miss and so performing this as early as possible will
  decrease the time for cache misses later in the process. Tests using
  Sysbench indicates that this prefetch gains about 5% in performance.

  See DblqhMain.cpp for more documentation of prepare_* methods.
*/

void Dbtup::prepare_tab_pointers(Uint32 frag_id)
{
  /**
   * A real-time break occurred in scanning, we setup the
   * fragment and table pointers in preparation for calls to
   * execTUPKEYREQ.
   */
  jamDebug();
  FragrecordPtr fragptr;
  TablerecPtr tabptr;

  fragptr.i = frag_id;
  const Uint32 RnoOfFragrec= cnoOfFragrec;
  const Uint32 RnoOfTablerec= cnoOfTablerec;
  Fragrecord * Rfragrecord = fragrecord;
  Tablerec * Rtablerec = tablerec;
  ndbrequire(fragptr.i < RnoOfFragrec);
  ptrAss(fragptr, Rfragrecord);
  tabptr.i = fragptr.p->fragTableId;
  ndbrequire(tabptr.i < RnoOfTablerec);
  prepare_fragptr = fragptr;
  ptrAss(tabptr, Rtablerec);
  prepare_tabptr = tabptr;
}

void Dbtup::prepareTUPKEYREQ(Uint32 page_id,
                             Uint32 page_idx,
                             Uint32 frag_id)
{
  FragrecordPtr fragptr;
  TablerecPtr tabptr;

  fragptr.i = frag_id;
  const Uint32 RnoOfFragrec= cnoOfFragrec;
  const Uint32 RnoOfTablerec= cnoOfTablerec;
  Fragrecord * Rfragrecord = fragrecord;
  Tablerec * Rtablerec = tablerec;

  jamEntryDebug();
  ndbrequire(fragptr.i < RnoOfFragrec);
  ptrAss(fragptr, Rfragrecord);
  tabptr.i = fragptr.p->fragTableId;
  ptrCheckGuard(tabptr, RnoOfTablerec, Rtablerec);
  prepare_tabptr = tabptr;
  prepare_fragptr = fragptr;
  prepare_scanTUPKEYREQ(page_id, page_idx);
}

void Dbtup::prepare_scanTUPKEYREQ(Uint32 page_id, Uint32 page_idx)
{
  Local_key key;
  PagePtr pagePtr;
#ifdef VM_TRACE
  prepare_orig_local_key.m_page_no = page_id;
  prepare_orig_local_key.m_page_idx = page_idx;
#endif
  bool is_page_key = (!(Local_key::isInvalid(page_id, page_idx) ||
                        isCopyTuple(page_id, page_idx)));

  if (is_page_key)
  {
    register Uint32 fixed_part_size_in_words =
      prepare_tabptr.p->m_offsets[MM].m_fix_header_size;
    prepare_frag_page_id = page_id;
    page_id = getRealpid(prepare_fragptr.p, page_id);
    key.m_page_no = page_id;
    key.m_page_idx = page_idx;
    register Uint32 *tuple_ptr = get_ptr(&pagePtr,
                                         &key,
                                         prepare_tabptr.p);
    jamDebug();
    prepare_pageptr = pagePtr;
    prepare_page_idx = page_idx;
    prepare_tuple_ptr = tuple_ptr;
    prepare_page_no = page_id;
    for (Uint32 i = 0; i < fixed_part_size_in_words; i+= 16)
    {
      NDB_PREFETCH_WRITE(tuple_ptr + i);
    }
  }
}

void Dbtup::prepare_scan_tux_TUPKEYREQ(Uint32 page_id, Uint32 page_idx)
{
  Local_key key;
  PagePtr pagePtr;
#ifdef VM_TRACE
  prepare_orig_local_key.m_page_no = page_id;
  prepare_orig_local_key.m_page_idx = page_idx;
#endif
  bool is_page_key = (!(Local_key::isInvalid(page_id, page_idx) ||
                        isCopyTuple(page_id, page_idx)));

  ndbrequire(is_page_key);
  {
    register Uint32 fixed_part_size_in_words =
      prepare_tabptr.p->m_offsets[MM].m_fix_header_size;
    key.m_page_no = page_id;
    key.m_page_idx = page_idx;
    register Uint32 *tuple_ptr = get_ptr(&pagePtr,
                                         &key,
                                         prepare_tabptr.p);
    jamDebug();
    prepare_pageptr = pagePtr;
    prepare_tuple_ptr = tuple_ptr;
    prepare_page_no = page_id;
    for (Uint32 i = 0; i < fixed_part_size_in_words; i+= 16)
    {
      NDB_PREFETCH_WRITE(tuple_ptr + i);
    }
  }
}

void Dbtup::prepare_op_pointer(Uint32 opPtrI)
{
  jamDebug();
  Ptr<Operationrec> operPtr;
  c_operation_pool.getPtr(operPtr, opPtrI);
  Uint32 *op_ptr = (Uint32*)operPtr.p;
  NDB_PREFETCH_WRITE(op_ptr);
  NDB_PREFETCH_WRITE(op_ptr + 14);
  prepare_oper_ptr = operPtr;
}

bool Dbtup::execTUPKEYREQ(Signal* signal) 
{
   TupKeyReq * tupKeyReq= (TupKeyReq *)signal->getDataPtr();
   Ptr<Operationrec> operPtr = prepare_oper_ptr;
   KeyReqStruct req_struct(this);

   jamEntryDebug();

#ifdef VM_TRACE
   {
     bool error_found = false;
     Local_key key;
     key.m_page_no = tupKeyReq->keyRef1;
     key.m_page_idx = tupKeyReq->keyRef2;
     if (key.m_page_no != prepare_orig_local_key.m_page_no)
     {
       ndbout << "page_no = " << prepare_orig_local_key.m_page_no;
       ndbout << " keyRef1 = " << key.m_page_no << endl;
       error_found = true;
     }
     if (key.m_page_idx != prepare_orig_local_key.m_page_idx)
     {
       ndbout << "page_idx = " << prepare_orig_local_key.m_page_idx;
       ndbout << " keyRef2 = " << key.m_page_idx << endl;
       error_found = true;
     }
     if (error_found)
     {
       ndbout << flush;
     }
     ndbassert(prepare_orig_local_key.m_page_no == key.m_page_no);
     ndbassert(prepare_orig_local_key.m_page_idx == key.m_page_idx);
     ndbassert(prepare_fragptr.i == tupKeyReq->fragPtr);
     FragrecordPtr fragPtr = prepare_fragptr;
     ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
     ndbassert(prepare_fragptr.p == fragPtr.p);
   }
#endif

   /**
    * DESIGN PATTERN DESCRIPTION
    * --------------------------
    * The variable operPtr.p is located on the block object, it is located
    * there to ensure that we can easily access it in many methods such
    * that we don't have to transport it through method calls. There are
    * a number of references to structs that we store in this manner.
    * Oftentimes they refer to the operation object, the table object,
    * the fragment object and sometimes also a transaction object.
    *
    * Given that we both need access to the .i-value and the .p-value
    * of all of those objects we store them on the block object to
    * avoid the need of transporting them from function to function.
    * This is an optimisation and obviously requires that one keeps
    * track of which variables are alive and which are not.
    * The function clear_global_variables used in debug mode ensures
    * that all pointer variables are cleared before an asynchronous
    * signal is executed.
    *
    * When we need to access data through the .p-value many times
    * (more than one time), then it often pays off to declare a
    * stack variable such as below regOperPtr. This helps the compiler
    * to avoid having to constantly reload the .p-value from the
    * block object after each store operation through a pointer.
    *
    * One has to take care though when doing this to ensure that
    * one doesn't create a stack variable that creates too much
    * pressure on the register allocation in the method. This is
    * particularly important in large methods.
    *
    * The pattern is to define the variable as:
    * Operationrec * const regOperPtr = operPtr.p;
    * This helps the compiler to understand that we won't change the
    * pointer here.
    */
   Operationrec * const regOperPtr= operPtr.p;

   Dbtup::TransState trans_state = get_trans_state(regOperPtr);

   req_struct.signal= signal;
   req_struct.operPtrP = regOperPtr;
   regOperPtr->fragmentPtr = prepare_fragptr.i;
   req_struct.num_fired_triggers= 0;
   req_struct.no_exec_instructions = 0;
   req_struct.read_length= 0;
   req_struct.last_row= false;
   req_struct.m_is_lcp = false;

   if (unlikely(trans_state != TRANS_IDLE))
   {
     TUPKEY_abort(&req_struct, 39);
     return false;
   }

 /* ----------------------------------------------------------------- */
 // Operation is ZREAD when we arrive here so no need to worry about the
 // abort process.
 /* ----------------------------------------------------------------- */
 /* -----------    INITIATE THE OPERATION RECORD       -------------- */
 /* ----------------------------------------------------------------- */
   {
     register Operationrec::OpStruct op_struct;
     op_struct.op_bit_fields = regOperPtr->op_struct.op_bit_fields;
     const Uint32 TrequestInfo= tupKeyReq->request;
     const Uint32 disable_fk_checks = tupKeyReq->disable_fk_checks;
     const Uint32 deferred_constraints = tupKeyReq->deferred_constraints;
     const Uint32 triggers = tupKeyReq->triggers;

     regOperPtr->m_copy_tuple_location.setNull();
     op_struct.bit_field.delete_insert_flag = false;
     op_struct.bit_field.m_gci_written = 0;
     op_struct.bit_field.m_triggers = triggers;
     op_struct.bit_field.m_disable_fk_checks = disable_fk_checks;
     op_struct.bit_field.m_deferred_constraints = deferred_constraints;
     op_struct.bit_field.m_reorg = TupKeyReq::getReorgFlag(TrequestInfo);
     op_struct.bit_field.tupVersion= ZNIL;

     req_struct.m_prio_a_flag = TupKeyReq::getPrioAFlag(TrequestInfo);
     req_struct.m_reorg = TupKeyReq::getReorgFlag(TrequestInfo);
     regOperPtr->op_struct.op_bit_fields = op_struct.op_bit_fields;
     regOperPtr->op_type= TupKeyReq::getOperation(TrequestInfo);
     req_struct.m_disable_fk_checks = disable_fk_checks;
     req_struct.m_use_rowid = TupKeyReq::getRowidFlag(TrequestInfo);
     req_struct.m_nr_copy_or_redo = TupKeyReq::getNrCopyFlag(TrequestInfo);
     req_struct.interpreted_exec= TupKeyReq::getInterpretedFlag(TrequestInfo);
     req_struct.dirty_op= TupKeyReq::getDirtyFlag(TrequestInfo);
   }
   {
     /**
      * DESIGN PATTERN DESCRIPTION
      * --------------------------
      * This code segment is using a common design pattern in the
      * signal reception and signal sending code of performance
      * critical functions such as execTUPKEYREQ.
      * The idea is that at signal reception we need to transfer
      * data from the signal object to state variables relating to
      * the operation we are about to execute.
      * The normal manner to do this would be to write:
      * regOperPtr->savePointId = tupKeyReq->savePointId;
      * 
      * This normal manner would however not work so well due to
      * that the compiler has to issue assembler code that does
      * a load operation immediately followed by a store operation.
      * Many modern CPUs can hide parts of this deficiency in the
      * code, but only to a certain extent.
      *
      * What we want to do here is instead to perform a series of
      * six loads followed by six stores. The delay after a load
      * is ready for a store operation is oftentimes 3 cycles. Many
      * CPUs can handle two loads per cycle. So by using 6 loads
      * we ensure that we execute at full speed as long as the data
      * is available in the first level CPU cache.
      *
      * The reason we don't want to use more than 6 loads before
      * we start storing is that CPUs have a limited amount of
      * CPU registers. The x86 have 16 CPU registers available.
      * Here is a short description of commonly used registers:
      * RIP: Instruction pointer, not available
      * RSP: Top of Stack pointer, not available for L/S
      * RBP: Current Stack frame pointer, not available for L/S
      * RDI: Usually this-pointer, reference to Dbtup object here
      * 
      * In this particular example we also need to have a register
      * for storing:
      * tupKeyReq, req_struct, regOperPtr.
      *
      * The compiler also needs a few more registers to track some
      * of the other live variables such that not all of the live
      * variables have to be spilled to the stack.
      *
      * Thus the design pattern uses between 4 to 6 variables loaded
      * before storing them. Another commonly used manner is to locate
      * all initialisations to constants in one or more of those
      * initialisation code blocks as well.
      *
      * The naming pattern is to define the temporary variable as
      * const Uint32 name_of_variable_to_assign = x->name;
      * y->name_of_variable_to_assign = name_of_variable_to_assign.
      * 
      * In the case where the receiver of the data is a signal object
      * we use the pattern:
      * const Uint32 sig0 = x->name;
      * signal->theData[0] = sig0;
      *
      * Finally if possible we should place this initialisation in a
      * separate code block by surrounding it with brackets, this is
      * to assist the compiler to understand that the variables used
      * are not needed after storing its value. Most compilers will
      * handle this well anyways, but it helps the compiler avoid
      * doing mistakes and it also clarifies for the reader of the
      * source code. As can be seen in code below this rule is
      * however not followed if it will remove other possibilities.
      */
     const Uint32 savePointId= tupKeyReq->savePointId;
     const Uint32 attrBufLen = tupKeyReq->attrBufLen;
     const Uint32 opRef = tupKeyReq->opRef;
     const Uint32 tcOpIndex = tupKeyReq->tcOpIndex;
     const Uint32 coordinatorTC= tupKeyReq->coordinatorTC;
     const Uint32 applRef = tupKeyReq->applRef;

     regOperPtr->savepointId= savePointId;
     req_struct.log_size= attrBufLen;
     req_struct.attrinfo_len= attrBufLen;
     req_struct.tc_operation_ptr= opRef;
     req_struct.TC_index= tcOpIndex;
     req_struct.TC_ref= coordinatorTC;
     req_struct.rec_blockref= applRef;
   }

   const Uint32 disk_page= tupKeyReq->disk_page;
   const Uint32 row_id_page_no = tupKeyReq->m_row_id_page_no;
   const Uint32 row_id_page_idx = tupKeyReq->m_row_id_page_idx;
   const Uint32 deferred_constraints = tupKeyReq->deferred_constraints;
   const Uint32 keyRef1= tupKeyReq->keyRef1;
   const Uint32 keyRef2 = tupKeyReq->keyRef2;

   req_struct.m_disk_page_ptr.i= disk_page;
   req_struct.m_row_id.m_page_no = row_id_page_no;
   req_struct.m_row_id.m_page_idx = row_id_page_idx;
   req_struct.m_deferred_constraints = deferred_constraints;
   /**
    * The pageid here is a page id of a row id except when we are
    * reading from an ordered index scan, in this case it is a
    * physical page id. We will only use this variable for LCP
    * scan reads and for inserts and refreshs. So it is not used
    * for TUX scans.
    */
   Uint32 pageid = req_struct.frag_page_id= keyRef1;
   Uint32 pageidx = regOperPtr->m_tuple_location.m_page_idx= keyRef2;

   const Uint32 transId1 = tupKeyReq->transId1;
   const Uint32 transId2 = tupKeyReq->transId2;
   Tablerec * const regTabPtr = prepare_tabptr.p;

   /* Get AttrInfo section if this is a long TUPKEYREQ */
   Fragrecord *regFragPtr = prepare_fragptr.p;
   
   req_struct.trans_id1= transId1;
   req_struct.trans_id2= transId2;
   req_struct.tablePtrP = regTabPtr;
   req_struct.fragPtrP = regFragPtr;

   const Uint32 Roptype = regOperPtr->op_type;

   regOperPtr->m_any_value= 0;

   const Uint32 loc_prepare_page_id = prepare_page_no;
   /**
    * Check operation
    */
   if (likely(Roptype == ZREAD))
   {
     jamDebug();
     regOperPtr->op_struct.bit_field.m_tuple_existed_at_start = 0;
     ndbassert(!Local_key::isInvalid(pageid, pageidx));

     if (unlikely(isCopyTuple(pageid, pageidx)))
     {
       jamDebug();
       /**
        * Only LCP reads a copy-tuple "directly"
        */
       ndbassert(disk_page == RNIL);
       setup_lcp_read_copy_tuple(&req_struct, regOperPtr, regTabPtr);
     }
     else
     {
       /**
        * Get pointer to tuple
        */
       jamDebug();
       regOperPtr->m_tuple_location.m_page_no = loc_prepare_page_id;
       setup_fixed_tuple_ref_opt(&req_struct);
       setup_fixed_part(&req_struct, regOperPtr, regTabPtr);
       if (unlikely(setup_read(&req_struct, regOperPtr, regTabPtr, 
		               disk_page != RNIL) == false))
       {
         jam();
         tupkeyErrorLab(&req_struct);
         return false;
       }
     }
     if (handleReadReq(signal, regOperPtr, regTabPtr, &req_struct) != -1)
     {
       req_struct.log_size= 0;
       /* ---------------------------------------------------------------- */
       // Read Operations need not to be taken out of any lists. 
       // We also do not need to wait for commit since there is no changes 
       // to commit. Thus we
       // prepare the operation record already now for the next operation.
       // Write operations set the state to STARTED indicating that they
       // are waiting for the Commit or Abort decision.
       /* ---------------------------------------------------------------- */
       returnTUPKEYCONF(signal, &req_struct, regOperPtr, TRANS_IDLE);
       return true;
     }
     jamDebug();
     return false;
   }
   req_struct.changeMask.clear();

   if (!Local_key::isInvalid(pageid, pageidx))
   {
     regOperPtr->op_struct.bit_field.m_tuple_existed_at_start = 1;
   }
   else
   {
     regOperPtr->op_struct.bit_field.m_tuple_existed_at_start = 0;
     if (Roptype == ZINSERT)
     {
       // No tuple allocated yet
       goto do_insert;
     }
     if (Roptype == ZREFRESH)
     {
       // No tuple allocated yet
       goto do_refresh;
     }
   }
   ndbassert(!isCopyTuple(pageid, pageidx));
   /**
    * Get pointer to tuple
    */
   regOperPtr->m_tuple_location.m_page_no = loc_prepare_page_id;
   setup_fixed_tuple_ref_opt(&req_struct);
   setup_fixed_part(&req_struct, regOperPtr, regTabPtr);

   if(insertActiveOpList(operPtr, &req_struct))
   {
     if(Roptype == ZINSERT)
     {
       jam();
   do_insert:
       Local_key accminupdate;
       Local_key * accminupdateptr = &accminupdate;
       if (unlikely(handleInsertReq(signal, operPtr,
                                    prepare_fragptr, regTabPtr, &req_struct,
                                    &accminupdateptr) == -1))
       {
         return false;
       }

       terrorCode = 0;
       checkImmediateTriggersAfterInsert(&req_struct,
                                         regOperPtr,
                                         regTabPtr,
                                         disk_page != RNIL);

       if (likely(terrorCode == 0))
       {
         if (!regTabPtr->tuxCustomTriggers.isEmpty()) 
         {
           jam();
           
           executeTuxInsertTriggers(signal,
                                    regOperPtr,
                                    regFragPtr,
                                    regTabPtr);
         }
       }

       if (unlikely(terrorCode != 0))
       {
         jam();
         /*
          * TUP insert succeeded but immediate trigger firing or
          * add of TUX entries failed.  
          * All TUX changes have been rolled back at this point.
          *
          * We will abort via tupkeyErrorLab() as usual.  This routine
          * however resets the operation to ZREAD.  The TUP_ABORTREQ
          * arriving later cannot then undo the insert.
          *
          * Therefore we call TUP_ABORTREQ already now.  Diskdata etc
          * should be in memory and timeslicing cannot occur.  We must
          * skip TUX abort triggers since TUX is already aborted.  We
          * will dealloc the fixed and var parts if necessary.
          */
         signal->theData[0] = operPtr.i;
         do_tup_abortreq(signal, ZSKIP_TUX_TRIGGERS | ZABORT_DEALLOC);
         tupkeyErrorLab(&req_struct);
           return false;
       }

       if (accminupdateptr)
       {
         /**
          * Update ACC local-key, once *everything* has completed succesfully
          */
         c_lqh->accminupdate(signal,
                             regOperPtr->userpointer,
                             accminupdateptr);
       }

       returnTUPKEYCONF(signal, &req_struct, regOperPtr, TRANS_STARTED);
       return true;
     }

     ndbassert(!(req_struct.m_tuple_ptr->m_header_bits & Tuple_header::FREE));

     if (Roptype == ZUPDATE) {
       jamDebug();
       if (unlikely(handleUpdateReq(signal, regOperPtr,
                                    regFragPtr, regTabPtr,
                                    &req_struct, disk_page != RNIL) == -1))
       {
         return false;
       }

       terrorCode = 0;
       checkImmediateTriggersAfterUpdate(&req_struct,
                                         regOperPtr,
                                         regTabPtr,
                                         disk_page != RNIL);

       if (unlikely(terrorCode != 0))
       {
         tupkeyErrorLab(&req_struct);
         return false;
       }

       if (!regTabPtr->tuxCustomTriggers.isEmpty())
       {
         jam();
         if (unlikely(executeTuxUpdateTriggers(signal,
                                               regOperPtr,
                                               regFragPtr,
                                               regTabPtr) != 0))
         {
           jam();
           /*
            * See insert case.
            */
           signal->theData[0] = operPtr.i;
           do_tup_abortreq(signal, ZSKIP_TUX_TRIGGERS);
           tupkeyErrorLab(&req_struct);
           return false;
         }
       }

       returnTUPKEYCONF(signal, &req_struct, regOperPtr, TRANS_STARTED);
       return true;
     } 
     else if(Roptype == ZDELETE)
     {
       jam();
       req_struct.log_size= 0;
       if (unlikely(handleDeleteReq(signal, regOperPtr,
                                    regFragPtr, regTabPtr,
                                    &req_struct,
                                    disk_page != RNIL) == -1))
       {
         return false;
       }

       terrorCode = 0;
       checkImmediateTriggersAfterDelete(&req_struct,
                                         regOperPtr,
                                         regTabPtr,
                                         disk_page != RNIL);

       if (unlikely(terrorCode != 0))
       {
         tupkeyErrorLab(&req_struct);
         return false;
       }

       /*
        * TUX doesn't need to check for triggers at delete since entries in
        * the index are kept until commit time.
        */

       returnTUPKEYCONF(signal, &req_struct, regOperPtr, TRANS_STARTED);
       return true;
     }
     else if (Roptype == ZREFRESH)
     {
       /**
        * No TUX or immediate triggers, just detached triggers
        */
   do_refresh:
       if (unlikely(handleRefreshReq(signal, operPtr,
                                     prepare_fragptr, regTabPtr,
                                     &req_struct, disk_page != RNIL) == -1))
       {
         return false;
       }

       returnTUPKEYCONF(signal, &req_struct, regOperPtr, TRANS_STARTED);
       return true;
     }
     else
     {
       ndbrequire(false); // Invalid op type
     }
   }

   tupkeyErrorLab(&req_struct);
   return false;
}

void
Dbtup::setup_fixed_part(KeyReqStruct* req_struct,
			Operationrec* regOperPtr,
			Tablerec* regTabPtr)
{
  ndbassert(regOperPtr->op_type == ZINSERT ||
            (! (req_struct->m_tuple_ptr->m_header_bits & Tuple_header::FREE)));
  Uint32 descr_start = regTabPtr->tabDescriptor;
  TableDescriptor *loc_tab_descriptor = tableDescriptor;
  Uint32 num_attr= regTabPtr->m_no_of_attributes;
  Uint32 TnoOfTabDescrRec = cnoOfTabDescrRec;
  Uint32 mm_check_offset = regTabPtr->get_check_offset(MM);
  Uint32 dd_check_offset = regTabPtr->get_check_offset(DD);
  TableDescriptor *tab_descr= &loc_tab_descriptor[descr_start];
  
  ndbrequire(descr_start + (num_attr << ZAD_LOG_SIZE) <= TnoOfTabDescrRec);
  req_struct->check_offset[MM] = mm_check_offset;
  req_struct->check_offset[DD] = dd_check_offset;
  req_struct->attr_descr= tab_descr;
  NDB_PREFETCH_READ((char*)tab_descr);
}

void
Dbtup::setup_lcp_read_copy_tuple(KeyReqStruct* req_struct,
                                 Operationrec* regOperPtr,
                                 Tablerec* regTabPtr)
{
  Local_key tmp;
  tmp.m_page_no = req_struct->frag_page_id;
  tmp.m_page_idx = regOperPtr->m_tuple_location.m_page_idx;
  clearCopyTuple(tmp.m_page_no, tmp.m_page_idx);

  Uint32 * copytuple = get_copy_tuple_raw(&tmp);
  Local_key rowid;
  memcpy(&rowid, copytuple+0, sizeof(Local_key));

  req_struct->frag_page_id = rowid.m_page_no;
  regOperPtr->m_tuple_location.m_page_idx = rowid.m_page_idx;

  Tuple_header * th = get_copy_tuple(copytuple);
  req_struct->m_page_ptr.setNull();
  req_struct->m_tuple_ptr = (Tuple_header*)th;
  th->m_operation_ptr_i = RNIL;
  ndbassert((th->m_header_bits & Tuple_header::COPY_TUPLE) != 0);

  Uint32 num_attr= regTabPtr->m_no_of_attributes;
  Uint32 descr_start= regTabPtr->tabDescriptor;
  TableDescriptor *tab_descr= &tableDescriptor[descr_start];
  ndbrequire(descr_start + (num_attr << ZAD_LOG_SIZE) <= cnoOfTabDescrRec);
  req_struct->attr_descr= tab_descr;

  bool disk = false;
  if (regTabPtr->need_expand(disk))
  {
    jam();
    prepare_read(req_struct, regTabPtr, disk);
  }
}

 /* ---------------------------------------------------------------- */
 /* ------------------------ CONFIRM REQUEST ----------------------- */
 /* ---------------------------------------------------------------- */
 inline
 void Dbtup::returnTUPKEYCONF(Signal* signal,
			    KeyReqStruct *req_struct,
			    Operationrec * regOperPtr,
                            TransState trans_state)
{
  /**
   * When we arrive here we have been executing read code and/or write
   * code to read/write the tuple. During this execution path we have
   * not accessed the regOperPtr object for a long time and we have
   * accessed lots of other data in the meantime. This prefetch was
   * shown useful by using the perf tool. So not an obvious prefetch.
   */
  NDB_PREFETCH_WRITE(regOperPtr);
  TupKeyConf * tupKeyConf= (TupKeyConf *)signal->getDataPtrSend();  
  
  Uint32 Rcreate_rowid = req_struct->m_use_rowid;
  Uint32 RuserPointer= regOperPtr->userpointer;
  Uint32 RnumFiredTriggers= req_struct->num_fired_triggers;
  const Uint32 RnoExecInstructions = req_struct->no_exec_instructions;
  Uint32 log_size= req_struct->log_size;
  Uint32 read_length= req_struct->read_length;
  Uint32 last_row= req_struct->last_row;
  
  tupKeyConf->userPtr= RuserPointer;
  tupKeyConf->readLength= read_length;
  tupKeyConf->writeLength= log_size;
  tupKeyConf->numFiredTriggers= RnumFiredTriggers;
  tupKeyConf->lastRow= last_row;
  tupKeyConf->rowid = Rcreate_rowid;
  tupKeyConf->noExecInstructions = RnoExecInstructions;
  set_tuple_state(regOperPtr, TUPLE_PREPARED);
  set_trans_state(regOperPtr, trans_state);
}


#define MAX_READ (MIN(sizeof(signal->theData), MAX_SEND_MESSAGE_BYTESIZE))

/* ---------------------------------------------------------------- */
/* ----------------------------- READ  ---------------------------- */
/* ---------------------------------------------------------------- */
int Dbtup::handleReadReq(Signal* signal,
                         Operationrec* regOperPtr,
                         Tablerec* regTabPtr,
                         KeyReqStruct* req_struct)
{
  Uint32 *dst;
  Uint32 dstLen, start_index;
  const BlockReference sendBref= req_struct->rec_blockref;
  if (unlikely(((regTabPtr->m_bits & Tablerec::TR_Checksum) &&
       (calculateChecksum(req_struct->m_tuple_ptr, regTabPtr) != 0)) ||
      ERROR_INSERTED(4036))) {
    jam();
    return corruptedTupleDetected(req_struct, regTabPtr);
  }

  const Uint32 node = refToNode(sendBref);
  if(node != 0 && node != getOwnNodeId()) {
    start_index= 25;
  } else {
    jamDebug();
    /**
     * execute direct
     */
    start_index= AttrInfo::HeaderLength;  //3;
  }
  dst= &signal->theData[start_index];
  dstLen= (MAX_READ / 4) - start_index;
  if (!req_struct->interpreted_exec)
  {
    jamDebug();
    int ret = readAttributes(req_struct,
			     &cinBuffer[0],
			     req_struct->attrinfo_len,
			     dst,
			     dstLen,
			     false);
    if (likely(ret >= 0)) {
/* ------------------------------------------------------------------------- */
// We have read all data into coutBuffer. Now send it to the API.
/* ------------------------------------------------------------------------- */
      jamDebug();
      const Uint32 TnoOfDataRead= (Uint32) ret;
      sendReadAttrinfo(signal, req_struct, TnoOfDataRead);
      return 0;
    }
    else
    {
      terrorCode = Uint32(-ret);
    }
  }
  else
  {
    return interpreterStartLab(signal, req_struct);
  }

  jam();
  tupkeyErrorLab(req_struct);
  return -1;
}

static
void
handle_reorg(Dbtup::KeyReqStruct * req_struct,
             Dbtup::Fragrecord::FragState state)
{
  Uint32 reorg = req_struct->m_reorg;
  switch(state){
  case Dbtup::Fragrecord::FS_FREE:
  case Dbtup::Fragrecord::FS_REORG_NEW:
  case Dbtup::Fragrecord::FS_REORG_COMMIT_NEW:
  case Dbtup::Fragrecord::FS_REORG_COMPLETE_NEW:
    return;
  case Dbtup::Fragrecord::FS_REORG_COMMIT:
  case Dbtup::Fragrecord::FS_REORG_COMPLETE:
    if (reorg != ScanFragReq::REORG_NOT_MOVED)
      return;
    break;
  case Dbtup::Fragrecord::FS_ONLINE:
    if (reorg != ScanFragReq::REORG_MOVED)
      return;
    break;
  default:
    return;
  }
  req_struct->m_tuple_ptr->m_header_bits |= Dbtup::Tuple_header::REORG_MOVE;
}

/* ---------------------------------------------------------------- */
/* ---------------------------- UPDATE ---------------------------- */
/* ---------------------------------------------------------------- */
int Dbtup::handleUpdateReq(Signal* signal,
                           Operationrec* operPtrP,
                           Fragrecord* regFragPtr,
                           Tablerec* regTabPtr,
                           KeyReqStruct* req_struct,
			   bool disk) 
{
  Tuple_header *dst;
  Tuple_header *base= req_struct->m_tuple_ptr, *org;
  ChangeMask * change_mask_ptr;
  if ((dst= alloc_copy_tuple(regTabPtr, &operPtrP->m_copy_tuple_location))== 0)
  {
    terrorCode= ZNO_COPY_TUPLE_MEMORY_ERROR;
    goto error;
  }

  Uint32 tup_version;
  change_mask_ptr = get_change_mask_ptr(regTabPtr, dst);
  if(operPtrP->is_first_operation())
  {
    org= req_struct->m_tuple_ptr;
    tup_version= org->get_tuple_version();
    clear_change_mask_info(regTabPtr, change_mask_ptr);
  }
  else
  {
    jam();
    Operationrec* prevOp= req_struct->prevOpPtr.p;
    tup_version= prevOp->op_struct.bit_field.tupVersion;
    Uint32 * rawptr = get_copy_tuple_raw(&prevOp->m_copy_tuple_location);
    org= get_copy_tuple(rawptr);
    copy_change_mask_info(regTabPtr,
                          change_mask_ptr,
                          get_change_mask_ptr(rawptr));
  }

  /**
   * Check consistency before update/delete
   */
  req_struct->m_tuple_ptr= org;
  if ((regTabPtr->m_bits & Tablerec::TR_Checksum) &&
      (calculateChecksum(req_struct->m_tuple_ptr, regTabPtr) != 0)) 
  {
    jam();
    return corruptedTupleDetected(req_struct, regTabPtr);
  }

  req_struct->m_tuple_ptr= dst;

  union {
    Uint32 sizes[4];
    Uint64 cmp[2];
  };
  
  disk = disk || (org->m_header_bits & Tuple_header::DISK_INLINE);
  if (regTabPtr->need_expand(disk))
  {
    expand_tuple(req_struct, sizes, org, regTabPtr, disk);
    if(disk && operPtrP->m_undo_buffer_space == 0)
    {
      jam();
      operPtrP->op_struct.bit_field.m_wait_log_buffer = 1;
      operPtrP->op_struct.bit_field.m_load_diskpage_on_commit = 1;
      operPtrP->m_undo_buffer_space= 
	(sizeof(Dbtup::Disk_undo::Update) >> 2) + sizes[DD] - 1;

      {
        D("Logfile_client - handleUpdateReq");
        Logfile_client lgman(this, c_lgman, regFragPtr->m_logfile_group_id);
        DEB_LCP_LGMAN(("(%u)alloc_log_space(%u): %u",
                       instance(),
                       __LINE__,
                       operPtrP->m_undo_buffer_space));
        terrorCode= lgman.alloc_log_space(operPtrP->m_undo_buffer_space,
                                          true,
                                          !req_struct->m_nr_copy_or_redo,
                                          jamBuffer());
      }
      if(unlikely(terrorCode))
      {
        jam();
	operPtrP->m_undo_buffer_space= 0;
	goto error;
      }
    }
  }
  else
  {
    memcpy(dst, org, 4*regTabPtr->m_offsets[MM].m_fix_header_size);
    req_struct->m_tuple_ptr->m_header_bits |= Tuple_header::COPY_TUPLE;
  }
  
  tup_version= (tup_version + 1) & ZTUP_VERSION_MASK;
  operPtrP->op_struct.bit_field.tupVersion= tup_version;

  req_struct->optimize_options = 0;
  
  if (!req_struct->interpreted_exec)
  {
    jamDebug();

    if (regTabPtr->m_bits & Tablerec::TR_ExtraRowAuthorBits)
    {
      jam();
      Uint32 attrId =
        regTabPtr->getExtraAttrId<Tablerec::TR_ExtraRowAuthorBits>();

      store_extra_row_bits(attrId, regTabPtr, dst, /* default */ 0, false);
    }
    int retValue = updateAttributes(req_struct,
				    &cinBuffer[0],
				    req_struct->attrinfo_len);
    if (unlikely(retValue < 0))
    {
      terrorCode = Uint32(-retValue);
      goto error;
    }
  }
  else
  {
    if (unlikely(interpreterStartLab(signal, req_struct) == -1))
      return -1;
  }

  update_change_mask_info(regTabPtr,
                          change_mask_ptr,
                          req_struct->changeMask.rep.data);

  switch (req_struct->optimize_options) {
    case AttributeHeader::OPTIMIZE_MOVE_VARPART:
      /**
       * optimize varpart of tuple,  move varpart of tuple from
       * big-free-size page list into small-free-size page list
       */
      if(base->m_header_bits & Tuple_header::VAR_PART)
        optimize_var_part(req_struct, base, operPtrP,
                          regFragPtr, regTabPtr);
      break;
    case AttributeHeader::OPTIMIZE_MOVE_FIXPART:
      //TODO: move fix part of tuple
      break;
    default:
      break;
  }

  if (regTabPtr->need_shrink())
  {  
    shrink_tuple(req_struct, sizes+2, regTabPtr, disk);
    if (cmp[0] != cmp[1] && handle_size_change_after_update(req_struct,
							    base,
							    operPtrP,
							    regFragPtr,
							    regTabPtr,
							    sizes)) {
      goto error;
    }
  }

  if (req_struct->m_reorg != ScanFragReq::REORG_ALL)
  {
    handle_reorg(req_struct, regFragPtr->fragStatus);
  }
  
  req_struct->m_tuple_ptr->set_tuple_version(tup_version);

  setChecksum(req_struct->m_tuple_ptr, regTabPtr);

  set_tuple_state(operPtrP, TUPLE_PREPARED);

  return 0;
  
error:
  tupkeyErrorLab(req_struct);
  return -1;
}

/*
  expand_dyn_part - copy dynamic attributes to fully expanded size.

  Both variable-sized and fixed-size attributes are stored in the same way
  in the expanded form as variable-sized attributes (in expand_var_part()).

  This method is used for both mem and disk dynamic data.

    dst         Destination for expanded data
    tabPtrP     Table descriptor
    src         Pointer to the start of dynamic bitmap in source row
    row_len     Total number of 32-bit words in dynamic part of row
    tabDesc     Array of table descriptors
    order       Array of indexes into tabDesc, dynfix followed by dynvar
*/
static
Uint32*
expand_dyn_part(Dbtup::KeyReqStruct::Var_data *dst,
		const Uint32* src,
		Uint32 row_len,
		const Uint32 * tabDesc,
		const Uint16* order,
		Uint32 dynvar,
		Uint32 dynfix,
		Uint32 max_bmlen)
{
  /* Copy the bitmap, zeroing out any words not stored in the row. */
  Uint32 *dst_bm_ptr= (Uint32*)dst->m_dyn_data_ptr;
  Uint32 bm_len = row_len ? (* src & Dbtup::DYN_BM_LEN_MASK) : 0;
  
  assert(bm_len <= max_bmlen);

  if(bm_len > 0)
    memcpy(dst_bm_ptr, src, 4*bm_len);
  if(bm_len < max_bmlen)
    bzero(dst_bm_ptr + bm_len, 4 * (max_bmlen - bm_len));

  /**
   * Store max_bmlen for homogen code in DbtupRoutines
   */
  Uint32 tmp = (* dst_bm_ptr);
  * dst_bm_ptr = (tmp & ~(Uint32)Dbtup::DYN_BM_LEN_MASK) | max_bmlen;

  char *src_off_start= (char*)(src + bm_len);
  assert((UintPtr(src_off_start)&3) == 0);
  Uint16 *src_off_ptr= (Uint16*)src_off_start;

  /*
    Prepare the variable-sized dynamic attributes, copying out data from the
    source row for any that are not NULL.
  */
  Uint32 no_attr= dst->m_dyn_len_offset;
  Uint16* dst_off_ptr= dst->m_dyn_offset_arr_ptr;
  Uint16* dst_len_ptr= dst_off_ptr + no_attr;
  Uint16 this_src_off= row_len ? * src_off_ptr++ : 0;
  /* We need to reserve room for the offsets written by shrink_tuple+padding.*/
  Uint16 dst_off= 4 * (max_bmlen + ((dynvar+2)>>1));
  char *dst_ptr= (char*)dst_bm_ptr + dst_off;
  for(Uint32 i= 0; i<dynvar; i++)
  {
    Uint16 j= order[dynfix+i];
    Uint32 max_len= 4 *AttributeDescriptor::getSizeInWords(tabDesc[j]);
    Uint32 len;
    Uint32 pos = AttributeOffset::getNullFlagPos(tabDesc[j+1]);
    if(bm_len > (pos >> 5) && BitmaskImpl::get(bm_len, src, pos))
    {
      Uint16 next_src_off= *src_off_ptr++;
      len= next_src_off - this_src_off;
      memcpy(dst_ptr, src_off_start+this_src_off, len);
      this_src_off= next_src_off;
    }
    else
    {
      len= 0;
    }
    dst_off_ptr[i]= dst_off;
    dst_len_ptr[i]= dst_off+len;
    dst_off+= max_len;
    dst_ptr+= max_len;
  }
  /*
    The fixed-size data is stored 32-bit aligned after the variable-sized
    data.
  */
  char *src_ptr= src_off_start+this_src_off;
  src_ptr= (char *)(ALIGN_WORD(src_ptr));

  /*
    Prepare the fixed-size dynamic attributes, copying out data from the
    source row for any that are not NULL.
    Note that the fixed-size data is stored in reverse from the end of the
    dynamic part of the row. This is true both for the stored/shrunken and
    for the expanded form.
  */
  for(Uint32 i= dynfix; i>0; )
  {
    i--;
    Uint16 j= order[i];
    Uint32 fix_size= 4*AttributeDescriptor::getSizeInWords(tabDesc[j]);
    dst_off_ptr[dynvar+i]= dst_off;
    /* len offset array is not used for fixed size. */
    Uint32 pos = AttributeOffset::getNullFlagPos(tabDesc[j+1]);
    if(bm_len > (pos >> 5) && BitmaskImpl::get(bm_len, src, pos))
    {
      assert((UintPtr(dst_ptr)&3) == 0);
      memcpy(dst_ptr, src_ptr, fix_size);
      src_ptr+= fix_size;
    }
    dst_off+= fix_size;
    dst_ptr+= fix_size;
  }

  return (Uint32 *)dst_ptr;
}

static
Uint32*
shrink_dyn_part(Dbtup::KeyReqStruct::Var_data *dst,
                Uint32 *dst_ptr,
                const Dbtup::Tablerec* tabPtrP,
                const Uint32 * tabDesc,
                const Uint16* order,
                Uint32 dynvar,
                Uint32 dynfix,
                Uint32 ind)
{
  /**
   * Now build the dynamic part, if any.
   * First look for any trailing all-NULL words of the bitmap; we do
   * not need to store those.
   */
  assert((UintPtr(dst->m_dyn_data_ptr)&3) == 0);
  char *dyn_src_ptr= dst->m_dyn_data_ptr;
  Uint32 bm_len = tabPtrP->m_offsets[ind].m_dyn_null_words; // In words

  /* If no dynamic variables, store nothing. */
  assert(bm_len);
  {
    /**
     * clear bm-len bits, so they won't incorrect indicate
     *   a non-zero map
     */
    * ((Uint32 *)dyn_src_ptr) &= ~Uint32(Dbtup::DYN_BM_LEN_MASK);

    Uint32 *bm_ptr= (Uint32 *)dyn_src_ptr + bm_len - 1;
    while(*bm_ptr == 0)
    {
      bm_ptr--;
      bm_len--;
      if(bm_len == 0)
        break;
    }
  }

  if (bm_len)
  {
    /**
     * Copy the bitmap, counting the number of variable sized
     * attributes that are not NULL on the way.
     */
    Uint32 *dyn_dst_ptr= dst_ptr;
    Uint32 dyn_var_count= 0;
    const Uint32 *src_bm_ptr= (Uint32 *)(dyn_src_ptr);
    Uint32 *dst_bm_ptr= (Uint32 *)dyn_dst_ptr;

    /* ToDo: Put all of the dynattr code inside if(bm_len>0) { ... },
     * split to separate function. */
    Uint16 dyn_dst_data_offset= 0;
    const Uint32 *dyn_bm_var_mask_ptr= tabPtrP->dynVarSizeMask[ind];
    for(Uint16 i= 0; i< bm_len; i++)
    {
      Uint32 v= src_bm_ptr[i];
      dyn_var_count+= BitmaskImpl::count_bits(v & *dyn_bm_var_mask_ptr++);
      dst_bm_ptr[i]= v;
    }

    Uint32 tmp = *dyn_dst_ptr;
    assert(bm_len <= Dbtup::DYN_BM_LEN_MASK);
    * dyn_dst_ptr = (tmp & ~(Uint32)Dbtup::DYN_BM_LEN_MASK) | bm_len;
    dyn_dst_ptr+= bm_len;
    dyn_dst_data_offset= 2*dyn_var_count + 2;

    Uint16 *dyn_src_off_array= dst->m_dyn_offset_arr_ptr;
    Uint16 *dyn_src_lenoff_array=
      dyn_src_off_array + dst->m_dyn_len_offset;
    Uint16* dyn_dst_off_array = (Uint16*)dyn_dst_ptr;

    /**
     * Copy over the variable sized not-NULL attributes.
     * Data offsets are counted from the start of the offset array, and
     * we store one additional offset to be able to easily compute the
     * data length as the difference between offsets.
     */
    Uint16 off_idx= 0;
    for(Uint32 i= 0; i<dynvar; i++)
    {
      /**
       * Note that we must use the destination (shrunken) bitmap here,
       * as the source (expanded) bitmap may have been already clobbered
       * (by offset data).
       */
      Uint32 attrDesc2 = tabDesc[order[dynfix+i]+1];
      Uint32 pos = AttributeOffset::getNullFlagPos(attrDesc2);
      if (bm_len > (pos >> 5) && BitmaskImpl::get(bm_len, dst_bm_ptr, pos))
      {
        dyn_dst_off_array[off_idx++]= dyn_dst_data_offset;
        Uint32 dyn_src_off= dyn_src_off_array[i];
        Uint32 dyn_len= dyn_src_lenoff_array[i] - dyn_src_off;
        memmove(((char *)dyn_dst_ptr) + dyn_dst_data_offset,
                dyn_src_ptr + dyn_src_off,
                dyn_len);
        dyn_dst_data_offset+= dyn_len;
      }
    }
    /* If all dynamic attributes are NULL, we store nothing. */
    dyn_dst_off_array[off_idx]= dyn_dst_data_offset;
    assert(dyn_dst_off_array + off_idx == (Uint16*)dyn_dst_ptr+dyn_var_count);

    char *dynvar_end_ptr= ((char *)dyn_dst_ptr) + dyn_dst_data_offset;
    char *dyn_dst_data_ptr= (char *)(ALIGN_WORD(dynvar_end_ptr));

    /**
     * Zero out any padding bytes. Might not be strictly necessary,
     * but seems cleaner than leaving random stuff in there.
     */
    bzero(dynvar_end_ptr, dyn_dst_data_ptr-dynvar_end_ptr);

    /* *
     * Copy over the fixed-sized not-NULL attributes.
     * Note that attributes are copied in reverse order; this is to avoid
     * overwriting not-yet-copied data, as the data is also stored in
     * reverse order.
     */
    for(Uint32 i= dynfix; i > 0; )
    {
      i--;
      Uint16 j= order[i];
      Uint32 attrDesc2 = tabDesc[j+1];
      Uint32 pos = AttributeOffset::getNullFlagPos(attrDesc2);
      if(bm_len > (pos >>5 ) && BitmaskImpl::get(bm_len, dst_bm_ptr, pos))
      {
        Uint32 fixsize=
          4*AttributeDescriptor::getSizeInWords(tabDesc[j]);
        memmove(dyn_dst_data_ptr,
                dyn_src_ptr + dyn_src_off_array[dynvar+i],
                fixsize);
        dyn_dst_data_ptr += fixsize;
      }
    }
    dst_ptr = (Uint32*)dyn_dst_data_ptr;
    assert((UintPtr(dst_ptr) & 3) == 0);
  }
  return (Uint32 *)dst_ptr;
}

/* ---------------------------------------------------------------- */
/* ----------------------------- INSERT --------------------------- */
/* ---------------------------------------------------------------- */
void
Dbtup::prepare_initial_insert(KeyReqStruct *req_struct, 
			      Operationrec* regOperPtr,
			      Tablerec* regTabPtr)
{
  Uint32 disk_undo = regTabPtr->m_no_of_disk_attributes ? 
    sizeof(Dbtup::Disk_undo::Alloc) >> 2 : 0;
  regOperPtr->nextActiveOp= RNIL;
  regOperPtr->prevActiveOp= RNIL;
  regOperPtr->op_struct.bit_field.in_active_list= true;
  regOperPtr->m_undo_buffer_space= disk_undo; 
  
  req_struct->check_offset[MM]= regTabPtr->get_check_offset(MM);
  req_struct->check_offset[DD]= regTabPtr->get_check_offset(DD);
  
  Uint32 num_attr= regTabPtr->m_no_of_attributes;
  Uint32 descr_start= regTabPtr->tabDescriptor;
  Uint32 order_desc= regTabPtr->m_real_order_descriptor;
  TableDescriptor *tab_descr= &tableDescriptor[descr_start];
  ndbrequire(descr_start + (num_attr << ZAD_LOG_SIZE) <= cnoOfTabDescrRec);
  req_struct->attr_descr= tab_descr; 
  Uint16* order= (Uint16*)&tableDescriptor[order_desc];
  order += regTabPtr->m_attributes[MM].m_no_of_fixsize;

  Uint32 bits = Tuple_header::COPY_TUPLE;
  bits |= disk_undo ? (Tuple_header::DISK_ALLOC|Tuple_header::DISK_INLINE) : 0;

  const Uint32 mm_vars= regTabPtr->m_attributes[MM].m_no_of_varsize;
  const Uint32 mm_dyns= regTabPtr->m_attributes[MM].m_no_of_dynamic;
  const Uint32 mm_dynvar= regTabPtr->m_attributes[MM].m_no_of_dyn_var;
  const Uint32 mm_dynfix= regTabPtr->m_attributes[MM].m_no_of_dyn_fix;
  const Uint32 dd_vars= regTabPtr->m_attributes[DD].m_no_of_varsize;
  Uint32 *ptr= req_struct->m_tuple_ptr->get_end_of_fix_part_ptr(regTabPtr);
  Var_part_ref* ref = req_struct->m_tuple_ptr->get_var_part_ref_ptr(regTabPtr);

  if (regTabPtr->m_bits & Tablerec::TR_ForceVarPart)
  {
    ref->m_page_no = RNIL; 
    ref->m_page_idx = Tup_varsize_page::END_OF_FREE_LIST;
  }

  if(mm_vars || mm_dyns)
  {
    jam();
    /* Init Varpart_copy struct */
    Varpart_copy * cp = (Varpart_copy*)ptr;
    cp->m_len = 0;
    ptr += Varpart_copy::SZ32;

    /* Prepare empty varsize part. */
    KeyReqStruct::Var_data* dst= &req_struct->m_var_data[MM];
    
    if (mm_vars)
    {
      dst->m_data_ptr= (char*)(((Uint16*)ptr)+mm_vars+1);
      dst->m_offset_array_ptr= req_struct->var_pos_array;
      dst->m_var_len_offset= mm_vars;
      dst->m_max_var_offset= regTabPtr->m_offsets[MM].m_max_var_offset;
      
      Uint32 pos= 0;
      Uint16 *pos_ptr = req_struct->var_pos_array;
      Uint16 *len_ptr = pos_ptr + mm_vars;
      for(Uint32 i= 0; i<mm_vars; i++)
      {
        * pos_ptr++ = pos;
        * len_ptr++ = pos;
        pos += AttributeDescriptor::getSizeInBytes(tab_descr[*order++].tabDescr);
      }
      
      // Disk/dynamic part is 32-bit aligned
      ptr = ALIGN_WORD(dst->m_data_ptr+pos);
      ndbassert(ptr == ALIGN_WORD(dst->m_data_ptr + 
                                  regTabPtr->m_offsets[MM].m_max_var_offset));
    }

    if (mm_dyns)
    {
      jam();
      /* Prepare empty dynamic part. */
      dst->m_dyn_data_ptr= (char *)ptr;
      dst->m_dyn_offset_arr_ptr= req_struct->var_pos_array+2*mm_vars;
      dst->m_dyn_len_offset= mm_dynvar+mm_dynfix;
      dst->m_max_dyn_offset= regTabPtr->m_offsets[MM].m_max_dyn_offset;
      
      ptr = expand_dyn_part(dst, 0, 0,
                            (Uint32*)tab_descr, order,
                            mm_dynvar, mm_dynfix,
                            regTabPtr->m_offsets[MM].m_dyn_null_words);
    }
    
    ndbassert((UintPtr(ptr)&3) == 0);
  }

  req_struct->m_disk_ptr= (Tuple_header*)ptr;
  
  ndbrequire(dd_vars == 0);
  
  req_struct->m_tuple_ptr->m_header_bits= bits;
  /**
   * The copy tuple will be copied directly into the rowid position of
   * the tuple. Since we use the GCI in this position to see if a row
   * has changed we need to ensure that the GCI value is initialised,
   * otherwise we will not count inserts as a changed row.
   */
  *req_struct->m_tuple_ptr->get_mm_gci(regTabPtr) = 0;

  // Set all null bits
  memset(req_struct->m_tuple_ptr->m_null_bits+
	 regTabPtr->m_offsets[MM].m_null_offset, 0xFF, 
	 4*regTabPtr->m_offsets[MM].m_null_words);
  memset(req_struct->m_disk_ptr->m_null_bits+
	 regTabPtr->m_offsets[DD].m_null_offset, 0xFF, 
	 4*regTabPtr->m_offsets[DD].m_null_words);
}

int Dbtup::handleInsertReq(Signal* signal,
                           Ptr<Operationrec> regOperPtr,
                           Ptr<Fragrecord> fragPtr,
                           Tablerec* regTabPtr,
                           KeyReqStruct *req_struct,
                           Local_key ** accminupdateptr)
{
  Uint32 tup_version = 1;
  Fragrecord* regFragPtr = fragPtr.p;
  Uint32 *ptr= 0;
  Tuple_header *dst;
  Tuple_header *base= req_struct->m_tuple_ptr, *org= base;
  Tuple_header *tuple_ptr;
    
  bool disk = regTabPtr->m_no_of_disk_attributes > 0;
  bool mem_insert = regOperPtr.p->is_first_operation();
  bool disk_insert = mem_insert && disk;
  bool vardynsize = (regTabPtr->m_attributes[MM].m_no_of_varsize ||
                     regTabPtr->m_attributes[MM].m_no_of_dynamic);
  bool varalloc = vardynsize || regTabPtr->m_bits & Tablerec::TR_ForceVarPart;
  bool rowid = req_struct->m_use_rowid;
  bool update_acc = false; 
  Uint32 real_page_id = regOperPtr.p->m_tuple_location.m_page_no;
  Uint32 frag_page_id = req_struct->frag_page_id;

  union {
    Uint32 sizes[4];
    Uint64 cmp[2];
  };
  cmp[0] = cmp[1] = 0;

  if (ERROR_INSERTED(4014))
  {
    dst = 0;
    goto undo_buffer_error;
  }

  dst= alloc_copy_tuple(regTabPtr, &regOperPtr.p->m_copy_tuple_location);

  if (unlikely(dst == 0))
  {
    goto undo_buffer_error;
  }
  tuple_ptr= req_struct->m_tuple_ptr= dst;
  set_change_mask_info(regTabPtr, get_change_mask_ptr(regTabPtr, dst));

  if(mem_insert)
  {
    jam();
    prepare_initial_insert(req_struct, regOperPtr.p, regTabPtr);
  }
  else
  {
    Operationrec* prevOp= req_struct->prevOpPtr.p;
    ndbassert(prevOp->op_type == ZDELETE);
    tup_version= prevOp->op_struct.bit_field.tupVersion + 1;
    
    if(!prevOp->is_first_operation())
    {
      jam();
      org= get_copy_tuple(&prevOp->m_copy_tuple_location);
    }
    if (regTabPtr->need_expand())
    {
      expand_tuple(req_struct, sizes, org, regTabPtr, !disk_insert);
      memset(req_struct->m_disk_ptr->m_null_bits+
             regTabPtr->m_offsets[DD].m_null_offset, 0xFF, 
             4*regTabPtr->m_offsets[DD].m_null_words);

      Uint32 bm_size_in_bytes= 4*(regTabPtr->m_offsets[MM].m_dyn_null_words);
      if (bm_size_in_bytes)
      {
        Uint32* ptr = 
          (Uint32*)req_struct->m_var_data[MM].m_dyn_data_ptr;
        bzero(ptr, bm_size_in_bytes);
        * ptr = bm_size_in_bytes >> 2;
      }
    } 
    else
    {
      memcpy(dst, org, 4*regTabPtr->m_offsets[MM].m_fix_header_size);
      tuple_ptr->m_header_bits |= Tuple_header::COPY_TUPLE;
    }
    memset(tuple_ptr->m_null_bits+
           regTabPtr->m_offsets[MM].m_null_offset, 0xFF, 
           4*regTabPtr->m_offsets[MM].m_null_words);
  }
  
  int res;
  if (disk_insert)
  {
    jam();
    if (ERROR_INSERTED(4015))
    {
      terrorCode = 1501;
      goto log_space_error;
    }

    {
      D("Logfile_client - handleInsertReq");
      Logfile_client lgman(this, c_lgman, regFragPtr->m_logfile_group_id);
      DEB_LCP_LGMAN(("(%u)alloc_log_space(%u): %u",
                     instance(),
                     __LINE__,
                     regOperPtr.p->m_undo_buffer_space));
      res= lgman.alloc_log_space(regOperPtr.p->m_undo_buffer_space,
                                 true,
                                 !req_struct->m_nr_copy_or_redo,
                                 jamBuffer());
    }
    if(unlikely(res))
    {
      jam();
      terrorCode= res;
      goto log_space_error;
    }
  }
  
  regOperPtr.p->op_struct.bit_field.tupVersion=
    tup_version & ZTUP_VERSION_MASK;
  tuple_ptr->set_tuple_version(tup_version);

  if (ERROR_INSERTED(4016))
  {
    terrorCode = ZAI_INCONSISTENCY_ERROR;
    goto update_error;
  }

  if (regTabPtr->m_bits & Tablerec::TR_ExtraRowAuthorBits)
  {
    Uint32 attrId =
      regTabPtr->getExtraAttrId<Tablerec::TR_ExtraRowAuthorBits>();

    store_extra_row_bits(attrId, regTabPtr, tuple_ptr, /* default */ 0, false);
  }
  
  if (!regTabPtr->m_default_value_location.isNull())
  {
    jam();
    Uint32 default_values_len;
    /* Get default values ptr + len for this table */
    Uint32* default_values = get_default_ptr(regTabPtr, default_values_len);
    ndbrequire(default_values_len != 0 && default_values != NULL);
    /*
     * Update default values into row first,
     * next update with data received from the client.
     */
    if(unlikely((res = updateAttributes(req_struct, default_values,
                                        default_values_len)) < 0))
    {
      jam();
      terrorCode = Uint32(-res);
      goto update_error;
    }
  }
  
  if(unlikely((res = updateAttributes(req_struct, &cinBuffer[0],
                                      req_struct->attrinfo_len)) < 0))
  {
    terrorCode = Uint32(-res);
    goto update_error;
  }

  if (ERROR_INSERTED(4017))
  {
    goto null_check_error;
  }
  if (unlikely(checkNullAttributes(req_struct, regTabPtr) == false))
  {
    goto null_check_error;
  }
  
  if (req_struct->m_is_lcp)
  {
    jam();
    sizes[2+MM] = req_struct->m_lcp_varpart_len;
  }
  else if (regTabPtr->need_shrink())
  {  
    shrink_tuple(req_struct, sizes+2, regTabPtr, true);
  }

  if (ERROR_INSERTED(4025))
  {
    goto mem_error;
  }

  if (ERROR_INSERTED(4026))
  {
    CLEAR_ERROR_INSERT_VALUE;
    goto mem_error;
  }

  if (ERROR_INSERTED(4027) && (rand() % 100) > 25)
  {
    goto mem_error;
  }
 
  if (ERROR_INSERTED(4028) && (rand() % 100) > 25)
  {
    CLEAR_ERROR_INSERT_VALUE;
    goto mem_error;
  }
  
  /**
   * Alloc memory
   */
  if(mem_insert)
  {
    terrorCode = 0;
    if (!rowid)
    {
      if (ERROR_INSERTED(4018))
      {
	goto mem_error;
      }

      if (!varalloc)
      {
	jam();
	ptr= alloc_fix_rec(jamBuffer(),
                           &terrorCode,
                           regFragPtr,
			   regTabPtr,
			   &regOperPtr.p->m_tuple_location,
			   &frag_page_id);
      } 
      else 
      {
	jam();
	regOperPtr.p->m_tuple_location.m_file_no= sizes[2+MM];
	ptr= alloc_var_rec(&terrorCode,
                           regFragPtr, regTabPtr,
			   sizes[2+MM],
			   &regOperPtr.p->m_tuple_location,
			   &frag_page_id);
      }
      if (unlikely(ptr == 0))
      {
	goto mem_error;
      }
      req_struct->m_use_rowid = true;
    }
    else
    {
      regOperPtr.p->m_tuple_location = req_struct->m_row_id;
      if (ERROR_INSERTED(4019))
      {
	terrorCode = ZROWID_ALLOCATED;
	goto alloc_rowid_error;
      }
      
      if (!varalloc)
      {
	jam();
	ptr= alloc_fix_rowid(&terrorCode,
                             regFragPtr,
			     regTabPtr,
			     &regOperPtr.p->m_tuple_location,
			     &frag_page_id);
      } 
      else 
      {
	jam();
	regOperPtr.p->m_tuple_location.m_file_no= sizes[2+MM];
	ptr= alloc_var_rowid(&terrorCode,
                             regFragPtr, regTabPtr,
			     sizes[2+MM],
			     &regOperPtr.p->m_tuple_location,
			     &frag_page_id);
      }
      if (unlikely(ptr == 0))
      {
	jam();
	goto alloc_rowid_error;
      }
    }
    real_page_id = regOperPtr.p->m_tuple_location.m_page_no;
    update_acc = true; /* Will be updated later once success is known */
    
    base = (Tuple_header*)ptr;
    base->m_operation_ptr_i= regOperPtr.i;

#ifdef DEBUG_DELETE
    char *insert_str;
    if (req_struct->m_is_lcp)
    {
      insert_str = (char*)"LCP_INSERT";
    }
    else
    {
      insert_str = (char*)"INSERT";
    }
    DEB_DELETE(("(%u)%s: tab(%u,%u) row(%u,%u)",
                instance(),
                insert_str,
                regFragPtr->fragTableId,
                regFragPtr->fragmentId,
                frag_page_id,
                regOperPtr.p->m_tuple_location.m_page_idx));
#endif

    /**
     * The LCP_SKIP and LCP_DELETE flags must be retained even when allocating
     * a new row since they record state for the rowid and not for the record
     * as such. So we need to know state of rowid in LCP scans.
     */
    Uint32 old_header_keep =
      base->m_header_bits &
       (Tuple_header::LCP_SKIP | Tuple_header::LCP_DELETE);
    base->m_header_bits= Tuple_header::ALLOC |
      (sizes[2+MM] > 0 ? Tuple_header::VAR_PART : 0) |
      old_header_keep;

    /**
     * No need to set checksum here, the tuple is allocated, but contains
     * no data, so if we attempt to read it in this state we even want the
     * checksum to be wrong since it is not allowed to read the tuple in
     * this state.
     */
  }
  else 
  {
    if (ERROR_INSERTED(4020))
    {
      goto size_change_error;
    }

    if (regTabPtr->need_shrink() && cmp[0] != cmp[1] &&
	unlikely(handle_size_change_after_update(req_struct,
                                                 base,
                                                 regOperPtr.p,
                                                 regFragPtr,
                                                 regTabPtr,
                                                 sizes) != 0))
    {
      goto size_change_error;
    }
    req_struct->m_use_rowid = false;
    Uint32 old_header = base->m_header_bits;
    base->m_header_bits &= ~(Uint32)Tuple_header::FREE;
    Uint32 new_header = base->m_header_bits;
    if (old_header != new_header)
    {
      jam();
      updateChecksum(base, regTabPtr, old_header, new_header);
    }
  }

  if (disk_insert)
  {
    jam();
    Local_key tmp;
    Uint32 size= regTabPtr->m_attributes[DD].m_no_of_varsize == 0 ? 
      1 : sizes[2+DD];
    
    if (ERROR_INSERTED(4021))
    {
      terrorCode = 1601;
      goto disk_prealloc_error;
    }

    int ret= disk_page_prealloc(signal, fragPtr, &tmp, size);
    if (unlikely(ret < 0))
    {
      jam();
      terrorCode = -ret;
      goto disk_prealloc_error;
    }
    
    regOperPtr.p->op_struct.bit_field.m_disk_preallocated= 1;
    tmp.m_page_idx= size;
    memcpy(tuple_ptr->get_disk_ref_ptr(regTabPtr), &tmp, sizeof(tmp));

    /**
     * Set ref from disk to mm
     */
    Local_key ref = regOperPtr.p->m_tuple_location;
    ref.m_page_no = frag_page_id;
    
    Tuple_header* disk_ptr= req_struct->m_disk_ptr;
    disk_ptr->set_base_record_ref(ref);
  }

  if (req_struct->m_reorg != ScanFragReq::REORG_ALL)
  {
    handle_reorg(req_struct, regFragPtr->fragStatus);
  }

  /* Have been successful with disk + mem, update ACC to point to
   * new record if necessary
   * Failures in disk alloc will skip this part
   */
  if (update_acc)
  {
    /* Acc stores the local key with the frag_page_id rather
     * than the real_page_id
     */
    ndbassert(regOperPtr.p->m_tuple_location.m_page_no == real_page_id);

    Local_key accKey = regOperPtr.p->m_tuple_location;
    accKey.m_page_no = frag_page_id;
    ** accminupdateptr = accKey;
  }
  else
  {
    * accminupdateptr = 0; // No accminupdate should be performed
  }

  setChecksum(req_struct->m_tuple_ptr, regTabPtr);

  set_tuple_state(regOperPtr.p, TUPLE_PREPARED);

  return 0;
  
size_change_error:
  jam();
  terrorCode = ZMEM_NOMEM_ERROR;
  goto exit_error;
  
undo_buffer_error:
  jam();
  terrorCode= ZNO_COPY_TUPLE_MEMORY_ERROR;
  regOperPtr.p->m_undo_buffer_space = 0;
  if (mem_insert)
    regOperPtr.p->m_tuple_location.setNull();
  regOperPtr.p->m_copy_tuple_location.setNull();
  tupkeyErrorLab(req_struct);
  return -1;
  
null_check_error:
  jam();
  terrorCode= ZNO_ILLEGAL_NULL_ATTR;
  goto update_error;

mem_error:
  jam();
  if (terrorCode == 0)
  {
    terrorCode= ZMEM_NOMEM_ERROR;
  }
  goto update_error;

log_space_error:
  jam();
  regOperPtr.p->m_undo_buffer_space = 0;
alloc_rowid_error:
  jam();
update_error:
  jam();
  if (mem_insert)
  {
    regOperPtr.p->op_struct.bit_field.in_active_list = false;
    regOperPtr.p->m_tuple_location.setNull();
  }
exit_error:
  if (!regOperPtr.p->m_tuple_location.isNull())
  {
    jam();
    /* Memory allocated, abort insert, releasing memory if appropriate */
    signal->theData[0] = regOperPtr.i;
    do_tup_abortreq(signal, ZSKIP_TUX_TRIGGERS | ZABORT_DEALLOC);
  }
  tupkeyErrorLab(req_struct);
  return -1;

disk_prealloc_error:
  jam();
  base->m_header_bits |= Tuple_header::FREE;
  setInvalidChecksum(base, regTabPtr);
  goto exit_error;
}

/* ---------------------------------------------------------------- */
/* ---------------------------- DELETE ---------------------------- */
/* ---------------------------------------------------------------- */
int Dbtup::handleDeleteReq(Signal* signal,
                           Operationrec* regOperPtr,
                           Fragrecord* regFragPtr,
                           Tablerec* regTabPtr,
                           KeyReqStruct *req_struct,
			   bool disk)
{
  Tuple_header* dst = alloc_copy_tuple(regTabPtr,
                                       &regOperPtr->m_copy_tuple_location);
  if (dst == 0) {
    terrorCode = ZNO_COPY_TUPLE_MEMORY_ERROR;
    goto error;
  }

  // delete must set but not increment tupVersion
  if (!regOperPtr->is_first_operation())
  {
    jam();
    Operationrec* prevOp= req_struct->prevOpPtr.p;
    regOperPtr->op_struct.bit_field.tupVersion=
      prevOp->op_struct.bit_field.tupVersion;
    // make copy since previous op is committed before this one
    const Tuple_header* org = get_copy_tuple(&prevOp->m_copy_tuple_location);
    Uint32 len = regTabPtr->total_rec_size -
      Uint32(((Uint32*)dst) -
             get_copy_tuple_raw(&regOperPtr->m_copy_tuple_location));
    memcpy(dst, org, 4 * len);
    req_struct->m_tuple_ptr = dst;
  }
  else
  {
    regOperPtr->op_struct.bit_field.tupVersion=
      req_struct->m_tuple_ptr->get_tuple_version();
    if (regTabPtr->m_no_of_disk_attributes)
    {
      dst->m_header_bits = req_struct->m_tuple_ptr->m_header_bits;
      memcpy(dst->get_disk_ref_ptr(regTabPtr),
	     req_struct->m_tuple_ptr->get_disk_ref_ptr(regTabPtr),
             sizeof(Local_key));
    }
  }
  req_struct->changeMask.set();
  set_change_mask_info(regTabPtr, get_change_mask_ptr(regTabPtr, dst));

  if(disk && regOperPtr->m_undo_buffer_space == 0)
  {
    jam();
    regOperPtr->op_struct.bit_field.m_wait_log_buffer = 1;
    regOperPtr->op_struct.bit_field.m_load_diskpage_on_commit = 1;
    regOperPtr->m_undo_buffer_space= 
      (sizeof(Dbtup::Disk_undo::Free) >> 2) + 
      regTabPtr->m_offsets[DD].m_fix_header_size - 1;

    {
      D("Logfile_client - handleDeleteReq");
      Logfile_client lgman(this, c_lgman, regFragPtr->m_logfile_group_id);
      DEB_LCP_LGMAN(("(%u)alloc_log_space(%u): %u",
                     instance(),
                     __LINE__,
                     regOperPtr->m_undo_buffer_space));
      terrorCode= lgman.alloc_log_space(regOperPtr->m_undo_buffer_space,
                                        true,
                                        !req_struct->m_nr_copy_or_redo,
                                        jamBuffer());
    }
    if(unlikely(terrorCode))
    {
      jam();
      regOperPtr->m_undo_buffer_space= 0;
      goto error;
    }
  }

  set_tuple_state(regOperPtr, TUPLE_PREPARED);

  if (req_struct->attrinfo_len == 0)
  {
    return 0;
  }

  if (regTabPtr->need_expand(disk))
  {
    prepare_read(req_struct, regTabPtr, disk);
  }
  
  {
    Uint32 RlogSize;
    int ret= handleReadReq(signal, regOperPtr, regTabPtr, req_struct);
    if (ret == 0 && (RlogSize= req_struct->log_size))
    {
      jam();
      sendLogAttrinfo(signal, req_struct, RlogSize, regOperPtr);
    }
    return ret;
  }

error:
  tupkeyErrorLab(req_struct);
  return -1;
}

int
Dbtup::handleRefreshReq(Signal* signal,
                        Ptr<Operationrec> regOperPtr,
                        Ptr<Fragrecord>  regFragPtr,
                        Tablerec* regTabPtr,
                        KeyReqStruct *req_struct,
                        bool disk)
{
  /* Here we setup the tuple so that a transition to its current
   * state can be observed by SUMA's detached triggers.
   *
   * If the tuple does not exist then we fabricate a tuple
   * so that it can appear to be 'deleted'.
   *   The fabricated tuple may have invalid NULL values etc.
   * If the tuple does exist then we fabricate a null-change
   * update to the tuple.
   *
   * The logic differs depending on whether there are already
   * other operations on the tuple in this transaction.
   * No other operations (including Refresh) are allowed after
   * a refresh.
   */
  Uint32 refresh_case;
  if (regOperPtr.p->is_first_operation())
  {
    jam();
    if (Local_key::isInvalid(req_struct->frag_page_id,
                             regOperPtr.p->m_tuple_location.m_page_idx))
    {
      jam();
      refresh_case = Operationrec::RF_SINGLE_NOT_EXIST;
      //ndbout_c("case 1");
      /**
       * This is refresh of non-existing tuple...
       *   i.e "delete", reuse initial insert
       */
       Local_key accminupdate;
       Local_key * accminupdateptr = &accminupdate;

       /**
        * We don't need ...in this scenario
        * - disk
        * - default values
        */
       Uint32 save_disk = regTabPtr->m_no_of_disk_attributes;
       Local_key save_defaults = regTabPtr->m_default_value_location;
       Bitmask<MAXNROFATTRIBUTESINWORDS> save_mask =
         regTabPtr->notNullAttributeMask;

       regTabPtr->m_no_of_disk_attributes = 0;
       regTabPtr->m_default_value_location.setNull();
       regOperPtr.p->op_type = ZINSERT;

       /**
        * Update notNullAttributeMask  to only include primary keys
        */
       regTabPtr->notNullAttributeMask.clear();
       const Uint32 * primarykeys =
         (Uint32*)&tableDescriptor[regTabPtr->readKeyArray].tabDescr;
       for (Uint32 i = 0; i<regTabPtr->noOfKeyAttr; i++)
         regTabPtr->notNullAttributeMask.set(primarykeys[i] >> 16);

       int res = handleInsertReq(signal, regOperPtr,
                                 regFragPtr, regTabPtr, req_struct,
                                 &accminupdateptr);

       regTabPtr->m_no_of_disk_attributes = save_disk;
       regTabPtr->m_default_value_location = save_defaults;
       regTabPtr->notNullAttributeMask = save_mask;

       if (unlikely(res == -1))
       {
         return -1;
       }

       regOperPtr.p->op_type = ZREFRESH;

       if (accminupdateptr)
       {
       /**
          * Update ACC local-key, once *everything* has completed succesfully
          */
         c_lqh->accminupdate(signal,
                             regOperPtr.p->userpointer,
                             accminupdateptr);
       }
    }
    else
    {
      refresh_case = Operationrec::RF_SINGLE_EXIST;
      //ndbout_c("case 2");
      jam();

      Tuple_header* origTuple = req_struct->m_tuple_ptr;
      Uint32 tup_version_save = origTuple->get_tuple_version();
      {
        /* Set new row version and update the tuple header */
        Uint32 old_header = origTuple->m_header_bits;
        Uint32 new_tup_version = decr_tup_version(tup_version_save);
        origTuple->set_tuple_version(new_tup_version);
        Uint32 new_header = origTuple->m_header_bits;
        updateChecksum(origTuple, regTabPtr, old_header, new_header);
      }
      int res = handleUpdateReq(signal, regOperPtr.p, regFragPtr.p,
                                regTabPtr, req_struct, disk);

      /* Now we must reset the original tuple header back
       * to the original version.
       * The copy tuple will have the correct version due to
       * the update incrementing it.
       * On commit, the tuple becomes the copy tuple.
       * On abort, the original tuple remains.  If we don't
       * reset it here, then aborts cause the version to
       * decrease
       *
       * We also need to recalculate checksum since we're changing the
       * row here.
       */
      {
        Uint32 old_header = origTuple->m_header_bits;
        origTuple->set_tuple_version(tup_version_save);
        Uint32 new_header = origTuple->m_header_bits;
        updateChecksum(origTuple, regTabPtr, old_header, new_header);
      }
      if (res == -1)
        return -1;
    }
  }
  else
  {
    /* Not first operation on tuple in transaction */
    jam();

    Uint32 tup_version_save =
      req_struct->prevOpPtr.p->op_struct.bit_field.tupVersion;
    Uint32 new_tup_version = decr_tup_version(tup_version_save);
    req_struct->prevOpPtr.p->op_struct.bit_field.tupVersion = new_tup_version;

    int res;
    if (req_struct->prevOpPtr.p->op_type == ZDELETE)
    {
      refresh_case = Operationrec::RF_MULTI_NOT_EXIST;
      //ndbout_c("case 3");

      jam();
      /**
       * We don't need ...in this scenario
       * - default values
       *
       * We keep disk attributes to avoid issues with 'insert'
       */
      Local_key save_defaults = regTabPtr->m_default_value_location;
      Bitmask<MAXNROFATTRIBUTESINWORDS> save_mask =
        regTabPtr->notNullAttributeMask;

      regTabPtr->m_default_value_location.setNull();
      regOperPtr.p->op_type = ZINSERT;

      /**
       * Update notNullAttributeMask  to only include primary keys
       */
      regTabPtr->notNullAttributeMask.clear();
      const Uint32 * primarykeys =
        (Uint32*)&tableDescriptor[regTabPtr->readKeyArray].tabDescr;
      for (Uint32 i = 0; i<regTabPtr->noOfKeyAttr; i++)
        regTabPtr->notNullAttributeMask.set(primarykeys[i] >> 16);

      /**
       * This is multi-update + DELETE + REFRESH
       */
      Local_key * accminupdateptr = 0;
      res = handleInsertReq(signal, regOperPtr,
                            regFragPtr, regTabPtr, req_struct,
                            &accminupdateptr);

      regTabPtr->m_default_value_location = save_defaults;
      regTabPtr->notNullAttributeMask = save_mask;

      if (unlikely(res == -1))
      {
        return -1;
      }

      regOperPtr.p->op_type = ZREFRESH;
    }
    else
    {
      jam();
      refresh_case = Operationrec::RF_MULTI_EXIST;
      //ndbout_c("case 4");
      /**
       * This is multi-update + INSERT/UPDATE + REFRESH
       */
      res = handleUpdateReq(signal, regOperPtr.p, regFragPtr.p,
                            regTabPtr, req_struct, disk);
    }
    req_struct->prevOpPtr.p->op_struct.bit_field.tupVersion = tup_version_save;
    if (res == -1)
      return -1;
  }

  /* Store the refresh scenario in the copy tuple location */
  // TODO : Verify this is never used as a copy tuple location!
  regOperPtr.p->m_copy_tuple_location.m_file_no = refresh_case;
  return 0;
}

bool
Dbtup::checkNullAttributes(KeyReqStruct * req_struct,
                           Tablerec* regTabPtr)
{
// Implement checking of updating all not null attributes in an insert here.
  Bitmask<MAXNROFATTRIBUTESINWORDS> attributeMask;  
  /* 
   * The idea here is maybe that changeMask is not-null attributes
   * and must contain notNullAttributeMask.  But:
   *
   * 1. changeMask has all bits set on insert
   * 2. not-null is checked in each UpdateFunction
   * 3. the code below does not work except trivially due to 1.
   *
   * XXX remove or fix
   */
  attributeMask.clear();
  attributeMask.bitOR(req_struct->changeMask);
  attributeMask.bitAND(regTabPtr->notNullAttributeMask);
  attributeMask.bitXOR(regTabPtr->notNullAttributeMask);
  if (!attributeMask.isclear()) {
    return false;
  }
  return true;
}

/* ---------------------------------------------------------------- */
/* THIS IS THE START OF THE INTERPRETED EXECUTION OF UPDATES. WE    */
/* START BY LINKING ALL ATTRINFO'S IN A DOUBLY LINKED LIST (THEY ARE*/
/* ALREADY IN A LINKED LIST). WE ALLOCATE A REGISTER MEMORY (EQUAL  */
/* TO AN ATTRINFO RECORD). THE INTERPRETER GOES THROUGH FOUR  PHASES*/
/* DURING THE FIRST PHASE IT IS ONLY ALLOWED TO READ ATTRIBUTES THAT*/
/* ARE SENT TO THE CLIENT APPLICATION. DURING THE SECOND PHASE IT IS*/
/* ALLOWED TO READ FROM ATTRIBUTES INTO REGISTERS, TO UPDATE        */
/* ATTRIBUTES BASED ON EITHER A CONSTANT VALUE OR A REGISTER VALUE, */
/* A DIVERSE SET OF OPERATIONS ON REGISTERS ARE AVAILABLE AS WELL.  */
/* IT IS ALSO POSSIBLE TO PERFORM JUMPS WITHIN THE INSTRUCTIONS THAT*/
/* BELONGS TO THE SECOND PHASE. ALSO SUBROUTINES CAN BE CALLED IN   */
/* THIS PHASE. THE THIRD PHASE IS TO AGAIN READ ATTRIBUTES AND      */
/* FINALLY THE FOURTH PHASE READS SELECTED REGISTERS AND SEND THEM  */
/* TO THE CLIENT APPLICATION.                                       */
/* THERE IS A FIFTH REGION WHICH CONTAINS SUBROUTINES CALLABLE FROM */
/* THE INTERPRETER EXECUTION REGION.                                */
/* THE FIRST FIVE WORDS WILL GIVE THE LENGTH OF THE FIVE REGIONS    */
/*                                                                  */
/* THIS MEANS THAT FROM THE APPLICATIONS POINT OF VIEW THE DATABASE */
/* CAN HANDLE SUBROUTINE CALLS WHERE THE CODE IS SENT IN THE REQUEST*/
/* THE RETURN PARAMETERS ARE FIXED AND CAN EITHER BE GENERATED      */
/* BEFORE THE EXECUTION OF THE ROUTINE OR AFTER.                    */
/*                                                                  */
/* IN LATER VERSIONS WE WILL ADD MORE THINGS LIKE THE POSSIBILITY   */
/* TO ALLOCATE MEMORY AND USE THIS AS LOCAL STORAGE. IT IS ALSO     */
/* IMAGINABLE TO HAVE SPECIAL ROUTINES THAT CAN PERFORM CERTAIN     */
/* OPERATIONS ON BLOB'S DEPENDENT ON WHAT THE BLOB REPRESENTS.      */
/*                                                                  */
/*                                                                  */
/*       -----------------------------------------                  */
/*       +   INITIAL READ REGION                 +                  */
/*       -----------------------------------------                  */
/*       +   INTERPRETED EXECUTE  REGION         +                  */
/*       -----------------------------------------                  */
/*       +   FINAL UPDATE REGION                 +                  */
/*       -----------------------------------------                  */
/*       +   FINAL READ REGION                   +                  */
/*       -----------------------------------------                  */
/*       +   SUBROUTINE REGION                   +                  */
/*       -----------------------------------------                  */
/*                                                                  */
/* For read operations it only makes sense to first perform the     */
/* interpreted execution (this will perform condition pushdown      */
/* where we evaluate the conditions that are not evaluated by       */
/* ranges implied by the scan operation. These conditions pushed    */
/* down can essentially check any type of condition.                */
/*                                                                  */
/* Since it only makes sense to interpret before reading we delay   */
/* the initial read to after interpreted execution for read         */
/* operations. This is safe from a protocol point of view since the */
/* interpreted execution cannot generate Attrinfo data.             */
/*                                                                  */
/* For updates it still makes sense to handle initial read and      */
/* final read separately since we might want to read values before  */
/* and after changes, the interpreter can write column values.      */
/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */
/* ----------------- INTERPRETED EXECUTION  ----------------------- */
/* ---------------------------------------------------------------- */
int Dbtup::interpreterStartLab(Signal* signal,
                               KeyReqStruct *req_struct)
{
  Operationrec * const regOperPtr = req_struct->operPtrP;
  int TnoDataRW;
  Uint32 RtotalLen, start_index, dstLen;
  Uint32 *dst;

  Uint32 RinitReadLen= cinBuffer[0];
  Uint32 RexecRegionLen= cinBuffer[1];
  Uint32 RfinalUpdateLen= cinBuffer[2];
  Uint32 RfinalRLen= cinBuffer[3];
  Uint32 RsubLen= cinBuffer[4];

  jamDebug();

  Uint32 RattrinbufLen= req_struct->attrinfo_len;
  const BlockReference sendBref= req_struct->rec_blockref;

  const Uint32 node = refToNode(sendBref);
  if(node != 0 && node != getOwnNodeId()) {
    start_index= 25;
  } else {
    jamDebug();
    /**
     * execute direct
     */
    start_index= TransIdAI::HeaderLength;  //3;
  }
  dst= &signal->theData[start_index];
  dstLen= (MAX_READ / 4) - start_index;
  
  RtotalLen= RinitReadLen;
  RtotalLen += RexecRegionLen;
  RtotalLen += RfinalUpdateLen;
  RtotalLen += RfinalRLen;
  RtotalLen += RsubLen;

  Uint32 RattroutCounter= 0;
  Uint32 RinstructionCounter= 5;

  /* All information to be logged/propagated to replicas
   * is generated from here on so reset the log word count
   */
  Uint32 RlogSize= req_struct->log_size= 0;
  if (likely(((RtotalLen + 5) == RattrinbufLen) &&
             (RattrinbufLen >= 5) &&
             (RattrinbufLen < ZATTR_BUFFER_SIZE)))
  {
    /* ---------------------------------------------------------------- */
    // We start by checking consistency. We must have the first five
    // words of the ATTRINFO to give us the length of the regions. The
    // size of these regions must be the same as the total ATTRINFO
    // length and finally the total length must be within the limits.
    /* ---------------------------------------------------------------- */

    if (likely(RinitReadLen > 0))
    {
      if (likely(regOperPtr->op_type == ZREAD))
      {
        jamDebug();
        RinstructionCounter += RinitReadLen;
      }
      else
      {
        jamDebug();
        /* ---------------------------------------------------------------- */
        // The first step that can be taken in the interpreter is to read
        // data of the tuple before any updates have been applied.
        /* ---------------------------------------------------------------- */
        TnoDataRW= readAttributes(req_struct,
                                  &cinBuffer[5],
                                  RinitReadLen,
                                  &dst[0],
                                  dstLen,
                                  false);
        if (TnoDataRW >= 0)
        {
          jamDebug();
          RattroutCounter= TnoDataRW;
          RinstructionCounter += RinitReadLen;
          RinitReadLen = 0;
        }
        else
        {
          jam();
          terrorCode = Uint32(-TnoDataRW);
          tupkeyErrorLab(req_struct);
          return -1;
        }
      }
    }
    if (RexecRegionLen > 0)
    {
      jamDebug();
      /* ---------------------------------------------------------------- */
      // The next step is the actual interpreted execution. This executes
      // a register-based virtual machine which can read and write attributes
      // to and from registers.
      /* ---------------------------------------------------------------- */
      Uint32 RsubPC= RinstructionCounter + RexecRegionLen 
        + RfinalUpdateLen + RfinalRLen;     
      TnoDataRW= interpreterNextLab(signal,
                                     req_struct,
                                     &clogMemBuffer[0],
                                     &cinBuffer[RinstructionCounter],
                                     RexecRegionLen,
                                     &cinBuffer[RsubPC],
                                     RsubLen,
                                     &coutBuffer[0],
                                     sizeof(coutBuffer) / 4);
      if (TnoDataRW != -1)
      {
        jamDebug();
        RinstructionCounter += RexecRegionLen;
        RlogSize= TnoDataRW;
      }
      else
      {
        jamDebug();
        /**
         * TUPKEY REF is sent from within interpreter
         */
        return -1;
      }
    }

    if ((RlogSize > 0) ||
        (RfinalUpdateLen > 0))
    {
      jamDebug();
      /* Operation updates row,
       * reset author pseudo-col before update takes effect
       * This should probably occur only if the interpreted program
       * did not explicitly write the value, but that requires a bit
       * to record whether the value has been written.
       */
      Tablerec* regTabPtr = req_struct->tablePtrP;
      Tuple_header* dst = req_struct->m_tuple_ptr;

      if (regTabPtr->m_bits & Tablerec::TR_ExtraRowAuthorBits)
      {
        Uint32 attrId =
          regTabPtr->getExtraAttrId<Tablerec::TR_ExtraRowAuthorBits>();

        store_extra_row_bits(attrId, regTabPtr, dst, /* default */ 0, false);
      }
    }

    if (unlikely(RfinalUpdateLen > 0))
    {
      /* ---------------------------------------------------------------- */
      // We can also apply a set of updates without any conditions as part
      // of the interpreted execution.
      /* ---------------------------------------------------------------- */
      if (regOperPtr->op_type == ZUPDATE)
      {
        jamDebug();
        TnoDataRW= updateAttributes(req_struct,
                                    &cinBuffer[RinstructionCounter],
                                    RfinalUpdateLen);
        if (TnoDataRW >= 0)
        {
          jamDebug();
          MEMCOPY_NO_WORDS(&clogMemBuffer[RlogSize],
                           &cinBuffer[RinstructionCounter],
                           RfinalUpdateLen);
          RinstructionCounter += RfinalUpdateLen;
          RlogSize += RfinalUpdateLen;
        }
        else
        {
          jam();
          terrorCode = Uint32(-TnoDataRW);
          tupkeyErrorLab(req_struct);
          return -1;
        }
      }
      else
      {
        jamDebug();
        return TUPKEY_abort(req_struct, 19);
      }
    }
    if (likely(RinitReadLen > 0))
    {
      jamDebug();
      TnoDataRW= readAttributes(req_struct,
                                &cinBuffer[5],
                                RinitReadLen,
                                &dst[0],
                                dstLen,
                                false);
      if (TnoDataRW >= 0)
      {
        jamDebug();
        RattroutCounter = TnoDataRW;
      }
      else
      {
        jam();
        terrorCode = Uint32(-TnoDataRW);
        tupkeyErrorLab(req_struct);
        return -1;
      }
    }
    if (RfinalRLen > 0)
    {
      jamDebug();
      /* ---------------------------------------------------------------- */
      // The final action is that we can also read the tuple after it has
      // been updated.
      /* ---------------------------------------------------------------- */
      TnoDataRW= readAttributes(req_struct,
                                &cinBuffer[RinstructionCounter],
                                RfinalRLen,
                                &dst[RattroutCounter],
                                (dstLen - RattroutCounter),
                                 false);
      if (TnoDataRW >= 0)
      {
        jamDebug();
        RattroutCounter += TnoDataRW;
      }
      else
      {
        jam();
        terrorCode = Uint32(-TnoDataRW);
        tupkeyErrorLab(req_struct);
        return -1;
      }
    }
    /* Add log words explicitly generated here to existing log size
     *  - readAttributes can generate log for ANYVALUE column
     *    It adds the words directly to req_struct->log_size
     *    This is used for ANYVALUE and interpreted delete.
     */
    req_struct->log_size+= RlogSize;
    sendReadAttrinfo(signal, req_struct, RattroutCounter);
    if (RlogSize > 0)
    {
      return sendLogAttrinfo(signal, req_struct, RlogSize, regOperPtr);
    }
    return 0;
  }
  else
  {
    return TUPKEY_abort(req_struct, 22);
  }
}

/* ---------------------------------------------------------------- */
/*       WHEN EXECUTION IS INTERPRETED WE NEED TO SEND SOME ATTRINFO*/
/*       BACK TO LQH FOR LOGGING AND SENDING TO BACKUP AND STANDBY  */
/*       NODES.                                                     */
/*       INPUT:  LOG_ATTRINFOPTR         WHERE TO FETCH DATA FROM   */
/*               TLOG_START              FIRST INDEX TO LOG         */
/*               TLOG_END                LAST INDEX + 1 TO LOG      */
/* ---------------------------------------------------------------- */
int Dbtup::sendLogAttrinfo(Signal* signal,
                           KeyReqStruct * req_struct,
                           Uint32 TlogSize,
                           Operationrec *  const regOperPtr)

{
  /* Copy from Log buffer to segmented section,
   * then attach to ATTRINFO and execute direct
   * to LQH
   */
  ndbrequire( TlogSize > 0 );
  Uint32 longSectionIVal= RNIL;
  bool ok= appendToSection(longSectionIVal, 
                           &clogMemBuffer[0],
                           TlogSize);
  if (unlikely(!ok))
  {
    /* Resource error, abort transaction */
    terrorCode = ZSEIZE_ATTRINBUFREC_ERROR;
    tupkeyErrorLab(req_struct);
    return -1;
  }
  
  /* Send a TUP_ATTRINFO signal to LQH, which contains
   * the relevant user pointer and the attrinfo section's
   * IVAL
   */
  signal->theData[0]= regOperPtr->userpointer;
  signal->theData[1]= TlogSize;
  signal->theData[2]= longSectionIVal;

  EXECUTE_DIRECT(DBLQH, 
                 GSN_TUP_ATTRINFO, 
                 signal, 
                 3);
  return 0;
}

inline
Uint32 
Dbtup::brancher(Uint32 TheInstruction, Uint32 TprogramCounter)
{         
  Uint32 TbranchDirection= TheInstruction >> 31;
  Uint32 TbranchLength= (TheInstruction >> 16) & 0x7fff;
  TprogramCounter--;
  if (TbranchDirection == 1)
  {
    jamDebug();
    /* ---------------------------------------------------------------- */
    /*       WE JUMP BACKWARDS.                                         */
    /* ---------------------------------------------------------------- */
    return (TprogramCounter - TbranchLength);
  }
  else
  {
    jamDebug();
    /* ---------------------------------------------------------------- */
    /*       WE JUMP FORWARD.                                           */
    /* ---------------------------------------------------------------- */
    return (TprogramCounter + TbranchLength);
  }
}

const Uint32 *
Dbtup::lookupInterpreterParameter(Uint32 paramNo,
                                  const Uint32 * subptr,
                                  Uint32 sublen) const
{
  /**
   * The parameters...are stored in the subroutine section
   *
   * WORD2         WORD3       WORD4         WORD5
   * [ P0 HEADER ] [ P0 DATA ] [ P1 HEADER ] [ P1 DATA ]
   *
   *
   * len=4 <=> 1 word
   */
  Uint32 pos = 0;
  while (paramNo)
  {
    const Uint32 * head = subptr + pos;
    Uint32 len = AttributeHeader::getDataSize(* head);
    paramNo --;
    pos += 1 + len;
    if (unlikely(pos >= sublen))
      return 0;
  }

  const Uint32 * head = subptr + pos;
  Uint32 len = AttributeHeader::getDataSize(* head);
  if (unlikely(pos + 1 + len > sublen))
    return 0;

  return head;
}

int Dbtup::interpreterNextLab(Signal* signal,
                              KeyReqStruct* req_struct,
                              Uint32* logMemory,
                              Uint32* mainProgram,
                              Uint32 TmainProgLen,
                              Uint32* subroutineProg,
                              Uint32 TsubroutineLen,
			      Uint32 * tmpArea,
			      Uint32 tmpAreaSz)
{
  register Uint32* TcurrentProgram= mainProgram;
  register Uint32 TcurrentSize= TmainProgLen;
  register Uint32 TprogramCounter= 0;
  register Uint32 theInstruction;
  register Uint32 theRegister;
  Uint32 TdataWritten= 0;
  Uint32 RstackPtr= 0;
  union {
    Uint32 TregMemBuffer[32];
    Uint64 align[16];
  };
  (void)align; // kill warning
  Uint32 TstackMemBuffer[32];

  Uint32& RnoOfInstructions = req_struct->no_exec_instructions;
  ndbassert(RnoOfInstructions == 0);
  /* ---------------------------------------------------------------- */
  // Initialise all 8 registers to contain the NULL value.
  // In this version we can handle 32 and 64 bit unsigned integers.
  // They are handled as 64 bit values. Thus the 32 most significant
  // bits are zeroed for 32 bit values.
  /* ---------------------------------------------------------------- */
  TregMemBuffer[0]= 0;
  TregMemBuffer[4]= 0;
  TregMemBuffer[8]= 0;
  TregMemBuffer[12]= 0;
  TregMemBuffer[16]= 0;
  TregMemBuffer[20]= 0;
  TregMemBuffer[24]= 0;
  TregMemBuffer[28]= 0;
  Uint32 tmpHabitant= ~0;

  while (RnoOfInstructions < 8000) {
    /* ---------------------------------------------------------------- */
    /* EXECUTE THE NEXT INTERPRETER INSTRUCTION.                        */
    /* ---------------------------------------------------------------- */
    RnoOfInstructions++;
    theInstruction= TcurrentProgram[TprogramCounter];
    theRegister= Interpreter::getReg1(theInstruction) << 2;
#ifdef TRACE_INTERPRETER
    ndbout_c("Interpreter : RnoOfInstructions : %u.  TprogramCounter : %u.  Opcode : %u",
             RnoOfInstructions, TprogramCounter, Interpreter::getOpCode(theInstruction));
#endif
    if (TprogramCounter < TcurrentSize)
    {
      TprogramCounter++;
      switch (Interpreter::getOpCode(theInstruction)) {
      case Interpreter::READ_ATTR_INTO_REG:
	jamDebug();
	/* ---------------------------------------------------------------- */
	// Read an attribute from the tuple into a register.
	// While reading an attribute we allow the attribute to be an array
	// as long as it fits in the 64 bits of the register.
	/* ---------------------------------------------------------------- */
	{
	  Uint32 theAttrinfo= theInstruction;
	  int TnoDataRW= readAttributes(req_struct,
				     &theAttrinfo,
				     (Uint32)1,
				     &TregMemBuffer[theRegister],
				     (Uint32)3,
                                     false);
	  if (TnoDataRW == 2)
          {
	    /* ------------------------------------------------------------- */
	    // Two words read means that we get the instruction plus one 32 
	    // word read. Thus we set the register to be a 32 bit register.
	    /* ------------------------------------------------------------- */
	    TregMemBuffer[theRegister]= 0x50;
            // arithmetic conversion if big-endian
            * (Int64*)(TregMemBuffer+theRegister+2)= TregMemBuffer[theRegister+1];
	  }
          else if (TnoDataRW == 3)
          {
	    /* ------------------------------------------------------------- */
	    // Three words read means that we get the instruction plus two 
	    // 32 words read. Thus we set the register to be a 64 bit register.
	    /* ------------------------------------------------------------- */
	    TregMemBuffer[theRegister]= 0x60;
            TregMemBuffer[theRegister+3]= TregMemBuffer[theRegister+2];
            TregMemBuffer[theRegister+2]= TregMemBuffer[theRegister+1];
	  }
          else if (TnoDataRW == 1)
          {
	    /* ------------------------------------------------------------- */
	    // One word read means that we must have read a NULL value. We set
	    // the register to indicate a NULL value.
	    /* ------------------------------------------------------------- */
	    TregMemBuffer[theRegister]= 0;
	    TregMemBuffer[theRegister + 2]= 0;
	    TregMemBuffer[theRegister + 3]= 0;
	  }
          else if (TnoDataRW < 0)
          {
	    jamDebug();
            terrorCode = Uint32(-TnoDataRW);
	    tupkeyErrorLab(req_struct);
	    return -1;
	  }
          else
          {
	    /* ------------------------------------------------------------- */
	    // Any other return value from the read attribute here is not 
	    // allowed and will lead to a system crash.
	    /* ------------------------------------------------------------- */
	    ndbrequire(false);
	  }
	  break;
	}

      case Interpreter::WRITE_ATTR_FROM_REG:
	{
	  jamDebug();
	  Uint32 TattrId= theInstruction >> 16;
	  Uint32 TattrDescrIndex= req_struct->tablePtrP->tabDescriptor +
	    (TattrId << ZAD_LOG_SIZE);
	  Uint32 TattrDesc1= tableDescriptor[TattrDescrIndex].tabDescr;
	  Uint32 TregType= TregMemBuffer[theRegister];

	  /* --------------------------------------------------------------- */
	  // Calculate the number of words of this attribute.
	  // We allow writes into arrays as long as they fit into the 64 bit
	  // register size.
	  /* --------------------------------------------------------------- */
          Uint32 TattrNoOfWords = AttributeDescriptor::getSizeInWords(TattrDesc1);
	  Uint32 Toptype = req_struct->operPtrP->op_type;
	  Uint32 TdataForUpdate[3];
	  Uint32 Tlen;

	  AttributeHeader ah(TattrId, TattrNoOfWords << 2);
          TdataForUpdate[0]= ah.m_value;
	  TdataForUpdate[1]= TregMemBuffer[theRegister + 2];
	  TdataForUpdate[2]= TregMemBuffer[theRegister + 3];
	  Tlen= TattrNoOfWords + 1;
	  if (Toptype == ZUPDATE)
          {
	    if (TattrNoOfWords <= 2)
            {
              if (TattrNoOfWords == 1)
              {
                // arithmetic conversion if big-endian
                Int64 * tmp = new (&TregMemBuffer[theRegister + 2]) Int64;
                TdataForUpdate[1] = Uint32(* tmp);
                TdataForUpdate[2] = 0;
              }
	      if (TregType == 0)
              {
		/* --------------------------------------------------------- */
		// Write a NULL value into the attribute
		/* --------------------------------------------------------- */
		ah.setNULL();
                TdataForUpdate[0]= ah.m_value;
		Tlen= 1;
	      }
	      int TnoDataRW= updateAttributes(req_struct,
					   &TdataForUpdate[0],
					   Tlen);
	      if (TnoDataRW >= 0)
              {
		/* --------------------------------------------------------- */
		// Write the written data also into the log buffer so that it 
		// will be logged.
		/* --------------------------------------------------------- */
		logMemory[TdataWritten + 0]= TdataForUpdate[0];
		logMemory[TdataWritten + 1]= TdataForUpdate[1];
		logMemory[TdataWritten + 2]= TdataForUpdate[2];
		TdataWritten += Tlen;
	      }
              else
              {
                terrorCode = Uint32(-TnoDataRW);
		tupkeyErrorLab(req_struct);
		return -1;
	      }
	    }
            else
            {
	      return TUPKEY_abort(req_struct, 15);
	    }
	  }
          else
          {
	    return TUPKEY_abort(req_struct, 16);
	  }
	  break;
	}

      case Interpreter::LOAD_CONST_NULL:
	jamDebug();
	TregMemBuffer[theRegister]= 0;	/* NULL INDICATOR */
	break;

      case Interpreter::LOAD_CONST16:
	jamDebug();
	TregMemBuffer[theRegister]= 0x50;	/* 32 BIT UNSIGNED CONSTANT */
	* (Int64*)(TregMemBuffer+theRegister+2)= theInstruction >> 16;
	break;

      case Interpreter::LOAD_CONST32:
	jamDebug();
	TregMemBuffer[theRegister]= 0x50;	/* 32 BIT UNSIGNED CONSTANT */
	* (Int64*)(TregMemBuffer+theRegister+2)= * 
	  (TcurrentProgram+TprogramCounter);
	TprogramCounter++;
	break;

      case Interpreter::LOAD_CONST64:
	jamDebug();
	TregMemBuffer[theRegister]= 0x60;	/* 64 BIT UNSIGNED CONSTANT */
        TregMemBuffer[theRegister + 2 ]= * (TcurrentProgram +
                                             TprogramCounter++);
        TregMemBuffer[theRegister + 3 ]= * (TcurrentProgram +
                                             TprogramCounter++);
	break;

      case Interpreter::ADD_REG_REG:
	jamDebug();
	{
	  Uint32 TrightRegister= Interpreter::getReg2(theInstruction) << 2;
	  Uint32 TdestRegister= Interpreter::getReg3(theInstruction) << 2;

	  Uint32 TrightType= TregMemBuffer[TrightRegister];
	  Int64 Tright0= * (Int64*)(TregMemBuffer + TrightRegister + 2);
	  

	  Uint32 TleftType= TregMemBuffer[theRegister];
	  Int64 Tleft0= * (Int64*)(TregMemBuffer + theRegister + 2);
         
	  if ((TleftType | TrightType) != 0)
          {
	    Uint64 Tdest0= Tleft0 + Tright0;
	    * (Int64*)(TregMemBuffer+TdestRegister+2)= Tdest0;
	    TregMemBuffer[TdestRegister]= 0x60;
	  }
          else
          {
	    return TUPKEY_abort(req_struct, 20);
	  }
	  break;
	}

      case Interpreter::SUB_REG_REG:
	jamDebug();
	{
	  Uint32 TrightRegister= Interpreter::getReg2(theInstruction) << 2;
	  Uint32 TdestRegister= Interpreter::getReg3(theInstruction) << 2;

	  Uint32 TrightType= TregMemBuffer[TrightRegister];
	  Int64 Tright0= * (Int64*)(TregMemBuffer + TrightRegister + 2);
	  
	  Uint32 TleftType= TregMemBuffer[theRegister];
	  Int64 Tleft0= * (Int64*)(TregMemBuffer + theRegister + 2);
         
	  if ((TleftType | TrightType) != 0)
          {
	    Int64 Tdest0= Tleft0 - Tright0;
	    * (Int64*)(TregMemBuffer+TdestRegister+2)= Tdest0;
	    TregMemBuffer[TdestRegister]= 0x60;
	  }
          else
          {
	    return TUPKEY_abort(req_struct, 20);
	  }
	  break;
	}

      case Interpreter::BRANCH:
	TprogramCounter= brancher(theInstruction, TprogramCounter);
	break;

      case Interpreter::BRANCH_REG_EQ_NULL:
	if (TregMemBuffer[theRegister] != 0)
        {
	  jamDebug();
	  continue;
	}
        else
        {
	  jamDebug();
	  TprogramCounter= brancher(theInstruction, TprogramCounter);
	}
	break;

      case Interpreter::BRANCH_REG_NE_NULL:
	if (TregMemBuffer[theRegister] == 0)
        {
	  jamDebug();
	  continue;
	}
        else
        {
	  jamDebug();
	  TprogramCounter= brancher(theInstruction, TprogramCounter);
	}
	break;


      case Interpreter::BRANCH_EQ_REG_REG:
	{
	  jamDebug();
	  Uint32 TrightRegister= Interpreter::getReg2(theInstruction) << 2;

	  Uint32 TleftType= TregMemBuffer[theRegister];
	  Uint32 Tleft0= TregMemBuffer[theRegister + 2];
	  Uint32 Tleft1= TregMemBuffer[theRegister + 3];

	  Uint32 TrightType= TregMemBuffer[TrightRegister];
	  Uint32 Tright0= TregMemBuffer[TrightRegister + 2];
	  Uint32 Tright1= TregMemBuffer[TrightRegister + 3];
	  if ((TrightType | TleftType) != 0)
          {
	    jamDebug();
	    if ((Tleft0 == Tright0) && (Tleft1 == Tright1))
            {
	      TprogramCounter= brancher(theInstruction, TprogramCounter);
	    }
	  }
          else
          {
	    return TUPKEY_abort(req_struct, 23);
	  }
	  break;
	}

      case Interpreter::BRANCH_NE_REG_REG:
	{
	  Uint32 TrightRegister= Interpreter::getReg2(theInstruction) << 2;

	  Uint32 TleftType= TregMemBuffer[theRegister];
	  Uint32 Tleft0= TregMemBuffer[theRegister + 2];
	  Uint32 Tleft1= TregMemBuffer[theRegister + 3];

	  Uint32 TrightType= TregMemBuffer[TrightRegister];
	  Uint32 Tright0= TregMemBuffer[TrightRegister + 2];
	  Uint32 Tright1= TregMemBuffer[TrightRegister + 3];
	  if ((TrightType | TleftType) != 0)
          {
	    jamDebug();
	    if ((Tleft0 != Tright0) || (Tleft1 != Tright1))
            {
	      TprogramCounter= brancher(theInstruction, TprogramCounter);
	    }
	  }
          else
          {
	    return TUPKEY_abort(req_struct, 24);
	  }
	  break;
	}

      case Interpreter::BRANCH_LT_REG_REG:
	{
	  Uint32 TrightRegister= Interpreter::getReg2(theInstruction) << 2;

	  Uint32 TrightType= TregMemBuffer[TrightRegister];
	  Int64 Tright0= * (Int64*)(TregMemBuffer + TrightRegister + 2);
	  
	  Uint32 TleftType= TregMemBuffer[theRegister];
	  Int64 Tleft0= * (Int64*)(TregMemBuffer + theRegister + 2);
         

	  if ((TrightType | TleftType) != 0)
          {
	    jamDebug();
	    if (Tleft0 < Tright0)
            {
	      TprogramCounter= brancher(theInstruction, TprogramCounter);
	    }
	  }
          else
          {
	    return TUPKEY_abort(req_struct, 24);
	  }
	  break;
	}

      case Interpreter::BRANCH_LE_REG_REG:
	{
	  Uint32 TrightRegister= Interpreter::getReg2(theInstruction) << 2;

	  Uint32 TrightType= TregMemBuffer[TrightRegister];
	  Int64 Tright0= * (Int64*)(TregMemBuffer + TrightRegister + 2);
	  
	  Uint32 TleftType= TregMemBuffer[theRegister];
	  Int64 Tleft0= * (Int64*)(TregMemBuffer + theRegister + 2);
	  

	  if ((TrightType | TleftType) != 0)
          {
	    jamDebug();
	    if (Tleft0 <= Tright0)
            {
	      TprogramCounter= brancher(theInstruction, TprogramCounter);
	    }
	  }
          else
          {
	    return TUPKEY_abort(req_struct, 26);
	  }
	  break;
	}

      case Interpreter::BRANCH_GT_REG_REG:
	{
	  Uint32 TrightRegister= Interpreter::getReg2(theInstruction) << 2;

	  Uint32 TrightType= TregMemBuffer[TrightRegister];
	  Int64 Tright0= * (Int64*)(TregMemBuffer + TrightRegister + 2);
	  
	  Uint32 TleftType= TregMemBuffer[theRegister];
	  Int64 Tleft0= * (Int64*)(TregMemBuffer + theRegister + 2);
	  

	  if ((TrightType | TleftType) != 0)
          {
	    jamDebug();
	    if (Tleft0 > Tright0)
            {
	      TprogramCounter= brancher(theInstruction, TprogramCounter);
	    }
	  }
          else
          {
	    return TUPKEY_abort(req_struct, 27);
	  }
	  break;
	}

      case Interpreter::BRANCH_GE_REG_REG:
	{
	  Uint32 TrightRegister= Interpreter::getReg2(theInstruction) << 2;

	  Uint32 TrightType= TregMemBuffer[TrightRegister];
	  Int64 Tright0= * (Int64*)(TregMemBuffer + TrightRegister + 2);
	  
	  Uint32 TleftType= TregMemBuffer[theRegister];
	  Int64 Tleft0= * (Int64*)(TregMemBuffer + theRegister + 2);
	  

	  if ((TrightType | TleftType) != 0)
          {
	    jamDebug();
	    if (Tleft0 >= Tright0)
            {
	      TprogramCounter= brancher(theInstruction, TprogramCounter);
	    }
	  }
          else
          {
	    return TUPKEY_abort(req_struct, 28);
	  }
	  break;
	}

      case Interpreter::BRANCH_ATTR_OP_ARG_2:
      case Interpreter::BRANCH_ATTR_OP_ARG:{
	jamDebug();
	Uint32 cond = Interpreter::getBinaryCondition(theInstruction);
	Uint32 ins2 = TcurrentProgram[TprogramCounter];
	Uint32 attrId = Interpreter::getBranchCol_AttrId(ins2) << 16;
	Uint32 argLen = Interpreter::getBranchCol_Len(ins2);
        Uint32 step = argLen;

	if (tmpHabitant != attrId)
        {
	  Int32 TnoDataR = readAttributes(req_struct,
					  &attrId, 1,
					  tmpArea, tmpAreaSz,
                                          false);
	  
	  if (unlikely(TnoDataR < 0))
          {
	    jam();
            terrorCode = Uint32(-TnoDataR);
	    tupkeyErrorLab(req_struct);
	    return -1;
	  }
	  tmpHabitant= attrId;
	}

        // get type
	attrId >>= 16;
	Uint32 TattrDescrIndex = req_struct->tablePtrP->tabDescriptor +
	  (attrId << ZAD_LOG_SIZE);
	Uint32 TattrDesc1 = tableDescriptor[TattrDescrIndex].tabDescr;
	Uint32 TattrDesc2 = tableDescriptor[TattrDescrIndex+1].tabDescr;
	Uint32 typeId = AttributeDescriptor::getType(TattrDesc1);
	void * cs = 0;
	if (AttributeOffset::getCharsetFlag(TattrDesc2))
	{
	  Uint32 pos = AttributeOffset::getCharsetPos(TattrDesc2);
	  cs = req_struct->tablePtrP->charsetArray[pos];
	}
	const NdbSqlUtil::Type& sqlType = NdbSqlUtil::getType(typeId);

        // get data
	AttributeHeader ah(tmpArea[0]);
        const char* s1 = (char*)&tmpArea[1];
        const char* s2 = (char*)&TcurrentProgram[TprogramCounter+1];
        // fixed length in 5.0
	Uint32 attrLen = AttributeDescriptor::getSizeInBytes(TattrDesc1);

        if (Interpreter::getOpCode(theInstruction) ==
            Interpreter::BRANCH_ATTR_OP_ARG_2)
        {
          jamDebug();
          Uint32 paramNo = Interpreter::getBranchCol_ParamNo(ins2);
          const Uint32 * paramptr = lookupInterpreterParameter(paramNo,
                                                               subroutineProg,
                                                               TsubroutineLen);
          if (unlikely(paramptr == 0))
          {
            jam();
            terrorCode = 99; // TODO
            tupkeyErrorLab(req_struct);
            return -1;
          }

          argLen = AttributeHeader::getByteSize(* paramptr);
          step = 0;
          s2 = (char*)(paramptr + 1);
        }
        
        if (typeId == NDB_TYPE_BIT)
        {
          /* Size in bytes for bit fields can be incorrect due to
           * rounding down
           */
          Uint32 bitFieldAttrLen= (AttributeDescriptor::getArraySize(TattrDesc1)
                                   + 7) / 8;
          attrLen= bitFieldAttrLen;
        }

	bool r1_null = ah.isNULL();
	bool r2_null = argLen == 0;
	int res1;
        if (cond <= Interpreter::GE)
        {
          /* Inequality - EQ, NE, LT, LE, GT, GE */
          if (r1_null || r2_null)
          {
            // NULL==NULL and NULL<not-NULL
            res1 = r1_null && r2_null ? 0 : r1_null ? -1 : 1;
          }
          else
          {
	    jamDebug();
	    if (unlikely(sqlType.m_cmp == 0))
	    {
	      return TUPKEY_abort(req_struct, 40);
	    }
            res1 = (*sqlType.m_cmp)(cs, s1, attrLen, s2, argLen);
          }
	}
        else
        {
          if ((cond == Interpreter::LIKE) ||
              (cond == Interpreter::NOT_LIKE))
          {
            if (r1_null || r2_null)
            {
              // NULL like NULL is true (has no practical use)
              res1 =  r1_null && r2_null ? 0 : -1;
            }
            else
            {
              jam();
              if (unlikely(sqlType.m_like == 0))
              {
                return TUPKEY_abort(req_struct, 40);
              }
              res1 = (*sqlType.m_like)(cs, s1, attrLen, s2, argLen);
            }
          }
          else
          {
            /* AND_XX_MASK condition */
            ndbassert(cond <= Interpreter::AND_NE_ZERO);
            if (unlikely(sqlType.m_mask == 0))
            {
              return TUPKEY_abort(req_struct,40);
            }
            /* If either arg is NULL, we say COL AND MASK
             * NE_ZERO and NE_MASK.
             */
            if (r1_null || r2_null)
            {
              res1= 1;
            }
            else
            {
              
              bool cmpZero= 
                (cond == Interpreter::AND_EQ_ZERO) ||
                (cond == Interpreter::AND_NE_ZERO);
              
              res1 = (*sqlType.m_mask)(s1, attrLen, s2, argLen, cmpZero);
            }
          }
        }

        int res = 0;
        switch ((Interpreter::BinaryCondition)cond) {
        case Interpreter::EQ:
          res = (res1 == 0);
          break;
        case Interpreter::NE:
          res = (res1 != 0);
          break;
        // note the condition is backwards
        case Interpreter::LT:
          res = (res1 > 0);
          break;
        case Interpreter::LE:
          res = (res1 >= 0);
          break;
        case Interpreter::GT:
          res = (res1 < 0);
          break;
        case Interpreter::GE:
          res = (res1 <= 0);
          break;
        case Interpreter::LIKE:
          res = (res1 == 0);
          break;
        case Interpreter::NOT_LIKE:
          res = (res1 == 1);
          break;
        case Interpreter::AND_EQ_MASK:
          res = (res1 == 0);
          break;
        case Interpreter::AND_NE_MASK:
          res = (res1 != 0);
          break;
        case Interpreter::AND_EQ_ZERO:
          res = (res1 == 0);
          break;
        case Interpreter::AND_NE_ZERO:
          res = (res1 != 0);
          break;
	  // XXX handle invalid value
        }
#ifdef TRACE_INTERPRETER
	ndbout_c("cond=%u attr(%d)='%.*s'(%d) str='%.*s'(%d) res1=%d res=%d",
		 cond, attrId >> 16,
                 attrLen, s1, attrLen, argLen, s2, argLen, res1, res);
#endif
        if (res)
          TprogramCounter = brancher(theInstruction, TprogramCounter);
        else 
	{
          Uint32 tmp = ((step + 3) >> 2) + 1;
          TprogramCounter += tmp;
        }
	break;
      }
	
      case Interpreter::BRANCH_ATTR_EQ_NULL:{
	jamDebug();
	Uint32 ins2= TcurrentProgram[TprogramCounter];
	Uint32 attrId= Interpreter::getBranchCol_AttrId(ins2) << 16;
	
	if (tmpHabitant != attrId)
        {
	  Int32 TnoDataR= readAttributes(req_struct,
					  &attrId, 1,
					  tmpArea, tmpAreaSz,
                                          false);
	  
	  if (unlikely(TnoDataR < 0))
          {
	    jam();
            terrorCode = Uint32(-TnoDataR);
	    tupkeyErrorLab(req_struct);
	    return -1;
	  }
	  tmpHabitant= attrId;
	}
	
	AttributeHeader ah(tmpArea[0]);
	if (ah.isNULL())
        {
	  TprogramCounter= brancher(theInstruction, TprogramCounter);
	}
        else
        {
	  TprogramCounter ++;
	}
	break;
      }

      case Interpreter::BRANCH_ATTR_NE_NULL:
      {
	jamDebug();
	Uint32 ins2= TcurrentProgram[TprogramCounter];
	Uint32 attrId= Interpreter::getBranchCol_AttrId(ins2) << 16;
	
	if (tmpHabitant != attrId)
        {
	  Int32 TnoDataR= readAttributes(req_struct,
					  &attrId, 1,
					  tmpArea, tmpAreaSz,
                                          false);
	  
	  if (unlikely(TnoDataR < 0))
          {
	    jam();
            terrorCode = Uint32(-TnoDataR);
	    tupkeyErrorLab(req_struct);
	    return -1;
	  }
	  tmpHabitant= attrId;
	}
	
	AttributeHeader ah(tmpArea[0]);
	if (ah.isNULL())
        {
	  TprogramCounter ++;
	}
        else
        {
	  TprogramCounter= brancher(theInstruction, TprogramCounter);
	}
	break;
      }
	
      case Interpreter::EXIT_OK:
	jamDebug();
#ifdef TRACE_INTERPRETER
	ndbout_c(" - exit_ok");
#endif
	return TdataWritten;

      case Interpreter::EXIT_OK_LAST:
	jamDebug();
#ifdef TRACE_INTERPRETER
	ndbout_c(" - exit_ok_last");
#endif
	req_struct->last_row= true;
	return TdataWritten;
	
      case Interpreter::EXIT_REFUSE:
      {
        /**
         * This is a very common exit path, particularly
         * for scans. It simply means that the row didn't
         * fulfil the search condition.
         */
	jamDebug();
#ifdef TRACE_INTERPRETER
	ndbout_c(" - exit_nok");
#endif
	terrorCode = theInstruction >> 16;
        tupkeyErrorLab(req_struct);
        return -1;
      }
      case Interpreter::CALL:
	jamDebug();
#ifdef TRACE_INTERPRETER
        ndbout_c(" - call addr=%u, subroutine len=%u ret addr=%u",
                 theInstruction >> 16, TsubroutineLen, TprogramCounter);
#endif
	RstackPtr++;
	if (RstackPtr < 32)
        {
          TstackMemBuffer[RstackPtr]= TprogramCounter;
          TprogramCounter= theInstruction >> 16;
	  if (TprogramCounter < TsubroutineLen)
          {
	    TcurrentProgram= subroutineProg;
	    TcurrentSize= TsubroutineLen;
	  }
          else
          {
	    return TUPKEY_abort(req_struct, 30);
	  }
	}
        else
        {
	  return TUPKEY_abort(req_struct, 31);
	}
	break;

      case Interpreter::RETURN:
	jamDebug();
#ifdef TRACE_INTERPRETER
        ndbout_c(" - return to %u from stack level %u",
                 TstackMemBuffer[RstackPtr],
                 RstackPtr);
#endif
	if (RstackPtr > 0)
        {
	  TprogramCounter= TstackMemBuffer[RstackPtr];
	  RstackPtr--;
	  if (RstackPtr == 0)
          {
	    jamDebug();
	    /* ------------------------------------------------------------- */
	    // We are back to the main program.
	    /* ------------------------------------------------------------- */
	    TcurrentProgram= mainProgram;
	    TcurrentSize= TmainProgLen;
	  }
	}
        else
        {
	  return TUPKEY_abort(req_struct, 32);
	}
	break;

      default:
	return TUPKEY_abort(req_struct, 33);
      }
    }
    else
    {
      return TUPKEY_abort(req_struct, 34);
    }
  }
  return TUPKEY_abort(req_struct, 35);
}

/**
 * expand_var_part - copy packed variable attributes to fully expanded size
 * 
 * dst:        where to start writing attribute data
 * dst_off_ptr where to write attribute offsets
 * src         pointer to packed attributes
 * tabDesc     array of attribute descriptors (used for getting max size)
 * no_of_attr  no of atributes to expand
 */
static
Uint32*
expand_var_part(Dbtup::KeyReqStruct::Var_data *dst, 
		const Uint32* src, 
		const Uint32 * tabDesc, 
		const Uint16* order)
{
  char* dst_ptr= dst->m_data_ptr;
  Uint32 no_attr= dst->m_var_len_offset;
  Uint16* dst_off_ptr= dst->m_offset_array_ptr;
  Uint16* dst_len_ptr= dst_off_ptr + no_attr;
  const Uint16* src_off_ptr= (const Uint16*)src;
  const char* src_ptr= (const char*)(src_off_ptr + no_attr + 1);
  
  Uint16 tmp= *src_off_ptr++, next_pos, len, max_len, dst_off= 0;
  for(Uint32 i = 0; i<no_attr; i++)
  {
    next_pos= *src_off_ptr++;
    len= next_pos - tmp;
    
    *dst_off_ptr++ = dst_off; 
    *dst_len_ptr++ = dst_off + len;
    memcpy(dst_ptr, src_ptr, len);
    src_ptr += len;
    
    max_len= AttributeDescriptor::getSizeInBytes(tabDesc[* order++]);
    dst_ptr += max_len; // Max size
    dst_off += max_len;
    
    tmp= next_pos;
  }
  
  return ALIGN_WORD(dst_ptr);
}

void
Dbtup::expand_tuple(KeyReqStruct* req_struct, 
		    Uint32 sizes[2],
		    Tuple_header* src, 
		    const Tablerec* tabPtrP,
		    bool disk,
                    bool from_lcp_keep)
{
  Uint32 bits= src->m_header_bits;
  Uint32 extra_bits = bits;
  Tuple_header* ptr= req_struct->m_tuple_ptr;
  
  Uint16 dd_tot= tabPtrP->m_no_of_disk_attributes;
  Uint16 mm_vars= tabPtrP->m_attributes[MM].m_no_of_varsize;
  Uint16 mm_dynvar= tabPtrP->m_attributes[MM].m_no_of_dyn_var;
  Uint16 mm_dynfix= tabPtrP->m_attributes[MM].m_no_of_dyn_fix;
  Uint16 mm_dyns= tabPtrP->m_attributes[MM].m_no_of_dynamic;
  Uint32 fix_size= tabPtrP->m_offsets[MM].m_fix_header_size;
  Uint32 order_desc= tabPtrP->m_real_order_descriptor;

  Uint32 *dst_ptr= ptr->get_end_of_fix_part_ptr(tabPtrP);
  const Uint32 *disk_ref= src->get_disk_ref_ptr(tabPtrP);
  const Uint32 *src_ptr= src->get_end_of_fix_part_ptr(tabPtrP);
  const Var_part_ref* var_ref = src->get_var_part_ref_ptr(tabPtrP);
  const Uint32 *desc= (Uint32*)req_struct->attr_descr;
  const Uint16 *order = (Uint16*)(&tableDescriptor[order_desc]);
  order += tabPtrP->m_attributes[MM].m_no_of_fixsize;
  
  // Copy fix part
  sizes[MM]= 1;
  memcpy(ptr, src, 4*fix_size);
  if(mm_vars || mm_dyns)
  { 
    /*
     * Reserve place for initial length word and offset array (with one extra
     * offset). This will be filled-in in later, in shrink_tuple().
     */
    dst_ptr += Varpart_copy::SZ32;

    KeyReqStruct::Var_data* dst= &req_struct->m_var_data[MM];
    Uint32 step; // in bytes
    Uint32 src_len;
    const Uint32 *src_data;
    if (bits & Tuple_header::VAR_PART)
    {
      KeyReqStruct::Var_data* dst= &req_struct->m_var_data[MM];
      if(! (bits & Tuple_header::COPY_TUPLE))
      {
        /* This is for the initial expansion of a stored row. */
        Ptr<Page> var_page;
        src_data= get_ptr(&var_page, *var_ref);
        src_len= get_len(&var_page, *var_ref);
        jam();
        /**
         * Coming here with MM_GROWN set is possible if we are coming here
         * from handle_lcp_keep_commit. In this case we are currently
         * performing a DELETE operation. This operation is the final
         * operation that will be committed. It could very well have
         * been preceeded by an UPDATE operation that did set the
         * MM_GROWN bit. In this case it is important to get the original
         * length from the end of the varsize part and not the page
         * entry length which is essentially the meaning of the MM_GROWN
         * bit.
         *
         * An original tuple can't have grown as we're expanding it...
         * else we would be "re-expanding". This is the case when coming
         * here as part of INSERT/UPDATE/REFRESH. We assert on that we
         * don't do any "re-expanding".
         */
        if (bits & Tuple_header::MM_GROWN)
        {
          jam();
          ndbrequire(from_lcp_keep);
          ndbassert(src_len>0);
          src_len= src_data[src_len-1];
        }
        sizes[MM]= src_len;
        step= 0;
        req_struct->m_varpart_page_ptr = var_page;
      }
      else
      {
        /* This is for the re-expansion of a shrunken row (update2 ...) */
        Varpart_copy* vp = (Varpart_copy*)src_ptr;
        src_len = vp->m_len;
        src_data= vp->m_data;
        step= (Varpart_copy::SZ32 + src_len); // 1+ is for extra word
        req_struct->m_varpart_page_ptr = req_struct->m_page_ptr;
        sizes[MM]= src_len;
      }

      if (mm_vars)
      {
        dst->m_data_ptr= (char*)(((Uint16*)dst_ptr)+mm_vars+1);
        dst->m_offset_array_ptr= req_struct->var_pos_array;
        dst->m_var_len_offset= mm_vars;
        dst->m_max_var_offset= tabPtrP->m_offsets[MM].m_max_var_offset;
        
        dst_ptr= expand_var_part(dst, src_data, desc, order);
        ndbassert(dst_ptr == ALIGN_WORD(dst->m_data_ptr + dst->m_max_var_offset));
        /**
         * Move to end of fix varpart
         */
        char* varstart = (char*)(((Uint16*)src_data)+mm_vars+1);
        Uint32 varlen = ((Uint16*)src_data)[mm_vars];
        Uint32 *dynstart = ALIGN_WORD(varstart + varlen);

        ndbassert((ptrdiff_t)src_len >= (dynstart - src_data));
        src_len -= Uint32(dynstart - src_data);
        src_data = dynstart;
      }
    }
    else
    {
      /**
       * No varpart...only allowed for dynattr
       */
      ndbassert(mm_vars == 0);
      src_len = step = sizes[MM] = 0;
      src_data = 0;
    }

    if (mm_dyns)
    {
      /**
       * dynattr needs to be expanded even if no varpart existed before
       */
      dst->m_dyn_offset_arr_ptr= req_struct->var_pos_array+2*mm_vars;
      dst->m_dyn_len_offset= mm_dynvar+mm_dynfix;
      dst->m_max_dyn_offset= tabPtrP->m_offsets[MM].m_max_dyn_offset;
      dst->m_dyn_data_ptr= (char*)dst_ptr;
      dst_ptr= expand_dyn_part(dst, src_data,
                               src_len,
                               desc, order + mm_vars,
                               mm_dynvar, mm_dynfix,
                               tabPtrP->m_offsets[MM].m_dyn_null_words);
    }
    
    ndbassert((UintPtr(src_ptr) & 3) == 0);
    src_ptr = src_ptr + step;
  }

  /**
   * The source tuple only touches the header parts. The updates of the
   * tuple is applied on the new copy tuple. We still need to ensure that
   * the checksum is correct on the tuple even after changing the header
   * parts since the header is part of the checksum. This is not covered
   * by setting checksum normally since mostly we don't touch the
   * original tuple.
   */
  updateChecksum(src, tabPtrP, bits, src->m_header_bits);

  sizes[DD]= 0;
  if(disk && dd_tot)
  {
    const Uint16 dd_vars= tabPtrP->m_attributes[DD].m_no_of_varsize;
    order+= mm_vars+mm_dynvar+mm_dynfix;
    
    if(bits & Tuple_header::DISK_INLINE)
    {
      // Only on copy tuple
      ndbassert(bits & Tuple_header::COPY_TUPLE);
    }
    else
    {
      Local_key key;
      memcpy(&key, disk_ref, sizeof(key));
      key.m_page_no= req_struct->m_disk_page_ptr.i;
      src_ptr= get_dd_ptr(&req_struct->m_disk_page_ptr, &key, tabPtrP);
    }
    extra_bits |= Tuple_header::DISK_INLINE;

    // Fix diskpart
    req_struct->m_disk_ptr= (Tuple_header*)dst_ptr;
    memcpy(dst_ptr, src_ptr, 4*tabPtrP->m_offsets[DD].m_fix_header_size);
    sizes[DD] = tabPtrP->m_offsets[DD].m_fix_header_size;
    
    ndbassert(! (req_struct->m_disk_ptr->m_header_bits & Tuple_header::FREE));
    
    ndbrequire(dd_vars == 0);
  }
  
  ptr->m_header_bits= (extra_bits | Tuple_header::COPY_TUPLE);
  req_struct->is_expanded= true;
}

void
Dbtup::dump_tuple(const KeyReqStruct* req_struct, const Tablerec* tabPtrP)
{
  Uint16 mm_vars= tabPtrP->m_attributes[MM].m_no_of_varsize;
  Uint16 mm_dyns= tabPtrP->m_attributes[MM].m_no_of_dynamic;
  //Uint16 dd_tot= tabPtrP->m_no_of_disk_attributes;
  const Tuple_header* ptr= req_struct->m_tuple_ptr;
  Uint32 bits= ptr->m_header_bits;
  const Uint32 *tuple_words= (Uint32 *)ptr;
  const Uint32 *fix_p;
  Uint32 fix_len;
  const Uint32 *var_p;
  Uint32 var_len;
  //const Uint32 *disk_p;
  //Uint32 disk_len;
  const char *typ;

  fix_p= tuple_words;
  fix_len= tabPtrP->m_offsets[MM].m_fix_header_size;
  if(req_struct->is_expanded)
  {
    typ= "expanded";
    var_p= ptr->get_end_of_fix_part_ptr(tabPtrP);
    var_len= 0;                                 // No dump of varpart in expanded
#if 0
    disk_p= (Uint32 *)req_struct->m_disk_ptr;
    disk_len= (dd_tot ? tabPtrP->m_offsets[DD].m_fix_header_size : 0);
#endif
  }
  else if(! (bits & Tuple_header::COPY_TUPLE))
  {
    typ= "stored";
    if(mm_vars+mm_dyns)
    {
      //const KeyReqStruct::Var_data* dst= &req_struct->m_var_data[MM];
      const Var_part_ref *varref= ptr->get_var_part_ref_ptr(tabPtrP);
      Ptr<Page> tmp;
      var_p= get_ptr(&tmp, * varref);
      var_len= get_len(&tmp, * varref);
    }
    else
    {
      var_p= 0;
      var_len= 0;
    }
#if 0
    if(dd_tot)
    {
      Local_key key;
      memcpy(&key, ptr->get_disk_ref_ptr(tabPtrP), sizeof(key));
      key.m_page_no= req_struct->m_disk_page_ptr.i;
      disk_p= get_dd_ptr(&req_struct->m_disk_page_ptr, &key, tabPtrP);
      disk_len= tabPtrP->m_offsets[DD].m_fix_header_size;
    }
    else
    {
      disk_p= var_p;
      disk_len= 0;
    }
#endif
  }
  else
  {
    typ= "shrunken";
    if(mm_vars+mm_dyns)
    {
      var_p= ptr->get_end_of_fix_part_ptr(tabPtrP);
      var_len= *((Uint16 *)var_p) + 1;
    }
    else
    {
      var_p= 0;
      var_len= 0;
    }
#if 0
    disk_p= (Uint32 *)(req_struct->m_disk_ptr);
    disk_len= (dd_tot ? tabPtrP->m_offsets[DD].m_fix_header_size : 0);
#endif
  }
  ndbout_c("Fixed part[%s](%p len=%u words)",typ, fix_p, fix_len);
  dump_hex(fix_p, fix_len);
  ndbout_c("Varpart part[%s](%p len=%u words)", typ , var_p, var_len);
  dump_hex(var_p, var_len);
#if 0
  ndbout_c("Disk part[%s](%p len=%u words)", typ, disk_p, disk_len);
  dump_hex(disk_p, disk_len);
#endif
}

void
Dbtup::prepare_read(KeyReqStruct* req_struct, 
		    Tablerec* tabPtrP, bool disk)
{
  Tuple_header* ptr= req_struct->m_tuple_ptr;
  
  Uint32 bits= ptr->m_header_bits;
  Uint16 dd_tot= tabPtrP->m_no_of_disk_attributes;
  Uint16 mm_vars= tabPtrP->m_attributes[MM].m_no_of_varsize;
  Uint16 mm_dyns= tabPtrP->m_attributes[MM].m_no_of_dynamic;
  
  const Uint32 *src_ptr= ptr->get_end_of_fix_part_ptr(tabPtrP);
  const Uint32 *disk_ref= ptr->get_disk_ref_ptr(tabPtrP);
  const Var_part_ref* var_ref = ptr->get_var_part_ref_ptr(tabPtrP);
  if(mm_vars || mm_dyns)
  {
    const Uint32 *src_data= src_ptr;
    Uint32 src_len;
    KeyReqStruct::Var_data* dst= &req_struct->m_var_data[MM];
    if (bits & Tuple_header::VAR_PART)
    {
      if(! (bits & Tuple_header::COPY_TUPLE))
      {
        Ptr<Page> tmp;
        src_data= get_ptr(&tmp, * var_ref);
        src_len= get_len(&tmp, * var_ref);

        /* If the original tuple was grown,
         * the old size is stored at the end. */
        if(bits & Tuple_header::MM_GROWN)
        {
          /**
           * This is when triggers read before value of update
           *   when original has been reallocated due to grow
           */
          ndbassert(src_len>0);
          src_len= src_data[src_len-1];
        }
      }
      else
      {
        Varpart_copy* vp = (Varpart_copy*)src_ptr;
        src_len = vp->m_len;
        src_data = vp->m_data;
        src_ptr++;
      }

      char* varstart;
      Uint32 varlen;
      const Uint32* dynstart;
      if (mm_vars)
      {
        varstart = (char*)(((Uint16*)src_data)+mm_vars+1);
        varlen = ((Uint16*)src_data)[mm_vars];
        dynstart = ALIGN_WORD(varstart + varlen);
      }
      else
      {
        varstart = 0;
        varlen = 0;
        dynstart = src_data;
      }

      dst->m_data_ptr= varstart;
      dst->m_offset_array_ptr= (Uint16*)src_data;
      dst->m_var_len_offset= 1;
      dst->m_max_var_offset= varlen;

      Uint32 dynlen = Uint32(src_len - (dynstart - src_data));
      ndbassert((ptrdiff_t)src_len >= (dynstart - src_data));
      dst->m_dyn_data_ptr= (char*)dynstart;
      dst->m_dyn_part_len= dynlen;
      // Do or not to to do
      // dst->m_dyn_offset_arr_ptr = dynlen ? (Uint16*)(dynstart + *(Uint8*)dynstart) : 0;

      /*
        dst->m_dyn_offset_arr_ptr and dst->m_dyn_len_offset are not used for
        reading the stored/shrunken format.
      */
    }
    else
    {
      src_len = 0;
      dst->m_max_var_offset = 0;
      dst->m_dyn_part_len = 0;
#if defined(VM_TRACE) || defined(ERROR_INSERT)
      bzero(dst, sizeof(* dst));
#endif
    }
    
    // disk part start after dynamic part.
    src_ptr+= src_len;
  }
  
  if(disk && dd_tot)
  {
    const Uint16 dd_vars= tabPtrP->m_attributes[DD].m_no_of_varsize;
    
    if(bits & Tuple_header::DISK_INLINE)
    {
      // Only on copy tuple
      ndbassert(bits & Tuple_header::COPY_TUPLE);
    }
    else
    {
      // XXX
      Local_key key;
      memcpy(&key, disk_ref, sizeof(key));
      key.m_page_no= req_struct->m_disk_page_ptr.i;
      src_ptr= get_dd_ptr(&req_struct->m_disk_page_ptr, &key, tabPtrP);
    }
    // Fix diskpart
    req_struct->m_disk_ptr= (Tuple_header*)src_ptr;
#ifdef VM_TRACE
    if (req_struct->m_disk_ptr->m_header_bits & Tuple_header::FREE)
    {
      Local_key key;
      memcpy(&key, disk_ref, sizeof(key));
      g_eventLogger->info(
        "Crash: page(%u,%u,%u,%u).%u, DISK_INLINE= %u, tab(%u,%u,%u)",
                          instance(),
                          req_struct->m_disk_page_ptr.i,
                          req_struct->m_disk_page_ptr.p->m_file_no,
                          req_struct->m_disk_page_ptr.p->m_page_no,
                          key.m_page_idx,
                          bits & Tuple_header::DISK_INLINE ? 1 : 0,
                          req_struct->m_disk_page_ptr.p->m_table_id,
                          req_struct->m_disk_page_ptr.p->m_fragment_id,
                          req_struct->m_disk_page_ptr.p->m_create_table_version);
    }
#endif
    ndbassert(! (req_struct->m_disk_ptr->m_header_bits & Tuple_header::FREE));
    ndbrequire(dd_vars == 0);
  }

  req_struct->is_expanded= false;
}

void
Dbtup::shrink_tuple(KeyReqStruct* req_struct, Uint32 sizes[2],
		    const Tablerec* tabPtrP, bool disk)
{
  ndbassert(tabPtrP->need_shrink());
  Tuple_header* ptr= req_struct->m_tuple_ptr;
  ndbassert(ptr->m_header_bits & Tuple_header::COPY_TUPLE);
  
  KeyReqStruct::Var_data* dst= &req_struct->m_var_data[MM];
  Uint32 order_desc= tabPtrP->m_real_order_descriptor;
  const Uint32 * tabDesc= (Uint32*)req_struct->attr_descr;
  const Uint16 *order = (Uint16*)(&tableDescriptor[order_desc]);
  Uint16 dd_tot= tabPtrP->m_no_of_disk_attributes;
  Uint16 mm_fix= tabPtrP->m_attributes[MM].m_no_of_fixsize;
  Uint16 mm_vars= tabPtrP->m_attributes[MM].m_no_of_varsize;
  Uint16 mm_dyns= tabPtrP->m_attributes[MM].m_no_of_dynamic;
  Uint16 mm_dynvar= tabPtrP->m_attributes[MM].m_no_of_dyn_var;
  Uint16 mm_dynfix= tabPtrP->m_attributes[MM].m_no_of_dyn_fix;
  Uint16 dd_vars= tabPtrP->m_attributes[DD].m_no_of_varsize;
  
  Uint32 *dst_ptr= ptr->get_end_of_fix_part_ptr(tabPtrP);
  Uint16* src_off_ptr= req_struct->var_pos_array;
  order += mm_fix;

  sizes[MM] = 1;
  sizes[DD] = 0;
  if(mm_vars || mm_dyns)
  {
    Varpart_copy* vp = (Varpart_copy*)dst_ptr;
    Uint32* varstart = dst_ptr = vp->m_data;

    if (mm_vars)
    {
      Uint16* dst_off_ptr= (Uint16*)dst_ptr;
      char*  dst_data_ptr= (char*)(dst_off_ptr + mm_vars + 1);
      char*  src_data_ptr= dst_data_ptr;
      Uint32 off= 0;
      for(Uint32 i= 0; i<mm_vars; i++)
      {
        const char* data_ptr= src_data_ptr + *src_off_ptr;
        Uint32 len= src_off_ptr[mm_vars] - *src_off_ptr;
        * dst_off_ptr++= off;
        memmove(dst_data_ptr, data_ptr, len);
        off += len;
        src_off_ptr++;
        dst_data_ptr += len;
      }
      *dst_off_ptr= off;
      dst_ptr = ALIGN_WORD(dst_data_ptr);
      order += mm_vars; // Point to first dynfix entry
    }
    
    if (mm_dyns)
    {
      dst_ptr = shrink_dyn_part(dst, dst_ptr, tabPtrP, tabDesc,
                                order, mm_dynvar, mm_dynfix, MM);
      ndbassert((char*)dst_ptr <= ((char*)ptr) + 14140); // NDB_MAX_TUPLE_SIZE + header
      order += mm_dynfix + mm_dynvar;
    }
    
    Uint32 varpart_len= Uint32(dst_ptr - varstart);
    vp->m_len = varpart_len;
    sizes[MM] = varpart_len;
    if (varpart_len != 0)
    {
      ptr->m_header_bits |= Tuple_header::VAR_PART;
    }
    else if ((ptr->m_header_bits & Tuple_header::VAR_PART) == 0)
    {
      /*
       * No varpart present.
       * And this is not an update where the dynamic column is set to null.
       * So skip storing the var part altogether.
       */
      ndbassert(((Uint32*) vp) == ptr->get_end_of_fix_part_ptr(tabPtrP));
      dst_ptr= (Uint32*)vp;
    }
    else
    {
      /*
       * varpart_len is now 0, but tuple already had a varpart.
       * It will be released at commit time.
       */
    }
    
    ndbassert((UintPtr(ptr) & 3) == 0);
    ndbassert(varpart_len < 0x10000);
  }
  
  if(disk && dd_tot)
  {
    Uint32 * src_ptr = (Uint32*)req_struct->m_disk_ptr;
    req_struct->m_disk_ptr = (Tuple_header*)dst_ptr;
    ndbrequire(dd_vars == 0);
    sizes[DD] = tabPtrP->m_offsets[DD].m_fix_header_size;
    memmove(dst_ptr, src_ptr, 4*tabPtrP->m_offsets[DD].m_fix_header_size);
  }

  req_struct->is_expanded= false;

}

void
Dbtup::validate_page(Tablerec* regTabPtr, Var_page* p)
{
  /* ToDo: We could also do some checks here for any dynamic part. */
  Uint32 mm_vars= regTabPtr->m_attributes[MM].m_no_of_varsize;
  Uint32 fix_sz= regTabPtr->m_offsets[MM].m_fix_header_size + 
    Tuple_header::HeaderSize;
    
  if(mm_vars == 0)
    return;
  
  for(Uint32 F= 0; F<NDB_ARRAY_SIZE(regTabPtr->fragrec); F++)
  {
    FragrecordPtr fragPtr;

    if((fragPtr.i = regTabPtr->fragrec[F]) == RNIL)
      continue;

    ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
    for(Uint32 P= 0; P<fragPtr.p->noOfPages; P++)
    {
      Uint32 real= getRealpid(fragPtr.p, P);
      Var_page* page= (Var_page*)c_page_pool.getPtr(real);

      for(Uint32 i=1; i<page->high_index; i++)
      {
	Uint32 idx= page->get_index_word(i);
	Uint32 len = (idx & Var_page::LEN_MASK) >> Var_page::LEN_SHIFT;
	if(!(idx & Var_page::FREE) && !(idx & Var_page::CHAIN))
	{
	  Tuple_header *ptr= (Tuple_header*)page->get_ptr(i);
	  Uint32 *part= ptr->get_end_of_fix_part_ptr(regTabPtr);
	  if(! (ptr->m_header_bits & Tuple_header::COPY_TUPLE))
	  {
	    ndbrequire(len == fix_sz + 1);
            Local_key tmp;
            Var_part_ref* vpart = reinterpret_cast<Var_part_ref*>(part);
            vpart->copyout(&tmp);
#if defined(VM_TRACE) || defined(ERROR_INSERT)
            ndbrequire(!"Looking for test coverage - found it!");
#endif
	    Ptr<Page> tmpPage;
	    part= get_ptr(&tmpPage, *vpart);
	    len= ((Var_page*)tmpPage.p)->get_entry_len(tmp.m_page_idx);
	    Uint32 sz= ((mm_vars + 1) << 1) + (((Uint16*)part)[mm_vars]);
	    ndbrequire(len >= ((sz + 3) >> 2));
	  } 
	  else
	  {
	    Uint32 sz= ((mm_vars + 1) << 1) + (((Uint16*)part)[mm_vars]);
	    ndbrequire(len >= ((sz+3)>>2)+fix_sz);
	  }
	  if(ptr->m_operation_ptr_i != RNIL)
	  {
	    c_operation_pool.getPtr(ptr->m_operation_ptr_i);
	  }
	} 
	else if(!(idx & Var_page::FREE))
	{
	  /**
	   * Chain
	   */
	  Uint32 *part= page->get_ptr(i);
	  Uint32 sz= ((mm_vars + 1) << 1) + (((Uint16*)part)[mm_vars]);
	  ndbrequire(len >= ((sz + 3) >> 2));
	} 
	else 
	{
	  
	}
      }
      if(p == 0 && page->high_index > 1)
	page->reorg((Var_page*)ctemp_page);
    }
  }
  
  if(p == 0)
  {
    validate_page(regTabPtr, (Var_page*)1);
  }
}

int
Dbtup::handle_size_change_after_update(KeyReqStruct* req_struct,
				       Tuple_header* org,
				       Operationrec* regOperPtr,
				       Fragrecord* regFragPtr,
				       Tablerec* regTabPtr,
				       Uint32 sizes[4])
{
  ndbrequire(sizes[1] == sizes[3]);
  //ndbout_c("%d %d %d %d", sizes[0], sizes[1], sizes[2], sizes[3]);
  if(0)
    printf("%p %d %d - handle_size_change_after_update ",
	   req_struct->m_tuple_ptr,
	   regOperPtr->m_tuple_location.m_page_no,
	   regOperPtr->m_tuple_location.m_page_idx);
  
  Uint32 bits= org->m_header_bits;
  Uint32 copy_bits= req_struct->m_tuple_ptr->m_header_bits;
  
  DEB_LCP(("size_change: tab(%u,%u), row_id(%u,%u), old: %u, new: %u",
          req_struct->fragPtrP->fragTableId,
          req_struct->fragPtrP->fragmentId,
          regOperPtr->m_tuple_location.m_page_no,
          regOperPtr->m_tuple_location.m_page_idx,
          sizes[2+MM],
          sizes[MM]));
  if(sizes[2+MM] == sizes[MM])
    jam();
  else if(sizes[2+MM] < sizes[MM])
  {
    if(0) ndbout_c("shrink");
    jam();
  }
  else
  {
    jam();
    if(0) printf("grow - ");
    Ptr<Page> pagePtr = req_struct->m_varpart_page_ptr;
    Var_page* pageP= (Var_page*)pagePtr.p;
    Var_part_ref *refptr= org->get_var_part_ref_ptr(regTabPtr);
    ndbassert(! (bits & Tuple_header::COPY_TUPLE));

    Local_key ref;
    refptr->copyout(&ref);
    Uint32 alloc;
    Uint32 idx= ref.m_page_idx;
    if (bits & Tuple_header::VAR_PART)
    {
      if (copy_bits & Tuple_header::COPY_TUPLE)
      {
        c_page_pool.getPtr(pagePtr, ref.m_page_no);
        pageP = (Var_page*)pagePtr.p;
      }
      alloc = pageP->get_entry_len(idx);
    }
    else
    {
      alloc = 0;
    }
    Uint32 orig_size= alloc;
    if(bits & Tuple_header::MM_GROWN)
    {
      /* Was grown before, so must fetch real original size from last word. */
      Uint32 *old_var_part= pageP->get_ptr(idx);
      ndbassert(alloc>0);
      orig_size= old_var_part[alloc-1];
    }

    if (alloc)
    {
#ifdef VM_TRACE
      if(!pageP->get_entry_chain(idx))
        ndbout << *pageP << endl;
#endif
      ndbassert(pageP->get_entry_chain(idx));
    }

    Uint32 needed= sizes[2+MM];

    if(needed <= alloc)
    {
      //ndbassert(!regOperPtr->is_first_operation());
      if (0) ndbout_c(" no grow");
      jam();
      return 0;
    }
    Uint32 *new_var_part=realloc_var_part(&terrorCode,
                                          regFragPtr, regTabPtr, pagePtr,
                                          refptr, alloc, needed);
    if (unlikely(new_var_part==NULL))
      return -1;
    /* Mark the tuple grown, store the original length at the end. */
    DEB_LCP(("tab(%u,%u), row_id(%u,%u), set MM_GROWN",
            req_struct->fragPtrP->fragTableId,
            req_struct->fragPtrP->fragmentId,
            regOperPtr->m_tuple_location.m_page_no,
            regOperPtr->m_tuple_location.m_page_idx));
    org->m_header_bits= bits | Tuple_header::MM_GROWN | Tuple_header::VAR_PART;
    new_var_part[needed-1]= orig_size;

    /**
     * Here we can change both header bits and the reference to the varpart,
     * this means that we need to completely recalculate the checksum here.
     */
    setChecksum(org, regTabPtr);
  }
  return 0;
}

int
Dbtup::optimize_var_part(KeyReqStruct* req_struct,
                         Tuple_header* org,
                         Operationrec* regOperPtr,
                         Fragrecord* regFragPtr,
                         Tablerec* regTabPtr)
{
  jam();
  Var_part_ref* refptr = org->get_var_part_ref_ptr(regTabPtr);

  Local_key ref;
  refptr->copyout(&ref);
  Uint32 idx = ref.m_page_idx;

  Ptr<Page> pagePtr;
  c_page_pool.getPtr(pagePtr, ref.m_page_no);

  Var_page* pageP = (Var_page*)pagePtr.p;
  Uint32 var_part_size = pageP->get_entry_len(idx);

  /**
   * if the size of page list_index is MAX_FREE_LIST,
   * we think it as full page, then need not optimize
   */
  if(pageP->list_index != MAX_FREE_LIST)
  {
    jam();
    /*
     * optimize var part of tuple by moving varpart, 
     * then we possibly reclaim free pages
     */
    move_var_part(regFragPtr, regTabPtr, pagePtr,
                  refptr, var_part_size);
    setChecksum(org, regTabPtr);
  }

  return 0;
}

int
Dbtup::nr_update_gci(Uint32 fragPtrI,
                     const Local_key* key,
                     Uint32 gci,
                     bool tuple_exists)
{
  FragrecordPtr fragPtr;
  fragPtr.i= fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
  TablerecPtr tablePtr;
  tablePtr.i= fragPtr.p->fragTableId;
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);

  /**
   * GCI on the row is mandatory since many versions back.
   * During restore we have temporarily disabled this
   * flag to avoid it being set other than when done
   * with a purpose to actually set it (happens in
   * DELETE BY PAGEID and DELETE BY ROWID).
   *
   * This code is called in restore for DELETE BY
   * ROWID and PAGEID. We want to set the GCI in
   * this specific case, but not for WRITEs and
   * INSERTs, so we make this condition always
   * true.
   */
  if (tablePtr.p->m_bits & Tablerec::TR_RowGCI || true)
  {
    Local_key tmp = *key;
    PagePtr pagePtr;

    pagePtr.i = getRealpidCheck(fragPtr.p, tmp.m_page_no);
    if (unlikely(pagePtr.i == RNIL))
    {
      jam();
      ndbassert(!tuple_exists);
      return 0;
    }

    c_page_pool.getPtr(pagePtr);
    
    Tuple_header* ptr = (Tuple_header*)
      ((Fix_page*)pagePtr.p)->get_ptr(tmp.m_page_idx, 0);

    if (tuple_exists)
    {
      ndbrequire(!(ptr->m_header_bits & Tuple_header::FREE));
    }
    else
    {
      ndbrequire(ptr->m_header_bits & Tuple_header::FREE);
    }
    update_gci(fragPtr.p, tablePtr.p, ptr, gci);
  }
  return 0;
}

int
Dbtup::nr_read_pk(Uint32 fragPtrI, 
		  const Local_key* key, Uint32* dst, bool& copy)
{
  
  FragrecordPtr fragPtr;
  fragPtr.i= fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
  TablerecPtr tablePtr;
  tablePtr.i= fragPtr.p->fragTableId;
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);

  Local_key tmp = *key;
  
  PagePtr pagePtr;
  pagePtr.i = getRealpidCheck(fragPtr.p, tmp.m_page_no);
  if (unlikely(pagePtr.i == RNIL))
  {
    jam();
    dst[0] = 0;
    return 0;
  }

  c_page_pool.getPtr(pagePtr);
  KeyReqStruct req_struct(this);
  Uint32* ptr= ((Fix_page*)pagePtr.p)->get_ptr(key->m_page_idx, 0);
  
  req_struct.m_page_ptr = pagePtr;
  req_struct.m_tuple_ptr = (Tuple_header*)ptr;
  Uint32 bits = req_struct.m_tuple_ptr->m_header_bits;

  int ret = 0;
  copy = false;
  if (! (bits & Tuple_header::FREE))
  {
    if (bits & Tuple_header::ALLOC)
    {
      Uint32 opPtrI= req_struct.m_tuple_ptr->m_operation_ptr_i;
      Operationrec* opPtrP= c_operation_pool.getPtr(opPtrI);
      ndbassert(!opPtrP->m_copy_tuple_location.isNull());
      req_struct.m_tuple_ptr=
        get_copy_tuple(&opPtrP->m_copy_tuple_location);
      copy = true;
    }
    req_struct.check_offset[MM]= tablePtr.p->get_check_offset(MM);
    req_struct.check_offset[DD]= tablePtr.p->get_check_offset(DD);
    
    Uint32 num_attr= tablePtr.p->m_no_of_attributes;
    Uint32 descr_start= tablePtr.p->tabDescriptor;
    TableDescriptor *tab_descr= &tableDescriptor[descr_start];
    ndbrequire(descr_start + (num_attr << ZAD_LOG_SIZE) <= cnoOfTabDescrRec);
    req_struct.attr_descr= tab_descr; 

    if (tablePtr.p->need_expand())
      prepare_read(&req_struct, tablePtr.p, false);
    
    const Uint32* attrIds= &tableDescriptor[tablePtr.p->readKeyArray].tabDescr;
    const Uint32 numAttrs= tablePtr.p->noOfKeyAttr;
    // read pk attributes from original tuple
    
    req_struct.tablePtrP = tablePtr.p;
    req_struct.fragPtrP = fragPtr.p;
    
    // do it
    ret = readAttributes(&req_struct,
			 attrIds,
			 numAttrs,
			 dst,
			 ZNIL, false);
    
    // done
    if (likely(ret >= 0)) {
      // remove headers
      Uint32 n= 0;
      Uint32 i= 0;
      while (n < numAttrs) {
	const AttributeHeader ah(dst[i]);
	Uint32 size= ah.getDataSize();
	ndbrequire(size != 0);
	for (Uint32 j= 0; j < size; j++) {
	  dst[i + j - n]= dst[i + j + 1];
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
    dst[ret] = *req_struct.m_tuple_ptr->get_mm_gci(tablePtr.p);
  }
  else
  {
    dst[ret] = 0;
  }
  return ret;
}

int
Dbtup::nr_delete(Signal* signal, Uint32 senderData,
		 Uint32 fragPtrI, const Local_key* key, Uint32 gci)
{
  FragrecordPtr fragPtr;
  fragPtr.i= fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
  TablerecPtr tablePtr;
  tablePtr.i= fragPtr.p->fragTableId;
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);

  Local_key tmp = * key;
  tmp.m_page_no= getRealpid(fragPtr.p, tmp.m_page_no); 
  
  PagePtr pagePtr;
  Tuple_header* ptr= (Tuple_header*)get_ptr(&pagePtr, &tmp, tablePtr.p);

  if (!tablePtr.p->tuxCustomTriggers.isEmpty()) 
  {
    jam();
    TuxMaintReq* req = (TuxMaintReq*)signal->getDataPtrSend();
    req->tableId = fragPtr.p->fragTableId;
    req->fragId = fragPtr.p->fragmentId;
    req->pageId = tmp.m_page_no;
    req->pageIndex = tmp.m_page_idx;
    req->tupVersion = ptr->get_tuple_version();
    req->opInfo = TuxMaintReq::OpRemove;
    removeTuxEntries(signal, tablePtr.p);
  }
  
  Local_key disk;
  memcpy(&disk, ptr->get_disk_ref_ptr(tablePtr.p), sizeof(disk));

  Uint32 lcpScan_ptr_i= fragPtr.p->m_lcp_scan_op;
  Uint32 bits = ptr->m_header_bits;
  if (lcpScan_ptr_i != RNIL &&
      ! (bits & (Tuple_header::LCP_SKIP |
                 Tuple_header::LCP_DELETE |
                 Tuple_header::ALLOC)))
  {
    /**
     * We are performing a node restart currently, at the same time we
     * are also running a LCP on the fragment. This can happen when the
     * UNDO log level becomes too high. In this case we can start a full
     * local LCP during the copy fragment process.
     *
     * Since we are about to delete a row now, we have to ensure that the
     * lcp keep list gets this row before we delete it. This will ensure
     * that the LCP becomes a consistent LCP based on what was there at the
     * start of the LCP.
     */
    jam();
    ScanOpPtr scanOp;
    c_scanOpPool.getPtr(scanOp, lcpScan_ptr_i);
    if (is_rowid_in_remaining_lcp_set(pagePtr.p,
                                      fragPtr.p,
                                      *key,
                                      *scanOp.p,
                                      0))
    {
      KeyReqStruct req_struct(jamBuffer(), KRS_PREPARE);
      Operationrec oprec;
      Tuple_header *copy;
      if ((copy = alloc_copy_tuple(tablePtr.p,
                                   &oprec.m_copy_tuple_location)) == 0)
      {
        /**
         * We failed to allocate the copy record, this is a critical error,
         * we will fail with an error message instruction to increase
         * SharedGlobalMemory.
         */
        char buf[256];
        BaseString::snprintf(buf, sizeof(buf),
                             "Out of memory when allocating copy tuple for"
                             " LCP keep list, increase SharedGlobalMemory");
        progError(__LINE__,
                  NDBD_EXIT_RESOURCE_ALLOC_ERROR,
                  buf);
      }
      req_struct.m_tuple_ptr = ptr;
      oprec.m_tuple_location = tmp;
      oprec.op_type = ZDELETE;
      DEB_LCP_SKIP_DELETE(("(%u)nr_delete: tab(%u,%u), row(%u,%u),"
                           " handle_lcp_keep_commit"
                           ", set LCP_SKIP, bits: %x",
                           instance(),
                           fragPtr.p->fragTableId,
                           fragPtr.p->fragmentId,
                           key->m_page_no,
                           key->m_page_idx,
                           bits));
      handle_lcp_keep_commit(key,
                             &req_struct,
                             &oprec,
                             fragPtr.p,
                             tablePtr.p);
      ptr->m_header_bits |= Tuple_header::LCP_SKIP;
      updateChecksum(ptr, tablePtr.p, bits, ptr->m_header_bits);
    }
  }

  /**
   * A row is deleted as part of Copy fragment or Restore
   * We need to keep track of the row count also during restore.
   * We increment number of changed rows, for restore this variable
   * will be cleared after completing the restore, but it is
   * important to count it while performing a COPY fragment
   * operation.
   */
  fragPtr.p->m_row_count--;
  fragPtr.p->m_lcp_changed_rows++;

  DEB_DELETE_NR(("(%u)nr_delete, tab(%u,%u) row(%u,%u), gci: %u"
                 ", row_count: %llu",
                 instance(),
                 fragPtr.p->fragTableId,
                 fragPtr.p->fragmentId,
                 key->m_page_no,
                 key->m_page_idx,
                 *ptr->get_mm_gci(tablePtr.p),
                 fragPtr.p->m_row_count));

  if (tablePtr.p->m_attributes[MM].m_no_of_varsize +
      tablePtr.p->m_attributes[MM].m_no_of_dynamic)
  {
    jam();
    free_var_rec(fragPtr.p, tablePtr.p, &tmp, pagePtr);
  } else {
    jam();
    free_fix_rec(fragPtr.p, tablePtr.p, &tmp, (Fix_page*)pagePtr.p);
  }

  if (tablePtr.p->m_no_of_disk_attributes)
  {
    jam();

    Uint32 sz = (sizeof(Dbtup::Disk_undo::Free) >> 2) + 
      tablePtr.p->m_offsets[DD].m_fix_header_size - 1;

    Ptr<GlobalPage> diskPagePtr;
    int res;

    {
    D("Logfile_client - nr_delete");
    Logfile_client lgman(this, c_lgman, fragPtr.p->m_logfile_group_id);
    res = lgman.alloc_log_space(sz, false, false, jamBuffer());
    ndbrequire(res == 0);
    
    /**
     * 1) alloc log buffer
     * 2) get page
     * 3) get log buffer
     * 4) delete tuple
     */
    Page_cache_client::Request preq;
    preq.m_page = disk;
    preq.m_table_id = fragPtr.p->fragTableId;
    preq.m_fragment_id = fragPtr.p->fragmentId;
    preq.m_callback.m_callbackData = senderData;
    preq.m_callback.m_callbackFunction =
      safe_cast(&Dbtup::nr_delete_page_callback);
    int flags = Page_cache_client::COMMIT_REQ;
    
#ifdef ERROR_INSERT
    if (ERROR_INSERTED(4023) || ERROR_INSERTED(4024))
    {
      int rnd = rand() % 100;
      int slp = 0;
      if (ERROR_INSERTED(4024))
      {
	slp = 3000;
      }
      else if (rnd > 90)
      {
	slp = 3000;
      }
      else if (rnd > 70)
      {
	slp = 100;
      }
      
      ndbout_c("rnd: %d slp: %d", rnd, slp);
      
      if (slp)
      {
	flags |= Page_cache_client::DELAY_REQ;
        const NDB_TICKS now = NdbTick_getCurrentTicks();
        preq.m_delay_until_time = NdbTick_AddMilliseconds(now,(Uint64)slp);
      }
    }
#endif
    {
      Page_cache_client pgman(this, c_pgman);
      res = pgman.get_page(signal, preq, flags);
      diskPagePtr = pgman.m_ptr;
      if (res == 0)
      {
        goto timeslice;
      }
      else if (unlikely(res == -1))
      {
        return -1;
      }

      /* Complete work on LGMAN before setting page to dirty */
      CallbackPtr cptr;
      cptr.m_callbackIndex = NR_DELETE_LOG_BUFFER_CALLBACK;
      cptr.m_callbackData = senderData;
      res= lgman.get_log_buffer(signal, sz, &cptr);
    }
    } // Unlock the LGMAN lock

    PagePtr disk_page((Tup_page*)diskPagePtr.p, diskPagePtr.i);
    disk_page_set_dirty(disk_page);

    switch(res){
    case 0:
      signal->theData[2] = disk_page.i;
      goto timeslice;
    case -1:
      ndbrequire("NOT YET IMPLEMENTED" == 0);
      break;
    }
    disk_page_free(signal,
                   tablePtr.p,
                   fragPtr.p,
		   &disk,
                   *(PagePtr*)&disk_page,
                   gci,
                   key,
                   sz);
    return 0;
  }
  
  return 0;

timeslice:
  memcpy(signal->theData, &disk, sizeof(disk));
  return 1;
}

void
Dbtup::nr_delete_page_callback(Signal* signal, 
			       Uint32 userpointer, Uint32 page_id)//unused
{
  Ptr<GlobalPage> gpage;
  m_global_page_pool.getPtr(gpage, page_id);
  PagePtr pagePtr((Tup_page*)gpage.p, gpage.i);
  disk_page_set_dirty(pagePtr);
  Dblqh::Nr_op_info op;
  op.m_ptr_i = userpointer;
  op.m_disk_ref.m_page_no = pagePtr.p->m_page_no;
  op.m_disk_ref.m_file_no = pagePtr.p->m_file_no;
  c_lqh->get_nr_op_info(&op, page_id);

  Ptr<Fragrecord> fragPtr;
  fragPtr.i= op.m_tup_frag_ptr_i;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);

  Ptr<Tablerec> tablePtr;
  tablePtr.i = fragPtr.p->fragTableId;
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);
  
  Uint32 sz = (sizeof(Dbtup::Disk_undo::Free) >> 2) + 
    tablePtr.p->m_offsets[DD].m_fix_header_size - 1;
  
  CallbackPtr cb;
  cb.m_callbackData = userpointer;
  cb.m_callbackIndex = NR_DELETE_LOG_BUFFER_CALLBACK;
  int res;
  {
    D("Logfile_client - nr_delete_page_callback");
    Logfile_client lgman(this, c_lgman, fragPtr.p->m_logfile_group_id);
    res= lgman.get_log_buffer(signal, sz, &cb);
  }
  switch(res){
  case 0:
    jam();
    return;
  case -1:
    ndbrequire("NOT YET IMPLEMENTED" == 0);
    break;
  }
  jam();
  disk_page_free(signal,
                 tablePtr.p, fragPtr.p,
		 &op.m_disk_ref,
                 pagePtr,
                 op.m_gci_hi,
                 &op.m_row_id,
                 sz);
  
  c_lqh->nr_delete_complete(signal, &op);
  return;
}

void
Dbtup::nr_delete_log_buffer_callback(Signal* signal, 
				    Uint32 userpointer, 
				    Uint32 unused)
{
  Dblqh::Nr_op_info op;
  op.m_ptr_i = userpointer;
  c_lqh->get_nr_op_info(&op, RNIL);
  
  Ptr<Fragrecord> fragPtr;
  fragPtr.i= op.m_tup_frag_ptr_i;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);

  Ptr<Tablerec> tablePtr;
  tablePtr.i = fragPtr.p->fragTableId;
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);

  Uint32 sz = (sizeof(Dbtup::Disk_undo::Free) >> 2) + 
    tablePtr.p->m_offsets[DD].m_fix_header_size - 1;
  
  Ptr<GlobalPage> gpage;
  m_global_page_pool.getPtr(gpage, op.m_page_id);
  PagePtr pagePtr((Tup_page*)gpage.p, gpage.i);

  /**
   * reset page no
   */
  jam();
  disk_page_free(signal,
                 tablePtr.p,
                 fragPtr.p,
		 &op.m_disk_ref,
                 pagePtr,
                 op.m_gci_hi,
                 &op.m_row_id,
                 sz);
  
  c_lqh->nr_delete_complete(signal, &op);
}
