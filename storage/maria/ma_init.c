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
    my_free(history);
  }
  my_free(closed_history);
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
    mysql_mutex_init(key_THR_LOCK_maria, &THR_LOCK_maria, MY_MUTEX_INIT_SLOW);
    _ma_init_block_record_data();
    trnman_end_trans_hook= _ma_trnman_end_trans_hook;
    maria_create_trn_hook= dummy_maria_create_trn_hook;
  }
  my_hash_init(&maria_stored_state, &my_charset_bin, 32,
            0, sizeof(LSN), 0, (my_hash_free_key) history_state_free, 0);
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
    if (translog_status == TRANSLOG_OK)
    {
      translog_soft_sync_end();
      translog_sync();
    }
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
    mysql_mutex_destroy(&THR_LOCK_maria);
    my_hash_free(&maria_stored_state);
  }
}

/**
   Upgrade from older Aria versions:

  - In MariaDB 5.1, the name of the control file and log files had the
    'maria' prefix, now they have the 'aria' prefix.

  @return: 0 ok
           1 error

*/

my_bool maria_upgrade()
{
  char name[FN_REFLEN], new_name[FN_REFLEN];
  DBUG_ENTER("maria_upgrade");

  fn_format(name, "maria_log_control", maria_data_root, "", MYF(MY_WME));

  if (!my_access(name,F_OK))
  {
    /*
      Old style control file found; Rename the control file and the log files.
      We start by renaming all log files, so that if we get a crash
      we will continue from where we left.
    */
    uint i;
    MY_DIR *dir= my_dir(maria_data_root, MYF(MY_WME));
    if (!dir)
      DBUG_RETURN(1);

    my_message(HA_ERR_INITIALIZATION,
               "Found old style Maria log files; "
               "Converting them to Aria names",
               MYF(ME_JUST_INFO));

    for (i= 0; i < dir->number_off_files; i++)
    {
      const char *file= dir->dir_entry[i].name;
      if (strncmp(file, "maria_log.", 10) == 0 &&
          file[10] >= '0' && file[10] <= '9' &&
        file[11] >= '0' && file[11] <= '9' &&
        file[12] >= '0' && file[12] <= '9' &&
        file[13] >= '0' && file[13] <= '9' &&
        file[14] >= '0' && file[14] <= '9' &&
        file[15] >= '0' && file[15] <= '9' &&
        file[16] >= '0' && file[16] <= '9' &&
        file[17] >= '0' && file[17] <= '9' &&
        file[18] == '\0')
      {
        /* Remove the 'm' in 'maria' */
        char old_logname[FN_REFLEN], new_logname[FN_REFLEN];
        fn_format(old_logname, file, maria_data_root, "", MYF(0));
        fn_format(new_logname, file+1, maria_data_root, "", MYF(0));
        if (mysql_file_rename(key_file_translog, old_logname,
                              new_logname, MYF(MY_WME)))
        {
          my_dirend(dir);
          DBUG_RETURN(1);
        }
      }
    }
    my_dirend(dir);
    
    fn_format(new_name, CONTROL_FILE_BASE_NAME, maria_data_root, "", MYF(0));
    if (mysql_file_rename(key_file_control, name, new_name, MYF(MY_WME)))
      DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}
