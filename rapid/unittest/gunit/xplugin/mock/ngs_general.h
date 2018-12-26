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

#include "ngs_common/operations_factory_interface.h"
#include "ngs_common/socket_interface.h"
#include "ngs_common/options.h"
#include "ngs/socket_events_interface.h"


namespace ngs {

namespace test {

class Mock_options_session : public IOptions_session {
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

class Mock_options_context : public IOptions_context {
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

class Mock_socket: public Socket_interface {
public:

  MOCK_METHOD2(bind, int (const struct sockaddr*, socklen_t));
  MOCK_METHOD3(accept, MYSQL_SOCKET(PSI_socket_key, struct sockaddr*, socklen_t*));
  MOCK_METHOD1(listen, int(int));

  MOCK_METHOD0(close, void ());

  MOCK_METHOD0(get_socket_mysql, MYSQL_SOCKET ());
  MOCK_METHOD0(get_socket_fd, my_socket ());

  MOCK_METHOD4(set_socket_opt, int (int, int, const SOCKBUF_T *, socklen_t));
  MOCK_METHOD0(set_socket_thread_owner, void ());
};

class Mock_system: public System_interface {
public:
  MOCK_METHOD1(unlink, int(const char*));
  MOCK_METHOD2(kill, int(int, int));

  MOCK_METHOD0(get_ppid, int());
  MOCK_METHOD0(get_pid, int());
  MOCK_METHOD0(get_errno, int());

  MOCK_METHOD0(get_socket_errno, int ());
  MOCK_METHOD2(get_socket_error_and_message, void (int&, std::string&));

  MOCK_METHOD1(freeaddrinfo, void (addrinfo *));
  MOCK_METHOD4(getaddrinfo,  int  (
      const char *,
      const char *,
      const addrinfo *,
      addrinfo **));

  MOCK_METHOD1(sleep, void (uint32));
};

class Mock_file: public File_interface {
public:
  MOCK_METHOD0(is_valid, bool ());

  MOCK_METHOD0(close, int());
  MOCK_METHOD2(read, int(void*, int));
  MOCK_METHOD2(write, int(void*, int));
  MOCK_METHOD0(fsync, int());
};

class Mock_factory: public Operations_factory_interface {
public:
  MOCK_METHOD4(create_socket, Socket_interface::Shared_ptr (PSI_socket_key, int, int, int));
  MOCK_METHOD1(create_socket, Socket_interface::Shared_ptr (MYSQL_SOCKET));

  MOCK_METHOD3(open_file, File_interface::Shared_ptr (const char*, int, int));

  MOCK_METHOD0(create_system_interface, System_interface::Shared_ptr ());
};

class Mock_socket_events : public Socket_events_interface {
 public:
  MOCK_METHOD2(listen, bool (Socket_interface::Shared_ptr, ngs::function<void (Connection_acceptor_interface &)>));
  MOCK_METHOD2(add_timer, void (const std::size_t, ngs::function<bool ()>));
  MOCK_METHOD0(loop, void ());
  MOCK_METHOD0(break_loop, void ());
};

} // namespace test

}  // namespace ngs
