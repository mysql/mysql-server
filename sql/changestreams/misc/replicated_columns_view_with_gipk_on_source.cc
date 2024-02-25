/* Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#include "sql/changestreams/misc/replicated_columns_view_with_gipk_on_source.h"

#include "sql/rpl_record.h"  // Bit_reader
#include "sql/sql_class.h"   // THD

cs::util::ReplicatedColumnsViewWithGipkOnSource::
    ReplicatedColumnsViewWithGipkOnSource(TABLE const *target, THD const *thd)
    : ReplicatedColumnsView{target, thd} {}

Table_columns_view<>::iterator
cs::util::ReplicatedColumnsViewWithGipkOnSource::begin() {
  Table_columns_view<>::iterator to_return{*this, -1, -1, 1};
  ++to_return;
  return to_return;
}
