/* Copyright (c) 2000, 2014, Oracle and/or its affiliates. All rights reserved.

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
  Static variables for mysys library. All definied here for easy making of
  a shared library
*/

#include "mysys_priv.h"
#include "my_static.h"

PSI_memory_key key_memory_array_buffer;
PSI_memory_key key_memory_charset_file;
PSI_memory_key key_memory_charset_loader;
PSI_memory_key key_memory_lf_node;
PSI_memory_key key_memory_lf_dynarray;
PSI_memory_key key_memory_lf_slist;
PSI_memory_key key_memory_LIST;
PSI_memory_key key_memory_IO_CACHE;
PSI_memory_key key_memory_KEY_CACHE;
PSI_memory_key key_memory_SAFE_HASH_ENTRY;
PSI_memory_key key_memory_MY_BITMAP_bitmap;
PSI_memory_key key_memory_my_compress_alloc;
PSI_memory_key key_memory_pack_frm;
PSI_memory_key key_memory_my_err_head;
PSI_memory_key key_memory_my_file_info;
PSI_memory_key key_memory_max_alloca;
PSI_memory_key key_memory_MY_DIR;
PSI_memory_key key_memory_MY_STAT;
PSI_memory_key key_memory_MY_TMPDIR_full_list;
PSI_memory_key key_memory_QUEUE;
PSI_memory_key key_memory_DYNAMIC_STRING;
PSI_memory_key key_memory_TREE;

#ifdef _WIN32
PSI_memory_key key_memory_win_SECURITY_ATTRIBUTES;
PSI_memory_key key_memory_win_PACL;
PSI_memory_key key_memory_win_IP_ADAPTER_ADDRESSES;
#endif /* _WIN32 */

	/* from my_init */
char *	home_dir=0;
const char      *my_progname=0;
char		curr_dir[FN_REFLEN]= {0},
		home_dir_buff[FN_REFLEN]= {0};
ulong		my_stream_opened=0,my_file_opened=0, my_tmp_file_created=0;
ulong           my_file_total_opened= 0;
int		my_umask=0664, my_umask_dir=0777;

struct st_my_file_info my_file_info_default[MY_NFILE];
uint   my_file_limit= MY_NFILE;
struct st_my_file_info *my_file_info= my_file_info_default;

	/* from mf_reccache.c */
ulong my_default_record_cache_size=RECORD_CACHE_SIZE;

	/* from soundex.c */
				/* ABCDEFGHIJKLMNOPQRSTUVWXYZ */
				/* :::::::::::::::::::::::::: */
const char *soundex_map=	  "01230120022455012623010202";

	/* from my_malloc */
USED_MEM* my_once_root_block=0;			/* pointer to first block */
uint	  my_once_extra=ONCE_ALLOC_INIT;	/* Memory to alloc / block */

	/* from my_largepage.c */
#ifdef HAVE_LINUX_LARGE_PAGES
my_bool my_use_large_pages= 0;
uint    my_large_page_size= 0;
#endif

	/* from errors.c */
void (*error_handler_hook)(uint error, const char *str, myf MyFlags)=
  my_message_stderr;
void (*fatal_error_handler_hook)(uint error, const char *str, myf MyFlags)=
  my_message_stderr;
void (*local_message_hook)(enum loglevel ll, const char *format, va_list args)=
  my_message_local_stderr;

static void proc_info_dummy(void *a __attribute__((unused)),
                            const PSI_stage_info *b __attribute__((unused)),
                            PSI_stage_info *c __attribute__((unused)),
                            const char *d __attribute__((unused)),
                            const char *e __attribute__((unused)),
                            const unsigned int f __attribute__((unused)))
{
  return;
}

/* this is to be able to call set_thd_proc_info from the C code */
void (*proc_info_hook)(void *, const PSI_stage_info *, PSI_stage_info *,
                       const char *, const char *, const unsigned int)= proc_info_dummy;

#if defined(ENABLED_DEBUG_SYNC)
/**
  Global pointer to be set if callback function is defined
  (e.g. in mysqld). See sql/debug_sync.cc.
*/
void (*debug_sync_C_callback_ptr)(const char *, size_t);
#endif /* defined(ENABLED_DEBUG_SYNC) */

#ifdef _WIN32
/* from my_getsystime.c */
ulonglong query_performance_frequency, query_performance_offset;
#endif

	/* How to disable options */
my_bool my_disable_locking=0;
my_bool my_enable_symlinks= 1;

