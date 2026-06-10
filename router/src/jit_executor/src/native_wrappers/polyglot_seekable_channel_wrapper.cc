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

#include "native_wrappers/polyglot_seekable_channel_wrapper.h"

#include <algorithm>
#include <cassert>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "utils/polyglot_api_clean.h"

namespace shcore {
namespace polyglot {

namespace {
struct Is_open {
  static constexpr const char *name = "isOpen";
  static poly_value callback(
      const std::shared_ptr<Polyglot_language> &language,
      const Polyglot_seekable_channel_wrapper::Native_ptr &seekable_channel) {
    return poly_bool(language->thread(), language->context(),
                     seekable_channel->is_open());
  }
};

struct Close {
  static constexpr const char *name = "close";
  static poly_value callback(
      const std::shared_ptr<Polyglot_language> & /*language*/,
      const Polyglot_seekable_channel_wrapper::Native_ptr &seekable_channel) {
    seekable_channel->close();
    return nullptr;
  }
};

struct Read {
  static constexpr const char *name = "read";
  static constexpr std::size_t argc = 2;
  static poly_value callback(
      const std::shared_ptr<Polyglot_language> &language,
      const Polyglot_seekable_channel_wrapper::Native_ptr &seekable_channel,
      const std::vector<poly_value> &argv) {
    size_t amount = to_int(language->thread(), argv[1]);

    std::string buffer;

    buffer.resize(amount);

    auto read = seekable_channel->read(&buffer[0], amount);

    throw_if_error(poly_write_to_byte_buffer, language->thread(),
                   language->context(), argv[0],
                   static_cast<const void *>(buffer.data()), read);

    return poly_int(language->thread(), language->context(), read);
  }
};

struct Write {
  static constexpr const char *name = "write";
  static constexpr std::size_t argc = 1;
  static poly_value callback(
      const std::shared_ptr<Polyglot_language> &language,
      const Polyglot_seekable_channel_wrapper::Native_ptr &seekable_channel,
      const std::vector<poly_value> & /*argv*/) {
    const char *buffer = nullptr;
    size_t amount = 0;
    int64_t size = seekable_channel->write(buffer, amount);
    return poly_int(language->thread(), language->context(), size);
  }
};

struct Position {
  static constexpr const char *name = "position";
  static poly_value callback(
      const std::shared_ptr<Polyglot_language> &language,
      const Polyglot_seekable_channel_wrapper::Native_ptr &seekable_channel) {
    return poly_int(language->thread(), language->context(),
                    seekable_channel->position());
  }
};

struct Set_position {
  static constexpr const char *name = "setPosition";
  static constexpr std::size_t argc = 1;
  static poly_value callback(
      const std::shared_ptr<Polyglot_language> &language,
      const Polyglot_seekable_channel_wrapper::Native_ptr &seekable_channel,
      const std::vector<poly_value> &argv) {
    seekable_channel->set_position(to_int(language->thread(), argv[0]));
    return Polyglot_seekable_channel_wrapper(language).wrap(seekable_channel);
  }
};

struct Size {
  static constexpr const char *name = "size";
  static poly_value callback(
      const std::shared_ptr<Polyglot_language> &language,
      const Polyglot_seekable_channel_wrapper::Native_ptr &seekable_channel) {
    poly_value value;
    poly_create_double(language->thread(), language->context(),
                       static_cast<double>(seekable_channel->size()), &value);
    return value;
  }
};

struct Truncate {
  static constexpr const char *name = "truncate";
  static constexpr std::size_t argc = 1;
  static poly_value callback(
      const std::shared_ptr<Polyglot_language> &language,
      const Polyglot_seekable_channel_wrapper::Native_ptr &seekable_channel,
      const std::vector<poly_value> &argv) {
    seekable_channel->truncate(to_int(language->thread(), argv[0]));
    return Polyglot_seekable_channel_wrapper(language).wrap(seekable_channel);
  }
};

}  // namespace

Polyglot_seekable_channel_wrapper::Polyglot_seekable_channel_wrapper(
    std::weak_ptr<Polyglot_language> language)
    : Polyglot_native_wrapper(std::move(language)) {}

poly_value Polyglot_seekable_channel_wrapper::create_wrapper(
    poly_thread thread, poly_context context, ICollectable *collectable) const {
  poly_value poly_object;
  throw_if_error(
      poly_create_proxy_seekable_byte_channel, thread, context, collectable,
      &Polyglot_seekable_channel_wrapper::polyglot_handler_no_args<Is_open>,
      &Polyglot_seekable_channel_wrapper::polyglot_handler_no_args<Close>,
      &Polyglot_seekable_channel_wrapper::polyglot_handler_fixed_args<Read>,
      &Polyglot_seekable_channel_wrapper::polyglot_handler_fixed_args<Write>,
      &Polyglot_seekable_channel_wrapper::polyglot_handler_no_args<Position>,
      &Polyglot_seekable_channel_wrapper::polyglot_handler_fixed_args<
          Set_position>,
      &Polyglot_seekable_channel_wrapper::polyglot_handler_no_args<Size>,
      &Polyglot_seekable_channel_wrapper::polyglot_handler_fixed_args<Truncate>,
      &Polyglot_seekable_channel_wrapper::handler_release_collectable,
      &poly_object);

  return poly_object;
}

}  // namespace polyglot
}  // namespace shcore
