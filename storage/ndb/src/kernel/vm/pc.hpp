/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PC_H
#define PC_H


#include "Emulator.hpp"
#include <NdbOut.hpp>
#include <ndb_limits.h>
#include <NdbThread.h>

#define JAM_FILE_ID 282

/* Jam buffer pointer. */
struct EmulatedJamBuffer;
extern thread_local EmulatedJamBuffer* NDB_THREAD_TLS_JAM;

/* Thread self pointer. */
struct thr_data;
extern thread_local thr_data* NDB_THREAD_TLS_THREAD;

#ifdef NDB_DEBUG_RES_OWNERSHIP

/* (Debug only) Shared resource owner. */
extern thread_local Uint32 NDB_THREAD_TLS_RES_OWNER;

#endif

/**
 * To enable jamDebug and its siblings in a production simply
 * remove the comment and get EXTRA_JAM defined.
 */
//#define EXTRA_JAM 1

#ifdef NO_EMULATED_JAM

#define jam()
#define jamLine(line)
#define jamEntry()
#define jamDebug()
#define jamLineDebug(line)
#define jamEntryDebug()
#define jamEntryLine(line)
#define jamBlock(block)
#define jamBlockLine(block, line)
#define jamEntryBlock(block)
#define jamEntryBlockLine(block, line)
#define jamNoBlock()
#define jamNoBlockLine(line)
#define thrjamEntry(buf)
#define thrjamEntryLine(buf, line)
#define thrjam(buf)
#define thrjamLine(buf, line)
#define thrjamEntryDebug(buf)
#define thrjamEntryLineDebug(buf, line)
#define thrjamDebug(buf)
#define thrjamLineDebug(buf, line)

#else

#define thrjamEntryBlockLine(jamBufferArg, blockNo, line) \
  thrjamLine(jamBufferArg, line)

/**
 * Make an entry in the jamBuffer to record that execution reached a given
 * point in the source code. For a description of how to maintain and debug 
 * JAM_FILE_IDs, please refer to the comments for jamFileNames in Emulator.cpp.
 */
#define thrjamLine(jamBufferArg, line) \
  do { \
    EmulatedJamBuffer* const jamBuffer = jamBufferArg; \
    Uint32 jamIndex = jamBuffer->theEmulatedJamIndex; \
    jamBuffer->theEmulatedJam[jamIndex++] = JamEvent((JAM_FILE_ID), (line)); \
    jamBuffer->theEmulatedJamIndex = jamIndex & JAM_MASK; \
    /* Occasionally check that the jam buffer belongs to this thread.*/ \
    assert((jamIndex & 3) != 0 || jamBuffer == NDB_THREAD_TLS_JAM);       \
    /* Occasionally check that jamFileNames[JAM_FILE_ID] matches __FILE__.*/ \
    assert((jamIndex & 0xff) != 0 ||                     \
           JamEvent::verifyId((JAM_FILE_ID), __FILE__)); \
  } while(0)

#define jamBlockLine(block, line) thrjamLine(block->jamBuffer(), line)
#define jamBlock(block) jamBlockLine((block), __LINE__)
#define jamLine(line) jamBlockLine(this, (line))
#define jam() jamLine(__LINE__)
#define jamBlockEntryLine(block, line) \
  thrjamEntryBlockLine(block->jamBuffer(), block->number(), line)
#define jamEntryBlock(block) jamEntryBlockLine(block, __LINE__)
#define jamEntryLine(line) jamBlockEntryLine(this, (line))
#define jamEntry() jamEntryLine(__LINE__)

#define jamNoBlockLine(line) \
    thrjamLine(NDB_THREAD_TLS_JAM, line)
#define jamNoBlock() jamNoBlockLine(__LINE__)

#define thrjamEntryLine(buf, line) thrjamEntryBlockLine(buf, number(), line)

#define thrjam(buf) thrjamLine(buf, __LINE__)
#define thrjamEntry(buf) thrjamEntryLine(buf, __LINE__)

#if defined VM_TRACE || defined ERROR_INSERT || defined EXTRA_JAM
#define jamDebug() jam()
#define jamLineDebug(line) jamLine(line)
#define jamEntryDebug() jamEntry()
#define thrjamEntryDebug(buf) thrjamEntry(buf)
#define thrjamEntryLineDebug(buf, line) thrJamEntryLine(guf, line)
#define thrjamDebug(buf) thrjam(buf)
#define thrjamLineDebug(buf, line) thrjamLine(buf, line)
#else
#define jamDebug()
#define jamLineDebug(line)
#define jamEntryDebug()
#define thrjamEntryDebug(buf)
#define thrjamEntryLineDebug(buf, line)
#define thrjamDebug(buf)
#define thrjamLineDebug(buf, line)
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
#define ptrCheckGuardErr(ptr, limit, rec, error) {\
  UintR TxxzLimit; \
  TxxzLimit = (limit); \
  UintR TxxxPtr; \
  TxxxPtr = ptr.i; \
  ptr.p = &rec[TxxxPtr]; \
  if (TxxxPtr < (TxxzLimit)) { \
    ; \
  } else { \
    progError(__LINE__, error, __FILE__); \
  }}
#define ptrAss(ptr, rec) ptr.p = &rec[ptr.i]
#define ptrNull(ptr) ptr.p = NULL
#define ptrGuardErr(ptr, error) if (ptr.p == NULL) \
    progError(__LINE__, error, __FILE__)
#define arrGuardErr(ind, size, error) if ((ind) >= (size)) \
    progError(__LINE__, error, __FILE__)
#else
#define ptrCheck(ptr, limit, rec) ptr.p = &rec[ptr.i]
#define ptrCheckGuardErr(ptr, limit, rec, error) ptr.p = &rec[ptr.i]
#define ptrAss(ptr, rec) ptr.p = &rec[ptr.i]
#define ptrNull(ptr) ptr.p = NULL
#define ptrGuardErr(ptr, error)
#define arrGuardErr(ind, size, error)
#endif

#define ptrCheckGuard(ptr, limit, rec) \
  ptrCheckGuardErr(ptr, limit, rec, NDBD_EXIT_POINTER_NOTINRANGE)
#define ptrGuard(ptr) ptrGuardErr(ptr, NDBD_EXIT_POINTER_NOTINRANGE)
#define arrGuard(ind, size) arrGuardErr(ind, size, NDBD_EXIT_INDEX_NOTINRANGE)

// -------- ERROR INSERT MACROS -------
#ifdef ERROR_INSERT
#define ERROR_INSERT_VARIABLE mutable UintR cerrorInsert, c_error_insert_extra
#define ERROR_INSERTED(x) (cerrorInsert == (x))
#define ERROR_INSERTED_CLEAR(x) (cerrorInsert == (x) ? (cerrorInsert = 0, true) : false)
#define ERROR_INSERT_VALUE cerrorInsert
#define ERROR_INSERT_EXTRA c_error_insert_extra
#define SET_ERROR_INSERT_VALUE(x) cerrorInsert = x
#define SET_ERROR_INSERT_VALUE2(x,y) cerrorInsert = x; c_error_insert_extra = y
#define CLEAR_ERROR_INSERT_VALUE cerrorInsert = 0
#else
#define ERROR_INSERT_VARIABLE typedef void * cerrorInsert // Will generate compiler error if used
#define ERROR_INSERTED(x) false
#define ERROR_INSERTED_CLEAR(x) false
#define ERROR_INSERT_VALUE 0
#define ERROR_INSERT_EXTRA Uint32(0)
#define SET_ERROR_INSERT_VALUE(x) do { } while(0)
#define SET_ERROR_INSERT_VALUE2(x,y) do { } while(0)
#define CLEAR_ERROR_INSERT_VALUE do { } while(0)
#endif

#define DECLARE_DUMP0(BLOCK, CODE, DESC) if (arg == CODE)

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
#define MAX_FRAG_PER_LQH 8

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
#define ZUNDEFINED_GCI_LIMIT 1

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
  if(likely(check)){ \
  } else {     \
    jamNoBlock(); \
    progError(__LINE__, NDBD_EXIT_NDBASSERT, __FILE__, #check); \
  }
#else
#define ndbassert(check) do { } while(0)
#endif

#define ndbrequireErr(check, error) \
  if(likely(check)){ \
  } else {     \
    jamNoBlock(); \
    progError(__LINE__, error, __FILE__, #check); \
  }

#define ndbrequire(check) \
  ndbrequireErr(check, NDBD_EXIT_NDBREQUIRE)

#define CRASH_INSERTION(errorType) \
  if (!ERROR_INSERTED((errorType))) { \
  } else { \
    jamNoBlock(); \
    progError(__LINE__, NDBD_EXIT_ERROR_INSERT, __FILE__); \
  }

#define CRASH_INSERTION2(errorNum, condition) \
  if (!(ERROR_INSERTED(errorNum) && condition)) { \
  } else { \
    jamNoBlock(); \
    progError(__LINE__, NDBD_EXIT_ERROR_INSERT, __FILE__); \
  }

#define MEMCOPY_PAGE(to, from, page_size_in_bytes) \
  memcpy((void*)(to), (void*)(from), (size_t)(page_size_in_bytes));
#define MEMCOPY_NO_WORDS(to, from, no_of_words) \
  memcpy((to), (void*)(from), (size_t)((no_of_words) << 2));

// Get the jam buffer for the current thread.
inline EmulatedJamBuffer* getThrJamBuf()
{
  return NDB_THREAD_TLS_JAM;
}

#undef JAM_FILE_ID

#endif
