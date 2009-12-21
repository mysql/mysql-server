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
#include <ndb_global.h>
#include <Vector.hpp>
#include "signaldata/QueryTree.hpp"

#include "Ndb.hpp"
#include "NdbDictionary.hpp"
#include "NdbDictionaryImpl.hpp"
#include "NdbRecord.hpp"
#include "AttributeHeader.hpp"
#include "NdbIndexScanOperation.hpp"
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

/* Various error codes that are not specific to NdbQuery. */
STATIC_CONST(Err_MemoryAlloc = 4000);
STATIC_CONST(Err_NotKeyRecord = 4292);

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

  const double m_value;
};

class NdbCharConstOperandImpl : public NdbConstOperandImpl
{
public:
  explicit NdbCharConstOperandImpl (const char* value) : 
    NdbConstOperandImpl(), m_value(value) {}

  int convertChar();
  int convertVChar();
//int convertLVChar();

  const char* const m_value;
};

class NdbGenericConstOperandImpl : public NdbConstOperandImpl
{
public:
  explicit NdbGenericConstOperandImpl (const void* value)
  : NdbConstOperandImpl(),
    m_value(value)
  {}

private:
  int convert2ColumnType();

  const void* const m_value;
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
  virtual int serializeOperation(Uint32Buffer& serializedDef);

  virtual const NdbQueryOperandImpl* const* getKeyOperands() const
  { return m_keys; }

  virtual const NdbRecord* getKeyRecord() const
  { return &m_key_record; }

protected:
  virtual ~NdbQueryLookupOperationDefImpl() {}

  explicit NdbQueryLookupOperationDefImpl (
                           const NdbRecord& key_record,          // Key NdbRecord, PK or unique index
                           const NdbRecord& tbl_record,
                           const NdbQueryOperand* const keys[],
                           const char* ident,
                           Uint32      ix);

  virtual const NdbQueryLookupOperationDef& getInterface() const
  { return m_interface; }

  virtual Type getType() const
  { return PrimaryKeyAccess; }

  virtual bool isScanOperation() const
  { return false; }

protected:
  NdbQueryLookupOperationDef m_interface;
  NdbQueryOperandImpl* m_keys[MAX_ATTRIBUTES_IN_INDEX+1];

  const NdbRecord& m_key_record;

}; // class NdbQueryLookupOperationDefImpl


class NdbQueryIndexOperationDefImpl : public NdbQueryLookupOperationDefImpl
{
  friend class NdbQueryBuilder;  // Allow privat access from builder interface

public:
  virtual int serializeOperation(Uint32Buffer& serializedDef);

private:
  virtual ~NdbQueryIndexOperationDefImpl() {}
  explicit NdbQueryIndexOperationDefImpl (
                           const NdbRecord& key_record,          // Unique index
                           const NdbRecord& tbl_record,
                           const NdbQueryOperand* const keys[],
                           const char* ident,
                           Uint32      ix)
  : NdbQueryLookupOperationDefImpl(key_record,tbl_record,keys,ident,ix)
  {}

  virtual Type getType() const
  { return UniqueIndexAccess; }

}; // class NdbQueryIndexOperationDefImpl


class NdbQueryTableScanOperationDefImpl : public NdbQueryScanOperationDefImpl
{
  friend class NdbQueryBuilder;  // Allow privat access from builder interface

public:
  virtual int serializeOperation(Uint32Buffer& serializedDef);

  virtual const NdbQueryTableScanOperationDef& getInterface() const
  { return m_interface; }

  virtual Type getType() const
  { return TableScan; }

private:
  virtual ~NdbQueryTableScanOperationDefImpl() {}
  explicit NdbQueryTableScanOperationDefImpl (
                           const NdbRecord& tbl_record,
                           const char* ident,
                           Uint32      ix)
    : NdbQueryScanOperationDefImpl(tbl_record,ident,ix),
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

NdbQueryDefImpl& 
NdbQueryDef::getImpl() const{
  return m_impl;
}

void 
NdbQueryDef::release() const
{
  delete &getImpl();
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
  return ::getImpl(*this).getTblRecord().table;
}

const NdbDictionary::Index*
NdbQueryLookupOperationDef::getIndex() const
{
  const NdbRecord* inxRecord = ::getImpl(*this).getKeyRecord();
  if (inxRecord)
    return inxRecord->table->m_index;
  else
    return NULL;
}

int 
NdbQueryIndexScanOperationDef::setOrdering(NdbScanOrdering ordering)
{
  return ::getImpl(*this).setOrdering(ordering);
}

NdbScanOrdering
NdbQueryIndexScanOperationDef::getOrdering() const
{
  return ::getImpl(*this).getOrdering();
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
  returnErrIf(constOp==0,Err_MemoryAlloc);

  m_pimpl->m_operands.push_back(constOp);
  return &constOp->m_interface;
}
NdbConstOperand* 
NdbQueryBuilder::constValue(const void* value)
{
  returnErrIf(value==0,QRY_REQ_ARG_IS_NULL);
  NdbConstOperandImpl* constOp = new NdbGenericConstOperandImpl(value);
  returnErrIf(constOp==0,Err_MemoryAlloc);

  m_pimpl->m_operands.push_back(constOp);
  return &constOp->m_interface;
}
NdbConstOperand* 
NdbQueryBuilder::constValue(Int32 value)
{
  NdbConstOperandImpl* constOp = new NdbInt64ConstOperandImpl(value);
  returnErrIf(constOp==0,Err_MemoryAlloc);

  m_pimpl->m_operands.push_back(constOp);
  return &constOp->m_interface;
}
NdbConstOperand* 
NdbQueryBuilder::constValue(Uint32 value)
{
  NdbConstOperandImpl* constOp = new NdbInt64ConstOperandImpl(value);
  returnErrIf(constOp==0,Err_MemoryAlloc);

  m_pimpl->m_operands.push_back(constOp);
  return &constOp->m_interface;
}
NdbConstOperand* 
NdbQueryBuilder::constValue(Int64 value)
{
  NdbConstOperandImpl* constOp = new NdbInt64ConstOperandImpl(value);
  returnErrIf(constOp==0,Err_MemoryAlloc);

  m_pimpl->m_operands.push_back(constOp);
  return &constOp->m_interface;
}
NdbConstOperand* 
NdbQueryBuilder::constValue(Uint64 value)
{
  NdbConstOperandImpl* constOp = new NdbInt64ConstOperandImpl(value);
  returnErrIf(constOp==0,Err_MemoryAlloc);

  m_pimpl->m_operands.push_back(constOp);
  return &constOp->m_interface;
}
NdbConstOperand* 
NdbQueryBuilder::constValue(double value)
{
  NdbConstOperandImpl* constOp = new NdbDoubleConstOperandImpl(value);
  returnErrIf(constOp==0,Err_MemoryAlloc);

  m_pimpl->m_operands.push_back(constOp);
  return &constOp->m_interface;
}
NdbParamOperand* 
NdbQueryBuilder::paramValue(const char* name)
{
  NdbParamOperandImpl* paramOp = new NdbParamOperandImpl(name,getImpl().m_paramCnt++);
  returnErrIf(paramOp==0,Err_MemoryAlloc);

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
  returnErrIf(linkedOp==0, Err_MemoryAlloc);

  m_pimpl->m_operands.push_back(linkedOp);
  return &linkedOp->m_interface;
}


NdbQueryLookupOperationDef*
NdbQueryBuilder::readTuple(const NdbDictionary::Table* table,    // Primary key lookup
                           const NdbQueryOperand* const keys[],  // Terminated by NULL element 
                           const char* ident)
{
  return readTuple(table->getDefaultRecord(),
                   table->getDefaultRecord(),
                   keys, ident);
}


NdbQueryLookupOperationDef*
NdbQueryBuilder::readTuple(const NdbDictionary::Index* index,    // Unique key lookup w/ index
                           const NdbDictionary::Table* table,    // Primary key lookup
                           const NdbQueryOperand* const keys[],  // Terminated by NULL element 
                           const char* ident)
{
  return readTuple(index->getDefaultRecord(),
                   table->getDefaultRecord(),
                   keys, ident);
}


NdbQueryTableScanOperationDef*
NdbQueryBuilder::scanTable(const NdbDictionary::Table* table,
                           const char* ident)
{
  return scanTable(table->getDefaultRecord(), ident);
}


NdbQueryIndexScanOperationDef*
NdbQueryBuilder::scanIndex(const NdbDictionary::Index* index, 
	                   const NdbDictionary::Table* table,
                           const NdbQueryIndexBound* bound,
                           const char* ident)
{
  return scanIndex(index->getDefaultRecord(),
                   table->getDefaultRecord(),
                   bound, ident);
}



NdbQueryLookupOperationDef*
NdbQueryBuilder::readTuple(const NdbRecord* key_record,          // key NdbRecord, PK or unique index
                           const NdbRecord* tbl_record,
                           const NdbQueryOperand* const keys[],  // Terminated by NULL element 
                           const char* ident)
{
  NdbQueryLookupOperationDefImpl* op = NULL;

  if (unlikely(m_pimpl->hasError()))
    return NULL;
  returnErrIf(key_record==0 || tbl_record==0 || keys==0, QRY_REQ_ARG_IS_NULL);
  returnErrIf(!(key_record->flags & NdbRecord::RecHasAllKeys), Err_NotKeyRecord);

  if (key_record->flags & NdbRecord::RecIsIndex)
  {
    const NdbIndexImpl* indexImpl = key_record->table->m_index;

    returnErrIf(indexImpl->m_table_id  != 
                (Uint32)(tbl_record->tableId) ||
                table_version_major(indexImpl->m_table_version) != 
                table_version_major(tbl_record->tableVersion), 
                QRY_UNRELATED_INDEX);

    // Only 'UNIQUE' indexes may be used for lookup operations:
    returnErrIf(indexImpl->getType()!=NdbDictionary::Index::UniqueHashIndex,
                QRY_WRONG_INDEX_TYPE);

    op = new NdbQueryIndexOperationDefImpl(*key_record, *tbl_record,
                                           keys,ident,
                                           m_pimpl->m_operations.size());
  }
  else
  {
    returnErrIf(key_record->tableId != tbl_record->tableId,
                QRY_WRONG_INDEX_TYPE);

    op = new NdbQueryLookupOperationDefImpl(*key_record, *tbl_record,
                                            keys,ident,
                                            m_pimpl->m_operations.size());
  }
  returnErrIf(op==0, Err_MemoryAlloc);
  m_pimpl->m_operations.push_back(op);

  int keyfields = key_record->key_index_length;
  for (int i=0; i<keyfields; ++i)
  {
    // Check: keys[] are specified for all fields in 'key_record'
    returnErrIf(op->m_keys[i]==NULL, QRY_TOO_FEW_KEY_VALUES);

    const NdbRecord::Attr& attr = key_record->columns[key_record->key_indexes[i]];
    assert (attr.flags & NdbRecord::IsKey);

    const int error = op->m_keys[i]->bindOperand(attr,*op);
    returnErrIf(error!=0, error);
  }
  // Check for propper NULL termination of keys[] spec
  returnErrIf(keys[keyfields]!=NULL, QRY_TOO_MANY_KEY_VALUES);

  return &op->m_interface;
}


NdbQueryTableScanOperationDef*
NdbQueryBuilder::scanTable(const NdbRecord* tbl_record,
                           const char* ident)
{
  if (m_pimpl->hasError())
    return NULL;
  returnErrIf(tbl_record==0, QRY_REQ_ARG_IS_NULL);  // Required non-NULL arguments

  NdbQueryTableScanOperationDefImpl* op =
    new NdbQueryTableScanOperationDefImpl(*tbl_record,ident,
                                          m_pimpl->m_operations.size());
  returnErrIf(op==0, Err_MemoryAlloc);

  m_pimpl->m_operations.push_back(op);
  return &op->m_interface;
}


NdbQueryIndexScanOperationDef*
NdbQueryBuilder::scanIndex(const NdbRecord* inx_record, 
                           const NdbRecord* tbl_record,
                           const NdbQueryIndexBound* bound,
                           const char* ident)
{
  if (m_pimpl->hasError())
    return NULL;
  // Required non-NULL arguments
  returnErrIf(inx_record==0 || inx_record==0, QRY_REQ_ARG_IS_NULL);

  const NdbIndexImpl* indexImpl = inx_record->table->m_index;

  returnErrIf(indexImpl->m_table_id  != 
              (Uint32)(tbl_record->tableId) ||
              table_version_major(indexImpl->m_table_version) != 
              table_version_major(tbl_record->tableVersion), 
              QRY_UNRELATED_INDEX);

  // Only ordered indexes may be used in scan operations:
  returnErrIf(indexImpl->getType()!=NdbDictionary::Index::OrderedIndex,
              QRY_WRONG_INDEX_TYPE);

  NdbQueryIndexScanOperationDefImpl* op =
    new NdbQueryIndexScanOperationDefImpl(*inx_record, *tbl_record,
                                          bound, ident,
                                          m_pimpl->m_operations.size());
  returnErrIf(op==0, Err_MemoryAlloc);

  if (unlikely(op->m_bound.lowKeys  > inx_record->key_index_length ||
               op->m_bound.highKeys > inx_record->key_index_length))
  { m_pimpl->setErrorCode(QRY_TOO_MANY_KEY_VALUES);
    delete op;
    return NULL;
  }

  Uint32 i;
  for (i=0; i<op->m_bound.lowKeys; ++i)
  {
    const NdbRecord::Attr& attr = inx_record->columns[inx_record->key_indexes[i]];

    assert (op->m_bound.low[i]);
    int error = op->m_bound.low[i]->bindOperand(attr,*op);
    if (unlikely(error))
    { m_pimpl->setErrorCode(error);
      delete op;
      return NULL;
    }
  }
  if (!op->m_bound.eqBound)
  {
    for (i=0; i<op->m_bound.highKeys; ++i)
    {
      const NdbRecord::Attr& attr = inx_record->columns[inx_record->key_indexes[i]];

      assert (op->m_bound.high[i]);
      int error = op->m_bound.high[i]->bindOperand(attr,*op);
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

  return def;
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

////////////////////////////////////////////////////////////////
// The (hidden) Impl of NdbQueryOperand (w/ various subclasses)
////////////////////////////////////////////////////////////////

/* Implicit typeconversion between related datatypes: */
int NdbInt64ConstOperandImpl::convertUint8()
{
  if (unlikely(m_value < 0 || m_value > 0xFF))
    return QRY_NUM_OPERAND_RANGE;
  m_converted.val.uint8 = (Uint8)m_value;
  m_converted.len = sizeof(m_converted.val.uint8);
  return 0;
}
int NdbInt64ConstOperandImpl::convertInt8()
{
  if (unlikely(m_value < -0x80L || m_value > 0x7F))
    return QRY_NUM_OPERAND_RANGE;
  m_converted.val.int8 = (Int8)m_value;
  m_converted.len = sizeof(m_converted.val.int8);
  return 0;
}
int NdbInt64ConstOperandImpl::convertUint16()
{
  if (unlikely(m_value < 0 || m_value > 0xFFFF))
    return QRY_NUM_OPERAND_RANGE;
  m_converted.val.uint16 = (Uint16)m_value;
  m_converted.len = sizeof(m_converted.val.uint16);
  return 0;
}
int NdbInt64ConstOperandImpl::convertInt16()
{
  if (unlikely(m_value < -0x8000L || m_value > 0x7FFF))
    return QRY_NUM_OPERAND_RANGE;
  m_converted.val.int16 = (Int16)m_value;
  m_converted.len = sizeof(m_converted.val.int16);
  return 0;
}
int NdbInt64ConstOperandImpl::convertUint24()
{
  if (unlikely(m_value < 0 || m_value > 0xFFFFFF))
    return QRY_NUM_OPERAND_RANGE;
  m_converted.val.uint32 = (Uint32)m_value;
  m_converted.len = sizeof(m_converted.val.uint32);
  return 0;
}
int NdbInt64ConstOperandImpl::convertInt24()
{
  if (unlikely(m_value < -0x800000L || m_value > 0x7FFFFF))
    return QRY_NUM_OPERAND_RANGE;
  m_converted.val.int32 = (Int32)m_value;
  m_converted.len = sizeof(m_converted.val.int32);
  return 0;
}
int NdbInt64ConstOperandImpl::convertUint32()
{
  if (unlikely(m_value < 0 || m_value > 0xFFFFFFFF))
    return QRY_NUM_OPERAND_RANGE;
  m_converted.val.uint32 = (Uint32)m_value;
  m_converted.len = sizeof(m_converted.val.uint32);
  return 0;
}
int NdbInt64ConstOperandImpl::convertInt32()
{
  if (unlikely(m_value < -((Int64)0x80000000L) || m_value > 0x7FFFFFFF))
    return QRY_NUM_OPERAND_RANGE;
  m_converted.val.int32 = (Int32)m_value;
  m_converted.len = sizeof(m_converted.val.int32);
  return 0;
}
int NdbInt64ConstOperandImpl::convertInt64()
{
  m_converted.val.int64 = m_value;
  m_converted.len = sizeof(m_converted.val.int64);
  return 0;
}
int NdbInt64ConstOperandImpl::convertUint64()
{
  m_converted.val.uint64 = (Uint64)m_value;
  m_converted.len = sizeof(m_converted.val.uint64);
  return 0;
}

int NdbDoubleConstOperandImpl::convertFloat()
{
  m_converted.val.flt = (float)m_value;
  m_converted.len = sizeof(m_converted.val.flt);
  return 0;
}
int NdbDoubleConstOperandImpl::convertDouble()
{
  m_converted.val.dbl = m_value;
  m_converted.len = sizeof(m_converted.val.dbl);
  return 0;
}

int NdbCharConstOperandImpl::convertChar()
{
  size_t len = m_column->getLength();
  size_t srclen = (m_value) ? strlen(m_value) : 0;
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
  size_t maxlen = m_column->getLength();
  size_t len = (m_value) ? strlen(m_value) : 0;
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
  size_t len;
  const unsigned char* const src= (unsigned char*)m_value;

  if (m_attr->flags & NdbRecord::IsMysqldShrinkVarchar)
  {
    assert (m_attr->flags & NdbRecord::IsVar1ByteLen);
    assert (m_attr->maxSize-1 <= 0xFF); // maxSize incl length in first byte
    len = uint2korr(src);

    if (unlikely(len > m_attr->maxSize-1))
      return QRY_CHAR_OPERAND_TRUNCATED;

    char* dst = m_converted.getCharBuffer(len+1);
    if (unlikely(dst==NULL))
      return Err_MemoryAlloc;

    memcpy (dst+1, src+2, len);
    *(uchar*)dst = len;
  }
  else
  {
    if (m_attr->flags & NdbRecord::IsVar1ByteLen)
    {
      len = 1 + *src;
      if (unlikely(len > m_attr->maxSize-1)) {
        return QRY_CHAR_OPERAND_TRUNCATED;
      }
    }
    else if (m_attr->flags & NdbRecord::IsVar2ByteLen)
    {
      len = 2 + uint2korr(src);
      if (unlikely(len > m_attr->maxSize-2)) {
        return QRY_CHAR_OPERAND_TRUNCATED;
      }
    }
    else
    {
      len = m_attr->maxSize;
    }

    if (unlikely(m_attr->maxSize != (size_t)m_column->getSizeInBytes())) {
      return QRY_OPERAND_HAS_WRONG_TYPE;
    }

    void* dst = m_converted.getCharBuffer(len);
    if (unlikely(dst==NULL))
      return Err_MemoryAlloc;

    memcpy (dst, m_value, len);
  }

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
NdbQueryOperandImpl::bindOperand(
                           const NdbRecord::Attr& attr,
                           NdbQueryOperationDefImpl& operation)
{
  if (m_attr  && m_attr != &attr)
    // Already bounded to a different ::Attr
    return QRY_OPERAND_ALREADY_BOUND;

  m_attr   = &attr;
  m_column = operation.getTblRecord().table->getColumn(attr.column_no);
  return 0;
}

int
NdbConstOperandImpl::bindOperand(
                           const NdbRecord::Attr& attr,
                           NdbQueryOperationDefImpl& operation)
{
  const int error = NdbQueryOperandImpl::bindOperand(attr,operation);
  if (unlikely(error))
    return error;

  return convert2ColumnType();
}


int
NdbLinkedOperandImpl::bindOperand(
                           const NdbRecord::Attr& attr,
                           NdbQueryOperationDefImpl& operation)
{
  const int error = NdbQueryOperandImpl::bindOperand(attr,operation);
  if (unlikely(error))
    return error;

  const NdbColumnImpl& parentColumn = getParentColumn();

  if (unlikely(m_column->m_type      != parentColumn.m_type ||
               m_column->m_precision != parentColumn.m_precision ||
               m_column->m_scale     != parentColumn.m_scale ||
               m_column->m_length    != parentColumn.m_length ||
               m_column->m_cs        != parentColumn.m_cs))
    return QRY_OPERAND_HAS_WRONG_TYPE;  // Incompatible datatypes

  if (unlikely(m_column->m_type == NdbDictionary::Column::Blob ||
               m_column->m_type == NdbDictionary::Column::Text))
    return QRY_OPERAND_HAS_WRONG_TYPE;  // BLOB/CLOB datatypes intentionally not supported

  // TODO: Allow and autoconvert compatible datatypes

  // Register parent/child operation relations
  return operation.linkWithParent(&this->m_parentOperation);
}


int
NdbParamOperandImpl::bindOperand(
                           const NdbRecord::Attr& attr,
                           NdbQueryOperationDefImpl& operation)
{
  const int error = NdbQueryOperandImpl::bindOperand(attr,operation);
  if (unlikely(error))
    return error;

  if (unlikely(m_column->m_type == NdbDictionary::Column::Blob ||
               m_column->m_type == NdbDictionary::Column::Text))
    return QRY_OPERAND_HAS_WRONG_TYPE;  // BLOB/CLOB datatypes intentionally not supported

  operation.addParamRef(this);
  return 0;
}


/////////////////////////////////////////////////////////////////
// The (hidden) Impl of NdbQueryOperation (w/ various subclasses)
///////////////////////>/////////////////////////////////////////

NdbQueryLookupOperationDefImpl::NdbQueryLookupOperationDefImpl (
                           const NdbRecord& key_record,          // Key NdbRecord, PK or unique index
                           const NdbRecord& tbl_record,
                           const NdbQueryOperand* const keys[],
                           const char* ident,
                           Uint32      ix)
 : NdbQueryOperationDefImpl(tbl_record,ident,ix),
   m_interface(*this),
   m_key_record(key_record)
{
  int i;
  for (i=0; i<MAX_ATTRIBUTES_IN_INDEX; ++i)
  { if (keys[i] == NULL)
      break;
    m_keys[i] = &keys[i]->getImpl();
  }
  assert (keys[i] == NULL);
  m_keys[i] = NULL;
}


NdbQueryIndexScanOperationDefImpl::NdbQueryIndexScanOperationDefImpl (
                           const NdbRecord& inx_record, 
                           const NdbRecord& tbl_record,
                           const NdbQueryIndexBound* bound,
                           const char* ident,
                           Uint32      ix)
: NdbQueryScanOperationDefImpl(tbl_record,ident,ix),
  m_interface(*this), 
  m_inx_record(inx_record), 
  m_bound(),
  m_ordering(NdbScanOrdering_void)
{
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
    m_bound.eqBound = (bound->m_low==bound->m_high && bound->m_low!=NULL);
  }
  else {
    m_bound.lowKeys = m_bound.highKeys = 0;
    m_bound.lowIncl = m_bound.highIncl = true;
    m_bound.eqBound = false;
  }
}

/***
const NdbIndexImpl*
NdbQueryIndexScanOperationDefImpl::getIndex() const
{ return m_inx_record.table->m_index; }
****/

int
NdbQueryIndexScanOperationDefImpl::checkPrunable(
                              const Uint32Buffer& keyInfo,
                              bool&   isPruned,
                              Uint32& hashValue) const  // 'hashValue' only defined if 'isPruned'
{ 
  /**
   * Determine if scan may be pruned to a single partition:
   */
  const Uint32 index_distkeys = m_inx_record.m_no_of_distribution_keys;
  const Uint32 distkey_min    = m_inx_record.m_min_distkey_prefix_length;
  const Uint32 table_distkeys = m_tbl_record.m_no_of_distribution_keys;

  bool isPrunable = (                             // Initial prunable propert:
            index_distkeys == table_distkeys &&   // Index has all base table d-keys
            m_bound.lowKeys >= distkey_min &&     // Low bounds have all d-keys
            m_bound.highKeys >= distkey_min);     // High bounds have all d-keys

  isPruned = false;
  if (isPrunable)
  {
    Ndb::Key_part_ptr lowKey[NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY];
    Ndb::Key_part_ptr highKey;

#if !defined(NDEBUG)
    for (unsigned keyNo = 0; keyNo < NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY; keyNo++)
    {
      lowKey[keyNo].len  = 0;
      lowKey[keyNo].ptr  = NULL;
    }
#endif

    // Aggregate prunable propert:
    // All hi/low keys values within 'distkey_min' must be equal
    int keyPos = 0;
    int keyEnd = keyInfo.get(keyPos) >> 16;

    for (unsigned i = 0; i < distkey_min; i++)
    {
      /* Bounds have been serialized in key_indexs[] order,
       * Retreive values directly from serialized bounds
       * and temp. store lowKey[] in columns[] order:
       */
      Uint32 keyIx  = m_inx_record.key_indexes[i];

      Uint32 type         = keyInfo.get(keyPos) & 0xFFFF;
      AttributeHeader ah1 = keyInfo.get(keyPos+1);
      lowKey[keyIx].len   = ah1.getByteSize();
      lowKey[keyIx].ptr   = keyInfo.addr(keyPos+2);

      keyPos += 1+1+ah1.getDataSize();  // Skip data read above.

      // Only has to compare values if not known to be 'BoundEQ'
      if (type != NdbIndexScanOperation::BoundEQ)
      {
        assert ((keyInfo.get(keyPos) & 0xFFFF) != NdbIndexScanOperation::BoundEQ);
        AttributeHeader ah2 = keyInfo.get(keyPos+1);
        highKey.len = ah2.getByteSize();
        highKey.ptr = keyInfo.addr(keyPos+2);

        keyPos += 1+1+ah2.getDataSize();  // Skip data read above.

        // Compare high and low bound value:
        const NdbRecord::Attr& recAttr = m_inx_record.columns[keyIx];
        const int res=
          (*recAttr.compare_function)(recAttr.charset_info,
                                       lowKey[keyIx].ptr, lowKey[keyIx].len,
                                       highKey.ptr, highKey.len, true);
        if (res!=0) {  // Not equal
          assert(res != NdbSqlUtil::CmpUnknown);
          isPrunable = false;
          return 0;
        }
      } // != BoundEQ
    } // for()

    // Scan is now known to be prunable, calculate hashValue
    assert (isPrunable);

    // FIXME: Don't handle multiple bounds yet, assumed non-prunable
    if (keyPos < keyEnd)
      return 0;

    isPruned = true;
    Ndb::Key_part_ptr distKey[NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY+1];

    // hi/low is equal and known to be a prunable bound.
    // Use prev. retrieved bounds in distkey_indexes[] order 
    // and build the distKey for hashValue calculation.
    for (unsigned i = 0; i<m_inx_record.distkey_index_length; i++)
    {
      Uint32 keyIx  = m_inx_record.distkey_indexes[i];
      distKey[i] = lowKey[keyIx];
    }
    distKey[m_inx_record.distkey_index_length].ptr = NULL;

    int error = Ndb::computeHash(&hashValue, m_tbl_record.table, distKey, NULL, 0);
    if (unlikely(error))
      return error;
  } // if 'isPrunable'

  return 0;

} // NdbQueryIndexScanOperationDefImpl::checkPrunable

int
NdbQueryIndexScanOperationDefImpl::setOrdering(NdbScanOrdering ordering)
{
  /* Check that query has not been prepared yet.*/
  if(m_isPrepared)
  {
    return -1;
  }
  m_ordering = ordering;
  return 0;
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

void
NdbQueryOperationDefImpl::removeChild(const NdbQueryOperationDefImpl* childOp)
{
  for (unsigned i=0; i<m_children.size(); ++i)
  { if (m_children[i] == childOp)
      m_children.erase(i);
  }
}

bool
NdbQueryOperationDefImpl::isChildOf(const NdbQueryOperationDefImpl* parentOp) const
{
  for (Uint32 i=0; i<m_parents.size(); ++i)
  { if (m_parents[i] == parentOp || m_parents[i]->isChildOf(parentOp))
      return true;
  }
  return false;
}

int
NdbQueryOperationDefImpl::linkWithParent(NdbQueryOperationDefImpl* parentOp)
{
  for (Uint32 i=0; i<m_parents.size(); ++i)
  { if (m_parents[i] == parentOp)
    { // Assert that parent already refer 'this' as a child.
      for (Uint32 j=0; j<parentOp->getNoOfChildOperations(); j++)
      { if (&parentOp->getChildOperation(j) == this)
          return 0;
      }
      assert(false);
      return 0;
    }
  }

  assert (m_parents.size() <= 1);
  if (unlikely(m_parents.size() == 1))
  {
    /**
     * Parent merging current disabled 
     *  - Will also need additional support in SPJ block
     */
    return QRY_MULTIPLE_PARENTS;

    /**
     * Multiple parents not allowed.
     * Resolve this by finding the closest related (grand)parent among 
     * the two parents. This is calculated as the parent having the 
     * other parent as grand parent.
     */
    if (m_parents[0]->isChildOf(parentOp))
    { // parentOp is grandparent of m_parent[0], don't interested in it
      return 0; // Accept existing m_parent[0] as my parent.
    }
    else if (parentOp->isChildOf(m_parents[0]))
    { // Remove existing grandparent linkage being replaced by .
      parentOp->removeChild(this);
      m_parents.erase(0);
    }
    else
    { // This is a real multiparent error.
      return QRY_MULTIPLE_PARENTS;
    }
  }
  m_parents.push_back(parentOp);
  assert (m_parents.size() <= 1);

  parentOp->addChild(this);
  return 0;
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


/** This class is used for serializing sequences of 16 bit integers,
 * where the first 16 bit integer specifies the length of the sequence.
 */
class Uint16Sequence{
public:
  explicit Uint16Sequence(Uint32Buffer& buffer, size_t size):
    m_seq(NULL),
    m_size(size),
    m_pos(0),
    m_finished(false)
 {
    if (size > 0) {
      m_seq = buffer.alloc(1 + size/2);
      assert (size <= 0xFFFF);
      m_seq[0] = size;
    }
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
  const size_t  m_size;     // Uint16 words available in m_seq
  size_t        m_pos;      // Current Uint16 word to fill in
  bool          m_finished; // Debug assert of correct call convention
};


void
NdbQueryOperationDefImpl::appendParentList(Uint32Buffer& serializedDef) const
{
  Uint16Sequence parentSeq(serializedDef, getNoOfParentOperations());
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
                 const NdbQueryOperandImpl* const *keys)
{
  Uint32 appendedPattern = 0;
  if (keys[0]!=NULL)
  {
    size_t startPos = serializedDef.getSize();
    serializedDef.append(0);     // Grab first word for length field, updated at end
    int paramCnt = 0;
    int keyNo = 0;
    const NdbQueryOperandImpl* key = keys[0];
    do
    {
      switch(key->getKind()){
      case NdbQueryOperandImpl::Linked:
      {
        appendedPattern |= DABits::NI_KEY_LINKED;
        const NdbLinkedOperandImpl& linkedOp = *static_cast<const NdbLinkedOperandImpl*>(key);
        serializedDef.append(QueryPattern::col(linkedOp.getLinkedColumnIx()));
        break;
      }
      case NdbQueryOperandImpl::Const:
      {
        appendedPattern |= DABits::NI_KEY_CONSTS;
        const NdbConstOperandImpl& constOp 
	  = *static_cast<const NdbConstOperandImpl*>(key);
     
        // No of words needed for storing the constant data.
        const Uint32 wordCount =  AttributeHeader::getDataSize(constOp.getSizeInBytes());
        // Set type and length in words of key pattern field. 
        serializedDef.append(QueryPattern::data(wordCount));
        serializedDef.append(constOp.getAddr(),constOp.getSizeInBytes());
        break;
      }
      case NdbQueryOperandImpl::Param:
      {
        appendedPattern |= DABits::NI_KEY_PARAMS;
        paramCnt++;
        const NdbParamOperandImpl& paramOp = *static_cast<const NdbParamOperandImpl*>(key);
        serializedDef.append(QueryPattern::param(paramOp.getParamIx()));
        break;
      }
      default:
        assert(false);
      }
      key = keys[++keyNo];
    } while (key!=NULL);

    // Set total length of key pattern.
    size_t len = serializedDef.getSize() - startPos -1;
    serializedDef.put(startPos, (paramCnt << 16) | (len));
  }

  return appendedPattern;
} // appendKeyPattern


int
NdbQueryLookupOperationDefImpl
::serializeOperation(Uint32Buffer& serializedDef)
{
  assert (m_keys[0]!=NULL);
  // This method should only be invoked once.
  assert (!m_isPrepared);
  m_isPrepared = true;

  // Reserve memory for LookupNode, fill in contents later when
  // 'length' and 'requestInfo' has been calculated.
  size_t startPos = serializedDef.getSize();
  serializedDef.alloc(QN_LookupNode::NodeSize);
  Uint32 requestInfo = 0;

  /**
   * NOTE: Order of sections within the optional part is fixed as:
   *    Part1:  'NI_HAS_PARENT'
   *    Part2:  'NI_KEY_PARAMS, NI_KEY_LINKED, NI_KEY_CONST'
   *    PART3:  'NI_LINKED_ATTR ++
   */

  // Optional part1: Make list of parent nodes.
  if (getNoOfParentOperations()>0){
    requestInfo |= DABits::NI_HAS_PARENT;
    appendParentList (serializedDef);
  }

  // Part2: Append m_keys[] values specifying lookup key.
  if (getQueryOperationIx() > 0) {
    requestInfo |= appendKeyPattern(serializedDef, m_keys);
  }

  /* Add the projection that should be send to the SPJ block such that 
   * child operations can be instantiated.*/
  if (getNoOfChildOperations()>0) {
    requestInfo |= DABits::NI_LINKED_ATTR;
    Uint16Sequence spjProjSeq(serializedDef, getSPJProjection().size());
    for (Uint32 i = 0; i<getSPJProjection().size(); i++) {
      spjProjSeq.append(getSPJProjection()[i]->getColumnNo());
    }
    spjProjSeq.finish();
  }

  // Fill in LookupNode contents (Already allocated, 'startPos' is our handle:
  QN_LookupNode* node = reinterpret_cast<QN_LookupNode*>(serializedDef.addr(startPos));
  if (unlikely(node==NULL)) {
    return Err_MemoryAlloc;
  }
  assert (m_tbl_record.tableId == m_key_record.tableId);
  node->tableId = m_tbl_record.tableId;
  node->tableVersion = m_tbl_record.tableVersion;
  node->requestInfo = requestInfo;
  const size_t length = serializedDef.getSize() - startPos;
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
} // NdbQueryLookupOperationDefImpl::serializeOperation


int
NdbQueryIndexOperationDefImpl
::serializeOperation(Uint32Buffer& serializedDef)
{
  assert (m_keys[0]!=NULL);
  // This method should only be invoked once.
  assert (!m_isPrepared);
  m_isPrepared = true;

  /**
   * Serialize index as a seperate lookupNode
   */
  {
    // Reserve memory for Index LookupNode, fill in contents later when
    // 'length' and 'requestInfo' has been calculated.
    size_t startPos = serializedDef.getSize();
    serializedDef.alloc(QN_LookupNode::NodeSize);
    Uint32 requestInfo = 0;

    // Optional part1: Make list of parent nodes.
    assert (getQueryOperationId() > 0);
    if (getNoOfParentOperations()>0) {
      requestInfo |= DABits::NI_HAS_PARENT;
      appendParentList (serializedDef);
    }

    // Part2: m_keys[] are the keys to be used for index
    if (getQueryOperationIx() > 0) {  // Root operation key is in KEYINFO 
      requestInfo |= appendKeyPattern(serializedDef, m_keys);
    }

    /* Basetable is executed as child operation of index:
     * Add projection of (only) NDB$PK column which is hidden *after* last index column.
     */
    {
      requestInfo |= DABits::NI_LINKED_ATTR;
      Uint16Sequence spjProjSeq(serializedDef,1);
      spjProjSeq.append(m_key_record.key_index_length);
      spjProjSeq.finish();
    }

    // Fill in LookupNode contents (Already allocated, 'startPos' is our handle:
    QN_LookupNode* node = reinterpret_cast<QN_LookupNode*>(serializedDef.addr(startPos));
    if (unlikely(node==NULL)) {
      return Err_MemoryAlloc;
    }
    node->tableId = m_key_record.tableId;
    node->tableVersion = m_key_record.tableVersion;
    node->requestInfo = requestInfo;
    const size_t length = serializedDef.getSize() - startPos;
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
  } // End: Serialize index table

  // Reserve memory for LookupNode, fill in contents later when
  // 'length' and 'requestInfo' has been calculated.
  size_t startPos = serializedDef.getSize();
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

  /* Add the projection that should be send to the SPJ block such that 
   * child operations can be instantiated.*/
  if (getNoOfChildOperations()>0) {
    requestInfo |= DABits::NI_LINKED_ATTR;
    Uint16Sequence spjProjSeq(serializedDef,getSPJProjection().size());
    for (Uint32 i = 0; i<getSPJProjection().size(); i++) {
      spjProjSeq.append(getSPJProjection()[i]->getColumnNo());
    }
    spjProjSeq.finish();
  }

  // Fill in LookupNode contents (Already allocated, 'startPos' is our handle:
  QN_LookupNode* node = reinterpret_cast<QN_LookupNode*>(serializedDef.addr(startPos)); 
  if (unlikely(node==NULL)) {
    return Err_MemoryAlloc;
  }
  node->tableId = m_tbl_record.tableId;
  node->tableVersion = m_tbl_record.tableVersion;
  node->requestInfo = requestInfo;
  const size_t length = serializedDef.getSize() - startPos;
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


int
NdbQueryScanOperationDefImpl::serialize(Uint32Buffer& serializedDef,
                                        const NdbRecord& tableOrIndex)
{
  // This method should only be invoked once.
  assert (!m_isPrepared);
  m_isPrepared = true;
  // Reserve memory for ScanFragNode, fill in contents later when
  // 'length' and 'requestInfo' has been calculated.
  size_t startPos = serializedDef.getSize();
  serializedDef.alloc(QN_ScanFragNode::NodeSize);
  Uint32 requestInfo = 0;

  // Optional part1: Make list of parent nodes.
  if (getNoOfParentOperations()>0) {
    assert(false); // Scan with parent not yet implemented.
    requestInfo |= DABits::NI_HAS_PARENT;
    appendParentList (serializedDef);
  }

  /** Add the projection that should be send to the SPJ block such that 
   *  child operations can be instantiated.
   */
  if (getNoOfChildOperations()>0){
    requestInfo |= DABits::NI_LINKED_ATTR;
    Uint16Sequence spjProjSeq(serializedDef,getSPJProjection().size());
    for(Uint32 i = 0; i<getSPJProjection().size(); i++){
      spjProjSeq.append(getSPJProjection()[i]->getColumnNo());
    }
    spjProjSeq.finish();
  }

  // Fill in ScanFragNode contents (Already allocated, 'startPos' is our handle:
  QN_ScanFragNode* node = reinterpret_cast<QN_ScanFragNode*>(serializedDef.addr(startPos)); 
  if (unlikely(node==NULL)) {
    return Err_MemoryAlloc;
  }
  node->tableId = tableOrIndex.tableId;
  node->tableVersion = tableOrIndex.tableVersion;
  node->requestInfo = requestInfo;

  const size_t length = serializedDef.getSize() - startPos;
  if (unlikely(length > 0xFFFF)) {
    return QRY_DEFINITION_TOO_LARGE; //Query definition too large.
  } else {
    QueryNode::setOpLen(node->len, QueryNode::QN_SCAN_FRAG, length);
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
  return NdbQueryScanOperationDefImpl::serialize(serializedDef, m_tbl_record);
} // NdbQueryTableScanOperationDefImpl::serializeOperation


int
NdbQueryIndexScanOperationDefImpl
::serializeOperation(Uint32Buffer& serializedDef)
{
  return NdbQueryScanOperationDefImpl::serialize(serializedDef, m_inx_record);
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
