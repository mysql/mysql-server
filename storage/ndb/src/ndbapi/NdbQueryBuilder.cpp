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
#include <Vector.hpp>

#include "Ndb.hpp"
#include "NdbQueryBuilder.hpp"
#include "NdbQueryBuilderImpl.hpp"
#include "NdbDictionary.hpp"

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
 *   - Impl classes does not have to be friend of the interface classes.
 *
 * ::getImpl() functions has been defined for convenient access 
 * to all available interface classes.
 *
 * CODE STATUS:
 *   Except for creating the Query objects, the NdbQueryBuilder factory
 *   does not do any usefull work yet. This is a framework for further
 *   logic to be added.
 * 
 */

static void
setErrorCode(NdbQueryBuilderImpl* qb, int aErrorCode)
{ qb->setErrorCode(aErrorCode);
}

static void
setErrorCode(NdbQueryBuilder* qb, int aErrorCode)
{ qb->getImpl().setErrorCode(aErrorCode);
}


//////////////////////////////////////////////////////////////////
// Convenient macro for returning specific errorcodes if
// 'cond' does not evaluate to true.
//////////////////////////////////////////////////////////////////
#define returnErrIf(cond,err)		\
  if (unlikely((cond)))			\
  { ::setErrorCode(this,err);		\
    return NULL;			\
  }


//////////////////////////////////////////////
// Implementation of NdbQueryOperand interface
//////////////////////////////////////////////


// Baseclass for the QueryOperand implementation
class NdbQueryOperandImpl
{
public:
  const NdbDictionary::Column* getColumn() const
  { return m_column; };

  virtual int bindOperand(const NdbDictionary::Column* column,
                          NdbQueryOperationDef* operation)
  { m_column = column;
    return 0;
  }

protected:
  virtual ~NdbQueryOperandImpl() {};
  friend NdbQueryBuilderImpl::~NdbQueryBuilderImpl();

  NdbQueryOperandImpl()
  : m_column(0) {}

private:
  const NdbDictionary::Column* m_column;  // Initial NULL, assigned w/ bindOperand()
}; // class NdbQueryOperandImpl


class NdbLinkedOperandImpl :
  public NdbLinkedOperand,
  public NdbQueryOperandImpl
{
  friend class NdbQueryBuilder;  // Allow privat access from builder interface

public:
  virtual int bindOperand(const NdbDictionary::Column* column,
                          NdbQueryOperationDef* operation);

private:
  virtual ~NdbLinkedOperandImpl() {};
  NdbLinkedOperandImpl (const NdbQueryOperationDef* parent, 
                        const NdbDictionary::Column* column)
   : NdbLinkedOperand(this), NdbQueryOperandImpl(),
     m_parent(parent), m_column(column)
  {};

  const NdbQueryOperationDef* const m_parent;
  const NdbDictionary::Column* const m_column;
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

  virtual int bindOperand(const NdbDictionary::Column* column,
                          NdbQueryOperationDef* operation);

  virtual NdbDictionary::Column::Type getType() const = 0;

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
  NdbDictionary::Column::Type getType() const { return NdbDictionary::Column::Int; };
private:
  const Int32 m_value;
};

class NdbUint32ConstOperandImpl : public NdbConstOperandImpl
{
public:
  NdbUint32ConstOperandImpl (Uint32 value) : NdbConstOperandImpl(), m_value(value) {};
  size_t getLength()    const { return sizeof(m_value); };
  const void* getAddr() const { return &m_value; };
  NdbDictionary::Column::Type getType() const { return NdbDictionary::Column::Unsigned; };
private:
  const Uint32 m_value;
};

class NdbInt64ConstOperandImpl : public NdbConstOperandImpl
{
public:
  NdbInt64ConstOperandImpl (Int64 value) : NdbConstOperandImpl(), m_value(value) {};
  size_t getLength()    const { return sizeof(m_value); };
  const void* getAddr() const { return &m_value; };
  NdbDictionary::Column::Type getType() const { return NdbDictionary::Column::Bigint; };
private:
  const Int64 m_value;
};

class NdbUint64ConstOperandImpl : public NdbConstOperandImpl
{
public:
  NdbUint64ConstOperandImpl (Uint64 value) : NdbConstOperandImpl(), m_value(value) {};
  size_t getLength()    const { return sizeof(m_value); };
  const void* getAddr() const { return &m_value; };
  NdbDictionary::Column::Type getType() const { return NdbDictionary::Column::Bigunsigned; };
private:
  const Uint64 m_value;
};

class NdbCharConstOperandImpl : public NdbConstOperandImpl
{
public:
  NdbCharConstOperandImpl (const char* value) : NdbConstOperandImpl(), m_value(value) {};
  size_t getLength()    const { return strlen(m_value); };
  const void* getAddr() const { return m_value; };
  NdbDictionary::Column::Type getType() const { return NdbDictionary::Column::Char; };
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
  NdbDictionary::Column::Type getType() const { return NdbDictionary::Column::Undefined; };
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
  { return m_parents.size(); };

  const NdbQueryOperationDef* getParentOperation(Uint32 i) const
  { return m_parents[i]; };

  Uint32 getNoOfChildOperations() const
  { return m_children.size(); };

  const NdbQueryOperationDef* getChildOperation(Uint32 i) const
  { return m_children[i]; };

  const NdbDictionary::Table* getTable() const
  { return m_table; };

  void addParent(const NdbQueryOperationDef *);
  void addChild(const NdbQueryOperationDef *);

protected:
  virtual ~NdbQueryOperationDefImpl() {};
  friend NdbQueryBuilderImpl::~NdbQueryBuilderImpl();
  friend NdbQueryDefImpl::~NdbQueryDefImpl();

  NdbQueryOperationDefImpl (
                           const NdbDictionary::Table* table,
                           const char* ident)
   : m_table(table), m_ident(ident),
     m_parents(), m_children()
 {};

private:
  const NdbDictionary::Table* const m_table;
  const char* const m_ident;

  // parent / child vectors are indexes into m_operation vector
  // which contains the real pointers to parent/child operations
  Vector<const NdbQueryOperationDef*> m_parents;
  Vector<const NdbQueryOperationDef*> m_children;

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



///////////////////////////////////////////////////
/////// End 'Impl' class declarations /////////////
///////////////////////////////////////////////////

NdbQueryDef::NdbQueryDef(NdbQueryDefImpl* pimpl) : m_pimpl(pimpl)
{}
NdbQueryDef::~NdbQueryDef()
{}

Uint32
NdbQueryDef::getNoOfOperations() const
{ return m_pimpl->m_operations.size();
}

const NdbQueryOperationDef*
NdbQueryDef::getQueryOperation(Uint32 index) const
{ return m_pimpl->m_operations[index];
}

const NdbQueryOperationDef*
NdbQueryDef::getQueryOperation(const char* ident) const
{ return NULL;  // FIXME
}

int
NdbQueryDef::getQueryOperationIx(const NdbQueryOperationDef* opDef) const
{
  for (int i=0; i<m_pimpl->m_operations.size(); ++i)
  { if (m_pimpl->m_operations[i] == opDef)
      return i;
  }
  return -1;
}


/*************************************************************************
 * Glue layer between NdbQueryOperand interface and its Impl'ementation.
 ************************************************************************/

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
: m_pimpl(new NdbQueryBuilderImpl(ndb))
{}

NdbQueryBuilder::~NdbQueryBuilder()
{ delete m_pimpl;
}

inline NdbQueryBuilderImpl&
NdbQueryBuilder::getImpl() const
{ return *m_pimpl;
}

const NdbError&
NdbQueryBuilder::getNdbError() const
{
  return m_pimpl->getNdbError();
}

//////////////////////////////////////////////////
// Implements different const datatypes by further
// subclassing of NdbConstOperand.
/////////////////////////////////////////////////
NdbConstOperand* 
NdbQueryBuilder::constValue(const char* value)
{
  returnErrIf(value==0,4800);
  NdbConstOperandImpl* constOp = new NdbCharConstOperandImpl(value);
  returnErrIf(constOp==0,4000);

  m_pimpl->m_constOperands.push_back(constOp);
  return constOp;
}
NdbConstOperand* 
NdbQueryBuilder::constValue(const void* value, size_t length)
{
  returnErrIf(value==0 && length>0,4800);
  NdbConstOperandImpl* constOp = new NdbGenericConstOperandImpl(value,length);
  returnErrIf(constOp==0,4000);

  m_pimpl->m_constOperands.push_back(constOp);
  return constOp;
}
NdbConstOperand* 
NdbQueryBuilder::constValue(Int32 value)
{
  NdbConstOperandImpl* constOp = new NdbInt32ConstOperandImpl(value);
  returnErrIf(constOp==0,4000);

  m_pimpl->m_constOperands.push_back(constOp);
  return constOp;
}
NdbConstOperand* 
NdbQueryBuilder::constValue(Uint32 value)
{
  NdbConstOperandImpl* constOp = new NdbUint32ConstOperandImpl(value);
  returnErrIf(constOp==0,4000);

  m_pimpl->m_constOperands.push_back(constOp);
  return constOp;
}
NdbConstOperand* 
NdbQueryBuilder::constValue(Int64 value)
{
  NdbConstOperandImpl* constOp = new NdbInt64ConstOperandImpl(value);
  returnErrIf(constOp==0,4000);

  m_pimpl->m_constOperands.push_back(constOp);
  return constOp;
}
NdbConstOperand* 
NdbQueryBuilder::constValue(Uint64 value)
{
  NdbConstOperandImpl* constOp = new NdbUint64ConstOperandImpl(value);
  returnErrIf(constOp==0,4000);

  m_pimpl->m_constOperands.push_back(constOp);
  return constOp;
}

NdbParamOperand* 
NdbQueryBuilder::paramValue(const char* name)
{
  NdbParamOperandImpl* paramOp = new NdbParamOperandImpl(name);
  returnErrIf(paramOp==0,4000);

  m_pimpl->m_paramOperands.push_back(paramOp);
  return paramOp;
}

NdbLinkedOperand* 
NdbQueryBuilder::linkedValue(const NdbQueryOperationDef* parent, const char* attr)
{
  returnErrIf(parent==0 || attr==0, 4800);  // Required non-NULL arguments

  // Parent should be a OperationDef contained in this query builder context
  returnErrIf(!m_pimpl->contains(parent), 4804); // Unknown parent

  // 'attr' should refer a column from the underlying table in parent:
  const NdbDictionary::Column* column = parent->getTable()->getColumn(attr);
  returnErrIf(column==0, 4805); // Unknown column

  NdbLinkedOperandImpl* linkedOp = new NdbLinkedOperandImpl(parent,column);
  returnErrIf(linkedOp==0, 4000);

  m_pimpl->m_linkedOperands.push_back(linkedOp);
  return linkedOp;
}


NdbQueryLookupOperationDef*
NdbQueryBuilder::readTuple(const NdbDictionary::Table* table,    // Primary key lookup
                           const NdbQueryOperand* const keys[],  // Terminated by NULL element 
                           const char* ident)
{
  if (m_pimpl->hasError())
    return NULL;

  int i;
  returnErrIf(table==0 || keys==0, 4800);  // Required non-NULL arguments

  // All column values being part of primary key should be specified:
  int keyfields = table->getNoOfPrimaryKeys();
  int colcount = table->getNoOfColumns();

  // Check: keys[] are specified for all fields in PK
  for (i=0; i<keyfields; ++i)
  {
    returnErrIf(keys[i]==NULL, 4801);  // A 'Key' value is undefineds
  }
  // Check for propper NULL termination of keys[] spec
  returnErrIf(keys[keyfields]!=NULL, 4802);

  NdbQueryLookupOperationDefImpl* op =
    new NdbQueryLookupOperationDefImpl(table,keys,ident);
  returnErrIf(op==0, 4000);

  int keyindex = 0;
  for (i=0; i<colcount; ++i)
  {
    const NdbDictionary::Column *col = table->getColumn(i);
    if (col->getPrimaryKey())
    {
      int error = keys[keyindex]->getImpl().bindOperand(col,op);
      if (unlikely(error))
      { m_pimpl->setErrorCode(error);
        delete op;
        return NULL;
      }

      keyindex++;
      if (keyindex >= keyfields)
        break;  // Seen all PK fields
    }
  }
  
  m_pimpl->m_operations.push_back(op);
  return op;
}


NdbQueryLookupOperationDef*
NdbQueryBuilder::readTuple(const NdbDictionary::Index* index,    // Unique key lookup w/ index
                           const NdbDictionary::Table* table,    // Primary key lookup
                           const NdbQueryOperand* const keys[],  // Terminated by NULL element 
                           const char* ident)
{
  if (m_pimpl->hasError())
    return NULL;
  returnErrIf(table==0 || index==0 || keys==0, 4800);  // Required non-NULL arguments

  NdbQueryLookupOperationDefImpl* op = 
    new NdbQueryLookupOperationDefImpl(index,table,keys,ident);
  returnErrIf(op==0, 4000);

  m_pimpl->m_operations.push_back(op);
  return op;
}


NdbQueryTableScanOperationDef*
NdbQueryBuilder::scanTable(const NdbDictionary::Table* table,
                           const char* ident)
{
  if (m_pimpl->hasError())
    return NULL;
  returnErrIf(table==0, 4800);  // Required non-NULL arguments

  NdbQueryTableScanOperationDefImpl* op =
    new NdbQueryTableScanOperationDefImpl(table,ident);
  returnErrIf(op==0, 4000);

  m_pimpl->m_operations.push_back(op);
  return op;
}


NdbQueryIndexScanOperationDef*
NdbQueryBuilder::scanIndex(const NdbDictionary::Index* index, 
	                   const NdbDictionary::Table* table,
                           const NdbQueryIndexBound* bound,
                           const char* ident)
{
  if (m_pimpl->hasError())
    return NULL;
  returnErrIf(table==0 || index==0 || bound==0, 4800);  // Required non-NULL arguments

  NdbQueryIndexScanOperationDefImpl* op =
    new NdbQueryIndexScanOperationDefImpl(index,table,bound,ident);
  returnErrIf(op==0, 4000);

  m_pimpl->m_operations.push_back(op);
  return op;
}

NdbQueryDef*
NdbQueryBuilder::prepare()
{
  return m_pimpl->prepare();
}

////////////////////////////////////////
// The (hidden) Impl of NdbQueryBuilder
////////////////////////////////////////

NdbQueryBuilderImpl::NdbQueryBuilderImpl(Ndb& ndb)
: m_ndb(ndb), m_error(), m_operations(),
  m_paramOperands(), m_constOperands(), m_linkedOperands()
{}

NdbQueryBuilderImpl::~NdbQueryBuilderImpl()
{
  Uint32 i;

  // Delete all operand and operator in Vector's
  for (i=0; i<m_operations.size(); ++i)
  { delete &m_operations[i]->getImpl();
  }
  for (i=0; i<m_paramOperands.size(); ++i)
  { delete &m_paramOperands[i]->getImpl();
  }
  for (i=0; i<m_constOperands.size(); ++i)
  { delete &m_constOperands[i]->getImpl();
  }
  for (i=0; i<m_linkedOperands.size(); ++i)
  { delete &m_linkedOperands[i]->getImpl();
  }
}


bool 
NdbQueryBuilderImpl::contains(const NdbQueryOperationDef* opDef)
{
  for (Uint32 i=0; i<m_operations.size(); ++i)
  { if (m_operations[i] == opDef)
      return true;
  }
  return false;
}


NdbQueryDef*
NdbQueryBuilderImpl::prepare()
{
/****
  // FIXME: Install named OperationDef's in HashMap
  for (i = 0; i<m_operation.size(); ++i)
  { const NdbQueryOperationDef *def = m_operations[i];
  }
****/

  NdbQueryDefImpl* def = new NdbQueryDefImpl(*this);
  returnErrIf(def==0, 4000);

  m_operations.clear();
  m_paramOperands.clear();
  m_constOperands.clear();
  m_linkedOperands.clear();

  return def;
}

///////////////////////////////////
// The (hidden) Impl of NdbQueryDef
///////////////////////////////////
NdbQueryDefImpl::NdbQueryDefImpl(const NdbQueryBuilderImpl& builder)
 : NdbQueryDef(this), 
   m_operations(builder.m_operations)
{}

NdbQueryDefImpl::~NdbQueryDefImpl()
{
  // Release all NdbQueryOperations
  for (Uint32 i=0; i<m_operations.size(); ++i)
  { delete &m_operations[i]->getImpl();
  }
}




void
NdbQueryOperationDefImpl::addParent(const NdbQueryOperationDef *opDef)
{
  for (Uint32 i=0; i<m_parents.size(); ++i)
  { if (m_parents[i] == opDef)
      return;
  }
  m_parents.push_back(opDef);
}

void
NdbQueryOperationDefImpl::addChild(const NdbQueryOperationDef *opDef)
{
  for (Uint32 i=0; i<m_children.size(); ++i)
  { if (m_children[i] == opDef)
      return;
  }
  m_children.push_back(opDef);
}


int
NdbLinkedOperandImpl::bindOperand(
                           const NdbDictionary::Column* column,
                           NdbQueryOperationDef* operation)
{
  NdbDictionary::Column::Type type = column->getType();
  if (type != m_column->getType())
    return 4803;  // Incompatible datatypes

  // TODO? Check length if Char, and prec,scale if decimal type

  // Register parent/child relations
  this->m_parent->getImpl().addChild(operation);
  operation->getImpl().addParent(this->m_parent);

  return NdbQueryOperandImpl::bindOperand(column,operation);
}


int
NdbConstOperandImpl::bindOperand(
                           const NdbDictionary::Column* column,
                           NdbQueryOperationDef* operation)
{
  NdbDictionary::Column::Type type = column->getType();
  if (type != this->getType())
    return 4803;  // Incompatible datatypes

  // TODO? Check length if Char, and prec,scale if decimal type

  return NdbQueryOperandImpl::bindOperand(column,operation);
}




// Instantiate Vector templates
template class Vector<const NdbQueryOperationDef*>;
template class Vector<const NdbParamOperand*>;
template class Vector<const NdbConstOperand*>;
template class Vector<const NdbLinkedOperand*>;


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
