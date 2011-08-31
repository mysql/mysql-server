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

static const char *proc_info_dummy(void *a __attribute__((unused)),
                                   const char *b __attribute__((unused)),
                                   const char *c __attribute__((unused)),
                                   const char *d __attribute__((unused)),
                                   const unsigned int e __attribute__((unused)))
{
  return 0;
}

/* this is to be able to call set_thd_proc_info from the C code */
const char *(*proc_info_hook)(void *, const char *, const char *, const char *,
                              const unsigned int)= proc_info_dummy;

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

/*
  Note that PSI_hook and PSI_server are unconditionally
  (no ifdef HAVE_PSI_INTERFACE) defined.
  This is to ensure binary compatibility between the server and plugins,
  in the case when:
  - the server is not compiled with HAVE_PSI_INTERFACE
  - a plugin is compiled with HAVE_PSI_INTERFACE
  See the doxygen documentation for the performance schema.
*/

/**
  Hook for the instrumentation interface.
  Code implementing the instrumentation interface should register here.
*/
struct PSI_bootstrap *PSI_hook= NULL;

/**
  Instance of the instrumentation interface for the MySQL server.
  @todo This is currently a global variable, which is handy when
  compiling instrumented code that is bundled with the server.
  When dynamic plugin are truly supported, this variable will need
  to be replaced by a macro, so that each XYZ plugin can have it's own
  xyz_psi_server variable, obtained from PSI_bootstrap::get_interface()
  with the version used at compile time for plugin XYZ.
*/
PSI *PSI_server= NULL;

