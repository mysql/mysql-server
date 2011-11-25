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

#ifndef BLOCK_NUMBERS_H
#define BLOCK_NUMBERS_H

#include <kernel_types.h>
#include <RefConvert.hpp>

/* 32768 */
#define MIN_API_BLOCK_NO  0x8000

/* 2047 */
#define API_PACKED     0x07ff

/* Fixed block numbers in API */
#define NO_API_FIXED_BLOCKS    2

/* 4002 */
#define API_CLUSTERMGR 0x0FA2
#define MGM_CONFIG_MAN 0x0FA3

#define MIN_API_FIXED_BLOCK_NO (API_CLUSTERMGR)
#define MAX_API_FIXED_BLOCK_NO (MIN_API_FIXED_BLOCK_NO + NO_API_FIXED_BLOCKS)


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
#define DBTUX      0x102
#define TSMAN      0x103
#define LGMAN      0x104
#define PGMAN      0x105
#define RESTORE    0x106
#define DBINFO     0x107
#define DBSPJ      0x108
#define THRMAN     0x109
#define TRPMAN     0x10A

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
const BlockReference DBTUX_REF   = numberToRef(DBTUX, 0);
const BlockReference TSMAN_REF   = numberToRef(TSMAN, 0);
const BlockReference LGMAN_REF   = numberToRef(LGMAN, 0);
const BlockReference PGMAN_REF   = numberToRef(PGMAN, 0);
const BlockReference RESTORE_REF = numberToRef(RESTORE, 0);
const BlockReference DBINFO_REF  = numberToRef(DBINFO, 0);
const BlockReference DBSPJ_REF  = numberToRef(DBSPJ, 0);
const BlockReference THRMAN_REF  = numberToRef(THRMAN, 0);
const BlockReference TRPMAN_REF  = numberToRef(TRPMAN, 0);

static inline void __hide_warnings_unused_ref_vars(void) {
  // Hide annoying warnings about unused variables
  (void)BACKUP_REF;  (void)DBTC_REF;    (void)DBDIH_REF;
  (void)DBLQH_REF;   (void)DBACC_REF;   (void)DBTUP_REF;
  (void)DBDICT_REF;  (void)NDBCNTR_REF; (void)QMGR_REF;
  (void)NDBFS_REF;   (void)CMVMI_REF;   (void)TRIX_REF;
  (void)DBUTIL_REF;  (void)SUMA_REF;    (void)DBTUX_REF;
  (void)TSMAN_REF;   (void)LGMAN_REF;   (void)PGMAN_REF;
  (void)RESTORE_REF; (void)DBINFO_REF;  (void)DBSPJ_REF;
  (void)THRMAN_REF;  (void)TRPMAN_REF;
}

const BlockNumber MIN_BLOCK_NO = BACKUP;
const BlockNumber MAX_BLOCK_NO = TRPMAN;
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
