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
#define DBTUP_COMMIT_CPP
#include "Dbtup.hpp"
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <pc.hpp>
#include <signaldata/TupCommit.hpp>
#include "../dblqh/Dblqh.hpp"

void Dbtup::execTUP_DEALLOCREQ(Signal* signal)
{
  TablerecPtr regTabPtr;
  FragrecordPtr regFragPtr;
  Uint32 frag_page_id, frag_id;

  jamEntry();

  frag_id= signal->theData[0];
  regTabPtr.i= signal->theData[1];
  frag_page_id= signal->theData[2];
  Uint32 page_index= signal->theData[3];

  ptrCheckGuard(regTabPtr, cnoOfTablerec, tablerec);
  
  getFragmentrec(regFragPtr, frag_id, regTabPtr.p);
  ndbassert(regFragPtr.p != NULL);
  
  if (! (((frag_page_id << MAX_TUPLES_BITS) + page_index) == ~ (Uint32) 0))
  {
    Local_key tmp;
    tmp.m_page_no= getRealpid(regFragPtr.p, frag_page_id); 
    tmp.m_page_idx= page_index;
    
    PagePtr pagePtr;
    Tuple_header* ptr= (Tuple_header*)get_ptr(&pagePtr, &tmp, regTabPtr.p);

    ndbassert(ptr->m_header_bits & Tuple_header::FREE);

    if (ptr->m_header_bits & Tuple_header::LCP_KEEP)
    {
      ndbassert(! (ptr->m_header_bits & Tuple_header::FREED));
      ptr->m_header_bits |= Tuple_header::FREED;
      return;
    }
    
    if (regTabPtr.p->m_attributes[MM].m_no_of_varsize +
        regTabPtr.p->m_attributes[MM].m_no_of_dynamic)
    {
      jam();
      free_var_rec(regFragPtr.p, regTabPtr.p, &tmp, pagePtr);
    } else {
      free_fix_rec(regFragPtr.p, regTabPtr.p, &tmp, (Fix_page*)pagePtr.p);
    }
  }
}

void Dbtup::execTUP_WRITELOG_REQ(Signal* signal)
{
  jamEntry();
  OperationrecPtr loopOpPtr;
  loopOpPtr.i= signal->theData[0];
  Uint32 gci_hi = signal->theData[1];
  Uint32 gci_lo = signal->theData[2];
  c_operation_pool.getPtr(loopOpPtr);
  while (loopOpPtr.p->prevActiveOp != RNIL) {
    jam();
    loopOpPtr.i= loopOpPtr.p->prevActiveOp;
    c_operation_pool.getPtr(loopOpPtr);
  }
  do {
    ndbrequire(get_trans_state(loopOpPtr.p) == TRANS_STARTED);
    signal->theData[0] = loopOpPtr.p->userpointer;
    signal->theData[1] = gci_hi;
    signal->theData[2] = gci_lo;
    if (loopOpPtr.p->nextActiveOp == RNIL) {
      jam();
      EXECUTE_DIRECT(DBLQH, GSN_LQH_WRITELOG_REQ, signal, 3);
      return;
    }
    jam();
    EXECUTE_DIRECT(DBLQH, GSN_LQH_WRITELOG_REQ, signal, 3);
    jamEntry();
    loopOpPtr.i= loopOpPtr.p->nextActiveOp;
    c_operation_pool.getPtr(loopOpPtr);
  } while (true);
}

/* ---------------------------------------------------------------- */
/* INITIALIZATION OF ONE CONNECTION RECORD TO PREPARE FOR NEXT OP.  */
/* ---------------------------------------------------------------- */
void Dbtup::initOpConnection(Operationrec* regOperPtr)
{
  set_tuple_state(regOperPtr, TUPLE_ALREADY_ABORTED);
  set_trans_state(regOperPtr, TRANS_IDLE);
  regOperPtr->currentAttrinbufLen= 0;
  regOperPtr->op_struct.op_type= ZREAD;
  regOperPtr->op_struct.m_disk_preallocated= 0;
  regOperPtr->op_struct.m_load_diskpage_on_commit= 0;
  regOperPtr->op_struct.m_wait_log_buffer= 0;
  regOperPtr->op_struct.in_active_list = false;
  regOperPtr->m_undo_buffer_space= 0;
}

static
inline
bool
operator>(const Local_key& key1, const Local_key& key2)
{
  return key1.m_page_no > key2.m_page_no ||
    (key1.m_page_no == key2.m_page_no && key1.m_page_idx > key2.m_page_idx);
}

void
Dbtup::dealloc_tuple(Signal* signal,
		     Uint32 gci,
		     Page* page,
		     Tuple_header* ptr, 
		     Operationrec* regOperPtr, 
		     Fragrecord* regFragPtr, 
		     Tablerec* regTabPtr)
{
  Uint32 lcpScan_ptr_i= regFragPtr->m_lcp_scan_op;
  Uint32 lcp_keep_list = regFragPtr->m_lcp_keep_list;

  Uint32 bits = ptr->m_header_bits;
  Uint32 extra_bits = Tuple_header::FREED;
  if (bits & Tuple_header::DISK_PART)
  {
    jam();
    Local_key disk;
    memcpy(&disk, ptr->get_disk_ref_ptr(regTabPtr), sizeof(disk));
    PagePtr tmpptr;
    tmpptr.i = m_pgman.m_ptr.i;
    tmpptr.p = reinterpret_cast<Page*>(m_pgman.m_ptr.p);
    disk_page_free(signal, regTabPtr, regFragPtr, 
		   &disk, tmpptr, gci);
  }
  
  if (! (bits & (Tuple_header::LCP_SKIP | Tuple_header::ALLOC)) && 
      lcpScan_ptr_i != RNIL)
  {
    jam();
    ScanOpPtr scanOp;
    c_scanOpPool.getPtr(scanOp, lcpScan_ptr_i);
    Local_key rowid = regOperPtr->m_tuple_location;
    Local_key scanpos = scanOp.p->m_scanPos.m_key;
    rowid.m_page_no = page->frag_page_id;
    if (rowid > scanpos)
    {
      jam();
      extra_bits = Tuple_header::LCP_KEEP; // Note REMOVE FREE
      ptr->m_operation_ptr_i = lcp_keep_list;
      regFragPtr->m_lcp_keep_list = rowid.ref();
    }
  }
  
  ptr->m_header_bits = bits | extra_bits;
  
  if (regTabPtr->m_bits & Tablerec::TR_RowGCI)
  {
    jam();
    * ptr->get_mm_gci(regTabPtr) = gci;
  }
}

static void dump_buf_hex(unsigned char *p, Uint32 bytes)
{
  char buf[3001];
  char *q= buf;
  buf[0]= '\0';

  for(Uint32 i=0; i<bytes; i++)
  {
    if(i==((sizeof(buf)/3)-1))
    {
      sprintf(q, "...");
      break;
    }
    sprintf(q+3*i, " %02X", p[i]);
  }
  ndbout_c("%8p: %s", p, buf);
}
void
Dbtup::commit_operation(Signal* signal,
			Uint32 gci,
			Tuple_header* tuple_ptr, 
			PagePtr pagePtr,
			Operationrec* regOperPtr, 
			Fragrecord* regFragPtr, 
			Tablerec* regTabPtr)
{
  ndbassert(regOperPtr->op_struct.op_type != ZDELETE);
  
  Uint32 lcpScan_ptr_i= regFragPtr->m_lcp_scan_op;
  Uint32 save= tuple_ptr->m_operation_ptr_i;
  Uint32 bits= tuple_ptr->m_header_bits;

  Tuple_header *disk_ptr= 0;
  Tuple_header *copy= (Tuple_header*)
    c_undo_buffer.get_ptr(&regOperPtr->m_copy_tuple_location);
  
  Uint32 copy_bits= copy->m_header_bits;

  Uint32 fixsize= regTabPtr->m_offsets[MM].m_fix_header_size;
  Uint32 mm_vars= regTabPtr->m_attributes[MM].m_no_of_varsize;
  Uint32 mm_dyns= regTabPtr->m_attributes[MM].m_no_of_dynamic;
  if((mm_vars+mm_dyns) == 0)
  {
    jam();
    memcpy(tuple_ptr, copy, 4*fixsize);
    disk_ptr= (Tuple_header*)(((Uint32*)copy)+fixsize);
  }
  else
  {
    jam();
    /**
     * Var_part_ref is only stored in *allocated* tuple
     * so memcpy from copy, will over write it...
     * hence subtle copyout/assign...
     */
    Local_key tmp; 
    Var_part_ref *ref= tuple_ptr->get_var_part_ref_ptr(regTabPtr);
    ref->copyout(&tmp);

    memcpy(tuple_ptr, copy, 4*fixsize);
    ref->assign(&tmp);

    PagePtr vpagePtr;
    if (copy_bits & Tuple_header::VAR_PART)
    {
      jam();
      ndbassert(tmp.m_page_no != RNIL);
      ndbassert(bits & Tuple_header::VAR_PART);
      Uint32 *dst= get_ptr(&vpagePtr, *ref);
      Var_page* vpagePtrP = (Var_page*)vpagePtr.p;
      Varpart_copy*vp =(Varpart_copy*)copy->get_end_of_fix_part_ptr(regTabPtr);
      ndbassert(copy_bits & Tuple_header::COPY_TUPLE);
      /* The first word of shrunken tuple holds the lenght in words. */
      Uint32 len = vp->m_len;
      memcpy(dst, vp->m_data, 4*len);

      if(copy_bits & Tuple_header::MM_SHRINK)
      {
        jam();
        ndbassert(vpagePtrP->get_entry_len(tmp.m_page_idx) >= len);
        if (len)
        {
          jam();
          vpagePtrP->shrink_entry(tmp.m_page_idx, len);
        }
        else
        {
          jam();
          vpagePtrP->free_record(tmp.m_page_idx, Var_page::CHAIN);
          tmp.m_page_no = RNIL;
          ref->assign(&tmp);
          copy_bits &= ~(Uint32)Tuple_header::VAR_PART;
        }
        update_free_page_list(regFragPtr, vpagePtr);
      }
      else
      {
        jam();
        ndbassert(vpagePtrP->get_entry_len(tmp.m_page_idx) == len);
      }

      /**
       * Find disk part after
       * header + fixed MM part + length word + varsize part.
       */
      disk_ptr = (Tuple_header*)(vp->m_data + len);
    }
    else
    {
      jam();
      ndbassert(tmp.m_page_no == RNIL);
      disk_ptr = (Tuple_header*)copy->get_end_of_fix_part_ptr(regTabPtr);
    }
  }

  if (regTabPtr->m_no_of_disk_attributes &&
      (copy_bits & Tuple_header::DISK_INLINE))
  {
    jam();
    Local_key key;
    memcpy(&key, copy->get_disk_ref_ptr(regTabPtr), sizeof(Local_key));
    Uint32 logfile_group_id= regFragPtr->m_logfile_group_id;

    PagePtr diskPagePtr = *(PagePtr*)&m_pgman.m_ptr;
    ndbassert(diskPagePtr.p->m_page_no == key.m_page_no);
    ndbassert(diskPagePtr.p->m_file_no == key.m_file_no);
    Uint32 sz, *dst;
    if(copy_bits & Tuple_header::DISK_ALLOC)
    {
      jam();
      disk_page_alloc(signal, regTabPtr, regFragPtr, &key, diskPagePtr, gci);
    }
    
    if(regTabPtr->m_attributes[DD].m_no_of_varsize == 0)
    {
      jam();
      sz= regTabPtr->m_offsets[DD].m_fix_header_size;
      dst= ((Fix_page*)diskPagePtr.p)->get_ptr(key.m_page_idx, sz);
    }
    else
    {
      jam();
      dst= ((Var_page*)diskPagePtr.p)->get_ptr(key.m_page_idx);
      sz= ((Var_page*)diskPagePtr.p)->get_entry_len(key.m_page_idx);
    }
    
    if(! (copy_bits & Tuple_header::DISK_ALLOC))
    {
      jam();
      disk_page_undo_update(diskPagePtr.p, 
			    &key, dst, sz, gci, logfile_group_id);
    }
    
    memcpy(dst, disk_ptr, 4*sz);
    memcpy(tuple_ptr->get_disk_ref_ptr(regTabPtr), &key, sizeof(Local_key));
    
    ndbassert(! (disk_ptr->m_header_bits & Tuple_header::FREE));
    copy_bits |= Tuple_header::DISK_PART;
  }
  
  if(lcpScan_ptr_i != RNIL && (bits & Tuple_header::ALLOC))
  {
    jam();
    ScanOpPtr scanOp;
    c_scanOpPool.getPtr(scanOp, lcpScan_ptr_i);
    Local_key rowid = regOperPtr->m_tuple_location;
    Local_key scanpos = scanOp.p->m_scanPos.m_key;
    rowid.m_page_no = pagePtr.p->frag_page_id;
    if(rowid > scanpos)
    {
      jam();
       copy_bits |= Tuple_header::LCP_SKIP;
    }
  }
  
  Uint32 clear= 
    Tuple_header::ALLOC | Tuple_header::FREE | Tuple_header::COPY_TUPLE |
    Tuple_header::DISK_ALLOC | Tuple_header::DISK_INLINE | 
    Tuple_header::MM_SHRINK | Tuple_header::MM_GROWN;
  copy_bits &= ~(Uint32)clear;
  
  tuple_ptr->m_header_bits= copy_bits;
  tuple_ptr->m_operation_ptr_i= save;
  
  if (regTabPtr->m_bits & Tablerec::TR_RowGCI)
  {
    jam();
    * tuple_ptr->get_mm_gci(regTabPtr) = gci;
  }
  
  if (regTabPtr->m_bits & Tablerec::TR_Checksum) {
    jam();
    setChecksum(tuple_ptr, regTabPtr);
  }
}

void
Dbtup::disk_page_commit_callback(Signal* signal, 
				 Uint32 opPtrI, Uint32 page_id)
{
  Uint32 hash_value;
  Uint32 gci_hi, gci_lo;
  OperationrecPtr regOperPtr;

  jamEntry();
  
  c_operation_pool.getPtr(regOperPtr, opPtrI);
  c_lqh->get_op_info(regOperPtr.p->userpointer, &hash_value, &gci_hi, &gci_lo);

  TupCommitReq * const tupCommitReq= (TupCommitReq *)signal->getDataPtr();
  
  tupCommitReq->opPtr= opPtrI;
  tupCommitReq->hashValue= hash_value;
  tupCommitReq->gci_hi= gci_hi;
  tupCommitReq->gci_lo= gci_lo;
  tupCommitReq->diskpage = page_id;

  regOperPtr.p->op_struct.m_load_diskpage_on_commit= 0;
  regOperPtr.p->m_commit_disk_callback_page= page_id;
  m_global_page_pool.getPtr(m_pgman.m_ptr, page_id);
  
  {
    PagePtr tmp;
    tmp.i = m_pgman.m_ptr.i;
    tmp.p = reinterpret_cast<Page*>(m_pgman.m_ptr.p);
    disk_page_set_dirty(tmp);
  }
  
  execTUP_COMMITREQ(signal);
  if(signal->theData[0] == 0)
  {
    jam();
    c_lqh->tupcommit_conf_callback(signal, regOperPtr.p->userpointer);
  }
}

void
Dbtup::disk_page_log_buffer_callback(Signal* signal, 
				     Uint32 opPtrI,
				     Uint32 unused)
{
  Uint32 hash_value;
  Uint32 gci_hi, gci_lo;
  OperationrecPtr regOperPtr;

  jamEntry();
  
  c_operation_pool.getPtr(regOperPtr, opPtrI);
  c_lqh->get_op_info(regOperPtr.p->userpointer, &hash_value, &gci_hi, &gci_lo);
  Uint32 page= regOperPtr.p->m_commit_disk_callback_page;

  TupCommitReq * const tupCommitReq= (TupCommitReq *)signal->getDataPtr();
  
  tupCommitReq->opPtr= opPtrI;
  tupCommitReq->hashValue= hash_value;
  tupCommitReq->gci_hi= gci_hi;
  tupCommitReq->gci_lo= gci_lo;
  tupCommitReq->diskpage = page;

  ndbassert(regOperPtr.p->op_struct.m_load_diskpage_on_commit == 0);
  regOperPtr.p->op_struct.m_wait_log_buffer= 0;
  m_global_page_pool.getPtr(m_pgman.m_ptr, page);
  
  execTUP_COMMITREQ(signal);
  ndbassert(signal->theData[0] == 0);
  
  c_lqh->tupcommit_conf_callback(signal, regOperPtr.p->userpointer);
}

/**
 * Move to the first operation performed on this tuple
 */
void
Dbtup::findFirstOp(OperationrecPtr & firstPtr)
{
  jam();
  printf("Detect out-of-order commit(%u) -> ", firstPtr.i);
  ndbassert(!firstPtr.p->is_first_operation());
  while(firstPtr.p->prevActiveOp != RNIL)
  {
    firstPtr.i = firstPtr.p->prevActiveOp;
    c_operation_pool.getPtr(firstPtr);    
  }
  ndbout_c("%u", firstPtr.i);
}

/* ----------------------------------------------------------------- */
/* --------------- COMMIT THIS PART OF A TRANSACTION --------------- */
/* ----------------------------------------------------------------- */
void Dbtup::execTUP_COMMITREQ(Signal* signal) 
{
  FragrecordPtr regFragPtr;
  OperationrecPtr regOperPtr;
  TablerecPtr regTabPtr;
  KeyReqStruct req_struct;
  TransState trans_state;
  Uint32 no_of_fragrec, no_of_tablerec;

  TupCommitReq * const tupCommitReq= (TupCommitReq *)signal->getDataPtr();

  regOperPtr.i= tupCommitReq->opPtr;
  Uint32 hash_value= tupCommitReq->hashValue;
  Uint32 gci_hi = tupCommitReq->gci_hi;
  Uint32 gci_lo = tupCommitReq->gci_lo;

  jamEntry();

  c_operation_pool.getPtr(regOperPtr);
  
  regFragPtr.i= regOperPtr.p->fragmentPtr;
  trans_state= get_trans_state(regOperPtr.p);

  no_of_fragrec= cnoOfFragrec;

  ndbrequire(trans_state == TRANS_STARTED);
  ptrCheckGuard(regFragPtr, no_of_fragrec, fragrecord);

  no_of_tablerec= cnoOfTablerec;
  regTabPtr.i= regFragPtr.p->fragTableId;

  req_struct.signal= signal;
  req_struct.hash_value= hash_value;
  req_struct.gci_hi = gci_hi;
  req_struct.gci_lo = gci_lo;
  regOperPtr.p->m_commit_disk_callback_page = tupCommitReq->diskpage;

#ifdef VM_TRACE
  if (tupCommitReq->diskpage == RNIL)
  {
    m_pgman.m_ptr.i = RNIL;
    m_pgman.m_ptr.p = 0;
    req_struct.m_disk_page_ptr.i = RNIL;
    req_struct.m_disk_page_ptr.p = 0;
  }
#endif
  
  ptrCheckGuard(regTabPtr, no_of_tablerec, tablerec);

  PagePtr page;
  Tuple_header* tuple_ptr= (Tuple_header*)
    get_ptr(&page, &regOperPtr.p->m_tuple_location, regTabPtr.p);

  /**
   * NOTE: This has to be run before potential time-slice when
   *       waiting for disk, as otherwise the "other-ops" in a multi-op
   *       commit might run while we're waiting for disk
   *
   */
  if (!regTabPtr.p->tuxCustomTriggers.isEmpty())
  {
    if(get_tuple_state(regOperPtr.p) == TUPLE_PREPARED)
    {
      jam();

      OperationrecPtr loopPtr = regOperPtr;
      if (unlikely(!regOperPtr.p->is_first_operation()))
      {
        findFirstOp(loopPtr);
      }

      /**
       * Execute all tux triggers at first commit
       *   since previous tuple is otherwise removed...
       */
      jam();
      goto first;
      while(loopPtr.i != RNIL)
      {
	c_operation_pool.getPtr(loopPtr);
    first:
	executeTuxCommitTriggers(signal,
				 loopPtr.p,
				 regFragPtr.p,
				 regTabPtr.p);
	set_tuple_state(loopPtr.p, TUPLE_TO_BE_COMMITTED);
	loopPtr.i = loopPtr.p->nextActiveOp;
      }
    }
  }
  
  bool get_page = false;
  if(regOperPtr.p->op_struct.m_load_diskpage_on_commit)
  {
    jam();
    Page_cache_client::Request req;

    /**
     * Only last op on tuple needs "real" commit,
     *   hence only this one should have m_load_diskpage_on_commit
     */
    ndbassert(tuple_ptr->m_operation_ptr_i == regOperPtr.i);

    /**
     * Check for page
     */
    if(!regOperPtr.p->m_copy_tuple_location.isNull())
    {
      jam();
      Tuple_header* tmp= (Tuple_header*)
	c_undo_buffer.get_ptr(&regOperPtr.p->m_copy_tuple_location);
      
      memcpy(&req.m_page, 
	     tmp->get_disk_ref_ptr(regTabPtr.p), sizeof(Local_key));

      if (unlikely(regOperPtr.p->op_struct.op_type == ZDELETE &&
		   tmp->m_header_bits & Tuple_header::DISK_ALLOC))
      {
        jam();
	/**
	 * Insert+Delete
	 */
        regOperPtr.p->op_struct.m_load_diskpage_on_commit = 0;
        regOperPtr.p->op_struct.m_wait_log_buffer = 0;	
        disk_page_abort_prealloc(signal, regFragPtr.p, 
				 &req.m_page, req.m_page.m_page_idx);
        
        c_lgman->free_log_space(regFragPtr.p->m_logfile_group_id, 
				regOperPtr.p->m_undo_buffer_space);
	goto skip_disk;
        if (0) ndbout_c("insert+delete");
        jamEntry();
        goto skip_disk;
      }
    } 
    else
    {
      jam();
      // initial delete
      ndbassert(regOperPtr.p->op_struct.op_type == ZDELETE);
      memcpy(&req.m_page, 
	     tuple_ptr->get_disk_ref_ptr(regTabPtr.p), sizeof(Local_key));
      
      ndbassert(tuple_ptr->m_header_bits & Tuple_header::DISK_PART);
    }
    req.m_callback.m_callbackData= regOperPtr.i;
    req.m_callback.m_callbackFunction = 
      safe_cast(&Dbtup::disk_page_commit_callback);

    /*
     * Consider commit to be correlated.  Otherwise pk op + commit makes
     * the page hot.   XXX move to TUP which knows better.
     */
    int flags= regOperPtr.p->op_struct.op_type |
      Page_cache_client::COMMIT_REQ | Page_cache_client::CORR_REQ;
    int res= m_pgman.get_page(signal, req, flags);
    switch(res){
    case 0:
      /**
       * Timeslice
       */
      jam();
      signal->theData[0] = 1;
      return;
    case -1:
      ndbrequire("NOT YET IMPLEMENTED" == 0);
      break;
    default:
      jam();
    }
    get_page = true;

    {
      PagePtr tmpptr;
      tmpptr.i = m_pgman.m_ptr.i;
      tmpptr.p = reinterpret_cast<Page*>(m_pgman.m_ptr.p);
      disk_page_set_dirty(tmpptr);
    }
    
    regOperPtr.p->m_commit_disk_callback_page= res;
    regOperPtr.p->op_struct.m_load_diskpage_on_commit= 0;
  } 
  
  if(regOperPtr.p->op_struct.m_wait_log_buffer)
  {
    jam();
    /**
     * Only last op on tuple needs "real" commit,
     *   hence only this one should have m_wait_log_buffer
     */
    ndbassert(tuple_ptr->m_operation_ptr_i == regOperPtr.i);
    
    Callback cb;
    cb.m_callbackData= regOperPtr.i;
    cb.m_callbackFunction = 
      safe_cast(&Dbtup::disk_page_log_buffer_callback);
    Uint32 sz= regOperPtr.p->m_undo_buffer_space;
    
    Logfile_client lgman(this, c_lgman, regFragPtr.p->m_logfile_group_id);
    int res= lgman.get_log_buffer(signal, sz, &cb);
    jamEntry();
    switch(res){
    case 0:
      jam();
      signal->theData[0] = 1;
      return;
    case -1:
      ndbrequire("NOT YET IMPLEMENTED" == 0);
      break;
    default:
      jam();
    }
  }
  
  assert(tuple_ptr);
skip_disk:
  req_struct.m_tuple_ptr = tuple_ptr;
  
  Uint32 nextOp = regOperPtr.p->nextActiveOp;
  Uint32 prevOp = regOperPtr.p->prevActiveOp;
  /**
   * The trigger code (which is shared between detached/imediate)
   *   check op-list to check were to read before values from
   *   detached triggers should always read from original tuple value
   *   from before transaction start, not from any intermediate update
   *
   * Setting the op-list has this effect
   */
  regOperPtr.p->nextActiveOp = RNIL;
  regOperPtr.p->prevActiveOp = RNIL;
  if(tuple_ptr->m_operation_ptr_i == regOperPtr.i)
  {
    jam();
    /**
     * Perform "real" commit
     */
    Uint32 disk = regOperPtr.p->m_commit_disk_callback_page;
    set_change_mask_info(&req_struct, regOperPtr.p);
    checkDetachedTriggers(&req_struct, regOperPtr.p, regTabPtr.p, 
                          disk != RNIL);
    
    tuple_ptr->m_operation_ptr_i = RNIL;
    
    if(regOperPtr.p->op_struct.op_type != ZDELETE)
    {
      jam();
      commit_operation(signal, gci_hi, tuple_ptr, page,
		       regOperPtr.p, regFragPtr.p, regTabPtr.p); 
    }
    else
    {
      jam();
      if (get_page)
	ndbassert(tuple_ptr->m_header_bits & Tuple_header::DISK_PART);
      dealloc_tuple(signal, gci_hi, page.p, tuple_ptr,
		    regOperPtr.p, regFragPtr.p, regTabPtr.p); 
    }
  } 

  if (nextOp != RNIL)
  {
    c_operation_pool.getPtr(nextOp)->prevActiveOp = prevOp;
  }
  
  if (prevOp != RNIL)
  {
    c_operation_pool.getPtr(prevOp)->nextActiveOp = nextOp;
  }
  
  if(!regOperPtr.p->m_copy_tuple_location.isNull())
  {
    jam();
    c_undo_buffer.free_copy_tuple(&regOperPtr.p->m_copy_tuple_location);
  }
  
  initOpConnection(regOperPtr.p);
  signal->theData[0] = 0;
}

void
Dbtup::set_change_mask_info(KeyReqStruct * const req_struct,
                            Operationrec * const regOperPtr)
{
  ChangeMaskState state = get_change_mask_state(regOperPtr);
  if (state == USE_SAVED_CHANGE_MASK) {
    jam();
    req_struct->changeMask.setWord(0, regOperPtr->saved_change_mask[0]);
    req_struct->changeMask.setWord(1, regOperPtr->saved_change_mask[1]);
  } else if (state == RECALCULATE_CHANGE_MASK) {
    jam();
    // Recompute change mask, for now set all bits
    req_struct->changeMask.set();
  } else if (state == SET_ALL_MASK) {
    jam();
    req_struct->changeMask.set();
  } else {
    jam();
    ndbrequire(state == DELETE_CHANGES);
    req_struct->changeMask.set();
  }
}

void
Dbtup::calculateChangeMask(Page* const pagePtr,
                           Tablerec* const regTabPtr,
                           KeyReqStruct * const req_struct)
{
  OperationrecPtr loopOpPtr;
  Uint32 saved_word1= 0;
  Uint32 saved_word2= 0;
  loopOpPtr.i= req_struct->m_tuple_ptr->m_operation_ptr_i;
  do {
    c_operation_pool.getPtr(loopOpPtr);
    ndbrequire(loopOpPtr.p->op_struct.op_type == ZUPDATE);
    ChangeMaskState change_mask= get_change_mask_state(loopOpPtr.p);
    if (change_mask == USE_SAVED_CHANGE_MASK) {
      jam();
      saved_word1|= loopOpPtr.p->saved_change_mask[0];
      saved_word2|= loopOpPtr.p->saved_change_mask[1];
    } else if (change_mask == RECALCULATE_CHANGE_MASK) {
      jam();
      //Recompute change mask, for now set all bits
      req_struct->changeMask.set();
      return;
    } else {
      ndbrequire(change_mask == SET_ALL_MASK);
      jam();
      req_struct->changeMask.set();
      return;
    }
    loopOpPtr.i= loopOpPtr.p->prevActiveOp;
  } while (loopOpPtr.i != RNIL);
  req_struct->changeMask.setWord(0, saved_word1);
  req_struct->changeMask.setWord(1, saved_word2);
}
