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

#ifndef NdbReceiver_H
#define NdbReceiver_H
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL  // Not part of public interface

#include <ndb_types.h>

class Ndb;
class NdbTransaction;
class NdbRecord;

class NdbReceiver
{
  friend class Ndb;
  friend class NdbOperation;
  friend class NdbScanOperation;
  friend class NdbIndexOperation;
  friend class NdbIndexScanOperation;
  friend class NdbTransaction;
public:
  enum ReceiverType	{ NDB_UNINITIALIZED,
			  NDB_OPERATION = 1,
			  NDB_SCANRECEIVER = 2,
			  NDB_INDEX_OPERATION = 3
  };
  
  NdbReceiver(Ndb *aNdb);
  int init(ReceiverType type, bool useRec, void* owner);
  void release();
  ~NdbReceiver();
  
  Uint32 getId(){
    return m_id;
  }

  ReceiverType getType(){
    return m_type;
  }
  
  inline NdbTransaction * getTransaction();
  void* getOwner(){
    return m_owner;
  }
  
  bool checkMagicNumber() const;

  inline void next(NdbReceiver* next_arg) { m_next = next_arg;}
  inline NdbReceiver* next() { return m_next; }
  
  void setErrorCode(int);
private:
  Uint32 theMagicNumber;
  Ndb* m_ndb;
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
  int do_get_value(NdbReceiver*, Uint32 rows, Uint32 key_size, Uint32 range);
  void prepareSend();
  void calculate_batch_size(Uint32, Uint32, Uint32&, Uint32&, Uint32&,
                            const NdbRecord *);
  /*
    Set up buffers for receiving TRANSID_AI and KEYINFO20 signals
    during a scan using NdbRecord.
  */
  void do_setup_ndbrecord(const NdbRecord *ndb_record, Uint32 batch_size,
                          Uint32 key_size, Uint32 read_range_no,
                          Uint32 rowsize, char *buf, Uint32 column_count);
  Uint32 ndbrecord_rowsize(const NdbRecord *ndb_record, Uint32 key_size,
                           Uint32 read_range_no, Uint32 extra_size);

  int execKEYINFO20(Uint32 info, const Uint32* ptr, Uint32 len);
  int execTRANSID_AI(const Uint32* ptr, Uint32 len); 
  int execTCOPCONF(Uint32 len);
  int execSCANOPCONF(Uint32 tcPtrI, Uint32 len, Uint32 rows);

  /*
    We keep different state for old NdbRecAttr based operation and for
    new NdbRecord style operation.
  */
  bool m_using_ndb_record;
  union {
    /* members used for NdbRecAttr operation. */
    struct {
      Uint32 m_hidden_count;
    } m_recattr;

    /* members used for NdbRecord operation. */
    struct {
      Uint32 m_column_count;
      const NdbRecord *m_ndb_record;
      char *m_row;
      /* Block of memory used to receive all rows in a batch during scan. */
      char *m_row_buffer;
      /*
        Offsets between two rows in m_row_buffer.
        This can be different from m_ndb_record->m_row_size, as we sometimes
        store extra information after each row (range_no and keyinfo).
        For non-scan operations, this is set to zero.
      */
      Uint32 m_row_offset;
      /*
        m_read_range_no is true if we are storing the range_no at the end of
        each row during scans.
      */
      bool m_read_range_no;
    } m_record;
  };
  class NdbRecAttr* theFirstRecAttr;
  class NdbRecAttr* theCurrentRecAttr;

  /*
    m_rows is only used in NdbRecAttr mode, but is kept during NdbRecord mode
    operation to avoid the need for re-allocation.
  */
  class NdbRecAttr** m_rows;
  
  /*
    When an NdbReceiver is sitting in the NdbScanOperation::m_sent_receivers
    array, waiting to receive TRANSID_AI data from the kernel, its index into
    m_sent_receivers is stored in m_list_index, so that we can remove it when
    done without having to search for it.
  */
  Uint32 m_list_index;
  /*
    m_current_row serves two purposes, both used during scans:

    1. While rows are being received from the kernel (and the receiver is
       sitting in the NdbScanOperation::m_sent_receivers array), it holds the
       row index (into m_rows) for the row to receive the next KEYINFO20 data.
       This is used to receive keyInfo during scans (for scans that request
       keyInfo).

    2. While rows are being delivered to the application (and the receiver is
       sitting in the NdbScanOperation::m_api_receivers array), it holds the
       next row to be delivered to the application.

    For NdbRecord operation, it works similarly, but instead indexes rows in
    the RdbRecord m_row_buffer.
  */
  Uint32 m_current_row;
  /* m_result_rows: Total number of rows contained in this batch. */
  Uint32 m_result_rows;
  /* m_defined_rows: One less that the allocated length of the m_rows array. */
  Uint32 m_defined_rows;

  /*
    m_expected_result_length: Total number of 32-bit words of TRANSID_AI and
    KEYINFO20 data to receive. This is set to zero until SCAN_TABCONF has
    been received.
   */
  Uint32 m_expected_result_length;
  Uint32 m_received_result_length;
  
  bool nextResult() const { return m_current_row < m_result_rows; }
  NdbRecAttr* copyout(NdbReceiver&);
  Uint32 receive_packed(NdbRecAttr**, Uint32 bmlen, 
                        const Uint32* aDataPtr, Uint32 aLength);
  /* get_row() returns the next available row during NdbRecord scans. */
  const char *get_row();
  /*
    peek_row() returns the row pointer that get_row() will return on next call,
    without advancing the internal pointer.
    So two successive calls to peek_row() will return the same pointer, whereas
    two successive calls to get_row would return different pointers.
  */
  const char *peek_row() const;
  /* get_range_no() returns the range_no from the last returned row. */
  int get_range_no() const;
  /* get_keyinfo20)_ returns keyinfo from KEYINFO20 signal. */
  int get_keyinfo20(Uint32 & scaninfo, Uint32 & length,
                    const char * & data_ptr) const;
  int getScanAttrData(const char * & data, Uint32 & size, Uint32 & pos) const;
};

#ifdef NDB_NO_DROPPED_SIGNAL
#include <stdlib.h>
#endif

inline
bool 
NdbReceiver::checkMagicNumber() const {
  bool retVal = (theMagicNumber == 0x11223344);
#ifdef NDB_NO_DROPPED_SIGNAL
  if(!retVal){
    abort();
  }
#endif
  return retVal;
}

inline
void
NdbReceiver::prepareSend(){
  /* Set pointers etc. to prepare for receiving the first row of the batch. */
  m_current_row = 0;
  m_received_result_length = 0;
  m_expected_result_length = 0;
  if (m_using_ndb_record)
  {
    if (m_type==NDB_SCANRECEIVER)
      m_record.m_row= m_record.m_row_buffer;
  }
  theCurrentRecAttr = theFirstRecAttr;
}

inline
int
NdbReceiver::execTCOPCONF(Uint32 len){
  Uint32 tmp = m_received_result_length;
  m_expected_result_length = len;
#ifdef assert
  assert(!(tmp && !len));
#endif
  return ((bool)len ^ (bool)tmp ? 0 : 1);
}

inline
int
NdbReceiver::execSCANOPCONF(Uint32 tcPtrI, Uint32 len, Uint32 rows){
  m_tcPtrI = tcPtrI;
  m_result_rows = rows;
  Uint32 tmp = m_received_result_length;
  m_expected_result_length = len;
  return (tmp == len ? 1 : 0);
}

inline
const char *
NdbReceiver::get_row()
{
  return m_record.m_row_buffer + m_current_row++ * m_record.m_row_offset;
}

inline
const char *
NdbReceiver::peek_row() const
{
  return m_record.m_row_buffer + m_current_row * m_record.m_row_offset;
}


#endif // DOXYGEN_SHOULD_SKIP_INTERNAL
#endif
