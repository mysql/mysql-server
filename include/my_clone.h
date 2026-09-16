/* Copyright (c) 2026 Oracle and/or its affiliates.

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

#pragma once

#include <optional>
#include <string>

/**
  @file include/my_clone.h Functions to compare server version strings and
  version metadata to determine if Clone should be allowed from Donor to
  Recipient
*/

/**
  Compares versions and determine if clone is allowed. Clone is allowed if both
  the donor and recipient have exactly same version string. In version series
  8.1 and above, cloning is allowed if Major and Minor versions match. In 8.0
  series, clone is allowed if patch version is above clone backport version. In
  this comparison, suffixes are ignored: i.e. 8.0.25 should be the same as
  8.0.25-debug, but 8.0.25 isn't the same as 8.0.251. Beyond version 9.7,
  Cloning is also allowed from a Donor in one LTS to a recipient in the next
  LTS. For example, Cloning from 9.7.x to 28.4.y is allowed but not from
  28.4.y to 9.7.x. Conversly, a LTS Recipient is allowed to clone from a LTS
  Donor if the recipient starts its LTS lineage after Donor.

  Note: The Recipient performs these checks and determines if cloning is allowed

  @param[in] recipient        Recipient's version string
  @param[in] donor            Donor's version string
  @param[in] is_recipient_lts true if recipient is LTS
  @param[in] is_donor_lts     true if donor is LTS
  @param[in] recipient_prev_lts  Previous LTS version of the Recipient
  @return true if recipient must allow clone from donor, false otherwise
*/
[[nodiscard]] extern bool are_versions_clone_compatible(
    const std::string &recipient, const std::string &donor,
    const bool is_recipient_lts = false, const bool is_donor_lts = false,
    const std::optional<std::string> &recipient_prev_lts = std::nullopt);
