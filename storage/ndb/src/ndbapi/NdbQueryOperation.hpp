/*
   Copyright (c) 2011, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef NdbQueryOperation_H
#define NdbQueryOperation_H

#include <ndb_types.h>

// this file is currently not located in include/ndbapi
// which means that we need to use <> to include instead of ""
// for files located in include/ndbapi

// this file is currently not located in include/ndbapi
// skip includes...and require them to be included first
// BUH!

/* There is no way to forward declare nested class NdbDictionary::Column,
 * so this header file must be included.*/
// #include <NdbDictionary.hpp>

// Needed to get NdbQueryOptions::ScanOrdering.
// #include "NdbQueryBuilder.hpp"
// #include <NdbIndexScanOperation.hpp>


class Ndb;
struct NdbError;
class NdbParamOperand;
class NdbQueryOperation;
class NdbQueryOperationDef;
class NdbRecAttr;
class NdbTransaction;
class NdbRecord;
class NdbInterpretedCode;

/** Opaque implementation classes*/
class NdbQueryImpl;
class NdbQueryOperationImpl;


/**********************  OVERVIEW    ***********************
 *
 * a NdbQuery is created when a NdbQueryDefinition is added to a
 * NdbTransaction for execution with NdbTransaction::creatQuery().
 *
 * A NdbQuery is associated with a collection of NdbQueryOperation which 
 * are instantiated (1::1) to reflect the NdbQueryOperationDef objects
 * which the NdbQueryDef consists of. The same NdbQueryDef may be used to
 * instantiate multiple NdbQuery obejects.
 *
 * When we have an instantiated NdbQuery, we should either bind result buffers
 * for retrieving entire rows from each operation, 
 * (aka NdbRecord interface,::setResultRowRef(), ::setResultRowBuf())
 * or set up retrieval operations for each attribute values, (::getValue()).
 * 
 * Optionally we may also:
 *  - Specify a scan ordering for the result set (parent only)
 *  - Add multiple bounds to a range scan, (::setBound()) (parent only)
 *  - Append a filter condition for each operation (aka mysqlds pushed condition)  
 *
 * The NdbQuery is then executed together with other pending operations 
 * in the next NdbTransaction::execute().
 * The resultset available from a NdbQuery is natively a 'left outer join'
 * between the parent / child operations. If an application is not interested
 * in the 'outer part' of the resultset, it is its own responsibility to
 * filter these rows. Same is valid for any filter condition which has
 * not been appended to the NdbQuery.
 *
 *
 * We provide two different interfaces for iterating the result set:
 *
 * The 'local cursor' (NdbQueryOperation::firstResult(), ::nextResult())
 *   Will navigate the resultset, and fetch results, from this specific operation.
 *   It will only be possible to navigate within those rows which depends
 *   on the current row(s) from any ancestor of the operation.
 *   The local cursor will only retrieve the results, or a NULL row, 
 *   resulting from its own operation. -> All child operations of a 
 *   renavigated local cursor should be navigated to ::firstResult() 
 *   to ensure that they contain results related to the renavigated parent. 
 *   
 * The 'global cursor' (NdbQuery::nextResult())
 *   Will present the result set as a scan on the root operation
 *   with rows from its child operations appearing in an unpredictable 
 *   order. A new set of results, or NULL rows, from *all* operations 
 *   in the query tree are retrieved for each ::nextResult().
 *   NULL rows resulting from the outer joins may appear anywhere 
 *   inside the resultset.
 *
 * As the global cursor is implemented on top of the local cursors, it is
 * possible to mix the usage of global and local cursors.
 *
 ************************************************************************/
class NdbQuery
{
private:
  // Only constructable through ::buildQuery() 
  friend class NdbQueryImpl;
  explicit NdbQuery(NdbQueryImpl& impl);
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

  int setBound(const NdbRecord *keyRecord,
               const struct NdbIndexScanOperation::IndexBound *bound);

  /**
   * When returning results from a multi-range-read, over multiple 'bounds',
   * we can get which 'range' (or bound) the returned row comes from.
   */
  int getRangeNo() const;

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
   *    itself to be returned (Like 'NoData', possible type conversion errors,
   *    or constraint violations associated with each specific row in the
   *    result set.)
   *
   * @return 
   * -  NextResult_error (-1):       if unsuccessful,<br>
   * -  NextResult_gotRow (0):       if another tuple was received, and<br> 
   * -  NextResult_scanComplete (1): if there are no more tuples to scan.
   * -  NextResult_bufferEmpty (2):  if there are no more cached records
   *                                 in NdbApi
   */
  NextResultOutcome nextResult(bool fetchAllowed = true, 
                               bool forceSend = false);

  /**
   * Get NdbTransaction object for this query operation
   */
  NdbTransaction* getNdbTransaction() const;

  /**
   * Close query.
   *
   * Will release most of the internally allocated objects owned 
   * by this NdbQuery and detach itself from the NdbQueryDef
   * used to instantiate it.
   *
   * The application may destruct the NdbQueryDef after 
   * ::close() has been called on *all* NdbQuery objects
   * instantiated from it.
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

  /**
   * Check if this is a pruned range scan. A range scan is pruned if the ranges
   * are such that only a subset of the fragments need to be scanned for 
   * matching tuples.
   *
   * @param pruned This will be set to true if the operation is a pruned range 
   * scan.
   * @return 0 if ok, -1 in case of error (call getNdbError() for details.)
   */
  int isPrunable(bool& pruned) const;

private:
  /** Opaque implementation NdbQuery interface.*/
  NdbQueryImpl& m_impl;

}; // class NdbQuery




class NdbQueryOperation
{
private:
  // Only constructable through executing a NdbQueryDef
  friend class NdbQueryOperationImpl;
  explicit NdbQueryOperation(NdbQueryOperationImpl& impl);
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
  NdbRecAttr* getValue(const char* anAttrName, char* resultBuffer = nullptr);
  NdbRecAttr* getValue(Uint32 anAttrId, char* resultBuffer = nullptr);
  NdbRecAttr* getValue(const NdbDictionary::Column* column, 
		       char* resultBuffer = nullptr);

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
                       const unsigned char* result_mask = nullptr);

  int setResultRowRef (const NdbRecord* rec,
                       const char* & bufRef,
                       const unsigned char* result_mask = nullptr);

  // TODO: define how BLOB/CLOB should be retrieved.
  // ... Replicate ::getBlobHandle() from NdbOperation class?

  /** Get object implementing NdbQueryOperation interface.*/
  NdbQueryOperationImpl& getImpl() const
  { return m_impl; }

  /** Define result ordering for ordered index scan. It is an error to call
   * this method on an operation that is not a scan, or to call it if an
   * ordering was already set on the operation definition by calling 
   * NdbQueryOperationDef::setOrdering().
   * @param ordering The desired ordering of results.
   * @return 0 if ok, -1 in case of error (call getNdbError() for details.)
   */
  int setOrdering(NdbQueryOptions::ScanOrdering ordering);

  /** Get the result ordering for this operation.*/
  NdbQueryOptions::ScanOrdering getOrdering() const;

  /** 
   * Set the number of fragments to be scanned in parallel. This only applies
   * to table scans and non-sorted scans of ordered indexes. This method is
   * only implemented for then root scan operation.
   * @return 0 if ok, -1 in case of error (call getNdbError() for details.)
   */
  int setParallelism(Uint32 parallelism);

  /**
   * Set the number of fragments to be scanned in parallel to the maximum
   * possible value. This is the default for the root scan operation.
   * @return 0 if ok, -1 in case of error (call getNdbError() for details.)
   */
  int setMaxParallelism();

  /**
   * Let the system dynamically choose the number of fragments to scan in
   * parallel. The system will try to choose a value that gives optimal
   * performance. This is the default for all scans but the root scan. This
   * method only implemented for non-root scan operations.
   * @return 0 if ok, -1 in case of error (call getNdbError() for details.)
   */
  int setAdaptiveParallelism();

  /** Set the batch size (max rows per batch) for this operation. This
   * only applies to scan operations, as lookup operations always will
   * have the same batch size as its parent operation, or 1 if it is the
   * root operation.
   * @param batchSize Batch size (in number of rows). A value of 0 means
   * use the default batch size.
   * @return 0 if ok, -1 in case of error (call getNdbError() for details.)
   */
  int setBatchSize(Uint32 batchSize);

  /**
   * Set the NdbInterpretedCode needed for defining a conditional filter 
   * (aka: predicate) for this operation. Might be used both on scan 
   * and lookup operations.
   *
   * Typically, one would create NdbScanFilter and NdbInterpretedCode objects
   * on the stack, e.g.:
   *   NdbInterpretedCode code(table);
   *   NdbScanFilter filter(code);
   *   filter.begin();
   *   filter.ge(0, 5U); // Check if column 1 is greater of equal to 5.
   *   filter.end();
   *   queryOp->setInterpretedCode(code);
   *
   * @param code The interpreted code. This object is copied internally, 
   * meaning that 'code' may be destroyed as soon as this method returns.
   * @return 0 if ok, -1 in case of error (call getNdbError() for details.)
   */
  int setInterpretedCode(const NdbInterpretedCode& code) const;

  /**  
   * Local cursor:
   *
   * Navigate to first result row in this batch of results which 
   * depends on the current row(s) from all its ancestors.
   * @return 
   * -  NextResult_error (-1):       if unsuccessful,<br>
   * -  NextResult_gotRow (0):       if another tuple was received, and<br> 
   * -  NextResult_scanComplete (1): if there are no more tuples to scan.
   * -  NextResult_bufferEmpty (2):  if there are no more cached records
   *                                 in NdbApi
   */
  NdbQuery::NextResultOutcome firstResult();

  /**  
   * Local cursor:
   *
   * Get the next tuple(s) from this operation (and all its descendants?)
   * which depends on the current row(s) from all its ancestors.
   *
   * Result row / columns will be updated in the respective result handlers
   * as previously specified on each NdbQueryOperation either by assigning a
   * NdbRecord/rowBuffer or assigning NdbRecAttr to each column to be retrieved.
   *
   * If the set of cached records in the NdbApi has been consumed, more will be
   * requested from the datanodes only iff:
   *    - This NdbOperation is the root of the entire pushed NdbQuery.
   *    - 'fetchAllowed==true'
   *
   * The arguments fetchAllowed and forceSend are ignored if this operation is 
   * not the root of the pushed query.
   *
   * @return 
   * -  NextResult_error (-1):       if unsuccessful,<br>
   * -  NextResult_gotRow (0):       if another tuple was received, and<br> 
   * -  NextResult_scanComplete (1): if there are no more tuples to scan.
   * -  NextResult_bufferEmpty (2):  if there are no more cached records
   *                                 in NdbApi
   */
  NdbQuery::NextResultOutcome nextResult(
                               bool fetchAllowed = true, 
                               bool forceSend = false);

  // Result handling for this NdbQueryOperation
  bool isRowNULL() const;    // Row associated with Operation is NULL value?

private:
  // Opaque implementation class instance.
  NdbQueryOperationImpl& m_impl;

}; // class NdbQueryOperation

 
class NdbQueryParamValue
{
public:

  // Raw data formatted according to bound Column format.
  // NOTE: This is how mysqld prepare parameter values!
  NdbQueryParamValue(const void* val, bool shrinkVarChar= false);

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

  /**
   * Serialize value into a seuqence of words suitable to be sent to the data
   * nodes.
   * @param column Specifies the format that the value should be serialized 
   * into.
   * @param dst Seralized data are appended to this.
   * @param len Length of serialized data (in number of bytes).
   * @param isNull Will be set to true iff this is a NULL value.
   * @return 0 if ok, otherwise an error code. 
   */
  int serializeValue(const class NdbColumnImpl& column,
                     class Uint32Buffer& dst,
                     Uint32& len,
                     bool& isNull) const;

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
