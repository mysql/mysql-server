/*
 * Copyright (c) 2015, 2017 Oracle and/or its affiliates. All rights reserved.
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

#ifndef MYSQLX_NGS_SERVER_CLIENT_TIMEOUT_H_
#define MYSQLX_NGS_SERVER_CLIENT_TIMEOUT_H_

#include "ngs_common/chrono.h"
#include "ngs/interface/client_interface.h"
#include "ngs_common/bind.h"


namespace ngs
{

class Server_client_timeout
{
public:
  Server_client_timeout(const chrono::time_point &release_all_before_time);
  void validate_client_state(
      ngs::shared_ptr<Client_interface> client);

  chrono::time_point get_oldest_client_accept_time();

private:
  chrono::time_point m_oldest_client_accept_time;
  const chrono::time_point& m_release_all_before_time;
};

} // namespace ngs

#endif // MYSQLX_NGS_SERVER_CLIENT_TIMEOUT_H_
