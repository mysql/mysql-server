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
 * Name:          NdbResultSet.hpp
 * Include:
 * Link:
 * Author:        Martin Sköld
 * Date:          2002-04-01
 * Version:       0.1
 * Description:   Cursor class
 * Documentation:
 * Adjust:  2002-04-01  Martin Sköld   First version.
 ****************************************************************************/

#ifndef NdbResultSet_H
#define NdbResultSet_H


#include <NdbScanOperation.hpp>

/**
 * @class NdbResultSet
 * @brief NdbResultSet contains a NdbScanOperation.
 */
class NdbResultSet
{
  friend class NdbScanOperation;

public:
  
  /**
   * Get the next tuple in a scan transaction. 
   * 
   * After each call to NdbResult::nextResult
   * the buffers and NdbRecAttr objects defined in 
   * NdbOperation::getValue are updated with values 
   * from the scanned tuple. 
   *
   * @param  fetchAllowed  If set to false, then fetching is disabled
   *
   * The NDB API will contact the NDB Kernel for more tuples 
   * when necessary to do so unless you set the fetchAllowed 
   * to false. 
   * This will force NDB to process any records it
   * already has in it's caches. When there are no more cached 
   * records it will return 2. You must then call nextResult
   * with fetchAllowed = true in order to contact NDB for more 
   * records.
   *
   * fetchAllowed = false is useful when you want to update or 
   * delete all the records fetched in one transaction(This will save a
   *  lot of round trip time and make updates or deletes of scanned 
   * records a lot faster). 
   * While nextResult(false)
   * returns 0 take over the record to another transaction. When 
   * nextResult(false) returns 2 you must execute and commit the other 
   * transaction. This will cause the locks to be transferred to the 
   * other transaction, updates or deletes will be made and then the 
   * locks will be released.
   * After that, call nextResult(true) which will fetch new records and
   * cache them in the NdbApi. 
   * 
   * @note  If you don't take over the records to another transaction the 
   *        locks on those records will be released the next time NDB Kernel
   *        is contacted for more records.
   *
   * @note  Please contact for examples of efficient scan
   *        updates and deletes.
   * 
   * @note  See ndb/examples/ndbapi_scan_example for usage.
   *
   * @return 
   * -  -1: if unsuccessful,<br>
   * -   0: if another tuple was received, and<br> 
   * -   1: if there are no more tuples to scan.
   * -   2: if there are no more cached records in NdbApi
   */
  int nextResult(bool fetchAllowed = true);

  /**
   * Close result set (scan)
   */
  void close();

  /**
   * Restart
   */
  int restart();
  
  /**
   * Transfer scan operation to an updating transaction. Use this function 
   * when a scan has found a record that you want to update. 
   * 1. Start a new transaction.
   * 2. Call the function takeOverForUpdate using your new transaction 
   *    as parameter, all the properties of the found record will be copied 
   *    to the new transaction.
   * 3. When you execute the new transaction, the lock held by the scan will 
   *    be transferred to the new transaction(it's taken over).
   *
   * @note You must have started the scan with openScanExclusive
   *       to be able to update the found tuple.
   *
   * @param updateTrans the update transaction connection.
   * @return an NdbOperation or NULL.
   */
  NdbOperation* updateTuple();
  NdbOperation*	updateTuple(NdbConnection* updateTrans);

  /**
   * Transfer scan operation to a deleting transaction. Use this function 
   * when a scan has found a record that you want to delete. 
   * 1. Start a new transaction.
   * 2. Call the function takeOverForDelete using your new transaction 
   *    as parameter, all the properties of the found record will be copied 
   *    to the new transaction.
   * 3. When you execute the new transaction, the lock held by the scan will 
   *    be transferred to the new transaction(its taken over).
   *
   * @note You must have started the scan with openScanExclusive
   *       to be able to delete the found tuple.
   *
   * @param deleteTrans the delete transaction connection.
   * @return an NdbOperation or NULL.
   */
  int deleteTuple();
  int deleteTuple(NdbConnection* takeOverTransaction);
  
private:
  NdbResultSet(NdbScanOperation*);

  ~NdbResultSet();

  void init();

  NdbScanOperation* m_operation;
};

#endif
