/*
  Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysql/harness/plugin_config.h"

#include <stdexcept>

#ifndef _WIN32
#include <sys/un.h>
#include <unistd.h>
#endif

#include "tcp_address.h"

namespace mysql_harness {

/* static */
std::string BasePluginConfig::get_section_name(
    const mysql_harness::ConfigSection *section) {
  if (section->key.empty()) return section->name;

  return section->name + ":" + section->key;
}

static std::string option_description(const std::string &section_name,
                                      const std::string &option) {
  return "option " + option + " in [" + section_name + "]";
}

std::string BasePluginConfig::get_option_description(
    const mysql_harness::ConfigSection *section,
    const std::string &option) const {
  auto section_name = section->get_section_name(option);
  // if get_section_name can't resolve the section name because the option is
  // unknown, fall back to the current section-name.
  if (section_name.empty()) {
    section_name = section_name_;
  }
  return option_description(section_name, option);
}

std::optional<std::string> BasePluginConfig::get_option_string_(
    const mysql_harness::ConfigSection *section,
    const std::string &option) const {
  std::optional<std::string> value;

  if (is_required(option)) {
    try {
      value = section->get(option);
    } catch (const mysql_harness::bad_option &) {
      throw option_not_present(get_option_description(section, option) +
                               " is required");
    }

    if (value->empty()) {
      throw option_empty(get_option_description(section, option) +
                         " needs a value");
    }

    return value;
  } else {
    try {
      return value = section->get(option);
    } catch (const mysql_harness::bad_option &) {
      return {};
    }
  }
}

std::string BasePluginConfig::get_option_string_or_default_(
    const mysql_harness::ConfigSection *section,
    const std::string &option) const {
  std::optional<std::string> value = get_option_string_(section, option);

  if (value && !value->empty()) return value.value();

  return get_default(option);
}

}  // namespace mysql_harness
