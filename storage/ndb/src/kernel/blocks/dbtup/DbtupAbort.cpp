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
#define DBTUP_ABORT_CPP
#include "Dbtup.hpp"
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <pc.hpp>

void Dbtup::freeAllAttrBuffers(Operationrec*  const regOperPtr)
{
  if (regOperPtr->storedProcedureId == RNIL) {
    jam();
    freeAttrinbufrec(regOperPtr->firstAttrinbufrec);
  } else {
    jam();
    StoredProcPtr storedPtr;
    c_storedProcPool.getPtr(storedPtr, (Uint32)regOperPtr->storedProcedureId);
    ndbrequire(storedPtr.p->storedCode == ZSCAN_PROCEDURE);
    storedPtr.p->storedCounter--;
    regOperPtr->storedProcedureId = ZNIL;
  }//if
  regOperPtr->firstAttrinbufrec = RNIL;
  regOperPtr->lastAttrinbufrec = RNIL;
  regOperPtr->m_any_value = 0;
}//Dbtup::freeAllAttrBuffers()

void Dbtup::freeAttrinbufrec(Uint32 anAttrBuf) 
{
  Uint32 Ttemp;
  AttrbufrecPtr localAttrBufPtr;
  Uint32 RnoFree = cnoFreeAttrbufrec;
  localAttrBufPtr.i = anAttrBuf;
  while (localAttrBufPtr.i != RNIL) {
    jam();
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
  jamEntry();
  do_tup_abortreq(signal, 0);
}

void Dbtup::do_tup_abortreq(Signal* signal, Uint32 flags)
{
  OperationrecPtr regOperPtr;
  FragrecordPtr regFragPtr;
  TablerecPtr regTabPtr;

  regOperPtr.i = signal->theData[0];
  c_operation_pool.getPtr(regOperPtr);
  TransState trans_state= get_trans_state(regOperPtr.p);
  ndbrequire((trans_state == TRANS_STARTED) ||
             (trans_state == TRANS_TOO_MUCH_AI) ||
             (trans_state == TRANS_ERROR_WAIT_TUPKEYREQ) ||
             (trans_state == TRANS_IDLE));
  if (regOperPtr.p->op_struct.op_type == ZREAD) {
    jam();
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
    jam();
    if (!regTabPtr.p->tuxCustomTriggers.isEmpty() &&
        (flags & ZSKIP_TUX_TRIGGERS) == 0)
      executeTuxAbortTriggers(signal,
			      regOperPtr.p,
			      regFragPtr.p,
			      regTabPtr.p);
    
    OperationrecPtr loopOpPtr;
    loopOpPtr.i = regOperPtr.p->nextActiveOp;
    while (loopOpPtr.i != RNIL) {
      jam();
      c_operation_pool.getPtr(loopOpPtr);
      if (get_tuple_state(loopOpPtr.p) != TUPLE_ALREADY_ABORTED &&
	  !regTabPtr.p->tuxCustomTriggers.isEmpty() &&
          (flags & ZSKIP_TUX_TRIGGERS) == 0) {
        jam();
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

  bool change = false;
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

    if(! (bits & Tuple_header::ALLOC))
    {
      if(bits & Tuple_header::MM_GROWN)
      {
	if (0) ndbout_c("abort grow");
	Ptr<Page> vpage;
	Uint32 idx= regOperPtr.p->m_tuple_location.m_page_idx;
        Uint32 *var_part;

	ndbassert(! (tuple_ptr->m_header_bits & Tuple_header::COPY_TUPLE));
	
	Var_part_ref *ref = tuple_ptr->get_var_part_ref_ptr(regTabPtr.p);

        Local_key tmp; 
        ref->copyout(&tmp);
	
        idx= tmp.m_page_idx;
        var_part= get_ptr(&vpage, *ref);
        Var_page* pageP = (Var_page*)vpage.p;
        Uint32 len= pageP->get_entry_len(idx) & ~Var_page::CHAIN;

        /*
          A MM_GROWN tuple was relocated with a bigger size in preparation for
          commit, so we need to shrink it back. The original size is stored in
          the last word of the relocated (oversized) tuple.
        */
        ndbassert(len > 0);
        Uint32 sz= var_part[len-1];
        ndbassert(sz < len);
        if (sz)
        {
          pageP->shrink_entry(idx, sz);
        }
        else
        {
          pageP->free_record(tmp.m_page_idx, Var_page::CHAIN);
          tmp.m_page_no = RNIL;
          ref->assign(&tmp);
          bits &= ~(Uint32)Tuple_header::VAR_PART;
        }
        update_free_page_list(regFragPtr.p, vpage);
        tuple_ptr->m_header_bits= bits & ~Tuple_header::MM_GROWN;
        change = true;
      } 
      else if(bits & Tuple_header::MM_SHRINK)
      {
	if (0) ndbout_c("abort shrink");
      }
    }
    else if (regOperPtr.p->is_first_operation() && 
	     regOperPtr.p->is_last_operation())
    {
      /**
       * Aborting last operation that performed ALLOC
       */
      change = true;
      tuple_ptr->m_header_bits &= ~(Uint32)Tuple_header::ALLOC;
      tuple_ptr->m_header_bits |= Tuple_header::FREED;
    }
  }
  else if (regOperPtr.p->is_first_operation() && 
	   regOperPtr.p->is_last_operation())
  {
    if (bits & Tuple_header::ALLOC)
    {
      change = true;
      tuple_ptr->m_header_bits &= ~(Uint32)Tuple_header::ALLOC;
      tuple_ptr->m_header_bits |= Tuple_header::FREED;
    }
  }
  
  if (change && (regTabPtr.p->m_bits & Tablerec::TR_Checksum)) 
  {
    jam();
    setChecksum(tuple_ptr, regTabPtr.p);
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
    jam();
    break;

  case 15:
    jam();
    terrorCode = ZREGISTER_INIT_ERROR;
    break;

  case 16:
    jam();
    terrorCode = ZTRY_TO_UPDATE_ERROR;
    break;

  case 17:
    jam();
    terrorCode = ZNO_ILLEGAL_NULL_ATTR;
    break;

  case 19:
    jam();
    terrorCode = ZTRY_TO_UPDATE_ERROR;
    break;

  case 20:
    jam();
    terrorCode = ZREGISTER_INIT_ERROR;
    break;

  case 22:
    jam();
    terrorCode = ZTOTAL_LEN_ERROR;
    break;

  case 23:
    jam();
    terrorCode = ZREGISTER_INIT_ERROR;
    break;

  case 24:
    jam();
    terrorCode = ZREGISTER_INIT_ERROR;
    break;

  case 26:
    jam();
    terrorCode = ZREGISTER_INIT_ERROR;
    break;

  case 27:
    jam();
    terrorCode = ZREGISTER_INIT_ERROR;
    break;

  case 28:
    jam();
    terrorCode = ZREGISTER_INIT_ERROR;
    break;

  case 29:
    jam();
    break;

  case 30:
    jam();
    terrorCode = ZCALL_ERROR;
    break;

  case 31:
    jam();
    terrorCode = ZSTACK_OVERFLOW_ERROR;
    break;

  case 32:
    jam();
    terrorCode = ZSTACK_UNDERFLOW_ERROR;
    break;

  case 33:
    jam();
    terrorCode = ZNO_INSTRUCTION_ERROR;
    break;

  case 34:
    jam();
    terrorCode = ZOUTSIDE_OF_PROGRAM_ERROR;
    break;

  case 35:
    jam();
    terrorCode = ZTOO_MANY_INSTRUCTIONS_ERROR;
    break;

  case 38:
    jam();
    terrorCode = ZTEMPORARY_RESOURCE_FAILURE;
    break;

  case 39:
    if (get_trans_state(operPtr.p) == TRANS_TOO_MUCH_AI) {
      jam();
      terrorCode = ZTOO_MUCH_ATTRINFO_ERROR;
    } else if (get_trans_state(operPtr.p) == TRANS_ERROR_WAIT_TUPKEYREQ) {
      jam();
      terrorCode = ZSEIZE_ATTRINBUFREC_ERROR;
    } else {
      ndbrequire(false);
    }//if
    break;
  case 40:
    jam();
    terrorCode = ZUNSUPPORTED_BRANCH;
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

  if (regOperPtr->m_undo_buffer_space &&
      (regOperPtr->is_first_operation() && regOperPtr->is_last_operation()))
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

