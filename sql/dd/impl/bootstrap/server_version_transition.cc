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

#include "sql/dd/impl/bootstrap/server_version_transition.h"

#include <cassert>

#include "mysql/components/services/log_builtins.h"
#include "mysql/my_loglevel.h"
#include "mysqld_error.h"

namespace dd::bootstrap {

Server_version_transition::Server_version_transition(My_server_version source,
                                                     My_server_version target)
    : m_source(source), m_target(target) {
#ifndef NDEBUG
  const auto is_valid_release_version = [](const My_server_version &version) {
    if (!version.is_calendar() && version.previous_lts_base() != 0)
      return false;
    if (version.major() < 9) return true;
    if (version.major() == 9) return version.minor() <= 7;
    if (version.major() < 26) return false;
    if (version.minor() < 1 || version.minor() > 12) return false;
    return version.major() != 26 || version.minor() >= 7;
  };

  assert(is_valid_release_version(m_source));
  assert(is_valid_release_version(m_target));
#endif
}

Server_version_transition::Result Server_version_transition::evaluate() const {
  /*
    MySQL moved from the legacy sequential major release model to
    calendar-based versioning after the 9.7 LTS release line. There is
    intentionally no product release named 10.x. The first calendar Innovation
    release is 26.7.0. MYSQL_VERSION declares the previous LTS release base,
    and runtime derives the effective compatibility lineage from that value. A
    source previous-LTS value of 0 means the source DD has no persisted
    previous-LTS metadata.

      Last legacy release line:
        9.7.x LTS

      First calendar lineage:
        Calendar releases whose MYSQL_PREVIOUS_LTS_VERSION is 9.7.0.

      Later calendar lineages:
        If a calendar release is designated LTS, subsequent releases in the
        next lineage use that LTS release base as MYSQL_PREVIOUS_LTS_VERSION.

    Within a compatibility lineage, forward movement is accepted. Moving into
    a lineage whose previous-LTS value is the source LTS release base is
    accepted only from that LTS source. Moving across more than one lineage
    boundary is rejected.

      Accepted:
        9.7.x LTS   -> 26.7.0  (last legacy LTS to first calendar lineage)
        9.7.x LTS   -> a first-lineage calendar LTS release
        Innovation   -> later release with the same previous-LTS value
        calendar LTS -> later release whose previous-LTS value is that LTS base

      Rejected:
        8.4.x LTS  -> 26.7.0  (skips the legacy 9.x release line)
        9.7.x LTS  -> 10.0.0   (10.x is not a release convention)
        9.7.x LTS  -> a later lineage that does not use 9.7.0 as previous LTS
        Innovation  -> release with a different previous-LTS value
        calendar LTS -> release that skips beyond the immediately next lineage
  */
  if (m_target.version == m_source.version) return Result::ACCEPTED;

  if (m_target.version > m_source.version) {
    bool accepted_upgrade_lineage = false;

    if (m_source.is_calendar()) {
      /*
        Lineage-aware upgrade.
        Calendar source releases declare a previous LTS release base. The
        transition logic derives the source and target compatibility lineage
        from those release bases. Legacy/pre-lineage sources have no persisted
        previous-LTS metadata and are handled only by the legacy-to-calendar
        migration bridge below.

        Accepted upgrade: the target remains in the same compatibility lineage.
          Example:
            A source and target whose previous-LTS release bases match.

        Accepted upgrade: the target's previous-LTS value is the source LTS
        release base.
          Example:
            A calendar LTS source to a later target in the lineage opened by
            that LTS.

        Rejected upgrade: the target crosses a lineage boundary from a non-LTS
        source, or skips a lineage even from an LTS source.
          Example:
            A non-LTS source to a target with a different previous-LTS release
            base.
      */
      accepted_upgrade_lineage =
          m_target.has_same_lts_lineage_as(m_source) ||
          (m_source.is_lts && m_target.starts_lts_lineage_after(m_source));
    } else {
      /*
        Legacy-to-calendar migration bridge.

        Accepted upgrade: the source is the final non-calendar LTS release
        line, 9.7.x, and the target declares that 9.7.0 is its previous LTS.
          Examples:
            9.7.x LTS -> 26.7.0
            9.7.x LTS -> 26.10.0
            9.7.x LTS -> a first-lineage calendar LTS release

        Rejected upgrade: the source is not 9.7 LTS, or the target skips the
        first calendar lineage.
          Examples:
            8.4.x LTS -> 26.7.0
            9.6.x     -> 26.7.0
            9.7.x LTS -> a target whose previous LTS is not 9.7.0

        The configured target version cannot be older than 26.7.0 because the
        build configuration rejects earlier calendar versions.
      */
      accepted_upgrade_lineage =
          m_source.major() == 9 && m_source.minor() == 7 && m_source.is_lts &&
          m_target.is_calendar() && m_target.starts_lts_lineage_after(m_source);
    }

    if (!accepted_upgrade_lineage) {
      /*
        Any upgrade that did not match one of the accepted paths above is
        rejected here. This catches cross-lineage violations, and versions
        which are intentionally neither legacy nor calendar.

        Rejected upgrade:
          8.4.x LTS -> 26.7.0  (not the final legacy LTS source)
          9.7.x LTS -> 10.0.0   (10.x is not released)
          25.10.0   -> 26.7.0  (25.x is not a valid predecessor)
      */
      if (m_source.major() == 9 && m_source.minor() == 7 && m_source.is_lts &&
          m_target.is_calendar() && m_target.previous_lts_base() != 0 &&
          m_target.previous_lts_base() != m_source.base()) {
        return Result::INVALID_SERVER_UPGRADE_SKIPS_LTS_LINEAGE;
      }

      return Result::INVALID_SERVER_UPGRADE_NOT_LTS;
    }

    if (m_target.version < m_source.upgrade_threshold &&
        m_target.base() != m_source.base()) {
      /*
        The source server may persist a minimum allowed upgrade target. This is
        used when an already-released server later discovers that upgrades to
        earlier targets are unsafe.

        Accepted upgrade:
          9.7.x LTS -> 26.10.0, when SERVER_UPGRADE_THRESHOLD <= 26.10.0

        Rejected upgrade:
          9.7.x LTS -> 26.7.0, when SERVER_UPGRADE_THRESHOLD is 26.10.0
      */
      return Result::BEYOND_SERVER_UPGRADE_THRESHOLD;
    }

    return Result::ACCEPTED;
  }

  // This is a downgrade attempt.
  if (m_target.base() != m_source.base()) {
    /*
      Downgrade semantics do not change for calendar versions. Only patch
      downgrades inside the same visible YY.M release base are candidates for
      acceptance.

      Accepted downgrade, subject to the threshold below:
        LTS patch release -> earlier patch in the same visible YY.M base

      Rejected downgrade:
        Release in one YY.M base -> release in a different YY.M base
    */
    return Result::INVALID_SERVER_DOWNGRADE_NOT_PATCH;
  }

  if (m_target.version < m_source.downgrade_threshold) {
    /*
      The source server may persist a minimum allowed downgrade target. LTS
      releases can allow a bounded patch downgrade. Non-LTS releases persist
      their own version as the threshold, so patch downgrade attempts from them
      reach this branch and get the Innovation-specific error.

      Accepted downgrade:
        LTS patch release -> earlier patch in the same visible YY.M base, when
        the target is at or above SERVER_DOWNGRADE_THRESHOLD.

      Rejected LTS downgrade:
        LTS patch release -> earlier patch in the same visible YY.M base, when
        the target is below SERVER_DOWNGRADE_THRESHOLD.

      Rejected Innovation downgrade:
        Innovation patch release -> earlier patch in the same visible YY.M base.
    */
    if (!m_source.is_lts)
      return Result::NO_PATCH_DOWNGRADE_FOR_INNOVATION_RELEASES;
    return Result::BEYOND_SERVER_DOWNGRADE_THRESHOLD;
  }

  return Result::ACCEPTED;
}

bool Server_version_transition::check_and_report() const {
  switch (evaluate()) {
    case Result::ACCEPTED:
      return false;
    case Result::INVALID_SERVER_UPGRADE_NOT_LTS:
      LogErr(ERROR_LEVEL, ER_INVALID_SERVER_UPGRADE_NOT_LTS, m_source.version,
             m_target.version, m_source.version);
      return true;
    case Result::INVALID_SERVER_UPGRADE_SKIPS_LTS_LINEAGE:
      LogErr(ERROR_LEVEL, ER_INVALID_SERVER_UPGRADE_SKIPS_LTS_LINEAGE,
             m_source.version, m_target.version, m_target.previous_lts,
             m_source.base() * 100);
      return true;
    case Result::BEYOND_SERVER_UPGRADE_THRESHOLD:
      LogErr(ERROR_LEVEL, ER_BEYOND_SERVER_UPGRADE_THRESHOLD, m_source.version,
             m_target.version, m_source.upgrade_threshold);
      return true;
    case Result::INVALID_SERVER_DOWNGRADE_NOT_PATCH:
      LogErr(ERROR_LEVEL, ER_INVALID_SERVER_DOWNGRADE_NOT_PATCH,
             m_source.version, m_target.version);
      return true;
    case Result::NO_PATCH_DOWNGRADE_FOR_INNOVATION_RELEASES:
      LogErr(ERROR_LEVEL, ER_NO_PATCH_DOWNGRADE_FOR_INNOVATION_RELEASES,
             m_source.version, m_target.version, m_target.version);
      return true;
    case Result::BEYOND_SERVER_DOWNGRADE_THRESHOLD:
      LogErr(ERROR_LEVEL, ER_BEYOND_SERVER_DOWNGRADE_THRESHOLD,
             m_source.version, m_target.version, m_source.downgrade_threshold);
      return true;
  }

  return true;
}

}  // namespace dd::bootstrap
