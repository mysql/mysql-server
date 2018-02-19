/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

#ifndef _NGS_CAPABILITIES_CONFIGURATOR_H_
#define _NGS_CAPABILITIES_CONFIGURATOR_H_

#include <string>
#include <vector>

#include "plugin/x/ngs/include/ngs/capabilities/handler.h"
#include "plugin/x/ngs/include/ngs/error_code.h"
#include "plugin/x/ngs/include/ngs/memory.h"
#include "plugin/x/ngs/include/ngs_common/protocol_protobuf.h"

namespace ngs {

class Client_interface;

class Capabilities_configurator {
 public:
  Capabilities_configurator(
      const std::vector<Capability_handler_ptr> &capabilities);
  virtual ~Capabilities_configurator() {}

  virtual ::Mysqlx::Connection::Capabilities *get();

  virtual ngs::Error_code prepare_set(
      const ::Mysqlx::Connection::Capabilities &capabilities);
  virtual void commit();

  void add_handler(Capability_handler_ptr handler);

 private:
  Capability_handler_ptr get_capabilitie_by_name(const std::string &name);

  std::vector<Capability_handler_ptr> m_capabilities;
  std::vector<Capability_handler_ptr> m_capabilities_prepared;
};

}  // namespace ngs

#endif  // _NGS_CAPABILITIES_CONFIGURATOR_H_
