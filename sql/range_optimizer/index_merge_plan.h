/* Copyright (c) 2000, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SQL_RANGE_OPTIMIZER_INDEX_MERGE_PLAN_H_
#define SQL_RANGE_OPTIMIZER_INDEX_MERGE_PLAN_H_

template <class T>
class Mem_root_array;
class Opt_trace_object;
class RANGE_OPT_PARAM;
class String;
class THD;
struct AccessPath;
struct MEM_ROOT;

void trace_basic_info_index_merge(THD *thd, const AccessPath *path,
                                  const RANGE_OPT_PARAM *param,
                                  Opt_trace_object *trace_object);

void add_keys_and_lengths_index_merge(const AccessPath *path, String *key_names,
                                      String *used_lengths);

#ifndef NDEBUG
void dbug_dump_index_merge(int indent, bool verbose,
                           const Mem_root_array<AccessPath *> &children);
#endif

#endif  // SQL_RANGE_OPTIMIZER_INDEX_MERGE_PLAN_H_
