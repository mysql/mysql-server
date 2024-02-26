/*
   Copyright (c) 2007, 2023, Oracle and/or its affiliates.

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

#ifndef NdbInterpretedCode_H
#define NdbInterpretedCode_H

#include <ndb_types.h>
#include "ndbapi_limits.h"

#include "NdbDictionary.hpp"
#include "NdbError.hpp"

class NdbTableImpl;
class NdbColumnImpl;

/*
  @brief Stand-alone interpreted programs, for use with NdbRecord

  @details This class is used to prepare an NDB interpreted program for use
  in operations created using NdbRecord, or scans created using the old
  API.  The ScanFilter class can also be used to generate an NDB interpreted
  program using NdbInterpretedCode.

  Usage :
    1) Create NdbInterpretedCode object, optionally supplying a table
       for the program to operate on, and a buffer for program storage
       and finalisation
       Note : 
       - If no table is supplied, then only instructions which do not
       access table attributes can be used.
       - If no buffer is supplied, then an internal buffer will be
       dynamically allocated and extended as necessary.
    2) Add instructions and labels to the NdbInterpretedCode object 
       by calling the methods below.
    3) When the program is complete, finalise it by calling the 
       finalise() method.  This will resolve internal branches and
       calls to label and subroutine offsets.
    4) Use the program with NdbRecord operations and scans by passing
       it at operation definition time via the OperationOptions or
       ScanOptions parameters.
       Alternatively, use the program with old-Api scans by passing it
       via the setInterpretedProgram() method.
    5) When the program is no longer required, the NdbInterpretedCode
       object can be deleted, along with any user-supplied buffer.

  Notes : 
    a) Each NDBAPI operation applies to one table, and so does any 
       NdbInterpretedCode program attached to that operation.
    b) A single finalised NdbInterpretedCode program can be used by
       more than one operation.  It need not be 'rebuilt' for each
       operation.
    c) Methods have minimal error checks, for efficiency
    d) Note that this interface may be subject to change without notice.
       The NdbScanFilter API is a more stable Api for defining scan-filter
       style programs.
*/
class NdbInterpretedCode
{
public:
  /**
   * NdbInterpretedCode constructor
   * 
   * @param table The table which this program will be run against.  This
   * parameter must be supplied if the program is table specific (i.e. 
   * reads from or writes to columns in the table).
   * @param buffer Pointer to a buffer of 32bit words used to store the 
   * program.  
   * @param buffer_word_size Length of the buffer passed in
   * If the program exceeds this length then adding new 
   * instructions will fail with error 4518, Too many instructions in 
   * interpreted program.
   *
   * Alternatively, if no buffer is passed, a buffer will be dynamically
   * allocated internally and extended to cope as instructions are
   * added.
   */
  NdbInterpretedCode(const NdbDictionary::Table *table= 0,
                     Uint32 *buffer= 0, 
                     Uint32 buffer_word_size= 0);

  /* Constructor variant that obtains table from NdbRecord
  */
  NdbInterpretedCode(const NdbRecord &,
                     Uint32 *buffer= 0,
                     Uint32 buffer_word_size= 0);


  ~NdbInterpretedCode();

  /**
   * Describe how a comparison involving a NULL value should behave.
   * Old API behaviour was to cmp 'NULL == NULL -> true' and
   * 'NULL < <any non-null> -> true. (CmpHasNoUnknowns). However,
   * MySQL specify that a comparison involving a NULL-value is 'unknown',
   * which (depending on AND/OR context) will require the branch-out to
   * be taken or ignored. (BranchIfUnknown or ContinueIfUnknown)
   */
  enum UnknownHandling {
    CmpHasNoUnknowns,   // Cmp Never compute boolean 'unknown'
    BranchIfUnknown,    // Cmp will take the 'branch' if unknown.
    ContinueIfUnknown   // 'Unknown' is inconclusive, continue
  };

  /**
   * Use semantics specified by SQL_ISO for comparing NULL values.
   */
  void set_sql_null_semantics(UnknownHandling unknown_action);

  /**
   * Discard any NdbInterpreter program constructed so far
   * and allow construction to start over again.
   */
  void reset();
  
  /* Register constant loads
   * -----------------------
   * These instructions allow numeric constants (and null)
   * to be loaded into the interpreter's registers
   * 
   * Space required      Buffer    Request message
   *   load_const_null   1 word    1 word
   *   load_const_u16    1 word    1 word
   *   load_const_u32    2 words   2 words
   *   load_const_u64    3 words   3 words
   *
   * @param RegDest Register to load constant into
   * @param Constant Value to load
   * @return 0 if successful, -1 otherwise
   */
  int load_const_null(Uint32 RegDest);
  int load_const_u16(Uint32 RegDest, Uint32 Constant);
  int load_const_u32(Uint32 RegDest, Uint32 Constant);
  int load_const_u64(Uint32 RegDest, Uint64 Constant);

  /* Register to / from table attribute load and store 
   * -------------------------------------------------
   * These instructions allow data to be moved between the
   * interpreter's numeric registers and numeric columns
   * in the current row.
   * These instructions require that the table being operated
   * on was specified with the NdbInterpretedCode object
   * was constructed.
   *
   * Space required   Buffer    Request message
   *   read_attr      1 word    1 word
   *   write_attr     1 word    1 word
   *
   * @param RegDest Register to load data into
   * @param attrId Table attribute to use
   * @param column Table column to use
   * @param RegSource Register to store data from
   * @return 0 if successful, -1 otherwise
   */
  int read_attr(Uint32 RegDest, Uint32 attrId);
  int read_attr(Uint32 RegDest, const NdbDictionary::Column *column);
  int write_attr(Uint32 attrId, Uint32 RegSource);
  int write_attr(const NdbDictionary::Column *column, Uint32 RegSource);

  /* Register arithmetic
   * -------------------
   * These instructions provide arithmetic operations on the
   * interpreter's registers.
   *
   * *RegDest= *RegSouce1 <operator> *RegSource2
   *
   * Space required   Buffer    Request message
   *   add_reg        1 word    1 word
   *   sub_reg        1 word    1 word
   *
   * @param RegDest Register to store operation result in
   * @param RegSource1 Register to use as LHS of operator
   * @param RegSource2 Register to use as RHS of operator
   * @return 0 if successful, -1 otherwise
   */
  int add_reg(Uint32 RegDest, Uint32 RegSource1, Uint32 RegSource2);
  int sub_reg(Uint32 RegDest, Uint32 RegSource1, Uint32 RegSource2);

  /* Control flow 
   * ------------
   */

  /* Label definition
   * ----------------
   * Space required   Buffer    Request message
   *   def_label      2 words   0 words
   *
   * @param LabelNum Unique number within this program for the label
   * @return 0 if successful, -1 otherwise
   */
  int def_label(int LabelNum);

  /* Unconditional jump
   * ------------------
   * Space required   Buffer    Request message
   *   branch_label   1 word    1 word
   *
   * @param label : Program label to jump to
   * @return 0 if successful, -1 otherwise
   */
  int branch_label(Uint32 label);

  /* Register based conditional branch ops
   * -------------------------------------
   * These instructions are used to branch based on numeric
   * register to register comparisons.
   *
   * if (RegLvalue <cond> RegRvalue)
   *   goto label;
   *
   * Space required   Buffer    Request message
   *   branch_*       1 word    1 word
   *
   * @param RegLValue register to use as left hand side of condition
   * @param RegRValue register to use as right hand side of condition
   * @param label Program label to jump to if condition is true
   * @return 0 if successful, -1 otherwise.
   */
  int branch_ge(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 label);
  int branch_gt(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 label);
  int branch_le(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 label);
  int branch_lt(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 label);
  int branch_eq(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 label);
  int branch_ne(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 label);
  int branch_ne_null(Uint32 RegLvalue, Uint32 label);
  int branch_eq_null(Uint32 RegLvalue, Uint32 label);

  /* Table data based conditional branch ops
   * ---------------------------------------
   * These instructions are used to branch based on comparisons
   * between columns and constants.
   *
   * These instructions require that the table being operated
   * upon was supplied when the NdbInterpretedCode object was
   * constructed.
   *
   * The comparison constant pointed to by val should
   * be in normal column format as described in the
   * documentation for NdbOperation.equal()
   * NOTE THE ORDER OF THE COMPARISON AND ARGUMENTS

   * NOTE that NULL values are compared according to the specified
   * 'UnknownHandling' (set_sql_null_semantics()). If not specified,
   * the default will be to compare NULL such that NULL is
   * less that any non-NULL value, and NULL is equal to NULL.
   *
   * BEWARE, that the later is not according to the specified SQL
   * std spec, which is also implemented by MySql.
   *
   * if ( *val <cond> ValueOf(AttrId) )
   *   goto label;
   *
   * Space required        Buffer          Request message
   *   branch_col_*_null   2 words         2 words
   *   branch_col_*        2 words +       2 words +
   *                       type length     type length
   *                       rounded to      rounded to
   *                       nearest word    nearest word
   *
   *                       Only significant words stored/
   *                       sent for VAR* types
   *
   * @param val       ptr to const value to compare against
   * @param unused    unnecessary
   * @param attrId    column to compare
   * @param label     Program label to jump to if condition is true
   * @return 0 if successful, -1 otherwise.
   */
  int branch_col_eq(const void *val,
                    Uint32 unused,
                    Uint32 attrId,
                    Uint32 label);
  int branch_col_ne(const void *val,
                    Uint32 unused,
                    Uint32 attrId,
                    Uint32 label);
  int branch_col_lt(const void *val,
                    Uint32 unused,
                    Uint32 attrId,
                    Uint32 label);
  int branch_col_le(const void *val,
                    Uint32 unused,
                    Uint32 attrId,
                    Uint32 label);
  int branch_col_gt(const void *val,
                    Uint32 unused,
                    Uint32 attrId,
                    Uint32 label);
  int branch_col_ge(const void *val,
                    Uint32 unused,
                    Uint32 attrId,
                    Uint32 label);

  /* Variants of above methods allowing us to compare two Attr
   * from the same table. Both Attr's has to be of the exact same
   * data type, including length, precision, scale, etc.
   *
   * NOTE that NULL values are compared according to the specified
   * 'UnknownHandling' (set_sql_null_semantics()). If not specified,
   * the default will be to compare NULL such that NULL is
   * less that any non-NULL value, and NULL is equal to NULL.
   *
   * BEWARE, that the later is not according to the specified SQL
   * std spec, which is also implemented by MySql.
   */
  int branch_col_eq(Uint32 attrId1, Uint32 attrId2, Uint32 label);
  int branch_col_ne(Uint32 attrId1, Uint32 attrId2, Uint32 label);
  int branch_col_lt(Uint32 attrId1, Uint32 attrId2, Uint32 label);
  int branch_col_le(Uint32 attrId1, Uint32 attrId2, Uint32 label);
  int branch_col_gt(Uint32 attrId1, Uint32 attrId2, Uint32 label);
  int branch_col_ge(Uint32 attrId1, Uint32 attrId2, Uint32 label);

  int branch_col_eq_null(Uint32 attrId, Uint32 label);
  int branch_col_ne_null(Uint32 attrId, Uint32 label);

  /*
   * Variants comparing an Attribute from this table with a parameter
   * value specified in the supplied attrInfo section.
   *
   * NULL values are allowed for the parameters, and are compared according
   * to the specified 'UnknownHandling' (set_sql_null_semantics()).
   * If not specified, the default will be to compare NULL such that NULL is
   * less that any non-NULL value, and NULL is equal to NULL.
   *
   * BEWARE, that the later is not according to the specified SQL
   * std spec, which is also implemented by MySql.
   */
  int branch_col_eq_param(Uint32 attrId, Uint32 paramId, Uint32 label);
  int branch_col_ne_param(Uint32 attrId, Uint32 paramId, Uint32 label);
  int branch_col_lt_param(Uint32 attrId, Uint32 paramId, Uint32 label);
  int branch_col_le_param(Uint32 attrId, Uint32 paramId, Uint32 label);
  int branch_col_gt_param(Uint32 attrId, Uint32 paramId, Uint32 label);
  int branch_col_ge_param(Uint32 attrId, Uint32 paramId, Uint32 label);

  /* Table based pattern match conditional operations
   * ------------------------------------------------
   * These instructions are used to branch based on comparisons
   * between CHAR/BINARY/VARCHAR/VARBINARY columns and
   * reg-exp patterns.
   *
   * These instructions require that the table being operated
   * upon was supplied when the NdbInterpretedCode object was
   * constructed.
   *
   * The pattern passed in val should be in plain CHAR
   * format even if the column is a VARCHAR
   * (i.e. no leading length bytes)
   *
   * if (ValueOf(attrId) <LIKE/NOTLIKE> *val)
   *   goto label;
   *
   * Space required        Buffer          Request message
   *   branch_col_like/
   *   branch_col_notlike  2 words +       2 words +
   *                       len bytes       len bytes
   *                       rounded to      rounded to
   *                       nearest word    nearest word
   *
   * @param val       ptr to const pattern to match against
   * @param len       length in bytes of const pattern
   * @param attrId    column to compare
   * @param label     Program label to jump to if condition is true
   * @return 0 if successful, -1 otherwise.
   *
   */
  int branch_col_like(const void *val, Uint32 len, Uint32 attrId, Uint32 label);
  int branch_col_notlike(const void *val,
                         Uint32 len,
                         Uint32 attrId,
                         Uint32 label);

  /* Table based bitwise logical conditional operations
   * --------------------------------------------------
   * These instructions are used to branch based on the
   * result of logical AND between Bit type column data
   * and a bitmask pattern.
   *
   * These instructions require that the table being operated
   * upon was supplied when the NdbInterpretedCode object was
   * constructed.
   *
   * The mask value should be the same size as the bit column
   * being compared.
   * Bitfields are passed in/out of NdbApi as 32-bit words
   * with bits set from lsb to msb.
   * The platform's endianness controls which byte contains
   * the ls bits.
   *   x86= first(0th) byte.  Sparc/PPC= last(3rd byte)
   *
   * To set bit n of a bitmask to 1 from a Uint32* mask :
   *   mask[n >> 5] |= (1 << (n & 31))
   *
   * if (BitWiseAnd(ValueOf(attrId), *mask) <EQ/NE> <*mask/0>)
   *   goto label;
   *
   * Space required        Buffer          Request message
   *   branch_col_and_mask_eq_mask/
   *   branch_col_and_mask_ne_mask/
   *   branch_col_and_mask_eq_zero/
   *   branch_col_and_mask_ne_zero
   *                       2 words +       2 words +
   *                       column width    column width
   *                       rounded to      rounded to
   *                       nearest word    nearest word
   *
   * @param mask      ptr to const mask to use
   * @param unused    unnecessary
   * @param attrId    column to compare
   * @param label     Program label to jump to if condition is true
   * @return 0 if successful, -1 otherwise.
   *
   */
  int branch_col_and_mask_eq_mask(const void *mask,
                                  Uint32 unused,
                                  Uint32 attrId,
                                  Uint32 label);
  int branch_col_and_mask_ne_mask(const void *mask,
                                  Uint32 unused,
                                  Uint32 attrId,
                                  Uint32 label);
  int branch_col_and_mask_eq_zero(const void *mask,
                                  Uint32 unused,
                                  Uint32 attrId,
                                  Uint32 label);
  int branch_col_and_mask_ne_zero(const void *mask,
                                  Uint32 unused,
                                  Uint32 attrId,
                                  Uint32 label);

  /* Program results 
   * ---------------
   * These instructions indicate to the interpreter that processing
   * for the current row is finished.
   * In a scanning operation, the program may then be re-run for 
   * the next row.
   * In a non-scanning operation, the program will not be run again.
   * 
   */

  /* interpret_exit_ok
   * 
   * Scanning operation     : This row should be returned as part of
   *                          the scan.  Move onto next row.
   * Non-scanning operation : Exit interpreted program.
   *
   * Space required        Buffer    Request message
   *   interpret_exit_ok   1 word    1 word
   *
   * @return 0 if successful, -1 otherwise.
   */
  int interpret_exit_ok();
  
  /* interpret_exit_nok
   *
   * Scanning operation     : Error codes 626 and 899: This row should not be 
   *                          returned as part of the scan.  Move onto next row.
   *                          Error codes [6000-6999]: Abort the scan.
   *
   * Non-scanning operation : Abort the operation
   *
   * Space required        Buffer    Request message
   *   interpret_exit_nok  1 word    1 word   
   *
   * @param ErrorCode An error code which will be returned as part
   * of the operation.  If not supplied, defaults to 626. Applications should 
   * use error code 626 or any code in the [6000-6999] range. Error code 899
   * is supported for backwards compatibility, but 626 is recommended instead.
   * For other codes, the behavior is undefined and may change at any time 
   * without prior notice.
   *
   * @return 0 if successful, -1 otherwise
   */
  int interpret_exit_nok(Uint32 ErrorCode);
  int interpret_exit_nok();

  /* interpret_exit_last_row
   * 
   * Scanning operation     : This row should be returned as part of
   *                          the scan.  No more rows should be scanned
   *                          in this fragment.
   * Non-scanning operation : Abort the operation
   *
   * Space required               Buffer    Request message
   *   interpret_exit_last_row    1 word    1 word
   *
   * @return 0 if successful, -1 otherwise
   */
  int interpret_exit_last_row();

  /* Utilities
   * These utilities insert multiple instructions into the
   * program and use specific registers to accomplish their 
   * goal.
   */

  /* add_val
   * Adds the supplied numeric value (32 or 64 bit) to the supplied
   * column.
   *
   * Uses registers 6 and 7, destroying any contents they have.
   * After execution : R6 = old column value  R7 = new column value
   *
   * These utilities require that the table being operated
   * upon was supplied when the NdbInterpretedCode object was
   * constructed.
   *
   * Space required     Buffer     Request message
   *   add_val(32bit)   4 words + 1 word if aValue >= 2^16
   *   add_val(64 bit)  4 words + 1 word if aValue >= 2^16
   *                            + 1 word if aValue >= 2^32
   *
   * @param attrId Column to be added to
   * @param aValue Value to add
   * @return 0 if successful, -1 otherwise
   */
  int add_val(Uint32 attrId, Uint32 aValue);
  int add_val(Uint32 attrId, Uint64 aValue);

  /* sub_val
   * Subtracts the supplied value (32 or 64 bit) from the
   * value of the supplied column.
   *
   * Uses registers 6 and 7, destroying any contents they have.
   * After execution : R6 = old column value  R7 = new column value
   *
   * These utilities require that the table being operated
   * upon was supplied when the NdbInterpretedCode object was
   * constructed.
   *
   * Space required     Buffer     Request message
   *   sub_val(32bit)   4 words + 1 word if aValue >= 2^16
   *   sub_val(64 bit)  4 words + 1 word if aValue >= 2^16
   *                            + 1 word if aValue >= 2^32
   *
   * @param attrId Column to be subtracted from
   * @param aValue Value to subtrace
   * @param 0 if successful, -1 otherwise
   */
  int sub_val(Uint32 attrId, Uint32 aValue);
  int sub_val(Uint32 attrId, Uint64 aValue);


  /* Subroutines
   * Subroutines which can be called from the 'main' part of
   * an interpreted program can be defined.
   * Subroutines are identified with a number.  Subroutine
   * numbers must be contiguous.
   */

  /**
   * def_subroutine
   * Define a subroutine.  Subroutines can only be defined 
   * after all main program instructions are defined.
   * Instructions following this, up to the next ret_sub() 
   * instruction are part of this subroutine.  
   * Subroutine numbers must be contiguous from zero but do 
   * not have to be in order.
   *
   * Space required     Buffer     Request message
   *   def_sub          2 words    0 words
   *
   * @param SubroutineNumber number to identify this subroutine
   * @return 0 if successful, -1 otherwise
   */
  int def_sub(Uint32 SubroutineNumber);

  /**
   * call_sub
   * Call a subroutine by number.  When the subroutine
   * returns, the program will continue executing at the
   * next instruction.  Subroutines can be called from the
   * main program, or from subroutines.
   * The maximum stack depth is currently 32.
   *
   * Space required     Buffer     Request message
   *   call_sub         1 word     1 word
   *
   * @param SubroutineNumber Which subroutine to call
   * @return 0 if successful, -1 otherwise
   */
  int call_sub(Uint32 SubroutineNumber);

  /**
   * ret_sub
   * Return from a subroutine.
   *
   * Space required     Buffer     Request message
   *   ret_sub          1 word     1 word
   *
   * @return 0 if successful, -1 otherwise
   */
  int ret_sub();

  /**
   * finalise
   * This method must be called after an Interpreted program 
   * is defined and before it is used.
   * It uses the label and subroutine meta information to 
   * resolve branch jumps and subroutine calls.
   * It can only be called once.
   * If no instructions have been defined, then it will attempt
   * to add a single interpret_exit_ok instruction before
   * finalisation.
   */
  int finalise();

  /**
   * getTable()
   * Returns a pointer to the table object representing the table
   * that this NdbInterpretedCode object operates on.
   * This can be NULL if no table object was supplied at 
   * construction time.
   */
  const NdbDictionary::Table* getTable() const;

  /**
   * getNdbError
   * This method returns the most recent error associated
   * with this NdbInterpretedCode object.
   */
  const struct NdbError & getNdbError() const;


  /**
   * getWordsUsed
   * Returns the number of words of the supplied or internal
   * buffer that have been used.
   */
  Uint32 getWordsUsed() const;

  /**
   * Makes a deep copy of 'src'
   * @return possible error code.
   */
  int copy(const NdbInterpretedCode& src);

private:
  friend class NdbOperation;
  friend class NdbScanOperation;
  friend class NdbQueryOperationImpl;
  friend class NdbQueryOptionsImpl;

  static const Uint32 MaxReg= 8;
  static const Uint32 MaxLabels= 65535;
  static const Uint32 MaxSubs=65535;
  static const Uint32 MaxDynamicBufSize= NDB_MAX_SCANFILTER_SIZE_IN_WORDS;

  const NdbTableImpl *m_table_impl;
  Uint32 *m_buffer;
  Uint32 m_buffer_length;               // In words
  Uint32 *m_internal_buffer;            // Self-managed buffer
  Uint32 m_number_of_labels;
  Uint32 m_number_of_subs;
  Uint32 m_number_of_calls;
  
  /* Offset of last meta info record from start of m_buffer
   * in words
   */
  Uint32 m_last_meta_pos;

  /* Number of words used for instructions. Includes main program
   * and subroutines
   */
  Uint32 m_instructions_length;

  /* Position of first subroutine word.
   * 0 if there are no subroutines.
   */
  Uint32 m_first_sub_instruction_pos;

  /* The end of the buffer is used to store label and subroutine
   * meta information used when resolving branches and calls when
   * the program is finalised.
   * As this meta information grows, the remaining words in the
   * buffer may be less than buffer length minus the 
   * instructions length
   */
  Uint32 m_available_length;

  enum Flags {
    /*
      We will set m_got_error if an error occurs, so that we can
      refuse to create an operation from InterpretedCode that the user
      forgot to do error checks on.
    */
    GotError= 0x1,
    /* Set if reading disk column. */
    UsesDisk= 0x2,
    /* Object state : Set if currently defining a subroutine */
    InSubroutineDef= 0x4,
    /* Has this program been finalised? */
    Finalised= 0x8
  };
  Uint32 m_flags;

  // Allow m_error to be updated even for read only methods
  mutable NdbError m_error;

  UnknownHandling m_unknown_action;

  enum InfoType {
    Label      = 0,
    Subroutine = 1};

  /* Instances of this type are stored at the end of
   * the buffer to describe label and subroutine
   * positions.  The instances are added as the labels and
   * subroutines are defined, so the order (working backwards
   * from the end of the buffer) would be : 
   *
   *   Main program labels (if any)
   *   First subroutine (if any)
   *   First subroutine label defs (if any)
   *   Second subroutine (if any)
   *   Second subroutine label defs ....
   * 
   * The subroutines should be in order of subroutine number
   * as they must be defined in-order.  The labels can be in
   * any order.
   *
   * Before this information is used for finalisation, it is
   * sorted so that the subroutines and labels are in-order.
   */
  struct CodeMetaInfo
  { 
    Uint16 type;
    Uint16 number;         // Label or sub num
    Uint16 firstInstrPos;  // Offset from start of m_buffer, or
                           // from start of subs section for
                           // subs defs
  };
  
  static const Uint32 CODEMETAINFO_WORDS = 2;


  enum Errors 
  {
    TooManyInstructions = 4518,
    BadAttributeId      = 4004,
    BadLabelNum         = 4226,
    BranchToBadLabel    = 4221,
    BadLength           = 4209,
    BadSubNumber        = 4227,
    BadState            = 4231
  };

  int error(Uint32 code);
  bool have_space_for(Uint32 wordsRequired);

  int add1(Uint32 x1);
  int add2(Uint32 x1, Uint32 x2);
  int add3(Uint32 x1, Uint32 x2, Uint32 x3);
  int addN(const Uint32 *data, Uint32 length);
  int addMeta(CodeMetaInfo& info);

  int add_branch(Uint32 instruction, Uint32 label);
  int read_attr_impl(const NdbColumnImpl *c, Uint32 RegDest);
  int write_attr_impl(const NdbColumnImpl *c, Uint32 RegSource);
  int branch_col_val(Uint32 branch_type, Uint32 attrId, const void * val,
                     Uint32 len, Uint32 label);
  int branch_col_col(Uint32 branch_type, Uint32 attrId1, Uint32 attrId2,
                     Uint32 label);
  int branch_col_param(Uint32 branch_type, Uint32 attrId, Uint32 paramId,
                       Uint32 label);
  int getInfo(Uint32 number, CodeMetaInfo &info) const;
  static int compareMetaInfo(const void *a, 
                             const void *b);

  NdbInterpretedCode(const NdbInterpretedCode&); // Not impl.
  NdbInterpretedCode&operator=(const NdbInterpretedCode&);
};
#endif

