/* Copyright (C) 2006-2008 MySQL AB, 2008-2009 Sun Microsystems, Inc.

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

/**
  trid - 6 uchar transaction identifier. Assigned when a transaction
  is created. Transaction can always be identified by its trid,
  even after transaction has ended.

  short_id - 2-byte transaction identifier, identifies a running
  transaction, is reassigned when transaction ends.

  when short_id is 0, TRN is not initialized, for all practical purposes
  it could be considered unused.

  when commit_trid is MAX_TRID the transaction is running, otherwise it's
  committed.

  state_lock mutex protects the state of a TRN, that is whether a TRN
  is committed/running/unused. Meaning that modifications of short_id and
  commit_trid happen under this mutex.
*/

struct st_ma_transaction
{
  LF_PINS              *pins;
  WT_THD               *wt;
  mysql_mutex_t         state_lock;
  void                 *used_tables;  /**< Tables used by transaction */
  TRN                  *next, *prev;
  TrID                 trid, min_read_from, commit_trid;
  LSN		       rec_lsn, undo_lsn;
  LSN_WITH_FLAGS       first_undo_lsn;
  uint                 locked_tables;
  uint16               short_id;
  uint16               flags;         /**< Various flags */
};

#define TRANSACTION_LOGGED_LONG_ID ULL(0x8000000000000000)
#define MAX_TRID (~(TrID)0)

extern WT_RESOURCE_TYPE ma_rc_dup_unique;

#ifdef HAVE_PSI_INTERFACE
extern PSI_mutex_key key_LOCK_trn_list, key_TRN_state_lock;
#endif

C_MODE_END

#endif

