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


#include <boost/move/move.hpp>

#include "ngs_common/connection_vio.h"

namespace ngs
{

namespace test
{

class Mock_options_session : public IOptions_session
{
public:
  MOCK_METHOD0(supports_tls,bool ());
  MOCK_METHOD0(active_tls,  bool ());
  MOCK_METHOD0(ssl_cipher,  std::string ());
  MOCK_METHOD0(ssl_cipher_list,  std::vector<std::string> ());
  MOCK_METHOD0(ssl_version,  std::string ());

  MOCK_METHOD0(ssl_verify_depth, long ());
  MOCK_METHOD0(ssl_verify_mode, long ());
  MOCK_METHOD0(ssl_sessions_reused, long ());
  MOCK_METHOD0(ssl_get_verify_result_and_cert, long ());

  MOCK_METHOD0(ssl_get_peer_certificate_issuer, std::string ());

  MOCK_METHOD0(ssl_get_peer_certificate_subject, std::string ());
};

class Mock_options_context : public IOptions_context
{
public:
  MOCK_METHOD0(ssl_ctx_verify_depth, long ());
  MOCK_METHOD0(ssl_ctx_verify_mode, long ());

  MOCK_METHOD0(ssl_server_not_after, std::string ());
  MOCK_METHOD0(ssl_server_not_before, std::string ());

  MOCK_METHOD0(ssl_accept_renegotiates ,long ());

  MOCK_METHOD0(ssl_session_cache_hits, long ());
  MOCK_METHOD0(ssl_session_cache_misses, long ());
  MOCK_METHOD0(ssl_session_cache_mode, std::string ());
  MOCK_METHOD0(ssl_session_cache_overflows, long ());
  MOCK_METHOD0(ssl_session_cache_size, long ());
  MOCK_METHOD0(ssl_session_cache_timeouts, long ());
  MOCK_METHOD0(ssl_used_session_cache_entries, long ());
};

class Mock_socket_operations: public Socket_operations_interface
{
public:

  MOCK_METHOD3(bind, int (const my_socket&, const struct sockaddr*, socklen_t));
  MOCK_METHOD3(accept, my_socket(const my_socket&, struct sockaddr*, socklen_t*));
  MOCK_METHOD3(socket, my_socket(int, int, int));
  MOCK_METHOD2(listen, int(const my_socket&, int));
  MOCK_METHOD0(get_socket_errno, int());
};

class Mock_system_operations: public System_operations_interface
{
public:
  MOCK_METHOD3(open, int(const char*, int, int));
  MOCK_METHOD1(close, int(int));
  MOCK_METHOD3(read, int(int, void*, int));
  MOCK_METHOD3(write, int(int, void*, int));
  MOCK_METHOD1(fsync, int(int));
  MOCK_METHOD1(unlink, int(const char*));
  MOCK_METHOD0(getppid, int());
  MOCK_METHOD0(getpid, int());
  MOCK_METHOD2(kill, int(int, int));
  MOCK_METHOD0(get_errno, int());
};

} // namespace test

}  // namespace ngs
