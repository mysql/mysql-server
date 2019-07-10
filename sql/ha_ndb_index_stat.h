/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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

/* provides declarations only to index_stat.cc */

extern struct st_ndb_status g_ndb_status;

extern pthread_mutex_t ndbcluster_mutex;

extern pthread_t ndb_index_stat_thread;
extern pthread_cond_t COND_ndb_index_stat_thread;
extern pthread_mutex_t LOCK_ndb_index_stat_thread;

/* protect entry lists where needed */
extern pthread_mutex_t ndb_index_stat_list_mutex;

/* protect and signal changes in stats entries */
extern pthread_mutex_t ndb_index_stat_stat_mutex;
extern pthread_cond_t ndb_index_stat_stat_cond;

/* these have to live in ha_ndbcluster.cc */
extern bool ndb_index_stat_get_enable(THD *thd);
extern long g_ndb_status_index_stat_cache_query;
extern long g_ndb_status_index_stat_cache_clean;

void 
compute_index_bounds(NdbIndexScanOperation::IndexBound & bound,
                     const KEY *key_info,
                     const key_range *start_key, const key_range *end_key,
                     int from);

/* error codes local to ha_ndb */

/* stats thread is not open for requests (should not happen) */
#define Ndb_index_stat_error_NOT_ALLOW          9001

/* stats entry for existing index not found (should not happen) */
#define Ndb_index_stat_error_NOT_FOUND          9002

/* request on stats entry with recent error was ignored */
#define Ndb_index_stat_error_HAS_ERROR          9003
