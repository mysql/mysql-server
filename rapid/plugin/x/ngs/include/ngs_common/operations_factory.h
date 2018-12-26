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

#ifndef NGS_OPERATIONS_FACTORY_H_
#define NGS_OPERATIONS_FACTORY_H_

#include "ngs_common/operations_factory_interface.h"
#include "ngs_common/smart_ptr.h"


namespace ngs {

class Operations_factory: public Operations_factory_interface {
public:
  ngs::shared_ptr<Socket_interface> create_socket(PSI_socket_key key, int domain, int type, int protocol);
  ngs::shared_ptr<Socket_interface> create_socket(MYSQL_SOCKET mysql_socket);

  ngs::shared_ptr<File_interface> open_file(const char* name, int access, int permission);

  ngs::shared_ptr<System_interface> create_system_interface();
};

} // namespace ngs

#endif // NGS_OPERATIONS_FACTORY_H_
