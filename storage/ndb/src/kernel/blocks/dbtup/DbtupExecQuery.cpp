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
#include <Dblqh.hpp>
#include "Dbtup.hpp"
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <pc.hpp>
#include <AttributeDescriptor.hpp>
#include "AttributeOffset.hpp"
#include <AttributeHeader.hpp>
#include <Interpreter.hpp>
#include <signaldata/TupCommit.hpp>
#include <signaldata/TupKey.hpp>
#include <signaldata/AttrInfo.hpp>
#include <NdbSqlUtil.hpp>

/* ----------------------------------------------------------------- */
/* -----------       INIT_STORED_OPERATIONREC         -------------- */
/* ----------------------------------------------------------------- */
int Dbtup::initStoredOperationrec(Operationrec* regOperPtr,
                                  KeyReqStruct* req_struct,
                                  Uint32 storedId) 
{
  jam();
  StoredProcPtr storedPtr;
  c_storedProcPool.getPtr(storedPtr, storedId);
  if (storedPtr.i != RNIL) {
    if (storedPtr.p->storedCode == ZSCAN_PROCEDURE) {
      storedPtr.p->storedCounter++;
      regOperPtr->firstAttrinbufrec= storedPtr.p->storedLinkFirst;
      regOperPtr->lastAttrinbufrec= storedPtr.p->storedLinkLast;
      regOperPtr->currentAttrinbufLen= storedPtr.p->storedProcLength;
      req_struct->attrinfo_len= storedPtr.p->storedProcLength;
      return ZOK;
    }
  }
  terrorCode= ZSTORED_PROC_ID_ERROR;
  return terrorCode;
}

void Dbtup::copyAttrinfo(Operationrec * regOperPtr,
                         Uint32* inBuffer)
{
  AttrbufrecPtr copyAttrBufPtr;
  Uint32 RnoOfAttrBufrec= cnoOfAttrbufrec;
  int RbufLen;
  Uint32 RinBufIndex= 0;
  Uint32 Rnext;
  Uint32 Rfirst;
  Uint32 TstoredProcedure= (regOperPtr->storedProcedureId != ZNIL);
  Uint32 RnoFree= cnoFreeAttrbufrec;

//-------------------------------------------------------------------------
// As a prelude to the execution of the TUPKEYREQ we will copy the program
// into the inBuffer to enable easy execution without any complex jumping
// between the buffers. In particular this will make the interpreter less
// complex. Hopefully it does also improve performance.
//-------------------------------------------------------------------------
  copyAttrBufPtr.i= regOperPtr->firstAttrinbufrec;
  while (copyAttrBufPtr.i != RNIL) {
    jam();
    ndbrequire(copyAttrBufPtr.i < RnoOfAttrBufrec);
    ptrAss(copyAttrBufPtr, attrbufrec);
    RbufLen= copyAttrBufPtr.p->attrbuf[ZBUF_DATA_LEN];
    Rnext= copyAttrBufPtr.p->attrbuf[ZBUF_NEXT];
    Rfirst= cfirstfreeAttrbufrec;
    MEMCOPY_NO_WORDS(&inBuffer[RinBufIndex],
                     &copyAttrBufPtr.p->attrbuf[0],
                     RbufLen);
    RinBufIndex += RbufLen;
    if (!TstoredProcedure) {
      copyAttrBufPtr.p->attrbuf[ZBUF_NEXT]= Rfirst;
      cfirstfreeAttrbufrec= copyAttrBufPtr.i;
      RnoFree++;
    }
    copyAttrBufPtr.i= Rnext;
  }
  cnoFreeAttrbufrec= RnoFree;
  if (TstoredProcedure) {
    jam();
    StoredProcPtr storedPtr;
    c_storedProcPool.getPtr(storedPtr, (Uint32)regOperPtr->storedProcedureId);
    ndbrequire(storedPtr.p->storedCode == ZSCAN_PROCEDURE);
    storedPtr.p->storedCounter--;
  }
  // Release the ATTRINFO buffers
  regOperPtr->storedProcedureId= RNIL;
  regOperPtr->firstAttrinbufrec= RNIL;
  regOperPtr->lastAttrinbufrec= RNIL;
}

void Dbtup::handleATTRINFOforTUPKEYREQ(Signal* signal,
                                       const Uint32 *data,
				       Uint32 len,
                                       Operationrec * regOperPtr) 
{
  while(len)
  {
    Uint32 length = len > AttrInfo::DataLength ? AttrInfo::DataLength : len;

    AttrbufrecPtr TAttrinbufptr;
    TAttrinbufptr.i= cfirstfreeAttrbufrec;
    if ((cfirstfreeAttrbufrec < cnoOfAttrbufrec) &&
	(cnoFreeAttrbufrec > MIN_ATTRBUF)) {
      ptrAss(TAttrinbufptr, attrbufrec);
      MEMCOPY_NO_WORDS(&TAttrinbufptr.p->attrbuf[0],
		       data,
		       length);
      Uint32 RnoFree= cnoFreeAttrbufrec;
      Uint32 Rnext= TAttrinbufptr.p->attrbuf[ZBUF_NEXT];
      TAttrinbufptr.p->attrbuf[ZBUF_DATA_LEN]= length;
      TAttrinbufptr.p->attrbuf[ZBUF_NEXT]= RNIL;
      
      AttrbufrecPtr locAttrinbufptr;
      Uint32 RnewLen= regOperPtr->currentAttrinbufLen;
      
      locAttrinbufptr.i= regOperPtr->lastAttrinbufrec;
      cfirstfreeAttrbufrec= Rnext;
      cnoFreeAttrbufrec= RnoFree - 1;
      RnewLen += length;
      regOperPtr->lastAttrinbufrec= TAttrinbufptr.i;
      regOperPtr->currentAttrinbufLen= RnewLen;
      if (locAttrinbufptr.i == RNIL) {
	regOperPtr->firstAttrinbufrec= TAttrinbufptr.i;
      } else {
	jam();
	ptrCheckGuard(locAttrinbufptr, cnoOfAttrbufrec, attrbufrec);
	locAttrinbufptr.p->attrbuf[ZBUF_NEXT]= TAttrinbufptr.i;
      }
      if (RnewLen < ZATTR_BUFFER_SIZE) {
      } else {
	jam();
	set_trans_state(regOperPtr, TRANS_TOO_MUCH_AI);
	return;
      }
    } else if (cnoFreeAttrbufrec <= MIN_ATTRBUF) {
      jam();
      set_trans_state(regOperPtr, TRANS_ERROR_WAIT_TUPKEYREQ);
    } else {
      ndbrequire(false);
    }
    
    len -= length;
    data += length;    
  }
}

void Dbtup::execATTRINFO(Signal* signal) 
{
  Uint32 Rsig0= signal->theData[0];
  Uint32 Rlen= signal->length();
  jamEntry();

  receive_attrinfo(signal, Rsig0, signal->theData+3, Rlen-3);
}
 
void
Dbtup::receive_attrinfo(Signal* signal, Uint32 op, 
			const Uint32* data, Uint32 Rlen)
{ 
  OperationrecPtr regOpPtr;
  regOpPtr.i= op;
  c_operation_pool.getPtr(regOpPtr, op);
  TransState trans_state= get_trans_state(regOpPtr.p);
  if (trans_state == TRANS_IDLE) {
    handleATTRINFOforTUPKEYREQ(signal, data, Rlen, regOpPtr.p);
    return;
  } else if (trans_state == TRANS_WAIT_STORED_PROCEDURE_ATTR_INFO) {
    storedProcedureAttrInfo(signal, regOpPtr.p, data, Rlen, false);
    return;
  }
  switch (trans_state) {
  case TRANS_ERROR_WAIT_STORED_PROCREQ:
    jam();
  case TRANS_TOO_MUCH_AI:
    jam();
  case TRANS_ERROR_WAIT_TUPKEYREQ:
    jam();
    return;	/* IGNORE ATTRINFO IN THOSE STATES, WAITING FOR ABORT SIGNAL */
  case TRANS_DISCONNECTED:
    jam();
  case TRANS_STARTED:
    jam();
  default:
    ndbrequire(false);
  }
}

void
Dbtup::setChecksum(Tuple_header* tuple_ptr,
                   Tablerec* regTabPtr)
{
  tuple_ptr->m_checksum= 0;
  tuple_ptr->m_checksum= calculateChecksum(tuple_ptr, regTabPtr);
}

Uint32
Dbtup::calculateChecksum(Tuple_header* tuple_ptr,
                         Tablerec* regTabPtr)
{
  Uint32 checksum;
  Uint32 i, rec_size, *tuple_header;
  rec_size= regTabPtr->m_offsets[MM].m_fix_header_size;
  tuple_header= tuple_ptr->m_data;
  checksum= 0;
  // includes tupVersion
  //printf("%p - ", tuple_ptr);
  
  if (regTabPtr->m_attributes[MM].m_no_of_varsize)
    rec_size += Tuple_header::HeaderSize;
  
  for (i= 0; i < rec_size-2; i++) {
    checksum ^= tuple_header[i];
    //printf("%.8x ", tuple_header[i]);
  }
  
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
    for (i= 0; i < vsize_words; i++) {
      checksum ^= var_data_part[i];
    }
  }
#endif
  return checksum;
}

/* ----------------------------------------------------------------- */
/* -----------       INSERT_ACTIVE_OP_LIST            -------------- */
/* ----------------------------------------------------------------- */
bool 
Dbtup::insertActiveOpList(OperationrecPtr regOperPtr,
			  KeyReqStruct* req_struct)
{
  OperationrecPtr prevOpPtr;
  ndbrequire(!regOperPtr.p->op_struct.in_active_list);
  regOperPtr.p->op_struct.in_active_list= true;
  req_struct->prevOpPtr.i= 
    prevOpPtr.i= req_struct->m_tuple_ptr->m_operation_ptr_i;
  regOperPtr.p->prevActiveOp= prevOpPtr.i;
  regOperPtr.p->nextActiveOp= RNIL;
  regOperPtr.p->m_undo_buffer_space= 0;
  req_struct->m_tuple_ptr->m_operation_ptr_i= regOperPtr.i;
  if (prevOpPtr.i == RNIL) {
    set_change_mask_state(regOperPtr.p, USE_SAVED_CHANGE_MASK);
    regOperPtr.p->saved_change_mask[0] = 0;
    regOperPtr.p->saved_change_mask[1] = 0;
    return true;
  } else {
    req_struct->prevOpPtr.p= prevOpPtr.p= c_operation_pool.getPtr(prevOpPtr.i);
    prevOpPtr.p->nextActiveOp= regOperPtr.i;

    regOperPtr.p->op_struct.m_wait_log_buffer= 
      prevOpPtr.p->op_struct.m_wait_log_buffer;
    regOperPtr.p->op_struct.m_load_diskpage_on_commit= 
      prevOpPtr.p->op_struct.m_load_diskpage_on_commit;
    regOperPtr.p->m_undo_buffer_space= prevOpPtr.p->m_undo_buffer_space;
    // start with prev mask (matters only for UPD o UPD)
    set_change_mask_state(regOperPtr.p, get_change_mask_state(prevOpPtr.p));
    regOperPtr.p->saved_change_mask[0] = prevOpPtr.p->saved_change_mask[0];
    regOperPtr.p->saved_change_mask[1] = prevOpPtr.p->saved_change_mask[1];

    prevOpPtr.p->op_struct.m_wait_log_buffer= 0;
    prevOpPtr.p->op_struct.m_load_diskpage_on_commit= 0;

    if(prevOpPtr.p->op_struct.tuple_state == TUPLE_PREPARED)
    {
      Uint32 op= regOperPtr.p->op_struct.op_type;
      Uint32 prevOp= prevOpPtr.p->op_struct.op_type;
      if (prevOp == ZDELETE)
      {
	if(op == ZINSERT)
	{
	  // mark both
	  prevOpPtr.p->op_struct.delete_insert_flag= true;
	  regOperPtr.p->op_struct.delete_insert_flag= true;
	  return true;
	} else {
	  terrorCode= ZTUPLE_DELETED_ERROR;
	  return false;
	}
      } 
      else if(op == ZINSERT && prevOp != ZDELETE)
      {
	terrorCode= ZINSERT_ERROR;
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
		  Fragrecord* regFragPtr,
		  Tablerec* regTabPtr,
		  bool disk)
{
  OperationrecPtr currOpPtr;
  currOpPtr.i= req_struct->m_tuple_ptr->m_operation_ptr_i;
  if (currOpPtr.i == RNIL)
  {
    if (regTabPtr->need_expand(disk))
      prepare_read(req_struct, regTabPtr, disk);
    return true;
  }

  do {
    Uint32 savepointId= regOperPtr->savepointId;
    bool dirty= req_struct->dirty_op;
    
    c_operation_pool.getPtr(currOpPtr);
    bool sameTrans= c_lqh->is_same_trans(currOpPtr.p->userpointer,
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

    OperationrecPtr prevOpPtr = currOpPtr;  
    bool found= false;
    while(true) 
    {
      if (savepointId > currOpPtr.p->savepointId) {
	found= true;
	break;
      }
      if (currOpPtr.p->is_first_operation()){
	break;
      }
      prevOpPtr= currOpPtr;
      currOpPtr.i = currOpPtr.p->prevActiveOp;
      c_operation_pool.getPtr(currOpPtr);
    }
    
    Uint32 currOp= currOpPtr.p->op_struct.op_type;
    
    if((found && currOp == ZDELETE) || 
       ((dirty || !found) && currOp == ZINSERT))
    {
      terrorCode= ZTUPLE_DELETED_ERROR;
      break;
    }
    
    if(dirty || !found)
    {
      
    }
    else
    {
      req_struct->m_tuple_ptr= (Tuple_header*)
	c_undo_buffer.get_ptr(&currOpPtr.p->m_copy_tuple_location);
    }      

    if (regTabPtr->need_expand(disk))
      prepare_read(req_struct, regTabPtr, disk);
    
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
		     Uint32 local_key, Uint32 flags)
{
  c_operation_pool.getPtr(operPtr, opRec);
  fragptr.i= fragPtrI;
  ptrCheckGuard(fragptr, cnoOfFragrec, fragrecord);
  
  Operationrec *  regOperPtr= operPtr.p;
  Fragrecord * regFragPtr= fragptr.p;
  
  tabptr.i = regFragPtr->fragTableId;
  ptrCheckGuard(tabptr, cnoOfTablerec, tablerec);
  Tablerec* regTabPtr = tabptr.p;
  
  if(local_key == ~(Uint32)0)
  {
    jam();
    regOperPtr->op_struct.m_wait_log_buffer= 1;
    regOperPtr->op_struct.m_load_diskpage_on_commit= 1;
    return 1;
  }
  
  jam();
  Uint32 page_idx= local_key & MAX_TUPLES_PER_PAGE;
  Uint32 frag_page_id= local_key >> MAX_TUPLES_BITS;
  regOperPtr->m_tuple_location.m_page_no= getRealpid(regFragPtr,
						     frag_page_id);
  regOperPtr->m_tuple_location.m_page_idx= page_idx;
  
  PagePtr page_ptr;
  Uint32* tmp= get_ptr(&page_ptr, &regOperPtr->m_tuple_location, regTabPtr);
  Tuple_header* ptr= (Tuple_header*)tmp;
  
  int res= 1;
  Uint32 opPtr= ptr->m_operation_ptr_i;
  if(ptr->m_header_bits & Tuple_header::DISK_PART)
  {
    Page_cache_client::Request req;
    memcpy(&req.m_page, ptr->get_disk_ref_ptr(regTabPtr), sizeof(Local_key));
    req.m_callback.m_callbackData= opRec;
    req.m_callback.m_callbackFunction= 
      safe_cast(&Dbtup::disk_page_load_callback);

#ifdef ERROR_INSERT
    if (ERROR_INSERTED(4022))
    {
      flags |= Page_cache_client::DELAY_REQ;
      req.m_delay_until_time = NdbTick_CurrentMillisecond()+(Uint64)3000;
    }
#endif
    
    if((res= m_pgman.get_page(signal, req, flags)) > 0)
    {
      //ndbout_c("in cache");
      // In cache
    } 
    else if(res == 0)
    {
      //ndbout_c("waiting for callback");
      // set state
    }
    else 
    {
      // Error
    }
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
    regOperPtr->op_struct.m_wait_log_buffer= 1;
    regOperPtr->op_struct.m_load_diskpage_on_commit= 1;
  }
  return res;
}

void
Dbtup::disk_page_load_callback(Signal* signal, Uint32 opRec, Uint32 page_id)
{
  c_operation_pool.getPtr(operPtr, opRec);
  c_lqh->acckeyconf_load_diskpage_callback(signal, 
					   operPtr.p->userpointer, page_id);
}

int
Dbtup::load_diskpage_scan(Signal* signal, 
			  Uint32 opRec, Uint32 fragPtrI, 
			  Uint32 local_key, Uint32 flags)
{
  c_operation_pool.getPtr(operPtr, opRec);
  fragptr.i= fragPtrI;
  ptrCheckGuard(fragptr, cnoOfFragrec, fragrecord);
  
  Operationrec *  regOperPtr= operPtr.p;
  Fragrecord * regFragPtr= fragptr.p;
  
  tabptr.i = regFragPtr->fragTableId;
  ptrCheckGuard(tabptr, cnoOfTablerec, tablerec);
  Tablerec* regTabPtr = tabptr.p;
  
  jam();
  Uint32 page_idx= local_key & MAX_TUPLES_PER_PAGE;
  Uint32 frag_page_id= local_key >> MAX_TUPLES_BITS;
  regOperPtr->m_tuple_location.m_page_no= getRealpid(regFragPtr,
						     frag_page_id);
  regOperPtr->m_tuple_location.m_page_idx= page_idx;
  regOperPtr->op_struct.m_load_diskpage_on_commit= 0;
  
  PagePtr page_ptr;
  Uint32* tmp= get_ptr(&page_ptr, &regOperPtr->m_tuple_location, regTabPtr);
  Tuple_header* ptr= (Tuple_header*)tmp;
  
  int res= 1;
  Uint32 opPtr= ptr->m_operation_ptr_i;
  if(ptr->m_header_bits & Tuple_header::DISK_PART)
  {
    Page_cache_client::Request req;
    memcpy(&req.m_page, ptr->get_disk_ref_ptr(regTabPtr), sizeof(Local_key));
    req.m_callback.m_callbackData= opRec;
    req.m_callback.m_callbackFunction= 
      safe_cast(&Dbtup::disk_page_load_scan_callback);
    
    if((res= m_pgman.get_page(signal, req, flags)) > 0)
    {
      // ndbout_c("in cache");
      // In cache
    } 
    else if(res == 0)
    {
      //ndbout_c("waiting for callback");
      // set state
    }
    else 
    {
      // Error
    }
  }
  return res;
}

void
Dbtup::disk_page_load_scan_callback(Signal* signal, 
				    Uint32 opRec, Uint32 page_id)
{
  c_operation_pool.getPtr(operPtr, opRec);
  c_lqh->next_scanconf_load_diskpage_callback(signal, 
					      operPtr.p->userpointer, page_id);
}

void Dbtup::execTUPKEYREQ(Signal* signal) 
{
   TupKeyReq * tupKeyReq= (TupKeyReq *)signal->getDataPtr();
   KeyReqStruct req_struct;
   Uint32 sig1, sig2, sig3, sig4;

   Uint32 RoperPtr= tupKeyReq->connectPtr;
   Uint32 Rfragptr= tupKeyReq->fragPtr;

   Uint32 RnoOfFragrec= cnoOfFragrec;
   Uint32 RnoOfTablerec= cnoOfTablerec;

   jamEntry();
   fragptr.i= Rfragptr;

   ndbrequire(Rfragptr < RnoOfFragrec);

   c_operation_pool.getPtr(operPtr, RoperPtr);
   ptrAss(fragptr, fragrecord);

   Uint32 TrequestInfo= tupKeyReq->request;

   Operationrec *  regOperPtr= operPtr.p;
   Fragrecord * regFragPtr= fragptr.p;

   tabptr.i = regFragPtr->fragTableId;
   ptrCheckGuard(tabptr, RnoOfTablerec, tablerec);
   Tablerec* regTabPtr = tabptr.p;

   req_struct.signal= signal;
   req_struct.dirty_op= TrequestInfo & 1;
   req_struct.interpreted_exec= (TrequestInfo >> 10) & 1;
   req_struct.no_fired_triggers= 0;
   req_struct.read_length= 0;
   req_struct.max_attr_id_updated= 0;
   req_struct.no_changed_attrs= 0;
   req_struct.last_row= false;
   req_struct.changeMask.clear();

   if (unlikely(get_trans_state(regOperPtr) != TRANS_IDLE))
   {
     TUPKEY_abort(signal, 39);
     return;
   }

 /* ----------------------------------------------------------------- */
 // Operation is ZREAD when we arrive here so no need to worry about the
 // abort process.
 /* ----------------------------------------------------------------- */
 /* -----------    INITIATE THE OPERATION RECORD       -------------- */
 /* ----------------------------------------------------------------- */
   Uint32 Rstoredid= tupKeyReq->storedProcedure;

   regOperPtr->fragmentPtr= Rfragptr;
   regOperPtr->op_struct.op_type= (TrequestInfo >> 6) & 0xf;
   regOperPtr->op_struct.delete_insert_flag = false;
   regOperPtr->storedProcedureId= Rstoredid;

   regOperPtr->m_copy_tuple_location.setNull();
   regOperPtr->tupVersion= ZNIL;

   sig1= tupKeyReq->savePointId;
   sig2= tupKeyReq->primaryReplica;
   sig3= tupKeyReq->keyRef2;
   
   regOperPtr->savepointId= sig1;
   regOperPtr->op_struct.primary_replica= sig2;
   Uint32 pageidx = regOperPtr->m_tuple_location.m_page_idx= sig3;

   sig1= tupKeyReq->opRef;
   sig2= tupKeyReq->tcOpIndex;
   sig3= tupKeyReq->coordinatorTC;
   sig4= tupKeyReq->keyRef1;

   req_struct.tc_operation_ptr= sig1;
   req_struct.TC_index= sig2;
   req_struct.TC_ref= sig3;
   Uint32 pageid = req_struct.frag_page_id= sig4;
   req_struct.m_use_rowid = (TrequestInfo >> 11) & 1;

   sig1= tupKeyReq->attrBufLen;
   sig2= tupKeyReq->applRef;
   sig3= tupKeyReq->transId1;
   sig4= tupKeyReq->transId2;

   Uint32 disk_page= tupKeyReq->disk_page;
   
   req_struct.log_size= sig1;
   req_struct.attrinfo_len= sig1;
   req_struct.rec_blockref= sig2;
   req_struct.trans_id1= sig3;
   req_struct.trans_id2= sig4;
   req_struct.m_disk_page_ptr.i= disk_page;

   sig1 = tupKeyReq->m_row_id_page_no;
   sig2 = tupKeyReq->m_row_id_page_idx;

   req_struct.m_row_id.m_page_no = sig1;
   req_struct.m_row_id.m_page_idx = sig2;
   
   Uint32 Roptype = regOperPtr->op_struct.op_type;

   if (Rstoredid != ZNIL) {
     ndbrequire(initStoredOperationrec(regOperPtr,
				       &req_struct,
				       Rstoredid) == ZOK);
   }

   copyAttrinfo(regOperPtr, &cinBuffer[0]);
   
   Uint32 localkey = (pageid << MAX_TUPLES_BITS) + pageidx;
   if(Roptype == ZINSERT && localkey == ~0)
   {
     // No tuple allocatated yet
     goto do_insert;
   }

   /**
    * Get pointer to tuple
    */
   regOperPtr->m_tuple_location.m_page_no= getRealpid(regFragPtr, 
						      req_struct.frag_page_id);
   
   setup_fixed_part(&req_struct, regOperPtr, regTabPtr);
   
   /**
    * Check operation
    */
   if (Roptype == ZREAD) {
     jam();
     
     if (setup_read(&req_struct, regOperPtr, regFragPtr, regTabPtr, 
		    disk_page != RNIL))
     {
       if(handleReadReq(signal, regOperPtr, regTabPtr, &req_struct) != -1) 
       {
	 req_struct.log_size= 0;
	 sendTUPKEYCONF(signal, &req_struct, regOperPtr);
	 /* ---------------------------------------------------------------- */
	 // Read Operations need not to be taken out of any lists. 
	 // We also do not need to wait for commit since there is no changes 
	 // to commit. Thus we
	 // prepare the operation record already now for the next operation.
	 // Write operations have set the state to STARTED above indicating 
	 // that they are waiting for the Commit or Abort decision.
	 /* ---------------------------------------------------------------- */
	 set_trans_state(regOperPtr, TRANS_IDLE);
	 regOperPtr->currentAttrinbufLen= 0;
       }
       return;
     }
     tupkeyErrorLab(signal);
     return;
   }
   
   if(insertActiveOpList(operPtr, &req_struct))
   {
     if(Roptype == ZINSERT)
     {
       jam();
   do_insert:
       if (handleInsertReq(signal, operPtr,
			   fragptr, regTabPtr, &req_struct) == -1) 
       {
	 return;
       }
       if (!regTabPtr->tuxCustomTriggers.isEmpty()) 
       {
	 jam();
	 if (executeTuxInsertTriggers(signal,
				      regOperPtr,
				      regFragPtr,
				      regTabPtr) != 0) {
	   jam();
           /*
            * TUP insert succeeded but add of TUX entries failed.  All
            * TUX changes have been rolled back at this point.
            *
            * We will abort via tupkeyErrorLab() as usual.  This routine
            * however resets the operation to ZREAD.  The TUP_ABORTREQ
            * arriving later cannot then undo the insert.
            *
            * Therefore we call TUP_ABORTREQ already now.  Diskdata etc
            * should be in memory and timeslicing cannot occur.  We must
            * skip TUX abort triggers since TUX is already aborted.
            */
           signal->theData[0] = operPtr.i;
           do_tup_abortreq(signal, ZSKIP_TUX_TRIGGERS);
	   tupkeyErrorLab(signal);
	   return;
	 }
       }
       checkImmediateTriggersAfterInsert(&req_struct,
					 regOperPtr,
					 regTabPtr);
       set_change_mask_state(regOperPtr, SET_ALL_MASK);
       sendTUPKEYCONF(signal, &req_struct, regOperPtr);
       return;
     }

     if (Roptype == ZUPDATE) {
       jam();
       if (handleUpdateReq(signal, regOperPtr,
			   regFragPtr, regTabPtr, &req_struct, disk_page != RNIL) == -1) {
	 return;
       }
       // If update operation is done on primary, 
       // check any after op triggers
       terrorCode= 0;
       if (!regTabPtr->tuxCustomTriggers.isEmpty()) {
	 jam();
	 if (executeTuxUpdateTriggers(signal,
				      regOperPtr,
				      regFragPtr,
				      regTabPtr) != 0) {
	   jam();
           /*
            * See insert case.
            */
           signal->theData[0] = operPtr.i;
           do_tup_abortreq(signal, ZSKIP_TUX_TRIGGERS);
	   tupkeyErrorLab(signal);
	   return;
	 }
       }
       checkImmediateTriggersAfterUpdate(&req_struct,
					 regOperPtr,
					 regTabPtr);
       // XXX use terrorCode for now since all methods are void
       if (terrorCode != 0) 
       {
	 tupkeyErrorLab(signal);
	 return;
       }
       update_change_mask_info(&req_struct, regOperPtr);
       sendTUPKEYCONF(signal, &req_struct, regOperPtr);
       return;
     } 
     else if(Roptype == ZDELETE)
     {
       jam();
       if (handleDeleteReq(signal, regOperPtr,
			   regFragPtr, regTabPtr, &req_struct) == -1) {
	 return;
       }
       /*
	* TUX doesn't need to check for triggers at delete since entries in
	* the index are kept until commit time.
	*/

       /*
	* Secondary index triggers fire on the primary after a delete.
	*/
       checkImmediateTriggersAfterDelete(&req_struct,
					 regOperPtr, 
					 regTabPtr);
       set_change_mask_state(regOperPtr, DELETE_CHANGES);
       req_struct.log_size= 0;
       sendTUPKEYCONF(signal, &req_struct, regOperPtr);
       return;
     }
     else
     {
       ndbrequire(false); // Invalid op type
     }
   }

   tupkeyErrorLab(signal);
 }

void
Dbtup::setup_fixed_part(KeyReqStruct* req_struct,
			Operationrec* regOperPtr,
			Tablerec* regTabPtr)
{
  PagePtr page_ptr;
  Uint32* ptr= get_ptr(&page_ptr, &regOperPtr->m_tuple_location, regTabPtr);
  req_struct->m_page_ptr = page_ptr;
  req_struct->m_tuple_ptr = (Tuple_header*)ptr;
  
  ndbassert(regOperPtr->op_struct.op_type == ZINSERT || (! (req_struct->m_tuple_ptr->m_header_bits & Tuple_header::FREE)));
  
  req_struct->check_offset[MM]= regTabPtr->get_check_offset(MM);
  req_struct->check_offset[DD]= regTabPtr->get_check_offset(DD);
  
  Uint32 num_attr= regTabPtr->m_no_of_attributes;
  Uint32 descr_start= regTabPtr->tabDescriptor;
  TableDescriptor *tab_descr= &tableDescriptor[descr_start];
  ndbrequire(descr_start + (num_attr << ZAD_LOG_SIZE) <= cnoOfTabDescrRec);
  req_struct->attr_descr= tab_descr; 
}

 /* ---------------------------------------------------------------- */
 /* ------------------------ CONFIRM REQUEST ----------------------- */
 /* ---------------------------------------------------------------- */
 void Dbtup::sendTUPKEYCONF(Signal* signal,
			    KeyReqStruct *req_struct,
			    Operationrec * regOperPtr)
{
  TupKeyConf * tupKeyConf= (TupKeyConf *)signal->getDataPtrSend();  
  
  Uint32 Rcreate_rowid = req_struct->m_use_rowid;
  Uint32 RuserPointer= regOperPtr->userpointer;
  Uint32 RnoFiredTriggers= req_struct->no_fired_triggers;
  Uint32 log_size= req_struct->log_size;
  Uint32 read_length= req_struct->read_length;
  Uint32 last_row= req_struct->last_row;
  
  set_trans_state(regOperPtr, TRANS_STARTED);
  set_tuple_state(regOperPtr, TUPLE_PREPARED);
  tupKeyConf->userPtr= RuserPointer;
  tupKeyConf->readLength= read_length;
  tupKeyConf->writeLength= log_size;
  tupKeyConf->noFiredTriggers= RnoFiredTriggers;
  tupKeyConf->lastRow= last_row;
  tupKeyConf->rowid = Rcreate_rowid;
  
  EXECUTE_DIRECT(DBLQH, GSN_TUPKEYCONF, signal,
		 TupKeyConf::SignalLength);
  
}


#define MAX_READ (sizeof(signal->theData) > MAX_MESSAGE_SIZE ? MAX_MESSAGE_SIZE : sizeof(signal->theData))

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
  if ((regTabPtr->m_bits & Tablerec::TR_Checksum) &&
      (calculateChecksum(req_struct->m_tuple_ptr, regTabPtr) != 0)) {
    jam();
    ndbout_c("here2");
    terrorCode= ZTUPLE_CORRUPTED_ERROR;
    tupkeyErrorLab(signal);
    return -1;
  }

  const Uint32 node = refToNode(sendBref);
  if(node != 0 && node != getOwnNodeId()) {
    start_index= 25;
  } else {
    jam();
    /**
     * execute direct
     */
    start_index= 3;
  }
  dst= &signal->theData[start_index];
  dstLen= (MAX_READ / 4) - start_index;
  if (!req_struct->interpreted_exec) {
    jam();
    int ret = readAttributes(req_struct,
			     &cinBuffer[0],
			     req_struct->attrinfo_len,
			     dst,
			     dstLen,
			     false);
    if (likely(ret != -1)) {
/* ------------------------------------------------------------------------- */
// We have read all data into coutBuffer. Now send it to the API.
/* ------------------------------------------------------------------------- */
      jam();
      Uint32 TnoOfDataRead= (Uint32) ret;
      req_struct->read_length= TnoOfDataRead;
      sendReadAttrinfo(signal, req_struct, TnoOfDataRead, regOperPtr);
      return 0;
    }
  } else {
    jam();
    if (likely(interpreterStartLab(signal, req_struct) != -1)) {
      return 0;
    }
    return -1;
  }

  jam();
  tupkeyErrorLab(signal);
  return -1;
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
  Uint32 *dst;
  Tuple_header *base= req_struct->m_tuple_ptr, *org;
  if ((dst= c_undo_buffer.alloc_copy_tuple(&operPtrP->m_copy_tuple_location,
					   regTabPtr->total_rec_size)) == 0)
  {
    terrorCode= ZMEM_NOMEM_ERROR;
    goto error;
  }

  Uint32 tup_version;
  if(operPtrP->is_first_operation())
  {
    org= req_struct->m_tuple_ptr;
    tup_version= org->get_tuple_version();
  }
  else
  {
    Operationrec* prevOp= req_struct->prevOpPtr.p;
    tup_version= prevOp->tupVersion;
    org= (Tuple_header*)c_undo_buffer.get_ptr(&prevOp->m_copy_tuple_location);
  }

  /**
   * Check consistency before update/delete
   */
  req_struct->m_tuple_ptr= org;
  if ((regTabPtr->m_bits & Tablerec::TR_Checksum) &&
      (calculateChecksum(req_struct->m_tuple_ptr, regTabPtr) != 0)) 
  {
    terrorCode= ZTUPLE_CORRUPTED_ERROR;
    goto error;
  }

  req_struct->m_tuple_ptr= (Tuple_header*)dst;

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
      operPtrP->op_struct.m_wait_log_buffer = 1;
      operPtrP->op_struct.m_load_diskpage_on_commit = 1;
      Uint32 sz= operPtrP->m_undo_buffer_space= 
	(sizeof(Dbtup::Disk_undo::Update) >> 2) + sizes[DD] - 1;
      
      terrorCode= c_lgman->alloc_log_space(regFragPtr->m_logfile_group_id,
					   sz);
      if(unlikely(terrorCode))
      {
	operPtrP->m_undo_buffer_space= 0;
	goto error;
      }
    }
  }
  else
  {
    memcpy(dst, org, 4*regTabPtr->m_offsets[MM].m_fix_header_size);
  }
  
  tup_version= (tup_version + 1) & ZTUP_VERSION_MASK;
  operPtrP->tupVersion= tup_version;
  
  int retValue;
  if (!req_struct->interpreted_exec) {
    jam();
    retValue= updateAttributes(req_struct,
                               &cinBuffer[0],
                               req_struct->attrinfo_len);
  } else {
    jam();
    if (unlikely(interpreterStartLab(signal, req_struct) == -1))
      return -1;
  }
  
  if (retValue == -1) {
    goto error;
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
  
  req_struct->m_tuple_ptr->set_tuple_version(tup_version);
  if (regTabPtr->m_bits & Tablerec::TR_Checksum) {
    jam();
    setChecksum(req_struct->m_tuple_ptr, regTabPtr);
  }
  return retValue;
  
error:
  tupkeyErrorLab(signal);  
  return -1;
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
  regOperPtr->op_struct.in_active_list= true;
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

  const Uint32 cnt1= regTabPtr->m_attributes[MM].m_no_of_varsize;
  const Uint32 cnt2= regTabPtr->m_attributes[DD].m_no_of_varsize;
  Uint32 *ptr= req_struct->m_tuple_ptr->get_var_part_ptr(regTabPtr);

  if(cnt1)
  {
    KeyReqStruct::Var_data* dst= &req_struct->m_var_data[MM];
    dst->m_data_ptr= (char*)(((Uint16*)ptr)+cnt1+1);
    dst->m_offset_array_ptr= req_struct->var_pos_array;
    dst->m_var_len_offset= cnt1;
    dst->m_max_var_offset= regTabPtr->m_offsets[MM].m_max_var_offset;
    // Disk part is 32-bit aligned
    ptr= ALIGN_WORD(dst->m_data_ptr+regTabPtr->m_offsets[MM].m_max_var_offset);
    order += regTabPtr->m_attributes[MM].m_no_of_fixsize;
    Uint32 pos= 0;
    Uint16 *pos_ptr = req_struct->var_pos_array;
    Uint16 *len_ptr = pos_ptr + cnt1;
    for(Uint32 i= 0; i<cnt1; i++)
    {
      * pos_ptr++ = pos;
      * len_ptr++ = pos;
      pos += AttributeDescriptor::getSizeInBytes(tab_descr[*order++].tabDescr);
    }
  } 
  else
  {
    ptr -= Tuple_header::HeaderSize;
  }

  req_struct->m_disk_ptr= (Tuple_header*)ptr;
  
  if(cnt2)
  {
    KeyReqStruct::Var_data *dst= &req_struct->m_var_data[DD];
    ptr=((Tuple_header*)ptr)->m_data+regTabPtr->m_offsets[DD].m_varpart_offset;
    dst->m_data_ptr= (char*)(((Uint16*)ptr)+cnt2+1);
    dst->m_offset_array_ptr= req_struct->var_pos_array + (cnt1 << 1);
    dst->m_var_len_offset= cnt2;
    dst->m_max_var_offset= regTabPtr->m_offsets[DD].m_max_var_offset;
  }
  
  // Set all null bits
  memset(req_struct->m_tuple_ptr->m_null_bits+
	 regTabPtr->m_offsets[MM].m_null_offset, 0xFF, 
	 4*regTabPtr->m_offsets[MM].m_null_words);
  memset(req_struct->m_disk_ptr->m_null_bits+
	 regTabPtr->m_offsets[DD].m_null_offset, 0xFF, 
	 4*regTabPtr->m_offsets[DD].m_null_words);
  req_struct->m_tuple_ptr->m_header_bits= 
    disk_undo ? (Tuple_header::DISK_ALLOC | Tuple_header::DISK_INLINE) : 0;
}

int Dbtup::handleInsertReq(Signal* signal,
                           Ptr<Operationrec> regOperPtr,
                           Ptr<Fragrecord> fragPtr,
                           Tablerec* regTabPtr,
                           KeyReqStruct *req_struct)
{
  Uint32 tup_version = 1;
  Fragrecord* regFragPtr = fragPtr.p;
  Uint32 *dst, *ptr= 0;
  Tuple_header *base= req_struct->m_tuple_ptr, *org= base;
  Tuple_header *tuple_ptr;
    
  bool disk = regTabPtr->m_no_of_disk_attributes > 0;
  bool mem_insert = regOperPtr.p->is_first_operation();
  bool disk_insert = mem_insert && disk;
  bool varsize = regTabPtr->m_attributes[MM].m_no_of_varsize;
  bool rowid = req_struct->m_use_rowid;
  Uint32 real_page_id = regOperPtr.p->m_tuple_location.m_page_no;
  Uint32 frag_page_id = req_struct->frag_page_id;

  union {
    Uint32 sizes[4];
    Uint64 cmp[2];
  };

  if (ERROR_INSERTED(4014))
  {
    dst = 0;
    goto undo_buffer_error;
  }

  dst= c_undo_buffer.alloc_copy_tuple(&regOperPtr.p->m_copy_tuple_location,
				      regTabPtr->total_rec_size);
  if (unlikely(dst == 0))
  {
    goto undo_buffer_error;
  }
  tuple_ptr= req_struct->m_tuple_ptr= (Tuple_header*)dst;

  if(mem_insert)
  {
    jam();
    prepare_initial_insert(req_struct, regOperPtr.p, regTabPtr);
  }
  else
  {
    Operationrec* prevOp= req_struct->prevOpPtr.p;
    ndbassert(prevOp->op_struct.op_type == ZDELETE);
    tup_version= prevOp->tupVersion + 1;
    
    if(!prevOp->is_first_operation())
      org= (Tuple_header*)c_undo_buffer.get_ptr(&prevOp->m_copy_tuple_location);
    if (regTabPtr->need_expand())
      expand_tuple(req_struct, sizes, org, regTabPtr, !disk_insert);
    else
      memcpy(dst, org, 4*regTabPtr->m_offsets[MM].m_fix_header_size);
  }
  
  if (disk_insert)
  {
    int res;
    
    if (ERROR_INSERTED(4015))
    {
      terrorCode = 1501;
      goto log_space_error;
    }

    res= c_lgman->alloc_log_space(regFragPtr->m_logfile_group_id,
				  regOperPtr.p->m_undo_buffer_space);
    if(unlikely(res))
    {
      terrorCode= res;
      goto log_space_error;
    }
  }
  
  regOperPtr.p->tupVersion= tup_version & ZTUP_VERSION_MASK;
  tuple_ptr->set_tuple_version(tup_version);

  if (ERROR_INSERTED(4016))
  {
    terrorCode = ZAI_INCONSISTENCY_ERROR;
    goto update_error;
  }

  if(unlikely(updateAttributes(req_struct, &cinBuffer[0], 
			       req_struct->attrinfo_len) == -1))
  {
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
  
  if (regTabPtr->need_shrink())
  {  
    shrink_tuple(req_struct, sizes+2, regTabPtr, true);
  }
  
  /**
   * Alloc memory
   */
  if(mem_insert)
  {
    if (!rowid)
    {
      if (ERROR_INSERTED(4018))
      {
	goto mem_error;
      }

      if (!varsize)
      {
	jam();
	ptr= alloc_fix_rec(regFragPtr,
			   regTabPtr,
			   &regOperPtr.p->m_tuple_location,
			   &frag_page_id);
      } 
      else 
      {
	jam();
	regOperPtr.p->m_tuple_location.m_file_no= sizes[2+MM];
	ptr= alloc_var_rec(regFragPtr, regTabPtr,
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
      
      if (!varsize)
      {
	jam();
	ptr= alloc_fix_rowid(regFragPtr,
			     regTabPtr,
			     &regOperPtr.p->m_tuple_location,
			     &frag_page_id);
      } 
      else 
      {
	jam();
	regOperPtr.p->m_tuple_location.m_file_no= sizes[2+MM];
	ptr= alloc_var_rowid(regFragPtr, regTabPtr,
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
    regOperPtr.p->m_tuple_location.m_page_no= frag_page_id;
    c_lqh->accminupdate(signal,
			regOperPtr.p->userpointer,
			&regOperPtr.p->m_tuple_location);
    
    base = (Tuple_header*)ptr;
    base->m_operation_ptr_i= regOperPtr.i;
    base->m_header_bits= Tuple_header::ALLOC | 
      (varsize ? Tuple_header::CHAINED_ROW : 0);
    regOperPtr.p->m_tuple_location.m_page_no = real_page_id;
  }
  else 
  {
    int ret;
    if (ERROR_INSERTED(4020))
    {
      goto size_change_error;
    }

    if (regTabPtr->need_shrink() && cmp[0] != cmp[1] &&
	unlikely(ret = handle_size_change_after_update(req_struct,
						       base,
						       regOperPtr.p,
						       regFragPtr,
						       regTabPtr,
						       sizes)))
    {
      goto size_change_error;
    }
    req_struct->m_use_rowid = false;
    base->m_header_bits &= ~(Uint32)Tuple_header::FREE;
  }

  base->m_header_bits |= Tuple_header::ALLOC & 
    (regOperPtr.p->is_first_operation() ? ~0 : 1);
  
  if (disk_insert)
  {
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
      terrorCode = -ret;
      goto disk_prealloc_error;
    }
    
    regOperPtr.p->op_struct.m_disk_preallocated= 1;
    tmp.m_page_idx= size;
    memcpy(tuple_ptr->get_disk_ref_ptr(regTabPtr), &tmp, sizeof(tmp));
    
    /**
     * Set ref from disk to mm
     */
    Local_key ref = regOperPtr.p->m_tuple_location;
    ref.m_page_no = frag_page_id;
    
    Tuple_header* disk_ptr= req_struct->m_disk_ptr;
    disk_ptr->m_header_bits = 0;
    disk_ptr->m_base_record_ref= ref.ref();
  }
  
  if (regTabPtr->m_bits & Tablerec::TR_Checksum) 
  {
    jam();
    setChecksum(req_struct->m_tuple_ptr, regTabPtr);
  }
  return 0;
  
size_change_error:
  jam();
  terrorCode = ZMEM_NOMEM_ERROR;
  goto exit_error;
  
undo_buffer_error:
  jam();
  terrorCode= ZMEM_NOMEM_ERROR;
  regOperPtr.p->m_undo_buffer_space = 0;
  if (mem_insert)
    regOperPtr.p->m_tuple_location.setNull();
  regOperPtr.p->m_copy_tuple_location.setNull();
  tupkeyErrorLab(signal);  
  return -1;
  
null_check_error:
  jam();
  terrorCode= ZNO_ILLEGAL_NULL_ATTR;
  goto update_error;

mem_error:
  jam();
  terrorCode= ZMEM_NOMEM_ERROR;
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
    regOperPtr.p->op_struct.in_active_list = false;
    regOperPtr.p->m_tuple_location.setNull();
  }
exit_error:
  tupkeyErrorLab(signal);
  return -1;

disk_prealloc_error:
  base->m_header_bits |= Tuple_header::FREED;
  goto exit_error;
}

/* ---------------------------------------------------------------- */
/* ---------------------------- DELETE ---------------------------- */
/* ---------------------------------------------------------------- */
int Dbtup::handleDeleteReq(Signal* signal,
                           Operationrec* regOperPtr,
                           Fragrecord* regFragPtr,
                           Tablerec* regTabPtr,
                           KeyReqStruct *req_struct)
{
  // delete must set but not increment tupVersion
  if (!regOperPtr->is_first_operation())
  {
    Operationrec* prevOp= req_struct->prevOpPtr.p;
    regOperPtr->tupVersion= prevOp->tupVersion;
    regOperPtr->m_copy_tuple_location= prevOp->m_copy_tuple_location;
  } 
  else 
  {
    regOperPtr->tupVersion= req_struct->m_tuple_ptr->get_tuple_version();
    if(regTabPtr->m_no_of_disk_attributes)
    {
      Uint32 sz;
      if(regTabPtr->m_attributes[DD].m_no_of_varsize)
      {
	/**
	 * Need to have page in memory to read size 
	 *   to alloc undo space
	 */
	abort();
      }
      else
	sz= (sizeof(Dbtup::Disk_undo::Free) >> 2) + 
	  regTabPtr->m_offsets[DD].m_fix_header_size - 1;
      
      regOperPtr->m_undo_buffer_space= sz;
      
      int res;
      if((res= c_lgman->alloc_log_space(regFragPtr->m_logfile_group_id, 
					sz)))
      {
	terrorCode= res;
	regOperPtr->m_undo_buffer_space= 0;
	goto error;
      }
      
    }
  }
  if (req_struct->attrinfo_len == 0)
  {
    return 0;
  }
  
  return handleReadReq(signal, regOperPtr, regTabPtr, req_struct);

error:
  tupkeyErrorLab(signal);
  return -1;
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
/* THE FIRST FIVE WORDS WILL GIVE THE LENGTH OF THE FIVEE REGIONS   */
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
/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */
/* ----------------- INTERPRETED EXECUTION  ----------------------- */
/* ---------------------------------------------------------------- */
int Dbtup::interpreterStartLab(Signal* signal,
                               KeyReqStruct *req_struct)
{
  Operationrec *  const regOperPtr= operPtr.p;
  int TnoDataRW;
  Uint32 RtotalLen, start_index, dstLen;
  Uint32 *dst;

  Uint32 RinitReadLen= cinBuffer[0];
  Uint32 RexecRegionLen= cinBuffer[1];
  Uint32 RfinalUpdateLen= cinBuffer[2];
  Uint32 RfinalRLen= cinBuffer[3];
  Uint32 RsubLen= cinBuffer[4];

  Uint32 RattrinbufLen= req_struct->attrinfo_len;
  const BlockReference sendBref= req_struct->rec_blockref;

  const Uint32 node = refToNode(sendBref);
  if(node != 0 && node != getOwnNodeId()) {
    start_index= 25;
  } else {
    jam();
    /**
     * execute direct
     */
    start_index= 3;
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
  Uint32 RlogSize= 0;
  if (((RtotalLen + 5) == RattrinbufLen) &&
      (RattrinbufLen >= 5) &&
      (RattrinbufLen < ZATTR_BUFFER_SIZE)) {
    /* ---------------------------------------------------------------- */
    // We start by checking consistency. We must have the first five
    // words of the ATTRINFO to give us the length of the regions. The
    // size of these regions must be the same as the total ATTRINFO
    // length and finally the total length must be within the limits.
    /* ---------------------------------------------------------------- */

    if (RinitReadLen > 0) {
      jam();
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
      if (TnoDataRW != -1) {
	RattroutCounter= TnoDataRW;
	RinstructionCounter += RinitReadLen;
      } else {
	jam();
	tupkeyErrorLab(signal);
	return -1;
      }
    }
    if (RexecRegionLen > 0) {
      jam();
      /* ---------------------------------------------------------------- */
      // The next step is the actual interpreted execution. This executes
      // a register-based virtual machine which can read and write attributes
      // to and from registers.
      /* ---------------------------------------------------------------- */
      Uint32 RsubPC= RinstructionCounter + RfinalUpdateLen + RfinalRLen;     
      TnoDataRW= interpreterNextLab(signal,
                                     req_struct,
				     &clogMemBuffer[0],
				     &cinBuffer[RinstructionCounter],
				     RexecRegionLen,
				     &cinBuffer[RsubPC],
				     RsubLen,
				     &coutBuffer[0],
				     sizeof(coutBuffer) / 4);
      if (TnoDataRW != -1) {
	RinstructionCounter += RexecRegionLen;
	RlogSize= TnoDataRW;
      } else {
	jam();
	tupkeyErrorLab(signal);
	return -1;
      }
    }
    if (RfinalUpdateLen > 0) {
      jam();
      /* ---------------------------------------------------------------- */
      // We can also apply a set of updates without any conditions as part
      // of the interpreted execution.
      /* ---------------------------------------------------------------- */
      if (regOperPtr->op_struct.op_type == ZUPDATE) {
	TnoDataRW= updateAttributes(req_struct,
				     &cinBuffer[RinstructionCounter],
				     RfinalUpdateLen);
	if (TnoDataRW != -1) {
	  MEMCOPY_NO_WORDS(&clogMemBuffer[RlogSize],
			   &cinBuffer[RinstructionCounter],
			   RfinalUpdateLen);
	  RinstructionCounter += RfinalUpdateLen;
	  RlogSize += RfinalUpdateLen;
	} else {
	  jam();
	  tupkeyErrorLab(signal);
	  return -1;
	}
      } else {
	return TUPKEY_abort(signal, 19);
      }
    }
    if (RfinalRLen > 0) {
      jam();
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
      if (TnoDataRW != -1) {
	RattroutCounter += TnoDataRW;
      } else {
	jam();
	tupkeyErrorLab(signal);
	return -1;
      }
    }
    req_struct->log_size= RlogSize;
    req_struct->read_length= RattroutCounter;
    sendReadAttrinfo(signal, req_struct, RattroutCounter, regOperPtr);
    if (RlogSize > 0) {
      sendLogAttrinfo(signal, RlogSize, regOperPtr);
    }
    return 0;
  } else {
    return TUPKEY_abort(signal, 22);
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
void Dbtup::sendLogAttrinfo(Signal* signal,
                            Uint32 TlogSize,
                            Operationrec *  const regOperPtr)

{
  Uint32 TbufferIndex= 0;
  signal->theData[0]= regOperPtr->userpointer;
  while (TlogSize > 22) {
    MEMCOPY_NO_WORDS(&signal->theData[3],
                     &clogMemBuffer[TbufferIndex],
                     22);
    EXECUTE_DIRECT(DBLQH, GSN_TUP_ATTRINFO, signal, 25);
    TbufferIndex += 22;
    TlogSize -= 22;
  }
  MEMCOPY_NO_WORDS(&signal->theData[3],
                   &clogMemBuffer[TbufferIndex],
                   TlogSize);
  EXECUTE_DIRECT(DBLQH, GSN_TUP_ATTRINFO, signal, 3 + TlogSize);
}

inline
Uint32 
brancher(Uint32 TheInstruction, Uint32 TprogramCounter)
{         
  Uint32 TbranchDirection= TheInstruction >> 31;
  Uint32 TbranchLength= (TheInstruction >> 16) & 0x7fff;
  TprogramCounter--;
  if (TbranchDirection == 1) {
    jam();
    /* ---------------------------------------------------------------- */
    /*       WE JUMP BACKWARDS.                                         */
    /* ---------------------------------------------------------------- */
    return (TprogramCounter - TbranchLength);
  } else {
    jam();
    /* ---------------------------------------------------------------- */
    /*       WE JUMP FORWARD.                                           */
    /* ---------------------------------------------------------------- */
    return (TprogramCounter + TbranchLength);
  }
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
  register Uint32 RnoOfInstructions= 0;
  register Uint32 TprogramCounter= 0;
  register Uint32 theInstruction;
  register Uint32 theRegister;
  Uint32 TdataWritten= 0;
  Uint32 RstackPtr= 0;
  union {
    Uint32 TregMemBuffer[32];
    Uint64 Tdummy[16];
  };
  Uint32 TstackMemBuffer[32];

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
    if (TprogramCounter < TcurrentSize) {
      TprogramCounter++;
      switch (Interpreter::getOpCode(theInstruction)) {
      case Interpreter::READ_ATTR_INTO_REG:
	jam();
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
	  if (TnoDataRW == 2) {
	    /* ------------------------------------------------------------- */
	    // Two words read means that we get the instruction plus one 32 
	    // word read. Thus we set the register to be a 32 bit register.
	    /* ------------------------------------------------------------- */
	    TregMemBuffer[theRegister]= 0x50;
            // arithmetic conversion if big-endian
            * (Int64*)(TregMemBuffer+theRegister+2)= TregMemBuffer[theRegister+1];
	  } else if (TnoDataRW == 3) {
	    /* ------------------------------------------------------------- */
	    // Three words read means that we get the instruction plus two 
	    // 32 words read. Thus we set the register to be a 64 bit register.
	    /* ------------------------------------------------------------- */
	    TregMemBuffer[theRegister]= 0x60;
            TregMemBuffer[theRegister+3]= TregMemBuffer[theRegister+2];
            TregMemBuffer[theRegister+2]= TregMemBuffer[theRegister+1];
	  } else if (TnoDataRW == 1) {
	    /* ------------------------------------------------------------- */
	    // One word read means that we must have read a NULL value. We set
	    // the register to indicate a NULL value.
	    /* ------------------------------------------------------------- */
	    TregMemBuffer[theRegister]= 0;
	    TregMemBuffer[theRegister + 2]= 0;
	    TregMemBuffer[theRegister + 3]= 0;
	  } else if (TnoDataRW == -1) {
	    jam();
	    tupkeyErrorLab(signal);
	    return -1;
	  } else {
	    /* ------------------------------------------------------------- */
	    // Any other return value from the read attribute here is not 
	    // allowed and will lead to a system crash.
	    /* ------------------------------------------------------------- */
	    ndbrequire(false);
	  }
	  break;
	}

      case Interpreter::WRITE_ATTR_FROM_REG:
	jam();
	{
	  Uint32 TattrId= theInstruction >> 16;
	  Uint32 TattrDescrIndex= tabptr.p->tabDescriptor +
	    (TattrId << ZAD_LOG_SIZE);
	  Uint32 TattrDesc1= tableDescriptor[TattrDescrIndex].tabDescr;
	  Uint32 TregType= TregMemBuffer[theRegister];

	  /* --------------------------------------------------------------- */
	  // Calculate the number of words of this attribute.
	  // We allow writes into arrays as long as they fit into the 64 bit
	  // register size.
	  /* --------------------------------------------------------------- */
          Uint32 TattrNoOfWords = AttributeDescriptor::getSizeInWords(TattrDesc1);
	  Uint32 Toptype = operPtr.p->op_struct.op_type;
	  Uint32 TdataForUpdate[3];
	  Uint32 Tlen;

	  AttributeHeader& ah= AttributeHeader::init(&TdataForUpdate[0], 
						      TattrId,
                                                      TattrNoOfWords << 2);
	  TdataForUpdate[1]= TregMemBuffer[theRegister + 2];
	  TdataForUpdate[2]= TregMemBuffer[theRegister + 3];
	  Tlen= TattrNoOfWords + 1;
	  if (Toptype == ZUPDATE) {
	    if (TattrNoOfWords <= 2) {
              if (TattrNoOfWords == 1) {
                // arithmetic conversion if big-endian
                TdataForUpdate[1] = *(Int64*)&TregMemBuffer[theRegister + 2];
                TdataForUpdate[2] = 0;
              }
	      if (TregType == 0) {
		/* --------------------------------------------------------- */
		// Write a NULL value into the attribute
		/* --------------------------------------------------------- */
		ah.setNULL();
		Tlen= 1;
	      }
	      int TnoDataRW= updateAttributes(req_struct,
					   &TdataForUpdate[0],
					   Tlen);
	      if (TnoDataRW != -1) {
		/* --------------------------------------------------------- */
		// Write the written data also into the log buffer so that it 
		// will be logged.
		/* --------------------------------------------------------- */
		logMemory[TdataWritten + 0]= TdataForUpdate[0];
		logMemory[TdataWritten + 1]= TdataForUpdate[1];
		logMemory[TdataWritten + 2]= TdataForUpdate[2];
		TdataWritten += Tlen;
	      } else {
		tupkeyErrorLab(signal);
		return -1;
	      }
	    } else {
	      return TUPKEY_abort(signal, 15);
	    }
	  } else {
	    return TUPKEY_abort(signal, 16);
	  }
	  break;
	}

      case Interpreter::LOAD_CONST_NULL:
	jam();
	TregMemBuffer[theRegister]= 0;	/* NULL INDICATOR */
	break;

      case Interpreter::LOAD_CONST16:
	jam();
	TregMemBuffer[theRegister]= 0x50;	/* 32 BIT UNSIGNED CONSTANT */
	* (Int64*)(TregMemBuffer+theRegister+2)= theInstruction >> 16;
	break;

      case Interpreter::LOAD_CONST32:
	jam();
	TregMemBuffer[theRegister]= 0x50;	/* 32 BIT UNSIGNED CONSTANT */
	* (Int64*)(TregMemBuffer+theRegister+2)= * 
	  (TcurrentProgram+TprogramCounter);
	TprogramCounter++;
	break;

      case Interpreter::LOAD_CONST64:
	jam();
	TregMemBuffer[theRegister]= 0x60;	/* 64 BIT UNSIGNED CONSTANT */
        TregMemBuffer[theRegister + 2 ]= * (TcurrentProgram +
                                             TprogramCounter++);
        TregMemBuffer[theRegister + 3 ]= * (TcurrentProgram +
                                             TprogramCounter++);
	break;

      case Interpreter::ADD_REG_REG:
	jam();
	{
	  Uint32 TrightRegister= Interpreter::getReg2(theInstruction) << 2;
	  Uint32 TdestRegister= Interpreter::getReg3(theInstruction) << 2;

	  Uint32 TrightType= TregMemBuffer[TrightRegister];
	  Int64 Tright0= * (Int64*)(TregMemBuffer + TrightRegister + 2);
	  

	  Uint32 TleftType= TregMemBuffer[theRegister];
	  Int64 Tleft0= * (Int64*)(TregMemBuffer + theRegister + 2);
         
	  if ((TleftType | TrightType) != 0) {
	    Uint64 Tdest0= Tleft0 + Tright0;
	    * (Int64*)(TregMemBuffer+TdestRegister+2)= Tdest0;
	    TregMemBuffer[TdestRegister]= 0x60;
	  } else {
	    return TUPKEY_abort(signal, 20);
	  }
	  break;
	}

      case Interpreter::SUB_REG_REG:
	jam();
	{
	  Uint32 TrightRegister= Interpreter::getReg2(theInstruction) << 2;
	  Uint32 TdestRegister= Interpreter::getReg3(theInstruction) << 2;

	  Uint32 TrightType= TregMemBuffer[TrightRegister];
	  Int64 Tright0= * (Int64*)(TregMemBuffer + TrightRegister + 2);
	  
	  Uint32 TleftType= TregMemBuffer[theRegister];
	  Int64 Tleft0= * (Int64*)(TregMemBuffer + theRegister + 2);
         
	  if ((TleftType | TrightType) != 0) {
	    Int64 Tdest0= Tleft0 - Tright0;
	    * (Int64*)(TregMemBuffer+TdestRegister+2)= Tdest0;
	    TregMemBuffer[TdestRegister]= 0x60;
	  } else {
	    return TUPKEY_abort(signal, 20);
	  }
	  break;
	}

      case Interpreter::BRANCH:
	TprogramCounter= brancher(theInstruction, TprogramCounter);
	break;

      case Interpreter::BRANCH_REG_EQ_NULL:
	if (TregMemBuffer[theRegister] != 0) {
	  jam();
	  continue;
	} else {
	  jam();
	  TprogramCounter= brancher(theInstruction, TprogramCounter);
	}
	break;

      case Interpreter::BRANCH_REG_NE_NULL:
	if (TregMemBuffer[theRegister] == 0) {
	  jam();
	  continue;
	} else {
	  jam();
	  TprogramCounter= brancher(theInstruction, TprogramCounter);
	}
	break;


      case Interpreter::BRANCH_EQ_REG_REG:
	{
	  Uint32 TrightRegister= Interpreter::getReg2(theInstruction) << 2;

	  Uint32 TleftType= TregMemBuffer[theRegister];
	  Uint32 Tleft0= TregMemBuffer[theRegister + 2];
	  Uint32 Tleft1= TregMemBuffer[theRegister + 3];

	  Uint32 TrightType= TregMemBuffer[TrightRegister];
	  Uint32 Tright0= TregMemBuffer[TrightRegister + 2];
	  Uint32 Tright1= TregMemBuffer[TrightRegister + 3];
	  if ((TrightType | TleftType) != 0) {
	    jam();
	    if ((Tleft0 == Tright0) && (Tleft1 == Tright1)) {
	      TprogramCounter= brancher(theInstruction, TprogramCounter);
	    }
	  } else {
	    return TUPKEY_abort(signal, 23);
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
	  if ((TrightType | TleftType) != 0) {
	    jam();
	    if ((Tleft0 != Tright0) || (Tleft1 != Tright1)) {
	      TprogramCounter= brancher(theInstruction, TprogramCounter);
	    }
	  } else {
	    return TUPKEY_abort(signal, 24);
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
         

	  if ((TrightType | TleftType) != 0) {
	    jam();
	    if (Tleft0 < Tright0) {
	      TprogramCounter= brancher(theInstruction, TprogramCounter);
	    }
	  } else {
	    return TUPKEY_abort(signal, 24);
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
	  

	  if ((TrightType | TleftType) != 0) {
	    jam();
	    if (Tleft0 <= Tright0) {
	      TprogramCounter= brancher(theInstruction, TprogramCounter);
	    }
	  } else {
	    return TUPKEY_abort(signal, 26);
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
	  

	  if ((TrightType | TleftType) != 0) {
	    jam();
	    if (Tleft0 > Tright0){
	      TprogramCounter= brancher(theInstruction, TprogramCounter);
	    }
	  } else {
	    return TUPKEY_abort(signal, 27);
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
	  

	  if ((TrightType | TleftType) != 0) {
	    jam();
	    if (Tleft0 >= Tright0){
	      TprogramCounter= brancher(theInstruction, TprogramCounter);
	    }
	  } else {
	    return TUPKEY_abort(signal, 28);
	  }
	  break;
	}

      case Interpreter::BRANCH_ATTR_OP_ARG:{
	jam();
	Uint32 cond = Interpreter::getBinaryCondition(theInstruction);
	Uint32 ins2 = TcurrentProgram[TprogramCounter];
	Uint32 attrId = Interpreter::getBranchCol_AttrId(ins2) << 16;
	Uint32 argLen = Interpreter::getBranchCol_Len(ins2);

	if(tmpHabitant != attrId){
	  Int32 TnoDataR = readAttributes(req_struct,
					  &attrId, 1,
					  tmpArea, tmpAreaSz,
                                          false);
	  
	  if (TnoDataR == -1) {
	    jam();
	    tupkeyErrorLab(signal);
	    return -1;
	  }
	  tmpHabitant= attrId;
	}

        // get type
	attrId >>= 16;
	Uint32 TattrDescrIndex = tabptr.p->tabDescriptor +
	  (attrId << ZAD_LOG_SIZE);
	Uint32 TattrDesc1 = tableDescriptor[TattrDescrIndex].tabDescr;
	Uint32 TattrDesc2 = tableDescriptor[TattrDescrIndex+1].tabDescr;
	Uint32 typeId = AttributeDescriptor::getType(TattrDesc1);
	void * cs = 0;
	if(AttributeOffset::getCharsetFlag(TattrDesc2))
	{
	  Uint32 pos = AttributeOffset::getCharsetPos(TattrDesc2);
	  cs = tabptr.p->charsetArray[pos];
	}
	const NdbSqlUtil::Type& sqlType = NdbSqlUtil::getType(typeId);

        // get data
	AttributeHeader ah(tmpArea[0]);
        const char* s1 = (char*)&tmpArea[1];
        const char* s2 = (char*)&TcurrentProgram[TprogramCounter+1];
        // fixed length in 5.0
	Uint32 attrLen = AttributeDescriptor::getSizeInBytes(TattrDesc1);

	bool r1_null = ah.isNULL();
	bool r2_null = argLen == 0;
	int res1;
        if (cond != Interpreter::LIKE &&
            cond != Interpreter::NOT_LIKE) {
          if (r1_null || r2_null) {
            // NULL==NULL and NULL<not-NULL
            res1 = r1_null && r2_null ? 0 : r1_null ? -1 : 1;
          } else {
            res1 = (*sqlType.m_cmp)(cs, s1, attrLen, s2, argLen, true);
          }
	} else {
          if (r1_null || r2_null) {
            // NULL like NULL is true (has no practical use)
            res1 =  r1_null && r2_null ? 0 : -1;
          } else {
            res1 = (*sqlType.m_like)(cs, s1, attrLen, s2, argLen);
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
          Uint32 tmp = ((argLen + 3) >> 2) + 1;
          TprogramCounter += tmp;
        }
	break;
      }
	
      case Interpreter::BRANCH_ATTR_EQ_NULL:{
	jam();
	Uint32 ins2= TcurrentProgram[TprogramCounter];
	Uint32 attrId= Interpreter::getBranchCol_AttrId(ins2) << 16;
	
	if (tmpHabitant != attrId){
	  Int32 TnoDataR= readAttributes(req_struct,
					  &attrId, 1,
					  tmpArea, tmpAreaSz,
                                          false);
	  
	  if (TnoDataR == -1) {
	    jam();
	    tupkeyErrorLab(signal);
	    return -1;
	  }
	  tmpHabitant= attrId;
	}
	
	AttributeHeader ah(tmpArea[0]);
	if (ah.isNULL()){
	  TprogramCounter= brancher(theInstruction, TprogramCounter);
	} else {
	  TprogramCounter ++;
	}
	break;
      }

      case Interpreter::BRANCH_ATTR_NE_NULL:{
	jam();
	Uint32 ins2= TcurrentProgram[TprogramCounter];
	Uint32 attrId= Interpreter::getBranchCol_AttrId(ins2) << 16;
	
	if (tmpHabitant != attrId){
	  Int32 TnoDataR= readAttributes(req_struct,
					  &attrId, 1,
					  tmpArea, tmpAreaSz,
                                          false);
	  
	  if (TnoDataR == -1) {
	    jam();
	    tupkeyErrorLab(signal);
	    return -1;
	  }
	  tmpHabitant= attrId;
	}
	
	AttributeHeader ah(tmpArea[0]);
	if (ah.isNULL()){
	  TprogramCounter ++;
	} else {
	  TprogramCounter= brancher(theInstruction, TprogramCounter);
	}
	break;
      }
	
      case Interpreter::EXIT_OK:
	jam();
#ifdef TRACE_INTERPRETER
	ndbout_c(" - exit_ok");
#endif
	return TdataWritten;

      case Interpreter::EXIT_OK_LAST:
	jam();
#ifdef TRACE_INTERPRETER
	ndbout_c(" - exit_ok_last");
#endif
	req_struct->last_row= true;
	return TdataWritten;
	
      case Interpreter::EXIT_REFUSE:
	jam();
#ifdef TRACE_INTERPRETER
	ndbout_c(" - exit_nok");
#endif
	terrorCode= theInstruction >> 16;
	return TUPKEY_abort(signal, 29);

      case Interpreter::CALL:
	jam();
	RstackPtr++;
	if (RstackPtr < 32) {
	  TstackMemBuffer[RstackPtr]= TprogramCounter + 1;
	  TprogramCounter= theInstruction >> 16;
	  if (TprogramCounter < TsubroutineLen) {
	    TcurrentProgram= subroutineProg;
	    TcurrentSize= TsubroutineLen;
	  } else {
	    return TUPKEY_abort(signal, 30);
	  }
	} else {
	  return TUPKEY_abort(signal, 31);
	}
	break;

      case Interpreter::RETURN:
	jam();
	if (RstackPtr > 0) {
	  TprogramCounter= TstackMemBuffer[RstackPtr];
	  RstackPtr--;
	  if (RstackPtr == 0) {
	    jam();
	    /* ------------------------------------------------------------- */
	    // We are back to the main program.
	    /* ------------------------------------------------------------- */
	    TcurrentProgram= mainProgram;
	    TcurrentSize= TmainProgLen;
	  }
	} else {
	  return TUPKEY_abort(signal, 32);
	}
	break;

      default:
	return TUPKEY_abort(signal, 33);
      }
    } else {
      return TUPKEY_abort(signal, 34);
    }
  }
  return TUPKEY_abort(signal, 35);
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
		    bool disk)
{
  Uint32 bits= src->m_header_bits;
  Tuple_header* ptr= req_struct->m_tuple_ptr;
  
  Uint16 dd_tot= tabPtrP->m_no_of_disk_attributes;
  Uint16 mm_vars= tabPtrP->m_attributes[MM].m_no_of_varsize;
  Uint32 fix_size= tabPtrP->m_offsets[MM].m_varpart_offset;
  Uint32 order_desc= tabPtrP->m_real_order_descriptor;

  Uint32 *dst_ptr= ptr->get_var_part_ptr(tabPtrP);
  const Uint32 *disk_ref= src->get_disk_ref_ptr(tabPtrP);
  const Uint32 *src_ptr= src->get_var_part_ptr(tabPtrP);
  const Uint32 * desc= (Uint32*)req_struct->attr_descr;
  const Uint16 *order = (Uint16*)(&tableDescriptor[order_desc]);
  order += tabPtrP->m_attributes[MM].m_no_of_fixsize;
  
  if(mm_vars)
  {

    Uint32 step; // in bytes
    const Uint32 *src_data= src_ptr;
    KeyReqStruct::Var_data* dst= &req_struct->m_var_data[MM];
    if(bits & Tuple_header::CHAINED_ROW)
    {
      Ptr<Page> var_page;
      src_data= get_ptr(&var_page, * (Var_part_ref*)src_ptr);
      step= 4;
      sizes[MM]= (2 + (mm_vars << 1) + ((Uint16*)src_data)[mm_vars] + 3) >> 2;
      req_struct->m_varpart_page_ptr = var_page;
    }
    else
    {
      step= (2 + (mm_vars << 1) + ((Uint16*)src_ptr)[mm_vars]);
      sizes[MM]= (step + 3) >> 2;
      req_struct->m_varpart_page_ptr = req_struct->m_page_ptr;
    }
    dst->m_data_ptr= (char*)(((Uint16*)dst_ptr)+mm_vars+1);
    dst->m_offset_array_ptr= req_struct->var_pos_array;
    dst->m_var_len_offset= mm_vars;
    dst->m_max_var_offset= tabPtrP->m_offsets[MM].m_max_var_offset;
    
    dst_ptr= expand_var_part(dst, src_data, desc, order);
    ndbassert(dst_ptr == ALIGN_WORD(dst->m_data_ptr + dst->m_max_var_offset));
    ndbassert((UintPtr(src_ptr) & 3) == 0);
    src_ptr = ALIGN_WORD(((char*)src_ptr)+step);
    
    sizes[MM] += fix_size + Tuple_header::HeaderSize;
    memcpy(ptr, src, 4*(fix_size + Tuple_header::HeaderSize));
  } 
  else 
  {
    sizes[MM]= 1;
    dst_ptr -= Tuple_header::HeaderSize;
    src_ptr -= Tuple_header::HeaderSize;
    memcpy(ptr, src, 4*fix_size);
  }

  src->m_header_bits= bits & 
    ~(Uint32)(Tuple_header::MM_SHRINK | Tuple_header::MM_GROWN);
  
  sizes[DD]= 0;
  if(disk && dd_tot)
  {
    const Uint16 dd_vars= tabPtrP->m_attributes[DD].m_no_of_varsize;
    order += mm_vars;
    
    if(bits & Tuple_header::DISK_INLINE)
    {
      // Only on copy tuple
      ndbassert((bits & Tuple_header::CHAINED_ROW) == 0);
    }
    else
    {
      Local_key key;
      memcpy(&key, disk_ref, sizeof(key));
      key.m_page_no= req_struct->m_disk_page_ptr.i;
      src_ptr= get_dd_ptr(&req_struct->m_disk_page_ptr, &key, tabPtrP);
    }
    bits |= Tuple_header::DISK_INLINE;

    // Fix diskpart
    req_struct->m_disk_ptr= (Tuple_header*)dst_ptr;
    memcpy(dst_ptr, src_ptr, 4*tabPtrP->m_offsets[DD].m_fix_header_size);
    sizes[DD] = tabPtrP->m_offsets[DD].m_fix_header_size;
    
    ndbassert(! (req_struct->m_disk_ptr->m_header_bits & Tuple_header::FREE));
    
    if(dd_vars)
    {
      KeyReqStruct::Var_data* dst= &req_struct->m_var_data[DD];
      dst_ptr += tabPtrP->m_offsets[DD].m_varpart_offset;
      src_ptr += tabPtrP->m_offsets[DD].m_varpart_offset;
      order += tabPtrP->m_attributes[DD].m_no_of_fixsize;
      
      dst->m_data_ptr= (char*)(char*)(((Uint16*)dst_ptr)+dd_vars+1);
      dst->m_offset_array_ptr= req_struct->var_pos_array + (mm_vars << 1);
      dst->m_var_len_offset= dd_vars;
      dst->m_max_var_offset= tabPtrP->m_offsets[DD].m_max_var_offset;

      expand_var_part(dst, src_ptr, desc, order);
    }
  }
  
  ptr->m_header_bits= (bits & ~(Uint32)(Tuple_header::CHAINED_ROW));
}

void
Dbtup::prepare_read(KeyReqStruct* req_struct, 
		    Tablerec* tabPtrP, bool disk)
{
  Tuple_header* ptr= req_struct->m_tuple_ptr;
  
  Uint32 bits= ptr->m_header_bits;
  Uint16 dd_tot= tabPtrP->m_no_of_disk_attributes;
  Uint16 mm_vars= tabPtrP->m_attributes[MM].m_no_of_varsize;
  
  const Uint32 *src_ptr= ptr->get_var_part_ptr(tabPtrP);
  const Uint32 *disk_ref= ptr->get_disk_ref_ptr(tabPtrP);
  
  if(mm_vars)
  {
    const Uint32 *src_data= src_ptr;
    KeyReqStruct::Var_data* dst= &req_struct->m_var_data[MM];
    if(bits & Tuple_header::CHAINED_ROW)
    {
#if VM_TRACE
      
#endif
      src_data= get_ptr(* (Var_part_ref*)src_ptr);
    }
    dst->m_data_ptr= (char*)(((Uint16*)src_data)+mm_vars+1);
    dst->m_offset_array_ptr= (Uint16*)src_data;
    dst->m_var_len_offset= 1;
    dst->m_max_var_offset= ((Uint16*)src_data)[mm_vars];
    
    // disk part start after varsize (aligned)
    src_ptr = ALIGN_WORD(dst->m_data_ptr + dst->m_max_var_offset);
  } 
  else
  {
    // disk part if after fixsize part...
    src_ptr -= Tuple_header::HeaderSize; 
  }
  
  if(disk && dd_tot)
  {
    const Uint16 dd_vars= tabPtrP->m_attributes[DD].m_no_of_varsize;
    
    if(bits & Tuple_header::DISK_INLINE)
    {
      // Only on copy tuple
      ndbassert((bits & Tuple_header::CHAINED_ROW) == 0);
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
    ndbassert(! (req_struct->m_disk_ptr->m_header_bits & Tuple_header::FREE));
    if(dd_vars)
    {
      KeyReqStruct::Var_data* dst= &req_struct->m_var_data[DD];
      src_ptr += tabPtrP->m_offsets[DD].m_varpart_offset;
      
      dst->m_data_ptr= (char*)(char*)(((Uint16*)src_ptr)+dd_vars+1);
      dst->m_offset_array_ptr= (Uint16*)src_ptr;
      dst->m_var_len_offset= 1;
      dst->m_max_var_offset= ((Uint16*)src_ptr)[dd_vars];
    }
  }
}

void
Dbtup::shrink_tuple(KeyReqStruct* req_struct, Uint32 sizes[2],
		    const Tablerec* tabPtrP, bool disk)
{
  ndbassert(tabPtrP->need_shrink());
  Tuple_header* ptr= req_struct->m_tuple_ptr;
  
  Uint16 dd_tot= tabPtrP->m_no_of_disk_attributes;
  Uint16 mm_vars= tabPtrP->m_attributes[MM].m_no_of_varsize;
  Uint16 dd_vars= tabPtrP->m_attributes[DD].m_no_of_varsize;
  
  Uint32 *dst_ptr= ptr->get_var_part_ptr(tabPtrP);
  Uint16* src_off_ptr= req_struct->var_pos_array;

  sizes[MM]= sizes[DD]= 0;
  if(mm_vars)
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
    ndbassert(dst_data_ptr <= ((char*)ptr) + 8192);
    ndbassert((UintPtr(ptr) & 3) == 0);
    sizes[MM]= (dst_data_ptr + 3 - ((char*)ptr)) >> 2;

    dst_ptr = ALIGN_WORD(dst_data_ptr);
  }
  else
  {
    sizes[MM] = 1;
    dst_ptr -= Tuple_header::HeaderSize;
  }
  
  if(disk && dd_tot)
  {
    Uint32 * src_ptr = (Uint32*)req_struct->m_disk_ptr;
    req_struct->m_disk_ptr = (Tuple_header*)dst_ptr;
    if (unlikely(dd_vars))
    {
      abort();
    }
    else
    {
      sizes[DD] = tabPtrP->m_offsets[DD].m_fix_header_size;
      memmove(dst_ptr, src_ptr, 4*tabPtrP->m_offsets[DD].m_fix_header_size);
    }
  }
}

void
Dbtup::validate_page(Tablerec* regTabPtr, Var_page* p)
{
  Uint32 mm_vars= regTabPtr->m_attributes[MM].m_no_of_varsize;
  Uint32 fix_sz= regTabPtr->m_offsets[MM].m_fix_header_size + 
    Tuple_header::HeaderSize;
    
  if(mm_vars == 0)
    return;
  
  for(Uint32 F= 0; F<MAX_FRAG_PER_NODE; F++)
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
	  Uint32 *part= ptr->get_var_part_ptr(regTabPtr);
	  if(ptr->m_header_bits & Tuple_header::CHAINED_ROW)
	  {
	    ndbassert(len == fix_sz + 1);
	    Local_key tmp; tmp.assref(*part);
	    Ptr<Page> tmpPage;
	    part= get_ptr(&tmpPage, *(Var_part_ref*)part);
	    len= ((Var_page*)tmpPage.p)->get_entry_len(tmp.m_page_idx);
	    Uint32 sz= ((mm_vars + 1) << 1) + (((Uint16*)part)[mm_vars]);
	    ndbassert(len >= ((sz + 3) >> 2));
	  } 
	  else
	  {
	    Uint32 sz= ((mm_vars + 1) << 1) + (((Uint16*)part)[mm_vars]);
	    ndbassert(len >= ((sz+3)>>2)+fix_sz);
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
	  ndbassert(len >= ((sz + 3) >> 2));
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
  Uint32 fix_sz = Tuple_header::HeaderSize + 
    regTabPtr->m_offsets[MM].m_fix_header_size;
  
  if(sizes[MM] == sizes[2+MM])
    ;
  else if(sizes[MM] > sizes[2+MM])
  {
    if(0) ndbout_c("shrink");
    copy_bits |= Tuple_header::MM_SHRINK;
  }
  else
  {
    if(0) printf("grow - ");
    Ptr<Page> pagePtr = req_struct->m_varpart_page_ptr;
    Var_page* pageP= (Var_page*)pagePtr.p;
    Uint32 idx, alloc, needed;
    Uint32 *refptr = org->get_var_part_ptr(regTabPtr);
    ndbassert(bits & Tuple_header::CHAINED_ROW);

    Local_key ref;
    ref.assref(*refptr);
    idx= ref.m_page_idx;
    if (! (copy_bits & Tuple_header::CHAINED_ROW))
    {
      c_page_pool.getPtr(pagePtr, ref.m_page_no);
      pageP = (Var_page*)pagePtr.p;
    }
    alloc= pageP->get_entry_len(idx);
#ifdef VM_TRACE
    if(!pageP->get_entry_chain(idx))
      ndbout << *pageP << endl;
#endif
    ndbassert(pageP->get_entry_chain(idx));
    needed= sizes[2+MM] - fix_sz;
    
    if(needed <= alloc)
    {
      //ndbassert(!regOperPtr->is_first_operation());
      ndbout_c(" no grow");
      return 0;
    }
    copy_bits |= Tuple_header::MM_GROWN;
    if (unlikely(realloc_var_part(regFragPtr, regTabPtr, pagePtr, 
				  (Var_part_ref*)refptr, alloc, needed)))
      return -1;
  }
  req_struct->m_tuple_ptr->m_header_bits = copy_bits;
  return 0;
}

int
Dbtup::nr_update_gci(Uint32 fragPtrI, const Local_key* key, Uint32 gci)
{
  FragrecordPtr fragPtr;
  fragPtr.i= fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
  TablerecPtr tablePtr;
  tablePtr.i= fragPtr.p->fragTableId;
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);

  if (tablePtr.p->m_bits & Tablerec::TR_RowGCI)
  {
    Local_key tmp = *key;
    PagePtr page_ptr;

    int ret;
    if (tablePtr.p->m_attributes[MM].m_no_of_varsize)
    {
      tablePtr.p->m_offsets[MM].m_fix_header_size += 
	Tuple_header::HeaderSize+1;
      ret = alloc_page(tablePtr.p, fragPtr.p, &page_ptr, tmp.m_page_no);
      tablePtr.p->m_offsets[MM].m_fix_header_size -= 
	Tuple_header::HeaderSize+1;
    } 
    else
    {
      ret = alloc_page(tablePtr.p, fragPtr.p, &page_ptr, tmp.m_page_no);  
    }

    if (ret)
      return -1;
    
    Tuple_header* ptr = (Tuple_header*)
      ((Fix_page*)page_ptr.p)->get_ptr(tmp.m_page_idx, 0);
    
    ndbrequire(ptr->m_header_bits & Tuple_header::FREE);
    *ptr->get_mm_gci(tablePtr.p) = gci;
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
  Uint32 pages = fragPtr.p->noOfPages;
  
  int ret;
  PagePtr page_ptr;
  if (tablePtr.p->m_attributes[MM].m_no_of_varsize)
  {
    tablePtr.p->m_offsets[MM].m_fix_header_size += Tuple_header::HeaderSize+1;
    ret = alloc_page(tablePtr.p, fragPtr.p, &page_ptr, tmp.m_page_no);
    tablePtr.p->m_offsets[MM].m_fix_header_size -= Tuple_header::HeaderSize+1;
  } 
  else
  {
    ret = alloc_page(tablePtr.p, fragPtr.p, &page_ptr, tmp.m_page_no);  
  }
  if (ret)
    return -1;
  
  KeyReqStruct req_struct;
  Uint32* ptr= ((Fix_page*)page_ptr.p)->get_ptr(key->m_page_idx, 0);
  
  req_struct.m_page_ptr = page_ptr;
  req_struct.m_tuple_ptr = (Tuple_header*)ptr;
  Uint32 bits = req_struct.m_tuple_ptr->m_header_bits;

  ret = 0;
  copy = false;
  if (! (bits & Tuple_header::FREE))
  {
    if (bits & Tuple_header::ALLOC)
    {
      Uint32 opPtrI= req_struct.m_tuple_ptr->m_operation_ptr_i;
      Operationrec* opPtrP= c_operation_pool.getPtr(opPtrI);
      ndbassert(!opPtrP->m_copy_tuple_location.isNull());
      req_struct.m_tuple_ptr= (Tuple_header*)
	c_undo_buffer.get_ptr(&opPtrP->m_copy_tuple_location);
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
    
    // new globals
    tabptr= tablePtr;
    fragptr= fragPtr;
    operPtr.i= RNIL;
    operPtr.p= NULL;
    
    // do it
    ret = readAttributes(&req_struct,
			 attrIds,
			 numAttrs,
			 dst,
			 ZNIL, false);
    
    // done
    if (likely(ret != -1)) {
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
      return terrorCode ? (-(int)terrorCode) : -1;
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

#include <signaldata/TuxMaint.hpp>

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
  
  if (tablePtr.p->m_attributes[MM].m_no_of_varsize)
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
    
    int res = c_lgman->alloc_log_space(fragPtr.p->m_logfile_group_id, sz);
    ndbrequire(res == 0);
    
    /**
     * 1) alloc log buffer
     * 2) get page
     * 3) get log buffer
     * 4) delete tuple
     */
    Page_cache_client::Request preq;
    preq.m_page = disk;
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
	preq.m_delay_until_time = NdbTick_CurrentMillisecond()+(Uint64)slp;
      }
    }
#endif
    
    res = m_pgman.get_page(signal, preq, flags);
    if (res == 0)
    {
      goto timeslice;
    }
    else if (unlikely(res == -1))
    {
      return -1;
    }

    PagePtr disk_page = *(PagePtr*)&m_pgman.m_ptr;
    disk_page_set_dirty(disk_page);

    preq.m_callback.m_callbackFunction =
      safe_cast(&Dbtup::nr_delete_logbuffer_callback);      
    Logfile_client lgman(this, c_lgman, fragPtr.p->m_logfile_group_id);
    res= lgman.get_log_buffer(signal, sz, &preq.m_callback);
    switch(res){
    case 0:
      signal->theData[2] = disk_page.i;
      goto timeslice;
    case -1:
      ndbrequire("NOT YET IMPLEMENTED" == 0);
      break;
    }

    ndbout << "DIRECT DISK DELETE: " << disk << endl;
    disk_page_free(signal, tablePtr.p, fragPtr.p,
		   &disk, *(PagePtr*)&disk_page, gci);
    return 0;
  }
  
  return 0;

timeslice:
  memcpy(signal->theData, &disk, sizeof(disk));
  return 1;
}

void
Dbtup::nr_delete_page_callback(Signal* signal, 
			       Uint32 userpointer, Uint32 page_id)
{
  Ptr<GlobalPage> gpage;
  m_global_page_pool.getPtr(gpage, page_id);
  PagePtr pagePtr= *(PagePtr*)&gpage;
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
  
  Callback cb;
  cb.m_callbackData = userpointer;
  cb.m_callbackFunction =
    safe_cast(&Dbtup::nr_delete_logbuffer_callback);      
  Logfile_client lgman(this, c_lgman, fragPtr.p->m_logfile_group_id);
  int res= lgman.get_log_buffer(signal, sz, &cb);
  switch(res){
  case 0:
    return;
  case -1:
    ndbrequire("NOT YET IMPLEMENTED" == 0);
    break;
  }
    
  ndbout << "PAGE CALLBACK DISK DELETE: " << op.m_disk_ref << endl;
  disk_page_free(signal, tablePtr.p, fragPtr.p,
		 &op.m_disk_ref, pagePtr, op.m_gci);
  
  c_lqh->nr_delete_complete(signal, &op);
  return;
}

void
Dbtup::nr_delete_logbuffer_callback(Signal* signal, 
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

  Ptr<GlobalPage> gpage;
  m_global_page_pool.getPtr(gpage, op.m_page_id);
  PagePtr pagePtr= *(PagePtr*)&gpage;

  /**
   * reset page no
   */
  ndbout << "LOGBUFFER CALLBACK DISK DELETE: " << op.m_disk_ref << endl;
  
  disk_page_free(signal, tablePtr.p, fragPtr.p,
		 &op.m_disk_ref, pagePtr, op.m_gci);
  
  c_lqh->nr_delete_complete(signal, &op);
}
