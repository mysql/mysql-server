/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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

#ifndef _NGS_VIO_WRAPPER_H_
#define _NGS_VIO_WRAPPER_H_

#include "plugin/x/ngs/include/ngs/interface/vio_interface.h"

namespace ngs {

class Vio_wrapper : public Vio_interface {
public:
  Vio_wrapper(Vio *vio);

  ssize_t read(uchar* buffer, ssize_t bytes_to_send) override;
  ssize_t write(const uchar* buffer, ssize_t bytes_to_send) override;

  void set_timeout(const Direction direction, const uint32_t timeout) override;

  void set_state(const PSI_socket_state state) override;
  void set_thread_owner() override;

  my_socket get_fd() override;
  enum_vio_type get_type() override;
  sockaddr_storage *peer_addr(std::string &address, uint16 &port) override;

  int shutdown() override;

  Vio *get_vio() { return m_vio; }

  ~Vio_wrapper();

private:
  Vio *m_vio;
};

} // namespace ngs

#endif // _NGS_VIO_WRAPPER_H_

