/* Copyright (C) 2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

/* Initialize an maria-database */

#include "maria_def.h"
#include <ft_global.h>
#include "ma_blockrec.h"
#include "trnman_public.h"
#include "ma_checkpoint.h"
#include <hash.h>

void history_state_free(MARIA_STATE_HISTORY_CLOSED *closed_history)
{
  MARIA_STATE_HISTORY *history, *next;

  /*
    Free all active history
    In case of maria_open() this list should be empty as the history is moved
    to handler->share.
 */
  for (history= closed_history->state_history; history ; history= next)
  {
    next= history->next;
    my_free(history, MYF(0));
  }
  my_free(closed_history, MYF(0));
}


static int dummy_maria_create_trn_hook(MARIA_HA *info __attribute__((unused)))
{
  return 0;
}

/*
  Initialize maria

  SYNOPSIS
    maria_init()

  TODO
    Open log files and do recovery if need

  RETURN
  0  ok
  #  error number
*/

int maria_init(void)
{
  DBUG_ASSERT(maria_block_size &&
              maria_block_size % MARIA_MIN_KEY_BLOCK_LENGTH == 0);
  if (!maria_inited)
  {
    maria_inited= TRUE;
    pthread_mutex_init(&THR_LOCK_maria,MY_MUTEX_INIT_SLOW);
    _ma_init_block_record_data();
    trnman_end_trans_hook= _ma_trnman_end_trans_hook;
    maria_create_trn_hook= dummy_maria_create_trn_hook;
    my_handler_error_register();
  }
  hash_init(&maria_stored_state, &my_charset_bin, 32,
            0, sizeof(LSN), 0, (hash_free_key) history_state_free, 0);
  DBUG_PRINT("info",("dummy_transaction_object: %p",
                     &dummy_transaction_object));
  return 0;
}


void maria_end(void)
{
  if (maria_inited)
  {
    TrID trid;
    maria_inited= maria_multi_threaded= FALSE;
    ft_free_stopwords();
    ma_checkpoint_end();
    if ((trid= trnman_get_max_trid()) > max_trid_in_control_file)
    {
      /*
        Store max transaction id into control file, in case logs are removed
        by user, or maria_chk wants to check tables (it cannot access max trid
        from the log, as it cannot process REDOs).
      */
      (void)ma_control_file_write_and_force(last_checkpoint_lsn, last_logno,
                                            trid, recovery_failures);
    }
    trnman_destroy();
    if (translog_status == TRANSLOG_OK || translog_status == TRANSLOG_READONLY)
      translog_destroy();
    end_pagecache(maria_log_pagecache, TRUE);
    end_pagecache(maria_pagecache, TRUE);
    ma_control_file_end();
    pthread_mutex_destroy(&THR_LOCK_maria);
    hash_free(&maria_stored_state);
  }
}
