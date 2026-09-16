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

#ifndef DD__BOOTSTRAP__SERVER_VERSION_TRANSITION_INCLUDED
#define DD__BOOTSTRAP__SERVER_VERSION_TRANSITION_INCLUDED

#include "my_sys.h"  // My_server_version

namespace dd::bootstrap {

/**
  Decide whether a stored server version may move to a target server version.

  This class is intentionally independent of DD_properties and THD. Tests can
  construct it directly from persisted source-version details and target
  metadata, inspect the pure Result from evaluate(), and leave error-log
  side-effects to check_and_report().
*/
class Server_version_transition {
 public:
  enum class Result {
    ACCEPTED,
    INVALID_SERVER_UPGRADE_NOT_LTS,
    INVALID_SERVER_UPGRADE_SKIPS_LTS_LINEAGE,
    BEYOND_SERVER_UPGRADE_THRESHOLD,
    INVALID_SERVER_DOWNGRADE_NOT_PATCH,
    NO_PATCH_DOWNGRADE_FOR_INNOVATION_RELEASES,
    BEYOND_SERVER_DOWNGRADE_THRESHOLD
  };

  Server_version_transition(My_server_version source, My_server_version target);

  Result evaluate() const;

  /**
    Emit the matching error-log message if evaluate() rejects the transition.

    @retval false  Transition accepted.
    @retval true   Transition rejected and error reported.
  */
  bool check_and_report() const;

 private:
  My_server_version m_source;
  My_server_version m_target;
};

}  // namespace dd::bootstrap

#endif  // DD__BOOTSTRAP__SERVER_VERSION_TRANSITION_INCLUDED
