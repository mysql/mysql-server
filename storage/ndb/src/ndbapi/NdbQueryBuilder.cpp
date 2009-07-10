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

#include "NdbQueryBuilderImpl.hpp"
#include <ndb_global.h>
#include <Vector.hpp>

#include "Ndb.hpp"
#include "NdbDictionary.hpp"
#include "NdbOut.hpp"
#include "NdbDictionaryImpl.hpp"
#include "signaldata/QueryTree.hpp"

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

/** The type of an operand. This corresponds to the set of subclasses
 * of NdbQueryOperand.*/
enum OperandKind{
  OperandKind_Linked,
  OperandKind_Param,
  OperandKind_Const
};

//////////////////////////////////////////////
// Implementation of NdbQueryOperand interface
//////////////////////////////////////////////


// Baseclass for the QueryOperand implementation
class NdbQueryOperandImpl
{
public:
  const NdbDictionary::Column* getColumn() const
  { return m_column; };

  virtual int bindOperand(const NdbDictionary::Column& column,
                          NdbQueryOperationDefImpl& operation)
  { if (m_column  && m_column != &column)
      return 4807;  // Already bounded to a different column
    m_column = &column;
    return 0;
  }
  
  virtual const NdbQueryOperand& getInterface() const = 0; 

  OperandKind getKind() const
  { return m_kind;
  }
protected:
  virtual ~NdbQueryOperandImpl()=0;

  NdbQueryOperandImpl(OperandKind kind)
    : m_column(0),
      m_kind(kind)
  {}

private:
  const NdbDictionary::Column* m_column;  // Initial NULL, assigned w/ bindOperand()
  /** This is used to tell the type of an NdbQueryOperand. This allow safe
   * downcasting to a subclass.
   */
  const OperandKind m_kind;
}; // class NdbQueryOperandImpl


class NdbLinkedOperandImpl : public NdbQueryOperandImpl
{
  friend class NdbQueryBuilder;  // Allow privat access from builder interface

public:
  virtual int bindOperand(const NdbDictionary::Column& column,
                          NdbQueryOperationDefImpl& operation);

  const NdbQueryOperationDefImpl& getParentOperation() const
  { return m_parentOperation;
  }
  // 'LinkedSrc' is index into parent op's spj-projection list where
  // the refered column value is available
  Uint32 getLinkedSrc() const
  { return m_parentColumnIx;
  }
  const NdbDictionary::Column& getParentColumn() const
  { return *m_parentOperation.getSPJProjection()[m_parentColumnIx];
  }
  virtual const NdbQueryOperand& getInterface() const
  { return m_interface;
  }

private:
  friend NdbQueryBuilderImpl::~NdbQueryBuilderImpl();
  virtual ~NdbLinkedOperandImpl() {};

  NdbLinkedOperandImpl (NdbQueryOperationDefImpl& parent, 
                        Uint32 columnIx)
   : NdbQueryOperandImpl(OperandKind_Linked),
     m_interface(*this), 
     m_parentOperation(parent),
     m_parentColumnIx(columnIx)
  {};

  NdbLinkedOperand m_interface;
  NdbQueryOperationDefImpl& m_parentOperation;
  const Uint32 m_parentColumnIx;
}; // class NdbLinkedOperandImpl


class NdbParamOperandImpl : public NdbQueryOperandImpl
{
  friend class NdbQueryBuilder;  // Allow privat access from builder interface

public:
  const char* getName() const
  { return m_name; };

  Uint32 getEnum() const
  { return 0; };  // FIXME

  virtual const NdbQueryOperand& getInterface() const
  { return m_interface;
  }

private:
  friend NdbQueryBuilderImpl::~NdbQueryBuilderImpl();
  virtual ~NdbParamOperandImpl() {};
  NdbParamOperandImpl (const char* name)
   : NdbQueryOperandImpl(OperandKind_Param),
     m_interface(*this), 
     m_name(name)
  {};

  NdbParamOperand m_interface;
  const char* const m_name;     // Optional parameter name or NULL
}; // class NdbParamOperandImpl


/////////////////////////////////////////////////////
// Pure virtual baseclass for ConstOperand.
// Each specific const datatype has its own subclass.
/////////////////////////////////////////////////////
class NdbConstOperandImpl : public NdbQueryOperandImpl
{
  friend class NdbQueryBuilder;  // Allow privat access from builder interface
public:
  virtual size_t getLength() const = 0;
  virtual const void* getAddr() const = 0;

  virtual int bindOperand(const NdbDictionary::Column& column,
                          NdbQueryOperationDefImpl& operation);

  virtual NdbDictionary::Column::Type getType() const = 0;

  virtual const NdbQueryOperand& getInterface() const
  { return m_interface;
  }

protected:
  friend NdbQueryBuilderImpl::~NdbQueryBuilderImpl();
  virtual ~NdbConstOperandImpl() {};
  NdbConstOperandImpl ()
    : NdbQueryOperandImpl(OperandKind_Const),
      m_interface(*this)
  {};

private:
  NdbConstOperand m_interface;
}; // class NdbConstOperandImpl

//////////////////////////////////////////////////
// Implements different const datatypes by further
// subclassing of NdbConstOperand.
//////////////////////////////////////////////////
class NdbInt32ConstOperandImpl : public NdbConstOperandImpl
{
public:
  explicit NdbInt32ConstOperandImpl (Int32 value) : 
    NdbConstOperandImpl(), 
    m_value(value) {};
  size_t getLength()    const { return sizeof(m_value); };
  const void* getAddr() const { return &m_value; };
  NdbDictionary::Column::Type getType() const { return NdbDictionary::Column::Int; };
private:
  const Int32 m_value;
};

class NdbUint32ConstOperandImpl : public NdbConstOperandImpl
{
public:
  explicit NdbUint32ConstOperandImpl (Uint32 value) : 
    NdbConstOperandImpl(), 
    m_value(value) {};
  size_t getLength()    const { return sizeof(m_value); };
  const void* getAddr() const { return &m_value; };
  NdbDictionary::Column::Type getType() const { return NdbDictionary::Column::Unsigned; };
private:
  const Uint32 m_value;
};

class NdbInt64ConstOperandImpl : public NdbConstOperandImpl
{
public:
  explicit NdbInt64ConstOperandImpl (Int64 value) : 
    NdbConstOperandImpl(), 
    m_value(value) {};
  size_t getLength()    const { return sizeof(m_value); };
  const void* getAddr() const { return &m_value; };
  NdbDictionary::Column::Type getType() const { return NdbDictionary::Column::Bigint; };
private:
  const Int64 m_value;
};

class NdbUint64ConstOperandImpl : public NdbConstOperandImpl
{
public:
  explicit NdbUint64ConstOperandImpl (Uint64 value) : 
    NdbConstOperandImpl(), 
    m_value(value) {};
  size_t getLength()    const { return sizeof(m_value); };
  const void* getAddr() const { return &m_value; };
  NdbDictionary::Column::Type getType() const { return NdbDictionary::Column::Bigunsigned; };
private:
  const Uint64 m_value;
};

class NdbCharConstOperandImpl : public NdbConstOperandImpl
{
public:
  explicit NdbCharConstOperandImpl (const char* value) : 
    NdbConstOperandImpl(), m_value(value) {};
  size_t getLength()    const { return strlen(m_value); };
  const void* getAddr() const { return m_value; };
  NdbDictionary::Column::Type getType() const { return NdbDictionary::Column::Char; };
private:
  const char* const m_value;
};

class NdbGenericConstOperandImpl : public NdbConstOperandImpl
{
public:
  explicit NdbGenericConstOperandImpl (const void* value, size_t length)
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

// Common Baseclass 'class NdbQueryOperationDefImp' is 
// defined in "NdbQueryBuilderImpl.hpp"


class NdbQueryLookupOperationDefImpl : public NdbQueryOperationDefImpl
{
  friend class NdbQueryBuilder;  // Allow privat access from builder interface

public:
  const NdbDictionary::Index* getIndex() const
  { return m_index; };

  virtual int serializeOperation(Uint32Buffer& serializedDef) const;

private:
  virtual ~NdbQueryLookupOperationDefImpl() {};
  explicit NdbQueryLookupOperationDefImpl (
                           const NdbDictionary::Index& index,
                           const NdbDictionary::Table& table,
                           const NdbQueryOperand* const keys[],
                           const char* ident,
                           Uint32      ix);
  explicit NdbQueryLookupOperationDefImpl (
                           const NdbDictionary::Table& table,
                           const NdbQueryOperand* const keys[],
                           const char* ident,
                           Uint32      ix);
  
  virtual const NdbQueryOperationDef& getInterface() const
  { return m_interface; }

private:
  NdbQueryLookupOperationDef m_interface;
  const NdbDictionary::Index* const m_index;
  NdbQueryOperandImpl* m_keys[MAX_ATTRIBUTES_IN_INDEX+1];

}; // class NdbQueryLookupOperationDefImpl


class NdbQueryScanOperationDefImpl :
  public NdbQueryOperationDefImpl
{
public:
  virtual ~NdbQueryScanOperationDefImpl()=0;
  explicit NdbQueryScanOperationDefImpl (
                           const NdbDictionary::Table& table,
                           const char* ident,
                           Uint32      ix)
  : NdbQueryOperationDefImpl(table,ident,ix)
  {};
}; // class NdbQueryScanOperationDefImpl

class NdbQueryTableScanOperationDefImpl : public NdbQueryScanOperationDefImpl
{
  friend class NdbQueryBuilder;  // Allow privat access from builder interface

public:
  virtual int serializeOperation(Uint32Buffer& serializedDef) const;

  virtual const NdbQueryOperationDef& getInterface() const
  { return m_interface; }

private:
  virtual ~NdbQueryTableScanOperationDefImpl() {};
  explicit NdbQueryTableScanOperationDefImpl (
                           const NdbDictionary::Table& table,
                           const char* ident,
                           Uint32      ix)
    : NdbQueryScanOperationDefImpl(table,ident,ix),
      m_interface(*this) 
  {};

  NdbQueryTableScanOperationDef m_interface;

}; // class NdbQueryTableScanOperationDefImpl


class NdbQueryIndexScanOperationDefImpl : public NdbQueryScanOperationDefImpl
{
  friend class NdbQueryBuilder;  // Allow privat access from builder interface

public:
  const NdbDictionary::Index& getIndex() const
  { return m_index; };

  virtual int serializeOperation(Uint32Buffer& serializedDef) const;

  virtual const NdbQueryOperationDef& getInterface() const
  { return m_interface; }

private:
  virtual ~NdbQueryIndexScanOperationDefImpl() {};
  explicit NdbQueryIndexScanOperationDefImpl (
                           const NdbDictionary::Index& index,
                           const NdbDictionary::Table& table,
                           const NdbQueryIndexBound* bound,
                           const char* ident,
                           Uint32      ix);

private:
  NdbQueryIndexScanOperationDef m_interface;
  const NdbDictionary::Index& m_index;

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
  return ::getImpl(*this).getEnum();
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
  returnErrIf(value==0,4800);
  NdbConstOperandImpl* constOp = new NdbCharConstOperandImpl(value);
  returnErrIf(constOp==0,4000);

  m_pimpl->m_constOperands.push_back(constOp);
  return &constOp->m_interface;
}
NdbConstOperand* 
NdbQueryBuilder::constValue(const void* value, size_t length)
{
  returnErrIf(value==0 && length>0,4800);
  NdbConstOperandImpl* constOp = new NdbGenericConstOperandImpl(value,length);
  returnErrIf(constOp==0,4000);

  m_pimpl->m_constOperands.push_back(constOp);
  return &constOp->m_interface;
}
NdbConstOperand* 
NdbQueryBuilder::constValue(Int32 value)
{
  NdbConstOperandImpl* constOp = new NdbInt32ConstOperandImpl(value);
  returnErrIf(constOp==0,4000);

  m_pimpl->m_constOperands.push_back(constOp);
  return &constOp->m_interface;
}
NdbConstOperand* 
NdbQueryBuilder::constValue(Uint32 value)
{
  NdbConstOperandImpl* constOp = new NdbUint32ConstOperandImpl(value);
  returnErrIf(constOp==0,4000);

  m_pimpl->m_constOperands.push_back(constOp);
  return &constOp->m_interface;
}
NdbConstOperand* 
NdbQueryBuilder::constValue(Int64 value)
{
  NdbConstOperandImpl* constOp = new NdbInt64ConstOperandImpl(value);
  returnErrIf(constOp==0,4000);

  m_pimpl->m_constOperands.push_back(constOp);
  return &constOp->m_interface;
}
NdbConstOperand* 
NdbQueryBuilder::constValue(Uint64 value)
{
  NdbConstOperandImpl* constOp = new NdbUint64ConstOperandImpl(value);
  returnErrIf(constOp==0,4000);

  m_pimpl->m_constOperands.push_back(constOp);
  return &constOp->m_interface;
}

NdbParamOperand* 
NdbQueryBuilder::paramValue(const char* name)
{
  NdbParamOperandImpl* paramOp = new NdbParamOperandImpl(name);
  returnErrIf(paramOp==0,4000);

  m_pimpl->m_paramOperands.push_back(paramOp);
  return &paramOp->m_interface;
}

NdbLinkedOperand* 
NdbQueryBuilder::linkedValue(const NdbQueryOperationDef* parent, const char* attr)
{
  returnErrIf(parent==0 || attr==0, 4800);  // Required non-NULL arguments
  NdbQueryOperationDefImpl& parentImpl = parent->getImpl();

  // Parent should be a OperationDef contained in this query builder context
  returnErrIf(!m_pimpl->contains(&parentImpl), 4804); // Unknown parent

  // 'attr' should refer a column from the underlying table in parent:
  const NdbDictionary::Column* column = parentImpl.getTable().getColumn(attr);
  returnErrIf(column==0, 4805); // Unknown column

  // Locate refered parrent column in parent operations SPJ projection list;
  // Add if not already present
  Uint32 spjRef = parentImpl.addColumnRef(column);

  NdbLinkedOperandImpl* linkedOp = new NdbLinkedOperandImpl(parentImpl,spjRef);
  returnErrIf(linkedOp==0, 4000);

  m_pimpl->m_linkedOperands.push_back(linkedOp);
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
    new NdbQueryLookupOperationDefImpl(*table,keys,ident,
                                       m_pimpl->m_operations.size());
  returnErrIf(op==0, 4000);

  int keyindex = 0;
  for (i=0; i<colcount; ++i)
  {
    const NdbDictionary::Column *col = table->getColumn(i);
    if (col->getPrimaryKey())
    {
      int error = op->m_keys[keyindex]->bindOperand(*col,*op);
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
  returnErrIf(table==0 || index==0 || keys==0, 4800);  // Required non-NULL arguments

  // TODO: Restrict to only table_version_major() mismatch?
  returnErrIf(NdbIndexImpl::getImpl(*index).m_table_id != table->getObjectId() ||
              NdbIndexImpl::getImpl(*index).m_table_version != table->getObjectVersion(), 4806);

  // Check: keys[] are specified for all fields in 'index'
  int inxfields = index->getNoOfColumns();
  for (i=0; i<inxfields; ++i)
  {
    returnErrIf(keys[i]==NULL, 4801);  // A 'Key' value is undefineds
  }
  // Check for propper NULL termination of keys[] spec
  returnErrIf(keys[inxfields]!=NULL, 4802);

  NdbQueryLookupOperationDefImpl* op = 
    new NdbQueryLookupOperationDefImpl(*index,*table,keys,ident,
                                       m_pimpl->m_operations.size());
  returnErrIf(op==0, 4000);

  // Bind to Column and check type compatibility
  for (i=0; i<inxfields; ++i)
  {
    const NdbDictionary::Column *col = index->getColumn(i);
    int error = keys[i]->getImpl().bindOperand(*col,*op);
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
  returnErrIf(table==0, 4800);  // Required non-NULL arguments

  NdbQueryTableScanOperationDefImpl* op =
    new NdbQueryTableScanOperationDefImpl(*table,ident,
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
  returnErrIf(table==0 || index==0, 4800);  // Required non-NULL arguments

  // TODO: Restrict to only table_version_major() mismatch?
  returnErrIf(NdbIndexImpl::getImpl(*index).m_table_id != table->getObjectId() ||
              NdbIndexImpl::getImpl(*index).m_table_version != table->getObjectVersion(), 4806);

  NdbQueryIndexScanOperationDefImpl* op =
    new NdbQueryIndexScanOperationDefImpl(*index,*table,bound,ident,
                                          m_pimpl->m_operations.size());
  returnErrIf(op==0, 4000);

  int i;
  int inxfields = index->getNoOfColumns();
  for (i=0; i<inxfields; ++i)
  {
    if (op->m_bound.low[i] == NULL)
      break;
    const NdbDictionary::Column *col = index->getColumn(i);
    int error = op->m_bound.low[i]->bindOperand(*col,*op);
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
      const NdbDictionary::Column *col = index->getColumn(i);
      int error = op->m_bound.high[i]->bindOperand(*col,*op);
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
: m_ndb(ndb), m_error(), m_operations(),
  m_paramOperands(), m_constOperands(), m_linkedOperands()
{}

NdbQueryBuilderImpl::~NdbQueryBuilderImpl()
{
  Uint32 i;

  // Delete all operand and operator in Vector's
  for (i=0; i<m_operations.size(); ++i)
  { delete m_operations[i];
  }
  for (i=0; i<m_paramOperands.size(); ++i)
  { delete m_paramOperands[i];
  }
  for (i=0; i<m_constOperands.size(); ++i)
  { delete m_constOperands[i];
  }
  for (i=0; i<m_linkedOperands.size(); ++i)
  { delete m_linkedOperands[i];
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
  returnErrIf(def==0, 4000);
  if(unlikely(error!=0)){
    delete def;
    setErrorCode(error);
    return NULL;
  }
  m_operations.clear();
  m_paramOperands.clear();
  m_constOperands.clear();
  m_linkedOperands.clear();

  return def;
}

///////////////////////////////////
// The (hidden) Impl of NdbQueryDef
///////////////////////////////////
NdbQueryDefImpl::
NdbQueryDefImpl(const NdbQueryBuilderImpl& builder,
                const Vector<NdbQueryOperationDefImpl*>& operations,
                int& error)
 : m_interface(*this), 
   m_operations(operations)
{
  /* Sets size to 1, such that serialization of operation 0 will start from 
   * offset 1, leaving space for the length field.*/
  m_serializedDef.get(0); 
  for(Uint32 i = 0; i<m_operations.size(); i++){
    error = m_operations[i]->serializeOperation(m_serializedDef);
    if(unlikely(error != 0)){
      return;
    }
  }
  // Set length and number of nodes in tree.
  QueryTree::setCntLen(m_serializedDef.get(0), 
		       m_operations.size(),
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
                           const NdbDictionary::Table& table,
                           const NdbQueryOperand* const keys[],
                           const char* ident,
                           Uint32      ix)
 : NdbQueryOperationDefImpl(table,ident,ix),
   m_interface(*this), 
   m_index(0)
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

NdbQueryLookupOperationDefImpl::NdbQueryLookupOperationDefImpl (
                           const NdbDictionary::Index& index,
                           const NdbDictionary::Table& table,
                           const NdbQueryOperand* const keys[],
                           const char* ident,
                           Uint32      ix)
 : NdbQueryOperationDefImpl(table,ident,ix),
   m_interface(*this), 
   m_index(&index)
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
                           const NdbDictionary::Index& index,
                           const NdbDictionary::Table& table,
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
NdbQueryOperationDefImpl::addColumnRef(const NdbDictionary::Column* column)
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
                           const NdbDictionary::Column& column,
                           NdbQueryOperationDefImpl& operation)
{
  NdbDictionary::Column::Type type = column.getType();
  if (type != getParentColumn().getType())
    return 4803;  // Incompatible datatypes

  // TODO? Check length if Char, and prec, scale if decimal type

  // Register parent/child operation relations
  this->m_parentOperation.addChild(&operation);
  operation.addParent(&this->m_parentOperation);

  return NdbQueryOperandImpl::bindOperand(column,operation);
}


int
NdbConstOperandImpl::bindOperand(
                           const NdbDictionary::Column& column,
                           NdbQueryOperationDefImpl& operation)
{
  NdbDictionary::Column::Type type = column.getType();
  if (type != this->getType())
    return 4803;  // Incompatible datatypes

  // TODO? Check length if Char, and prec,scale if decimal type

  return NdbQueryOperandImpl::bindOperand(column,operation);
}

/** This class is used for serializing sequences of 16 bit integers,
 * where the first 16 bit integer specifies the length of the sequence.
 */
class Uint16Sequence{
public:
  explicit Uint16Sequence(Uint32Slice buffer):
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
  };
private:
  /** Should not be copied.*/
  Uint16Sequence(Uint16Sequence&);
  /** Should not be copied.*/
  Uint16Sequence& operator=(Uint16Sequence&);
  Uint32Slice m_buffer;
  int m_length;
};


// Find offset (in 32-bit words) of field within struct QN_LookupNode.
#define POS_IN_LOOKUP(field) (offsetof(QN_LookupNode, field)/sizeof(Uint32)) 

int
NdbQueryLookupOperationDefImpl
::serializeOperation(Uint32Buffer& serializedDef) const{
  Uint32Slice lookupNode(serializedDef, serializedDef.getSize());
  lookupNode.get(POS_IN_LOOKUP(tableId)) 
    = getTable().getObjectId();
  lookupNode.get(POS_IN_LOOKUP(tableVersion))
    = getTable().getObjectVersion();
  lookupNode.get(POS_IN_LOOKUP(requestInfo)) = 0;
  Uint32 requestInfo = 0;

  /**
   * NOTE: Order of sections within the optional part is fixed as:
   *    Part1:  'NI_HAS_PARENT'
   *    Part2:  'NI_KEY_PARAMS, NI_KEY_LINKED, NI_KEY_CONST'
   *    PART3:  'NI_LINKED_ATTR ++
   */

  Uint32Slice optional(lookupNode, POS_IN_LOOKUP(optional));
  int optPos = 0;
  if(getNoOfParentOperations()>0){
    // Optional part1: Make list of parent nodes.
    requestInfo |= DABits::NI_HAS_PARENT;
    Uint16Sequence parentSeq(optional);
    // Multiple parents not yet supported.
    assert(getNoOfParentOperations()==1);
    for(Uint32 i = 0; 
        i < getNoOfParentOperations(); 
        i++){
      parentSeq.append(getParentOperation(i).getQueryOperationIx());
    }
    optPos = parentSeq.finish();
  }
  Uint32Slice keyPattern(optional, optPos+1);
  int keyPatternPos = 0;
  int keyNo = 0;
  int paramCnt = 0;
  const NdbQueryOperandImpl* op = m_keys[0];
  while(op!=NULL){
    switch(op->getKind()){
    case OperandKind_Linked:
    {
      requestInfo |= DABits::NI_KEY_LINKED;
      const NdbLinkedOperandImpl& linkedOp = *static_cast<const NdbLinkedOperandImpl*>(op);
      keyPattern.get(keyPatternPos++) = QueryPattern::col(linkedOp.getLinkedSrc());
      break;
    }
    case OperandKind_Const:
    {
      requestInfo |= DABits::NI_KEY_CONSTS;
      const NdbConstOperandImpl& constOp 
	= *static_cast<const NdbConstOperandImpl*>(op);
      // No of words needed for storing the constant data.
      const Uint32 wordCount 
	= (sizeof(Uint32) + constOp.getLength() - 1) / sizeof(Uint32);
      // Make sure that any trailing bytes in the last word are zero.
      keyPattern.get(keyPatternPos+wordCount) = 0;
      // Sent type and length in words of key pattern field. 
      keyPattern.get(keyPatternPos++) 
	= QueryPattern::data(wordCount);
      // Copy constant data.
      memcpy(&keyPattern.get(keyPatternPos, wordCount), 
             constOp.getAddr(),
             constOp.getLength());
      keyPatternPos += wordCount;
      break;
    }
    case OperandKind_Param:  // TODO: Implement this
/**** FIXME: can't set NI_KEY_PARAMS yet as this also require PI_KEY_PARAMS in parameter part
      requestInfo |= DABits::NI_KEY_PARAMS;
      paramCnt++;
*****/
      keyPattern.get(keyPatternPos++) = QueryPattern::data(0);  // Simple hack to avoid 'assert(keyPatternPos>0)' below
      break;
    default:
      assert(false);
    }
    op = m_keys[++keyNo];
  }
  if(m_keys[0]!=NULL){
    assert(keyPatternPos>0);  
    // Set total length of key pattern.
    optional.get(optPos) = (paramCnt << 16) | keyPatternPos;
    optPos += keyPatternPos+1;
  }

  /* Add the projection that should be send to the SPJ block such that 
   * child operations can be instantiated.*/
//assert ((getNoOfChildOperations()==0) == (getSPJProjection().size()==0));
  if (getNoOfChildOperations()>0){
    requestInfo |= DABits::NI_LINKED_ATTR;
    Uint16Sequence spjProjSeq(Uint32Slice(optional, optPos));
    for(Uint32 i = 0; i<getSPJProjection().size(); i++){
      spjProjSeq.append(getSPJProjection()[i]->getColumnNo());
    }
    optPos += spjProjSeq.finish();
  }
  lookupNode.get(POS_IN_LOOKUP(requestInfo)) = requestInfo;
  const int length = POS_IN_LOOKUP(optional)+optPos;

  // Set node length and type.
  QueryNode::setOpLen(lookupNode.get(POS_IN_LOOKUP(len)), 
		      QueryNode::QN_LOOKUP, length);
  if(unlikely(serializedDef.isMaxSizeExceeded())){
    return 4808; //Query definition too large.
  }    
#ifdef TRACE_SERIALIZATION
  ndbout << "Serialized node " << getQueryOperationIx() << " : ";
  for(int i = 0; i < length; i++){
    char buf[12];
    sprintf(buf, "%.8x", lookupNode.get(i));
    ndbout << buf << " ";
  }
  ndbout << endl;
#endif
  return 0;
}

int
NdbQueryTableScanOperationDefImpl
::serializeOperation(Uint32Buffer& serializedDef) const{
  // TODO:Implement this
  assert(false);
  return 0;
}

int
NdbQueryIndexScanOperationDefImpl
::serializeOperation(Uint32Buffer& serializedDef) const{
  // TODO:Implement this
  assert(false);
  return 0;
}

// Instantiate Vector templates
template class Vector<NdbQueryOperationDefImpl*>;
template class Vector<NdbParamOperandImpl*>;
template class Vector<NdbConstOperandImpl*>;
template class Vector<NdbLinkedOperandImpl*>;
template class Vector<const NdbDictionary::Column*>;

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
