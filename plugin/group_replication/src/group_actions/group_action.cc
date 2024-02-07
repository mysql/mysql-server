/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#include "plugin/group_replication/include/group_actions/group_action.h"

Group_action_diagnostics::Group_action_diagnostics()
    : message_level(GROUP_ACTION_LOG_END),
      log_message(""),
      warning_message("") {}

void Group_action_diagnostics::set_execution_info(
    Group_action_diagnostics *diagnostics) {
  message_level = diagnostics->get_execution_message_level();
  log_message.assign(diagnostics->get_execution_message());
  warning_message.assign(diagnostics->get_warning_message());
}

void Group_action_diagnostics::set_execution_message_level(
    enum_action_result_level level) {
  message_level = level;
}

void Group_action_diagnostics::set_execution_message(
    enum_action_result_level level, std::string &message) {
  assert(level != GROUP_ACTION_LOG_INFO || warning_message.empty());
  log_message.assign(message);
  message_level = level;
}

void Group_action_diagnostics::set_execution_message(
    enum_action_result_level level, const char *message) {
  assert(level != GROUP_ACTION_LOG_INFO || warning_message.empty());
  log_message.assign(message);
  message_level = level;
}

void Group_action_diagnostics::append_execution_message(const char *message) {
  log_message.append(message);
}

void Group_action_diagnostics::append_execution_message(std::string &message) {
  log_message.append(message);
}

void Group_action_diagnostics::set_warning_message(const char *warning_msg) {
  warning_message.assign(warning_msg);
}

void Group_action_diagnostics::append_warning_message(const char *warning_msg) {
  warning_message.append(warning_msg);
}

std::string &Group_action_diagnostics::get_execution_message() {
  return log_message;
}

std::string &Group_action_diagnostics::get_warning_message() {
  return warning_message;
}

Group_action_diagnostics::enum_action_result_level
Group_action_diagnostics::get_execution_message_level() {
  return message_level;
}

bool Group_action_diagnostics::has_warning() {
  return !warning_message.empty();
}

void Group_action_diagnostics::clear_info() {
  message_level = GROUP_ACTION_LOG_END;
  log_message.clear();
  warning_message.clear();
}

Group_action::~Group_action() = default;

PSI_stage_key Group_action::get_action_stage_termination_key() {
  return -1; /* purecov: inspected */
}
