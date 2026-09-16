// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <grpc/grpc.h>
#include <grpcpp/support/channel_arguments.h>
#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "opentelemetry/exporters/otlp/otlp_grpc_client.h"
#include "opentelemetry/exporters/otlp/otlp_grpc_client_options.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace otlp
{

namespace
{

const grpc_arg *FindChannelArg(const grpc_channel_args &channel_args, const char *key)
{
  for (std::size_t i = 0; i < channel_args.num_args; ++i)
  {
    const grpc_arg &arg = channel_args.args[i];
    if (arg.key != nullptr && std::strcmp(arg.key, key) == 0)
    {
      return &arg;
    }
  }

  return nullptr;
}

}  // namespace

class OtlpGrpcClientTestPeer : public ::testing::Test
{
public:
  static grpc::ChannelArguments BuildChannelArguments(const OtlpGrpcClientOptions &options)
  {
    grpc::ChannelArguments grpc_arguments;
    OtlpGrpcClient::PopulateChannelArguments(options, grpc_arguments);
    return grpc_arguments;
  }
};

TEST(OtlpGrpcClientEndpointTest, GrpcClientTest)
{
  OtlpGrpcClientOptions opts1;
  opts1.endpoint = "unix:///tmp/otel1.sock";

  OtlpGrpcClientOptions opts2;
  opts2.endpoint = "unix:tmp/otel2.sock";

  OtlpGrpcClientOptions opts3;
  opts3.endpoint = "localhost:4317";

  auto target1 = OtlpGrpcClient::GetGrpcTarget(opts1.endpoint);
  auto target2 = OtlpGrpcClient::GetGrpcTarget(opts2.endpoint);
  auto target3 = OtlpGrpcClient::GetGrpcTarget(opts3.endpoint);

  EXPECT_EQ(target1, "unix:/tmp/otel1.sock");
  EXPECT_EQ(target2, "");
  EXPECT_EQ(target3, "localhost:4317");
}

TEST_F(OtlpGrpcClientTestPeer, ChannelArgumentsOptionTest)
{
  OtlpGrpcClientOptions options;
  options.user_agent = "otlp-grpc-target-test";

  grpc::ChannelArguments channel_arguments;
  channel_arguments.SetMaxReceiveMessageSize(1);
  options.channel_arguments = &channel_arguments;

  auto built_arguments = BuildChannelArguments(options);
  channel_arguments.SetMaxReceiveMessageSize(-1);

  auto built_channel_args = built_arguments.c_channel_args();

  const grpc_arg *max_receive_arg =
      FindChannelArg(built_channel_args, GRPC_ARG_MAX_RECEIVE_MESSAGE_LENGTH);
  ASSERT_NE(max_receive_arg, nullptr);
  EXPECT_EQ(max_receive_arg->type, GRPC_ARG_INTEGER);
  EXPECT_EQ(max_receive_arg->value.integer, 1);

  const grpc_arg *user_agent_arg =
      FindChannelArg(built_channel_args, GRPC_ARG_PRIMARY_USER_AGENT_STRING);
  ASSERT_NE(user_agent_arg, nullptr);
  EXPECT_EQ(user_agent_arg->type, GRPC_ARG_STRING);
  EXPECT_EQ(std::strncmp(user_agent_arg->value.string, options.user_agent.c_str(),
                         options.user_agent.size()),
            0);
}

}  // namespace otlp
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
