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

#include <ndb_global.h>
#include "NdbImpl.hpp"
#include <NdbReceiver.hpp>
#include "NdbDictionaryImpl.hpp"
#include <NdbRecAttr.hpp>
#include <AttributeHeader.hpp>
#include <NdbConnection.hpp>
#include <TransporterFacade.hpp>
#include <signaldata/TcKeyConf.hpp>

NdbReceiver::NdbReceiver(Ndb *aNdb) :
  theMagicNumber(0),
  m_ndb(aNdb),
  m_id(NdbObjectIdMap::InvalidId),
  m_type(NDB_UNINITIALIZED),
  m_owner(0)
{
  theCurrentRecAttr = theFirstRecAttr = 0;
  m_defined_rows = 0;
  m_rows = new NdbRecAttr*[0];
}
 
NdbReceiver::~NdbReceiver()
{
  DBUG_ENTER("NdbReceiver::~NdbReceiver");
  if (m_id != NdbObjectIdMap::InvalidId) {
    m_ndb->theNdbObjectIdMap->unmap(m_id, this);
  }
  delete[] m_rows;
  DBUG_VOID_RETURN;
}

void
NdbReceiver::init(ReceiverType type, void* owner)
{
  theMagicNumber = 0x11223344;
  m_type = type;
  m_owner = owner;
  if (m_id == NdbObjectIdMap::InvalidId) {
    if (m_ndb)
      m_id = m_ndb->theNdbObjectIdMap->map(this);
  }

  theFirstRecAttr = NULL;
  theCurrentRecAttr = NULL;
}

void
NdbReceiver::release(){
  NdbRecAttr* tRecAttr = theFirstRecAttr;
  while (tRecAttr != NULL)
  {
    NdbRecAttr* tSaveRecAttr = tRecAttr;
    tRecAttr = tRecAttr->next();
    m_ndb->releaseRecAttr(tSaveRecAttr);
  }
  theFirstRecAttr = NULL;
  theCurrentRecAttr = NULL;
}
  
NdbRecAttr *
NdbReceiver::getValue(const NdbColumnImpl* tAttrInfo, char * user_dst_ptr){
  NdbRecAttr* tRecAttr = m_ndb->getRecAttr();
  if(tRecAttr && !tRecAttr->setup(tAttrInfo, user_dst_ptr)){
    if (theFirstRecAttr == NULL)
      theFirstRecAttr = tRecAttr;
    else
      theCurrentRecAttr->next(tRecAttr);
    theCurrentRecAttr = tRecAttr;
    tRecAttr->next(NULL);
    return tRecAttr;
  }
  if(tRecAttr){
    m_ndb->releaseRecAttr(tRecAttr);
  }    
  return 0;
}

#define KEY_ATTR_ID (~(Uint32)0)

void
NdbReceiver::calculate_batch_size(Uint32 key_size,
                                  Uint32 parallelism,
                                  Uint32& batch_size,
                                  Uint32& batch_byte_size,
                                  Uint32& first_batch_size)
{
  TransporterFacade *tp= TransporterFacade::instance();
  Uint32 max_scan_batch_size= tp->get_scan_batch_size();
  Uint32 max_batch_byte_size= tp->get_batch_byte_size();
  Uint32 max_batch_size= tp->get_batch_size();
  Uint32 tot_size= (key_size ? (key_size + 32) : 0); //key + signal overhead
  NdbRecAttr *rec_attr= theFirstRecAttr;
  while (rec_attr != NULL) {
    Uint32 attr_size= rec_attr->attrSize() * rec_attr->arraySize();
    attr_size= ((attr_size + 7) >> 2) << 2; //Even to word + overhead
    tot_size+= attr_size;
    rec_attr= rec_attr->next();
  }
  tot_size+= 32; //include signal overhead

  /**
   * Now we calculate the batch size by trying to get upto SCAN_BATCH_SIZE
   * bytes sent for each batch from each node. We do however ensure that
   * no more than MAX_SCAN_BATCH_SIZE is sent from all nodes in total per
   * batch.
   */
  batch_byte_size= max_batch_byte_size;
  if (batch_byte_size * parallelism > max_scan_batch_size) {
    batch_byte_size= max_scan_batch_size / parallelism;
  }
  batch_size= batch_byte_size / tot_size;
  if (batch_size == 0) {
    batch_size= 1;
  } else {
    if (batch_size > max_batch_size) {
      batch_size= max_batch_size;
    } else if (batch_size > MAX_PARALLEL_OP_PER_SCAN) {
      batch_size= MAX_PARALLEL_OP_PER_SCAN;
    }
  }
  first_batch_size= batch_size;
  return;
}

void
NdbReceiver::do_get_value(NdbReceiver * org, Uint32 rows, Uint32 key_size){
  if(rows > m_defined_rows){
    delete[] m_rows;
    m_defined_rows = rows;
    m_rows = new NdbRecAttr*[rows + 1]; 
  }
  m_rows[rows] = 0;
  
  NdbColumnImpl key;
  if(key_size){
    key.m_attrId = KEY_ATTR_ID;
    key.m_arraySize = key_size+1;
    key.m_attrSize = 4;
    key.m_nullable = true; // So that receive works w.r.t KEYINFO20
  }
  m_key_info = key_size;
  
  for(Uint32 i = 0; i<rows; i++){
    NdbRecAttr * prev = theCurrentRecAttr;
    assert(prev == 0 || i > 0);
    
    // Put key-recAttr fir on each row
    if(key_size && !getValue(&key, (char*)0)){
      abort();
      return ; // -1
    }
    
    NdbRecAttr* tRecAttr = org->theFirstRecAttr;
    while(tRecAttr != 0){
      if(getValue(&NdbColumnImpl::getImpl(*tRecAttr->m_column), (char*)0) != 0)
	tRecAttr = tRecAttr->next();
      else
	break;
    }
    
    if(tRecAttr){
      abort();
      return ;// -1;
    }

    // Store first recAttr for each row in m_rows[i]
    if(prev){
      m_rows[i] = prev->next();
    } else {
      m_rows[i] = theFirstRecAttr;
    }
  } 

  prepareSend();
  return;
}

void
NdbReceiver::copyout(NdbReceiver & dstRec){
  NdbRecAttr* src = m_rows[m_current_row++];
  NdbRecAttr* dst = dstRec.theFirstRecAttr;
  Uint32 tmp = m_key_info;
  if(tmp > 0){
    src = src->next();
  }
  
  while(dst){
    Uint32 len = ((src->theAttrSize * src->theArraySize)+3)/4;
    dst->receive_data((Uint32*)src->aRef(),  src->isNULL() ? 0 : len);
    src = src->next();
    dst = dst->next();
  }
}

int
NdbReceiver::execTRANSID_AI(const Uint32* aDataPtr, Uint32 aLength)
{
  bool ok = true;
  NdbRecAttr* currRecAttr = theCurrentRecAttr;
  
  for (Uint32 used = 0; used < aLength ; used++){
    AttributeHeader ah(* aDataPtr++);
    const Uint32 tAttrId = ah.getAttributeId();
    const Uint32 tAttrSize = ah.getDataSize();

    /**
     * Set all results to NULL if  not found...
     */
    while(currRecAttr && currRecAttr->attrId() != tAttrId){
      ok &= currRecAttr->setNULL();
      currRecAttr = currRecAttr->next();
    }
    
    if(ok && currRecAttr && currRecAttr->receive_data(aDataPtr, tAttrSize)){
      used += tAttrSize;
      aDataPtr += tAttrSize;
      currRecAttr = currRecAttr->next();
    } else {
      ndbout_c("%p: ok: %d tAttrId: %d currRecAttr: %p", 
	       this,ok, tAttrId, currRecAttr);
      currRecAttr = theCurrentRecAttr;
      while(currRecAttr != 0){
	ndbout_c("%d ", currRecAttr->attrId());
	currRecAttr = currRecAttr->next();
      }
      abort();
      return -1;
    }
  }

  theCurrentRecAttr = currRecAttr;
  
  /**
   * Update m_received_result_length
   */
  Uint32 exp = m_expected_result_length; 
  Uint32 tmp = m_received_result_length + aLength;
  m_received_result_length = tmp;

  return (tmp == exp || (exp > TcKeyConf::SimpleReadBit) ? 1 : 0);
}

int
NdbReceiver::execKEYINFO20(Uint32 info, const Uint32* aDataPtr, Uint32 aLength)
{
  NdbRecAttr* currRecAttr = m_rows[m_current_row++];
  assert(currRecAttr->attrId() == KEY_ATTR_ID);
  currRecAttr->receive_data(aDataPtr, aLength + 1);
  
  /**
   * Save scanInfo in the end of keyinfo
   */
  ((Uint32*)currRecAttr->aRef())[aLength] = info;
  
  Uint32 tmp = m_received_result_length + aLength;
  m_received_result_length = tmp;
  
  return (tmp == m_expected_result_length ? 1 : 0);
}

void
NdbReceiver::setErrorCode(int code)
{
  theMagicNumber = 0;
  NdbOperation* op = (NdbOperation*)getOwner();
  op->setErrorCode(code);
}
