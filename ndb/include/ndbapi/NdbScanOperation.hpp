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

/*****************************************************************************
 * Name:          NdbScanOperation.hpp
 * Include:
 * Link:
 * Author:        Martin Sköld
 * Date:          2002-04-01
 * Version:       0.1
 * Description:   Table scan support
 * Documentation:
 * Adjust:  2002-04-01  Martin Sköld   First version.
 ****************************************************************************/

#ifndef NdbScanOperation_H
#define NdbScanOperation_H

#include <NdbOperation.hpp>

class NdbBlob;

/**
 * @class NdbScanOperation
 * @brief Class of scan operations for use in transactions.  
 */
class NdbScanOperation : public NdbOperation {
  friend class Ndb;
  friend class NdbConnection;
  friend class NdbResultSet;
  friend class NdbOperation;
  friend class NdbBlob;
public:
  /**
   * Type of cursor
   */
  enum CursorType {
    NoCursor = 0,
    ScanCursor = 1,
    IndexCursor = 2
  };

  /**
   * Type of cursor
   */
  CursorType get_cursor_type() const;

  /**
   * readTuples returns a NdbResultSet where tuples are stored.
   * Tuples are not stored in NdbResultSet until execute(NoCommit) 
   * has been executed and nextResult has been called.
   * 
   * @param parallel  Scan parallelism
   * @param batch No of rows to fetch from each fragment at a time
   * @param LockMode  Scan lock handling   
   * @returns NdbResultSet.
   * @note specifying 0 for batch and parallall means max performance
   */ 
  NdbResultSet* readTuples(LockMode = LM_Read, 
			   Uint32 batch = 0, Uint32 parallel = 0);
  
  inline NdbResultSet* readTuples(int parallell){
    return readTuples(LM_Read, 0, parallell);
  }
  
  inline NdbResultSet* readTuplesExclusive(int parallell = 0){
    return readTuples(LM_Exclusive, 0, parallell);
  }
  
  NdbBlob* getBlobHandle(const char* anAttrName);
  NdbBlob* getBlobHandle(Uint32 anAttrId);

protected:
  CursorType m_cursor_type;

  NdbScanOperation(Ndb* aNdb);
  virtual ~NdbScanOperation();

  int nextResult(bool fetchAllowed = true);
  virtual void release();
  
  void closeScan();
  int close_impl(class TransporterFacade*);

  // Overloaded methods from NdbCursorOperation
  int executeCursor(int ProcessorId);

  // Overloaded private methods from NdbOperation
  int init(const NdbTableImpl* tab, NdbConnection* myConnection);
  int prepareSend(Uint32  TC_ConnectPtr, Uint64  TransactionId);
  int doSend(int ProcessorId);

  virtual void setErrorCode(int aErrorCode);
  virtual void setErrorCodeAbort(int aErrorCode);

  NdbResultSet * m_resultSet;
  NdbResultSet* getResultSet();
  NdbConnection *m_transConnection;

  // Scan related variables
  Uint32 theParallelism;
  Uint32 m_keyInfo;
  NdbApiSignal* theSCAN_TABREQ;

  int getFirstATTRINFOScan();
  int doSendScan(int ProcessorId);
  int prepareSendScan(Uint32 TC_ConnectPtr, Uint64 TransactionId);
  
  int fix_receivers(Uint32 parallel);
  void reset_receivers(Uint32 parallel, Uint32 ordered);
  Uint32* m_array; // containing all arrays below
  Uint32 m_allocated_receivers;
  NdbReceiver** m_receivers;      // All receivers

  Uint32* m_prepared_receivers;   // These are to be sent
  
  Uint32 m_current_api_receiver;
  Uint32 m_api_receivers_count;
  NdbReceiver** m_api_receivers;  // These are currently used by api
  
  Uint32 m_conf_receivers_count;  // NOTE needs mutex to access
  NdbReceiver** m_conf_receivers; // receive thread puts them here
  
  Uint32 m_sent_receivers_count;  // NOTE needs mutex to access
  NdbReceiver** m_sent_receivers; // receive thread puts them here
  
  int send_next_scan(Uint32 cnt, bool close);
  void receiver_delivered(NdbReceiver*);
  void receiver_completed(NdbReceiver*);
  void execCLOSE_SCAN_REP();

  int getKeyFromKEYINFO20(Uint32* data, unsigned size);
  NdbOperation*	takeOverScanOp(OperationType opType, NdbConnection*);
  
  Uint32 m_ordered;

  int restart();
};

inline
NdbScanOperation::CursorType
NdbScanOperation::get_cursor_type() const {
  return m_cursor_type;
}

#endif
