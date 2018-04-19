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

#ifndef _NGS_CAPABILITIES_HANDLER_H_
#define _NGS_CAPABILITIES_HANDLER_H_

#include <string>

#include "plugin/x/ngs/include/ngs_common/protocol_protobuf.h"
#include "plugin/x/ngs/include/ngs_common/smart_ptr.h"

namespace ngs {
class Client_interface;

class Capability_handler {
 public:
  virtual ~Capability_handler() {}

  virtual const std::string name() const = 0;

  virtual bool is_supported() const = 0;

  virtual void get(::Mysqlx::Datatypes::Any &any) = 0;
  virtual bool set(const ::Mysqlx::Datatypes::Any &any) = 0;

  virtual void commit() = 0;
};

typedef ngs::shared_ptr<Capability_handler> Capability_handler_ptr;

}  // namespace ngs

#endif
