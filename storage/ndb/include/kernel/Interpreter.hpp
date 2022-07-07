/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_INTERPRETER_HPP
#define NDB_INTERPRETER_HPP

#include <ndb_types.h>
#include <EventLogger.hpp>

#define JAM_FILE_ID 215


class Interpreter {
public:

  inline static Uint32 mod4(Uint32 len){
    return len + ((4 - (len & 3)) & 3);
  }
  

  /**
   * General Mnemonic format
   *
   * i = Instruction            -  5 Bits ( 0 - 5 ) max 63
   * x = Register 1             -  3 Bits ( 6 - 8 ) max 7
   * y = Register 2             -  3 Bits ( 9 -11 ) max 7
   * b = Branch offset (only branches)
   *
   *           1111111111222222222233
   * 01234567890123456789012345678901
   * iiiiiixxxyyy    bbbbbbbbbbbbbbbb
   *
   *
   */

  /**
   * Instructions
   */
  static constexpr Uint32 READ_ATTR_INTO_REG = 1;
  static constexpr Uint32 WRITE_ATTR_FROM_REG = 2;
  static constexpr Uint32 LOAD_CONST_NULL = 3;
  static constexpr Uint32 LOAD_CONST16 = 4;
  static constexpr Uint32 LOAD_CONST32 = 5;
  static constexpr Uint32 LOAD_CONST64 = 6;
  static constexpr Uint32 ADD_REG_REG = 7;
  static constexpr Uint32 SUB_REG_REG = 8;
  static constexpr Uint32 BRANCH = 9;
  static constexpr Uint32 BRANCH_REG_EQ_NULL = 10;
  static constexpr Uint32 BRANCH_REG_NE_NULL = 11;
  static constexpr Uint32 BRANCH_EQ_REG_REG = 12;
  static constexpr Uint32 BRANCH_NE_REG_REG = 13;
  static constexpr Uint32 BRANCH_LT_REG_REG = 14;
  static constexpr Uint32 BRANCH_LE_REG_REG = 15;
  static constexpr Uint32 BRANCH_GT_REG_REG = 16;
  static constexpr Uint32 BRANCH_GE_REG_REG = 17;
  static constexpr Uint32 EXIT_OK = 18;
  static constexpr Uint32 EXIT_REFUSE = 19;
  static constexpr Uint32 CALL = 20;
  static constexpr Uint32 RETURN = 21;
  static constexpr Uint32 EXIT_OK_LAST = 22;
  static constexpr Uint32 BRANCH_ATTR_OP_ARG = 23;
  static constexpr Uint32 BRANCH_ATTR_EQ_NULL = 24;
  static constexpr Uint32 BRANCH_ATTR_NE_NULL = 25;
  static constexpr Uint32 BRANCH_ATTR_OP_PARAM = 26;
  static constexpr Uint32 BRANCH_ATTR_OP_ATTR = 27;

  /**
   * Macros for creating code
   */
  static Uint32 Read(Uint32 AttrId, Uint32 Register);
  static Uint32 Write(Uint32 AttrId, Uint32 Register);
  
  static Uint32 LoadNull(Uint32 Register);
  static Uint32 LoadConst16(Uint32 Register, Uint32 Value);
  static Uint32 LoadConst32(Uint32 Register); // Value in next word
  static Uint32 LoadConst64(Uint32 Register); // Value in next 2 words
  static Uint32 Add(Uint32 DstReg, Uint32 SrcReg1, Uint32 SrcReg2);
  static Uint32 Sub(Uint32 DstReg, Uint32 SrcReg1, Uint32 SrcReg2);
  static Uint32 Branch(Uint32 Inst, Uint32 Reg1, Uint32 Reg2);
  static Uint32 ExitOK();
  static Uint32 ExitLastOK();

  /**
   * Branch OP_ARG (Attr1 <op> <value arg>)
   *
   * i = Instruction              -  6 Bits ( 0 - 5 ) max 63
   * n = NULL cmp semantic        -  2 bits ( 6 - 7 )
   * a = Attribute id             -  16 bits
   * l = Length of string (bytes) -  16 bits OP_ARG
   * p = parameter no             -  16 bits OP_PARAM
   * b = Branch offset (words)    -  16 bits
   * t = branch type              -  4 bits
   * d = Array length diff  // UNUSED
   * v = Varchar flag       // UNUSED
   *
   *           1111111111222222222233
   * 01234567890123456789012345678901
   * iiiiii   ddvttttbbbbbbbbbbbbbbbb
   * aaaaaaaaaaaaaaaallllllllllllllll
   * -string....                    -
   *
   *
   * Branch OP_PARAM (Attr1 <op> <ParamNo>)
   *
   * i = Instruction              -  6 Bits ( 0 - 5 ) max 63
   * n = NULL cmp semantic        -  2 bits ( 6 - 7 )
   * a = Attribute id             -  16 bits
   * p = parameter no             -  16 bits OP_PARAM
   * b = Branch offset (words)    -  16 bits
   * t = branch type              -  4 bits
   *
   *           1111111111222222222233
   * 01234567890123456789012345678901
   * iiiiii      ttttbbbbbbbbbbbbbbbb
   * aaaaaaaaaaaaaaaapppppppppppppppp
   *
   *
   * Branch OP_ATTR (Attr1 <op> Attr2)
   *
   * i = Instruction              -  6 Bits ( 0 - 5 ) max 63
   * n = NULL cmp semantic        -  2 bits ( 6 - 7 )
   * a = Attribute id1            -  16 bits
   * A = Attribute id2            -  16 bits
   * b = Branch offset (words)    -  16 bits
   * t = branch type              -  4 bits
   *
   *           1111111111222222222233
   * 01234567890123456789012345678901
   * iiiiii      ttttbbbbbbbbbbbbbbbb
   * aaaaaaaaaaaaaaaaAAAAAAAAAAAAAAAA
   */

  enum UnaryCondition {
    IS_NULL = 0,
    IS_NOT_NULL = 1
  };

  enum BinaryCondition {
    EQ = 0,
    NE = 1,
    LT = 2,
    LE = 3,
    GT = 4,
    GE = 5,
    LIKE = 6,
    NOT_LIKE = 7,
    AND_EQ_MASK = 8,
    AND_NE_MASK = 9,
    AND_EQ_ZERO = 10,
    AND_NE_ZERO = 11
  };

  enum NullSemantics {
    NULL_CMP_EQUAL = 0x0,    // Old cmp mode; 'NULL == NULL' and 'NULL < x'
    IF_NULL_BREAK_OUT = 0x2, // Jump to branch destination IF NULL
    IF_NULL_CONTINUE = 0x3   // Ignore IF NULL, continue with next OP
  };

  // Compare Attr with literal
  static Uint32 BranchCol(BinaryCondition cond, NullSemantics nulls);
  static Uint32 BranchCol_2(Uint32 AttrId);
  static Uint32 BranchCol_2(Uint32 AttrId, Uint32 Len);

  // Compare Attr with parameter
  static Uint32 BranchColParameter(BinaryCondition cond, NullSemantics nulls);
  static Uint32 BranchColParameter_2(Uint32 AttrId, Uint32 ParamNo);

  // Compare two Attr from same table
  static Uint32 BranchColAttrId(BinaryCondition cond, NullSemantics nulls);
  static Uint32 BranchColAttrId_2(Uint32 AttrId1, Uint32 AttrId2);

  static Uint32 getNullSemantics(Uint32 op);
  static Uint32 getBinaryCondition(Uint32 op1);
  static Uint32 getBranchCol_AttrId(Uint32 op2);
  static Uint32 getBranchCol_AttrId2(Uint32 op2);
  static Uint32 getBranchCol_Len(Uint32 op2);
  static Uint32 getBranchCol_ParamNo(Uint32 op2);
  
  /**
   * Macros for decoding code
   */
  static Uint32 getOpCode(Uint32 op);
  static Uint32 getReg1(Uint32 op);
  static Uint32 getReg2(Uint32 op);
  static Uint32 getReg3(Uint32 op);
  static Uint32 getLabel(Uint32 op);

  /**
   * Instruction pre-processing required.
   */
  enum InstructionPreProcessing
  {
    NONE,
    LABEL_ADDRESS_REPLACEMENT,
    SUB_ADDRESS_REPLACEMENT
  };

  /* This method is used to determine what sort of 
   * instruction processing is required, and the address
   * of the next instruction in the stream
   */
  static Uint32 *getInstructionPreProcessingInfo(Uint32 *op,
                                                 InstructionPreProcessing& processing);
};

inline
Uint32
Interpreter::Read(Uint32 AttrId, Uint32 Register){
  return (AttrId << 16) + (Register << 6) + READ_ATTR_INTO_REG;
}

inline
Uint32
Interpreter::Write(Uint32 AttrId, Uint32 Register){
  return (AttrId << 16) + (Register << 6) + WRITE_ATTR_FROM_REG;
}

inline
Uint32
Interpreter::LoadNull(Uint32 Register){
  return (Register << 6) + LOAD_CONST_NULL;
}

inline
Uint32
Interpreter::LoadConst16(Uint32 Register, Uint32 Value){
  return (Value << 16) + (Register << 6) + LOAD_CONST16;
}

inline
Uint32
Interpreter::LoadConst32(Uint32 Register){
  return (Register << 6) + LOAD_CONST32;
}

inline
Uint32
Interpreter::LoadConst64(Uint32 Register){
  return (Register << 6) + LOAD_CONST64;
}

inline
Uint32
Interpreter::Add(Uint32 Dcoleg, Uint32 SrcReg1, Uint32 SrcReg2){
  return (SrcReg1 << 6) + (SrcReg2 << 9) + (Dcoleg << 16) + ADD_REG_REG;
}

inline
Uint32
Interpreter::Sub(Uint32 Dcoleg, Uint32 SrcReg1, Uint32 SrcReg2){
  return (SrcReg1 << 6) + (SrcReg2 << 9) + (Dcoleg << 16) + SUB_REG_REG;
}

inline
Uint32
Interpreter::Branch(Uint32 Inst, Uint32 Reg1, Uint32 Reg2){
  return (Reg1 << 9) + (Reg2 << 6) + Inst;
}

inline
Uint32
Interpreter::BranchColAttrId(BinaryCondition cond, NullSemantics nulls) {
  return
    BRANCH_ATTR_OP_ATTR +     // Compare two ATTRs
    (nulls << 6) +
    (cond << 12);
}

inline
Uint32
Interpreter::BranchColAttrId_2(Uint32 AttrId1, Uint32 AttrId2) {
  return (AttrId1 << 16) + AttrId2;
}

inline
Uint32
Interpreter::BranchCol(BinaryCondition cond, NullSemantics nulls){
  return 
    BRANCH_ATTR_OP_ARG +
    (nulls << 6) +
    (cond << 12);
}

inline
Uint32
Interpreter::BranchColParameter(BinaryCondition cond, NullSemantics nulls)
{
  return
    BRANCH_ATTR_OP_PARAM +
    (nulls << 6) +
    (cond << 12);
}

inline
Uint32
Interpreter::BranchColParameter_2(Uint32 AttrId, Uint32 ParamNo){
  return (AttrId << 16) + ParamNo;
}

inline
Uint32 
Interpreter::BranchCol_2(Uint32 AttrId, Uint32 Len){
  return (AttrId << 16) + Len;
}

inline
Uint32 
Interpreter::BranchCol_2(Uint32 AttrId){
  return (AttrId << 16);
}

inline
Uint32
Interpreter::getNullSemantics(Uint32 op){
  return ((op >> 6) & 0x3);
}

inline
Uint32
Interpreter::getBinaryCondition(Uint32 op){
  return (op >> 12) & 0xf;
}

inline
Uint32
Interpreter::getBranchCol_AttrId(Uint32 op){
  return (op >> 16) & 0xFFFF;
}

inline
Uint32
Interpreter::getBranchCol_AttrId2(Uint32 op){
  return op & 0xFFFF;
}

inline
Uint32
Interpreter::getBranchCol_Len(Uint32 op){
  return op & 0xFFFF;
}

inline
Uint32
Interpreter::getBranchCol_ParamNo(Uint32 op){
  return op & 0xFFFF;
}

inline
Uint32
Interpreter::ExitOK(){
  return EXIT_OK;
}

inline
Uint32
Interpreter::ExitLastOK(){
  return EXIT_OK_LAST;
}

inline
Uint32
Interpreter::getOpCode(Uint32 op){
  return op & 0x3f;
}

inline
Uint32
Interpreter::getReg1(Uint32 op){
  return (op >> 6) & 0x7;
}

inline
Uint32
Interpreter::getReg2(Uint32 op){
  return (op >> 9) & 0x7;
}

inline
Uint32
Interpreter::getReg3(Uint32 op){
  return (op >> 16) & 0x7;
}

inline
Uint32
Interpreter::getLabel(Uint32 op){
  return (op >> 16) & 0xffff;
}

inline
Uint32*
Interpreter::getInstructionPreProcessingInfo(Uint32 *op,
                                             InstructionPreProcessing& processing )
{
  /* Given an instruction, get a pointer to the 
   * next instruction in the stream.
   * Returns NULL on error.
   */
  processing= NONE;
  Uint32 opCode= getOpCode(*op);
  
  switch( opCode )
  {
  case READ_ATTR_INTO_REG:
  case WRITE_ATTR_FROM_REG:
  case LOAD_CONST_NULL:
  case LOAD_CONST16:
    return op + 1;
  case LOAD_CONST32:
    return op + 2;
  case LOAD_CONST64:
    return op + 3;
  case ADD_REG_REG:
  case SUB_REG_REG:
    return op + 1;
  case BRANCH:
  case BRANCH_REG_EQ_NULL:
  case BRANCH_REG_NE_NULL:
  case BRANCH_EQ_REG_REG:
  case BRANCH_NE_REG_REG:
  case BRANCH_LT_REG_REG:
  case BRANCH_LE_REG_REG:
  case BRANCH_GT_REG_REG:
  case BRANCH_GE_REG_REG:
    processing= LABEL_ADDRESS_REPLACEMENT;
    return op + 1;
  case BRANCH_ATTR_OP_ARG:
  {
    /* We need to take the length from the second word of the
     * branch instruction so we can skip over the inline const
     * comparison data.
     */
    processing= LABEL_ADDRESS_REPLACEMENT;
    Uint32 byteLength= getBranchCol_Len(*(op+1));
    Uint32 wordLength= (byteLength + 3) >> 2;
    return op + 2 + wordLength;
  }
  case BRANCH_ATTR_OP_PARAM:
  case BRANCH_ATTR_OP_ATTR:
  {
    /* Second word of the branch instruction refer either paramNo
     * or attrId to be compared -> fixed length.
     */
    processing= LABEL_ADDRESS_REPLACEMENT;
    return op + 2;
  }
  case BRANCH_ATTR_EQ_NULL:
  case BRANCH_ATTR_NE_NULL:
    processing= LABEL_ADDRESS_REPLACEMENT;
    return op + 2;
  case EXIT_OK:
  case EXIT_OK_LAST:
  case EXIT_REFUSE:
    return op + 1;
  case CALL:
    processing= SUB_ADDRESS_REPLACEMENT;
    return op + 1;
  case RETURN:
    return op + 1;

  default:
    return nullptr;
  }
}


#undef JAM_FILE_ID

#endif
