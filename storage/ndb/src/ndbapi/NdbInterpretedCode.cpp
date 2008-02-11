/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "NdbInterpretedCode.hpp"
#include "Interpreter.hpp"
#include "NdbDictionaryImpl.hpp"


int NdbInterpretedCode::add_branch(Uint32 instruction, Uint32 Label)
{
  /* NOTE: subroutines not supported yet. */
  /* Store the jump location for backpatching. */
  if (m_available_length < 2 ||
      m_instructions_length > 0xffff ||
      Label > 0xffff)
    return error();
  m_number_of_jumps++;
  m_available_length--;
  Uint32 pos= m_buffer_length - (m_number_of_labels + m_number_of_jumps);
  m_buffer[pos]= m_instructions_length | (Label << 16);
  return add1(instruction);
}

NdbInterpretedCode::NdbInterpretedCode(Uint32 *buffer, Uint32 buffer_word_size,
                                       Uint32 num_labels) :
  m_buffer(buffer),
  m_buffer_length(buffer_word_size),
  m_number_of_labels(num_labels),
  m_instructions_length(0),
  m_available_length(m_buffer_length),
  m_number_of_jumps(0),
  m_flags(0)
{
  Uint32 i;
  /*
    At the end of the buffer we store the address of all labels as they are
    defined, so that we can back-patch forward jumps later.
    If the supplied buffer is too small, we cannot fail in the constructor,
    but all attempts to add instructions will fail due to full buffer.
  */
  for (i = 0; i < num_labels && i < buffer_word_size; i++)
  {
    buffer[buffer_word_size - (i + 1)] = ~(Uint32)0;
    m_available_length--;
  }
}

int
NdbInterpretedCode::def_label(int tLabelNo)
{
  if (tLabelNo < 0 ||
      (Uint32)tLabelNo >= m_number_of_labels)
    return error();
  Uint32 pos= m_buffer_length - (tLabelNo + 1);
  /* Check label not already defined. */
  if (m_buffer[pos] != ~(Uint32)0)
    return error();
  m_buffer[pos]= m_instructions_length;
  return 0;
}

int
NdbInterpretedCode::add_reg(Uint32 RegSource1, Uint32 RegSource2,
                            Uint32 RegDest)
{
  return add1(Interpreter::Add(RegDest % MaxReg, RegSource1 % MaxReg,
                               RegSource2 % MaxReg));
}

int
NdbInterpretedCode::sub_reg(Uint32 RegSource1, Uint32 RegSource2,
                            Uint32 RegDest)
{
  return add1(Interpreter::Sub(RegDest % MaxReg, RegSource1 % MaxReg,
                               RegSource2 % MaxReg));
}

int
NdbInterpretedCode::load_const_u32(Uint32 RegDest, Uint32 Constant)
{
  return add2(Interpreter::LoadConst32(RegDest % MaxReg), Constant);
}

int
NdbInterpretedCode::load_const_u64(Uint32 RegDest, Uint64 Constant)
{
  const Uint32* p= (const Uint32 *)(&Constant);
  return add3(Interpreter::LoadConst64(RegDest % MaxReg), p[0], p[1]);
}

int
NdbInterpretedCode::load_const_null(Uint32 RegDest)
{
  return add1(Interpreter::LoadNull(RegDest % MaxReg));
}

int
NdbInterpretedCode::read_attr_impl(const NdbColumnImpl *c, Uint32 RegDest)
{
  if (c->m_storageType == NDB_STORAGETYPE_DISK)
    m_flags|= UsesDisk;
  return add1(Interpreter::Read(c->m_attrId, RegDest % MaxReg));
}

int
NdbInterpretedCode::read_attr(const NdbDictionary::Table *table,
                              Uint32 attrId, Uint32 RegDest)
{
  const NdbTableImpl *t= & NdbTableImpl::getImpl(*table);
  const NdbColumnImpl *c= t->getColumn(attrId);
  if (unlikely(c == NULL))
    return error();
  return read_attr_impl(c, RegDest);
}

int
NdbInterpretedCode::read_attr(const NdbDictionary::Column *column,
                              Uint32 RegDest)
{
  return read_attr_impl(&NdbColumnImpl::getImpl(*column), RegDest);
}

int
NdbInterpretedCode::write_attr_impl(const NdbColumnImpl *c, Uint32 RegSource)
{
  if (c->m_storageType == NDB_STORAGETYPE_DISK)
    m_flags|= UsesDisk;
  return add1(Interpreter::Write(c->m_attrId, RegSource % MaxReg));
}

int
NdbInterpretedCode::write_attr(const NdbDictionary::Table *table,
                               Uint32 attrId, Uint32 RegSource)
{
  const NdbTableImpl *t= & NdbTableImpl::getImpl(*table);
  const NdbColumnImpl *c= t->getColumn(attrId);
  if (unlikely(c == NULL))
    return error();
  return write_attr_impl(c, RegSource);
}

int
NdbInterpretedCode::write_attr(const NdbDictionary::Column *column,
                               Uint32 RegSource)
{
  return write_attr_impl(&NdbColumnImpl::getImpl(*column), RegSource);
}

int
NdbInterpretedCode::branch_ge(Uint32 RegLvalue, Uint32 RegRvalue,
                              Uint32 Label)
{
  Uint32 instr = Interpreter::Branch(Interpreter::BRANCH_GE_REG_REG,
                                     RegLvalue, RegRvalue);
  return add_branch(instr, Label);
}

int
NdbInterpretedCode::branch_gt(Uint32 RegLvalue, Uint32 RegRvalue,
                              Uint32 Label)
{
  Uint32 instr = Interpreter::Branch(Interpreter::BRANCH_GT_REG_REG,
                                     RegLvalue, RegRvalue);
  return add_branch(instr, Label);
}

int
NdbInterpretedCode::branch_le(Uint32 RegLvalue, Uint32 RegRvalue,
                              Uint32 Label)
{
  Uint32 instr = Interpreter::Branch(Interpreter::BRANCH_LE_REG_REG,
                                     RegLvalue, RegRvalue);
  return add_branch(instr, Label);
}

int
NdbInterpretedCode::branch_lt(Uint32 RegLvalue, Uint32 RegRvalue,
                              Uint32 Label)
{
  Uint32 instr = Interpreter::Branch(Interpreter::BRANCH_LT_REG_REG,
                                     RegLvalue, RegRvalue);
  return add_branch(instr, Label);
}

int
NdbInterpretedCode::branch_eq(Uint32 RegLvalue, Uint32 RegRvalue,
                              Uint32 Label)
{
  Uint32 instr = Interpreter::Branch(Interpreter::BRANCH_EQ_REG_REG,
                                     RegLvalue, RegRvalue);
  return add_branch(instr, Label);
}

int
NdbInterpretedCode::branch_ne(Uint32 RegLvalue, Uint32 RegRvalue,
                              Uint32 Label)
{
  Uint32 instr = Interpreter::Branch(Interpreter::BRANCH_NE_REG_REG,
                                     RegLvalue, RegRvalue);
  return add_branch(instr, Label);
}

int
NdbInterpretedCode::branch_ne_null(Uint32 RegLvalue, Uint32 Label)
{
  return add_branch
    (((RegLvalue % MaxReg) << 6) | Interpreter::BRANCH_REG_NE_NULL, Label);
}

int
NdbInterpretedCode::branch_eq_null(Uint32 RegLvalue, Uint32 Label)
{
  return add_branch
    (((RegLvalue % MaxReg) << 6) | Interpreter::BRANCH_REG_EQ_NULL, Label);
}

int
NdbInterpretedCode::branch_label(Uint32 Label)
{
  return add_branch(Interpreter::BRANCH, Label);
}

int
NdbInterpretedCode::interpret_exit_ok()
{
  return add1(Interpreter::EXIT_OK);
}

int
NdbInterpretedCode::interpret_exit_nok(Uint32 ErrorCode)
{
  return add1((ErrorCode << 16) | Interpreter::EXIT_REFUSE);
}

int
NdbInterpretedCode::interpret_exit_nok()
{
  return add1((899 << 16) | Interpreter::EXIT_REFUSE);
}

int
NdbInterpretedCode::interpret_exit_last_row()
{
  return add1(Interpreter::EXIT_OK_LAST);
}

int
NdbInterpretedCode::backpatch()
{
  Uint32 i;
  Uint32 jumplist_end = m_buffer_length - m_number_of_labels;
  Uint32 jumplist_start = jumplist_end - m_number_of_jumps;

  for (i = jumplist_start; i < jumplist_end; i++)
  {
    Uint32 value = m_buffer[i];
    Uint32 instruction_address = value & 0xffff;
    Uint32 label_idx = value >> 16;
    assert(label_idx < m_number_of_labels);
    assert(instruction_address < m_instructions_length);
    Uint32 label_address = m_buffer[m_buffer_length - (label_idx + 1)];
    /* Check for jump to undefined label. */
    if (label_address == ~(Uint32)0)
      return error();
    /* Check if backwards or forwards jump. */
    if (label_address < instruction_address)
      m_buffer[instruction_address] |=
        (((instruction_address - label_address) << 16) | ((Uint32)1 << 31));
    else
      m_buffer[instruction_address] |=
        ((label_address - instruction_address) << 16);
  }
  return 0;
}
