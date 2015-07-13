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
#define DBTUP_ABORT_CPP
#include "Dbtup.hpp"
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <pc.hpp>

#define JAM_FILE_ID 409


/**
 * Abort abort this operation and all after (nextActiveOp's)
 */
void Dbtup::execTUP_ABORTREQ(Signal* signal) 
{
  jamEntry();
  do_tup_abortreq(signal, 0);
}

void
Dbtup::do_tup_abort_operation(Signal* signal,
                              Tuple_header *tuple_ptr,
                              Operationrec* opPtrP,
                              Fragrecord* fragPtrP,
                              Tablerec* tablePtrP)
{
  Uint32 bits= tuple_ptr->m_header_bits;  
  if (opPtrP->op_type != ZDELETE)
  {
    Tuple_header *copy= get_copy_tuple(&opPtrP->m_copy_tuple_location);
    
    if (opPtrP->op_struct.bit_field.m_disk_preallocated)
    {
      jam();
      Local_key key;
      memcpy(&key, copy->get_disk_ref_ptr(tablePtrP), sizeof(key));
      disk_page_abort_prealloc(signal, fragPtrP, &key, key.m_page_idx);
    }

    if(! (bits & Tuple_header::ALLOC))
    {
      jam();
      if(bits & Tuple_header::MM_GROWN)
      {
        jam();
	if (0) ndbout_c("abort grow");
	Ptr<Page> vpage;
	Uint32 idx= opPtrP->m_tuple_location.m_page_idx;
        Uint32 *var_part;
        
	ndbassert(! (tuple_ptr->m_header_bits & Tuple_header::COPY_TUPLE));
	
	Var_part_ref *ref = tuple_ptr->get_var_part_ref_ptr(tablePtrP);
        
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
          jam();
          ndbassert(fragPtrP->m_varWordsFree >= pageP->free_space);
          fragPtrP->m_varWordsFree -= pageP->free_space;
          pageP->shrink_entry(idx, sz);
          // Adds the new free space value for the page to the fragment total.
          update_free_page_list(fragPtrP, vpage);
        }
        else
        {
          jam();
          free_var_part(fragPtrP, vpage, tmp.m_page_idx);
          tmp.m_page_no = RNIL;
          ref->assign(&tmp);
          bits &= ~(Uint32)Tuple_header::VAR_PART;
        }
        tuple_ptr->m_header_bits= bits & ~Tuple_header::MM_GROWN;
      } 
      else if(bits & Tuple_header::MM_SHRINK)
      {
        jam();
	if (0) ndbout_c("abort shrink");
      }
    }
    else if (opPtrP->is_first_operation())
    {
      jam();
      /**
       * Aborting last operation that performed ALLOC
       */
      tuple_ptr->m_header_bits &= ~(Uint32)Tuple_header::ALLOC;
      tuple_ptr->m_header_bits |= Tuple_header::FREED;
    }
  }
  else if (opPtrP->is_first_operation())
  {
    jam();
    if (bits & Tuple_header::ALLOC)
    {
      jam();
      tuple_ptr->m_header_bits &= ~(Uint32)Tuple_header::ALLOC;
      tuple_ptr->m_header_bits |= Tuple_header::FREED;
    }
  }
  return;
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
  if (regOperPtr.p->op_type == ZREAD) {
    jam();
    initOpConnection(regOperPtr.p);
    return;
  }//if

  regFragPtr.i = regOperPtr.p->fragmentPtr;
  ptrCheckGuard(regFragPtr, cnoOfFragrec, fragrecord);

  regTabPtr.i = regFragPtr.p->fragTableId;
  ptrCheckGuard(regTabPtr, cnoOfTablerec, tablerec);

  PagePtr page;
  Tuple_header *tuple_ptr= (Tuple_header*)
    get_ptr(&page, &regOperPtr.p->m_tuple_location, regTabPtr.p);

  if (get_tuple_state(regOperPtr.p) == TUPLE_PREPARED)
  {
    jam();

    /**
     * abort all TUX entries first...if present
     */
    if (!regTabPtr.p->tuxCustomTriggers.isEmpty() && 
        ! (flags & ZSKIP_TUX_TRIGGERS))
    {
      jam();
      executeTuxAbortTriggers(signal,
                              regOperPtr.p,
                              regFragPtr.p,
                              regTabPtr.p);

      OperationrecPtr loopOpPtr;
      loopOpPtr.i = regOperPtr.p->nextActiveOp;
      while (loopOpPtr.i != RNIL) 
      {
        jam();
        c_operation_pool.getPtr(loopOpPtr);
        if (get_tuple_state(loopOpPtr.p) != TUPLE_ALREADY_ABORTED)
        {
          jam();
          executeTuxAbortTriggers(signal,
                                  loopOpPtr.p,
                                  regFragPtr.p,
                                  regTabPtr.p);
        }
        loopOpPtr.i = loopOpPtr.p->nextActiveOp;
      }
    }

    /**
     * Then abort all data changes
     */
    {
      do_tup_abort_operation(signal, 
                             tuple_ptr,
                             regOperPtr.p,
                             regFragPtr.p,
                             regTabPtr.p);
      
      OperationrecPtr loopOpPtr;
      loopOpPtr.i = regOperPtr.p->nextActiveOp;
      while (loopOpPtr.i != RNIL) 
      {
        jam();
        c_operation_pool.getPtr(loopOpPtr);
        if (get_tuple_state(loopOpPtr.p) != TUPLE_ALREADY_ABORTED)
        {
          jam();
          do_tup_abort_operation(signal,
                                 tuple_ptr,
                                 loopOpPtr.p,
                                 regFragPtr.p,
                                 regTabPtr.p);
          set_tuple_state(loopOpPtr.p, TUPLE_ALREADY_ABORTED);      
        }
        loopOpPtr.i = loopOpPtr.p->nextActiveOp;
      }
      if (tuple_ptr->m_header_bits & Tuple_header::FREED)
      {
        jam();
        setInvalidChecksum(tuple_ptr, regTabPtr.p);
      }
      else
      {
        jam();
        setChecksum(tuple_ptr, regTabPtr.p);
      }
    }
  }

  bool first_and_last = false;
  if (regOperPtr.p->is_first_operation() && regOperPtr.p->is_last_operation())
  {
    jam();
    /* Abort of only (remaining) op on this row */
    first_and_last = true;
    if (regOperPtr.p->m_undo_buffer_space)
    {
      jam();
      D("Logfile_client - do_tup_abortreq");
      Logfile_client lgman(this, c_lgman, regFragPtr.p->m_logfile_group_id);
      lgman.free_log_space(regOperPtr.p->m_undo_buffer_space,
                           jamBuffer());
    }
  }

  removeActiveOpList(regOperPtr.p, tuple_ptr);

  if (first_and_last &&
      (flags & ZABORT_DEALLOC) &&
      (tuple_ptr->m_header_bits & Tuple_header::FREED))
  {
    jam();
    /* Free var and fixed records for this row */
    Local_key tmp = regOperPtr.p->m_tuple_location;
    if (regTabPtr.p->m_attributes[MM].m_no_of_varsize +
        regTabPtr.p->m_attributes[MM].m_no_of_dynamic)
    {
      jam();
      free_var_rec(regFragPtr.p, regTabPtr.p, &tmp, page);
    }
    else
    {
      jam();
      free_fix_rec(regFragPtr.p, regTabPtr.p, &tmp, (Fix_page*)page.p);
    }
    regOperPtr.p->m_tuple_location.setNull();
  }

  initOpConnection(regOperPtr.p);
}

/* **************************************************************** */
/* ********************** TRANSACTION ERROR MODULE **************** */
/* **************************************************************** */
int Dbtup::TUPKEY_abort(KeyReqStruct * req_struct, int error_type)
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
    if (get_trans_state(req_struct->operPtrP) == TRANS_TOO_MUCH_AI) {
      jam();
      terrorCode = ZTOO_MUCH_ATTRINFO_ERROR;
    } else if (get_trans_state(req_struct->operPtrP) == TRANS_ERROR_WAIT_TUPKEYREQ) {
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
  tupkeyErrorLab(req_struct);
  return -1;
}

void Dbtup::early_tupkey_error(KeyReqStruct* req_struct)
{
  Operationrec * const regOperPtr = req_struct->operPtrP;
  ndbrequire(!regOperPtr->op_struct.bit_field.in_active_list);
  set_trans_state(regOperPtr, TRANS_IDLE);
  set_tuple_state(regOperPtr, TUPLE_PREPARED);
  initOpConnection(regOperPtr);
  send_TUPKEYREF(req_struct);
}

void Dbtup::tupkeyErrorLab(KeyReqStruct* req_struct)
{
  Operationrec * const regOperPtr = req_struct->operPtrP;
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
    jam();
    D("Logfile_client - tupkeyErrorLab");
    Logfile_client lgman(this, c_lgman, fragPtr.p->m_logfile_group_id);
    lgman.free_log_space(regOperPtr->m_undo_buffer_space,
                         jamBuffer());
  }

  Uint32 *ptr = 0;
  if (!regOperPtr->m_tuple_location.isNull())
  {
    PagePtr tmp;
    ptr= get_ptr(&tmp, &regOperPtr->m_tuple_location, tabPtr.p);
  }


  removeActiveOpList(regOperPtr, (Tuple_header*)ptr);
  initOpConnection(regOperPtr);
  send_TUPKEYREF(req_struct);
}

void Dbtup::send_TUPKEYREF(const KeyReqStruct* req_struct)
{
  TupKeyRef * const tupKeyRef =
    (TupKeyRef *)req_struct->signal->getDataPtrSend();  
  tupKeyRef->userRef = req_struct->operPtrP->userpointer;
  tupKeyRef->errorCode = terrorCode;
  tupKeyRef->noExecInstructions = req_struct->no_exec_instructions;
}

/**
 * Unlink one operation from the m_operation_ptr_i list in the tuple.
 */
void Dbtup::removeActiveOpList(Operationrec*  const regOperPtr,
                               Tuple_header *tuple_ptr)
{
  OperationrecPtr nextOperPtr;
  OperationrecPtr prevOperPtr;

  if(!regOperPtr->m_copy_tuple_location.isNull())
  {
    jam();
    c_undo_buffer.free_copy_tuple(&regOperPtr->m_copy_tuple_location);
  }

  if (regOperPtr->op_struct.bit_field.in_active_list)
  {
    nextOperPtr.i = regOperPtr->nextActiveOp;
    prevOperPtr.i = regOperPtr->prevActiveOp;
    regOperPtr->op_struct.bit_field.in_active_list= false;
    if (nextOperPtr.i != RNIL)
    {
      jam();
      c_operation_pool.getPtr(nextOperPtr);
      nextOperPtr.p->prevActiveOp = prevOperPtr.i;
    }
    else
    {
      jam();
      tuple_ptr->m_operation_ptr_i = prevOperPtr.i;
    }
    if (prevOperPtr.i != RNIL)
    {
      jam();
      c_operation_pool.getPtr(prevOperPtr);
      prevOperPtr.p->nextActiveOp = nextOperPtr.i;
      if (nextOperPtr.i == RNIL)
      {
        jam();
        /**
         * We are the leader in the list of the operations on this row.
         * There is more operations behind us, so thus we are the leader
         * in a group of more than one operation. This means that we
         * to transfer the leader functionality to the second in line.
         */
        prevOperPtr.p->op_struct.bit_field.m_load_diskpage_on_commit =
          regOperPtr->op_struct.bit_field.m_load_diskpage_on_commit;
        prevOperPtr.p->op_struct.bit_field.m_wait_log_buffer =
          regOperPtr->op_struct.bit_field.m_wait_log_buffer;
        if (regOperPtr->op_struct.bit_field.delete_insert_flag &&
            regOperPtr->op_type == ZINSERT &&
            prevOperPtr.p->op_type == ZDELETE)
        {
          jam();
          /**
           * If someone somehow manages to first delete the record and then
           * starts a new operation on the same record using an insert, given
           * that we now abort the insert operation we need to reset the
           * delete+insert flag on the delete operation if this operation for
           * some reason continues and becomes committed. In this case we
           * want to ensure that the delete executes its index triggers.
           */
          prevOperPtr.p->op_struct.bit_field.delete_insert_flag = false;
        }
      }
    }
    regOperPtr->prevActiveOp= RNIL;
    regOperPtr->nextActiveOp= RNIL;
  }
}
