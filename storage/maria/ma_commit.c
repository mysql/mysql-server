/* Copyright (C) 2007-2008 MySQL AB, 2008-2009 Sun Microsystems, Inc.

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

#include "maria_def.h"
#include "trnman.h"

/**
   writes a COMMIT record to log and commits transaction in memory

   @param  trn              transaction

   @return Operation status
     @retval 0      ok
     @retval 1      error (disk error or out of memory)
*/

int ma_commit(TRN *trn)
{
  int res;
  LSN commit_lsn;
  LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS];
  DBUG_ENTER("ma_commit");

  DBUG_ASSERT(trn->rec_lsn == LSN_IMPOSSIBLE);
  if (trn->undo_lsn == 0) /* no work done, rollback (cheaper than commit) */
    DBUG_RETURN(trnman_rollback_trn(trn));
  /*
    - if COMMIT record is written before trnman_commit_trn():
    if Checkpoint comes in the middle it will see trn is not committed,
    then if crash, Recovery might roll back trn (if min(rec_lsn) is after
    COMMIT record) and this is not an issue as
    * transaction's updates were not made visible to other transactions
    * "commit ok" was not sent to client
    Alternatively, Recovery might commit trn (if min(rec_lsn) is before COMMIT
    record), which is ok too. All in all it means that "trn committed" is not
    100% equal to "COMMIT record written".
    - if COMMIT record is written after trnman_commit_trn():
    if crash happens between the two, trn will be rolled back which is an
    issue (transaction's updates were made visible to other transactions).
    So we need to go the first way.

    Note that we have to use | here to ensure that all calls are made.
  */

  /*
    We do not store "thd->transaction.xid_state.xid" for now, it will be
    needed only when we support XA.
  */
  res= (translog_write_record(&commit_lsn, LOGREC_COMMIT,
                             trn, NULL, 0,
                             sizeof(log_array)/sizeof(log_array[0]),
                             log_array, NULL, NULL) |
        translog_flush(commit_lsn));

  DBUG_EXECUTE_IF("maria_sleep_in_commit",
                  {
                    DBUG_PRINT("info", ("maria_sleep_in_commit"));
                    sleep(3);
                  });
  res|= trnman_commit_trn(trn);


  /*
    Note: if trnman_commit_trn() fails above, we have already
    written the COMMIT record, so Checkpoint and Recovery will see the
    transaction as committed.
  */
  DBUG_RETURN(res);
}


/**
   Writes a COMMIT record for a transaciton associated with a file

   @param  info              Maria handler

   @return Operation status
     @retval 0      ok
     @retval #      error (disk error or out of memory)
*/

int maria_commit(MARIA_HA *info)
{
  return info->s->now_transactional ? ma_commit(info->trn) : 0;
}


/**
   Starts a transaction on a file handle

   @param  info              Maria handler

   @return Operation status
     @retval 0      ok
     @retval #      Error code.

   @note this can be used only in single-threaded programs (tests),
   because we create a transaction (trnman_new_trn) with WT_THD=0.
   XXX it needs to be fixed when we'll start using maria_begin from SQL.
*/

int maria_begin(MARIA_HA *info)
{
  DBUG_ENTER("maria_begin");

  if (info->s->now_transactional)
  {
    TRN *trn= trnman_new_trn(0);
    if (unlikely(!trn))
      DBUG_RETURN(HA_ERR_OUT_OF_MEM);

    DBUG_PRINT("info", ("TRN set to 0x%lx", (ulong) trn));
    _ma_set_trn_for_table(info, trn);
  }
  DBUG_RETURN(0);
}

