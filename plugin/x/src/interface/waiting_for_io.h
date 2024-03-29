// Copyright (c) 2018, 2023, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is also distributed with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have included with MySQL.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#ifndef PLUGIN_X_SRC_INTERFACE_WAITING_FOR_IO_H_
#define PLUGIN_X_SRC_INTERFACE_WAITING_FOR_IO_H_

namespace xpl {
namespace iface {

/**
 Interface for reporting idle waiting on IO

 In case when a thread can be blocked by read operation
 the decoder allows to periodical interrupt the waiting
 to execute in meantime an action.
 */
class Waiting_for_io {
 public:
  virtual ~Waiting_for_io() = default;

  /**
   Check if idle processing is needed

   This method returns `true` in case when there is
   an task that needs periodic checking and an IO operation
   may block the thread for a longer. Such case the IO waiting
   must be broken for multiple shorter periods.
   Between those shorter periods the code that does IO must
   call `on_idle_or_before_read`.
   */
  virtual bool has_to_report_idle_waiting() = 0;

  /**
    Performs idle action

    Method which must implement an action which needs to be
    executed periodically (asynchronous to any flow). The code
    that does the IO must call it when `has_to_report_idle_waiting`
    returns true and one of following occurred:

    * IO code read packet header
    * IO code is waiting for a header
    * long-executing-sql

    @retval true     OK
    @retval false    I/O error
   */
  virtual bool on_idle_or_before_read() = 0;
};

}  // namespace iface
}  // namespace xpl

#endif  // PLUGIN_X_SRC_INTERFACE_WAITING_FOR_IO_H_
