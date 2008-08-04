/* Copyright (C) 2006 MySQL AB

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

#ifndef _trnman_h
#define _trnman_h

C_MODE_START

#include <lf.h>
#include "trnman_public.h"
#include "ma_loghandler_lsn.h"

/*
  trid - 6 uchar transaction identifier. Assigned when a transaction
  is created. Transaction can always be identified by its trid,
  even after transaction has ended.

  short_trid - 2-byte transaction identifier, identifies a running
  transaction, is reassigned when transaction ends.
*/

struct st_transaction
{
  LF_PINS              *pins;
  void                 *used_tables;  /* Tables used by transaction */
  TRN                  *next, *prev;
  TrID                 trid, min_read_from, commit_trid;
  LSN		       rec_lsn, undo_lsn;
  LSN_WITH_FLAGS       first_undo_lsn;
  uint                 locked_tables;
  uint16               short_id;
  /* Note! if short_id is 0, trn is NOT initialized */
};

#define TRANSACTION_LOGGED_LONG_ID ULL(0x8000000000000000)

C_MODE_END

#endif

