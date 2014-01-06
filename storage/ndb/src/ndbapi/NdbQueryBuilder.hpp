/*
   Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef NdbQueryBuilder_H
#define NdbQueryBuilder_H

#include <stdlib.h>
#include <ndb_types.h>

// this file is currently not located in include/ndbapi
// skip includes...and require them to be included first
// BUH!

class NdbQueryDef;
class NdbQueryDefImpl;
class NdbQueryBuilderImpl;
class NdbQueryOptionsImpl;
class NdbQueryOperandImpl;
class NdbQueryOperationDefImpl;


/**
 * This is the API interface for building a (composite) query definition,
 * possibly existing of multiple operations linked together (aka 'joined')
 *
 * A query mainly consist of two types of objects:
 *  - NdbQueryOperationDef defines a lookup, or scan on a single table.
 *  - NdbQueryOperand defines a single value which may be used to
 *    define a key, filter or bound on a NdbQueryOperationDef.
 *
 * Construction of these objects are through the NdbQueryBuilder factory.
 * To enforce this restriction, c'tor, d'tor operator
 * for the NdbQuery objects has been declared 'private'.
 * NdbQuery objects should not be copied - Copy constructor and assignment
 * operand has been private declared to enforce this restriction.
 *
 */

/**
 * NdbQueryOperand, a construct for specifying values which are used 
 * to specify lookup keys, bounds or filters in the query tree.
 */
class NdbQueryOperand  // A base class specifying a single value
{
public:
  // Column which this operand relates to
  const NdbDictionary::Column* getColumn() const;
  NdbQueryOperandImpl& getImpl() const;

protected:
  // Enforce object creation through NdbQueryBuilder factory 
  explicit NdbQueryOperand(NdbQueryOperandImpl& impl);
  ~NdbQueryOperand();

private:
  // Copying disallowed:
  NdbQueryOperand(const NdbQueryOperand& other);
  NdbQueryOperand& operator = (const NdbQueryOperand& other);

  NdbQueryOperandImpl& m_impl;
};

// A NdbQueryOperand is either of these:
class NdbConstOperand : public NdbQueryOperand
{
private:
  friend class NdbConstOperandImpl;
  explicit NdbConstOperand(NdbQueryOperandImpl& impl);
  ~NdbConstOperand();
};

class NdbLinkedOperand : public NdbQueryOperand
{
private:
  friend class NdbLinkedOperandImpl;
  explicit NdbLinkedOperand(NdbQueryOperandImpl& impl);
  ~NdbLinkedOperand();
};

class NdbParamOperand  : public NdbQueryOperand {
public:
  const char* getName() const;
  Uint32 getEnum() const;

private:
  friend class NdbParamOperandImpl;
  explicit NdbParamOperand(NdbQueryOperandImpl& impl);
  ~NdbParamOperand();
};


/**
 *  NdbQueryOptions used to pass options when building a NdbQueryOperationDef.
 *
 *  It will normally be constructed on the stack, the required options specified
 *  with the set'ers methods, and then supplied as an argument when creating the 
 *  NdbQueryOperationDef.
 */
class NdbQueryOptions
{
public:

  /**
   * Different match criteria may be specified for an operation.
   * These controls when rows are considdered equal, and a result row
   * is produced (or accepted).
   *
   * These are hints only.
   * The implementation is allowed to take a conservative approach
   * and produce more rows than specified by the MatchType.
   * However, not more rows than specified by 'MatchAll' should be produced.
   * As additional rows should be expected, the receiver should be prepared to
   * filter away unwanted rows if another MatchType than 'MatchAll' was specified.
   */
  enum MatchType
  {
    MatchAll,        // DEFAULT: Output all matches, including duplicates.
                     // Append a single NULL complemented row for non-matching childs.
    MatchNonNull,    // Output all matches, including duplicates. 
                     // Parents without any matches are discarded.
    MatchNullOnly,   // Output only parent rows without any child matches.
                     // Append a single NULL complemented row for the non_matching child
    MatchSingle,     // Output a single row when >=1 child matches.
                     // One of the matching child row is included in the output.
    Default = MatchAll
  };

  /** Ordering of scan results when scanning ordered indexes.*/
  enum ScanOrdering
  {
    /** Undefined (not yet set). */
    ScanOrdering_void, 
    /** Results will not be ordered.*/
    ScanOrdering_unordered, 
    ScanOrdering_ascending,
    ScanOrdering_descending
  };

  explicit NdbQueryOptions();
  ~NdbQueryOptions();

  /** Define result ordering. Alternatively, ordering may be set when the 
   * query definition has been instantiated, using 
   * NdbQueryOperation::setOrdering().
   * @param ordering The desired ordering of results.
   */
  int setOrdering(ScanOrdering ordering);

  /** Define how NULL values and duplicates should be handled when equal tuples are matched.
   */
  int setMatchType(MatchType matchType);

  /**
   * Define an (additional) parent dependency on the specified parent operation.
   * If linkedValues are also defined for the operation which setParent() applies to,
   * all implicit parents specified as part of the 'linkedValues' should be
   * grandparents of 'parent'.
   */
  int setParent(const class NdbQueryOperationDef* parent);

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
  int setInterpretedCode(const class NdbInterpretedCode& code);

  const NdbQueryOptionsImpl& getImpl() const;

private:
  // Copying disallowed:
  NdbQueryOptions(const NdbQueryOptions& other);
  NdbQueryOptions& operator = (const NdbQueryOptions& other);

  NdbQueryOptionsImpl* m_pimpl;

}; // class NdbQueryOptions


/**
 * NdbQueryOperationDef defines an operation on a single NDB table
 */
class NdbQueryOperationDef // Base class for all operation definitions
{
public:

  /**
   * Different access / query operation types
   */
  enum Type {
    PrimaryKeyAccess,     ///< Read using pk
    UniqueIndexAccess,    ///< Read using unique index
    TableScan,            ///< Full table scan
    OrderedIndexScan      ///< Ordered index scan, optionaly w/ bounds
  };

  static const char* getTypeName(Type type);

  /**
   * Get the ordinal position of this operation within the QueryDef.
   */
  Uint32 getOpNo() const;

  Uint32 getNoOfParentOperations() const;
  const NdbQueryOperationDef* getParentOperation(Uint32 i) const;

  Uint32 getNoOfChildOperations() const;
  const NdbQueryOperationDef* getChildOperation(Uint32 i) const;

  Type getType() const;

  /**
   * Get table object for this operation
   */
  const NdbDictionary::Table* getTable() const;

  /**
   * Get index object for this operation if relevant,
   * return NULL else.
   */
  const NdbDictionary::Index* getIndex() const;

  NdbQueryOperationDefImpl& getImpl() const;

protected:
  // Enforce object creation through NdbQueryBuilder factory 
  explicit NdbQueryOperationDef(NdbQueryOperationDefImpl& impl);
  ~NdbQueryOperationDef();

private:
  // Copying disallowed:
  NdbQueryOperationDef(const NdbQueryOperationDef& other);
  NdbQueryOperationDef& operator = (const NdbQueryOperationDef& other);

  NdbQueryOperationDefImpl& m_impl;
}; // class NdbQueryOperationDef


class NdbQueryLookupOperationDef : public NdbQueryOperationDef
{
public:

private:
  // Enforce object creation through NdbQueryBuilder factory
  friend class NdbQueryLookupOperationDefImpl;
  explicit NdbQueryLookupOperationDef(NdbQueryOperationDefImpl& impl);
  ~NdbQueryLookupOperationDef();
}; // class NdbQueryLookupOperationDef

class NdbQueryScanOperationDef : public NdbQueryOperationDef  // Base class for scans
{
protected:
  // Enforce object creation through NdbQueryBuilder factory 
  explicit NdbQueryScanOperationDef(NdbQueryOperationDefImpl& impl);
  ~NdbQueryScanOperationDef();
}; // class NdbQueryScanOperationDef

class NdbQueryTableScanOperationDef : public NdbQueryScanOperationDef
{
private:
  // Enforce object creation through NdbQueryBuilder factory 
  friend class NdbQueryTableScanOperationDefImpl;
  explicit NdbQueryTableScanOperationDef(NdbQueryOperationDefImpl& impl);
  ~NdbQueryTableScanOperationDef();
}; // class NdbQueryTableScanOperationDef

class NdbQueryIndexScanOperationDef : public NdbQueryScanOperationDef
{
public:

private:
  // Enforce object creation through NdbQueryBuilder factory 
  friend class NdbQueryIndexScanOperationDefImpl;
  explicit NdbQueryIndexScanOperationDef(NdbQueryOperationDefImpl& impl);
  ~NdbQueryIndexScanOperationDef();
}; // class NdbQueryIndexScanOperationDef


/**
 * class NdbQueryIndexBound is an argument container for defining
 * a NdbQueryIndexScanOperationDef.
 * The contents of this object is copied into the
 * NdbQueryIndexScanOperationDef and does not have to be 
 * persistent after the NdbQueryBuilder::scanIndex() call
 */
class NdbQueryIndexBound
{
public:
  // C'tor for an equal bound:
  NdbQueryIndexBound(const NdbQueryOperand* const *eqKey)
   : m_low(eqKey), m_lowInclusive(true), m_high(eqKey), m_highInclusive(true)
  {};

  // C'tor for a normal range including low & high limit:
  NdbQueryIndexBound(const NdbQueryOperand* const *low,
                     const NdbQueryOperand* const *high)
   : m_low(low), m_lowInclusive(true), m_high(high), m_highInclusive(true)
  {};

  // Complete C'tor where limits might be exluded:
  NdbQueryIndexBound(const NdbQueryOperand* const *low,  bool lowIncl,
                     const NdbQueryOperand* const *high, bool highIncl)
   : m_low(low), m_lowInclusive(lowIncl), m_high(high), m_highInclusive(highIncl)
  {}

private:
  friend class NdbQueryBuilder;
  friend class NdbQueryIndexScanOperationDefImpl;
  const NdbQueryOperand* const *m_low;  // 'Pointer to array of pointers', NULL terminated
  const bool m_lowInclusive;
  const NdbQueryOperand* const *m_high; // 'Pointer to array of pointers', NULL terminated
  const bool m_highInclusive;
};


/**
 *
 * The Query builder constructs a NdbQueryDef which is a collection of
 * (possibly linked) NdbQueryOperationDefs
 * Each NdbQueryOperationDef may use NdbQueryOperands to specify keys and bounds.
 *
 * LIFETIME:
 * - All NdbQueryOperand and NdbQueryOperationDef objects created in the 
 *   context of a NdbQueryBuilder has a lifetime restricted by:
 *    1. The NdbQueryDef created by the ::prepare() methode.
 *    2. The NdbQueryBuilder *if* the builder is destructed before the
 *       query was prepared.

 *   A single NdbQueryOperand or NdbQueryOperationDef object may be 
 *   used/referrer multiple times during the build process whenever
 *   we need a reference to the same value/node during the 
 *   build phase.
 *
 * - The NdbQueryDef produced by the ::prepare() method has a lifetime 
 *   until it is explicit released by NdbQueryDef::release()
 *
 */
class NdbQueryBuilder 
{
  friend class NdbQueryBuilderImpl;
private:
  // Constructor is private, since application should use 'create()'.
  explicit NdbQueryBuilder(NdbQueryBuilderImpl& impl);
  // Destructor is private, since application should use 'destroy()'
 ~NdbQueryBuilder();

  // No copying of NdbQueryBuilder objects.
  NdbQueryBuilder(const NdbQueryBuilder&);
  NdbQueryBuilder& operator=(const NdbQueryBuilder&);

public:
  /**
   * Allocate an instance.
   * @return New instance, or NULL if allocation failed.
   */
  static NdbQueryBuilder* create();

  /**
   * Release this object and any resources held by it.
   */
  void destroy();

  const NdbQueryDef* prepare();    // Complete building a queryTree from 'this' NdbQueryBuilder

  // NdbQueryOperand builders:
  //
  // ::constValue constructors variants, considder to added/removed variants
  // Typechecking is provided  and will reject constValues/() which is
  // incompatible with the type of the column it is used against.
  // Some very basic typeconversion is available to match destination column
  // with different numeric presicion and spacepad character strings to defined length.
  NdbConstOperand* constValue(Int32  value); 
  NdbConstOperand* constValue(Uint32 value); 
  NdbConstOperand* constValue(Int64  value); 
  NdbConstOperand* constValue(Uint64 value); 
  NdbConstOperand* constValue(double value); 
  NdbConstOperand* constValue(const char* value);  // Null terminated char/varchar C-type string

  // Raw constValue data with specified length - No typeconversion performed to match destination format.
  // Fixed sized datatypes requires 'len' to exactly match the destination column.
  // Fixed sized character values should be spacepadded to required length.
  // Variable sized datatypes requires 'len <= max(len)'.
  NdbConstOperand* constValue(const void* value, Uint32 len);

  // ::paramValue() is a placeholder for a parameter value to be specified when
  // a query instance is created for execution.
  NdbParamOperand* paramValue(const char* name = 0);  // Parameterized

  // ::linkedValue() defines a value available from execution of a previously defined
  // NdbQueryOperationDef. This NdbQueryOperationDef will become the 'parent' of the
  // NdbQueryOperationDef which uses this linkedValue() in any expression.
  NdbLinkedOperand* linkedValue(const NdbQueryOperationDef*, const char* attr); // Linked value


  // NdbQueryOperationDef builders:
  //
  // Common (optional) arguments:
  //
  //    - 'ident' may be used to identify each NdbQueryOperationDef with a name.
  //       This may later be used to find the corresponding NdbQueryOperation instance when
  //       the NdbQueryDef is executed. 
  //       Each NdbQueryOperationDef will also be assigned an numeric ident (starting from 0)
  //       as an alternative way of locating the NdbQueryOperation.
  
  const NdbQueryLookupOperationDef* readTuple(
                                const NdbDictionary::Table*,          // Primary key lookup
                                const NdbQueryOperand* const keys[],  // Terminated by NULL element
                                const NdbQueryOptions* options = 0,
                                const char* ident = 0);

  const NdbQueryLookupOperationDef* readTuple(
                                const NdbDictionary::Index*,          // Unique key lookup w/ index
			        const NdbDictionary::Table*,
                                const NdbQueryOperand* const keys[],  // Terminated by NULL element
                                const NdbQueryOptions* options = 0,
                                const char* ident = 0);

  const NdbQueryTableScanOperationDef* scanTable(
                                const NdbDictionary::Table*,
                                const NdbQueryOptions* options = 0,
                                const char* ident = 0);

  const NdbQueryIndexScanOperationDef* scanIndex(
                                const NdbDictionary::Index*, 
	                        const NdbDictionary::Table*,
                                const NdbQueryIndexBound* bound = 0,
                                const NdbQueryOptions* options = 0,
                                const char* ident = 0);


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

  NdbQueryBuilderImpl& getImpl() const;

private:
  NdbQueryBuilderImpl& m_impl;

}; // class NdbQueryBuilder

/**
 * NdbQueryDef represents a ::prepare()'d object from NdbQueryBuilder.
 *
 * The NdbQueryDef is reusable in the sense that it may be executed multiple
 * times. It is valid until it is explicitely released().
 *
 * The NdbQueryDef *must* be keept alive until the last thread
 * which executing a query based on this NdbQueryDef has called
 * NdbQuery::close().
 *
 * A NdbQueryDef is scheduled for execution by appending it to an open 
 * transaction - optionally together with a set of parameters specifying 
 * the actuall values required by ::execute() (ie. Lookup an bind keys).
 *
 */
class NdbQueryDef
{
  friend class NdbQueryDefImpl;

public:

  /**
   * The different types of query types supported
   */
  enum QueryType {
    LookupQuery,     ///< All operations are PrimaryKey- or UniqueIndexAccess
    SingleScanQuery, ///< Root is Table- or OrderedIndexScan, childs are 'lookup'
    MultiScanQuery   ///< Root, and some childs are scans
  };

  Uint32 getNoOfOperations() const;

  // Get a specific NdbQueryOperationDef by ident specified
  // when the NdbQueryOperationDef was created.
  const NdbQueryOperationDef* getQueryOperation(const char* ident) const;
  const NdbQueryOperationDef* getQueryOperation(Uint32 index) const;

  // A scan query may return multiple rows, and may be ::close'ed when
  // the client has completed access to it.
  bool isScanQuery() const;

  // Return the 'enum QueryType' as defined above.
  QueryType getQueryType() const;

  // Remove this NdbQueryDef including operation and operands it contains
  void destroy() const;

  NdbQueryDefImpl& getImpl() const;

  /**
   * Print the query in a semi human readable form
   */
  void print() const;
private:
  NdbQueryDefImpl& m_impl;

  explicit NdbQueryDef(NdbQueryDefImpl& impl);
  ~NdbQueryDef();

  /** Private and undefined. */
  NdbQueryDef(const NdbQueryDef& other);
  /** Private and undefined.*/
  NdbQueryDef& operator = (const NdbQueryDef& other);
};


#endif
