/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_INTERPRETER_HPP
#define NDB_INTERPRETER_HPP

#include <ndb_types.h>

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
  STATIC_CONST( READ_ATTR_INTO_REG    = 1 );
  STATIC_CONST( WRITE_ATTR_FROM_REG   = 2 );
  STATIC_CONST( LOAD_CONST_NULL       = 3 );
  STATIC_CONST( LOAD_CONST16          = 4 );
  STATIC_CONST( LOAD_CONST32          = 5 );
  STATIC_CONST( LOAD_CONST64          = 6 );
  STATIC_CONST( ADD_REG_REG           = 7 );
  STATIC_CONST( SUB_REG_REG           = 8 );
  STATIC_CONST( BRANCH                = 9 );
  STATIC_CONST( BRANCH_REG_EQ_NULL    = 10 );
  STATIC_CONST( BRANCH_REG_NE_NULL    = 11 );
  STATIC_CONST( BRANCH_EQ_REG_REG     = 12 );
  STATIC_CONST( BRANCH_NE_REG_REG     = 13 );
  STATIC_CONST( BRANCH_LT_REG_REG     = 14 );
  STATIC_CONST( BRANCH_LE_REG_REG     = 15 );
  STATIC_CONST( BRANCH_GT_REG_REG     = 16 );
  STATIC_CONST( BRANCH_GE_REG_REG     = 17 );
  STATIC_CONST( EXIT_OK               = 18 );
  STATIC_CONST( EXIT_REFUSE           = 19 );
  STATIC_CONST( CALL                  = 20 );
  STATIC_CONST( RETURN                = 21 );
  STATIC_CONST( EXIT_OK_LAST          = 22 );
  STATIC_CONST( BRANCH_ATTR_OP_ARG    = 23 );
  STATIC_CONST( BRANCH_ATTR_EQ_NULL   = 24 );
  STATIC_CONST( BRANCH_ATTR_NE_NULL   = 25 );
  STATIC_CONST( BRANCH_ATTR_OP_ARG_2  = 26 );

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

  /**
   * Branch OP_ARG
   *
   * i = Instruction              -  5 Bits ( 0 - 5 ) max 63
   * a = Attribute id             -  16 bits
   * l = Length of string (bytes) -  16 bits OP_ARG
   * p = parameter no             -  16 bits OP_ARG_2
   * b = Branch offset (words)    -  16 bits
   * t = branch type              -  4 bits
   * d = Array length diff
   * v = Varchar flag
   *
   *           1111111111222222222233
   * 01234567890123456789012345678901
   * iiiiii   ddvttttbbbbbbbbbbbbbbbb
   * aaaaaaaaaaaaaaaallllllllllllllll
   * -string....                    -
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
  // TODO : Remove other 2 unused parameters.
  static Uint32 BranchCol(BinaryCondition cond, 
			  Uint32 arrayLengthDiff, Uint32 varchar);
  static Uint32 BranchCol_2(Uint32 AttrId);
  static Uint32 BranchCol_2(Uint32 AttrId, Uint32 Len);

  static Uint32 BranchColParameter(BinaryCondition cond);
  static Uint32 BranchColParameter_2(Uint32 AttrId, Uint32 ParamNo);

  static Uint32 getBinaryCondition(Uint32 op1);
  static Uint32 getArrayLengthDiff(Uint32 op1);
  static Uint32 isVarchar(Uint32 op1);
  static Uint32 getBranchCol_AttrId(Uint32 op2);
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
Interpreter::BranchCol(BinaryCondition cond, 
		       Uint32 arrayLengthDiff,
		       Uint32 varchar){
  //ndbout_c("BranchCol: cond=%d diff=%u varchar=%u",
      //cond, arrayLengthDiff, varchar);
  return 
    BRANCH_ATTR_OP_ARG + 
    (arrayLengthDiff << 9) + 
    (varchar << 11) +
    (cond << 12);
}

inline
Uint32
Interpreter::BranchColParameter(BinaryCondition cond)
{
  return BRANCH_ATTR_OP_ARG_2 + (cond << 12);
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
Interpreter::getBinaryCondition(Uint32 op){
  return (op >> 12) & 0xf;
}

inline
Uint32
Interpreter::getArrayLengthDiff(Uint32 op){
  return (op >> 9) & 0x3;
}

inline
Uint32
Interpreter::isVarchar(Uint32 op){
  return (op >> 11) & 1;
}

inline
Uint32
Interpreter::getBranchCol_AttrId(Uint32 op){
  return (op >> 16) & 0xFFFF;
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
    return op+2+wordLength;
  }
  case BRANCH_ATTR_OP_ARG_2:
  {
    /* We need to take the length from the second word of the
     * branch instruction so we can skip over the inline const
     * comparison data.
     */
    processing= LABEL_ADDRESS_REPLACEMENT;
    return op+2;
  }
  case BRANCH_ATTR_EQ_NULL:
  case BRANCH_ATTR_NE_NULL:
    processing= LABEL_ADDRESS_REPLACEMENT;
    return op+2;
  case EXIT_OK:
  case EXIT_OK_LAST:
  case EXIT_REFUSE:
    return op+1;
  case CALL:
    processing= SUB_ADDRESS_REPLACEMENT;
  case RETURN:
    return op+1;

  default:
    return NULL;
  }
}

#endif
