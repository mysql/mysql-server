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

#include "native_wrappers/polyglot_file_system_wrapper.h"

#include <algorithm>
#include <cassert>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "native_wrappers/polyglot_seekable_channel_wrapper.h"
#include "utils/polyglot_api_clean.h"

namespace shcore {
namespace polyglot {

namespace {
struct Parse_uri_path {
  static constexpr const char *name = "parsePath(uri)";
  static constexpr std::size_t argc = 1;
  static poly_value callback(
      const std::shared_ptr<Polyglot_language> &language,
      const Polyglot_file_system_wrapper::Native_ptr &file_stream,
      const std::vector<poly_value> &argv) {
    auto result = file_stream->parse_uri_path(language->to_string(argv[0]));
    return poly_string(language->thread(), language->context(), result);
  }
};

struct Parse_string_path {
  static constexpr const char *name = "parsePath(path)";
  static constexpr std::size_t argc = 1;
  static poly_value callback(
      const std::shared_ptr<Polyglot_language> &language,
      const Polyglot_file_system_wrapper::Native_ptr &file_stream,
      const std::vector<poly_value> &argv) {
    auto result = file_stream->parse_string_path(language->to_string(argv[0]));
    return poly_string(language->thread(), language->context(), result);
  }
};

struct Check_access {
  static constexpr const char *name = "checkAccess";
  static constexpr std::size_t argc = 2;
  static poly_value callback(
      const std::shared_ptr<Polyglot_language> &language,
      const Polyglot_file_system_wrapper::Native_ptr &file_stream,
      const std::vector<poly_value> &argv) {
    auto flags = to_int(language->thread(), argv[1]);

    file_stream->check_access(language->to_string(argv[0]), flags);

    return nullptr;
  }
};

struct Create_directory {
  static constexpr const char *name = "createDirectory";
  static constexpr std::size_t argc = 1;
  static poly_value callback(
      const std::shared_ptr<Polyglot_language> &language,
      const Polyglot_file_system_wrapper::Native_ptr &file_stream,
      const std::vector<poly_value> &argv) {
    file_stream->create_directory(language->to_string(argv[0]));

    return nullptr;
  }
};

struct Remove {
  static constexpr const char *name = "delete";
  static constexpr std::size_t argc = 1;
  static poly_value callback(
      const std::shared_ptr<Polyglot_language> &language,
      const Polyglot_file_system_wrapper::Native_ptr &file_stream,
      const std::vector<poly_value> &argv) {
    file_stream->remove(language->to_string(argv[0]));

    return nullptr;
  }
};

struct New_byte_channel {
  static constexpr const char *name = "newByteChannel";
  static constexpr std::size_t argc = 1;
  static poly_value callback(
      const std::shared_ptr<Polyglot_language> &language,
      const Polyglot_file_system_wrapper::Native_ptr &file_stream,
      const std::vector<poly_value> &argv) {
    auto channel = file_stream->new_byte_channel(language->to_string(argv[0]));
    return Polyglot_seekable_channel_wrapper(language).wrap(channel);
  }
};

struct To_absolute_path {
  static constexpr const char *name = "toAbsolutePath";
  static constexpr std::size_t argc = 1;
  static poly_value callback(
      const std::shared_ptr<Polyglot_language> &language,
      const Polyglot_file_system_wrapper::Native_ptr &file_stream,
      const std::vector<poly_value> &argv) {
    auto result = file_stream->to_absolute_path(language->to_string(argv[0]));
    return poly_string(language->thread(), language->context(), result);
  }
};

struct To_real_path {
  static constexpr const char *name = "toRealPath";
  static constexpr std::size_t argc = 1;
  static poly_value callback(
      const std::shared_ptr<Polyglot_language> &language,
      const Polyglot_file_system_wrapper::Native_ptr &file_stream,
      const std::vector<poly_value> &argv) {
    auto result = file_stream->to_real_path(language->to_string(argv[0]));
    return poly_string(language->thread(), language->context(), result);
  }
};

}  // namespace

Polyglot_file_system_wrapper::Polyglot_file_system_wrapper(
    std::weak_ptr<Polyglot_language> language)
    : Polyglot_native_wrapper(std::move(language)) {}

poly_value Polyglot_file_system_wrapper::create_wrapper(
    poly_thread thread, poly_context /*context*/,
    ICollectable *collectable) const {
  poly_value poly_object;
  throw_if_error(
      poly_create_proxy_file_system, thread, collectable,
      &Polyglot_file_system_wrapper::polyglot_handler_fixed_args<
          Parse_uri_path>,
      &Polyglot_file_system_wrapper::polyglot_handler_fixed_args<
          Parse_string_path>,
      &Polyglot_file_system_wrapper::polyglot_handler_fixed_args<Check_access>,
      &Polyglot_file_system_wrapper::polyglot_handler_fixed_args<
          Create_directory>,
      &Polyglot_file_system_wrapper::polyglot_handler_fixed_args<Remove>,
      &Polyglot_file_system_wrapper::polyglot_handler_fixed_args<
          New_byte_channel>,
      nullptr,  // new directory stream
      &Polyglot_file_system_wrapper::polyglot_handler_fixed_args<
          To_absolute_path>,
      &Polyglot_file_system_wrapper::polyglot_handler_fixed_args<To_real_path>,
      nullptr,  // read attributes,
      &Polyglot_file_system_wrapper::handler_release_collectable, &poly_object);

  return poly_object;
}

}  // namespace polyglot
}  // namespace shcore
