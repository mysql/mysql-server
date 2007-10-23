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
#define DBTUP_META_CPP
#include "Dbtup.hpp"
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <pc.hpp>
#include <signaldata/TupFrag.hpp>
#include <signaldata/FsRef.hpp>
#include <signaldata/FsConf.hpp>
#include <signaldata/FsRemoveReq.hpp>
#include <signaldata/DropTab.hpp>
#include <signaldata/AlterTab.hpp>
#include <signaldata/AlterTable.hpp>
#include <signaldata/CreateFilegroupImpl.hpp>
#include <AttributeDescriptor.hpp>
#include "AttributeOffset.hpp"
#include <my_sys.h>

void Dbtup::execTUPFRAGREQ(Signal* signal)
{
  jamEntry();

  TupFragReq* tupFragReq = (TupFragReq*)signal->getDataPtr();
  if (tupFragReq->userPtr == (Uint32)-1) {
    jam();
    abortAddFragOp(signal);
    return;
  }

  FragoperrecPtr fragOperPtr;
  FragrecordPtr regFragPtr;
  TablerecPtr regTabPtr;

  Uint32 userptr        = tupFragReq->userPtr;
  Uint32 userblockref   = tupFragReq->userRef;
  Uint32 reqinfo        = tupFragReq->reqInfo;
  regTabPtr.i           = tupFragReq->tableId;
  Uint32 noOfAttributes = tupFragReq->noOfAttr;
  Uint32 fragId         = tupFragReq->fragId;
  /*  Uint32 schemaVersion = tupFragReq->schemaVersion;*/
  Uint32 noOfKeyAttr = tupFragReq->noOfKeyAttr;
  Uint32 noOfCharsets = tupFragReq->noOfCharsets;

  Uint32 checksumIndicator = tupFragReq->checksumIndicator;
  Uint32 gcpIndicator = tupFragReq->globalCheckpointIdIndicator;
  Uint32 tablespace_id= tupFragReq->tablespaceid;
  Uint32 forceVarPart = tupFragReq->forceVarPartFlag;

  Uint64 maxRows =
    (((Uint64)tupFragReq->maxRowsHigh) << 32) + tupFragReq->maxRowsLow;
  Uint64 minRows =
    (((Uint64)tupFragReq->minRowsHigh) << 32) + tupFragReq->minRowsLow;

#ifndef VM_TRACE
  // config mismatch - do not crash if release compiled
  if (regTabPtr.i >= cnoOfTablerec) {
    jam();
    tupFragReq->userPtr = userptr;
    tupFragReq->userRef = 800;
    sendSignal(userblockref, GSN_TUPFRAGREF, signal, 2, JBB);
    return;
  }
#endif

  ptrCheckGuard(regTabPtr, cnoOfTablerec, tablerec);
  if (cfirstfreeFragopr == RNIL) {
    jam();
    tupFragReq->userPtr = userptr;
    tupFragReq->userRef = ZNOFREE_FRAGOP_ERROR;
    sendSignal(userblockref, GSN_TUPFRAGREF, signal, 2, JBB);
    return;
  }
  seizeFragoperrec(fragOperPtr);

  fragOperPtr.p->nextFragoprec = RNIL;
  fragOperPtr.p->lqhBlockrefFrag = userblockref;
  fragOperPtr.p->lqhPtrFrag = userptr;
  fragOperPtr.p->fragidFrag = fragId;
  fragOperPtr.p->tableidFrag = regTabPtr.i;
  fragOperPtr.p->attributeCount = noOfAttributes;
  
  memset(fragOperPtr.p->m_null_bits, 0, sizeof(fragOperPtr.p->m_null_bits));

  fragOperPtr.p->charsetIndex = 0;
  fragOperPtr.p->minRows = minRows;
  fragOperPtr.p->maxRows = maxRows;

  ndbrequire(reqinfo == ZADDFRAG);

  getFragmentrec(regFragPtr, fragId, regTabPtr.p);
  if (regFragPtr.i != RNIL) {
    jam();
    terrorCode= ZEXIST_FRAG_ERROR;
    fragrefuse1Lab(signal, fragOperPtr);
    return;
  }
  if (cfirstfreefrag != RNIL) {
    jam();
    seizeFragrecord(regFragPtr);
  } else {
    jam();
    terrorCode= ZFULL_FRAGRECORD_ERROR;
    fragrefuse1Lab(signal, fragOperPtr);
    return;
  }
  if (!addfragtotab(regTabPtr.p, fragId, regFragPtr.i)) {
    jam();
    terrorCode= ZNO_FREE_TAB_ENTRY_ERROR;
    fragrefuse2Lab(signal, fragOperPtr, regFragPtr);
    return;
  }

  regFragPtr.p->fragTableId= regTabPtr.i;
  regFragPtr.p->fragmentId= fragId;
  regFragPtr.p->m_tablespace_id= tablespace_id;
  regFragPtr.p->m_undo_complete= false;
  regFragPtr.p->m_lcp_scan_op = RNIL; 
  regFragPtr.p->m_lcp_keep_list = RNIL;
  regFragPtr.p->m_var_page_chunks = RNIL;  
  regFragPtr.p->noOfPages = 0;
  regFragPtr.p->noOfVarPages = 0;
  ndbrequire(regFragPtr.p->m_page_map.isEmpty());
  regFragPtr.p->m_restore_lcp_id = RNIL;

  if (ERROR_INSERTED(4007) && regTabPtr.p->fragid[0] == fragId ||
      ERROR_INSERTED(4008) && regTabPtr.p->fragid[1] == fragId) {
    jam();
    terrorCode = 1;
    fragrefuse4Lab(signal, fragOperPtr, regFragPtr, regTabPtr.p, fragId);
    CLEAR_ERROR_INSERT_VALUE;
    return;
  }

  if (regTabPtr.p->tableStatus == NOT_DEFINED) {
    jam();
//-----------------------------------------------------------------------------
// We are setting up references to the header of the tuple.
// Active operation  This word contains a reference to the operation active
//                   on the tuple at the moment. RNIL means no one active at
//                   all.  Not optional.
// Tuple version     Uses only low 16 bits.  Not optional.
// Checksum          The third header word is optional and contains a checksum
//                   of the tuple header.
// Null-bits         A number of words to contain null bits for all
//                   non-dynamic attributes. Each word contains upto 32 null
//                   bits. Each time a new word is needed we allocate the
//                   complete word. Zero nullable attributes means that there
//                   is no word at all
//                   Note that the null-bits are also used for storing the
//                   data for (non-dynamic) bit types.
//-----------------------------------------------------------------------------
    fragOperPtr.p->definingFragment= true;
    regTabPtr.p->tableStatus= DEFINING;
    regTabPtr.p->m_bits = 0;
    regTabPtr.p->m_bits |= (checksumIndicator ? Tablerec::TR_Checksum : 0);
    regTabPtr.p->m_bits |= (gcpIndicator ? Tablerec::TR_RowGCI : 0);
    regTabPtr.p->m_bits |= (forceVarPart ? Tablerec::TR_ForceVarPart : 0);
    
    regTabPtr.p->m_offsets[MM].m_disk_ref_offset= 0;
    regTabPtr.p->m_offsets[MM].m_null_words= 0;
    regTabPtr.p->m_offsets[MM].m_fix_header_size= 0;
    regTabPtr.p->m_offsets[MM].m_max_var_offset= 0;
    regTabPtr.p->m_offsets[MM].m_max_dyn_offset= 0;
    regTabPtr.p->m_offsets[MM].m_dyn_null_words= 0;

    regTabPtr.p->m_offsets[DD].m_disk_ref_offset= 0;
    regTabPtr.p->m_offsets[DD].m_null_words= 0;
    regTabPtr.p->m_offsets[DD].m_fix_header_size= 0;
    regTabPtr.p->m_offsets[DD].m_max_var_offset= 0;
    regTabPtr.p->m_offsets[DD].m_max_dyn_offset= 0;
    regTabPtr.p->m_offsets[DD].m_dyn_null_words= 0;

    regTabPtr.p->m_attributes[MM].m_no_of_fixsize= 0;
    regTabPtr.p->m_attributes[MM].m_no_of_varsize= 0;
    regTabPtr.p->m_attributes[MM].m_no_of_dynamic= 0;
    regTabPtr.p->m_attributes[MM].m_no_of_dyn_fix= 0;
    regTabPtr.p->m_attributes[MM].m_no_of_dyn_var= 0;
    regTabPtr.p->m_attributes[DD].m_no_of_fixsize= 0;
    regTabPtr.p->m_attributes[DD].m_no_of_varsize= 0;
    regTabPtr.p->m_attributes[DD].m_no_of_dynamic= 0;
    regTabPtr.p->m_attributes[DD].m_no_of_dyn_fix= 0;
    regTabPtr.p->m_attributes[DD].m_no_of_dyn_var= 0;

    // Reserve space for bitmap length
    regTabPtr.p->m_dyn_null_bits= DYN_BM_LEN_BITS; 
    regTabPtr.p->noOfKeyAttr= noOfKeyAttr;
    regTabPtr.p->noOfCharsets= noOfCharsets;
    regTabPtr.p->m_no_of_attributes= noOfAttributes;
    
    regTabPtr.p->dynTabDescriptor= RNIL;
    
    Uint32 offset[10];
    Uint32 allocSize= getTabDescrOffsets(noOfAttributes, noOfCharsets,
                                         noOfKeyAttr, offset);
    Uint32 tableDescriptorRef= allocTabDescr(allocSize);
    if (tableDescriptorRef == RNIL) {
      jam();
      fragrefuse4Lab(signal, fragOperPtr, regFragPtr, regTabPtr.p, fragId);
      return;
    }
    setUpDescriptorReferences(tableDescriptorRef, regTabPtr.p, offset);
  } else {
    jam();
    fragOperPtr.p->definingFragment= false;
  }
  signal->theData[0]= fragOperPtr.p->lqhPtrFrag;
  signal->theData[1]= fragOperPtr.i;
  signal->theData[2]= regFragPtr.i;
  signal->theData[3]= fragId;
  sendSignal(fragOperPtr.p->lqhBlockrefFrag, GSN_TUPFRAGCONF, signal, 4, JBB);
  return;
}

bool Dbtup::addfragtotab(Tablerec* const regTabPtr,
                         Uint32 fragId,
                         Uint32 fragIndex) 
{
  for (Uint32 i = 0; i < MAX_FRAG_PER_NODE; i++) {
    jam();
    if (regTabPtr->fragid[i] == RNIL) {
      jam();
      regTabPtr->fragid[i]= fragId;
      regTabPtr->fragrec[i]= fragIndex;
      return true;
    }
  }
  return false;
}

void Dbtup::getFragmentrec(FragrecordPtr& regFragPtr,
                           Uint32 fragId,
                           Tablerec* const regTabPtr) 
{
  for (Uint32 i = 0; i < MAX_FRAG_PER_NODE; i++) {
    jam();
    if (regTabPtr->fragid[i] == fragId) {
      jam();
      regFragPtr.i= regTabPtr->fragrec[i];
      ptrCheckGuard(regFragPtr, cnoOfFragrec, fragrecord);
      return;
    }
  }
  regFragPtr.i= RNIL;
  ptrNull(regFragPtr);
}

void Dbtup::seizeFragrecord(FragrecordPtr& regFragPtr) 
{
  regFragPtr.i= cfirstfreefrag;
  ptrCheckGuard(regFragPtr, cnoOfFragrec, fragrecord);
  cfirstfreefrag= regFragPtr.p->nextfreefrag;
  regFragPtr.p->nextfreefrag= RNIL;
}

void Dbtup::seizeFragoperrec(FragoperrecPtr& fragOperPtr) 
{
  fragOperPtr.i= cfirstfreeFragopr;
  ptrCheckGuard(fragOperPtr, cnoOfFragoprec, fragoperrec);
  cfirstfreeFragopr = fragOperPtr.p->nextFragoprec;
  fragOperPtr.p->nextFragoprec = RNIL;
  fragOperPtr.p->inUse = true;
}//Dbtup::seizeFragoperrec()

void Dbtup::seizeAlterTabOperation(AlterTabOperationPtr& alterTabOpPtr)
{
  alterTabOpPtr.i= cfirstfreeAlterTabOp;
  ptrCheckGuard(alterTabOpPtr, cnoOfAlterTabOps, alterTabOperRec);
  cfirstfreeAlterTabOp= alterTabOpPtr.p->nextAlterTabOp;
  alterTabOpPtr.p->nextAlterTabOp= RNIL;
}

/* **************************************************************** */
/* **************          TUP_ADD_ATTRREQ       ****************** */
/* **************************************************************** */
void Dbtup::execTUP_ADD_ATTRREQ(Signal* signal)
{
  FragrecordPtr regFragPtr;
  FragoperrecPtr fragOperPtr;
  TablerecPtr regTabPtr;

  jamEntry();
  fragOperPtr.i= signal->theData[0];
  ptrCheckGuard(fragOperPtr, cnoOfFragoprec, fragoperrec);
  Uint32 attrId = signal->theData[2];
  Uint32 attrDescriptor = signal->theData[3];
  // DICT sends charset number in upper half
  Uint32 csNumber = (signal->theData[4] >> 16);

  regTabPtr.i= fragOperPtr.p->tableidFrag;
  ptrCheckGuard(regTabPtr, cnoOfTablerec, tablerec);

  Uint32 fragId= fragOperPtr.p->fragidFrag;

  getFragmentrec(regFragPtr, fragId, regTabPtr.p);
  ndbrequire(regFragPtr.i != RNIL);

  ndbrequire(fragOperPtr.p->attributeCount > 0);
  fragOperPtr.p->attributeCount--;
  const bool lastAttr = (fragOperPtr.p->attributeCount == 0);

  if (regTabPtr.p->tableStatus != DEFINING)
  {
    ndbrequire(regTabPtr.p->tableStatus == DEFINED);
    signal->theData[0] = fragOperPtr.p->lqhPtrFrag;
    signal->theData[1] = lastAttr;
    sendSignal(fragOperPtr.p->lqhBlockrefFrag, GSN_TUP_ADD_ATTCONF, 
	       signal, 2, JBB);
    
    if(lastAttr)
    {
      jam();
      /**
       * Init Disk_alloc_info
       */
      CreateFilegroupImplReq rep;
      if(regTabPtr.p->m_no_of_disk_attributes)
      {
	Tablespace_client tsman(0, c_tsman, 0, 0, 
				regFragPtr.p->m_tablespace_id);
	ndbrequire(tsman.get_tablespace_info(&rep) == 0);
	regFragPtr.p->m_logfile_group_id= rep.tablespace.logfile_group_id;
      }
      else
      {
	jam();
	regFragPtr.p->m_logfile_group_id = RNIL;
      }
      new (&regFragPtr.p->m_disk_alloc_info)
	Disk_alloc_info(regTabPtr.p, rep.tablespace.extent_size);
      releaseFragoperrec(fragOperPtr);      
    }
    return;
  }
    
  Uint32 firstTabDesIndex= regTabPtr.p->tabDescriptor + (attrId * ZAD_SIZE);
  setTabDescrWord(firstTabDesIndex, attrDescriptor);
  Uint32 attrLen = AttributeDescriptor::getSize(attrDescriptor);
  
  Uint32 attrDes2= 0;
  Uint32 bytes= AttributeDescriptor::getSizeInBytes(attrDescriptor);
  Uint32 words= (bytes + 3) / 4;
  Uint32 ind= AttributeDescriptor::getDiskBased(attrDescriptor);
  if (!AttributeDescriptor::getDynamic(attrDescriptor)) {
    jam();
    Uint32 null_pos;
    ndbrequire(ind <= 1);
    null_pos= fragOperPtr.p->m_null_bits[ind];

    if (AttributeDescriptor::getNullable(attrDescriptor)) 
    {
      jam();
      fragOperPtr.p->m_null_bits[ind]++;
    } 

    if (AttributeDescriptor::getArrayType(attrDescriptor)==NDB_ARRAYTYPE_FIXED)
    {
      jam();
      regTabPtr.p->m_attributes[ind].m_no_of_fixsize++;
      if(attrLen == 0)
      {
        /* Static bit type. */
	jam();
	Uint32 bitCount = AttributeDescriptor::getArraySize(attrDescriptor);
	fragOperPtr.p->m_null_bits[ind] += bitCount;
      }
    }
    else
    {
      jam();
      regTabPtr.p->m_attributes[ind].m_no_of_varsize++;
    }
    
    AttributeOffset::setNullFlagPos(attrDes2, null_pos);
  } else {
    /* A dynamic attribute. */
    ndbrequire(ind==MM);
    regTabPtr.p->m_attributes[ind].m_no_of_dynamic++;
    /*
       The dynamic attribute format always require a 'null' bit. So
       storing NOT NULL attributes as dynamic is not all that useful
       (but not harmful in any way either).
       Later we might implement NOT NULL DEFAULT xxx by storing the value
       xxx internally as 'null'.
    */

    Uint32 null_pos= regTabPtr.p->m_dyn_null_bits;

    if (AttributeDescriptor::getArrayType(attrDescriptor)==NDB_ARRAYTYPE_FIXED)
    {
      /* A fixed-size dynamic attribute. */
      jam();
      if (AttributeDescriptor::getSize(attrDescriptor)==0)
      {
        /*
          Bit type. These are stored directly in the bitmap.
          This means that we will still use some space for a dynamic NULL
          bittype if a following dynamic attribute is non-NULL.
        */
        Uint32 bits= AttributeDescriptor::getArraySize(attrDescriptor);
        /*
          The NULL bit is stored after the data bits, so that we automatically
          ensure that the full size bitmap is stored when non-NULL.
        */
        null_pos+= bits;
        regTabPtr.p->m_dyn_null_bits+= bits+1;
      }
      else
      {
        jam();
        /*
          We use one NULL bit per 4 bytes of dynamic fixed-size attribute. So
          for dynamic fixsize longer than 64 bytes (16 null bits), it is more
          efficient to store them as dynamic varsize internally.
        */
        if(words > InternalMaxDynFix)
          goto treat_as_varsize;

        regTabPtr.p->m_attributes[ind].m_no_of_dyn_fix++;
        Uint32 null_bits= (bytes+3) >> 2;
        regTabPtr.p->m_dyn_null_bits+= null_bits;
      }
    }
    else
    {
      /* A variable-sized dynamic attribute. */
    treat_as_varsize:
      jam();
      regTabPtr.p->m_attributes[ind].m_no_of_dyn_var++;
      regTabPtr.p->m_dyn_null_bits++;
    }
    AttributeOffset::setNullFlagPos(attrDes2, null_pos);

    ndbassert((regTabPtr.p->m_attributes[ind].m_no_of_dyn_var +
               regTabPtr.p->m_attributes[ind].m_no_of_dyn_fix) <=
              regTabPtr.p->m_attributes[ind].m_no_of_dynamic);
  }
  handleCharsetPos(csNumber, regTabPtr.p->charsetArray,
                   regTabPtr.p->noOfCharsets,
                   fragOperPtr.p->charsetIndex, attrDes2);
  setTabDescrWord(firstTabDesIndex + 1, attrDes2);

  if (ERROR_INSERTED(4009) && regTabPtr.p->fragid[0] == fragId && attrId == 0||
      ERROR_INSERTED(4010) && regTabPtr.p->fragid[0] == fragId && lastAttr ||
      ERROR_INSERTED(4011) && regTabPtr.p->fragid[1] == fragId && attrId == 0||
      ERROR_INSERTED(4012) && regTabPtr.p->fragid[1] == fragId && lastAttr) {
    jam();
    terrorCode = 1;
    addattrrefuseLab(signal, regFragPtr, fragOperPtr, regTabPtr.p, fragId);
    CLEAR_ERROR_INSERT_VALUE;
    return;
  }

/* **************************************************************** */
/* **************          TUP_ADD_ATTCONF       ****************** */
/* **************************************************************** */
  if (! lastAttr) {
    jam();
    signal->theData[0] = fragOperPtr.p->lqhPtrFrag;
    signal->theData[1] = lastAttr;
    sendSignal(fragOperPtr.p->lqhBlockrefFrag, GSN_TUP_ADD_ATTCONF, 
	       signal, 2, JBB);
    return;
  }
  
  ndbrequire(regTabPtr.p->tableStatus == DEFINING);
  regTabPtr.p->tableStatus= DEFINED;
  regFragPtr.p->fragStatus= ACTIVE;
  
#define BTW(x) ((x+31) >> 5)
  regTabPtr.p->m_offsets[MM].m_null_words= BTW(fragOperPtr.p->m_null_bits[MM]);
  regTabPtr.p->m_offsets[DD].m_null_words= BTW(fragOperPtr.p->m_null_bits[DD]);
#undef BTW

  /* Allocate  dynamic descriptor. */
  Uint32 offset[3];
  Uint32 allocSize= getDynTabDescrOffsets((regTabPtr.p->m_dyn_null_bits+31)>>5,
                                          offset);
  Uint32 dynTableDescriptorRef= allocTabDescr(allocSize);
  if (dynTableDescriptorRef == RNIL) {
    jam();
    addattrrefuseLab(signal, regFragPtr, fragOperPtr, regTabPtr.p, fragId);
    return;
  }
  setupDynDescriptorReferences(dynTableDescriptorRef, regTabPtr.p, offset);

  /* Compute table aggregate metadata. */
  computeTableMetaData(regTabPtr.p);

#if 0
  ndbout << *regTabPtr.p << endl;
  Uint32 idx= regTabPtr.p->tabDescriptor;
  for(Uint32 i = 0; i<regTabPtr.p->m_no_of_attributes; i++)
  {
    ndbout << i << ": " << endl;
    ndbout << *(AttributeDescriptor*)(tableDescriptor+idx) << endl;
    ndbout << *(AttributeOffset*)(tableDescriptor+idx+1) << endl;
    idx += 2;
  }
#endif
  
  {
#ifdef MIN_ROWS_NOT_SUPPORTED
    Uint32 fix_tupheader = regTabPtr.p->m_offsets[MM].m_fix_header_size;
    ndbassert(fix_tupheader > 0);
    Uint32 noRowsPerPage = ZWORDS_ON_PAGE / fix_tupheader;
    Uint32 noAllocatedPages =
      (fragOperPtr.p->minRows + noRowsPerPage - 1 )/ noRowsPerPage;
    if (fragOperPtr.p->minRows == 0)
      noAllocatedPages = 2;
    else if (noAllocatedPages == 0)
      noAllocatedPages = 2;
#endif
    
    Uint32 noAllocatedPages = allocFragPage(regFragPtr.p);
    
    if (noAllocatedPages == 0) {
      jam();
      terrorCode = ZNO_PAGES_ALLOCATED_ERROR;
      addattrrefuseLab(signal, regFragPtr, fragOperPtr, regTabPtr.p, fragId);
      return;
    }//if
  }

  CreateFilegroupImplReq rep;
  if(regTabPtr.p->m_no_of_disk_attributes)
  {
    jam();
    Tablespace_client tsman(0, c_tsman, 0, 0, 
			    regFragPtr.p->m_tablespace_id);
    ndbrequire(tsman.get_tablespace_info(&rep) == 0);
    regFragPtr.p->m_logfile_group_id= rep.tablespace.logfile_group_id;
  }
  else
  {
    jam();
    regFragPtr.p->m_logfile_group_id = RNIL;
  }

  new (&regFragPtr.p->m_disk_alloc_info)
    Disk_alloc_info(regTabPtr.p, rep.tablespace.extent_size); 
  
  if (regTabPtr.p->m_no_of_disk_attributes)
  {
    jam();
    if(!(getNodeState().startLevel == NodeState::SL_STARTING && 
	 getNodeState().starting.startPhase <= 4))
    {
      Callback cb;
      jam();

      cb.m_callbackData= fragOperPtr.i;
      cb.m_callbackFunction = 
	safe_cast(&Dbtup::undo_createtable_callback);
      Uint32 sz= sizeof(Disk_undo::Create) >> 2;
      
      Logfile_client lgman(this, c_lgman, regFragPtr.p->m_logfile_group_id);
      if((terrorCode = 
          c_lgman->alloc_log_space(regFragPtr.p->m_logfile_group_id, sz)))
      {
        addattrrefuseLab(signal, regFragPtr, fragOperPtr, regTabPtr.p, fragId);
        return;
      }
      
      int res= lgman.get_log_buffer(signal, sz, &cb);
      switch(res){
      case 0:
        jam();
	signal->theData[0] = 1;
	return;
      case -1:
	ndbrequire("NOT YET IMPLEMENTED" == 0);
	break;
      }
      execute(signal, cb, regFragPtr.p->m_logfile_group_id);
      return;
    }
  }
  
  signal->theData[0] = fragOperPtr.p->lqhPtrFrag;
  signal->theData[1] = lastAttr;
  sendSignal(fragOperPtr.p->lqhBlockrefFrag, GSN_TUP_ADD_ATTCONF, 
	     signal, 2, JBB);

  releaseFragoperrec(fragOperPtr);

  return;
}

void
Dbtup::execALTER_TAB_REQ(Signal *signal)
{
  jamEntry();
  if(!assembleFragments(signal))
    return;
  AlterTabReq *const req= (AlterTabReq *)signal->getDataPtr();
  if (!AlterTableReq::getAddAttrFlag(req->changeMask))
  {
    /* Nothing to do in TUP. */
    releaseSections(signal);
    sendAlterTabConf(signal, req);
    return;
  }

  TablerecPtr regTabPtr;
  regTabPtr.i= req->tableId;
  ptrCheckGuard(regTabPtr, cnoOfTablerec, tablerec);

  AlterTabReq::RequestType alterType=
    (AlterTabReq::RequestType)req->requestType;

  if(alterType==AlterTabReq::AlterTablePrepare)
  {
    handleAlterTabPrepare(signal, regTabPtr.p);
    return;
  }

  AlterTabOperationPtr regAlterTabOpPtr;
  if (req->clientData==RNIL)
  {
    /* This means that we failed in prepare, or never got there. */
    sendAlterTabConf(signal, req);
    return;
  }
  regAlterTabOpPtr.i= req->clientData;
  ptrCheckGuard(regAlterTabOpPtr, cnoOfAlterTabOps, alterTabOperRec);

  if(alterType==AlterTabReq::AlterTableCommit)
  {
    handleAlterTableCommit(signal, regAlterTabOpPtr, regTabPtr.p);
  }
  else if(alterType==AlterTabReq::AlterTableRevert)
  {
    handleAlterTableAbort(signal, regAlterTabOpPtr, regTabPtr.p);
  }
  else
  {
    ndbrequire(false);
  }
}

void
Dbtup::handleAlterTabPrepare(Signal *signal, const Tablerec *regTabPtr)
{
  AlterTabReq *const req= (AlterTabReq *)signal->getDataPtr();

  Uint32 noOfNewAttr= req->noOfNewAttr;
  Uint32 newNoOfCharsets= req->newNoOfCharsets;
  Uint32 newNoOfKeyAttrs= req->newNoOfKeyAttrs;

  ndbrequire(signal->getNoOfSections() == 1);
  ndbrequire((25+noOfNewAttr*2)<<2 < sizeof(signal->theData));

  /* Get the array of attribute descriptor words. */
  SegmentedSectionPtr ssPtr;
  Uint32 *attrInfo= signal->theData+25;
  signal->getSection(ssPtr, 0);
  copy(attrInfo, ssPtr);
  releaseSections(signal);

  Uint32 oldNoOfAttr= regTabPtr->m_no_of_attributes;
  Uint32 newNoOfAttr= oldNoOfAttr+noOfNewAttr;

  /* Can only add attributes if varpart already present. */
  if((regTabPtr->m_attributes[MM].m_no_of_varsize +
      regTabPtr->m_attributes[MM].m_no_of_dynamic +
      (regTabPtr->m_bits & Tablerec::TR_ForceVarPart)) == 0)
  {
    sendAlterTabRef(signal, req, ZINVALID_ALTER_TAB);
    return;
  }

  AlterTabOperationPtr regAlterTabOpPtr;
  seizeAlterTabOperation(regAlterTabOpPtr);

  regAlterTabOpPtr.p->newNoOfAttrs= newNoOfAttr;
  regAlterTabOpPtr.p->newNoOfCharsets= newNoOfCharsets;
  regAlterTabOpPtr.p->newNoOfKeyAttrs= newNoOfKeyAttrs;

  /* Allocate a new (possibly larger) table descriptor buffer. */
  Uint32 allocSize= getTabDescrOffsets(newNoOfAttr, newNoOfCharsets,
                                       newNoOfKeyAttrs,
                                       regAlterTabOpPtr.p->tabDesOffset);
  Uint32 tableDescriptorRef= allocTabDescr(allocSize);
  if (tableDescriptorRef == RNIL) {
    jam();
    releaseAlterTabOpRec(regAlterTabOpPtr);
    sendAlterTabRef(signal, req, terrorCode);
    return;
  }
  regAlterTabOpPtr.p->tableDescriptor= tableDescriptorRef;
  regAlterTabOpPtr.p->desAllocSize= allocSize;

  /*
    Get new pointers into tableDescriptor, and copy over old data.
    (Rest will be recomputed in computeTableMetaData() in case of
    ALTER_TAB_REQ[commit]).
  */
  Uint32* desc= &tableDescriptor[tableDescriptorRef].tabDescr;
  CHARSET_INFO** CharsetArray=
    (CHARSET_INFO**)(desc + regAlterTabOpPtr.p->tabDesOffset[2]);
  memcpy(CharsetArray, regTabPtr->charsetArray,
         sizeof(*CharsetArray)*regTabPtr->noOfCharsets);
  Uint32 *attrDesPtr= desc + regAlterTabOpPtr.p->tabDesOffset[4];
  memcpy(attrDesPtr,
         &tableDescriptor[regTabPtr->tabDescriptor].tabDescr,
         (ZAD_SIZE<<2)*oldNoOfAttr);
  attrDesPtr+= ZAD_SIZE*oldNoOfAttr;

  /*
    Loop over the new attributes to add.
     - Save AttributeDescriptor word in new TabDescriptor record.
     - Compute charset pos, as we will not save original csNumber.
     - Compute size needed for dynamic bitmap mask allocation.
     - Compute number of dynamic varsize, needed for fixsize offset calculation
       in ALTER_TAB_REQ[commit];
   */
  Uint32 charsetIndex= regTabPtr->noOfCharsets;
  Uint32 dyn_nullbits= regTabPtr->m_dyn_null_bits;
  if (dyn_nullbits == 0)
  {
    jam();
    dyn_nullbits = DYN_BM_LEN_BITS;
  }

  Uint32 noDynFix= regTabPtr->m_attributes[MM].m_no_of_dyn_fix;
  Uint32 noDynVar= regTabPtr->m_attributes[MM].m_no_of_dyn_var;
  Uint32 noDynamic= regTabPtr->m_attributes[MM].m_no_of_dynamic;
  for (Uint32 i= 0; i<noOfNewAttr; i++)
  {
    Uint32 attrDescriptor= *attrInfo++;
    Uint32 csNumber= (*attrInfo++ >> 16);
    Uint32 attrDes2= 0;

    /* Only dynamic attributes possible for add attr */
    ndbrequire(AttributeDescriptor::getDynamic(attrDescriptor));
    ndbrequire(!AttributeDescriptor::getDiskBased(attrDescriptor));

    handleCharsetPos(csNumber, CharsetArray, newNoOfCharsets,
                     charsetIndex, attrDes2);

    Uint32 null_pos= dyn_nullbits;
    Uint32 arrType= AttributeDescriptor::getArrayType(attrDescriptor);
    noDynamic++;
    if (arrType==NDB_ARRAYTYPE_FIXED)
    {
      Uint32 words= AttributeDescriptor::getSizeInWords(attrDescriptor);

      if(AttributeDescriptor::getSize(attrDescriptor) > 0)
      {
        jam();
        if(words > InternalMaxDynFix)
          goto treat_as_varsize;
        noDynFix++;
        dyn_nullbits+= words;
      }
      else
      {
        /* Bit type. */
        jam();
        Uint32 bits= AttributeDescriptor::getArraySize(attrDescriptor);
        null_pos+= bits;
        dyn_nullbits+= bits+1;
      }
    }
    else
    {
      jam();
    treat_as_varsize:
      noDynVar++;
      dyn_nullbits++;
    }
    AttributeOffset::setNullFlagPos(attrDes2, null_pos);

    *attrDesPtr++= attrDescriptor;
    *attrDesPtr++= attrDes2;
  }
  ndbassert(newNoOfCharsets==charsetIndex);

  regAlterTabOpPtr.p->noOfDynNullBits= dyn_nullbits;
  ndbassert(noDynamic ==
            regTabPtr->m_attributes[MM].m_no_of_dynamic + noOfNewAttr);
  regAlterTabOpPtr.p->noOfDynFix= noDynFix;
  regAlterTabOpPtr.p->noOfDynVar= noDynVar;
  regAlterTabOpPtr.p->noOfDynamic= noDynamic;

  /* Allocate the new (possibly larger) dynamic descriptor. */
  allocSize= getDynTabDescrOffsets((dyn_nullbits+31)>>5,
                                   regAlterTabOpPtr.p->dynTabDesOffset);
  Uint32 dynTableDescriptorRef= allocTabDescr(allocSize);
  if (dynTableDescriptorRef == RNIL) {
    jam();
    freeTabDescr(tableDescriptorRef, regAlterTabOpPtr.p->desAllocSize);
    releaseAlterTabOpRec(regAlterTabOpPtr);
    sendAlterTabRef(signal, req, terrorCode);
    return;
  }
  regAlterTabOpPtr.p->dynDesAllocSize= allocSize;
  regAlterTabOpPtr.p->dynTableDescriptor= dynTableDescriptorRef;

  sendAlterTabConf(signal, req, regAlterTabOpPtr.i);
}

void
Dbtup::sendAlterTabRef(Signal *signal, AlterTabReq *req, Uint32 errorCode)
{
  signal->header.m_noOfSections = 0;

  AlterTabAll *const src= (AlterTabAll *)req;
  Uint32 senderRef= src->req.senderRef;
  Uint32 senderData= src->req.senderData;
  Uint32 requestType= src->req.requestType;

  AlterTabAll *const dst= (AlterTabAll *)signal->getDataPtrSend();
  dst->ref.senderRef= reference();
  dst->ref.senderData= senderData;
  dst->ref.errorCode= errorCode;
  dst->ref.errorLine= 0;
  dst->ref.errorKey= 0;
  dst->ref.errorStatus= 0;
  dst->ref.requestType= requestType;

  sendSignal(senderRef, GSN_ALTER_TAB_REF, signal,
             AlterTabRef::SignalLength, JBB);
}

void
Dbtup::sendAlterTabConf(Signal *signal, AlterTabReq *req, Uint32 clientData)
{
  signal->header.m_noOfSections = 0;

  AlterTabAll *const src= (AlterTabAll *)req;
  Uint32 senderRef= src->req.senderRef;
  Uint32 senderData= src->req.senderData;
  Uint32 changeMask= src->req.changeMask;
  Uint32 tableId= src->req.tableId;
  Uint32 tableVersion= src->req.tableVersion;
  Uint32 gci= src->req.gci;
  Uint32 requestType= src->req.requestType;

  AlterTabAll *const dst= (AlterTabAll *)signal->getDataPtrSend();
  dst->conf.senderRef= reference();
  dst->conf.senderData= senderData;
  dst->conf.changeMask= changeMask;
  dst->conf.tableId= tableId;
  dst->conf.tableVersion= tableVersion;
  dst->conf.gci= gci;
  dst->conf.requestType= requestType;
  dst->conf.clientData= clientData;

  sendSignal(senderRef, GSN_ALTER_TAB_CONF, signal,
             AlterTabConf::SignalLength, JBB);
}

void
Dbtup::handleAlterTableCommit(Signal *signal,
                              AlterTabOperationPtr regAlterTabOpPtr,
                              Tablerec *regTabPtr)
{
  /* Free old table descriptors. */
  releaseTabDescr(regTabPtr);

  /* Set new attribute counts. */
  regTabPtr->m_no_of_attributes= regAlterTabOpPtr.p->newNoOfAttrs;
  regTabPtr->noOfCharsets= regAlterTabOpPtr.p->newNoOfCharsets;
  regTabPtr->noOfKeyAttr= regAlterTabOpPtr.p->newNoOfKeyAttrs;
  regTabPtr->m_attributes[MM].m_no_of_dyn_fix= regAlterTabOpPtr.p->noOfDynFix;
  regTabPtr->m_attributes[MM].m_no_of_dyn_var= regAlterTabOpPtr.p->noOfDynVar;
  regTabPtr->m_attributes[MM].m_no_of_dynamic= regAlterTabOpPtr.p->noOfDynamic;
  regTabPtr->m_dyn_null_bits= regAlterTabOpPtr.p->noOfDynNullBits;

  /* Install the new (larger) table descriptors. */
  setUpDescriptorReferences(regAlterTabOpPtr.p->tableDescriptor,
                            regTabPtr,
                            regAlterTabOpPtr.p->tabDesOffset);
  setupDynDescriptorReferences(regAlterTabOpPtr.p->dynTableDescriptor,
                               regTabPtr,
                               regAlterTabOpPtr.p->dynTabDesOffset);

  releaseAlterTabOpRec(regAlterTabOpPtr);

  /* Recompute aggregate table meta data. */
  computeTableMetaData(regTabPtr);

  sendAlterTabConf(signal, (AlterTabReq *)signal->getDataPtr());
}

void
Dbtup::handleAlterTableAbort(Signal *signal,
                             AlterTabOperationPtr regAlterTabOpPtr,
                              Tablerec *regTabPtr)
{
  freeTabDescr(regAlterTabOpPtr.p->tableDescriptor,
               regAlterTabOpPtr.p->desAllocSize);
  freeTabDescr(regAlterTabOpPtr.p->dynTableDescriptor,
               regAlterTabOpPtr.p->dynDesAllocSize);
  releaseAlterTabOpRec(regAlterTabOpPtr);

  sendAlterTabConf(signal, (AlterTabReq *)signal->getDataPtr());
}

/*
  Update information for charset for a new attribute.
  If needed, attrDes2 will be updated with the correct charsetPos and
  charsetIndex will be updated to point to next free charsetPos slot.
*/
void
Dbtup::handleCharsetPos(Uint32 csNumber, CHARSET_INFO** charsetArray,
                        Uint32 noOfCharsets,
                        Uint32 & charsetIndex, Uint32 & attrDes2)
{
  if (csNumber != 0)
  { 
    CHARSET_INFO* cs = all_charsets[csNumber];
    ndbrequire(cs != NULL);
    Uint32 i= 0;
    while (i < charsetIndex)
    {
      jam();
      if (charsetArray[i] == cs)
	break;
      i++;
    }
    if (i == charsetIndex) {
      jam();
      ndbrequire(i < noOfCharsets);
      charsetArray[i]= cs;
      charsetIndex++;
    }
    AttributeOffset::setCharsetPos(attrDes2, i);
  }
}

/*
  This function (re-)computes aggregated metadata. It is called for
  both ALTER TABLE and CREATE TABLE.
 */
void
Dbtup::computeTableMetaData(Tablerec *regTabPtr)
{
  if (regTabPtr->m_dyn_null_bits == DYN_BM_LEN_BITS)
  {
    regTabPtr->m_dyn_null_bits = 0;
  }
  
  Uint32 dyn_null_words= (regTabPtr->m_dyn_null_bits+31)>>5;
  regTabPtr->m_offsets[MM].m_dyn_null_words= dyn_null_words;

  /* Compute the size of the static headers. */
  Uint32 pos[2] = { 0, 0 };
  if (regTabPtr->m_bits & Tablerec::TR_Checksum)
  {
    pos[MM]++; 
  }

  if (regTabPtr->m_bits & Tablerec::TR_RowGCI)
  {
    pos[MM]++;
    pos[DD]++;
  }

  regTabPtr->m_no_of_disk_attributes= 
    regTabPtr->m_attributes[DD].m_no_of_fixsize +
    regTabPtr->m_attributes[DD].m_no_of_varsize;
  if(regTabPtr->m_no_of_disk_attributes > 0)
  {
    /* Room for disk part location. */
    regTabPtr->m_offsets[MM].m_disk_ref_offset= pos[MM];
    pos[MM] += Disk_part_ref::SZ32; // 8 bytes
  }
  else
  {
    regTabPtr->m_offsets[MM].m_disk_ref_offset= pos[MM] - Disk_part_ref::SZ32;
  }

  if (regTabPtr->m_attributes[MM].m_no_of_varsize ||
      regTabPtr->m_attributes[MM].m_no_of_dynamic)
  {
    pos[MM] += Var_part_ref::SZ32;
    regTabPtr->m_bits &= ~(Uint32)Tablerec::TR_ForceVarPart;
  }
  else if (regTabPtr->m_bits & Tablerec::TR_ForceVarPart)
  {
    pos[MM] += Var_part_ref::SZ32;
  }

  regTabPtr->m_offsets[MM].m_null_offset= pos[MM];
  regTabPtr->m_offsets[DD].m_null_offset= pos[DD];
  pos[MM]+= regTabPtr->m_offsets[MM].m_null_words;
  pos[DD]+= regTabPtr->m_offsets[DD].m_null_words;

  /*
    Compute the offsets for the attributes.
    For static fixed-size, this is the offset from the tuple pointer of the
    actual data.
    For static var-size and dynamic, this is the index into the offset array.

    We also compute the dynamic bitmasks here.
  */
  Uint32 *tabDesc= (Uint32*)(tableDescriptor+regTabPtr->tabDescriptor);
  Uint32 *dynDesc= (Uint32*)(tableDescriptor+regTabPtr->dynTabDescriptor);
  Uint32 fix_size[2]= {0, 0};
  Uint32 var_size[2]= {0, 0};
  Uint32 dyn_size[2]= {0, 0};
  Uint32 statvar_count= 0;
  Uint32 dynfix_count= 0;
  Uint32 dynvar_count= 0;
  Uint32 dynamic_count= 0;
  regTabPtr->blobAttributeMask.clear();
  regTabPtr->notNullAttributeMask.clear();
  bzero(regTabPtr->dynVarSizeMask, dyn_null_words<<2);
  bzero(regTabPtr->dynFixSizeMask, dyn_null_words<<2);

  for(Uint32 i= 0; i<regTabPtr->m_no_of_attributes; i++)
  {
    Uint32 attrDescriptor= *tabDesc++;
    Uint32 attrDes2= *tabDesc;
    Uint32 ind= AttributeDescriptor::getDiskBased(attrDescriptor);
    Uint32 attrLen = AttributeDescriptor::getSize(attrDescriptor);
    Uint32 arr= AttributeDescriptor::getArrayType(attrDescriptor);
    Uint32 size_in_words= AttributeDescriptor::getSizeInWords(attrDescriptor);
    Uint32 size_in_bytes= AttributeDescriptor::getSizeInBytes(attrDescriptor);
    Uint32 extType = AttributeDescriptor::getType(attrDescriptor);
    Uint32 off;

    if (extType == NDB_TYPE_BLOB || extType == NDB_TYPE_TEXT)
      regTabPtr->blobAttributeMask.set(i);
    if(!AttributeDescriptor::getNullable(attrDescriptor))
      regTabPtr->notNullAttributeMask.set(i);
    if (!AttributeDescriptor::getDynamic(attrDescriptor))
    {
      if(arr == NDB_ARRAYTYPE_FIXED)
      {
        if (attrLen!=0)
        {
          off= fix_size[ind] + pos[ind];
          fix_size[ind]+= size_in_words;
        }
        else
          off= 0;                               // Bit type
      }
      else
      {
        /* Static varsize. */
        ndbassert(ind==MM);
        off= statvar_count++;
        var_size[ind]+= size_in_bytes;
      }
    }
    else
    {
      /* Dynamic attribute. */
      dynamic_count++;
      ndbrequire(ind==MM);
      Uint32 null_pos= AttributeOffset::getNullFlagPos(attrDes2);
      dyn_size[ind]+= (size_in_words<<2);
      if(arr == NDB_ARRAYTYPE_FIXED)
      {
        jam();
        //if (extType == NDB_TYPE_BLOB || extType == NDB_TYPE_TEXT)
          //regTabPtr->blobAttributeMask.set(i);
        // ToDo: I wonder what else is needed to handle BLOB/TEXT, if anything?

        if (attrLen!=0)
        {
          jam();
          if(size_in_words>InternalMaxDynFix)
            goto treat_as_varsize;

          off= dynfix_count++ + regTabPtr->m_attributes[ind].m_no_of_dyn_var;
          while(size_in_words-- > 0)
	  {
	    BitmaskImpl::set(dyn_null_words, 
			     regTabPtr->dynFixSizeMask, null_pos++);
	  }
        }
        else
          off= 0;                               // Bit type
      }
      else
      {
      treat_as_varsize:
        jam();
        off= dynvar_count++;
	BitmaskImpl::set(dyn_null_words, regTabPtr->dynVarSizeMask, null_pos);
      }
    }
    AttributeOffset::setOffset(attrDes2, off);
    *tabDesc++= attrDes2;
  }
  ndbassert(dynvar_count==regTabPtr->m_attributes[MM].m_no_of_dyn_var);
  ndbassert(dynfix_count==regTabPtr->m_attributes[MM].m_no_of_dyn_fix);
  ndbassert(dynamic_count==regTabPtr->m_attributes[MM].m_no_of_dynamic);
  ndbassert(statvar_count==regTabPtr->m_attributes[MM].m_no_of_varsize);

  regTabPtr->m_offsets[MM].m_fix_header_size= 
    Tuple_header::HeaderSize + fix_size[MM] + pos[MM];
  regTabPtr->m_offsets[DD].m_fix_header_size= 
    fix_size[DD] + pos[DD];

  if(regTabPtr->m_attributes[DD].m_no_of_varsize == 0 &&
     regTabPtr->m_attributes[DD].m_no_of_fixsize > 0)
    regTabPtr->m_offsets[DD].m_fix_header_size += Tuple_header::HeaderSize;

  Uint32 mm_vars= regTabPtr->m_attributes[MM].m_no_of_varsize;
  Uint32 mm_dyns= regTabPtr->m_attributes[MM].m_no_of_dyn_fix +
                  regTabPtr->m_attributes[MM].m_no_of_dyn_var;
  Uint32 dd_vars= regTabPtr->m_attributes[MM].m_no_of_varsize;
  Uint32 dd_dyns= regTabPtr->m_attributes[DD].m_no_of_dynamic;

  regTabPtr->m_offsets[MM].m_max_var_offset= var_size[MM];
  /*
    Size of the expanded dynamic part. Needs room for bitmap, (N+1) 16-bit
    offset words with 32-bit padding, and all attribute data.
  */
  regTabPtr->m_offsets[MM].m_max_dyn_offset= 
    (regTabPtr->m_offsets[MM].m_dyn_null_words<<2) + 4*((mm_dyns+2)>>1) +
    dyn_size[MM];
  
  regTabPtr->m_offsets[DD].m_max_var_offset= var_size[DD];
  regTabPtr->m_offsets[DD].m_max_dyn_offset= 
    (regTabPtr->m_offsets[DD].m_dyn_null_words<<2) + 4*((dd_dyns+2)>>1) +
    dyn_size[DD];

  /* Room for data for all the attributes. */
  Uint32 total_rec_size=
    pos[MM] + fix_size[MM] + pos[DD] + fix_size[DD] +
    ((var_size[MM] + 3) >> 2) + ((dyn_size[MM] + 3) >> 2) +
    ((var_size[DD] + 3) >> 2) + ((dyn_size[DD] + 3) >> 2);
  /*
    Room for offset arrays and dynamic bitmaps. There is one extra 16-bit
    offset in each offset array (for easy computation of final length).
    Also one word for storing total length of varsize+dynamic part
  */
  if(mm_vars + regTabPtr->m_attributes[MM].m_no_of_dynamic)
  {
    total_rec_size+= (mm_vars + 2) >> 1;
    total_rec_size+= regTabPtr->m_offsets[MM].m_dyn_null_words;
    total_rec_size+= (mm_dyns + 2) >> 1;
    total_rec_size+= 1;
  }
  /* Disk data varsize offset array (not currently used). */
  if(dd_vars)
    total_rec_size+= (dd_vars + 2) >> 1;
  /* Room for the header. */
  total_rec_size+= Tuple_header::HeaderSize;
  if(regTabPtr->m_no_of_disk_attributes)
    total_rec_size+= Tuple_header::HeaderSize;
  regTabPtr->total_rec_size= total_rec_size;

  setUpQueryRoutines(regTabPtr);
  setUpKeyArray(regTabPtr);
}

void
Dbtup::undo_createtable_callback(Signal* signal, Uint32 opPtrI, Uint32 unused)
{
  FragrecordPtr regFragPtr;
  FragoperrecPtr fragOperPtr;
  TablerecPtr regTabPtr;

  fragOperPtr.i= opPtrI;
  ptrCheckGuard(fragOperPtr, cnoOfFragoprec, fragoperrec);

  regTabPtr.i= fragOperPtr.p->tableidFrag;
  ptrCheckGuard(regTabPtr, cnoOfTablerec, tablerec);

  getFragmentrec(regFragPtr, fragOperPtr.p->fragidFrag, regTabPtr.p);
  ndbrequire(regFragPtr.i != RNIL);
  
  Logfile_client lgman(this, c_lgman, regFragPtr.p->m_logfile_group_id);

  Disk_undo::Create create;
  create.m_type_length= Disk_undo::UNDO_CREATE << 16 | (sizeof(create) >> 2);
  create.m_table = regTabPtr.i;
  
  Logfile_client::Change c[1] = {{ &create, sizeof(create) >> 2 } };
  
  Uint64 lsn= lgman.add_entry(c, 1);

  Logfile_client::Request req;
  req.m_callback.m_callbackData= fragOperPtr.i;
  req.m_callback.m_callbackFunction = 
    safe_cast(&Dbtup::undo_createtable_logsync_callback);
  
  int ret = lgman.sync_lsn(signal, lsn, &req, 0);
  switch(ret){
  case 0:
    return;
  case -1:
    warningEvent("Failed to sync log for create of table: %u", regTabPtr.i);
  default:
    execute(signal, req.m_callback, regFragPtr.p->m_logfile_group_id);
  }
}

void
Dbtup::undo_createtable_logsync_callback(Signal* signal, Uint32 ptrI, 
					 Uint32 res)
{
  jamEntry();
  FragoperrecPtr fragOperPtr;
  fragOperPtr.i= ptrI;
  ptrCheckGuard(fragOperPtr, cnoOfFragoprec, fragoperrec);
  
  signal->theData[0] = fragOperPtr.p->lqhPtrFrag;
  signal->theData[1] = 1;
  sendSignal(fragOperPtr.p->lqhBlockrefFrag, GSN_TUP_ADD_ATTCONF, 
	     signal, 2, JBB);
  
  releaseFragoperrec(fragOperPtr);  
}

/*
 * Descriptor has these parts:
 *
 * 0 readFunctionArray ( one for each attribute )
 * 1 updateFunctionArray ( ditto )
 * 2 charsetArray ( pointers to distinct CHARSET_INFO )
 * 3 readKeyArray ( attribute ids of keys )
 * 5 tabDescriptor ( attribute descriptors, each ZAD_SIZE )
 */
void Dbtup::setUpDescriptorReferences(Uint32 descriptorReference,
                                      Tablerec* const regTabPtr,
                                      const Uint32* offset)
{
  Uint32* desc= &tableDescriptor[descriptorReference].tabDescr;
  regTabPtr->readFunctionArray= (ReadFunction*)(desc + offset[0]);
  regTabPtr->updateFunctionArray= (UpdateFunction*)(desc + offset[1]);
  regTabPtr->charsetArray= (CHARSET_INFO**)(desc + offset[2]);
  regTabPtr->readKeyArray= descriptorReference + offset[3];
  regTabPtr->tabDescriptor= descriptorReference + offset[4];
  regTabPtr->m_real_order_descriptor = descriptorReference + offset[5];
}

void Dbtup::setupDynDescriptorReferences(Uint32 dynDescr,
                                         Tablerec* const regTabPtr,
                                         const Uint32* offset)
{
  regTabPtr->dynTabDescriptor= dynDescr;
  Uint32* desc= &tableDescriptor[dynDescr].tabDescr;
  regTabPtr->dynVarSizeMask= desc+offset[0];
  regTabPtr->dynFixSizeMask= desc+offset[1];
}

Uint32
Dbtup::sizeOfReadFunction()
{
  ReadFunction* tmp= (ReadFunction*)&tableDescriptor[0];
  TableDescriptor* start= &tableDescriptor[0];
  TableDescriptor * end= (TableDescriptor*)(tmp + 1);
  return (Uint32)(end - start);
}

void Dbtup::setUpKeyArray(Tablerec* const regTabPtr)
{
  ndbrequire((regTabPtr->readKeyArray + regTabPtr->noOfKeyAttr) <
              cnoOfTabDescrRec);
  Uint32* keyArray= &tableDescriptor[regTabPtr->readKeyArray].tabDescr;
  Uint32 countKeyAttr= 0;
  for (Uint32 i= 0; i < regTabPtr->m_no_of_attributes; i++) {
    jam();
    Uint32 refAttr= regTabPtr->tabDescriptor + (i * ZAD_SIZE);
    Uint32 attrDescriptor= getTabDescrWord(refAttr);
    if (AttributeDescriptor::getPrimaryKey(attrDescriptor)) {
      jam();
      AttributeHeader::init(&keyArray[countKeyAttr], i, 0);
      countKeyAttr++;
    }
  }
  ndbrequire(countKeyAttr == regTabPtr->noOfKeyAttr);

  /**
   * Setup real order array (16 bit per column)
   *
   * Sequence is [mm_fix mm_var mm_dynfix mm_dynvar dd_fix]
   */
  const Uint32 off= regTabPtr->m_real_order_descriptor;
  const Uint32 sz= (regTabPtr->m_no_of_attributes + 1) >> 1;
  ndbrequire((off + sz) < cnoOfTabDescrRec);
  
  Uint32 cnt= 0;
  Uint16* order= (Uint16*)&tableDescriptor[off].tabDescr;
  for (Uint32 type = 0; type < 5; type++)
  {
    for (Uint32 i= 0; i < regTabPtr->m_no_of_attributes; i++) 
    {
      jam();
      Uint32 refAttr= regTabPtr->tabDescriptor + (i * ZAD_SIZE);
      Uint32 desc = getTabDescrWord(refAttr);
      Uint32 t = 0;

      if (AttributeDescriptor::getDynamic(desc) &&
          AttributeDescriptor::getArrayType(desc) == NDB_ARRAYTYPE_FIXED &&
          AttributeDescriptor::getSize(desc) == 0)
      {
        /*
          Dynamic bit types are stored inside the dynamic NULL bitmap, and are
          never expanded. So we do not need any real_order_descriptor for
          them.
        */
        jam();
        if(type==0)
          cnt++;
        continue;
      }

      if (AttributeDescriptor::getArrayType(desc) != NDB_ARRAYTYPE_FIXED ||
          (AttributeDescriptor::getDynamic(desc) &&
           AttributeDescriptor::getArrayType(desc) == NDB_ARRAYTYPE_FIXED &&
           AttributeDescriptor::getSizeInWords(desc) > InternalMaxDynFix))
      {
	t += 1;
      }
      if (AttributeDescriptor::getDynamic(desc)) 
      {
	t += 2;
      }
      if (AttributeDescriptor::getDiskBased(desc))
      {
	t += 4;
      }
      ndbrequire(t < 5);              // Disk data currently only static/fixed
      if(t == type)
      {
	* order++ = i << ZAD_LOG_SIZE;
	cnt++;
      }
    }
  }
  ndbrequire(cnt == regTabPtr->m_no_of_attributes);
}

void Dbtup::addattrrefuseLab(Signal* signal,
                             FragrecordPtr regFragPtr,
                             FragoperrecPtr fragOperPtr,
                             Tablerec* const regTabPtr,
                             Uint32 fragId) 
{
  deleteFragTab(regTabPtr, fragId);
  releaseFragrec(regFragPtr);
  releaseTabDescr(regTabPtr);
  initTab(regTabPtr);

  signal->theData[0]= fragOperPtr.p->lqhPtrFrag;
  signal->theData[1]= terrorCode;
  sendSignal(fragOperPtr.p->lqhBlockrefFrag,
              GSN_TUP_ADD_ATTRREF, signal, 2, JBB);
  releaseFragoperrec(fragOperPtr);
}

void Dbtup::fragrefuse4Lab(Signal* signal,
                           FragoperrecPtr fragOperPtr,
                           FragrecordPtr regFragPtr,
                           Tablerec* const regTabPtr,
                           Uint32 fragId) 
{
  fragrefuse3Lab(signal, fragOperPtr, regFragPtr, regTabPtr, fragId);
  initTab(regTabPtr);
}

void Dbtup::fragrefuse3Lab(Signal* signal,
                           FragoperrecPtr fragOperPtr,
                           FragrecordPtr regFragPtr,
                           Tablerec* const regTabPtr,
                           Uint32 fragId) 
{
  fragrefuse2Lab(signal, fragOperPtr, regFragPtr);
}

void Dbtup::fragrefuse2Lab(Signal* signal,
                           FragoperrecPtr fragOperPtr,
                           FragrecordPtr regFragPtr) 
{
  fragrefuse1Lab(signal, fragOperPtr);
  releaseFragrec(regFragPtr);
}

void Dbtup::fragrefuse1Lab(Signal* signal, FragoperrecPtr fragOperPtr) 
{
  fragrefuseLab(signal, fragOperPtr);
  releaseFragoperrec(fragOperPtr);
}

void Dbtup::fragrefuseLab(Signal* signal, FragoperrecPtr fragOperPtr) 
{
  signal->theData[0]= fragOperPtr.p->lqhPtrFrag;
  signal->theData[1]= terrorCode;
  sendSignal(fragOperPtr.p->lqhBlockrefFrag, GSN_TUPFRAGREF, signal, 2, JBB);
}

void Dbtup::releaseFragoperrec(FragoperrecPtr fragOperPtr) 
{
  fragOperPtr.p->inUse = false;
  fragOperPtr.p->nextFragoprec = cfirstfreeFragopr;
  cfirstfreeFragopr = fragOperPtr.i;
}//Dbtup::releaseFragoperrec()

void Dbtup::releaseAlterTabOpRec(AlterTabOperationPtr regAlterTabOpPtr)
{
  regAlterTabOpPtr.p->nextAlterTabOp= cfirstfreeAlterTabOp;
  cfirstfreeAlterTabOp= regAlterTabOpPtr.i;
}

void Dbtup::deleteFragTab(Tablerec* const regTabPtr, Uint32 fragId) 
{
  for (Uint32 i = 0; i < MAX_FRAG_PER_NODE; i++) {
    jam();
    if (regTabPtr->fragid[i] == fragId) {
      jam();
      regTabPtr->fragid[i]= RNIL;
      regTabPtr->fragrec[i]= RNIL;
      return;
    }
  }
  ndbrequire(false);
}

/*
 * LQH aborts on-going create table operation.  The table is later
 * dropped by DICT.
 */
void Dbtup::abortAddFragOp(Signal* signal)
{
  FragoperrecPtr fragOperPtr;

  fragOperPtr.i = signal->theData[1];
  ptrCheckGuard(fragOperPtr, cnoOfFragoprec, fragoperrec);
  ndbrequire(fragOperPtr.p->inUse);
  releaseFragoperrec(fragOperPtr);
}

void
Dbtup::execDROP_TAB_REQ(Signal* signal)
{
  jamEntry();
  if (ERROR_INSERTED(4013)) {
#ifdef VM_TRACE
    verifytabdes();
#endif
  }
  DropTabReq* req= (DropTabReq*)signal->getDataPtr();
  
  TablerecPtr tabPtr;
  tabPtr.i= req->tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  
  tabPtr.p->m_dropTable.tabUserRef = req->senderRef;
  tabPtr.p->m_dropTable.tabUserPtr = req->senderData;
  tabPtr.p->tableStatus = DROPPING;

  signal->theData[0]= ZREL_FRAG;
  signal->theData[1]= tabPtr.i;
  signal->theData[2]= RNIL;
  sendSignal(cownref, GSN_CONTINUEB, signal, 3, JBB);
}

void Dbtup::releaseTabDescr(Tablerec* const regTabPtr) 
{
  Uint32 descriptor= regTabPtr->readKeyArray;
  if (descriptor != RNIL) {
    jam();
    Uint32 offset[10];
    getTabDescrOffsets(regTabPtr->m_no_of_attributes,
                       regTabPtr->noOfCharsets,
                       regTabPtr->noOfKeyAttr,
                       offset);

    regTabPtr->tabDescriptor= RNIL;
    regTabPtr->readKeyArray= RNIL;
    regTabPtr->readFunctionArray= NULL;
    regTabPtr->updateFunctionArray= NULL;
    regTabPtr->charsetArray= NULL;

    // move to start of descriptor
    descriptor -= offset[3];
    Uint32 retNo= getTabDescrWord(descriptor + ZTD_DATASIZE);
    ndbrequire(getTabDescrWord(descriptor + ZTD_HEADER) == ZTD_TYPE_NORMAL);
    ndbrequire(retNo == getTabDescrWord((descriptor + retNo) - ZTD_TR_SIZE));
    ndbrequire(ZTD_TYPE_NORMAL ==
               getTabDescrWord((descriptor + retNo) - ZTD_TR_TYPE));
    freeTabDescr(descriptor, retNo);
  }

  descriptor= regTabPtr->dynTabDescriptor;
  if(descriptor != RNIL)
  {
    jam();
    regTabPtr->dynTabDescriptor= RNIL;
    regTabPtr->dynVarSizeMask= NULL;
    regTabPtr->dynFixSizeMask= NULL;
    Uint32 retNo= getTabDescrWord(descriptor + ZTD_DATASIZE);
    ndbrequire(getTabDescrWord(descriptor + ZTD_HEADER) == ZTD_TYPE_NORMAL);
    ndbrequire(retNo == getTabDescrWord((descriptor + retNo) - ZTD_TR_SIZE));
    ndbrequire(ZTD_TYPE_NORMAL ==
               getTabDescrWord((descriptor + retNo) - ZTD_TR_TYPE));
    freeTabDescr(descriptor, retNo);
  }
}

void Dbtup::releaseFragment(Signal* signal, Uint32 tableId, 
			    Uint32 logfile_group_id)
{
  TablerecPtr tabPtr;
  tabPtr.i= tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  Uint32 fragIndex = RNIL;
  Uint32 fragId = RNIL;
  Uint32 i = 0;
  for (i = 0; i < MAX_FRAG_PER_NODE; i++) {
    jam();
    if (tabPtr.p->fragid[i] != RNIL) {
      jam();
      fragIndex= tabPtr.p->fragrec[i];
      fragId= tabPtr.p->fragid[i];
      break;
    }
  }
  if (fragIndex != RNIL) {
    jam();
    
    signal->theData[0] = ZUNMAP_PAGES;
    signal->theData[1] = tabPtr.i;
    signal->theData[2] = fragIndex;
    signal->theData[3] = 0;
    sendSignal(cownref, GSN_CONTINUEB, signal, 4, JBB);  
    return;
  }

  if (logfile_group_id != RNIL)
  {
    Callback cb;
    cb.m_callbackData= tabPtr.i;
    cb.m_callbackFunction = 
      safe_cast(&Dbtup::drop_table_log_buffer_callback);
    Uint32 sz= sizeof(Disk_undo::Drop) >> 2;
    int r0 = c_lgman->alloc_log_space(logfile_group_id, sz);
    if (r0)
    {
      jam();
      warningEvent("Failed to alloc log space for drop table: %u",
 		   tabPtr.i);
      goto done;
    }

    Logfile_client lgman(this, c_lgman, logfile_group_id);
    int res= lgman.get_log_buffer(signal, sz, &cb);
    switch(res){
    case 0:
      jam();
      return;
    case -1:
      warningEvent("Failed to get log buffer for drop table: %u",
		   tabPtr.i);
      c_lgman->free_log_space(logfile_group_id, sz);
      goto done;
      break;
    default:
      execute(signal, cb, logfile_group_id);
      return;
    }
  }

done:
  drop_table_logsync_callback(signal, tabPtr.i, RNIL);
}

void
Dbtup::drop_fragment_unmap_pages(Signal *signal, 
				 TablerecPtr tabPtr, 
				 FragrecordPtr fragPtr,
				 Uint32 pos)
{
  if (tabPtr.p->m_no_of_disk_attributes)
  {
    jam();
    Disk_alloc_info& alloc_info= fragPtr.p->m_disk_alloc_info;

    if (!alloc_info.m_unmap_pages.isEmpty())
    {
      jam();
      ndbout_c("waiting for unmape pages");
      signal->theData[0] = ZUNMAP_PAGES;
      signal->theData[1] = tabPtr.i;
      signal->theData[2] = fragPtr.i;
      signal->theData[3] = pos;
      sendSignal(cownref, GSN_CONTINUEB, signal, 4, JBB);  
      return;
    }
    while(alloc_info.m_dirty_pages[pos].isEmpty() && pos < MAX_FREE_LIST)
      pos++;
    
    if (pos == MAX_FREE_LIST)
    {
      if(alloc_info.m_curr_extent_info_ptr_i != RNIL)
      {
	Local_extent_info_list
	  list(c_extent_pool, alloc_info.m_free_extents[0]);
	Ptr<Extent_info> ext_ptr;
	c_extent_pool.getPtr(ext_ptr, alloc_info.m_curr_extent_info_ptr_i);
	list.add(ext_ptr);
	alloc_info.m_curr_extent_info_ptr_i= RNIL;
      }
      
      drop_fragment_free_extent(signal, tabPtr, fragPtr, 0);
      return;
    }
    
    Ptr<Page> pagePtr;
    ArrayPool<Page> *pool= (ArrayPool<Page>*)&m_global_page_pool;
    {
      LocalDLList<Page> list(*pool, alloc_info.m_dirty_pages[pos]);
      list.first(pagePtr);
      list.remove(pagePtr);
    }
    
    Page_cache_client::Request req;
    req.m_page.m_page_no = pagePtr.p->m_page_no;
    req.m_page.m_file_no = pagePtr.p->m_file_no;
    
    req.m_callback.m_callbackData= pos;
    req.m_callback.m_callbackFunction = 
      safe_cast(&Dbtup::drop_fragment_unmap_page_callback);
    
    int flags= Page_cache_client::COMMIT_REQ;
    int res= m_pgman.get_page(signal, req, flags);
    switch(res)
    {
    case 0:
    case -1:
      break;
    default:
      ndbrequire((Uint32)res == pagePtr.i);
      drop_fragment_unmap_page_callback(signal, pos, res);
    }
    return;
  }
  drop_fragment_free_extent(signal, tabPtr, fragPtr, 0);  
}

void
Dbtup::drop_fragment_unmap_page_callback(Signal* signal, 
					 Uint32 pos, Uint32 page_id)
{
  Ptr<GlobalPage> page;
  m_global_page_pool.getPtr(page, page_id);
  
  Local_key key;
  key.m_page_no = ((Page*)page.p)->m_page_no;
  key.m_file_no = ((Page*)page.p)->m_file_no;

  Uint32 fragId = ((Page*)page.p)->m_fragment_id;
  Uint32 tableId = ((Page*)page.p)->m_table_id;
  m_pgman.drop_page(key, page_id);

  TablerecPtr tabPtr;
  tabPtr.i= tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  
  FragrecordPtr fragPtr;
  getFragmentrec(fragPtr, fragId, tabPtr.p);
  
  signal->theData[0] = ZUNMAP_PAGES;
  signal->theData[1] = tabPtr.i;
  signal->theData[2] = fragPtr.i;
  signal->theData[3] = pos;
  sendSignal(cownref, GSN_CONTINUEB, signal, 4, JBB);  
}

void
Dbtup::drop_fragment_free_extent(Signal *signal, 
				 TablerecPtr tabPtr, 
				 FragrecordPtr fragPtr,
				 Uint32 pos)
{
  if (tabPtr.p->m_no_of_disk_attributes)
  {
    Disk_alloc_info& alloc_info= fragPtr.p->m_disk_alloc_info;
    for(; pos<EXTENT_SEARCH_MATRIX_SIZE; pos++)
    {
      if(!alloc_info.m_free_extents[pos].isEmpty())
      {
	jam();
	Callback cb;
	cb.m_callbackData= fragPtr.i;
	cb.m_callbackFunction = 
	  safe_cast(&Dbtup::drop_fragment_free_extent_log_buffer_callback);
#if NOT_YET_UNDO_FREE_EXTENT
	Uint32 sz= sizeof(Disk_undo::FreeExtent) >> 2;
	(void) c_lgman->alloc_log_space(fragPtr.p->m_logfile_group_id, sz);
	
	Logfile_client lgman(this, c_lgman, fragPtr.p->m_logfile_group_id);
	
	int res= lgman.get_log_buffer(signal, sz, &cb);
	switch(res){
	case 0:
	  jam();
	  return;
	case -1:
	  ndbrequire("NOT YET IMPLEMENTED" == 0);
	  break;
	default:
	  execute(signal, cb, fragPtr.p->m_logfile_group_id);
	  return;
	}
#else
	execute(signal, cb, fragPtr.p->m_logfile_group_id);	
	return;
#endif
      }
    }
    
    ArrayPool<Page> *cheat_pool= (ArrayPool<Page>*)&m_global_page_pool;
    for(pos= 0; pos<MAX_FREE_LIST; pos++)
    {
      ndbrequire(alloc_info.m_page_requests[pos].isEmpty());
      LocalDLList<Page> list(* cheat_pool, alloc_info.m_dirty_pages[pos]);
      list.remove();
    }
  }
  
  signal->theData[0] = ZFREE_VAR_PAGES;
  signal->theData[1] = tabPtr.i;
  signal->theData[2] = fragPtr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);  
}

void
Dbtup::drop_table_log_buffer_callback(Signal* signal, Uint32 tablePtrI,
				      Uint32 logfile_group_id)
{
  TablerecPtr tabPtr;
  tabPtr.i = tablePtrI;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  
  ndbrequire(tabPtr.p->m_no_of_disk_attributes);

  Disk_undo::Drop drop;
  drop.m_table = tabPtr.i;
  drop.m_type_length = 
    (Disk_undo::UNDO_DROP << 16) | (sizeof(drop) >> 2);
  Logfile_client lgman(this, c_lgman, logfile_group_id);
  
  Logfile_client::Change c[1] = {{ &drop, sizeof(drop) >> 2 } };
  Uint64 lsn = lgman.add_entry(c, 1);

  Logfile_client::Request req;
  req.m_callback.m_callbackData= tablePtrI;
  req.m_callback.m_callbackFunction = 
    safe_cast(&Dbtup::drop_table_logsync_callback);
  
  int ret = lgman.sync_lsn(signal, lsn, &req, 0);
  switch(ret){
  case 0:
    return;
  case -1:
    warningEvent("Failed to syn log for drop of table: %u", tablePtrI);
  default:
    execute(signal, req.m_callback, logfile_group_id);
  }
}

void
Dbtup::drop_table_logsync_callback(Signal* signal, 
				   Uint32 tabPtrI, 
				   Uint32 logfile_group_id)
{
  TablerecPtr tabPtr;
  tabPtr.i = tabPtrI;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  
  DropTabConf * const dropConf= (DropTabConf *)signal->getDataPtrSend();
  dropConf->senderRef= reference();
  dropConf->senderData= tabPtr.p->m_dropTable.tabUserPtr;
  dropConf->tableId= tabPtr.i;
  sendSignal(tabPtr.p->m_dropTable.tabUserRef, GSN_DROP_TAB_CONF,
             signal, DropTabConf::SignalLength, JBB);
  
  releaseTabDescr(tabPtr.p);
  initTab(tabPtr.p);
}

void
Dbtup::drop_fragment_free_extent_log_buffer_callback(Signal* signal,
						     Uint32 fragPtrI,
						     Uint32 unused)
{
  FragrecordPtr fragPtr;
  fragPtr.i = fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);

  TablerecPtr tabPtr;
  tabPtr.i = fragPtr.p->fragTableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);

  ndbrequire(tabPtr.p->m_no_of_disk_attributes);
  Disk_alloc_info& alloc_info= fragPtr.p->m_disk_alloc_info;  

  for(Uint32 pos = 0; pos<EXTENT_SEARCH_MATRIX_SIZE; pos++)
  {
    if(!alloc_info.m_free_extents[pos].isEmpty())
    {
      jam();
      Local_extent_info_list
	list(c_extent_pool, alloc_info.m_free_extents[pos]);
      Ptr<Extent_info> ext_ptr;
      list.first(ext_ptr);

#if NOT_YET_UNDO_FREE_EXTENT
#error "This code is complete"
#error "but not needed until we do dealloc of empty extents"
      Disk_undo::FreeExtent free;
      free.m_table = tabPtr.i;
      free.m_fragment = fragPtr.p->fragmentId;
      free.m_file_no = ext_ptr.p->m_key.m_file_no;
      free.m_page_no = ext_ptr.p->m_key.m_page_no;
      free.m_type_length = 
	(Disk_undo::UNDO_FREE_EXTENT << 16) | (sizeof(free) >> 2);
      Logfile_client lgman(this, c_lgman, fragPtr.p->m_logfile_group_id);
      
      Logfile_client::Change c[1] = {{ &free, sizeof(free) >> 2 } };
      Uint64 lsn = lgman.add_entry(c, 1);
#else
      Uint64 lsn = 0;
#endif
      
      Tablespace_client tsman(signal, c_tsman, tabPtr.i, 
			      fragPtr.p->fragmentId,
			      fragPtr.p->m_tablespace_id);
      
      tsman.free_extent(&ext_ptr.p->m_key, lsn);
      c_extent_hash.remove(ext_ptr);
      list.release(ext_ptr);
      
      signal->theData[0] = ZFREE_EXTENT;
      signal->theData[1] = tabPtr.i;
      signal->theData[2] = fragPtr.i;
      signal->theData[3] = pos;
      sendSignal(cownref, GSN_CONTINUEB, signal, 4, JBB);  
      return;
    }
  }
  ndbrequire(false);
}

void
Dbtup::drop_fragment_free_var_pages(Signal* signal)
{
  jam();
  Uint32 tableId = signal->theData[1];
  Uint32 fragPtrI = signal->theData[2];
  
  TablerecPtr tabPtr;
  tabPtr.i= tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  
  FragrecordPtr fragPtr;
  fragPtr.i = fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
  
  PagePtr pagePtr;
  if ((pagePtr.i = fragPtr.p->m_var_page_chunks) != RNIL)
  {
    c_page_pool.getPtr(pagePtr);
    Var_page* page = (Var_page*)pagePtr.p;
    fragPtr.p->m_var_page_chunks = page->next_chunk;

    Uint32 sz = page->chunk_size;
    returnCommonArea(pagePtr.i, sz);
    
    signal->theData[0] = ZFREE_VAR_PAGES;
    signal->theData[1] = tabPtr.i;
    signal->theData[2] = fragPtr.i;
    sendSignal(cownref, GSN_CONTINUEB, signal, 3, JBB);  
    return;
  }

  DynArr256::ReleaseIterator iter;
  DynArr256 map(c_page_map_pool, fragPtr.p->m_page_map);
  map.init(iter);
  signal->theData[0] = ZFREE_PAGES;
  signal->theData[1] = tabPtr.i;
  signal->theData[2] = fragPtrI;
  memcpy(signal->theData+3, &iter, sizeof(iter));
  sendSignal(reference(), GSN_CONTINUEB, signal, 3 + sizeof(iter)/4, JBB);
}

void
Dbtup::drop_fragment_free_pages(Signal* signal)
{
  Uint32 i;
  Uint32 tableId = signal->theData[1];
  Uint32 fragPtrI = signal->theData[2];
  DynArr256::ReleaseIterator iter;
  memcpy(&iter, signal->theData+3, sizeof(iter));

  FragrecordPtr fragPtr;
  fragPtr.i = fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
  
  DynArr256 map(c_page_map_pool, fragPtr.p->m_page_map);
  Uint32 realpid;
  for (i = 0; i<16; i++)
  {
    switch(map.release(iter, &realpid)){
    case 0:
      jam();
      goto done;
    case 1:
      if (realpid != RNIL)
      {
	jam();
	returnCommonArea(realpid, 1);
      }
    case 2:
      jam();
      break;
    }
  }
  
  signal->theData[0] = ZFREE_PAGES;
  signal->theData[1] = tableId;
  signal->theData[2] = fragPtrI;
  memcpy(signal->theData+3, &iter, sizeof(iter));
  sendSignal(reference(), GSN_CONTINUEB, signal, 3 + sizeof(iter)/4, JBB);
  return;

done:
  for (i = 0; i<MAX_FREE_LIST; i++)
  {
    LocalDLList<Page> tmp(c_page_pool, fragPtr.p->free_var_page_array[i]);
    tmp.remove();
  }
  
  {
    LocalDLList<Page> tmp(c_page_pool, fragPtr.p->emptyPrimPage);
    tmp.remove();
  }
  
  {
    LocalDLFifoList<Page> tmp(c_page_pool, fragPtr.p->thFreeFirst);
    tmp.remove();
  }
  
  {
    LocalSLList<Page> tmp(c_page_pool, fragPtr.p->m_empty_pages);
    tmp.remove();
  }

  /**
   * Finish
   */
  TablerecPtr tabPtr;
  tabPtr.i= tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);

  /**
   * Remove LCP's for fragment
   */
  tabPtr.p->m_dropTable.m_lcpno = 0;
  tabPtr.p->m_dropTable.m_fragPtrI = fragPtr.i;
  drop_fragment_fsremove(signal, tabPtr, fragPtr);
}

void
Dbtup::drop_fragment_fsremove_done(Signal* signal,
                                   TablerecPtr tabPtr,
                                   FragrecordPtr fragPtr)
{
  /**
   * LCP's removed...
   *   now continue with "next"
   */
  Uint32 logfile_group_id = fragPtr.p->m_logfile_group_id ;

  Uint32 i;
  for(i= 0; i<MAX_FRAG_PER_NODE; i++)
    if(tabPtr.p->fragrec[i] == fragPtr.i)
      break;

  ndbrequire(i != MAX_FRAG_PER_NODE);
  tabPtr.p->fragid[i]= RNIL;
  tabPtr.p->fragrec[i]= RNIL;
  releaseFragrec(fragPtr);

  signal->theData[0]= ZREL_FRAG;
  signal->theData[1]= tabPtr.i;
  signal->theData[2]= logfile_group_id;
  sendSignal(cownref, GSN_CONTINUEB, signal, 3, JBB);

}

// Remove LCP
void
Dbtup::drop_fragment_fsremove(Signal* signal, 
                              TablerecPtr tabPtr, 
                              FragrecordPtr fragPtr)
{
  FsRemoveReq* req = (FsRemoveReq*)signal->getDataPtrSend();
  req->userReference = reference();
  req->userPointer = tabPtr.i;
  req->directory = 0;
  req->ownDirectory = 0;
  
  Uint32 lcpno = tabPtr.p->m_dropTable.m_lcpno;
  Uint32 fragId = fragPtr.p->fragmentId;
  Uint32 tableId = fragPtr.p->fragTableId;

  FsOpenReq::setVersion(req->fileNumber, 5);
  FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_DATA);
  FsOpenReq::v5_setLcpNo(req->fileNumber, lcpno);
  FsOpenReq::v5_setTableId(req->fileNumber, tableId);
  FsOpenReq::v5_setFragmentId(req->fileNumber, fragId);
  sendSignal(NDBFS_REF, GSN_FSREMOVEREQ, signal, 
             FsRemoveReq::SignalLength, JBB);
}

void
Dbtup::execFSREMOVEREF(Signal* signal)
{
  jamEntry();
  FsRef* ref = (FsRef*)signal->getDataPtr();
  Uint32 userPointer = ref->userPointer;
  FsConf* conf = (FsConf*)signal->getDataPtrSend();
  conf->userPointer = userPointer;
  execFSREMOVECONF(signal);
}

void
Dbtup::execFSREMOVECONF(Signal* signal)
{
  jamEntry();
  FsConf* conf = (FsConf*)signal->getDataPtrSend();
  
  TablerecPtr tabPtr; 
  FragrecordPtr fragPtr;

  tabPtr.i = conf->userPointer;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);

  ndbrequire(tabPtr.p->tableStatus == DROPPING);
  
  fragPtr.i = tabPtr.p->m_dropTable.m_fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);

  tabPtr.p->m_dropTable.m_lcpno++;
  if (tabPtr.p->m_dropTable.m_lcpno < 3)
  {
    jam();
    drop_fragment_fsremove(signal, tabPtr, fragPtr);
  }
  else
  {
    jam();
    drop_fragment_fsremove_done(signal, tabPtr, fragPtr);
  }
}
// End remove LCP

void
Dbtup::start_restore_lcp(Uint32 tableId, Uint32 fragId)
{
  TablerecPtr tabPtr;
  tabPtr.i= tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  
  tabPtr.p->m_dropTable.tabUserPtr= tabPtr.p->m_attributes[DD].m_no_of_fixsize;
  tabPtr.p->m_dropTable.tabUserRef= tabPtr.p->m_attributes[DD].m_no_of_varsize;
  
  Uint32 *tabDesc = (Uint32*)(tableDescriptor+tabPtr.p->tabDescriptor);
  for(Uint32 i= 0; i<tabPtr.p->m_no_of_attributes; i++)
  {
    Uint32 disk= AttributeDescriptor::getDiskBased(* tabDesc);
    Uint32 null= AttributeDescriptor::getNullable(* tabDesc);

    ndbrequire(tabPtr.p->notNullAttributeMask.get(i) != null);
    if(disk)
      tabPtr.p->notNullAttributeMask.clear(i);
    tabDesc += 2;
  }
  
  tabPtr.p->m_no_of_disk_attributes = 0;
  tabPtr.p->m_attributes[DD].m_no_of_fixsize = 0;
  tabPtr.p->m_attributes[DD].m_no_of_varsize = 0;
}
void
Dbtup::complete_restore_lcp(Uint32 tableId, Uint32 fragId)
{
  TablerecPtr tabPtr;
  tabPtr.i= tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  
  tabPtr.p->m_attributes[DD].m_no_of_fixsize= tabPtr.p->m_dropTable.tabUserPtr;
  tabPtr.p->m_attributes[DD].m_no_of_varsize= tabPtr.p->m_dropTable.tabUserRef;
  
  tabPtr.p->m_no_of_disk_attributes = 
    tabPtr.p->m_attributes[DD].m_no_of_fixsize + 
    tabPtr.p->m_attributes[DD].m_no_of_varsize;
  
  Uint32 *tabDesc = (Uint32*)(tableDescriptor+tabPtr.p->tabDescriptor);
  for(Uint32 i= 0; i<tabPtr.p->m_no_of_attributes; i++)
  {
    Uint32 disk= AttributeDescriptor::getDiskBased(* tabDesc);
    Uint32 null= AttributeDescriptor::getNullable(* tabDesc);
    
    if(disk && !null)
      tabPtr.p->notNullAttributeMask.set(i);
    
    tabDesc += 2;
  }
}

bool
Dbtup::get_frag_info(Uint32 tableId, Uint32 fragId, Uint32* maxPage)
{
  jamEntry();
  TablerecPtr tabPtr;
  tabPtr.i= tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);

  FragrecordPtr fragPtr;
  getFragmentrec(fragPtr, fragId, tabPtr.p);
  
  if (maxPage)
  {
    * maxPage = fragPtr.p->noOfPages;
  }

  return true;
}
