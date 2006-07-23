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
#include <signaldata/TupFrag.hpp>
#include <signaldata/FsConf.hpp>
#include <signaldata/FsRemoveReq.hpp>
#include <signaldata/DropTab.hpp>
#include <signaldata/AlterTab.hpp>
#include <signaldata/CreateFilegroupImpl.hpp>
#include <AttributeDescriptor.hpp>
#include "AttributeOffset.hpp"
#include <my_sys.h>

#define ljam() { jamLine(20000 + __LINE__); }
#define ljamEntry() { jamEntryLine(20000 + __LINE__); }

void Dbtup::execTUPFRAGREQ(Signal* signal)
{
  ljamEntry();

  TupFragReq* tupFragReq = (TupFragReq*)signal->getDataPtr();
  if (tupFragReq->userPtr == (Uint32)-1) {
    ljam();
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
  Uint32 noOfNullAttr = tupFragReq->noOfNullAttr;
  /*  Uint32 schemaVersion = tupFragReq->schemaVersion;*/
  Uint32 noOfKeyAttr = tupFragReq->noOfKeyAttr;
  Uint32 noOfCharsets = tupFragReq->noOfCharsets;

  Uint32 checksumIndicator = tupFragReq->checksumIndicator;
  Uint32 gcpIndicator = tupFragReq->globalCheckpointIdIndicator;
  Uint32 tablespace_id= tupFragReq->tablespaceid;

  Uint64 maxRows =
    (((Uint64)tupFragReq->maxRowsHigh) << 32) + tupFragReq->maxRowsLow;
  Uint64 minRows =
    (((Uint64)tupFragReq->minRowsHigh) << 32) + tupFragReq->minRowsLow;

#ifndef VM_TRACE
  // config mismatch - do not crash if release compiled
  if (regTabPtr.i >= cnoOfTablerec) {
    ljam();
    tupFragReq->userPtr = userptr;
    tupFragReq->userRef = 800;
    sendSignal(userblockref, GSN_TUPFRAGREF, signal, 2, JBB);
    return;
  }
#endif

  ptrCheckGuard(regTabPtr, cnoOfTablerec, tablerec);
  if (cfirstfreeFragopr == RNIL) {
    ljam();
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
  memset(fragOperPtr.p->m_fix_attributes_size, 0, 
	 sizeof(fragOperPtr.p->m_fix_attributes_size));
  memset(fragOperPtr.p->m_var_attributes_size, 0, 
	 sizeof(fragOperPtr.p->m_var_attributes_size));

  fragOperPtr.p->charsetIndex = 0;
  fragOperPtr.p->minRows = minRows;
  fragOperPtr.p->maxRows = maxRows;

  ndbrequire(reqinfo == ZADDFRAG);

  getFragmentrec(regFragPtr, fragId, regTabPtr.p);
  if (regFragPtr.i != RNIL) {
    ljam();
    terrorCode= ZEXIST_FRAG_ERROR;
    fragrefuse1Lab(signal, fragOperPtr);
    return;
  }
  if (cfirstfreefrag != RNIL) {
    ljam();
    seizeFragrecord(regFragPtr);
  } else {
    ljam();
    terrorCode= ZFULL_FRAGRECORD_ERROR;
    fragrefuse1Lab(signal, fragOperPtr);
    return;
  }
  initFragRange(regFragPtr.p);
  if (!addfragtotab(regTabPtr.p, fragId, regFragPtr.i)) {
    ljam();
    terrorCode= ZNO_FREE_TAB_ENTRY_ERROR;
    fragrefuse2Lab(signal, fragOperPtr, regFragPtr);
    return;
  }
  if (cfirstfreerange == RNIL) {
    ljam();
    terrorCode= ZNO_FREE_PAGE_RANGE_ERROR;
    fragrefuse3Lab(signal, fragOperPtr, regFragPtr, regTabPtr.p, fragId);
    return;
  }

  regFragPtr.p->fragTableId= regTabPtr.i;
  regFragPtr.p->fragmentId= fragId;
  regFragPtr.p->m_tablespace_id= tablespace_id;
  regFragPtr.p->m_undo_complete= false;
  regFragPtr.p->m_lcp_scan_op = RNIL; 
  regFragPtr.p->m_lcp_keep_list = RNIL;
  regFragPtr.p->m_var_page_chunks = RNIL;  

  if (ERROR_INSERTED(4007) && regTabPtr.p->fragid[0] == fragId ||
      ERROR_INSERTED(4008) && regTabPtr.p->fragid[1] == fragId) {
    ljam();
    terrorCode = 1;
    fragrefuse4Lab(signal, fragOperPtr, regFragPtr, regTabPtr.p, fragId);
    CLEAR_ERROR_INSERT_VALUE;
    return;
  }

  if (regTabPtr.p->tableStatus == NOT_DEFINED) {
    ljam();
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
//-----------------------------------------------------------------------------
    fragOperPtr.p->definingFragment= true;
    regTabPtr.p->tableStatus= DEFINING;
    regTabPtr.p->m_bits = 0;
    regTabPtr.p->m_bits |= (checksumIndicator ? Tablerec::TR_Checksum : 0);
    regTabPtr.p->m_bits |= (gcpIndicator ? Tablerec::TR_RowGCI : 0);
    
    regTabPtr.p->m_offsets[MM].m_disk_ref_offset= 0;
    regTabPtr.p->m_offsets[MM].m_null_words= 0;
    regTabPtr.p->m_offsets[MM].m_varpart_offset= 0;
    regTabPtr.p->m_offsets[MM].m_max_var_offset= 0;

    regTabPtr.p->m_offsets[DD].m_disk_ref_offset= 0;
    regTabPtr.p->m_offsets[DD].m_null_words= 0;
    regTabPtr.p->m_offsets[DD].m_varpart_offset= 0;
    regTabPtr.p->m_offsets[DD].m_max_var_offset= 0;

    regTabPtr.p->m_attributes[MM].m_no_of_fixsize= 0;
    regTabPtr.p->m_attributes[MM].m_no_of_varsize= 0;
    regTabPtr.p->m_attributes[DD].m_no_of_fixsize= 0;
    regTabPtr.p->m_attributes[DD].m_no_of_varsize= 0;

    regTabPtr.p->noOfKeyAttr= noOfKeyAttr;
    regTabPtr.p->noOfCharsets= noOfCharsets;
    regTabPtr.p->m_no_of_attributes= noOfAttributes;
    
    regTabPtr.p->notNullAttributeMask.clear();
    regTabPtr.p->blobAttributeMask.clear();
    
    Uint32 offset[10];
    Uint32 tableDescriptorRef= allocTabDescr(regTabPtr.p, offset);
    if (tableDescriptorRef == RNIL) {
      ljam();
      fragrefuse4Lab(signal, fragOperPtr, regFragPtr, regTabPtr.p, fragId);
      return;
    }
    setUpDescriptorReferences(tableDescriptorRef, regTabPtr.p, offset);
  } else {
    ljam();
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
    ljam();
    if (regTabPtr->fragid[i] == RNIL) {
      ljam();
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
    ljam();
    if (regTabPtr->fragid[i] == fragId) {
      ljam();
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

/* **************************************************************** */
/* **************          TUP_ADD_ATTRREQ       ****************** */
/* **************************************************************** */
void Dbtup::execTUP_ADD_ATTRREQ(Signal* signal)
{
  FragrecordPtr regFragPtr;
  FragoperrecPtr fragOperPtr;
  TablerecPtr regTabPtr;

  ljamEntry();
  fragOperPtr.i= signal->theData[0];
  ptrCheckGuard(fragOperPtr, cnoOfFragoprec, fragoperrec);
  Uint32 attrId = signal->theData[2];
  Uint32 attrDescriptor = signal->theData[3];
  Uint32 extType = AttributeDescriptor::getType(attrDescriptor);
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
  if (!AttributeDescriptor::getDynamic(attrDescriptor)) {
    ljam();
    Uint32 pos= 0, null_pos;
    Uint32 bytes= AttributeDescriptor::getSizeInBytes(attrDescriptor);
    Uint32 words= (bytes + 3) / 4;
    Uint32 ind= AttributeDescriptor::getDiskBased(attrDescriptor);
    ndbrequire(ind <= 1);
    null_pos= fragOperPtr.p->m_null_bits[ind];

    if (AttributeDescriptor::getNullable(attrDescriptor)) 
    {
      ljam();
      fragOperPtr.p->m_null_bits[ind]++;
    } 
    else 
    {
      regTabPtr.p->notNullAttributeMask.set(attrId);
    }

    if (extType == NDB_TYPE_BLOB || extType == NDB_TYPE_TEXT) {
      regTabPtr.p->blobAttributeMask.set(attrId);
    }

    switch (AttributeDescriptor::getArrayType(attrDescriptor)) {
    case NDB_ARRAYTYPE_FIXED:
    {
      ljam();
      regTabPtr.p->m_attributes[ind].m_no_of_fixsize++;
      if(attrLen != 0)
      {
	ljam();
	pos= fragOperPtr.p->m_fix_attributes_size[ind];
	fragOperPtr.p->m_fix_attributes_size[ind] += words;
      }
      else
      {
	ljam();
	Uint32 bitCount = AttributeDescriptor::getArraySize(attrDescriptor);
	fragOperPtr.p->m_null_bits[ind] += bitCount;
      }
      break;
    }
    default:
    {
      ljam();
      fragOperPtr.p->m_var_attributes_size[ind] += bytes;
      pos= regTabPtr.p->m_attributes[ind].m_no_of_varsize++;
      break;
    }
    }//switch
    
    AttributeOffset::setOffset(attrDes2, pos);
    AttributeOffset::setNullFlagPos(attrDes2, null_pos);
  } else {
    ndbrequire(false);
  }
  if (csNumber != 0) { 
    CHARSET_INFO* cs = all_charsets[csNumber];
    ndbrequire(cs != NULL);
    Uint32 i = 0;
    while (i < fragOperPtr.p->charsetIndex) {
      ljam();
      if (regTabPtr.p->charsetArray[i] == cs)
	break;
      i++;
    }
    if (i == fragOperPtr.p->charsetIndex) {
      ljam();
      fragOperPtr.p->charsetIndex++;
    }
    ndbrequire(i < regTabPtr.p->noOfCharsets);
    regTabPtr.p->charsetArray[i]= cs;
    AttributeOffset::setCharsetPos(attrDes2, i);
  }
  setTabDescrWord(firstTabDesIndex + 1, attrDes2);

  if (ERROR_INSERTED(4009) && regTabPtr.p->fragid[0] == fragId && attrId == 0||
      ERROR_INSERTED(4010) && regTabPtr.p->fragid[0] == fragId && lastAttr ||
      ERROR_INSERTED(4011) && regTabPtr.p->fragid[1] == fragId && attrId == 0||
      ERROR_INSERTED(4012) && regTabPtr.p->fragid[1] == fragId && lastAttr) {
    ljam();
    terrorCode = 1;
    addattrrefuseLab(signal, regFragPtr, fragOperPtr, regTabPtr.p, fragId);
    CLEAR_ERROR_INSERT_VALUE;
    return;
  }

/* **************************************************************** */
/* **************          TUP_ADD_ATTCONF       ****************** */
/* **************************************************************** */
  if (! lastAttr) {
    ljam();
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

  /**
   * Fix offsets
   */
  Uint32 pos[2] = { 0, 0 };
  if (regTabPtr.p->m_bits & Tablerec::TR_Checksum)
  {
    pos[0]= 1; 
  }

  if (regTabPtr.p->m_bits & Tablerec::TR_RowGCI)
  {
    pos[MM]++;
    pos[DD]++;
  }
  
  regTabPtr.p->m_no_of_disk_attributes= 
    regTabPtr.p->m_attributes[DD].m_no_of_fixsize +
    regTabPtr.p->m_attributes[DD].m_no_of_varsize;
  
  if(regTabPtr.p->m_no_of_disk_attributes > 0)
  {
    regTabPtr.p->m_offsets[MM].m_disk_ref_offset= pos[MM];
    pos[MM] += 2; // 8 bytes
  }
  
  regTabPtr.p->m_offsets[MM].m_null_offset= pos[MM];
  regTabPtr.p->m_offsets[DD].m_null_offset= pos[DD];
  
  pos[MM]+= regTabPtr.p->m_offsets[MM].m_null_words;
  pos[DD]+= regTabPtr.p->m_offsets[DD].m_null_words;

  Uint32 *tabDesc = (Uint32*)(tableDescriptor+regTabPtr.p->tabDescriptor);
  for(Uint32 i= 0; i<regTabPtr.p->m_no_of_attributes; i++)
  {
    Uint32 ind= AttributeDescriptor::getDiskBased(* tabDesc);
    Uint32 arr= AttributeDescriptor::getArrayType(* tabDesc++);

    if(arr == NDB_ARRAYTYPE_FIXED)
    {
      Uint32 desc= * tabDesc;
      Uint32 off= AttributeOffset::getOffset(desc) + pos[ind];
      AttributeOffset::setOffset(desc, off);
      * tabDesc= desc;
    }
    tabDesc++;
  }

  regTabPtr.p->m_offsets[MM].m_fix_header_size= 
    fragOperPtr.p->m_fix_attributes_size[MM] + 
    pos[MM];

  regTabPtr.p->m_offsets[DD].m_fix_header_size= 
    fragOperPtr.p->m_fix_attributes_size[DD] + 
    pos[DD];

  if(regTabPtr.p->m_attributes[MM].m_no_of_varsize == 0)
    regTabPtr.p->m_offsets[MM].m_fix_header_size += Tuple_header::HeaderSize;

  if(regTabPtr.p->m_attributes[DD].m_no_of_varsize == 0 &&
     regTabPtr.p->m_attributes[DD].m_no_of_fixsize > 0)
    regTabPtr.p->m_offsets[DD].m_fix_header_size += Tuple_header::HeaderSize;
  
  regTabPtr.p->m_offsets[MM].m_max_var_offset= 
    fragOperPtr.p->m_var_attributes_size[MM];
  
  regTabPtr.p->m_offsets[DD].m_max_var_offset= 
    fragOperPtr.p->m_var_attributes_size[DD];

  regTabPtr.p->total_rec_size= 
    pos[MM] + fragOperPtr.p->m_fix_attributes_size[MM] +
    pos[DD] + fragOperPtr.p->m_fix_attributes_size[DD] +
    ((fragOperPtr.p->m_var_attributes_size[MM] + 3) >> 2) +
    ((fragOperPtr.p->m_var_attributes_size[DD] + 3) >> 2) +
    (regTabPtr.p->m_attributes[MM].m_no_of_varsize ? 
     (regTabPtr.p->m_attributes[MM].m_no_of_varsize + 2) >> 1 : 0) +
    (regTabPtr.p->m_attributes[DD].m_no_of_varsize ? 
     (regTabPtr.p->m_attributes[DD].m_no_of_varsize + 2) >> 1 : 0) +
    Tuple_header::HeaderSize +
    (regTabPtr.p->m_no_of_disk_attributes ? Tuple_header::HeaderSize : 0);
  
  setUpQueryRoutines(regTabPtr.p);
  setUpKeyArray(regTabPtr.p);

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
    Uint32 fix_tupheader = regTabPtr.p->m_offsets[MM].m_fix_header_size;
    if(regTabPtr.p->m_attributes[MM].m_no_of_varsize != 0)
      fix_tupheader += Tuple_header::HeaderSize + 1;
    ndbassert(fix_tupheader > 0);
    Uint32 noRowsPerPage = ZWORDS_ON_PAGE / fix_tupheader;
    Uint32 noAllocatedPages =
      (fragOperPtr.p->minRows + noRowsPerPage - 1 )/ noRowsPerPage;
    if (fragOperPtr.p->minRows == 0)
      noAllocatedPages = 2;
    else if (noAllocatedPages == 0)
      noAllocatedPages = 2;
    noAllocatedPages = allocFragPages(regFragPtr.p, noAllocatedPages);

    if (noAllocatedPages == 0) {
      ljam();
      terrorCode = ZNO_PAGES_ALLOCATED_ERROR;
      addattrrefuseLab(signal, regFragPtr, fragOperPtr, regTabPtr.p, fragId);
      return;
    }//if
  }

  CreateFilegroupImplReq rep;
  if(regTabPtr.p->m_no_of_disk_attributes)
  {
    ljam();
    Tablespace_client tsman(0, c_tsman, 0, 0, 
			    regFragPtr.p->m_tablespace_id);
    ndbrequire(tsman.get_tablespace_info(&rep) == 0);
    regFragPtr.p->m_logfile_group_id= rep.tablespace.logfile_group_id;
  }    

  new (&regFragPtr.p->m_disk_alloc_info)
    Disk_alloc_info(regTabPtr.p, rep.tablespace.extent_size); 
  
  if (regTabPtr.p->m_no_of_disk_attributes)
  {
    ljam();
    if(!(getNodeState().startLevel == NodeState::SL_STARTING && 
	 getNodeState().starting.startPhase <= 4))
    {
      Callback cb;
      ljam();

      cb.m_callbackData= fragOperPtr.i;
      cb.m_callbackFunction = 
	safe_cast(&Dbtup::undo_createtable_callback);
      Uint32 sz= sizeof(Disk_undo::Create) >> 2;
      
      Logfile_client lgman(this, c_lgman, regFragPtr.p->m_logfile_group_id);
      int r0 = c_lgman->alloc_log_space(regFragPtr.p->m_logfile_group_id,
					sz);
      
      int res= lgman.get_log_buffer(signal, sz, &cb);
      switch(res){
      case 0:
        ljam();
	signal->theData[0] = 1;
	return;
      case -1:
	ndbrequire("NOT YET IMPLEMENTED" == 0);
	break;
      }
      execute(signal, cb, 0);
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
  default:
    ndbout_c("ret: %d", ret);
    ndbrequire(false);
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
    ljam();
    Uint32 refAttr= regTabPtr->tabDescriptor + (i * ZAD_SIZE);
    Uint32 attrDescriptor= getTabDescrWord(refAttr);
    if (AttributeDescriptor::getPrimaryKey(attrDescriptor)) {
      ljam();
      AttributeHeader::init(&keyArray[countKeyAttr], i, 0);
      countKeyAttr++;
    }
  }
  ndbrequire(countKeyAttr == regTabPtr->noOfKeyAttr);

  /**
   * Setup real order array (16 bit per column)
   */
  const Uint32 off= regTabPtr->m_real_order_descriptor;
  const Uint32 sz= (regTabPtr->m_no_of_attributes + 1) >> 1;
  ndbrequire((off + sz) < cnoOfTabDescrRec);
  
  Uint32 cnt= 0;
  Uint16* order= (Uint16*)&tableDescriptor[off].tabDescr;
  for (Uint32 type = 0; type < 4; type++)
  {
    for (Uint32 i= 0; i < regTabPtr->m_no_of_attributes; i++) 
    {
      ljam();
      Uint32 refAttr= regTabPtr->tabDescriptor + (i * ZAD_SIZE);
      Uint32 desc = getTabDescrWord(refAttr);
      Uint32 t = 0;

      if (AttributeDescriptor::getArrayType(desc) != NDB_ARRAYTYPE_FIXED) 
      {
	t += 1;
      }
      if (AttributeDescriptor::getDiskBased(desc))
      {
	t += 2;
      }
      ndbrequire(t < 4);
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
  releaseFragPages(regFragPtr.p);
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
  releaseFragPages(regFragPtr.p);
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
  deleteFragTab(regTabPtr, fragId);
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

void Dbtup::deleteFragTab(Tablerec* const regTabPtr, Uint32 fragId) 
{
  for (Uint32 i = 0; i < MAX_FRAG_PER_NODE; i++) {
    ljam();
    if (regTabPtr->fragid[i] == fragId) {
      ljam();
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
  ljamEntry();
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
  sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
}

void Dbtup::releaseTabDescr(Tablerec* const regTabPtr) 
{
  Uint32 descriptor= regTabPtr->readKeyArray;
  if (descriptor != RNIL) {
    ljam();
    Uint32 offset[10];
    getTabDescrOffsets(regTabPtr, offset);

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
}

void Dbtup::releaseFragment(Signal* signal, Uint32 tableId)
{
  TablerecPtr tabPtr;
  tabPtr.i= tableId;
  ptrCheckGuard(tabPtr, cnoOfTablerec, tablerec);
  Uint32 fragIndex = RNIL;
  Uint32 fragId = RNIL;
  Uint32 i = 0;
  for (i = 0; i < MAX_FRAG_PER_NODE; i++) {
    ljam();
    if (tabPtr.p->fragid[i] != RNIL) {
      ljam();
      fragIndex= tabPtr.p->fragrec[i];
      fragId= tabPtr.p->fragid[i];
      break;
    }
  }
  if (fragIndex != RNIL) {
    ljam();
    
    signal->theData[0] = ZUNMAP_PAGES;
    signal->theData[1] = tabPtr.i;
    signal->theData[2] = fragIndex;
    signal->theData[3] = 0;
    sendSignal(cownref, GSN_CONTINUEB, signal, 4, JBB);  
    return;
  }
  
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
Dbtup::drop_fragment_unmap_pages(Signal *signal, 
				 TablerecPtr tabPtr, 
				 FragrecordPtr fragPtr,
				 Uint32 pos)
{
  if (tabPtr.p->m_no_of_disk_attributes)
  {
    Disk_alloc_info& alloc_info= fragPtr.p->m_disk_alloc_info;
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
      
      drop_fragment_free_exent(signal, tabPtr, fragPtr, 0);
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
  drop_fragment_free_exent(signal, tabPtr, fragPtr, 0);  
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
Dbtup::drop_fragment_free_exent(Signal *signal, 
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
	Local_extent_info_list
	  list(c_extent_pool, alloc_info.m_free_extents[pos]);
	Ptr<Extent_info> ext_ptr;
	list.first(ext_ptr);
	
	Tablespace_client tsman(signal, c_tsman, tabPtr.i, 
				fragPtr.p->fragmentId,
				fragPtr.p->m_tablespace_id);
	
	tsman.free_extent(&ext_ptr.p->m_key);
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
Dbtup::drop_fragment_free_var_pages(Signal* signal)
{
  ljam();
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
  
  releaseFragPages(fragPtr.p);
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
  sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);  
  return;
}

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
