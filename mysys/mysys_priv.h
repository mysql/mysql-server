/* Copyright (c) 2000, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSYS_PRIV_INCLUDED
#define MYSYS_PRIV_INCLUDED

#include "my_global.h"
#include "mysql/psi/mysql_thread.h"

#ifdef HAVE_PSI_INTERFACE

#include <mysql/psi/mysql_file.h>
#include <mysql/psi/mysql_thread.h>

C_MODE_START

extern PSI_mutex_key key_BITMAP_mutex, key_IO_CACHE_append_buffer_lock,
  key_IO_CACHE_SHARE_mutex, key_KEY_CACHE_cache_lock,
  key_THR_LOCK_charset, key_THR_LOCK_heap,
  key_THR_LOCK_lock, key_THR_LOCK_malloc,
  key_THR_LOCK_mutex, key_THR_LOCK_myisam, key_THR_LOCK_net,
  key_THR_LOCK_open, key_THR_LOCK_threads,
  key_TMPDIR_mutex, key_THR_LOCK_myisam_mmap;

extern PSI_rwlock_key key_SAFE_HASH_lock;

extern PSI_cond_key key_IO_CACHE_SHARE_cond,
  key_IO_CACHE_SHARE_cond_writer,
  key_THR_COND_threads;

#endif /* HAVE_PSI_INTERFACE */

extern PSI_stage_info stage_waiting_for_table_level_lock;

extern mysql_mutex_t THR_LOCK_malloc, THR_LOCK_open, THR_LOCK_keycache;
extern mysql_mutex_t THR_LOCK_lock, THR_LOCK_net;
extern mysql_mutex_t THR_LOCK_charset;

#ifdef HAVE_PSI_INTERFACE
#ifdef HAVE_LINUX_LARGE_PAGES
extern PSI_file_key key_file_proc_meminfo;
#endif /* HAVE_LINUX_LARGE_PAGES */
extern PSI_file_key key_file_charset;

C_MODE_END

#endif /* HAVE_PSI_INTERFACE */

/* These keys are always defined. */

C_MODE_START

extern PSI_memory_key key_memory_charset_file;
extern PSI_memory_key key_memory_charset_loader;
extern PSI_memory_key key_memory_lf_node;
extern PSI_memory_key key_memory_lf_dynarray;
extern PSI_memory_key key_memory_lf_slist;
extern PSI_memory_key key_memory_LIST;
extern PSI_memory_key key_memory_IO_CACHE;
extern PSI_memory_key key_memory_KEY_CACHE;
extern PSI_memory_key key_memory_SAFE_HASH_ENTRY;
extern PSI_memory_key key_memory_MY_TMPDIR_full_list;
extern PSI_memory_key key_memory_MY_BITMAP_bitmap;
extern PSI_memory_key key_memory_my_compress_alloc;
extern PSI_memory_key key_memory_pack_frm;
extern PSI_memory_key key_memory_my_err_head;
extern PSI_memory_key key_memory_my_file_info;
extern PSI_memory_key key_memory_MY_DIR;
extern PSI_memory_key key_memory_MY_STAT;
extern PSI_memory_key key_memory_QUEUE;
extern PSI_memory_key key_memory_DYNAMIC_STRING;
extern PSI_memory_key key_memory_TREE;
extern PSI_memory_key key_memory_defaults;

#ifdef _WIN32
extern PSI_memory_key key_memory_win_SECURITY_ATTRIBUTES;
extern PSI_memory_key key_memory_win_PACL;
extern PSI_memory_key key_memory_win_IP_ADAPTER_ADDRESSES;
#endif

C_MODE_END

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

#endif /* MYSYS_PRIV_INCLUDED */
