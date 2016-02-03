/* Copyright (c) 2008, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_PSI_THREAD_H
#define MYSQL_PSI_THREAD_H

/**
  @file include/mysql/psi/psi_thread.h
  Performance schema instrumentation interface.

  @defgroup psi_abi_thread Thread Instrumentation (ABI)
  @ingroup psi_abi
  @{
*/

#include "my_global.h"
#include "psi_base.h"
#include "my_thread.h" /* my_thread_handle */

C_MODE_START

#ifdef __cplusplus
  class THD;
#else
  /*
    Phony declaration when compiling C code.
    This is ok, because the C code will never have a THD anyway.
  */
  struct opaque_THD
  {
    int dummy;
  };
  typedef struct opaque_THD THD;
#endif

#ifdef HAVE_PSI_INTERFACE

/**
  @def PSI_THREAD_VERSION_1
  Performance Schema Thread Interface number for version 1.
  This version is supported.
*/
#define PSI_THREAD_VERSION_1 1

/**
  @def PSI_THREAD_VERSION_2
  Performance Schema Thread Interface number for version 2.
  This version is not implemented, it's a placeholder.
*/
#define PSI_THREAD_VERSION_2 2

/**
  @def PSI_CURRENT_THREAD_VERSION
  Performance Schema Thread Interface number for the most recent version.
  The most current version is @c PSI_THREAD_VERSION_1
*/
#define PSI_CURRENT_THREAD_VERSION 1

#ifndef USE_PSI_THREAD_2
#ifndef USE_PSI_THREAD_1
#define USE_PSI_THREAD_1
#endif /* USE_PSI_THREAD_1 */
#endif /* USE_PSI_THREAD_2 */

#ifdef USE_PSI_THREAD_1
#define HAVE_PSI_THREAD_1
#endif /* USE_PSI_THREAD_1 */

#ifdef USE_PSI_THREAD_2
#define HAVE_PSI_THREAD_2
#endif /* USE_PSI_THREAD_2 */

/** Entry point for the performance schema interface. */
struct PSI_thread_bootstrap
{
  /**
    ABI interface finder.
    Calling this method with an interface version number returns either
    an instance of the ABI for this version, or NULL.
    @sa PSI_THREAD_VERSION_1
    @sa PSI_THREAD_VERSION_2
    @sa PSI_CURRENT_THREAD_VERSION
  */
  void* (*get_interface)(int version);
};
typedef struct PSI_thread_bootstrap PSI_thread_bootstrap;

#ifdef HAVE_PSI_THREAD_1

/** @sa enum_vio_type. */
typedef int opaque_vio_type;

/**
  Interface for an instrumented thread.
  This is an opaque structure.
*/
struct PSI_thread;
typedef struct PSI_thread PSI_thread;

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
typedef struct PSI_thread_info_v1 PSI_thread_info_v1;

/**
  Thread registration API.
  @param category a category name (typically a plugin name)
  @param info an array of thread info to register
  @param count the size of the info array
*/
typedef void (*register_thread_v1_t)
  (const char *category, struct PSI_thread_info_v1 *info, int count);

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
                                 my_thread_handle *thread,
                                 const my_thread_attr_t *attr,
                                 void *(*start_routine)(void*), void *arg);

/**
  Create instrumentation for a thread.
  @param key the registered key
  @param identity an address typical of the thread
  @return an instrumented thread
*/
typedef struct PSI_thread* (*new_thread_v1_t)
  (PSI_thread_key key, const void *identity, ulonglong thread_id);

/**
  Assign a THD to an instrumented thread.
  @param thread the instrumented thread
  @param thd the sql layer THD to assign
*/
typedef void (*set_thread_THD_v1_t)(struct PSI_thread *thread,
                                    THD *thd);

/**
  Assign an id to an instrumented thread.
  @param thread the instrumented thread
  @param id the id to assign
*/
typedef void (*set_thread_id_v1_t)(struct PSI_thread *thread,
                                   ulonglong id);

/**
  Assign the current operating system thread id to an instrumented thread.
  The operating system task id is obtained from @c gettid()
  @param thread the instrumented thread
*/
typedef void (*set_thread_os_id_v1_t)(struct PSI_thread *thread);

/**
  Get the instrumentation for the running thread.
  For this function to return a result,
  the thread instrumentation must have been attached to the
  running thread using @c set_thread()
  @return the instrumentation for the running thread
*/
typedef struct PSI_thread* (*get_thread_v1_t)(void);

/**
  Assign a user name to the instrumented thread.
  @param user the user name
  @param user_len the user name length
*/
typedef void (*set_thread_user_v1_t)(const char *user, int user_len);

/**
  Assign a user name and host name to the instrumented thread.
  @param user the user name
  @param user_len the user name length
  @param host the host name
  @param host_len the host name length
*/
typedef void (*set_thread_account_v1_t)(const char *user, int user_len,
                                        const char *host, int host_len);

/**
  Assign a current database to the instrumented thread.
  @param db the database name
  @param db_len the database name length
*/
typedef void (*set_thread_db_v1_t)(const char* db, int db_len);

/**
  Assign a current command to the instrumented thread.
  @param command the current command
*/
typedef void (*set_thread_command_v1_t)(int command);

/**
  Assign a connection type to the instrumented thread.
  @param conn_type the connection type
*/
typedef void (*set_connection_type_v1_t)(opaque_vio_type conn_type);

/**
  Assign a start time to the instrumented thread.
  @param start_time the thread start time
*/
typedef void (*set_thread_start_time_v1_t)(time_t start_time);

/**
  Assign a state to the instrumented thread.
  @param state the thread state
*/
typedef void (*set_thread_state_v1_t)(const char* state);

/**
  Assign a process info to the instrumented thread.
  @param info the process into string
  @param info_len the process into string length
*/
typedef void (*set_thread_info_v1_t)(const char* info, uint info_len);

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
  Stores an array of connection attributes
  @param buffer         char array of length encoded connection attributes
                        in network format
  @param length         length of the data in buffer
  @param from_cs        charset in which @c buffer is encoded
  @return state
    @retval  non_0    attributes truncated
    @retval  0        stored the attribute
*/
typedef int (*set_thread_connect_attrs_v1_t)(const char *buffer, uint length,
                                             const void *from_cs);

/**
  Performance Schema Thread Interface, version 1.
  @since PSI_IDLE_VERSION_1
*/
struct PSI_thread_service_v1
{
  /** @sa register_thread_v1_t. */
  register_thread_v1_t register_thread;
  /** @sa spawn_thread_v1_t. */
  spawn_thread_v1_t spawn_thread;
  /** @sa new_thread_v1_t. */
  new_thread_v1_t new_thread;
  /** @sa set_thread_id_v1_t. */
  set_thread_id_v1_t set_thread_id;
  /** @sa set_thread_THD_v1_t. */
  set_thread_THD_v1_t set_thread_THD;
  /** @sa set_thread_os_id_v1_t. */
  set_thread_os_id_v1_t set_thread_os_id;
  /** @sa get_thread_v1_t. */
  get_thread_v1_t get_thread;
  /** @sa set_thread_user_v1_t. */
  set_thread_user_v1_t set_thread_user;
  /** @sa set_thread_account_v1_t. */
  set_thread_account_v1_t set_thread_account;
  /** @sa set_thread_db_v1_t. */
  set_thread_db_v1_t set_thread_db;
  /** @sa set_thread_command_v1_t. */
  set_thread_command_v1_t set_thread_command;
  /** @sa set_connection_type_v1_t. */
  set_connection_type_v1_t set_connection_type;
  /** @sa set_thread_start_time_v1_t. */
  set_thread_start_time_v1_t set_thread_start_time;
  /** @sa set_thread_state_v1_t. */
  set_thread_state_v1_t set_thread_state;
  /** @sa set_thread_info_v1_t. */
  set_thread_info_v1_t set_thread_info;
  /** @sa set_thread_v1_t. */
  set_thread_v1_t set_thread;
  /** @sa delete_current_thread_v1_t. */
  delete_current_thread_v1_t delete_current_thread;
  /** @sa delete_thread_v1_t. */
  delete_thread_v1_t delete_thread;
  /** @sa set_thread_connect_attrs_v1_t. */
  set_thread_connect_attrs_v1_t set_thread_connect_attrs;
};

#endif /* HAVE_PSI_THREAD_1 */

/* Export the required version */
#ifdef USE_PSI_THREAD_1
typedef struct PSI_thread_service_v1 PSI_thread_service_t;
typedef struct PSI_thread_info_v1 PSI_thread_info;
#else
typedef struct PSI_placeholder PSI_thread_service_t;
typedef struct PSI_placeholder PSI_thread_info;
#endif

extern MYSQL_PLUGIN_IMPORT PSI_thread_service_t *psi_thread_service;

/** @} (end of group psi_abi_thread) */

#endif /* HAVE_PSI_INTERFACE */

C_MODE_END

#endif /* MYSQL_PSI_THREAD_H */

