// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <stdint.h>
#include <stdlib.h>
#include <map>
#include <string>
#include <utility>

#include "opentelemetry/context/context.h"
#include "opentelemetry/context/propagation/environment_carrier.h"
#include "opentelemetry/context/runtime_context.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/trace/default_span.h"
#include "opentelemetry/trace/propagation/http_trace_context.h"
#include "opentelemetry/trace/scope.h"
#include "opentelemetry/trace/span.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/span_id.h"
#include "opentelemetry/trace/span_metadata.h"
#include "opentelemetry/trace/trace_flags.h"
#include "opentelemetry/trace/trace_id.h"
#include "opentelemetry/trace/trace_state.h"

// Platform-portable setenv/unsetenv wrappers
#ifdef _WIN32
static void test_setenv(const char *name, const char *value)
{
  _putenv_s(name, value);
}
static void test_unsetenv(const char *name)
{
  _putenv_s(name, "");
}
#else
static void test_setenv(const char *name, const char *value)
{
  ::setenv(name, value, 1);
}
static void test_unsetenv(const char *name)
{
  ::unsetenv(name);
}
#endif

using namespace opentelemetry;

template <typename T>
static std::string Hex(const T &id_item)
{
  char buf[T::kSize * 2];
  id_item.ToLowerBase16(buf);
  return std::string(buf, sizeof(buf));
}

class EnvironmentCarrierTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    test_unsetenv("TRACEPARENT");
    test_unsetenv("TRACESTATE");
    test_unsetenv("BAGGAGE");
  }

  void TearDown() override
  {
    test_unsetenv("TRACEPARENT");
    test_unsetenv("TRACESTATE");
    test_unsetenv("BAGGAGE");
  }
};

TEST_F(EnvironmentCarrierTest, GetReadsFromEnvironment)
{
  test_setenv("TRACEPARENT", "00-4bf92f3577b34da6a3ce929d0e0e4736-0102030405060708-01");

  context::propagation::EnvironmentCarrier carrier;
  auto value = carrier.Get("traceparent");
  EXPECT_EQ(value, "00-4bf92f3577b34da6a3ce929d0e0e4736-0102030405060708-01");
}

TEST_F(EnvironmentCarrierTest, GetReturnsEmptyForMissingKey)
{
  context::propagation::EnvironmentCarrier carrier;
  auto value = carrier.Get("traceparent");
  EXPECT_EQ(value, "");
}

TEST_F(EnvironmentCarrierTest, GetCachesValues)
{
  test_setenv("TRACEPARENT", "00-4bf92f3577b34da6a3ce929d0e0e4736-0102030405060708-01");

  context::propagation::EnvironmentCarrier carrier;

  // First call reads from environment
  auto value1 = carrier.Get("traceparent");
  EXPECT_EQ(value1, "00-4bf92f3577b34da6a3ce929d0e0e4736-0102030405060708-01");

  // Change environment - cached value should be returned
  test_setenv("TRACEPARENT", "changed-value");
  auto value2 = carrier.Get("traceparent");
  EXPECT_EQ(value2, "00-4bf92f3577b34da6a3ce929d0e0e4736-0102030405060708-01");
}

TEST_F(EnvironmentCarrierTest, SetNoOpWithoutMap)
{
  context::propagation::EnvironmentCarrier carrier;
  // Should not crash
  carrier.Set("traceparent", "00-4bf92f3577b34da6a3ce929d0e0e4736-0102030405060708-01");
}

TEST_F(EnvironmentCarrierTest, SetWritesToMap)
{
  auto env_map = std::make_shared<std::map<std::string, std::string>>();
  context::propagation::EnvironmentCarrier carrier(env_map);

  carrier.Set("traceparent", "00-4bf92f3577b34da6a3ce929d0e0e4736-0102030405060708-01");
  EXPECT_EQ(env_map->at("TRACEPARENT"), "00-4bf92f3577b34da6a3ce929d0e0e4736-0102030405060708-01");
}

TEST_F(EnvironmentCarrierTest, SetUppercaseConversion)
{
  auto env_map = std::make_shared<std::map<std::string, std::string>>();
  context::propagation::EnvironmentCarrier carrier(env_map);

  carrier.Set("tracestate", "key=value");
  EXPECT_EQ(env_map->count("TRACESTATE"), 1u);
  EXPECT_EQ(env_map->at("TRACESTATE"), "key=value");

  // Original lowercase key should not be in the map
  EXPECT_EQ(env_map->count("tracestate"), 0u);
}

TEST_F(EnvironmentCarrierTest, ExtractTraceContext)
{
  test_setenv("TRACEPARENT", "00-4bf92f3577b34da6a3ce929d0e0e4736-0102030405060708-01");

  context::propagation::EnvironmentCarrier carrier;
  trace::propagation::HttpTraceContext propagator;
  context::Context ctx;
  auto new_ctx = propagator.Extract(carrier, ctx);

  auto span_ctx_val = new_ctx.GetValue(trace::kSpanKey);
  EXPECT_TRUE(nostd::holds_alternative<nostd::shared_ptr<trace::Span>>(span_ctx_val));

  auto span = nostd::get<nostd::shared_ptr<trace::Span>>(span_ctx_val);
  EXPECT_EQ(Hex(span->GetContext().trace_id()), "4bf92f3577b34da6a3ce929d0e0e4736");
  EXPECT_EQ(Hex(span->GetContext().span_id()), "0102030405060708");
  EXPECT_TRUE(span->GetContext().IsSampled());
  EXPECT_TRUE(span->GetContext().IsRemote());
}

TEST_F(EnvironmentCarrierTest, ExtractWithTraceState)
{
  test_setenv("TRACEPARENT", "00-4bf92f3577b34da6a3ce929d0e0e4736-0102030405060708-01");
  test_setenv("TRACESTATE", "congo=t61rcWkgMzE");

  context::propagation::EnvironmentCarrier carrier;
  trace::propagation::HttpTraceContext propagator;
  context::Context ctx;
  auto new_ctx = propagator.Extract(carrier, ctx);

  auto span_ctx_val = new_ctx.GetValue(trace::kSpanKey);
  auto span         = nostd::get<nostd::shared_ptr<trace::Span>>(span_ctx_val);

  EXPECT_EQ(Hex(span->GetContext().trace_id()), "4bf92f3577b34da6a3ce929d0e0e4736");

  auto trace_state = span->GetContext().trace_state();
  ASSERT_NE(trace_state, nullptr);

  std::string congo_val;
  trace_state->Get("congo", congo_val);
  EXPECT_EQ(congo_val, "t61rcWkgMzE");
}

TEST_F(EnvironmentCarrierTest, ExtractNoEnvVars)
{
  context::propagation::EnvironmentCarrier carrier;
  trace::propagation::HttpTraceContext propagator;
  context::Context ctx;
  auto new_ctx = propagator.Extract(carrier, ctx);

  auto span_ctx_val = new_ctx.GetValue(trace::kSpanKey);
  // With no env vars, Extract should produce an invalid span context
  if (nostd::holds_alternative<nostd::shared_ptr<trace::Span>>(span_ctx_val))
  {
    auto span = nostd::get<nostd::shared_ptr<trace::Span>>(span_ctx_val);
    EXPECT_FALSE(span->GetContext().IsValid());
  }
}

TEST_F(EnvironmentCarrierTest, InjectTraceContext)
{
  constexpr uint8_t buf_span[]  = {1, 2, 3, 4, 5, 6, 7, 8};
  constexpr uint8_t buf_trace[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  trace::SpanContext span_context{trace::TraceId{buf_trace}, trace::SpanId{buf_span},
                                  trace::TraceFlags{true}, false};
  nostd::shared_ptr<trace::Span> sp{new trace::DefaultSpan{span_context}};

  trace::Scope scoped_span{sp};

  auto env_map = std::make_shared<std::map<std::string, std::string>>();
  context::propagation::EnvironmentCarrier carrier(env_map);
  trace::propagation::HttpTraceContext propagator;

  propagator.Inject(carrier, context::RuntimeContext::GetCurrent());

  EXPECT_EQ(env_map->at("TRACEPARENT"), "00-0102030405060708090a0b0c0d0e0f10-0102030405060708-01");
}

TEST_F(EnvironmentCarrierTest, RoundTrip)
{
  // Step 1: Create a span context and inject it into a map
  constexpr uint8_t buf_span[]  = {0xAB, 0xCD, 0xEF, 0x12, 0x34, 0x56, 0x78, 0x9A};
  constexpr uint8_t buf_trace[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
                                   0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10};
  trace::SpanContext span_context{trace::TraceId{buf_trace}, trace::SpanId{buf_span},
                                  trace::TraceFlags{true}, false};
  nostd::shared_ptr<trace::Span> sp{new trace::DefaultSpan{span_context}};
  trace::Scope scoped_span{sp};

  auto env_map = std::make_shared<std::map<std::string, std::string>>();
  context::propagation::EnvironmentCarrier inject_carrier(env_map);
  trace::propagation::HttpTraceContext propagator;

  propagator.Inject(inject_carrier, context::RuntimeContext::GetCurrent());

  // Step 2: Set the injected values as environment variables
  for (const auto &entry : *env_map)
  {
    test_setenv(entry.first.c_str(), entry.second.c_str());
  }

  // Step 3: Extract from environment using a new carrier
  context::propagation::EnvironmentCarrier extract_carrier;
  context::Context ctx;
  auto new_ctx = propagator.Extract(extract_carrier, ctx);

  auto span_ctx_val = new_ctx.GetValue(trace::kSpanKey);
  ASSERT_TRUE(nostd::holds_alternative<nostd::shared_ptr<trace::Span>>(span_ctx_val));

  auto extracted_span = nostd::get<nostd::shared_ptr<trace::Span>>(span_ctx_val);
  EXPECT_EQ(Hex(extracted_span->GetContext().trace_id()), Hex(span_context.trace_id()));
  EXPECT_EQ(Hex(extracted_span->GetContext().span_id()), Hex(span_context.span_id()));
  EXPECT_EQ(extracted_span->GetContext().IsSampled(), span_context.IsSampled());
  EXPECT_TRUE(extracted_span->GetContext().IsRemote());
}
