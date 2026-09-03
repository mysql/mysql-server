// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <stdint.h>
#include <string>

#include "opentelemetry/context/context.h"
#include "opentelemetry/nostd/shared_ptr.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/trace/context.h"
#include "opentelemetry/trace/default_span.h"
#include "opentelemetry/trace/span.h"
#include "opentelemetry/trace/span_context.h"
#include "opentelemetry/trace/span_id.h"
#include "opentelemetry/trace/span_metadata.h"
#include "opentelemetry/trace/trace_flags.h"
#include "opentelemetry/trace/trace_id.h"

namespace
{

namespace context_api = opentelemetry::context;
namespace trace_api   = opentelemetry::trace;
namespace nostd       = opentelemetry::nostd;

nostd::shared_ptr<trace_api::Span> MakeValidSpan()
{
  const uint8_t trace_id_bytes[trace_api::TraceId::kSize] = {0x10, 0x32, 0x54, 0x76, 0x98, 0xba,
                                                             0xdc, 0xfe, 0x01, 0x23, 0x45, 0x67,
                                                             0x89, 0xab, 0xcd, 0xef};
  const uint8_t span_id_bytes[trace_api::SpanId::kSize]   = {0xde, 0xad, 0xbe, 0xef,
                                                             0xca, 0xfe, 0xba, 0xbe};

  trace_api::SpanContext valid_span_context(
      trace_api::TraceId(trace_id_bytes), trace_api::SpanId(span_id_bytes),
      trace_api::TraceFlags(trace_api::TraceFlags::kIsSampled), false);

  return nostd::shared_ptr<trace_api::Span>(new trace_api::DefaultSpan(valid_span_context));
}

TEST(TraceContextTest, GetSpan)
{
  {
    context_api::Context context;
    auto span = trace_api::GetSpan(context);
    ASSERT_TRUE(span != nullptr);
    EXPECT_FALSE(span->GetContext().IsValid());
  }

  {
    context_api::Context context;
    auto context_with_wrong_type = context.SetValue(trace_api::kSpanKey, true);
    auto span                    = trace_api::GetSpan(context_with_wrong_type);
    ASSERT_TRUE(span != nullptr);
    EXPECT_FALSE(span->GetContext().IsValid());
  }

  {
    context_api::Context context;
    auto input_span = MakeValidSpan();
    ASSERT_TRUE(input_span->GetContext().IsValid());
    auto context_with_span = context.SetValue(trace_api::kSpanKey, input_span);
    auto output_span       = trace_api::GetSpan(context_with_span);
    ASSERT_TRUE(output_span != nullptr);
    EXPECT_TRUE(output_span->GetContext().IsValid());
    EXPECT_EQ(output_span.get(), input_span.get());
  }
}

TEST(TraceContextTest, SetSpan)
{
  context_api::Context context;
  auto input_span = MakeValidSpan();

  auto context_with_span = trace_api::SetSpan(context, input_span);
  auto output_span       = trace_api::GetSpan(context_with_span);
  ASSERT_TRUE(output_span != nullptr);
  EXPECT_EQ(output_span.get(), input_span.get());
}

TEST(TraceContextTest, IsRootSpan)
{
  {
    context_api::Context context;
    EXPECT_FALSE(trace_api::IsRootSpan(context));
    auto root_context = context.SetValue(trace_api::kIsRootSpanKey, true);
    EXPECT_TRUE(trace_api::IsRootSpan(root_context));
  }

  {
    context_api::Context context;
    uint64_t value{1};
    auto context_with_wrong_type = context.SetValue(trace_api::kIsRootSpanKey, value);
    EXPECT_FALSE(trace_api::IsRootSpan(context_with_wrong_type));
  }
}
}  // namespace
