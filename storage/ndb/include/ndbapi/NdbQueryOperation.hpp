/*
   Copyright (C) 2009 Sun Microsystems Inc
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef NdbQueryOperation_H
#define NdbQueryOperation_H

#include <ndb_types.h>
/* There is no way to forward declare nested class NdbDictionary::Column,
 * so this header file must be included.*/
#include "NdbDictionary.hpp"
// Needed to get NdbScanOrdering.
#include "NdbQueryBuilder.hpp" 
#include "NdbIndexScanOperation.hpp"


class Ndb;
struct NdbError;
class NdbOperation;
class NdbParamOperand;
class NdbQueryDef;
class NdbQueryOperation;
class NdbQueryOperationDef;
class NdbRecAttr;
class NdbTransaction;
class NdbRecord;
class NdbScanFilter;
class NdbInterpretedCode;

/** Opaque implementation classes*/
class NdbQueryImpl;
class NdbQueryOperationImpl;

/**
 * NdbQuery are create when a NdbQueryDefinition is submitted for
 * execution.
 *
 * It is associated with a collection of NdbQueryOperation which 
 * are instantiated (1::1) to reflect the NdbQueryOperationDef objects
 * which the NdbQueryDef consists of.
 */
class NdbQuery
{
protected:
  // Only constructable through ::buildQuery() 
  friend class NdbQueryImpl;
  NdbQuery(NdbQueryImpl& impl);
  ~NdbQuery();

public:
  /** Possible return values from nextResult().*/
  enum NextResultOutcome{
    NextResult_error = -1,
    NextResult_gotRow = 0,
    NextResult_scanComplete = 1,
    NextResult_bufferEmpty = 2
  };

  Uint32 getNoOfOperations() const;

  // Get a specific NdbQueryOperation by ident specified
  // when the NdbQueryOperationDef was created.
  NdbQueryOperation* getQueryOperation(const char* ident) const;
  NdbQueryOperation* getQueryOperation(Uint32 index) const;
//NdbQueryOperation* getQueryOperation(const NdbQueryOperationDef* def) const;

  Uint32 getNoOfParameters() const;
  const NdbParamOperand* getParameter(const char* name) const;
  const NdbParamOperand* getParameter(Uint32 num) const;

  int setBound(const NdbRecord *keyRecord,
               const struct NdbIndexScanOperation::IndexBound *bound);

  /**
   * Get the next tuple(s) from the global cursor on the query.
   *
   * Result row / columns will be updated in the respective result handlers
   * as previously specified on each NdbQueryOperation either by assigning a
   * NdbRecord/rowBuffer or assigning NdbRecAttr to each column to be retrieved.
   * 
   * @param fetchAllowed  If set to false, then fetching is disabled
   * @param forceSend If true send will occur immediately (see @ref secAdapt)
   *
   * When fetchAllowed is set to false,
   * the NDB API will not request new batches from the NDB Kernel when
   * all received rows have been exhausted, but will instead return 2
   * from nextResult(), indicating that new batches must be
   * requested. You must then call nextResult with fetchAllowed = true
   * in order to contact the NDB Kernel for more records, after taking over
   * locks as appropriate.
   *
   * @note: All result returned from a NdbQuery are handled as scan results
   *       in a cursor like interface.(Even single tuple 'lookup' operations!)
   *  - After ::execute() the current position of the result set is 'before'
   *    the first row. There is no valid data yet in the 'RecAttr'
   *    or NdbRecord associated with the NdbQueryOperation!
   *  - ::nextResult() is required to retrieve the first row. This may
   *    also cause any error / status info assicioated with the result set
   *    iself to be returned (Like 'NoData', posible type conversion errors,
   *    or constraint violations associated with each specific row in the
   *    result set.)
   *
   * @return 
   * -  -1: if unsuccessful,<br>
   * -   0: if another tuple was received, and<br> 
   * -   1: if there are no more tuples to scan.
   * -   2: if there are no more cached records in NdbApi
   */
  NextResultOutcome nextResult(bool fetchAllowed = true, 
                               bool forceSend = false);

  /**
   * Get NdbTransaction object for this query operation
   */
  NdbTransaction* getNdbTransaction() const;

  /**
   * Close query
   */
  void close(bool forceSend = false);

  /** 
   * @name Error Handling
   * @{
   */

  /**
   * Get error object with information about the latest error.
   *
   * @return An error object with information about the latest error.
   */
  const NdbError& getNdbError() const;

  /** Get object implementing NdbQuery interface.*/
  NdbQueryImpl& getImpl() const
  { return m_impl; }

private:
  /** Opaque implementation NdbQuery interface.*/
  NdbQueryImpl& m_impl;

}; // class NdbQuery




class NdbQueryOperation
{
private:
  // Only constructable through executing a NdbQueryDef
  friend class NdbQueryOperationImpl;
  NdbQueryOperation(NdbQueryOperationImpl& impl);
  ~NdbQueryOperation();

public:
  // Collection of get'ers to navigate in root, parent/child hierarchy

  Uint32 getNoOfParentOperations() const;
  NdbQueryOperation* getParentOperation(Uint32 parentNo) const;

  Uint32 getNoOfChildOperations() const;
  NdbQueryOperation* getChildOperation(Uint32 childNo) const;

  const NdbQueryOperationDef& getQueryOperationDef() const;

  // Get the entire query object which this operation is part of
  NdbQuery& getQuery() const;



  /**
   * Defines a retrieval operation of an attribute value.
   * The NDB API allocate memory for the NdbRecAttr object that
   * will hold the returned attribute value. 
   *
   * @note Note that it is the applications responsibility
   *       to allocate enough memory for resultBuffer (if non-NULL).
   *       The buffer resultBuffer supplied by the application must be
   *       aligned appropriately.  The buffer is used directly
   *       (avoiding a copy penalty) only if it is aligned on a
   *       4-byte boundary and the attribute size in bytes
   *       (i.e. NdbRecAttr::attrSize times NdbRecAttr::arraySize is
   *       a multiple of 4).
   *
   * @note There are three versions of NdbQueryOperation::getValue with
   *       slightly different parameters.
   *
   * @note This method does not fetch the attribute value from 
   *       the database!  The NdbRecAttr object returned by this method 
   *       is <em>not</em> readable/printable before the 
   *       transaction has been executed with NdbTransaction::execute.
   *
   * @param anAttrName   Attribute name 
   * @param resultBuffer If this is non-NULL, then the attribute value 
   *                     will be returned in this parameter.<br>
   *                     If NULL, then the attribute value will only 
   *                     be stored in the returned NdbRecAttr object.
   * @return             An NdbRecAttr object to hold the value of 
   *                     the attribute, or a NULL pointer 
   *                     (indicating error).
   */
  NdbRecAttr* getValue(const char* anAttrName, char* resultBuffer = 0);
  NdbRecAttr* getValue(Uint32 anAttrId, char* resultBuffer = 0);
  NdbRecAttr* getValue(const NdbDictionary::Column* column, 
		       char* resultBuffer = 0);

  /**
   * Retrieval of entire or partial rows may also be specified. For partial
   * retrieval a bitmask should supplied.
   *
   * The behaviour of mixing NdbRecord retrieval style with NdbRecAttr is
   * is undefined - It should probably not be allowed.
   *
   * @param rec  Is a pointer to a NdbRecord specifying the byte layout of the
   *             result row.
   *
   * @resBuffer  Defines a buffer sufficient large to hold the result row.
   *
   * @bufRef     Refers a pointer which will be updated to refer the current result row
   *             for this operand.
   *
   * @param  result_mask defines as subset of attributes to read. 
   *         The column is only affected if 'mask[attrId >> 3]  & (1<<(attrId & 7))' is set
   * @return 0 on success, -1 otherwise (call getNdbError() for details).
   */
  int setResultRowBuf (const NdbRecord *rec,
                       char* resBuffer,
                       const unsigned char* result_mask = 0);

  int setResultRowRef (const NdbRecord* rec,
                       const char* & bufRef,
                       const unsigned char* result_mask = 0);

  // TODO: define how BLOB/CLOB should be retrieved.
  // ... Replicate ::getBlobHandle() from NdbOperation class?


  // Result handling for this NdbQueryOperation
  bool isRowNULL() const;    // Row associated with Operation is NULL value?

  bool isRowChanged() const; // Prev ::nextResult() on NdbQuery retrived a new
                             // value for this NdbQueryOperation

  /** Get object implementing NdbQueryOperation interface.*/
  NdbQueryOperationImpl& getImpl() const
  { return m_impl; }

  /** Define result ordering for ordered index scan. It is an error to call
   * this method on an operation that is not a scan, or to call it if an
   * ordering was already set on the operation defintion by calling 
   * NdbQueryOperationDef::setOrdering().
   * @param ordering The desired ordering of results.
   * @return 0 if ok, -1 in case of error (call getNdbError() for details.)
   */
  int setOrdering(NdbScanOrdering ordering);

  /** Get the result ordering for this operation.*/
  NdbScanOrdering getOrdering() const;


  /**
   * Set the NdbInterpretedCode needed for defining a scan filter for 
   * this operation. 
   *
   * Typically, one would create NdbScanFilter and NdbInterpretedCode objects
   * on the stack, e.g.:
   *   NdbInterpretedCode code(table);
   *   NdbScanFilter filter(code);
   *   filter.begin();
   *   filter.ge(0, 5U); // Check if column 1 is greater of equal to 5.
   *   filter.end();
   *   scanOp->setInterpretedCode(code);
   *
   * It is an error to call this method on a lookup operation.
   * @param code The interpreted code. This object is copied internally, 
   * meaning that 'code' may be destroyed as soon as this method returns.
   * @return 0 if ok, -1 in case of error (call getNdbError() for details.)
   */
  int setInterpretedCode(NdbInterpretedCode& code) const;

private:
  // Opaque implementation class instance.
  NdbQueryOperationImpl& m_impl;

}; // class NdbQueryOperation

 
class NdbQueryParamValue
{
public:

  // Raw data:
  // NOTE: This is how mysqld prepare parameter values!
  NdbQueryParamValue(const void* val);

  // C-type string, terminated by '\0'
  NdbQueryParamValue(const char* val);

  // NULL-value, also used as optional end marker 
  NdbQueryParamValue();
  NdbQueryParamValue(Uint16 val);
  NdbQueryParamValue(Uint32 val);
  NdbQueryParamValue(Uint64 val);
  NdbQueryParamValue(double val);

  // More parameter C'tor to be added when required:
//NdbQueryParamValue(Uint8 val);
//NdbQueryParamValue(Int8 val);
//NdbQueryParamValue(Int16 val);
//NdbQueryParamValue(Int32 val);
//NdbQueryParamValue(Int64 val);

  // Get parameter value with required typeconversion to fit format
  // expected by paramOp
  int getValue(const class NdbParamOperandImpl& paramOp,
               const void*& addr, size_t& len,
               bool& is_null) const;

private:
  int m_type;

  union
  {
    Uint8     uint8;
    Int8      int8;
    Uint16    uint16;
    Int16     int16;
    Uint32    uint32;
    Int32     int32;
    Uint64    uint64;
    Int64     int64;
    double    dbl;
    const char* string;
    const void* raw;
  } m_value;
};

#endif
