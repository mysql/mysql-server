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

/*
 * TABLE DESCRIPTOR MEMORY MANAGER
 *
 * Each table has a descriptor which is a contiguous array of words.
 * The descriptor is allocated from a global array using a buddy
 * algorithm.  Free lists exist for each power of 2 words.  Freeing
 * a piece first merges with free right and left neighbours and then
 * divides itself up into free list chunks.
 */

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
  offset[1] = allocSize += regTabPtr->m_no_of_attributes* sizeOfReadFunction();
  offset[2] = allocSize += regTabPtr->m_no_of_attributes* sizeOfReadFunction();
  offset[3] = allocSize += regTabPtr->noOfCharsets * sizeOfPointer;
  offset[4] = allocSize += regTabPtr->noOfKeyAttr;
  offset[5] = allocSize += regTabPtr->m_no_of_attributes * ZAD_SIZE;
  offset[6] = allocSize += (regTabPtr->m_no_of_attributes + 1) >> 1; // real order
  allocSize += ZTD_TRAILER_SIZE;
  // return number of words
  return allocSize;
}

Uint32 Dbtup::allocTabDescr(const Tablerec* regTabPtr, Uint32* offset)
{
  Uint32 reference = RNIL;
  Uint32 allocSize = getTabDescrOffsets(regTabPtr, offset);
/* ---------------------------------------------------------------- */
/*       ALWAYS ALLOCATE A MULTIPLE OF 16 WORDS                     */
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
        // return unused words, of course without attempting left merge
        Uint32 retRef = reference + allocSize;
        freeTabDescr(retRef, retNo, false);
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

void Dbtup::freeTabDescr(Uint32 retRef, Uint32 retNo, bool normal)
{
  itdaMergeTabDescr(retRef, retNo, normal);       /* MERGE WITH POSSIBLE NEIGHBOURS   */
  while (retNo >= ZTD_FREE_SIZE) {
    ljam();
    Uint32 list = nextHigherTwoLog(retNo);
    list--;	/* RETURN TO NEXT LOWER LIST    */
    Uint32 sizeOfChunk = 1 << list;
    insertTdArea(retRef, list);
    retRef += sizeOfChunk;
    retNo -= sizeOfChunk;
  }//while
  ndbassert(retNo == 0);
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

void Dbtup::insertTdArea(Uint32 tabDesRef, Uint32 list) 
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

/*
 * Merge to-be-removed chunk (which need not be initialized with header
 * and trailer) with left and right buddies.  The start point retRef
 * moves to left and the size retNo increases to match the new chunk.
 */
void Dbtup::itdaMergeTabDescr(Uint32& retRef, Uint32& retNo, bool normal)
{
  // merge right
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
      break;
    }
  }
  // merge left
  const bool mergeLeft = normal;
  while (mergeLeft && retRef > 0) {
    ljam();
    Uint32 trailerWord = getTabDescrWord(retRef - ZTD_TR_TYPE);
    if (trailerWord == ZTD_TYPE_FREE) {
      ljam();
      Uint32 sizeOfMergedPart = getTabDescrWord(retRef - ZTD_TR_SIZE);
      ndbrequire(retRef >= sizeOfMergedPart);
      retRef -= sizeOfMergedPart;
      retNo += sizeOfMergedPart;
      Uint32 list = nextHigherTwoLog(sizeOfMergedPart - 1);
      removeTdArea(retRef, list);
    } else {
      ljam();
      break;
    }
  }
  ndbrequire((retRef + retNo) <= cnoOfTabDescrRec);
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

#ifdef VM_TRACE
void
Dbtup::verifytabdes()
{
  struct WordType {
    short fl;   // free list 0-15
    short ti;   // table id
    WordType() : fl(-1), ti(-1) {}
  };
  WordType* wt = new WordType [cnoOfTabDescrRec];
  uint free_frags = 0;
  // free lists
  {
    for (uint i = 0; i < 16; i++) {
      Uint32 desc2 = RNIL;
      Uint32 desc = cfreeTdList[i];
      while (desc != RNIL) {
        const Uint32 size = (1 << i);
        ndbrequire(size >= ZTD_FREE_SIZE);
        ndbrequire(desc + size <= cnoOfTabDescrRec);
        { Uint32 index = desc + ZTD_FL_HEADER;
          ndbrequire(tableDescriptor[index].tabDescr == ZTD_TYPE_FREE);
        }
        { Uint32 index = desc + ZTD_FL_SIZE;
          ndbrequire(tableDescriptor[index].tabDescr == size);
        }
        { Uint32 index = desc + size - ZTD_TR_TYPE;
          ndbrequire(tableDescriptor[index].tabDescr == ZTD_TYPE_FREE);
        }
        { Uint32 index = desc + size - ZTD_TR_SIZE;
          ndbrequire(tableDescriptor[index].tabDescr == size);
        }
        { Uint32 index = desc + ZTD_FL_PREV;
          ndbrequire(tableDescriptor[index].tabDescr == desc2);
        }
        for (uint j = 0; j < size; j++) {
          ndbrequire(wt[desc + j].fl == -1);
          wt[desc + j].fl = i;
        }
        desc2 = desc;
        desc = tableDescriptor[desc + ZTD_FL_NEXT].tabDescr;
        free_frags++;
      }
    }
  }
  // tables
  {
    for (uint i = 0; i < cnoOfTablerec; i++) {
      TablerecPtr ptr;
      ptr.i = i;
      ptrAss(ptr, tablerec);
      if (ptr.p->tableStatus == DEFINED) {
        Uint32 offset[10];
        const Uint32 alloc = getTabDescrOffsets(ptr.p, offset);
        const Uint32 desc = ptr.p->readKeyArray - offset[3];
        Uint32 size = alloc;
        if (size % ZTD_FREE_SIZE != 0)
          size += ZTD_FREE_SIZE - size % ZTD_FREE_SIZE;
        ndbrequire(desc + size <= cnoOfTabDescrRec);
        { Uint32 index = desc + ZTD_FL_HEADER;
          ndbrequire(tableDescriptor[index].tabDescr == ZTD_TYPE_NORMAL);
        }
        { Uint32 index = desc + ZTD_FL_SIZE;
          ndbrequire(tableDescriptor[index].tabDescr == size);
        }
        { Uint32 index = desc + size - ZTD_TR_TYPE;
          ndbrequire(tableDescriptor[index].tabDescr == ZTD_TYPE_NORMAL);
        }
        { Uint32 index = desc + size - ZTD_TR_SIZE;
          ndbrequire(tableDescriptor[index].tabDescr == size);
        }
        for (uint j = 0; j < size; j++) {
          ndbrequire(wt[desc + j].ti == -1);
          wt[desc + j].ti = i;
        }
      }
    }
  }
  // all words
  {
    for (uint i = 0; i < cnoOfTabDescrRec; i++) {
      bool is_fl = wt[i].fl != -1;
      bool is_ti = wt[i].ti != -1;
      ndbrequire(is_fl != is_ti);
    }
  }
  delete [] wt;
  ndbout << "verifytabdes: frags=" << free_frags << endl;
}
#endif
