/*
 * Copyright (c) 2019, 2024, Oracle and/or its affiliates.
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
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */
#ifndef PLUGIN_X_PROTOCOL_STREAM_COMPRESSION_DECOMPRESSION_ALGORITHM_INTERFACE_H_
#define PLUGIN_X_PROTOCOL_STREAM_COMPRESSION_DECOMPRESSION_ALGORITHM_INTERFACE_H_

namespace protocol {

class Decompression_algorithm_interface {
 public:
  virtual ~Decompression_algorithm_interface() = default;

  virtual bool needs_input() const = 0;
  virtual void set_input(uint8_t *in_ptr, const int in_size) = 0;
  virtual bool decompress(uint8_t *out_ptr, int64_t *out_size) = 0;
  virtual bool was_error() const = 0;
};

}  // namespace protocol

#endif  // PLUGIN_X_PROTOCOL_STREAM_COMPRESSION_DECOMPRESSION_ALGORITHM_INTERFACE_H_
