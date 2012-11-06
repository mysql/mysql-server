/*
 Copyright (c) 2011, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */
#ifndef NDBMEMCACHE_NDB_ERROR_LOGGER_H
#define NDBMEMCACHE_NDB_ERROR_LOGGER_H

/* Log NDB error messages, 
   but take care to prevent flooding the log file with repeated errors.
*/


#include "ndbmemcache_global.h"
#include "workitem.h"

#ifdef __cplusplus
#include <NdbApi.hpp>

enum {
  ERR_SUCCESS = ndberror_st_success,  /* == 0 */
  ERR_TEMP = ndberror_st_temporary,
  ERR_PERM = ndberror_st_permanent,
  ERR_UR   = ndberror_st_unknown
};

int log_ndb_error(const NdbError &);
int log_app_error(ndberror_struct const *);
#endif


DECLARE_FUNCTIONS_WITH_C_LINKAGE

void ndb_error_logger_init(SERVER_CORE_API *, size_t log_level);
void ndb_error_logger_stats(ADD_STAT add_stat, const void *cookie);

END_FUNCTIONS_WITH_C_LINKAGE


#endif

