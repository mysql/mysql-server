/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef KERNEL_MUTEXES_HPP
#define KERNEL_MUTEXES_HPP

#include <ndb_types.h>

#define JAM_FILE_ID 461


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

/**
 * This rw lock is ued by DIH to serialize LCP/COPY TABREQ/CREATE FRAG REQ
 */
#define DIH_FRAGMENT_INFO 2



#undef JAM_FILE_ID

#endif
