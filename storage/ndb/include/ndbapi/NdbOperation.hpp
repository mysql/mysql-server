/* Copyright (c) 2003-2007 MySQL AB, 2010 Sun Microsystems, Inc.
   Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

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

/**
 * @class NdbOperation
 * @brief Class of operations for use in transactions.  
 */
class NdbOperation
{
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  friend class Ndb;
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
   * @depricated
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
   */
  virtual NdbBlob* getBlobHandle(const char* anAttrName);
  virtual NdbBlob* getBlobHandle(Uint32 anAttrId);
 
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
   * @param labelNumber   Label number.
   * @return              -1 if unsuccessful.
   */
  int   def_label(int labelNumber);

  /**
   * Interpreted program instruction:
   * Add two registers into a third.
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
   * @param RegDest      Destination register.
   * @return             -1 if unsuccessful.
   */ 
  int   load_const_null(Uint32 RegDest);

  /**
   * Interpreted program instruction:
   * Read an attribute into a register.
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
   * @param anAttrName   Attribute name.
   * @param RegSource    Source register.
   * @return             -1 if unsuccessful.
   */
  int   write_attr(const char* anAttrName, Uint32 RegSource);

  /**
   * Interpreted program instruction:
   * Read an attribute into a register.
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
   * @param RegLvalue the value to check.
   * @param Label the label to jump to.
   * @return -1 if unsuccessful. 
   */
  int   branch_ne_null(Uint32 RegLvalue, Uint32 Label);

  /**
   * Interpreted program instruction:
   * Jump to Label if RegLvalue is equal to NULL.
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
   * @param  Label  Label to jump to.
   * @return -1 if unsuccessful.
   */
  int   branch_label(Uint32 Label);

  /**
   * Interpreted program instruction:  branch after memcmp
   * @param  ColId   Column to check
   * @param  Label   Label to jump to
   * @return -1 if unsuccessful
   */
  int branch_col_eq_null(Uint32 ColId, Uint32 Label);
  int branch_col_ne_null(Uint32 ColId, Uint32 Label);

  /**
   * Interpreted program instruction:  branch after memcmp
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
   * The argument is always plain char, even if the field is varchar
   * (changed in 5.0.22).
   */
  int branch_col_like(Uint32 ColId, const void *, Uint32 len, 
		      bool nopad, Uint32 Label);
  int branch_col_notlike(Uint32 ColId, const void *, Uint32 len, 
			 bool nopad, Uint32 Label);
  
  /**
   * Interpreted program instruction: Exit with Ok
   *
   * For scanning transactions,
   * end interpreted operation and return the row to the application.
   *
   * For non-scanning transactions,
   * exit interpreted program.
   *
   * @return -1 if unsuccessful.
   */
  int	interpret_exit_ok();

  /**
   * Interpreted program instruction: Exit with Not Ok
   *
   * For scanning transactions, 
   * continue with the next row without returning the current row.
   *
   * For non-scanning transactions,
   * abort the whole transaction.
   *
   * @note A method also exists without the error parameter.
   * 
   * @param ErrorCode   An error code given by the application programmer.
   * @return            -1 if unsuccessful.
   */
  int   interpret_exit_nok(Uint32 ErrorCode);
  int   interpret_exit_nok();

  
  /**
   * Interpreted program instruction:
   *
   * For scanning transactions, 
   * return this row, but no more from this fragment
   *
   * For non-scanning transactions,
   * abort the whole transaction.
   *
   * @return            -1 if unsuccessful.
   */
  int interpret_exit_last_row();
  
  /**
   * Interpreted program instruction:
   * Define a subroutine in an interpreted operation.
   *
   * @param SubroutineNumber the subroutine number.
   * @return -1 if unsuccessful.
   */
  int   def_subroutine(int SubroutineNumber);

  /**
   * Interpreted program instruction:
   * Call a subroutine.
   *
   * @param Subroutine the subroutine to call.
   * @return -1 if unsuccessful. 
   */
  int   call_sub(Uint32 Subroutine);

  /**
   * Interpreted program instruction:
   * End a subroutine.
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
  int getNdbErrorLine();

  /**
   * Get table name of this operation.
   */
  const char* getTableName() const;

  /**
   * Get table object for this operation
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
  
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  
  /**
   * Set/get partition key
   */
  void setPartitionId(Uint32 id);
  void setPartitionHash(Uint32 key);
  void setPartitionHash(const Uint64 *, Uint32 len);
  Uint32 getPartitionId() const;
#endif
protected:
  int handle_distribution_key(const Uint64 *, Uint32 len);
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
  NdbTransaction* getNdbTransaction();
  const NdbOperation* next() const;
  const NdbRecAttr* getFirstRecAttr() const;
#endif
protected:

  enum OperationStatus
  { 
    Init,                       
    OperationDefined,
    TupleKeyDefined,
    GetValue,
    SetValue,
    ExecInterpretedValue,
    SetValueInterpreted,
    FinalGetValue,
    SubroutineExec,
    SubroutineEnd,
    WaitResponse,
    WaitCommitResponse,
    Finished,
    ReceiveFinished
  };

  OperationStatus   Status();	         	// Read the status information
  
  void		    Status(OperationStatus);    // Set the status information

  void		    NdbCon(NdbTransaction*);	// Set reference to connection
  						// object.

  virtual void	    release();			// Release all operations 
                                                // connected to
					      	// the operations object.      
  void		    setStartIndicator();

/******************************************************************************
 * The methods below is the execution part of the NdbOperation
 * class. This is where the NDB signals are sent and received. The
 * operation can send TC[KEY/INDX]REQ, [INDX]ATTRINFO. 
 * It can receive TC[KEY/INDX]CONF, TC[KEY/INDX]REF, [INDX]ATTRINFO. 
 * When an operation is received in its fulness or a refuse message 
 * was sent, then the connection object is told about this situation.
 *****************************************************************************/

  int    doSend(int ProcessorId, Uint32 lastFlag);
  virtual int	 prepareSend(Uint32  TC_ConnectPtr,
                             Uint64  TransactionId,
			     AbortOption);
  virtual void   setLastFlag(NdbApiSignal* signal, Uint32 lastFlag);
    
  int	 prepareSendInterpreted();            // Help routine to prepare*
   
  int	 receiveTCKEYREF(NdbApiSignal*); 

  int	 checkMagicNumber(bool b = true); // Verify correct object

  int    checkState_TransId(NdbApiSignal* aSignal);

/******************************************************************************
 *	These are support methods only used locally in this class.
******************************************************************************/

  virtual int equal_impl(const NdbColumnImpl*,const char* aValue);
  virtual NdbRecAttr* getValue_impl(const NdbColumnImpl*, char* aValue = 0);
  int setValue(const NdbColumnImpl* anAttrObject, const char* aValue);
  NdbBlob* getBlobHandle(NdbTransaction* aCon, const NdbColumnImpl* anAttrObject);
  int incValue(const NdbColumnImpl* anAttrObject, Uint32 aValue);
  int incValue(const NdbColumnImpl* anAttrObject, Uint64 aValue);
  int subValue(const NdbColumnImpl* anAttrObject, Uint32 aValue);
  int subValue(const NdbColumnImpl* anAttrObject, Uint64 aValue);
  int read_attr(const NdbColumnImpl* anAttrObject, Uint32 RegDest);
  int write_attr(const NdbColumnImpl* anAttrObject, Uint32 RegSource);
  int branch_reg_reg(Uint32 type, Uint32, Uint32, Uint32);
  int branch_col(Uint32 type, Uint32, const void *, Uint32, bool, Uint32 Label);
  int branch_col_null(Uint32 type, Uint32 col, Uint32 Label);
  
  // Handle ATTRINFO signals   
  int insertATTRINFO(Uint32 aData);
  int insertATTRINFOloop(const Uint32* aDataPtr, Uint32 aLength);
  
  int insertKEYINFO(const char* aValue,	
		    Uint32 aStartPosition,	
		    Uint32 aKeyLenInByte);
  void reorderKEYINFO();
  
  virtual void setErrorCode(int aErrorCode);
  virtual void setErrorCodeAbort(int aErrorCode);

  void        handleFailedAI_ElemLen();	   // When not all attribute data
                                           // were received

  int	      incCheck(const NdbColumnImpl* anAttrObject);
  int	      initial_interpreterCheck();
  int	      intermediate_interpreterCheck();
  int	      read_attrCheck(const NdbColumnImpl* anAttrObject);
  int	      write_attrCheck(const NdbColumnImpl* anAttrObject);
  int	      labelCheck();
  int	      insertCall(Uint32 aCall);
  int	      insertBranch(Uint32 aBranch);

  Uint32 ptr2int() { return theReceiver.getId(); };

  // get table or index key from prepared signals
  int getKeyFromTCREQ(Uint32* data, Uint32 & size);

  virtual void setReadLockMode(LockMode lockMode);

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
  Uint32*           theATTRINFOptr;      // Pointer to where to write ATTRINFO

  const class NdbTableImpl* m_currentTable; // The current table
  const class NdbTableImpl* m_accessTable;  // Index table (== current for pk)

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
  Uint8  theInterpretIndicator;  // Indicator of whether interpreted operation
  Int8  theDistrKeyIndicator_;    // Indicates whether distr. key is used
  Uint8  m_no_disk_flag;          

  Uint16 m_tcReqGSN;
  Uint16 m_keyInfoGSN;
  Uint16 m_attrInfoGSN;

  // Blobs in this operation
  NdbBlob* theBlobList;

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
};

#ifdef NDB_NO_DROPPED_SIGNAL
#include <stdlib.h>
#endif

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL

inline
int
NdbOperation::checkMagicNumber(bool b)
{
  if (theMagicNumber != 0xABCDEF01){
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
  return theReceiver.theFirstRecAttr;
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
NdbOperation::equal(const char* anAttrName, const char* aValue, Uint32 len)
{
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
NdbOperation::equal(Uint32 anAttrId, const char* aValue, Uint32 len)
{
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
NdbOperation::setValue(const char* anAttrName, const char* aValue, Uint32 len)
{
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
NdbOperation::setValue(Uint32 anAttrId, const char* aValue, Uint32 len)
{
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

#endif // doxygen

#endif
