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
#include <signaldata/TupCommit.hpp>
#include "../dblqh/Dblqh.hpp"

#define ljam() { jamLine(5000 + __LINE__); }
#define ljamEntry() { jamEntryLine(5000 + __LINE__); }

void Dbtup::execTUP_DEALLOCREQ(Signal* signal)
{
  TablerecPtr regTabPtr;
  FragrecordPtr regFragPtr;
  Uint32 frag_page_id, frag_id;

  ljamEntry();

  frag_id= signal->theData[0];
  regTabPtr.i= signal->theData[1];
  frag_page_id= signal->theData[2];
  Uint32 page_index= signal->theData[3];

  ptrCheckGuard(regTabPtr, cnoOfTablerec, tablerec);
  
  getFragmentrec(regFragPtr, frag_id, regTabPtr.p);
  ndbassert(regFragPtr.p != NULL);
  
  if (! (((frag_page_id << MAX_TUPLES_BITS) + page_index) == ~0))
  {
    Local_key tmp;
    tmp.m_page_no= getRealpid(regFragPtr.p, frag_page_id); 
    tmp.m_page_idx= page_index;
    
    PagePtr pagePtr;
    Tuple_header* ptr= (Tuple_header*)get_ptr(&pagePtr, &tmp, regTabPtr.p);
    
    if (regTabPtr.p->m_attributes[MM].m_no_of_varsize)
    {
      ljam();
      
      if(ptr->m_header_bits & Tuple_header::CHAINED_ROW)
      {
	free_var_part(regFragPtr.p, regTabPtr.p,
		      *(Var_part_ref*)ptr->get_var_part_ptr(regTabPtr.p),
		      Var_page::CHAIN);
      }
      free_var_part(regFragPtr.p, regTabPtr.p, &tmp, (Var_page*)pagePtr.p, 0);
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
  Uint32 gci= signal->theData[1];
  c_operation_pool.getPtr(loopOpPtr);
  while (loopOpPtr.p->prevActiveOp != RNIL) {
    ljam();
    loopOpPtr.i= loopOpPtr.p->prevActiveOp;
    c_operation_pool.getPtr(loopOpPtr);
  }
  do {
    ndbrequire(get_trans_state(loopOpPtr.p) == TRANS_STARTED);
    signal->theData[0]= loopOpPtr.p->userpointer;
    signal->theData[1]= gci;
    if (loopOpPtr.p->nextActiveOp == RNIL) {
      ljam();
      EXECUTE_DIRECT(DBLQH, GSN_LQH_WRITELOG_REQ, signal, 2);
      return;
    }
    ljam();
    EXECUTE_DIRECT(DBLQH, GSN_LQH_WRITELOG_REQ, signal, 2);
    jamEntry();
    loopOpPtr.i= loopOpPtr.p->nextActiveOp;
    c_operation_pool.getPtr(loopOpPtr);
  } while (true);
}

void Dbtup::removeActiveOpList(Operationrec*  const regOperPtr,
                               Tuple_header *tuple_ptr)
{
  OperationrecPtr raoOperPtr;

  /**
   * Release copy tuple
   */
  if(regOperPtr->op_struct.op_type != ZDELETE && 
     !regOperPtr->m_copy_tuple_location.isNull())
    c_undo_buffer.free_copy_tuple(&regOperPtr->m_copy_tuple_location);
  
  if (regOperPtr->op_struct.in_active_list) {
    regOperPtr->op_struct.in_active_list= false;
    if (regOperPtr->nextActiveOp != RNIL) {
      ljam();
      raoOperPtr.i= regOperPtr->nextActiveOp;
      c_operation_pool.getPtr(raoOperPtr);
      raoOperPtr.p->prevActiveOp= regOperPtr->prevActiveOp;
    } else {
      ljam();
      tuple_ptr->m_operation_ptr_i = regOperPtr->prevActiveOp;
    }
    if (regOperPtr->prevActiveOp != RNIL) {
      ljam();
      raoOperPtr.i= regOperPtr->prevActiveOp;
      c_operation_pool.getPtr(raoOperPtr);
      raoOperPtr.p->nextActiveOp= regOperPtr->nextActiveOp;
    }
    regOperPtr->prevActiveOp= RNIL;
    regOperPtr->nextActiveOp= RNIL;
  }
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
  regOperPtr->m_undo_buffer_space= 0;
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
  if (ptr->m_header_bits & Tuple_header::DISK_PART)
  {
    Local_key disk;
    memcpy(&disk, ptr->get_disk_ref_ptr(regTabPtr), sizeof(disk));
    Ptr<GlobalPage> disk_page;
    m_global_page_pool.getPtr(disk_page, 
			      regOperPtr->m_commit_disk_callback_page);
    disk_page_free(signal, regTabPtr, regFragPtr, 
		   &disk, *(PagePtr*)&disk_page, gci);
  }
}

static
inline
bool
operator>=(const Local_key& key1, const Local_key& key2)
{
  return key1.m_page_no > key2.m_page_no ||
    (key1.m_page_no == key2.m_page_no && key1.m_page_idx >= key2.m_page_idx);
}

void
Dbtup::commit_operation(Signal* signal,
			Uint32 gci,
			Tuple_header* tuple_ptr, 
			Page* page,
			Operationrec* regOperPtr, 
			Fragrecord* regFragPtr, 
			Tablerec* regTabPtr)
{
  ndbassert(regOperPtr->op_struct.op_type != ZDELETE);
  
  Uint32 save= tuple_ptr->m_operation_ptr_i;
  Uint32 bits= tuple_ptr->m_header_bits;

  Tuple_header *disk_ptr= 0;
  Tuple_header *copy= (Tuple_header*)
    c_undo_buffer.get_ptr(&regOperPtr->m_copy_tuple_location);
  
  Uint32 copy_bits= copy->m_header_bits;

  Uint32 fix_size= regTabPtr->m_offsets[MM].m_fix_header_size;
  Uint32 mm_vars= regTabPtr->m_attributes[MM].m_no_of_varsize;
  if(mm_vars == 0)
  {
    memcpy(tuple_ptr, copy, 4*fix_size);
    //ndbout_c("commit: memcpy %p %p %d", tuple_ptr, copy, 4*fix_size);
    disk_ptr= (Tuple_header*)(((Uint32*)copy)+fix_size);
  }
  else if(bits & Tuple_header::CHAINED_ROW)
  {
    Uint32 *ref= tuple_ptr->get_var_part_ptr(regTabPtr);
    memcpy(tuple_ptr, copy, 4*(Tuple_header::HeaderSize+fix_size));

    Local_key tmp; tmp.assref(*ref);
    if(0) printf("%p %d %d (%d bytes) - ref: %x ", tuple_ptr,
	   regOperPtr->m_tuple_location.m_page_no,
	   regOperPtr->m_tuple_location.m_page_idx,
	   4*(Tuple_header::HeaderSize+fix_size),
	   *ref);
    Ptr<Var_page> vpagePtr;
    Uint32 *dst= get_ptr(&vpagePtr, *(Var_part_ref*)ref);
    Uint32 *src= copy->get_var_part_ptr(regTabPtr);
    Uint32 sz= ((mm_vars + 1) << 1) + (((Uint16*)src)[mm_vars]);
    ndbassert(4*vpagePtr.p->get_entry_len(tmp.m_page_idx) >= sz);
    memcpy(dst, src, sz);
    if(0) printf("ptr: %p %d ref: %x - chain commit", dst, sz, *ref);
    copy_bits |= Tuple_header::CHAINED_ROW;
    
    if(0)
    {
      for(Uint32 i = 0; i<((sz+3)>>2); i++)
	printf(" %.8x", src[i]);
      printf("\n");
    }
    
    if(copy_bits & Tuple_header::MM_SHRINK)
    {
      if(0) printf(" - shrink %d -> %d - ", 
	     vpagePtr.p->get_entry_len(tmp.m_page_idx), (sz + 3) >> 2);
      vpagePtr.p->shrink_entry(tmp.m_page_idx, (sz + 3) >> 2);
      if(0)ndbout_c("%p->shrink_entry(%d, %d)", vpagePtr.p, tmp.m_page_idx, 
	       (sz + 3) >> 2);
      update_free_page_list(regFragPtr, vpagePtr.p);
    } 
    if(0) ndbout_c("");
    disk_ptr = (Tuple_header*)
      (((Uint32*)copy)+Tuple_header::HeaderSize+fix_size+((sz + 3) >> 2));
  } 
  else 
  {
    Uint32 *var_part= copy->get_var_part_ptr(regTabPtr);
    Uint32 sz= Tuple_header::HeaderSize + fix_size +
      ((((mm_vars + 1) << 1) + (((Uint16*)var_part)[mm_vars]) + 3)>> 2);
    ndbassert(((Var_page*)page)->
	      get_entry_len(regOperPtr->m_tuple_location.m_page_idx) >= sz);
    memcpy(tuple_ptr, copy, 4*sz);      
    if(0) ndbout_c("%p %d %d (%d bytes)", tuple_ptr,
	     regOperPtr->m_tuple_location.m_page_no,
	     regOperPtr->m_tuple_location.m_page_idx,
	     4*sz);
    if(copy_bits & Tuple_header::MM_SHRINK)
    {
      ((Var_page*)page)->shrink_entry(regOperPtr->m_tuple_location.m_page_idx, 
				      sz);
      if(0)ndbout_c("%p->shrink_entry(%d, %d)", 
	       page, regOperPtr->m_tuple_location.m_page_idx, sz);
      update_free_page_list(regFragPtr, (Var_page*)page);
    } 
    disk_ptr = (Tuple_header*)(((Uint32*)copy)+sz);
  }
  
  if (regTabPtr->m_no_of_disk_attributes &&
      (copy_bits & Tuple_header::DISK_INLINE))
  {
    Local_key key;
    memcpy(&key, copy->get_disk_ref_ptr(regTabPtr), sizeof(Local_key));
    Uint32 logfile_group_id= regFragPtr->m_logfile_group_id;
    Uint32 lcpScan_ptr_i= regFragPtr->m_lcp_scan_op;

    PagePtr pagePtr = *(PagePtr*)&m_pgman.m_ptr;
    ndbassert(pagePtr.p->m_page_no == key.m_page_no);
    ndbassert(pagePtr.p->m_file_no == key.m_file_no);
    Uint32 sz, *dst;
    if(copy_bits & Tuple_header::DISK_ALLOC)
    {
      disk_page_alloc(signal, regTabPtr, regFragPtr, &key, pagePtr, gci);

      if(lcpScan_ptr_i != RNIL)
      {
	ScanOpPtr scanOp;
	c_scanOpPool.getPtr(scanOp, lcpScan_ptr_i);
	Local_key rowid = regOperPtr->m_tuple_location;
	Local_key scanpos = scanOp.p->m_scanPos.m_key;
	rowid.m_page_no = pagePtr.p->frag_page_id;
	if(rowid >= scanpos)
	{
	  copy_bits |= Tuple_header::LCP_SKIP;
	}
      }
    }
    
    if(regTabPtr->m_attributes[DD].m_no_of_varsize == 0)
    {
      sz= regTabPtr->m_offsets[DD].m_fix_header_size;
      dst= ((Fix_page*)pagePtr.p)->get_ptr(key.m_page_idx, sz);
    }
    else
    {
      dst= ((Var_page*)pagePtr.p)->get_ptr(key.m_page_idx);
      sz= ((Var_page*)pagePtr.p)->get_entry_len(key.m_page_idx);
    }
    
    if(! (copy_bits & Tuple_header::DISK_ALLOC))
    {
      disk_page_undo_update(pagePtr.p, &key, dst, sz, gci, logfile_group_id);
    }
    
    memcpy(dst, disk_ptr, 4*sz);
    memcpy(tuple_ptr->get_disk_ref_ptr(regTabPtr), &key, sizeof(Local_key));
    
    ndbassert(! (disk_ptr->m_header_bits & Tuple_header::FREE));
    copy_bits |= Tuple_header::DISK_PART;
  }

  
  Uint32 clear= 
    Tuple_header::ALLOC |
    Tuple_header::DISK_ALLOC | Tuple_header::DISK_INLINE | 
    Tuple_header::MM_SHRINK | Tuple_header::MM_GROWN;
  copy_bits &= ~(Uint32)clear;
  
  tuple_ptr->m_header_bits= copy_bits;
  tuple_ptr->m_operation_ptr_i= save;
  
  if (regTabPtr->checksumIndicator) {
    jam();
    setChecksum(tuple_ptr, regTabPtr);
  }
}

void
Dbtup::disk_page_commit_callback(Signal* signal, 
				 Uint32 opPtrI, Uint32 page_id)
{
  Uint32 hash_value;
  Uint32 gci;
  OperationrecPtr regOperPtr;

  ljamEntry();
  
  c_operation_pool.getPtr(regOperPtr, opPtrI);
  c_lqh->get_op_info(regOperPtr.p->userpointer, &hash_value, &gci);

  TupCommitReq * const tupCommitReq= (TupCommitReq *)signal->getDataPtr();
  
  tupCommitReq->opPtr= opPtrI;
  tupCommitReq->hashValue= hash_value;
  tupCommitReq->gci= gci;

  regOperPtr.p->op_struct.m_load_diskpage_on_commit= 0;
  regOperPtr.p->m_commit_disk_callback_page= page_id;
  m_global_page_pool.getPtr(m_pgman.m_ptr, page_id);
  
  execTUP_COMMITREQ(signal);
  if(signal->theData[0] == 0)
    c_lqh->tupcommit_conf_callback(signal, regOperPtr.p->userpointer);
}

void
Dbtup::disk_page_log_buffer_callback(Signal* signal, 
				     Uint32 opPtrI,
				     Uint32 unused)
{
  Uint32 hash_value;
  Uint32 gci;
  OperationrecPtr regOperPtr;

  ljamEntry();
  
  c_operation_pool.getPtr(regOperPtr, opPtrI);
  c_lqh->get_op_info(regOperPtr.p->userpointer, &hash_value, &gci);

  TupCommitReq * const tupCommitReq= (TupCommitReq *)signal->getDataPtr();
  
  tupCommitReq->opPtr= opPtrI;
  tupCommitReq->hashValue= hash_value;
  tupCommitReq->gci= gci;

  Uint32 page= regOperPtr.p->m_commit_disk_callback_page;
  ndbassert(regOperPtr.p->op_struct.m_load_diskpage_on_commit == 0);
  regOperPtr.p->op_struct.m_wait_log_buffer= 0;
  m_global_page_pool.getPtr(m_pgman.m_ptr, page);
  
  execTUP_COMMITREQ(signal);
  ndbassert(signal->theData[0] == 0);
  
  c_lqh->tupcommit_conf_callback(signal, regOperPtr.p->userpointer);
}

void
Dbtup::fix_commit_order(OperationrecPtr opPtr)
{
  ndbassert(!opPtr.p->is_first_operation());
  OperationrecPtr firstPtr = opPtr;
  while(firstPtr.p->prevActiveOp != RNIL)
  {
    firstPtr.i = firstPtr.p->prevActiveOp;
    c_operation_pool.getPtr(firstPtr);    
  }

  ndbout_c("fix_commit_order (swapping %d and %d)",
	   opPtr.i, firstPtr.i);
  
  /**
   * Swap data between first and curr
   */
  Uint32 prev= opPtr.p->prevActiveOp;
  Uint32 next= opPtr.p->nextActiveOp;
  Uint32 seco= firstPtr.p->nextActiveOp;

  Operationrec tmp = *opPtr.p;
  * opPtr.p = * firstPtr.p;
  * firstPtr.p = tmp;

  c_operation_pool.getPtr(seco)->prevActiveOp = opPtr.i;
  c_operation_pool.getPtr(prev)->nextActiveOp = firstPtr.i;
  if(next != RNIL)
    c_operation_pool.getPtr(next)->prevActiveOp = firstPtr.i;
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
  Uint32 no_of_fragrec, no_of_tablerec, hash_value, gci;

  TupCommitReq * const tupCommitReq= (TupCommitReq *)signal->getDataPtr();

  regOperPtr.i= tupCommitReq->opPtr;
  ljamEntry();

  c_operation_pool.getPtr(regOperPtr);
  if(!regOperPtr.p->is_first_operation())
  {
    /**
     * Out of order commit 
     */
    fix_commit_order(regOperPtr);
  }
  ndbassert(regOperPtr.p->is_first_operation());
  
  regFragPtr.i= regOperPtr.p->fragmentPtr;
  trans_state= get_trans_state(regOperPtr.p);

  no_of_fragrec= cnoOfFragrec;

  ndbrequire(trans_state == TRANS_STARTED);
  ptrCheckGuard(regFragPtr, no_of_fragrec, fragrecord);

  no_of_tablerec= cnoOfTablerec;
  regTabPtr.i= regFragPtr.p->fragTableId;
  hash_value= tupCommitReq->hashValue;
  gci= tupCommitReq->gci;

  req_struct.signal= signal;
  req_struct.hash_value= hash_value;
  req_struct.gci= gci;

  ptrCheckGuard(regTabPtr, no_of_tablerec, tablerec);

  PagePtr page;
  Tuple_header* tuple_ptr= 0;
  if(regOperPtr.p->op_struct.m_load_diskpage_on_commit)
  {
    ndbassert(regOperPtr.p->is_first_operation() && 
	      regOperPtr.p->is_last_operation());

    Page_cache_client::Request req;
    /**
     * Check for page
     */
    if(!regOperPtr.p->m_copy_tuple_location.isNull())
    {
      Tuple_header* tmp= (Tuple_header*)
	c_undo_buffer.get_ptr(&regOperPtr.p->m_copy_tuple_location);
      
      memcpy(&req.m_page, 
	     tmp->get_disk_ref_ptr(regTabPtr.p), sizeof(Local_key));
    } 
    else
    {
      // initial delete
      ndbassert(regOperPtr.p->op_struct.op_type == ZDELETE);
      tuple_ptr= (Tuple_header*)
	get_ptr(&page, &regOperPtr.p->m_tuple_location, regTabPtr.p);
      memcpy(&req.m_page, 
	     tuple_ptr->get_disk_ref_ptr(regTabPtr.p), sizeof(Local_key));
    }
    req.m_callback.m_callbackData= regOperPtr.i;
    req.m_callback.m_callbackFunction = 
      safe_cast(&Dbtup::disk_page_commit_callback);

    int flags= regOperPtr.p->op_struct.op_type |
      Page_cache_client::COMMIT_REQ | Page_cache_client::STRICT_ORDER;
    int res= m_pgman.get_page(signal, req, flags);
    switch(res){
    case 0:
      /**
       * Timeslice
       */
      signal->theData[0] = 1;
      return;
    case -1:
      ndbrequire("NOT YET IMPLEMENTED" == 0);
      break;
    }
    regOperPtr.p->m_commit_disk_callback_page= res;
    regOperPtr.p->op_struct.m_load_diskpage_on_commit= 0;
  } 
  
  if(regOperPtr.p->op_struct.m_wait_log_buffer)
  {
    ndbassert(regOperPtr.p->is_first_operation() && 
	      regOperPtr.p->is_last_operation());
    
    Callback cb;
    cb.m_callbackData= regOperPtr.i;
    cb.m_callbackFunction = 
      safe_cast(&Dbtup::disk_page_log_buffer_callback);
    Uint32 sz= regOperPtr.p->m_undo_buffer_space;
    
    Logfile_client lgman(this, c_lgman, regFragPtr.p->m_logfile_group_id);
    int res= lgman.get_log_buffer(signal, sz, &cb);
    switch(res){
    case 0:
      signal->theData[0] = 1;
      return;
    case -1:
      ndbrequire("NOT YET IMPLEMENTED" == 0);
      break;
    }
  }
  
  if(!tuple_ptr)
  {
    req_struct.m_tuple_ptr= tuple_ptr = (Tuple_header*)
      get_ptr(&page, &regOperPtr.p->m_tuple_location,regTabPtr.p);
  }
  
  if(get_tuple_state(regOperPtr.p) == TUPLE_PREPARED)
  {
    /**
     * Execute all tux triggers at first commit
     *   since previous tuple is otherwise removed...
     *   btw...is this a "good" solution??
     *   
     *   why can't we instead remove "own version" (when approriate ofcourse)
     */
    if (!regTabPtr.p->tuxCustomTriggers.isEmpty()) {
      ljam();
      OperationrecPtr loopPtr= regOperPtr;
      while(loopPtr.i != RNIL)
      {
	c_operation_pool.getPtr(loopPtr);
	executeTuxCommitTriggers(signal,
				 loopPtr.p,
				 regFragPtr.p,
				 regTabPtr.p);
	set_tuple_state(loopPtr.p, TUPLE_TO_BE_COMMITTED);
	loopPtr.i = loopPtr.p->nextActiveOp;
      }
    }
  }
  
  if(regOperPtr.p->is_last_operation())
  {
    /**
     * Perform "real" commit
     */
    set_change_mask_info(&req_struct, regOperPtr.p);
    checkDetachedTriggers(&req_struct, regOperPtr.p, regTabPtr.p);
    
    if(regOperPtr.p->op_struct.op_type != ZDELETE)
    {
      commit_operation(signal, gci, tuple_ptr, page.p,
		       regOperPtr.p, regFragPtr.p, regTabPtr.p); 
      removeActiveOpList(regOperPtr.p, tuple_ptr);
    }
    else
    {
      removeActiveOpList(regOperPtr.p, tuple_ptr);
      dealloc_tuple(signal, gci, page.p, tuple_ptr, 
		    regOperPtr.p, regFragPtr.p, regTabPtr.p); 
    }
  } 
  else
  {
    removeActiveOpList(regOperPtr.p, tuple_ptr);   
  }
  
  initOpConnection(regOperPtr.p);
  signal->theData[0] = 0;
}

void
Dbtup::set_change_mask_info(KeyReqStruct * const req_struct,
                            Operationrec * const regOperPtr)
{
  ChangeMaskState change_mask= get_change_mask_state(regOperPtr);
  if (change_mask == USE_SAVED_CHANGE_MASK) {
    ljam();
    req_struct->changeMask.setWord(0, regOperPtr->saved_change_mask[0]);
    req_struct->changeMask.setWord(1, regOperPtr->saved_change_mask[1]);
    //get saved state
  } else if (change_mask == RECALCULATE_CHANGE_MASK) {
    ljam();
    //Recompute change mask, for now set all bits
    req_struct->changeMask.set();
  } else if (change_mask == SET_ALL_MASK) {
    ljam();
    req_struct->changeMask.set();
  } else {
    ljam();
    ndbrequire(change_mask == DELETE_CHANGES);
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
      ljam();
      saved_word1|= loopOpPtr.p->saved_change_mask[0];
      saved_word2|= loopOpPtr.p->saved_change_mask[1];
    } else if (change_mask == RECALCULATE_CHANGE_MASK) {
      ljam();
      //Recompute change mask, for now set all bits
      req_struct->changeMask.set();
      return;
    } else {
      ndbrequire(change_mask == SET_ALL_MASK);
      ljam();
      req_struct->changeMask.set();
      return;
    }
    loopOpPtr.i= loopOpPtr.p->prevActiveOp;
  } while (loopOpPtr.i != RNIL);
  req_struct->changeMask.setWord(0, saved_word1);
  req_struct->changeMask.setWord(1, saved_word2);
}
