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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <NdbDictionary.hpp>
#include "NdbIndexScanOperation.hpp"
#include "NdbQueryBuilder.hpp"
#include "NdbQueryBuilderImpl.hpp"
#include <ndb_global.h>
#include <Vector.hpp>
#include "signaldata/QueryTree.hpp"

#include "NdbDictionaryImpl.hpp"
#include <NdbRecord.hpp>
#include "AttributeHeader.hpp"
#include "NdbOut.hpp"
#include <NdbInterpretedCode.hpp>

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

// For debugging purposes. Enable to print query tree graph to ndbout.
static const bool doPrintQueryTree = false;

/* Various error codes that are not specific to NdbQuery. */
static const int Err_MemoryAlloc = 4000;
static const int Err_FinaliseNotCalled = 4519;

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
// subclassing of the baseclass NdbConstOperandImpl.
//////////////////////////////////////////////////
class NdbInt64ConstOperandImpl : public NdbConstOperandImpl
{
public:
  explicit NdbInt64ConstOperandImpl (Int64 value) : 
    NdbConstOperandImpl(), 
    m_value(value) {}

private:
  int convertInt8();
  int convertUint8();
  int convertInt16();
  int convertUint16();
  int convertInt24();
  int convertUint24();
  int convertInt32();
  int convertUint32();
  int convertInt64();
  int convertUint64();

  const Int64 m_value;
};

class NdbDoubleConstOperandImpl : public NdbConstOperandImpl
{
public:
  explicit NdbDoubleConstOperandImpl (double value) : 
    NdbConstOperandImpl(), 
    m_value(value) {}

private:
  int convertDouble();
  int convertFloat();
private:
  const double m_value;
};

class NdbCharConstOperandImpl : public NdbConstOperandImpl
{
public:
  explicit NdbCharConstOperandImpl (const char* value) : 
    NdbConstOperandImpl(), m_value(value) {}

private:
  int convertChar();
  int convertVChar();
//int convertLVChar();
private:
  const char* const m_value;
};

class NdbGenericConstOperandImpl : public NdbConstOperandImpl
{
public:
  explicit NdbGenericConstOperandImpl (const void* value,
                                       Uint32 len)
  : NdbConstOperandImpl(),
    m_value(value),
    m_len(len)
  {}

private:
  int convert2ColumnType();

  const void* const m_value;
  const Uint32 m_len;
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
  virtual const NdbQueryOperandImpl* const* getKeyOperands() const
  { return m_keys; }
 
protected:

  explicit NdbQueryLookupOperationDefImpl (
                           const NdbTableImpl& table,
                           const NdbQueryOperand* const keys[],
                           const NdbQueryOptionsImpl& options,
                           const char* ident,
                           Uint32      ix,
                           int& error);

  virtual const NdbQueryLookupOperationDef& getInterface() const
  { return m_interface; }

  // Append pattern for creating lookup key to serialized code 
  Uint32 appendKeyPattern(Uint32Buffer& serializedDef) const;

  virtual bool isScanOperation() const
  { return false; }

protected:
  NdbQueryLookupOperationDef m_interface;
  NdbQueryOperandImpl* m_keys[MAX_ATTRIBUTES_IN_INDEX+1];

}; // class NdbQueryLookupOperationDefImpl

class NdbQueryPKLookupOperationDefImpl : public NdbQueryLookupOperationDefImpl
{
  friend class NdbQueryBuilder;  // Allow privat access from builder interface

public:
  virtual int serializeOperation(Uint32Buffer& serializedDef);

private:

  explicit NdbQueryPKLookupOperationDefImpl (
                           const NdbTableImpl& table,
                           const NdbQueryOperand* const keys[],
                           const NdbQueryOptionsImpl& options,
                           const char* ident,
                           Uint32      ix,
                           int& error)
    : NdbQueryLookupOperationDefImpl(table,keys,options,ident,ix,error)
  {}

  virtual NdbQueryOperationDef::Type getType() const
  { return NdbQueryOperationDef::PrimaryKeyAccess; }

}; // class NdbQueryPKLookupOperationDefImpl


class NdbQueryIndexOperationDefImpl : public NdbQueryLookupOperationDefImpl
{
  friend class NdbQueryBuilder;  // Allow privat access from builder interface

public:
  virtual const NdbIndexImpl* getIndex() const
  { return &m_index; }

  virtual int serializeOperation(Uint32Buffer& serializedDef);

private:

  explicit NdbQueryIndexOperationDefImpl (
                           const NdbIndexImpl& index,
                           const NdbTableImpl& table,
                           const NdbQueryOperand* const keys[],
                           const NdbQueryOptionsImpl& options,
                           const char* ident,
                           Uint32      ix,
                           int& error)
    : NdbQueryLookupOperationDefImpl(table,keys,options,ident,ix,error),
    m_index(index)
  {}

  virtual NdbQueryOperationDef::Type getType() const
  { return NdbQueryOperationDef::UniqueIndexAccess; }

private:
  const NdbIndexImpl& m_index;

}; // class NdbQueryIndexOperationDefImpl


class NdbQueryTableScanOperationDefImpl : public NdbQueryScanOperationDefImpl
{
  friend class NdbQueryBuilder;  // Allow privat access from builder interface

public:
  virtual int serializeOperation(Uint32Buffer& serializedDef);

  virtual const NdbQueryTableScanOperationDef& getInterface() const
  { return m_interface; }

  virtual NdbQueryOperationDef::Type getType() const
  { return NdbQueryOperationDef::TableScan; }

private:

  explicit NdbQueryTableScanOperationDefImpl (
                           const NdbTableImpl& table,
                           const NdbQueryOptionsImpl& options,
                           const char* ident,
                           Uint32      ix,
                           int& error)
    : NdbQueryScanOperationDefImpl(table,options,ident,ix,error),
      m_interface(*this) 
  {}

  NdbQueryTableScanOperationDef m_interface;

}; // class NdbQueryTableScanOperationDefImpl




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

bool
NdbQueryDef::isScanQuery() const
{ return m_impl.isScanQuery();
}

NdbQueryDef::QueryType
NdbQueryDef::getQueryType() const
{ return m_impl.getQueryType();
}

NdbQueryDefImpl& 
NdbQueryDef::getImpl() const{
  return m_impl;
}

void 
NdbQueryDef::destroy() const
{
  delete &getImpl();
}

void
NdbQueryDef::print() const
{
  m_impl.getQueryOperation(0U).printTree(0, Bitmask<(NDB_SPJ_MAX_TREE_NODES+31)/32>());
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
 * Implementation of NdbQueryOptions.
 * Because of the simplicity of this class, the implementation has *not*
 * been split into methods in the Impl class with a glue layer to the
 * outside interface. 
 ****************************************************************************/
static NdbQueryOptionsImpl defaultOptions;

NdbQueryOptions::NdbQueryOptions()
: m_pimpl(&defaultOptions)
{}

NdbQueryOptions::~NdbQueryOptions()
{
  if (m_pimpl!=&defaultOptions)
    delete m_pimpl;
}

const NdbQueryOptionsImpl& 
NdbQueryOptions::getImpl() const
{
  return *m_pimpl;
}

int
NdbQueryOptions::setOrdering(ScanOrdering ordering)
{
  if (m_pimpl==&defaultOptions)
  {
    m_pimpl = new NdbQueryOptionsImpl;
    if (unlikely(m_pimpl==0))
    {
      return Err_MemoryAlloc;
    }
  }
  m_pimpl->m_scanOrder = ordering;
  return 0;
}

int
NdbQueryOptions::setMatchType(MatchType matchType)
{
  if (m_pimpl==&defaultOptions)
  {
    m_pimpl = new NdbQueryOptionsImpl;
    if (unlikely(m_pimpl==0))
    {
      return Err_MemoryAlloc;
    }
  }
  m_pimpl->m_matchType = matchType;
  return 0;
}

int
NdbQueryOptions::setParent(const NdbQueryOperationDef* parent)
{
  if (m_pimpl==&defaultOptions)
  {
    m_pimpl = new NdbQueryOptionsImpl;
    if (unlikely(m_pimpl==0))
    {
      return Err_MemoryAlloc;
    }
  }
  m_pimpl->m_parent = &parent->getImpl();
  return 0;
}

int
NdbQueryOptions::setInterpretedCode(const NdbInterpretedCode& code)
{
  if (m_pimpl==&defaultOptions)
  {
    m_pimpl = new NdbQueryOptionsImpl;
    if (unlikely(m_pimpl==0))
    {
      return Err_MemoryAlloc;
    }
  }
  return m_pimpl->copyInterpretedCode(code);
}


NdbQueryOptionsImpl::~NdbQueryOptionsImpl()
{
  delete m_interpretedCode;
}

NdbQueryOptionsImpl::NdbQueryOptionsImpl(const NdbQueryOptionsImpl& src)
 : m_matchType(src.m_matchType),
   m_scanOrder(src.m_scanOrder),
   m_parent(src.m_parent),
   m_interpretedCode(NULL)
{
  if (src.m_interpretedCode)
  {
    copyInterpretedCode(*src.m_interpretedCode);
  }
}

/* 
 * Make a deep copy, such that 'src' can be destroyed when this method 
 * returns.
 */
int
NdbQueryOptionsImpl::copyInterpretedCode(const NdbInterpretedCode& src)
{
  /* Check the program's finalised */
  if (unlikely(!(src.m_flags & NdbInterpretedCode::Finalised)))
  {
    return Err_FinaliseNotCalled;  // NdbInterpretedCode::finalise() not called.
  }
  if (src.m_instructions_length == 0)
  {
    return 0;
  }

  NdbInterpretedCode* interpretedCode = new NdbInterpretedCode();
  if (unlikely(interpretedCode==NULL))
  {
    return Err_MemoryAlloc;
  }

  const int error = interpretedCode->copy(src);
  if (unlikely(error))
  {
    delete interpretedCode;
    return error;
  }

  /* Replace existing NdbInterpretedCode */
  if (m_interpretedCode)
    delete m_interpretedCode;

  m_interpretedCode = interpretedCode;
  return 0;
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
{
  // Unlink any parent and child refering this object
  if (m_parent != NULL)
  {
    m_parent->removeChild(this);
  }
  for (Uint32 i = 0; i<m_children.size(); i++)
  {
    assert(m_children[i]->m_parent == this);
    m_children[i]->m_parent = NULL;
  }
}

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
NdbQueryOperationDef::getQueryOperationIx() const
{
  return ::getImpl(*this).getQueryOperationIx();
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

const char* 
NdbQueryOperationDef::getTypeName(Type type)
{
  switch(type)
  {
  case PrimaryKeyAccess:
    return "PrimaryKeyAccess";
  case UniqueIndexAccess:
    return "UniqueIndexAccess";
  case TableScan:
    return "TableScan";
  case OrderedIndexScan:
    return "OrderedIndexScan";
  default:
    return "<Invalid NdbQueryOperationDef::Type value>";
  }
}


NdbQueryOperationDef::Type
NdbQueryOperationDef::getType() const
{
  return ::getImpl(*this).getType();
}

const NdbDictionary::Table*
NdbQueryOperationDef::getTable() const
{
  return &::getImpl(*this).getTable();
}

const NdbDictionary::Index*
NdbQueryOperationDef::getIndex() const
{
  return ::getImpl(*this).getIndex();
}

/*******************************************
 * Implementation of NdbQueryBuilder factory
 ******************************************/
// Static method.
NdbQueryBuilder* NdbQueryBuilder::create()
{
  NdbQueryBuilderImpl* const impl = new NdbQueryBuilderImpl();
  if (likely (impl != NULL))
  {
    if (likely(impl->getNdbError().code == 0))
    {
      return &impl->m_interface;
    }
    else
    {
      // Probably failed to create Vector instances.
      assert(impl->getNdbError().code == Err_MemoryAlloc);
      delete impl;
      return NULL;
    }
  }
  else
  {
    return NULL;
  }
}

void NdbQueryBuilder::destroy()
{
  delete &m_impl;
}

NdbQueryBuilder::NdbQueryBuilder(NdbQueryBuilderImpl& impl)
: m_impl(impl)
{}

NdbQueryBuilder::~NdbQueryBuilder()
{}

inline NdbQueryBuilderImpl&
NdbQueryBuilder::getImpl() const
{ return m_impl;
}

const NdbError&
NdbQueryBuilder::getNdbError() const
{
  return m_impl.getNdbError();
}

//////////////////////////////////////////////////
// Implements different const datatypes by further
// subclassing of NdbConstOperand.
/////////////////////////////////////////////////
NdbConstOperand* 
NdbQueryBuilder::constValue(const char* value)
{
  returnErrIf(value==0,QRY_REQ_ARG_IS_NULL);
  return static_cast<NdbConstOperand*>
    (m_impl.addOperand(new NdbCharConstOperandImpl(value)));
}

NdbConstOperand* 
NdbQueryBuilder::constValue(const void* value, Uint32 len)
{
  returnErrIf(value == 0, QRY_REQ_ARG_IS_NULL);
  return static_cast<NdbConstOperand*>
    (m_impl.addOperand(new NdbGenericConstOperandImpl(value,len)));
}

NdbConstOperand* 
NdbQueryBuilder::constValue(Int32 value)
{
  return static_cast<NdbConstOperand*>
    (m_impl.addOperand(new NdbInt64ConstOperandImpl(value)));
}

NdbConstOperand* 
NdbQueryBuilder::constValue(Uint32 value)
{
  return static_cast<NdbConstOperand*>
    (m_impl.addOperand(new NdbInt64ConstOperandImpl(value)));
}

NdbConstOperand* 
NdbQueryBuilder::constValue(Int64 value)
{
  return static_cast<NdbConstOperand*>
    (m_impl.addOperand(new NdbInt64ConstOperandImpl(value)));
}

NdbConstOperand* 
NdbQueryBuilder::constValue(Uint64 value)
{
  return static_cast<NdbConstOperand*>
    (m_impl.addOperand(new NdbInt64ConstOperandImpl(value)));
}

NdbConstOperand* 
NdbQueryBuilder::constValue(double value)
{
  return static_cast<NdbConstOperand*>
    (m_impl.addOperand(new NdbDoubleConstOperandImpl(value)));
}

NdbParamOperand* 
NdbQueryBuilder::paramValue(const char* name)
{
  return static_cast<NdbParamOperand*>
    (m_impl.addOperand(new NdbParamOperandImpl(name,getImpl().m_paramCnt++)));
}

NdbLinkedOperand* 
NdbQueryBuilder::linkedValue(const NdbQueryOperationDef* parent, const char* attr)
{
  // Required non-NULL arguments
  returnErrIf(parent==0 || attr==0, QRY_REQ_ARG_IS_NULL);
  NdbQueryOperationDefImpl& parentImpl = parent->getImpl();

  // Parent should be a OperationDef contained in this query builder context
  returnErrIf(!m_impl.contains(&parentImpl), QRY_UNKONWN_PARENT);

  // 'attr' should refer a column from the underlying table in parent:
  const NdbColumnImpl* column = parentImpl.getTable().getColumn(attr);
  returnErrIf(column==0, QRY_UNKNOWN_COLUMN); // Unknown column

  // Locate refered parrent column in parent operations SPJ projection list;
  // Add if not already present
  int error = 0;
  Uint32 colIx = parentImpl.addColumnRef(column, error);
  if (unlikely(error != 0))
  {
    m_impl.setErrorCode(error);
    return NULL;
  }

  return static_cast<NdbLinkedOperand*>
    (m_impl.addOperand(new NdbLinkedOperandImpl(parentImpl,colIx)));
}

const NdbQueryLookupOperationDef*
NdbQueryBuilder::readTuple(const NdbDictionary::Table* table,    // Primary key lookup
                           const NdbQueryOperand* const keys[],  // Terminated by NULL element 
                           const NdbQueryOptions* options,
                           const char* ident)
{
  int i;
  if (m_impl.hasError())
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

  int error = 0;
  NdbQueryPKLookupOperationDefImpl* op =
    new NdbQueryPKLookupOperationDefImpl(tableImpl,
                                       keys, 
                                       options ? options->getImpl() : defaultOptions,
                                       ident,
                                       m_impl.m_operations.size(),
                                       error);

  returnErrIf(m_impl.takeOwnership(op)!=0, Err_MemoryAlloc);
  returnErrIf(error!=0, error); // C'tor returned error, bailout

  Uint32 keyindex = 0;
  for (i=0; i<colcount; ++i)
  {
    const NdbColumnImpl *col = tableImpl.getColumn(i);
    if (col->getPrimaryKey())
    {
      assert (keyindex==col->m_keyInfoPos);
      int error = op->m_keys[col->m_keyInfoPos]->bindOperand(*col,*op);
      returnErrIf(error!=0, error);

      keyindex++;
      if (keyindex >= static_cast<Uint32>(keyfields))
        break;  // Seen all PK fields
    }
  }
  
  return &op->m_interface;
}


const NdbQueryLookupOperationDef*
NdbQueryBuilder::readTuple(const NdbDictionary::Index* index,    // Unique key lookup w/ index
                           const NdbDictionary::Table* table,    // Primary key lookup
                           const NdbQueryOperand* const keys[],  // Terminated by NULL element 
                           const NdbQueryOptions* options,
                           const char* ident)
{
  int i;

  if (m_impl.hasError())
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

  // Only 'UNIQUE' indexes may be used for lookup operations:
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

  int error = 0;
  NdbQueryIndexOperationDefImpl* op = 
    new NdbQueryIndexOperationDefImpl(indexImpl, tableImpl,
                                       keys,
                                       options ? options->getImpl() : defaultOptions,
                                       ident,
                                       m_impl.m_operations.size(),
                                       error);

  returnErrIf(m_impl.takeOwnership(op)!=0, Err_MemoryAlloc);
  returnErrIf(error!=0, error); // C'tor returned error, bailout

  // Bind to Column and check type compatibility
  for (i=0; i<inxfields; ++i)
  {
    const NdbColumnImpl& col = NdbColumnImpl::getImpl(*indexImpl.getColumn(i));
    assert (col.getColumnNo() == i);

    error = keys[i]->getImpl().bindOperand(col,*op);
    returnErrIf(error!=0, error);
  }

  return &op->m_interface;
}


const NdbQueryTableScanOperationDef*
NdbQueryBuilder::scanTable(const NdbDictionary::Table* table,
                           const NdbQueryOptions* options,
                           const char* ident)
{
  if (m_impl.hasError())
    return NULL;
  returnErrIf(table==0, QRY_REQ_ARG_IS_NULL);  // Required non-NULL arguments

  int error = 0;
  NdbQueryTableScanOperationDefImpl* op =
    new NdbQueryTableScanOperationDefImpl(NdbTableImpl::getImpl(*table),
                                          options ? options->getImpl() : defaultOptions,
                                          ident,
                                          m_impl.m_operations.size(),
                                          error);

  returnErrIf(m_impl.takeOwnership(op)!=0, Err_MemoryAlloc);
  returnErrIf(error!=0, error); // C'tor returned error, bailout

  return &op->m_interface;
}


const NdbQueryIndexScanOperationDef*
NdbQueryBuilder::scanIndex(const NdbDictionary::Index* index, 
	                   const NdbDictionary::Table* table,
                           const NdbQueryIndexBound* bound,
                           const NdbQueryOptions* options,
                           const char* ident)
{
  if (m_impl.hasError())
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

  int error = 0;
  NdbQueryIndexScanOperationDefImpl* op =
    new NdbQueryIndexScanOperationDefImpl(indexImpl, tableImpl,
                                          bound,
                                          options ? options->getImpl() : defaultOptions,
                                          ident,
                                          m_impl.m_operations.size(),
                                          error);

  returnErrIf(m_impl.takeOwnership(op)!=0, Err_MemoryAlloc);
  returnErrIf(error!=0, error); // C'tor returned error, bailout

  returnErrIf(op->m_bound.lowKeys  > indexImpl.getNoOfColumns() ||
              op->m_bound.highKeys > indexImpl.getNoOfColumns(),
              QRY_TOO_MANY_KEY_VALUES);

  // Bind lowKeys, and if applicable, highKeys to the column being refered
  Uint32 i;
  for (i=0; i<op->m_bound.lowKeys; ++i)
  {
    const NdbColumnImpl& col = NdbColumnImpl::getImpl(*indexImpl.getColumn(i));

    const int error = (i<op->m_bound.highKeys && op->m_bound.high[i]!=op->m_bound.low[i])
       ? op->m_bound.low[i]->bindOperand(col,*op) || op->m_bound.high[i]->bindOperand(col,*op)
       : op->m_bound.low[i]->bindOperand(col,*op);

    returnErrIf(error!=0, error);
  }

  // Bind any remaining highKeys past '#lowKeys'
  for (; i<op->m_bound.highKeys; ++i)
  {
    const NdbColumnImpl& col = NdbColumnImpl::getImpl(*indexImpl.getColumn(i));
    error = op->m_bound.high[i]->bindOperand(col,*op);
    returnErrIf(error!=0, error);
  }

  return &op->m_interface;
}

const NdbQueryDef*
NdbQueryBuilder::prepare()
{
  const NdbQueryDefImpl* def = m_impl.prepare();
  return (def) ? &def->getInterface() : NULL;
}

////////////////////////////////////////
// The (hidden) Impl of NdbQueryBuilder
////////////////////////////////////////

NdbQueryBuilderImpl::NdbQueryBuilderImpl()
: m_interface(*this),
  m_error(),
  m_operations(),
  m_operands(),
  m_paramCnt(0),
  m_hasError(false)
{
  if (errno == ENOMEM)
  {
    // ENOMEM probably comes from Vector().
    setErrorCode(Err_MemoryAlloc);
  }
}

NdbQueryBuilderImpl::~NdbQueryBuilderImpl()
{
  // Delete all operand and operator in Vector's
  for (Uint32 i=0; i<m_operations.size(); ++i)
  {
    delete m_operations[i];
  }
  for (Uint32 i=0; i<m_operands.size(); ++i)
  {
    delete m_operands[i];
  }
}

void NdbQueryBuilderImpl::setErrorCode(int aErrorCode)
{ 
  DBUG_ASSERT(aErrorCode != 0);
  m_error.code = aErrorCode;
  if (aErrorCode == Err_MemoryAlloc)
  {
    m_hasError = true;
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
  const bool sorted =
    m_operations.size() > 0 &&
    m_operations[0]->getOrdering() != NdbQueryOptions::ScanOrdering_unordered &&
    m_operations[0]->getOrdering() != NdbQueryOptions::ScanOrdering_void;

  int error;
  NdbQueryDefImpl* def = new NdbQueryDefImpl(m_operations, m_operands, error);
  m_operations.clear();
  m_operands.clear();
  m_paramCnt = 0;

  returnErrIf(def==0, Err_MemoryAlloc);
  if(unlikely(error!=0)){
    delete def;
    setErrorCode(error);
    return NULL;
  }

  /* Check if query is sorted and has multiple scan operations. This 
   * combination is not implemented.
   */
  if (sorted && def->getQueryType() == NdbQueryDef::MultiScanQuery)
  {
    delete def;
    setErrorCode(QRY_MULTIPLE_SCAN_SORTED);
    return NULL;
  }

  if (doPrintQueryTree)
  {
    ndbout << "Query tree:" << endl;
    def->getQueryOperation(0U).printTree(0, Bitmask<(NDB_SPJ_MAX_TREE_NODES+31)/32>());
  }

  return def;
}

inline int 
NdbQueryBuilderImpl::takeOwnership(NdbQueryOperandImpl* operand)
{
  if (unlikely(operand == NULL))
  {
    return Err_MemoryAlloc;
  }
  else if (unlikely(m_operands.push_back(operand) != 0))
  {
    assert(errno == ENOMEM);
    delete operand;
    return Err_MemoryAlloc;
  }
  return 0;
}

inline int 
NdbQueryBuilderImpl::takeOwnership(NdbQueryOperationDefImpl* operation)
{
  if (unlikely(operation == NULL))
  {
    return Err_MemoryAlloc;
  }
  else if (unlikely(m_operations.push_back(operation) != 0))
  {
    assert(errno == ENOMEM);
    delete operation;
    return Err_MemoryAlloc;
  }
  return 0;
}

NdbQueryOperand* 
NdbQueryBuilderImpl::addOperand(NdbQueryOperandImpl* operand)
{
  returnErrIf(takeOwnership(operand)!=0, Err_MemoryAlloc);
  return &operand->getInterface();
}

///////////////////////////////////
// The (hidden) Impl of NdbQueryDef
///////////////////////////////////
NdbQueryDefImpl::
NdbQueryDefImpl(const Vector<NdbQueryOperationDefImpl*>& operations,
                const Vector<NdbQueryOperandImpl*>& operands,
                int& error)
 : m_interface(*this), 
   m_operations(operations),
   m_operands(operands)
{
  if (errno == ENOMEM)
  {
    // Failed to allocate memory for m_operations or m_operands.
    error = Err_MemoryAlloc;
    return;
  }

  Uint32 nodeId = 0;

  /* Grab first word, such that serialization of operation 0 will start from 
   * offset 1, leaving space for the length field to be updated later
   */
  m_serializedDef.append(0); 
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
  Uint32 cntLen;
  QueryTree::setCntLen(cntLen, 
		       nodeId,
		       m_serializedDef.getSize());
  m_serializedDef.put(0,cntLen);

#ifdef __TRACE_SERIALIZATION
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
  {
    delete m_operations[i];
  }
  for (Uint32 i=0; i<m_operands.size(); ++i)
  {
    delete m_operands[i];
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

NdbQueryDef::QueryType
NdbQueryDefImpl::getQueryType() const
{
  if (!m_operations[0]->isScanOperation())
    return NdbQueryDef::LookupQuery;

  for (Uint32 i=1; i<m_operations.size(); ++i)
  {
    if (m_operations[i]->isScanOperation())
      return NdbQueryDef::MultiScanQuery;
  }
  return NdbQueryDef::SingleScanQuery;
}

////////////////////////////////////////////////////////////////
// The (hidden) Impl of NdbQueryOperand (w/ various subclasses)
////////////////////////////////////////////////////////////////

/* Implicit typeconversion between related datatypes: */
int NdbInt64ConstOperandImpl::convertUint8()
{
  if (unlikely(m_value < 0 || m_value > 0xFF))
    return QRY_NUM_OPERAND_RANGE;
  m_converted.val.uint8 = (Uint8)m_value;
  m_converted.len = static_cast<Uint32>(sizeof(m_converted.val.uint8));
  return 0;
}
int NdbInt64ConstOperandImpl::convertInt8()
{
  if (unlikely(m_value < -0x80L || m_value > 0x7F))
    return QRY_NUM_OPERAND_RANGE;
  m_converted.val.int8 = (Int8)m_value;
  m_converted.len = static_cast<Uint32>(sizeof(m_converted.val.int8));
  return 0;
}
int NdbInt64ConstOperandImpl::convertUint16()
{
  if (unlikely(m_value < 0 || m_value > 0xFFFF))
    return QRY_NUM_OPERAND_RANGE;
  m_converted.val.uint16 = (Uint16)m_value;
  m_converted.len = static_cast<Uint32>(sizeof(m_converted.val.uint16));
  return 0;
}
int NdbInt64ConstOperandImpl::convertInt16()
{
  if (unlikely(m_value < -0x8000L || m_value > 0x7FFF))
    return QRY_NUM_OPERAND_RANGE;
  m_converted.val.int16 = (Int16)m_value;
  m_converted.len = static_cast<Uint32>(sizeof(m_converted.val.int16));
  return 0;
}
int NdbInt64ConstOperandImpl::convertUint24()
{
  if (unlikely(m_value < 0 || m_value > 0xFFFFFF))
    return QRY_NUM_OPERAND_RANGE;
  m_converted.val.uint32 = (Uint32)m_value;
  m_converted.len = static_cast<Uint32>(sizeof(m_converted.val.uint32));
  return 0;
}
int NdbInt64ConstOperandImpl::convertInt24()
{
  if (unlikely(m_value < -0x800000L || m_value > 0x7FFFFF))
    return QRY_NUM_OPERAND_RANGE;
  m_converted.val.int32 = (Int32)m_value;
  m_converted.len = static_cast<Uint32>(sizeof(m_converted.val.int32));
  return 0;
}
int NdbInt64ConstOperandImpl::convertUint32()
{
  if (unlikely(m_value < 0 || m_value > 0xFFFFFFFF))
    return QRY_NUM_OPERAND_RANGE;
  m_converted.val.uint32 = (Uint32)m_value;
  m_converted.len = static_cast<Uint32>(sizeof(m_converted.val.uint32));
  return 0;
}
int NdbInt64ConstOperandImpl::convertInt32()
{
  if (unlikely(m_value < -((Int64)0x80000000L) || m_value > 0x7FFFFFFF))
    return QRY_NUM_OPERAND_RANGE;
  m_converted.val.int32 = (Int32)m_value;
  m_converted.len = static_cast<Uint32>(sizeof(m_converted.val.int32));
  return 0;
}
int NdbInt64ConstOperandImpl::convertInt64()
{
  m_converted.val.int64 = m_value;
  m_converted.len = static_cast<Uint32>(sizeof(m_converted.val.int64));
  return 0;
}
int NdbInt64ConstOperandImpl::convertUint64()
{
  m_converted.val.uint64 = (Uint64)m_value;
  m_converted.len = static_cast<Uint32>(sizeof(m_converted.val.uint64));
  return 0;
}

int NdbDoubleConstOperandImpl::convertFloat()
{
  m_converted.val.flt = (float)m_value;
  m_converted.len = static_cast<Uint32>(sizeof(m_converted.val.flt));
  return 0;
}
int NdbDoubleConstOperandImpl::convertDouble()
{
  m_converted.val.dbl = m_value;
  m_converted.len = static_cast<Uint32>(sizeof(m_converted.val.dbl));
  return 0;
}

int NdbCharConstOperandImpl::convertChar()
{
  Uint32 len = m_column->getLength();
  Uint32 srclen = (m_value) ? static_cast<Uint32>(strlen(m_value)) : 0;
  if (unlikely(srclen > len)) {
    // TODO: Truncates: May silently remove trailing spaces:
    return QRY_CHAR_OPERAND_TRUNCATED;
    srclen = len;
  }

  char* dst = m_converted.getCharBuffer(len);
  if (unlikely(dst==NULL))
    return Err_MemoryAlloc;

  memcpy (dst, m_value, srclen);
  if (unlikely(srclen < len)) {
    memset (dst+srclen, ' ', len-srclen);
  }
  return 0;
} //NdbCharConstOperandImpl::convertChar


int NdbCharConstOperandImpl::convertVChar()
{
  Uint32 maxlen = m_column->getLength();
  Uint32 len = (m_value) ? static_cast<Uint32>(strlen(m_value)) : 0;
  if (unlikely(len > maxlen)) {
    // TODO: Truncates: May silently remove trailing spaces:
    return QRY_CHAR_OPERAND_TRUNCATED;
    len = maxlen;
  }

  char* dst = m_converted.getCharBuffer(len);
  if (unlikely(dst==NULL))
    return Err_MemoryAlloc;

  memcpy (dst, m_value, len);
  return 0;
} //NdbCharConstOperandImpl::convertVChar


/**
 * GenericConst is 'raw data' with minimal type checking and conversion capability.
 */
int
NdbGenericConstOperandImpl::convert2ColumnType()
{
  Uint32 len = m_len;
  Uint32 maxSize = m_column->getSizeInBytes();

  char* dst = NULL;

  if (likely(m_column->m_arrayType == NDB_ARRAYTYPE_FIXED))
  {
    if (unlikely(len != maxSize))
      return QRY_OPERAND_HAS_WRONG_TYPE;

    dst = m_converted.getCharBuffer(len);
    if (unlikely(dst==NULL))
      return Err_MemoryAlloc;
  }
  else if (m_column->m_arrayType == NDB_ARRAYTYPE_SHORT_VAR)
  {
    if (unlikely(len+1 > maxSize))
      return QRY_CHAR_OPERAND_TRUNCATED;

    dst = m_converted.getCharBuffer(len+1);
    if (unlikely(dst==NULL))
      return Err_MemoryAlloc;

    *(Uint8*)dst++ = (Uint8)len;
  }
  else if (m_column->m_arrayType == NDB_ARRAYTYPE_MEDIUM_VAR)
  {
    if (unlikely(len+2 > maxSize))
      return QRY_CHAR_OPERAND_TRUNCATED;

    dst = m_converted.getCharBuffer(len+2);
    if (unlikely(dst==NULL))
      return Err_MemoryAlloc;

    *(Uint8*)dst++ = (Uint8)(len & 0xFF);
    *(Uint8*)dst++ = (Uint8)(len >> 8);
  }
  else
  {
    DBUG_ASSERT(0);
  }

  memcpy (dst, m_value, len);
  return 0;
}  //NdbGenericConstOperandImpl::convert2ColumnType


int
NdbConstOperandImpl::convert2ColumnType()
{
  switch(m_column->getType()) {
    case NdbDictionary::Column::Tinyint:         return convertInt8();
    case NdbDictionary::Column::Tinyunsigned:    return convertUint8();
    case NdbDictionary::Column::Smallint:        return convertInt16();
    case NdbDictionary::Column::Smallunsigned:   return convertUint16();
    case NdbDictionary::Column::Mediumint:       return convertInt24();
    case NdbDictionary::Column::Mediumunsigned:  return convertUint24();
    case NdbDictionary::Column::Int:             return convertInt32();
    case NdbDictionary::Column::Unsigned:        return convertUint32();
    case NdbDictionary::Column::Bigint:          return convertInt64();
    case NdbDictionary::Column::Bigunsigned:     return convertUint64();
    case NdbDictionary::Column::Float:           return convertFloat();
    case NdbDictionary::Column::Double:          return convertDouble();

    case NdbDictionary::Column::Decimal:         return convertDec();
    case NdbDictionary::Column::Decimalunsigned: return convertUDec();

    case NdbDictionary::Column::Char:            return convertChar();
    case NdbDictionary::Column::Varchar:         return convertVChar();
    case NdbDictionary::Column::Longvarchar:     return convertLVChar();
    case NdbDictionary::Column::Binary:          return convertBin();
    case NdbDictionary::Column::Varbinary:       return convertVBin();
    case NdbDictionary::Column::Longvarbinary:   return convertLVBin();
    case NdbDictionary::Column::Bit:             return convertBit();

    case NdbDictionary::Column::Date:            return convertDate();
    case NdbDictionary::Column::Time:            return convertTime();
    case NdbDictionary::Column::Datetime:        return convertDatetime();
    case NdbDictionary::Column::Timestamp:       return convertTimestamp();
    case NdbDictionary::Column::Year:            return convertYear();

    // Type conversion intentionally not supported (yet)
    case NdbDictionary::Column::Olddecimal:
    case NdbDictionary::Column::Olddecimalunsigned: 
    case NdbDictionary::Column::Blob:
    case NdbDictionary::Column::Text: 
      // Fall through:

    default:
    case NdbDictionary::Column::Undefined:    return QRY_OPERAND_HAS_WRONG_TYPE;
  }

  return 0;
} //NdbConstOperandImpl::convert2ColumnType


int
NdbConstOperandImpl::bindOperand(
                           const NdbColumnImpl& column,
                           NdbQueryOperationDefImpl& operation)
{
  const int error = NdbQueryOperandImpl::bindOperand(column,operation);
  if (unlikely(error))
    return error;

  return convert2ColumnType();
}


int
NdbLinkedOperandImpl::bindOperand(
                           const NdbColumnImpl& column,
                           NdbQueryOperationDefImpl& operation)
{
  const NdbColumnImpl& parentColumn = getParentColumn();
  
  if (unlikely(column.m_type      != parentColumn.m_type ||
               column.m_precision != parentColumn.m_precision ||
               column.m_scale     != parentColumn.m_scale ||
               column.m_length    != parentColumn.m_length ||
               column.m_cs        != parentColumn.m_cs))
    return QRY_OPERAND_HAS_WRONG_TYPE;  // Incompatible datatypes

  if (unlikely(column.m_type == NdbDictionary::Column::Blob ||
               column.m_type == NdbDictionary::Column::Text))
    return QRY_OPERAND_HAS_WRONG_TYPE;  // BLOB/CLOB datatypes intentionally not supported

  // TODO: Allow and autoconvert compatible datatypes

  // Register parent/child operation relations
  const int error = operation.linkWithParent(&this->m_parentOperation);
  if (unlikely(error))
    return error;

  return NdbQueryOperandImpl::bindOperand(column,operation);
}


int
NdbParamOperandImpl::bindOperand(
                           const NdbColumnImpl& column,
                           NdbQueryOperationDefImpl& operation)
{
  if (unlikely(column.m_type == NdbDictionary::Column::Blob ||
               column.m_type == NdbDictionary::Column::Text))
    return QRY_OPERAND_HAS_WRONG_TYPE;  // BLOB/CLOB datatypes intentionally not supported

  const int res = operation.addParamRef(this);
  if (unlikely(res != 0))
  {
    return res;
  }
  return NdbQueryOperandImpl::bindOperand(column,operation);
}


/////////////////////////////////////////////////////////////////
// The (hidden) Impl of NdbQueryOperation (w/ various subclasses)
///////////////////////>/////////////////////////////////////////

NdbQueryLookupOperationDefImpl::NdbQueryLookupOperationDefImpl (
                           const NdbTableImpl& table,
                           const NdbQueryOperand* const keys[],
                           const NdbQueryOptionsImpl& options,
                           const char* ident,
                           Uint32      ix,
                           int& error)
  : NdbQueryOperationDefImpl(table,options,ident,ix,error),
   m_interface(*this)
{
  int i;
  for (i=0; i<MAX_ATTRIBUTES_IN_INDEX; ++i)
  { if (keys[i] == NULL)
      break;
    m_keys[i] = &keys[i]->getImpl();
  }
  assert (i > 0);
  assert (keys[i] == NULL);
  m_keys[i] = NULL;
}

NdbQueryIndexScanOperationDefImpl::NdbQueryIndexScanOperationDefImpl (
                           const NdbIndexImpl& index,
                           const NdbTableImpl& table,
                           const NdbQueryIndexBound* bound,
                           const NdbQueryOptionsImpl& options,
                           const char* ident,
                           Uint32      ix,
                           int& error)
  : NdbQueryScanOperationDefImpl(table,options,ident,ix,error),
  m_interface(*this), 
  m_index(index)
{
  memset(&m_bound, 0, sizeof m_bound);
  if (bound!=NULL) {

    if (bound->m_low!=NULL) {
      int i;
      for (i=0; bound->m_low[i] != NULL; ++i)
      { assert (i<MAX_ATTRIBUTES_IN_INDEX);
        m_bound.low[i] = &bound->m_low[i]->getImpl();
      }
      m_bound.lowKeys = i;
    } else {
      m_bound.lowKeys = 0;
    }

    if (bound->m_high!=NULL) {
      int i;
      for (i=0; bound->m_high[i] != NULL; ++i)
      { assert (i<MAX_ATTRIBUTES_IN_INDEX);
        m_bound.high[i] = &bound->m_high[i]->getImpl();
      }
      m_bound.highKeys = i;
    } else {
      m_bound.highKeys = 0;
    }

    m_bound.lowIncl = bound->m_lowInclusive;
    m_bound.highIncl = bound->m_highInclusive;
  }
  else {
    m_bound.lowKeys = m_bound.highKeys = 0;
    m_bound.lowIncl = m_bound.highIncl = true;
  }
}

int
NdbQueryIndexScanOperationDefImpl::checkPrunable(
                              const Uint32Buffer& keyInfo,
			      Uint32  shortestBound,
                              bool&   isPruned,
                              Uint32& hashValue) const  // 'hashValue' only defined if 'isPruned'
{ 
  /**
   * Determine if scan may be pruned to a single partition:
   */
  isPruned = false;
  const NdbRecord* const tableRecord = getTable().getDefaultRecord();
  const NdbRecord* const indexRecord = m_index.getDefaultRecord();
  /**
   * This is the prefix (in number of fields) of the index key that will contain
   * all the distribution key fields.
   */
  const Uint32 prefixLength = indexRecord->m_min_distkey_prefix_length;

  if (indexRecord->m_no_of_distribution_keys != tableRecord->m_no_of_distribution_keys)
  {
    return 0; // Index does not contain all fields in the distribution key.
  } 
  else if (shortestBound < prefixLength)
  {
    // Bounds set on query instance are to short to contain full dist key.
    return 0; 
  }
  // Bounds being part of query definitions should have been covered by 'shortestBound' above
  assert( (m_bound.lowKeys+m_bound.highKeys==0) ||
          (m_bound.lowKeys >= prefixLength && m_bound.highKeys >= prefixLength));

  /** 
   * The scan will be prunable if all upper and lower bound pairs are equal
   * for the prefix containing the distribution key, and if all bounds
   * give the same hash value for the distribution key.
   */
  Uint32 keyPos = 0;

  // Loop over all bounds.
  Uint32 boundNo = 0;
  while (keyPos < keyInfo.getSize())
  {
    const Uint32 keyEnd = keyPos + (keyInfo.get(keyPos) >> 16);
    Ndb::Key_part_ptr distKey[NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY+1]
      = {{NULL, 0}};
    
    // Loop over the fields in each bound.
    Uint32 keyPartNo = 0;
    Uint32 distKeyPartNo = 0;
    while (keyPos < keyEnd)
    {
      const NdbIndexScanOperation::BoundType type = 
        static_cast<NdbIndexScanOperation::BoundType>
        (keyInfo.get(keyPos) & 0xF);
      const AttributeHeader attHead1(keyInfo.get(keyPos+1));
      const Ndb::Key_part_ptr keyPart1 = { keyInfo.addr(keyPos+2),
                                           attHead1.getByteSize() };
  
      keyPos += 1+1+attHead1.getDataSize();  // Skip data read above.

      const NdbColumnImpl& column 
        = NdbColumnImpl::getImpl(*m_index.getColumn(keyPartNo));

      switch (type)
      {
      case NdbIndexScanOperation::BoundEQ:
        break;
      case NdbIndexScanOperation::BoundGE:
      case NdbIndexScanOperation::BoundGT:
        /**
         * We have a one-sided limit for this field, which is part of
         * the prefix containing the distribution key. We thus cannot prune. 
         */
        return 0;
      case NdbIndexScanOperation::BoundLE:
      case NdbIndexScanOperation::BoundLT:
        /**
         * If keyPart1 is a lower limit for this column, there may be an upper 
         * limit also.
         */
        if (keyPos == keyEnd ||
            ((keyInfo.get(keyPos) & 0xF) != NdbIndexScanOperation::BoundGE &&
             (keyInfo.get(keyPos) & 0xF) != NdbIndexScanOperation::BoundGT))
        {
          /**
           * We have a one-sided limit for this field, which is part of
           * the prefix containing the distribution key. We thus cannot prune. 
           */
          return 0;
        }
        else // There is an upper limit.
        {
          const AttributeHeader attHeadHigh(keyInfo.get(keyPos+1));
          const Ndb::Key_part_ptr highKeyPart = { keyInfo.addr(keyPos+2),
                                                  attHeadHigh.getByteSize() };
  
          keyPos += 1+1+attHeadHigh.getDataSize();  // Skip data read above.
  
          /**
           * We must compare key parts in the prefix that contains the 
           * distribution key. Even if subseqent parts should be different,
           * all matching tuples must have the same (distribution) hash.
           *
           * For example, assume there is a ordered index on {a, dist_key, b}.
           * Then any tuple in the range {a=c1, <dist key=c2>, b=c3} to 
           * {a=c1, dist_key=c2, b=c4} will have dist_key=c2.
           * Then consider a range where fields before the distribution key are 
           * different, e.g. {a=c6, <dist key=c7>, b=c8} to 
           * {a=c9, <dist key=c7>, b=c8} then matching tuples can have any value 
           * for the distribution key as long as c6 <= a <= c9, so there can be 
           * no pruning.
           */
          assert(column.m_keyInfoPos < tableRecord->noOfColumns);
          const NdbRecord::Attr& recAttr = tableRecord->columns[column.m_keyInfoPos];
          /**
           * Shinked varchars should already have been converted in 
           * NdbQueryImpl::setBound(), so no need to deal with them here.
           * (See also bug#56853 and http://lists.mysql.com/commits/121387 .)
           */
          assert((recAttr.flags & NdbRecord::IsMysqldShrinkVarchar) == 0);
          const int res=
            (*recAttr.compare_function)(recAttr.charset_info,
                                        keyPart1.ptr, keyPart1.len,
                                        highKeyPart.ptr, highKeyPart.len);
          if (res!=0)
          {  // Not equal
            return 0;
          }
        } // if (keyPos == keyEnd ||
        break;
      default:
        assert(false);
      } // switch (type)

      // If this field is part of the distribution key:
      // Keep the key value for later use by Ndb::computeHash.
      if (getTable().m_columns[column.m_keyInfoPos]->m_distributionKey)
      {
        /* Find the proper place for this field in the distribution key.*/
        Ndb::Key_part_ptr* distKeyPtr = distKey;
        for (Uint32 i = 0; i < column.m_keyInfoPos; i++)
        {
          if (getTable().m_columns[i]->m_distributionKey)
          {
            distKeyPtr++;
          }
        }
          
        assert(distKeyPtr->len == 0 && distKeyPtr->ptr == NULL);
        *distKeyPtr = keyPart1;
        distKeyPartNo++;
      }

      keyPartNo++;
      if (keyPartNo == prefixLength)
      {
        /** 
         * Skip key parts after the prefix containing the distribution key,
         * as these do not affect prunability.
         */
        keyPos = keyEnd;
      }
    } // while (keyPos < keyEnd)
    
    assert(distKeyPartNo == tableRecord->m_no_of_distribution_keys);
    
    // hi/low are equal and prunable bounds.
    Uint32 newHashValue = 0;
    const int error = Ndb::computeHash(&newHashValue, &getTable(), distKey, 
                                       NULL, 0);
    if (unlikely(error))
      return error;

    if (boundNo == 0)
    {
      hashValue = newHashValue;
    }
    else if (hashValue != newHashValue)
    {
      /* This bound does not have the same hash value as the previous one. 
       * So we make the pessimistic assumtion that it will not hash to the 
       * same node. (See also comments in 
       * NdbScanOperation::getPartValueFromInfo()).
       */
      return 0;
    }

    boundNo++;
  } // while (keyPos < keyInfo.getSize())

  isPruned = true;
  return 0;
} // NdbQueryIndexScanOperationDefImpl::checkPrunable


NdbQueryOperationDefImpl::NdbQueryOperationDefImpl (
                                     const NdbTableImpl& table,
                                     const NdbQueryOptionsImpl& options,
                                     const char* ident,
                                     Uint32      ix,
                                     int& error)
  :m_isPrepared(false), 
   m_diskInChildProjection(false), 
   m_table(table), 
   m_ident(ident), 
   m_ix(ix), m_id(ix),
   m_options(options),
   m_parent(NULL), 
   m_children(), 
   m_params(),
   m_spjProjection() 
{
  if (unlikely(errno == ENOMEM))
  {
    // Heap allocation in Vector() must have failed.
    error = Err_MemoryAlloc;
    return;
  }
  if (m_options.m_parent != NULL)
  {
    m_parent = m_options.m_parent;
    const int res = m_parent->addChild(this);
    if (unlikely(res != 0))
    {
      error = res;
      return;
    }
  }  // else, ::linkWithParent() will assign 'm_parent'
}


int
NdbQueryOperationDefImpl::addChild(NdbQueryOperationDefImpl* childOp)
{
  for (Uint32 i=0; i<m_children.size(); ++i)
  { if (m_children[i] == childOp)
      return 0;
  }
  if (likely(m_children.push_back(childOp) == 0))
  {
    return 0;
  }
  else
  {
    assert(errno == ENOMEM);
    return Err_MemoryAlloc;
  }
}

void
NdbQueryOperationDefImpl::removeChild(const NdbQueryOperationDefImpl* childOp)
{
  for (unsigned i=0; i<m_children.size(); ++i)
  {
    if (m_children[i] == childOp)
    {
      m_children.erase(i);
      return;
    }
  }
}

bool
NdbQueryOperationDefImpl::isChildOf(const NdbQueryOperationDefImpl* parentOp) const
{
  if (m_parent != NULL)
  { if (this->m_parent == parentOp)
    {
#ifndef NDEBUG
      // Assert that parentOp also refer 'this' as a child.
      for (Uint32 j=0; j<parentOp->getNoOfChildOperations(); j++)
      { if (&parentOp->getChildOperation(j) == this)
          return true;
      }
      assert(false);
#endif
      return true;
    }
    else if (m_parent->isChildOf(parentOp))
    {
      return true;
    }
  }
  return false;
}

int
NdbQueryOperationDefImpl::linkWithParent(NdbQueryOperationDefImpl* parentOp)
{
  if (this->isChildOf(parentOp))
  {
    // There should only be a single parent/child relationship registered.
    return 0;
  }

  if (m_parent != NULL)
  {
    /**
     * Multiple parental relationship not allowed.
     * It is likely that the conflict is due to one of the
     * parents actually being a grand parent.
     * This can be resolved if existing parent actually was a grandparent.
     * Then register new parentOp as the real parrent.
     */
    if (parentOp->isChildOf(m_parent))
    { // Remove existing grandparent linkage being replaced by parentOp.
      m_parent->removeChild(this);
      m_parent = NULL;
    }
    else
    { // This is a real multiparent error.
      return QRY_MULTIPLE_PARENTS;
    }
  }
  m_parent = parentOp;
  parentOp->addChild(this);
  return 0;
}


// Register a linked reference to a column available from this operation
Uint32
NdbQueryOperationDefImpl::addColumnRef(const NdbColumnImpl* column,
                                       int& error)
{
  Uint32 spjRef;
  for (spjRef=0; spjRef<m_spjProjection.size(); ++spjRef)
  { if (m_spjProjection[spjRef] == column)
      return spjRef;
  }

  // Add column if not already available
  if (unlikely(m_spjProjection.push_back(column) != 0))
  {
    assert(errno == ENOMEM);
    error = Err_MemoryAlloc;
    return ~0;
  }
  if (column->getStorageType() == NDB_STORAGETYPE_DISK)
  {
    m_diskInChildProjection = true;
  }
  return spjRef;
}

int NdbQueryOperationDefImpl::addParamRef(const NdbParamOperandImpl* param)
{ 
  if (unlikely(m_params.push_back(param) != 0))
  {
    assert(errno == ENOMEM);
    return Err_MemoryAlloc;
  }
  return 0;
}

/** This class is used for serializing sequences of 16 bit integers,
 * where the first 16 bit integer specifies the length of the sequence.
 */
class Uint16Sequence{
public:
  explicit Uint16Sequence(Uint32Buffer& buffer, Uint32 size):
    m_seq(NULL),
    m_size(size),
    m_pos(0),
    m_finished(false)
 {
    m_seq = buffer.alloc(1 + size/2);
    assert (size <= 0xFFFF);
    m_seq[0] = size;
  }

  ~Uint16Sequence()
  { assert(m_finished);
  }

  /** Add an item to the sequence.*/
  void append(Uint16 value) {
    assert(m_pos < m_size);
    assert(m_seq);
    m_pos++;
    if ((m_pos & 1) == 1) {
      m_seq[m_pos/2] |= (value<<16);
    } else {
      m_seq[m_pos/2] = value;
    }
  }


  /** End the sequence and pad possibly unused Int16 word at end. */
  void finish() {
    assert(m_pos == m_size);
    assert(!m_finished);
    m_finished = true;
    if (m_pos>0) {
      if ((m_pos & 1) == 0) {
	m_seq[m_pos/2] |= (0xBABE<<16);
      }
    }
  }

private:
  /** Should not be copied.*/
  Uint16Sequence(Uint16Sequence&);
  /** Should not be assigned.*/
  Uint16Sequence& operator=(Uint16Sequence&);

  Uint32*       m_seq;      // Preallocated buffer to append Uint16's into
  const Uint32  m_size;     // Uint16 words available in m_seq
  Uint32        m_pos;      // Current Uint16 word to fill in
  bool          m_finished; // Debug assert of correct call convention
};


Uint32
NdbQueryOperationDefImpl::appendParentList(Uint32Buffer& serializedDef) const
{
  if (getParentOperation() != NULL)
  {
    Uint16Sequence parentSeq(serializedDef, 1);
    assert (getParentOperation()->getQueryOperationId() < getQueryOperationId());
    parentSeq.append(getParentOperation()->getQueryOperationId());
    parentSeq.finish();
    return DABits::NI_HAS_PARENT;
  }
  return 0;
} // NdbQueryOperationDefImpl::appendParentList


/* Add the projection that should be send to the SPJ block such that 
 * child operations can be instantiated.
 */
Uint32
NdbQueryOperationDefImpl::appendChildProjection(Uint32Buffer& serializedDef) const
{
  Uint32 requestInfo = 0;
  if (m_spjProjection.size() > 0 || getNoOfChildOperations() > 0)
  {
    requestInfo |= DABits::NI_LINKED_ATTR;
    Uint16Sequence spjProjSeq(serializedDef, m_spjProjection.size());
    for (Uint32 i = 0; i<m_spjProjection.size(); i++)
    {
      spjProjSeq.append(m_spjProjection[i]->getColumnNo());
    }
    spjProjSeq.finish();

    if (m_diskInChildProjection)
    {
      requestInfo |= DABits::NI_LINKED_DISK;
    }
  }
  return requestInfo;
} // NdbQueryOperationDefImpl::appendChildProjection

/** Used by NdbQueryOperationDefImpl::printTree() to print the arrows
 * that connect the tree nodes.
 */
static void printMargin(Uint32 depth, 
                        Bitmask<(NDB_SPJ_MAX_TREE_NODES+31)/32> hasMoreSiblingsMask, 
                        bool header)
{
  if (depth > 0)
  {
    // Print vertical lines to the siblings of the ancestore nodes.
    for (Uint32 i = 0; i<depth-1; i++)
    {
      if (hasMoreSiblingsMask.get(i+1))
      {
        ndbout << "|  ";
      }
      else
      {
        ndbout << "   ";
      }
    }
    if (header)
    {
      ndbout << "+->";
    }
    else if (hasMoreSiblingsMask.get(depth))
    {
      ndbout << "|  ";
    }
    else
    {
      ndbout << "   ";
    }
  }
}

void 
NdbQueryOperationDefImpl::printTree(Uint32 depth, 
                                    Bitmask<(NDB_SPJ_MAX_TREE_NODES+31)/32> 
                                    hasMoreSiblingsMask) const
{
  // Print vertical line leading down to this node.
  Bitmask<(NDB_SPJ_MAX_TREE_NODES+31)/32> firstLineMask = hasMoreSiblingsMask;
  firstLineMask.set(depth);
  printMargin(depth, firstLineMask, false);
  ndbout << endl;
  // Print +-> leading to this node.
  printMargin(depth, hasMoreSiblingsMask, true);
  ndbout << NdbQueryOperationDef::getTypeName(getType()) << endl;
  printMargin(depth, hasMoreSiblingsMask, false);
  // Print attributes.
  ndbout << " opNo: " << getQueryOperationIx() << endl;
  printMargin(depth, hasMoreSiblingsMask, false);
  ndbout << " table: " << getTable().getName() << endl;
  if (getIndex() != NULL)
  {
    printMargin(depth, hasMoreSiblingsMask, false);
    ndbout << " index: " << getIndex()->getName() << endl; 
  }
  /* For each child but the last one, use a mask with an extra bit set to
   * indicate that there are more siblings.
   */
  hasMoreSiblingsMask.set(depth+1);
  for (int childNo = 0; 
       childNo < static_cast<int>(getNoOfChildOperations()) - 1; 
       childNo++)
  {
    getChildOperation(childNo).printTree(depth+1, hasMoreSiblingsMask);
  }
  if (getNoOfChildOperations() > 0)
  {
    // The last child has no more siblings.
    hasMoreSiblingsMask.clear(depth+1);
    getChildOperation(getNoOfChildOperations() - 1)
      .printTree(depth+1, hasMoreSiblingsMask);
  }
} // NdbQueryOperationDefImpl::printTree()


Uint32
NdbQueryLookupOperationDefImpl::appendKeyPattern(Uint32Buffer& serializedDef) const
{
  Uint32 appendedPattern = 0;

  /**
   * Key value for root operation is constructed when query is instantiated with
   * NdbQueryOperationImpl::prepareIndexKeyInfo()
   */
  if (getQueryOperationIx() == 0)
    return 0;

  if (m_keys[0]!=NULL)
  {
    Uint32 startPos = serializedDef.getSize();
    serializedDef.append(0);     // Grab first word for length field, updated at end
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
        const NdbQueryOperationDefImpl* parent = getParentOperation();
        uint32 levels = 0;
        while (parent != &linkedOp.getParentOperation())
        {
          if (parent->getType() == NdbQueryOperationDef::UniqueIndexAccess)  // Represented with two nodes in QueryTree
            levels+=2;
          else
            levels+=1;
          parent = parent->getParentOperation();
          assert(parent != NULL);
        }
        if (levels > 0)
        {
          serializedDef.append(QueryPattern::parent(levels));
        }
        serializedDef.append(QueryPattern::col(linkedOp.getLinkedColumnIx()));
        break;
      }
      case NdbQueryOperandImpl::Const:
      {
        appendedPattern |= DABits::NI_KEY_CONSTS;
        const NdbConstOperandImpl& constOp = *static_cast<const NdbConstOperandImpl*>(key);
     
        // No of words needed for storing the constant data.
        const Uint32 wordCount =  AttributeHeader::getDataSize(constOp.getSizeInBytes());
        // Set type and length in words of key pattern field. 
        serializedDef.append(QueryPattern::data(wordCount));
        serializedDef.appendBytes(constOp.getAddr(),constOp.getSizeInBytes());
        break;
      }
      case NdbQueryOperandImpl::Param:
      {
        appendedPattern |= DABits::NI_KEY_PARAMS;
        serializedDef.append(QueryPattern::param(paramCnt++));
        break;
      }
      default:
        assert(false);
      }
      key = m_keys[++keyNo];
    } while (key!=NULL);

    // Set total length of key pattern.
    Uint32 len = serializedDef.getSize() - startPos -1;
    serializedDef.put(startPos, (paramCnt << 16) | (len));
  }

  return appendedPattern;
} // NdbQueryLookupOperationDefImpl::appendKeyPattern


Uint32
NdbQueryIndexScanOperationDefImpl::appendPrunePattern(Uint32Buffer& serializedDef) const
{
  Uint32 appendedPattern = 0;

  /**
   * Bound value for root operation is constructed when query is instantiated with
   * NdbQueryOperationImpl::prepareIndexKeyInfo()
   */
  if (getQueryOperationIx() == 0)
    return 0;

  if (m_bound.lowKeys>0 || m_bound.highKeys>0)
  {
    const NdbRecord* const tableRecord = getTable().getDefaultRecord();
    const NdbRecord* const indexRecord = m_index.getDefaultRecord();

    if (indexRecord->m_no_of_distribution_keys != tableRecord->m_no_of_distribution_keys)
    {
      return 0; // Index does not contain all fields in the distribution key.
    } 

    /**
     * This is the prefix (in number of fields) of the index key that will contain
     * all the distribution key fields.
     */
    Uint32 distKeys = indexRecord->m_min_distkey_prefix_length;
    if (m_bound.lowKeys < distKeys || m_bound.highKeys < distKeys)
    {
      // Bounds set on query definition are to short to contain full dist key.
      return 0; 
    }

    /* All low/high bounds should be defined equal within the 'distKey' */
    for (unsigned keyNo = 0; keyNo < distKeys; keyNo++)
    {
      if (m_bound.low[keyNo] != m_bound.high[keyNo])
        return 0; 
    }

    {
      int paramCnt = 0;
      Uint32 startPos = serializedDef.getSize();
      serializedDef.append(0);     // Grab first word for length field, updated at end

      for (unsigned i = 0; i < indexRecord->distkey_index_length; i++)
      {
        const unsigned keyNo = indexRecord->distkey_indexes[i];
        assert(keyNo<indexRecord->noOfColumns);
        const NdbRecord::Attr& indexAttr = indexRecord->columns[keyNo];
        assert(indexAttr.flags & NdbRecord::IsDistributionKey);
        assert(indexAttr.index_attrId<m_bound.lowKeys);
        const NdbQueryOperandImpl* key = m_bound.low[indexAttr.index_attrId];

        switch(key->getKind())
        {
          case NdbQueryOperandImpl::Linked:
          {
            appendedPattern |= QN_ScanIndexNode::SI_PRUNE_LINKED;
            const NdbLinkedOperandImpl& linkedOp = *static_cast<const NdbLinkedOperandImpl*>(key);
            const NdbQueryOperationDefImpl* parent = getParentOperation();
            uint32 levels = 0;
            while (parent != &linkedOp.getParentOperation())
            {
              if (parent->getType() == NdbQueryOperationDef::UniqueIndexAccess)  // Represented with two nodes in QueryTree
                levels+=2;
              else
                levels+=1;
              parent = parent->getParentOperation();
              assert(parent != NULL);
            }
            if (levels > 0)
            {
              serializedDef.append(QueryPattern::parent(levels));
            }
            serializedDef.append(QueryPattern::col(linkedOp.getLinkedColumnIx()));
            break;
          }
          case NdbQueryOperandImpl::Const:
          {
//          appendedPattern |= QN_ScanIndexNode::SI_PRUNE_CONST;
            const NdbConstOperandImpl& constOp = *static_cast<const NdbConstOperandImpl*>(key);
     
            // No of words needed for storing the constant data.
            const Uint32 wordCount =  AttributeHeader::getDataSize(constOp.getSizeInBytes());
            // Set type and length in words of key pattern field. 
            serializedDef.append(QueryPattern::data(wordCount));
            serializedDef.appendBytes(constOp.getAddr(),
                                      constOp.getSizeInBytes());
            break;
          }
          case NdbQueryOperandImpl::Param:
            appendedPattern |= QN_ScanIndexNode::SI_PRUNE_PARAMS;
            serializedDef.append(QueryPattern::param(paramCnt++));
            break;
          default:
            assert(false);
        }
      }

      // Set total length of bound pattern.
      Uint32 len = serializedDef.getSize() - startPos -1;
      serializedDef.put(startPos, (paramCnt << 16) | (len));
      appendedPattern |= QN_ScanIndexNode::SI_PRUNE_PATTERN;
    }
  }
  return appendedPattern;
} // NdbQueryIndexScanOperationDefImpl::appendPrunePattern


Uint32
NdbQueryIndexScanOperationDefImpl::appendBoundValue(
                                 Uint32Buffer& serializedDef,
                                 NdbIndexScanOperation::BoundType type,
                                 const NdbQueryOperandImpl* value,
                                 int& paramCnt) const
{
  Uint32 appendedPattern = 0;

  // Append BoundType as a constant value
  serializedDef.append(QueryPattern::data(1));
  serializedDef.append(type);

  switch(value->getKind())
  {
    case NdbQueryOperandImpl::Linked:
    {
      appendedPattern |= DABits::NI_KEY_LINKED;
      const NdbLinkedOperandImpl& linkedOp = *static_cast<const NdbLinkedOperandImpl*>(value);
      const NdbQueryOperationDefImpl* parent = getParentOperation();
      uint32 levels = 0;
      while (parent != &linkedOp.getParentOperation())
      {
        if (parent->getType() == NdbQueryOperationDef::UniqueIndexAccess)  // Represented with two nodes in QueryTree
          levels+=2;
        else
          levels+=1;
        parent = parent->getParentOperation();
        assert(parent != NULL);
      }
      if (levels > 0)
      {
        serializedDef.append(QueryPattern::parent(levels));
      }
//    serializedDef.append(QueryPattern::col(linkedOp.getLinkedColumnIx()));
      serializedDef.append(QueryPattern::attrInfo(linkedOp.getLinkedColumnIx())); // col w/ AttributeHeader
      break;
    }
    case NdbQueryOperandImpl::Const:
    {
      appendedPattern |= DABits::NI_KEY_CONSTS;
      const NdbConstOperandImpl& constOp = *static_cast<const NdbConstOperandImpl*>(value);
     
      // Build the AttributeHeader for const value
      // (AttributeId is later filled in by SPJ in Dbspj::scanIndex_fixupBound())
      AttributeHeader ah(0, constOp.getSizeInBytes());

      // Constant is then appended as AttributeHeader + const-value
      serializedDef.append(QueryPattern::data(1+ah.getDataSize()));
      serializedDef.append(ah.m_value);
      serializedDef.appendBytes(constOp.getAddr(),constOp.getSizeInBytes());
      break;
    }
    case NdbQueryOperandImpl::Param:
    {
      appendedPattern |= DABits::NI_KEY_PARAMS;
//    serializedDef.append(QueryPattern::param(paramCnt++));
      serializedDef.append(QueryPattern::paramHeader(paramCnt++));
      break;
    }
    default:
      assert(false);
  }

  return appendedPattern;
} // NdbQueryIndexScanOperationDefImpl::appendBoundValue


/**
 * Append the complete patterns for hi & low bound for an index range scan.
 * Each bound may consist of multiple values which in turn are
 * added with NdbQueryIndexScanOperationDefImpl::appendBoundPattern()
 */
Uint32
NdbQueryIndexScanOperationDefImpl::appendBoundPattern(Uint32Buffer& serializedDef) const
{
  Uint32 appendedPattern = 0;

  /**
   * Bound value for root operation is constructed when query is instantiated with
   * NdbQueryOperationImpl::prepareIndexKeyInfo()
   */
  if (getQueryOperationIx() == 0)
    return 0;

  if (m_bound.lowKeys>0 || m_bound.highKeys>0)
  {
    int paramCnt = 0;
    Uint32 startPos = serializedDef.getSize();
    serializedDef.append(0);     // Grab first word for length field, updated at end

    const uint key_count = 
       (m_bound.lowKeys >= m_bound.highKeys) ? m_bound.lowKeys : m_bound.highKeys;

    for (uint keyNo=0; keyNo<key_count; ++keyNo)
    {
      NdbIndexScanOperation::BoundType bound_type;

      /* If upper and lower limits are equal, a single BoundEQ is sufficient */
      if (keyNo < m_bound.lowKeys  &&
          keyNo < m_bound.highKeys &&
          m_bound.low[keyNo] == m_bound.high[keyNo])
      {
        /* Inclusive if defined, or matching rows can include this value */
        bound_type= NdbIndexScanOperation::BoundEQ;

        appendedPattern |=
          appendBoundValue(serializedDef, bound_type, m_bound.low[keyNo], paramCnt);

      } else {

        /* If key is part of lower bound */
        if (keyNo < m_bound.lowKeys)
        {
          /* Inclusive if defined, or matching rows can include this value */
          bound_type= m_bound.lowIncl || keyNo+1 < m_bound.lowKeys ?
              NdbIndexScanOperation::BoundLE : NdbIndexScanOperation::BoundLT;

          appendedPattern |=
            appendBoundValue(serializedDef, bound_type, m_bound.low[keyNo], paramCnt);
        }

        /* If key is part of upper bound */
        if (keyNo < m_bound.highKeys)
        {
          /* Inclusive if defined, or matching rows can include this value */
          bound_type= m_bound.highIncl || keyNo+1 < m_bound.highKeys ?
              NdbIndexScanOperation::BoundGE : NdbIndexScanOperation::BoundGT;

          appendedPattern |=
            appendBoundValue(serializedDef, bound_type, m_bound.high[keyNo], paramCnt);
        }
      }
    } // for 'all bound values'

    // Set total length of bound pattern.
    Uint32 len = serializedDef.getSize() - startPos -1;
    serializedDef.put(startPos, (paramCnt << 16) | (len));
  }

  return appendedPattern;
} // NdbQueryIndexScanOperationDefImpl::appendBoundPattern


int
NdbQueryPKLookupOperationDefImpl
::serializeOperation(Uint32Buffer& serializedDef)
{
  assert (m_keys[0]!=NULL);
  // This method should only be invoked once.
  assert (!m_isPrepared);
  m_isPrepared = true;

  // Reserve memory for LookupNode, fill in contents later when
  // 'length' and 'requestInfo' has been calculated.
  Uint32 startPos = serializedDef.getSize();
  serializedDef.alloc(QN_LookupNode::NodeSize);
  Uint32 requestInfo = 0;

  /**
   * NOTE: Order of sections within the optional part is fixed as:
   *    Part1:  'NI_HAS_PARENT'
   *    Part2:  'NI_KEY_PARAMS, NI_KEY_LINKED, NI_KEY_CONST'
   *    PART3:  'NI_LINKED_ATTR ++
   */

  // Optional part1: Make list of parent nodes.
  requestInfo |= appendParentList (serializedDef);

  // Part2: Append m_keys[] values specifying lookup key.
  requestInfo |= appendKeyPattern(serializedDef);

  // Part3: Columns required by SPJ to instantiate further child operations.
  requestInfo |= appendChildProjection(serializedDef);

  // Fill in LookupNode contents (Already allocated, 'startPos' is our handle:
  QN_LookupNode* node = reinterpret_cast<QN_LookupNode*>(serializedDef.addr(startPos));
  if (unlikely(node==NULL)) {
    return Err_MemoryAlloc;
  }
  node->tableId = getTable().getObjectId();
  node->tableVersion = getTable().getObjectVersion();
  node->requestInfo = requestInfo;
  const Uint32 length = serializedDef.getSize() - startPos;
  if (unlikely(length > 0xFFFF)) {
    return QRY_DEFINITION_TOO_LARGE; //Query definition too large.
  } else {
    QueryNode::setOpLen(node->len, QueryNode::QN_LOOKUP, length);
  }

#ifdef __TRACE_SERIALIZATION
  ndbout << "Serialized node " << getQueryOperationId() << " : ";
  for (Uint32 i = startPos; i < serializedDef.getSize(); i++) {
    char buf[12];
    sprintf(buf, "%.8x", serializedDef.get(i));
    ndbout << buf << " ";
  }
  ndbout << endl;
#endif

  return 0;
} // NdbQueryPKLookupOperationDefImpl::serializeOperation


int
NdbQueryIndexOperationDefImpl
::serializeOperation(Uint32Buffer& serializedDef)
{
  assert (m_keys[0]!=NULL);
  // This method should only be invoked once.
  assert (!m_isPrepared);
  m_isPrepared = true;

  /**
   * Serialize unique index as a seperate lookupNode
   */
  {
    // Reserve memory for Index LookupNode, fill in contents later when
    // 'length' and 'requestInfo' has been calculated.
    Uint32 startPos = serializedDef.getSize();
    serializedDef.alloc(QN_LookupNode::NodeSize);
    Uint32 requestInfo = QN_LookupNode::L_UNIQUE_INDEX;

    // Optional part1: Make list of parent nodes.
    assert (getQueryOperationId() > 0);
    requestInfo |= appendParentList (serializedDef);

    // Part2: m_keys[] are the keys to be used for index
    requestInfo |= appendKeyPattern(serializedDef);

    /* Basetable is executed as child operation of index:
     * Add projection of (only) NDB$PK column which is hidden *after* last index column.
     */
    {
      requestInfo |= DABits::NI_LINKED_ATTR;
      Uint16Sequence spjProjSeq(serializedDef,1);
      spjProjSeq.append(getIndex()->getNoOfColumns());
      spjProjSeq.finish();
    }

    // Fill in LookupNode contents (Already allocated, 'startPos' is our handle:
    QN_LookupNode* node = reinterpret_cast<QN_LookupNode*>(serializedDef.addr(startPos));
    if (unlikely(node==NULL)) {
      return Err_MemoryAlloc;
    }
    node->tableId = getIndex()->getObjectId();
    node->tableVersion = getIndex()->getObjectVersion();
    node->requestInfo = requestInfo;
    const Uint32 length = serializedDef.getSize() - startPos;
    if (unlikely(length > 0xFFFF)) {
      return QRY_DEFINITION_TOO_LARGE; //Query definition too large.
    } else {
      QueryNode::setOpLen(node->len, QueryNode::QN_LOOKUP, length);
    }

#ifdef __TRACE_SERIALIZATION
    ndbout << "Serialized index " << getQueryOperationId()-1 << " : ";
    for (Uint32 i = startPos; i < serializedDef.getSize(); i++){
      char buf[12];
      sprintf(buf, "%.8x", serializedDef.get(i));
      ndbout << buf << " ";
    }
    ndbout << endl;
#endif
  } // End: Serialize unique index access

  // Reserve memory for LookupNode, fill in contents later when
  // 'length' and 'requestInfo' has been calculated.
  Uint32 startPos = serializedDef.getSize();
  serializedDef.alloc(QN_LookupNode::NodeSize);
  Uint32 requestInfo = 0;

  /**
   * NOTE: Order of sections within the optional part is fixed as:
   *    Part1:  'NI_HAS_PARENT'
   *    Part2:  'NI_KEY_PARAMS, NI_KEY_LINKED, NI_KEY_CONST'
   *    PART3:  'NI_LINKED_ATTR ++
   */

  // Optional part1: Append index as (single) parent op..
  { requestInfo |= DABits::NI_HAS_PARENT;
    Uint16Sequence parentSeq(serializedDef,1);
    parentSeq.append(getQueryOperationId()-1);
    parentSeq.finish();
  }

  // Part2: Append projected NDB$PK column as index -> table linkage
  {
    requestInfo |= DABits::NI_KEY_LINKED;
    serializedDef.append(1U); // Length: Key pattern contains only the single PK column
    serializedDef.append(QueryPattern::colPk(0));
  }

  // Part3: Columns required by SPJ to instantiate descendant child operations.
  requestInfo |= appendChildProjection(serializedDef);

  // Fill in LookupNode contents (Already allocated, 'startPos' is our handle:
  QN_LookupNode* node = reinterpret_cast<QN_LookupNode*>(serializedDef.addr(startPos)); 
  if (unlikely(node==NULL)) {
    return Err_MemoryAlloc;
  }
  node->tableId = getTable().getObjectId();
  node->tableVersion = getTable().getObjectVersion();
  node->requestInfo = requestInfo;
  const Uint32 length = serializedDef.getSize() - startPos;
  if (unlikely(length > 0xFFFF)) {
    return QRY_DEFINITION_TOO_LARGE; //Query definition too large.
  } else {
    QueryNode::setOpLen(node->len, QueryNode::QN_LOOKUP, length);
  }

#ifdef __TRACE_SERIALIZATION
  ndbout << "Serialized node " << getQueryOperationId() << " : ";
  for (Uint32 i = startPos; i < serializedDef.getSize(); i++) {
    char buf[12];
    sprintf(buf, "%.8x", serializedDef.get(i));
    ndbout << buf << " ";
  }
  ndbout << endl;
#endif

  return 0;
} // NdbQueryIndexOperationDefImpl::serializeOperation


NdbQueryScanOperationDefImpl::NdbQueryScanOperationDefImpl (
                           const NdbTableImpl& table,
                           const NdbQueryOptionsImpl& options,
                           const char* ident,
                           Uint32      ix,
                           int& error)
  : NdbQueryOperationDefImpl(table,options,ident,ix,error)
{}

int
NdbQueryScanOperationDefImpl::serialize(Uint32Buffer& serializedDef,
                                        const NdbTableImpl& tableOrIndex)
{
  bool isRoot = (getQueryOperationIx()==0);

  // This method should only be invoked once.
  assert (!m_isPrepared);
  m_isPrepared = true;
  // Reserve memory for ScanFragNode, fill in contents later when
  // 'length' and 'requestInfo' has been calculated.
  Uint32 startPos = serializedDef.getSize();
  assert (QN_ScanFragNode::NodeSize==QN_ScanIndexNode::NodeSize);
  serializedDef.alloc(QN_ScanFragNode::NodeSize);
  Uint32 requestInfo = 0;

  // Optional part1: Make list of parent nodes.
  requestInfo |= appendParentList (serializedDef);

  // Part2: Append pattern for building upper/lower bounds.
  requestInfo |= appendBoundPattern(serializedDef);

  // Part3: Columns required by SPJ to instantiate descendant child operations.
  requestInfo |= appendChildProjection(serializedDef);

  // Part4: Pattern to creating a prune key for range scan
  requestInfo |= appendPrunePattern(serializedDef);

  const Uint32 length = serializedDef.getSize() - startPos;
  if (unlikely(length > 0xFFFF))
  {
    return QRY_DEFINITION_TOO_LARGE; //Query definition too large.
  }
  // Fill in ScanFragNode/ScanIndexNode contents (Already allocated, 'startPos' is our handle:
  if (isRoot)
  {
    QN_ScanFragNode* node = reinterpret_cast<QN_ScanFragNode*>(serializedDef.addr(startPos)); 
    if (unlikely(node==NULL)) {
      return Err_MemoryAlloc;
    }
    node->tableId = tableOrIndex.getObjectId();
    node->tableVersion = tableOrIndex.getObjectVersion();
    node->requestInfo = requestInfo;
    QueryNode::setOpLen(node->len, QueryNode::QN_SCAN_FRAG, length);
  }
  else 
  {
    QN_ScanIndexNode* node = reinterpret_cast<QN_ScanIndexNode*>(serializedDef.addr(startPos)); 
    if (unlikely(node==NULL)) {
      return Err_MemoryAlloc;
    }
    node->tableId = tableOrIndex.getObjectId();
    node->tableVersion = tableOrIndex.getObjectVersion();
    // Need NI_REPEAT_SCAN_RESULT if there are star-joined scans 
    node->requestInfo = requestInfo | DABits::NI_REPEAT_SCAN_RESULT;
    QueryNode::setOpLen(node->len, QueryNode::QN_SCAN_INDEX, length);
  }

#ifdef __TRACE_SERIALIZATION
  ndbout << "Serialized node " << getQueryOperationId() << " : ";
  for(Uint32 i = startPos; i < serializedDef.getSize(); i++){
    char buf[12];
    sprintf(buf, "%.8x", serializedDef.get(i));
    ndbout << buf << " ";
  }
  ndbout << endl;
#endif
  return 0;
} // NdbQueryScanOperationDefImpl::serialize


int
NdbQueryTableScanOperationDefImpl
::serializeOperation(Uint32Buffer& serializedDef)
{
  return NdbQueryScanOperationDefImpl::serialize(serializedDef, getTable());
} // NdbQueryTableScanOperationDefImpl::serializeOperation


int
NdbQueryIndexScanOperationDefImpl
::serializeOperation(Uint32Buffer& serializedDef)
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

  NdbQueryBuilder* const myBuilder= NdbQueryBuilder::create();

  const NdbDictionary::Table *manager = (NdbDictionary::Table*)0xDEADBEAF;
//  const NdbDictionary::Index *ix = (NdbDictionary::Index*)0x11223344;

  const NdbQueryDef* q1 = 0;
  {
    NdbQueryBuilder* qb = myBuilder;

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
