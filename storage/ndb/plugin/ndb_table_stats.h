/*
   Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#ifndef NDB_TABLE_STATS_H
#define NDB_TABLE_STATS_H

class THD;
class Ndb;

#include "storage/ndb/include/ndb_types.h"
#include "storage/ndb/include/ndbapi/NdbDictionary.hpp"

struct Ndb_table_stats {
  Uint64 row_count;
  Uint64 row_size;
  Uint64 fragment_memory;
  Uint64 fragment_extent_space;
  Uint64 fragment_extent_free_space;
};

/*
  @brief Return statistics for a table or just a specified partition

  @param thd          Thread handle
  @param ndb          The Ndb object instance
  @param ndbtab       The NDB table to return statistics for
  @param[out] stats   Pointer to Ndb_table_stats which should be updated with
                      the new stats.
  @param[out] ndb_err The NDB error which caused function to fail
  @param part_id      The partition id or ~0 for all partitions

  @note If "part_id" contains a legal partition id, it will return statistics
  only for the specified partition. Otherwise it will return the statistics,
  which is an aggregate over all partitions of that table.

  @return false on success
*/

bool ndb_get_table_statistics(THD *thd, Ndb *ndb,
                              const NdbDictionary::Table *ndbtab,
                              Ndb_table_stats *stats, NdbError &ndb_error,
                              Uint32 part_id = ~(Uint32)0);

/*
  @brief Return commit count for a table

  @param ndb                 The Ndb object instance
  @param ndbtab              The NDB table to return statistics for
  @param[out] ndb_err        The NDB error which caused function to fail
  @param[out] commit_count   Pointer to variable where to return the commit
                             count for the table.

  @return false on success
*/
bool ndb_get_table_commit_count(Ndb *ndb, const NdbDictionary::Table *ndbtab,
                                NdbError &ndb_error, Uint64 *commit_count);

#endif
