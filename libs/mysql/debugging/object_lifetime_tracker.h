// Copyright (c) 2025, 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#ifndef MYSQL_DEBUGGING_OBJECT_LIFETIME_TRACKER_H
#define MYSQL_DEBUGGING_OBJECT_LIFETIME_TRACKER_H

/// @file
/// Experimental API header

#include <atomic>       // atomic
#include <iostream>     // cout
#include <string_view>  // string_view

/// @addtogroup GroupLibsMysqlDebugging
/// @{

namespace mysql::debugging {

/// Integral type used to uniquely identify objects.
using Tracker_id = int;

/// The value to use for m_id next time we construct an Allocator.
///
/// Not a member variable of Object_lifetime_tracker because we need one global
/// instance, not one per specialization.
///
/// @tparam channel Identifies a unique sequence of object ids.
template <int channel = 0>
[[nodiscard]] Tracker_id tracker_get_object_id() {
  static std::atomic<Tracker_id> tracker_id_counter = 0;
  Tracker_id ret = tracker_id_counter.fetch_add(1);
  return ret;
}

/// Return the name of the given type (not demangled).
template <class Type>
[[nodiscard]] std::string_view type_name() {
  return typeid(Type).name();
}

/// Debug facility to log constructor/assignment/destructor usage for a class.
///
/// To make a class log such life cycle events, make it inherit from this class.
///
/// This is only for temporary use in debugging sessions and must never be used
/// in released code (not even debug-only code). It is intended to aid in
/// debugging memory related issues, such as objects used after destruction. It
/// writes a line to stdout for every object life cycle event: to keep the
/// output size manageable it is usually good to minimize the amount of
/// problematic code before using this class.
///
/// @tparam Self_tp If given, log entries for constructor invocations will be
/// annotated with the type name of this class.
template <class Self_tp = void>
class Object_lifetime_tracker {
  // True if the type of the subclass was specified as template argument.
  static constexpr bool known_type = !std::same_as<Self_tp, void>;

  // Use ANSI colors for a more readable log.
  static constexpr const char *m_reset = "\033[30m";
  static constexpr const char *m_red = "\033[31m";
  static constexpr const char *m_green = "\033[32m";
  static constexpr const char *m_yellow = "\033[33m";
  static constexpr const char *m_blue = "\033[34m";
  static constexpr const char *m_magenta = "\033[35m";
  static constexpr const char *m_cyan = "\033[36m";
  static constexpr const char *grey = "\033[37m";

 public:
  /// Default constructor.
  Object_lifetime_tracker() noexcept : m_id(tracker_get_object_id()) {
    log_construct("Default");
  }

  /// Construct using the message "FLAVOR-construct".
  ///
  /// This is usable in custom subclass constructors.
  explicit Object_lifetime_tracker(std::string_view flavor) noexcept
      : m_id(tracker_get_object_id()) {
    log_construct(flavor);
  }

  /// Construct using the message "FLAVOR-construct from SOURCE".
  ///
  /// This is usable in custom subclass constructors.
  explicit Object_lifetime_tracker(std::string_view flavor,
                                   Tracker_id source) noexcept
      : m_id(tracker_get_object_id()) {
    log_construct_from(flavor, source);
  }

  /// Copy-construct using the message "Copy-construct from ID".
  ///
  /// This will be called when the subclass is copy-constructed, unless the
  /// subclass copy constructor explicitly invokes another constructor in this
  /// class.
  Object_lifetime_tracker(const Object_lifetime_tracker &other) noexcept
      : m_id(tracker_get_object_id()) {
    log_construct_from("Copy", other.m_id);
  }

  /// Copy-construct using the message "Move-construct from ID".
  ///
  /// This will be called when the subclass is move-constructed, unless the
  /// subclass move constructor explicitly invokes another constructor in this
  /// class.
  Object_lifetime_tracker(Object_lifetime_tracker &&other) noexcept
      : m_id(tracker_get_object_id()) {
    log_construct_from("Move", other.m_id);
    log_move(other.m_id);
  }

  /// Copy-construct using the message "Copy-assign from ID".
  ///
  /// This will be called when the subclass is copy-assigned to.
  // NOLINTNEXTLINE(cert-oop54-cpp)
  Object_lifetime_tracker &operator=(
      const Object_lifetime_tracker &other) noexcept {
    log_assign("Copy", other.m_id);
    return *this;
  }

  /// Move-construct using the message "Move-assign from ID".
  ///
  /// This will be called when the subclass is move-assigned to.
  Object_lifetime_tracker &operator=(Object_lifetime_tracker &&other) noexcept {
    log_assign("Move", other.m_id);
    log_move(other.m_id);
    return *this;
  }

  /// Destruct using the message "Destruct".
  ~Object_lifetime_tracker() noexcept { log(m_red, "Destruct ", m_reset); }

  /// Write a message to the log.
  ///
  /// The parameters will be passed to a std::stringstream using operator<<.
  void log(const auto &...args) const { log_for(m_id, args...); }

  /// Write a message to the log, on behalf of another object having the given
  /// ID.
  ///
  /// The parameters will be passed to a std::stringstream using operator<<.
  void log_for(const auto &id, const auto &...args) const {
    do_log(id, ": ", args..., "\n");
  }

  /// Return the ID for this object.
  [[nodiscard]] Tracker_id tracker_id() const { return m_id; }

  /// Return the type of this object (not demangled).
  [[nodiscard]] std::string_view type_name() const {
    if constexpr (known_type) {
      return type_name<Self_tp>();
    } else {
      return "?";
    }
  }

 private:
  /// Write a message during object construction.
  void log_construct(std::string_view flavor) const {
    if constexpr (known_type) {
      log(m_green, flavor, "-construct", m_reset, " (", this->type_name(), ")");
    } else {
      log(m_green, flavor, "-construct", m_reset);
    }
  }

  /// Write a message during object construction, including a " from SOURCE"
  /// text.
  void log_construct_from(std::string_view flavor, Tracker_id source) const {
    if constexpr (known_type) {
      log(m_green, flavor, "-construct", m_reset, " from ", source, " (",
          this->type_name(), ")");
    } else {
      log(m_green, flavor, "-construct", m_reset, " from ", source);
    }
  }

  /// Write a message during object assignment.
  void log_assign(std::string_view flavor, Tracker_id source) const {
    log(m_cyan, flavor, "-assign", m_reset, " from ", source);
  }

  /// Write a message on behalf of a moved-from object.
  void log_move(Tracker_id id) const {
    log_for(id, m_magenta, "Move-from", m_reset);
  }

  /// Low level to log any message.
  void do_log(const auto &...args) const {
    (std::cout << ... << args);
    std::cout.flush();
  }

  // Integer representing object "identity". Each object has a distinct value
  // for m_id.
  Tracker_id m_id;
};  // class Object_lifetime_tracker

}  // namespace mysql::debugging

// addtogroup GroupLibsMysqlDebugging
/// @}

#endif  // ifndef MYSQL_DEBUGGING_OBJECT_LIFETIME_TRACKER_H
