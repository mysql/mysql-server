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
  rest_arguments_.clear();

  vector<std::pair<CmdOption::ActionFunc, string>> schedule;
  vector<CmdOption::AtEndActionFunc> at_end_schedule;

  const auto args_end = arguments.end();
  for (auto part = arguments.begin(); part < args_end; ++part) {
    string argpart;
    string value;
    bool got_value{false};

    size_t pos;
    if ((pos = (*part).find('=')) != string::npos) {
      // Option like --config=/path/to/config.conf
      argpart = (*part).substr(0, pos);
      value = (*part).substr(pos + 1);
      got_value = true;
    } else {
      argpart = *part;
    }

    // Save none-option arguments
    if (!is_valid_option_name(argpart)) {
      if (!allow_rest_arguments) {
        throw std::invalid_argument("invalid argument '" + *part + "'.");
      }
      rest_arguments_.push_back(*part);
      continue;
    }

    const auto opt_iter = find_option(argpart);
    if (opt_iter == options_.end()) {
      throw std::invalid_argument("unknown option '" + argpart + "'.");
    }
    const auto &option = *opt_iter;

    switch (option.value_req) {
      case CmdOptionValueReq::required:
      case CmdOptionValueReq::optional:
        if (!got_value) {
          // no value provided after =, check next arg

          auto next_part_it = std::next(part);
          if (option.value_req == CmdOptionValueReq::required) {
            if (next_part_it == args_end) {
              throw std::invalid_argument("option '" + argpart +
                                          "' expects a value, got nothing");
            } else if (next_part_it->empty()) {
              // accept and ignore
              ++part;
            } else if (next_part_it->at(0) == '-') {
              throw std::invalid_argument("option '" + argpart +
                                          "' expects a value, got nothing");
            } else {
              // accept
              value = *next_part_it;
              ++part;
            }
          } else {
            // optional
            if (next_part_it == args_end) {
              // ok
            } else if (next_part_it->empty()) {
              // accept and ignore
              ++part;
            } else if (next_part_it->at(0) == '-') {
              // skip
            } else {
              value = *next_part_it;
              ++part;
            }
          }
        } else if (option.value_req == CmdOptionValueReq::required) {
          // even empty value is ok
        }
        break;
      case CmdOptionValueReq::none:
        if (!value.empty()) {
          throw std::invalid_argument(
              "option '" + argpart +
              "' does not expect a value, but got a value");
        }
        break;
      default:
        throw std::invalid_argument(
            "unsupported req: " +
            std::to_string(static_cast<int>(option.value_req)));
    }

    // Execute actions after
    if (option.action != nullptr) {
      schedule.emplace_back(option.action, value);
      at_end_schedule.push_back(option.at_end_action);
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

vector<string> CmdArgHandler::usage_lines_if(const string &prefix,
                                             const string &rest_metavar,
                                             size_t width,
                                             UsagePredicate predicate) const
    noexcept {
  std::stringstream ss;
  vector<string> usage;

  for (auto option : options_) {
    bool accepted;

    std::tie(accepted, option) = predicate(option);

    if (!accepted) continue;

    ss.clear();
    ss.str(string());

    bool has_multiple_names = option.names.size() > 1;

    if (!option.required) {
      ss << "[";
    } else if (has_multiple_names) {
      ss << "(";
    }
    {
      auto name_it = option.names.begin();
      ss << *name_it;
      for (++name_it; name_it != option.names.end(); ++name_it) {
        ss << "|" << *name_it;
      }
    }
    if (option.value_req != CmdOptionValueReq::none) {
      if (option.value_req == CmdOptionValueReq::optional) {
        ss << "=[";
      } else {
        ss << "=";
      }
      ss << "<" << (option.metavar.empty() ? "VALUE" : option.metavar) << ">";
      if (option.value_req == CmdOptionValueReq::optional) {
        ss << "]";
      }
    }
    if (!option.required) {
      ss << "]";
    } else if (has_multiple_names) {
      ss << ")";
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
                                                  size_t indent) const
    noexcept {
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
