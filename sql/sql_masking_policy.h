/* Copyright (c) 2026, Oracle and/or its affiliates.

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

#ifndef SQL_MASKING_POLICY_INCLUDED
#define SQL_MASKING_POLICY_INCLUDED

#include <optional>
#include <string>

#include "lex_string.h"
#include "my_inttypes.h"

class Create_field;
class Item;
class Item_field;
class THD;
struct TABLE;

struct Sql_masking_policy_spec {
  LEX_CSTRING policy_name{NULL_CSTR};
  LEX_CSTRING masking_expression{NULL_CSTR};
  LEX_CSTRING argument_name{NULL_CSTR};
};

/**
  Returns the masking policy with the given name if it can be found.

  Returns an empty result if the masking policy with that name could not be
  found. No error is raised, but the output parameter `reason` contains the
  reason why the masking policy was not found.

  @param thd          Thread context
  @param policy_name  The name of the policy to look for
  @param[out] reason  If the function returns an empty value, this parameter
                      will return the reason why the policy was not found.
  @return The policy specification if found; otherwise empty.
*/
std::optional<Sql_masking_policy_spec> get_masking_policy_spec(
    THD *thd, LEX_CSTRING policy_name, std::string *reason);

/**
  Checks if the current user has the MANAGE_DATA_MASKING_POLICY privilege.

  Raises an error if the privilege is missing.

  @param thd  Thread context
  @retval false  Privilege is granted
  @retval true   Privilege is missing (error is reported)
*/
bool check_masking_policy_manage_privilege(THD *thd);

/**
  Drop the masking policy with the given name.

  @param thd          Thread context
  @param policy_name  The name of the policy to drop
  @param if_exists    True if DROP IF EXISTS, which means it is not an error
                      to call it with a policy name that does not exist.
  @retval true   If an error is raised
  @retval false  On success
*/
bool drop_masking_policy(THD *thd, LEX_CSTRING policy_name, bool if_exists);

/**
  Create a masking policy with the given specification.

  @param thd   Thread context
  @param if_not_exists True if CREATE MASKING POLICY IF NOT EXIST, which means
  it is not an error if the policy already exists. The existing policy is kept.
  @param spec  Masking policy specification
  @retval false  On success
  @retval true   If an error has been raised
*/
bool create_masking_policy(THD *thd, bool if_not_exists,
                           const Sql_masking_policy_spec &spec);

/**
  Check if the name is valid for a masking policy name or a masking policy
  argument name. Raises an error if it is not valid.

  @param name  Name to validate
  @retval false  Name is valid
  @retval true   Validation failed (error is reported)
*/
bool check_masking_policy_name(LEX_CSTRING name);

/**
  Validate structural and semantic restrictions for a masking policy expression.

  Rules enforced:
   - Must use the form `CASE WHEN <CURRENT_USER_IN|CURRENT_ROLE_IN>(...) THEN
     <expr> ELSE <expr>`
   - Exactly one WHEN clause and a required ELSE clause
   - THEN/ELSE must meet generated-column-like rules (UDFs allowed)
   - Only the policy argument may reference a column
   - Either THEN or ELSE must return the unmasked value

  @param thd           Thread context
  @param argument_name Name of the argument used in the policy
  @param expr          Expression tree of the policy
  @retval true   Validation failed (error reported)
  @retval false  Validation succeeded
*/
bool validate_masking_policy_syntax(THD *thd, LEX_CSTRING argument_name,
                                    Item *expr);

/**
  Parse and resolve the column’s masking expression under the column’s security
  context.

  Replaces the policy argument with the actual Item_field and keeps name
  resolution otherwise empty to prevent references to other columns.

  @param thd        Thread context
  @param item_field Column reference substituted for the policy argument
  @param spec       Masking policy specification (previously fetched)
  @return a pointer to the resolved Item on success, or nullptr on error (error
  is reported)
*/
Item *resolve_masking_expression(THD *thd, Item_field *item_field,
                                 const Sql_masking_policy_spec &spec);

/**
  Validates masking policies for CREATE/ALTER TABLE.

  Performs validation in three categories and delegates details to helpers:
   - Column eligibility for masking
  (validate_masking_policy_column_constraints()).
   - Masking function resolution and post-resolve validation
  (validate_masking_function_post_resolve()).
   - Column/function type compatibility (compatible_types()).

  See the referenced helpers for detailed rules and rationale.

  @param thd    Thread context
  @param buf    Row buffer used to back a temporary Field instance
  @param table  Table being created/altered
  @param field  Column definition being validated
  @retval true  Validation failed (error was reported)
  @retval false Validation succeeded
*/
bool validate_masking_policy_for_create_alter_table(THD *thd, uchar *buf,
                                                    TABLE *table,
                                                    const Create_field &field);

#endif  // SQL_MASKING_POLICY_INCLUDED
