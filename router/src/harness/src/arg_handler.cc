/*
  Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "mysql/harness/arg_handler.h"

#include "utilities.h"

#include <assert.h>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using std::string;
using std::unique_ptr;
using std::vector;

using mysql_harness::utility::regex_pattern_matches;
using mysql_harness::utility::string_format;
using mysql_harness::utility::wrap_string;

#ifndef NDEBUG
bool CmdArgHandler::debug_check_option_names(
    const CmdOption::OptionNames &names) const {
  for (auto &&name : names) {
    if (!is_valid_option_name(name))  // valid option names
      return false;
    if (options_.end() != find_option(name))  // unique name
      return false;
  }
  return true;
}
#endif

void CmdArgHandler::add_option(
    const CmdOption::OptionNames &names, const std::string &description,
    const CmdOptionValueReq &value_req, const std::string &metavar,
    CmdOption::ActionFunc action,
    CmdOption::AtEndActionFunc at_end_action) noexcept {
  assert(!names.empty());  // need none empty names container
  assert(debug_check_option_names(names));
  options_.emplace_back(names, description, value_req, metavar, action,
                        at_end_action);
}

void CmdArgHandler::add_option(const CmdOption &other) noexcept {
  assert(!other.names.empty());  // need none empty names container
  assert(debug_check_option_names(other.names));

  options_.emplace_back(other.names, other.description, other.value_req,
                        other.metavar, other.action, other.at_end_action);
}

OptionContainer::const_iterator CmdArgHandler::find_option(
    const std::string &name) const noexcept {
  for (auto opt = options_.begin(); opt != options_.end(); ++opt) {
    auto res = std::find(opt->names.begin(), opt->names.end(), name);
    if (res != opt->names.end()) {
      return opt;
    }
  }

  return options_.end();
}

/** @fn CmdArgHandler::is_valid_option_name(const string name) noexcept
 *
 * @internal
 * Some compilers, like gcc 4.8, have no support for C++11 regular expression.
 * @endinternal
 */
bool CmdArgHandler::is_valid_option_name(const string &name) const noexcept {
  // Handle tokens like -h or -v
  if (name.size() == 2 && name.at(1) != '-') {
    return name.at(0) == '-';
  }

  // Handle tokens like --help or --with-sauce
  return regex_pattern_matches(name, "^--[A-Za-z][A-Za-z_-]*[A-Za-z]$");
}

void CmdArgHandler::process(const vector<string> &arguments) {
  size_t pos;
  string argpart;
  string value;
  rest_arguments_.clear();
  auto args_end = arguments.end();
  vector<std::pair<CmdOption::ActionFunc, string>> schedule;
  vector<CmdOption::AtEndActionFunc> at_end_schedule;

  for (auto part = arguments.begin(); part < args_end; ++part) {
    if ((pos = (*part).find('=')) != string::npos) {
      // Option like --config=/path/to/config.conf
      argpart = (*part).substr(0, pos);
      value = (*part).substr(pos + 1);
    } else {
      argpart = *part;
      value = "";
    }

    // Save none-option arguments
    if (!is_valid_option_name(argpart)) {
      if (!allow_rest_arguments) {
        throw std::invalid_argument("invalid argument '" + argpart + "'.");
      }
      rest_arguments_.push_back(argpart);
      continue;
    }

    auto opt_iter = find_option(argpart);
    if (options_.end() != opt_iter) {
      auto &option = *opt_iter;
      string err_value_req =
          string_format("option '%s' requires a value.", argpart.c_str());

      if (option.value_req == CmdOptionValueReq::required) {
        if (value.empty()) {
          if (part == (args_end - 1)) {
            // No more parts to get value from
            throw std::invalid_argument(err_value_req);
          }

          ++part;
          if (!part->empty() && part->at(0) == '-') {
            throw std::invalid_argument(err_value_req);
          }
          value = *part;
        }
      } else if (option.value_req == CmdOptionValueReq::optional) {
        if (value.empty() && part != (args_end - 1)) {
          ++part;
          if (part->empty() || part->at(0) != '-') {
            value = *part;
          }
        }
      }

      // Execute actions after
      if (option.action != nullptr) {
        schedule.emplace_back(option.action, value);
        at_end_schedule.push_back(option.at_end_action);
      }
    } else {
      auto message = string_format("unknown option '%s'.", argpart.c_str());
      throw std::invalid_argument(message);
    }
  }

  // Execute actions after processing
  for (auto it : schedule) {
    std::bind(it.first, it.second)();
  }

  // Execute at the end actions
  for (auto at_end_action : at_end_schedule) {
    at_end_action();
  }
}

vector<string> CmdArgHandler::usage_lines(const string &prefix,
                                          const string &rest_metavar,
                                          size_t width) const noexcept {
  std::stringstream ss;
  vector<string> usage;

  for (auto option = options_.begin(); option != options_.end(); ++option) {
    ss.clear();
    ss.str(string());

    ss << "[";
    for (auto name = option->names.begin(); name != option->names.end();
         ++name) {
      ss << *name;
      if (name == --option->names.end()) {
        if (option->value_req != CmdOptionValueReq::none) {
          if (option->value_req == CmdOptionValueReq::optional) {
            ss << "=[";
          } else {
            ss << "=";
          }
          ss << "<" << (option->metavar.empty() ? "VALUE" : option->metavar)
             << ">";
          if (option->value_req == CmdOptionValueReq::optional) {
            ss << "]";
          }
        }
        ss << "]";
      } else {
        ss << "|";
      }
    }
    usage.push_back(ss.str());
  }

  if (allow_rest_arguments && !rest_metavar.empty()) {
    ss.clear();
    ss.str(string());
    ss << "[" << rest_metavar << "]";
    usage.push_back(ss.str());
  }

  ss.clear();
  ss.str(string());
  size_t line_size = 0;
  vector<string> result{};

  ss << prefix;
  line_size = ss.str().size();
  auto indent = string(line_size, ' ');

  auto end_usage = usage.end();
  for (auto item = usage.begin(); item != end_usage; ++item) {
    // option can not be bigger than width
    assert(((*item).size() + indent.size()) < width);
    auto need_newline = (line_size + (*item).size() + indent.size()) > width;

    if (need_newline) {
      result.push_back(ss.str());
      ss.clear();
      ss.str(string());
      ss << indent;
    }

    ss << " " << *item;
    line_size = ss.str().size();
  }

  // Add the last line
  result.push_back(ss.str());

  return result;
}

vector<string> CmdArgHandler::option_descriptions(size_t width,
                                                  size_t indent) noexcept {
  std::stringstream ss;
  vector<string> desc_lines;

  for (auto option = options_.begin(); option != options_.end(); ++option) {
    auto value_req = option->value_req;
    ss.clear();
    ss.str(string());

    ss << "  ";
    for (auto iter_name = option->names.begin();
         iter_name != option->names.end(); ++iter_name) {
      auto name = *iter_name;
      ss << name;

      if (value_req != CmdOptionValueReq::none) {
        if (value_req == CmdOptionValueReq::optional) {
          ss << " [";
        }
        ss << " <" << (option->metavar.empty() ? "VALUE" : option->metavar);
        ss << ">";
        if (value_req == CmdOptionValueReq::optional) {
          ss << "]";
        }
      }

      if (iter_name != --option->names.end()) {
        ss << ", ";
      }
    }
    desc_lines.push_back(ss.str());

    ss.clear();
    ss.str(string());

    string desc = option->description;
    for (auto line : wrap_string(option->description, width, indent)) {
      desc_lines.push_back(line);
    }
  }

  return desc_lines;
}
