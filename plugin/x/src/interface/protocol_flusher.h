/*
 * Copyright (c) 2018, 2023, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
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

#ifndef PLUGIN_X_SRC_INTERFACE_PROTOCOL_FLUSHER_H_
#define PLUGIN_X_SRC_INTERFACE_PROTOCOL_FLUSHER_H_

#include <cstdint>
#include <memory>

#include "plugin/x/src/interface/vio.h"

namespace xpl {
namespace iface {

class Protocol_flusher {
 public:
  enum class Result { k_error, k_flushed, k_not_flushed };

 public:
  virtual ~Protocol_flusher() = default;
  /**
    Force that next `try_flush` is going to dispatch data.
   */
  virtual void trigger_flush_required() = 0;
  virtual void trigger_on_message(const uint8_t type) = 0;

  /**
    Check if flush is required and try to execute it

    Flush is not going to be executed when the flusher is locked or
    when no other conditions to flush were fulfilled.

    @return result of flush operation
      @retval == k_flushed     flush was successful
      @retval == k_not_flushed nothing important to flush
      @retval == k_error       flush IO was failed
   */
  virtual Result try_flush() = 0;

  /**
    Check if flush is required
   */
  virtual bool is_going_to_flush() = 0;

  /**
    Write timeout to be used at flush execution
   */
  virtual void set_write_timeout(const uint32_t timeout) = 0;

  virtual Vio *get_connection() = 0;
};

}  // namespace iface
}  // namespace xpl

#endif  // PLUGIN_X_SRC_INTERFACE_PROTOCOL_FLUSHER_H_
