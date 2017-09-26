/*
   Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PFS_UNIT_TEST_CONFIG_INCLUDED
#define PFS_UNIT_TEST_CONFIG_INCLUDED

#include "my_config.h"

/* Always build unit tests with the performance schema */

#ifdef DISABLE_PSI_MUTEX
#undef DISABLE_PSI_MUTEX
#endif

#ifdef DISABLE_PSI_RWLOCK
#undef DISABLE_PSI_RWLOCK
#endif

#ifdef DISABLE_PSI_COND
#undef DISABLE_PSI_COND
#endif

#ifdef DISABLE_PSI_FILE
#undef DISABLE_PSI_FILE
#endif

#ifdef DISABLE_PSI_THREAD
#undef DISABLE_PSI_THREAD
#endif

#ifdef DISABLE_PSI_TABLE
#undef DISABLE_PSI_TABLE
#endif

#ifdef DISABLE_PSI_STAGE
#undef DISABLE_PSI_STAGE
#endif

#ifdef DISABLE_PSI_STATEMENT
#undef DISABLE_PSI_STATEMENT
#endif

#ifdef DISABLE_PSI_SP
#undef DISABLE_PSI_SP
#endif

#ifdef DISABLE_PSI_STATEMENT
#undef DISABLE_PSI_STATEMENT
#endif

#ifdef DISABLE_PSI_STATEMENT_DIGEST
#undef DISABLE_PSI_STATEMENT_DIGEST
#endif

#ifdef DISABLE_PSI_TRANSACTION
#undef DISABLE_PSI_TRANSACTION
#endif

#ifdef DISABLE_PSI_SOCKET
#undef DISABLE_PSI_SOCKET
#endif

#ifdef DISABLE_PSI_MEMORY
#undef DISABLE_PSI_MEMORY
#endif

#ifdef DISABLE_PSI_ERROR
#undef DISABLE_PSI_ERROR
#endif

#ifdef DISABLE_PSI_IDLE
#undef DISABLE_PSI_IDLE
#endif

#ifdef DISABLE_PSI_METADATA
#undef DISABLE_PSI_METADATA
#endif

#ifdef DISABLE_PSI_DATA_LOCK
#undef DISABLE_PSI_DATA_LOCK
#endif

#endif /* PFS_UNIT_TEST_CONFIG_INCLUDED */

