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


#include <Vector.hpp>
#include "NdbQueryBuilder.hpp"
#include "NdbDictionary.hpp"


// Forward declared
class NdbQueryBuilderImpl;
class NdbQueryOperationDefImpl;
class NdbParamOperandImpl;
class NdbConstOperandImpl;
class NdbLinkedOperandImpl;

/** An extensible array, used for holding serialized representaions of
 * queries and parameters.*/
class Uint32Buffer{
public:
  Uint32Buffer():m_size(0){
  }
  /** Get the i'th element. Elements are not guaranteed to be contigious in 
   *  memory, so doing (&get(i))[j] would be wrong.
   */
  Uint32& get(Uint32 i){
    assert(i<300);
    if(i>=m_size){
      m_size = i+1;
    }
    return m_array[i];
  }
  const Uint32& get(Uint32 i)const{
    assert(i<m_size);
    return m_array[i];
  }
  Uint32 getSize() const {
    return m_size;
  }
private:
  /* TODO: replace array with something that can grow and allocate from a 
   * pool as needed..*/
  Uint32 m_array[300];
  Uint32 m_size;
};

/** A reference to a subset of an Uint32Buffer.
*/
class Uint32Slice{
public:
  explicit Uint32Slice(Uint32Buffer& buffer, Uint32 offset):
    m_buffer(buffer),
    m_offset(offset){
  }
  explicit Uint32Slice(Uint32Slice& slice, Uint32 offset):
    m_buffer(slice.m_buffer),
    m_offset(offset+slice.m_offset){
  }
  Uint32& get(int i){
    return m_buffer.get(i+m_offset);
  }
  const Uint32& get(int i)const{
    return m_buffer.get(i+m_offset);
  }
  Uint32 getOffset() const {return m_offset;}
  Uint32 getSize() const {return m_buffer.getSize() - m_offset;}
private:
  Uint32Buffer& m_buffer;
  const Uint32 m_offset;
};



class NdbQueryDefImpl
{
  friend class NdbQueryDef;

public:
  NdbQueryDefImpl(const NdbQueryBuilderImpl& builder);
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
//Vector<NdbParamOperand*> m_paramOperand;
//Vector<NdbConstOperand*> m_constOperand;
//Vector<NdbLinkedOperand*> m_linkedOperand;
  Uint32Buffer m_serializedDef; 
}; // class NdbQueryDefImpl

// For debuggging.
#define TRACE_SERIALIZATION

class NdbQueryBuilderImpl
{
  friend class NdbQueryBuilder;
  friend NdbQueryDefImpl::NdbQueryDefImpl(const NdbQueryBuilderImpl& builder);

public:
  ~NdbQueryBuilderImpl();
  NdbQueryBuilderImpl(Ndb& ndb);

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
  Vector<NdbParamOperandImpl*> m_paramOperands;
  Vector<NdbConstOperandImpl*> m_constOperands;
  Vector<NdbLinkedOperandImpl*> m_linkedOperands;

}; // class NdbQueryBuilderImpl



class NdbQueryOperationDefImpl
{
public:
  // Get the ordinal position of this operation within the query
  Uint32 getQueryOperationIx() const
  { return m_ix; };

  Uint32 getNoOfParentOperations() const
  { return m_parents.size(); };

  const NdbQueryOperationDefImpl& getParentOperation(Uint32 i) const
  { return *m_parents[i]; };

  Uint32 getNoOfChildOperations() const
  { return m_children.size(); };

  const NdbQueryOperationDefImpl& getChildOperation(Uint32 i) const
  { return *m_children[i]; };

  const NdbDictionary::Table& getTable() const
  { return m_table; };

  const char* getName() const
  { return m_ident; };

  // Register a operation as parent of this operation
  void addParent(NdbQueryOperationDefImpl*);

  // Register a linked child refering this operation
  void addChild(NdbQueryOperationDefImpl*);

  // Register a linked reference to a column from operation
  Uint32 addColumnRef(const NdbDictionary::Column*);

  virtual const NdbQueryOperationDef& getInterface() const = 0; 

  /** Make a serialized representation of this operation, corresponding to
   * the struct QueryNode type.
   */
  virtual void serializeOperation(Uint32Buffer& serializedTree) const = 0;

  /** Find the projection that should be sent to the SPJ block. This should
   * contain the attributes needed to instantiate all child operations.
   */
  const Vector<const NdbDictionary::Column*>& getSPJProjection() const{
    return m_spjProjection;
  }

protected:
  virtual ~NdbQueryOperationDefImpl() = 0;
  friend NdbQueryBuilderImpl::~NdbQueryBuilderImpl();
  friend NdbQueryDefImpl::~NdbQueryDefImpl();

  NdbQueryOperationDefImpl (
                           const NdbDictionary::Table& table,
                           const char* ident,
                           Uint32      ix)
   : m_table(table), m_ident(ident), m_ix(ix),
     m_parents(), m_children(),
     m_spjProjection()
 {};
  
private:
  const NdbDictionary::Table& m_table;
  const char* const m_ident; // Optional name specified by aplication
  const Uint32 m_ix;         // Index if this operation within operation array

  // parent / child vectors contains dependencies as defined
  // with linkedValues
  Vector<NdbQueryOperationDefImpl*> m_parents;
  Vector<NdbQueryOperationDefImpl*> m_children;
  Vector<const NdbDictionary::Column*> m_spjProjection;
}; // class NdbQueryOperationDefImpl


#endif
