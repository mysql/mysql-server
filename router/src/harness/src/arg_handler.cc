/*
  Copyright (c) 2015, 2023, Oracle and/or its affiliates.

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

#include <algorithm>
#include <cassert>
#include <sstream>
#include <string>
#include <vector>

#include "mysql/harness/utility/string.h"  // wrap_string
#include "utilities.h"                     // regex_pattern_matches

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
bool CmdArgHandler::is_valid_option_name(
    const std::string &name) const noexcept {
  // Handle tokens like -h or -v
  if (name.size() == 2 && name.at(1) != '-') {
    return name.at(0) == '-';
  }

  // Handle tokens like --help or --with-sauce
  return regex_pattern_matches(
      name, "^--[A-Za-z][0-9A-Za-z._-]*(:[0-9A-Za-z._-]*)?[0-9A-Za-z]$");
}

namespace {
bool is_valid_option_value(const std::string &value) {
  return value.find_first_of("\n") == std::string::npos;
}
}  // namespace

void CmdArgHandler::process(const std::vector<std::string> &arguments) {
  rest_arguments_.clear();

  std::vector<std::pair<CmdOption::ActionFunc, std::string>> schedule;
  std::vector<std::pair<CmdOption::AtEndActionFunc, std::string>>
      at_end_schedule;

  const auto args_end = arguments.end();
  for (auto part = arguments.begin(); part < args_end; ++part) {
    std::string argpart;
    std::string value;
    bool got_value{false};

    size_t pos;
    if ((pos = (*part).find('=')) != std::string::npos) {
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

    const auto dot_pos = argpart.find_first_of('.');
    if (dot_pos != std::string::npos) {
      if (!got_value) {
        auto next_part_it = std::next(part);
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
      }

      if (!is_valid_option_value(value)) {
        throw std::invalid_argument("invalid value '" + value +
                                    "' for option '" + argpart + "'");
      }

      std::string section_str = argpart.substr(2, dot_pos - 2);  // skip "--"
      // split A:B into pair<A,B> or A into pair<A,"">
      std::pair<std::string, std::string> section_id;
      const auto colon_pos = section_str.find_first_of(':');

      if (colon_pos != std::string::npos) {
        section_id.first = section_str.substr(0, colon_pos);
        section_id.second = section_str.substr(colon_pos + 1);
      } else {
        section_id.first = section_str;
      }

      std::transform(section_id.first.begin(), section_id.first.end(),
                     section_id.first.begin(), ::tolower);

      if (section_id.first == "default") {
        std::transform(section_id.first.begin(), section_id.first.end(),
                       section_id.first.begin(), ::toupper);
      }

      const std::string arg_key = argpart.substr(dot_pos + 1);
      auto &section_overwrites = config_overwrites_[section_id];
      section_overwrites[arg_key] = value;
      continue;
    }

    const auto opt_iter = find_option(argpart);
    if (opt_iter == options_.end()) {
      if (!ignore_unknown_arguments)
        throw std::invalid_argument("unknown option '" + argpart + "'.");
    }

    // if ignore_unknown_arguments is true we use this no-op handler when we see
    // one; this helps keeping the below code simple (skipping  '--', skipping
    // option arg, etc. common for both known and ignored arguments)
    CmdOption ignored_option(std::vector<std::string>{}, "",
                             CmdOptionValueReq::optional, "",
                             [](const std::string &) {});

    const auto &option =
        opt_iter == options_.end() ? ignored_option : *opt_iter;

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
      at_end_schedule.emplace_back(option.at_end_action, value);
    }
  }

  // Execute actions after processing
  for (const auto &pr : schedule) {
    pr.first(pr.second);
  }

  // Execute at the end actions
  for (const auto &pr : at_end_schedule) {
    pr.first(pr.second);
  }
}

std::vector<std::string> CmdArgHandler::usage_lines_if(
    const std::string &prefix, const std::string &rest_metavar, size_t width,
    UsagePredicate predicate) const noexcept {
  std::stringstream ss;
  std::vector<std::string> usage;

  for (auto option : options_) {
    bool accepted;

    std::tie(accepted, option) = predicate(option);

    if (!accepted) continue;

    ss.clear();
    ss.str({});

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
    ss.str({});
    ss << "[" << rest_metavar << "]";
    usage.push_back(ss.str());
  }

  ss.clear();
  ss.str({});
  size_t line_size = 0;
  std::vector<std::string> result{};

  ss << prefix;
  line_size = ss.str().size();
  auto indent = std::string(line_size, ' ');

  auto end_usage = usage.end();
  for (auto item = usage.begin(); item != end_usage; ++item) {
    // option can not be bigger than width
    assert(((*item).size() + indent.size()) < width);
    auto need_newline = (line_size + (*item).size() + indent.size()) > width;

    if (need_newline) {
      result.push_back(ss.str());
      ss.clear();
      ss.str({});
      ss << indent;
    }

    ss << " " << *item;
    line_size = ss.str().size();
  }

  // Add the last line
  result.push_back(ss.str());

  return result;
}

std::vector<std::string> CmdArgHandler::option_descriptions(
    size_t width, size_t indent) const noexcept {
  std::stringstream ss;
  std::vector<std::string> desc_lines;

  for (auto option = options_.begin(); option != options_.end(); ++option) {
    auto value_req = option->value_req;
    ss.clear();
    ss.str({});

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
    ss.str({});

    std::string desc = option->description;
    for (auto line : wrap_string(option->description, width, indent)) {
      desc_lines.push_back(line);
    }
  }

  return desc_lines;
}
