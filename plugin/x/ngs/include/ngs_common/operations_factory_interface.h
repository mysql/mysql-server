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

#ifndef NGS_OPERATIONS_FACTORY_INTERFACE_H_
#define NGS_OPERATIONS_FACTORY_INTERFACE_H_

#include "plugin/x/ngs/include/ngs/memory.h"
#include "plugin/x/ngs/include/ngs_common/file_interface.h"
#include "plugin/x/ngs/include/ngs_common/socket_interface.h"
#include "plugin/x/ngs/include/ngs_common/system_interface.h"

namespace ngs {

class Operations_factory_interface {
 public:
  typedef ngs::shared_ptr<Operations_factory_interface> Shared_ptr;

  virtual ~Operations_factory_interface() {}

  virtual ngs::shared_ptr<Socket_interface> create_socket(PSI_socket_key key,
                                                          int domain, int type,
                                                          int protocol) = 0;
  virtual ngs::shared_ptr<Socket_interface> create_socket(
      MYSQL_SOCKET socket) = 0;

  virtual ngs::shared_ptr<File_interface> open_file(const char *name,
                                                    int access,
                                                    int permission) = 0;

  virtual ngs::shared_ptr<System_interface> create_system_interface() = 0;
};

}  // namespace ngs

#endif  // NGS_OPERATIONS_FACTORY_INTERFACE_H_
