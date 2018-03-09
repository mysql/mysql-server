/* Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__BOOTSTRAP_CTX_INCLUDED
#define DD__BOOTSTRAP_CTX_INCLUDED

#include <set>

#include "my_inttypes.h"        // uint
#include "sql/dd/dd_version.h"  // DD_VERSION
#include "sql/mysqld.h"         // opt_initialize

class THD;

namespace dd {
namespace bootstrap {

// Enumeration of bootstrapping stages.
enum class Stage {
  NOT_STARTED,          // Not started.
  STARTED,              // Started, nothing prepared yet.
  CREATED_TABLESPACES,  // Created predefined tablespaces.
  FETCHED_PROPERTIES,   // Done reading DD properties.
  CREATED_TABLES,       // Tables created, able to store persistently.
  SYNCED,               // Cached meta data synced with persistent storage.
  UPGRADED_TABLES,      // Created new table versions and migrated meta data.
  POPULATED,            // (Re)populated tables with meta data.
  STORED_DD_META_DATA,  // Stored the hard coded meta data of the DD tables.
  VERSION_UPDATED,      // The properties in 'dd_properties' are updated.
  FINISHED              // Completed.
};

// Individual version labels that we can refer to.
static constexpr uint DD_VERSION_80011 = 80011;

/*
  Set of supported DD version labels. A supported DD version is a version
  from which we can upgrade. In the case of downgrade, this is not relevant,
  since the set of supported versions is defined when the server is built,
  and newer version numbers are not added to this set. in the case of
  downgrade, we instead have to check the MINOR_DOWNGRADE_THRESHOLD, which is
  stored in the 'dd_properties' table by the server from which we downgrade.
*/
static std::set<uint> supported_dd_versions = {DD_VERSION_80011};

class DD_bootstrap_ctx {
 private:
  uint m_actual_dd_version = 0;
  Stage m_stage = Stage::NOT_STARTED;

 public:
  DD_bootstrap_ctx() {}

  static DD_bootstrap_ctx &instance();

  Stage get_stage() const { return m_stage; }

  void set_stage(Stage stage) { m_stage = stage; }

  bool supported_dd_version() const {
    return (supported_dd_versions.find(m_actual_dd_version) !=
            supported_dd_versions.end());
  }

  void set_actual_dd_version(uint actual_dd_version) {
    m_actual_dd_version = actual_dd_version;
  }

  uint get_actual_dd_version() const { return m_actual_dd_version; }

  bool actual_dd_version_is(uint compare_actual_dd_version) const {
    return (m_actual_dd_version == compare_actual_dd_version);
  }

  bool is_restart() const {
    return !opt_initialize && (m_actual_dd_version == dd::DD_VERSION);
  }

  bool is_upgrade() const {
    return !opt_initialize && (m_actual_dd_version < dd::DD_VERSION);
  }

  bool is_upgrade_from_before(uint compare_actual_dd_version) const {
    return (is_upgrade() && m_actual_dd_version < compare_actual_dd_version);
  }

  bool is_minor_downgrade() const {
    return !opt_initialize &&
           (m_actual_dd_version / 10000 == dd::DD_VERSION / 10000) &&
           (m_actual_dd_version > dd::DD_VERSION);
  }

  bool is_above_minor_downgrade_threshold(THD *thd) const;

  bool is_initialize() const {
    return opt_initialize && (m_actual_dd_version == dd::DD_VERSION);
  }
};

}  // namespace bootstrap
}  // namespace dd

#endif  // DD__BOOTSTRAP_CTX_INCLUDED
