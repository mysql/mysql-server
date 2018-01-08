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

#ifndef _NGS_PROTOCOL_MONITOR_INTERFACE_H_
#define _NGS_PROTOCOL_MONITOR_INTERFACE_H_


namespace ngs
{


class Protocol_monitor_interface
{
public:
  virtual ~Protocol_monitor_interface() {}

  virtual void on_notice_warning_send() = 0;
  virtual void on_notice_other_send() = 0;
  virtual void on_fatal_error_send() = 0;
  virtual void on_init_error_send() = 0;
  virtual void on_row_send() = 0;
  virtual void on_send(long bytes_transferred) = 0;
  virtual void on_receive(long bytes_transferred) = 0;

  virtual void on_error_send() = 0;
  virtual void on_error_unknown_msg_type() = 0;
};


} // namespace ngs


#endif // _NGS_PROTOCOL_MONITOR_INTERFACE_H_
