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

#ifndef NdbReceiver_H
#define NdbReceiver_H
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL  // Not part of public interface

#include <ndb_types.h>


class Ndb;
class NdbImpl;
class NdbTransaction;
class NdbRecord;
class NdbRecAttr;
class NdbQueryOperationImpl;
class NdbReceiverBuffer;

class NdbReceiver
{
  friend class Ndb;
  friend class NdbImpl;
  friend class NdbOperation;
  friend class NdbQueryImpl;
  friend class NdbQueryOperationImpl;
  friend class NdbResultStream;
  friend class NdbScanOperation;
  friend class NdbIndexOperation;
  friend class NdbIndexScanOperation;
  friend class NdbTransaction;
  friend class NdbWorker;
  friend int compare_ndbrecord(const NdbReceiver *r1,
                      const NdbReceiver *r2,
                      const NdbRecord *key_record,
                      const NdbRecord *result_record,
                      const unsigned char *result_mask,
                      bool descending,
                      bool read_range_no);
  friend int spjTest(int argc, char** argv);

public:
  enum ReceiverType	{ NDB_UNINITIALIZED,
			  NDB_OPERATION = 1,
			  NDB_SCANRECEIVER = 2,
			  NDB_INDEX_OPERATION = 3,
                          NDB_QUERY_OPERATION = 4
  };

  NdbReceiver(Ndb *aNdb);
  int init(ReceiverType type, void* owner);
  void release();
  ~NdbReceiver();

  Uint32 getId() const{
    return m_id;
  }

  ReceiverType getType() const {
    return m_type;
  }

  inline NdbTransaction * getTransaction(ReceiverType type) const;
  void* getOwner() const {
    return m_owner;
  }

  bool checkMagicNumber() const;
  static Uint32 getMagicNumber() { return (Uint32)0x11223344; }
  Uint32 getMagicNumberFromObject() const;

  inline void next(NdbReceiver* next_arg) { m_next = next_arg;}
  inline NdbReceiver* next() { return m_next; }

  void setErrorCode(int);

  /**
   * Construct a receive buffer for a batched result set.
   * 'buffer' has to be allocated with size as calculated by
   * result_bufsize, and pointer should be Uint32 aligned.
   */
  static
  NdbReceiverBuffer* initReceiveBuffer(
                           Uint32 *buffer,
                           Uint32 bufSize,      // Size in Uint32 words
                           Uint32 batchRows);

  /**
   * Prepare for receiving of rows into specified buffer.
   * This buffer is later navigated, and retrieved from,
   * by either getNextRow() or setCurrentRow(). The row is
   * then 'unpacked' into 'row_buffer' set by do_setup_ndbrecord().
   */
  void prepareReceive(NdbReceiverBuffer *buf);

private:
  Uint32 theMagicNumber;
  Ndb* const m_ndb;
  Uint32 m_id;
  Uint32 m_tcPtrI;
  ReceiverType m_type;
  void* m_owner;
  NdbReceiver* m_next;

  /**
   * At setup
   */
  class NdbRecAttr * getValue(const class NdbColumnImpl*, char * user_dst_ptr);
  void getValues(const NdbRecord*, char*);
  void prepareSend();

  static
  void calculate_batch_size(const NdbImpl&,
                            Uint32 parallelism,
                            Uint32& batch_size,
                            Uint32& batch_byte_size);

  void calculate_batch_size(Uint32 parallelism,
                            Uint32& batch_size,
                            Uint32& batch_byte_size) const;

  /**
   * Calculate size of result buffer which has to be
   * allocated for a buffered result set, and later given to
   * initReceiveBuffer() as 'buffer' argument.
   *
   * The 'batch_rows' and 'batch_bytes' argument may have been
   * set by 'calculate_batch_size'. On return from this method
   * the 'batch_bytes' size may be capped to the max possible
   * batch size if 'batch_rows' are returned.
   */
  static
  void result_bufsize(const NdbRecord *result_record,
                      const Uint32* read_mask,
                      const NdbRecAttr *first_rec_attr,
                      Uint32 key_size,
                      bool   read_range_no,
                      bool   read_correlation,
                      Uint32 parallelism,
                      Uint32  batch_rows,    //In:     'REQ' argument to TC
                      Uint32& batch_bytes,   //In/Out: 'REQ' Argument to TC
                      Uint32& buffer_bytes); //Out:     ReceiveBuffer size

  /*
    Set up buffers for receiving TRANSID_AI and KEYINFO20 signals
    during a scan using NdbRecord.
  */
  void do_setup_ndbrecord(const NdbRecord *ndb_record,
                          char *row_buffer,
                          bool read_range_no, bool read_key_info);


  /**
   * Calculate size required for an 'unpacked' result row
   * where the current result row is stored. A buffer of this size is used
   * as 'row_buffer' argument to do_setup_ndbrecord().
   */
  static
  Uint32 ndbrecord_rowsize(const NdbRecord *ndb_record,
                           bool  read_range_no);

  int execKEYINFO20(Uint32 info, const Uint32* ptr, Uint32 len);
  int execTRANSID_AI(const Uint32* ptr, Uint32 len);
  int execTCOPCONF(Uint32 len);
  int execSCANOPCONF(Uint32 tcPtrI, Uint32 len, Uint32 rows);

  /* Assist functions to execTRANSID_AI */
  const Uint32 * handle_extra_get_values(Uint32 & save_pos,
                                         Uint32 * aLength,
                                         const Uint32 *aDataPtr,
                                         Uint32 attrSize,
                                         bool isScan,
                                         Uint32 attrId,
                                         Uint32 origLength,
                                         bool & ndbrecord_part_done);
  const Uint32 * handle_attached_rec_attrs(Uint32 attrId,
                                           const Uint32 *aDataPtr,
                                           Uint32 origLength,
                                           Uint32 attrSize,
                                           Uint32 * aLength);

  /* Convert from packed transporter to NdbRecord / RecAttr format. */
  int unpackRow(const Uint32* ptr, Uint32 len, char* row);

  /* NdbRecord describing row layout expected by API */
  const NdbRecord *m_ndb_record;

  /* The (single) current row in 'unpacked' NdbRecord format */
  char *m_row_buffer;

  /* Block of memory used to buffer all rows in a batch during scan. */
  NdbReceiverBuffer *m_recv_buffer;

  /**
   * m_read_range_no & m_read_key_info is true if we are reading
   * range / keyinfo as part of scans.
   */
  bool m_read_range_no;
  bool m_read_key_info;

  /**
   * Holds the list of RecAttr defined by getValue()
   * which to retrieve data into when a row is unpacked.
   * These RecAttr's are owner by this NdbReceiver and
   * terminated by ::release()
   */
  class NdbRecAttr* m_firstRecAttr;
  class NdbRecAttr* m_lastRecAttr; // A helper for getValue()

  /* Savepoint for unprocessed RecAttr data from current row. */
  const Uint32* m_rec_attr_data;
  Uint32        m_rec_attr_len;

  /*
    When an NdbReceiver is sitting in the NdbScanOperation::m_sent_receivers
    array, waiting to receive TRANSID_AI data from the kernel, its index into
    m_sent_receivers is stored in m_list_index, so that we can remove it when
    done without having to search for it.
  */
  Uint32 m_list_index;
  /*
    m_current_row holds the next row / key to be delivered to
    the application.
  */
  Uint32 m_current_row;

  /*
    m_expected_result_length: Total number of 32-bit words of TRANSID_AI and
    KEYINFO20 data to receive. This is set to zero until SCAN_TABCONF has
    been received.
   */
  Uint32 m_expected_result_length;
  Uint32 m_received_result_length;

  /**
   * Unpack a packed stream of field values, whose presence and nullness
   * is indicated by a leading bitmap into a list of NdbRecAttr objects
   * Return the number of words read from the input stream.
   * On failure UINT32_MAX is returned.
   */
  static
  Uint32 unpackRecAttr(NdbRecAttr**, Uint32 bmlen,
                       const Uint32* aDataPtr, Uint32 aLength);

  /**
   * Unpack a stream of field values, whose presence and nullness
   * is indicated by a leading bitmap, into an NdbRecord row.
   * Return the number of words consumed.
   */
  static
  Uint32 unpackNdbRecord(const NdbRecord *record, Uint32 bmlen,
                         const Uint32* aDataPtr,
                         char* row);

  /**
   * Handle a stream of field values, both 'READ_PACKED' and plain
   * unpacked fields, into a list of NdbRecAttr objects.
   * Return 0 on success, or -1 on error
   */
  static
  int handle_rec_attrs(NdbRecAttr* rec_attr_list,
                       const Uint32* aDataPtr,
                       Uint32 aLength);


  /**
   * Unpack data for the specified 'row' previously stored into
   * the 'buffer'. Handles both the row in NdbRecord format, and
   * the key received as KEYINFO, if present.
   */
  const char *unpackBuffer(const NdbReceiverBuffer *buffer, Uint32 row);

  /**
   * Result set is navigated either sequentially or randomly to a
   * specific row. The NdbRecord contents is then unpacked into
   * 'm_row_buffer' and returned. KeyInfo, Range no and RecAttr
   * values may be retrieved by specific calls below.
   */
  const char *getRow(const NdbReceiverBuffer* buffer, Uint32 row);
  const char *getNextRow();

  /* Fetch the NdbRecord part of current row */
  const char *getCurrentRow() const { return m_row_buffer; }

  /* get_range_no() returns the range_no for current row. */
  int get_range_no() const;

  /* Fetch keyinfo from KEYINFO20 signal for current row. */
  int get_keyinfo20(Uint32 & scaninfo, Uint32 & length,
                    const char * & data_ptr) const;

  /** Fetch RecAttr values for current row. */
  int get_AttrValues(NdbRecAttr* rec_attr_list) const;
};

#ifdef NDB_NO_DROPPED_SIGNAL
#include <stdlib.h>
#endif

inline
bool
NdbReceiver::checkMagicNumber() const {
  bool retVal = (theMagicNumber == getMagicNumber());
#ifdef NDB_NO_DROPPED_SIGNAL
  if(!retVal){
    abort();
  }
#endif
  return retVal;
}

inline
Uint32
NdbReceiver::getMagicNumberFromObject() const
{
  return theMagicNumber;
}

inline
int
NdbReceiver::execTCOPCONF(Uint32 len){
  const Uint32 tmp = m_received_result_length;
  m_expected_result_length = len;
#ifdef assert
  assert(!(tmp && !len));
#endif
  return ((bool)len ^ (bool)tmp ? 0 : 1);
}

#endif // DOXYGEN_SHOULD_SKIP_INTERNAL
#endif
