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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>

#include "Ndb.hpp"
#include "NdbQueryBuilder.hpp"
#include "NdbDictionary.hpp"

#define dynamic_cast reinterpret_cast
/**
 * Implementation of all QueryBuilder objects are completely hidden from
 * both the API interface and other internals in the NDBAPI using the
 * pimpl idiom.
 *
 * The object hierarch visible through the interface has its 'Impl'
 * counterparts inside this module. Some classes are
 * even subclassed further as part of the implementation.
 * (Particular the ConstOperant in order to implement multiple datatypes)
 *
 * In order to avoid allocating both an interface object and its particular
 * Impl object, all 'final' Impl objects inherit its interface class.
 * As all 'Impl' object 'is a' interface object:
 *   - C++ auto downcasting may be used to get the interface object.
 *   - No explicit 'pimpl ptr' required in the interface. Impl object
 *     available by casting the interface obj. to its Impl class.
 *   - Impl classes does not have to be friend of the interface classes.
 *
 * ::getImpl() functions has been defined for convenient access 
 * all available interface classes.
 *
 * CODE STATUS:
 *   Except for creating the Query objects, the NdbQueryBuilder factory
 *   does not do any usefull work yet. This is a framework for further
 *   logic to be added.
 * 
 */

//////////////////////////////////////////////
// Implementation of NdbQueryOperand interface
//////////////////////////////////////////////

// Baseclass for the QueryOperand implementation
class NdbQueryOperandImpl
{
public:
  const NdbDictionary::Column* getColumn() const
  { return m_column; };

protected:
  virtual ~NdbQueryOperandImpl() {};
  NdbQueryOperandImpl()
  : m_column(0) {}

private:
  const NdbDictionary::Column* m_column;
}; // class NdbQueryOperandImpl


class NdbLinkedOperandImpl :
  public NdbLinkedOperand,
  public NdbQueryOperandImpl
{
  friend class NdbQueryBuilder;  // Allow privat access from builder interface

private:
  virtual ~NdbLinkedOperandImpl() {};
  NdbLinkedOperandImpl (const NdbQueryOperationDef* parent, const char* attr)
   : NdbLinkedOperand(this), NdbQueryOperandImpl(),
     m_parent(parent), m_attr(attr)
  {};

  const NdbQueryOperationDef* const m_parent;
  const char* const m_attr;
}; // class NdbLinkedOperandImpl


class NdbParamOperandImpl :
  public NdbParamOperand,
  public NdbQueryOperandImpl
{
  friend class NdbQueryBuilder;  // Allow privat access from builder interface

public:
  const char* getName() const
  { return m_name; };

  Uint32 getEnum() const
  { return 0; };  // FIXME

private:
  virtual ~NdbParamOperandImpl() {};
  NdbParamOperandImpl (const char* name)
   : NdbParamOperand(this), NdbQueryOperandImpl(),
     m_name(name)
  {};

  const char* const m_name;
}; // class NdbParamOperandImpl


/////////////////////////////////////////////////////
// Pure virtual baseclass for ConstOperand.
// Each specific const datatype has its own subclass.
/////////////////////////////////////////////////////
class NdbConstOperandImpl :
  public NdbConstOperand,
  public NdbQueryOperandImpl
{
  friend class NdbQueryBuilder;  // Allow privat access from builder interface
public:
  virtual size_t getLength() const = 0;
  virtual const void* getAddr() const = 0;

protected:
  virtual ~NdbConstOperandImpl() {};
  NdbConstOperandImpl ()
   : NdbConstOperand(this), NdbQueryOperandImpl()
  {};
}; // class NdbConstOperandImpl

//////////////////////////////////////////////////
// Implements different const datatypes by further
// subclassing of NdbConstOperand.
//////////////////////////////////////////////////
class NdbInt32ConstOperandImpl : public NdbConstOperandImpl
{
public:
  NdbInt32ConstOperandImpl (Int32 value) : NdbConstOperandImpl(), m_value(value) {};
  size_t getLength()    const { return sizeof(m_value); };
  const void* getAddr() const { return &m_value; };
private:
  const Int32 m_value;
};

class NdbUint32ConstOperandImpl : public NdbConstOperandImpl
{
public:
  NdbUint32ConstOperandImpl (Uint32 value) : NdbConstOperandImpl(), m_value(value) {};
  size_t getLength()    const { return sizeof(m_value); };
  const void* getAddr() const { return &m_value; };
private:
  const Uint32 m_value;
};

class NdbInt64ConstOperandImpl : public NdbConstOperandImpl
{
public:
  NdbInt64ConstOperandImpl (Int64 value) : NdbConstOperandImpl(), m_value(value) {};
  size_t getLength()    const { return sizeof(m_value); };
  const void* getAddr() const { return &m_value; };
private:
  const Int64 m_value;
};

class NdbUint64ConstOperandImpl : public NdbConstOperandImpl
{
public:
  NdbUint64ConstOperandImpl (Uint64 value) : NdbConstOperandImpl(), m_value(value) {};
  size_t getLength()    const { return sizeof(m_value); };
  const void* getAddr() const { return &m_value; };
private:
  const Uint64 m_value;
};

class NdbCharConstOperandImpl : public NdbConstOperandImpl
{
public:
  NdbCharConstOperandImpl (const char* value) : NdbConstOperandImpl(), m_value(value) {};
  size_t getLength()    const { return strlen(m_value); };
  const void* getAddr() const { return m_value; };
private:
  const char* const m_value;
};

class NdbGenericConstOperandImpl : public NdbConstOperandImpl
{
public:
  NdbGenericConstOperandImpl (const void* value, size_t length)
  : NdbConstOperandImpl(), m_value(value), m_length(length)
  {};

  size_t getLength()    const { return m_length; };
  const void* getAddr() const { return m_value; };
private:
  const void* const m_value;
  const size_t m_length;
};

////////////////////////////////////////////////
// Implementation of NdbQueryOperation interface
////////////////////////////////////////////////

class NdbQueryOperationDefImpl
{
public:
  Uint32 getNoOfParentOperations() const
  { return 0; };  // FIXME.

  const NdbQueryOperationDef* getParentOperation(Uint32 i) const
  { return 0; };  // FIXME.

  Uint32 getNoOfChildOperations() const
  { return 0; };  // FIXME.

  const NdbQueryOperationDef* getChildOperation(Uint32 i) const
  { return 0; };  // FIXME.

  const NdbDictionary::Table* getTable() const
  { return m_table; };

protected:
  virtual ~NdbQueryOperationDefImpl() {};
  NdbQueryOperationDefImpl (
                           const NdbDictionary::Table* table,
                           const char* ident)
   : m_table(table), m_ident(ident) {};

private:
  const NdbDictionary::Table* const m_table;
  const char* const m_ident;
}; // class NdbQueryOperationDefImpl


class NdbQueryLookupOperationDefImpl :
  public NdbQueryLookupOperationDef,
  public NdbQueryOperationDefImpl
{
  friend class NdbQueryBuilder;  // Allow privat access from builder interface

public:

  const NdbDictionary::Index* getIndex() const
  { return m_index; };

private:
  virtual ~NdbQueryLookupOperationDefImpl() {};
  NdbQueryLookupOperationDefImpl (
                           const NdbDictionary::Table* table,
                           const NdbQueryOperand* const keys[],
                           const char* ident)
   : NdbQueryLookupOperationDef(this), NdbQueryOperationDefImpl(table,ident),
     m_index(0), m_keys(keys)
  {};
  NdbQueryLookupOperationDefImpl (
                           const NdbDictionary::Index* index,
                           const NdbDictionary::Table* table,
                           const NdbQueryOperand* const keys[],
                           const char* ident)
   : NdbQueryLookupOperationDef(this), NdbQueryOperationDefImpl(table,ident),
     m_index(index), m_keys(keys)
  {};

private:
  const NdbDictionary::Index* const m_index;
  const NdbQueryOperand* const *m_keys;
}; // class NdbQueryLookupOperationDefImpl


class NdbQueryScanOperationDefImpl :
  public NdbQueryOperationDefImpl
{
public:
  virtual ~NdbQueryScanOperationDefImpl() {};
  NdbQueryScanOperationDefImpl (
                           const NdbDictionary::Table* table,
                           const char* ident)
  : NdbQueryOperationDefImpl(table,ident)
  {};
}; // class NdbQueryScanOperationDefImpl

class NdbQueryTableScanOperationDefImpl :
  public NdbQueryTableScanOperationDef,
  public NdbQueryScanOperationDefImpl
{
  friend class NdbQueryBuilder;  // Allow privat access from builder interface

private:
  virtual ~NdbQueryTableScanOperationDefImpl() {};
  NdbQueryTableScanOperationDefImpl (
                           const NdbDictionary::Table* table,
                           const char* ident)
  : NdbQueryTableScanOperationDef(this), NdbQueryScanOperationDefImpl(table,ident)
  {};
}; // class NdbQueryTableScanOperationDefImpl


class NdbQueryIndexScanOperationDefImpl :
  public NdbQueryIndexScanOperationDef,
  public NdbQueryScanOperationDefImpl
{
  friend class NdbQueryBuilder;  // Allow privat access from builder interface

public:
  const NdbDictionary::Index* getIndex() const
  { return m_index; };

private:
  virtual ~NdbQueryIndexScanOperationDefImpl() {};
  NdbQueryIndexScanOperationDefImpl (
                           const NdbDictionary::Index* index,
                           const NdbDictionary::Table* table,
                           const NdbQueryIndexBound* bound,
                           const char* ident)
  : NdbQueryIndexScanOperationDef(this), NdbQueryScanOperationDefImpl(table,ident),
    m_index(index), m_bound(bound)
  {};

private:
  const NdbDictionary::Index* const m_index;
  const NdbQueryIndexBound* const m_bound;
}; // class NdbQueryIndexScanOperationDefImpl


class NdbQueryDefImpl : public NdbQueryDef
{
private:
}; // class NdbQueryDefImpl

/**
 * class NdbQueryBuilder is not (yet?) hidden in a pimpl object.
 *
 */
// class NdbQueryBuilderImpl : public NdbQueryBuilder {};

/*************************************************************************
 * Glue layer between NdbQueryOperand interface and its Impl'ementation.
 ************************************************************************/
NdbQueryDef::NdbQueryDef()
{}

NdbQueryOperand::NdbQueryOperand(NdbQueryOperandImpl* pimpl) : m_pimpl(pimpl)
{ assert(pimpl!=NULL); }
NdbConstOperand::NdbConstOperand(NdbQueryOperandImpl* pimpl) : NdbQueryOperand(pimpl)
{}
NdbParamOperand::NdbParamOperand(NdbQueryOperandImpl* pimpl) : NdbQueryOperand(pimpl)
{}
NdbLinkedOperand::NdbLinkedOperand(NdbQueryOperandImpl* pimpl) : NdbQueryOperand(pimpl)
{}

// D'tors
NdbQueryOperand::~NdbQueryOperand()
{}
NdbConstOperand::~NdbConstOperand()
{}
NdbParamOperand::~NdbParamOperand()
{}
NdbLinkedOperand::~NdbLinkedOperand()
{}

/**
 * Get'ers for NdbQueryOperand...Impl object.
 * Functions overridden to supply 'impl' casted to the correct '...OperandImpl' type
 * for each available interface class.
 */
inline NdbQueryOperandImpl&
NdbQueryOperand::getImpl() const
{ return *m_pimpl; // Asserted != NULL in C'tor
}
inline static
NdbQueryOperandImpl& getImpl(const NdbQueryOperand* op)
{ return op->getImpl();
}
inline static
NdbConstOperandImpl& getImpl(const NdbConstOperand* op)
{ return static_cast<NdbConstOperandImpl&>(op->getImpl());
}
inline static
NdbParamOperandImpl& getImpl(const NdbParamOperand* op)
{ return static_cast<NdbParamOperandImpl&>(op->getImpl());
}
inline static
NdbLinkedOperandImpl& getImpl(const NdbLinkedOperand* op)
{ return static_cast<NdbLinkedOperandImpl&>(op->getImpl());
}

const NdbDictionary::Column*
NdbQueryOperand::getColumn() const
{
  return ::getImpl(this).getColumn();
}

const char*
NdbParamOperand::getName() const
{
  return ::getImpl(this).getName();
}

Uint32
NdbParamOperand::getEnum() const
{
  return ::getImpl(this).getEnum();
}

/****************************************************************************
 * Glue layer between NdbQueryOperationDef interface and its Impl'ementation.
 ****************************************************************************/
NdbQueryOperationDef::NdbQueryOperationDef(NdbQueryOperationDefImpl *pimpl) : m_pimpl(pimpl)
{ assert(pimpl!=NULL); }
NdbQueryLookupOperationDef::NdbQueryLookupOperationDef(NdbQueryOperationDefImpl *pimpl) : NdbQueryOperationDef(pimpl) 
{}
NdbQueryScanOperationDef::NdbQueryScanOperationDef(NdbQueryOperationDefImpl *pimpl) : NdbQueryOperationDef(pimpl) 
{}
NdbQueryTableScanOperationDef::NdbQueryTableScanOperationDef(NdbQueryOperationDefImpl *pimpl) : NdbQueryScanOperationDef(pimpl) 
{}
NdbQueryIndexScanOperationDef::NdbQueryIndexScanOperationDef(NdbQueryOperationDefImpl *pimpl) : NdbQueryScanOperationDef(pimpl) 
{}


// D'tors
NdbQueryOperationDef::~NdbQueryOperationDef()
{}
NdbQueryLookupOperationDef::~NdbQueryLookupOperationDef()
{}
NdbQueryScanOperationDef::~NdbQueryScanOperationDef()
{}
NdbQueryTableScanOperationDef::~NdbQueryTableScanOperationDef()
{}
NdbQueryIndexScanOperationDef::~NdbQueryIndexScanOperationDef()
{}


/**
 * Get'ers for QueryOperation...DefImpl object.
 * Functions overridden to supply 'impl' casted to the correct '...DefImpl' type
 * for each available interface class.
 */ 
NdbQueryOperationDefImpl&
NdbQueryOperationDef::getImpl() const
{ return *m_pimpl;  // Asserted != NULL in C'tor
}
inline static
NdbQueryOperationDefImpl& getImpl(const NdbQueryOperationDef* op)
{ return op->getImpl();
}
inline static
NdbQueryLookupOperationDefImpl& getImpl(const NdbQueryLookupOperationDef* op)
{ return static_cast<NdbQueryLookupOperationDefImpl&>(op->getImpl());
}
inline static
NdbQueryTableScanOperationDefImpl& getImpl(const NdbQueryTableScanOperationDef* op)
{ return static_cast<NdbQueryTableScanOperationDefImpl&>(op->getImpl());
}
inline static
NdbQueryIndexScanOperationDefImpl& getImpl(const NdbQueryIndexScanOperationDef* op)
{ return static_cast<NdbQueryIndexScanOperationDefImpl&>(op->getImpl());
}



Uint32
NdbQueryOperationDef::getNoOfParentOperations() const
{
  return ::getImpl(this).getNoOfParentOperations();
}

const NdbQueryOperationDef*
NdbQueryOperationDef::getParentOperation(Uint32 i) const
{
  return ::getImpl(this).getParentOperation(i);
}

Uint32 
NdbQueryOperationDef::getNoOfChildOperations() const
{
  return ::getImpl(this).getNoOfChildOperations();
}

const NdbQueryOperationDef* 
NdbQueryOperationDef::getChildOperation(Uint32 i) const
{
  return ::getImpl(this).getChildOperation(i);
}

const NdbDictionary::Table*
NdbQueryOperationDef::getTable() const
{
  return ::getImpl(this).getTable();
}

const NdbDictionary::Index*
NdbQueryLookupOperationDef::getIndex() const
{
  return ::getImpl(this).getIndex();
}


/*******************************************
 * Implementation of NdbQueryBuilder factory
 ******************************************/
NdbQueryBuilder::NdbQueryBuilder(Ndb& ndb)
: m_ndb(ndb), m_error()
{}

NdbQueryBuilder::~NdbQueryBuilder()
{}

//////////////////////////////////////////////////
// Implements different const datatypes by further
// subclassing of NdbConstOperand.
/////////////////////////////////////////////////
NdbConstOperand* 
NdbQueryBuilder::constValue(const char* value)
{
  NdbConstOperandImpl* constOp = new NdbCharConstOperandImpl(value);
  return constOp;
}
NdbConstOperand* 
NdbQueryBuilder::constValue(const void* value, size_t length)
{
  NdbConstOperandImpl* constOp = new NdbGenericConstOperandImpl(value,length);
  return constOp;
}
NdbConstOperand* 
NdbQueryBuilder::constValue(Int32 value)
{
  NdbConstOperandImpl* constOp = new NdbInt32ConstOperandImpl(value);
  return constOp;
}
NdbConstOperand* 
NdbQueryBuilder::constValue(Uint32 value)
{
  NdbConstOperandImpl* constOp = new NdbUint32ConstOperandImpl(value);
  return constOp;
}
NdbConstOperand* 
NdbQueryBuilder::constValue(Int64 value)
{
  NdbConstOperandImpl* constOp = new NdbInt64ConstOperandImpl(value);
  return constOp;
}
NdbConstOperand* 
NdbQueryBuilder::constValue(Uint64 value)
{
  NdbConstOperandImpl* constOp = new NdbUint64ConstOperandImpl(value);
  return constOp;
}

NdbParamOperand* 
NdbQueryBuilder::paramValue(const char* name)
{
  NdbParamOperandImpl* paramOp = new NdbParamOperandImpl(name);
  return paramOp;
}

NdbLinkedOperand* 
NdbQueryBuilder::linkedValue(const NdbQueryOperationDef* parent, const char* attr)
{
  if (parent && attr)
  { NdbLinkedOperandImpl* linkedOp = new NdbLinkedOperandImpl(parent,attr);
    return linkedOp;
  }
  return NULL;
}


NdbQueryLookupOperationDef*
NdbQueryBuilder::readTuple(const NdbDictionary::Table* table,    // Primary key lookup
                           const NdbQueryOperand* const keys[],  // Terminated by NULL element 
                           const char* ident)
{
  if (table)
  {
    NdbQueryLookupOperationDefImpl* op =
      new NdbQueryLookupOperationDefImpl(table,keys,ident);
    return op;
  }

//  setOperationErrorCodeAbort(4271);
  return NULL;
}

NdbQueryLookupOperationDef*
NdbQueryBuilder::readTuple(const NdbDictionary::Index* index,    // Unique key lookup w/ index
                           const NdbDictionary::Table* table,    // Primary key lookup
                           const NdbQueryOperand* const keys[],  // Terminated by NULL element 
                           const char* ident)
{
  if (index && table)
  {
    NdbQueryLookupOperationDefImpl* op = 
      new NdbQueryLookupOperationDefImpl(index,table,keys,ident);
    return op;
  }
//  setOperationErrorCodeAbort(4271);
  return NULL;
}


NdbQueryTableScanOperationDef*
NdbQueryBuilder::scanTable(const NdbDictionary::Table* table,
                           const char* ident)
{
  NdbQueryTableScanOperationDefImpl* op =
    new NdbQueryTableScanOperationDefImpl(table,ident);

  return op;
}


NdbQueryIndexScanOperationDef*
NdbQueryBuilder::scanIndex(const NdbDictionary::Index* index, 
	                   const NdbDictionary::Table* table,
                           const NdbQueryIndexBound* bound,
                           const char* ident)
{
  NdbQueryIndexScanOperationDefImpl* op =
    new NdbQueryIndexScanOperationDefImpl(index,table,bound,ident);

  return op;
}



NdbQueryDef*
NdbQueryBuilder::prepare()
{
  return new NdbQueryDefImpl();
}


#if 0
/**********************************************
 * Simple hack for module test & experimenting
 **********************************************/
#include <stdio.h>
#include <assert.h>

int
main(int argc, const char** argv)
{
  printf("Hello, I am the unit test for NdbQueryBuilder\n");

  printf("sizeof(NdbQueryOperationDef): %d\n", sizeof(NdbQueryOperationDef));
  printf("sizeof(NdbQueryLookupOperationDef): %d\n", sizeof(NdbQueryLookupOperationDef));

  // Assert that interfaces *only* contain the pimpl pointer:
  assert (sizeof(NdbQueryOperationDef) == sizeof(NdbQueryOperationDefImpl*));
  assert (sizeof(NdbQueryLookupOperationDef) == sizeof(NdbQueryOperationDefImpl*));
  assert (sizeof(NdbQueryTableScanOperationDef) == sizeof(NdbQueryOperationDefImpl*));
  assert (sizeof(NdbQueryIndexScanOperationDef) == sizeof(NdbQueryOperationDefImpl*));

  assert (sizeof(NdbQueryOperand) == sizeof(NdbQueryOperandImpl*));
  assert (sizeof(NdbConstOperand) == sizeof(NdbQueryOperandImpl*));
  assert (sizeof(NdbParamOperand) == sizeof(NdbQueryOperandImpl*));
  assert (sizeof(NdbLinkedOperand) == sizeof(NdbQueryOperandImpl*));

  Ndb *myNdb = 0;
  NdbQueryBuilder myBuilder(*myNdb);

  const NdbDictionary::Table *manager = (NdbDictionary::Table*)0xDEADBEAF;
//  const NdbDictionary::Index *ix = (NdbDictionary::Index*)0x11223344;

  NdbQueryDef* q1 = 0;
  {
    NdbQueryBuilder* qb = &myBuilder; //myDict->getQueryBuilder();

    const NdbQueryOperand* managerKey[] =  // Manager is indexed om {"dept_no", "emp_no"}
    {  qb->constValue("d005"),             // dept_no = "d005"
       qb->constValue(110567),             // emp_no  = 110567
       0
    };

    const NdbQueryLookupOperationDef *readManager = qb->readTuple(manager, managerKey);
//  if (readManager == NULL) APIERROR(myNdb.getNdbError());
    assert (readManager);

    printf("readManager : %p\n", readManager);
    printf("Index : %p\n", readManager->getIndex());
    printf("Table : %p\n", readManager->getTable());

    q1 = qb->prepare();
//  if (q1 == NULL) APIERROR(qb->getNdbError());
    assert (q1);

    // Some operations are intentionally disallowed through private declaration 
//  delete readManager;
//  NdbQueryLookupOperationDef illegalAssign = *readManager;
//  NdbQueryLookupOperationDef *illegalCopy1 = new NdbQueryLookupOperationDef(*readManager);
//  NdbQueryLookupOperationDef illegalCopy2(*readManager);
  }
}

#endif
