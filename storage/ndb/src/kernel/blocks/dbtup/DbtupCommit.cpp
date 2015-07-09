/*
   Copyright (c) 2003, 2015, Oracle and/or its affiliates. All rights reserved.

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
#define DBTUP_COMMIT_CPP
#include "Dbtup.hpp"
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <pc.hpp>
#include <signaldata/TupCommit.hpp>
#include <EventLogger.hpp>
#include "../dblqh/Dblqh.hpp"

#define JAM_FILE_ID 416

extern EventLogger *g_eventLogger;

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
  
  if (! Local_key::isInvalid(frag_page_id, page_index))
  {
    Local_key tmp;
    tmp.m_page_no= getRealpid(regFragPtr.p, frag_page_id); 
    tmp.m_page_idx= page_index;
    
    PagePtr pagePtr;
    Tuple_header* ptr= (Tuple_header*)get_ptr(&pagePtr, &tmp, regTabPtr.p);

    ndbrequire(ptr->m_header_bits & Tuple_header::FREED);

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
  regOperPtr->op_type= ZREAD;
  regOperPtr->op_struct.bit_field.m_disk_preallocated= 0;
  regOperPtr->op_struct.bit_field.m_load_diskpage_on_commit= 0;
  regOperPtr->op_struct.bit_field.m_wait_log_buffer= 0;
  regOperPtr->op_struct.bit_field.in_active_list = false;
  regOperPtr->m_undo_buffer_space= 0;
}

bool
Dbtup::is_rowid_in_remaining_lcp_set(const Page* page,
                                     const Local_key& key1,
                                     const Dbtup::ScanOp& op) const
{
  Local_key key2 = op.m_scanPos.m_key;
  switch (op.m_state) {
  case Dbtup::ScanOp::First:
  {
    jam();
    ndbrequire(key2.isNull());
    return key1.m_page_no < op.m_endPage;
  }
  case Dbtup::ScanOp::Current:
  {
    /* Impossible state for LCP scans */
    ndbrequire(false);
  }
  case Dbtup::ScanOp::Next:
  {
    ndbrequire(!key2.isNull());
    if (key1.m_page_no < key2.m_page_no)
    {
      jam();
      /* Ignore pages already LCP:ed */
      return false;
    }
    if (key1.m_page_no >= op.m_endPage)
    {
      jam();
      /* Ignore pages beyond last page at LCP start */
      return false;
    }
    if (page->is_page_to_skip_lcp())
    {
      jam();
      /* Ignore new pages created after LCP start */
      return false;
    }
    if (key1.m_page_no > key2.m_page_no)
    {
      jam();
      /* Include pages not LCP:ed yet */
      return true;
    }
    ndbassert(key1.m_page_no == key2.m_page_no);
    if (op.m_scanPos.m_get == ScanPos::Get_next_page_mm)
    {
      jam();
      /**
       * We got a real-time break while switching to a new page.
       * In this case we can skip the page since it is already
       * LCP:ed.
       */
      return false;
    }
    if (key1.m_page_idx < key2.m_page_idx)
    {
      jam();
      /* Ignore rows already LCP:ed */
      return false;
    }
    if (key1.m_page_idx > key2.m_page_idx)
    {
      jam();
      /* Include rows not LCP:ed yet */
      return true;
    }
    ndbassert(key1.m_page_idx == key2.m_page_idx);
    /* keys are equal */
    jam();
    /* Ignore current row that already have been LCP:ed. */
    return false;
  }
  case Dbtup::ScanOp::Last:
  { 
    jam();
    return false;
  }
  default:
    ndbrequire(false);
    break;
  }
  /* Will never arrive here */
  return true;
}

void
Dbtup::dealloc_tuple(Signal* signal,
		     Uint32 gci_hi,
                     Uint32 gci_lo,
		     Page* page,
		     Tuple_header* ptr, 
                     KeyReqStruct * req_struct,
		     Operationrec* regOperPtr, 
		     Fragrecord* regFragPtr, 
		     Tablerec* regTabPtr,
                     Ptr<GlobalPage> pagePtr)
{
  Uint32 lcpScan_ptr_i= regFragPtr->m_lcp_scan_op;

  Uint32 bits = ptr->m_header_bits;
  Uint32 extra_bits = Tuple_header::FREED;
  if (bits & Tuple_header::DISK_PART)
  {
    jam();
    Local_key disk;
    memcpy(&disk, ptr->get_disk_ref_ptr(regTabPtr), sizeof(disk));
    PagePtr tmpptr;
    ndbrequire(pagePtr.i != RNIL);
    tmpptr.i = pagePtr.i;
    tmpptr.p = reinterpret_cast<Page*>(pagePtr.p);
    disk_page_free(signal, regTabPtr, regFragPtr, 
		   &disk, tmpptr, gci_hi);
  }
  
  if (! (bits & (Tuple_header::LCP_SKIP | Tuple_header::ALLOC)) && 
      lcpScan_ptr_i != RNIL && regTabPtr->m_no_of_disk_attributes > 0)
  {
    jam();
    ScanOpPtr scanOp;
    c_scanOpPool.getPtr(scanOp, lcpScan_ptr_i);
    Local_key rowid = regOperPtr->m_tuple_location;
    rowid.m_page_no = page->frag_page_id;
    if (is_rowid_in_remaining_lcp_set(page, rowid, *scanOp.p))
    {
      jam();

      /**
       * We're committing a delete, on a row that should
       *   be part of LCP. Copy original row into copy-tuple
       *   and add this copy-tuple to lcp-keep-list
       *
       */
      handle_lcp_keep_commit(&rowid,
                             req_struct, regOperPtr, regFragPtr, regTabPtr);
    }
  }
  
  ptr->m_header_bits = bits | extra_bits;
  
  if (regTabPtr->m_bits & Tablerec::TR_RowGCI)
  {
    jam();
    * ptr->get_mm_gci(regTabPtr) = gci_hi;
    if (regTabPtr->m_bits & Tablerec::TR_ExtraRowGCIBits)
    {
      Uint32 attrId = regTabPtr->getExtraAttrId<Tablerec::TR_ExtraRowGCIBits>();
      store_extra_row_bits(attrId, regTabPtr, ptr, gci_lo, /* truncate */true);
    }
  }
  setInvalidChecksum(ptr, regTabPtr);
}

void
Dbtup::handle_lcp_keep_commit(const Local_key* rowid,
                              KeyReqStruct * req_struct,
                              Operationrec * opPtrP,
                              Fragrecord * regFragPtr,
                              Tablerec * regTabPtr)
{
  bool disk = false;
  Uint32 sizes[4];
  Uint32 * copytuple = get_copy_tuple_raw(&opPtrP->m_copy_tuple_location);
  Tuple_header * dst = get_copy_tuple(copytuple);
  Tuple_header * org = req_struct->m_tuple_ptr;
  Uint32 old_header_bits = org->m_header_bits;
  if (regTabPtr->need_expand(disk))
  {
    setup_fixed_tuple_ref(req_struct, opPtrP, regTabPtr);
    setup_fixed_part(req_struct, opPtrP, regTabPtr);
    req_struct->m_tuple_ptr = dst;
    expand_tuple(req_struct, sizes, org, regTabPtr, disk);
    shrink_tuple(req_struct, sizes+2, regTabPtr, disk);
  }
  else
  {
    memcpy(dst, org, 4*regTabPtr->m_offsets[MM].m_fix_header_size);
  }
  dst->m_header_bits |= Tuple_header::COPY_TUPLE;

  updateChecksum(dst, regTabPtr, old_header_bits, dst->m_header_bits);

  /**
   * Store original row-id in copytuple[0,1]
   * Store next-ptr in copytuple[1,2] (set to RNIL/RNIL)
   *
   */
  assert(sizeof(Local_key) == 8);
  memcpy(copytuple+0, rowid, sizeof(Local_key));

  Local_key nil;
  nil.setNull();
  memcpy(copytuple+2, &nil, sizeof(nil));

  /**
   * Link it to list
   */
  if (regFragPtr->m_lcp_keep_list_tail.isNull())
  {
    jam();
    regFragPtr->m_lcp_keep_list_head = opPtrP->m_copy_tuple_location;
  }
  else
  {
    jam();
    Uint32 * tail = get_copy_tuple_raw(&regFragPtr->m_lcp_keep_list_tail);
    Local_key nextptr;
    memcpy(&nextptr, tail+2, sizeof(Local_key));
    ndbassert(nextptr.isNull());
    nextptr = opPtrP->m_copy_tuple_location;
    memcpy(tail+2, &nextptr, sizeof(Local_key));
  }
  regFragPtr->m_lcp_keep_list_tail = opPtrP->m_copy_tuple_location;

  /**
   * And finally clear m_copy_tuple_location so that it won't be freed
   */
  opPtrP->m_copy_tuple_location.setNull();
}

#if 0
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
#endif

void
Dbtup::commit_operation(Signal* signal,
			Uint32 gci_hi,
                        Uint32 gci_lo,
			Tuple_header* tuple_ptr, 
			PagePtr pagePtr,
			Operationrec* regOperPtr, 
			Fragrecord* regFragPtr, 
			Tablerec* regTabPtr,
                        Ptr<GlobalPage> globDiskPagePtr)
{
  ndbassert(regOperPtr->op_type != ZDELETE);
  
  Uint32 lcpScan_ptr_i= regFragPtr->m_lcp_scan_op;
  Uint32 save= tuple_ptr->m_operation_ptr_i;
  Uint32 bits= tuple_ptr->m_header_bits;

  Tuple_header *disk_ptr= 0;
  Tuple_header *copy= get_copy_tuple(&regOperPtr->m_copy_tuple_location);
  
  Uint32 copy_bits= copy->m_header_bits;

  Uint32 fixsize= regTabPtr->m_offsets[MM].m_fix_header_size;
  Uint32 mm_vars= regTabPtr->m_attributes[MM].m_no_of_varsize;
  Uint32 mm_dyns= regTabPtr->m_attributes[MM].m_no_of_dynamic;
  bool update_gci_at_commit = ! regOperPtr->op_struct.bit_field.m_gci_written;
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
      ndbassert(bits & Tuple_header::VAR_PART);
      ndbassert(tmp.m_page_no != RNIL);
      ndbassert(copy_bits & Tuple_header::COPY_TUPLE);

      Uint32 *dst= get_ptr(&vpagePtr, *ref);
      Var_page* vpagePtrP = (Var_page*)vpagePtr.p;
      Varpart_copy*vp =(Varpart_copy*)copy->get_end_of_fix_part_ptr(regTabPtr);
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
          ndbassert(regFragPtr->m_varWordsFree >= vpagePtrP->free_space);
          regFragPtr->m_varWordsFree -= vpagePtrP->free_space;
          vpagePtrP->shrink_entry(tmp.m_page_idx, len);
          // Adds the new free space value for the page to the fragment total.
          update_free_page_list(regFragPtr, vpagePtr);
        }
        else
        {
          jam();
          free_var_part(regFragPtr, vpagePtr, tmp.m_page_idx);
          tmp.m_page_no = RNIL;
          ref->assign(&tmp);
          copy_bits &= ~(Uint32)Tuple_header::VAR_PART;
        }
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

    PagePtr diskPagePtr((Tup_page*)globDiskPagePtr.p, globDiskPagePtr.i);
    ndbassert(diskPagePtr.p->m_page_no == key.m_page_no);
    ndbassert(diskPagePtr.p->m_file_no == key.m_file_no);
    Uint32 sz, *dst;
    if(copy_bits & Tuple_header::DISK_ALLOC)
    {
      jam();
      disk_page_alloc(signal, regTabPtr, regFragPtr, &key, diskPagePtr, gci_hi);
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
			    &key, dst, sz, gci_hi, logfile_group_id);
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
    rowid.m_page_no = pagePtr.p->frag_page_id;
    if (is_rowid_in_remaining_lcp_set(pagePtr.p, rowid, *scanOp.p))
    {
      /**
       * Rows that are inserted during LCPs are never required to be
       * recorded as part of the LCP, this can be avoided in multiple ways,
       * in this case we avoid it by setting bit on Tuple header.
       */
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
  
  if (regTabPtr->m_bits & Tablerec::TR_RowGCI  &&
      update_gci_at_commit)
  {
    jam();
    * tuple_ptr->get_mm_gci(regTabPtr) = gci_hi;
    if (regTabPtr->m_bits & Tablerec::TR_ExtraRowGCIBits)
    {
      Uint32 attrId = regTabPtr->getExtraAttrId<Tablerec::TR_ExtraRowGCIBits>();
      store_extra_row_bits(attrId, regTabPtr, tuple_ptr, gci_lo,
                           /* truncate */true);
    }
  }
  setChecksum(tuple_ptr, regTabPtr);
}

void
Dbtup::disk_page_commit_callback(Signal* signal, 
				 Uint32 opPtrI, Uint32 page_id)
{
  Uint32 hash_value;
  Uint32 gci_hi, gci_lo;
  Uint32 transId1, transId2;
  OperationrecPtr regOperPtr;
  Ptr<GlobalPage> diskPagePtr;

  jamEntry();
  
  c_operation_pool.getPtr(regOperPtr, opPtrI);
  c_lqh->get_op_info(regOperPtr.p->userpointer, &hash_value, &gci_hi, &gci_lo,
                     &transId1, &transId2);

  TupCommitReq * const tupCommitReq= (TupCommitReq *)signal->getDataPtr();
  
  tupCommitReq->opPtr= opPtrI;
  tupCommitReq->hashValue= hash_value;
  tupCommitReq->gci_hi= gci_hi;
  tupCommitReq->gci_lo= gci_lo;
  tupCommitReq->diskpage = page_id;
  tupCommitReq->transId1 = transId1;
  tupCommitReq->transId2 = transId2;

  regOperPtr.p->op_struct.bit_field.m_load_diskpage_on_commit= 0;
  regOperPtr.p->m_commit_disk_callback_page= page_id;
  m_global_page_pool.getPtr(diskPagePtr, page_id);
  
  {
    PagePtr tmp;
    tmp.i = diskPagePtr.i;
    tmp.p = reinterpret_cast<Page*>(diskPagePtr.p);
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
  Uint32 transId1, transId2;
  OperationrecPtr regOperPtr;

  jamEntry();
  
  c_operation_pool.getPtr(regOperPtr, opPtrI);
  c_lqh->get_op_info(regOperPtr.p->userpointer, &hash_value, &gci_hi, &gci_lo,
                     &transId1, &transId2);
  Uint32 page= regOperPtr.p->m_commit_disk_callback_page;

  TupCommitReq * const tupCommitReq= (TupCommitReq *)signal->getDataPtr();
  
  tupCommitReq->opPtr= opPtrI;
  tupCommitReq->hashValue= hash_value;
  tupCommitReq->gci_hi= gci_hi;
  tupCommitReq->gci_lo= gci_lo;
  tupCommitReq->diskpage = page;
  tupCommitReq->transId1 = transId1;
  tupCommitReq->transId2 = transId2;

  ndbassert(regOperPtr.p->op_struct.bit_field.m_load_diskpage_on_commit == 0);
  regOperPtr.p->op_struct.bit_field.m_wait_log_buffer= 0;
  
  execTUP_COMMITREQ(signal);
  ndbassert(signal->theData[0] == 0);
  
  c_lqh->tupcommit_conf_callback(signal, regOperPtr.p->userpointer);
}

int Dbtup::retrieve_data_page(Signal *signal,
                              Page_cache_client::Request req,
                              OperationrecPtr regOperPtr,
                              Ptr<GlobalPage> &diskPagePtr)
{
  req.m_callback.m_callbackData= regOperPtr.i;
  req.m_callback.m_callbackFunction =
    safe_cast(&Dbtup::disk_page_commit_callback);

  /*
   * Consider commit to be correlated.  Otherwise pk op + commit makes
   * the page hot.   XXX move to TUP which knows better.
   */
  int flags= regOperPtr.p->op_type |
    Page_cache_client::COMMIT_REQ | Page_cache_client::CORR_REQ;
  Page_cache_client pgman(this, c_pgman);
  int res= pgman.get_page(signal, req, flags);
  diskPagePtr = pgman.m_ptr;

  switch(res){
  case 0:
    /**
     * Timeslice
     */
    jam();
    signal->theData[0] = 1;
    return res;
  case -1:
    ndbrequire("NOT YET IMPLEMENTED" == 0);
    break;
  default:
    jam();
  }
  {
    PagePtr tmpptr;
    tmpptr.i = diskPagePtr.i;
    tmpptr.p = reinterpret_cast<Page*>(diskPagePtr.p);

    disk_page_set_dirty(tmpptr);
  }
  regOperPtr.p->m_commit_disk_callback_page= res;
  regOperPtr.p->op_struct.bit_field.m_load_diskpage_on_commit= 0;

  return res;
}

int Dbtup::retrieve_log_page(Signal *signal,
                             FragrecordPtr regFragPtr,
                             OperationrecPtr regOperPtr)
{
  jam();
  /**
   * Only last op on tuple needs "real" commit,
   *   hence only this one should have m_wait_log_buffer
   */

  CallbackPtr cb;
  cb.m_callbackData= regOperPtr.i;
  cb.m_callbackIndex = DISK_PAGE_LOG_BUFFER_CALLBACK;
  Uint32 sz= regOperPtr.p->m_undo_buffer_space;

  D("Logfile_client - execTUP_COMMITREQ");
  Logfile_client lgman(this, c_lgman, regFragPtr.p->m_logfile_group_id);
  int res= lgman.get_log_buffer(signal, sz, &cb);
  jamEntry();
  switch(res){
  case 0:
    jam();
    signal->theData[0] = 1;
    return res;
  case -1:
    g_eventLogger->warning("Out of space in RG_DISK_OPERATIONS resource,"
                           " increase config parameter GlobalSharedMemory");
    ndbrequire("NOT YET IMPLEMENTED" == 0);
    break;
  default:
    jam();
  }
  regOperPtr.p->op_struct.bit_field.m_wait_log_buffer= 0;

  return res;
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
  KeyReqStruct req_struct(this, KRS_COMMIT);
  TransState trans_state;
  Ptr<GlobalPage> diskPagePtr;
  Uint32 no_of_fragrec, no_of_tablerec;

  TupCommitReq * const tupCommitReq= (TupCommitReq *)signal->getDataPtr();

  regOperPtr.i= tupCommitReq->opPtr;
  Uint32 hash_value= tupCommitReq->hashValue;
  Uint32 gci_hi = tupCommitReq->gci_hi;
  Uint32 gci_lo = tupCommitReq->gci_lo;
  Uint32 transId1 = tupCommitReq->transId1;
  Uint32 transId2 = tupCommitReq->transId2;

  jamEntry();

  c_operation_pool.getPtr(regOperPtr);
 
  diskPagePtr.i = tupCommitReq->diskpage;
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
  /* Put transid in req_struct, so detached triggers can access it */
  req_struct.trans_id1 = transId1;
  req_struct.trans_id2 = transId2;
  req_struct.m_reorg = regOperPtr.p->op_struct.bit_field.m_reorg;
  regOperPtr.p->m_commit_disk_callback_page = tupCommitReq->diskpage;

  if (diskPagePtr.i == RNIL)
  {
    jam();
    diskPagePtr.p = 0;
    req_struct.m_disk_page_ptr.i = RNIL;
    req_struct.m_disk_page_ptr.p = 0;
  }
  else
  {
    m_global_page_pool.getPtr(diskPagePtr, diskPagePtr.i);
  }
  
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
  if(regOperPtr.p->op_struct.bit_field.m_load_diskpage_on_commit)
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
      Tuple_header* tmp= get_copy_tuple(&regOperPtr.p->m_copy_tuple_location);
      
      memcpy(&req.m_page, 
	     tmp->get_disk_ref_ptr(regTabPtr.p), sizeof(Local_key));

      if (unlikely(regOperPtr.p->op_type == ZDELETE &&
		   tmp->m_header_bits & Tuple_header::DISK_ALLOC))
      {
        jam();
	/**
	 * Insert+Delete
         * In this case we want to release the Copy page tuple that was
         * allocated for the insert operation since the commit of the
         * delete operation here makes it unnecessary to save the
         * new record.
	 */
        regOperPtr.p->op_struct.bit_field.m_load_diskpage_on_commit = 0;
        regOperPtr.p->op_struct.bit_field.m_wait_log_buffer = 0;	
        disk_page_abort_prealloc(signal, regFragPtr.p, 
				 &req.m_page, req.m_page.m_page_idx);
        
        D("Logfile_client - execTUP_COMMITREQ");
        Logfile_client lgman(this, c_lgman, regFragPtr.p->m_logfile_group_id);
        lgman.free_log_space(regOperPtr.p->m_undo_buffer_space,
                             jamBuffer());
	goto skip_disk;
      }
    } 
    else
    {
      jam();
      // initial delete
      ndbassert(regOperPtr.p->op_type == ZDELETE);
      memcpy(&req.m_page, 
	     tuple_ptr->get_disk_ref_ptr(regTabPtr.p), sizeof(Local_key));
      
      ndbassert(tuple_ptr->m_header_bits & Tuple_header::DISK_PART);
    }

    if (retrieve_data_page(signal,
                           req,
                           regOperPtr,
                           diskPagePtr) == 0)
    {
      return; // Data page has not been retrieved yet.
    }
    get_page = true;
  } 
  
  if(regOperPtr.p->op_struct.bit_field.m_wait_log_buffer)
  {
    jam();
    /**
     * Only last op on tuple needs "real" commit,
     *   hence only this one should have m_wait_log_buffer
     */
    ndbassert(tuple_ptr->m_operation_ptr_i == regOperPtr.i);
    
    if (retrieve_log_page(signal, regFragPtr, regOperPtr) == 0)
    {
      return; // Log page has not been retrieved yet.
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
    set_commit_change_mask_info(regTabPtr.p, &req_struct, regOperPtr.p);
    checkDetachedTriggers(&req_struct,
                          regOperPtr.p,
                          regTabPtr.p, 
                          disk != RNIL,
                          diskPagePtr.i);
    
    tuple_ptr->m_operation_ptr_i = RNIL;
    
    if (regOperPtr.p->op_type == ZDELETE)
    {
      jam();
      if (get_page)
      {
        ndbassert(tuple_ptr->m_header_bits & Tuple_header::DISK_PART);
      }
      dealloc_tuple(signal,
                    gci_hi,
                    gci_lo,
                    page.p,
                    tuple_ptr,
                    &req_struct,
                    regOperPtr.p,
                    regFragPtr.p,
                    regTabPtr.p,
                    diskPagePtr);
    }
    else if(regOperPtr.p->op_type != ZREFRESH)
    {
      jam();
      commit_operation(signal,
                       gci_hi,
                       gci_lo,
                       tuple_ptr,
                       page,
		       regOperPtr.p,
                       regFragPtr.p,
                       regTabPtr.p,
                       diskPagePtr); 
    }
    else
    {
      jam();
      commit_refresh(signal,
                     gci_hi,
                     gci_lo,
                     tuple_ptr,
                     page,
                     &req_struct,
                     regOperPtr.p,
                     regFragPtr.p,
                     regTabPtr.p,
                     diskPagePtr);
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
Dbtup::set_commit_change_mask_info(const Tablerec* regTabPtr,
                                   KeyReqStruct * req_struct,
                                   const Operationrec * regOperPtr)
{
  Uint32 masklen = (regTabPtr->m_no_of_attributes + 31) >> 5;
  if (regOperPtr->m_copy_tuple_location.isNull())
  {
    ndbassert(regOperPtr->op_type == ZDELETE);
    req_struct->changeMask.set();
  }
  else
  {
    Uint32 * dst = req_struct->changeMask.rep.data;
    Uint32 * rawptr = get_copy_tuple_raw(&regOperPtr->m_copy_tuple_location);
    ChangeMask * maskptr = get_change_mask_ptr(rawptr);
    Uint32 cols = maskptr->m_cols;
    if (cols == regTabPtr->m_no_of_attributes)
    {
      memcpy(dst, maskptr->m_mask, 4*masklen);
    }
    else
    {
      ndbassert(regTabPtr->m_no_of_attributes > cols); // no drop column
      memcpy(dst, maskptr->m_mask, 4*((cols + 31) >> 5));
      req_struct->changeMask.setRange(cols,
                                      regTabPtr->m_no_of_attributes - cols);
    }
  }
}

void
Dbtup::commit_refresh(Signal* signal,
                      Uint32 gci_hi,
                      Uint32 gci_lo,
                      Tuple_header* tuple_ptr,
                      PagePtr pagePtr,
                      KeyReqStruct * req_struct,
                      Operationrec* regOperPtr,
                      Fragrecord* regFragPtr,
                      Tablerec* regTabPtr,
                      Ptr<GlobalPage> diskPagePtr)
{
  /* Committing a refresh operation.
   * Refresh of an existing row looks like an update
   * and can commit normally.
   * Refresh of a non-existing row looks like an Insert which
   * is 'undone' at commit time.
   * This is achieved by making special calls to ACC to get
   * it to forget, before deallocating the tuple locally.
   */
  switch(regOperPtr->m_copy_tuple_location.m_file_no){
  case Operationrec::RF_SINGLE_NOT_EXIST:
  case Operationrec::RF_MULTI_NOT_EXIST:
    break;
  case Operationrec::RF_SINGLE_EXIST:
  case Operationrec::RF_MULTI_EXIST:
    // "Normal" update
    commit_operation(signal,
                     gci_hi,
                     gci_lo,
                     tuple_ptr,
                     pagePtr,
                     regOperPtr,
                     regFragPtr,
                     regTabPtr,
                     diskPagePtr);
    return;

  default:
    ndbrequire(false);
  }

  Local_key key = regOperPtr->m_tuple_location;
  key.m_page_no = pagePtr.p->frag_page_id;

  /**
   * Tell ACC to delete
   */
  c_lqh->accremoverow(signal, regOperPtr->userpointer, &key);
  dealloc_tuple(signal,
                gci_hi,
                gci_lo,
                pagePtr.p,
                tuple_ptr,
                req_struct,
                regOperPtr,
                regFragPtr,
                regTabPtr,
                diskPagePtr);
}
