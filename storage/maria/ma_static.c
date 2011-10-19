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


/*
  Static variables for MARIA library. All definied here for easy making of
  a shared library
*/

#ifndef _global_h
#include "maria_def.h"
#include "trnman.h"
#endif

LIST	*maria_open_list=0;
uchar	maria_file_magic[]=
{ (uchar) 254, (uchar) 254, (uchar) 9, '\003', };
uchar	maria_pack_file_magic[]=
{ (uchar) 254, (uchar) 254, (uchar) 10, '\001', };
/* Unique number for this maria instance */
uchar   maria_uuid[MY_UUID_SIZE];
uint	maria_quick_table_bits=9;
ulong	maria_block_size= MARIA_KEY_BLOCK_LENGTH;
my_bool maria_flush= 0, maria_single_user= 0;
my_bool maria_delay_key_write= 0, maria_page_checksums= 1;
my_bool maria_inited= FALSE;
my_bool maria_in_ha_maria= FALSE; /* If used from ha_maria or not */
my_bool maria_recovery_changed_data= 0, maria_recovery_verbose= 0;
my_bool maria_assert_if_crashed_table= 0;
my_bool maria_checkpoint_disabled= 0;

mysql_mutex_t THR_LOCK_maria;
#ifdef DONT_USE_RW_LOCKS
ulong maria_concurrent_insert= 0;
#else
/* Do concurrent inserts at file end or in old holes */
ulong maria_concurrent_insert= 2;
#endif

my_off_t maria_max_temp_length= MAX_FILE_SIZE;
ulong    maria_bulk_insert_tree_size=8192*1024;
ulong    maria_data_pointer_size= 4;

PAGECACHE maria_pagecache_var;
PAGECACHE *maria_pagecache= &maria_pagecache_var;

PAGECACHE maria_log_pagecache_var;
PAGECACHE *maria_log_pagecache= &maria_log_pagecache_var;
MY_TMPDIR *maria_tmpdir;                        /* Tempdir for redo */
char *maria_data_root;
HASH maria_stored_state;
int (*maria_create_trn_hook)(MARIA_HA *);

/**
   @brief when transactionality does not matter we can use this transaction

   Used in external programs like ma_test*, and also internally inside
   libmaria when there is no transaction around and the operation isn't
   transactional (CREATE/DROP/RENAME/OPTIMIZE/REPAIR).
*/
TRN dummy_transaction_object;

/* a WT_RESOURCE_TYPE for transactions waiting on a unique key conflict */
WT_RESOURCE_TYPE ma_rc_dup_unique={ wt_resource_id_memcmp, 0};

/* Enough for comparing if number is zero */
uchar maria_zero_string[]= {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

/*
  read_vec[] is used for converting between P_READ_KEY.. and SEARCH_
  Position is , == , >= , <= , > , <
*/

uint32 maria_read_vec[]=
{
  SEARCH_FIND,                               /* HA_READ_KEY_EXACT */
  SEARCH_FIND | SEARCH_BIGGER,               /* HA_READ_KEY_OR_NEXT */
  SEARCH_FIND | SEARCH_SMALLER,              /* HA_READ_KEY_OR_PREV */
  SEARCH_NO_FIND | SEARCH_BIGGER,            /* HA_READ_AFTER_KEY */
  SEARCH_NO_FIND | SEARCH_SMALLER,	     /* HA_READ_BEFORE_KEY */
  SEARCH_FIND | SEARCH_PART_KEY,	     /* HA_READ_PREFIX */
  SEARCH_LAST,                               /* HA_READ_PREFIX_LAST */
  SEARCH_LAST | SEARCH_SMALLER,              /* HA_READ_PREFIX_LAST_OR_PREV */
  MBR_CONTAIN,                               /* HA_READ_MBR_CONTAIN */
  MBR_INTERSECT,                             /* HA_READ_MBR_INTERSECT */
  MBR_WITHIN,                                /* HA_READ_MBR_WITHIN */
  MBR_DISJOINT,                              /* HA_READ_MBR_DISJOINT */
  MBR_EQUAL                                  /* HA_READ_MBR_EQUAL */
};

uint32 maria_readnext_vec[]=
{
  SEARCH_BIGGER, SEARCH_BIGGER, SEARCH_SMALLER, SEARCH_BIGGER, SEARCH_SMALLER,
  SEARCH_BIGGER, SEARCH_SMALLER, SEARCH_SMALLER
};

static int always_valid(const char *filename __attribute__((unused)))
{
  return 0;
}

int (*maria_test_invalid_symlink)(const char *filename)= always_valid;
my_bool (*ma_killed)(MARIA_HA *)= ma_killed_standalone;

#ifdef HAVE_PSI_INTERFACE

PSI_mutex_key key_SHARE_BITMAP_lock, key_SORT_INFO_mutex,
              key_THR_LOCK_maria, key_TRANSLOG_BUFFER_mutex,
              key_LOCK_soft_sync,
              key_TRANSLOG_DESCRIPTOR_dirty_buffer_mask_lock,
              key_TRANSLOG_DESCRIPTOR_sent_to_disk_lock,
              key_TRANSLOG_DESCRIPTOR_log_flush_lock,
              key_TRANSLOG_DESCRIPTOR_file_header_lock,
              key_TRANSLOG_DESCRIPTOR_unfinished_files_lock,
              key_TRANSLOG_DESCRIPTOR_purger_lock,
              key_SHARE_intern_lock, key_SHARE_key_del_lock,
              key_SHARE_close_lock, key_PAGECACHE_cache_lock,
              key_SERVICE_THREAD_CONTROL_lock,
              key_LOCK_trn_list, key_TRN_state_lock;

PSI_cond_key key_SHARE_key_del_cond, key_SERVICE_THREAD_CONTROL_cond,
             key_SORT_INFO_cond, key_SHARE_BITMAP_cond,
             key_COND_soft_sync, key_TRANSLOG_BUFFER_waiting_filling_buffer,
             key_TRANSLOG_BUFFER_prev_sent_to_disk_cond,
             key_TRANSLOG_DESCRIPTOR_log_flush_cond,
             key_TRANSLOG_DESCRIPTOR_new_goal_cond;

PSI_rwlock_key key_KEYINFO_root_lock, key_SHARE_mmap_lock,
               key_TRANSLOG_DESCRIPTOR_open_files_lock;

PSI_thread_key key_thread_checkpoint, key_thread_find_all_keys,
               key_thread_soft_sync;

PSI_file_key key_file_translog, key_file_kfile, key_file_dfile,
             key_file_control, key_file_tmp;

#endif /* HAVE_PSI_INTERFACE */
