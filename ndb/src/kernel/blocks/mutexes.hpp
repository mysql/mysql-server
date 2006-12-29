/* Copyright (C) 2003 MySQL AB

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

#ifndef KERNEL_MUTEXES_HPP
#define KERNEL_MUTEXES_HPP

#include <ndb_types.h>

/**
 * This mutex is used by:
 *  DIH  - before sending START_LCP to all participants
 *  DICT - before commiting a CREATE TABLE
 *  BACKUP - before sending DEFINE_BACKUP
 */
#define DIH_START_LCP_MUTEX 0
#define DICT_COMMIT_TABLE_MUTEX 0

/**
 * This mutex is used by
 *  DIH - before switching primary replica
 *  BACKUP - before sending DEFINE_BACKUP
 */
#define DIH_SWITCH_PRIMARY_MUTEX 1
#define BACKUP_DEFINE_MUTEX      1

#endif
