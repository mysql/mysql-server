/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTING_SQL_EXEC_CONTEXT_INCLUDED
#define ROUTING_SQL_EXEC_CONTEXT_INCLUDED

#include <string>
#include <vector>

#include "mysql/harness/stdx/expected.h"

/**
 * execution context for SQL.
 *
 * - diagnostics area
 */
class ExecutionContext {
 public:
  /**
   * diagnostics area.
   *
   * - warnings, errors and notes.
   *
   * used by:
   *
   * - SHOW WARNINGS
   * - SHOW ERRORS
   * - SHOW COUNT(*) WARNINGS
   * - SHOW COUNT(*) ERRORS
   * - SELECT @@warning_count
   * - SELECT @@error_count
   */
  class DiagnosticsArea {
   public:
    class Warning {
     public:
      Warning(std::string level, uint64_t code, std::string msg)
          : level_(std::move(level)), code_(code), msg_{std::move(msg)} {}

      std::string level() const { return level_; }
      uint64_t code() const { return code_; }
      std::string message() const { return msg_; }

     private:
      std::string level_;
      uint64_t code_;
      std::string msg_;
    };

    std::vector<Warning> &warnings() { return warnings_; }
    const std::vector<Warning> &warnings() const { return warnings_; }

   private:
    std::vector<Warning> warnings_;
  };

  DiagnosticsArea &diagnostics_area() { return diagnostics_area_; }

  const DiagnosticsArea &diagnostics_area() const { return diagnostics_area_; }

 private:
  DiagnosticsArea diagnostics_area_;
};

#endif
