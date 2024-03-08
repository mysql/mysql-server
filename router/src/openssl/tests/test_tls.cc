/*
  Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <gmock/gmock.h>

#include <csignal>
#include <ctime>  // time_t
#include <fstream>
#include <functional>
#include <memory>
#include <random>
#include <string>

#include "helpers/router_test_helpers.h"
#include "my_macros.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/tls_context.h"
#include "tls/trace_stream.h"

#include "test_tls/client/actions.h"
#include "test_tls/client/async_client.h"
#include "test_tls/client/sync_client.h"
#include "test_tls/interconnected/connected_tcp_streams.h"
#include "test_tls/interconnected/connected_tls_tcp_streams.h"
#include "test_tls/interconnected/connected_tls_unix_local_streams.h"
#include "test_tls/interconnected/connected_unix_local_streams.h"
#include "test_tls/pair_stream.h"

std::default_random_engine *g_rengin = nullptr;
std::string g_data_dir;

using VectorOfBytes = std::vector<uint8_t>;

static VectorOfBytes generate_vector(const size_t size) {
  VectorOfBytes source_data(size, '\0');
  std::uniform_int_distribution<int> distribution(0, 255);
  std::generate(
      source_data.begin(), source_data.end(), [&distribution]() -> auto {
        return distribution(*g_rengin);
      });

  return source_data;
}

class NetContext {
 public:
  void process_start_io_context() {
    auto guard = net::make_work_guard(context_.get_executor());
    while (!context_.stopped()) context_.run();
  }

  net::io_context context_{std::make_unique<net::impl::socket::SocketService>(),
                           std::make_unique<net::poll_io_service>()};
};

template <typename ConnectedStreams, size_t bytes, size_t by = bytes>
class Transfer : public ConnectedStreams {
 public:
  size_t get_number_bytes() { return bytes; }
  size_t get_packed_size() { return by; }
};

template <typename ConnectedStreams>
class StreamTest : public ::testing::Test, public NetContext {
 public:
  using TestStream = typename ConnectedStreams::Stream;
  using TestStreamPtr = typename ConnectedStreams::StreamPtr;
  using TestAsyncClient = AsyncClient<TestStream>;
  using TestSyncClient = SyncClient<TestStream>;

 public:
  void SetUp() override {
    connectedStreams_.create_interconnected(context_, objectStreamServer_,
                                            objectStreamClient_);
  }

  ConnectedStreams connectedStreams_;
  TestStreamPtr objectStreamServer_;
  TestStreamPtr objectStreamClient_;

  std::atomic<int> async_io_running_{2};
};

using ConnectedLocalStream = ConnectedUnixLocalStreams;
using ConnectedTlsLocalStreams = ConnectedTlsUnixLocalStreams;

constexpr int k_one_byte = 1;
constexpr int k_below_ssl_record_bytes = 10'000L;
constexpr int k_over_ssl_record_bytes = 17'000L;
constexpr int k_mutiple_ssl_records_bytes = 1000'000L;

constexpr int k_bytes_small = 100;
constexpr int k_bytes_medium = 100000;
constexpr int k_bytes_large = 1000000;

constexpr int k_split_one_byte = 1;
// less than `k_bytes_small`, still large enough to transfer medium buffer in a
// fast way.
constexpr int k_split_50_bytes = k_bytes_small / 2;

using StreamTypes = ::testing::Types<
#ifndef _WIN32
    Transfer<ConnectedLocalStream, k_one_byte>,
    Transfer<ConnectedLocalStream, k_below_ssl_record_bytes>,
    Transfer<ConnectedLocalStream, k_over_ssl_record_bytes>,
    Transfer<ConnectedLocalStream, k_mutiple_ssl_records_bytes>,
#endif  // _WIN32

    Transfer<ConnectedTcpStreams, k_one_byte>,
    Transfer<ConnectedTcpStreams, k_below_ssl_record_bytes>,
    Transfer<ConnectedTcpStreams, k_over_ssl_record_bytes>,
    Transfer<ConnectedTcpStreams, k_mutiple_ssl_records_bytes>,

#ifndef _WIN32
    Transfer<ConnectedTlsLocalStreams, k_one_byte>,
    Transfer<ConnectedTlsLocalStreams, k_below_ssl_record_bytes>,
    Transfer<ConnectedTlsLocalStreams, k_over_ssl_record_bytes>,
    Transfer<ConnectedTlsLocalStreams, k_mutiple_ssl_records_bytes>,
#endif  // _WIN32

    Transfer<ConnectedTlsTcpStreams, k_one_byte>,
    Transfer<ConnectedTlsTcpStreams, k_below_ssl_record_bytes>,
    Transfer<ConnectedTlsTcpStreams, k_over_ssl_record_bytes>,
    Transfer<ConnectedTlsTcpStreams, k_mutiple_ssl_records_bytes>,

#ifndef _WIN32
    Transfer<ConnectedLocalStream, k_bytes_small, k_split_one_byte>,
    Transfer<ConnectedLocalStream, k_bytes_small, k_split_50_bytes>,
    Transfer<ConnectedLocalStream, k_bytes_medium, k_split_one_byte>,
    Transfer<ConnectedLocalStream, k_bytes_medium, k_split_50_bytes>,
    Transfer<ConnectedLocalStream, k_bytes_large, k_split_50_bytes>,
#endif  // _WIN32

    Transfer<ConnectedTlsTcpStreams, k_bytes_small, k_split_one_byte>,
    Transfer<ConnectedTlsTcpStreams, k_bytes_small, k_split_50_bytes>,
    Transfer<ConnectedTlsTcpStreams, k_bytes_medium, k_split_one_byte>,
    Transfer<ConnectedTlsTcpStreams, k_bytes_medium, k_split_50_bytes>,
    Transfer<ConnectedTlsTcpStreams, k_bytes_large, k_split_50_bytes>>;

TYPED_TEST_SUITE(StreamTest, StreamTypes);

TYPED_TEST(StreamTest, transfer_from_server_to_client) {
  using TestClient = typename TestFixture::TestAsyncClient;
  const size_t to_transffer = this->connectedStreams_.get_number_bytes();
  const size_t block_size = this->connectedStreams_.get_packed_size();
  const VectorOfBytes send_by_server = generate_vector(to_transffer);
  const VectorOfBytes send_by_client;

  TestClient io_server{
      &this->context_, &this->async_io_running_,
      this->objectStreamServer_.get(), &send_by_server,
      generate_action_sequence<ActionWrite>(to_transffer, block_size)};
  TestClient io_client{
      &this->context_, &this->async_io_running_,
      this->objectStreamClient_.get(), &send_by_client,
      generate_action_sequence<ActionRead>(to_transffer, block_size)};

  TestFixture::process_start_io_context();

  ASSERT_THAT(io_server.get_received_data(),
              ::testing::ElementsAreArray(send_by_client));
  ASSERT_THAT(io_client.get_received_data(),
              ::testing::ElementsAreArray(send_by_server));
}

TYPED_TEST(StreamTest, transfer_from_client_to_server) {
  using TestClient = typename TestFixture::TestAsyncClient;
  const size_t to_transffer = this->connectedStreams_.get_number_bytes();
  const size_t block_size = this->connectedStreams_.get_packed_size();
  const VectorOfBytes transmitted_by_server;
  const VectorOfBytes transmitted_by_client = generate_vector(to_transffer);

  TestClient io_server{
      &this->context_, &this->async_io_running_,
      this->objectStreamServer_.get(), &transmitted_by_server,
      generate_action_sequence<ActionRead>(to_transffer, block_size)};
  TestClient io_client{
      &this->context_, &this->async_io_running_,
      this->objectStreamClient_.get(), &transmitted_by_client,
      generate_action_sequence<ActionWrite>(to_transffer, block_size)};

  TestFixture::process_start_io_context();

  ASSERT_THAT(io_server.get_received_data(),
              ::testing::ElementsAreArray(transmitted_by_client));
  ASSERT_THAT(io_client.get_received_data(),
              ::testing::ElementsAreArray(transmitted_by_server));
}

TYPED_TEST(StreamTest, transfer_from_client_to_server_exchange_in_seq) {
  using TestClient = typename TestFixture::TestAsyncClient;
  const size_t to_transffer = this->connectedStreams_.get_number_bytes();
  const size_t block_size = this->connectedStreams_.get_packed_size();
  const auto operations_done_by_server =
      generate_action_sequence<ActionRead, ActionWrite>(to_transffer,
                                                        block_size);
  const auto operations_done_by_client =
      generate_action_sequence<ActionWrite, ActionRead>(to_transffer,
                                                        block_size);

  const VectorOfBytes transmitted_by_server =
      generate_vector(action_count_send(operations_done_by_server));
  const VectorOfBytes transmitted_by_client =
      generate_vector(action_count_send(operations_done_by_client));

  TestClient io_server{&this->context_, &this->async_io_running_,
                       this->objectStreamServer_.get(), &transmitted_by_server,
                       operations_done_by_server};
  TestClient io_client{&this->context_, &this->async_io_running_,
                       this->objectStreamClient_.get(), &transmitted_by_client,
                       operations_done_by_client};

  TestFixture::process_start_io_context();

  ASSERT_THAT(io_server.get_received_data(),
              ::testing::ElementsAreArray(transmitted_by_client));
  ASSERT_THAT(io_client.get_received_data(),
              ::testing::ElementsAreArray(transmitted_by_server));
}

template <typename Connections>
class ParalleStreamTest : public StreamTest<Connections> {
 public:
  void SetUp() override {
    // Ignore default creation of connection pair in
    // `StreamTest::SetUp`.
  }

  void process_start_io_context_with_multiple_threads() {
    std::vector<std::thread> threads;

    for (int i = 0; i < 8; ++i) {
      threads.emplace_back([this]() { this->process_start_io_context(); });
    }

    for (auto &t : threads) {
      t.join();
    }
  }
};

using ParalleStreamTypes = ::testing::Types<
#ifndef _WIN32
    Transfer<ConnectedLocalStream, k_bytes_medium, k_split_50_bytes>,
    Transfer<ConnectedLocalStream, k_bytes_medium, k_split_50_bytes>,
#endif  // _WIN32
    Transfer<ConnectedTcpStreams, k_bytes_medium, k_split_50_bytes>,
    Transfer<ConnectedTlsTcpStreams, k_bytes_medium, k_split_50_bytes>>;

TYPED_TEST_SUITE(ParalleStreamTest, ParalleStreamTypes);

TYPED_TEST(ParalleStreamTest,
           parallel_transfer_from_client_to_server_exchange_in_seq) {
  using TestClient = typename TestFixture::TestAsyncClient;
  const size_t k_number_of_connection_pairs = 1;
  const size_t to_transffer = this->connectedStreams_.get_number_bytes();
  const size_t block_size = this->connectedStreams_.get_packed_size();
  const auto operations_done_by_server =
      generate_action_sequence<ActionRead, ActionWrite>(to_transffer,
                                                        block_size);
  const auto operations_done_by_client =
      generate_action_sequence<ActionWrite, ActionRead>(to_transffer,
                                                        block_size);

  const VectorOfBytes transmitted_by_server =
      generate_vector(action_count_send(operations_done_by_server));
  const VectorOfBytes transmitted_by_client =
      generate_vector(action_count_send(operations_done_by_client));

  std::vector<std::unique_ptr<typename TestFixture::TestStream>> hold_streams;
  std::vector<std::unique_ptr<TestClient>> clients;

  this->async_io_running_ = k_number_of_connection_pairs * 2;

#ifdef CONNECTION_TLS_TCP_STREAM_MONITOR
  auto tracing_file_name =
      std::string("test-") + std::to_string(getpid()) + ".log";
  auto by_client_file_name =
      std::string("client-") + std::to_string(getpid()) + ".log";
  auto by_server_file_name =
      std::string("server-") + std::to_string(getpid()) + ".log";
  std::ofstream stream_tracing(tracing_file_name);
  std::ofstream stream_by_client(by_client_file_name);
  std::ofstream stream_by_server(by_server_file_name);

  stream_by_server.write((const char *)transmitted_by_server.data(),
                         transmitted_by_server.size());
  stream_by_client.write((const char *)transmitted_by_client.data(),
                         transmitted_by_client.size());

  this->connectedStreams_.change_output(&stream_tracing);
#endif  // CONNECTION_TLS_TCP_STREAM_MONITOR

  for (size_t i = 0; i < k_number_of_connection_pairs; ++i) {
    this->connectedStreams_.create_interconnected(
        this->context_, this->objectStreamServer_, this->objectStreamClient_);

    clients.push_back(std::make_unique<TestClient>(
        &this->context_, &this->async_io_running_,
        this->objectStreamServer_.get(), &transmitted_by_server,
        operations_done_by_server));
    clients.push_back(std::make_unique<TestClient>(
        &this->context_, &this->async_io_running_,
        this->objectStreamClient_.get(), &transmitted_by_client,
        operations_done_by_client));

    hold_streams.push_back(std::move(this->objectStreamClient_));
    hold_streams.push_back(std::move(this->objectStreamServer_));
  }

  TestFixture::process_start_io_context();

  // Using multiple threads that go inside io_context::run,
  // causes a hang.
  // Using this function create number of threads pointed in
  // this->async_io_running_ field.
  // this->process_start_io_context_with_multiple_threads();

  for (size_t i = 0; i < k_number_of_connection_pairs; ++i) {
    EXPECT_THAT(clients[i * 2]->get_received_data(),
                ::testing::ElementsAreArray(transmitted_by_client));
    EXPECT_THAT(clients[i * 2 + 1]->get_received_data(),
                ::testing::ElementsAreArray(transmitted_by_server));
  }

  clients.clear();

#ifdef CONNECTION_TLS_TCP_STREAM_MONITOR
  if (!testing::Test::HasFailure()) {
    stream_tracing.close();
    remove(tracing_file_name.c_str());
    stream_by_client.close();
    remove(by_client_file_name.c_str());
    stream_by_server.close();
    remove(by_server_file_name.c_str());
  }
#endif  // CONNECTION_TLS_TCP_STREAM_MONITOR
}

template <typename T>
class ClosureStreamTest : public StreamTest<T> {
 public:
};

using ClosureStreamTypes = ::testing::Types<
#ifndef _WIN32
    ConnectedLocalStream, ConnectedTlsLocalStreams,
#endif  // _WIN32
    ConnectedTcpStreams, ConnectedTlsTcpStreams>;

TYPED_TEST_SUITE(ClosureStreamTest, ClosureStreamTypes);

TYPED_TEST(ClosureStreamTest, disconnect_while_data_transfer) {
  using TestClient = typename TestFixture::TestAsyncClient;
  const size_t blocks_size = 100;
  const VectorOfBytes send_by_server = generate_vector(blocks_size);
  const VectorOfBytes send_by_client = generate_vector(blocks_size);

  TestClient io_server{
      &this->context_,
      &this->async_io_running_,
      this->objectStreamServer_.get(),
      &send_by_server,
      {ActionWrite(blocks_size), ActionRead(blocks_size), ActionDisconnect()}};
  TestClient io_client{&this->context_,
                       &this->async_io_running_,
                       this->objectStreamClient_.get(),
                       &send_by_client,
                       {ActionRead(blocks_size), ActionWrite(blocks_size),
                        ActionExpectDisconnect()}};

  ASSERT_THROW(TestFixture::process_start_io_context(), std::error_code);

  ASSERT_THAT(io_server.get_received_data(),
              ::testing::ElementsAreArray(send_by_client));
  ASSERT_THAT(io_client.get_received_data(),
              ::testing::ElementsAreArray(send_by_server));
}

TYPED_TEST(ClosureStreamTest, disconnect_at_start) {
  using TestClient = typename TestFixture::TestAsyncClient;
  const VectorOfBytes send_by_server;
  const VectorOfBytes send_by_client;

  TestClient io_server{&this->context_,
                       &this->async_io_running_,
                       this->objectStreamServer_.get(),
                       &send_by_server,
                       {ActionDisconnect()}};
  TestClient io_client{&this->context_,
                       &this->async_io_running_,
                       this->objectStreamClient_.get(),
                       &send_by_client,
                       {ActionExpectDisconnect()}};

  ASSERT_THROW(TestFixture::process_start_io_context(), std::error_code);

  ASSERT_THAT(io_server.get_received_data(),
              ::testing::ElementsAreArray(send_by_client));
  ASSERT_THAT(io_client.get_received_data(),
              ::testing::ElementsAreArray(send_by_server));
}

template <typename T>
class SyncStreamTest : public StreamTest<T> {
 public:
  using Parent = StreamTest<T>;

 public:
  void SetUp() override {
    Parent::connectedStreams_.change_non_blocking(false);
    Parent::SetUp();
  }
};

using SyncStreamTypes = ::testing::Types<
#ifndef _WIN32
    ConnectedLocalStream, ConnectedTlsLocalStreams,
#endif  // _WIN32
    ConnectedTcpStreams, ConnectedTlsTcpStreams>;

TYPED_TEST_SUITE(SyncStreamTest, SyncStreamTypes);

TYPED_TEST(SyncStreamTest, transfer_from_server_to_client) {
  using TestClient = typename TestFixture::TestSyncClient;
  const size_t to_transffer = k_over_ssl_record_bytes;
  const size_t block_size = k_over_ssl_record_bytes;
  const VectorOfBytes send_by_server = generate_vector(to_transffer);
  const VectorOfBytes send_by_client;

  TestClient io_server{
      this->objectStreamServer_.get(), &send_by_server,
      generate_action_sequence<ActionWrite>(to_transffer, block_size)};
  TestClient io_client{
      this->objectStreamClient_.get(), &send_by_client,
      generate_action_sequence<ActionRead>(to_transffer, block_size)};

  std::thread th_client{[&io_client]() { io_client.execute(); }};
  std::thread th_server{[&io_server]() { io_server.execute(); }};

  th_client.join();
  th_server.join();

  ASSERT_THAT(io_server.get_received_data(),
              ::testing::ElementsAreArray(send_by_client));
  ASSERT_THAT(io_client.get_received_data(),
              ::testing::ElementsAreArray(send_by_server));
}

TYPED_TEST(SyncStreamTest, transfer_from_client_to_server) {
  using TestClient = typename TestFixture::TestSyncClient;
  const size_t to_transffer = k_over_ssl_record_bytes;
  const size_t block_size = k_over_ssl_record_bytes;
  const VectorOfBytes send_by_server;
  const VectorOfBytes send_by_client = generate_vector(to_transffer);

  TestClient io_server{
      this->objectStreamServer_.get(), &send_by_server,
      generate_action_sequence<ActionRead>(to_transffer, block_size)};
  TestClient io_client{
      this->objectStreamClient_.get(), &send_by_client,
      generate_action_sequence<ActionWrite>(to_transffer, block_size)};

  std::thread th_client{[&io_client]() { io_client.execute(); }};
  std::thread th_server{[&io_server]() { io_server.execute(); }};

  th_client.join();
  th_server.join();

  ASSERT_THAT(io_server.get_received_data(),
              ::testing::ElementsAreArray(send_by_client));
  ASSERT_THAT(io_client.get_received_data(),
              ::testing::ElementsAreArray(send_by_server));
}

TYPED_TEST(SyncStreamTest, transfer_from_sequence) {
  using TestClient = typename TestFixture::TestSyncClient;
  const size_t to_transffer = k_over_ssl_record_bytes;
  const size_t block_size = k_bytes_small;

  const auto operations_done_by_server =
      generate_action_sequence<ActionRead, ActionWrite>(to_transffer,
                                                        block_size);
  const auto operations_done_by_client =
      generate_action_sequence<ActionWrite, ActionRead>(to_transffer,
                                                        block_size);

  const VectorOfBytes transmitted_by_server =
      generate_vector(action_count_send(operations_done_by_server));
  const VectorOfBytes transmitted_by_client =
      generate_vector(action_count_send(operations_done_by_client));

  TestClient io_server{this->objectStreamServer_.get(), &transmitted_by_server,
                       operations_done_by_server};
  TestClient io_client{this->objectStreamClient_.get(), &transmitted_by_client,
                       operations_done_by_client};

  std::thread th_client{[&io_client]() { io_client.execute(); }};
  std::thread th_server{[&io_server]() { io_server.execute(); }};

  th_client.join();
  th_server.join();

  ASSERT_THAT(io_server.get_received_data(),
              ::testing::ElementsAreArray(transmitted_by_client));
  ASSERT_THAT(io_client.get_received_data(),
              ::testing::ElementsAreArray(transmitted_by_server));
}

int main(int argc, char *argv[]) {
  using namespace mysql_harness;
  std::random_device rdev;
  std::default_random_engine rengin(rdev());
  TlsLibraryContext tls_lib_context;

  init_windows_sockets();
#ifndef _WIN32
  // In case when socket connection is closed, it may generate
  // SIGPIPE, which indicates that other side closed the endpoint.
  // We need to ignore that signal that it doesn't close the application
  struct sigaction sigact;
  memset(&sigact, 0, sizeof(sigact));
  sigact.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &sigact, NULL);
#endif

  g_data_dir = get_tests_data_dir(Path(argv[0]).dirname().str());
  g_rengin = &rengin;

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
