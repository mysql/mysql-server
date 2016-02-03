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

#ifndef MYSQL_PSI_STAGE_H
#define MYSQL_PSI_STAGE_H

/**
  @file include/mysql/psi/psi_stage.h
  Performance schema instrumentation interface.

  @defgroup psi_abi_stage Stage Instrumentation (ABI)
  @ingroup psi_abi
  @{
*/

#include "my_global.h"
#include "psi_base.h"

C_MODE_START

#ifdef HAVE_PSI_INTERFACE

/**
  @def PSI_STAGE_VERSION_1
  Performance Schema Stage Interface number for version 1.
  This version is supported.
*/
#define PSI_STAGE_VERSION_1 1

/**
  @def PSI_STAGE_VERSION_2
  Performance Schema Stage Interface number for version 2.
  This version is not implemented, it's a placeholder.
*/
#define PSI_STAGE_VERSION_2 2

/**
  @def PSI_CURRENT_STAGE_VERSION
  Performance Schema Stage Interface number for the most recent version.
  The most current version is @c PSI_STAGE_VERSION_1
*/
#define PSI_CURRENT_STAGE_VERSION 1

#ifndef USE_PSI_STAGE_2
#ifndef USE_PSI_STAGE_1
#define USE_PSI_STAGE_1
#endif /* USE_PSI_STAGE_1 */
#endif /* USE_PSI_STAGE_2 */

#ifdef USE_PSI_STAGE_1
#define HAVE_PSI_STAGE_1
#endif /* USE_PSI_STAGE_1 */

#ifdef USE_PSI_STAGE_2
#define HAVE_PSI_STAGE_2
#endif /* USE_PSI_STAGE_2 */

/** Entry point for the performance schema interface. */
struct PSI_stage_bootstrap
{
  /**
    ABI interface finder.
    Calling this method with an interface version number returns either
    an instance of the ABI for this version, or NULL.
    @sa PSI_STAGE_VERSION_1
    @sa PSI_STAGE_VERSION_2
    @sa PSI_CURRENT_STAGE_VERSION
  */
  void* (*get_interface)(int version);
};
typedef struct PSI_stage_bootstrap PSI_stage_bootstrap;

#ifdef HAVE_PSI_STAGE_1

/**
  Interface for an instrumented stage progress.
  This is a public structure, for efficiency.
*/
struct PSI_stage_progress_v1
{
  ulonglong m_work_completed;
  ulonglong m_work_estimated;
};
typedef struct PSI_stage_progress_v1 PSI_stage_progress_v1;

/**
  Stage instrument information.
  @since PSI_VERSION_1
  This structure is used to register an instrumented stage.
*/
struct PSI_stage_info_v1
{
  /** The registered stage key. */
  PSI_stage_key m_key;
  /** The name of the stage instrument to register. */
  const char *m_name;
  /** The flags of the stage instrument to register. */
  int m_flags;
};
typedef struct PSI_stage_info_v1 PSI_stage_info_v1;

/**
  Stage registration API.
  @param category a category name
  @param info an array of stage info to register
  @param count the size of the info array
*/
typedef void (*register_stage_v1_t)
  (const char *category, struct PSI_stage_info_v1 **info, int count);

/**
  Start a new stage, and implicitly end the previous stage.
  @param key the key of the new stage
  @param src_file the source file name
  @param src_line the source line number
  @return the new stage progress
*/
typedef PSI_stage_progress_v1* (*start_stage_v1_t)
  (PSI_stage_key key, const char *src_file, int src_line);

/**
  Get the current stage progress.
  @return the stage progress
*/
typedef PSI_stage_progress_v1* (*get_current_stage_progress_v1_t)(void);

/** End the current stage. */
typedef void (*end_stage_v1_t) (void);

/**
  Performance Schema Stage Interface, version 1.
  @since PSI_STAGE_VERSION_1
*/
struct PSI_stage_service_v1
{
  /** @sa register_stage_v1_t. */
  register_stage_v1_t register_stage;
  /** @sa start_stage_v1_t. */
  start_stage_v1_t start_stage;
  /** @sa get_current_stage_progress_v1_t. */
  get_current_stage_progress_v1_t get_current_stage_progress;
  /** @sa end_stage_v1_t. */
  end_stage_v1_t end_stage;
};

#endif /* HAVE_PSI_STAGE_1 */

/* Export the required version */
#ifdef USE_PSI_STAGE_1
typedef struct PSI_stage_service_v1 PSI_stage_service_t;
typedef struct PSI_stage_info_v1 PSI_stage_info;
typedef struct PSI_stage_progress_v1 PSI_stage_progress;
#else
typedef struct PSI_placeholder PSI_stage_service_t;
typedef struct PSI_placeholder PSI_stage_info;
typedef struct PSI_placeholder PSI_stage_progress;
#endif

extern MYSQL_PLUGIN_IMPORT PSI_stage_service_t *psi_stage_service;

/** @} (end of group psi_abi_stage) */

#else

/**
  Stage instrument information.
  @since PSI_VERSION_1
  This structure is used to register an instrumented stage.
*/
struct PSI_stage_info_none
{
  /** Unused stage key. */
  unsigned int m_key;
  /** The name of the stage instrument. */
  const char *m_name;
  /** Unused stage flags. */
  int m_flags;
};

/**
  The stage instrumentation has to co exist with the legacy
  THD::set_proc_info instrumentation.
  To avoid duplication of the instrumentation in the server,
  the common PSI_stage_info structure is used,
  so we export it here, even when not building
  with HAVE_PSI_INTERFACE.
*/
typedef struct PSI_stage_info_none PSI_stage_info;

typedef struct PSI_placeholder PSI_stage_progress;

#endif /* HAVE_PSI_INTERFACE */

C_MODE_END

#endif /* MYSQL_PSI_FILE_H */

