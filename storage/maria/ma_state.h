/* Copyright (C) 2008 Sun AB & Michael Widenius

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

/* Struct to store tables in use by one transaction */

typedef struct st_maria_status_info
{
  ha_rows records;                      /* Rows in table */
  ha_rows del;                          /* Removed rows */
  my_off_t empty;                       /* lost space in datafile */
  my_off_t key_empty;                   /* lost space in indexfile */
  my_off_t key_file_length;
  my_off_t data_file_length;
  ha_checksum checksum;
  uint32 changed:1,                     /* Set if table was changed */
         no_transid:1;                  /* Set if no transid was set on rows */
} MARIA_STATUS_INFO;


typedef struct st_used_tables {
  struct st_used_tables *next;
  struct st_maria_share *share;
  MARIA_STATUS_INFO state_current;
  MARIA_STATUS_INFO state_start;
} MARIA_USED_TABLES;


/* Struct to store commit state at different times */

typedef struct st_state_history {
  struct st_state_history *next;
  TrID trid;
  MARIA_STATUS_INFO state;
} MARIA_STATE_HISTORY;


/* struct to remember history for closed tables */

typedef struct st_state_history_closed {
  LSN create_rename_lsn;
  MARIA_STATE_HISTORY *state_history;
} MARIA_STATE_HISTORY_CLOSED;


my_bool _ma_setup_live_state(MARIA_HA *info);
MARIA_STATE_HISTORY *_ma_remove_not_visible_states(MARIA_STATE_HISTORY
                                                   *org_history,
                                                   my_bool all,
                                                   my_bool trman_is_locked);
void _ma_reset_state(MARIA_HA *info);
void _ma_get_status(void* param, my_bool concurrent_insert);
void _ma_update_status(void* param);
void _ma_update_status_with_lock(MARIA_HA *info);
void _ma_restore_status(void *param);
void _ma_copy_status(void* to, void *from);
void _ma_reset_update_flag(void *param, my_bool concurrent_insert);
my_bool _ma_check_status(void *param);
void _ma_block_get_status(void* param, my_bool concurrent_insert);
void _ma_block_update_status(void *param);
void _ma_block_restore_status(void *param);
my_bool _ma_block_check_status(void *param);
void maria_versioning(MARIA_HA *info, my_bool versioning);
void _ma_set_share_data_file_length(struct st_maria_share *share,
                                    ulonglong new_length);
void _ma_copy_nontrans_state_information(MARIA_HA *info);
my_bool _ma_trnman_end_trans_hook(TRN *trn, my_bool commit,
                                  my_bool active_transactions);
my_bool _ma_row_visible_always(MARIA_HA *info);
my_bool _ma_row_visible_non_transactional_table(MARIA_HA *info);
my_bool _ma_row_visible_transactional_table(MARIA_HA *info);
void _ma_remove_not_visible_states_with_lock(struct st_maria_share *share,
                                             my_bool all);
void _ma_remove_table_from_trnman(struct st_maria_share *share, TRN *trn);
void _ma_reset_history(struct st_maria_share *share);
