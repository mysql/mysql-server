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

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "mysql/harness/stdx/expected.h"
#include "sql_value.h"

/**
 * execution context for SQL.
 *
 * - system-variables
 * - diagnostics area
 */
class ExecutionContext {
 public:
  /**
   * system-variables as returned by the server.
   *
   * can be queried from the server with:
   *
   * - SELECT @@SESSION.{k}
   * - SELECT @@LOCAL.{k}
   *
   * can be set on the server with:
   *
   * - SET k = v;
   * - SET @@SESSION.k = v;
   * - SET @@LOCAL.k = v;
   * - SET SESSION k = v;
   * - SET LOCAL k = v;
   *
   * changes to system-vars on the server are returned via
   * the sesssion-tracker for system-variables.
   */
  class SystemVariables {
   public:
    using key_type = std::string;
    using key_view_type = std::string_view;
    using value_type = Value;  // aka std::optional<std::string>

    /**
     * set k to v.
     *
     * if k doesn't exist in the system-vars yet, it gets inserted.
     */
    void set(key_type k, value_type v) {
      vars_.insert_or_assign(std::move(k), std::move(v));
    }

    /**
     * find 'k' in sytem-vars.
     *
     * @param k key
     *
     * if 'k' does not exist in system-vars, a NULL-like value is returned.
     * otherwise return the value for the system-var referenced by 'k'
     *
     * @returns std::nullopt if key is not found, the found value otherwise.
     */
    std::optional<value_type> find(const key_view_type &k) const {
      const auto it = vars_.find(k);
      if (it == vars_.end()) return {std::nullopt};

      return it->second;
    }

    /**
     * get 'k' from system-vars.
     *
     * @param k key
     *
     * if 'k' does not exist in system-vars, a NULL-like value is returned.
     * otherwise return the value for the system-var referenced by 'k' which may
     * be NULL-like or a string.
     *
     * @returns std::nullopt if key is not found or value is NULL-like, the
     * found value otherwise
     */
    value_type get(const key_view_type &k) const {
      const auto it = vars_.find(k);
      if (it == vars_.end()) return {std::nullopt};

      return it->second;
    }

    using iterator = std::map<key_type, value_type>::iterator;
    using const_iterator = std::map<key_type, value_type>::const_iterator;

    iterator begin() { return vars_.begin(); }
    const_iterator begin() const { return vars_.begin(); }
    iterator end() { return vars_.end(); }
    const_iterator end() const { return vars_.end(); }

    /**
     * check if their is a no system-var.
     */
    bool empty() const { return vars_.empty(); }

    /**
     * clear the system-vars.
     */
    void clear() { vars_.clear(); }

   private:
    std::map<key_type, value_type, std::less<>> vars_;
  };

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

  SystemVariables &system_variables() { return system_variables_; }

  const SystemVariables &system_variables() const { return system_variables_; }

 private:
  SystemVariables system_variables_;

  DiagnosticsArea diagnostics_area_;
};

#endif
