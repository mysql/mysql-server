/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef NGS_OPERATIONS_FACTORY_INTERFACE_H_
#define NGS_OPERATIONS_FACTORY_INTERFACE_H_

#include "ngs/memory.h"
#include "ngs_common/socket_interface.h"
#include "ngs_common/file_interface.h"
#include "ngs_common/system_interface.h"


namespace ngs {

class Operations_factory_interface {
public:
  typedef ngs::shared_ptr<Operations_factory_interface> Shared_ptr;

  virtual ~Operations_factory_interface() {}

  virtual ngs::shared_ptr<Socket_interface> create_socket(PSI_socket_key key, int domain, int type, int protocol) = 0;
  virtual ngs::shared_ptr<Socket_interface> create_socket(MYSQL_SOCKET socket) = 0;

  virtual ngs::shared_ptr<File_interface> open_file(const char* name, int access, int permission) = 0;

  virtual ngs::shared_ptr<System_interface> create_system_interface() = 0;
};

} // namespace ngs

#endif // NGS_OPERATIONS_FACTORY_INTERFACE_H_
