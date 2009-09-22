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


#ifndef NdbQueryBuilderImpl_H
#define NdbQueryBuilderImpl_H

// Query-related error codes.
#define QRY_REQ_ARG_IS_NULL 4800
#define QRY_TOO_FEW_KEY_VALUES 4801
#define QRY_TOO_MANY_KEY_VALUES 4802
#define QRY_OPERAND_HAS_WRONG_TYPE 4803
#define QRY_UNKONWN_PARENT 4804
#define QRY_UNKNOWN_COLUMN 4805
#define QRY_UNRELATED_INDEX 4806
#define QRY_WRONG_INDEX_TYPE 4807
#define QRY_OPERAND_ALREADY_BOUND 4808
#define QRY_DEFINITION_TOO_LARGE 4809
#define QRY_DUPLICATE_COLUMN_IN_PROJ 4810
#define QRY_NEED_PARAMETER 4811
#define QRY_RESULT_ROW_ALREADY_DEFINED 4812

#ifdef __cplusplus
#include "signaldata/TcKeyReq.hpp"
#include "signaldata/ScanTab.hpp"
#include <Vector.hpp>
#include "NdbQueryBuilder.hpp"

// Forward declared
class NdbTableImpl;
class NdbIndexImpl;
class NdbColumnImpl;
class NdbQueryBuilderImpl;
class NdbQueryDefImpl;
class NdbQueryOperationDefImpl;
class NdbParamOperandImpl;
class NdbConstOperandImpl;
class NdbLinkedOperandImpl;

// For debuggging.
#define TRACE_SERIALIZATION

/** A buffer for holding serialized data.*/
class Uint32Buffer{
public:
  STATIC_CONST(maxSize = MAX(TcKeyReq::MaxTotalAttrInfo, 
                             ScanTabReq::MaxTotalAttrInfo));
  explicit Uint32Buffer():
    m_size(0),
    m_maxSizeExceeded(false){
  }
  
  /**
   * Allocate a buffer extension at end of this buffer.
   * NOTE: Return memory even if allocation failed ->
   *       Always check ::isMaxSizeExceeded() before buffer
   *       content is used
   */   
  Uint32* alloc(size_t count) {
    if(unlikely(m_size+count >= maxSize)) {
      //ndbout << "Uint32Buffer::get() Attempt to access " << newMax 
      //       << "th element.";
      m_maxSizeExceeded = true;
      assert(count <= maxSize);
      return &m_array[0];
    }
    Uint32* extend = &m_array[m_size];
    m_size += count;
    return extend;
  }

  /** Put the idx'th element. Make sure there is space for 'count' elements.
   *  Prefer usage of append() when putting at end of the buffer.
   */
  void put(Uint32 idx, Uint32 value) {
    assert(idx < m_size);
    m_array[idx] = value;
  }

  /** append 'src' word to end of this buffer
   */
  void append(const Uint32 src) {
    if (likely(m_size < maxSize))
      m_array[m_size++] = src;
    else
      m_maxSizeExceeded = true;
  }

  /** append 'src' buffer to end of this buffer
   */
  void append(const Uint32Buffer& src) {
    size_t len = src.getSize();
    if (len > 0) {
      memcpy(alloc(len), src.addr(), len*sizeof(Uint32));
    }
  }

  /** append 'src' *bytes* to end of this buffer
   *  Zero pad possibly odd bytes in last Uint32 word
   */
  void append(const void* src, size_t len) {
    if (len > 0) {
      size_t wordCount = (len + sizeof(Uint32)-1) / sizeof(Uint32);
      Uint32* dst = alloc(wordCount);
      // Make sure that any trailing bytes in the last word are zero.
      dst[wordCount-1] = 0;
      // Copy src 
      memcpy(dst, src, len);
    }
  }

  const Uint32* addr() const {
    return (m_size>0) ?m_array :NULL;
  }

  /** Get the idx'th element. Make sure there is space for 'count' elements.*/
  Uint32 get(Uint32 idx) const {
    assert(idx < m_size);
    assert(!m_maxSizeExceeded);
    return m_array[idx];
  }

  /** Check for out-of-bounds access.*/
  bool isMaxSizeExceeded() const {
    return m_maxSizeExceeded;
  }

  size_t getSize() const {
    return m_size;
  }

private:
  /** Should not be copied, nor assigned.*/
  Uint32Buffer(Uint32Buffer&);
  Uint32Buffer& operator=(Uint32Buffer&);

private:
  /* TODO: replace array with something that can grow and allocate from a 
   * pool as needed..*/
  Uint32 m_array[maxSize];
  Uint32 m_size;
  bool m_maxSizeExceeded;
};


////////////////////////////////////////////////
// Implementation of NdbQueryOperation interface
////////////////////////////////////////////////

/** For making complex declarations more readable.*/
typedef const void* constVoidPtr;

class NdbQueryOperationDefImpl
{
  friend class NdbQueryOperationImpl;
  friend class NdbQueryImpl;

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

  Uint32 getNoOfParentOperations() const
  { return m_parents.size(); }

  const NdbQueryOperationDefImpl& getParentOperation(Uint32 i) const
  { return *m_parents[i]; }

  Uint32 getNoOfChildOperations() const
  { return m_children.size(); }

  const NdbQueryOperationDefImpl& getChildOperation(Uint32 i) const
  { return *m_children[i]; }

  const NdbTableImpl& getTable() const
  { return m_table; }

  const char* getName() const
  { return m_ident; }

  Uint32 assignQueryOperationId(Uint32& nodeId)
  { if (getType()==UniqueIndexAccess) nodeId++;
    m_id = nodeId++;
    return m_id;
  }

  // Register a operation as parent of this operation
  void addParent(NdbQueryOperationDefImpl*);

  // Register a linked child refering this operation
  void addChild(NdbQueryOperationDefImpl*);

  // Register a linked reference to a column from operation
  // Return position in list of refered columns available from
  // this (parent) operation. Child ops later refer linked 
  // columns by its position in this list
  Uint32 addColumnRef(const NdbColumnImpl*);

  // Register a param operand which is refered by this operation.
  // Param values are supplied pr. operation when code is serialized.
  void addParamRef(const NdbParamOperandImpl* param)
  { m_params.push_back(param); }

  Uint32 getNoOfParameters() const
  { return m_params.size(); }

  const NdbParamOperandImpl& getParameter(Uint32 ix) const
  { return *m_params[ix]; }

  virtual const NdbIndexImpl* getIndex() const
  { return NULL; }

  // Return 'true' is query type is a multi-row scan
  virtual bool isScanOperation() const = 0;

  virtual const NdbQueryOperationDef& getInterface() const = 0; 

  /** Make a serialized representation of this operation, corresponding to
   * the struct QueryNode type.
   * @return Possible error code.
   */
  virtual int serializeOperation(Uint32Buffer& serializedTree) const = 0;

  /** Find the projection that should be sent to the SPJ block. This should
   * contain the attributes needed to instantiate all child operations.
   */
  const Vector<const NdbColumnImpl*>& getSPJProjection() const{
    return m_spjProjection;
  }

  /**
   * Expand keys and bounds for the root operation into the KEYINFO section.
   * @param keyInfo Actuall KEYINFO section the key / bounds are 
   *                put into
   * @param actualParam Instance values for NdbParamOperands.
   * @param isPruned 'true' for a scan of pruned to single partition.
   * @param hashValue Valid only if 'isPruned'.
   * Returns: 0 if OK, or possible an errorcode.
   */
  virtual int prepareKeyInfo(Uint32Buffer& keyInfo,
                             const constVoidPtr actualParam[],
                             bool&   isPruned,
                             Uint32& hashValue) const = 0;

  virtual ~NdbQueryOperationDefImpl() = 0;

protected:
  explicit NdbQueryOperationDefImpl (
                                     const NdbTableImpl& table,
                                     const char* ident,
                                     Uint32      ix)
   : m_table(table), m_ident(ident), m_ix(ix), m_id(ix),
     m_parents(), m_children(), m_params(),
     m_spjProjection()
  {}

  // Get the ordinal position of this operation within the query def.
  Uint32 getQueryOperationIx() const
  { return m_ix; }

  // Get id of node as known inside queryTree
  Uint32 getQueryOperationId() const
  { return m_id; }

  // Get type of query operation
  virtual Type getType() const = 0;

  // QueryTree building:
  // Append list of parent nodes to serialized code
  void appendParentList(Uint32Buffer& serializedDef) const;

private:
  const NdbTableImpl& m_table;
  const char* const m_ident; // Optional name specified by aplication
  const Uint32 m_ix;         // Index of this operation within operation array
  Uint32       m_id;         // Operation id when materialized into queryTree.
                             // If op has index, index id is 'm_id-1'.

  // parent / child vectors contains dependencies as defined
  // with linkedValues
  Vector<NdbQueryOperationDefImpl*> m_parents;
  Vector<NdbQueryOperationDefImpl*> m_children;

  // Params required by this operation
  Vector<const NdbParamOperandImpl*> m_params;

  // Column from this operation required by its child operations
  Vector<const NdbColumnImpl*> m_spjProjection;
}; // class NdbQueryOperationDefImpl


class NdbQueryDefImpl
{
  friend class NdbQueryDef;

public:
  explicit NdbQueryDefImpl(const NdbQueryBuilderImpl& builder, 
                           const Vector<NdbQueryOperationDefImpl*>& operations,
                           int& error);
  ~NdbQueryDefImpl();

  // Entire query is a scan iff root operation is scan. 
  // May change in the future as we implement more complicated SPJ operations.
  bool isScanQuery() const
  { return m_operations[0]->isScanOperation(); }

  Uint32 getNoOfOperations() const
  { return m_operations.size(); }

  // Get a specific NdbQueryOperationDef by ident specified
  // when the NdbQueryOperationDef was created.
  const NdbQueryOperationDefImpl& getQueryOperation(Uint32 index) const
  { return *m_operations[index]; } 

  const NdbQueryOperationDefImpl* getQueryOperation(const char* ident) const;

  const NdbQueryDef& getInterface() const
  { return m_interface; }

  /** Get serialized representation of query definition.*/
  Uint32Buffer& getSerialized()
  { return m_serializedDef; }

  /** Get serialized representation of query definition.*/
  const Uint32Buffer& getSerialized() const
  { return m_serializedDef; }

private:
  NdbQueryDef m_interface;

  Vector<NdbQueryOperationDefImpl*> m_operations;
  Vector<NdbQueryOperandImpl*> m_operands;
  Uint32Buffer m_serializedDef; 
}; // class NdbQueryDefImpl


class NdbQueryBuilderImpl
{
  friend class NdbQueryBuilder;

public:
  ~NdbQueryBuilderImpl();
  explicit NdbQueryBuilderImpl(Ndb& ndb);

  const NdbQueryDefImpl* prepare();

  const NdbError& getNdbError() const;

  void setErrorCode(int aErrorCode)
  { if (!m_error.code)
      m_error.code = aErrorCode;
  }

private:
  bool hasError() const
  { return (m_error.code!=0); }

  bool contains(const NdbQueryOperationDefImpl*);

  Ndb& m_ndb;
  NdbError m_error;

  Vector<NdbQueryOperationDefImpl*> m_operations;
  Vector<NdbQueryOperandImpl*> m_operands;
  Uint32 m_paramCnt;
}; // class NdbQueryBuilderImpl


//////////////////////////////////////////////
// Implementation of NdbQueryOperand interface
//////////////////////////////////////////////

// Baseclass for the QueryOperand implementation
class NdbQueryOperandImpl
{
public:

  /** The type of an operand. This corresponds to the set of subclasses
   * of NdbQueryOperandImpl.
   */
  enum Kind {
    Linked,
    Param,
    Const
  };

  const NdbColumnImpl* getColumn() const
  { return m_column; }

  virtual int bindOperand(const NdbColumnImpl& column,
                          NdbQueryOperationDefImpl& operation)
  { if (m_column  && m_column != &column)
      // Already bounded to a different column
      return QRY_OPERAND_ALREADY_BOUND;
    m_column = &column;
    return 0;
  }
  
  Kind getKind() const
  { return m_kind; }

protected:
  friend NdbQueryBuilderImpl::~NdbQueryBuilderImpl();
  friend NdbQueryDefImpl::~NdbQueryDefImpl();

  virtual ~NdbQueryOperandImpl()=0;

  NdbQueryOperandImpl(Kind kind)
    : m_column(0),
      m_kind(kind)
  {}

private:
  const NdbColumnImpl* m_column;       // Initial NULL, assigned w/ bindOperand()
  /** This is used to tell the type of an NdbQueryOperand. This allow safe
   * downcasting to a subclass.
   */
  const Kind m_kind;
}; // class NdbQueryOperandImpl


class NdbLinkedOperandImpl : public NdbQueryOperandImpl
{
  friend class NdbQueryBuilder;  // Allow privat access from builder interface

public:
  virtual int bindOperand(const NdbColumnImpl& column,
                          NdbQueryOperationDefImpl& operation);

  const NdbQueryOperationDefImpl& getParentOperation() const
  { return m_parentOperation; }

  // 'LinkedSrc' is index into parent op's spj-projection list where
  // the refered column value is available
  Uint32 getLinkedColumnIx() const
  { return m_parentColumnIx; }

  const NdbColumnImpl& getParentColumn() const
  { return *m_parentOperation.getSPJProjection()[m_parentColumnIx]; }

  virtual const NdbLinkedOperand& getInterface() const
  { return m_interface; }

private:
  virtual ~NdbLinkedOperandImpl() {}

  NdbLinkedOperandImpl (NdbQueryOperationDefImpl& parent, 
                        Uint32 columnIx)
   : NdbQueryOperandImpl(Linked),
     m_interface(*this), 
     m_parentOperation(parent),
     m_parentColumnIx(columnIx)
  {}

  NdbLinkedOperand m_interface;
  NdbQueryOperationDefImpl& m_parentOperation;
  const Uint32 m_parentColumnIx;
}; // class NdbLinkedOperandImpl


class NdbParamOperandImpl : public NdbQueryOperandImpl
{
  friend class NdbQueryBuilder;  // Allow privat access from builder interface

public:
  virtual int bindOperand(const NdbColumnImpl& column,
                          NdbQueryOperationDefImpl& operation);

  const char* getName() const
  { return m_name; }

  Uint32 getParamIx() const
  { return m_paramIx; }

  virtual const NdbParamOperand& getInterface() const
  { return m_interface; }

private:
  virtual ~NdbParamOperandImpl() {}
  NdbParamOperandImpl (const char* name, Uint32 paramIx)
   : NdbQueryOperandImpl(Param),
     m_interface(*this), 
     m_name(name),
     m_paramIx(paramIx)
  {}

  NdbParamOperand m_interface;
  const char* const m_name;     // Optional parameter name or NULL
  const Uint32 m_paramIx;
}; // class NdbParamOperandImpl


class NdbConstOperandImpl : public NdbQueryOperandImpl
{
  friend class NdbQueryBuilder;  // Allow privat access from builder interface
public:
  virtual size_t getLength() const = 0;
  virtual const void* getAddr() const = 0;

  virtual int bindOperand(const NdbColumnImpl& column,
                          NdbQueryOperationDefImpl& operation);

  virtual NdbDictionary::Column::Type getType() const = 0;

  virtual const NdbConstOperand& getInterface() const
  { return m_interface; }

protected:
  virtual ~NdbConstOperandImpl() {}
  NdbConstOperandImpl ()
    : NdbQueryOperandImpl(Const),
      m_interface(*this)
  {}

private:
  NdbConstOperand m_interface;
}; // class NdbConstOperandImpl


#endif // __cplusplus
#endif
