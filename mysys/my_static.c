/* Copyright (c) 2000, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
  Static variables for mysys library. All definied here for easy making of
  a shared library
*/

#include "mysys_priv.h"
#include "my_static.h"
#include "my_alarm.h"

my_bool timed_mutexes= 0;

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

	/* From mf_brkhant */
int			my_dont_interrupt=0;
volatile int		_my_signals=0;
struct st_remember _my_sig_remember[MAX_SIGNALS]={{0,0}};

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
#ifdef HAVE_LARGE_PAGES
my_bool my_use_large_pages= 0;
uint    my_large_page_size= 0;
#endif

	/* from my_alarm */
int volatile my_have_got_alarm=0;	/* declare variable to reset */
ulong my_time_to_wait_for_lock=2;	/* In seconds */

	/* from errors.c */
#ifdef SHARED_LIBRARY
const char *globerrs[GLOBERRS];		/* my_error_messages is here */
#endif
void (*my_abort_hook)(int) = (void(*)(int)) exit;
void (*error_handler_hook)(uint error, const char *str, myf MyFlags)=
  my_message_stderr;
void (*fatal_error_handler_hook)(uint error, const char *str, myf MyFlags)=
  my_message_stderr;

static void proc_info_dummy(void *a MY_ATTRIBUTE((unused)),
                            const PSI_stage_info *b MY_ATTRIBUTE((unused)),
                            PSI_stage_info *c MY_ATTRIBUTE((unused)),
                            const char *d MY_ATTRIBUTE((unused)),
                            const char *e MY_ATTRIBUTE((unused)),
                            const unsigned int f MY_ATTRIBUTE((unused)))
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

#ifdef __WIN__
/* from my_getsystime.c */
ulonglong query_performance_frequency, query_performance_offset;
#endif

	/* How to disable options */
my_bool my_disable_locking=0;
my_bool my_disable_async_io=0;
my_bool my_disable_flush_key_blocks=0;
my_bool my_disable_symlinks=0;

