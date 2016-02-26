/* Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */


#include "myasio/wrapper_ssl.h"

namespace ngs
{

namespace test
{

typedef long (*socket_recv_type)(void *, void *, std::size_t );
typedef long (*socket_send_type)(void *, const void *, std::size_t );

class MockWrapper_ssl : public IWrapper_ssl
{
public:
  MOCK_METHOD0(ssl_initialize, void());
  MOCK_METHOD0(get_ssl_options, boost::shared_ptr<IOptions_session> ());
  MOCK_METHOD0(get_boost_error, boost::system::error_code());
  MOCK_METHOD0(ssl_set_error_none, void());
  MOCK_METHOD0(ssl_set_error_want_read, void());
  MOCK_METHOD0(ssl_is_no_fatal_error, bool());
  MOCK_METHOD0(ssl_is_error_would_block, bool());
  MOCK_METHOD1(ssl_set_socket_error, void(int error));
  MOCK_METHOD0(ssl_handshake, bool());
  MOCK_METHOD2(ssl_read, int(void* buffer, int sz));
  MOCK_METHOD2(ssl_write, int(const void* buffer, int sz));
  MOCK_METHOD1(ssl_set_fd, void(int file_descriptor));
  MOCK_METHOD1(ssl_set_transport_recv, void(socket_recv_type));
  MOCK_METHOD1(ssl_set_transport_send, void(socket_send_type));
  MOCK_METHOD1(ssl_set_transport_data, void(void *error));
};


} // namespace test

}  // namespace ngs
