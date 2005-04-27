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

#ifndef PC_H
#define PC_H


#include "Emulator.hpp"
#include <NdbOut.hpp>
#include <ndb_limits.h>

#ifdef NO_EMULATED_JAM

#define jam()
#define jamLine(line)
#define jamEntry()
#define jamEntryLine(line)

#else
#ifdef NDB_WIN32

#define jam() { \
  Uint32 tEmulatedJamIndex = theEmulatedJamIndex; \
  *(Uint32*)(theEmulatedJam + tEmulatedJamIndex) = __LINE__; \
  theEmulatedJamIndex = (tEmulatedJamIndex + 4) & JAM_MASK; }
#define jamLine(line) { \
  Uint32 tEmulatedJamIndex = theEmulatedJamIndex; \
  *(Uint32*)(theEmulatedJam + tEmulatedJamIndex) = line; \
  theEmulatedJamIndex = (tEmulatedJamIndex + 4) & JAM_MASK; }
#define jamEntry() { \
  theEmulatedJamBlockNumber = number(); \
  Uint32 tEmulatedJamIndex = theEmulatedJamIndex; \
  *(Uint32*)(theEmulatedJam + tEmulatedJamIndex) = \
    ((theEmulatedJamBlockNumber << 20) | __LINE__); \
  theEmulatedJamIndex = (tEmulatedJamIndex + 4) & JAM_MASK; }
#define jamEntryLine(line) { \
  theEmulatedJamBlockNumber = number(); \
  Uint32 tEmulatedJamIndex = theEmulatedJamIndex; \
  *(Uint32*)(theEmulatedJam + tEmulatedJamIndex) = \
    ((theEmulatedJamBlockNumber << 20) | line); \
  theEmulatedJamIndex = (tEmulatedJamIndex + 4) & JAM_MASK; }

#else

#define jam() { \
  Uint32 tEmulatedJamIndex = theEmulatedJamIndex; \
  *(Uint32*)((UintPtr)theEmulatedJam + (Uint32)tEmulatedJamIndex) = __LINE__; \
  theEmulatedJamIndex = (tEmulatedJamIndex + 4) & JAM_MASK; }
#define jamLine(line) { \
  Uint32 tEmulatedJamIndex = theEmulatedJamIndex; \
  *(Uint32*)((UintPtr)theEmulatedJam + (Uint32)tEmulatedJamIndex) = line; \
  theEmulatedJamIndex = (tEmulatedJamIndex + 4) & JAM_MASK; }
#define jamEntry() { \
  theEmulatedJamBlockNumber = number(); \
  Uint32 tEmulatedJamIndex = theEmulatedJamIndex; \
  *(Uint32*)((UintPtr)theEmulatedJam + (Uint32)tEmulatedJamIndex) = \
    ((theEmulatedJamBlockNumber << 20) | __LINE__); \
  theEmulatedJamIndex = (tEmulatedJamIndex + 4) & JAM_MASK; }
#define jamEntryLine(line) { \
  theEmulatedJamBlockNumber = number(); \
  Uint32 tEmulatedJamIndex = theEmulatedJamIndex; \
  *(Uint32*)((UintPtr)theEmulatedJam + (Uint32)tEmulatedJamIndex) = \
    ((theEmulatedJamBlockNumber << 20) | line); \
  theEmulatedJamIndex = (tEmulatedJamIndex + 4) & JAM_MASK; }

#endif

#endif
#ifndef NDB_OPT
#define ptrCheck(ptr, limit, rec) if (ptr.i < (limit)) ptr.p = &rec[ptr.i]; else ptr.p = NULL

/**
 * Sets the p-value of a ptr-struct to be a pointer to record no i  
 * (where i is the i-value of the ptr-struct)
 *
 * @param ptr    ptr-struct with a set i-value  (the p-value in this gets set)
 * @param limit  max no of records in rec
 * @param rec    pointer to first record in an array of records
 */
#define ptrCheckGuard(ptr, limit, rec) {\
  UintR TxxzLimit; \
  TxxzLimit = (limit); \
  UintR TxxxPtr; \
  TxxxPtr = ptr.i; \
  ptr.p = &rec[TxxxPtr]; \
  if (TxxxPtr < (TxxzLimit)) { \
    ; \
  } else { \
    progError(__LINE__, ERR_POINTER_NOTINRANGE, __FILE__); \
  }}

#define ptrAss(ptr, rec) ptr.p = &rec[ptr.i]
#define ptrNull(ptr) ptr.p = NULL
#define ptrGuard(ptr) if (ptr.p == NULL) \
    progError(__LINE__, ERR_POINTER_NOTINRANGE, __FILE__)
#define arrGuard(ind, size) if ((ind) >= (size)) \
    progError(__LINE__, ERR_INDEX_NOTINRANGE, __FILE__)
#else
#define ptrCheck(ptr, limit, rec) ptr.p = &rec[ptr.i]
#define ptrCheckGuard(ptr, limit, rec) ptr.p = &rec[ptr.i]
#define ptrAss(ptr, rec) ptr.p = &rec[ptr.i]
#define ptrNull(ptr) ptr.p = NULL
#define ptrGuard(ptr)
#define arrGuard(ind, size)
#endif

// -------- ERROR INSERT MACROS -------
#ifdef ERROR_INSERT
#define ERROR_INSERT_VARIABLE UintR cerrorInsert
#define ERROR_INSERTED(x) (cerrorInsert == (x))
#define SET_ERROR_INSERT_VALUE(x) cerrorInsert = x
#define CLEAR_ERROR_INSERT_VALUE cerrorInsert = 0
#else
#define ERROR_INSERT_VARIABLE typedef void * cerrorInsert // Will generate compiler error if used
#define ERROR_INSERTED(x) false
#define SET_ERROR_INSERT_VALUE(x)
#define CLEAR_ERROR_INSERT_VALUE
#endif

/* ------------------------------------------------------------------------- */
/*       COMMONLY USED CONSTANTS.                                            */
/* ------------------------------------------------------------------------- */
#define ZFALSE 0
#define ZTRUE 1
#define ZSET 1
#define ZOK 0
#define ZNOT_OK 1
#define ZCLOSE_FILE 2
#define ZNIL 0xffff
#define Z8NIL 255

/* ------------------------------------------------------------------------- */
// Number of fragments stored per node. Should be settable on a table basis
// in future version since small tables want small value and large tables
// need large value.
/* ------------------------------------------------------------------------- */
#define NO_OF_FRAG_PER_NODE 1
#define MAX_FRAG_PER_NODE 8

/**
* DIH allocates fragments in chunk for fast find of fragment record.
* These parameters define chunk size and log of chunk size.
*/
#define NO_OF_FRAGS_PER_CHUNK 4
#define LOG_NO_OF_FRAGS_PER_CHUNK 2

/* ---------------------------------------------------------------- */
// To avoid synching too big chunks at a time we synch after writing
// a certain number of data/UNDO pages. (e.g. 2 MBytes).
/* ---------------------------------------------------------------- */
#define MAX_PAGES_WITHOUT_SYNCH 64
#define MAX_REDO_PAGES_WITHOUT_SYNCH 32

/* ------------------------------------------------------------------ */
// We have these constants to ensure that we can easily change the
// parallelism of node recovery and the amount of scan 
// operations needed for node recoovery.
/* ------------------------------------------------------------------ */
#define MAX_NO_WORDS_OUTSTANDING_COPY_FRAGMENT 6000
#define MAGIC_CONSTANT 56
#define NODE_RECOVERY_SCAN_OP_RECORDS \
         (4 + ((4*MAX_NO_WORDS_OUTSTANDING_COPY_FRAGMENT)/ \
         ((MAGIC_CONSTANT + 2) * 5)))

#ifdef NO_CHECKPOINT
#define NO_LCP
#define NO_GCP
#endif

/**
 * Ndb kernel blocks assertion handling
 *
 * Two type of assertions:
 * - ndbassert  - Only used when compiling VM_TRACE
 * - ndbrequire - Always checked
 *
 * If a ndbassert/ndbrequire fails, the system will 
 * shutdown and generate an error log
 *
 *
 * NOTE these may only be used within blocks
 */
#if defined VM_TRACE
#define ndbassert(check) \
  if((check)){ \
  } else {     \
    progError(__LINE__, ERR_NDBREQUIRE, __FILE__); \
  }           

#define ndbrequire(check) \
  if((check)){ \
  } else {     \
    progError(__LINE__, ERR_NDBREQUIRE, __FILE__); \
  }           
#else
#define ndbassert(check)

#define ndbrequire(check) \
  if((check)){ \
  } else {     \
    progError(__LINE__, ERR_NDBREQUIRE, __FILE__); \
  }           
#endif

#define CRASH_INSERTION(errorType) \
  if (!ERROR_INSERTED((errorType))) { \
  } else { \
    progError(__LINE__, ERR_ERROR_INSERT, __FILE__); \
  }

#define CRASH_INSERTION2(errorNum, condition) \
  if (!(ERROR_INSERTED(errorNum) && condition)) { \
  } else { \
    progError(__LINE__, ERR_ERROR_INSERT, __FILE__); \
  }

#define MEMCOPY_PAGE(to, from, page_size_in_bytes) \
  memcpy((void*)(to), (void*)(from), (size_t)(page_size_in_bytes));
#define MEMCOPY_NO_WORDS(to, from, no_of_words) \
  memcpy((to), (void*)(from), (size_t)(no_of_words << 2));

template <class T>
struct Ptr {
  T * p;
  Uint32 i;
  inline bool isNull() const { return i == RNIL; }
  inline void setNull() { i = RNIL; }
};

template <class T>
struct ConstPtr {
  const T * p;
  Uint32 i;
  inline bool isNull() const { return i == RNIL; }
  inline void setNull() { i = RNIL; }
};

#endif
