/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <NdbScanFilter.hpp>
#include <NdbOperation.hpp>
#include "NdbDictionaryImpl.hpp"
#include <Vector.hpp>
#include <NdbOut.hpp>
#include <Interpreter.hpp>

#ifdef VM_TRACE
#include <NdbEnv.h>
#define INT_DEBUG(x) \
  { const char* tmp = NdbEnv_GetEnv("INT_DEBUG", (char*)0, 0); \
  if (tmp != 0 && strlen(tmp) != 0) { ndbout << "INT:"; ndbout_c x; } }
#else
#define INT_DEBUG(x)
#endif

class NdbScanFilterImpl {
public:
  struct State {
    NdbScanFilter::Group m_group;
    Uint32 m_popCount;
    Uint32 m_ownLabel;
    Uint32 m_trueLabel;
    Uint32 m_falseLabel;
  };

  int m_label;
  State m_current;
  Vector<State> m_stack;
  NdbOperation * m_operation;
  Uint32 m_latestAttrib;

  int cond_col(Interpreter::UnaryCondition, Uint32 attrId);
  
  template<typename T>
  int cond_col_const(Interpreter::BinaryCondition, Uint32 attrId, T value);

  int cond_col_const(Interpreter::BinaryCondition, Uint32 attrId, 
		     const char * value, Uint32 len, bool nopad);
};

const Uint32 LabelExit = ~0;


NdbScanFilter::NdbScanFilter(class NdbOperation * op)
  : m_impl(* new NdbScanFilterImpl())
{
  m_impl.m_current.m_group = (NdbScanFilter::Group)0;
  m_impl.m_current.m_popCount = 0;
  m_impl.m_current.m_ownLabel = 0;
  m_impl.m_current.m_trueLabel = ~0;
  m_impl.m_current.m_falseLabel = ~0;
  m_impl.m_label = 0;
  m_impl.m_latestAttrib = ~0;
  m_impl.m_operation = op;
}

NdbScanFilter::~NdbScanFilter(){
  delete &m_impl;
}
 
int
NdbScanFilter::begin(Group group){

  switch(group){
  case NdbScanFilter::AND:
    INT_DEBUG(("Begin(AND)"));
    break;
  case NdbScanFilter::OR:
    INT_DEBUG(("Begin(OR)"));
    break;
  case NdbScanFilter::NAND:
    INT_DEBUG(("Begin(NAND)"));
    break;
  case NdbScanFilter::NOR:
    INT_DEBUG(("Begin(NOR)"));
    break;
  }

  if(group == m_impl.m_current.m_group){
    switch(group){
    case NdbScanFilter::AND:
    case NdbScanFilter::OR:
      m_impl.m_current.m_popCount++;
      return 0;
    case NdbScanFilter::NOR:
    case NdbScanFilter::NAND:
      break;
    }
  }

  NdbScanFilterImpl::State tmp = m_impl.m_current;
  m_impl.m_stack.push_back(m_impl.m_current);
  m_impl.m_current.m_group = group;
  m_impl.m_current.m_ownLabel = m_impl.m_label++;
  m_impl.m_current.m_popCount = 0;
  
  switch(group){
  case NdbScanFilter::AND:
  case NdbScanFilter::NAND:
    m_impl.m_current.m_falseLabel = m_impl.m_current.m_ownLabel;
    m_impl.m_current.m_trueLabel = tmp.m_trueLabel;
    break;
  case NdbScanFilter::OR:
  case NdbScanFilter::NOR:
    m_impl.m_current.m_falseLabel = tmp.m_falseLabel;
    m_impl.m_current.m_trueLabel = m_impl.m_current.m_ownLabel;
    break;
  default: 
    m_impl.m_operation->setErrorCodeAbort(4260);
    return -1;
  }
  
  return 0;
}

int
NdbScanFilter::end(){

  switch(m_impl.m_current.m_group){
  case NdbScanFilter::AND:
    INT_DEBUG(("End(AND pc=%d)", m_impl.m_current.m_popCount));
    break;
  case NdbScanFilter::OR:
    INT_DEBUG(("End(OR pc=%d)", m_impl.m_current.m_popCount));
    break;
  case NdbScanFilter::NAND:
    INT_DEBUG(("End(NAND pc=%d)", m_impl.m_current.m_popCount));
    break;
  case NdbScanFilter::NOR:
    INT_DEBUG(("End(NOR pc=%d)", m_impl.m_current.m_popCount));
    break;
  }

  if(m_impl.m_current.m_popCount > 0){
    m_impl.m_current.m_popCount--;
    return 0;
  }
  
  NdbScanFilterImpl::State tmp = m_impl.m_current;  
  m_impl.m_current = m_impl.m_stack.back();
  m_impl.m_stack.erase(m_impl.m_stack.size() - 1);
  
  switch(tmp.m_group){
  case NdbScanFilter::AND:
    if(tmp.m_trueLabel == (Uint32)~0){
      m_impl.m_operation->interpret_exit_ok();
    } else {
      m_impl.m_operation->branch_label(tmp.m_trueLabel);
    }
    break;
  case NdbScanFilter::NAND:
    if(tmp.m_trueLabel == (Uint32)~0){
      m_impl.m_operation->interpret_exit_nok();
    } else {
      m_impl.m_operation->branch_label(tmp.m_falseLabel);
    }
    break;
  case NdbScanFilter::OR:
    if(tmp.m_falseLabel == (Uint32)~0){
      m_impl.m_operation->interpret_exit_nok();
    } else {
      m_impl.m_operation->branch_label(tmp.m_falseLabel);
    }
    break;
  case NdbScanFilter::NOR:
    if(tmp.m_falseLabel == (Uint32)~0){
      m_impl.m_operation->interpret_exit_ok();
    } else {
      m_impl.m_operation->branch_label(tmp.m_trueLabel);
    }
    break;
  default:
    m_impl.m_operation->setErrorCodeAbort(4260);
    return -1;
  }

  m_impl.m_operation->def_label(tmp.m_ownLabel);

  if(m_impl.m_stack.size() == 0){
    switch(tmp.m_group){
    case NdbScanFilter::AND:
    case NdbScanFilter::NOR:
      m_impl.m_operation->interpret_exit_nok();
      break;
    case NdbScanFilter::OR:
    case NdbScanFilter::NAND:
      m_impl.m_operation->interpret_exit_ok();
      break;
    default:
      m_impl.m_operation->setErrorCodeAbort(4260);
      return -1;
    }
  }
  
  return 0;
}

int
NdbScanFilter::istrue(){
  if(m_impl.m_current.m_group < NdbScanFilter::AND || 
     m_impl.m_current.m_group > NdbScanFilter::NOR){
    m_impl.m_operation->setErrorCodeAbort(4260);
    return -1;
  }

  if(m_impl.m_current.m_trueLabel == (Uint32)~0){
    return m_impl.m_operation->interpret_exit_ok();
  } else {
    return m_impl.m_operation->branch_label(m_impl.m_current.m_trueLabel);
  }
}

int
NdbScanFilter::isfalse(){
  if(m_impl.m_current.m_group < NdbScanFilter::AND || 
     m_impl.m_current.m_group > NdbScanFilter::NOR){
    m_impl.m_operation->setErrorCodeAbort(4260);
    return -1;
  }
  
  if(m_impl.m_current.m_falseLabel == (Uint32)~0){
    return m_impl.m_operation->interpret_exit_nok();
  } else {
    return m_impl.m_operation->branch_label(m_impl.m_current.m_falseLabel);
  }
}


#define action(x, y, z)


typedef int (NdbOperation:: * Branch1)(Uint32, Uint32 label);
typedef int (NdbOperation:: * Branch2)(Uint32, Uint32, Uint32 label);
typedef int (NdbOperation:: * StrBranch2)(Uint32, const char*,Uint32,bool,Uint32);

struct tab {
  Branch2 m_branches[5];
};

static const tab table[] = {
  /**
   * EQ (AND, OR, NAND, NOR)
   */
  { { 0, 
      &NdbOperation::branch_ne, 
      &NdbOperation::branch_eq, 
      &NdbOperation::branch_eq,  
      &NdbOperation::branch_ne } }
  
  /**
   * NEQ
   */
  ,{ { 0, 
       &NdbOperation::branch_eq, 
       &NdbOperation::branch_ne, 
       &NdbOperation::branch_ne, 
       &NdbOperation::branch_eq } }
  
  /**
   * LT
   */
  ,{ { 0, 
       &NdbOperation::branch_le, 
       &NdbOperation::branch_gt, 
       &NdbOperation::branch_gt,
       &NdbOperation::branch_le } }
  
  /**
   * LE
   */
  ,{ { 0, 
       &NdbOperation::branch_lt, 
       &NdbOperation::branch_ge, 
       &NdbOperation::branch_ge, 
       &NdbOperation::branch_lt } }
  
  /**
   * GT
   */
  ,{ { 0, 
       &NdbOperation::branch_ge, 
       &NdbOperation::branch_lt, 
       &NdbOperation::branch_lt, 
       &NdbOperation::branch_ge } }

  /**
   * GE
   */
  ,{ { 0, 
       &NdbOperation::branch_gt, 
       &NdbOperation::branch_le, 
       &NdbOperation::branch_le, 
       &NdbOperation::branch_gt } }
};

struct tab2 {
  Branch1 m_branches[5];
};

static const tab2 table2[] = {
  /**
   * IS NULL
   */
  { { 0, 
      &NdbOperation::branch_col_ne_null, 
      &NdbOperation::branch_col_eq_null, 
      &NdbOperation::branch_col_eq_null,  
      &NdbOperation::branch_col_ne_null } }
  
  /**
   * IS NOT NULL
   */
  ,{ { 0, 
       &NdbOperation::branch_col_eq_null, 
       &NdbOperation::branch_col_ne_null, 
       &NdbOperation::branch_col_ne_null,  
       &NdbOperation::branch_col_eq_null } }
};

const int tab_sz = sizeof(table)/sizeof(table[0]);
const int tab2_sz = sizeof(table2)/sizeof(table2[0]);

int
matchType(const NdbDictionary::Column * col){
  return 1;
}

template<typename T> int load_const(NdbOperation* op, T value, Uint32 reg);

template<>
int
load_const(NdbOperation* op, Uint32 value, Uint32 reg){
  return op->load_const_u32(reg, value);
}

template<>
int
load_const(NdbOperation* op, Uint64 value, Uint32 reg){
  return op->load_const_u64(reg, value);
}

template<typename T>
int
NdbScanFilterImpl::cond_col_const(Interpreter::BinaryCondition op, 
				  Uint32 AttrId, T value){
  
  if(op < 0 || op >= tab_sz){
    m_operation->setErrorCodeAbort(4262);
    return -1;
  }

  if(m_current.m_group < NdbScanFilter::AND || 
     m_current.m_group > NdbScanFilter::NOR){
    m_operation->setErrorCodeAbort(4260);
    return -1;
  }

  Branch2 branch = table[op].m_branches[m_current.m_group];
  const NdbDictionary::Column * col = 
    m_operation->m_currentTable->getColumn(AttrId);
  
  if(col == 0){
    m_operation->setErrorCodeAbort(4261);
    return -1;
  }
  
  if(!matchType(col)){
    /**
     * Code not reached
     */
    return -1;
  }

  if(m_latestAttrib != AttrId){
    m_operation->read_attr(&NdbColumnImpl::getImpl(* col), 4);
    m_latestAttrib = AttrId;
  }
  
  load_const<T>(m_operation, value, 5);
  (m_operation->* branch)(4, 5, m_current.m_ownLabel);

  return 0;
};

int
NdbScanFilter::eq(int AttrId, Uint32 value){
  return m_impl.cond_col_const(Interpreter::EQ, AttrId, value);
}

int
NdbScanFilter::ne(int AttrId, Uint32 value){
  return m_impl.cond_col_const(Interpreter::NE, AttrId, value);
}

int
NdbScanFilter::lt(int AttrId, Uint32 value){
  return m_impl.cond_col_const(Interpreter::LT, AttrId, value);
}

int
NdbScanFilter::le(int AttrId, Uint32 value){
  return m_impl.cond_col_const(Interpreter::LE, AttrId, value);
}

int
NdbScanFilter::gt(int AttrId, Uint32 value){
  return m_impl.cond_col_const(Interpreter::GT, AttrId, value);
}

int
NdbScanFilter::ge(int AttrId, Uint32 value){
  return m_impl.cond_col_const(Interpreter::GE, AttrId, value);
}


int
NdbScanFilter::eq(int AttrId, Uint64 value){
  return m_impl.cond_col_const(Interpreter::EQ, AttrId, value);
}

int
NdbScanFilter::ne(int AttrId, Uint64 value){
  return m_impl.cond_col_const(Interpreter::NE, AttrId, value);
}

int
NdbScanFilter::lt(int AttrId, Uint64 value){
  return m_impl.cond_col_const(Interpreter::LT, AttrId, value);
}

int
NdbScanFilter::le(int AttrId, Uint64 value){
  return m_impl.cond_col_const(Interpreter::LE, AttrId, value);
}

int
NdbScanFilter::gt(int AttrId, Uint64 value){
  return m_impl.cond_col_const(Interpreter::GT, AttrId, value);
}

int
NdbScanFilter::ge(int AttrId, Uint64 value){
  return m_impl.cond_col_const(Interpreter::GE, AttrId, value);
}


int
NdbScanFilterImpl::cond_col(Interpreter::UnaryCondition op, Uint32 AttrId){
  
  if(op < 0 || op >= tab2_sz){
    m_operation->setErrorCodeAbort(4262);
    return -1;
  }
  
  if(m_current.m_group < NdbScanFilter::AND || 
     m_current.m_group > NdbScanFilter::NOR){
    m_operation->setErrorCodeAbort(4260);
    return -1;
  }
  
  Branch1 branch = table2[op].m_branches[m_current.m_group];
  (m_operation->* branch)(AttrId, m_current.m_ownLabel);
  return 0;
};

int
NdbScanFilter::isnull(int AttrId){
  return m_impl.cond_col(Interpreter::IS_NULL, AttrId);
}

int
NdbScanFilter::isnotnull(int AttrId){
  return m_impl.cond_col(Interpreter::IS_NOT_NULL, AttrId);
}

struct tab3 {
  StrBranch2 m_branches[5];
};

static const tab3 table3[] = {
  /**
   * EQ (AND, OR, NAND, NOR)
   */
  { { 0, 
      &NdbOperation::branch_col_ne, 
      &NdbOperation::branch_col_eq, 
      &NdbOperation::branch_col_ne,  
      &NdbOperation::branch_col_eq } }
  
  /**
   * NEQ
   */
  ,{ { 0, 
       &NdbOperation::branch_col_eq, 
       &NdbOperation::branch_col_ne, 
       &NdbOperation::branch_col_eq, 
       &NdbOperation::branch_col_ne } }
  
  /**
   * LT
   */
  ,{ { 0, 
       &NdbOperation::branch_col_le, 
       &NdbOperation::branch_col_gt, 
       &NdbOperation::branch_col_le,
       &NdbOperation::branch_col_gt } }
  
  /**
   * LE
   */
  ,{ { 0, 
       &NdbOperation::branch_col_lt, 
       &NdbOperation::branch_col_ge, 
       &NdbOperation::branch_col_lt, 
       &NdbOperation::branch_col_ge } }
  
  /**
   * GT
   */
  ,{ { 0, 
       &NdbOperation::branch_col_ge, 
       &NdbOperation::branch_col_lt, 
       &NdbOperation::branch_col_ge, 
       &NdbOperation::branch_col_lt } }

  /**
   * GE
   */
  ,{ { 0, 
       &NdbOperation::branch_col_gt, 
       &NdbOperation::branch_col_le, 
       &NdbOperation::branch_col_gt, 
       &NdbOperation::branch_col_le } }

  /**
   * LIKE
   */
  ,{ { 0, 
       &NdbOperation::branch_col_notlike, 
       &NdbOperation::branch_col_like, 
       &NdbOperation::branch_col_notlike, 
       &NdbOperation::branch_col_like } }

  /**
   * NOT LIKE
   */
  ,{ { 0, 
       &NdbOperation::branch_col_like, 
       &NdbOperation::branch_col_notlike, 
       &NdbOperation::branch_col_like, 
       &NdbOperation::branch_col_notlike } }
};

const int tab3_sz = sizeof(table3)/sizeof(table3[0]);


int
NdbScanFilterImpl::cond_col_const(Interpreter::BinaryCondition op, 
				  Uint32 AttrId, 
				  const char * value, Uint32 len, bool nopad){
  if(op < 0 || op >= tab3_sz){
    m_operation->setErrorCodeAbort(4260);
    return -1;
  }
  
  if(m_current.m_group < NdbScanFilter::AND || 
     m_current.m_group > NdbScanFilter::NOR){
    m_operation->setErrorCodeAbort(4260);
    return -1;
  }
  
  StrBranch2 branch = table3[op].m_branches[m_current.m_group];
  const NdbDictionary::Column * col = 
    m_operation->m_currentTable->getColumn(AttrId);
  
  if(col == 0){
    m_operation->setErrorCodeAbort(4261);
    return -1;
  }
  
  (m_operation->* branch)(AttrId, value, len, nopad, m_current.m_ownLabel);
  return 0;
}

int
NdbScanFilter::eq(int ColId, const char * val, Uint32 len, bool nopad){
  return m_impl.cond_col_const(Interpreter::EQ, ColId, val, len, nopad);
}

int
NdbScanFilter::ne(int ColId, const char * val, Uint32 len, bool nopad){
  return m_impl.cond_col_const(Interpreter::NE, ColId, val, len, nopad);
}

int
NdbScanFilter::lt(int ColId, const char * val, Uint32 len, bool nopad){
  return m_impl.cond_col_const(Interpreter::LT, ColId, val, len, nopad);
}

int
NdbScanFilter::le(int ColId, const char * val, Uint32 len, bool nopad){
  return m_impl.cond_col_const(Interpreter::LE, ColId, val, len, nopad);
}

int
NdbScanFilter::gt(int ColId, const char * val, Uint32 len, bool nopad){
  return m_impl.cond_col_const(Interpreter::GT, ColId, val, len, nopad);
}

int
NdbScanFilter::ge(int ColId, const char * val, Uint32 len, bool nopad){
  return m_impl.cond_col_const(Interpreter::GE, ColId, val, len, nopad);
}

int
NdbScanFilter::like(int ColId, const char * val, Uint32 len, bool nopad){
  return m_impl.cond_col_const(Interpreter::LIKE, ColId, val, len, nopad);
}

int
NdbScanFilter::notlike(int ColId, const char * val, Uint32 len, bool nopad){
  return m_impl.cond_col_const(Interpreter::NOT_LIKE, ColId, val, len, nopad);
}

#if 0
int
main(void){
  if(0)
  {
    ndbout << "a > 7 AND b < 9 AND c = 4" << endl;
    NdbScanFilter f(0);
    f.begin(NdbScanFilter::AND);
    f.gt(0, 7);
    f.lt(1, 9);
    f.eq(2, 4);
    f.end();
    ndbout << endl;
  }

  if(0)
  {
    ndbout << "a > 7 OR b < 9 OR c = 4" << endl;
    NdbScanFilter f(0);
    f.begin(NdbScanFilter::OR);
    f.gt(0, 7);
    f.lt(1, 9);
    f.eq(2, 4);
    f.end();
    ndbout << endl;
  }

  if(0)
  {
    ndbout << "a > 7 AND (b < 9 OR c = 4)" << endl;
    NdbScanFilter f(0);
    f.begin(NdbScanFilter::AND);
    f.gt(0, 7);
    f.begin(NdbScanFilter::OR);
    f.lt(1, 9);
    f.eq(2, 4);
    f.end();
    f.end();
    ndbout << endl;
  }

  if(0)
  {
    ndbout << "a > 7 AND (b < 9 AND c = 4)" << endl;
    NdbScanFilter f(0);
    f.begin(NdbScanFilter::AND);
    f.gt(0, 7);
    f.begin(NdbScanFilter::AND);
    f.lt(1, 9);
    f.eq(2, 4);
    f.end();
    f.end();
    ndbout << endl;
  }

  if(0)
  {
    ndbout << "(a > 7 AND b < 9) AND c = 4" << endl;
    NdbScanFilter f(0);
    f.begin(NdbScanFilter::AND);
    f.begin(NdbScanFilter::AND);
    f.gt(0, 7);
    f.lt(1, 9);
    f.end();
    f.eq(2, 4);
    f.end();
    ndbout << endl;
  }

  if(1)
  {
    ndbout << "(a > 7 OR b < 9) AND (c = 4 OR c = 5)" << endl;
    NdbScanFilter f(0);
    f.begin(NdbScanFilter::AND);
    f.begin(NdbScanFilter::OR);
    f.gt(0, 7);
    f.lt(1, 9);
    f.end();
    f.begin(NdbScanFilter::OR);    
    f.eq(2, 4);
    f.eq(2, 5);
    f.end();
    f.end();
    ndbout << endl;
  }

  if(1)
  {
    ndbout << "(a > 7 AND b < 9) OR (c = 4 AND c = 5)" << endl;
    NdbScanFilter f(0);
    f.begin(NdbScanFilter::OR);
    f.begin(NdbScanFilter::AND);
    f.gt(0, 7);
    f.lt(1, 9);
    f.end();
    f.begin(NdbScanFilter::AND);    
    f.eq(2, 4);
    f.eq(2, 5);
    f.end();
    f.end();
    ndbout << endl;
  }

  if(1)
  {
    ndbout << 
      "((a > 7 AND b < 9) OR (c = 4 AND d = 5)) AND " 
      "((e > 6 AND f < 8) OR (g = 2 AND h = 3)) "  << endl;
    NdbScanFilter f(0);
    f.begin(NdbScanFilter::AND);
    f.begin(NdbScanFilter::OR);
    f.begin(NdbScanFilter::AND);
    f.gt(0, 7);
    f.lt(1, 9);
    f.end();
    f.begin(NdbScanFilter::AND);    
    f.eq(2, 4);
    f.eq(3, 5);
    f.end();
    f.end();

    f.begin(NdbScanFilter::OR);
    f.begin(NdbScanFilter::AND);
    f.gt(4, 6);
    f.lt(5, 8);
    f.end();
    f.begin(NdbScanFilter::AND);    
    f.eq(6, 2);
    f.eq(7, 3);
    f.end();
    f.end();
    f.end();
  }
  
  return 0;
}
#endif

template class Vector<NdbScanFilterImpl::State>;
#if __SUNPRO_CC != 0x560
template int NdbScanFilterImpl::cond_col_const(Interpreter::BinaryCondition, Uint32 attrId, Uint32);
template int NdbScanFilterImpl::cond_col_const(Interpreter::BinaryCondition, Uint32 attrId, Uint64);
#endif

