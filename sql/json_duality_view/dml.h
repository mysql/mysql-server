#pragma once

/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "mem_root_deque.h"

class THD;
class Table_ref;
class Sql_cmd_insert_base;
class Item;
using List_item = mem_root_deque<Item *>;

namespace jdv {

[[nodiscard]] bool jdv_prepare_insert(THD *, const Table_ref *,
                                      Sql_cmd_insert_base *);

[[nodiscard]] bool jdv_prepare_update(THD *, const Table_ref *, bool);

[[nodiscard]] bool jdv_prepare_delete(THD *, const Table_ref *, bool);

[[nodiscard]] bool jdv_insert(THD *, const Table_ref *,
                              const mem_root_deque<List_item *> &);

[[nodiscard]] bool jdv_update(THD *thd, const Table_ref *,
                              const mem_root_deque<Item *> *,
                              const mem_root_deque<Item *> *, ulonglong *);

[[nodiscard]] bool jdv_delete(THD *thd, const Table_ref *, ulonglong *);

}  // namespace jdv
