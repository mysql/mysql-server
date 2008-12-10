/* Copyright (C) 2007 MySQL AB & Sanja Belkin

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

#ifndef _ma_loghandler_lsn_h
#define _ma_loghandler_lsn_h

/*
  Transaction log record address:
  file_no << 32 | offset
  file_no is only 3 bytes so we can use signed integer to make
  comparison simpler.
*/
typedef int64 TRANSLOG_ADDRESS;

/*
  Compare addresses
    A1 >  A2 -> result  > 0
    A1 == A2 -> 0
    A1 <  A2 -> result < 0
*/
#define cmp_translog_addr(A1,A2) ((A1) - (A2))

/*
  TRANSLOG_ADDRESS is just address of some byte in the log (usually some
    chunk)
  LSN used where address of some record in the log needed (not just any
    address)
*/
typedef TRANSLOG_ADDRESS LSN;

/* Gets file number part of a LSN/log address */
#define LSN_FILE_NO(L) (uint32) ((L) >> 32)

/* Gets raw file number part of a LSN/log address */
#define LSN_FILE_NO_PART(L) ((L) & ((int64)0xFFFFFF00000000LL))

/* Parts of LSN for printing */
#define LSN_IN_PARTS(L) (ulong)LSN_FILE_NO(L),(ulong)LSN_OFFSET(L)

/* Gets record offset of a LSN/log address */
#define LSN_OFFSET(L) (ulong) ((L) & 0xFFFFFFFFL)

/* Makes lsn/log address from file number and record offset */
#define MAKE_LSN(F,S) ((LSN) ((((uint64)(F)) << 32) | (S)))

/* checks LSN */
#define LSN_VALID(L)                                    \
  ((LSN_FILE_NO_PART(L) != FILENO_IMPOSSIBLE) &&        \
   (LSN_OFFSET(L) != LOG_OFFSET_IMPOSSIBLE))

/* size of stored LSN on a disk, don't change it! */
#define LSN_STORE_SIZE 7

/* Puts LSN into buffer (dst) */
#define lsn_store(dst, lsn) \
  do { \
    int3store((dst), LSN_FILE_NO(lsn)); \
    int4store((char*)(dst) + 3, LSN_OFFSET(lsn)); \
  } while (0)

/* Unpacks LSN from the buffer (P) */
#define lsn_korr(P) MAKE_LSN(uint3korr(P), uint4korr((const char*)(P) + 3))

/* what we need to add to LSN to increase it on one file */
#define LSN_ONE_FILE ((int64)0x100000000LL)

#define LSN_REPLACE_OFFSET(L, S) (LSN_FILE_NO_PART(L) | (S))

/*
  an 8-byte type whose most significant uchar is used for "flags"; 7
  other bytes are a LSN.
*/
typedef LSN LSN_WITH_FLAGS;
#define LSN_WITH_FLAGS_TO_LSN(x)   (x & ULL(0x00FFFFFFFFFFFFFF))
#define LSN_WITH_FLAGS_TO_FLAGS(x) (x & ULL(0xFF00000000000000))

#define FILENO_IMPOSSIBLE     0 /**< log file's numbering starts at 1 */
#define LOG_OFFSET_IMPOSSIBLE 0 /**< log always has a header */
#define LSN_IMPOSSIBLE        ((LSN)0)
/* following LSN also is impossible */
#define LSN_ERROR             ((LSN)1)

/** @brief some impossible LSN serve as markers */

/**
   When table is modified by maria_chk, or auto-zerofilled, old REDOs don't
   apply, table is freshly born again somehow: its state's LSNs need to be
   updated to the new instance which receives this table.
*/
#define LSN_NEEDS_NEW_STATE_LSNS ((LSN)2)

/**
   @brief the maximum valid LSN.
   Unlike ULONGLONG_MAX, it can be safely used in comparison with valid LSNs
   (ULONGLONG_MAX is too big for correctness of cmp_translog_addr()).
*/
#define LSN_MAX (LSN)ULL(0x00FFFFFFFFFFFFFF)

#endif
