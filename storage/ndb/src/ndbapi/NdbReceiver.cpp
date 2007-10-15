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
#include <NdbBlob.hpp>
#include <signaldata/TcKeyConf.hpp>
#include <signaldata/DictTabInfo.hpp>

NdbReceiver::NdbReceiver(Ndb *aNdb) :
  theMagicNumber(0),
  m_ndb(aNdb),
  m_id(NdbObjectIdMap::InvalidId),
  m_type(NDB_UNINITIALIZED),
  m_owner(0),
  m_using_ndb_record(false)
{
  theCurrentRecAttr = theFirstRecAttr = 0;
  m_defined_rows = 0;
  m_rows = NULL;
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

int
NdbReceiver::init(ReceiverType type, bool useRec, void* owner)
{
  theMagicNumber = 0x11223344;
  m_type = type;
  m_using_ndb_record= useRec;
  m_owner = owner;

  if (useRec)
  {
    m_record.m_ndb_record= NULL;
    m_record.m_row= NULL;
    m_record.m_row_buffer= NULL;
    m_record.m_row_offset= 0;
    m_record.m_read_range_no= false;
    m_record.m_column_count= 0;
  }
  theFirstRecAttr = NULL;
  theCurrentRecAttr = NULL;

  if (m_id == NdbObjectIdMap::InvalidId) {
    if (m_ndb)
    {
      m_id = m_ndb->theImpl->theNdbObjectIdMap.map(this);
      if (m_id == NdbObjectIdMap::InvalidId)
      {
        setErrorCode(4000);
        return -1;
      }
    }
  }

  return 0;
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
  m_using_ndb_record= false;
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

void
NdbReceiver::getValues(const NdbRecord* rec, char *row_ptr)
{
  assert(m_using_ndb_record);

  m_record.m_ndb_record= rec;
  m_record.m_row= row_ptr;
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
                                  Uint32& first_batch_size,
                                  const NdbRecord *record)
{
  TransporterFacade *tp= m_ndb->theImpl->m_transporter_facade;
  Uint32 max_scan_batch_size= tp->get_scan_batch_size();
  Uint32 max_batch_byte_size= tp->get_batch_byte_size();
  Uint32 max_batch_size= tp->get_batch_size();
  Uint32 tot_size= (key_size ? (key_size + 32) : 0); //key + signal overhead

  if (record)
  {
    tot_size+= record->m_max_transid_ai_bytes;
  }

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
int
NdbReceiver::do_get_value(NdbReceiver * org, 
			  Uint32 batch_size, 
			  Uint32 key_size,
			  Uint32 range_no){
  assert(!m_using_ndb_record);
  if(batch_size > m_defined_rows){
    delete[] m_rows;
    m_defined_rows = batch_size;
    if ((m_rows = new NdbRecAttr*[batch_size + 1]) == NULL)
    {
      setErrorCode(4000);
      return -1;
    }
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
  m_recattr.m_hidden_count = (key_size ? 1 : 0) + range_no ;
  
  for(Uint32 i = 0; i<batch_size; i++){
    NdbRecAttr * prev = theCurrentRecAttr;
    assert(prev == 0 || i > 0);
    
    // Put key-recAttr fir on each row
    if(key_size && !getValue(&key, (char*)0)){
      abort();
      return -1;
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
      return -1;
    }

    // Store first recAttr for each row in m_rows[i]
    if(prev){
      m_rows[i] = prev->next();
    } else {
      m_rows[i] = theFirstRecAttr;
    }
  } 

  prepareSend();
  return 0;
}

void
NdbReceiver::do_setup_ndbrecord(const NdbRecord *ndb_record, Uint32 batch_size,
                                Uint32 key_size, Uint32 read_range_no,
                                Uint32 rowsize, char *row_buffer,
                                Uint32 column_count)
{
  m_using_ndb_record= true;
  m_record.m_ndb_record= ndb_record;
  m_record.m_row= row_buffer;
  m_record.m_row_buffer= row_buffer;
  m_record.m_row_offset= rowsize;
  m_record.m_read_range_no= read_range_no;
  m_record.m_column_count= column_count;
}

Uint32
NdbReceiver::ndbrecord_rowsize(const NdbRecord *ndb_record, Uint32 key_size,
                               Uint32 read_range_no, Uint32 extra_size)
{
  Uint32 rowsize= ndb_record->m_row_size;
  /* Room for range_no. */
  if (read_range_no)
    rowsize+= 4;
  /*
    If keyinfo, need room for max. key + 4 bytes of actual key length + 4
    bytes of scan info (all from KEYINFO20 signal).
  */
  if (key_size)
    rowsize+= 8 + key_size*4;
  /* Space for reading blob heads. */
  rowsize+= extra_size;
  /* Ensure 4-byte alignment. */
  rowsize= (rowsize+3) & 0xfffffffc;
  return rowsize;
}

NdbRecAttr*
NdbReceiver::copyout(NdbReceiver & dstRec){
  assert(!m_using_ndb_record);
  NdbRecAttr *src = m_rows[m_current_row++];
  NdbRecAttr *dst = dstRec.theFirstRecAttr;
  NdbRecAttr *start = src;
  Uint32 tmp = m_recattr.m_hidden_count;
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

static
inline
const Uint8*
pad(const Uint8* src, Uint32 align, Uint32 bitPos)
{
  UintPtr ptr = UintPtr(src);
  switch(align){
  case DictTabInfo::aBit:
  case DictTabInfo::a32Bit:
  case DictTabInfo::a64Bit:
  case DictTabInfo::a128Bit:
    return (Uint8*)(((ptr + 3) & ~(UintPtr)3) + 4 * ((bitPos + 31) >> 5));
charpad:
  case DictTabInfo::an8Bit:
  case DictTabInfo::a16Bit:
    return src + 4 * ((bitPos + 31) >> 5);
  default:
#ifdef VM_TRACE
    abort();
#endif
    goto charpad;
  }
}

static
void
handle_packed_bit(const Uint8* _src, Uint32 pos, Uint32 len, NdbRecAttr* curr)
{
  Uint32 * src = (Uint32*)_src;
  Uint32 * dst = (Uint32*)curr->aRef();
  assert((UintPtr(dst) & 3) == 0);
  assert((UintPtr(src) & 3) == 0);
  BitmaskImpl::getField(1 + ((len + 31) >> 5), src, pos, len, dst);
}

Uint32
NdbReceiver::receive_packed(NdbRecAttr** recAttr, 
			    Uint32 bmlen, 
			    const Uint32* aDataPtr, 
			    Uint32 aLength)
{
  NdbRecAttr* currRecAttr = *recAttr;
  const Uint8 *src = (Uint8*)(aDataPtr + bmlen);
  Uint32 bitPos = 0;
  for (Uint32 i = 0, attrId = 0; i<32*bmlen; i++, attrId++)
  {
    if (BitmaskImpl::get(bmlen, aDataPtr, i))
    {
      const NdbColumnImpl & col = 
	NdbColumnImpl::getImpl(* currRecAttr->getColumn());
      if (unlikely(attrId != (Uint32)col.m_attrId))
        goto err;
      if (col.m_nullable)
      {
	if (BitmaskImpl::get(bmlen, aDataPtr, ++i))
	{
	  currRecAttr->setNULL();
	  currRecAttr = currRecAttr->next();
	  continue;
	}
      }
      Uint32 align = col.m_orgAttrSize;
      Uint32 attrSize = col.m_attrSize;
      Uint32 array = col.m_arraySize;
      Uint32 len = col.m_length;
      Uint32 sz = attrSize * array;
      Uint32 arrayType = col.m_arrayType;
      
      switch(align){
      case DictTabInfo::aBit: // Bit
        src = pad(src, 0, 0);
	handle_packed_bit(src, bitPos, len, currRecAttr);
	src += 4 * ((bitPos + len) >> 5);
	bitPos = (bitPos + len) & 31;
        goto next;
      default:
        src = pad(src, align, bitPos);
      }
      switch(arrayType){
      case NDB_ARRAYTYPE_FIXED:
        break;
      case NDB_ARRAYTYPE_SHORT_VAR:
        sz = 1 + src[0];
        break;
      case NDB_ARRAYTYPE_MEDIUM_VAR:
	sz = 2 + src[0] + 256 * src[1];
        break;
      default:
        goto err;
      }
      
      bitPos = 0;
      currRecAttr->receive_data((Uint32*)src, sz);
      src += sz;
  next:
      currRecAttr = currRecAttr->next();
    }
  }
  * recAttr = currRecAttr;
  return ((Uint32*)pad(src, 0, bitPos)) - aDataPtr;

err:
  abort();
}

int
NdbReceiver::get_range_no() const
{
  int range_no;
  assert(m_using_ndb_record);
  Uint32 idx= m_current_row;
  if (idx == 0 || !m_record.m_read_range_no)
    return -1;
  memcpy(&range_no,
         m_record.m_row_buffer +
           (idx-1)*m_record.m_row_offset +
           m_record.m_ndb_record->m_row_size,
         4);
  return range_no;
}

int
NdbReceiver::get_keyinfo20(Uint32 & scaninfo, Uint32 & length,
                           const char * & data_ptr) const
{
  assert(m_using_ndb_record);
  Uint32 idx= m_current_row;
  if (idx == 0)
    return -1;                                  // No rows fetched yet
  const char *p= m_record.m_row_buffer +
    (idx-1)*m_record.m_row_offset +
    m_record.m_ndb_record->m_row_size;
  if (m_record.m_read_range_no)
    p+= 4;
  scaninfo= uint4korr(p);
  p+= 4;
  length= uint4korr(p);
  p+= 4;
  data_ptr= p;
  return 0;
}

/* Set NdbRecord field to non-NULL value. */
static void assignToRec(const NdbRecord::Attr *col,
                        char *row,
                        const Uint32 *src,
                        Uint32 byteSize)
{
  /* Set NULLable attribute to "not NULL". */
  if (col->flags & NdbRecord::IsNullable)
    row[col->nullbit_byte_offset]&= ~(1 << col->nullbit_bit_in_byte);

  memcpy(&row[col->offset], src, byteSize);
}

/* Set NdbRecord field to NULL. */
static void setRecToNULL(const NdbRecord::Attr *col,
                         char *row)
{
  assert(col->flags & NdbRecord::IsNullable);
  row[col->nullbit_byte_offset]|= 1 << col->nullbit_bit_in_byte;
}

int
NdbReceiver::getScanAttrData(const char * & data, Uint32 & size, Uint32 & pos) const
{
  assert(m_using_ndb_record);
  Uint32 idx= m_current_row;
  if (idx == 0)
    return -1;                                  // No rows fetched yet
  const char *row_end= m_record.m_row_buffer + idx*m_record.m_row_offset;

  pos+= sizeof(Uint32);
  memcpy(&size, row_end - pos, sizeof(Uint32));
  pos+= size;
  data= row_end - pos;

  return 0;
}

int
NdbReceiver::execTRANSID_AI(const Uint32* aDataPtr, Uint32 aLength)
{
  if (m_using_ndb_record)
  {
    Uint32 exp= m_expected_result_length;
    Uint32 tmp= m_received_result_length + aLength;
    const NdbRecord *rec= m_record.m_ndb_record;
    Uint32 rec_pos= 0;
    Uint32 save_pos= 0;
    Uint32 column_count= 0;

    while (aLength > 0)
    {
      AttributeHeader ah(* aDataPtr++);
      const Uint32 attrId= ah.getAttributeId();
      Uint32 attrSize= ah.getByteSize();
      aLength--;

      /* Special case for RANGE_NO, which is stored just after the row. */
      if (attrId==AttributeHeader::RANGE_NO)
      {
        assert(m_record.m_read_range_no);
        assert(attrSize==4);
        memcpy(m_record.m_row+m_record.m_ndb_record->m_row_size, aDataPtr++, 4);
        aLength--;
        continue;
      }

      /*
        Skip all not returned columns.
        The rows should be returned in the same order as we requested them
        (which is in any case in attribute ID order).
      */
      while (rec_pos < rec->noOfColumns &&
             rec->columns[rec_pos].attrId < attrId)
        rec_pos++;

      /*
        To support extra getValue(), there may be extra attribute data after
        all NdbRecord columns have been fetched. But the fast path is for pure
        NdbRecord operation, with no extra getValue().
      */
      if (unlikely(column_count >= m_record.m_column_count))
      {
        if (m_type == NDB_SCANRECEIVER)
        {
          /* For scans, save the data and copy to NdbRecAttr in nextResult(). */
          save_pos+= sizeof(Uint32);
          memcpy(m_record.m_row + m_record.m_row_offset - save_pos,
                 &attrSize, sizeof(Uint32));
          if (attrSize > 0)
          {
            save_pos+= attrSize;
            memcpy(m_record.m_row + m_record.m_row_offset - save_pos,
                   aDataPtr, attrSize);
          }
        }
        else
        {
          /* Handle extra attributes requested with getValue(). */
          assert(theCurrentRecAttr != NULL);
          assert(theCurrentRecAttr->attrId() == attrId);
          bool res= theCurrentRecAttr->receive_data(aDataPtr, attrSize);
          assert(res);
          theCurrentRecAttr= theCurrentRecAttr->next();
        }
        Uint32 sizeInWords= (attrSize+3)>>2;
        aDataPtr+= sizeInWords;
        aLength-= sizeInWords;
        continue;
      }
      column_count++;

      const NdbRecord::Attr *col= &rec->columns[rec_pos];

      /* We should never get back an attribute not originally requested. */
      assert(rec_pos < rec->noOfColumns && col->attrId == attrId);

      /* Blobs heads are read with getValue(), not using NdbRecord. */
      assert((col->flags & NdbRecord::IsBlob) == 0);
      /*
        The fast path is for a plain offset/length column (not
        mysqld-format bit field).
      */
      if (likely(!(col->flags & NdbRecord::IsMysqldBitfield)))
      {
        if (attrSize == 0)
        {
          setRecToNULL(col, m_record.m_row);
        }
        else
        {
          assert(attrSize <= col->maxSize);
          Uint32 sizeInWords= (attrSize+3)>>2;
          /* Not sure how to deal with this, shouldn't happen. */
          if (unlikely(sizeInWords > aLength))
          {
            sizeInWords= aLength;
            attrSize= 4*aLength;
          }

          assignToRec(col, m_record.m_row, aDataPtr, attrSize);
          aDataPtr+= sizeInWords;
          aLength-= sizeInWords;
        }
      }
      else
      {
        /* Mysqld format bitfield. */
        if (attrSize == 0)
        {
          setRecToNULL(col, m_record.m_row);
        }
        else
        {
          assert(attrSize == col->maxSize);
          Uint32 sizeInWords= (attrSize+3)>>2;
          if (col->flags & NdbRecord::IsNullable)
            m_record.m_row[col->nullbit_byte_offset]&=
              ~(1 << col->nullbit_bit_in_byte);
          col->put_mysqld_bitfield(m_record.m_row, (const char *)aDataPtr);
          aDataPtr+= sizeInWords;
          aLength-= sizeInWords;
        }
      }
      rec_pos++;
    }

    m_received_result_length = tmp;
    m_record.m_row+= m_record.m_row_offset;

    return (tmp == exp || (exp > TcKeyConf::DirtyReadBit) ? 1 : 0);
  }

  /* The old way, using getValue() and NdbRecAttr. */
  NdbRecAttr* currRecAttr = theCurrentRecAttr;
  
  for (Uint32 used = 0; used < aLength ; used++){
    AttributeHeader ah(* aDataPtr++);
    const Uint32 tAttrId = ah.getAttributeId();
    const Uint32 tAttrSize = ah.getByteSize();
    
    if (tAttrId == AttributeHeader::READ_PACKED)
    {
      NdbRecAttr* tmp = currRecAttr;
      Uint32 len = receive_packed(&tmp, tAttrSize>>2, aDataPtr, aLength); 
      aDataPtr += len;
      used += len;
      currRecAttr = tmp;
      continue;
    }
    
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
      ndbout_c("this=%p: tAttrId: %d currRecAttr: %p theCurrentRecAttr: %p "
               "tAttrSize: %d %d", this,
	       tAttrId, currRecAttr, theCurrentRecAttr, tAttrSize,
               currRecAttr ? currRecAttr->get_size_in_bytes() : 0);
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

  return (tmp == exp || (exp > TcKeyConf::DirtyReadBit) ? 1 : 0);
}

int
NdbReceiver::execKEYINFO20(Uint32 info, const Uint32* aDataPtr, Uint32 aLength)
{
  if (m_using_ndb_record)
  {
    /* Copy in the keyinfo after the user row and any range_no value. */

    char *keyinfo_ptr= m_record.m_row_buffer + 
                       m_current_row++ * m_record.m_row_offset +
                       m_record.m_ndb_record->m_row_size;
    if (m_record.m_read_range_no)
      keyinfo_ptr+= 4;

    int4store(keyinfo_ptr, info);
    keyinfo_ptr+= 4;
    int4store(keyinfo_ptr, aLength);
    keyinfo_ptr+= 4;
    memcpy(keyinfo_ptr, aDataPtr, 4*aLength);

    Uint32 tmp= m_received_result_length + aLength;
    m_received_result_length = tmp;
  
    return (tmp == m_expected_result_length ? 1 : 0);
  }

  /* The old method, using NdbRecAttr. */
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
