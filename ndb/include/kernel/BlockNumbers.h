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

#ifndef BLOCK_NUMBERS_H
#define BLOCK_NUMBERS_H

#include <kernel_types.h>
#include <RefConvert.hpp>

/* 240 */
#define MIN_API_BLOCK_NO  0x8000

/* 2047 */
#define API_PACKED     0x07ff

/* 4002 */
#define API_CLUSTERMGR 0x0FA2

#define BACKUP      0xF4
#define DBTC        0xF5
#define DBDIH       0xF6
#define DBLQH       0xF7
#define DBACC       0xF8
#define DBTUP       0xF9
#define DBDICT      0xFA
#define NDBCNTR     0xFB
#define CNTR        0xFB
#define QMGR        0xFC
#define NDBFS       0xFD
#define CMVMI       0xFE
#define TRIX        0xFF
#define DBUTIL     0x100
#define SUMA       0x101
#define GREP       0x102
#define DBTUX      0x103

const BlockReference BACKUP_REF  = numberToRef(BACKUP, 0);
const BlockReference DBTC_REF    = numberToRef(DBTC, 0);
const BlockReference DBDIH_REF   = numberToRef(DBDIH, 0);
const BlockReference DBLQH_REF   = numberToRef(DBLQH, 0);
const BlockReference DBACC_REF   = numberToRef(DBACC, 0);
const BlockReference DBTUP_REF   = numberToRef(DBTUP, 0);
const BlockReference DBDICT_REF  = numberToRef(DBDICT, 0);
const BlockReference NDBCNTR_REF = numberToRef(NDBCNTR, 0);
const BlockReference QMGR_REF    = numberToRef(QMGR, 0);
const BlockReference NDBFS_REF   = numberToRef(NDBFS, 0);
const BlockReference CMVMI_REF   = numberToRef(CMVMI, 0);
const BlockReference TRIX_REF    = numberToRef(TRIX, 0);
const BlockReference DBUTIL_REF  = numberToRef(DBUTIL, 0);
const BlockReference SUMA_REF    = numberToRef(SUMA, 0);
const BlockReference GREP_REF    = numberToRef(GREP, 0);
const BlockReference DBTUX_REF   = numberToRef(DBTUX, 0);

const BlockNumber MIN_BLOCK_NO = BACKUP;
const BlockNumber MAX_BLOCK_NO = DBTUX;
const BlockNumber NO_OF_BLOCKS = (MAX_BLOCK_NO - MIN_BLOCK_NO + 1);

/**
 * Used for printing and stuff
 */
struct BlockName {
  const char* name;
  BlockNumber number;
};

extern const BlockName BlockNames[];
extern const BlockNumber NO_OF_BLOCK_NAMES;

#endif
