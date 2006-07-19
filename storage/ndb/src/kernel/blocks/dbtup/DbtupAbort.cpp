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

#define ljam() { jamLine(9000 + __LINE__); }
#define ljamEntry() { jamEntryLine(9000 + __LINE__); }

void Dbtup::freeAllAttrBuffers(Operationrec*  const regOperPtr)
{
  if (regOperPtr->storedProcedureId == RNIL) {
    ljam();
    freeAttrinbufrec(regOperPtr->firstAttrinbufrec);
  } else {
    ljam();
    StoredProcPtr storedPtr;
    c_storedProcPool.getPtr(storedPtr, (Uint32)regOperPtr->storedProcedureId);
    ndbrequire(storedPtr.p->storedCode == ZSCAN_PROCEDURE);
    storedPtr.p->storedCounter--;
    regOperPtr->storedProcedureId = ZNIL;
  }//if
  regOperPtr->firstAttrinbufrec = RNIL;
  regOperPtr->lastAttrinbufrec = RNIL;
}//Dbtup::freeAllAttrBuffers()

void Dbtup::freeAttrinbufrec(Uint32 anAttrBuf) 
{
  Uint32 Ttemp;
  AttrbufrecPtr localAttrBufPtr;
  Uint32 RnoFree = cnoFreeAttrbufrec;
  localAttrBufPtr.i = anAttrBuf;
  while (localAttrBufPtr.i != RNIL) {
    ljam();
    ptrCheckGuard(localAttrBufPtr, cnoOfAttrbufrec, attrbufrec);
    Ttemp = localAttrBufPtr.p->attrbuf[ZBUF_NEXT];
    localAttrBufPtr.p->attrbuf[ZBUF_NEXT] = cfirstfreeAttrbufrec;
    cfirstfreeAttrbufrec = localAttrBufPtr.i;
    localAttrBufPtr.i = Ttemp;
    RnoFree++;
  }//if
  cnoFreeAttrbufrec = RnoFree;
}//Dbtup::freeAttrinbufrec()

/**
 * Abort abort this operation and all after (nextActiveOp's)
 */
void Dbtup::execTUP_ABORTREQ(Signal* signal) 
{
  do_tup_abortreq(signal, 0);
}

void Dbtup::do_tup_abortreq(Signal* signal, Uint32 flags)
{
  OperationrecPtr regOperPtr;
  FragrecordPtr regFragPtr;
  TablerecPtr regTabPtr;

  ljamEntry();
  regOperPtr.i = signal->theData[0];
  c_operation_pool.getPtr(regOperPtr);
  TransState trans_state= get_trans_state(regOperPtr.p);
  ndbrequire((trans_state == TRANS_STARTED) ||
             (trans_state == TRANS_TOO_MUCH_AI) ||
             (trans_state == TRANS_ERROR_WAIT_TUPKEYREQ) ||
             (trans_state == TRANS_IDLE));
  if (regOperPtr.p->op_struct.op_type == ZREAD) {
    ljam();
    freeAllAttrBuffers(regOperPtr.p);
    initOpConnection(regOperPtr.p);
    return;
  }//if

  regFragPtr.i = regOperPtr.p->fragmentPtr;
  ptrCheckGuard(regFragPtr, cnoOfFragrec, fragrecord);

  regTabPtr.i = regFragPtr.p->fragTableId;
  ptrCheckGuard(regTabPtr, cnoOfTablerec, tablerec);

  if (get_tuple_state(regOperPtr.p) == TUPLE_PREPARED)
  {
    ljam();
    if (!regTabPtr.p->tuxCustomTriggers.isEmpty() &&
        (flags & 0x1) == 0)
      executeTuxAbortTriggers(signal,
			      regOperPtr.p,
			      regFragPtr.p,
			      regTabPtr.p);
    
    OperationrecPtr loopOpPtr;
    loopOpPtr.i = regOperPtr.p->nextActiveOp;
    while (loopOpPtr.i != RNIL) {
      ljam();
      c_operation_pool.getPtr(loopOpPtr);
      if (get_tuple_state(loopOpPtr.p) != TUPLE_ALREADY_ABORTED &&
	  !regTabPtr.p->tuxCustomTriggers.isEmpty() &&
          (flags & 0x1) == 0) {
        ljam();
        executeTuxAbortTriggers(signal,
                                loopOpPtr.p,
                                regFragPtr.p,
                                regTabPtr.p);
      }
      set_tuple_state(loopOpPtr.p, TUPLE_ALREADY_ABORTED);      
      loopOpPtr.i = loopOpPtr.p->nextActiveOp;
    }
  }

  PagePtr page;
  Tuple_header *tuple_ptr= (Tuple_header*)
    get_ptr(&page, &regOperPtr.p->m_tuple_location, regTabPtr.p);

  Uint32 bits= tuple_ptr->m_header_bits;  
  if(regOperPtr.p->op_struct.op_type != ZDELETE)
  {
    Tuple_header *copy= (Tuple_header*)
      c_undo_buffer.get_ptr(&regOperPtr.p->m_copy_tuple_location);
    
    if(regOperPtr.p->op_struct.m_disk_preallocated)
    {
      jam();
      Local_key key;
      memcpy(&key, copy->get_disk_ref_ptr(regTabPtr.p), sizeof(key));
      disk_page_abort_prealloc(signal, regFragPtr.p, &key, key.m_page_idx);
    }
    

    Uint32 copy_bits= copy->m_header_bits;
    if(! (bits & Tuple_header::ALLOC))
    {
      if(copy_bits & Tuple_header::MM_GROWN)
      {
	ndbout_c("abort grow");
	Ptr<Page> vpage;
	Uint32 idx= regOperPtr.p->m_tuple_location.m_page_idx;
	Uint32 mm_vars= regTabPtr.p->m_attributes[MM].m_no_of_varsize;
	Uint32 *var_part;

	ndbassert(tuple_ptr->m_header_bits & Tuple_header::CHAINED_ROW);
	
	Uint32 ref= * tuple_ptr->get_var_part_ptr(regTabPtr.p);
	Local_key tmp; 
	tmp.assref(ref); 
	
	idx= tmp.m_page_idx;
	var_part= get_ptr(&vpage, *(Var_part_ref*)&ref);
	Var_page* pageP = (Var_page*)vpage.p;
	Uint32 len= pageP->get_entry_len(idx) & ~Var_page::CHAIN;
	Uint32 sz = ((((mm_vars + 1) << 1) + (((Uint16*)var_part)[mm_vars]) + 3)>> 2);
	ndbassert(sz <= len);
	pageP->shrink_entry(idx, sz);
	update_free_page_list(regFragPtr.p, vpage);
      } 
      else if(bits & Tuple_header::MM_SHRINK)
      {
	ndbout_c("abort shrink");
      }
    }
    else if (regOperPtr.p->is_first_operation() && 
	     regOperPtr.p->is_last_operation())
    {
      /**
       * Aborting last operation that performed ALLOC
       */
      tuple_ptr->m_header_bits &= ~(Uint32)Tuple_header::ALLOC;
      tuple_ptr->m_header_bits |= Tuple_header::FREED;
    }
  }
  else if (regOperPtr.p->is_first_operation() && 
	   regOperPtr.p->is_last_operation())
  {
    if (bits & Tuple_header::ALLOC)
    {
      tuple_ptr->m_header_bits &= ~(Uint32)Tuple_header::ALLOC;
      tuple_ptr->m_header_bits |= Tuple_header::FREED;
    }
  }
  
  if(regOperPtr.p->is_first_operation() && regOperPtr.p->is_last_operation())
  {
    if (regOperPtr.p->m_undo_buffer_space)
      c_lgman->free_log_space(regFragPtr.p->m_logfile_group_id, 
			      regOperPtr.p->m_undo_buffer_space);
  }

  removeActiveOpList(regOperPtr.p, tuple_ptr);
  initOpConnection(regOperPtr.p);
}

/* **************************************************************** */
/* ********************** TRANSACTION ERROR MODULE **************** */
/* **************************************************************** */
int Dbtup::TUPKEY_abort(Signal* signal, int error_type)
{
  switch(error_type) {
  case 1:
//tmupdate_alloc_error:
    terrorCode= ZMEM_NOMEM_ERROR;
    ljam();
    break;

  case 15:
    ljam();
    terrorCode = ZREGISTER_INIT_ERROR;
    break;

  case 16:
    ljam();
    terrorCode = ZTRY_TO_UPDATE_ERROR;
    break;

  case 17:
    ljam();
    terrorCode = ZNO_ILLEGAL_NULL_ATTR;
    break;

  case 19:
    ljam();
    terrorCode = ZTRY_TO_UPDATE_ERROR;
    break;

  case 20:
    ljam();
    terrorCode = ZREGISTER_INIT_ERROR;
    break;

  case 22:
    ljam();
    terrorCode = ZTOTAL_LEN_ERROR;
    break;

  case 23:
    ljam();
    terrorCode = ZREGISTER_INIT_ERROR;
    break;

  case 24:
    ljam();
    terrorCode = ZREGISTER_INIT_ERROR;
    break;

  case 26:
    ljam();
    terrorCode = ZREGISTER_INIT_ERROR;
    break;

  case 27:
    ljam();
    terrorCode = ZREGISTER_INIT_ERROR;
    break;

  case 28:
    ljam();
    terrorCode = ZREGISTER_INIT_ERROR;
    break;

  case 29:
    ljam();
    break;

  case 30:
    ljam();
    terrorCode = ZCALL_ERROR;
    break;

  case 31:
    ljam();
    terrorCode = ZSTACK_OVERFLOW_ERROR;
    break;

  case 32:
    ljam();
    terrorCode = ZSTACK_UNDERFLOW_ERROR;
    break;

  case 33:
    ljam();
    terrorCode = ZNO_INSTRUCTION_ERROR;
    break;

  case 34:
    ljam();
    terrorCode = ZOUTSIDE_OF_PROGRAM_ERROR;
    break;

  case 35:
    ljam();
    terrorCode = ZTOO_MANY_INSTRUCTIONS_ERROR;
    break;

  case 38:
    ljam();
    terrorCode = ZTEMPORARY_RESOURCE_FAILURE;
    break;

  case 39:
    if (get_trans_state(operPtr.p) == TRANS_TOO_MUCH_AI) {
      ljam();
      terrorCode = ZTOO_MUCH_ATTRINFO_ERROR;
    } else if (get_trans_state(operPtr.p) == TRANS_ERROR_WAIT_TUPKEYREQ) {
      ljam();
      terrorCode = ZSEIZE_ATTRINBUFREC_ERROR;
    } else {
      ndbrequire(false);
    }//if
    break;
  default:
    ndbrequire(false);
    break;
  }//switch
  tupkeyErrorLab(signal);
  return -1;
}

void Dbtup::early_tupkey_error(Signal* signal)
{
  Operationrec * const regOperPtr = operPtr.p;
  ndbrequire(!regOperPtr->op_struct.in_active_list);
  set_trans_state(regOperPtr, TRANS_IDLE);
  set_tuple_state(regOperPtr, TUPLE_PREPARED);
  initOpConnection(regOperPtr);
  send_TUPKEYREF(signal, regOperPtr);
}

void Dbtup::tupkeyErrorLab(Signal* signal) 
{
  Operationrec * const regOperPtr = operPtr.p;
  set_trans_state(regOperPtr, TRANS_IDLE);
  set_tuple_state(regOperPtr, TUPLE_PREPARED);

  FragrecordPtr fragPtr;
  fragPtr.i= regOperPtr->fragmentPtr;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);

  TablerecPtr tabPtr;
  tabPtr.i= fragPtr.p->fragTableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);

  if (regOperPtr->m_undo_buffer_space)
  {
    c_lgman->free_log_space(fragPtr.p->m_logfile_group_id, 
			    regOperPtr->m_undo_buffer_space);
  }

  Uint32 *ptr = 0;
  if (!regOperPtr->m_tuple_location.isNull())
  {
    PagePtr tmp;
    ptr= get_ptr(&tmp, &regOperPtr->m_tuple_location, tabPtr.p);
  }


  removeActiveOpList(regOperPtr, (Tuple_header*)ptr);
  initOpConnection(regOperPtr);
  send_TUPKEYREF(signal, regOperPtr);
}

void Dbtup::send_TUPKEYREF(Signal* signal,
                           Operationrec* const regOperPtr)
{
  TupKeyRef * const tupKeyRef = (TupKeyRef *)signal->getDataPtrSend();  
  tupKeyRef->userRef = regOperPtr->userpointer;
  tupKeyRef->errorCode = terrorCode;
  sendSignal(DBLQH_REF, GSN_TUPKEYREF, signal, 
             TupKeyRef::SignalLength, JBB);
}

