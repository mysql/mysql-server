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
#define QRY_OPERAND_ALREADY_BOUND 4807
#define QRY_DEFINITION_TOO_LARGE 4808
#define QRY_DUPLICATE_COLUMN_IN_PROJ 4809
#define QRY_NEED_PARAMETER 4810
#define QRY_RESULT_ROW_ALREADY_DEFINED 4811


#ifdef __cplusplus
#include "signaldata/TcKeyReq.hpp"
#include "signaldata/ScanTab.hpp"
#include <Vector.hpp>
#include "NdbQueryBuilder.hpp"
#include "NdbDictionary.hpp"

// Forward declared
class NdbQueryBuilderImpl;
class NdbQueryOperationDefImpl;
class NdbParamOperandImpl;
class NdbConstOperandImpl;
class NdbLinkedOperandImpl;

/** A buffer for holding serialized data.*/
class Uint32Buffer{
public:
  STATIC_CONST(maxSize = MAX(TcKeyReq::MaxTotalAttrInfo, 
                             ScanTabReq::MaxTotalAttrInfo));
  explicit Uint32Buffer():
    m_size(0),
    m_maxSizeExceeded(false){
  }
  
  /** Get the idx'th element. Make sure there is space for 'count' elements.*/
  Uint32& get(Uint32 idx, Uint32 count = 1){
    const Uint32 newMax = idx+count-1;
    if(newMax >= m_size){
      if(unlikely(newMax >= maxSize)){
        //ndbout << "Uint32Buffer::get() Attempt to access " << newMax 
        //       << "th element.";
        m_maxSizeExceeded = true;
        assert(count <= maxSize);
        return  m_array[0];
      }
      m_size = newMax+1;
    }
    return m_array[idx];
  }

  /** Get the idx'th element. Make sure there is space for 'count' elements.*/
  const Uint32& get(Uint32 idx, Uint32 count=1) const {
    assert(idx+count-idx < m_size);
    assert(!m_maxSizeExceeded);
    return m_array[idx];
  }

  /** append 'src' to end of this buffer */
  size_t append(const Uint32Buffer& src) {
    memcpy(&get(getSize(), src.getSize()),
           &src.get(0),
           src.getSize()*sizeof(Uint32));
    return src.getSize();
  }

  /** append 'src' *bytes* to end of this buffer
   *  Zero pad possibly odd bytes in last Uint32 word */
  size_t append(const void* src, size_t len) {
    size_t wordCount = (len + sizeof(Uint32)-1) / sizeof(Uint32);
    Uint32* dst = &get(getSize(), wordCount);

    // Make sure that any trailing bytes in the last word are zero.
    dst[wordCount-1] = 0;
    // Copy src 
    memcpy(dst, src, len);
    return wordCount;
  }

  /** Check for out-of-bounds access.*/
  bool isMaxSizeExceeded() const {
    return m_maxSizeExceeded;
  }

  Uint32 getSize() const {return m_size;}
private:
  /* TODO: replace array with something that can grow and allocate from a 
   * pool as needed..*/
  Uint32 m_array[maxSize];
  Uint32 m_size;
  bool m_maxSizeExceeded;
};

/** A reference to a subset of an Uint32Buffer.
*/
class Uint32Slice{
public:
  /** Constructs a slice at end of underlying buffer */
  explicit Uint32Slice(Uint32Buffer& buffer):
    m_buffer(buffer),
    m_offset(buffer.getSize()){
  }

  explicit Uint32Slice(Uint32Buffer& buffer, Uint32 offset):
    m_buffer(buffer),
    m_offset(offset){
  }

  explicit Uint32Slice(const Uint32Slice& slice, Uint32 offset):
    m_buffer(slice.m_buffer),
    m_offset(offset+slice.m_offset){
  }

  Uint32& get(int idx, Uint32 count=1){
    return m_buffer.get(idx+m_offset, count);
  }

  const Uint32& get(int i, Uint32 count=1)const{
    return m_buffer.get(i+m_offset, count);
  }

  /** append 'src' *bytes* to end of this buffer
   *  Zero pad possibly odd bytes in last Uint32 word */
  size_t append(const void* src, size_t len) const {
    return m_buffer.append(src,len);
  }

  Uint32 getOffset() const {return m_offset;}

  Uint32 getSize() const {return m_buffer.getSize() - m_offset;}

  /** Check for out-of-bounds access.*/
  bool isMaxSizeExceeded() const {
    return m_buffer.isMaxSizeExceeded();
  }
private:
  Uint32Buffer& m_buffer;
  const Uint32 m_offset;
};



class NdbQueryDefImpl
{
  friend class NdbQueryDef;

public:
  explicit NdbQueryDefImpl(const NdbQueryBuilderImpl& builder, 
                           const Vector<NdbQueryOperationDefImpl*>& operations,
                           int& error);
  ~NdbQueryDefImpl();

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
  Uint32Buffer& getSerialized(){
    return m_serializedDef;
  }

  /** Get serialized representation of query definition.*/
  const Uint32Buffer& getSerialized() const {
    return m_serializedDef;
  }

private:
  NdbQueryDef m_interface;

  Vector<NdbQueryOperationDefImpl*> m_operations;
  Vector<NdbQueryOperandImpl*> m_operands;
  Uint32Buffer m_serializedDef; 
}; // class NdbQueryDefImpl

// For debuggging.
#define TRACE_SERIALIZATION

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


////////////////////////////////////////////////
// Implementation of NdbQueryOperation interface
////////////////////////////////////////////////

/** For making complex declarations more readable.*/
typedef const void* constVoidPtr;

class NdbQueryOperationDefImpl
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

  // Get the ordinal position of this operation within the query
  Uint32 getQueryOperationIx() const
  { return m_ix; }

  Uint32 getNoOfParentOperations() const
  { return m_parents.size(); }

  const NdbQueryOperationDefImpl& getParentOperation(Uint32 i) const
  { return *m_parents[i]; }

  Uint32 getNoOfChildOperations() const
  { return m_children.size(); }

  const NdbQueryOperationDefImpl& getChildOperation(Uint32 i) const
  { return *m_children[i]; }

  const NdbDictionary::Table& getTable() const
  { return m_table; }

  const char* getName() const
  { return m_ident; }

  // Register a operation as parent of this operation
  void addParent(NdbQueryOperationDefImpl*);

  // Register a linked child refering this operation
  void addChild(NdbQueryOperationDefImpl*);

  // Register a linked reference to a column from operation
  // Return position in list of refered columns available from
  // this (parent) operation. Child ops later refer linked 
  // columns by its position in this list
  Uint32 addColumnRef(const NdbDictionary::Column*);

  // Register a param operand which is refered by this operation.
  // Param values are supplied pr. operation when code is serialized.
  void addParamRef(const NdbParamOperandImpl* param)
  { m_params.push_back(param); }

  Uint32 getNoOfParameters() const
  { return m_params.size(); }

  const NdbParamOperandImpl& getParameter(Uint32 ix) const
  { return *m_params[ix]; }

  // Get type of query operation
  virtual Type getType() const = 0;

  virtual const NdbQueryOperationDef& getInterface() const = 0; 

  /** Make a serialized representation of this operation, corresponding to
   * the struct QueryNode type.
   * @return Possible error code.
   */
  virtual int serializeOperation(Uint32Buffer& serializedTree) const = 0;

  /** Find the projection that should be sent to the SPJ block. This should
   * contain the attributes needed to instantiate all child operations.
   */
  const Vector<const NdbDictionary::Column*>& getSPJProjection() const{
    return m_spjProjection;
  }

  /** Expand keys, bound and filters for the root operation. This is needed
   * for allowing TCKEYREQ/LQHKEYREQ messages to be hashed to the right data
   * node
   * @param ndbOperation The operation that the query is piggy backed on.
   * @param actualParam Instance values for NdbParamOperands.
   */
  virtual void 
  materializeRootOperands(class NdbOperation& ndbOperation,
                          const constVoidPtr actualParam[]) const = 0;

  virtual ~NdbQueryOperationDefImpl() = 0;

protected:
  explicit NdbQueryOperationDefImpl (
                                     const NdbDictionary::Table& table,
                                     const char* ident,
                                     Uint32      ix)
   : m_table(table), m_ident(ident), m_ix(ix),
     m_parents(), m_children(), m_params(),
     m_spjProjection()
  {}

private:
  const NdbDictionary::Table& m_table;
  const char* const m_ident; // Optional name specified by aplication
  const Uint32 m_ix;         // Index of this operation within operation array

  // parent / child vectors contains dependencies as defined
  // with linkedValues
  Vector<NdbQueryOperationDefImpl*> m_parents;
  Vector<NdbQueryOperationDefImpl*> m_children;

  // Params required by this operation
  Vector<const NdbParamOperandImpl*> m_params;

  // Column from this operation required by its child operations
  Vector<const NdbDictionary::Column*> m_spjProjection;
}; // class NdbQueryOperationDefImpl


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

  const NdbDictionary::Column* getColumn() const
  { return m_column; }

  virtual int bindOperand(const NdbDictionary::Column& column,
                          NdbQueryOperationDefImpl& operation)
  { if (m_column  && m_column != &column)
      // Already bounded to a different column
      return QRY_OPERAND_ALREADY_BOUND;
    m_column = &column;
    return 0;
  }
  
  virtual const NdbQueryOperand& getInterface() const = 0; 

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
  const NdbDictionary::Column* m_column;  // Initial NULL, assigned w/ bindOperand()
  /** This is used to tell the type of an NdbQueryOperand. This allow safe
   * downcasting to a subclass.
   */
  const Kind m_kind;
}; // class NdbQueryOperandImpl


class NdbLinkedOperandImpl : public NdbQueryOperandImpl
{
  friend class NdbQueryBuilder;  // Allow privat access from builder interface

public:
  virtual int bindOperand(const NdbDictionary::Column& column,
                          NdbQueryOperationDefImpl& operation);

  const NdbQueryOperationDefImpl& getParentOperation() const
  { return m_parentOperation; }

  // 'LinkedSrc' is index into parent op's spj-projection list where
  // the refered column value is available
  Uint32 getLinkedColumnIx() const
  { return m_parentColumnIx; }

  const NdbDictionary::Column& getParentColumn() const
  { return *m_parentOperation.getSPJProjection()[m_parentColumnIx]; }

  virtual const NdbQueryOperand& getInterface() const
  { return m_interface; }

private:
  virtual ~NdbLinkedOperandImpl() {};

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
  virtual int bindOperand(const NdbDictionary::Column& column,
                          NdbQueryOperationDefImpl& operation);

  const char* getName() const
  { return m_name; }

  Uint32 getParamIx() const
  { return m_paramIx; }

  virtual const NdbQueryOperand& getInterface() const
  { return m_interface; }

private:
  virtual ~NdbParamOperandImpl() {};
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

  virtual int bindOperand(const NdbDictionary::Column& column,
                          NdbQueryOperationDefImpl& operation);

  virtual NdbDictionary::Column::Type getType() const = 0;

  virtual const NdbQueryOperand& getInterface() const
  { return m_interface; }

protected:
  virtual ~NdbConstOperandImpl() {};
  NdbConstOperandImpl ()
    : NdbQueryOperandImpl(Const),
      m_interface(*this)
  {}

private:
  NdbConstOperand m_interface;
}; // class NdbConstOperandImpl


#endif // __cplusplus
#endif
