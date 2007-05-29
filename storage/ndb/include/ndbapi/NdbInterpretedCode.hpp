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

#ifndef NdbInterpretedCode_H
#define NdbInterpretedCode_H

#include <ndb_global.h>
#include <ndb_types.h>
#include "NdbDictionary.hpp"

class NdbTableImpl;
class NdbColumnImpl;


#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
/*
  @brief Stand-alone interpreted programs, for use with NdbRecord

  @details This class is used to prepare an NDB interpreted program for use
  in operations created using NdbRecord.

  The caller supplies the memory buffer to be used for the operations. If
  this is too small, an error will be returned when attempting to add
  instructions. The caller must deallocate the buffer if necessary after
  the InterpretedCode object is destroyed.

  Methods have minimal error checks, for efficiency.

  Note that this is currently considered an internal interface, and subject
  to change without notice.

  ToDo: We should add placeholders to this, so that one can use a single
  InterpretedCode object to create many operations from different embedded
  constants. Once we do this, we could add more error checks, as the cost
  would then not be paid for each operation creation.

*/
class NdbInterpretedCode
{
private:
  friend class NdbOperation;

  static const Uint32 MaxReg= 8;
  Uint32 *m_buffer;
  const Uint32 m_buffer_length;               // In words
  const Uint32 m_number_of_labels;
  /* Number of words used for instructions. */
  Uint32 m_instructions_length;
  /*
    Number of words left in buffer.
    This can be smaller than m_buffer_length-m_instructions_length, as
    the end of the buffer is used to store jump and label addresses for
    back-patching jumps.
  */
  Uint32 m_available_length;
  Uint32 m_number_of_jumps;
  enum Flags {
    /*
      We will set m_got_error if an error occurs, so that we can
      refuse to create an operation from InterpretedCode that the user
      forgot to do error checks on.
    */
    GotError= 0x1,
    /* Set if reading disk column. */
    UsesDisk= 0x2
  };
  Uint32 m_flags;

  int error()
  {
    m_flags|= GotError;
    return -1;
  }

  int add1(Uint32 x1)
  {
    if (unlikely(m_available_length < 1))
      return error();
    Uint32 current = m_instructions_length;
    m_buffer[current    ] = x1;
    m_instructions_length = current + 1;
    m_available_length--;
    return 0;
  }

  int add2(Uint32 x1, Uint32 x2)
  {
    if (unlikely(m_available_length < 2))
      return error();
    Uint32 current = m_instructions_length;
    m_buffer[current    ] = x1;
    m_buffer[current + 1] = x2;
    m_instructions_length = current + 2;
    m_available_length -= 2;
    return 0;
  }

  int add3(Uint32 x1, Uint32 x2, Uint32 x3)
  {
    if (unlikely(m_available_length < 3))
      return error();
    Uint32 current = m_instructions_length;
    m_buffer[current    ] = x1;
    m_buffer[current + 1] = x2;
    m_buffer[current + 2] = x3;
    m_instructions_length = current + 3;
    m_available_length -= 3;
    return 0;
  }

  int add_branch(Uint32 instruction, Uint32 Label);
  int read_attr_impl(const NdbColumnImpl *c, Uint32 RegDest);
  int write_attr_impl(const NdbColumnImpl *c, Uint32 RegSource);
public:
  NdbInterpretedCode(Uint32 *buffer, Uint32 buffer_word_size,
                     Uint32 num_labels);
  int def_label(int tLabelNo);
  int add_reg(Uint32 RegSource1, Uint32 RegSource2, Uint32 RegDest);
  int sub_reg(Uint32 RegSource1, Uint32 RegSource2, Uint32 RegDest);
  int load_const_u32(Uint32 RegDest, Uint32 Constant);
  int load_const_u64(Uint32 RegDest, Uint64 Constant);
  int load_const_null(Uint32 RegDest);
  int read_attr(const NdbDictionary::Table *table, Uint32 attrId,
                Uint32 RegDest);
  int read_attr(const NdbDictionary::Column *column, Uint32 RegDest);
  int write_attr(const NdbDictionary::Table *table, Uint32 attrId,
                 Uint32 RegSource);
  int write_attr(const NdbDictionary::Column *column, Uint32 RegSource);


  int branch_ge(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label);
  int branch_gt(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label);
  int branch_le(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label);
  int branch_lt(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label);
  int branch_eq(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label);
  int branch_ne(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label);
  int branch_ne_null(Uint32 RegLvalue, Uint32 Label);
  int branch_eq_null(Uint32 RegLvalue, Uint32 Label);
  int branch_label(Uint32 Label);
  int interpret_exit_ok();
  int interpret_exit_nok(Uint32 ErrorCode);
  int interpret_exit_nok();
  int interpret_exit_last_row();
  /*
    This must be called after all instructions have been defined, but
    before using the NdbInterpretedCode object, to resolve label adresses
    for all jumps.
  */
  int backpatch();
};
#endif

#endif
