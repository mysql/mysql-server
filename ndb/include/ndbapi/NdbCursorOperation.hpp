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

#ifndef NdbCursorOperation_H
#define NdbCursorOperation_H

#include <NdbOperation.hpp>

class NdbResultSet;

/**
 * @class  NdbCursorOperation
 * @brief  Operation using cursors
 */
class NdbCursorOperation : public NdbOperation
{
  friend class NdbResultSet;
  friend class NdbConnection;

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
   * Lock when performing scan
   */
  enum LockMode {
    LM_Read = 0,
    LM_Exclusive = 1,
    LM_CommittedRead = 2,
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    LM_Dirty = 2
#endif
  };
  
  virtual CursorType cursorType() = 0;

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
				   LockMode = LM_Read ) = 0;
  
  inline NdbResultSet* readTuplesExclusive(int parallell = 0){
    return readTuples(parallell, LM_Exclusive);
  }

protected:
  NdbCursorOperation(Ndb* aNdb);	

  ~NdbCursorOperation();

  void cursInit();

  virtual int executeCursor(int ProcessorId) = 0;

  NdbResultSet* getResultSet();
  NdbResultSet* m_resultSet;

private:

  virtual int nextResult(bool fetchAllowed) = 0;
  
  virtual void closeScan() = 0;
};


#endif
