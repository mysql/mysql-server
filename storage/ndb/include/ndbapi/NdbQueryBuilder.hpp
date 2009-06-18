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

#ifndef NdbQueryBuilder_H
#define NdbQueryBuilder_H

#include <ndb_types.h>

#include "NdbError.hpp"
#include "NdbDictionary.hpp"

class Ndb;


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
 * for the NdbQuery objects has been declared 'protected'.
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

protected:
  // Enforce object creation through NdbQueryBuilder factory 
  NdbQueryOperand();
  virtual ~NdbQueryOperand();

private:
  // Copying disallowed:
  NdbQueryOperand(const NdbQueryOperand& other);
  NdbQueryOperand& operator = (const NdbQueryOperand& other);
};

// A NdbQueryOperand is either of these:
class NdbConstOperand  : public NdbQueryOperand
{
protected:
  NdbConstOperand();
  virtual ~NdbConstOperand();
};

class NdbLinkedOperand : public NdbQueryOperand
{
protected:
  NdbLinkedOperand();
  virtual ~NdbLinkedOperand();
};

class NdbParamOperand  : public NdbQueryOperand {
public:
  const char* getName() const;
  Uint32 getEnum() const;

protected:
  NdbParamOperand();
  virtual ~NdbParamOperand();
};


class NdbQueryIndexBound
{
  const NdbQueryOperand* const low_key[];  // NULL terminated
  bool low_inclusive;
  const NdbQueryOperand* const high_key[]; // NULL terminated
  bool high_inclusive;
};


/**
 * NdbQueryOperationDef defines an operation on a single NDB table
 */
class NdbQueryOperationDef // Base class for all operation definitions
{
public:

  Uint32 getNoOfParentOperations() const;
  const NdbQueryOperationDef* getParentOperation(Uint32 i) const;

  Uint32 getNoOfChildOperations() const;
  const NdbQueryOperationDef* getChildOperation(Uint32 i) const;

  const NdbQueryOperationDef* getRootOperation() const;
  // assert(getRootOperation()->getNoOfParentOperations() == 0);

  /**
   * Get table object for this operation
   */
  const NdbDictionary::Table* getTable() const;

protected:
  // Enforce object creation through NdbQueryBuilder factory 
  NdbQueryOperationDef();
  virtual ~NdbQueryOperationDef();

private:
  // Copying disallowed:
  NdbQueryOperationDef(const NdbQueryOperationDef& other);
  NdbQueryOperationDef& operator = (const NdbQueryOperationDef& other);

}; // class NdbQueryOperationDef


class NdbQueryLookupOperationDef : public NdbQueryOperationDef
{
public:
  /**
   * Get possible index object for this operation
   */
  const NdbDictionary::Index* getIndex() const;

protected:
  // Enforce object creation through NdbQueryBuilder factory 
  NdbQueryLookupOperationDef();
  virtual ~NdbQueryLookupOperationDef();
}; // class NdbQueryLookupOperationDef

class NdbQueryScanOperationDef : public NdbQueryOperationDef  // Base class for scans
{
protected:
  // Enforce object creation through NdbQueryBuilder factory 
  NdbQueryScanOperationDef();
  virtual ~NdbQueryScanOperationDef();
}; // class NdbQueryScanOperationDef

class NdbQueryTableScanOperationDef : public NdbQueryScanOperationDef
{
protected:
  // Enforce object creation through NdbQueryBuilder factory 
  NdbQueryTableScanOperationDef();
  virtual ~NdbQueryTableScanOperationDef();
}; // class NdbQueryTableScanOperationDef

class NdbQueryIndexScanOperationDef : public NdbQueryScanOperationDef
{
protected:
  // Enforce object creation through NdbQueryBuilder factory 
  NdbQueryIndexScanOperationDef();
  virtual ~NdbQueryIndexScanOperationDef();
}; // class NdbQueryIndexScanOperationDef




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
 *   determined by the Ndb object, or until it is explicit released by
 *   NdbQueryDef::release()
 *  
 */
class NdbQueryBuilder 
{
public:
  NdbQueryBuilder(Ndb&);    // Or getQueryBuilder() from Ndb..
 ~NdbQueryBuilder();

   
  class NdbQueryDef* prepare();    // Complete building a queryTree from 'this' NdbQueryBuilder

  // NdbQueryOperand builders:
  // ::constValue constructors variants, considder to added/removed variants
  // Partly based on value types currently supported through NdbOperation::equal()
  NdbConstOperand* constValue(const char* value);
  NdbConstOperand* constValue(const void* value, size_t length);
  NdbConstOperand* constValue(Int32  value); 
  NdbConstOperand* constValue(Uint32 value); 
  NdbConstOperand* constValue(Int64  value); 
  NdbConstOperand* constValue(Uint64 value); 

  // ::paramValue()
  NdbParamOperand* paramValue(const char* name = 0);  // Parameterized

  NdbLinkedOperand* linkedValue(const NdbQueryOperationDef*, const char* attr); // Linked value


  // NdbQueryOperationDef builders:
  //
  // Common argument 'ident' may be used to identify each NdbQueryOperationDef with a name.
  // This may later be used to find the corresponding NdbQueryOperation instance when
  // the NdbQueryDef is executed. 
  // Each NdbQueryOperationDef will also be assigned an numeric ident (starting from 0)
  // as an alternative way of locating the NdbQueryOperation.
  
  NdbQueryLookupOperationDef* readTuple(
                                const NdbDictionary::Table*,          // Primary key lookup
				const NdbQueryOperand* const keys[],  // Terminated by NULL element 
                                const char* ident = 0);

  NdbQueryLookupOperationDef* readTuple(
                                const NdbDictionary::Index*,          // Unique key lookup w/ index
			        const NdbDictionary::Table*,
				const NdbQueryOperand* const keys[],  // Terminated by NULL element 
                                const char* ident = 0);

  NdbQueryTableScanOperationDef* scanTable(
                                const NdbDictionary::Table*,
                                const char* ident = 0);

  NdbQueryIndexScanOperationDef* scanIndex(
                                const NdbDictionary::Index*, 
	                        const NdbDictionary::Table*,
                                const NdbQueryIndexBound* bound = 0,
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

  /** 
   * Get the method number where the latest error occured.
   * 
   * @return Line number where latest error occured.
   */
//int getNdbErrorLine();

/*** LIKELY TO BE REMOVED:
  void next(NdbQueryBuilder* next)      // Set next pointer
  { m_next = next; };
		      
  NdbQueryBuilder*  next()              // Get next pointer
  { return m_next; };
		
private:
  NdbQueryBuilder* m_next;
********/

private:
  Ndb& m_ndb;
  NdbError m_error;
}; // class NdbQueryBuilder

/**
 * NdbQueryDef represents a ::prepare()'d object from NdbQueryBuilder.
 *
 * The NdbQueryDef is reusable in the sense that it may be executed multiple
 * times. Its lifetime is defined by the Ndb object which it was created with,
 * or it may be explicitely released() when no longer required.
 *
 * The NdbQueryDef *must* be keept alive until the last thread
 * which executing a query based on this NdbQueryDef has completed execution 
 * *and* result handling. Used from multiple threads this implies either:
 *
 *  - Keep the NdbQueryDef until all threads terminates.
 *  - Implement reference counting on the NdbQueryDef.
 *  - Use the supplied copy constructor to give each thread its own copy
 *    of the NdbQueryDef.
 *
 * A NdbQueryDef is scheduled for execution by appending it to an open 
 * transaction - optionally together with a set of parameters specifying 
 * the actuall values required by ::execute() (ie. Lookup an bind keys).
 *
 */
class NdbQueryDef
{
protected:
  // C'tor is private - only NdbQueryBuilder::prepare() is allowed to construct a new NdbQueryDef
  NdbQueryDef();

public:
  ~NdbQueryDef();

  // Copy construction of the NdbQueryDef IS defined.
  // May be convenient to take a copy when the same query is used from
  // multiple threads.
  NdbQueryDef(const NdbQueryDef& other);
  NdbQueryDef& operator = (const NdbQueryDef& other);

  // Remove this NdbQueryDef.
//void release();    Just delete it instead ?
};


#endif
