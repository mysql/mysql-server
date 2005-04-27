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


#define DBTUP_C
#include "Dbtup.hpp"
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <pc.hpp>

#define ljam() { jamLine(22000 + __LINE__); }
#define ljamEntry() { jamEntryLine(22000 + __LINE__); }

/* **************************************************************** */
/* *********** TABLE DESCRIPTOR MEMORY MANAGER ******************** */
/* **************************************************************** */
/* This module is used to allocate and deallocate table descriptor  */
/* memory attached to fragments (could be allocated per table       */
/* instead. Performs its task by a buddy algorithm.                 */
/* **************************************************************** */

Uint32
Dbtup::getTabDescrOffsets(const Tablerec* regTabPtr, Uint32* offset)
{
  // belongs to configure.in
  unsigned sizeOfPointer = sizeof(CHARSET_INFO*);
  ndbrequire((sizeOfPointer & 0x3) == 0);
  sizeOfPointer = (sizeOfPointer >> 2);
  // do in layout order and return offsets (see DbtupMeta.cpp)
  Uint32 allocSize = 0;
  // magically aligned to 8 bytes
  offset[0] = allocSize += ZTD_SIZE;
  offset[1] = allocSize += regTabPtr->noOfAttr * sizeOfReadFunction();
  offset[2] = allocSize += regTabPtr->noOfAttr * sizeOfReadFunction();
  offset[3] = allocSize += regTabPtr->noOfCharsets * sizeOfPointer;
  offset[4] = allocSize += regTabPtr->noOfKeyAttr;
  offset[5] = allocSize += regTabPtr->noOfAttributeGroups;
  allocSize += regTabPtr->noOfAttr * ZAD_SIZE;
  allocSize += ZTD_TRAILER_SIZE;
  // return number of words
  return allocSize;
}

Uint32 Dbtup::allocTabDescr(const Tablerec* regTabPtr, Uint32* offset)
{
  Uint32 reference = RNIL;
  Uint32 allocSize = getTabDescrOffsets(regTabPtr, offset);
/* ---------------------------------------------------------------- */
/*       ALWAYS ALLOCATE A MULTIPLE OF 16 BYTES                     */
/* ---------------------------------------------------------------- */
  allocSize = (((allocSize - 1) >> 4) + 1) << 4;
  Uint32 list = nextHigherTwoLog(allocSize - 1);	/* CALCULATE WHICH LIST IT BELONGS TO     */
  for (Uint32 i = list; i < 16; i++) {
    ljam();
    if (cfreeTdList[i] != RNIL) {
      ljam();
      reference = cfreeTdList[i];
      removeTdArea(reference, i);	                /* REMOVE THE AREA FROM THE FREELIST      */
      Uint32 retNo = (1 << i) - allocSize;	        /* CALCULATE THE DIFFERENCE               */
      if (retNo >= ZTD_FREE_SIZE) {
        ljam();
        Uint32 retRef = reference + allocSize;          /* SET THE RETURN POINTER                 */
        retNo = itdaMergeTabDescr(retRef, retNo);       /* MERGE WITH POSSIBLE RIGHT NEIGHBOURS   */
        freeTabDescr(retRef, retNo);	                /* RETURN UNUSED TD SPACE TO THE TD AREA  */
      } else {
        ljam();
        allocSize = 1 << i;
      }//if
      break;
    }//if
  }//for
  if (reference == RNIL) {
    ljam();
    terrorCode = ZMEM_NOTABDESCR_ERROR;
    return RNIL;
  } else {
    ljam();
    setTabDescrWord((reference + allocSize) - ZTD_TR_TYPE, ZTD_TYPE_NORMAL);
    setTabDescrWord(reference + ZTD_DATASIZE, allocSize);

     /* INITIALIZE THE TRAILER RECORD WITH TYPE AND SIZE     */
     /* THE TRAILER IS USED TO SIMPLIFY MERGE OF FREE AREAS  */

    setTabDescrWord(reference + ZTD_HEADER, ZTD_TYPE_NORMAL);
    setTabDescrWord((reference + allocSize) - ZTD_TR_SIZE, allocSize);
    return reference;
  }//if
}//Dbtup::allocTabDescr()

void Dbtup::freeTabDescr(Uint32 retRef, Uint32 retNo) 
{
  while (retNo >= ZTD_FREE_SIZE) {
    ljam();
    Uint32 list = nextHigherTwoLog(retNo);
    list--;	/* RETURN TO NEXT LOWER LIST    */
    Uint32 sizeOfChunk = 1 << list;
    insertTdArea(sizeOfChunk, retRef, list);
    retRef += sizeOfChunk;
    retNo -= sizeOfChunk;
  }//while
}//Dbtup::freeTabDescr()

Uint32
Dbtup::getTabDescrWord(Uint32 index)
{
  ndbrequire(index < cnoOfTabDescrRec);
  return tableDescriptor[index].tabDescr;
}//Dbtup::getTabDescrWord()

void
Dbtup::setTabDescrWord(Uint32 index, Uint32 word)
{
  ndbrequire(index < cnoOfTabDescrRec);
  tableDescriptor[index].tabDescr = word;
}//Dbtup::setTabDescrWord()

void Dbtup::insertTdArea(Uint32 sizeOfChunk, Uint32 tabDesRef, Uint32 list) 
{
  ndbrequire(list < 16);
  setTabDescrWord(tabDesRef + ZTD_FL_HEADER, ZTD_TYPE_FREE);
  setTabDescrWord(tabDesRef + ZTD_FL_NEXT, cfreeTdList[list]);
  if (cfreeTdList[list] != RNIL) {
    ljam();                                                /* PREVIOUSLY EMPTY SLOT     */
    setTabDescrWord(cfreeTdList[list] + ZTD_FL_PREV, tabDesRef);
  }//if
  cfreeTdList[list] = tabDesRef;	/* RELINK THE LIST           */

  setTabDescrWord(tabDesRef + ZTD_FL_PREV, RNIL);
  setTabDescrWord(tabDesRef + ZTD_FL_SIZE, 1 << list);
  setTabDescrWord((tabDesRef + (1 << list)) - ZTD_TR_TYPE, ZTD_TYPE_FREE);
  setTabDescrWord((tabDesRef + (1 << list)) - ZTD_TR_SIZE, 1 << list);
}//Dbtup::insertTdArea()

/* ---------------------------------------------------------------- */
/* ----------------------- MERGE_TAB_DESCR ------------------------ */
/* ---------------------------------------------------------------- */
/* INPUT:  TAB_DESCR_PTR   POINTING AT THE CURRENT CHUNK            */
/*                                                                  */
/* SHORTNAME:   MTD                                                 */
/* -----------------------------------------------------------------*/
Uint32 Dbtup::itdaMergeTabDescr(Uint32 retRef, Uint32 retNo) 
{
   /* THE SIZE OF THE PART TO MERGE MUST BE OF THE SAME SIZE AS THE INSERTED PART */
   /* THIS IS TRUE EITHER IF ONE PART HAS THE SAME SIZE OR THE SUM OF BOTH PARTS  */
   /* TOGETHER HAS THE SAME SIZE AS THE PART TO BE INSERTED                       */
   /* FIND THE SIZES OF THE PARTS TO THE RIGHT OF THE PART TO BE REINSERTED */
  while ((retRef + retNo) < cnoOfTabDescrRec) {
    ljam();
    Uint32 tabDesRef = retRef + retNo;
    Uint32 headerWord = getTabDescrWord(tabDesRef + ZTD_FL_HEADER);
    if (headerWord == ZTD_TYPE_FREE) {
      ljam();
      Uint32 sizeOfMergedPart = getTabDescrWord(tabDesRef + ZTD_FL_SIZE);

      retNo += sizeOfMergedPart;
      Uint32 list = nextHigherTwoLog(sizeOfMergedPart - 1);
      removeTdArea(tabDesRef, list);
    } else {
      ljam();
      return retNo;
    }//if
  }//while
  ndbrequire((retRef + retNo) == cnoOfTabDescrRec);
  return retNo;
}//Dbtup::itdaMergeTabDescr()

/* ---------------------------------------------------------------- */
/* ------------------------ REMOVE_TD_AREA ------------------------ */
/* ---------------------------------------------------------------- */
/*                                                                  */
/* THIS ROUTINE REMOVES A TD CHUNK FROM THE POOL OF TD RECORDS      */
/*                                                                  */
/* INPUT:  TLIST          LIST TO USE                               */
/*         TAB_DESCR_PTR  POINTS TO THE CHUNK TO BE REMOVED         */
/*                                                                  */
/* SHORTNAME:   RMTA                                                */
/* -----------------------------------------------------------------*/
void Dbtup::removeTdArea(Uint32 tabDesRef, Uint32 list) 
{
  ndbrequire(list < 16);
  Uint32 tabDescrNextPtr = getTabDescrWord(tabDesRef + ZTD_FL_NEXT);
  Uint32 tabDescrPrevPtr = getTabDescrWord(tabDesRef + ZTD_FL_PREV);

  setTabDescrWord(tabDesRef + ZTD_HEADER, ZTD_TYPE_NORMAL);
  setTabDescrWord((tabDesRef + (1 << list)) - ZTD_TR_TYPE, ZTD_TYPE_NORMAL);

  if (tabDesRef == cfreeTdList[list]) {
    ljam();
    cfreeTdList[list] = tabDescrNextPtr;	/* RELINK THE LIST           */
  }//if
  if (tabDescrNextPtr != RNIL) {
    ljam();
    setTabDescrWord(tabDescrNextPtr + ZTD_FL_PREV, tabDescrPrevPtr);
  }//if
  if (tabDescrPrevPtr != RNIL) {
    ljam();
    setTabDescrWord(tabDescrPrevPtr + ZTD_FL_NEXT, tabDescrNextPtr);
  }//if
}//Dbtup::removeTdArea()
