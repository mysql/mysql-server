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
 * Name:          NdbIndexOperation.hpp
 * Include:
 * Link:
 * Author:        Martin Sköld
 * Date:          2002-04-01
 * Version:       0.1
 * Description:   Secondary index support
 * Documentation:
 * Adjust:  2002-04-01  Martin Sköld   First version.
 ****************************************************************************/

#ifndef NdbIndexOperation_H
#define NdbIndexOperation_H

#include "NdbOperation.hpp"

class Index;
class NdbResultSet;

/**
 * @class NdbIndexOperation
 * @brief Class of index operations for use in transactions
 */
class NdbIndexOperation : public NdbOperation
{
  friend class Ndb;
  friend class NdbConnection;

public:
  /**
   * @name Define Standard Operation
   * @{
   */

  /** insert is not allowed */
  int insertTuple();

  /**
   * Define the NdbIndexOperation to be a standard operation of type readTuple.
   * When calling NdbConnection::execute, this operation
   * reads a tuple.
   *
   * @return 0 if successful otherwise -1.
   */
  int readTuple(LockMode);

  /**
   * Define the NdbIndexOperation to be a standard operation of type readTuple.
   * When calling NdbConnection::execute, this operation
   * reads a tuple.
   *
   * @return 0 if successful otherwise -1.
   */
  int readTuple();

  /**
   * Define the NdbIndexOperation to be a standard operation of type
   * readTupleExclusive.
   * When calling NdbConnection::execute, this operation
   * read a tuple using an exclusive lock.
   *
   * @return 0 if successful otherwise -1.
   */
  int readTupleExclusive();

  /**
   * Define the NdbIndexOperation to be a standard operation of type simpleRead.
   * When calling NdbConnection::execute, this operation
   * reads an existing tuple (using shared read lock),
   * but releases lock immediately after read.
   *
   * @note  Using this operation twice in the same transaction
   *        may produce different results (e.g. if there is another
   *        transaction which updates the value between the
   *        simple reads).
   *
   * Note that simpleRead can read the value from any database node while
   * standard read always read the value on the database node which is
   * primary for the record.
   *
   * @return 0 if successful otherwise -1.
   */
  int simpleRead();

  /**
   * Define the NdbOperation to be a standard operation of type committedRead.
   * When calling NdbConnection::execute, this operation 
   * read latest committed value of the record.
   *
   * This means that if another transaction is updating the 
   * record, then the current transaction will not wait.  
   * It will instead use the latest committed value of the 
   * record.
   *
   * @return 0 if successful otherwise -1.
   */
  int dirtyRead();

#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
  int committedRead();
#endif

  /**
   * Define the NdbIndexOperation to be a standard operation of type 
   * updateTuple.
   *
   * When calling NdbConnection::execute, this operation
   * updates a tuple in the table.
   *
   * @return 0 if successful otherwise -1.
   */
  int updateTuple();

  /**
   * Define the NdbIndexOperation to be a standard operation of type 
   * deleteTuple.
   *
   * When calling NdbConnection::execute, this operation
   * deletes a tuple.
   *
   * @return 0 if successful otherwise -1.
   */
  int deleteTuple();

  /**
   * Define the NdbIndexOperation to be a standard operation of type 
   * dirtyUpdate.
   *
   * When calling NdbConnection::execute, this operation
   * updates without two-phase commit.
   *
   * @return 0 if successful otherwise -1.
   */
  int dirtyUpdate();

  /** @} *********************************************************************/
  /**
   * @name Define Interpreted Program Operation 
   * @{
   */

  /**
   * Update a tuple using an interpreted program.
   *
   * @return 0 if successful otherwise -1.
   */
  int interpretedUpdateTuple();

  /**
   * Delete a tuple using an interpreted program.
   *
   * @return 0 if successful otherwise -1.
   */
  int interpretedDeleteTuple();
  
  /** @} *********************************************************************/

private:
  NdbIndexOperation(Ndb* aNdb);
  ~NdbIndexOperation();

  void closeScan();

  int receiveTCINDXREF(NdbApiSignal* aSignal);

  // Overloaded method from NdbOperation
  void setLastFlag(NdbApiSignal* signal, Uint32 lastFlag);

  // Overloaded methods from NdbCursorOperation
  int executeCursor(int ProcessorId);

  // Overloaded methods from NdbCursorOperation
  int indxInit(const class NdbIndexImpl* anIndex,
	       const class NdbTableImpl* aTable, 
	       NdbConnection* myConnection);

  int equal_impl(const class NdbColumnImpl*, const char* aValue, Uint32 len);
  int prepareSend(Uint32  TC_ConnectPtr, Uint64  TransactionId);

  // Private attributes
  const NdbIndexImpl* m_theIndex;
  const NdbTableImpl* m_thePrimaryTable;
  Uint32 m_theIndexDefined[NDB_MAX_ATTRIBUTES_IN_INDEX][3];
  Uint32 m_theIndexLen;	  	 // Length of the index in words
  Uint32 m_theNoOfIndexDefined;  // The number of index attributes
  friend struct Ndb_free_list_t<NdbIndexOperation>;
};

#endif
