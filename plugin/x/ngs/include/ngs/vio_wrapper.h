/*
 * Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_VIO_WRAPPER_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_VIO_WRAPPER_H_

#include "plugin/x/ngs/include/ngs/interface/vio_interface.h"
#include "plugin/x/ngs/include/ngs/thread.h"
#include "plugin/x/ngs/include/ngs_common/connection_type.h"

namespace ngs {

class Vio_wrapper : public Vio_interface {
 public:
  Vio_wrapper(Vio *vio);

  ssize_t read(uchar *buffer, ssize_t bytes_to_send) override;
  ssize_t write(const uchar *buffer, ssize_t bytes_to_send) override;

  void set_timeout(const Direction direction, const uint32_t timeout) override;

  void set_state(const PSI_socket_state state) override;
  void set_thread_owner() override;

  my_socket get_fd() override;
  Connection_type get_type() override;
  sockaddr_storage *peer_addr(std::string &address, uint16 &port) override;

  int shutdown() override;

  Vio *get_vio() override { return m_vio; }
  MYSQL_SOCKET &get_mysql_socket() override { return m_vio->mysql_socket; }

  ~Vio_wrapper();

 private:
  Vio *m_vio;
  Mutex m_shutdown_mutex;
};

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_VIO_WRAPPER_H_
