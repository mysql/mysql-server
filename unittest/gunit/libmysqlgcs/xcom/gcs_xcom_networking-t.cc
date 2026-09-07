/* Copyright (c) 2016, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <string>
#include <vector>

#include "gcs_base_test.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/xcom/task_os.h"

using std::vector;

namespace gcs_xcom_networking_unittest {

void clean_sock_probe(sock_probe *s) { free(s); }

class mock_gcs_sock_probe_interface : public Gcs_sock_probe_interface {
 public:
  MOCK_METHOD1(init_sock_probe, int(sock_probe *s));
  MOCK_METHOD1(number_of_interfaces, int(sock_probe *s));
  MOCK_METHOD3(get_sockaddr_address,
               void(sock_probe *s, int count, struct sockaddr **out));
  MOCK_METHOD3(get_sockaddr_netmask,
               void(sock_probe *s, int count, struct sockaddr **out));
  MOCK_METHOD2(get_if_name, char *(sock_probe *s, int count));
  MOCK_METHOD2(is_if_running, bool_t(sock_probe *s, int count));
  MOCK_METHOD1(close_sock_probe, void(sock_probe *s));

  void mock_gcs_sock_probe_interface_default() {
    ON_CALL(*this, close_sock_probe(_)).WillByDefault(Invoke(clean_sock_probe));
  }
};

class GcsXComNetworking : public GcsBaseTest {
 protected:
  GcsXComNetworking() : m_sock_probe_mock() {}
  ~GcsXComNetworking() override = default;

  void SetUp() override {
    m_sock_probe_mock.mock_gcs_sock_probe_interface_default();
  }

  void verify_sock_descriptor_to_string(sa_family_t family,
                                        const std::string &expected_address) {
    struct Socket_guard {
      explicit Socket_guard(int socket) : m_socket(socket) {}
      ~Socket_guard() {
        if (m_socket != -1) CLOSESOCKET(m_socket);
      }

      int m_socket;
    };

    Socket_guard listener(
        static_cast<int>(socket(family, SOCK_STREAM, IPPROTO_TCP)));
    ASSERT_FALSE(is_socket_error(listener.m_socket));

    sockaddr_storage address{};
    socklen_t address_size;
    if (family == AF_INET) {
      auto *ipv4_address = reinterpret_cast<sockaddr_in *>(&address);
      ipv4_address->sin_family = AF_INET;
      ipv4_address->sin_port = htons(0);
      ASSERT_EQ(1, inet_pton(AF_INET, expected_address.c_str(),
                             &ipv4_address->sin_addr));
      address_size = static_cast<socklen_t>(sizeof(*ipv4_address));
    } else {
      auto *ipv6_address = reinterpret_cast<sockaddr_in6 *>(&address);
      ipv6_address->sin6_family = AF_INET6;
      ipv6_address->sin6_port = htons(0);
      ASSERT_EQ(1, inet_pton(AF_INET6, expected_address.c_str(),
                             &ipv6_address->sin6_addr));
      address_size = static_cast<socklen_t>(sizeof(*ipv6_address));
    }

    ASSERT_EQ(0, bind(listener.m_socket, reinterpret_cast<sockaddr *>(&address),
                      address_size));
    ASSERT_EQ(
        0, getsockname(listener.m_socket,
                       reinterpret_cast<sockaddr *>(&address), &address_size));
    ASSERT_EQ(0, listen(listener.m_socket, 1));

    Socket_guard client(
        static_cast<int>(socket(family, SOCK_STREAM, IPPROTO_TCP)));
    ASSERT_FALSE(is_socket_error(client.m_socket));
    ASSERT_EQ(0, connect(client.m_socket,
                         reinterpret_cast<sockaddr *>(&address), address_size));

    Socket_guard accepted(
        static_cast<int>(accept(listener.m_socket, nullptr, nullptr)));
    ASSERT_FALSE(is_socket_error(accepted.m_socket));

    std::string actual_address;
    EXPECT_FALSE(sock_descriptor_to_string(accepted.m_socket, actual_address));
    EXPECT_EQ(expected_address, actual_address);
  }

  mock_gcs_sock_probe_interface m_sock_probe_mock;
};

TEST_F(GcsXComNetworking, SockDescriptorToStringIPv4) {
  verify_sock_descriptor_to_string(AF_INET, "127.0.0.1");
}

TEST_F(GcsXComNetworking, SockDescriptorToStringIPv6) {
  verify_sock_descriptor_to_string(AF_INET6, "::1");
}

TEST_F(GcsXComNetworking, SockProbeInvalid) {
  EXPECT_CALL(m_sock_probe_mock, init_sock_probe(_))
      .Times(1)
      .WillOnce(Return(-1));

  std::map<std::string, int> out_value;
  bool const result = get_local_addresses(m_sock_probe_mock, out_value);

  ASSERT_TRUE(result);
}

TEST_F(GcsXComNetworking, NoInterfaces) {
  EXPECT_CALL(m_sock_probe_mock, init_sock_probe(_))
      .Times(1)
      .WillOnce(Return(0));

  EXPECT_CALL(m_sock_probe_mock, close_sock_probe(_)).Times(1);

  EXPECT_CALL(m_sock_probe_mock, number_of_interfaces(_))
      .Times(1)
      .WillOnce(Return(0));

  std::map<std::string, int> out_value;
  bool const result = get_local_addresses(m_sock_probe_mock, out_value);

  ASSERT_TRUE(result);
}

TEST_F(GcsXComNetworking, ErrorRetrievingSockaddr) {
  std::string const if_name("interface");

  EXPECT_CALL(m_sock_probe_mock, init_sock_probe(_))
      .Times(1)
      .WillOnce(Return(0));

  EXPECT_CALL(m_sock_probe_mock, close_sock_probe(_)).Times(1);

  EXPECT_CALL(m_sock_probe_mock, number_of_interfaces(_))
      .Times(3)
      .WillRepeatedly(Return(1));

  struct sockaddr *null_sockaddr = nullptr;
  EXPECT_CALL(m_sock_probe_mock, get_sockaddr_netmask(_, _, _))
      .Times(1)
      .WillOnce(testing::SetArgPointee<2>(null_sockaddr));

  EXPECT_CALL(m_sock_probe_mock, get_sockaddr_address(_, _, _))
      .Times(1)
      .WillOnce(testing::SetArgPointee<2>(null_sockaddr));

  EXPECT_CALL(m_sock_probe_mock, get_if_name(_, _))
      .Times(1)
      .WillOnce(Return(const_cast<char *>(if_name.c_str())));

  std::map<std::string, int> out_value;
  bool const result = get_local_addresses(m_sock_probe_mock, out_value);

  ASSERT_TRUE(result);
}

TEST_F(GcsXComNetworking, ResolveAllIPV6) {
  std::vector<std::pair<sa_family_t, std::string>> out_value;
  bool const retval = resolve_all_ip_addr_from_hostname("::1", out_value);

  ASSERT_FALSE(retval);
}

}  // namespace gcs_xcom_networking_unittest
