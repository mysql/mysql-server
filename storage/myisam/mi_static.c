/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
  Static variables for MyISAM library. All definied here for easy making of
  a shared library
*/

#ifndef _global_h
#include "myisamdef.h"
#endif

LIST	*myisam_open_list=0;
uchar	myisam_file_magic[]=
{ (uchar) 254, (uchar) 254,'\007', '\001', };
uchar	myisam_pack_file_magic[]=
{ (uchar) 254, (uchar) 254,'\010', '\002', };
char * myisam_log_filename=(char*) "myisam.log";
File	myisam_log_file= -1;
uint	myisam_quick_table_bits=9;
ulong	myisam_block_size= MI_KEY_BLOCK_LENGTH;		/* Best by test */
my_bool myisam_flush=0, myisam_delay_key_write=0, myisam_single_user=0;
#if !defined(DONT_USE_RW_LOCKS)
ulong myisam_concurrent_insert= 2;
#else
ulong myisam_concurrent_insert= 0;
#endif
ulonglong myisam_max_temp_length= MAX_FILE_SIZE;
ulong    myisam_data_pointer_size=4;
ulonglong    myisam_mmap_size= SIZE_T_MAX, myisam_mmap_used= 0;

static int always_valid(const char *filename __attribute__((unused)))
{
  return 0;
}

int (*myisam_test_invalid_symlink)(const char *filename)= always_valid;


/*
  read_vec[] is used for converting between P_READ_KEY.. and SEARCH_
  Position is , == , >= , <= , > , <
*/

uint myisam_read_vec[]=
{
  SEARCH_FIND, SEARCH_FIND | SEARCH_BIGGER, SEARCH_FIND | SEARCH_SMALLER,
  SEARCH_NO_FIND | SEARCH_BIGGER, SEARCH_NO_FIND | SEARCH_SMALLER,
  SEARCH_FIND | SEARCH_PREFIX, SEARCH_LAST, SEARCH_LAST | SEARCH_SMALLER,
  MBR_CONTAIN, MBR_INTERSECT, MBR_WITHIN, MBR_DISJOINT, MBR_EQUAL
};

uint myisam_readnext_vec[]=
{
  SEARCH_BIGGER, SEARCH_BIGGER, SEARCH_SMALLER, SEARCH_BIGGER, SEARCH_SMALLER,
  SEARCH_BIGGER, SEARCH_SMALLER, SEARCH_SMALLER
};

#ifdef HAVE_PSI_INTERFACE
PSI_mutex_key mi_key_mutex_MYISAM_SHARE_intern_lock,
  mi_key_mutex_MI_SORT_INFO_mutex, mi_key_mutex_MI_CHECK_print_msg;

static PSI_mutex_info all_myisam_mutexes[]=
{
  { &mi_key_mutex_MI_SORT_INFO_mutex, "MI_SORT_INFO::mutex", 0},
  { &mi_key_mutex_MYISAM_SHARE_intern_lock, "MYISAM_SHARE::intern_lock", 0},
  { &mi_key_mutex_MI_CHECK_print_msg, "MI_CHECK::print_msg", 0}
};

PSI_rwlock_key mi_key_rwlock_MYISAM_SHARE_key_root_lock,
  mi_key_rwlock_MYISAM_SHARE_mmap_lock;

static PSI_rwlock_info all_myisam_rwlocks[]=
{
  { &mi_key_rwlock_MYISAM_SHARE_key_root_lock, "MYISAM_SHARE::key_root_lock", 0},
  { &mi_key_rwlock_MYISAM_SHARE_mmap_lock, "MYISAM_SHARE::mmap_lock", 0}
};

PSI_cond_key mi_key_cond_MI_SORT_INFO_cond;

static PSI_cond_info all_myisam_conds[]=
{
  { &mi_key_cond_MI_SORT_INFO_cond, "MI_SORT_INFO::cond", 0}
};

PSI_file_key mi_key_file_datatmp, mi_key_file_dfile, mi_key_file_kfile,
  mi_key_file_log;

static PSI_file_info all_myisam_files[]=
{
  { & mi_key_file_datatmp, "data_tmp", 0},
  { & mi_key_file_dfile, "dfile", 0},
  { & mi_key_file_kfile, "kfile", 0},
  { & mi_key_file_log, "log", 0}
};

PSI_thread_key mi_key_thread_find_all_keys;

static PSI_thread_info all_myisam_threads[]=
{
  { &mi_key_thread_find_all_keys, "find_all_keys", 0},
};

void init_myisam_psi_keys()
{
  const char* category= "myisam";
  int count;

  if (PSI_server == NULL)
    return;

  count= array_elements(all_myisam_mutexes);
  PSI_server->register_mutex(category, all_myisam_mutexes, count);

  count= array_elements(all_myisam_rwlocks);
  PSI_server->register_rwlock(category, all_myisam_rwlocks, count);

  count= array_elements(all_myisam_conds);
  PSI_server->register_cond(category, all_myisam_conds, count);

  count= array_elements(all_myisam_files);
  PSI_server->register_file(category, all_myisam_files, count);

  count= array_elements(all_myisam_threads);
  PSI_server->register_thread(category, all_myisam_threads, count);
}
#endif /* HAVE_PSI_INTERFACE */

