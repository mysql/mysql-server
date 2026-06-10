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

#ifndef MYSQLSHDK_SCRIPTING_POLYGLOT_NATIVE_WRAPPERS_POLYGLOT_SEEKABLE_CHANNEL_WRAPPER_H_
#define MYSQLSHDK_SCRIPTING_POLYGLOT_NATIVE_WRAPPERS_POLYGLOT_SEEKABLE_CHANNEL_WRAPPER_H_

#include <memory>
#include <string>

#include "utils/polyglot_api_clean.h"

#include "mysqlrouter/jit_executor_value.h"

#include "mysqlrouter/polyglot_file_system.h"

#include "native_wrappers/polyglot_native_wrapper.h"

namespace shcore {
namespace polyglot {

class Polyglot_language;

class Polyglot_seekable_channel_wrapper
    : public Polyglot_native_wrapper<ISeekable_channel,
                                     Collectable_type::OBJECT> {
 public:
  Polyglot_seekable_channel_wrapper() = delete;

  explicit Polyglot_seekable_channel_wrapper(
      std::weak_ptr<Polyglot_language> language);

  Polyglot_seekable_channel_wrapper(const Polyglot_seekable_channel_wrapper &) =
      delete;
  Polyglot_seekable_channel_wrapper &operator=(
      const Polyglot_seekable_channel_wrapper &) = delete;

  Polyglot_seekable_channel_wrapper(Polyglot_seekable_channel_wrapper &&) =
      delete;
  Polyglot_seekable_channel_wrapper &operator=(
      Polyglot_seekable_channel_wrapper &&) = delete;

  ~Polyglot_seekable_channel_wrapper() override = default;

 private:
  poly_value create_wrapper(poly_thread thread, poly_context context,
                            ICollectable *collectable) const override;
};

}  // namespace polyglot
}  // namespace shcore

#endif  // MYSQLSHDK_SCRIPTING_POLYGLOT_NATIVE_WRAPPERS_POLYGLOT_SEEKABLE_CHANNEL_WRAPPER_H_
