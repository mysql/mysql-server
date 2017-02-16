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


/*
  External definitions for trnman.h
  We need to split this into two files as gcc 4.1.2 gives error if it tries
  to include my_atomic.h in C++ code.
*/

#ifndef _trnman_public_h
#define _trnman_public_h

#include "ma_loghandler_lsn.h"
#include <waiting_threads.h>

C_MODE_START
typedef uint64 TrID; /* our TrID is 6 bytes */
typedef struct st_ma_transaction TRN;

#define SHORT_TRID_MAX 65535

extern uint trnman_active_transactions, trnman_allocated_transactions;
extern TRN dummy_transaction_object;
extern my_bool (*trnman_end_trans_hook)(TRN *trn, my_bool commit,
                                        my_bool active_transactions);

int trnman_init(TrID);
void trnman_destroy(void);
TRN *trnman_new_trn(WT_THD *wt);
my_bool trnman_end_trn(TRN *trn, my_bool commit);
#define trnman_commit_trn(T) trnman_end_trn(T, TRUE)
#define trnman_abort_trn(T)  trnman_end_trn(T, FALSE)
#define trnman_rollback_trn(T)  trnman_end_trn(T, FALSE)
int trnman_can_read_from(TRN *trn, TrID trid);
TRN *trnman_trid_to_trn(TRN *trn, TrID trid);
void trnman_new_statement(TRN *trn);
void trnman_rollback_statement(TRN *trn);
my_bool trnman_collect_transactions(LEX_STRING *str_act, LEX_STRING *str_com,
                                    LSN *min_rec_lsn,
                                    LSN *min_first_undo_lsn);

uint trnman_increment_locked_tables(TRN *trn);
uint trnman_decrement_locked_tables(TRN *trn);
uint trnman_has_locked_tables(TRN *trn);
void trnman_reset_locked_tables(TRN *trn, uint locked_tables);
TRN *trnman_recreate_trn_from_recovery(uint16 shortid, TrID longid);
TRN *trnman_get_any_trn(void);
TrID trnman_get_min_trid(void);
TrID trnman_get_max_trid(void);
TrID trnman_get_min_safe_trid();
my_bool trnman_exists_active_transactions(TrID min_id, TrID max_id,
                                          my_bool trnman_is_locked);
#define TRANSID_SIZE		6
#define transid_store(dst, id) int6store(dst,id)
#define transid_korr(P) uint6korr(P)
void trnman_lock();
void trnman_unlock();
my_bool trman_is_inited();
#ifdef EXTRA_DEBUG
uint16 trnman_get_flags(TRN *);
void trnman_set_flags(TRN *, uint16 flags);
#else
#define trnman_get_flags(A) 0
#define trnman_set_flags(A, B) do { } while (0)
#endif

/* Flag bits */
#define TRN_STATE_INFO_LOGGED       1  /* Query is logged */
#define TRN_STATE_TABLES_CAN_CHANGE 2  /* Things can change during trans. */

C_MODE_END
#endif
