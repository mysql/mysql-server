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
  static const Uint32 READ_ATTR_INTO_REG    = 1;
  static const Uint32 WRITE_ATTR_FROM_REG   = 2;
  static const Uint32 LOAD_CONST_NULL       = 3;
  static const Uint32 LOAD_CONST16          = 4;
  static const Uint32 LOAD_CONST32          = 5;
  static const Uint32 LOAD_CONST64          = 6;
  static const Uint32 ADD_REG_REG           = 7;
  static const Uint32 SUB_REG_REG           = 8;
  static const Uint32 BRANCH                = 9;
  static const Uint32 BRANCH_REG_EQ_NULL    = 10;
  static const Uint32 BRANCH_REG_NE_NULL    = 11;
  static const Uint32 BRANCH_EQ_REG_REG     = 12;
  static const Uint32 BRANCH_NE_REG_REG     = 13;
  static const Uint32 BRANCH_LT_REG_REG     = 14;
  static const Uint32 BRANCH_LE_REG_REG     = 15;
  static const Uint32 BRANCH_GT_REG_REG     = 16;
  static const Uint32 BRANCH_GE_REG_REG     = 17;
  static const Uint32 EXIT_OK               = 18;
  static const Uint32 EXIT_REFUSE           = 19;
  static const Uint32 CALL                  = 20;
  static const Uint32 RETURN                = 21;
  static const Uint32 EXIT_OK_LAST          = 22;
  static const Uint32 BRANCH_ATTR_OP_ARG    = 23;
  static const Uint32 BRANCH_ATTR_EQ_NULL   = 24;
  static const Uint32 BRANCH_ATTR_NE_NULL   = 25;
  
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
   * Branch string
   *
   * i = Instruction            -  5 Bits ( 0 - 5 ) max 63
   * a = Attribute id
   * l = Length of string
   * b = Branch offset
   * t = branch type
   * d = Array length diff
   * v = Varchar flag
   * p = No-blank-padding flag for char compare
   *
   *           1111111111222222222233
   * 01234567890123456789012345678901
   * iiiiii   ddvtttpbbbbbbbbbbbbbbbb
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
    NOT_LIKE = 7
  };
  static Uint32 BranchCol(BinaryCondition cond, 
			  Uint32 arrayLengthDiff, Uint32 varchar, bool nopad);
  static Uint32 BranchCol_2(Uint32 AttrId);
  static Uint32 BranchCol_2(Uint32 AttrId, Uint32 Len);

  static Uint32 getBinaryCondition(Uint32 op1);
  static Uint32 getArrayLengthDiff(Uint32 op1);
  static Uint32 isVarchar(Uint32 op1);
  static Uint32 isNopad(Uint32 op1);
  static Uint32 getBranchCol_AttrId(Uint32 op2);
  static Uint32 getBranchCol_Len(Uint32 op2);
  
  /**
   * Macros for decoding code
   */
  static Uint32 getOpCode(Uint32 op);
  static Uint32 getReg1(Uint32 op);
  static Uint32 getReg2(Uint32 op);
  static Uint32 getReg3(Uint32 op);
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
		       Uint32 varchar, bool nopad){
  //ndbout_c("BranchCol: cond=%d diff=%u varchar=%u nopad=%d",
      //cond, arrayLengthDiff, varchar, nopad);
  return 
    BRANCH_ATTR_OP_ARG + 
    (arrayLengthDiff << 9) + 
    (varchar << 11) +
    (cond << 12) +
    (nopad << 15);
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
  return (op >> 12) & 0x7;
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
Interpreter::isNopad(Uint32 op){
  return (op >> 15) & 1;
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

#endif
