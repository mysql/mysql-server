// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "opentelemetry/trace/default_span.h"
#include "opentelemetry/trace/span_context.h"

#include <cstring>
#include <string>

#include <gtest/gtest.h>

namespace
{

using opentelemetry::trace::DefaultSpan;
using opentelemetry::trace::SpanContext;

TEST(DefaultSpanTest, GetContext)
{
  SpanContext span_context = SpanContext(false, false);
  DefaultSpan sp           = DefaultSpan(span_context);
  EXPECT_EQ(span_context, sp.GetContext());
}

TEST(DefaultSpanTest, CopyConstructor)
{
  SpanContext span_context = SpanContext(true, false);
  DefaultSpan original(span_context);

  DefaultSpan copied(original);

  EXPECT_EQ(span_context, copied.GetContext());
}

TEST(DefaultSpanTest, MoveConstructor)
{
  SpanContext span_context = SpanContext(true, false);
  DefaultSpan original(span_context);

  DefaultSpan moved(std::move(original));

  EXPECT_EQ(span_context, moved.GetContext());
}

TEST(DefaultSpanTest, CopyAssignment)
{
  SpanContext source_context = SpanContext(true, false);
  DefaultSpan source(source_context);
  DefaultSpan target(SpanContext(false, false));

  target = source;
  EXPECT_EQ(source_context, target.GetContext());

  target = target;
  EXPECT_EQ(source_context, target.GetContext());
}

TEST(DefaultSpanTest, MoveAssignment)
{
  SpanContext source_context = SpanContext(true, false);
  DefaultSpan source(source_context);
  DefaultSpan target(SpanContext(false, false));

  target = std::move(source);
  EXPECT_EQ(source_context, target.GetContext());

  target = std::move(target);
  EXPECT_EQ(source_context, target.GetContext());
}
}  // namespace
