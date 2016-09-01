/*
 * Copyright (c) 2016 Oracle and/or its affiliates. All rights reserved.
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

#ifndef _XPL_LISTENER_FACTORY_INTERFACE_H_
#define _XPL_LISTENER_FACTORY_INTERFACE_H_

#include "ngs/interface/listener_factory_interface.h"


namespace xpl
{

class Listener_factory: public ngs::Listener_factory_interface
{
public:
  ngs::Listener_interface_ptr create_unix_socket_listener(const std::string &unix_socket_path, ngs::Time_and_socket_events &event, const uint32 backlog);
  ngs::Listener_interface_ptr create_tcp_socket_listener(const unsigned short port, ngs::Time_and_socket_events &event, const uint32 backlog);
};

} // namespace xpl

#endif // _XPL_LISTENER_FACTORY_INTERFACE_H_
