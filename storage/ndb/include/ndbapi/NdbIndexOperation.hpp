/*
   Copyright (c) 2003, 2015, Oracle and/or its affiliates. All rights reserved.

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
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  friend class Ndb;
  friend class NdbImpl;
  friend class NdbTransaction;
#endif

public:
  /**
   * @name Define Standard Operation
   * @{
   */

  /** insert is not allowed */
  int insertTuple();

  /**
   * Define the NdbIndexOperation to be a standard operation of type readTuple.
   * When calling NdbTransaction::execute, this operation
   * reads a tuple.
   *
   * @return 0 if successful otherwise -1.
   */
  int readTuple(LockMode);

#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
  /**
   * Define the NdbIndexOperation to be a standard operation of type readTuple.
   * When calling NdbTransaction::execute, this operation
   * reads a tuple.
   *
   * @return 0 if successful otherwise -1.
   */
  int readTuple();

  /**
   * Define the NdbIndexOperation to be a standard operation of type
   * readTupleExclusive.
   * When calling NdbTransaction::execute, this operation
   * read a tuple using an exclusive lock.
   *
   * @return 0 if successful otherwise -1.
   */
  int readTupleExclusive();

  /**
   * Define the NdbIndexOperation to be a standard operation of type simpleRead.
   * When calling NdbTransaction::execute, this operation
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
   * When calling NdbTransaction::execute, this operation 
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

  int committedRead();
#endif

  /**
   * Define the NdbIndexOperation to be a standard operation of type 
   * updateTuple.
   *
   * When calling NdbTransaction::execute, this operation
   * updates a tuple in the table.
   *
   * @return 0 if successful otherwise -1.
   */
  int updateTuple();

  /**
   * Define the NdbIndexOperation to be a standard operation of type 
   * deleteTuple.
   *
   * When calling NdbTransaction::execute, this operation
   * deletes a tuple.
   *
   * @return 0 if successful otherwise -1.
   */
  int deleteTuple();

  /**
   * Get index object for this operation
   */
  const NdbDictionary::Index * getIndex() const;

#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
  /**
   * Define the NdbIndexOperation to be a standard operation of type 
   * dirtyUpdate.
   *
   * When calling NdbTransaction::execute, this operation
   * updates without two-phase commit.
   *
   * @return 0 if successful otherwise -1.
   */
  int dirtyUpdate();
#endif

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
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
#endif
  
  /** @} *********************************************************************/

private:
  NdbIndexOperation(Ndb* aNdb);
  ~NdbIndexOperation();

  int receiveTCINDXREF(const NdbApiSignal* aSignal);

  // Overloaded methods from NdbCursorOperation
  int indxInit(const class NdbIndexImpl* anIndex,
	       const class NdbTableImpl* aTable, 
	       NdbTransaction*);

  // Private attributes
  const NdbIndexImpl* m_theIndex;
  friend struct Ndb_free_list_t<NdbIndexOperation>;
};

#endif
