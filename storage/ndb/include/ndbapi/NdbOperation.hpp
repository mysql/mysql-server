/*
   Copyright (c) 2003, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NdbOperation_H
#define NdbOperation_H

#include <ndb_types.h>
#include "ndbapi_limits.h"
#include "NdbError.hpp"
#include "NdbReceiver.hpp"
#include "NdbDictionary.hpp"
#include "Ndb.hpp"

class Ndb;
class NdbApiSignal;
class NdbRecAttr;
class NdbOperation;
class NdbTransaction;
class NdbColumnImpl;
class NdbBlob;
class TcKeyReq;
class NdbRecord;
class NdbInterpretedCode;
struct GenericSectionPtr;
class NdbLockHandle;

/**
 * @class NdbOperation
 * @brief Class of operations for use in transactions.  
 */
class NdbOperation
{
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  friend class Ndb;
  friend class NdbImpl;
  friend class NdbTransaction;
  friend class NdbScanOperation;
  friend class NdbScanReceiver;
  friend class NdbScanFilter;
  friend class NdbScanFilterImpl;
  friend class NdbReceiver;
  friend class NdbBlob;
#endif

public:
  /** 
   * @name Define Standard Operation Type
   * @{
   */

  /**
   * Different access types (supported by sub-classes of NdbOperation)
   */

  enum Type {
    PrimaryKeyAccess     ///< Read, insert, update, or delete using pk
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    = 0                  // NdbOperation
#endif
    ,UniqueIndexAccess   ///< Read, update, or delete using unique index
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    = 1                  // NdbIndexOperation
#endif
    ,TableScan          ///< Full table scan
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    = 2                  // NdbScanOperation
#endif
    ,OrderedIndexScan   ///< Ordered index scan
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    = 3                  // NdbIndexScanOperation
#endif
  };
  
  /**
   * Lock when performing read
   */

  enum LockMode {
    LM_Read                 ///< Read with shared lock
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    = 0
#endif
    ,LM_Exclusive           ///< Read with exclusive lock
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    = 1
#endif
    ,LM_CommittedRead       ///< Ignore locks, read last committed value
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    = 2,
    LM_Dirty = 2,
#endif
    LM_SimpleRead = 3       ///< Read with shared lock, but release lock directly
  };

  /**
   * How should transaction be handled if operation fails.
   *
   * If AO_IgnoreError, a failure in one operation will not abort the
   * transaction, and NdbTransaction::execute() will return 0 (success). Use
   * NdbOperation::getNdbError() to check for errors from individual
   * operations.
   *
   * If AbortOnError, a failure in one operation will abort the transaction
   * and cause NdbTransaction::execute() to return -1.
   * 
   * Abort option can be set on execute(), or in the individual operation.
   * Setting AO_IgnoreError or AbortOnError in execute() overrides the settings
   * on individual operations. Setting DefaultAbortOption in execute() (the
   * default) causes individual operation settings to be used.
   *
   * For READ, default is AO_IgnoreError
   *     DML,  default is AbortOnError
   * CommittedRead does _only_ support AO_IgnoreError
   */
  enum AbortOption {
    DefaultAbortOption = -1,///< Use default as specified by op-type
    AbortOnError = 0,       ///< Abort transaction on failed operation
    AO_IgnoreError = 2      ///< Transaction continues on failed operation
  };

  /**
   * Define the NdbOperation to be a standard operation of type insertTuple.
   * When calling NdbTransaction::execute, this operation 
   * adds a new tuple to the table.
   *
   * @return 0 if successful otherwise -1.
   */		
  virtual int 			insertTuple();
		
  /**
   * Define the NdbOperation to be a standard operation of type updateTuple.
   * When calling NdbTransaction::execute, this operation 
   * updates a tuple in the table.
   *
   * @return 0 if successful otherwise -1.
   */  
  virtual int 			updateTuple();

  /**
   * Define the NdbOperation to be a standard operation of type writeTuple.
   * When calling NdbTransaction::execute, this operation 
   * writes a tuple to the table.
   * If the tuple exists, it updates it, otherwise an insert takes place.
   *
   * @return 0 if successful otherwise -1.
   */  
  virtual int 			writeTuple();

  /**
   * Define the NdbOperation to be a standard operation of type deleteTuple.
   * When calling NdbTransaction::execute, this operation 
   * delete a tuple.
   *
   * @return 0 if successful otherwise -1.
   */
  virtual int 			deleteTuple();
		
  /**
   * Define the NdbOperation to be a standard operation of type readTuple.
   * When calling NdbTransaction::execute, this operation 
   * reads a tuple.
   *
   * @return 0 if successful otherwise -1.
   */
  virtual int 			readTuple(LockMode);

#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
  /**
   * Define the NdbOperation to be a standard operation of type readTuple.
   * When calling NdbTransaction::execute, this operation 
   * reads a tuple.
   *
   * @return 0 if successful otherwise -1.
   */  
  virtual int 			readTuple();				

  /**
   * Define the NdbOperation to be a standard operation of type 
   * readTupleExclusive.
   * When calling NdbTransaction::execute, this operation 
   * read a tuple using an exclusive lock.
   *
   * @return 0 if successful otherwise -1.
   */
  virtual int 			readTupleExclusive();

  /**
   * Define the NdbOperation to be a standard operation of type 
   * simpleRead.
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
  virtual int			simpleRead();

  /**
   * Define the NdbOperation to be a standard operation of type committedRead.
   * When calling NdbTransaction::execute, this operation 
   * read latest committed value of the record.
   *
   * This means that if another transaction is updating the 
   * record, then the current transaction will not wait.  
   * It will instead use the latest committed value of the 
   * record.
   * dirtyRead is a deprecated name for committedRead
   *
   * @return 0 if successful otherwise -1.
   * @deprecated
   */
  virtual int			dirtyRead();

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
  virtual int			committedRead();

  /**
   * Define the NdbOperation to be a standard operation of type dirtyUpdate.
   * When calling NdbTransaction::execute, this operation 
   * updates without two-phase commit.
   *
   * @return 0 if successful otherwise -1.
   */
  virtual int			dirtyUpdate();

  /**
   * Define the NdbOperation to be a standard operation of type dirtyWrite.
   * When calling NdbTransaction::execute, this operation 
   * writes without two-phase commit.
   *
   * @return 0 if successful otherwise -1.
   */
  virtual int			dirtyWrite();
#endif

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  /** @} *********************************************************************/
  /** 
   * @name Define Interpreted Program Operation Type
   * @{
   */

  /**
   * Update a tuple using an interpreted program.
   *
   * @return 0 if successful otherwise -1.
   */  
  virtual int			interpretedUpdateTuple();
		
  /**
   * Delete a tuple using an interpreted program.
   *
   * @return 0 if successful otherwise -1.
   */
  virtual int			interpretedDeleteTuple();
#endif

  /** @} *********************************************************************/

  /** 
   * @name Specify Search Conditions
   * @{
   */
  /**
   * Define a search condition with equality.
   * The condition is true if the attribute has the given value.
   * To set search conditions on multiple attributes,
   * use several equals (then all of them must be satisfied for the
   * tuple to be selected).
   *
   * @note For insertTuple() it is also allowed to define the
   *       search key by using setValue().
   *
   * @note There are 10 versions of equal() with
   *       slightly different parameters.
   *
   * @note  If attribute has fixed size, value must include all bytes.
   *        In particular a Char must be native-blank padded.
   *        If attribute has variable size, value must start with
   *        1 or 2 little-endian length bytes (2 if Long*).
   * 
   * @param   anAttrName   Attribute name 
   * @param   aValue       Attribute value.
   * @return               -1 if unsuccessful. 
   */
#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
  int  equal(const char* anAttrName, const char* aValue, Uint32 len);
#endif
  int  equal(const char* anAttrName, const char* aValue);
  int  equal(const char* anAttrName, Int32 aValue);	
  int  equal(const char* anAttrName, Uint32 aValue);	
  int  equal(const char* anAttrName, Int64 aValue);	
  int  equal(const char* anAttrName, Uint64 aValue);
#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
  int  equal(Uint32 anAttrId, const char* aValue, Uint32 len);
#endif
  int  equal(Uint32 anAttrId, const char* aValue);
  int  equal(Uint32 anAttrId, Int32 aValue);	
  int  equal(Uint32 anAttrId, Uint32 aValue);	
  int  equal(Uint32 anAttrId, Int64 aValue);	
  int  equal(Uint32 anAttrId, Uint64 aValue);
	
  /** @} *********************************************************************/
  /** 
   * @name Specify Attribute Actions for Operations
   * @{
   */

  /**
   * Defines a retrieval operation of an attribute value.
   * The NDB API allocate memory for the NdbRecAttr object that
   * will hold the returned attribute value. 
   *
   * @note Note that it is the applications responsibility
   *       to allocate enough memory for aValue (if non-NULL).
   *       The buffer aValue supplied by the application must be
   *       aligned appropriately.  The buffer is used directly
   *       (avoiding a copy penalty) only if it is aligned on a
   *       4-byte boundary and the attribute size in bytes
   *       (i.e. NdbRecAttr::attrSize times NdbRecAttr::arraySize is
   *       a multiple of 4).
   *
   * @note There are two versions of NdbOperation::getValue with
   *       slightly different parameters.
   *
   * @note This method does not fetch the attribute value from 
   *       the database!  The NdbRecAttr object returned by this method 
   *       is <em>not</em> readable/printable before the 
   *       transaction has been executed with NdbTransaction::execute.
   *
   * @param anAttrName  Attribute name 
   * @param aValue      If this is non-NULL, then the attribute value 
   *                    will be returned in this parameter.<br>
   *                    If NULL, then the attribute value will only 
   *                    be stored in the returned NdbRecAttr object.
   * @return            An NdbRecAttr object to hold the value of 
   *                    the attribute, or a NULL pointer 
   *                    (indicating error).
   */
  NdbRecAttr* getValue(const char* anAttrName, char* aValue = 0);
  NdbRecAttr* getValue(Uint32 anAttrId, char* aValue = 0);
  NdbRecAttr* getValue(const NdbDictionary::Column*, char* val = 0);
  
  /**
   * Define an attribute to set or update in query.
   *
   * To set a NULL value, use the following construct:
   * @code
   *   setValue("ATTR_NAME", (char*)NULL);
   * @endcode
   * 
   * There are a number of NdbOperation::setValue methods that 
   * take a certain type as input
   * (pass by value rather than passing a pointer). 
   * As the interface is currently implemented it is the responsibility 
   * of the application programmer to use the correct types.
   *
   * The NDB API will however check that the application sends
   * a correct length to the interface as given in the length parameter.  
   * The passing of char* as the value can contain any type or 
   * any type of array. 
   * If length is not provided or set to zero, 
   * then the API will assume that the pointer
   * is correct and not bother with checking it.
   *
   * @note For insertTuple() the NDB API will automatically detect that 
   *       it is supposed to use equal() instead. 
   *
   * @note For insertTuple() it is not necessary to use
   *       setValue() on key attributes before other attributes.
   *
   * @note There are 14 versions of NdbOperation::setValue with
   *       slightly different parameters.
   *
   * @note See note under equal() about value format and length.
   * 
   * @param anAttrName     Name (or Id) of attribute.
   * @param aValue         Attribute value to set.
   * @return               -1 if unsuccessful.
   */
#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
  int  setValue(const char* anAttrName, const char* aValue, Uint32 len);
#endif
  int  setValue(const char* anAttrName, const char* aValue);
  int  setValue(const char* anAttrName, Int32 aValue);
  int  setValue(const char* anAttrName, Uint32 aValue);
  int  setValue(const char* anAttrName, Int64 aValue);
  int  setValue(const char* anAttrName, Uint64 aValue);
  int  setValue(const char* anAttrName, float aValue);
  int  setValue(const char* anAttrName, double aValue);
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  int  setAnyValue(Uint32 aValue);
  int  setOptimize(Uint32 options);
#endif

#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
  int  setValue(Uint32 anAttrId, const char* aValue, Uint32 len);
#endif
  int  setValue(Uint32 anAttrId, const char* aValue);
  int  setValue(Uint32 anAttrId, Int32 aValue);
  int  setValue(Uint32 anAttrId, Uint32 aValue);
  int  setValue(Uint32 anAttrId, Int64 aValue);
  int  setValue(Uint32 anAttrId, Uint64 aValue);
  int  setValue(Uint32 anAttrId, float aValue);
  int  setValue(Uint32 anAttrId, double aValue);

  /**
   * This method replaces getValue/setValue for blobs.  It creates
   * a blob handle NdbBlob.  A second call with same argument returns
   * the previously created handle.  The handle is linked to the
   * operation and is maintained automatically.
   *
   * See NdbBlob for details.
   *
   * For NdbRecord operation, this method can be used to fetch the blob
   * handle for an NdbRecord operation that references the blob, but extra
   * blob columns can not be added with this call (it will return 0).
   *
   * For reading with NdbRecord, the NdbRecord entry for each blob must
   * reserve space in the row for sizeof(NdbBlob *). The blob handle
   * will be stored there, providing an alternative way of obtaining the
   * blob handle.
   */
  virtual NdbBlob* getBlobHandle(const char* anAttrName);
  virtual NdbBlob* getBlobHandle(Uint32 anAttrId);
  virtual NdbBlob* getBlobHandle(const char* anAttrName) const;
  virtual NdbBlob* getBlobHandle(Uint32 anAttrId) const;
 
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  /** @} *********************************************************************/
  /** 
   * @name Specify Interpreted Program Instructions
   * @{
   */

  /**
   * Interpreted program instruction: Add a value to an attribute.
   *
   * @note Destroys the contents of registers 6 and 7.
   *       (The instruction uses these registers for its operation.)
   *
   * @note There are four versions of NdbOperation::incValue with
   *       slightly different parameters.
   * 
   * @note For Scans and NdbRecord operations, use the 
   *       NdbInterpretedCode interface.
   *
   * @param anAttrName     Attribute name.
   * @param aValue         Value to add.
   * @return               -1 if unsuccessful.
   */
  int   incValue(const char* anAttrName, Uint32 aValue);
  int   incValue(const char* anAttrName, Uint64 aValue);
  int   incValue(Uint32 anAttrId, Uint32 aValue);
  int   incValue(Uint32 anAttrId, Uint64 aValue);

  /**
   * Interpreted program instruction:
   * Subtract a value from an attribute in an interpreted operation.
   *
   * @note Destroys the contents of registers 6 and 7.
   *       (The instruction uses these registers for its operation.)
   *
   * @note There are four versions of NdbOperation::subValue with
   *       slightly different parameters.
   * 
   * @note For Scans and NdbRecord operations, use the 
   *       NdbInterpretedCode interface.
   *
   * @param anAttrName    Attribute name.
   * @param aValue        Value to subtract.
   * @return              -1 if unsuccessful.
   */
  int   subValue(const char* anAttrName, Uint32 aValue);
  int   subValue(const char* anAttrName, Uint64 aValue);
  int   subValue(Uint32 anAttrId, Uint32 aValue);
  int   subValue(Uint32 anAttrId, Uint64 aValue);

  /**
   * Interpreted program instruction:
   * Define a jump label in an interpreted operation.
   *
   * @note The labels are automatically numbered starting with 0.  
   *       The parameter used by NdbOperation::def_label should 
   *       match the automatic numbering to make it easier to 
   *       debug the interpreted program.
   * 
   * @note For Scans and NdbRecord operations, use the 
   *       NdbInterpretedCode interface.
   * 
   * @param labelNumber   Label number.
   * @return              Label number, -1 if unsuccessful.
   */
  int   def_label(int labelNumber);

  /**
   * Interpreted program instruction:
   * Add two registers into a third.
   * 
   * @note For Scans and NdbRecord operations, use the 
   *       NdbInterpretedCode interface.
   *
   * @param RegSource1   First register.
   * @param RegSource2   Second register.
   * @param RegDest      Destination register where the result will be stored.
   * @return -1 if unsuccessful.
   */
  int   add_reg(Uint32 RegSource1, Uint32 RegSource2, Uint32 RegDest);

  /**
   * Interpreted program instruction:
   * Substract RegSource2 from RegSource1 and put the result in RegDest.
   * 
   * @note For Scans and NdbRecord operations, use the 
   *       NdbInterpretedCode interface.
   *
   * @param RegSource1   First register.
   * @param RegSource2   Second register.
   * @param RegDest      Destination register where the result will be stored.
   * @return             -1 if unsuccessful.
   */
  int   sub_reg(Uint32 RegSource1, Uint32 RegSource2, Uint32 RegDest);

  /**
   * Interpreted program instruction:
   * Load a constant into a register.
   * 
   * @note For Scans and NdbRecord operations, use the 
   *       NdbInterpretedCode interface.
   *
   * @param RegDest      Destination register.
   * @param Constant     Value to load.
   * @return             -1 if unsuccessful.
   */
  int   load_const_u32(Uint32 RegDest, Uint32 Constant);
  int   load_const_u64(Uint32 RegDest, Uint64 Constant);

  /**
   * Interpreted program instruction:
   * Load NULL value into a register.
   * 
   * @note For Scans and NdbRecord operations, use the 
   *       NdbInterpretedCode interface.
   *
   * @param RegDest      Destination register.
   * @return             -1 if unsuccessful.
   */ 
  int   load_const_null(Uint32 RegDest);

  /**
   * Interpreted program instruction:
   * Read an attribute into a register.
   * 
   * @note For Scans and NdbRecord operations, use the 
   *       NdbInterpretedCode interface.
   *
   * @param anAttrName   Attribute name.
   * @param RegDest      Destination register.
   * @return             -1 if unsuccessful.
   */
  int   read_attr(const char* anAttrName, Uint32 RegDest);

  /**
   * Interpreted program instruction:
   * Write an attribute from a register. 
   * 
   * @note For Scans and NdbRecord operations, use the 
   *       NdbInterpretedCode interface.
   *
   * @param anAttrName   Attribute name.
   * @param RegSource    Source register.
   * @return             -1 if unsuccessful.
   */
  int   write_attr(const char* anAttrName, Uint32 RegSource);

  /**
   * Interpreted program instruction:
   * Read an attribute into a register.
   * 
   * @note For Scans and NdbRecord operations, use the 
   *       NdbInterpretedCode interface.
   *
   * @param anAttrId the attribute id.
   * @param RegDest the destination register.
   * @return -1 if unsuccessful.
   */
  int   read_attr(Uint32 anAttrId, Uint32 RegDest);

  /**
   * Interpreted program instruction:
   * Write an attribute from a register. 
   * 
   * @note For Scans and NdbRecord operations, use the 
   *       NdbInterpretedCode interface.
   *
   * @param anAttrId the attribute id.
   * @param RegSource the source register.
   * @return -1 if unsuccessful.
   */
  int   write_attr(Uint32 anAttrId, Uint32 RegSource);

  /**
   * Interpreted program instruction:
   * Define a search condition. Last two letters in the function name 
   * describes the search condition.
   * The condition compares RegR with RegL and therefore appears
   * to be reversed.
   *
   * - ge RegR >= RegL
   * - gt RegR >  RegL
   * - le RegR <= RegL
   * - lt RegR <  RegL
   * - eq RegR =  RegL
   * - ne RegR <> RegL
   * 
   * @note For Scans and NdbRecord operations, use the 
   *       NdbInterpretedCode interface.
   *
   * @param RegLvalue left value. 
   * @param RegRvalue right value.
   * @param Label the label to jump to.
   * @return -1 if unsuccessful.
   */
  int   branch_ge(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label);
  int   branch_gt(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label);
  int   branch_le(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label);
  int   branch_lt(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label);
  int   branch_eq(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label);
  int   branch_ne(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label);

  /**
   * Interpreted program instruction:
   * Jump to Label if RegLvalue is not NULL.
   * 
   * @note For Scans and NdbRecord operations, use the 
   *       NdbInterpretedCode interface.
   *
   * @param RegLvalue the value to check.
   * @param Label the label to jump to.
   * @return -1 if unsuccessful. 
   */
  int   branch_ne_null(Uint32 RegLvalue, Uint32 Label);

  /**
   * Interpreted program instruction:
   * Jump to Label if RegLvalue is equal to NULL.
   * 
   * @note For Scans and NdbRecord operations, use the 
   *       NdbInterpretedCode interface.
   *
   * @param  RegLvalue  Value to check.
   * @param  Label      Label to jump to.
   * @return -1 if unsuccessful. 
   */
  int   branch_eq_null(Uint32 RegLvalue, Uint32 Label);

  /**
   * Interpreted program instruction:
   * Jump to Label.
   * 
   * @note For Scans and NdbRecord operations, use the 
   *       NdbInterpretedCode interface.
   *
   * @param  Label  Label to jump to.
   * @return -1 if unsuccessful.
   */
  int   branch_label(Uint32 Label);

  /**
   * Interpreted program instruction:  branch after memcmp
   * 
   * @note For Scans and NdbRecord operations, use the 
   *       NdbInterpretedCode interface.
   *
   * @param  ColId   Column to check
   * @param  Label   Label to jump to
   * @return -1 if unsuccessful
   */
  int branch_col_eq_null(Uint32 ColId, Uint32 Label);
  int branch_col_ne_null(Uint32 ColId, Uint32 Label);

  /**
   * Interpreted program instruction:  branch after memcmp
   * 
   * @note For Scans and NdbRecord operations, use the 
   *       NdbInterpretedCode interface.
   *
   * @param  ColId   column to check
   * @param  val     search value
   * @param  len     length of search value   
   * @param  nopad   force non-padded comparison for a Char column
   * @param  Label   label to jump to
   * @return -1 if unsuccessful
   */
  int branch_col_eq(Uint32 ColId, const void * val, Uint32 len, 
		    bool nopad, Uint32 Label);
  int branch_col_ne(Uint32 ColId, const void * val, Uint32 len, 
		    bool nopad, Uint32 Label);
  int branch_col_lt(Uint32 ColId, const void * val, Uint32 len, 
		    bool nopad, Uint32 Label);
  int branch_col_le(Uint32 ColId, const void * val, Uint32 len, 
		    bool nopad, Uint32 Label);
  int branch_col_gt(Uint32 ColId, const void * val, Uint32 len, 
		    bool nopad, Uint32 Label);
  int branch_col_ge(Uint32 ColId, const void * val, Uint32 len, 
		    bool nopad, Uint32 Label);
  /**
   * LIKE/NOTLIKE wildcard comparisons
   * These instructions support SQL-style % and _ wildcards for
   * (VAR)CHAR/BINARY columns only
   *
   * The argument is always plain char format, even if the field 
   * is varchar
   * (changed in 5.0.22).
   * 
   * @note For Scans and NdbRecord operations, use the 
   *       NdbInterpretedCode interface.
   */
  int branch_col_like(Uint32 ColId, const void *, Uint32 len, 
		      bool nopad, Uint32 Label);
  int branch_col_notlike(Uint32 ColId, const void *, Uint32 len, 
			 bool nopad, Uint32 Label);

  /**
   * Bitwise logical comparisons
   *
   * These comparison types are only supported for the Bitfield
   * type
   * They can be used to test for bit patterns in bitfield columns
   *   The value passed is a bitmask which is bitwise-ANDed with the
   *   column data.
   *   Bitfields are passed in/out of NdbApi as 32-bit words with
   *   bits set from lsb to msb.
   *   The platform's endianness controls which byte contains the ls
   *   bits.
   *     x86= first(0th) byte.  Sparc/PPC= last (3rd byte)
   *
   *   To set bit n of a bitmask to 1 from a Uint32* mask : 
   *     mask[n >> 5] |= (1 << (n & 31))
   *
   *   The branch can be taken in 4 cases :
   *     - Column data AND Mask == Mask (all masked bits are set in data)
   *     - Column data AND Mask != Mask (not all masked bits are set in data)
   *     - Column data AND Mask == 0    (No masked bits are set in data)
   *     - Column data AND Mask != 0    (Some masked bits are set in data)
   *   
   */
  int branch_col_and_mask_eq_mask(Uint32 ColId, const void *, Uint32 len, 
                                  bool nopad, Uint32 Label);
  int branch_col_and_mask_ne_mask(Uint32 ColId, const void *, Uint32 len, 
                                  bool nopad, Uint32 Label);
  int branch_col_and_mask_eq_zero(Uint32 ColId, const void *, Uint32 len, 
                                  bool nopad, Uint32 Label);
  int branch_col_and_mask_ne_zero(Uint32 ColId, const void *, Uint32 len, 
                                  bool nopad, Uint32 Label);

  /**
   * Interpreted program instruction: Exit with Ok
   *
   * @note For Scans and NdbRecord operations, use the 
   *       NdbInterpretedCode interface.
   *
   * @return -1 if unsuccessful.
   */
  int	interpret_exit_ok();

  /**
   * Interpreted program instruction: Exit with Not Ok
   *
   * @note For Scans and NdbRecord operations, use the 
   *       NdbInterpretedCode interface.
   *
   * @note A method also exists without the error parameter.
   * 
   * @param ErrorCode   An error code given by the application programmer.
   *                    If not supplied, defaults to 899. Applications should 
   *                    use error code 626 or any code in the [6000-6999] 
   *                    range.  Error code 899 is supported for backwards 
   *                    compatibility, but 626 is recommmended instead. For 
   *                    other codes, the behavior is undefined and may change 
   *                    at any time without prior notice.
   * @return            -1 if unsuccessful.
   */
  int   interpret_exit_nok(Uint32 ErrorCode);
  int   interpret_exit_nok();

  
  /**
   * Interpreted program instruction:
   *
   * abort the whole transaction.
   *
   * @note For Scans and NdbRecord operations, use the 
   *       NdbInterpretedCode interface.
   *
   * @return            -1 if unsuccessful.
   */
  int interpret_exit_last_row();
  
  /**
   * Interpreted program instruction:
   * Define a subroutine in an interpreted operation.
   *
   * @note For Scans and NdbRecord operations, use the 
   *       NdbInterpretedCode interface.
   *
   * @param SubroutineNumber the subroutine number.
   * @return -1 if unsuccessful.
   */
  int   def_subroutine(int SubroutineNumber);

  /**
   * Interpreted program instruction:
   * Call a subroutine.
   *
   * @note For Scans and NdbRecord operations, use the 
   *       NdbInterpretedCode interface.
   *
   * @param Subroutine the subroutine to call.
   * @return -1 if unsuccessful. 
   */
  int   call_sub(Uint32 Subroutine);

  /**
   * Interpreted program instruction:
   * End a subroutine.
   *
   * @note For Scans and NdbRecord operations, use the 
   *       NdbInterpretedCode interface.
   *
   * @return -1 if unsuccessful. 
   */
  int   ret_sub();
#endif

  /** @} *********************************************************************/

  /** 
   * @name Error Handling
   * @{
   */

  /**
   * Get the latest error code.
   *
   * @return error code.
   */
  const NdbError & getNdbError() const;

  /**
   * Get the method number where the error occured.
   * 
   * @return method number where the error occured.
   */
#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
  int getNdbErrorLine();
#endif
  int getNdbErrorLine() const;

  /**
   * Get table name of this operation.
   * Not supported for NdbRecord operation.
   */
  const char* getTableName() const;

  /**
   * Get table object for this operation
   * Not supported for NdbRecord operation.
   */
  const NdbDictionary::Table * getTable() const;

  /**
   * Get the type of access for this operation
   */
  Type getType() const;

  /** @} *********************************************************************/

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  /**
   * Type of operation
   */
  enum OperationType { 
    ReadRequest = 0,              ///< Read operation
    UpdateRequest = 1,            ///< Update Operation
    InsertRequest = 2,            ///< Insert Operation
    DeleteRequest = 3,            ///< Delete Operation
    WriteRequest = 4,             ///< Write Operation
    ReadExclusive = 5,            ///< Read exclusive
    RefreshRequest = 6,           ///<
    UnlockRequest = 7,            ///< Unlock operation
    OpenScanRequest,              ///< Scan Operation
    OpenRangeScanRequest,         ///< Range scan operation
    NotDefined2,                  ///< Internal for debugging
    NotDefined                    ///< Internal for debugging
  };
#endif

  /**
   * Return lock mode for operation
   */
  LockMode getLockMode() const { return theLockMode; }

  /**
   * Get/set abort option
   */
  AbortOption getAbortOption() const;
  int setAbortOption(AbortOption);

  /**
   * Get NdbTransaction object pointer for this operation
   */
  virtual NdbTransaction* getNdbTransaction() const;
  
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  
  /**
   * Set/get partition key
   */
  void setPartitionId(Uint32 id);
  Uint32 getPartitionId() const;
#endif

  /* Specification of an extra value to get
   * as part of an NdbRecord operation.
   * Inputs : 
   *  To specify an extra value to read, the
   *  caller must provide a column, and a 
   *  (optionally NULL) appStorage pointer.
   * Outputs : 
   *  After the operation is defined, the 
   *  recAttr member will contain a pointer
   *  to the NdbRecAttr object for receiving
   *  the data.
   * 
   * appStorage pointer
   *  If the appStorage pointer is null, then
   *  the received value will be stored in
   *  memory managed by the NdbRecAttr object.
   *
   *  If the appStorage pointer is non-null then 
   *  the received value will be stored at the 
   *  location pointed to (and will still be 
   *  accessable via the NdbRecAttr object).  
   *  It is the caller's responsibility to 
   *  ensure that :
   *    - appStorage points to sufficient space 
   *      to store any returned data.
   *    - Memory pointed to by appStorage is not
   *      reused/freed until after the execute()
   *      call returns.
   *
   * Limitation : Blob reads cannot be specified 
   * using GetValueSpec.
   */
  struct GetValueSpec
  {
    const NdbDictionary::Column *column;
    void *appStorage;
    NdbRecAttr *recAttr;
  };
  
  /* Specification of an extra value to set
   * as part of an NdbRecord operation.
   * The value ptr must point to the value
   * to set, or NULL if the attribute is to
   * be set to NULL.
   * The pointed to value is copied when the 
   * operation is defined and need not remain
   * in place until execution time.
   *
   * Limitation : Blobs cannot be set using 
   * SetValueSpec.
   */
  struct SetValueSpec
  {
    const NdbDictionary::Column *column;
    const void * value;
  };

  /*
   * OperationOptions
   *  These are options passed to the NdbRecord primary key and scan 
   *  takeover operation methods defined in the NdbTransaction and 
   *  NdbScanOperation classes.
   *  
   *  Each option type is marked as present by setting the corresponding
   *  bit in the optionsPresent field.  Only the option types marked in the
   *  optionsPresent structure need have sensible data.
   *  All data is copied out of the OperationOptions structure (and any
   *  subtended structures) at operation definition time.
   *  If no options are required, then NULL may be passed as the 
   *  OperationOptions pointer.
   *
   *  Most methods take a supplementary sizeOfOptions parameter.  This
   *  is optional, and is intended to allow the interface implementation
   *  to remain backwards compatible with older un-recompiled clients 
   *  that may pass an older (smaller) version of the OperationOptions 
   *  structure.  This effect is achieved by passing
   *  sizeof(OperationOptions) into this parameter.
   */
  struct OperationOptions
  {
    /*
      Size of the OperationOptions structure.
    */
    static inline Uint32 size()
    {
        return sizeof(OperationOptions);
    }

    /*
     * Which options are present.  See below for option details
     */
    Uint64 optionsPresent;
    enum Flags { OO_ABORTOPTION  = 0x01,
                 OO_GETVALUE     = 0x02, 
                 OO_SETVALUE     = 0x04, 
                 OO_PARTITION_ID = 0x08, 
                 OO_INTERPRETED  = 0x10,
                 OO_ANYVALUE     = 0x20,
                 OO_CUSTOMDATA   = 0x40,
                 OO_LOCKHANDLE   = 0x80,
                 OO_QUEUABLE     = 0x100,
                 OO_NOT_QUEUABLE = 0x200,
                 OO_DEFERRED_CONSTAINTS = 0x400,
                 OO_DISABLE_FK   = 0x800
    };

    /* An operation-specific abort option.
     * Only necessary if the default abortoption behaviour
     * is not satisfactory 
     */
    AbortOption abortOption;

    /* Extra column values to be read */
    GetValueSpec *extraGetValues;
    Uint32        numExtraGetValues;
    
    /* Extra column values to be set  */
    const SetValueSpec *extraSetValues;
    Uint32              numExtraSetValues;

    /* Specific partition to execute this operation on */
    Uint32 partitionId;

    /* Interpreted code to be executed in this operation
     * Only supported for update operations currently 
     */
    const NdbInterpretedCode *interpretedCode;

    /* anyValue to be used for this operation */
    Uint32 anyValue;

    /* customData ptr for this operation */
    void * customData;
  };

  /* getLockHandle
   * Returns a pointer to this operation's LockHandle.
   * For NdbRecord, the lock handle must first be requested using
   * the OO_LOCKHANDLE operation option.
   * For non-NdbRecord operations, this call can be used alone.
   * The returned LockHandle cannot be used until the operation
   * has been executed.
   */
  const NdbLockHandle* getLockHandle() const;
  const NdbLockHandle* getLockHandle();

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  // XXX until NdbRecord is used in ndb_restore
  void set_disable_fk() { m_flags |= OF_DISABLE_FK; }
#endif

protected:
/******************************************************************************
 * These are the methods used to create and delete the NdbOperation objects.
 *****************************************************************************/

  bool                  needReply();
/******************************************************************************
 * These methods are service routines used by the other NDB API classes.
 *****************************************************************************/
//--------------------------------------------------------------
// Initialise after allocating operation to a transaction		      
//--------------------------------------------------------------
  int init(const class NdbTableImpl*, NdbTransaction* aCon);
  void initInterpreter();

  NdbOperation(Ndb* aNdb, Type aType = PrimaryKeyAccess);	
  virtual ~NdbOperation();
  void	next(NdbOperation*);		// Set next pointer		      
  NdbOperation*	    next();	        // Get next pointer		       

public:
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  const NdbOperation* next() const;
  const NdbRecAttr* getFirstRecAttr() const;

  void* getCustomData() const { return m_customData; }
  void setCustomData(void* p) { m_customData = p; }
protected:
  void* m_customData;
#endif
protected:

  /*
    Methods that define the operation (readTuple(), getValue(), etc). can be
    called in any order, but not all are valid.

    To keep track of things, we store a 'current state of definitin operation'
    in member 'theStatus', with possible values given here.
  */
  enum OperationStatus
  {
    /*
      Init: Initial state after getting NdbOperation.
      At this point, the type of operation must be set (insertTuple(),
      readTuple(), etc.).

    */
    Init,
    /*
      OperationDefined: State in which the primary key search condition is
      defined with equal().
    */
    OperationDefined,
    /*
      TupleKeyDefined: All parts of the primary key have been specified with
      equal().
    */
    TupleKeyDefined,
    /*
      GetValue: The state in which the attributes to read are defined with
      calls to getValue(). For interpreted operations, these are the initial
      reads, before the interpreted program.
    */
    GetValue,
    /*
      SetValue: The state in which attributes to update are defined with
      calls to setValue().
    */
    SetValue,
    /*
      ExecInterpretedValue: The state in which the interpreted program is
      defined.
    */
    ExecInterpretedValue,
    /*
      SetValueInterpreted: Updates after interpreted program.
    */
    SetValueInterpreted,
    /*
      FinalGetValue: Attributes to read after interpreted program.
    */
    FinalGetValue,
    /*
      SubroutineExec: In the middle of a subroutine definition being defined.
    */
    SubroutineExec,
    /*
      SubroutineEnd: A subroutine has been fully defined, but a new subroutine
      definition may still be defined after.
    */
    SubroutineEnd,
    /*
      WaitResponse: Operation has been sent to kernel, waiting for reply.
    */
    WaitResponse,
    /*
      Finished: The TCKEY{REF,CONF} signal for this operation has been
      received.
    */
    Finished,
    /*
      NdbRecord: For operations using NdbRecord. Built in a single call (like
      NdbTransaction::readTuple(), and no state transitions possible before
      execute().
    */
    UseNdbRecord
  };

  OperationStatus   Status();	         	// Read the status information
  
  void		    Status(OperationStatus);    // Set the status information

  void		    NdbCon(NdbTransaction*);	// Set reference to connection
  						// object.

  virtual void	    release();			// Release all operations 
                                                // connected to
					      	// the operations object.      
  void              postExecuteRelease();       // Release resources
                                                // no longer needed after
                                                // exceute 
  void		    setStartIndicator();

  /* Utility method to 'add' operation options to an NdbOperation
   *
   * @return 0 for success.  NDBAPI to set error otherwise.
   */
  static int        handleOperationOptions (const OperationType type,
                                            const OperationOptions *opts,
                                            const Uint32 sizeOfOptions,
                                            NdbOperation *op);

/******************************************************************************
 * The methods below is the execution part of the NdbOperation
 * class. This is where the NDB signals are sent and received. The
 * operation can send TC[KEY/INDX]REQ, [INDX]ATTRINFO. 
 * It can receive TC[KEY/INDX]CONF, TC[KEY/INDX]REF, [INDX]ATTRINFO. 
 * When an operation is received in its fulness or a refuse message 
 * was sent, then the connection object is told about this situation.
 *****************************************************************************/

  int    doSendKeyReq(int processorId,
                      GenericSectionPtr* secs,
                      Uint32 numSecs,
                      bool lastFlag);
  int    doSend(int ProcessorId, Uint32 lastFlag);
  void   setRequestInfoTCKEYREQ(bool lastFlag, bool longSignal);
  virtual int	 prepareSend(Uint32  TC_ConnectPtr,
                             Uint64  TransactionId,
			     AbortOption);
  virtual void   setLastFlag(NdbApiSignal* signal, Uint32 lastFlag);
    
  int	 prepareSendInterpreted();            // Help routine to prepare*

  int initInterpretedInfo(const NdbInterpretedCode *code,
                          Uint32*& interpretedInfo,
                          Uint32* stackSpace,
                          Uint32 stackSpaceEntries,
                          Uint32*& dynamicSpace);

  void freeInterpretedInfo(Uint32*& dynamicSpace);


  /* Method for adding signals for an interpreted program
   * to the signal train 
   */
  int buildInterpretedProgramSignals(Uint32 aTC_ConnectPtr, 
                                     Uint64 aTransId,
                                     Uint32 **attrInfoPtr,
                                     Uint32 *remain,
                                     const NdbInterpretedCode *code,
                                     Uint32 *interpretedWorkspace,
                                     bool mainProgram,
                                     Uint32 &wordsWritten);

  // Method which prepares signals at operation definition time.
  int    buildSignalsNdbRecord(Uint32 aTC_ConnectPtr, Uint64 aTransId,
                               const Uint32 * read_mask);

  // Method which does final preparations at execute time.
  int    prepareSendNdbRecord(AbortOption ao);

  /* Helper routines for buildSignalsNdbRecord(). */
  Uint32 fillTcKeyReqHdr(TcKeyReq *tcKeyReq,
                         Uint32 connectPtr,
                         Uint64 transId);
  int    allocKeyInfo();
  int    allocAttrInfo();
  int    insertKEYINFO_NdbRecord(const char *value,
                                 Uint32 byteSize);
  int    insertATTRINFOHdr_NdbRecord(Uint32 attrId,
                                     Uint32 attrLen);
  int    insertATTRINFOData_NdbRecord(const char *value,
                                      Uint32 size);

  int	 receiveTCKEYREF(const NdbApiSignal*);

  int	 checkMagicNumber(bool b = true); // Verify correct object
  static Uint32 getMagicNumber() { return (Uint32)0xABCDEF01; }

  int    checkState_TransId(const NdbApiSignal* aSignal);

/******************************************************************************
 *	These are support methods only used locally in this class.
******************************************************************************/

  virtual int equal_impl(const NdbColumnImpl*,const char* aValue);
  virtual NdbRecAttr* getValue_impl(const NdbColumnImpl*, char* aValue = 0);
  NdbRecAttr* getValue_NdbRecord(const NdbColumnImpl* tAttrInfo, char* aValue);
  int setValue(const NdbColumnImpl* anAttrObject, const char* aValue);
  NdbBlob* getBlobHandle(NdbTransaction* aCon, const NdbColumnImpl* anAttrObject);
  NdbBlob* getBlobHandle(NdbTransaction* aCon, const NdbColumnImpl* anAttrObject) const;
  int incValue(const NdbColumnImpl* anAttrObject, Uint32 aValue);
  int incValue(const NdbColumnImpl* anAttrObject, Uint64 aValue);
  int subValue(const NdbColumnImpl* anAttrObject, Uint32 aValue);
  int subValue(const NdbColumnImpl* anAttrObject, Uint64 aValue);
  int read_attr(const NdbColumnImpl* anAttrObject, Uint32 RegDest);
  int write_attr(const NdbColumnImpl* anAttrObject, Uint32 RegSource);
  int branch_reg_reg(Uint32 type, Uint32, Uint32, Uint32);
  int branch_col(Uint32 type, Uint32, const void *, Uint32, Uint32 Label);
  int branch_col_null(Uint32 type, Uint32 col, Uint32 Label);
  NdbBlob *linkInBlobHandle(NdbTransaction *aCon,
                            const NdbColumnImpl *column,
                            NdbBlob * & lastPtr);
  int getBlobHandlesNdbRecord(NdbTransaction* aCon, const Uint32 * mask);
  int getBlobHandlesNdbRecordDelete(NdbTransaction* aCon, 
                                    bool checkReadSet, const Uint32 * mask);

  // Handle ATTRINFO signals   
  int insertATTRINFO(Uint32 aData);
  int insertATTRINFOloop(const Uint32* aDataPtr, Uint32 aLength);
  
  int insertKEYINFO(const char* aValue,	
		    Uint32 aStartPosition,	
		    Uint32 aKeyLenInByte);
  void reorderKEYINFO();
  
  virtual void setErrorCode(int aErrorCode) const;
  virtual void setErrorCodeAbort(int aErrorCode) const;

  bool        isNdbRecordOperation();
  int	      incCheck(const NdbColumnImpl* anAttrObject);
  int	      initial_interpreterCheck();
  int	      intermediate_interpreterCheck();
  int	      read_attrCheck(const NdbColumnImpl* anAttrObject);
  int	      write_attrCheck(const NdbColumnImpl* anAttrObject);
  int	      labelCheck();
  int	      insertCall(Uint32 aCall);
  int	      insertBranch(Uint32 aBranch);

  Uint32 ptr2int() { return theReceiver.getId(); };
  Uint32 ptr2int() const { return theReceiver.getId(); };

  // get table or index key from prepared signals
  int getKeyFromTCREQ(Uint32* data, Uint32 & size);

  int getLockHandleImpl();
  int prepareGetLockHandle();
  int prepareGetLockHandleNdbRecord();

  virtual void setReadLockMode(LockMode lockMode);
  void setReadCommittedBase();
  Uint32 getReadCommittedBase();

/******************************************************************************
 * These are the private variables that are defined in the operation objects.
 *****************************************************************************/

  Type m_type;

  NdbReceiver theReceiver;

  NdbError theError;			// Errorcode	       
  int 	   theErrorLine;		// Error line       

  Ndb*		   theNdb;	      	// Point back to the Ndb object.
  NdbTransaction*   theNdbCon;	       	// Point back to the connection object.
  NdbOperation*	   theNext;	       	// Next pointer to operation.

  union {
    NdbApiSignal* theTCREQ;		// The TC[KEY/INDX]REQ signal object
    NdbApiSignal* theSCAN_TABREQ;
    NdbApiSignal* theRequest;
  };

  NdbApiSignal*	   theFirstATTRINFO;	// The first ATTRINFO signal object 
  NdbApiSignal*	   theCurrentATTRINFO;	// The current ATTRINFO signal object  
  Uint32	   theTotalCurrAI_Len;	// The total number of attribute info
  		      			// words currently defined    
  Uint32	   theAI_LenInCurrAI;	// The number of words defined in the
		      		     	// current ATTRINFO signal
  NdbApiSignal*	   theLastKEYINFO;	// The first KEYINFO signal object 

  class NdbLabel*	    theFirstLabel;
  class NdbLabel*	    theLastLabel;
  class NdbBranch*	    theFirstBranch;
  class NdbBranch*	    theLastBranch;
  class NdbCall*	    theFirstCall;
  class NdbCall*	    theLastCall;
  class NdbSubroutine*    theFirstSubroutine;
  class NdbSubroutine*    theLastSubroutine;
  Uint32	    theNoOfLabels;
  Uint32	    theNoOfSubroutines;

  Uint32*           theKEYINFOptr;       // Pointer to where to write KEYINFO
  Uint32            keyInfoRemain;       // KeyInfo space in current signal
  Uint32*           theATTRINFOptr;      // Pointer to where to write ATTRINFO
  Uint32            attrInfoRemain;      // AttrInfo space in current signal

  /* 
     The table object for the table to read or modify (for index operations,
     it is the table being indexed.)
  */
  const class NdbTableImpl* m_currentTable;

  /*
    The table object for the index used to access the table. For primary key
    lookups, it is equal to m_currentTable.
  */
  const class NdbTableImpl* m_accessTable;

  // Set to TRUE when a tuple key attribute has been defined. 
  Uint32	    theTupleKeyDefined[NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY][3];

  Uint32	    theTotalNrOfKeyWordInSignal;     // The total number of
  						     // keyword in signal.

  Uint32	    theTupKeyLen;	// Length of the tuple key in words
		       		        // left until done
  Uint8	theNoOfTupKeyLeft;  // The number of tuple key attributes
  OperationType	theOperationType;        // Read Request, Update Req......   
  
  LockMode        theLockMode;	   // Can be set to WRITE if read operation 
  OperationStatus theStatus;	   // The status of the operation.	
  
  Uint32         theMagicNumber;  // Magic number to verify that object 
                                   // is correct
  Uint32 theScanInfo;      	   // Scan info bits (take over flag etc)
  Uint32 theDistributionKey;       // Distribution Key size if used

  Uint32 theSubroutineSize;	   // Size of subroutines for interpretation
  Uint32 theInitialReadSize;	   // Size of initial reads for interpretation
  Uint32 theInterpretedSize;	   // Size of interpretation
  Uint32 theFinalUpdateSize;	   // Size of final updates for interpretation
  Uint32 theFinalReadSize;	   // Size of final reads for interpretation

  Uint8  theStartIndicator;	 // Indicator of whether start operation
  Uint8  theCommitIndicator;	 // Indicator of whether commit operation
  Uint8  theSimpleIndicator;	 // Indicator of whether simple operation
  Uint8  theDirtyIndicator;	 // Indicator of whether dirty operation
  /**
   * Indicates that the base operation is ReadCommitted although it has
   * been upgraded to use locking read.
   */
  Uint8  theReadCommittedBaseIndicator;
  Uint8  theInterpretIndicator;  // Indicator of whether interpreted operation
                                 // Note that scan operations always have this
                                 // set true
  Int8  theDistrKeyIndicator_;    // Indicates whether distr. key is used

  enum OP_FLAGS {
    OF_NO_DISK = 0x1,

    /*
      For NdbRecord, this flag indicates that we need to send the Event-attached
      word set by setAnyValue().
    */
    OF_USE_ANY_VALUE = 0x2,
    OF_QUEUEABLE = 0x4,
    OF_DEFERRED_CONSTRAINTS = 0x8,
    OF_DISABLE_FK = 0x10
  };
  Uint8  m_flags;

  Uint8 _unused1;

  Uint16 m_tcReqGSN;
  Uint16 m_keyInfoGSN;
  Uint16 m_attrInfoGSN;

  /*
    Members for NdbRecord operations.
    ToDo: We might overlap these (with anonymous unions) with members used
    for NdbRecAttr access (theKEYINFOptr etc), to save a bit of memory. Not
    sure if it is worth the loss of code clarity though.
  */

  /*
    NdbRecord describing the placement of Primary key in row.
    As a special case, we set this to NULL for scan lock take-over operations,
    in which case the m_key_row points to keyinfo obtained from the KEYINFO20
    signal.
  */
  const NdbRecord *m_key_record;
  /* Row containing the primary key to operate on, or KEYINFO20 data. */
  const char *m_key_row;
  /* Size in words of keyinfo in m_key_row. */
  Uint32 m_keyinfo_length;
  /*
    NdbRecord describing attributes to update (or read for scans).
    We also use m_attribute_record!=NULL to indicate that the operation is
    using the NdbRecord interface (as opposed to NdbRecAttr).
  */
  const NdbRecord *m_attribute_record;
  /* Row containing the update values. */
  const char *m_attribute_row;
  /*
    Bitmask to disable selected columns.
    Do not use clas Bitmask/BitmaskPOD here, to avoid having to
    #include <Bitmask.hpp> in application code.
  */
  Uint32 m_unused_read_mask[(128+31)>>5];
  /* Interpreted program for NdbRecord operations. */
  const NdbInterpretedCode *m_interpreted_code;

  /* Ptr to supplied SetValueSpec for NdbRecord */
  const SetValueSpec *m_extraSetValues;
  Uint32 m_numExtraSetValues;

  Uint32 m_any_value;                           // Valid if m_use_any_value!=0

  // Blobs in this operation
  NdbBlob* theBlobList;

  // ONLY for blob V2 implementation (not virtual, only PK ops)
  NdbRecAttr*
  getVarValue(const NdbColumnImpl*, char* aBareValue, Uint16* aLenLoc);
  int
  setVarValue(const NdbColumnImpl*, const char* aBareValue, const Uint16&  aLen);

  /*
   * Abort option per operation, used by blobs.
   * See also comments on enum AbortOption.
   */
  Int8 m_abortOption;

  /*
   * For blob impl, option to not propagate error to trans level.
   * Could be AO_IgnoreError variant if we want it public.
   * Ignored unless AO_IgnoreError is also set.
   */
  Int8 m_noErrorPropagation;

  friend struct Ndb_free_list_t<NdbOperation>;

  Uint32 repack_read(Uint32 len);

  NdbLockHandle* theLockHandle;

  bool m_blob_lock_upgraded; /* Did Blob code upgrade LM_CommittedRead
                              * to LM_Read?
                              */

private:
  NdbOperation(const NdbOperation&); // Not impl.
  NdbOperation&operator=(const NdbOperation&);
};

#ifdef NDB_NO_DROPPED_SIGNAL
#include <stdlib.h>
#endif

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL

inline
int
NdbOperation::checkMagicNumber(bool b)
{
#ifndef NDB_NO_DROPPED_SIGNAL
  (void)b;  // unused param in this context
#endif
  if (theMagicNumber != getMagicNumber()){
#ifdef NDB_NO_DROPPED_SIGNAL
    if(b) abort();
#endif
    return -1;
  }
  return 0;
}

inline
void
NdbOperation::setStartIndicator()
{
  theStartIndicator = 1;
}

inline
int
NdbOperation::getNdbErrorLine()
{
  // delegate to overloaded const function for same semantics
  const NdbOperation * const cthis = this;
  return cthis->NdbOperation::getNdbErrorLine();
}

inline
int
NdbOperation::getNdbErrorLine() const
{
  return theErrorLine;
}

/******************************************************************************
void next(NdbOperation* aNdbOperation);

Parameters:    aNdbOperation: Pointers to the NdbOperation object.
Remark:        Set the next variable of the operation object.
******************************************************************************/
inline
void
NdbOperation::next(NdbOperation* aNdbOperation)
{
  theNext = aNdbOperation;
}

/******************************************************************************
NdbOperation* next();

Return Value:  	Return  next pointer to NdbOperation object.
Remark:         Get the next variable of the operation object.
******************************************************************************/
inline
NdbOperation*
NdbOperation::next()
{
  return theNext;
}

inline
const NdbOperation*
NdbOperation::next() const 
{
  return theNext;
}

inline
const NdbRecAttr*
NdbOperation::getFirstRecAttr() const 
{
  return theReceiver.m_firstRecAttr;
}

/******************************************************************************
Type getType()
                                                                                
Return Value    Return the Type.
Remark:         Gets type of access.
******************************************************************************/
inline
NdbOperation::Type
NdbOperation::getType() const
{
  return m_type;
}
                                                                                
/******************************************************************************
OperationStatus  Status();

Return Value    Return the OperationStatus.	
Parameters:     aStatus:  The status.
Remark:         Sets Operation status. 
******************************************************************************/
inline
NdbOperation::OperationStatus			
NdbOperation::Status()
{
  return theStatus;
}

/******************************************************************************
void  Status(OperationStatus aStatus);

Parameters:     aStatus: The status.
Remark:         Sets Operation
 status. 
******************************************************************************/
inline
void			
NdbOperation::Status( OperationStatus aStatus )
{
  theStatus = aStatus;
}

/******************************************************************************
void NdbCon(NdbTransaction* aNdbCon);

Parameters:    aNdbCon: Pointers to NdbTransaction object.
Remark:        Set the reference to the connection in the operation object.
******************************************************************************/
inline
void
NdbOperation::NdbCon(NdbTransaction* aNdbCon)
{
  theNdbCon = aNdbCon;
}

inline
int
NdbOperation::equal(const char* anAttrName, const char* aValue,
                    Uint32 len)
{
  (void)len;   // unused
  return equal(anAttrName, aValue);
}

inline
int
NdbOperation::equal(const char* anAttrName, Int32 aPar)
{
  return equal(anAttrName, (const char*)&aPar, (Uint32)4);
}

inline
int
NdbOperation::equal(const char* anAttrName, Uint32 aPar)
{
  return equal(anAttrName, (const char*)&aPar, (Uint32)4);
}

inline
int
NdbOperation::equal(const char* anAttrName, Int64 aPar)
{
  return equal(anAttrName, (const char*)&aPar, (Uint32)8);
}

inline
int
NdbOperation::equal(const char* anAttrName, Uint64 aPar)
{
  return equal(anAttrName, (const char*)&aPar, (Uint32)8);
}

inline
int
NdbOperation::equal(Uint32 anAttrId, const char* aValue,
                    Uint32 len)
{
  (void)len;   // unused
  return equal(anAttrId, aValue);
}

inline
int
NdbOperation::equal(Uint32 anAttrId, Int32 aPar)
{
  return equal(anAttrId, (const char*)&aPar, (Uint32)4);
}

inline
int
NdbOperation::equal(Uint32 anAttrId, Uint32 aPar)
{
  return equal(anAttrId, (const char*)&aPar, (Uint32)4);
}

inline
int
NdbOperation::equal(Uint32 anAttrId, Int64 aPar)
{
  return equal(anAttrId, (const char*)&aPar, (Uint32)8);
}

inline
int
NdbOperation::equal(Uint32 anAttrId, Uint64 aPar)
{
  return equal(anAttrId, (const char*)&aPar, (Uint32)8);
}

inline
int
NdbOperation::setValue(const char* anAttrName, const char* aValue,
                       Uint32 len)
{
  (void)len;   // unused
  return setValue(anAttrName, aValue);
}

inline
int
NdbOperation::setValue(const char* anAttrName, Int32 aPar)
{
  return setValue(anAttrName, (const char*)&aPar, (Uint32)4);
}

inline
int
NdbOperation::setValue(const char* anAttrName, Uint32 aPar)
{
  return setValue(anAttrName, (const char*)&aPar, (Uint32)4);
}

inline
int
NdbOperation::setValue(const char* anAttrName, Int64 aPar)
{
  return setValue(anAttrName, (const char*)&aPar, (Uint32)8);
}

inline
int
NdbOperation::setValue(const char* anAttrName, Uint64 aPar)
{
  return setValue(anAttrName, (const char*)&aPar, (Uint32)8);
}

inline
int
NdbOperation::setValue(const char* anAttrName, float aPar)
{
  return setValue(anAttrName, (const char*)&aPar, (Uint32)4);
}

inline
int
NdbOperation::setValue(const char* anAttrName, double aPar)
{
  return setValue(anAttrName, (const char*)&aPar, (Uint32)8);
}

inline
int
NdbOperation::setValue(Uint32 anAttrId, const char* aValue,
                       Uint32 len)
{
  (void)len;   // unused
  return setValue(anAttrId, aValue);
}

inline
int
NdbOperation::setValue(Uint32 anAttrId, Int32 aPar)
{
  return setValue(anAttrId, (const char*)&aPar, (Uint32)4);
}

inline
int
NdbOperation::setValue(Uint32 anAttrId, Uint32 aPar)
{
  return setValue(anAttrId, (const char*)&aPar, (Uint32)4);
}

inline
int
NdbOperation::setValue(Uint32 anAttrId, Int64 aPar)
{
  return setValue(anAttrId, (const char*)&aPar, (Uint32)8);
}

inline
int
NdbOperation::setValue(Uint32 anAttrId, Uint64 aPar)
{
  return setValue(anAttrId, (const char*)&aPar, (Uint32)8);
}

inline
int
NdbOperation::setValue(Uint32 anAttrId, float aPar)
{
  return setValue(anAttrId, (char*)&aPar, (Uint32)4);
}

inline
int
NdbOperation::setValue(Uint32 anAttrId, double aPar)
{
  return setValue(anAttrId, (const char*)&aPar, (Uint32)8);
}

inline
void
NdbOperation::setReadCommittedBase()
{
  theReadCommittedBaseIndicator = 1;
}

inline
Uint32
NdbOperation::getReadCommittedBase()
{
  return theReadCommittedBaseIndicator;
}
#endif // doxygen

#endif
