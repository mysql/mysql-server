/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.
    Use is subject to license terms.

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

#ifndef NDBT_DATA_SET_TRANSACTION_HPP
#define NDBT_DATA_SET_TRANSACTION_HPP

#include "NDBT_Table.hpp"
#include <NdbApi.hpp>

class NDBT_DataSet;

/**
 * This class contains a bunch a methods
 * that operates on NDB together with a NDBT_DataSet
 * using <b>one</b> transaction
 */
class NDBT_DataSetTransaction {
public:
  /**
   * Store the data into ndb
   */
  static void insert(Ndb * theNdbObject,
		     const NDBT_DataSet *,
		     bool rollback = false);
  
  /**
   * Read data (using pk) from ndb
   */
  static void readByPk(Ndb * theNdbObject,
		       NDBT_DataSet *,
		       bool rollback = false);
  
  /**
   * Update data using pk
   *
   */
  static void updateByPk(Ndb * theNdbObject,
			 const NDBT_DataSet *,
			 bool rollback = false);

  /**
   * Delete 
   */
  static void deleteByPk(Ndb * theNdbObject,
			 const NDBT_DataSet *,
			 bool rollback = false);
};

class NDBT_DataSetAsyncTransaction {
public:
  enum OperationType {
    OT_Insert,
    OT_ReadByPk,
    OT_UpdateByPk,
    OT_DeleteByPk
  };

  /**
   * A callback for the NDBT_DataSetAsyncTransaction 
   * interface.
   *
   * The callback method returns:
   * - the operation performed
   * - the data set
   * - if the transaction was committed or aborted
   */
  typedef (* NDBT_DataSetAsyncTransactionCallback)(OperationType,
						   const NDBT_DataSet *,
						   bool commit,
						   void * anyObject);
  /**
   * Store the data into ndb
   */
  static void insert(Ndb * theNdbObject,
		     const NDBT_DataSet *,
		     bool rollback = false,
		     NDBT_DataSetAsyncTransactionCallback fun,
		     void * anyObject);
  
  /**
   * Read data (using pk) from ndb
   */
  static void readByPk(Ndb * theNdbObject,
		       NDBT_DataSet *,
		       bool rollback = false,
		       NDBT_DataSetAsyncTransactionCallback fun,
		       void * anyObject);

  
  /**
   * Update data using pk
   *
   */
  static void updateByPk(Ndb * theNdbObject,
			 const NDBT_DataSet *,
			 bool rollback = false,
			 NDBT_DataSetAsyncTransactionCallback fun,
			 void * anyObject);


  /**
   * Delete 
   */
  static void deleteByPk(Ndb * theNdbObject,
			 const NDBT_DataSet *,
			 bool rollback = false,
			 NDBT_DataSetAsyncTransactionCallback fun,
			 void * anyObject);
  
};

class NDBT_DataSetBulkOperation {
public:
  /**
   * Store the data into ndb
   */
  static void insert(Ndb * theNdbObject,
		     const NDBT_DataSet *,
		     int parallellTransactions = 1,
		     int operationsPerTransaction = 10);
  
  /**
   * Read data (using pk) from ndb
   */
  static void readByPk(Ndb * theNdbObject,
		       NDBT_DataSet *,
		       int parallellTransactions = 1,
		       int operationsPerTransaction = 10);
  
  /**
   * Update data using pk
   *
   */
  static void updateByPk(Ndb * theNdbObject,
			 const NDBT_DataSet *,
			 int parallellTransactions = 1,
			 int operationsPerTransaction = 10);

  /**
   * Delete 
   */
  static void deleteByPk(Ndb * theNdbObject,
			 const NDBT_DataSet *,
			 int parallellTransactions = 1,
			 int operationsPerTransaction = 10);
};


#endif
