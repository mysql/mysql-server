/* Copyright (C) 2000 MySQL AB

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

#ifndef _trnman_h
#define _trnman_h

#include "lockman.h"

typedef uint64 TrID; /* our TrID is 6 bytes */
typedef struct st_transaction TRN;

struct st_transaction
{
  LOCK_OWNER           locks;
  LF_PINS             *pins;
  TrID                 trid, min_read_from, commit_trid;
  TRN                 *next, *prev;
  /* Note! if locks.loid is 0, trn is NOT initialized */
};

#define SHORT_TRID_MAX 65535

extern uint trnman_active_transactions, trnman_allocated_transactions;

int trnman_init(void);
int trnman_destroy(void);
TRN *trnman_new_trn(pthread_mutex_t *mutex, pthread_cond_t *cond);
void trnman_end_trn(TRN *trn, my_bool commit);
#define trnman_commit_trn(T) trnman_end_trn(T, TRUE)
#define trnman_abort_trn(T)  trnman_end_trn(T, FALSE)
void trnman_free_trn(TRN *trn);
my_bool trnman_can_read_from(TRN *trn, TrID trid);

#endif

