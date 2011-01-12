/* Copyright (C) 2000 MySQL AB, 2008-2009 Sun Microsystems, Inc

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

#include <my_global.h>
#include <my_sys.h>

#ifdef USE_SYSTEM_WRAPPERS
#include "system_wrappers.h"
#endif

#ifdef HAVE_GETRUSAGE
#include <sys/resource.h>
#endif

#include <my_pthread.h>

#ifdef HAVE_PSI_INTERFACE

#if !defined(HAVE_PREAD) && !defined(_WIN32)
extern PSI_mutex_key key_my_file_info_mutex;
#endif /* !defined(HAVE_PREAD) && !defined(_WIN32) */

#if !defined(HAVE_LOCALTIME_R) || !defined(HAVE_GMTIME_R)
extern PSI_mutex_key key_LOCK_localtime_r;
#endif /* !defined(HAVE_LOCALTIME_R) || !defined(HAVE_GMTIME_R) */

#ifndef HAVE_GETHOSTBYNAME_R
extern PSI_mutex_key key_LOCK_gethostbyname_r;
#endif /* HAVE_GETHOSTBYNAME_R */

extern PSI_mutex_key key_BITMAP_mutex, key_IO_CACHE_append_buffer_lock,
  key_IO_CACHE_SHARE_mutex, key_KEY_CACHE_cache_lock, key_LOCK_alarm,
  key_my_thread_var_mutex, key_THR_LOCK_charset, key_THR_LOCK_heap,
  key_THR_LOCK_isam, key_THR_LOCK_lock, key_THR_LOCK_malloc,
  key_THR_LOCK_mutex, key_THR_LOCK_myisam, key_THR_LOCK_net,
  key_THR_LOCK_open, key_THR_LOCK_threads, key_THR_LOCK_time,
  key_TMPDIR_mutex, key_THR_LOCK_myisam_mmap;

extern PSI_cond_key key_COND_alarm, key_IO_CACHE_SHARE_cond,
  key_IO_CACHE_SHARE_cond_writer, key_my_thread_var_suspend,
  key_THR_COND_threads;

#ifdef USE_ALARM_THREAD
extern PSI_thread_key key_thread_alarm;
#endif /* USE_ALARM_THREAD */

#endif /* HAVE_PSI_INTERFACE */

extern mysql_mutex_t THR_LOCK_malloc, THR_LOCK_open, THR_LOCK_keycache;
extern mysql_mutex_t THR_LOCK_lock, THR_LOCK_isam, THR_LOCK_net;
extern mysql_mutex_t THR_LOCK_charset, THR_LOCK_time;

#include <mysql/psi/mysql_file.h>

#ifdef HAVE_PSI_INTERFACE
#ifdef HUGETLB_USE_PROC_MEMINFO
extern PSI_file_key key_file_proc_meminfo;
#endif /* HUGETLB_USE_PROC_MEMINFO */
extern PSI_file_key key_file_charset, key_file_cnf;
#endif /* HAVE_PSI_INTERFACE */

/*
  EDQUOT is used only in 3 C files only in mysys/. If it does not exist on
  system, we set it to some value which can never happen.
*/
#ifndef EDQUOT
#define EDQUOT (-1)
#endif

void my_error_unregister_all(void);

#ifdef _WIN32
#include <sys/stat.h>
/* my_winfile.c exports, should not be used outside mysys */
extern File     my_win_open(const char *path, int oflag);
extern int      my_win_close(File fd);
extern size_t   my_win_read(File fd, uchar *buffer, size_t  count);
extern size_t   my_win_write(File fd, const uchar *buffer, size_t count);
extern size_t   my_win_pread(File fd, uchar *buffer, size_t count, 
                             my_off_t offset);
extern size_t   my_win_pwrite(File fd, const uchar *buffer, size_t count, 
                              my_off_t offset);
extern my_off_t my_win_lseek(File fd, my_off_t pos, int whence);
extern int      my_win_chsize(File fd,  my_off_t newlength);
extern FILE*    my_win_fopen(const char *filename, const char *type);
extern File     my_win_fclose(FILE *file);
extern File     my_win_fileno(FILE *file);
extern FILE*    my_win_fdopen(File Filedes, const char *type);
extern int      my_win_stat(const char *path, struct _stati64 *buf);
extern int      my_win_fstat(File fd, struct _stati64 *buf);
extern int      my_win_fsync(File fd);
extern File     my_win_dup(File fd);
extern File     my_win_sopen(const char *path, int oflag, int shflag, int perm);
extern File     my_open_osfhandle(HANDLE handle, int oflag);
#endif
