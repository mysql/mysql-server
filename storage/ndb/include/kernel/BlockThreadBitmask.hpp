/*
   Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#ifndef BLOCK_THREAD_BITMASK_HPP
#define BLOCK_THREAD_BITMASK_HPP

#include "kernel_types.h"
#include "ndb_limits.h"
#include "ndb_math.h"
#include "util/Bitmask.hpp"

#define JAM_FILE_ID 513

static constexpr Uint32 NDB_MAX_BLOCK_THREADS = NDBMT_MAIN_THREADS +
                                                MAX_NDBMT_TC_THREADS +
                                                MAX_NDBMT_RECEIVE_THREADS +
                                                MAX_NDBMT_LQH_THREADS;

static constexpr Uint32 NDB_BLOCK_THREAD_BITMASK_SIZE =
    ndb_ceil_div(NDB_MAX_BLOCK_THREADS, 32U);

typedef Bitmask<NDB_BLOCK_THREAD_BITMASK_SIZE> BlockThreadBitmask;

#undef JAM_FILE_ID

#endif
