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

#include "sql/sql_lex.h"

namespace jdv {
/**
 * @brief Check if duality view prepared is required.
 *
 * @param   thd            Thread handle.
 * @param   table_ref      Table_ref* object for given JSON duality view.
 *
 * @return  true           If JDV prepare should be performed.
 * @return  false          Otherwise.
 */
bool is_prepare_required(THD *thd, Table_ref *table_ref);

/**
   Performs Syntax validation, parepares metadata tree and performs Semantic
   validation of a given JSON DUALITY VIEW.

   @param [in] thd Current THD object
   @param [in, out] table_ref Table_ref* object for given JSON DUALITY VIEW

   @retval false in case of success, true in case of failure
*/
bool prepare(THD *thd, Table_ref *table_ref);
}  // namespace jdv
