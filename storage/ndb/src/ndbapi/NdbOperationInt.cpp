/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

#include "API.hpp"
#include "Interpreter.hpp"
#include <signaldata/AttrInfo.hpp>

#ifdef VM_TRACE
#ifdef NDB_USE_GET_ENV
#include <NdbEnv.h>
#define INT_DEBUG(x) \
  { const char* tmp = NdbEnv_GetEnv("INT_DEBUG", (char*)0, 0); \
  if (tmp != 0 && strlen(tmp) != 0) { ndbout << "INT:"; ndbout_c x; } }
#else
#define INT_DEBUG(x)
#endif
#else
#define INT_DEBUG(x)
#endif

void
NdbOperation::initInterpreter(){
  theFirstLabel = nullptr;
  theLastLabel = nullptr;
  theFirstBranch = nullptr;
  theLastBranch = nullptr;
  
  theFirstCall = nullptr;
  theLastCall = nullptr;
  theFirstSubroutine = nullptr;
  theLastSubroutine = nullptr;
  
  theNoOfLabels = 0;
  theNoOfSubroutines = 0;
  
  theSubroutineSize = 0;
  theInitialReadSize = 0;
  theInterpretedSize = 0;
  theFinalUpdateSize = 0;
  theFinalReadSize = 0;
  theInterpretIndicator = 1;

  theTotalCurrAI_Len = AttrInfo::SectionSizeInfoLength;
}

bool
NdbOperation::isNdbRecordOperation()
{
  /* All scans are 'NdbRecord'.  For PK and UK access
   * check if we've got an m_attribute_record set 
   */
  return !(((m_type == PrimaryKeyAccess) ||
            (m_type == UniqueIndexAccess)) &&
           (m_attribute_record == nullptr));
}

int
NdbOperation::incCheck(const NdbColumnImpl* tNdbColumnImpl)
{
  if (isNdbRecordOperation()) {
    /* Wrong API.  Use NdbInterpretedCode for NdbRecord operations */
    setErrorCodeAbort(4537);
    return -1;
  }

  if (theInterpretIndicator == 1) {
    if (tNdbColumnImpl == nullptr)
      goto inc_check_error1;
    if ((tNdbColumnImpl->getInterpretableType() != true) ||
        (tNdbColumnImpl->m_pk != false) ||
        (tNdbColumnImpl->m_nullable))
      goto inc_check_error2;
    if (theStatus == ExecInterpretedValue) {
      ; // Simply continue with interpretation
    } else if (theStatus == GetValue) {
      theInitialReadSize = theTotalCurrAI_Len - AttrInfo::SectionSizeInfoLength;
      theStatus = ExecInterpretedValue;
    } else if (theStatus == SubroutineExec) {
      ; // Simply continue with interpretation
    } else {
      setErrorCodeAbort(4231);
      return -1;
    }
    if (tNdbColumnImpl->m_storageType == NDB_STORAGETYPE_DISK)
    {
      m_flags &= ~(Uint8)OF_NO_DISK;
    }
    return tNdbColumnImpl->m_attrId;
  } else {
    if (theNdbCon->theCommitStatus == NdbTransaction::Started)
      setErrorCodeAbort(4200);
  }
  return -1;
  
 inc_check_error1:
  setErrorCodeAbort(4004);
  return -1;
  
 inc_check_error2:
  if (tNdbColumnImpl->m_pk){
    setErrorCodeAbort(4202);
    return -1;
  }//if
  if (!tNdbColumnImpl->getInterpretableType()){
    setErrorCodeAbort(4217);
    return -1;
  }//if
  if (tNdbColumnImpl->m_nullable){
    setErrorCodeAbort(4218);
    return -1;
  }//if
  setErrorCodeAbort(4219);
  return -1;
}

int
NdbOperation::write_attrCheck(const NdbColumnImpl* tNdbColumnImpl)
{
  if (isNdbRecordOperation()) {
    /* Wrong API.  Use NdbInterpretedCode for NdbRecord operations */
    setErrorCodeAbort(4537);
    return -1;
  }

  if (theInterpretIndicator == 1) {
    if (tNdbColumnImpl == nullptr)
      goto write_attr_check_error1;
    if ((tNdbColumnImpl->getInterpretableType() == false) ||
        (tNdbColumnImpl->m_pk))
      goto write_attr_check_error2;
    if (theStatus == ExecInterpretedValue) {
      ; // Simply continue with interpretation
    } else if (theStatus == SubroutineExec) {
      ; // Simply continue with interpretation
    } else {
      setErrorCodeAbort(4231);
      return -1;
    }
    if (tNdbColumnImpl->m_storageType == NDB_STORAGETYPE_DISK)
    {
      m_flags &= ~(Uint8)OF_NO_DISK;
    }
    return tNdbColumnImpl->m_attrId;
  } else {
    if (theNdbCon->theCommitStatus == NdbTransaction::Started)
      setErrorCodeAbort(4200);
  }
  return -1;

write_attr_check_error1:
  setErrorCodeAbort(4004);
  return -1;

write_attr_check_error2:
  if (tNdbColumnImpl->m_pk) {
    setErrorCodeAbort(4202);
    return -1;
  }//if
  if (tNdbColumnImpl->getInterpretableType() == false){
    setErrorCodeAbort(4217);
    return -1;
  }//if
  setErrorCodeAbort(4219);
  return -1;
}

int
NdbOperation::read_attrCheck(const NdbColumnImpl* tNdbColumnImpl)
{
  if (isNdbRecordOperation()) {
    /* Wrong API.  Use NdbInterpretedCode for NdbRecord operations */
    setErrorCodeAbort(4537);
    return -1;
  }

  if (theInterpretIndicator == 1) {
    if (tNdbColumnImpl == nullptr)
      goto read_attr_check_error1;
    if (tNdbColumnImpl->getInterpretableType() == false)
      goto read_attr_check_error2;
    if (theStatus == ExecInterpretedValue) {
      ; // Simply continue with interpretation
    } else if (theStatus == GetValue) {
      theInitialReadSize = theTotalCurrAI_Len - AttrInfo::SectionSizeInfoLength;
      theStatus = ExecInterpretedValue;
    } else if (theStatus == SubroutineExec) {
      ; // Simply continue with interpretation
    } else {
      setErrorCodeAbort(4231);
      return -1;
    }
    if (tNdbColumnImpl->m_storageType == NDB_STORAGETYPE_DISK)
    {
      m_flags &= ~(Uint8)OF_NO_DISK;
    }
    return tNdbColumnImpl->m_attrId;
  } else {
    if (theNdbCon->theCommitStatus == NdbTransaction::Started)
      setErrorCodeAbort(4200);
  }
  return -1;
  
 read_attr_check_error1:
  setErrorCodeAbort(4004);
  return -1;
  
 read_attr_check_error2:
  if (tNdbColumnImpl->getInterpretableType() == false)
    setErrorCodeAbort(4217);
  else
    setErrorCodeAbort(4219);
  return -1;

}

int
NdbOperation::initial_interpreterCheck()
{
  if (isNdbRecordOperation()) {
    /* Wrong API.  Use NdbInterpretedCode for NdbRecord operations */
    setErrorCodeAbort(4537);
    return -1;
  }

  if (theInterpretIndicator == 1) {
    if (theStatus == ExecInterpretedValue) {
      return 0; // Simply continue with interpretation
    } else if (theStatus == GetValue) {
      theInitialReadSize = theTotalCurrAI_Len - AttrInfo::SectionSizeInfoLength;
      theStatus = ExecInterpretedValue;
      return 0;
    } else if (theStatus == SubroutineExec) {
       return 0; // Simply continue with interpretation
    } else {
      setErrorCodeAbort(4231);
      return -1;
    }
    return 0;
  } else {
    if (theNdbCon->theCommitStatus == NdbTransaction::Started)
      setErrorCodeAbort(4200);
  }
  return -1;
}

int
NdbOperation::labelCheck()
{
  if (isNdbRecordOperation()) {
    /* Wrong API.  Use NdbInterpretedCode for NdbRecord operations */
    setErrorCodeAbort(4537);
    return -1;
  }

  if (theInterpretIndicator == 1) {
    if (theStatus == ExecInterpretedValue) {
      return 0; // Simply continue with interpretation
    } else if (theStatus == GetValue) {
      theInitialReadSize = theTotalCurrAI_Len - AttrInfo::SectionSizeInfoLength;
      theStatus = ExecInterpretedValue;
      return 0;
    } else if (theStatus == SubroutineExec) {
       return 0; // Simply continue with interpretation
    } else if (theStatus == SubroutineEnd) {
       theStatus = SubroutineExec;
    } else {
      setErrorCodeAbort(4231);
      return -1;
    }
    return 0;
  } else {
    if (theNdbCon->theCommitStatus == NdbTransaction::Started)
      setErrorCodeAbort(4200);
  }
  return -1;
}

int
NdbOperation::intermediate_interpreterCheck()
{
  if (isNdbRecordOperation()) {
    /* Wrong API.  Use NdbInterpretedCode for NdbRecord operations */
    setErrorCodeAbort(4537);
    return -1;
  }

  if (theInterpretIndicator == 1) {
    if (theStatus == ExecInterpretedValue) {
      return 0; // Simply continue with interpretation
    } else if (theStatus == SubroutineExec) {
       return 0; // Simply continue with interpretation
    } else {
      setErrorCodeAbort(4231);
      return -1;
    }
    return 0;
  } else {
    if (theNdbCon->theCommitStatus == NdbTransaction::Started)
      setErrorCodeAbort(4200);
  }
  return -1;
}

/*****************************************************************************
 * int incValue(const char* anAttrName, char* aValue, Uint32 aValue)
 *
 * Return Value:  Return 0 : incValue was successful.
 *                Return -1: In all other case.   
 * Parameters:    anAttrName : Attribute name where the attribute value 
 *                             will be save.
 *                aValue : The constant to increment the attribute value with. 
 *****************************************************************************/
int
NdbOperation::incValue(const NdbColumnImpl* tNdbColumnImpl, Uint32 aValue)
{
  INT_DEBUG(("incValue32 %d %u", tNdbColumnImpl->m_attrId, aValue));
  int tAttrId;

  tAttrId = incCheck(tNdbColumnImpl);
  if (tAttrId == -1)
    goto incValue_error1;

// Load Attribute into register 6

  if (insertATTRINFO( Interpreter::Read(tAttrId, 6)) == -1)
    goto incValue_error1;
// Load aValue into register 7
  if (aValue < 65536)
  {
    if (insertATTRINFO(Interpreter::LoadConst16(7, aValue)) == -1)
      goto incValue_error1;
  } else {
    if (insertATTRINFO(Interpreter::LoadConst32(7)) == -1)
      goto incValue_error1;
    if (insertATTRINFO(aValue) == -1)
      goto incValue_error1;
  }
  // Add register 6 and 7 and put result in register 7

  if (insertATTRINFO( Interpreter::Add(7, 6, 7)) == -1)
    goto incValue_error1;
  if (insertATTRINFO( Interpreter::Write(tAttrId, 7)) == -1)
    goto incValue_error1;
  
  theErrorLine++;
  return 0;

incValue_error1:
  return -1;
}

/*****************************************************************************
 * int subValue(const char* anAttrName, char* aValue, Uint32 aValue)
 *
 * Return Value:   Return 0 : incValue was successful.
 *                 Return -1: In all other case.   
 * Parameters:     anAttrName : Attribute name where the attribute value 
 *                              will be save.
 *                aValue : The constant to increment the attribute value with. 
******************************************************************************/
int
NdbOperation::subValue(const NdbColumnImpl* tNdbColumnImpl, Uint32 aValue)
{
  INT_DEBUG(("subValue32 %d %u", tNdbColumnImpl->m_attrId, aValue));
  int tAttrId;

  tAttrId = incCheck(tNdbColumnImpl);
  if (tAttrId == -1)
    goto subValue_error1;

// Load Attribute into register 6

  if (insertATTRINFO( Interpreter::Read(tAttrId, 6)) == -1)
    goto subValue_error1;
// Load aValue into register 7
  if (aValue < 65536)
  {
    if (insertATTRINFO( Interpreter::LoadConst16(7, aValue)) == -1)
      goto subValue_error1;
  } else {
    if (insertATTRINFO( Interpreter::LoadConst32(7)) == -1)
      goto subValue_error1;
    if (insertATTRINFO(aValue) == -1)
      goto subValue_error1;
  }
  // Subtract register 6 and 7 and put result in register 7
  
  if (insertATTRINFO( Interpreter::Sub(7, 6, 7)) == -1)
    goto subValue_error1;
  if (insertATTRINFO( Interpreter::Write(tAttrId, 7)) == -1)
    goto subValue_error1;
  
  theErrorLine++;
  return 0;
  
 subValue_error1:
  return -1;
}

/******************************************************************************
 * int incValue(const char* anAttrName, char* aValue, Uint64 aValue)
 *
 * Return Value:   Return 0 : incValue was successful.
 *                 Return -1: In all other case.   
 * Parameters:     anAttrName : Attribute name where the attribute value will 
 *                             be save.
 *                aValue : The constant to increment the attribute value with. 
 *****************************************************************************/
int
NdbOperation::incValue(const NdbColumnImpl* tNdbColumnImpl, Uint64 aValue)
{
  INT_DEBUG(("incValue64 %d %llu", tNdbColumnImpl->m_attrId, aValue));
  int tAttrId;

  tAttrId = incCheck(tNdbColumnImpl);
  if (tAttrId == -1)
    goto incValue_error1;

// Load Attribute into register 6

  if (insertATTRINFO( Interpreter::Read(tAttrId, 6)) == -1)
    goto incValue_error1;
// Load aValue into register 7
  if (insertATTRINFO( Interpreter::LoadConst64(7)) == -1)
    goto incValue_error1;
  if (insertATTRINFOloop((Uint32*)&aValue, 2) == -1)
    goto incValue_error1;
  // Add register 6 and 7 and put result in register 7
  if (insertATTRINFO( Interpreter::Add(7, 6, 7)) == -1)
    goto incValue_error1;
  if (insertATTRINFO( Interpreter::Write(tAttrId, 7)) == -1)
    goto incValue_error1;
  
  theErrorLine++;
  return 0;  

incValue_error1:
  return -1;
}

/*****************************************************************************
 * int subValue(const char* anAttrName, char* aValue, Uint64 aValue)
 *
 * Return Value:   Return 0 : incValue was successful.
 *                Return -1: In all other case.   
 * Parameters:     anAttrName : Attribute name where the attribute value will 
 *                              be save.
 *                aValue : The constant to increment the attribute value with. 
******************************************************************************/
int
NdbOperation::subValue(const NdbColumnImpl* tNdbColumnImpl, Uint64 aValue)
{
  INT_DEBUG(("subValue64 %d %llu", tNdbColumnImpl->m_attrId, aValue));
  int tAttrId;

  tAttrId = incCheck(tNdbColumnImpl);
  if (tAttrId == -1)
    goto subValue_error1;

// Load Attribute into register 6

  if (insertATTRINFO( Interpreter::Read(6, tAttrId)) == -1)
    goto subValue_error1;
// Load aValue into register 7
  if (insertATTRINFO( Interpreter::LoadConst64(7)) == -1)
    goto subValue_error1;
  if (insertATTRINFOloop((Uint32*)&aValue, 2) == -1)
    goto subValue_error1;
// Subtract register 6 and 7 and put result in register 7
  if (insertATTRINFO( Interpreter::Sub(7, 6, 7)) == -1)
    goto subValue_error1;
  if (insertATTRINFO( Interpreter::Write(tAttrId, 7)) == -1)
    goto subValue_error1;

  theErrorLine++;
  return 0;  

subValue_error1:
  return -1;
}

/*****************************************************************************
 * int NdbOperation::def_label()
 *****************************************************************************/
int
NdbOperation::def_label(int tLabelNo)
{
  INT_DEBUG(("def_label %d", tLabelNo));
  Uint32 tLabelIndex;
  if (labelCheck() == -1)
    return -1;

  tLabelIndex = theNoOfLabels - ((theNoOfLabels >> 4) << 4);
  if (tLabelIndex == 0)
  {
    NdbLabel* tNdbLabel = theNdb->getNdbLabel();
    if (tNdbLabel == nullptr)
    {
      setErrorCodeAbort(4000);
      return -1;
    }
    if (theFirstLabel == nullptr)
      theFirstLabel = tNdbLabel;
    else
      theLastLabel->theNext = tNdbLabel;

    theLastLabel = tNdbLabel;
    tNdbLabel->theNext = nullptr;
  }

  /**
   * Here we set the address that the label should point to (jump address), 
   * the first 5 words are excluded since they are length specifications and 
   * not part of the data. 
   * We need to add 1 to the current ATTRINFO length since the last inserted 
   * item is not where we want to jump to. 
   * Later on we will update the branch items with this address, this is done in
   * the NdbOperation::prepareSendInterpreted method.
   */

  Uint32 initialOffset= theInitialReadSize + AttrInfo::SectionSizeInfoLength;

  if (theNoOfSubroutines > 0)
  {
    /* Label in a sub, needs to be offset from the start of the subroutines
     * section
     */
    initialOffset+= (theInterpretedSize + theFinalUpdateSize + theFinalReadSize); 

  }

  theLastLabel->theLabelNo[tLabelIndex] = tLabelNo;
  theLastLabel->theLabelAddress[tLabelIndex] = (theTotalCurrAI_Len + 1) - initialOffset;
  theLastLabel->theSubroutine[tLabelIndex] = theNoOfSubroutines;
  theNoOfLabels++;
  theErrorLine++;

  return (theNoOfLabels - 1);
}

/************************************************************************************************
int NdbOperation::def_subroutine()

************************************************************************************************/
int
NdbOperation::def_subroutine(int tSubNo)
{
  INT_DEBUG(("def_subroutine %d", tSubNo));
  Uint32 tSubroutineIndex;

  if (theInterpretIndicator != 1)
  {
    setErrorCodeAbort(4200);
    return -1;
  }

  if (int(theNoOfSubroutines) != tSubNo)
  {
    setErrorCodeAbort(4227);
    return -1;
  }
  if (theStatus == FinalGetValue)
  {
    theFinalReadSize = theTotalCurrAI_Len -
       (theInitialReadSize + theInterpretedSize + 
        theFinalUpdateSize + AttrInfo::SectionSizeInfoLength);

  } else if (theStatus == SubroutineEnd)
  {
     ; // Correct Status, last call was ret_sub()
  } else if (theStatus == ExecInterpretedValue)
  {
    if (insertATTRINFO(Interpreter::EXIT_OK) == -1)
      return -1;
    theInterpretedSize = theTotalCurrAI_Len - 
      (theInitialReadSize + AttrInfo::SectionSizeInfoLength);
  } else if (theStatus == SetValueInterpreted)
  {
    theFinalUpdateSize = theTotalCurrAI_Len -
      (theInitialReadSize + theInterpretedSize + 
       AttrInfo::SectionSizeInfoLength);

  } else if (theStatus == GetValue)
  {

    theInitialReadSize = theTotalCurrAI_Len - 
      AttrInfo::SectionSizeInfoLength;

  } else
  {
    setErrorCodeAbort(4200);
    return -1;
  }
  theStatus = SubroutineExec;
  tSubroutineIndex = theNoOfSubroutines - ((theNoOfSubroutines >> 4) << 4);
  if (tSubroutineIndex == 0)
  {
    NdbSubroutine* tNdbSubroutine = theNdb->getNdbSubroutine();
    if (tNdbSubroutine == nullptr)
    {
      setErrorCodeAbort(4000);
      return -1;
    }
    if (theFirstSubroutine == nullptr)
      theFirstSubroutine = tNdbSubroutine;
    else
      theLastSubroutine->theNext = tNdbSubroutine;

    theLastSubroutine = tNdbSubroutine;
    tNdbSubroutine->theNext = nullptr;
  }
  theLastSubroutine->theSubroutineAddress[tSubroutineIndex] = theTotalCurrAI_Len - 
    (AttrInfo::SectionSizeInfoLength + theInitialReadSize + theInterpretedSize + 
     theFinalUpdateSize + theFinalReadSize); // Preceding sections + sizes array

  theNoOfSubroutines++;
  theErrorLine++;
  return (theNoOfSubroutines - 1);
}

/************************************************************************************************
int NdbOperation::add_reg(Uint32 RegSource1, Uint32 RegSource2, Uint32 RegDest)

************************************************************************************************/
int
NdbOperation::add_reg(Uint32 RegSource1, Uint32 RegSource2, Uint32 RegDest)
{
  INT_DEBUG(("add_reg %u %u %u", RegSource1, RegSource2, RegDest));
  if (intermediate_interpreterCheck() == -1)
    return -1;

  if (RegSource1 >= 8)
  {
    setErrorCodeAbort(4229);
    return -1;
  }
  if (RegSource2 >= 8)
  {
    setErrorCodeAbort(4229);
    return -1;
  }
  if (RegDest >= 8)
  {
    setErrorCodeAbort(4229);
    return -1;
  }
  if (insertATTRINFO( Interpreter::Add(RegDest, RegSource1, RegSource2)) == -1)
    return -1;
  theErrorLine++;
  return 0;
}

/************************************************************************************************
int NdbOperation::sub_reg(Uint32 RegSource1, Uint32 RegSource2, Uint32 RegDest)

************************************************************************************************/
int
NdbOperation::sub_reg(Uint32 RegSource1, Uint32 RegSource2, Uint32 RegDest)
{
  INT_DEBUG(("sub_reg %u %u %u", RegSource1, RegSource2, RegDest));
  if (intermediate_interpreterCheck() == -1)
    return -1;

  if (RegSource1 >= 8)
  {
    setErrorCodeAbort(4229);
    return -1;
  }
  if (RegSource2 >= 8)
  {
    setErrorCodeAbort(4229);
    return -1;
  }
  if (RegDest >= 8)
  {
    setErrorCodeAbort(4229);
    return -1;
  }
  if (insertATTRINFO( Interpreter::Sub(RegDest, RegSource1, RegSource2)) == -1)
    return -1;
  theErrorLine++;
  return 0;
}

/************************************************************************************************
int NdbOperation::load_const_u32(Uint32 RegDest, Uint32 Constant)

************************************************************************************************/
int
NdbOperation::load_const_u32(Uint32 RegDest, Uint32 Constant)
{
  INT_DEBUG(("load_const_u32 %u %u", RegDest, Constant));
  if (initial_interpreterCheck() == -1)
    goto l_u32_error1;
  if (RegDest >= 8)
    goto l_u32_error2;
  if (insertATTRINFO( Interpreter::LoadConst32(RegDest)) == -1)
    goto l_u32_error1;
  if (insertATTRINFO(Constant) == -1)
    goto l_u32_error1;
  theErrorLine++;
  return 0;

 l_u32_error1:
  return -1;
  
 l_u32_error2:
  setErrorCodeAbort(4229);
  return -1;
}

/************************************************************************************************
int NdbOperation::load_const_u64(Uint32 RegDest, Uint64 Constant)

************************************************************************************************/
int
NdbOperation::load_const_u64(Uint32 RegDest, Uint64 Constant)
{
  INT_DEBUG(("load_const_u64 %u %llu", RegDest, Constant));
  if (initial_interpreterCheck() == -1)
    return -1;
  if (RegDest >= 8)
  {
    setErrorCodeAbort(4229);
    return -1;
  }
  
  // 64 bit value
  if (insertATTRINFO( Interpreter::LoadConst64(RegDest)) == -1)
    return -1;
  if (insertATTRINFOloop((Uint32*)&Constant, 2) == -1)
    return -1;
  theErrorLine++;
  return 0;
}

/************************************************************************************************
int NdbOperation::load_const_null(Uint32 RegDest)

************************************************************************************************/
int
NdbOperation::load_const_null(Uint32 RegDest)
{
  INT_DEBUG(("load_const_null %u", RegDest));
  if (initial_interpreterCheck() == -1)
    return -1;
  if (RegDest >= 8)
  {
    setErrorCodeAbort(4229);
    return -1;
  }
  if (insertATTRINFO( Interpreter::LOAD_CONST_NULL) == -1)
    return -1;
  theErrorLine++;
  return 0;
}

/************************************************************************************************
int NdbOperation::read_attr(const char* anAttrName, Uint32 RegDest)

************************************************************************************************/
int
NdbOperation::read_attr(const NdbColumnImpl* anAttrObject, Uint32 RegDest)
{
  INT_DEBUG(("read_attr %d %u", anAttrObject->m_attrId, RegDest));
  if (initial_interpreterCheck() == -1)
    return -1;

  int tAttrId = read_attrCheck(anAttrObject);
  if (tAttrId == -1)
    goto read_attr_error1;
  if (RegDest >= 8)
    goto read_attr_error2;
  if (insertATTRINFO( Interpreter::Read(tAttrId, RegDest)) != -1) {
    return 0;
    theErrorLine++;
  }//if
  return -1;

read_attr_error1:
  return -1;

read_attr_error2:
  setErrorCodeAbort(4229);
  return -1;
}

/************************************************************************************************
int NdbOperation::write_attr(const char* anAttrName, Uint32 RegSource)

************************************************************************************************/
int
NdbOperation::write_attr(const NdbColumnImpl* anAttrObject, Uint32 RegSource)
{
  INT_DEBUG(("write_attr %d %u", anAttrObject->m_attrId, RegSource));
  int tAttrId = write_attrCheck(anAttrObject);
  if (tAttrId == -1)
    return -1;
  if (insertATTRINFO( Interpreter::Write(tAttrId, RegSource)) == -1)
    return -1;
  theErrorLine++;
  return 0;
}

int
NdbOperation::branch_reg_reg(Uint32 type,
			     Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label)
{
  if (intermediate_interpreterCheck() == -1)
    return -1;
  if (insertATTRINFO(Interpreter::Branch(type, RegLvalue, RegRvalue)) == -1)
    return -1;
  if (insertBranch(Label) == -1)
    return -1;
  theErrorLine++;
  return 0;
}

int
NdbOperation::branch_ge(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label)
{
  INT_DEBUG(("branch_ge %u %u %u", RegLvalue, RegRvalue, Label));
  return branch_reg_reg(Interpreter::BRANCH_GE_REG_REG,
			RegLvalue, RegRvalue, Label);
}

int
NdbOperation::branch_gt(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label)
{
  INT_DEBUG(("branch_gt %u %u %u", RegLvalue, RegRvalue, Label));
  return branch_reg_reg(Interpreter::BRANCH_GT_REG_REG,
			RegLvalue, RegRvalue, Label);
}

int
NdbOperation::branch_le(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label)
{
  INT_DEBUG(("branch_le %u %u %u", RegLvalue, RegRvalue, Label));
  return branch_reg_reg(Interpreter::BRANCH_LE_REG_REG,
			RegLvalue, RegRvalue, Label);
}

int
NdbOperation::branch_lt(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label)
{
  INT_DEBUG(("branch_lt %u %u %u", RegLvalue, RegRvalue, Label));
  return branch_reg_reg(Interpreter::BRANCH_LT_REG_REG,
			RegLvalue, RegRvalue, Label);
}

int
NdbOperation::branch_eq(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label)
{
  INT_DEBUG(("branch_eq %u %u %u", RegLvalue, RegRvalue, Label));
  return branch_reg_reg(Interpreter::BRANCH_EQ_REG_REG,
			RegLvalue, RegRvalue, Label);
}

int
NdbOperation::branch_ne(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label)
{
  INT_DEBUG(("branch_ne %u %u %u", RegLvalue, RegRvalue, Label));
  return branch_reg_reg(Interpreter::BRANCH_NE_REG_REG,
			RegLvalue, RegRvalue, Label);
}

int
NdbOperation::branch_ne_null(Uint32 RegLvalue, Uint32 Label)
{
  INT_DEBUG(("branch_ne_null %u %u", RegLvalue, Label));
  if (intermediate_interpreterCheck() == -1)
    return -1;
  if (insertATTRINFO((RegLvalue << 6) + Interpreter::BRANCH_REG_NE_NULL) == -1)
    return -1;
  if (insertBranch(Label) == -1)
    return -1;
  theErrorLine++;
  return 0;
}

int
NdbOperation::branch_eq_null(Uint32 RegLvalue, Uint32 Label)
{
  INT_DEBUG(("branch_eq_null %u %u", RegLvalue, Label));
  if (intermediate_interpreterCheck() == -1)
    return -1;
  if (insertATTRINFO((RegLvalue << 6) + Interpreter::BRANCH_REG_EQ_NULL) == -1)
    return -1;
  if (insertBranch(Label) == -1)
    return -1;
  theErrorLine++;
  return 0;
}

int
NdbOperation::branch_label(Uint32 Label)
{
  INT_DEBUG(("branch_label %u", Label));
  if (initial_interpreterCheck() == -1)
    return -1;
  if (insertATTRINFO(Interpreter::BRANCH) == -1)
    return -1;
  if (insertBranch(Label) == -1)
    return -1;
  theErrorLine++;
  return 0;
}

/************************************************************************************************
int NdbOperation::interpret_exit_ok()

************************************************************************************************/
int
NdbOperation::interpret_exit_ok()
{
  INT_DEBUG(("interpret_exit_ok"));
  if (initial_interpreterCheck() == -1)
    return -1;
  if (insertATTRINFO(Interpreter::EXIT_OK) == -1)
    return -1;
  theErrorLine++;
  return 0;
}

int
NdbOperation::interpret_exit_last_row()
{
  INT_DEBUG(("interpret_exit_last_row"));
  if (initial_interpreterCheck() == -1)
    return -1;
  if (insertATTRINFO(Interpreter::EXIT_OK_LAST) == -1)
    return -1;
  theErrorLine++;
  return 0;
}

/************************************************************************************************
int NdbOperation::interpret_exit_nok(Uint32 ErrorCode)

************************************************************************************************/
int
NdbOperation::interpret_exit_nok(Uint32 ErrorCode)
{
  INT_DEBUG(("interpret_exit_nok %u", ErrorCode));
  if (initial_interpreterCheck() == -1)
    return -1;
  if (insertATTRINFO( (ErrorCode << 16) + Interpreter::EXIT_REFUSE) == -1)
    return -1;
  theErrorLine++;
  return 0;
}

int
NdbOperation::interpret_exit_nok()
{
  INT_DEBUG(("interpret_exit_nok"));
  /**
   * 899 is used here for historical reasons. Observe that this collides with 
   * "Rowid already allocated" (see ndberror.c).
   */
  Uint32 ErrorCode = 899;

  if (initial_interpreterCheck() == -1)
    return -1;
  if (insertATTRINFO( (ErrorCode << 16) + Interpreter::EXIT_REFUSE) == -1)
    return -1;
  theErrorLine++;
  return 0;
}

int
NdbOperation::call_sub(Uint32 Subroutine)
{
  INT_DEBUG(("call_sub %u", Subroutine));
  if (initial_interpreterCheck() == -1)
    return -1;
  if (insertATTRINFO( (Subroutine << 16) + Interpreter::CALL) == -1)
    return -1;
  if (insertCall(Subroutine) == -1)
    return -1;
  theErrorLine++;
  return 0;
}

int
NdbOperation::ret_sub()
{
  INT_DEBUG(("ret_sub"));
  if (theInterpretIndicator != 1)
  {
    setErrorCodeAbort(4200);
    return -1;
  }
  if (theStatus == SubroutineExec)
  {
     ; // Simply continue with interpretation
  } else
  {
    setErrorCodeAbort(4200);
    return -1;
  }
  if (insertATTRINFO(Interpreter::RETURN) == -1)
    return -1;
  theStatus = SubroutineEnd;
  theErrorLine++;
  return 0;
}

int
NdbOperation::insertBranch(Uint32 aLabel)
{
  Uint32 tAddress;
  NdbBranch* tBranch = theNdb->getNdbBranch();
  if (tBranch == nullptr)
    goto insertBranch_error1;
  if (theFirstBranch == nullptr)
    theFirstBranch = tBranch;
  else
    theLastBranch->theNext = tBranch;
  theLastBranch = tBranch;
  if (theNoOfSubroutines == 0)
    tAddress = theTotalCurrAI_Len - 
      (theInitialReadSize + AttrInfo::SectionSizeInfoLength);
  else
    tAddress = theTotalCurrAI_Len - 
      (theInitialReadSize + theInterpretedSize +
       theFinalUpdateSize + theFinalReadSize + AttrInfo::SectionSizeInfoLength);

  tBranch->theBranchAddress = tAddress;
  tBranch->theSignal = theCurrentATTRINFO;
  tBranch->theSignalAddress = theAI_LenInCurrAI; // + 1;  theAI_LenInCurrAI has already been updated in
  tBranch->theSubroutine = theNoOfSubroutines;   //       insertATTRINFO which was done before insertBranch!!
  tBranch->theBranchLabel = aLabel;
  return 0;

insertBranch_error1:
  setErrorCodeAbort(4000);
  return -1;
}

int
NdbOperation::insertCall(Uint32 aCall)
{
  NdbCall* tCall = theNdb->getNdbCall();
  if (tCall == nullptr)
  {
    setErrorCodeAbort(4000);
    return -1;
  }
  if (theFirstCall == nullptr)
    theFirstCall = tCall;
  else
    theLastCall->theNext = tCall;
  theLastCall = tCall;

  tCall->theSignal = theCurrentATTRINFO;
  tCall->theSignalAddress = theAI_LenInCurrAI;
  tCall->theSubroutine = aCall;
  return 0;
}

int
NdbOperation::branch_col(Uint32 type, 
			 Uint32 ColId, const void * val, Uint32 len, 
			 Uint32 Label){

  DBUG_ENTER("NdbOperation::branch_col");
  DBUG_PRINT("enter", ("type: %u  col:%u  val: %p  len: %u  label: %u",
                       type, ColId, val, len, Label));
  if (val != nullptr) DBUG_DUMP("value", (const uchar*)val, len);

  if (initial_interpreterCheck() == -1)
    DBUG_RETURN(-1);

  Interpreter::BinaryCondition c = (Interpreter::BinaryCondition)type;
  
  const NdbColumnImpl * col = 
    m_currentTable->getColumn(ColId);
  
  if(col == nullptr){
    abort();
  }

  Uint32 lastWordMask= ~0;
  if (val == nullptr)
    len = 0;
  else {
    if (! col->getStringType())
    {
      /* Fixed size type */
      if (col->getType() == NDB_TYPE_BIT)
      {
        /* We want to zero out insignificant bits in the
         * last word of a bit type
         */
        Uint32 bitLen= col->getLength();
        Uint32 lastWordBits= bitLen & 0x1F;
        if (lastWordBits)
          lastWordMask= (1 << lastWordBits) -1;
      }
      len= col->m_attrSize * col->m_arraySize;
    }
    else
    {
      /* For Like and Not like we must use the passed in 
       * length.  Otherwise we use the length encoded
       * in the passed string
       */
      if ((type != Interpreter::LIKE) &&
          (type != Interpreter::NOT_LIKE))
      {
        if (! col->get_var_length(val, len))
        {
          setErrorCodeAbort(4209);
          DBUG_RETURN(-1);
        }
      }
    }
  }

  if (col->m_storageType == NDB_STORAGETYPE_DISK)
  {
    m_flags &= ~(Uint8)OF_NO_DISK;
  }

  Uint32 tempData[ NDB_MAX_TUPLE_SIZE_IN_WORDS ];
  if (((UintPtr)val & 3) != 0) {
    memcpy(tempData, val, len);
    val = tempData;
  }

  const Interpreter::NullSemantics nulls = Interpreter::NULL_CMP_EQUAL;
  if (insertATTRINFO(Interpreter::BranchCol(c, nulls)) == -1)
    DBUG_RETURN(-1);
  
  if (insertBranch(Label) == -1)
    DBUG_RETURN(-1);
  
  if (insertATTRINFO(Interpreter::BranchCol_2(col->m_attrId, len)))
    DBUG_RETURN(-1);
  
  Uint32 len2 = Interpreter::mod4(len);
  if((len2 == len) &&
     (lastWordMask == (Uint32)~0)){
    insertATTRINFOloop((const Uint32*)val, len2 >> 2);
  } else {
    len2 -= 4;
    insertATTRINFOloop((const Uint32*)val, len2 >> 2);
    Uint32 tmp = 0;
    for (Uint32 i = 0; i < len-len2; i++) {
      char* p = (char*)&tmp;
      p[i] = ((const char*)val)[len2 + i];
    }
    insertATTRINFO(tmp & lastWordMask);
  }
  
  theErrorLine++;
  DBUG_RETURN(0);
}

int NdbOperation::branch_col_eq(Uint32 ColId, const void* val, Uint32 len, bool,
                                Uint32 Label)
{
  INT_DEBUG(("branch_col_eq %u %.*s(%u) -> %u", ColId, len, (char*)val, len, Label));
  return branch_col(Interpreter::EQ, ColId, val, len, Label);
}

int NdbOperation::branch_col_ne(Uint32 ColId, const void* val, Uint32 len, bool,
                                Uint32 Label)
{
  INT_DEBUG(("branch_col_ne %u %.*s(%u) -> %u", ColId, len, (char*)val, len, Label));
  return branch_col(Interpreter::NE, ColId, val, len, Label);
}
int NdbOperation::branch_col_lt(Uint32 ColId, const void* val, Uint32 len, bool,
                                Uint32 Label)
{
  INT_DEBUG(("branch_col_lt %u %.*s(%u) -> %u", ColId, len, (char*)val, len, Label));
  return branch_col(Interpreter::LT, ColId, val, len, Label);
}
int NdbOperation::branch_col_le(Uint32 ColId, const void* val, Uint32 len, bool,
                                Uint32 Label)
{
  INT_DEBUG(("branch_col_le %u %.*s(%u) -> %u", ColId, len, (char*)val, len, Label));
  return branch_col(Interpreter::LE, ColId, val, len, Label);
}
int NdbOperation::branch_col_gt(Uint32 ColId, const void* val, Uint32 len, bool,
                                Uint32 Label)
{
  INT_DEBUG(("branch_col_gt %u %.*s(%u) -> %u", ColId, len, (char*)val, len, Label));
  return branch_col(Interpreter::GT, ColId, val, len, Label);
}

int NdbOperation::branch_col_ge(Uint32 ColId, const void* val, Uint32 len, bool,
                                Uint32 Label)
{
  INT_DEBUG(("branch_col_ge %u %.*s(%u) -> %u", ColId, len, (char*)val, len, Label));
  return branch_col(Interpreter::GE, ColId, val, len, Label);
}

int NdbOperation::branch_col_like(Uint32 ColId, const void* val, Uint32 len,
                                  bool, Uint32 Label)
{
  INT_DEBUG(("branch_col_like %u %.*s(%u) -> %u", ColId, len, (char*)val, len,
             Label));
  return branch_col(Interpreter::LIKE, ColId, val, len, Label);
}

int NdbOperation::branch_col_notlike(Uint32 ColId, const void* val, Uint32 len,
                                     bool, Uint32 Label)
{
  INT_DEBUG(("branch_col_notlike %u %.*s(%u,%d) -> %u", ColId, len, (char*)val,
             len, Label));
  return branch_col(Interpreter::NOT_LIKE, ColId, val, len, Label);
}

int NdbOperation::branch_col_and_mask_eq_mask(Uint32 ColId, const void* mask,
                                              Uint32 len, bool, Uint32 Label)
{
  INT_DEBUG(("branch_col_and_mask_eq_mask %u %.*s(%u) -> %u", ColId, len,
             (char*)mask, len, Label));
  return branch_col(Interpreter::AND_EQ_MASK, ColId, mask, len, Label);
}

int NdbOperation::branch_col_and_mask_ne_mask(Uint32 ColId, const void* mask,
                                              Uint32 len, bool, Uint32 Label)
{
  INT_DEBUG(("branch_col_and_mask_ne_mask %u %.*s(%u) -> %u", ColId, len,
             (char*)mask, len, Label));
  return branch_col(Interpreter::AND_NE_MASK, ColId, mask, len, Label);
}

int NdbOperation::branch_col_and_mask_eq_zero(Uint32 ColId, const void* mask,
                                              Uint32 len, bool, Uint32 Label)
{
  INT_DEBUG(("branch_col_and_mask_eq_zero %u %.*s(%u) -> %u", ColId, len,
             (char*)mask, len, Label));
  return branch_col(Interpreter::AND_EQ_ZERO, ColId, mask, len, Label);
}

int NdbOperation::branch_col_and_mask_ne_zero(Uint32 ColId, const void* mask,
                                              Uint32 len, bool, Uint32 Label)
{
  INT_DEBUG(("branch_col_and_mask_ne_zero %u %.*s(%u) -> %u", ColId, len,
             (char*)mask, len, Label));
  return branch_col(Interpreter::AND_NE_ZERO, ColId, mask, len, Label);
}

int
NdbOperation::branch_col_null(Uint32 type, Uint32 ColId, Uint32 Label){
  
  if (initial_interpreterCheck() == -1)
    return -1;
  
  if (insertATTRINFO(type) == -1)
    return -1;
  
  if (insertBranch(Label) == -1)
    return -1;

  Uint32 attrId= 
    NdbColumnImpl::getImpl(* m_currentTable->getColumn(ColId)).m_attrId;
  
  if (insertATTRINFO(Interpreter::BranchCol_2(attrId)))
    return -1;
  
  theErrorLine++;
  return 0;
}

int
NdbOperation::branch_col_eq_null(Uint32 ColId, Uint32 Label){
  
  INT_DEBUG(("branch_col_eq_null %u -> %u", ColId, Label));
  return branch_col_null(Interpreter::BRANCH_ATTR_EQ_NULL, ColId, Label);
}

int
NdbOperation::branch_col_ne_null(Uint32 ColId, Uint32 Label){
  
  INT_DEBUG(("branch_col_ne_null %u -> %u", ColId, Label));
  return branch_col_null(Interpreter::BRANCH_ATTR_NE_NULL, ColId, Label);
}

