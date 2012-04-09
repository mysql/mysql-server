/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

#include "API.hpp"
#include <AttributeHeader.hpp>
#include <signaldata/TcKeyConf.hpp>
#include <signaldata/DictTabInfo.hpp>

NdbReceiver::NdbReceiver(Ndb *aNdb) :
  theMagicNumber(0),
  m_ndb(aNdb),
  m_id(NdbObjectIdMap::InvalidId),
  m_tcPtrI(RNIL),
  m_type(NDB_UNINITIALIZED),
  m_owner(0),
  m_using_ndb_record(false),
  theFirstRecAttr(NULL),
  theCurrentRecAttr(NULL),
  m_rows(NULL),
  m_current_row(0xffffffff),
  m_result_rows(0)
{}
 
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
    m_record.m_row_recv= NULL;
    m_record.m_row_buffer= NULL;
    m_record.m_row_offset= 0;
    m_record.m_read_range_no= false;
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
  theMagicNumber = 0;
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
  m_record.m_row_recv= row_ptr;
  m_record.m_row_offset= rec->m_row_size;
}

void
NdbReceiver::prepareReceive(char *buf)
{
  /* Set pointers etc. to prepare for receiving the first row of the batch. */
  assert(theMagicNumber == 0x11223344);
  m_received_result_length = 0;
  m_expected_result_length = 0;
  if (m_using_ndb_record)
  {
    m_record.m_row_recv= buf;
  }
  theCurrentRecAttr = theFirstRecAttr;
}

void
NdbReceiver::prepareRead(char *buf, Uint32 rows)
{
  /* Set pointers etc. to prepare for reading the first row of the batch. */
  assert(theMagicNumber == 0x11223344);
  m_current_row = 0;
  m_result_rows = rows;
  if (m_using_ndb_record)
  {
    m_record.m_row_buffer = buf;
  }
}

 #define KEY_ATTR_ID (~(Uint32)0)

/*
  Compute the batch size (rows between each NEXT_TABREQ / SCAN_TABCONF) to
  use, taking into account limits in the transporter, user preference, etc.

  Hm, there are some magic overhead numbers (4 bytes/attr, 32 bytes/row) here,
  would be nice with some explanation on how these numbers were derived.

  TODO : Check whether these numbers need to be revised w.r.t. read packed
*/
//static
void
NdbReceiver::calculate_batch_size(const NdbImpl& theImpl,
                                  const NdbRecord *record,
                                  const NdbRecAttr *first_rec_attr,
                                  Uint32 key_size,
                                  Uint32 parallelism,
                                  Uint32& batch_size,
                                  Uint32& batch_byte_size,
                                  Uint32& first_batch_size)
{
  const NdbApiConfig & cfg = theImpl.get_ndbapi_config_parameters();
  const Uint32 max_scan_batch_size= cfg.m_scan_batch_size;
  const Uint32 max_batch_byte_size= cfg.m_batch_byte_size;
  const Uint32 max_batch_size= cfg.m_batch_size;

  Uint32 tot_size= (key_size ? (key_size + 32) : 0); //key + signal overhead
  if (record)
  {
    tot_size+= record->m_max_transid_ai_bytes;
  }

  const NdbRecAttr *rec_attr= first_rec_attr;
  while (rec_attr != NULL) {
    Uint32 attr_size= rec_attr->getColumn()->getSizeInBytes();
    attr_size= ((attr_size + 4 + 3) >> 2) << 2; //Even to word + overhead
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

void
NdbReceiver::calculate_batch_size(Uint32 key_size,
                                  Uint32 parallelism,
                                  Uint32& batch_size,
                                  Uint32& batch_byte_size,
                                  Uint32& first_batch_size,
                                  const NdbRecord *record) const
{
  calculate_batch_size(* m_ndb->theImpl,
                       record,
                       theFirstRecAttr,
                       key_size, parallelism, batch_size, batch_byte_size,
                       first_batch_size);
}

void
NdbReceiver::do_setup_ndbrecord(const NdbRecord *ndb_record, Uint32 batch_size,
                                Uint32 key_size, Uint32 read_range_no,
                                Uint32 rowsize, char *row_buffer)
{
  m_using_ndb_record= true;
  m_record.m_ndb_record= ndb_record;
  m_record.m_row_recv= row_buffer;
  m_record.m_row_buffer= row_buffer;
  m_record.m_row_offset= rowsize;
  m_record.m_read_range_no= read_range_no;
}

//static
Uint32
NdbReceiver::ndbrecord_rowsize(const NdbRecord *ndb_record,
                               const NdbRecAttr *first_rec_attr,
                               Uint32 key_size,
                               bool read_range_no)
{
  Uint32 rowsize= (ndb_record) ? ndb_record->m_row_size : 0;

  /* Room for range_no. */
  if (read_range_no)
    rowsize+= 4;
  /*
    If keyinfo, need room for max. key + 4 bytes of actual key length + 4
    bytes of scan info (all from KEYINFO20 signal).
  */
  if (key_size)
    rowsize+= 8 + key_size*4;
  /*
    Compute extra space needed to buffer getValue() results in NdbRecord
    scans.
  */
  const NdbRecAttr *ra= first_rec_attr;
  while (ra != NULL)
  {
    rowsize+= sizeof(Uint32) + ra->getColumn()->getSizeInBytes();
    ra= ra->next();
  }
  /* Ensure 4-byte alignment. */
  rowsize= (rowsize+3) & 0xfffffffc;
  return rowsize;
}

/**
 * pad
 * This function determines how much 'padding' should be applied
 * to the passed in pointer and bitPos to get to the start of a 
 * field with the passed in alignment.
 * The rules are : 
 *   - First bit field is 32-bit aligned
 *   - Subsequent bit fields are packed in the next available bits
 *   - 8 and 16 bit aligned fields are packed in the next available
 *     word (but not necessarily word aligned.
 *   - 32, 64 and 128 bit aligned fields are packed in the next
 *     aligned 32-bit word.
 * This algorithm is used to unpack a stream of fields packed by the code
 * in src/kernel/blocks/dbtup/DbtupRoutines::read_packed()
 */
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

/**
 * handle_packed_bit
 * This function copies the bitfield of length len, offset pos from
 * word-aligned ptr _src to memory starting at the byte ptr dst.
 */
static
void
handle_packed_bit(const char* _src, Uint32 pos, Uint32 len, char* _dst)
{
  Uint32 * src = (Uint32*)_src;
  assert((UintPtr(src) & 3) == 0);

  /* Convert char* to aligned Uint32* and some byte offset */
  UintPtr uiPtr= UintPtr((Uint32*)_dst);
  Uint32 dstByteOffset= Uint32(uiPtr) & 3;
  Uint32* dst= (Uint32*) (uiPtr - dstByteOffset); 

  BitmaskImpl::copyField(dst, dstByteOffset << 3,
                         src, pos, len);
}


/**
 * receive_packed_recattr
 * Receive a packed stream of field values, whose presence and nullness
 * is indicated by a leading bitmap into a list of NdbRecAttr objects
 * Return the number of words read from the input stream.
 */
Uint32
NdbReceiver::receive_packed_recattr(NdbRecAttr** recAttr, 
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
	handle_packed_bit((const char*)src, bitPos, len, 
                          currRecAttr->aRef());
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
  return (Uint32)(((Uint32*)pad(src, 0, bitPos)) - aDataPtr);

err:
  abort();
  return 0;
}


/* Set NdbRecord field to non-NULL value. */
static void assignToRec(const NdbRecord::Attr *col,
                        char *row,
                        const Uint8 *src,
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

/**
 * handle_bitfield_ndbrecord
 * Packed bitfield handling for NdbRecord - also deals with 
 * mapping the bitfields into MySQLD format if necessary.
 */
static void
handle_bitfield_ndbrecord(const NdbRecord::Attr* col,
                          const Uint8*& src,
                          Uint32& bitPos,
                          Uint32& len,
                          char* row)
{
  if (col->flags & NdbRecord::IsNullable)
  {
    /* Clear nullbit in row */
    row[col->nullbit_byte_offset] &=
      ~(1 << col->nullbit_bit_in_byte);
  }

  char* dest;
  Uint64 mysqldSpace;

  /* For MySqldBitField, we read it as normal into a local on the 
   * stack and then use the put_mysqld_bitfield function to rearrange
   * and write it to the row
   */
  bool isMDBitfield= (col->flags & NdbRecord::IsMysqldBitfield) != 0;

  if (isMDBitfield)
  {
    assert(len <= 64);
    dest= (char*) &mysqldSpace;
  }
  else
    dest= row + col->offset;
  
  /* Copy bitfield to memory starting at dest */
  src = pad(src, 0, 0);
  handle_packed_bit((const char*)src, bitPos, len, dest); 
  src += 4 * ((bitPos + len) >> 5);
  bitPos = (bitPos + len) & 31;
  
  if (isMDBitfield)
    /* Rearrange bitfield from stack to row storage */
    col->put_mysqld_bitfield(row, dest);
}


/**
 * receive_packed_ndbrecord
 * Receive a packed stream of field values, whose presence and nullness
 * is indicated by a leading bitmap, into an NdbRecord row.
 * Return the number of words consumed from the input stream.
 */
Uint32
NdbReceiver::receive_packed_ndbrecord(Uint32 bmlen, 
                                      const Uint32* aDataPtr,
                                      char* row)
{
  const Uint8 *src = (Uint8*)(aDataPtr + bmlen);
  Uint32 bitPos = 0;
  const NdbRecord* rec= m_record.m_ndb_record;
  const Uint32 maxAttrId= rec->columns[rec->noOfColumns -1].attrId;
  const Uint32 bmSize= bmlen << 5;

  /* Use bitmap to determine which columns have been sent */
  for (Uint32 i = 0, attrId = 0; 
       (i < bmSize) && (attrId <= maxAttrId);
       i++, attrId++)
  {
    if (BitmaskImpl::get(bmlen, aDataPtr, i))
    {
      /* Found bit in column presence bitmask, get corresponding
       * Attr struct from NdbRecord
       */
      assert(attrId < rec->m_attrId_indexes_length);
      assert((Uint32) rec->m_attrId_indexes[attrId] 
             < rec->noOfColumns);
      const NdbRecord::Attr* col= &rec->columns[rec->m_attrId_indexes[attrId]];

      assert((col->flags & NdbRecord::IsBlob) == 0);

      /* If col is nullable, check for null and 
       * set bit
       */ 
      if (col->flags & NdbRecord::IsNullable)
      {
	if (BitmaskImpl::get(bmlen, aDataPtr, ++i))
	{
          setRecToNULL(col, m_record.m_row_recv);

          // Next column...
	  continue;
	}
      }
      
      Uint32 align = col->orgAttrSize;
      Uint32 sz = col->maxSize;
      Uint32 len = col->bitCount;
      Uint32 arrayType = 
        (col->flags & NdbRecord::IsVar1ByteLen)? 
        NDB_ARRAYTYPE_SHORT_VAR :
        (
         (col->flags & NdbRecord::IsVar2ByteLen)?
         NDB_ARRAYTYPE_MEDIUM_VAR : 
         NDB_ARRAYTYPE_FIXED);

      switch(align){
      case DictTabInfo::aBit: // Bit
        handle_bitfield_ndbrecord(col,
                                  src,
                                  bitPos,
                                  len,
                                  row);
        continue; // Next column
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
        abort();
      }
      
      bitPos = 0;
      assignToRec(col,
                  row,
                  src,
                  sz);

      src += sz;
    }
  }

  return (Uint32)(((Uint32*)pad(src, 0, bitPos)) - aDataPtr);
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

  assert (pos <= m_record.m_row_offset);
  return 0;
}

int
NdbReceiver::execTRANSID_AI(const Uint32* aDataPtr, Uint32 aLength)
{
  /*
   * NdbRecord and NdbRecAttr row result handling are merged here
   *   First any NdbRecord attributes are extracted
   *   Then any NdbRecAttr attributes are extracted
   *   NdbRecord scans with extra NdbRecAttr getValue() attrs
   *   are handled separately in the NdbRecord code
   * Scenarios : 
   *   NdbRecord only PK read result
   *   NdbRecAttr only PK read result
   *   Mixed PK read results
   *   NdbRecord only scan read result
   *   NdbRecAttr only scan read result
   *   Mixed scan read results
   */
  Uint32 exp= m_expected_result_length;
  Uint32 tmp= m_received_result_length + aLength;
  Uint32 origLength=aLength;
  NdbRecAttr* currRecAttr = theCurrentRecAttr;
  Uint32 save_pos= 0;

  bool ndbrecord_part_done= !m_using_ndb_record;
  const bool isScan= (m_type == NDB_SCANRECEIVER) ||
    (m_type == NDB_QUERY_OPERATION);

  /* Read words from the incoming signal train.
   * The length passed in is enough for one row, either as an individual
   * read op, or part of a scan.  When there are no more words, we're at
   * the end of the row
   */
  while (aLength > 0)
  {
    AttributeHeader ah(* aDataPtr++);
    const Uint32 attrId= ah.getAttributeId();
    Uint32 attrSize= ah.getByteSize();
    aLength--;

    if (!ndbrecord_part_done)
    {
      /* Special case for RANGE_NO, which is received first and is
       * stored just after the row. */
      if (attrId == AttributeHeader::RANGE_NO)
      {
        assert(m_record.m_read_range_no);
        assert(attrSize==4);
        assert (m_record.m_row_offset >= m_record.m_ndb_record->m_row_size+attrSize);
        memcpy(m_record.m_row_recv+m_record.m_ndb_record->m_row_size, 
               aDataPtr++, 4);
        aLength--;
        continue; // Next
      }

      /* Normal case for all NdbRecord primary key, index key, table scan
       * and index scan reads.  Extract all requested columns from packed
       * format into the row.
       */
      if (attrId == AttributeHeader::READ_PACKED)
      {
        assert (m_record.m_row_offset >= m_record.m_ndb_record->m_row_size);
        Uint32 len= receive_packed_ndbrecord(attrSize >> 2, // Bitmap length
                                             aDataPtr,
                                             m_record.m_row_recv);
        aDataPtr+= len;
        aLength-= len;
        continue;  // Next
      }

      /* If we get here then we must have 'extra getValues' - columns
       * requested outwith the normal NdbRecord + bitmask mechanism.
       * This could be : pseudo columns, columns read via an old-Api 
       * scan, or just some extra columns added by the user to an 
       * NdbRecord operation.
       * If the extra values are part of a scan then they get copied
       * to a special area after the end of the normal row data.  
       * When the user calls NdbScanOperation.nextResult() they will
       * be copied into the correct NdbRecAttr objects.
       * If the extra values are not part of a scan then they are
       * put into their NdbRecAttr objects now.
       */
      if (isScan)
      {
        /* For scans, we save the extra information at the end of the
         * row buffer, in reverse order.  When nextResult() is called,
         * this data is copied into the correct NdbRecAttr objects.
         */
        
        /* Save this extra getValue */
        save_pos+= sizeof(Uint32);
        memcpy(m_record.m_row_recv + m_record.m_row_offset - save_pos,
               &attrSize, sizeof(Uint32));
        if (attrSize > 0)
        {
          save_pos+= attrSize;
          assert (save_pos<=m_record.m_row_offset);
          memcpy(m_record.m_row_recv + m_record.m_row_offset - save_pos,
                 aDataPtr, attrSize);
        }

        Uint32 sizeInWords= (attrSize+3)>>2;
        aDataPtr+= sizeInWords;
        aLength-= sizeInWords;
        continue; // Next
      }
      else
      {
        /* Not a scan, so extra information is added to RecAttrs in
         * the 'normal' way.
         */
        assert(theCurrentRecAttr != NULL);
        assert(theCurrentRecAttr->attrId() == attrId);
        /* Handle extra attributes requested with getValue(). */
        /* This implies that we've finished with the NdbRecord part
           of the read, so move onto NdbRecAttr */
        ndbrecord_part_done=true;
        // Fall through to RecAttr handling
      }
    } // / if (!ndbrecord_part_done)
    
    /* If we get here then there are some attribute values to be
     * read into the attached list of NdbRecAttrs.
     * This occurs for old-Api primary and unique index keyed operations
     * and for NdbRecord primary and unique index keyed operations
     * using 'extra GetValues'.
     */
    if (ndbrecord_part_done)
    {
      // We've processed the NdbRecord part of the TRANSID_AI, if
      // any.  There are signal words left, so they must be
      // RecAttr data
      //
      if (attrId == AttributeHeader::READ_PACKED)
      {
        assert(!m_using_ndb_record);
        NdbRecAttr* tmp = currRecAttr;
        Uint32 len = receive_packed_recattr(&tmp, attrSize>>2, aDataPtr, origLength);
        aDataPtr += len;
        aLength -= len;
        currRecAttr = tmp;
        continue;
      }
      /**
       * Skip over missing attributes
       * TODO : How can this happen?
       */
      while(currRecAttr && currRecAttr->attrId() != attrId){
            currRecAttr = currRecAttr->next();
      }

      if(currRecAttr && currRecAttr->receive_data(aDataPtr, attrSize))
      {
        Uint32 add= (attrSize + 3) >> 2;
        aLength -= add;
        aDataPtr += add;
        currRecAttr = currRecAttr->next();
      } else {
        /*
          This should not happen: we got back an attribute for which we have no
          stored NdbRecAttr recording that we requested said attribute (or we got
          back attributes in the wrong order).
          So dump some info for debugging, and abort.
        */
        ndbout_c("this=%p: attrId: %d currRecAttr: %p theCurrentRecAttr: %p "
                 "attrSize: %d %d", this,
	         attrId, currRecAttr, theCurrentRecAttr, attrSize,
                 currRecAttr ? currRecAttr->get_size_in_bytes() : 0);
        currRecAttr = theCurrentRecAttr;
        while(currRecAttr != 0){
	  ndbout_c("%d ", currRecAttr->attrId());
	  currRecAttr = currRecAttr->next();
        }
        abort();
        return -1;
      } // if (currRecAttr...)      
    } // /if (ndbrecord_part_done)
  } // / while (aLength > 0)

  theCurrentRecAttr = currRecAttr;

  m_received_result_length = tmp;

  if (m_using_ndb_record) {
    /* Move onto next row in scan buffer */
    m_record.m_row_recv+= m_record.m_row_offset;
  }
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
  if (getType()==NDB_QUERY_OPERATION)
  {
    NdbQueryOperationImpl* op = (NdbQueryOperationImpl*)getOwner();
    op->getQuery().setErrorCode(code);
  }
  else
  {
    NdbOperation* const op = (NdbOperation*)getOwner();
    assert(op->checkMagicNumber()==0);
    op->setErrorCode(code);
  }
}
