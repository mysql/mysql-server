/* Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_PSI_FILE_H
#define MYSQL_PSI_FILE_H

/**
  @file include/mysql/psi/psi_file.h
  Performance schema instrumentation interface.

  @defgroup psi_abi_file File Instrumentation (ABI)
  @ingroup psi_abi
  @{
*/

#include "my_inttypes.h"
#include "my_io.h"
#include "my_macros.h"
#include "my_psi_config.h"  // IWYU pragma: keep
#include "my_sharedlib.h"
#include "psi_base.h"

C_MODE_START

#ifdef HAVE_PSI_INTERFACE

/**
  @def PSI_FILE_VERSION_1
  Performance Schema File Interface number for version 1.
  This version is supported.
*/
#define PSI_FILE_VERSION_1 1

/**
  @def PSI_FILE_VERSION_2
  Performance Schema File Interface number for version 2.
  This version is not implemented, it's a placeholder.
*/
#define PSI_FILE_VERSION_2 2

/**
  @def PSI_CURRENT_FILE_VERSION
  Performance Schema File Interface number for the most recent version.
  The most current version is @c PSI_FILE_VERSION_1
*/
#define PSI_CURRENT_FILE_VERSION 1

#ifndef USE_PSI_FILE_2
#ifndef USE_PSI_FILE_1
#define USE_PSI_FILE_1
#endif /* USE_PSI_FILE_1 */
#endif /* USE_PSI_FILE_2 */

#ifdef USE_PSI_FILE_1
#define HAVE_PSI_FILE_1
#endif /* USE_PSI_FILE_1 */

#ifdef USE_PSI_FILE_2
#define HAVE_PSI_FILE_2
#endif /* USE_PSI_FILE_2 */

/**
  Interface for an instrumented file handle.
  This is an opaque structure.
*/
struct PSI_file;
typedef struct PSI_file PSI_file;

/** Entry point for the performance schema interface. */
struct PSI_file_bootstrap
{
  /**
    ABI interface finder.
    Calling this method with an interface version number returns either
    an instance of the ABI for this version, or NULL.
    @sa PSI_FILE_VERSION_1
    @sa PSI_FILE_VERSION_2
    @sa PSI_CURRENT_FILE_VERSION
  */
  void *(*get_interface)(int version);
};
typedef struct PSI_file_bootstrap PSI_file_bootstrap;

#ifdef HAVE_PSI_FILE_1

/**
  Interface for an instrumented file operation.
  This is an opaque structure.
*/
struct PSI_file_locker;
typedef struct PSI_file_locker PSI_file_locker;

/** Operation performed on an instrumented file. */
enum PSI_file_operation
{
  /** File creation, as in @c create(). */
  PSI_FILE_CREATE = 0,
  /** Temporary file creation, as in @c create_temp_file(). */
  PSI_FILE_CREATE_TMP = 1,
  /** File open, as in @c open(). */
  PSI_FILE_OPEN = 2,
  /** File open, as in @c fopen(). */
  PSI_FILE_STREAM_OPEN = 3,
  /** File close, as in @c close(). */
  PSI_FILE_CLOSE = 4,
  /** File close, as in @c fclose(). */
  PSI_FILE_STREAM_CLOSE = 5,
  /**
    Generic file read, such as @c fgets(), @c fgetc(), @c fread(), @c read(),
    @c pread().
  */
  PSI_FILE_READ = 6,
  /**
    Generic file write, such as @c fputs(), @c fputc(), @c fprintf(),
    @c vfprintf(), @c fwrite(), @c write(), @c pwrite().
  */
  PSI_FILE_WRITE = 7,
  /** Generic file seek, such as @c fseek() or @c seek(). */
  PSI_FILE_SEEK = 8,
  /** Generic file tell, such as @c ftell() or @c tell(). */
  PSI_FILE_TELL = 9,
  /** File flush, as in @c fflush(). */
  PSI_FILE_FLUSH = 10,
  /** File stat, as in @c stat(). */
  PSI_FILE_STAT = 11,
  /** File stat, as in @c fstat(). */
  PSI_FILE_FSTAT = 12,
  /** File chsize, as in @c my_chsize(). */
  PSI_FILE_CHSIZE = 13,
  /** File delete, such as @c my_delete() or @c my_delete_with_symlink(). */
  PSI_FILE_DELETE = 14,
  /** File rename, such as @c my_rename() or @c my_rename_with_symlink(). */
  PSI_FILE_RENAME = 15,
  /** File sync, as in @c fsync() or @c my_sync(). */
  PSI_FILE_SYNC = 16
};
typedef enum PSI_file_operation PSI_file_operation;

/**
  File instrument information.
  @since PSI_FILE_VERSION_1
  This structure is used to register an instrumented file.
*/
struct PSI_file_info_v1
{
  /**
    Pointer to the key assigned to the registered file.
  */
  PSI_file_key *m_key;
  /**
    The name of the file instrument to register.
  */
  const char *m_name;
  /**
    The flags of the file instrument to register.
    @sa PSI_FLAG_GLOBAL
  */
  int m_flags;
};
typedef struct PSI_file_info_v1 PSI_file_info_v1;

/**
  State data storage for @c get_thread_file_name_locker_v1_t.
  This structure provide temporary storage to a file locker.
  The content of this structure is considered opaque,
  the fields are only hints of what an implementation
  of the psi interface can use.
  This memory is provided by the instrumented code for performance reasons.
  @sa get_thread_file_name_locker_v1_t
  @sa get_thread_file_stream_locker_v1_t
  @sa get_thread_file_descriptor_locker_v1_t
*/
struct PSI_file_locker_state_v1
{
  /** Internal state. */
  uint m_flags;
  /** Current operation. */
  enum PSI_file_operation m_operation;
  /** Current file. */
  struct PSI_file *m_file;
  /** Current file name. */
  const char *m_name;
  /** Current file class. */
  void *m_class;
  /** Current thread. */
  struct PSI_thread *m_thread;
  /** Operation number of bytes. */
  size_t m_number_of_bytes;
  /** Timer start. */
  ulonglong m_timer_start;
  /** Timer function. */
  ulonglong (*m_timer)(void);
  /** Internal data. */
  void *m_wait;
};
typedef struct PSI_file_locker_state_v1 PSI_file_locker_state_v1;

/**
  File registration API.
  @param category a category name (typically a plugin name)
  @param info an array of file info to register
  @param count the size of the info array
*/
typedef void (*register_file_v1_t)(const char *category,
                                   struct PSI_file_info_v1 *info,
                                   int count);

/**
  Create a file instrumentation for a created file.
  This method does not create the file itself, but is used to notify the
  instrumentation interface that a file was just created.
  @param key the file instrumentation key for this file
  @param name the file name
  @param file the file handle
*/
typedef void (*create_file_v1_t)(PSI_file_key key, const char *name, File file);

/**
  Get a file instrumentation locker, for opening or creating a file.
  @param state data storage for the locker
  @param key the file instrumentation key
  @param op the operation to perform
  @param name the file name
  @param identity a pointer representative of this file.
  @return a file locker, or NULL
*/
typedef struct PSI_file_locker *(*get_thread_file_name_locker_v1_t)(
  struct PSI_file_locker_state_v1 *state,
  PSI_file_key key,
  enum PSI_file_operation op,
  const char *name,
  const void *identity);

/**
  Get a file stream instrumentation locker.
  @param state data storage for the locker
  @param file the file stream to access
  @param op the operation to perform
  @return a file locker, or NULL
*/
typedef struct PSI_file_locker *(*get_thread_file_stream_locker_v1_t)(
  struct PSI_file_locker_state_v1 *state,
  struct PSI_file *file,
  enum PSI_file_operation op);

/**
  Get a file instrumentation locker.
  @param state data storage for the locker
  @param file the file descriptor to access
  @param op the operation to perform
  @return a file locker, or NULL
*/
typedef struct PSI_file_locker *(*get_thread_file_descriptor_locker_v1_t)(
  struct PSI_file_locker_state_v1 *state,
  File file,
  enum PSI_file_operation op);

/**
  Start a file instrumentation open operation.
  @param locker the file locker
  @param src_file the source file name
  @param src_line the source line number
*/
typedef void (*start_file_open_wait_v1_t)(struct PSI_file_locker *locker,
                                          const char *src_file,
                                          uint src_line);

/**
  End a file instrumentation open operation, for file streams.
  @param locker the file locker.
  @param result the opened file (NULL indicates failure, non NULL success).
  @return an instrumented file handle
*/
typedef struct PSI_file *(*end_file_open_wait_v1_t)(
  struct PSI_file_locker *locker, void *result);

/**
  End a file instrumentation open operation, for non stream files.
  @param locker the file locker.
  @param file the file number assigned by open() or create() for this file.
*/
typedef void (*end_file_open_wait_and_bind_to_descriptor_v1_t)(
  struct PSI_file_locker *locker, File file);

/**
  End a file instrumentation open operation, for non stream temporary files.
  @param locker the file locker.
  @param file the file number assigned by open() or create() for this file.
  @param filename the file name generated during temporary file creation.
*/
typedef void (*end_temp_file_open_wait_and_bind_to_descriptor_v1_t)(
  struct PSI_file_locker *locker, File file, const char *filename);

/**
  Record a file instrumentation start event.
  @param locker a file locker for the running thread
  @param count the number of bytes requested, or 0 if not applicable
  @param src_file the source file name
  @param src_line the source line number
*/
typedef void (*start_file_wait_v1_t)(struct PSI_file_locker *locker,
                                     size_t count,
                                     const char *src_file,
                                     uint src_line);

/**
  Record a file instrumentation end event.
  Note that for file close operations, the instrumented file handle
  associated with the file (which was provided to obtain a locker)
  is invalid after this call.
  @param locker a file locker for the running thread
  @param count the number of bytes actually used in the operation,
  or 0 if not applicable, or -1 if the operation failed
  @sa get_thread_file_name_locker
  @sa get_thread_file_stream_locker
  @sa get_thread_file_descriptor_locker
*/
typedef void (*end_file_wait_v1_t)(struct PSI_file_locker *locker,
                                   size_t count);

/**
  Start a file instrumentation close operation.
  @param locker the file locker
  @param src_file the source file name
  @param src_line the source line number
*/
typedef void (*start_file_close_wait_v1_t)(struct PSI_file_locker *locker,
                                           const char *src_file,
                                           uint src_line);

/**
  End a file instrumentation close operation.
  @param locker the file locker.
  @param rc the close operation return code (0 for success).
*/
typedef void (*end_file_close_wait_v1_t)(struct PSI_file_locker *locker,
                                         int rc);

/**
  Performance Schema file Interface, version 1.
  @since PSI_FILE_VERSION_1
*/
struct PSI_file_service_v1
{
  /** @sa register_file_v1_t. */
  register_file_v1_t register_file;
  /** @sa create_file_v1_t. */
  create_file_v1_t create_file;
  /** @sa get_thread_file_name_locker_v1_t. */
  get_thread_file_name_locker_v1_t get_thread_file_name_locker;
  /** @sa get_thread_file_stream_locker_v1_t. */
  get_thread_file_stream_locker_v1_t get_thread_file_stream_locker;
  /** @sa get_thread_file_descriptor_locker_v1_t. */
  get_thread_file_descriptor_locker_v1_t get_thread_file_descriptor_locker;
  /** @sa start_file_open_wait_v1_t. */
  start_file_open_wait_v1_t start_file_open_wait;
  /** @sa end_file_open_wait_v1_t. */
  end_file_open_wait_v1_t end_file_open_wait;
  /** @sa end_file_open_wait_and_bind_to_descriptor_v1_t. */
  end_file_open_wait_and_bind_to_descriptor_v1_t
    end_file_open_wait_and_bind_to_descriptor;
  /** @sa end_temp_file_open_wait_and_bind_to_descriptor_v1_t. */
  end_temp_file_open_wait_and_bind_to_descriptor_v1_t
    end_temp_file_open_wait_and_bind_to_descriptor;
  /** @sa start_file_wait_v1_t. */
  start_file_wait_v1_t start_file_wait;
  /** @sa end_file_wait_v1_t. */
  end_file_wait_v1_t end_file_wait;
  /** @sa start_file_close_wait_v1_t. */
  start_file_close_wait_v1_t start_file_close_wait;
  /** @sa end_file_close_wait_v1_t. */
  end_file_close_wait_v1_t end_file_close_wait;
};

#endif /* HAVE_PSI_FILE_1 */

/* Export the required version */
#ifdef USE_PSI_FILE_1
typedef struct PSI_file_service_v1 PSI_file_service_t;
typedef struct PSI_file_info_v1 PSI_file_info;
typedef struct PSI_file_locker_state_v1 PSI_file_locker_state;
#else
typedef struct PSI_placeholder PSI_file_service_t;
typedef struct PSI_placeholder PSI_file_info;
typedef struct PSI_placeholder PSI_file_locker_state;
#endif

extern MYSQL_PLUGIN_IMPORT PSI_file_service_t *psi_file_service;

/** @} (end of group psi_abi_file) */

#endif /* HAVE_PSI_INTERFACE */

C_MODE_END

#endif /* MYSQL_PSI_FILE_H */
