/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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

#ifndef GROUP_REPLICATION_PRIMARY_ELECT_PREFER_MOST_UPDATED_SERVICE_H_
#define GROUP_REPLICATION_PRIMARY_ELECT_PREFER_MOST_UPDATED_SERVICE_H_

#include <mysql/components/service.h>
#include <stddef.h>
#include <string>

BEGIN_SERVICE_DEFINITION(group_replication_primary_election)

/**
  This function SHALL be called whenever the caller wants to
  update last time most uptodate method was used to order a list of members to
  select new primary.
  Members of the group will be ordered by number of transactions
  applied, following by weight and lexical order.

  @param[in]   timestamp String representation of time last change
  @param[in]   transactions_delta Members delta on transactions applied

  @return false success, true on failure.
*/

DECLARE_BOOL_METHOD(update_primary_election_status,
                    (char *timestamp, uint64_t transactions_delta));

END_SERVICE_DEFINITION(group_replication_primary_election)

#endif  // GROUP_REPLICATION_PRIMARY_ELECT_PREFER_MOST_UPDATED_SERVICE_H_
