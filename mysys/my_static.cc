/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file mysys/my_static.cc
  Static variables for mysys library. All defined here for easy making of
  a shared library.
*/

#include "my_config.h"

#include <stdarg.h>
#include <stdarg.h>
#include <stddef.h>

#include "my_compiler.h"
#include "my_loglevel.h"
#include "my_static.h"
#include "mysql/psi/mysql_cond.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/psi/psi_base.h"
#include "mysql/psi/psi_memory.h"
#include "mysql/psi/psi_stage.h"
#include "mysys_priv.h"  // IWYU pragma: keep

/* get memory in hunks */
constexpr uint ONCE_ALLOC_INIT= 4096 - MALLOC_OVERHEAD;

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
PSI_memory_key key_memory_my_err_head;
PSI_memory_key key_memory_my_file_info;
PSI_memory_key key_memory_max_alloca;
PSI_memory_key key_memory_MY_DIR;
PSI_memory_key key_memory_MY_TMPDIR_full_list;
PSI_memory_key key_memory_DYNAMIC_STRING;
PSI_memory_key key_memory_TREE;

PSI_thread_key key_thread_timer_notifier;

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

	/* from my_malloc */
USED_MEM* my_once_root_block=0;			/* pointer to first block */
uint	  my_once_extra=ONCE_ALLOC_INIT;	/* Memory to alloc / block */

	/* from my_largepage.c */
#ifdef HAVE_LINUX_LARGE_PAGES
bool my_use_large_pages= 0;
uint my_large_page_size= 0;
#endif

	/* from errors.c */
extern "C" {
void (*error_handler_hook)(uint error, const char *str, myf MyFlags)=
  my_message_stderr;
void (*fatal_error_handler_hook)(uint error, const char *str, myf MyFlags)=
  my_message_stderr;
void (*local_message_hook)(enum loglevel ll, const char *format, va_list args)=
  my_message_local_stderr;

static void enter_cond_dummy(void *a MY_ATTRIBUTE((unused)),
                             mysql_cond_t *b MY_ATTRIBUTE((unused)),
                             mysql_mutex_t *c MY_ATTRIBUTE((unused)),
                             const PSI_stage_info *d MY_ATTRIBUTE((unused)),
                             PSI_stage_info *e MY_ATTRIBUTE((unused)),
                             const char *f MY_ATTRIBUTE((unused)),
                             const char *g MY_ATTRIBUTE((unused)),
                             int h MY_ATTRIBUTE((unused)))
{ }

static void exit_cond_dummy(void *a MY_ATTRIBUTE((unused)),
                            const PSI_stage_info *b MY_ATTRIBUTE((unused)),
                            const char *c MY_ATTRIBUTE((unused)),
                            const char *d MY_ATTRIBUTE((unused)),
                            int e MY_ATTRIBUTE((unused)))
{ }

static int is_killed_dummy(const void *a MY_ATTRIBUTE((unused)))
{
  return 0;
}

/*
  Initialize these hooks to dummy implementations. The real server
  implementations will be set during server startup by
  init_server_components().
*/
void (*enter_cond_hook)(void *, mysql_cond_t *, mysql_mutex_t *,
                        const PSI_stage_info *, PSI_stage_info *,
                        const char *, const char *, int)= enter_cond_dummy;

void (*exit_cond_hook)(void *, const PSI_stage_info *,
                       const char *, const char *, int)= exit_cond_dummy;

int (*is_killed_hook)(const void *)= is_killed_dummy;

#if defined(ENABLED_DEBUG_SYNC)
/**
  Global pointer to be set if callback function is defined
  (e.g. in mysqld). See sql/debug_sync.cc.
*/
void (*debug_sync_C_callback_ptr)(const char *, size_t);
#endif /* defined(ENABLED_DEBUG_SYNC) */
} // extern C

#ifdef _WIN32
/* from my_getsystime.c */
ulonglong query_performance_frequency, query_performance_offset,
          query_performance_offset_micros;
#endif

	/* How to disable options */
bool my_disable_locking=0;
bool my_enable_symlinks= 1;

