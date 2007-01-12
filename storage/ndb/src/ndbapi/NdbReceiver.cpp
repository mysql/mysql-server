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

#include <ndb_global.h>
#include "NdbImpl.hpp"
#include <NdbReceiver.hpp>
#include "NdbDictionaryImpl.hpp"
#include <NdbRecAttr.hpp>
#include <AttributeHeader.hpp>
#include <NdbTransaction.hpp>
#include <TransporterFacade.hpp>
#include <signaldata/TcKeyConf.hpp>

NdbReceiver::NdbReceiver(Ndb *aNdb) :
  theMagicNumber(0),
  m_ndb(aNdb),
  m_id(NdbObjectIdMap::InvalidId),
  m_type(NDB_UNINITIALIZED),
  m_owner(0),
  usingNdbRecord(false)
{
  theCurrentRecAttr = theFirstRecAttr = 0;
  m_defined_rows = 0;
  m_rows = new NdbRecAttr*[0];
}
 
NdbReceiver::~NdbReceiver()
{
  DBUG_ENTER("NdbReceiver::~NdbReceiver");
  if (m_id != NdbObjectIdMap::InvalidId) {
    m_ndb->theImpl->theNdbObjectIdMap.unmap(m_id, this);
  }
  delete[] m_rows;
  DBUG_VOID_RETURN;
}

void
NdbReceiver::init(ReceiverType type, bool useRec, void* owner)
{
  theMagicNumber = 0x11223344;
  m_type = type;
  usingNdbRecord= useRec;
  m_owner = owner;
  if (m_id == NdbObjectIdMap::InvalidId) {
    if (m_ndb)
      m_id = m_ndb->theImpl->theNdbObjectIdMap.map(this);
  }

  theFirstRecAttr = NULL;
  theCurrentRecAttr = NULL;
}

void
NdbReceiver::release(){
  if (!usingNdbRecord)
  {
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
}
  
NdbRecAttr *
NdbReceiver::getValue(const NdbColumnImpl* tAttrInfo, char * user_dst_ptr){
  assert(!usingNdbRecord);

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

void
NdbReceiver::getValues(const NdbRecord* rec, char *row_ptr)
{
  assert(usingNdbRecord);

  theNdbRecord= rec;
  m_RecPos= 0;
  theRow= row_ptr;
}

#define KEY_ATTR_ID (~(Uint32)0)

/*
  Compute the batch size (rows between each NEXT_TABREQ / SCAN_TABCONF) to
  use, taking into account limits in the transporter, user preference, etc.

  Hm, there are some magic overhead numbers (4 bytes/attr, 32 bytes/row) here,
  would be nice with some explanation on how these numbers were derived.
*/
void
NdbReceiver::calculate_batch_size(Uint32 key_size,
                                  Uint32 parallelism,
                                  Uint32& batch_size,
                                  Uint32& batch_byte_size,
                                  Uint32& first_batch_size)
{
  TransporterFacade *tp= m_ndb->theImpl->m_transporter_facade;
  Uint32 max_scan_batch_size= tp->get_scan_batch_size();
  Uint32 max_batch_byte_size= tp->get_batch_byte_size();
  Uint32 max_batch_size= tp->get_batch_size();
  Uint32 tot_size= (key_size ? (key_size + 32) : 0); //key + signal overhead
  NdbRecAttr *rec_attr= theFirstRecAttr;
  while (rec_attr != NULL) {
    Uint32 attr_size= rec_attr->getColumn()->getSizeInBytes();
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
  if (batch_size == 0)
  {
    batch_byte_size= max_batch_byte_size;
  }
  else
  {
    batch_byte_size= batch_size * tot_size;
  }
  
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

/*
  Call getValue() on all required attributes in all rows in the batch to be
  received.
*/
void
NdbReceiver::do_get_value(NdbReceiver * org, 
			  Uint32 batch_size, 
			  Uint32 key_size,
			  Uint32 range_no){
  if(batch_size > m_defined_rows){
    delete[] m_rows;
    m_defined_rows = batch_size;
    m_rows = new NdbRecAttr*[batch_size + 1]; 
  }
  m_rows[batch_size] = 0;
  
  NdbColumnImpl key;
  if(key_size){
    key.m_attrId = KEY_ATTR_ID;
    /*
      We need to add one extra word to key size due to extra info word at end.
    */
    key.m_arraySize = key_size+1;
    key.m_attrSize = 4;
    key.m_nullable = true; // So that receive works w.r.t KEYINFO20
  }
  m_hidden_count = (key_size ? 1 : 0) + range_no ;
  
  for(Uint32 i = 0; i<batch_size; i++){
    NdbRecAttr * prev = theCurrentRecAttr;
    assert(prev == 0 || i > 0);
    
    // Put key-recAttr fir on each row
    if(key_size && !getValue(&key, (char*)0)){
      abort();
      return ; // -1
    }
    
    if(range_no && 
       !getValue(&NdbColumnImpl::getImpl(* NdbDictionary::Column::RANGE_NO),0))
    {
      abort();
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

NdbRecAttr*
NdbReceiver::copyout(NdbReceiver & dstRec){
  NdbRecAttr *src = m_rows[m_current_row++];
  NdbRecAttr *dst = dstRec.theFirstRecAttr;
  NdbRecAttr *start = src;
  Uint32 tmp = m_hidden_count;
  while(tmp--)
    src = src->next();
  
  while(dst){
    Uint32 len = src->get_size_in_bytes();
    dst->receive_data((Uint32*)src->aRef(), len);
    src = src->next();
    dst = dst->next();
  }

  return start;
}

static void assignToRec(const NdbRecord::Attr *col,
                        char *row,
                        const Uint32 *src,
                        Uint32 byteSize)
{
  switch (col->type)
  {
    case NdbRecord::AttrNotNULL:
      memcpy(&row[col->offset], src, byteSize);
      break;

    default:
      assert(false);
  }
}

static void setRecToNULL(const NdbRecord::Attr *col,
                         char *row)
{
  switch (col->type)
  {
    case NdbRecord::AttrNotNULL:
      assert(false);
      break;

    default:
      assert(false);
  }
}

int
NdbReceiver::execTRANSID_AI(const Uint32* aDataPtr, Uint32 aLength)
{
  if (usingNdbRecord)
  {
    Uint32 exp= m_expected_result_length;
    Uint32 tmp= m_received_result_length + aLength;
    const NdbRecord *rec= theNdbRecord;

    while (aLength > 0)
    {
      AttributeHeader ah(* aDataPtr++);
      const Uint32 attrId= ah.getAttributeId();
      aLength--;

      /*
        Set all not returned columns to NULL.
        The rows should be returned in the same order as we requested them
        (which is in any case in attribute ID order).
      */
      while (m_RecPos < rec->noOfColumns &&
             rec->columns[m_RecPos].attrId < attrId)
      {
        setRecToNULL(&rec->columns[m_RecPos], theRow);
        m_RecPos++;
      }

      /* We should never get back an attribute not originally requested. */
      assert(m_RecPos < rec->noOfColumns &&
             rec->columns[m_RecPos].attrId == attrId);

      Uint32 attrSize= ah.getByteSize();
      if (attrSize == 0)
      {
        setRecToNULL(&rec->columns[m_RecPos], theRow);
      }
      else
      {
        assert(attrSize <= rec->columns[m_RecPos].maxSize);
        Uint32 sizeInWords= (attrSize+3)>>2;
        /* Not sure how to deal with this, shouldn't happen. */
        if (unlikely(sizeInWords > aLength))
        {
          sizeInWords= aLength;
          attrSize= 4*aLength;
        }

        assignToRec(&rec->columns[m_RecPos], theRow, aDataPtr, attrSize);
        aDataPtr+= sizeInWords;
        aLength-= sizeInWords;
      }
      m_RecPos++;
    }

    m_received_result_length = tmp;

    return (tmp == exp || (exp > TcKeyConf::SimpleReadBit) ? 1 : 0);
  }

  /* The old way, using getValue() and NdbRecAttr. */
  NdbRecAttr* currRecAttr = theCurrentRecAttr;
  
  for (Uint32 used = 0; used < aLength ; used++){
    AttributeHeader ah(* aDataPtr++);
    const Uint32 tAttrId = ah.getAttributeId();
    const Uint32 tAttrSize = ah.getByteSize();

    /**
     * Set all results to NULL if  not found...
     */
    while(currRecAttr && currRecAttr->attrId() != tAttrId){
      currRecAttr = currRecAttr->next();
    }
    
    if(currRecAttr && currRecAttr->receive_data(aDataPtr, tAttrSize)){
      Uint32 add= (tAttrSize + 3) >> 2;
      used += add;
      aDataPtr += add;
      currRecAttr = currRecAttr->next();
    } else {
      /*
        This should not happen: we got back an attribute for which we have no
        stored NdbRecAttr recording that we requested said attribute (or we got
        back attributes in the wrong order).
        So dump some info for debugging, and abort.
      */
      ndbout_c("%p: tAttrId: %d currRecAttr: %p tAttrSize: %d %d", this,
	       tAttrId, currRecAttr, 
	       tAttrSize, currRecAttr->get_size_in_bytes());
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
  /*
    This is actually reading data one word off the end of the received
    signal (or off the end of the long signal data section 0, for a
    long signal), due to the aLength+1. This is to ensure the correct length
    being set for the NdbRecAttr (one extra word for the scanInfo word placed
    at the end), overwritten immediately below.
    But it's a bit ugly that we rely on being able to read one word over the
    end of the signal without crashing...
  */
  currRecAttr->receive_data(aDataPtr, 4*(aLength + 1));
  
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
