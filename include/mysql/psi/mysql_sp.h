/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_SP_H
#define MYSQL_SP_H

/**
  @file mysql/psi/mysql_sp.h
  Instrumentation helpers for stored programs.
*/

#include "mysql/psi/psi.h"

#ifndef PSI_SP_CALL
#define PSI_SP_CALL(M) PSI_DYNAMIC_CALL(M)                               
#endif    

#ifdef HAVE_PSI_SP_INTERFACE
  #define MYSQL_START_SP(STATE) \
    inline_mysql_start_sp(STATE)
#else
  #define MYSQL_START_SP(STATE) \
    NULL
#endif


#ifdef HAVE_PSI_SP_INTERFACE
  #define MYSQL_END_SP(LOCKER) \
    inline_mysql_end_sp(LOCKER)
#else
  #define MYSQL_END_SP(LOCKER) \
    do {} while (0)
#endif

#ifdef HAVE_PSI_SP_INTERFACE
  #define MYSQL_DROP_SP(STATE) \
    inline_mysql_drop_sp(STATE)
#else
  #define MYSQL_DROP_SP(STATE) \
    do {} while (0)
#endif

/**
  Enum values for the various OBJECT_TYPE.
*/
enum enum_sp_object_type
{
  SP_OBJECT_TYPE_TABLE= 1,
  SP_OBJECT_TYPE_EVENT= 2,
  SP_OBJECT_TYPE_FUNCTION= 3,
  SP_OBJECT_TYPE_PROCEDURE= 4,
  SP_OBJECT_TYPE_TRIGGER= 5,
  SP_OBJECT_TYPE_TEMPORARY_TABLE= 6
};

static inline struct PSI_sp_locker *
inline_mysql_start_sp(PSI_sp_locker_state *state)
{
  PSI_sp_locker *locker;
 
  locker= PSI_SP_CALL(get_thread_sp_locker)(state);
  if (likely(locker != NULL))
    PSI_SP_CALL(start_sp)(locker);
  return locker;
}

static inline void inline_mysql_end_sp(PSI_sp_locker *locker)
{
  if (likely(locker != NULL))
    PSI_SP_CALL(end_sp)(locker);
}

static inline void inline_mysql_drop_sp(PSI_sp_locker_state *state)
{
  if (likely(state != NULL))
    PSI_SP_CALL(drop_sp)(state);
}
#endif
