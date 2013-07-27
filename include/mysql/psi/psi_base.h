/* Copyright (c) 2008, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_PSI_BASE_H
#define MYSQL_PSI_BASE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
  @file mysql/psi/psi_base.h
  Performance schema instrumentation interface.

  @defgroup Instrumentation_interface Instrumentation Interface
  @ingroup Performance_schema
  @{
*/

#define PSI_INSTRUMENT_ME 0

#define PSI_NOT_INSTRUMENTED 0

#ifdef HAVE_PSI_INTERFACE

/**
  Global flag.
  This flag indicate that an instrumentation point is a global variable,
  or a singleton.
*/
#define PSI_FLAG_GLOBAL (1 << 0)

/**
  Mutable flag.
  This flag indicate that an instrumentation point is a general placeholder,
  that can mutate into a more specific instrumentation point.
*/
#define PSI_FLAG_MUTABLE (1 << 1)

#define PSI_FLAG_THREAD (1 << 2)

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

#ifndef USE_PSI_2
#ifndef USE_PSI_1
#define USE_PSI_1
#endif
#endif

#ifdef USE_PSI_1
#define HAVE_PSI_1
#endif

#ifdef USE_PSI_2
#define HAVE_PSI_2
#endif

/*
  Allow to override PSI_XXX_CALL at compile time
  with more efficient implementations, if available.
  If nothing better is available,
  make a dynamic call using the PSI_server function pointer.
*/

#define PSI_DYNAMIC_CALL(M) PSI_server->M

#endif /* HAVE_PSI_INTERFACE */

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* MYSQL_PSI_BASE_H */

