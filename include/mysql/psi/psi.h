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

#ifndef MYSQL_PERFORMANCE_SCHEMA_INTERFACE_H
#define MYSQL_PERFORMANCE_SCHEMA_INTERFACE_H

#ifndef _global_h
/*
  Make sure a .c or .cc file contains an include to my_global.h first.
  When this include is missing, all the #ifdef HAVE_XXX have no effect,
  and the resulting binary won't build, or won't link,
  or will crash at runtime
  since various structures will have different binary definitions.
*/
#error "You must include my_global.h in the code for the build to be correct."
#endif

C_MODE_START

/**
  @file mysql/psi/psi.h
  Performance schema instrumentation interface.

  @defgroup Instrumentation_interface Instrumentation Interface
  @ingroup Performance_schema
  @{
*/

/**
  Interface for an instrumented mutex.
  This is an opaque structure.
*/
struct PSI_mutex;

/**
  Interface for an instrumented rwlock.
  This is an opaque structure.
*/
struct PSI_rwlock;

/**
  Interface for an instrumented condition.
  This is an opaque structure.
*/
struct PSI_cond;

/**
  Interface for an instrumented table share.
  This is an opaque structure.
*/
struct PSI_table_share;

/**
  Interface for an instrumented table handle.
  This is an opaque structure.
*/
struct PSI_table;

/**
  Interface for an instrumented thread.
  This is an opaque structure.
*/
struct PSI_thread;

/**
  Interface for an instrumented file handle.
  This is an opaque structure.
*/
struct PSI_file;

/** Entry point for the performance schema interface. */
struct PSI_bootstrap
{
  /**
    ABI interface finder.
    Calling this method with an interface version number returns either
    an instance of the ABI for this version, or NULL.
    @param version the interface version number to find
    @return a versioned interface (PSI_v1, PSI_v2 or PSI)
    @sa PSI_VERSION_1
    @sa PSI_v1
    @sa PSI_VERSION_2
    @sa PSI_v2
    @sa PSI_CURRENT_VERSION
    @sa PSI
  */
  void* (*get_interface)(int version);
};

#ifdef HAVE_PSI_INTERFACE

/**
  @def PSI_VERSION_1
  Performance Schema Interface number for version 1.
  This version is supported.
*/
#define PSI_VERSION_1 1

/**
  @def PSI_VERSION_2
  Performance Schema Interface number for version 2.
  This version is not implemented, it's a placeholder.
*/
#define PSI_VERSION_2 2

/**
  @def PSI_CURRENT_VERSION
  Performance Schema Interface number for the most recent version.
  The most current version is @c PSI_VERSION_1
*/
#define PSI_CURRENT_VERSION 1

#ifndef USE_PSI_2
#ifndef USE_PSI_1
#define USE_PSI_1
#endif
#endif

/**
  Interface for an instrumented mutex operation.
  This is an opaque structure.
*/
struct PSI_mutex_locker;

/**
  Interface for an instrumented rwlock operation.
  This is an opaque structure.
*/

struct PSI_rwlock_locker;
/**
  Interface for an instrumented condition operation.
  This is an opaque structure.
*/

struct PSI_cond_locker;

/**
  Interface for an instrumented file operation.
  This is an opaque structure.
*/
struct PSI_file_locker;

/** Operation performed on an instrumented mutex. */
enum PSI_mutex_operation
{
  /** Lock. */
  PSI_MUTEX_LOCK= 0,
  /** Lock attempt. */
  PSI_MUTEX_TRYLOCK= 1
};

/** Operation performed on an instrumented rwlock. */
enum PSI_rwlock_operation
{
  /** Read lock. */
  PSI_RWLOCK_READLOCK= 0,
  /** Write lock. */
  PSI_RWLOCK_WRITELOCK= 1,
  /** Read lock attempt. */
  PSI_RWLOCK_TRYREADLOCK= 2,
  /** Write lock attempt. */
  PSI_RWLOCK_TRYWRITELOCK= 3
};

/** Operation performed on an instrumented condition. */
enum PSI_cond_operation
{
  /** Wait. */
  PSI_COND_WAIT= 0,
  /** Wait, with timeout. */
  PSI_COND_TIMEDWAIT= 1
};

/** Operation performed on an instrumented file. */
enum PSI_file_operation
{
  /** File creation, as in @c create(). */
  PSI_FILE_CREATE= 0,
  /** Temporary file creation, as in @c create_temp_file(). */
  PSI_FILE_CREATE_TMP= 1,
  /** File open, as in @c open(). */
  PSI_FILE_OPEN= 2,
  /** File open, as in @c fopen(). */
  PSI_FILE_STREAM_OPEN= 3,
  /** File close, as in @c close(). */
  PSI_FILE_CLOSE= 4,
  /** File close, as in @c fclose(). */
  PSI_FILE_STREAM_CLOSE= 5,
  /**
    Generic file read, such as @c fgets(), @c fgetc(), @c fread(), @c read(),
    @c pread().
  */
  PSI_FILE_READ= 6,
  /**
    Generic file write, such as @c fputs(), @c fputc(), @c fprintf(),
    @c vfprintf(), @c fwrite(), @c write(), @c pwrite().
  */
  PSI_FILE_WRITE= 7,
  /** Generic file seek, such as @c fseek() or @c seek(). */
  PSI_FILE_SEEK= 8,
  /** Generic file tell, such as @c ftell() or @c tell(). */
  PSI_FILE_TELL= 9,
  /** File flush, as in @c fflush(). */
  PSI_FILE_FLUSH= 10,
  /** File stat, as in @c stat(). */
  PSI_FILE_STAT= 11,
  /** File stat, as in @c fstat(). */
  PSI_FILE_FSTAT= 12,
  /** File chsize, as in @c my_chsize(). */
  PSI_FILE_CHSIZE= 13,
  /** File delete, such as @c my_delete() or @c my_delete_with_symlink(). */
  PSI_FILE_DELETE= 14,
  /** File rename, such as @c my_rename() or @c my_rename_with_symlink(). */
  PSI_FILE_RENAME= 15,
  /** File sync, as in @c fsync() or @c my_sync(). */
  PSI_FILE_SYNC= 16
};

/**
  Interface for an instrumented table operation.
  This is an opaque structure.
*/
struct PSI_table_locker;

/**
  Instrumented mutex key.
  To instrument a mutex, a mutex key must be obtained using @c register_mutex.
  Using a zero key always disable the instrumentation.
*/
typedef unsigned int PSI_mutex_key;

/**
  Instrumented rwlock key.
  To instrument a rwlock, a rwlock key must be obtained
  using @c register_rwlock.
  Using a zero key always disable the instrumentation.
*/
typedef unsigned int PSI_rwlock_key;

/**
  Instrumented cond key.
  To instrument a condition, a condition key must be obtained
  using @c register_cond.
  Using a zero key always disable the instrumentation.
*/
typedef unsigned int PSI_cond_key;

/**
  Instrumented thread key.
  To instrument a thread, a thread key must be obtained
  using @c register_thread.
  Using a zero key always disable the instrumentation.
*/
typedef unsigned int PSI_thread_key;

/**
  Instrumented file key.
  To instrument a file, a file key must be obtained using @c register_file.
  Using a zero key always disable the instrumentation.
*/
typedef unsigned int PSI_file_key;

/**
  @def USE_PSI_1
  Define USE_PSI_1 to use the interface version 1.
*/

/**
  @def USE_PSI_2
  Define USE_PSI_2 to use the interface version 2.
*/

/**
  @def HAVE_PSI_1
  Define HAVE_PSI_1 if the interface version 1 needs to be compiled in.
*/

/**
  @def HAVE_PSI_2
  Define HAVE_PSI_2 if the interface version 2 needs to be compiled in.
*/

/**
  Global flag.
  This flag indicate that an instrumentation point is a global variable,
  or a singleton.
*/
#define PSI_FLAG_GLOBAL (1 << 0)

#ifdef USE_PSI_1
#define HAVE_PSI_1
#endif

#ifdef HAVE_PSI_1

/**
  @defgroup Group_PSI_v1 Application Binary Interface, version 1
  @ingroup Instrumentation_interface
  @{
*/

/**
  Mutex information.
  @since PSI_VERSION_1
  This structure is used to register an instrumented mutex.
*/
struct PSI_mutex_info_v1
{
  /**
    Pointer to the key assigned to the registered mutex.
  */
  PSI_mutex_key *m_key;
  /**
    The name of the mutex to register.
  */
  const char *m_name;
  /**
    The flags of the mutex to register.
    @sa PSI_FLAG_GLOBAL
  */
  int m_flags;
};

/**
  Rwlock information.
  @since PSI_VERSION_1
  This structure is used to register an instrumented rwlock.
*/
struct PSI_rwlock_info_v1
{
  /**
    Pointer to the key assigned to the registered rwlock.
  */
  PSI_rwlock_key *m_key;
  /**
    The name of the rwlock to register.
  */
  const char *m_name;
  /**
    The flags of the rwlock to register.
    @sa PSI_FLAG_GLOBAL
  */
  int m_flags;
};

/**
  Condition information.
  @since PSI_VERSION_1
  This structure is used to register an instrumented cond.
*/
struct PSI_cond_info_v1
{
  /**
    Pointer to the key assigned to the registered cond.
  */
  PSI_cond_key *m_key;
  /**
    The name of the cond to register.
  */
  const char *m_name;
  /**
    The flags of the cond to register.
    @sa PSI_FLAG_GLOBAL
  */
  int m_flags;
};

/**
  Thread instrument information.
  @since PSI_VERSION_1
  This structure is used to register an instrumented thread.
*/
struct PSI_thread_info_v1
{
  /**
    Pointer to the key assigned to the registered thread.
  */
  PSI_thread_key *m_key;
  /**
    The name of the thread instrument to register.
  */
  const char *m_name;
  /**
    The flags of the thread to register.
    @sa PSI_FLAG_GLOBAL
  */
  int m_flags;
};

/**
  File instrument information.
  @since PSI_VERSION_1
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

/**
  State data storage for @c get_thread_mutex_locker_v1_t.
  This structure provide temporary storage to a mutex locker.
  The content of this structure is considered opaque,
  the fields are only hints of what an implementation
  of the psi interface can use.
  This memory is provided by the instrumented code for performance reasons.
  @sa get_thread_mutex_locker_v1_t
*/
struct PSI_mutex_locker_state_v1
{
  /** Internal state. */
  uint m_flags;
  /** Current mutex. */
  struct PSI_mutex *m_mutex;
  /** Current thread. */
  struct PSI_thread *m_thread;
  /** Timer start. */
  ulonglong m_timer_start;
  /** Timer function. */
  ulonglong (*m_timer)(void);
  /** Current operation. */
  enum PSI_mutex_operation m_operation;
  /** Source file. */
  const char* m_src_file;
  /** Source line number. */
  int m_src_line;
  /** Internal data. */
  void *m_wait;
};

/**
  State data storage for @c get_thread_rwlock_locker_v1_t.
  This structure provide temporary storage to a rwlock locker.
  The content of this structure is considered opaque,
  the fields are only hints of what an implementation
  of the psi interface can use.
  This memory is provided by the instrumented code for performance reasons.
  @sa get_thread_rwlock_locker_v1_t
*/
struct PSI_rwlock_locker_state_v1
{
  /** Internal state. */
  uint m_flags;
  /** Current rwlock. */
  struct PSI_rwlock *m_rwlock;
  /** Current thread. */
  struct PSI_thread *m_thread;
  /** Timer start. */
  ulonglong m_timer_start;
  /** Timer function. */
  ulonglong (*m_timer)(void);
  /** Current operation. */
  enum PSI_rwlock_operation m_operation;
  /** Source file. */
  const char* m_src_file;
  /** Source line number. */
  int m_src_line;
  /** Internal data. */
  void *m_wait;
};

/**
  State data storage for @c get_thread_cond_locker_v1_t.
  This structure provide temporary storage to a condition locker.
  The content of this structure is considered opaque,
  the fields are only hints of what an implementation
  of the psi interface can use.
  This memory is provided by the instrumented code for performance reasons.
  @sa get_thread_cond_locker_v1_t
*/
struct PSI_cond_locker_state_v1
{
  /** Internal state. */
  uint m_flags;
  /** Current condition. */
  struct PSI_cond *m_cond;
  /** Current mutex. */
  struct PSI_mutex *m_mutex;
  /** Current thread. */
  struct PSI_thread *m_thread;
  /** Timer start. */
  ulonglong m_timer_start;
  /** Timer function. */
  ulonglong (*m_timer)(void);
  /** Current operation. */
  enum PSI_cond_operation m_operation;
  /** Source file. */
  const char* m_src_file;
  /** Source line number. */
  int m_src_line;
  /** Internal data. */
  void *m_wait;
};

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
  /** Current file. */
  struct PSI_file *m_file;
  /** Current thread. */
  struct PSI_thread *m_thread;
  /** Operation number of bytes. */
  size_t m_number_of_bytes;
  /** Timer start. */
  ulonglong m_timer_start;
  /** Timer function. */
  ulonglong (*m_timer)(void);
  /** Current operation. */
  enum PSI_file_operation m_operation;
  /** Source file. */
  const char* m_src_file;
  /** Source line number. */
  int m_src_line;
  /** Internal data. */
  void *m_wait;
};

/**
  State data storage for @c get_thread_table_locker_v1_t.
  This structure provide temporary storage to a table locker.
  The content of this structure is considered opaque,
  the fields are only hints of what an implementation
  of the psi interface can use.
  This memory is provided by the instrumented code for performance reasons.
  @sa get_thread_table_locker_v1_t
*/
struct PSI_table_locker_state_v1
{
  /** Internal state. */
  uint m_flags;
  /** Current table handle. */
  struct PSI_table *m_table;
  /** Current table share. */
  struct PSI_table_share *m_table_share;
  /** Instrumentation class. */
  void *m_class;
  /** Current thread. */
  struct PSI_thread *m_thread;
  /** Timer start. */
  ulonglong m_timer_start;
  /** Timer function. */
  ulonglong (*m_timer)(void);
  /* Current operation (waiting for WL#4895). */
  /* enum PSI_table_operation m_operation; */
  /** Current table io index. */
  uint m_index;
  /** Current table lock index. */
  uint m_lock_index;
  /** Source file. */
  const char* m_src_file;
  /** Source line number. */
  int m_src_line;
  /** Internal data. */
  void *m_wait;
};

/* Using typedef to make reuse between PSI_v1 and PSI_v2 easier later. */

/**
  Mutex registration API.
  @param category a category name (typically a plugin name)
  @param info an array of mutex info to register
  @param count the size of the info array
*/
typedef void (*register_mutex_v1_t)
  (const char *category, struct PSI_mutex_info_v1 *info, int count);

/**
  Rwlock registration API.
  @param category a category name (typically a plugin name)
  @param info an array of rwlock info to register
  @param count the size of the info array
*/
typedef void (*register_rwlock_v1_t)
  (const char *category, struct PSI_rwlock_info_v1 *info, int count);

/**
  Cond registration API.
  @param category a category name (typically a plugin name)
  @param info an array of cond info to register
  @param count the size of the info array
*/
typedef void (*register_cond_v1_t)
  (const char *category, struct PSI_cond_info_v1 *info, int count);

/**
  Thread registration API.
  @param category a category name (typically a plugin name)
  @param info an array of thread info to register
  @param count the size of the info array
*/
typedef void (*register_thread_v1_t)
  (const char *category, struct PSI_thread_info_v1 *info, int count);

/**
  File registration API.
  @param category a category name (typically a plugin name)
  @param info an array of file info to register
  @param count the size of the info array
*/
typedef void (*register_file_v1_t)
  (const char *category, struct PSI_file_info_v1 *info, int count);

/**
  Mutex instrumentation initialisation API.
  @param key the registered mutex key
  @param identity the address of the mutex itself
  @return an instrumented mutex
*/
typedef struct PSI_mutex* (*init_mutex_v1_t)
  (PSI_mutex_key key, const void *identity);

/**
  Mutex instrumentation destruction API.
  @param mutex the mutex to destroy
*/
typedef void (*destroy_mutex_v1_t)(struct PSI_mutex *mutex);

/**
  Rwlock instrumentation initialisation API.
  @param key the registered rwlock key
  @param identity the address of the rwlock itself
  @return an instrumented rwlock
*/
typedef struct PSI_rwlock* (*init_rwlock_v1_t)
  (PSI_rwlock_key key, const void *identity);

/**
  Rwlock instrumentation destruction API.
  @param rwlock the rwlock to destroy
*/
typedef void (*destroy_rwlock_v1_t)(struct PSI_rwlock *rwlock);

/**
  Cond instrumentation initialisation API.
  @param key the registered key
  @param identity the address of the rwlock itself
  @return an instrumented cond
*/
typedef struct PSI_cond* (*init_cond_v1_t)
  (PSI_cond_key key, const void *identity);

/**
  Cond instrumentation destruction API.
  @param cond the rcond to destroy
*/
typedef void (*destroy_cond_v1_t)(struct PSI_cond *cond);

/**
  Acquire a table info by name.
  @param schema_name name of the table schema
  @param schema_name_length length of schema_name
  @param table_name name of the table
  @param table_name_length length of table_name
  @param identity table identity pointer, typically the table share
  @return a table info, or NULL if the table is not instrumented
*/
typedef struct PSI_table_share* (*get_table_share_v1_t)
  (const char *schema_name, int schema_name_length, const char *table_name,
   int table_name_length, const void *identity);

/**
  Release a table share.
  @param info the table share to release
*/
typedef void (*release_table_share_v1_t)(struct PSI_table_share *share);

/**
  Open an instrumentation table handle.
  @param share the table to open
  @param identity table handle identity
  @return a table handle, or NULL
*/
typedef struct PSI_table* (*open_table_v1_t)
  (struct PSI_table_share *share, const void *identity);

/**
  Close an instrumentation table handle.
  Note that the table handle is invalid after this call.
  @param table the table handle to close
*/
typedef void (*close_table_v1_t)(struct PSI_table *table);

/**
  Create a file instrumentation for a created file.
  This method does not create the file itself, but is used to notify the
  instrumentation interface that a file was just created.
  @param key the file instrumentation key for this file
  @param name the file name
  @param file the file handle
*/
typedef void (*create_file_v1_t)(PSI_file_key key, const char *name,
                                 File file);

/**
  Spawn a thread.
  This method creates a new thread, with instrumentation.
  @param key the instrumentation key for this thread
  @param thread the resulting thread
  @param attr the thread attributes
  @param start_routine the thread start routine
  @param arg the thread start routine argument
*/
typedef int (*spawn_thread_v1_t)(PSI_thread_key key,
                                 pthread_t *thread,
                                 const pthread_attr_t *attr,
                                 void *(*start_routine)(void*), void *arg);

/**
  Create instrumentation for a thread.
  @param key the registered key
  @param identity an address typical of the thread
  @return an instrumented thread
*/
typedef struct PSI_thread* (*new_thread_v1_t)
  (PSI_thread_key key, const void *identity, ulong thread_id);

/**
  Assign an id to an instrumented thread.
  @param thread the instrumented thread
  @param id the id to assign
*/
typedef void (*set_thread_id_v1_t)(struct PSI_thread *thread,
                                   unsigned long id);

/**
  Get the instrumentation for the running thread.
  For this function to return a result,
  the thread instrumentation must have been attached to the
  running thread using @c set_thread()
  @return the instrumentation for the running thread
*/
typedef struct PSI_thread* (*get_thread_v1_t)(void);

/**
  Attach a thread instrumentation to the running thread.
  In case of thread pools, this method should be called when
  a worker thread picks a work item and runs it.
  Also, this method should be called if the instrumented code does not
  keep the pointer returned by @c new_thread() and relies on @c get_thread()
  instead.
  @param thread the thread instrumentation
*/
typedef void (*set_thread_v1_t)(struct PSI_thread *thread);

/** Delete the current thread instrumentation. */
typedef void (*delete_current_thread_v1_t)(void);

/** Delete a thread instrumentation. */
typedef void (*delete_thread_v1_t)(struct PSI_thread *thread);

/**
  Get a mutex instrumentation locker.
  @param state data storage for the locker
  @param mutex the instrumented mutex to lock
  @return a mutex locker, or NULL
*/
typedef struct PSI_mutex_locker* (*get_thread_mutex_locker_v1_t)
  (struct PSI_mutex_locker_state_v1 *state,
   struct PSI_mutex *mutex,
   enum PSI_mutex_operation op);

/**
  Get a rwlock instrumentation locker.
  @param state data storage for the locker
  @param rwlock the instrumented rwlock to lock
  @return a rwlock locker, or NULL
*/
typedef struct PSI_rwlock_locker* (*get_thread_rwlock_locker_v1_t)
  (struct PSI_rwlock_locker_state_v1 *state,
   struct PSI_rwlock *rwlock,
   enum PSI_rwlock_operation op);

/**
  Get a cond instrumentation locker.
  @param state data storage for the locker
  @param cond the instrumented condition to wait on
  @param mutex the instrumented mutex associated with the condition
  @return a condition locker, or NULL
*/
typedef struct PSI_cond_locker* (*get_thread_cond_locker_v1_t)
  (struct PSI_cond_locker_state_v1 *state,
   struct PSI_cond *cond, struct PSI_mutex *mutex,
   enum PSI_cond_operation op);

/**
  Get a table instrumentation locker.
  @param state data storage for the locker
  @param table the instrumented table to lock
  @return a table locker, or NULL
*/
typedef struct PSI_table_locker* (*get_thread_table_locker_v1_t)
  (struct PSI_table_locker_state_v1 *state,
   struct PSI_table *table);

/**
  Get a file instrumentation locker, for opening or creating a file.
  @param state data storage for the locker
  @param key the file instrumentation key
  @param op the operation to perform
  @param name the file name
  @param identity a pointer representative of this file.
  @return a file locker, or NULL
*/
typedef struct PSI_file_locker* (*get_thread_file_name_locker_v1_t)
  (struct PSI_file_locker_state_v1 *state,
   PSI_file_key key, enum PSI_file_operation op, const char *name,
   const void *identity);

/**
  Get a file stream instrumentation locker.
  @param state data storage for the locker
  @param file the file stream to access
  @param op the operation to perform
  @return a file locker, or NULL
*/
typedef struct PSI_file_locker* (*get_thread_file_stream_locker_v1_t)
  (struct PSI_file_locker_state_v1 *state,
   struct PSI_file *file, enum PSI_file_operation op);

/**
  Get a file instrumentation locker.
  @param state data storage for the locker
  @param file the file descriptor to access
  @param op the operation to perform
  @return a file locker, or NULL
*/
typedef struct PSI_file_locker* (*get_thread_file_descriptor_locker_v1_t)
  (struct PSI_file_locker_state_v1 *state,
   File file, enum PSI_file_operation op);

/**
  Record a mutex instrumentation unlock event.
  @param mutex the mutex instrumentation
*/
typedef void (*unlock_mutex_v1_t)
  (struct PSI_mutex *mutex);

/**
  Record a rwlock instrumentation unlock event.
  @param rwlock the rwlock instrumentation
*/
typedef void (*unlock_rwlock_v1_t)
  (struct PSI_rwlock *rwlock);

/**
  Record a condition instrumentation signal event.
  @param cond the cond instrumentation
*/
typedef void (*signal_cond_v1_t)
  (struct PSI_cond *cond);

/**
  Record a condition instrumentation broadcast event.
  @param cond the cond instrumentation
*/
typedef void (*broadcast_cond_v1_t)
  (struct PSI_cond *cond);

/**
  Record a mutex instrumentation wait start event.
  @param locker a thread locker for the running thread
*/
typedef void (*start_mutex_wait_v1_t)
  (struct PSI_mutex_locker *locker, const char *src_file, uint src_line);

/**
  Record a mutex instrumentation wait end event.
  @param locker a thread locker for the running thread
  @param rc the wait operation return code
*/
typedef void (*end_mutex_wait_v1_t)
  (struct PSI_mutex_locker *locker, int rc);

/**
  Record a rwlock instrumentation read wait start event.
  @param locker a thread locker for the running thread
  @param must must block: 1 for lock, 0 for trylock
*/
typedef void (*start_rwlock_rdwait_v1_t)
  (struct PSI_rwlock_locker *locker, const char *src_file, uint src_line);

/**
  Record a rwlock instrumentation read wait end event.
  @param locker a thread locker for the running thread
  @param rc the wait operation return code
*/
typedef void (*end_rwlock_rdwait_v1_t)
  (struct PSI_rwlock_locker *locker, int rc);

/**
  Record a rwlock instrumentation write wait start event.
  @param locker a thread locker for the running thread
  @param must must block: 1 for lock, 0 for trylock
*/
typedef void (*start_rwlock_wrwait_v1_t)
  (struct PSI_rwlock_locker *locker, const char *src_file, uint src_line);

/**
  Record a rwlock instrumentation write wait end event.
  @param locker a thread locker for the running thread
  @param rc the wait operation return code
*/
typedef void (*end_rwlock_wrwait_v1_t)
  (struct PSI_rwlock_locker *locker, int rc);

/**
  Record a condition instrumentation wait start event.
  @param locker a thread locker for the running thread
  @param must must block: 1 for wait, 0 for timedwait
*/
typedef void (*start_cond_wait_v1_t)
  (struct PSI_cond_locker *locker, const char *src_file, uint src_line);

/**
  Record a condition instrumentation wait end event.
  @param locker a thread locker for the running thread
  @param rc the wait operation return code
*/
typedef void (*end_cond_wait_v1_t)
  (struct PSI_cond_locker *locker, int rc);

/**
  Record a table instrumentation wait start event.
  @param locker a table locker for the running thread
  @param file the source file name
  @param line the source line number
*/
typedef void (*start_table_wait_v1_t)
  (struct PSI_table_locker *locker, const char *src_file, uint src_line);

/**
  Record a table instrumentation wait end event.
  @param locker a table locker for the running thread
*/
typedef void (*end_table_wait_v1_t)(struct PSI_table_locker *locker);

/**
  Start a file instrumentation open operation.
  @param locker the file locker
  @param op the operation to perform
  @param src_file the source file name
  @param src_line the source line number
  @return an instrumented file handle
*/
typedef struct PSI_file* (*start_file_open_wait_v1_t)
  (struct PSI_file_locker *locker, const char *src_file, uint src_line);

/**
  End a file instrumentation open operation, for file streams.
  @param locker the file locker.
*/
typedef void (*end_file_open_wait_v1_t)(struct PSI_file_locker *locker);

/**
  End a file instrumentation open operation, for non stream files.
  @param locker the file locker.
  @param file the file number assigned by open() or create() for this file.
*/
typedef void (*end_file_open_wait_and_bind_to_descriptor_v1_t)
  (struct PSI_file_locker *locker, File file);

/**
  Record a file instrumentation start event.
  @param locker a file locker for the running thread
  @param op file operation to be performed
  @param count the number of bytes requested, or 0 if not applicable
  @param src_file the source file name
  @param src_line the source line number
*/
typedef void (*start_file_wait_v1_t)
  (struct PSI_file_locker *locker, size_t count,
   const char *src_file, uint src_line);

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
typedef void (*end_file_wait_v1_t)
  (struct PSI_file_locker *locker, size_t count);

/**
  Performance Schema Interface, version 1.
  @since PSI_VERSION_1
*/
struct PSI_v1
{
  /** @sa register_mutex_v1_t. */
  register_mutex_v1_t register_mutex;
  /** @sa register_rwlock_v1_t. */
  register_rwlock_v1_t register_rwlock;
  /** @sa register_cond_v1_t. */
  register_cond_v1_t register_cond;
  /** @sa register_thread_v1_t. */
  register_thread_v1_t register_thread;
  /** @sa register_file_v1_t. */
  register_file_v1_t register_file;
  /** @sa init_mutex_v1_t. */
  init_mutex_v1_t init_mutex;
  /** @sa destroy_mutex_v1_t. */
  destroy_mutex_v1_t destroy_mutex;
  /** @sa init_rwlock_v1_t. */
  init_rwlock_v1_t init_rwlock;
  /** @sa destroy_rwlock_v1_t. */
  destroy_rwlock_v1_t destroy_rwlock;
  /** @sa init_cond_v1_t. */
  init_cond_v1_t init_cond;
  /** @sa destroy_cond_v1_t. */
  destroy_cond_v1_t destroy_cond;
  /** @sa get_table_share_v1_t. */
  get_table_share_v1_t get_table_share;
  /** @sa release_table_share_v1_t. */
  release_table_share_v1_t release_table_share;
  /** @sa open_table_v1_t. */
  open_table_v1_t open_table;
  /** @sa close_table_v1_t. */
  close_table_v1_t close_table;
  /** @sa create_file_v1_t. */
  create_file_v1_t create_file;
  /** @sa spawn_thread_v1_t. */
  spawn_thread_v1_t spawn_thread;
  /** @sa new_thread_v1_t. */
  new_thread_v1_t new_thread;
  /** @sa set_thread_id_v1_t. */
  set_thread_id_v1_t set_thread_id;
  /** @sa get_thread_v1_t. */
  get_thread_v1_t get_thread;
  /** @sa set_thread_v1_t. */
  set_thread_v1_t set_thread;
  /** @sa delete_current_thread_v1_t. */
  delete_current_thread_v1_t delete_current_thread;
  /** @sa delete_thread_v1_t. */
  delete_thread_v1_t delete_thread;
  /** @sa get_thread_mutex_locker_v1_t. */
  get_thread_mutex_locker_v1_t get_thread_mutex_locker;
  /** @sa get_thread_rwlock_locker_v1_t. */
  get_thread_rwlock_locker_v1_t get_thread_rwlock_locker;
  /** @sa get_thread_cond_locker_v1_t. */
  get_thread_cond_locker_v1_t get_thread_cond_locker;
  /** @sa get_thread_table_locker_v1_t. */
  get_thread_table_locker_v1_t get_thread_table_locker;
  /** @sa get_thread_file_name_locker_v1_t. */
  get_thread_file_name_locker_v1_t get_thread_file_name_locker;
  /** @sa get_thread_file_stream_locker_v1_t. */
  get_thread_file_stream_locker_v1_t get_thread_file_stream_locker;
  /** @sa get_thread_file_descriptor_locker_v1_t. */
  get_thread_file_descriptor_locker_v1_t get_thread_file_descriptor_locker;
  /** @sa unlock_mutex_v1_t. */
  unlock_mutex_v1_t unlock_mutex;
  /** @sa unlock_rwlock_v1_t. */
  unlock_rwlock_v1_t unlock_rwlock;
  /** @sa signal_cond_v1_t. */
  signal_cond_v1_t signal_cond;
  /** @sa broadcast_cond_v1_t. */
  broadcast_cond_v1_t broadcast_cond;
  /** @sa start_mutex_wait_v1_t. */
  start_mutex_wait_v1_t start_mutex_wait;
  /** @sa end_mutex_wait_v1_t. */
  end_mutex_wait_v1_t end_mutex_wait;
  /** @sa start_rwlock_rdwait_v1_t. */
  start_rwlock_rdwait_v1_t start_rwlock_rdwait;
  /** @sa end_rwlock_rdwait_v1_t. */
  end_rwlock_rdwait_v1_t end_rwlock_rdwait;
  /** @sa start_rwlock_wrwait_v1_t. */
  start_rwlock_wrwait_v1_t start_rwlock_wrwait;
  /** @sa end_rwlock_wrwait_v1_t. */
  end_rwlock_wrwait_v1_t end_rwlock_wrwait;
  /** @sa start_cond_wait_v1_t. */
  start_cond_wait_v1_t start_cond_wait;
  /** @sa end_cond_wait_v1_t. */
  end_cond_wait_v1_t end_cond_wait;
  /** @sa start_table_wait_v1_t. */
  start_table_wait_v1_t start_table_wait;
  /** @sa end_table_wait_v1_t. */
  end_table_wait_v1_t end_table_wait;
  /** @sa start_file_open_wait_v1_t. */
  start_file_open_wait_v1_t start_file_open_wait;
  /** @sa end_file_open_wait_v1_t. */
  end_file_open_wait_v1_t end_file_open_wait;
  /** @sa end_file_open_wait_and_bind_to_descriptor_v1_t. */
  end_file_open_wait_and_bind_to_descriptor_v1_t
    end_file_open_wait_and_bind_to_descriptor;
  /** @sa start_file_wait_v1_t. */
  start_file_wait_v1_t start_file_wait;
  /** @sa end_file_wait_v1_t. */
  end_file_wait_v1_t end_file_wait;
};

/** @} (end of group Group_PSI_v1) */

#endif /* HAVE_PSI_1 */

#ifdef USE_PSI_2
#define HAVE_PSI_2
#endif

#ifdef HAVE_PSI_2

/**
  @defgroup Group_PSI_v2 Application Binary Interface, version 2
  @ingroup Instrumentation_interface
  @{
*/

/**
  Performance Schema Interface, version 2.
  This is a placeholder, this interface is not defined yet.
  @since PSI_VERSION_2
*/
struct PSI_v2
{
  /** Placeholder */
  int placeholder;
  /* ... extended interface ... */
};

/** Placeholder */
struct PSI_mutex_info_v2
{
  /** Placeholder */
  int placeholder;
};

/** Placeholder */
struct PSI_rwlock_info_v2
{
  /** Placeholder */
  int placeholder;
};

/** Placeholder */
struct PSI_cond_info_v2
{
  /** Placeholder */
  int placeholder;
};

/** Placeholder */
struct PSI_thread_info_v2
{
  /** Placeholder */
  int placeholder;
};

/** Placeholder */
struct PSI_file_info_v2
{
  /** Placeholder */
  int placeholder;
};

struct PSI_mutex_locker_state_v2
{
  /** Placeholder */
  int placeholder;
};

struct PSI_rwlock_locker_state_v2
{
  /** Placeholder */
  int placeholder;
};

struct PSI_cond_locker_state_v2
{
  /** Placeholder */
  int placeholder;
};

struct PSI_file_locker_state_v2
{
  /** Placeholder */
  int placeholder;
};

struct PSI_table_locker_state_v2
{
  /** Placeholder */
  int placeholder;
};

/** @} (end of group Group_PSI_v2) */

#endif /* HAVE_PSI_2 */

/**
  @typedef PSI
  The instrumentation interface for the current version.
  @sa PSI_CURRENT_VERSION
*/

/**
  @typedef PSI_mutex_info
  The mutex information structure for the current version.
*/

/**
  @typedef PSI_rwlock_info
  The rwlock information structure for the current version.
*/

/**
  @typedef PSI_cond_info
  The cond information structure for the current version.
*/

/**
  @typedef PSI_thread_info
  The thread information structure for the current version.
*/

/**
  @typedef PSI_file_info
  The file information structure for the current version.
*/

/* Export the required version */
#ifdef USE_PSI_1
typedef struct PSI_v1 PSI;
typedef struct PSI_mutex_info_v1 PSI_mutex_info;
typedef struct PSI_rwlock_info_v1 PSI_rwlock_info;
typedef struct PSI_cond_info_v1 PSI_cond_info;
typedef struct PSI_thread_info_v1 PSI_thread_info;
typedef struct PSI_file_info_v1 PSI_file_info;
typedef struct PSI_mutex_locker_state_v1 PSI_mutex_locker_state;
typedef struct PSI_rwlock_locker_state_v1 PSI_rwlock_locker_state;
typedef struct PSI_cond_locker_state_v1 PSI_cond_locker_state;
typedef struct PSI_file_locker_state_v1 PSI_file_locker_state;
typedef struct PSI_table_locker_state_v1 PSI_table_locker_state;
#endif

#ifdef USE_PSI_2
typedef struct PSI_v2 PSI;
typedef struct PSI_mutex_info_v2 PSI_mutex_info;
typedef struct PSI_rwlock_info_v2 PSI_rwlock_info;
typedef struct PSI_cond_info_v2 PSI_cond_info;
typedef struct PSI_thread_info_v2 PSI_thread_info;
typedef struct PSI_file_info_v2 PSI_file_info;
typedef struct PSI_mutex_locker_state_v2 PSI_mutex_locker_state;
typedef struct PSI_rwlock_locker_state_v2 PSI_rwlock_locker_state;
typedef struct PSI_cond_locker_state_v2 PSI_cond_locker_state;
typedef struct PSI_file_locker_state_v2 PSI_file_locker_state;
typedef struct PSI_table_locker_state_v2 PSI_table_locker_state;
#endif

#else /* HAVE_PSI_INTERFACE */

/**
  Dummy structure, used to declare PSI_server when no instrumentation
  is available.
  The content does not matter, since PSI_server will be NULL.
*/
struct PSI_none
{
  int opaque;
};
typedef struct PSI_none PSI;

#endif /* HAVE_PSI_INTERFACE */

extern MYSQL_PLUGIN_IMPORT PSI *PSI_server;

/** @} */

C_MODE_END
#endif /* MYSQL_PERFORMANCE_SCHEMA_INTERFACE_H */

