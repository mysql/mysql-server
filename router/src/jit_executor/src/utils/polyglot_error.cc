/*
 * Copyright (c) 2024, 2026, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "utils/polyglot_error.h"

#include <limits>
#include <numeric>
#include <optional>
#include <regex>
#include <sstream>
#include <vector>

#include "utils/polyglot_api_clean.h"

#include "native_wrappers/polyglot_collectable.h"
#include "native_wrappers/polyglot_map_wrapper.h"
#include "utils/utils_string.h"

namespace shcore {
namespace polyglot {

namespace {
inline const char *k_key_type{"type"};
inline const char *k_key_backtrace{"backtrace"};
inline const char *k_key_source{"source"};
inline const char *k_key_line{"line"};
inline const char *k_key_column{"column"};
inline const char *k_key_source_line{"source_line"};
inline const char *k_key_code{"code"};

/**
 * The backtrace may come from different sources:
 * - The shell itself (internal code, user code, processed scripts)
 * - graalvm - non JavaScript specific
 * - graalvm - JavaScript specific
 *
 * This function will filter out the backtrace to only display the relevant
 * elements to the user:
 *
 * - Excludes graalvm frames that are not JavaScript specific
 * - Excludes shell internal frames
 */
std::vector<std::string> filter_backtrace(const std::string &backtrace,
                                          std::string *header = nullptr,
                                          int *column = nullptr,
                                          std::string *src_line = nullptr) {
  std::vector<std::string> filtered;
  assert(!column || src_line);

  // A backtrace frame comes in the following format:
  // <call>(<source>:<line>:<start-end>)\n
  // Where:
  // - <call> is the function call at the given frame, examples:
  //   - module.function
  //   - function
  //   - :program (for an instruction given by the user in the shell prompt)
  // - <source> is the name of the file containing the <call> or any of the
  // shell sources:
  //   - (internal): for internal code
  //   - (shell): for code provided by the user in the shell prompt.
  // <line>: the 1 based line number containing the code in the given <source>
  // <start>: first byte of the sentence that generated the error (using
  // <source> as reference) <end>: last byte of the sentence that generated the
  // error
  //
  // This expression matches the sequence capturing the content between
  // parenthesis:
  //     <source>:<line>:<start>
  // Which is the only information included in the resulting backtrace
  static const std::regex bt_source_location_message(
      R"*(<[a-z]+> (.*?)\((.+:\d+):.*\).*)*",
      std::regex::icase | std::regex::optimize);

  std::vector<std::string> lines;
  std::istringstream stream(backtrace);
  for (std::string line; std::getline(stream, line);) {
    lines.push_back(std::move(line));
  }

  auto it = lines.begin();
  if (header) {
    (*header) = std::move(*it);
    ++it;
  }

  std::vector<std::string>::iterator last_filtered_out;
  for (; it != lines.end(); ++it) {
    std::cmatch match;
    if (std::regex_match((*it).c_str(), (*it).c_str() + (*it).size(), match,
                         bt_source_location_message)) {
      // Excludes (internal) frames
      auto source = match[2].str();
      if (!source.starts_with("(internal)")) {
        filtered.push_back(match[1].str() + "(" + source + ")");
      }
    } else if (column && (*it).back() == '^') {
      // The column information is not available on the backtrace frames,
      // however, in some cases, a last line including the ^ symbol is included
      // and prepended with the number of spaces that would represent the column
      *src_line = *last_filtered_out;
      *column = (*it).size() - 1;
    } else {
      last_filtered_out = it;
    }
  }

  return filtered;
}

std::string get_poly_exception_message(
    poly_thread thread, poly_exception exc, int *column = nullptr,
    std::string *src_line = nullptr,
    std::vector<std::string> *backtrace = nullptr) {
  size_t length{0};
  std::string message;

  if (poly_ok == poly_exception_get_message(thread, exc, nullptr, 0, &length)) {
    message.resize(length++);

    throw_if_error(poly_exception_get_message, thread, exc, &message[0], length,
                   &length);

    if (message.find('\n') != std::string::npos) {
      auto local_backtrace =
          filter_backtrace(message, &message, column, src_line);
      if (backtrace && !local_backtrace.empty()) {
        *backtrace = std::move(local_backtrace);
      }
    }
  }

  return message;
}

std::vector<std::string> get_exception_stack_trace(poly_thread thread,
                                                   poly_exception exc) {
  size_t length{0};

  if (poly_ok ==
      poly_exception_get_guest_stack_trace(thread, exc, nullptr, 0, &length)) {
    std::string message;
    message.resize(length++);

    if (poly_ok == poly_exception_get_guest_stack_trace(
                       thread, exc, &message[0], length, &length)) {
      return filter_backtrace(message);
    }
  }

  return {};
}

}  // namespace

void Polyglot_error::initialize(poly_thread thread) {
  const poly_extended_error_info *err_info = nullptr;
  poly_get_last_error_info(thread, &err_info);

  // This is not expected to happen, but it is always a possibility so we handle
  // the case
  if (!err_info) {
    throw Polyglot_generic_error(
        "generic error occurred in the polyglot library");
  }

  set_message(err_info->error_message);
}

Polyglot_error::Polyglot_error(poly_thread thread, int64_t rc) {
  if (rc == poly_pending_exception) {
    poly_exception exc;
    if (poly_ok == poly_get_last_exception(thread, &exc)) {
      initialize(thread, exc);
    } else {
      throw Polyglot_generic_error(
          "Error retrieving last exception in the polyglot library.");
    }
  } else {
    initialize(thread);
  }
}

Polyglot_error::Polyglot_error(poly_thread thread, poly_exception exc) {
  initialize(thread, exc);
}

void Polyglot_error::initialize(poly_thread thread, poly_exception exc) {
  if (bool result{false};
      poly_ok == poly_exception_is_resource_exhausted(thread, exc, &result) &&
      result) {
    m_resource_exhausted = true;
    return;
  }

  if (bool result{false};
      poly_ok == poly_exception_is_interrupted(thread, exc, &result) &&
      result) {
    m_interrupted = true;
    return;
  }

  int column = -1;
  std::string src_line;
  auto msg =
      get_poly_exception_message(thread, exc, &column, &src_line, &m_backtrace);

  if (column != -1) {
    m_column = column;
    m_source_line = src_line;
  }

  // Retrieve the exception data coming in the error message
  parse_and_translate(msg);

  // Sometimes, backtrace comes with the error message, in other cases it
  // comes apart, we need to handle both cases
  if (m_backtrace.empty()) {
    m_backtrace = get_exception_stack_trace(thread, exc);
  }

  // If the initial message did not come with line (and source??) we locate
  // them on the first backtrace line
  if (!m_line.has_value() && !m_backtrace.empty()) {
    parse_and_translate(m_backtrace[0]);

    // If the first line provided the location, it gets deleted
    if (m_line.has_value()) {
      m_backtrace.erase(m_backtrace.begin());
    }
  }

  if (message().empty()) {
    set_message(msg);
  }

  // If an exception object is found, this would be the real initial
  // exception, so we override whatever data contained in it

  if (bool has_object = false;
      poly_ok == poly_exception_has_object(thread, exc, &has_object) &&
      has_object) {
    poly_value error_obj = nullptr;

    if (poly_ok != poly_exception_get_object(thread, exc, &error_obj)) {
      throw Polyglot_generic_error(
          "Error trying to retrieve an exception object");
    }

    poly_value error_cause = nullptr;
    if (std::string class_name;
        is_object(thread, error_obj, &class_name) && class_name == "Error") {
      get_member(thread, error_obj, "cause", &error_cause);
    }

    auto data = make_dict();
    if (error_cause &&
        Polyglot_map_wrapper::unwrap(thread, error_cause, &data)) {
      if (data->has_key(k_key_message)) {
        set_message(data->get_string(k_key_message));
      }

      if (data->has_key(k_key_type)) {
        m_type = data->get_string(k_key_type);
      }

      if (data->has_key(k_key_source)) {
        m_source = data->get_string(k_key_source);
      }

      if (data->has_key(k_key_code)) {
        auto code = data->get_int(k_key_code);
        if (code > 0) {
          m_code = code;
        }
      }

      if (data->has_key(k_key_line)) {
        m_line = data->get_uint(k_key_line);
      }

      if (data->has_key(k_key_column)) {
        m_column = data->get_uint(k_key_column);
      }

      if (data->has_key(k_key_source_line)) {
        m_source_line = data->get_string(k_key_source_line);
      }

      if (data->has_key(k_key_backtrace)) {
        m_backtrace.clear();
        for (const auto &frame : *data->at(k_key_backtrace).as_array()) {
          m_backtrace.push_back(frame.as_string());
        }
      }
    }
  }
}

void Polyglot_error::set_message(const std::string &msg) {
  Polyglot_generic_error::set_message(msg);

  static std::string k_exceeded_gc_error =
      "Garbage-collected heap size exceeded.";

  if (strncmp(msg.data(), k_exceeded_gc_error.data(),
              k_exceeded_gc_error.size()) == 0) {
    m_resource_exhausted = true;
  }
}

/**
 * This function is used to normalize as much as possible the exceptions, i.e.
 * to try getting as much as possible the separate components for future unified
 * formatting
 */
void Polyglot_error::parse_and_translate(const std::string &source) {
  // This pattern identifies exceptions in the format of:
  // <type>: <details> such as all the exceptions produced internally, i.e.
  // - SyntaxError: (shell):2:20 Missing close quote
  static const std::regex type_message(
      R"*((\w*Error):\s(.*))*", std::regex::icase | std::regex::optimize);

  // This pattern is used to parse the details found with the previous pattern
  // which come in the following format:
  // <source>:<line>:<start>-<end> <message>
  // NOTE: The -<end> portion may come or not
  static const std::regex source_location_message(
      R"*((.*):(\d+):(\d+)(-\d+)?\s(.*))*",
      std::regex::icase | std::regex::optimize);

  // This pattern is used to parse the details on a backtrace entry which comes
  // in the format of <source>:<line>:<offset> (as formatted in
  // filter_backtrace)
  static const std::regex bt_source_location_message(
      R"*(^.*?\((.+):(\d+)\)$)*", std::regex::icase | std::regex::optimize);

  std::string msg = {source};

  std::cmatch m;
  if (std::regex_match(msg.c_str(), msg.c_str() + msg.size(), m,
                       type_message)) {
    std::string type = m[1];
    msg = m[2];

    if (type == k_syntax_error) {
      if (std::regex_match(msg.c_str(), msg.c_str() + msg.size(), m,
                           source_location_message)) {
        m_source = m[1];
        m_line = static_cast<size_t>(std::stoi(m[2]));

        msg = m[5];
      }
    }

    // Set the values if not already defined, if already defined they are indeed
    // more accurate
    if (!m_type.has_value()) {
      m_type = std::move(type);
    }

    if (message().empty()) {
      set_message(msg);
    }
  } else if (std::regex_match(msg.c_str(), msg.c_str() + msg.size(), m,
                              bt_source_location_message)) {
    m_source = m[1];
    m_line = static_cast<size_t>(std::stoi(m[2]));
  }
}

bool Polyglot_error::is_syntax_error() const {
  return m_type.has_value() && (*m_type) == k_syntax_error;
}

std::string Polyglot_error::format(bool include_location) const {
  std::string error_message;

  if (!message().empty()) {
    error_message += message();

    if (m_type.has_value() && m_code.has_value()) {
      error_message += " (" + *m_type + " " + std::to_string(*m_code) + ")\n";
    } else if (m_type.has_value()) {
      error_message += " (" + *m_type + ")\n";
    } else if (m_code.has_value()) {
      error_message += " (" + std::to_string(*m_code) + ") ";
    }

    if (!include_location || (m_backtrace.size() > 1)) {
      std::string location;
      if (m_source.has_value()) {
        location.append(" at ").append(*m_source);
      }

      if (m_line.has_value()) {
        location += ':';
        location.append(std::to_string(*m_line));

        std::string line_location;
        if (m_column.has_value()) {
          location += ':';
          location.append(std::to_string(*m_column));

          if (m_source_line.has_value()) {
            line_location = "\n in " + *m_source_line + '\n' +
                            std::string(*m_column + 4, ' ') + '^';
          }
        }

        if (!m_backtrace.empty()) {
          location.append(std::accumulate(
              m_backtrace.begin(), m_backtrace.end(), std::string(""),
              [](const std::string &a, const std::string &b) {
                return a + "\n at " + b;
              }));
        }

        if (!line_location.empty()) {
          location.append(line_location);
        }
      }

      if (!location.empty()) error_message += location;
    }

    if (error_message.back() != '\n') {
      error_message += '\n';
    }
  }

  return error_message;
}

shcore::Dictionary_t Polyglot_error::data() const {
  auto ret_val = shcore::make_dict();

  ret_val->set(k_key_message, shcore::Value(message()));
  if (!m_backtrace.empty()) {
    auto bt = shcore::make_array(m_backtrace);
    ret_val->set(k_key_backtrace, shcore::Value(std::move(bt)));
  }

  if (m_code.has_value()) {
    ret_val->set(k_key_code, shcore::Value(*m_code));
  }

  if (m_column.has_value()) {
    ret_val->set(k_key_column, shcore::Value(static_cast<uint64_t>(*m_column)));
  }

  if (m_source_line.has_value()) {
    ret_val->set(k_key_source_line, shcore::Value(*m_source_line));
  }

  if (m_line.has_value()) {
    ret_val->set(k_key_line, shcore::Value(static_cast<uint64_t>(*m_line)));
  }

  if (m_source.has_value()) {
    ret_val->set(k_key_source, shcore::Value(*m_source));
  }

  if (m_type.has_value()) {
    ret_val->set(k_key_type, shcore::Value(*m_type));
  }

  return ret_val;
}

}  // namespace polyglot
}  // namespace shcore
