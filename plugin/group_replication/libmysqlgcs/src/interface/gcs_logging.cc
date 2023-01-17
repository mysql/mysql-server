/* Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#include <algorithm>
#include <cassert>
#include <sstream>
#include <string>

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_logging.h"

#define SIZE_DEBUG_OPTIONS \
  sizeof(gcs_xcom_debug_strings) / sizeof(*gcs_xcom_debug_strings)

Logger_interface *Gcs_log_manager::m_logger = nullptr;

// Logging infrastructure interface
Logger_interface *Gcs_log_manager::get_logger() { return m_logger; }

enum_gcs_error Gcs_log_manager::initialize(Logger_interface *logger) {
  m_logger = logger;
  return m_logger->initialize();
}

enum_gcs_error Gcs_log_manager::finalize() {
  enum_gcs_error ret = GCS_NOK;

  if (m_logger != nullptr) {
    ret = m_logger->finalize();
    m_logger = nullptr;
  }

  return ret;
}

std::atomic<std::int64_t> Gcs_debug_options::m_debug_options{GCS_DEBUG_NONE};

const std::string Gcs_debug_options::m_debug_none(
    gcs_xcom_debug_strings[SIZE_DEBUG_OPTIONS - 1]);

const std::string Gcs_debug_options::m_debug_all(
    gcs_xcom_debug_strings[SIZE_DEBUG_OPTIONS - 2]);

int64_t Gcs_debug_options::get_valid_debug_options() {
  int64_t ret = 0;
  unsigned int num_options = get_number_debug_options();

  unsigned int i = 0;
  for (i = 0; i < num_options; i++) {
    ret = ret | (static_cast<int64_t>(1) << i);
  }

  return ret;
}

bool Gcs_debug_options::is_valid_debug_options(const int64_t debug_options) {
  if (debug_options == GCS_DEBUG_NONE || debug_options == GCS_DEBUG_ALL)
    return true;

  return !((debug_options & (~get_valid_debug_options())));
}

bool Gcs_debug_options::is_valid_debug_options(
    const std::string &debug_options) {
  int64_t res_debug_options;
  return !get_debug_options(debug_options, res_debug_options);
}

int64_t Gcs_debug_options::get_current_debug_options() {
  return load_debug_options();
}

int64_t Gcs_debug_options::get_current_debug_options(
    std::string &res_debug_options) {
  int64_t debug_options = load_debug_options();
  get_debug_options(debug_options, res_debug_options);
  return debug_options;
}

bool Gcs_debug_options::get_debug_options(const int64_t debug_options,
                                          std::string &res_debug_options) {
  unsigned int i = 0;
  unsigned int num_options = get_number_debug_options();

  /*
    There are options that are not valid here so an error is returned.
  */
  if (!is_valid_debug_options(debug_options))
    return true; /* purecov: inspected */

  res_debug_options.clear();

  if (debug_options == GCS_DEBUG_NONE) {
    res_debug_options += m_debug_none;
    return debug_options;
  }

  if (debug_options == GCS_DEBUG_ALL) {
    res_debug_options += m_debug_all;
    return debug_options;
  }

  for (i = 0; i < num_options; i++) {
    if ((debug_options & (static_cast<int64_t>(1) << i))) {
      res_debug_options += gcs_xcom_debug_strings[i];
      res_debug_options += ",";
    }
  }

  res_debug_options.erase(res_debug_options.length() - 1);

  return false;
}

bool Gcs_debug_options::get_debug_options(const std::string &debug_options,
                                          int64_t &res_debug_options) {
  bool found;
  bool match = false;
  unsigned int i;
  unsigned int num_options = get_number_debug_options();

  res_debug_options = GCS_DEBUG_NONE;

  std::stringstream it(debug_options);
  std::string option;

  while (std::getline(it, option, ',')) {
    /*
       Remove blank spaces and convert the string to upper case.
    */
    option.erase(std::remove(option.begin(), option.end(), ' '), option.end());
    std::transform(option.begin(), option.end(), option.begin(), ::toupper);

    if (!option.compare(m_debug_all)) {
      res_debug_options = GCS_DEBUG_ALL;
      match = true;
      continue;
    }

    /*
      Check if the parameter option matches a valid option but if it does
      not match an error is returned.
    */
    found = false;
    for (i = 0; i < num_options; i++) {
      if (!option.compare(gcs_xcom_debug_strings[i])) {
        res_debug_options = res_debug_options | (static_cast<int64_t>(1) << i);
        found = true;
        break;
      }
    }

    match |= found;

    if (!found && option.compare("") && option.compare(m_debug_none))
      return true;
  }

  if (!match && (debug_options.find(",") != std::string::npos)) return true;

  return false;
}

unsigned int Gcs_debug_options::get_number_debug_options() {
  return (SIZE_DEBUG_OPTIONS - 2);
}

bool Gcs_debug_options::set_debug_options(const int64_t debug_options) {
  /*
    There are options that are not valid here so an error is returned.
  */
  if (!is_valid_debug_options(debug_options)) return true;

  /*
    Note that we execute this change in two steps. This is not a problem
    since we only want to guarantee that there will be no corrupted
    values when there is concurrency.
  */
  store_debug_options(load_debug_options() | debug_options);

  return false;
}

bool Gcs_debug_options::force_debug_options(const int64_t debug_options) {
  if (!is_valid_debug_options(debug_options)) return true;

  store_debug_options(debug_options);

  return false;
}

bool Gcs_debug_options::set_debug_options(const std::string &debug_options) {
  bool ret;
  int64_t res_debug_options;

  ret = get_debug_options(debug_options, res_debug_options);
  return ret ? ret : set_debug_options(res_debug_options);
}

bool Gcs_debug_options::force_debug_options(const std::string &debug_options) {
  bool ret;
  int64_t res_debug_options;

  ret = get_debug_options(debug_options, res_debug_options);
  return ret ? ret : force_debug_options(res_debug_options);
}

bool Gcs_debug_options::unset_debug_options(const int64_t debug_options) {
  /*
    There are options that are not valid here so an error is returned.
  */
  if (!is_valid_debug_options(debug_options)) return true;

  /*
    Note that we execute this change in two steps. This is not a problem
    since we only want to guarantee that there will be no corrupted
    values when there is concurrency.
  */
  store_debug_options(load_debug_options() & (~debug_options));

  return false;
}

bool Gcs_debug_options::unset_debug_options(const std::string &debug_options) {
  bool ret;
  int64_t res_debug_options;

  ret = get_debug_options(debug_options, res_debug_options);
  return ret ? ret : unset_debug_options(res_debug_options);
}
