/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_TIMEOUT_CALLBACK_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_TIMEOUT_CALLBACK_H_

#include "plugin/x/ngs/include/ngs/interface/socket_events_interface.h"
#include "plugin/x/ngs/include/ngs/interface/timeout_callback_interface.h"

namespace ngs {

class Timeout_callback : public Timeout_callback_interface {
 public:
  Timeout_callback(const std::shared_ptr<Socket_events_interface> &event)
      : m_event(event) {}

  void add_callback(const std::size_t delay_ms,
                    const On_callback &callback) override {
    m_event->add_timer(delay_ms, callback);
  }

 private:
  std::shared_ptr<Socket_events_interface> m_event;
};

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_TIMEOUT_CALLBACK_H_
