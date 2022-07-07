/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "API.hpp"
#include <AttributeHeader.hpp>
#include <signaldata/TcKeyConf.hpp>
#include <signaldata/DictTabInfo.hpp>
#include "portlib/ndb_compiler.h"
#include <cstddef>
#include <cstdint>

/**
 * 'class NdbReceiveBuffer' takes care of buffering multi-row
 * result sets received as the result of scan- or query- operations.
 * Rows are stored in the 'raw' transporter format in the buffer,
 * and later (only) the 'current row' is retrieved from the buffer,
 * and unpacked into the full NdbRecord row format when navigated to.
 */
class NdbReceiverBuffer
{
public:

  /**
   * End of row and key buffer area has an 'eodMagic'
   * as a debugging aid in detecting buffer overflows.
   */
  static const Uint32 eodMagic = 0xacbd1234;

  explicit NdbReceiverBuffer(Uint32 bufSizeBytes,
                             Uint32 batchRows);

  void reset()
  { m_rows = m_keys = 0; }

  Uint32 getBufSizeWords() const
  { return m_bufSizeWords; }

  Uint32 getMaxRows() const
  { return m_maxRows; }

  Uint32 getRowCount() const
  { return m_rows; }

  Uint32 getKeyCount() const
  { return m_keys; }

  /**
   * Rows are buffered in the first part of 'm_buffer'. The first
   * 'm_maxrows+1' Uint32s in the buffer is a row_ix[] containing
   * buffer indexes, such that:
   *  - Row 'n' starts at m_buffer[row_ix[n]].
   *  - Length of row 'n' is 'row_ix[n+1] - row_ix[n].
   * row_ix[] contains one more item than 'm_maxrows'. The item
   * past the last row is maintained such that the length of last row
   * can be calculated.
   */
  Uint32 *allocRow(Uint32 noOfWords)
  {
    assert(verifyBuffer());
    const Uint32 pos = rowIx(m_rows);  //First free
    rowIx(++m_rows) = pos + noOfWords; //Next free
#ifndef NDEBUG
    m_buffer[pos + noOfWords] = eodMagic;
    assert(verifyBuffer());
#endif
    return &m_buffer[pos];
  }

  /**
   * Keys are allocated from the end of 'm_buffer', it grows and are
   * indexed in *reverse order*. key_ix[] is allocated at the very end of
   * the buffer:
   *  - Key 'n' starts at m_buffer[key_ix[n]]
   *  - Length of key 'n' is 'key_ix[n-1] - key_ix[n]'.
   */
  Uint32 *allocKey(Uint32 noOfWords)
  {
    assert(verifyBuffer());
    const Uint32 prev = keyIx(m_keys-1);
    const Uint32 pos = prev - noOfWords;
    keyIx(m_keys) = pos;
    m_keys++;
#ifndef NDEBUG
    m_buffer[pos-1] = eodMagic;
    assert(verifyBuffer());
#endif
    return &m_buffer[pos];
  }

  const Uint32 *getRow(Uint32 row, Uint32& noOfWords) const
  {
    assert(verifyBuffer());
    if (unlikely(row >= m_rows))
      return nullptr;

    const Uint32 ix = rowIx(row);
    noOfWords = rowIx(row+1) - ix;
    assert(noOfWords < m_bufSizeWords);  // Sanity check
    return m_buffer + ix;
  }

  const Uint32 *getKey(Uint32 key, Uint32& noOfWords) const
  {
    assert(verifyBuffer());
    if (unlikely(key >= m_keys))
      return nullptr;

    const Uint32 ix = keyIx(key);
    noOfWords = keyIx(key-1) - ix;
    assert(noOfWords < m_bufSizeWords);  // Sanity check
    return m_buffer + ix;
  }

  /**
   * Calculate total words required to be allocated for the
   * NdbReceiverBuffer structure.
   *
   * We know 'batchSizeWords', the total max size of data to fetch
   * from the data nodes. In addition there are some overhead required by
   * the buffer management itself. Calculate total words required to
   * be allocated for the NdbReceiverBuffer structure.
   */
  static Uint32 calculateBufferSizeInWords(Uint32 batchRows,
                                           Uint32 batchSizeWords, 
                                           Uint32 keySize)
  {
    return  batchSizeWords +        // Words to store
            1 +                     // 'eodMagic' in buffer
            headerWords +           // Admin overhead
            ((keySize > 0)          // Row + optional key indexes
              ? (batchRows+1) * 2   // Row + key indexes
	      : (batchRows+1));     // Row index only
  }

private:
  static const Uint32 headerWords= 4; //4*Uint32's below

  // No copying / assignment:
  NdbReceiverBuffer(const NdbReceiverBuffer&);
  NdbReceiverBuffer& operator=(const NdbReceiverBuffer&);

  const Uint32 m_maxRows;       // Max capacity in #rows / #keys
  const Uint32 m_bufSizeWords;  // Size of 'm_buffer'

  Uint32 m_rows;                // Current #rows in m_buffer
  Uint32 m_keys;                // Current #keys in m_buffer

  Uint32 m_buffer[1];           // Variable size buffer area (m_bufSizeWords)

  /**
   * Index to row offset is first 'maxrows' items in m_buffer.
   * We maintain a 'next free row' position for all
   * 'm_maxrows' in the buffer. Thus, this index array has
   * to contain 'm_maxrows+1' items, indexed from [0..m_maxrows].
   * This allows is to calculate data length for all rows 
   * as 'start(row+1) - start(row))
   */
  Uint32  rowIx(Uint32 row) const { return m_buffer[row]; }
  Uint32& rowIx(Uint32 row)       { return m_buffer[row]; }

  /**
   * Index to key offset is last 'maxrows' items in m_buffer.
   * We maintain a 'previous row start' position for all
   * 'm_maxrows' in the buffer - even for 'key 0'. 
   * Thus, this index array has to contain 'm_maxrows+1'
   * items, indexed from [-1..maxrows-1].
   * This allows is to calculate data length for all keys 
   * as 'start(key-1) - start(key)).
   */

  // 'm_bufSizeWords-2' is keyIx(0), that place keyIx(-1) at 'm_bufSizeWords-1'
  Uint32  keyIx(Uint32 key) const { return m_buffer[m_bufSizeWords-2-key]; }
  Uint32& keyIx(Uint32 key)       { return m_buffer[m_bufSizeWords-2-key]; }

  bool verifyBuffer() const
  {
    assert(m_rows <= m_maxRows);
    assert(m_keys <= m_maxRows);
    // Check rows startpos and end within buffer
    assert(rowIx(0) == m_maxRows+1);
    assert(rowIx(m_rows) <= m_bufSizeWords);

    // Rest of rows in sequence with non-negative length
    for (Uint32 row=0; row<m_rows; row++)
    {
      assert(rowIx(row) <= rowIx(row+1));
    }
    // Overflow protection
    assert(m_rows == 0 ||
           m_buffer[rowIx(m_rows)] == eodMagic);

    if (m_keys > 0)
    {
      // Check keys startpos and end before row buffer
      assert(keyIx(-1) == (m_bufSizeWords - (m_maxRows+1)));
      assert(keyIx(m_keys-1) >= rowIx(m_rows));

      // Rest of keys in sequence with non-negative length
      for (Uint32 key=0; key<m_keys; key++)
      {
        assert(keyIx(key) <= keyIx(key-1));
      }

      // Overflow protection
      assert(m_buffer[keyIx(m_keys-1)-1] == eodMagic);
    }
    return true;
  }

}; //class NdbReceiverBuffer

NdbReceiverBuffer::NdbReceiverBuffer(
			     Uint32 bufSizeBytes, // Word aligned size
                             Uint32 batchRows)
  : m_maxRows(batchRows), 
    m_bufSizeWords((bufSizeBytes/sizeof(Uint32)) - headerWords), 
    m_rows(0), 
    m_keys(0)
{
  assert((bufSizeBytes/sizeof(Uint32)) > headerWords);

  /**
   * Init row and key index arrays. Row indexes maintain
   * a 'next free row' position which for rows start imm.
   * after the 'm_maxrows+1' indexes.
   */ 
  rowIx(0) = m_maxRows+1;

  /**
   * Key indexes maintain a 'prev key startpos', even for key(0).
   * Thus, for an empty key_ix[], we set startpos for 
   * (the non-existing) key(-1) which is imm. after the
   * available key buffer area.
   *
   * NOTE: We init key_ix[] even if keyinfo not present
   * in result set. that case it might later be overwritten
   * by rows, which is ok as the keyinfo is then never used.
   */
  keyIx(-1) = m_bufSizeWords - (m_maxRows+1);
  assert(verifyBuffer());
}


/**
 * 'BEFORE' is used as the initial position before having a
 * valid 'current' row. Beware, wraparound is assumed such
 * that ' beforeFirstRow+1' -> 0 (first row)
 */
static const Uint32 beforeFirstRow = 0xFFFFFFFF;

static
const Uint8*
pad(const Uint8* src, Uint32 align, Uint32 bitPos);

static
size_t
pad_pos(size_t pos, Uint32 align, Uint32 bitPos);

NdbReceiver::NdbReceiver(Ndb *aNdb) :
  theMagicNumber(0),
  m_ndb(aNdb),
  m_id(NdbObjectIdMap::InvalidId),
  m_tcPtrI(RNIL),
  m_type(NDB_UNINITIALIZED),
  m_owner(nullptr),
  m_ndb_record(nullptr),
  m_row_buffer(nullptr),
  m_recv_buffer(nullptr),
  m_read_range_no(false),
  m_read_key_info(false),
  m_firstRecAttr(nullptr),
  m_lastRecAttr(nullptr),
  m_rec_attr_data(nullptr),
  m_rec_attr_len(0),
  m_current_row(beforeFirstRow),
  m_expected_result_length(0),
  m_received_result_length(0)
{}
 
NdbReceiver::~NdbReceiver()
{
  DBUG_ENTER("NdbReceiver::~NdbReceiver");
  if (m_id != NdbObjectIdMap::InvalidId) {
    m_ndb->theImpl->unmapRecipient(m_id, this);
  }
  DBUG_VOID_RETURN;
}

//static
NdbReceiverBuffer*
NdbReceiver::initReceiveBuffer(Uint32 *buffer,      // Uint32 aligned buffer
                               Uint32 bufSizeBytes, // Size, from ::result_bufsize()
                               Uint32 batchRows)
{
  assert(((UintPtr)buffer % sizeof(Uint32)) == 0);   //Is Uint32 aligned

  return new(buffer) NdbReceiverBuffer(bufSizeBytes, batchRows);
}

int
NdbReceiver::init(ReceiverType type, void* owner)
{
  theMagicNumber = getMagicNumber();
  m_type = type;
  m_owner = owner;
  m_ndb_record= nullptr;
  m_row_buffer= nullptr;
  m_recv_buffer= nullptr;
  m_read_range_no= false;
  m_read_key_info= false;
  m_firstRecAttr = nullptr;
  m_lastRecAttr = nullptr;
  m_rec_attr_data = nullptr;
  m_rec_attr_len = 0;

  if (m_id == NdbObjectIdMap::InvalidId)
  {
    if (m_ndb)
    {
      m_id = m_ndb->theImpl->mapRecipient(this);
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
NdbReceiver::do_setup_ndbrecord(const NdbRecord *ndb_record,
                                char *row_buffer,
                                bool read_range_no, bool read_key_info)
{
  m_ndb_record= ndb_record;
  m_row_buffer= row_buffer;
  m_recv_buffer= nullptr;
  m_read_range_no= read_range_no;
  m_read_key_info= read_key_info;
}

void
NdbReceiver::release()
{
  theMagicNumber = 0;
  NdbRecAttr* tRecAttr = m_firstRecAttr;
  while (tRecAttr != nullptr)
  {
    NdbRecAttr* tSaveRecAttr = tRecAttr;
    tRecAttr = tRecAttr->next();
    m_ndb->releaseRecAttr(tSaveRecAttr);
  }
  m_firstRecAttr = nullptr;
  m_lastRecAttr = nullptr;
  m_rec_attr_data = nullptr;
  m_rec_attr_len = 0;
  m_ndb_record= nullptr;
  m_row_buffer= nullptr;
  m_recv_buffer= nullptr;
}
  
NdbRecAttr *
NdbReceiver::getValue(const NdbColumnImpl* tAttrInfo, char * user_dst_ptr)
{
  NdbRecAttr* tRecAttr = m_ndb->getRecAttr();
  if(tRecAttr && !tRecAttr->setup(tAttrInfo, user_dst_ptr)){
    if (m_firstRecAttr == nullptr)
      m_firstRecAttr = tRecAttr;
    else
      m_lastRecAttr->next(tRecAttr);
    m_lastRecAttr = tRecAttr;
    tRecAttr->next(nullptr);
    return tRecAttr;
  }
  if(tRecAttr){
    m_ndb->releaseRecAttr(tRecAttr);
  }    
  return nullptr;
}

void
NdbReceiver::getValues(const NdbRecord* rec, char *row_ptr)
{
  assert(m_recv_buffer == nullptr);
  assert(rec != nullptr);

  m_ndb_record= rec;
  m_row_buffer= row_ptr;
}

void
NdbReceiver::prepareSend()
{
  /* Set pointers etc. to prepare for receiving the first row of the batch. */
  theMagicNumber = 0x11223344;
  m_current_row = beforeFirstRow;
  m_received_result_length = 0;
  m_expected_result_length = 0;

  if (m_recv_buffer != nullptr)
  {
    m_recv_buffer->reset();
  }
}

void
NdbReceiver::prepareReceive(NdbReceiverBuffer *buffer)
{
  m_recv_buffer= buffer;
  prepareSend();
}

/*
  Compute the batch size (rows between each NEXT_TABREQ / SCAN_TABCONF) to
  use, taking into account limits in the transporter, user preference, etc.

  It is the responsibility of the batch producer (LQH+TUP) to
  stay within these 'batch_size' and 'batch_byte_size' limits.:

  - It should stay strictly within the 'batch_size' (#rows) limit.
  - It is allowed to overallocate the 'batch_byte_size' (slightly)
    in order to complete the current row when it hit the limit.
    (Up to ::packed_rowsize())

  The client should be prepared to receive, and buffer, up to 
  'batch_size' rows from each fragment.
*/
//static
void
NdbReceiver::calculate_batch_size(const NdbImpl& theImpl,
                                  Uint32 parallelism,
                                  Uint32& batch_size,
                                  Uint32& batch_byte_size)
{
  const NdbApiConfig & cfg = theImpl.get_ndbapi_config_parameters();
  const Uint32 max_scan_batch_size= cfg.m_scan_batch_size;
  const Uint32 max_batch_byte_size= cfg.m_batch_byte_size;
  const Uint32 max_batch_size= cfg.m_batch_size;

  batch_byte_size= max_batch_byte_size;
  if (batch_byte_size * parallelism > max_scan_batch_size) {
    batch_byte_size= max_scan_batch_size / parallelism;
  }

  if (batch_size == 0 || batch_size > max_batch_size) {
    batch_size= max_batch_size;
  }
  if (unlikely(batch_size > MAX_PARALLEL_OP_PER_SCAN)) {
    batch_size= MAX_PARALLEL_OP_PER_SCAN;
  }
  if (unlikely(batch_size > batch_byte_size)) {
    batch_size= batch_byte_size;
  }
}

void
NdbReceiver::calculate_batch_size(Uint32 parallelism,
                                  Uint32& batch_size,
                                  Uint32& batch_byte_size) const
{
  calculate_batch_size(* m_ndb->theImpl,
                       parallelism,
                       batch_size,
                       batch_byte_size);
}

//static
Uint32
NdbReceiver::ndbrecord_rowsize(const NdbRecord *result_record,
                               bool read_range_no)
{
  // Unpacked NdbRecords are stored in its full unprojected form
  Uint32 rowsize= (result_record) 
                 ? result_record->m_row_size
                 : 0;

  // After unpack, the optional RANGE_NO is stored as an Uint32 
  if (read_range_no)
    rowsize+= sizeof(Uint32);

  return (rowsize+3) & 0xfffffffc;
}

/**
 * Calculate max size (In Uint32 words) of a 'packed' result row,
 * including optional 'keyinfo', 'range_no' and 'correlation'.
 *
 * Note that
 *   - keyInfo is stored in its own 'key storage' in the buffer.
 *   - 'correlation' is not stored in the receive buffer at all.
 */
static
Uint32 packed_rowsize(const NdbRecord *result_record,
                      const Uint32* read_mask,
                      const NdbRecAttr *first_rec_attr,
                      Uint32 keySizeWords,
                      bool read_range_no,
                      bool read_correlation)
{
  Uint32 nullCount = 0;
  Uint32 bitPos = 0;
  UintPtr pos = 0;

  bool pk_is_known = false;
  if (likely(result_record != nullptr))
  {
    for (Uint32 i= 0; i<result_record->noOfColumns; i++)
    {
      const NdbRecord::Attr *col= &result_record->columns[i];
      const bool is_pk= (col->flags & NdbRecord::IsKey);
      const Uint32 attrId= col->attrId;

      if (is_pk)
      {
        pk_is_known = true;
      }
      /* Skip column if result_mask says so and we don't need
       * to read it 
       */
      if (BitmaskImpl::get(MAXNROFATTRIBUTESINWORDS, read_mask, attrId))
      {
        const Uint32 align = col->orgAttrSize;

        switch(align){
        case DictTabInfo::aBit:
          pos = pad_pos(pos, 0, 0);
          bitPos += col->bitCount;
          pos += 4 * (bitPos / 32);
          bitPos = (bitPos % 32);
          break;
        default:
          pos = pad_pos(pos, align, bitPos);
          bitPos = 0;
          pos += col->maxSize;
          break;
        }

        if (col->flags & NdbRecord::IsNullable)
          nullCount++;
      }
    }
  }
  Uint32 sizeInWords = pad_pos(pos, 0, bitPos);

  // Add AttributeHeader::READ_PACKED or ::READ_ALL (Uint32) and
  // variable size bitmask the 'packed' columns and their null bits.
  if (sizeInWords > 0)
  {
    Uint32 attrCount= result_record->columns[result_record->noOfColumns -1].attrId+1;
    if (! pk_is_known)
    {
      // Hidden key column is still present in bitmask
      attrCount++;
    }
    const Uint32 sigBitmaskWords= ((attrCount+nullCount+31)>>5);
    sizeInWords += (1+sigBitmaskWords);   //AttrHeader + bitMask
  }

  // The optional RANGE_NO is transferred and stored in buffer
  // as AttributeHeader::RANGE_NO + an Uint32 'range_no'
  if (read_range_no)
  {
    sizeInWords += 2;
  }
  // The optional CORR_FACTOR is transferred
  // as AttributeHeader::CORR_FACTOR64 + an Uint64
  if (read_correlation)
  {
    sizeInWords += 3;
  }

  // KeyInfo is transferred in a separate signal,
  // and is stored in the packed buffer together with 'info' word
  if (keySizeWords > 0)
  {
    sizeInWords+= keySizeWords+1;
  }

  /* Add extra needed to transfer RecAttrs requested by getValue() */
  const NdbRecAttr *ra= first_rec_attr;
  while (ra != nullptr)
  {
    // AttrHeader + max column size. Aligned to word boundary
    sizeInWords+= 1 + ((ra->getColumn()->getSizeInBytes() + 3) / 4);
    ra= ra->next();
  }

  return sizeInWords;
}

/**
 * Calculate the two parameters 'batch_bytes' and 
 * 'buffer_bytes' required for result set of 'batch_rows':
 *
 * - 'batch_bytes' is the 'batch_size_bytes' argument to be
 *   specified as part of a SCANREQ signal. It could be set
 *   as an IN argument, in which case it would be an upper limit
 *   of the allowed batch size. If '0' it will return the max
 *   'byte' size required for all 'batch_rows'. If set, it will
 *   also be capped to the max required 'batch_rows' size.
 *
 * - 'buffer_bytes' is the size of the buffer needed to be allocated
 *   in order to store the result batch of size batch_rows / _bytes.
 *   Size also include overhead required by the NdbReceiverBuffer itself.
 */
//static
void
NdbReceiver::result_bufsize(const NdbRecord *result_record,
                            const Uint32* read_mask,
                            const NdbRecAttr *first_rec_attr,
                            Uint32 keySizeWords,
                            bool   read_range_no,
                            bool   read_correlation,
                            Uint32  parallelism,
                            Uint32  batch_rows,   //Argument in SCANREQ
                            Uint32& batch_bytes,  //Argument in SCANREQ
                            Uint32& buffer_bytes) //Buffer needed to store result
{
  assert(parallelism >= 1);
  assert(batch_rows  > 0);

  /**
   * Calculate size of a single row as sent by TUP.
   * Include optional 'keyInfo', RANGE_NO and CORR_FACTOR.
   */
  const Uint32 rowSizeWords= packed_rowsize(
                                       result_record,
                                       read_mask,
                                       first_rec_attr,
                                       keySizeWords,
                                       read_range_no,
                                       read_correlation);

  // Size of a full result set of 'batch_rows':
  const Uint32 fullBatchSizeWords = batch_rows * rowSizeWords;

  /**
   * Size of batch, and the required 'buffer_bytes', is either 
   * limited by fetching all 'batch_rows', or by exhausting the max
   * allowed 'batch_bytes'.
   *
   * In the later case we can make no assumption about number of rows we
   * actually fetched, except that it will be in the range 1..'batch_rows'.
   * So we need to take a conservative approach in our calculations here. 
   *
   * Furthermore, LQH doesn't terminate the batch until *after*
   * 'batch_bytes' has been exceeded. Thus it could over-deliver
   * up to 'rowSizeWords-1' more than specified in 'batch_bytes'!
   * When used from SPJ, the available 'batch_bytes' may be divided
   * among a number of 'parallelism' fragment scans being joined.
   * Each of these may over-deliver on the last row as described above.
   *
   * Note that the CORR_FACTOR is special in that SPJ does not store
   * it in the receiver buffer. Thus, the size of the CORR_FACTOR64
   * is subtracted when calculating needed buffer space for the batch.
   *
   * If KeyInfo is requested, an additional 'info' word is stored 
   * in the buffer in addition to the 'keySize' already being part
   * of the calculated packed_rowsize().
   */
  Uint32 maxWordsToBuffer = 0;

  if (batch_bytes == 0 ||
      batch_bytes > fullBatchSizeWords*sizeof(Uint32))
  {
    /**
     * The result batch is only limited by max 'rows'.
     * Exclude fetched correlation factors in calculation of
     * required result buffers.
     * Note: TUP will not 'over-return' in this case as 
     * the specified 'batch_bytes' can not be exceeded.
     */
    maxWordsToBuffer = fullBatchSizeWords
                     - ((read_correlation) ?(batch_rows * 3) :0);

    /**
     * Set/Limit 'batch_bytes' to max 'fullBatchSizeWords', as that
     * is what it will be allocated result buffer for.
     */
    batch_bytes = fullBatchSizeWords*sizeof(Uint32);
  }
  else
  {
    // Round batch size to 'Words'
    const Uint32 batchWords = (batch_bytes+sizeof(Uint32)-1)/sizeof(Uint32);

    /**
     * Batch may be limited by 'bytes' before reaching max 'rows'.
     * - Add 'over-returned' result from each fragment retrieving rows
     *   into this batch.
     * - Subtract CORR_FACTORs retrieved in batch, but not buffered.
     *   As number of rows returned is not known, we can only assume
     *   that at least 1 row is returned.
     */
    maxWordsToBuffer = batchWords
                     + ((rowSizeWords-1) * parallelism)  // over-return
                     - ((read_correlation) ? (1 * 3) : 0); // 1 row

    //Note: 'batch_bytes' is used unmodified in 'SCANREQ'
  }

  /**
   * NdbReceiver::execKEYINFO20() will allocate an extra word (allocKey())
   * for storing the 'info' word in the buffer. 'info' is not part of
   * the 'key' returned from datanodes, so not part of what packed_rowsize()
   * already calculated.
   */ 
  if (keySizeWords > 0)
  {
    maxWordsToBuffer += (1 * batch_rows); // Add 'info' part of keyInfo
  }

  /**
   * Calculate max size (In bytes) of a NdbReceiverBuffer containing
   * 'batch_rows' of packed result rows. Size also include 
   * overhead required by the NdbReceiverBuffer itself.
   */
  buffer_bytes =
    NdbReceiverBuffer::calculateBufferSizeInWords(batch_rows, maxWordsToBuffer, keySizeWords)*4;
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
UintPtr
pad_pos(UintPtr pos, Uint32 align, Uint32 bitPos)
{
  UintPtr ptr = pos;
  switch(align)
  {
  case DictTabInfo::aBit:
  case DictTabInfo::a32Bit:
  case DictTabInfo::a64Bit:
  case DictTabInfo::a128Bit:
    return (((ptr + 3) & ~UintPtr{3}) + 4 * ((bitPos + 31) >> 5));

  default:
#ifdef VM_TRACE
    abort();
#endif
    [[fallthrough]];

  case DictTabInfo::an8Bit:
  case DictTabInfo::a16Bit:
    return pos + 4 * ((bitPos + 31) >> 5);
  }
}

static
inline
const Uint8*
pad(const Uint8* src, Uint32 align, Uint32 bitPos)
{
  UintPtr ptr = UintPtr(src);
  return (const Uint8*)pad_pos(ptr, align, bitPos);
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
  const Uint32* src = (const Uint32*)_src;
  assert((UintPtr(src) & 3) == 0);

  /* Convert char* to aligned Uint32* and some byte offset */
  UintPtr uiPtr= UintPtr((Uint32*)_dst);
  Uint32 dstByteOffset= Uint32(uiPtr) & 3;
  Uint32* dst= (Uint32*) (uiPtr - dstByteOffset); 

  BitmaskImpl::copyField(dst, dstByteOffset << 3,
                         src, pos, len);
}

/**
 * unpackRecAttr
 * Unpack a packed stream of field values, whose presence and nullness
 * is indicated by a leading bitmap into a list of NdbRecAttr objects
 * Return the number of words read from the input stream.
 * On failure UINT32_MAX is returned.
 */
Uint32 NdbReceiver::unpackRecAttr(NdbRecAttr** recAttr,
                                  Uint32 bmlen,
                                  const Uint32* const aDataPtr,
                                  Uint32 aLength)
{
  constexpr Uint32 ERROR = UINT32_MAX;
  if (unlikely(bmlen > aLength)) return ERROR;
  NdbRecAttr* currRecAttr = *recAttr;
  const Uint8* src = (const Uint8*)(aDataPtr + bmlen);
  const Uint8* const end = (const Uint8*)(aDataPtr + aLength);
  Uint32 bitPos = 0;
  for (Uint32 i = 0, attrId = 0; i<32*bmlen; i++, attrId++)
  {
    if (BitmaskImpl::get(bmlen, aDataPtr, i))
    {
      const NdbColumnImpl & col = 
	NdbColumnImpl::getImpl(* currRecAttr->getColumn());
      if (unlikely(attrId != (Uint32)col.m_attrId)) return ERROR;
      if (col.m_nullable)
      {
        if (unlikely(i + 1 >= 32 * bmlen)) return ERROR;
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
      {
        src = pad(src, 0, 0);
        size_t byte_len = 4 * ((bitPos + len) >> 5);
        if (unlikely(end < src + byte_len)) return ERROR;
        handle_packed_bit((const char*)src, bitPos, len, currRecAttr->aRef());
        src += byte_len;
        bitPos = (bitPos + len) & 31;
        currRecAttr->set_size_in_bytes(sz);
        goto next;
      }
      default:
        src = pad(src, align, bitPos);
      }
      switch(arrayType){
      case NDB_ARRAYTYPE_FIXED:
        break;
      case NDB_ARRAYTYPE_SHORT_VAR:
        if (unlikely(end < src + 1)) return ERROR;
        sz = 1 + src[0];
        break;
      case NDB_ARRAYTYPE_MEDIUM_VAR:
        if (unlikely(end < src + 2)) return ERROR;
        sz = 2 + src[0] + 256 * src[1];
        break;
      default:
        return ERROR;
      }
      
      bitPos = 0;
      if (unlikely(end < src + sz)) return ERROR;
      currRecAttr->receive_data((const Uint32*)src, sz);
      src += sz;
  next:
      currRecAttr = currRecAttr->next();
    }
  }
  * recAttr = currRecAttr;
  const Uint8* read_src = pad(src, 0, bitPos);
  if (unlikely(end < read_src)) return ERROR;
  const std::ptrdiff_t read_words = (const Uint32*)read_src - aDataPtr;
  if (unlikely(read_words < 0) || unlikely(read_words > INT32_MAX))
    return ERROR;
  return (Uint32)read_words;
}


int
NdbReceiver::get_range_no() const
{
  Uint32 range_no;
  assert(m_ndb_record != nullptr);
  assert(m_row_buffer != nullptr);

  if (unlikely(!m_read_range_no))
    return -1;

  memcpy(&range_no,
         m_row_buffer + m_ndb_record->m_row_size,
         sizeof(range_no));
  return (int)range_no;
}

/**
 * handle_bitfield_ndbrecord
 * Packed bitfield handling for NdbRecord - also deals with 
 * mapping the bitfields into MySQLD format if necessary.
 */
ATTRIBUTE_NOINLINE
static void
handle_bitfield_ndbrecord(const NdbRecord::Attr* col,
                          const Uint8*& src,
                          Uint32& bitPos,
                          char* row)
{
  Uint32 len = col->bitCount;
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
    mysqldSpace = 0;
    dest= (char*) &mysqldSpace;
  }
  else
  {
    dest= row + col->offset;
  }
  
  /* Copy bitfield to memory starting at dest */
  src = pad(src, 0, 0);
  handle_packed_bit((const char*)src, bitPos, len, dest); 
  src += 4 * ((bitPos + len) >> 5);
  bitPos = (bitPos + len) & 31;
  
  if (isMDBitfield)
  {
    /* Rearrange bitfield from stack to row storage */
    col->put_mysqld_bitfield(row, dest);
  }
}


/**
 * unpackNdbRecord
 * Unpack a stream of field values, whose presence and nullness
 * is indicated by a leading bitmap, into an NdbRecord row.
 * Return the number of words consumed.
 */
//static
Uint32
NdbReceiver::unpackNdbRecord(const NdbRecord *rec,
                             const Uint32 bmlen,
                             const Uint32* aDataPtr,
                             char* row)
{
  assert(bmlen <= 0x07FF);
  const Uint8* src = (const Uint8*)(aDataPtr + bmlen);
  uint bitPos = 0;
  uint attrId = 0;
  uint bitIndex = 0;

  /* Use bitmap to determine which columns have been sent */
  for (uint nextBit = BitmaskImpl::find_first(bmlen, aDataPtr);
       nextBit != BitmaskImpl::NotFound;
       nextBit = BitmaskImpl::find_next(bmlen, aDataPtr, bitIndex+1))
  {
    /* Found bit in column presence bitmask, get corresponding
     * Attr struct from NdbRecord
     */
    attrId += (nextBit-bitIndex);
    bitIndex = nextBit;
    assert(attrId < rec->m_attrId_indexes_length);

    const uint next_index = rec->m_attrId_indexes[attrId];
    assert (next_index < rec->noOfColumns);

    const NdbRecord::Attr* col = &rec->columns[next_index];
    assert((col->flags & NdbRecord::IsBlob) == 0);

    /* If col is nullable, check for null and set/clear NULL-bit */
    if (col->flags & NdbRecord::IsNullable)
    {
      const char null_byte = row[col->nullbit_byte_offset];
      const char null_mask = (1 << col->nullbit_bit_in_byte);
      bitIndex++;
      if (BitmaskImpl::get(bmlen, aDataPtr, bitIndex))
      {
        /* NULL value -> set NULL indicator bit */
        row[col->nullbit_byte_offset] = null_byte | null_mask;
        assert(bitPos < 32);
        continue; /* Next column */
      }
      /* not-NULL, clear NULL indicator */
      row[col->nullbit_byte_offset] = null_byte & ~null_mask;
    }

    const Uint32 align = col->orgAttrSize;
    if (unlikely(align == DictTabInfo::aBit))
    {
      const Uint8 *loc_src = src;
      handle_bitfield_ndbrecord(col,
                                loc_src,
                                bitPos,
                                row);
      src = loc_src;
      assert(bitPos < 32);
      continue; /* Next column */
    }

    src = pad(src, align, bitPos);
    bitPos = 0;
    Uint32 sz;
    char *col_row_ptr = &row[col->offset];
    const Uint32 flags = col->flags &
                             (NdbRecord::IsVar1ByteLen |
                              NdbRecord::IsVar2ByteLen);
    if (!flags)
      sz = col->maxSize;
    else if (flags & NdbRecord::IsVar1ByteLen)
      sz = 1 + src[0];
    else
      sz = 2 + src[0] + 256 * src[1];

    const Uint8 *source = src;
    src += sz;
    memcpy(col_row_ptr, source, sz);
  }
  const Uint32 len = (Uint32)(((const Uint32*)pad(src, 0, bitPos)) - aDataPtr);
  return len;
}

int
NdbReceiver::get_keyinfo20(Uint32 & scaninfo, Uint32 & length,
                           const char * & data_ptr) const
{
  if (unlikely(!m_read_key_info))
    return -1;

  Uint32 len;
  const Uint32 *p = m_recv_buffer->getKey(m_current_row, len);
  if (unlikely(p == nullptr))
    return -1;

  scaninfo = *p;
  data_ptr = reinterpret_cast<const char*>(p+1);
  length = len-1;
  return 0;
}

const char* 
NdbReceiver::unpackBuffer(const NdbReceiverBuffer *buffer, Uint32 row)
{
  assert(buffer != nullptr);

  Uint32 aLength;
  const Uint32 *aDataPtr = buffer->getRow(row, aLength);
  if (likely(aDataPtr != nullptr))
  {
    if (unpackRow(aDataPtr, aLength, m_row_buffer) == -1)
      return nullptr;

    return m_row_buffer;
  }

  /* ReceiveBuffer may contain only keyinfo */
  const Uint32 *key = buffer->getKey(row, aLength);
  if (key != nullptr)
  {
    assert(m_row_buffer != nullptr);
    return m_row_buffer; // Row is empty, used as non-NULL return
  }
  return nullptr;
}

int 
NdbReceiver::unpackRow(const Uint32* aDataPtr, Uint32 aLength, char* row)
{
  /*
   * NdbRecord and NdbRecAttr row result handling are merged here
   *   First any NdbRecord attributes are extracted
   *   Then any NdbRecAttr attributes are extracted
   * Scenarios : 
   *   NdbRecord only PK read result
   *   NdbRecAttr only PK read result
   *   Mixed PK read results
   *   NdbRecord only scan read result
   *   NdbRecAttr only scan read result
   *   Mixed scan read results
   */

  /* If present, NdbRecord data will come first */
  if (m_ndb_record != nullptr)
  {
    /* Read words from the incoming signal train.
     * The length passed in is enough for one row, either as an individual
     * read op, or part of a scan.  When there are no more words, we're at
     * the end of the row
     */
    while (aLength > 0)
    {
      const AttributeHeader ah(* aDataPtr++);
      const Uint32 attrId= ah.getAttributeId();
      const Uint32 attrSize= ah.getByteSize();
      aLength--;
      assert(aLength >= (attrSize/sizeof(Uint32)));

      /* Normal case for all NdbRecord primary key, index key, table scan
       * and index scan reads. Extract all requested columns from packed
       * format into the row.
       */
      if (likely(attrId == AttributeHeader::READ_PACKED))
      {
        assert(row != nullptr);
        const Uint32 len= unpackNdbRecord(m_ndb_record,
                                          attrSize >> 2, // Bitmap length
                                          aDataPtr,
                                          row);
        assert(aLength >= len);
        aDataPtr+= len;
        aLength-= len;
      }

      /* Special case for RANGE_NO, which is received first and is
       * stored just after the row. */
      else if (attrId == AttributeHeader::RANGE_NO)
      {
        assert(row != nullptr);
        assert(m_read_range_no);
        assert(attrSize==sizeof(Uint32));
        memcpy(row+m_ndb_record->m_row_size, aDataPtr++, sizeof(Uint32));
        aLength--;
      }

      else
      {
        /* If we get here then we must have 'extra getValues' - columns
         * requested outwith the normal NdbRecord + bitmask mechanism.
         * This could be : pseudo columns, columns read via an old-Api 
         * scan, or just some extra columns added by the user to an 
         * NdbRecord operation.
         */
        aDataPtr--;   // Undo read of AttributeHeader
        aLength++;
        break;
      }
    } // while (aLength > 0)
  } // if (m_ndb_record != NULL)

  /* Handle 'getValues', possible requested after NdbRecord columns. */
  if (aLength > 0)
  {
    /**
     * If we get here then there are some attribute values to be
     * read into the attached list of NdbRecAttrs.
     * This occurs for old-Api primary and unique index keyed operations
     * and for NdbRecord primary and unique index keyed operations
     * using 'extra GetValues'.
     *
     * If the values are part of a scan then we save
     * the starting point of these RecAttr values.
     * When the user calls NdbScanOperation.nextResult(), they will
     * be copied into the correct NdbRecAttr objects by calling
     * NdbRecord::get_AttrValues.
     * If the extra values are not part of a scan, then they are
     * put into their NdbRecAttr objects now.
     */
    const bool isScan= (m_type == NDB_SCANRECEIVER) ||
                       (m_type == NDB_QUERY_OPERATION);

    if (isScan)
    {
      /* Save position for RecAttr values for later retrieval. */
      m_rec_attr_data = aDataPtr;
      m_rec_attr_len = aLength;
      return 0;
    }
    else
    {
      /* Put values into RecAttr now */ 
      const int ret = handle_rec_attrs(m_firstRecAttr, aDataPtr, aLength);
      if (unlikely(ret != 0))
        return -1;

      aDataPtr += aLength;
      aLength  = 0;
    }
  } // if (aLength > 0)

  m_rec_attr_data = nullptr;
  m_rec_attr_len = 0;
  return 0;
}

//static
int
NdbReceiver::handle_rec_attrs(NdbRecAttr* rec_attr_list,
                              const Uint32* aDataPtr,
                              Uint32 aLength)
{
  NdbRecAttr* currRecAttr = rec_attr_list;

  /* If we get here then there are some attribute values to be
   * read into the attached list of NdbRecAttrs.
   * This occurs for old-Api primary and unique index keyed operations
   * and for NdbRecord primary and unique index keyed operations
   * using 'extra GetValues'.
   */
  while (aLength > 0)
  {
    const AttributeHeader ah(* aDataPtr++);
    const Uint32 attrId= ah.getAttributeId();
    const Uint32 attrSize= ah.getByteSize();
    aLength--;
    assert(aLength >= (attrSize/sizeof(Uint32)));

    {
      // We've processed the NdbRecord part of the TRANSID_AI, if
      // any.  There are signal words left, so they must be
      // RecAttr data
      //
      if (attrId == AttributeHeader::READ_PACKED)
      {
        const Uint32 len = unpackRecAttr(&currRecAttr, 
                                         attrSize>>2, aDataPtr, aLength);
        if (unlikely(len == UINT32_MAX)) return -1;
        assert(aLength >= len);
        if (unlikely(aLength < len)) return -1;
        aDataPtr += len;
        aLength -= len;
        continue;
      }

      if(currRecAttr && 
         currRecAttr->attrId() == attrId &&
         currRecAttr->receive_data(aDataPtr, attrSize))
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
        g_eventLogger->info(
            "NdbReceiver::handle_rec_attrs:"
            " attrId: %d currRecAttr: %p rec_attr_list: %p attrSize: %d %d",
            attrId, currRecAttr, rec_attr_list, attrSize,
            currRecAttr ? currRecAttr->get_size_in_bytes() : 0);
        currRecAttr = rec_attr_list;
        while(currRecAttr != nullptr){
          g_eventLogger->info("%d ", currRecAttr->attrId());
          currRecAttr = currRecAttr->next();
        }
        abort();
        return -1;
      } // if (currRecAttr...)      
    }
  } // while (aLength > 0)

  return 0;
}

int
NdbReceiver::get_AttrValues(NdbRecAttr* rec_attr_list) const
{
  return handle_rec_attrs(rec_attr_list, 
                          m_rec_attr_data,
                          m_rec_attr_len);
}

int
NdbReceiver::execTRANSID_AI(const Uint32* aDataPtr, Uint32 aLength)
{
  const Uint32 exp= m_expected_result_length;
  const Uint32 tmp= m_received_result_length + aLength;

  /*
   * Store received data unprocessed into receive buffer
   * in its packed format.
   * It is unpacked into NdbRecord format when
   * we navigate to each row.
   */
  if (m_recv_buffer != nullptr)
  {
    Uint32 *row_recv = m_recv_buffer->allocRow(aLength);
    if (likely(aLength > 0))
    {
      memcpy(row_recv, aDataPtr, aLength*sizeof(Uint32));
    }
  }
  else
  {
    if (unpackRow(aDataPtr, aLength, m_row_buffer) == -1)
      return -1;
  }
  m_received_result_length = tmp;
  return (tmp == exp || (exp > TcKeyConf::DirtyReadBit) ? 1 : 0);
}

int
NdbReceiver::execKEYINFO20(Uint32 info, const Uint32* aDataPtr, Uint32 aLength)
{
  assert(m_read_key_info);
  assert(m_recv_buffer != nullptr);

  Uint32 *keyinfo_ptr = m_recv_buffer->allocKey(aLength+1);

  // Copy in key 'info', followed by 'data'
  *keyinfo_ptr= info;
  memcpy(keyinfo_ptr+1, aDataPtr, 4*aLength);

  const Uint32 tmp= m_received_result_length + aLength;
  m_received_result_length = tmp;

  return (tmp == m_expected_result_length ? 1 : 0);
}

const char*
NdbReceiver::getRow(const NdbReceiverBuffer* buffer, Uint32 row)
{
  return unpackBuffer(buffer, row);
}

const char* 
NdbReceiver::getNextRow()
{
  assert(m_recv_buffer != nullptr);
  const Uint32 nextRow =  m_current_row+1;
  const char *row = unpackBuffer(m_recv_buffer, nextRow);
  if (likely(row != nullptr))
  {
    m_current_row = nextRow;
  }
  return row;
}

int
NdbReceiver::execSCANOPCONF(Uint32 tcPtrI, Uint32 len, Uint32 rows)
{
  assert(m_recv_buffer != nullptr);
  assert(m_recv_buffer->getMaxRows() >= rows);
  assert(m_recv_buffer->getBufSizeWords() >= len);

  m_tcPtrI = tcPtrI;

  if (unlikely(len == 0))
  {
    /**
     * No TRANSID_AI will be received. (Likely an empty projection requested.)
     * To get row count correct, we simulate specified number of 
     * empty TRANSID_AIs being received.
     */
    for (Uint32 row=0; row<rows; row++)
    {
      execTRANSID_AI(nullptr,0);
    }
  }

  const Uint32 tmp = m_received_result_length;
  m_expected_result_length = len;
  return (tmp == len ? 1 : 0);
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
