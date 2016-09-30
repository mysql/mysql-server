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

#ifndef _NGS_HANDLER_INTERFACE_H_
#define _NGS_HANDLER_INTERFACE_H_

#include "violite.h"
#include "ngs/thread.h"

namespace ngs
{

class Server_task_interface
{
public:
  virtual ~Server_task_interface() {}

  virtual void pre_loop() = 0;
  virtual void post_loop() = 0;
  virtual void loop() = 0;
};

} // namespace ngs

#endif // _NGS_HANDLER_INTERFACE_H_
