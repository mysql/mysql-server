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
#include <NdbCursorOperation.hpp>

class NdbBlob;

/**
 * @class NdbScanOperation
 * @brief Class of scan operations for use in transactions.  
 */
class NdbScanOperation : public NdbCursorOperation
{
  friend class Ndb;
  friend class NdbConnection;
  friend class NdbResultSet;
  friend class NdbOperation;

public:
  /**
   * readTuples returns a NdbResultSet where tuples are stored.
   * Tuples are not stored in NdbResultSet until execute(NoCommit) 
   * has been executed and nextResult has been called.
   * 
   * @param parallel  Scan parallelism
   * @param LockMode  Scan lock handling   
   * @returns NdbResultSet.
   */ 
  virtual NdbResultSet* readTuples(unsigned parallel = 0, 
				   LockMode = LM_Read );
  
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL

  int updateTuples();
  int updateTuples(Uint32 parallelism);

  int deleteTuples();
  int deleteTuples(Uint32 parallelism);

  // Overload setValue for updateTuples
  int setValue(const char* anAttrName, const char* aValue, Uint32 len = 0);
  int setValue(const char* anAttrName, Int32 aValue);
  int setValue(const char* anAttrName, Uint32 aValue);
  int setValue(const char* anAttrName, Int64 aValue);
  int setValue(const char* anAttrName, Uint64 aValue);
  int setValue(const char* anAttrName, float aValue);
  int setValue(const char* anAttrName, double aValue);

  int setValue(Uint32 anAttrId, const char* aValue, Uint32 len = 0);
  int setValue(Uint32 anAttrId, Int32 aValue);
  int setValue(Uint32 anAttrId, Uint32 aValue);
  int setValue(Uint32 anAttrId, Int64 aValue);
  int setValue(Uint32 anAttrId, Uint64 aValue);
  int setValue(Uint32 anAttrId, float aValue);
  int setValue(Uint32 anAttrId, double aValue);
#endif

  NdbBlob* getBlobHandle(const char* anAttrName);
  NdbBlob* getBlobHandle(Uint32 anAttrId);

private:
  NdbScanOperation(Ndb* aNdb);

  ~NdbScanOperation();

  NdbCursorOperation::CursorType cursorType();

  virtual int nextResult(bool fetchAllowed = true);
  virtual void release();
  
  void closeScan();

  // Overloaded methods from NdbCursorOperation
  int executeCursor(int ProcessorId);

  // Overloaded private methods from NdbOperation
  int init(NdbTableImpl* tab, NdbConnection* myConnection);
  int prepareSend(Uint32  TC_ConnectPtr, Uint64  TransactionId);
  int doSend(int ProcessorId);

  virtual void setErrorCode(int aErrorCode);
  virtual void setErrorCodeAbort(int aErrorCode);

  virtual int equal_impl(const NdbColumnImpl* anAttrObject, 
                         const char* aValue, 
                         Uint32 len);
private:
  NdbConnection *m_transConnection;
  bool m_autoExecute;
  bool m_updateOp;
  bool m_writeOp;
  bool m_deleteOp;
  class SetValueRecList* m_setValueList;
};

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
class AttrInfo;
class SetValueRecList;

class SetValueRec {
  friend class SetValueRecList;
public:
  SetValueRec();
  ~SetValueRec();

  enum SetValueType {
    SET_STRING_ATTR1 = 0,
    SET_INT32_ATTR1  = 1,
    SET_UINT32_ATTR1 = 2,
    SET_INT64_ATTR1  = 3,
    SET_UINT64_ATTR1 = 4,
    SET_FLOAT_ATTR1  = 5,
    SET_DOUBLE_ATTR1 = 6,
    SET_STRING_ATTR2 = 7,
    SET_INT32_ATTR2  = 8,
    SET_UINT32_ATTR2 = 9,
    SET_INT64_ATTR2  = 10,
    SET_UINT64_ATTR2 = 11,
    SET_FLOAT_ATTR2  = 12,
    SET_DOUBLE_ATTR2 = 13
  };

  SetValueType stype;
  union {
    char* anAttrName;
    Uint32 anAttrId;
  };
  struct String {
    char* aStringValue;
    Uint32 len;
  };
  union {
    String stringStruct;
    Int32 anInt32Value;
    Uint32 anUint32Value;
    Int64 anInt64Value;
    Uint64 anUint64Value;
    float aFloatValue;
    double aDoubleValue;
  };
private:
  SetValueRec* next;
};

inline 
SetValueRec::SetValueRec() :
  next(0) 
{
}

class SetValueRecList {
public:
  SetValueRecList();
  ~SetValueRecList();

  void add(const char* anAttrName, const char* aValue, Uint32 len = 0);
  void add(const char* anAttrName, Int32 aValue);
  void add(const char* anAttrName, Uint32 aValue);
  void add(const char* anAttrName, Int64 aValue);
  void add(const char* anAttrName, Uint64 aValue);
  void add(const char* anAttrName, float aValue);
  void add(const char* anAttrName, double aValue);
  void add(Uint32 anAttrId, const char* aValue, Uint32 len = 0);
  void add(Uint32 anAttrId, Int32 aValue);
  void add(Uint32 anAttrId, Uint32 aValue);
  void add(Uint32 anAttrId, Int64 aValue);
  void add(Uint32 anAttrId, Uint64 aValue);
  void add(Uint32 anAttrId, float aValue);
  void add(Uint32 anAttrId, double aValue);

  typedef void(* IterateFn)(SetValueRec&, NdbOperation&);
  static void callSetValueFn(SetValueRec&, NdbOperation&);
  void iterate(IterateFn nextfn, NdbOperation&);
private:
  SetValueRec* first;    
  SetValueRec* last;    
};

inline
SetValueRecList::SetValueRecList() : 
  first(0),
  last(0) 
{
}
  
inline
SetValueRecList::~SetValueRecList() {
  if (first) delete first;
  first = last = 0;
}


inline
void SetValueRecList::iterate(SetValueRecList::IterateFn nextfn, NdbOperation& oper) 
{
  SetValueRec* recPtr = first;
  while(recPtr) {
    (*nextfn)(*recPtr, oper);
    recPtr = recPtr->next; // Move to next in list - MASV
  }
}

#endif

#endif
