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

#ifndef NdbReceiver_H
#define NdbReceiver_H
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL  // Not part of public interface

#include <ndb_types.h>

class Ndb;
class NdbReceiver
{
  friend class Ndb;
  friend class NdbOperation;
  friend class NdbScanOperation;
  friend class NdbIndexOperation;
  friend class NdbIndexScanOperation;
  friend class NdbConnection;
public:
  enum ReceiverType	{ NDB_UNINITIALIZED,
			  NDB_OPERATION = 1,
			  NDB_SCANRECEIVER = 2,
			  NDB_INDEX_OPERATION = 3
  };
  
  NdbReceiver(Ndb *aNdb);
  void init(ReceiverType type, void* owner);
  void release();
  ~NdbReceiver();
  
  Uint32 getId(){
    return m_id;
  }

  ReceiverType getType(){
    return m_type;
  }
  
  inline NdbConnection * getTransaction();
  void* getOwner(){
    return m_owner;
  }
  
  bool checkMagicNumber() const;

  inline void next(NdbReceiver* next) { m_next = next;}
  inline NdbReceiver* next() { return m_next; }
  
private:
  Uint32 theMagicNumber;
  Ndb* m_ndb;
  Uint32 m_id;
  Uint32 m_tcPtrI;
  Uint32 m_key_info;
  ReceiverType m_type;
  void* m_owner;
  NdbReceiver* m_next;

  /**
   * At setup
   */
  class NdbRecAttr * getValue(const class NdbColumnImpl*, char * user_dst_ptr);
  void do_get_value(NdbReceiver*, Uint32 rows, Uint32 key_size);
  void prepareSend();

  int execKEYINFO20(Uint32 info, const Uint32* ptr, Uint32 len);
  int execTRANSID_AI(const Uint32* ptr, Uint32 len); 
  int execTCOPCONF(Uint32 len);
  int execSCANOPCONF(Uint32 tcPtrI, Uint32 len, Uint32 rows);
  class NdbRecAttr* theFirstRecAttr;
  class NdbRecAttr* theCurrentRecAttr;
  class NdbRecAttr** m_rows;
  
  Uint32 m_list_index; // When using multiple
  Uint32 m_current_row;
  Uint32 m_result_rows;
  Uint32 m_defined_rows;

  Uint32 m_expected_result_length;
  Uint32 m_received_result_length;
  
  bool nextResult() const { return m_current_row < m_result_rows; }
  void copyout(NdbReceiver&);
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
  m_current_row = 0;
  m_received_result_length = 0;
  m_expected_result_length = 0;
  theCurrentRecAttr = theFirstRecAttr;
}

inline
int
NdbReceiver::execTCOPCONF(Uint32 len){
  Uint32 tmp = m_received_result_length;
  m_expected_result_length = len;
  return (tmp == len ? 1 : 0);
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

#endif
#endif
