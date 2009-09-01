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

#include "NdbQueryBuilder.hpp"
#include "NdbQueryBuilderImpl.hpp"
#include "NdbIndexScanOperation.hpp"  // Temp intil we remove NdbOperation dependencies
#include <ndb_global.h>
#include <Vector.hpp>
#include "signaldata/QueryTree.hpp"

#include "Ndb.hpp"
#include "NdbDictionary.hpp"
#include "NdbDictionaryImpl.hpp"
#include "AttributeHeader.hpp"
#include "NdbRecord.hpp"              // Temp as above
#include "NdbOut.hpp"


/**
 * Implementation of all QueryBuilder objects are hidden from
 * both the API interface and other internals in the NDBAPI using the
 * pimpl idiom.
 *
 * The object hierarch visible through the interface has its 'Impl'
 * counterparts inside this module. Some classes are
 * even subclassed further as part of the implementation.
 * (Particular the ConstOperant in order to implement multiple datatypes)
 *
 * In order to avoid allocating both an interface object and its particular
 * Impl object, all 'final' Impl objects 'has a' interface object 
 * which is accessible through the virtual method ::getInterface.
 *
 * ::getImpl() methods has been defined for convenient access 
 * to all available Impl classes.
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
//
// The common baseclass 'class NdbQueryOperandImpl',
// and its 'const', 'linked' and 'param' subclasses are
// defined in "NdbQueryBuilderImpl.hpp"
// The 'const' operand subclass is a pure virtual baseclass
// which has different type specific subclasses defined below:
//////////////////////////////////////////////////////////////

//////////////////////////////////////////////////
// Implements different const datatypes by further
// subclassing of the baseclass NdbConstOperand.
//////////////////////////////////////////////////
class NdbInt32ConstOperandImpl : public NdbConstOperandImpl
{
public:
  explicit NdbInt32ConstOperandImpl (Int32 value) : 
    NdbConstOperandImpl(), 
    m_value(value) {}
  size_t getLength()    const { return sizeof(m_value); }
  const void* getAddr() const { return &m_value; }
  NdbDictionary::Column::Type getType() const { return NdbDictionary::Column::Int; }
private:
  const Int32 m_value;
};

class NdbUint32ConstOperandImpl : public NdbConstOperandImpl
{
public:
  explicit NdbUint32ConstOperandImpl (Uint32 value) : 
    NdbConstOperandImpl(), 
    m_value(value) {}
  size_t getLength()    const { return sizeof(m_value); }
  const void* getAddr() const { return &m_value; }
  NdbDictionary::Column::Type getType() const { return NdbDictionary::Column::Unsigned; }
private:
  const Uint32 m_value;
};

class NdbInt64ConstOperandImpl : public NdbConstOperandImpl
{
public:
  explicit NdbInt64ConstOperandImpl (Int64 value) : 
    NdbConstOperandImpl(), 
    m_value(value) {}
  size_t getLength()    const { return sizeof(m_value); }
  const void* getAddr() const { return &m_value; }
  NdbDictionary::Column::Type getType() const { return NdbDictionary::Column::Bigint; }
private:
  const Int64 m_value;
};

class NdbUint64ConstOperandImpl : public NdbConstOperandImpl
{
public:
  explicit NdbUint64ConstOperandImpl (Uint64 value) : 
    NdbConstOperandImpl(), 
    m_value(value) {}
  size_t getLength()    const { return sizeof(m_value); }
  const void* getAddr() const { return &m_value; }
  NdbDictionary::Column::Type getType() const { return NdbDictionary::Column::Bigunsigned; }
private:
  const Uint64 m_value;
};

class NdbCharConstOperandImpl : public NdbConstOperandImpl
{
public:
  explicit NdbCharConstOperandImpl (const char* value) : 
    NdbConstOperandImpl(), m_value(value) {}
  size_t getLength()    const { return strlen(m_value); }
  const void* getAddr() const { return m_value; }
  NdbDictionary::Column::Type getType() const { return NdbDictionary::Column::Char; }
private:
  const char* const m_value;
};

class NdbGenericConstOperandImpl : public NdbConstOperandImpl
{
public:
  explicit NdbGenericConstOperandImpl (const void* value, size_t length)
  : NdbConstOperandImpl(), m_value(value), m_length(length)
  {}

  size_t getLength()    const { return m_length; }
  const void* getAddr() const { return m_value; }
  NdbDictionary::Column::Type getType() const { return NdbDictionary::Column::Undefined; }
private:
  const void* const m_value;
  const size_t m_length;
};

////////////////////////////////////////////////
// Implementation of NdbQueryOperation interface
////////////////////////////////////////////////

// Common Baseclass 'class NdbQueryOperationDefImpl' is 
// defined in "NdbQueryBuilderImpl.hpp"

class NdbQueryLookupOperationDefImpl : public NdbQueryOperationDefImpl
{
  friend class NdbQueryBuilder;  // Allow privat access from builder interface

public:
  virtual int serializeOperation(Uint32Buffer& serializedDef) const;

  virtual void materializeRootOperands(NdbOperation& ndbOperation,
                                       const constVoidPtr actualParam[]) const;

protected:
  virtual ~NdbQueryLookupOperationDefImpl() {}

  explicit NdbQueryLookupOperationDefImpl (
                           const NdbTableImpl& table,
                           const NdbQueryOperand* const keys[],
                           const char* ident,
                           Uint32      ix);

  virtual const NdbQueryLookupOperationDef& getInterface() const
  { return m_interface; }

  virtual Type getType() const
  { return PrimaryKeyAccess; }

protected:
  NdbQueryLookupOperationDef m_interface;
  NdbQueryOperandImpl* m_keys[MAX_ATTRIBUTES_IN_INDEX+1];

}; // class NdbQueryLookupOperationDefImpl


class NdbQueryIndexOperationDefImpl : public NdbQueryLookupOperationDefImpl
{
  friend class NdbQueryBuilder;  // Allow privat access from builder interface

public:
  virtual const NdbIndexImpl* getIndex() const
  { return &m_index; }

  virtual int serializeOperation(Uint32Buffer& serializedDef) const;

private:
  virtual ~NdbQueryIndexOperationDefImpl() {}
  explicit NdbQueryIndexOperationDefImpl (
                           const NdbIndexImpl& index,
                           const NdbTableImpl& table,
                           const NdbQueryOperand* const keys[],
                           const char* ident,
                           Uint32      ix)
  : NdbQueryLookupOperationDefImpl(table,keys,ident,ix),
    m_index(index)
  {}

  virtual Type getType() const
  { return UniqueIndexAccess; }

private:
  const NdbIndexImpl& m_index;

}; // class NdbQueryIndexOperationDefImpl


class NdbQueryScanOperationDefImpl :
  public NdbQueryOperationDefImpl
{
public:
  virtual ~NdbQueryScanOperationDefImpl()=0;
  explicit NdbQueryScanOperationDefImpl (
                           const NdbTableImpl& table,
                           const char* ident,
                           Uint32      ix)
  : NdbQueryOperationDefImpl(table,ident,ix)
  {}

protected:
  int serialize(Uint32Buffer& serializedDef,
                const NdbTableImpl& tableOrIndex) const;

}; // class NdbQueryScanOperationDefImpl

class NdbQueryTableScanOperationDefImpl : public NdbQueryScanOperationDefImpl
{
  friend class NdbQueryBuilder;  // Allow privat access from builder interface

public:
  virtual int serializeOperation(Uint32Buffer& serializedDef) const;

  virtual const NdbQueryTableScanOperationDef& getInterface() const
  { return m_interface; }

  virtual Type getType() const
  { return TableScan; }

  virtual void materializeRootOperands(NdbOperation& ndbOperation,
                                       const constVoidPtr actualParam[]) const;

private:
  virtual ~NdbQueryTableScanOperationDefImpl() {}
  explicit NdbQueryTableScanOperationDefImpl (
                           const NdbTableImpl& table,
                           const char* ident,
                           Uint32      ix)
    : NdbQueryScanOperationDefImpl(table,ident,ix),
      m_interface(*this) 
  {}

  NdbQueryTableScanOperationDef m_interface;

}; // class NdbQueryTableScanOperationDefImpl


class NdbQueryIndexScanOperationDefImpl : public NdbQueryScanOperationDefImpl
{
  friend class NdbQueryBuilder;  // Allow privat access from builder interface

public:
  virtual const NdbIndexImpl* getIndex() const
  { return &m_index; }

  virtual int serializeOperation(Uint32Buffer& serializedDef) const;

  virtual const NdbQueryIndexScanOperationDef& getInterface() const
  { return m_interface; }

  virtual Type getType() const
  { return OrderedIndexScan; }

  virtual void materializeRootOperands(NdbOperation& ndbOperation,
                                       const constVoidPtr actualParam[]) const;

private:
  virtual ~NdbQueryIndexScanOperationDefImpl() {};
  explicit NdbQueryIndexScanOperationDefImpl (
                           const NdbIndexImpl& index,
                           const NdbTableImpl& table,
                           const NdbQueryIndexBound* bound,
                           const char* ident,
                           Uint32      ix);

private:
  NdbQueryIndexScanOperationDef m_interface;
  const NdbIndexImpl& m_index;

  struct bound {  // Limiting 'bound ' definition
    NdbQueryOperandImpl* low[MAX_ATTRIBUTES_IN_INDEX+1];
    NdbQueryOperandImpl* high[MAX_ATTRIBUTES_IN_INDEX+1];
    bool lowIncl, highIncl;
    bool eqBound;  // True if 'low == high'
  } m_bound;
}; // class NdbQueryIndexScanOperationDefImpl



///////////////////////////////////////////////////
/////// End 'Impl' class declarations /////////////
///////////////////////////////////////////////////

NdbQueryDef::NdbQueryDef(NdbQueryDefImpl& impl) : m_impl(impl)
{}
NdbQueryDef::~NdbQueryDef()
{}

Uint32
NdbQueryDef::getNoOfOperations() const
{ return m_impl.getNoOfOperations();
}

const NdbQueryOperationDef*
NdbQueryDef::getQueryOperation(Uint32 index) const
{ return &m_impl.getQueryOperation(index).getInterface();
}

const NdbQueryOperationDef*
NdbQueryDef::getQueryOperation(const char* ident) const
{ const NdbQueryOperationDefImpl *opDef = m_impl.getQueryOperation(ident);
  return (opDef!=NULL) ? &opDef->getInterface() : NULL;
}

NdbQueryDefImpl& 
NdbQueryDef::getImpl() const{
  return m_impl;
}

/*************************************************************************
 * Glue layer between NdbQueryOperand interface and its Impl'ementation.
 ************************************************************************/

NdbQueryOperand::NdbQueryOperand(NdbQueryOperandImpl& impl) : m_impl(impl)
{}
NdbConstOperand::NdbConstOperand(NdbQueryOperandImpl& impl) : NdbQueryOperand(impl)
{}
NdbParamOperand::NdbParamOperand(NdbQueryOperandImpl& impl) : NdbQueryOperand(impl)
{}
NdbLinkedOperand::NdbLinkedOperand(NdbQueryOperandImpl& impl) : NdbQueryOperand(impl)
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
NdbQueryOperandImpl::~NdbQueryOperandImpl()
{}

/**
 * Get'ers for NdbQueryOperand...Impl object.
 * Functions overridden to supply 'impl' casted to the correct '...OperandImpl' type
 * for each available interface class.
 */
inline NdbQueryOperandImpl&
NdbQueryOperand::getImpl() const
{ return m_impl;
}
inline static
NdbQueryOperandImpl& getImpl(const NdbQueryOperand& op)
{ return op.getImpl();
}
inline static
NdbConstOperandImpl& getImpl(const NdbConstOperand& op)
{ return static_cast<NdbConstOperandImpl&>(op.getImpl());
}
inline static
NdbParamOperandImpl& getImpl(const NdbParamOperand& op)
{ return static_cast<NdbParamOperandImpl&>(op.getImpl());
}
inline static
NdbLinkedOperandImpl& getImpl(const NdbLinkedOperand& op)
{ return static_cast<NdbLinkedOperandImpl&>(op.getImpl());
}

/**
 * BEWARE: Return 'NULL' Until the operand has been bound to an operation.
 */
const NdbDictionary::Column*
NdbQueryOperand::getColumn() const
{
  return ::getImpl(*this).getColumn();
}

const char*
NdbParamOperand::getName() const
{
  return ::getImpl(*this).getName();
}

Uint32
NdbParamOperand::getEnum() const
{
  return ::getImpl(*this).getParamIx();
}

/****************************************************************************
 * Glue layer between NdbQueryOperationDef interface and its Impl'ementation.
 ****************************************************************************/
NdbQueryOperationDef::NdbQueryOperationDef(NdbQueryOperationDefImpl& impl) : m_impl(impl)
{}
NdbQueryLookupOperationDef::NdbQueryLookupOperationDef(NdbQueryOperationDefImpl& impl) : NdbQueryOperationDef(impl) 
{}
NdbQueryScanOperationDef::NdbQueryScanOperationDef(NdbQueryOperationDefImpl& impl) : NdbQueryOperationDef(impl) 
{}
NdbQueryTableScanOperationDef::NdbQueryTableScanOperationDef(NdbQueryOperationDefImpl& impl) : NdbQueryScanOperationDef(impl) 
{}
NdbQueryIndexScanOperationDef::NdbQueryIndexScanOperationDef(NdbQueryOperationDefImpl& impl) : NdbQueryScanOperationDef(impl) 
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
NdbQueryOperationDefImpl::~NdbQueryOperationDefImpl()
{}
NdbQueryScanOperationDefImpl::~NdbQueryScanOperationDefImpl()
{}


/**
 * Get'ers for QueryOperation...DefImpl object.
 * Functions overridden to supply 'impl' casted to the correct '...DefImpl' type
 * for each available interface class.
 */ 
NdbQueryOperationDefImpl&
NdbQueryOperationDef::getImpl() const
{ return m_impl;
}
inline static
NdbQueryOperationDefImpl& getImpl(const NdbQueryOperationDef& op)
{ return op.getImpl();
}
inline static
NdbQueryLookupOperationDefImpl& getImpl(const NdbQueryLookupOperationDef& op)
{ return static_cast<NdbQueryLookupOperationDefImpl&>(op.getImpl());
}
inline static
NdbQueryTableScanOperationDefImpl& getImpl(const NdbQueryTableScanOperationDef& op)
{ return static_cast<NdbQueryTableScanOperationDefImpl&>(op.getImpl());
}
inline static
NdbQueryIndexScanOperationDefImpl& getImpl(const NdbQueryIndexScanOperationDef& op)
{ return static_cast<NdbQueryIndexScanOperationDefImpl&>(op.getImpl());
}



Uint32
NdbQueryOperationDef::getNoOfParentOperations() const
{
  return ::getImpl(*this).getNoOfParentOperations();
}

const NdbQueryOperationDef*
NdbQueryOperationDef::getParentOperation(Uint32 i) const
{
  return &::getImpl(*this).getParentOperation(i).getInterface();
}

Uint32 
NdbQueryOperationDef::getNoOfChildOperations() const
{
  return ::getImpl(*this).getNoOfChildOperations();
}

const NdbQueryOperationDef* 
NdbQueryOperationDef::getChildOperation(Uint32 i) const
{
  return &::getImpl(*this).getChildOperation(i).getInterface();
}

const NdbDictionary::Table*
NdbQueryOperationDef::getTable() const
{
  return &::getImpl(*this).getTable();
}

const NdbDictionary::Index*
NdbQueryLookupOperationDef::getIndex() const
{
  return ::getImpl(*this).getIndex();
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
  returnErrIf(value==0,QRY_REQ_ARG_IS_NULL);
  NdbConstOperandImpl* constOp = new NdbCharConstOperandImpl(value);
  returnErrIf(constOp==0,4000);

  m_pimpl->m_operands.push_back(constOp);
  return &constOp->m_interface;
}
NdbConstOperand* 
NdbQueryBuilder::constValue(const void* value, size_t length)
{
  returnErrIf(value==0 && length>0,QRY_REQ_ARG_IS_NULL);
  NdbConstOperandImpl* constOp = new NdbGenericConstOperandImpl(value,length);
  returnErrIf(constOp==0,4000);

  m_pimpl->m_operands.push_back(constOp);
  return &constOp->m_interface;
}
NdbConstOperand* 
NdbQueryBuilder::constValue(Int32 value)
{
  NdbConstOperandImpl* constOp = new NdbInt32ConstOperandImpl(value);
  returnErrIf(constOp==0,4000);

  m_pimpl->m_operands.push_back(constOp);
  return &constOp->m_interface;
}
NdbConstOperand* 
NdbQueryBuilder::constValue(Uint32 value)
{
  NdbConstOperandImpl* constOp = new NdbUint32ConstOperandImpl(value);
  returnErrIf(constOp==0,4000);

  m_pimpl->m_operands.push_back(constOp);
  return &constOp->m_interface;
}
NdbConstOperand* 
NdbQueryBuilder::constValue(Int64 value)
{
  NdbConstOperandImpl* constOp = new NdbInt64ConstOperandImpl(value);
  returnErrIf(constOp==0,4000);

  m_pimpl->m_operands.push_back(constOp);
  return &constOp->m_interface;
}
NdbConstOperand* 
NdbQueryBuilder::constValue(Uint64 value)
{
  NdbConstOperandImpl* constOp = new NdbUint64ConstOperandImpl(value);
  returnErrIf(constOp==0,4000);

  m_pimpl->m_operands.push_back(constOp);
  return &constOp->m_interface;
}

NdbParamOperand* 
NdbQueryBuilder::paramValue(const char* name)
{
  NdbParamOperandImpl* paramOp = new NdbParamOperandImpl(name,getImpl().m_paramCnt++);
  returnErrIf(paramOp==0,4000);

  m_pimpl->m_operands.push_back(paramOp);
  return &paramOp->m_interface;
}

NdbLinkedOperand* 
NdbQueryBuilder::linkedValue(const NdbQueryOperationDef* parent, const char* attr)
{
  // Required non-NULL arguments
  returnErrIf(parent==0 || attr==0, QRY_REQ_ARG_IS_NULL);
  NdbQueryOperationDefImpl& parentImpl = parent->getImpl();

  // Parent should be a OperationDef contained in this query builder context
  returnErrIf(!m_pimpl->contains(&parentImpl), QRY_UNKONWN_PARENT);

  // 'attr' should refer a column from the underlying table in parent:
  const NdbColumnImpl* column = parentImpl.getTable().getColumn(attr);
  returnErrIf(column==0, QRY_UNKNOWN_COLUMN); // Unknown column

  // Locate refered parrent column in parent operations SPJ projection list;
  // Add if not already present
  Uint32 colIx = parentImpl.addColumnRef(column);

  NdbLinkedOperandImpl* linkedOp = new NdbLinkedOperandImpl(parentImpl,colIx);
  returnErrIf(linkedOp==0, 4000);

  m_pimpl->m_operands.push_back(linkedOp);
  return &linkedOp->m_interface;
}


NdbQueryLookupOperationDef*
NdbQueryBuilder::readTuple(const NdbDictionary::Table* table,    // Primary key lookup
                           const NdbQueryOperand* const keys[],  // Terminated by NULL element 
                           const char* ident)
{
  int i;
  if (m_pimpl->hasError())
    return NULL;

  returnErrIf(table==0 || keys==0, QRY_REQ_ARG_IS_NULL);

  const NdbTableImpl& tableImpl = NdbTableImpl::getImpl(*table);

  // All column values being part of primary key should be specified:
  int keyfields = table->getNoOfPrimaryKeys();
  int colcount = table->getNoOfColumns();

  // Check: keys[] are specified for all fields in PK
  for (i=0; i<keyfields; ++i)
  {
    // A 'Key' value is undefined
    returnErrIf(keys[i]==NULL, QRY_TOO_FEW_KEY_VALUES);
  }
  // Check for propper NULL termination of keys[] spec
  returnErrIf(keys[keyfields]!=NULL, QRY_TOO_MANY_KEY_VALUES);

  NdbQueryLookupOperationDefImpl* op =
    new NdbQueryLookupOperationDefImpl(tableImpl,
                                       keys,ident,
                                       m_pimpl->m_operations.size());
  returnErrIf(op==0, 4000);

  Uint32 keyindex = 0;
  for (i=0; i<colcount; ++i)
  {
    const NdbColumnImpl *col = tableImpl.getColumn(i);
    if (col->getPrimaryKey())
    {
      assert (keyindex==col->m_keyInfoPos);
      int error = op->m_keys[col->m_keyInfoPos]->bindOperand(*col,*op);
      if (unlikely(error))
      { m_pimpl->setErrorCode(error);
        delete op;
        return NULL;
      }

      keyindex++;
      if (keyindex >= static_cast<Uint32>(keyfields))
        break;  // Seen all PK fields
    }
  }
  
  m_pimpl->m_operations.push_back(op);
  return &op->m_interface;
}


NdbQueryLookupOperationDef*
NdbQueryBuilder::readTuple(const NdbDictionary::Index* index,    // Unique key lookup w/ index
                           const NdbDictionary::Table* table,    // Primary key lookup
                           const NdbQueryOperand* const keys[],  // Terminated by NULL element 
                           const char* ident)
{
  int i;

  if (m_pimpl->hasError())
    return NULL;
  returnErrIf(table==0 || index==0 || keys==0, QRY_REQ_ARG_IS_NULL);

  const NdbIndexImpl& indexImpl = NdbIndexImpl::getImpl(*index);
  const NdbTableImpl& tableImpl = NdbTableImpl::getImpl(*table);

  // TODO: Restrict to only table_version_major() mismatch?
  returnErrIf(indexImpl.m_table_id 
              != static_cast<Uint32>(table->getObjectId()) ||
              indexImpl.m_table_version 
              != static_cast<Uint32>(table->getObjectVersion()), 
              QRY_UNRELATED_INDEX);

  // Only 'UNUQUE' indexes may be used for lookup operations:
  returnErrIf(index->getType()!=NdbDictionary::Index::UniqueHashIndex,
              QRY_WRONG_INDEX_TYPE);

  // Check: keys[] are specified for all fields in 'index'
  int inxfields = index->getNoOfColumns();
  for (i=0; i<inxfields; ++i)
  {
    // A 'Key' value is undefined
    returnErrIf(keys[i]==NULL, QRY_TOO_FEW_KEY_VALUES);
  }
  // Check for propper NULL termination of keys[] spec
  returnErrIf(keys[inxfields]!=NULL, QRY_TOO_MANY_KEY_VALUES);

  NdbQueryIndexOperationDefImpl* op = 
    new NdbQueryIndexOperationDefImpl(indexImpl, tableImpl,
                                       keys,ident,
                                       m_pimpl->m_operations.size());
  returnErrIf(op==0, 4000);

  // Bind to Column and check type compatibility
  for (i=0; i<inxfields; ++i)
  {
    const NdbColumnImpl& col = NdbColumnImpl::getImpl(*indexImpl.getColumn(i));
    assert (col.getColumnNo() == i);

    int error = keys[i]->getImpl().bindOperand(col,*op);
    if (unlikely(error))
    { m_pimpl->setErrorCode(error);
      delete op;
      return NULL;
    }
  }

  m_pimpl->m_operations.push_back(op);
  return &op->m_interface;
}


NdbQueryTableScanOperationDef*
NdbQueryBuilder::scanTable(const NdbDictionary::Table* table,
                           const char* ident)
{
  if (m_pimpl->hasError())
    return NULL;
  returnErrIf(table==0, QRY_REQ_ARG_IS_NULL);  // Required non-NULL arguments

  NdbQueryTableScanOperationDefImpl* op =
    new NdbQueryTableScanOperationDefImpl(NdbTableImpl::getImpl(*table),ident,
                                          m_pimpl->m_operations.size());
  returnErrIf(op==0, 4000);

  m_pimpl->m_operations.push_back(op);
  return &op->m_interface;
}


NdbQueryIndexScanOperationDef*
NdbQueryBuilder::scanIndex(const NdbDictionary::Index* index, 
	                   const NdbDictionary::Table* table,
                           const NdbQueryIndexBound* bound,
                           const char* ident)
{
  if (m_pimpl->hasError())
    return NULL;
  // Required non-NULL arguments
  returnErrIf(table==0 || index==0, QRY_REQ_ARG_IS_NULL);

  const NdbIndexImpl& indexImpl = NdbIndexImpl::getImpl(*index);
  const NdbTableImpl& tableImpl = NdbTableImpl::getImpl(*table);

  // TODO: Restrict to only table_version_major() mismatch?
  returnErrIf(indexImpl.m_table_id 
              != static_cast<Uint32>(table->getObjectId()) ||
              indexImpl.m_table_version 
              != static_cast<Uint32>(table->getObjectVersion()), 
              QRY_UNRELATED_INDEX);

  // Only ordered indexes may be used in scan operations:
  returnErrIf(index->getType()!=NdbDictionary::Index::OrderedIndex,
              QRY_WRONG_INDEX_TYPE);

  NdbQueryIndexScanOperationDefImpl* op =
    new NdbQueryIndexScanOperationDefImpl(indexImpl, tableImpl,
                                          bound, ident,
                                          m_pimpl->m_operations.size());
  returnErrIf(op==0, 4000);

  int i;
  int inxfields = index->getNoOfColumns();
  for (i=0; i<inxfields; ++i)
  {
    if (op->m_bound.low[i] == NULL)
      break;
    const NdbColumnImpl& col = NdbColumnImpl::getImpl(*indexImpl.getColumn(i));
    int error = op->m_bound.low[i]->bindOperand(col,*op);
    if (unlikely(error))
    { m_pimpl->setErrorCode(error);
      delete op;
      return NULL;
    }
  }
  if (!op->m_bound.eqBound)
  {
    for (i=0; i<inxfields; ++i)
    {
      if (op->m_bound.high[i] == NULL)
        break;
      const NdbColumnImpl& col = NdbColumnImpl::getImpl(*indexImpl.getColumn(i));
      int error = op->m_bound.high[i]->bindOperand(col,*op);
      if (unlikely(error))
      { m_pimpl->setErrorCode(error);
        delete op;
        return NULL;
      }
    }
  }

  m_pimpl->m_operations.push_back(op);
  return &op->m_interface;
}

const NdbQueryDef*
NdbQueryBuilder::prepare()
{
  const NdbQueryDefImpl* def = m_pimpl->prepare();
  return (def) ? &def->getInterface() : NULL;
}

////////////////////////////////////////
// The (hidden) Impl of NdbQueryBuilder
////////////////////////////////////////

NdbQueryBuilderImpl::NdbQueryBuilderImpl(Ndb& ndb)
: m_ndb(ndb), m_error(),
  m_operations(),
  m_operands(),
  m_paramCnt(0)
{}

NdbQueryBuilderImpl::~NdbQueryBuilderImpl()
{
  Uint32 i;

  // Delete all operand and operator in Vector's
  for (i=0; i<m_operations.size(); ++i)
  { delete m_operations[i];
  }
  for (i=0; i<m_operands.size(); ++i)
  { delete m_operands[i];
  }
}


bool 
NdbQueryBuilderImpl::contains(const NdbQueryOperationDefImpl* opDef)
{
  for (Uint32 i=0; i<m_operations.size(); ++i)
  { if (m_operations[i] == opDef)
      return true;
  }
  return false;
}


const NdbQueryDefImpl*
NdbQueryBuilderImpl::prepare()
{
  int error;
  NdbQueryDefImpl* def = new NdbQueryDefImpl(*this, m_operations, error);
  m_operations.clear();
  m_operands.clear();
  m_paramCnt = 0;

  returnErrIf(def==0, 4000);
  if(unlikely(error!=0)){
    delete def;
    setErrorCode(error);
    return NULL;
  }

  return def;
}

///////////////////////////////////
// The (hidden) Impl of NdbQueryDef
///////////////////////////////////
NdbQueryDefImpl::
NdbQueryDefImpl(const NdbQueryBuilderImpl& builder,
                const Vector<NdbQueryOperationDefImpl*>& operations,
                //const Vector<NdbQueryOperandImpl*> operands;  FIXME
                int& error)
 : m_interface(*this), 
   m_operations(operations),
   m_operands()
{
  Uint32 nodeId = 0;

  /* Grab first word, such that serialization of operation 0 will start from 
   * offset 1, leaving space for the length field.
   */
  m_serializedDef.get(0); 
  for(Uint32 i = 0; i<m_operations.size(); i++){
    NdbQueryOperationDefImpl* op =  m_operations[i];
    op->assignQueryOperationId(nodeId);
    error = op->serializeOperation(m_serializedDef);
    if(unlikely(error != 0)){
      return;
    }
  }
  assert (nodeId >= m_operations.size());

  // Set length and number of nodes in tree.
  QueryTree::setCntLen(m_serializedDef.get(0), 
		       nodeId,
		       m_serializedDef.getSize());
#ifdef TRACE_SERIALIZATION
  ndbout << "Serialized tree : ";
  for(Uint32 i = 0; i < m_serializedDef.getSize(); i++){
    char buf[12];
    sprintf(buf, "%.8x", m_serializedDef.get(i));
    ndbout << buf << " ";
  }
  ndbout << endl;
#endif
}

NdbQueryDefImpl::~NdbQueryDefImpl()
{
  // Release all NdbQueryOperations
  for (Uint32 i=0; i<m_operations.size(); ++i)
  { delete m_operations[i];
  }
  for (Uint32 i=0; i<m_operands.size(); ++i)
  { delete m_operands[i];
  }
}

const NdbQueryOperationDefImpl*
NdbQueryDefImpl::getQueryOperation(const char* ident) const
{
  if (ident==NULL)
    return NULL;

  Uint32 sz = m_operations.size();
  const NdbQueryOperationDefImpl* const* opDefs = m_operations.getBase();
  for(Uint32 i = 0; i<sz; i++, opDefs++){
    const char* opName = (*opDefs)->getName();
    if(opName!=NULL && strcmp(opName, ident) == 0)
      return *opDefs;
  }
  return NULL;
}


NdbQueryLookupOperationDefImpl::NdbQueryLookupOperationDefImpl (
                           const NdbTableImpl& table,
                           const NdbQueryOperand* const keys[],
                           const char* ident,
                           Uint32      ix)
 : NdbQueryOperationDefImpl(table,ident,ix),
   m_interface(*this)
{
  int i;
  for (i=0; i<=MAX_ATTRIBUTES_IN_INDEX; ++i)
  { if (keys[i] == NULL)
      break;
    m_keys[i] = &keys[i]->getImpl();
  }
  assert (keys[i] == NULL);
  m_keys[i] = NULL;
}

NdbQueryIndexScanOperationDefImpl::NdbQueryIndexScanOperationDefImpl (
                           const NdbIndexImpl& index,
                           const NdbTableImpl& table,
                           const NdbQueryIndexBound* bound,
                           const char* ident,
                           Uint32      ix)
: NdbQueryScanOperationDefImpl(table,ident,ix),
  m_interface(*this), 
  m_index(index), m_bound()
{
  if (bound!=NULL) {

    if (bound->m_low!=NULL) {
      int i;
      for (i=0; i<=MAX_ATTRIBUTES_IN_INDEX; ++i)
      { if (bound->m_low[i] == NULL)
          break;
        m_bound.low[i] = &bound->m_low[i]->getImpl();
      }
      assert (bound->m_low[i] == NULL);
      m_bound.low[i] = NULL;
    }
    if (bound->m_high!=NULL) {
      int i;
      for (i=0; i<=MAX_ATTRIBUTES_IN_INDEX; ++i)
      { if (bound->m_high[i] == NULL)
          break;
        m_bound.high[i] = &bound->m_high[i]->getImpl();
      }
      assert (bound->m_high[i] == NULL);
      m_bound.high[i] = NULL;
    }
    m_bound.lowIncl = bound->m_lowInclusive;
    m_bound.highIncl = bound->m_highInclusive;
    m_bound.eqBound = (bound->m_low==bound->m_high && bound->m_low!=NULL);
  }
  else {
    m_bound.low[0] = NULL;
    m_bound.high[0] = NULL;
    m_bound.lowIncl = true;
    m_bound.highIncl = true;
    m_bound.eqBound = false;
  }
}


void 
NdbQueryLookupOperationDefImpl
::materializeRootOperands(NdbOperation& ndbOperation,
                          const constVoidPtr actualParam[]) const
{
  assert(getQueryOperationIx()==0); // Should only be called for root operation.
  assert (getQueryOperationId()==(getIndex() ?1 :0));
  const int keyCount = getIndex()==NULL ? 
    getTable().getNoOfPrimaryKeys() :
    static_cast<int>(getIndex()->getNoOfColumns());
  int keyNo;
  for(keyNo = 0; keyNo<keyCount; keyNo++)
  {
    switch(m_keys[keyNo]->getKind()){
    case NdbQueryOperandImpl::Const:
    {
      const NdbConstOperandImpl* const constOp 
        = static_cast<const NdbConstOperandImpl*>(m_keys[keyNo]);
      int ret = 
        ndbOperation.equal(keyNo, static_cast<const char*>(constOp->getAddr()));
      assert(!ret);
      break;
    }
    case NdbQueryOperandImpl::Param:
    {
      const NdbParamOperandImpl* const paramOp 
        = static_cast<const NdbParamOperandImpl*>(m_keys[keyNo]);
      int paramNo = paramOp->getParamIx();
      assert(actualParam != NULL);
      assert(actualParam[paramNo] != NULL);
      int ret = 
        ndbOperation.equal(keyNo, 
                           static_cast<const char*>(actualParam[paramNo]));
      assert(!ret);
      break;
    }
    case NdbQueryOperandImpl::Linked:    // Root operation cannot have linked operands.
    default:
      assert(false);
    }
  }
  // All key fields should have been assigned a value. 
  assert(m_keys[keyNo] == NULL);
}

/**
 * Helper function for NdbQueryIndexScanOperationDefImpl::materializeRootOperands()
 * Fill in values for either a low or high bound as defined in boundDef[].
 */
static void 
fillBoundValues (const NdbRecord* key_rec,
                 char* buffer, Uint32& cnt,
                 NdbQueryOperandImpl* const boundDef[],
                 const constVoidPtr actualParam[])
{
  assert (key_rec->flags & NdbRecord::RecHasAllKeys);

  /**
   * Serialize upper/lower bounds definitions.
   */
  Uint32 keyNo;
  const Uint32 keyCount = key_rec->key_index_length;
  assert (keyCount <= key_rec->noOfColumns);
  for (keyNo = 0; keyNo<keyCount; keyNo++)
  {
    const NdbQueryOperandImpl* bound = boundDef[keyNo];
    if (bound == NULL)
      break;

    assert (key_rec->key_indexes[keyNo] <= key_rec->noOfColumns);
    const NdbRecord::Attr& attr = key_rec->columns[key_rec->key_indexes[keyNo]];
    assert (attr.flags & NdbRecord::IsKey);
    Uint32 offset = attr.offset;

    switch(bound->getKind()){
    case NdbQueryOperandImpl::Const:
    {
      const NdbConstOperandImpl* constOp = static_cast<const NdbConstOperandImpl*>(bound);
      assert (key_rec->columns[keyNo].maxSize >= constOp->getLength());
      memcpy(buffer+offset, constOp->getAddr(), constOp->getLength());
      break;
    }
    case NdbQueryOperandImpl::Param:
    {
      const NdbParamOperandImpl* const paramOp 
        = static_cast<const NdbParamOperandImpl*>(bound);
      int paramNo = paramOp->getParamIx();
      assert(actualParam != NULL);
      assert(actualParam[paramNo] != NULL);
      memcpy(buffer+offset, actualParam[paramNo], key_rec->columns[keyNo].maxSize);
      break;
    }
    case NdbQueryOperandImpl::Linked:    // Root operation cannot have linked operands.
    default:
      assert(false);
    }
  }
  cnt = keyNo;
} // fillBoundValues()



void 
NdbQueryIndexScanOperationDefImpl
::materializeRootOperands(NdbOperation& ndbOperation,
                          const constVoidPtr actualParam[]) const
{
  assert(getQueryOperationIx()==0); // Should only be called for root operation.

  char low[1024], high[1024];
  NdbIndexScanOperation::IndexBound bound;
  const NdbRecord* key_rec = m_index.getDefaultRecord();
  assert (key_rec->flags & NdbRecord::RecHasAllKeys);

  /**
   * Fill in upper and lower bounds as declared with specified values.
   */
  fillBoundValues (key_rec, low, bound.low_key_count, m_bound.low, actualParam);
  bound.low_key = low;

  if (m_bound.eqBound)
  { // low / high are the same
    bound.high_key = bound.low_key;
    bound.high_key_count = bound.low_key_count;
  }
  else
  {
    fillBoundValues (key_rec, high, bound.high_key_count, m_bound.high, actualParam);
    bound.high_key = high;
  }
  bound.low_inclusive=m_bound.lowIncl;
  bound.high_inclusive=m_bound.highIncl;
  bound.range_no=0;

  NdbIndexScanOperation& inxOp = static_cast<NdbIndexScanOperation&>(ndbOperation);
  int err = inxOp.setBound(key_rec,bound);
  assert (!err);
}

void 
NdbQueryTableScanOperationDefImpl
::materializeRootOperands(NdbOperation& ndbOperation,
                          const constVoidPtr actualParam[]) const
{
  // TODO: Implement this.
  // ... Or does it not make sense for a plain scan.....
}


void
NdbQueryOperationDefImpl::addParent(NdbQueryOperationDefImpl* parentOp)
{

  for (Uint32 i=0; i<m_parents.size(); ++i)
  { if (m_parents[i] == parentOp)
      return;
  }
  m_parents.push_back(parentOp);
}

void
NdbQueryOperationDefImpl::addChild(NdbQueryOperationDefImpl* childOp)
{
  for (Uint32 i=0; i<m_children.size(); ++i)
  { if (m_children[i] == childOp)
      return;
  }
  m_children.push_back(childOp);
}


// Register a linked reference to a column available from this operation
Uint32
NdbQueryOperationDefImpl::addColumnRef(const NdbColumnImpl* column)
{
  Uint32 spjRef;
  for (spjRef=0; spjRef<m_spjProjection.size(); ++spjRef)
  { if (m_spjProjection[spjRef] == column)
      return spjRef;
  }

  // Add column if not already available
  m_spjProjection.push_back(column);
  return spjRef;
}


int
NdbLinkedOperandImpl::bindOperand(
                           const NdbColumnImpl& column,
                           NdbQueryOperationDefImpl& operation)
{
  NdbDictionary::Column::Type type = column.getType();
  if (type != getParentColumn().getType())
    return QRY_OPERAND_HAS_WRONG_TYPE ;  // Incompatible datatypes

  // TODO? Check length if Char, and prec, scale if decimal type

  // Register parent/child operation relations
  this->m_parentOperation.addChild(&operation);
  operation.addParent(&this->m_parentOperation);

  return NdbQueryOperandImpl::bindOperand(column,operation);
}


int
NdbParamOperandImpl::bindOperand(
                           const NdbColumnImpl& column,
                           NdbQueryOperationDefImpl& operation)
{
  operation.addParamRef(this);
  return NdbQueryOperandImpl::bindOperand(column,operation);
}


int
NdbConstOperandImpl::bindOperand(
                           const NdbColumnImpl& column,
                           NdbQueryOperationDefImpl& operation)
{
  NdbDictionary::Column::Type type = column.getType();
  if (type != this->getType())
    return QRY_OPERAND_HAS_WRONG_TYPE ;  // Incompatible datatypes

  // TODO? Check length if Char, and prec,scale if decimal type

  return NdbQueryOperandImpl::bindOperand(column,operation);
}

/** This class is used for serializing sequences of 16 bit integers,
 * where the first 16 bit integer specifies the length of the sequence.
 */
class Uint16Sequence{
public:
  explicit Uint16Sequence(Uint32Buffer& buffer):
    m_buffer(buffer),
    m_length(0){}

  /** Add an item to the sequence.*/
  void append(Uint16 value){
    if(m_length==0) {
      m_buffer.get(0) = value<<16;
    } else if((m_length & 1) == 0) {
      m_buffer.get(m_length/2) |= value<<16;
    } else {
      m_buffer.get((m_length+1)/2) = value;
    }
    m_length++;
  }
  
  /** End the sequence and set the length */
  int finish(){
    assert(m_length<=0xffff);
    if(m_length>0){
      m_buffer.get(0) |= m_length;
      if((m_length & 1) == 0){
	m_buffer.get(m_length/2) |= 0xBABE<<16;
      }
      return m_length/2+1;
    }else{
      return 0;
    }
  }

private:
  /** Should not be copied.*/
  Uint16Sequence(Uint16Sequence&);
  /** Should not be assigned.*/
  Uint16Sequence& operator=(Uint16Sequence&);
  Uint32Slice m_buffer;
  int m_length;
};


void
NdbQueryOperationDefImpl::appendParentList(Uint32Buffer& serializedDef) const
{
  Uint16Sequence parentSeq(serializedDef);
  // Multiple parents not yet supported.
  assert(getNoOfParentOperations()==1);
  for (Uint32 i = 0; 
      i < getNoOfParentOperations(); 
      i++){
    assert (getParentOperation(i).getQueryOperationId() < getQueryOperationId());
    parentSeq.append(getParentOperation(i).getQueryOperationId());
  }
  parentSeq.finish();
}

static Uint32
appendKeyPattern(Uint32Buffer& serializedDef,
                 const NdbQueryOperandImpl* const *m_keys)
{
  Uint32 appendedPattern = 0;
  if (m_keys[0]!=NULL)
  {
    Uint32Slice keyPattern(serializedDef);
    keyPattern.get(0);     // Grab first word for length field, updated at end
    int keyPatternPos = 1; // Length at offs '0' set later
    int paramCnt = 0;
    int keyNo = 0;
    const NdbQueryOperandImpl* key = m_keys[0];
    do
    {
      switch(key->getKind()){
      case NdbQueryOperandImpl::Linked:
      {
        appendedPattern |= DABits::NI_KEY_LINKED;
        const NdbLinkedOperandImpl& linkedOp = *static_cast<const NdbLinkedOperandImpl*>(key);
        keyPattern.get(keyPatternPos++) = QueryPattern::col(linkedOp.getLinkedColumnIx());
        break;
      }
      case NdbQueryOperandImpl::Const:
      {
        appendedPattern |= DABits::NI_KEY_CONSTS;
        const NdbConstOperandImpl& constOp 
	  = *static_cast<const NdbConstOperandImpl*>(key);
     
        // No of words needed for storing the constant data.
        const Uint32 wordCount =  AttributeHeader::getDataSize(constOp.getLength());
        // Set type and length in words of key pattern field. 
        keyPattern.get(keyPatternPos++) = QueryPattern::data(wordCount);
        keyPatternPos += keyPattern.append(constOp.getAddr(),constOp.getLength());
        break;
      }
      case NdbQueryOperandImpl::Param:
      {
        appendedPattern |= DABits::NI_KEY_PARAMS;
        paramCnt++;
        const NdbParamOperandImpl& paramOp = *static_cast<const NdbParamOperandImpl*>(key);
        keyPattern.get(keyPatternPos++) = QueryPattern::param(paramOp.getParamIx());
        break;
      }
      default:
        assert(false);
      }
      key = m_keys[++keyNo];
    } while (key!=NULL);

    // Set total length of key pattern.
    keyPattern.get(0) = (paramCnt << 16) | (keyPatternPos-1);
  }

  return appendedPattern;
} // appendKeyPattern

int
NdbQueryLookupOperationDefImpl
::serializeOperation(Uint32Buffer& serializedDef) const
{
  assert (m_keys[0]!=NULL);

  Uint32Slice nodeBuffer(serializedDef);
  QN_LookupNode& node = reinterpret_cast<QN_LookupNode&>
    (nodeBuffer.get(0, QN_LookupNode::NodeSize));
  node.tableId = getTable().getObjectId();
  node.tableVersion = getTable().getObjectVersion();
  node.requestInfo = 0;

  /**
   * NOTE: Order of sections within the optional part is fixed as:
   *    Part1:  'NI_HAS_PARENT'
   *    Part2:  'NI_KEY_PARAMS, NI_KEY_LINKED, NI_KEY_CONST'
   *    PART3:  'NI_LINKED_ATTR ++
   */

  // Optional part1: Make list of parent nodes.
  if (getNoOfParentOperations()>0){
    node.requestInfo |= DABits::NI_HAS_PARENT;
    appendParentList (serializedDef);
  }

  // Part2: Append m_keys[] values specifying lookup key.
//if (getQueryOperationIx() > 0) {
    node.requestInfo |= appendKeyPattern(serializedDef, m_keys);
//}

  /* Add the projection that should be send to the SPJ block such that 
   * child operations can be instantiated.*/
  if (getNoOfChildOperations()>0) {
    node.requestInfo |= DABits::NI_LINKED_ATTR;
    Uint16Sequence spjProjSeq(serializedDef);
    for (Uint32 i = 0; i<getSPJProjection().size(); i++) {
      spjProjSeq.append(getSPJProjection()[i]->getColumnNo());
    }
    spjProjSeq.finish();
  }

  // Set node length and type.
  const size_t length = nodeBuffer.getSize();
  QueryNode::setOpLen(node.len, QueryNode::QN_LOOKUP, length);
  if (unlikely(serializedDef.isMaxSizeExceeded())) {
    return QRY_DEFINITION_TOO_LARGE; //Query definition too large.
  }

#ifdef TRACE_SERIALIZATION
  ndbout << "Serialized node " << getQueryOperationId() << " : ";
  for (Uint32 i = 0; i < length; i++) {
    char buf[12];
    sprintf(buf, "%.8x", nodeBuffer.get(i));
    ndbout << buf << " ";
  }
  ndbout << endl;
#endif

  return 0;
} // NdbQueryLookupOperationDefImpl::serializeOperation


int
NdbQueryIndexOperationDefImpl
::serializeOperation(Uint32Buffer& serializedDef) const
{
  assert (m_keys[0]!=NULL);

  /**
   * Serialize index as a seperate lookupNode
   */
  {
    Uint32Slice indexBuffer(serializedDef);
    QN_LookupNode& node = reinterpret_cast<QN_LookupNode&>
      (indexBuffer.get(0, QN_LookupNode::NodeSize));
    node.tableId = getIndex()->getObjectId();
    node.tableVersion = getIndex()->getObjectVersion();
    node.requestInfo = 0;

    // Optional part1: Make list of parent nodes.
    assert (getQueryOperationId() > 0);
    if (getNoOfParentOperations()>0) {
      node.requestInfo |= DABits::NI_HAS_PARENT;
      appendParentList (serializedDef);
    }

    // Part2: m_keys[] are the keys to be used for index
//  if (getQueryOperationIx() > 0) {
      node.requestInfo |= appendKeyPattern(serializedDef, m_keys);
//  }

    /* Basetable is executed as child operation of index:
     * Add projection of NDB$PK column which is hidden *after* last index column.
     */
    {
      node.requestInfo |= DABits::NI_LINKED_ATTR;
      Uint16Sequence spjProjSeq(serializedDef);
      spjProjSeq.append(getIndex()->getNoOfColumns());
      spjProjSeq.finish();
    }

    // Set node length and type.
    const size_t length = indexBuffer.getSize();
    QueryNode::setOpLen(node.len, QueryNode::QN_LOOKUP, length);
    if (unlikely(serializedDef.isMaxSizeExceeded())) {
      return QRY_DEFINITION_TOO_LARGE; //Query definition too large.
    }

#ifdef TRACE_SERIALIZATION
    ndbout << "Serialized index " << getQueryOperationId()-1 << " : ";
    for (Uint32 i = 0; i < length; i++){
      char buf[12];
      sprintf(buf, "%.8x", indexBuffer.get(i));
      ndbout << buf << " ";
    }
    ndbout << endl;
#endif
  } // End: Serialize index table

  Uint32Slice nodeBuffer(serializedDef);
  QN_LookupNode& node = reinterpret_cast<QN_LookupNode&>
    (nodeBuffer.get(0, QN_LookupNode::NodeSize));
  node.tableId = getTable().getObjectId();
  node.tableVersion = getTable().getObjectVersion();
  node.requestInfo = 0;

  /**
   * NOTE: Order of sections within the optional part is fixed as:
   *    Part1:  'NI_HAS_PARENT'
   *    Part2:  'NI_KEY_PARAMS, NI_KEY_LINKED, NI_KEY_CONST'
   *    PART3:  'NI_LINKED_ATTR ++
   */

  // Optional part1: Append index as parent op..
  { node.requestInfo |= DABits::NI_HAS_PARENT;
    Uint16Sequence parentSeq(serializedDef);
    parentSeq.append(getQueryOperationId()-1);
    parentSeq.finish();
  }

  // Part2: Append projected NDB$PK column as index -> table linkage
  {
    Uint32Slice keyPattern(serializedDef);
    node.requestInfo |= DABits::NI_KEY_LINKED;
    keyPattern.get(0) = 1; // Key pattern contains only the single PK column
    keyPattern.get(1) = QueryPattern::colPk(0);
  }

  /* Add the projection that should be send to the SPJ block such that 
   * child operations can be instantiated.*/
  if (getNoOfChildOperations()>0) {
    node.requestInfo |= DABits::NI_LINKED_ATTR;
    Uint16Sequence spjProjSeq(serializedDef);
    for (Uint32 i = 0; i<getSPJProjection().size(); i++) {
      spjProjSeq.append(getSPJProjection()[i]->getColumnNo());
    }
    spjProjSeq.finish();
  }

  // Set node length and type.
  const size_t length = nodeBuffer.getSize();
  QueryNode::setOpLen(node.len, QueryNode::QN_LOOKUP, length);
  if (unlikely(serializedDef.isMaxSizeExceeded())) {
    return QRY_DEFINITION_TOO_LARGE; //Query definition too large.
  }

#ifdef TRACE_SERIALIZATION
  ndbout << "Serialized node " << getQueryOperationId() << " : ";
  for (Uint32 i = 0; i < length; i++) {
    char buf[12];
    sprintf(buf, "%.8x", nodeBuffer.get(i));
    ndbout << buf << " ";
  }
  ndbout << endl;
#endif

  return 0;
} // NdbQueryIndexOperationDefImpl::serializeOperation


int
NdbQueryScanOperationDefImpl::serialize(Uint32Buffer& serializedDef,
                                        const NdbTableImpl& tableOrIndex) const
{
  Uint32Slice nodeBuffer(serializedDef);
  QN_ScanFragNode& node = reinterpret_cast<QN_ScanFragNode&>
    (nodeBuffer.get(0, QN_ScanFragNode::NodeSize));
  node.tableId = tableOrIndex.getObjectId();
  node.tableVersion = tableOrIndex.getObjectVersion();
  node.requestInfo = 0;

  // Optional part1: Make list of parent nodes.
  if (getNoOfParentOperations()>0) {
    node.requestInfo |= DABits::NI_HAS_PARENT;
    appendParentList (serializedDef);
  }

  /** Add the projection that should be send to the SPJ block such that 
   *  child operations can be instantiated.
   */
  if (getNoOfChildOperations()>0){
    node.requestInfo |= DABits::NI_LINKED_ATTR;
    Uint16Sequence spjProjSeq(serializedDef);
    for(Uint32 i = 0; i<getSPJProjection().size(); i++){
      spjProjSeq.append(getSPJProjection()[i]->getColumnNo());
    }
    spjProjSeq.finish();
  }

  // Set node length and type.
  const size_t length = nodeBuffer.getSize();
  QueryNode::setOpLen(node.len, QueryNode::QN_SCAN_FRAG, length);
  if(unlikely(serializedDef.isMaxSizeExceeded())){
    return QRY_DEFINITION_TOO_LARGE; //Query definition too large.
  }    
#ifdef TRACE_SERIALIZATION
  ndbout << "Serialized node " << getQueryOperationId() << " : ";
  for(Uint32 i = 0; i < length; i++){
    char buf[12];
    sprintf(buf, "%.8x", nodeBuffer.get(i));
    ndbout << buf << " ";
  }
  ndbout << endl;
#endif
  return 0;
} // NdbQueryScanOperationDefImpl::serialize


int
NdbQueryTableScanOperationDefImpl
::serializeOperation(Uint32Buffer& serializedDef) const
{
  return NdbQueryScanOperationDefImpl::serialize(serializedDef, getTable());
} // NdbQueryTableScanOperationDefImpl::serializeOperation


int
NdbQueryIndexScanOperationDefImpl
::serializeOperation(Uint32Buffer& serializedDef) const
{
  return NdbQueryScanOperationDefImpl::serialize(serializedDef, *m_index.getIndexTable());
} // NdbQueryIndexScanOperationDefImpl::serializeOperation


// Instantiate Vector templates
template class Vector<NdbQueryOperationDefImpl*>;
template class Vector<NdbQueryOperandImpl*>;

template class Vector<const NdbParamOperandImpl*>;
template class Vector<const NdbColumnImpl*>;

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
