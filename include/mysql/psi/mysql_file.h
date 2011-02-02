/* Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef MYSQL_FILE_H
#define MYSQL_FILE_H

#include <my_global.h>

/* For strlen() */
#include <string.h>
/* For MY_STAT */
#include <my_dir.h>
/* For my_chsize */
#include <my_sys.h>

/**
  @file mysql/psi/mysql_file.h
  Instrumentation helpers for mysys file io.
  This header file provides the necessary declarations
  to use the mysys file API with the performance schema instrumentation.
  In some compilers (SunStudio), 'static inline' functions, when declared
  but not used, are not optimized away (because they are unused) by default,
  so that including a static inline function from a header file does
  create unwanted dependencies, causing unresolved symbols at link time.
  Other compilers, like gcc, optimize these dependencies by default.

  Since the instrumented APIs declared here are wrapper on top
  of mysys file io APIs, including mysql/psi/mysql_file.h assumes that
  the dependency on my_sys already exists.
*/

#include "mysql/psi/psi.h"

/**
  @defgroup File_instrumentation File Instrumentation
  @ingroup Instrumentation_interface
  @{
*/

/**
  @def mysql_file_fgets(P1, P2, F)
  Instrumented fgets.
  @c mysql_file_fgets is a replacement for @c fgets.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_fgets(P1, P2, F) \
    inline_mysql_file_fgets(__FILE__, __LINE__, P1, P2, F)
#else
  #define mysql_file_fgets(P1, P2, F) \
    inline_mysql_file_fgets(P1, P2, F)
#endif

/**
  @def mysql_file_fgetc(F)
  Instrumented fgetc.
  @c mysql_file_fgetc is a replacement for @c fgetc.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_fgetc(F) inline_mysql_file_fgetc(__FILE__, __LINE__, F)
#else
  #define mysql_file_fgetc(F) inline_mysql_file_fgetc(F)
#endif

/**
  @def mysql_file_fputs(P1, F)
  Instrumented fputs.
  @c mysql_file_fputs is a replacement for @c fputs.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_fputs(P1, F) \
    inline_mysql_file_fputs(__FILE__, __LINE__, P1, F)
#else
  #define mysql_file_fputs(P1, F)\
    inline_mysql_file_fputs(P1, F)
#endif

/**
  @def mysql_file_fputc(P1, F)
  Instrumented fputc.
  @c mysql_file_fputc is a replacement for @c fputc.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_fputc(P1, F) \
    inline_mysql_file_fputc(__FILE__, __LINE__, P1, F)
#else
  #define mysql_file_fputc(P1, F) \
    inline_mysql_file_fputc(P1, F)
#endif

/**
  @def mysql_file_fprintf
  Instrumented fprintf.
  @c mysql_file_fprintf is a replacement for @c fprintf.
*/
#define mysql_file_fprintf inline_mysql_file_fprintf

/**
  @def mysql_file_vfprintf(F, P1, P2)
  Instrumented vfprintf.
  @c mysql_file_vfprintf is a replacement for @c vfprintf.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_vfprintf(F, P1, P2) \
    inline_mysql_file_vfprintf(__FILE__, __LINE__, F, P1, P2)
#else
  #define mysql_file_vfprintf(F, P1, P2) \
    inline_mysql_file_vfprintf(F, P1, P2)
#endif

/**
  @def mysql_file_fflush(F, P1, P2)
  Instrumented fflush.
  @c mysql_file_fflush is a replacement for @c fflush.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_fflush(F) \
    inline_mysql_file_fflush(__FILE__, __LINE__, F)
#else
  #define mysql_file_fflush(F) \
    inline_mysql_file_fflush(F)
#endif

/**
  @def mysql_file_feof(F)
  Instrumented feof.
  @c mysql_file_feof is a replacement for @c feof.
*/
#define mysql_file_feof(F) inline_mysql_file_feof(F)

/**
  @def mysql_file_fstat(FN, S, FL)
  Instrumented fstat.
  @c mysql_file_fstat is a replacement for @c my_fstat.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_fstat(FN, S, FL) \
    inline_mysql_file_fstat(__FILE__, __LINE__, FN, S, FL)
#else
  #define mysql_file_fstat(FN, S, FL) \
    inline_mysql_file_fstat(FN, S, FL)
#endif

/**
  @def mysql_file_stat(K, FN, S, FL)
  Instrumented stat.
  @c mysql_file_stat is a replacement for @c my_stat.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_stat(K, FN, S, FL) \
    inline_mysql_file_stat(K, __FILE__, __LINE__, FN, S, FL)
#else
  #define mysql_file_stat(K, FN, S, FL) \
    inline_mysql_file_stat(FN, S, FL)
#endif

/**
  @def mysql_file_chsize(F, P1, P2, P3)
  Instrumented chsize.
  @c mysql_file_chsize is a replacement for @c my_chsize.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_chsize(F, P1, P2, P3) \
    inline_mysql_file_chsize(__FILE__, __LINE__, F, P1, P2, P3)
#else
  #define mysql_file_chsize(F, P1, P2, P3) \
    inline_mysql_file_chsize(F, P1, P2, P3)
#endif

/**
  @def mysql_file_fopen(K, N, F1, F2)
  Instrumented fopen.
  @c mysql_file_fopen is a replacement for @c my_fopen.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_fopen(K, N, F1, F2) \
    inline_mysql_file_fopen(K, __FILE__, __LINE__, N, F1, F2)
#else
  #define mysql_file_fopen(K, N, F1, F2) \
    inline_mysql_file_fopen(N, F1, F2)
#endif

/**
  @def mysql_file_fclose(FD, FL)
  Instrumented fclose.
  @c mysql_file_fclose is a replacement for @c my_fclose.
  Without the instrumentation, this call will have the same behavior as the
  undocumented and possibly platform specific my_fclose(NULL, ...) behavior.
  With the instrumentation, mysql_fclose(NULL, ...) will safely return 0,
  which is an extension compared to my_fclose and is therefore compliant.
  mysql_fclose is on purpose *not* implementing
  @code DBUG_ASSERT(file != NULL) @endcode,
  since doing so could introduce regressions.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_fclose(FD, FL) \
    inline_mysql_file_fclose(__FILE__, __LINE__, FD, FL)
#else
  #define mysql_file_fclose(FD, FL) \
    inline_mysql_file_fclose(FD, FL)
#endif

/**
  @def mysql_file_fread(FD, P1, P2, P3)
  Instrumented fread.
  @c mysql_file_fread is a replacement for @c my_fread.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_fread(FD, P1, P2, P3) \
    inline_mysql_file_fread(__FILE__, __LINE__, FD, P1, P2, P3)
#else
  #define mysql_file_fread(FD, P1, P2, P3) \
    inline_mysql_file_fread(FD, P1, P2, P3)
#endif

/**
  @def mysql_file_fwrite(FD, P1, P2, P3)
  Instrumented fwrite.
  @c mysql_file_fwrite is a replacement for @c my_fwrite.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_fwrite(FD, P1, P2, P3) \
    inline_mysql_file_fwrite(__FILE__, __LINE__, FD, P1, P2, P3)
#else
  #define mysql_file_fwrite(FD, P1, P2, P3) \
    inline_mysql_file_fwrite(FD, P1, P2, P3)
#endif

/**
  @def mysql_file_fseek(FD, P, W, F)
  Instrumented fseek.
  @c mysql_file_fseek is a replacement for @c my_fseek.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_fseek(FD, P, W, F) \
    inline_mysql_file_fseek(__FILE__, __LINE__, FD, P, W, F)
#else
  #define mysql_file_fseek(FD, P, W, F) \
    inline_mysql_file_fseek(FD, P, W, F)
#endif

/**
  @def mysql_file_ftell(FD, F)
  Instrumented ftell.
  @c mysql_file_ftell is a replacement for @c my_ftell.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_ftell(FD, F) \
    inline_mysql_file_ftell(__FILE__, __LINE__, FD, F)
#else
  #define mysql_file_ftell(FD, F) \
    inline_mysql_file_ftell(FD, F)
#endif

/**
  @def mysql_file_create(K, N, F1, F2, F3)
  Instrumented create.
  @c mysql_file_create is a replacement for @c my_create.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_create(K, N, F1, F2, F3) \
  inline_mysql_file_create(K, __FILE__, __LINE__, N, F1, F2, F3)
#else
  #define mysql_file_create(K, N, F1, F2, F3) \
    inline_mysql_file_create(N, F1, F2, F3)
#endif

/**
  @def mysql_file_create_temp(K, T, D, P, M, F)
  Instrumented create_temp_file.
  @c mysql_file_create_temp is a replacement for @c create_temp_file.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_create_temp(K, T, D, P, M, F) \
    inline_mysql_file_create_temp(K, T, D, P, M, F)
#else
  #define mysql_file_create_temp(K, T, D, P, M, F) \
    inline_mysql_file_create_temp(T, D, P, M, F)
#endif

/**
  @def mysql_file_open(K, N, F1, F2)
  Instrumented open.
  @c mysql_file_open is a replacement for @c my_open.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_open(K, N, F1, F2) \
    inline_mysql_file_open(K, __FILE__, __LINE__, N, F1, F2)
#else
  #define mysql_file_open(K, N, F1, F2) \
    inline_mysql_file_open(N, F1, F2)
#endif

/**
  @def mysql_file_close(FD, F)
  Instrumented close.
  @c mysql_file_close is a replacement for @c my_close.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_close(FD, F) \
    inline_mysql_file_close(__FILE__, __LINE__, FD, F)
#else
  #define mysql_file_close(FD, F) \
    inline_mysql_file_close(FD, F)
#endif

/**
  @def mysql_file_read(FD, B, S, F)
  Instrumented read.
  @c mysql_read is a replacement for @c my_read.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_read(FD, B, S, F) \
    inline_mysql_file_read(__FILE__, __LINE__, FD, B, S, F)
#else
  #define mysql_file_read(FD, B, S, F) \
    inline_mysql_file_read(FD, B, S, F)
#endif

/**
  @def mysql_file_write(FD, B, S, F)
  Instrumented write.
  @c mysql_file_write is a replacement for @c my_write.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_write(FD, B, S, F) \
    inline_mysql_file_write(__FILE__, __LINE__, FD, B, S, F)
#else
  #define mysql_file_write(FD, B, S, F) \
    inline_mysql_file_write(FD, B, S, F)
#endif

/**
  @def mysql_file_pread(FD, B, S, O, F)
  Instrumented pread.
  @c mysql_pread is a replacement for @c my_pread.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_pread(FD, B, S, O, F) \
    inline_mysql_file_pread(__FILE__, __LINE__, FD, B, S, O, F)
#else
  #define mysql_file_pread(FD, B, S, O, F) \
    inline_mysql_file_pread(FD, B, S, O, F)
#endif

/**
  @def mysql_file_pwrite(FD, B, S, O, F)
  Instrumented pwrite.
  @c mysql_file_pwrite is a replacement for @c my_pwrite.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_pwrite(FD, B, S, O, F) \
    inline_mysql_file_pwrite(__FILE__, __LINE__, FD, B, S, O, F)
#else
  #define mysql_file_pwrite(FD, B, S, O, F) \
    inline_mysql_file_pwrite(FD, B, S, O, F)
#endif

/**
  @def mysql_file_seek(FD, P, W, F)
  Instrumented seek.
  @c mysql_file_seek is a replacement for @c my_seek.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_seek(FD, P, W, F) \
    inline_mysql_file_seek(__FILE__, __LINE__, FD, P, W, F)
#else
  #define mysql_file_seek(FD, P, W, F) \
    inline_mysql_file_seek(FD, P, W, F)
#endif

/**
  @def mysql_file_tell(FD, F)
  Instrumented tell.
  @c mysql_file_tell is a replacement for @c my_tell.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_tell(FD, F) \
    inline_mysql_file_tell(__FILE__, __LINE__, FD, F)
#else
  #define mysql_file_tell(FD, F) \
    inline_mysql_file_tell(FD, F)
#endif

/**
  @def mysql_file_delete(K, P1, P2)
  Instrumented delete.
  @c mysql_file_delete is a replacement for @c my_delete.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_delete(K, P1, P2) \
    inline_mysql_file_delete(K, __FILE__, __LINE__, P1, P2)
#else
  #define mysql_file_delete(K, P1, P2) \
    inline_mysql_file_delete(P1, P2)
#endif

/**
  @def mysql_file_rename(K, P1, P2, P3)
  Instrumented rename.
  @c mysql_file_rename is a replacement for @c my_rename.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_rename(K, P1, P2, P3) \
    inline_mysql_file_rename(K, __FILE__, __LINE__, P1, P2, P3)
#else
  #define mysql_file_rename(K, P1, P2, P3) \
    inline_mysql_file_rename(P1, P2, P3)
#endif

/**
  @def mysql_file_create_with_symlink(K, P1, P2, P3, P4, P5)
  Instrumented create with symbolic link.
  @c mysql_file_create_with_symlink is a replacement
  for @c my_create_with_symlink.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_create_with_symlink(K, P1, P2, P3, P4, P5) \
  inline_mysql_file_create_with_symlink(K, __FILE__, __LINE__, \
                                        P1, P2, P3, P4, P5)
#else
  #define mysql_file_create_with_symlink(K, P1, P2, P3, P4, P5) \
  inline_mysql_file_create_with_symlink(P1, P2, P3, P4, P5)
#endif

/**
  @def mysql_file_delete_with_symlink(K, P1, P2)
  Instrumented delete with symbolic link.
  @c mysql_file_delete_with_symlink is a replacement
  for @c my_delete_with_symlink.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_delete_with_symlink(K, P1, P2) \
  inline_mysql_file_delete_with_symlink(K, __FILE__, __LINE__, P1, P2)
#else
  #define mysql_file_delete_with_symlink(K, P1, P2) \
  inline_mysql_file_delete_with_symlink(P1, P2)
#endif

/**
  @def mysql_file_rename_with_symlink(K, P1, P2, P3)
  Instrumented rename with symbolic link.
  @c mysql_file_rename_with_symlink is a replacement
  for @c my_rename_with_symlink.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_rename_with_symlink(K, P1, P2, P3) \
  inline_mysql_file_rename_with_symlink(K, __FILE__, __LINE__, P1, P2, P3)
#else
  #define mysql_file_rename_with_symlink(K, P1, P2, P3) \
  inline_mysql_file_rename_with_symlink(P1, P2, P3)
#endif

/**
  @def mysql_file_sync(P1, P2)
  Instrumented file sync.
  @c mysql_file_sync is a replacement for @c my_sync.
*/
#ifdef HAVE_PSI_INTERFACE
  #define mysql_file_sync(P1, P2) \
    inline_mysql_file_sync(__FILE__, __LINE__, P1, P2)
#else
  #define mysql_file_sync(P1, P2) \
    inline_mysql_file_sync(P1, P2)
#endif

/**
  An instrumented FILE structure.
  @sa MYSQL_FILE
*/
struct st_mysql_file
{
  /** The real file. */
  FILE *m_file;
  /**
    The instrumentation hook.
    Note that this hook is not conditionally defined,
    for binary compatibility of the @c MYSQL_FILE interface.
  */
  struct PSI_file *m_psi;
};

/**
  Type of an instrumented file.
  @c MYSQL_FILE is a drop-in replacement for @c FILE.
  @sa mysql_file_open
*/
typedef struct st_mysql_file MYSQL_FILE;

static inline char *
inline_mysql_file_fgets(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
  char *str, int size, MYSQL_FILE *file)
{
  char *result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_file_locker *locker= NULL;
  PSI_file_locker_state state;
  if (likely(PSI_server && file->m_psi))
  {
    locker= PSI_server->get_thread_file_stream_locker(&state, file->m_psi,
                                                      PSI_FILE_READ);
    if (likely(locker != NULL))
      PSI_server->start_file_wait(locker, (size_t) size, src_file, src_line);
  }
#endif
  result= fgets(str, size, file->m_file);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_file_wait(locker, result ? strlen(result) : 0);
#endif
  return result;
}

static inline int
inline_mysql_file_fgetc(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
  MYSQL_FILE *file)
{
  int result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_file_locker *locker= NULL;
  PSI_file_locker_state state;
  if (likely(PSI_server && file->m_psi))
  {
    locker= PSI_server->get_thread_file_stream_locker(&state, file->m_psi,
                                                      PSI_FILE_READ);
    if (likely(locker != NULL))
      PSI_server->start_file_wait(locker, (size_t) 1, src_file, src_line);
  }
#endif
  result= fgetc(file->m_file);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_file_wait(locker, (size_t) 1);
#endif
  return result;
}

static inline int
inline_mysql_file_fputs(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
  const char *str, MYSQL_FILE *file)
{
  int result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_file_locker *locker= NULL;
  PSI_file_locker_state state;
  size_t bytes= 0;
  if (likely(PSI_server && file->m_psi))
  {
    locker= PSI_server->get_thread_file_stream_locker(&state, file->m_psi,
                                                      PSI_FILE_WRITE);
    if (likely(locker != NULL))
    {
      bytes= str ? strlen(str) : 0;
      PSI_server->start_file_wait(locker, bytes, src_file, src_line);
    }
  }
#endif
  result= fputs(str, file->m_file);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_file_wait(locker, bytes);
#endif
  return result;
}

static inline int
inline_mysql_file_fputc(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
  char c, MYSQL_FILE *file)
{
  int result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_file_locker *locker= NULL;
  PSI_file_locker_state state;
  if (likely(PSI_server && file->m_psi))
  {
    locker= PSI_server->get_thread_file_stream_locker(&state, file->m_psi,
                                                      PSI_FILE_WRITE);
    if (likely(locker != NULL))
      PSI_server->start_file_wait(locker, (size_t) 1, src_file, src_line);
  }
#endif
  result= fputc(c, file->m_file);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_file_wait(locker, (size_t) 1);
#endif
  return result;
}

static inline int
inline_mysql_file_fprintf(MYSQL_FILE *file, const char *format, ...)
{
  /*
    TODO: figure out how to pass src_file and src_line from the caller.
  */
  int result;
  va_list args;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_file_locker *locker= NULL;
  PSI_file_locker_state state;
  if (likely(PSI_server && file->m_psi))
  {
    locker= PSI_server->get_thread_file_stream_locker(&state, file->m_psi,
                                                      PSI_FILE_WRITE);
    if (likely(locker != NULL))
      PSI_server->start_file_wait(locker, (size_t) 0, __FILE__, __LINE__);
  }
#endif
  va_start(args, format);
  result= vfprintf(file->m_file, format, args);
  va_end(args);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_file_wait(locker, (size_t) result);
#endif
  return result;
}

static inline int
inline_mysql_file_vfprintf(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
  MYSQL_FILE *file, const char *format, va_list args)
{
  int result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_file_locker *locker= NULL;
  PSI_file_locker_state state;
  if (likely(PSI_server && file->m_psi))
  {
    locker= PSI_server->get_thread_file_stream_locker(&state, file->m_psi,
                                                      PSI_FILE_WRITE);
    if (likely(locker != NULL))
      PSI_server->start_file_wait(locker, (size_t) 0, src_file, src_line);
  }
#endif
  result= vfprintf(file->m_file, format, args);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_file_wait(locker, (size_t) result);
#endif
  return result;
}

static inline int
inline_mysql_file_fflush(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
  MYSQL_FILE *file)
{
  int result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_file_locker *locker= NULL;
  PSI_file_locker_state state;
  if (likely(PSI_server && file->m_psi))
  {
    locker= PSI_server->get_thread_file_stream_locker(&state, file->m_psi,
                                                      PSI_FILE_FLUSH);
    if (likely(locker != NULL))
      PSI_server->start_file_wait(locker, (size_t) 0, src_file, src_line);
  }
#endif
  result= fflush(file->m_file);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_file_wait(locker, (size_t) 0);
#endif
  return result;
}

static inline int inline_mysql_file_feof(MYSQL_FILE *file)
{
  /* Not instrumented, there is no wait involved */
  return feof(file->m_file);
}

static inline int
inline_mysql_file_fstat(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
  int filenr, MY_STAT *stat_area, myf flags)
{
  int result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_file_locker *locker= NULL;
  PSI_file_locker_state state;
  if (likely(PSI_server != NULL))
  {
    locker= PSI_server->get_thread_file_descriptor_locker(&state, filenr,
                                                          PSI_FILE_FSTAT);
    if (likely(locker != NULL))
      PSI_server->start_file_wait(locker, (size_t) 0, src_file, src_line);
  }
#endif
  result= my_fstat(filenr, stat_area, flags);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_file_wait(locker, (size_t) 0);
#endif
  return result;
}

static inline MY_STAT *
inline_mysql_file_stat(
#ifdef HAVE_PSI_INTERFACE
  PSI_file_key key, const char *src_file, uint src_line,
#endif
  const char *path, MY_STAT *stat_area, myf flags)
{
  MY_STAT *result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_file_locker *locker= NULL;
  PSI_file_locker_state state;
  if (likely(PSI_server != NULL))
  {
    locker= PSI_server->get_thread_file_name_locker(&state,
                                                    key, PSI_FILE_STAT,
                                                    path, &locker);
    if (likely(locker != NULL))
      PSI_server->start_file_open_wait(locker, src_file, src_line);
  }
#endif
  result= my_stat(path, stat_area, flags);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_file_wait(locker, (size_t) 0);
#endif
  return result;
}

static inline int
inline_mysql_file_chsize(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
  File file, my_off_t newlength, int filler, myf flags)
{
  int result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_file_locker *locker= NULL;
  PSI_file_locker_state state;
  if (likely(PSI_server != NULL))
  {
    locker= PSI_server->get_thread_file_descriptor_locker(&state, file,
                                                          PSI_FILE_CHSIZE);
    if (likely(locker != NULL))
      PSI_server->start_file_wait(locker, (size_t) newlength, src_file,
                                  src_line);
  }
#endif
  result= my_chsize(file, newlength, filler, flags);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_file_wait(locker, (size_t) newlength);
#endif
  return result;
}

static inline MYSQL_FILE*
inline_mysql_file_fopen(
#ifdef HAVE_PSI_INTERFACE
  PSI_file_key key, const char *src_file, uint src_line,
#endif
  const char *filename, int flags, myf myFlags)
{
  MYSQL_FILE *that;
  that= (MYSQL_FILE*) my_malloc(sizeof(MYSQL_FILE), MYF(MY_WME));
  if (likely(that != NULL))
  {
    that->m_psi= NULL;
    {
#ifdef HAVE_PSI_INTERFACE
      struct PSI_file_locker *locker= NULL;
      PSI_file_locker_state state;
      if (likely(PSI_server != NULL))
      {
        locker= PSI_server->get_thread_file_name_locker
          (&state, key, PSI_FILE_STREAM_OPEN, filename, that);
        if (likely(locker != NULL))
          that->m_psi= PSI_server->start_file_open_wait(locker, src_file,
                                                        src_line);
      }
#endif
      that->m_file= my_fopen(filename, flags, myFlags);
#ifdef HAVE_PSI_INTERFACE
      if (likely(locker != NULL))
        PSI_server->end_file_open_wait(locker);
#endif
      if (unlikely(that->m_file == NULL))
      {
        my_free(that);
        return NULL;
      }
    }
  }
  return that;
}

static inline int
inline_mysql_file_fclose(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
  MYSQL_FILE *file, myf flags)
{
  int result= 0;
  if (likely(file != NULL))
  {
#ifdef HAVE_PSI_INTERFACE
    struct PSI_file_locker *locker= NULL;
    PSI_file_locker_state state;
    DBUG_ASSERT(file != NULL);
    if (likely(PSI_server && file->m_psi))
    {
      locker= PSI_server->get_thread_file_stream_locker(&state, file->m_psi,
                                                        PSI_FILE_STREAM_CLOSE);
      if (likely(locker != NULL))
        PSI_server->start_file_wait(locker, (size_t) 0, src_file, src_line);
    }
#endif
    result= my_fclose(file->m_file, flags);
#ifdef HAVE_PSI_INTERFACE
    if (likely(locker != NULL))
      PSI_server->end_file_wait(locker, (size_t) 0);
#endif
    my_free(file);
  }
  return result;
}

static inline size_t
inline_mysql_file_fread(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
  MYSQL_FILE *file, uchar *buffer, size_t count, myf flags)
{
  size_t result= 0;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_file_locker *locker= NULL;
  PSI_file_locker_state state;
  if (likely(PSI_server && file->m_psi))
  {
    locker= PSI_server->get_thread_file_stream_locker(&state, file->m_psi,
                                                      PSI_FILE_READ);
    if (likely(locker != NULL))
      PSI_server->start_file_wait(locker, count, src_file, src_line);
  }
#endif
  result= my_fread(file->m_file, buffer, count, flags);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
  {
    size_t bytes_read;
    if (flags & (MY_NABP | MY_FNABP))
      bytes_read= (result == 0) ? count : 0;
    else
      bytes_read= (result != MY_FILE_ERROR) ? result : 0;
    PSI_server->end_file_wait(locker, bytes_read);
  }
#endif
  return result;
}

static inline size_t
inline_mysql_file_fwrite(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
  MYSQL_FILE *file, const uchar *buffer, size_t count, myf flags)
{
  size_t result= 0;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_file_locker *locker= NULL;
  PSI_file_locker_state state;
  if (likely(PSI_server && file->m_psi))
  {
    locker= PSI_server->get_thread_file_stream_locker(&state, file->m_psi,
                                                      PSI_FILE_WRITE);
    if (likely(locker != NULL))
      PSI_server->start_file_wait(locker, count, src_file, src_line);
  }
#endif
  result= my_fwrite(file->m_file, buffer, count, flags);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
  {
    size_t bytes_written;
    if (flags & (MY_NABP | MY_FNABP))
      bytes_written= (result == 0) ? count : 0;
    else
      bytes_written= (result != MY_FILE_ERROR) ? result : 0;
    PSI_server->end_file_wait(locker, bytes_written);
  }
#endif
  return result;
}

static inline my_off_t
inline_mysql_file_fseek(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
  MYSQL_FILE *file, my_off_t pos, int whence, myf flags)
{
  my_off_t result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_file_locker *locker= NULL;
  PSI_file_locker_state state;
  if (likely(PSI_server && file->m_psi))
  {
    locker= PSI_server->get_thread_file_stream_locker(&state, file->m_psi,
                                                      PSI_FILE_SEEK);
    if (likely(locker != NULL))
      PSI_server->start_file_wait(locker, (size_t) 0, src_file, src_line);
  }
#endif
  result= my_fseek(file->m_file, pos, whence, flags);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_file_wait(locker, (size_t) 0);
#endif
  return result;
}

static inline my_off_t
inline_mysql_file_ftell(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
  MYSQL_FILE *file, myf flags)
{
  my_off_t result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_file_locker *locker= NULL;
  PSI_file_locker_state state;
  if (likely(PSI_server && file->m_psi))
  {
    locker= PSI_server->get_thread_file_stream_locker(&state, file->m_psi,
                                                      PSI_FILE_TELL);
    if (likely(locker != NULL))
      PSI_server->start_file_wait(locker, (size_t) 0, src_file, src_line);
  }
#endif
  result= my_ftell(file->m_file, flags);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_file_wait(locker, (size_t) 0);
#endif
  return result;
}

static inline File
inline_mysql_file_create(
#ifdef HAVE_PSI_INTERFACE
  PSI_file_key key, const char *src_file, uint src_line,
#endif
  const char *filename, int create_flags, int access_flags, myf myFlags)
{
  File file;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_file_locker *locker= NULL;
  PSI_file_locker_state state;
  if (likely(PSI_server != NULL))
  {
    locker= PSI_server->get_thread_file_name_locker(&state, key, PSI_FILE_CREATE,
                                                    filename, &locker);
    if (likely(locker != NULL))
      PSI_server->start_file_open_wait(locker, src_file, src_line);
  }
#endif
  file= my_create(filename, create_flags, access_flags, myFlags);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_file_open_wait_and_bind_to_descriptor(locker, file);
#endif
  return file;
}

static inline File
inline_mysql_file_create_temp(
#ifdef HAVE_PSI_INTERFACE
  PSI_file_key key,
#endif
  char *to, const char *dir, const char *pfx, int mode, myf myFlags)
{
  File file;
  /*
    TODO: This event is instrumented, but not timed.
    The problem is that the file name is now known
    before the create_temp_file call.
  */
  file= create_temp_file(to, dir, pfx, mode, myFlags);
#ifdef HAVE_PSI_INTERFACE
  if (likely(PSI_server != NULL))
    PSI_server->create_file(key, to, file);
#endif
  return file;
}

static inline File
inline_mysql_file_open(
#ifdef HAVE_PSI_INTERFACE
  PSI_file_key key, const char *src_file, uint src_line,
#endif
  const char *filename, int flags, myf myFlags)
{
  File file;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_file_locker *locker= NULL;
  PSI_file_locker_state state;
  if (likely(PSI_server != NULL))
  {
    locker= PSI_server->get_thread_file_name_locker(&state, key, PSI_FILE_OPEN,
                                                    filename, &locker);
    if (likely(locker != NULL))
      PSI_server->start_file_open_wait(locker, src_file, src_line);
  }
#endif
  file= my_open(filename, flags, myFlags);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_file_open_wait_and_bind_to_descriptor(locker, file);
#endif
  return file;
}

static inline int
inline_mysql_file_close(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
  File file, myf flags)
{
  int result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_file_locker *locker= NULL;
  PSI_file_locker_state state;
  if (likely(PSI_server != NULL))
  {
    locker= PSI_server->get_thread_file_descriptor_locker(&state, file,
                                                          PSI_FILE_CLOSE);
    if (likely(locker != NULL))
      PSI_server->start_file_wait(locker, (size_t) 0, src_file, src_line);
  }
#endif
  result= my_close(file, flags);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_file_wait(locker, (size_t) 0);
#endif
  return result;
}

static inline size_t
inline_mysql_file_read(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
  File file, uchar *buffer, size_t count, myf flags)
{
  size_t result= 0;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_file_locker *locker= NULL;
  PSI_file_locker_state state;
  if (likely(PSI_server != NULL))
  {
    locker= PSI_server->get_thread_file_descriptor_locker(&state, file,
                                                          PSI_FILE_READ);
    if (likely(locker != NULL))
      PSI_server->start_file_wait(locker, count, src_file, src_line);
  }
#endif
  result= my_read(file, buffer, count, flags);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
  {
    size_t bytes_read;
    if (flags & (MY_NABP | MY_FNABP))
      bytes_read= (result == 0) ? count : 0;
    else
      bytes_read= (result != MY_FILE_ERROR) ? result : 0;
    PSI_server->end_file_wait(locker, bytes_read);
  }
#endif
  return result;
}

static inline size_t
inline_mysql_file_write(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
  File file, const uchar *buffer, size_t count, myf flags)
{
  size_t result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_file_locker *locker= NULL;
  PSI_file_locker_state state;
  if (likely(PSI_server != NULL))
  {
    locker= PSI_server->get_thread_file_descriptor_locker(&state, file,
                                                          PSI_FILE_WRITE);
    if (likely(locker != NULL))
      PSI_server->start_file_wait(locker, count, src_file, src_line);
  }
#endif
  result= my_write(file, buffer, count, flags);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
  {
    size_t bytes_written;
    if (flags & (MY_NABP | MY_FNABP))
      bytes_written= (result == 0) ? count : 0;
    else
      bytes_written= (result != MY_FILE_ERROR) ? result : 0;
    PSI_server->end_file_wait(locker, bytes_written);
  }
#endif
  return result;
}

static inline size_t
inline_mysql_file_pread(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
  File file, uchar *buffer, size_t count, my_off_t offset, myf flags)
{
  size_t result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_file_locker *locker= NULL;
  PSI_file_locker_state state;
  if (likely(PSI_server != NULL))
  {
    locker= PSI_server->get_thread_file_descriptor_locker(&state, file, PSI_FILE_READ);
    if (likely(locker != NULL))
      PSI_server->start_file_wait(locker, count, src_file, src_line);
  }
#endif
  result= my_pread(file, buffer, count, offset, flags);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
  {
    size_t bytes_read;
    if (flags & (MY_NABP | MY_FNABP))
      bytes_read= (result == 0) ? count : 0;
    else
      bytes_read= (result != MY_FILE_ERROR) ? result : 0;
    PSI_server->end_file_wait(locker, bytes_read);
  }
#endif
  return result;
}

static inline size_t
inline_mysql_file_pwrite(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
  File file, const uchar *buffer, size_t count, my_off_t offset, myf flags)
{
  size_t result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_file_locker *locker= NULL;
  PSI_file_locker_state state;
  if (likely(PSI_server != NULL))
  {
    locker= PSI_server->get_thread_file_descriptor_locker(&state, file,
                                                          PSI_FILE_WRITE);
    if (likely(locker != NULL))
      PSI_server->start_file_wait(locker, count, src_file, src_line);
  }
#endif
  result= my_pwrite(file, buffer, count, offset, flags);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
  {
    size_t bytes_written;
    if (flags & (MY_NABP | MY_FNABP))
      bytes_written= (result == 0) ? count : 0;
    else
      bytes_written= (result != MY_FILE_ERROR) ? result : 0;
    PSI_server->end_file_wait(locker, bytes_written);
  }
#endif
  return result;
}

static inline my_off_t
inline_mysql_file_seek(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
  File file, my_off_t pos, int whence, myf flags)
{
  my_off_t result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_file_locker *locker= NULL;
  PSI_file_locker_state state;
  if (likely(PSI_server != NULL))
  {
    locker= PSI_server->get_thread_file_descriptor_locker(&state, file, PSI_FILE_SEEK);
    if (likely(locker != NULL))
      PSI_server->start_file_wait(locker, (size_t) 0, src_file, src_line);
  }
#endif
  result= my_seek(file, pos, whence, flags);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_file_wait(locker, (size_t) 0);
#endif
  return result;
}

static inline my_off_t
inline_mysql_file_tell(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
  File file, myf flags)
{
  my_off_t result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_file_locker *locker= NULL;
  PSI_file_locker_state state;
  if (likely(PSI_server != NULL))
  {
    locker= PSI_server->get_thread_file_descriptor_locker(&state, file, PSI_FILE_TELL);
    if (likely(locker != NULL))
      PSI_server->start_file_wait(locker, (size_t) 0, src_file, src_line);
  }
#endif
  result= my_tell(file, flags);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_file_wait(locker, (size_t) 0);
#endif
  return result;
}

static inline int
inline_mysql_file_delete(
#ifdef HAVE_PSI_INTERFACE
  PSI_file_key key, const char *src_file, uint src_line,
#endif
  const char *name, myf flags)
{
  int result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_file_locker *locker= NULL;
  PSI_file_locker_state state;
  if (likely(PSI_server != NULL))
  {
    locker= PSI_server->get_thread_file_name_locker(&state, key, PSI_FILE_DELETE,
                                                    name, &locker);
    if (likely(locker != NULL))
      PSI_server->start_file_wait(locker, (size_t) 0, src_file, src_line);
  }
#endif
  result= my_delete(name, flags);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_file_wait(locker, (size_t) 0);
#endif
  return result;
}

static inline int
inline_mysql_file_rename(
#ifdef HAVE_PSI_INTERFACE
  PSI_file_key key, const char *src_file, uint src_line,
#endif
  const char *from, const char *to, myf flags)
{
  int result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_file_locker *locker= NULL;
  PSI_file_locker_state state;
  if (likely(PSI_server != NULL))
  {
    locker= PSI_server->get_thread_file_name_locker(&state, key, PSI_FILE_RENAME,
                                                    to, &locker);
    if (likely(locker != NULL))
      PSI_server->start_file_wait(locker, (size_t) 0, src_file, src_line);
  }
#endif
  result= my_rename(from, to, flags);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_file_wait(locker, (size_t) 0);
#endif
  return result;
}

static inline File
inline_mysql_file_create_with_symlink(
#ifdef HAVE_PSI_INTERFACE
  PSI_file_key key, const char *src_file, uint src_line,
#endif
  const char *linkname, const char *filename, int create_flags,
  int access_flags, myf flags)
{
  File file;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_file_locker *locker= NULL;
  PSI_file_locker_state state;
  if (likely(PSI_server != NULL))
  {
    locker= PSI_server->get_thread_file_name_locker(&state, key, PSI_FILE_CREATE,
                                                    filename, &locker);
    if (likely(locker != NULL))
      PSI_server->start_file_open_wait(locker, src_file, src_line);
  }
#endif
  file= my_create_with_symlink(linkname, filename, create_flags, access_flags,
                               flags);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_file_open_wait_and_bind_to_descriptor(locker, file);
#endif
  return file;
}

static inline int
inline_mysql_file_delete_with_symlink(
#ifdef HAVE_PSI_INTERFACE
  PSI_file_key key, const char *src_file, uint src_line,
#endif
  const char *name, myf flags)
{
  int result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_file_locker *locker= NULL;
  PSI_file_locker_state state;
  if (likely(PSI_server != NULL))
  {
    locker= PSI_server->get_thread_file_name_locker(&state, key, PSI_FILE_DELETE,
                                                    name, &locker);
    if (likely(locker != NULL))
      PSI_server->start_file_wait(locker, (size_t) 0, src_file, src_line);
  }
#endif
  result= my_delete_with_symlink(name, flags);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_file_wait(locker, (size_t) 0);
#endif
  return result;
}

static inline int
inline_mysql_file_rename_with_symlink(
#ifdef HAVE_PSI_INTERFACE
  PSI_file_key key, const char *src_file, uint src_line,
#endif
  const char *from, const char *to, myf flags)
{
  int result;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_file_locker *locker= NULL;
  PSI_file_locker_state state;
  if (likely(PSI_server != NULL))
  {
    locker= PSI_server->get_thread_file_name_locker(&state, key, PSI_FILE_RENAME,
                                                    to, &locker);
    if (likely(locker != NULL))
      PSI_server->start_file_wait(locker, (size_t) 0, src_file, src_line);
  }
#endif
  result= my_rename_with_symlink(from, to, flags);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_file_wait(locker, (size_t) 0);
#endif
  return result;
}

static inline int
inline_mysql_file_sync(
#ifdef HAVE_PSI_INTERFACE
  const char *src_file, uint src_line,
#endif
  File fd, myf flags)
{
  int result= 0;
#ifdef HAVE_PSI_INTERFACE
  struct PSI_file_locker *locker= NULL;
  PSI_file_locker_state state;
  if (likely(PSI_server != NULL))
  {
    locker= PSI_server->get_thread_file_descriptor_locker(&state, fd, PSI_FILE_SYNC);
    if (likely(locker != NULL))
      PSI_server->start_file_wait(locker, (size_t) 0, src_file, src_line);
  }
#endif
  result= my_sync(fd, flags);
#ifdef HAVE_PSI_INTERFACE
  if (likely(locker != NULL))
    PSI_server->end_file_wait(locker, (size_t) 0);
#endif
  return result;
}

/** @} (end of group File_instrumentation) */

#endif

