// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <stdint.h>
#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "opentelemetry/common/attribute_value.h"
#include "opentelemetry/common/timestamp.h"
#include "opentelemetry/exporters/otlp/otlp_log_recordable.h"
#include "opentelemetry/exporters/otlp/otlp_recordable_utils.h"
#include "opentelemetry/logs/severity.h"
#include "opentelemetry/nostd/span.h"
#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/sdk/instrumentationscope/instrumentation_scope.h"
#include "opentelemetry/sdk/logs/readable_log_record.h"
#include "opentelemetry/sdk/logs/recordable.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "opentelemetry/trace/span_id.h"
#include "opentelemetry/trace/trace_id.h"
#include "opentelemetry/version.h"

// clang-format off
#include "opentelemetry/exporters/otlp/protobuf_include_prefix.h" // IWYU pragma: keep
// IWYU pragma: no_include "net/proto2/public/repeated_field.h"
#include "opentelemetry/proto/collector/logs/v1/logs_service.pb.h"
#include "opentelemetry/proto/resource/v1/resource.pb.h"
#include "opentelemetry/proto/common/v1/common.pb.h"
#include "opentelemetry/proto/logs/v1/logs.pb.h"
#include "opentelemetry/exporters/otlp/protobuf_include_suffix.h" // IWYU pragma: keep
// clang-format on

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace otlp
{
namespace resource = opentelemetry::sdk::resource;
namespace proto    = opentelemetry::proto;

TEST(OtlpLogRecordable, Basic)
{
  OtlpLogRecordable rec;
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
  common::SystemTimestamp start_timestamp(now);

  uint64_t unix_start =
      std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

  rec.SetTimestamp(start_timestamp);
  rec.SetSeverity(opentelemetry::logs::Severity::kError);
  nostd::string_view name = "Test Log Message";
  rec.SetBody(name);

  uint8_t trace_id_bin[opentelemetry::trace::TraceId::kSize] = {
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
  std::string expected_trace_id_bytes;
  expected_trace_id_bytes.assign(
      reinterpret_cast<char *>(trace_id_bin),
      reinterpret_cast<char *>(trace_id_bin) + opentelemetry::trace::TraceId::kSize);
  rec.SetTraceId(opentelemetry::trace::TraceId{trace_id_bin});
  uint8_t span_id_bin[opentelemetry::trace::SpanId::kSize] = {'7', '6', '5', '4',
                                                              '3', '2', '1', '0'};
  std::string expected_span_id_bytes;
  expected_span_id_bytes.assign(
      reinterpret_cast<char *>(span_id_bin),
      reinterpret_cast<char *>(span_id_bin) + opentelemetry::trace::SpanId::kSize);
  rec.SetSpanId(opentelemetry::trace::SpanId{span_id_bin});

  EXPECT_EQ(rec.log_record().time_unix_nano(), unix_start);
  EXPECT_EQ(rec.log_record().severity_number(), proto::logs::v1::SEVERITY_NUMBER_ERROR);
  EXPECT_EQ(rec.log_record().severity_text(), "ERROR");
  EXPECT_EQ(rec.log_record().body().string_value(), name);
  EXPECT_EQ(rec.log_record().trace_id(), expected_trace_id_bytes);
  EXPECT_EQ(rec.log_record().span_id(), expected_span_id_bytes);

  // Test bytes body
  uint8_t byte_arr[] = {'T', 'e', '\0', 's', 't'};
  common::AttributeValue byte_val(
      nostd::span<const uint8_t>{reinterpret_cast<const uint8_t *>(byte_arr), 5});
  rec.SetBody(byte_val);
  EXPECT_TRUE(0 ==
              memcmp(reinterpret_cast<const void *>(rec.log_record().body().bytes_value().data()),
                     reinterpret_cast<const void *>(byte_arr), 5));
  EXPECT_EQ(rec.log_record().body().bytes_value().size(), 5);

#if defined(ENABLE_OTLP_UTF8_VALIDITY)
  // Test bytes body: non-UTF8 string_view is automatically converted to bytes_value
  const char byte_body[] = {'\x80', '\x81', '\x82', '\x83', '\x84'};
  nostd::string_view byte_body_view(byte_body, 5);
  rec.SetBody(byte_body_view);
  EXPECT_TRUE(0 ==
              memcmp(reinterpret_cast<const void *>(rec.log_record().body().bytes_value().data()),
                     reinterpret_cast<const void *>(byte_body), 5));
  EXPECT_EQ(rec.log_record().body().bytes_value().size(), 5);
#endif
}

TEST(OtlpLogRecordable, GetResource)
{
  OtlpLogRecordable rec;
  const std::string service_name_key = "service.name";
  std::string service_name           = "test-otlp";
  auto resource =
      opentelemetry::sdk::resource::Resource::Create({{service_name_key, service_name}});
  rec.SetResource(resource);

  EXPECT_EQ(&rec.GetResource(), &resource);
}

TEST(OtlpLogRecordable, DefaultResource)
{
  OtlpLogRecordable rec;

  EXPECT_EQ(&rec.GetResource(), &opentelemetry::sdk::logs::ReadableLogRecord::GetDefaultResource());
}

// Test non-int single types. Int single types are tested using templates (see IntAttributeTest)
TEST(OtlpLogRecordable, SetSingleAttribute)
{
  OtlpLogRecordable rec;

  nostd::string_view bool_key = "bool_attr";
  common::AttributeValue bool_val(true);
  rec.SetAttribute(bool_key, bool_val);

  nostd::string_view double_key = "double_attr";
  common::AttributeValue double_val(3.3);
  rec.SetAttribute(double_key, double_val);

  nostd::string_view str_key = "str_attr";
  common::AttributeValue str_val(nostd::string_view("Test"));
  rec.SetAttribute(str_key, str_val);

  nostd::string_view byte_key = "byte_attr";
  uint8_t byte_arr[]          = {'T', 'e', 's', 't'};
  common::AttributeValue byte_val(
      nostd::span<const uint8_t>{reinterpret_cast<const uint8_t *>(byte_arr), 4});
  rec.SetAttribute(byte_key, byte_val);

#if defined(ENABLE_OTLP_UTF8_VALIDITY)
  // Non-UTF8 string_view is automatically converted to bytes_value
  nostd::string_view non_utf8_string_key = "non_utf8_string_attr";
  const char non_utf8_string_arr[]       = {'\x80', '\x81', '\x82', '\x83'};
  common::AttributeValue non_utf8_string_val(nostd::string_view(non_utf8_string_arr, 4));
  rec.SetAttribute(non_utf8_string_key, non_utf8_string_val);
#endif

  int checked_attributes = 0;
  for (auto &attribute : rec.log_record().attributes())
  {
    if (attribute.key() == bool_key)
    {
      ++checked_attributes;
      EXPECT_EQ(attribute.value().bool_value(), nostd::get<bool>(bool_val));
    }
    else if (attribute.key() == double_key)
    {
      ++checked_attributes;
      EXPECT_EQ(attribute.value().double_value(), nostd::get<double>(double_val));
    }
    else if (attribute.key() == str_key)
    {
      ++checked_attributes;
      EXPECT_EQ(attribute.value().string_value(), nostd::get<nostd::string_view>(str_val).data());
    }
    else if (attribute.key() == byte_key)
    {
      ++checked_attributes;
      EXPECT_TRUE(0 ==
                  memcmp(reinterpret_cast<const void *>(attribute.value().bytes_value().data()),
                         reinterpret_cast<const void *>(byte_arr), 4));
      EXPECT_EQ(attribute.value().bytes_value().size(), 4);
    }
#if defined(ENABLE_OTLP_UTF8_VALIDITY)
    else if (attribute.key() == non_utf8_string_key)
    {
      ++checked_attributes;
      EXPECT_TRUE(0 ==
                  memcmp(reinterpret_cast<const void *>(attribute.value().bytes_value().data()),
                         reinterpret_cast<const void *>(non_utf8_string_arr), 4));
      EXPECT_EQ(attribute.value().bytes_value().size(), 4);
    }
#endif
  }
#if defined(ENABLE_OTLP_UTF8_VALIDITY)
  EXPECT_EQ(5, checked_attributes);
#else
  EXPECT_EQ(4, checked_attributes);
#endif
}

// Test non-int array types. Int array types are tested using templates (see IntAttributeTest)
TEST(OtlpLogRecordable, SetArrayAttribute)
{
  OtlpLogRecordable rec;

  const int kArraySize = 3;

  bool bool_arr[kArraySize] = {true, false, true};
  nostd::span<const bool> bool_span(bool_arr);
  rec.SetAttribute("bool_arr_attr", bool_span);

  double double_arr[kArraySize] = {22.3, 33.4, 44.5};
  nostd::span<const double> double_span(double_arr);
  rec.SetAttribute("double_arr_attr", double_span);

  nostd::string_view str_arr[kArraySize] = {"Hello", "World", "Test"};
  nostd::span<const nostd::string_view> str_span(str_arr);
  rec.SetAttribute("str_arr_attr", str_span);

  for (int i = 0; i < kArraySize; i++)
  {
    EXPECT_EQ(rec.log_record().attributes(0).value().array_value().values(i).bool_value(),
              bool_span[i]);
    EXPECT_EQ(rec.log_record().attributes(1).value().array_value().values(i).double_value(),
              double_span[i]);
    EXPECT_EQ(rec.log_record().attributes(2).value().array_value().values(i).string_value(),
              str_span[i]);
  }
}

TEST(OtlpLogRecordable, SetInstrumentationScope)
{
  OtlpLogRecordable rec;

  auto inst_lib =
      opentelemetry::sdk::instrumentationscope::InstrumentationScope::Create("test", "v1");
  rec.SetInstrumentationScope(*inst_lib);

  EXPECT_EQ(&rec.GetInstrumentationScope(), inst_lib.get());
}

TEST(OtlpLogRecordable, SetEventName)
{
  OtlpLogRecordable rec;

  nostd::string_view event_name = "Test Event";
  rec.SetEventId(0, event_name);

  EXPECT_EQ(rec.log_record().event_name(), event_name);
}

/**
 * AttributeValue can contain different int types, such as int, int64_t,
 * unsigned int, and uint64_t. To avoid writing test cases for each, we can
 * use a template approach to test all int types.
 */
template <typename T>
struct OtlpLogRecordableIntAttributeTest : public testing::Test
{
  using IntParamType = T;
};

using IntTypes = testing::Types<int, int64_t, unsigned int, uint64_t>;
TYPED_TEST_SUITE(OtlpLogRecordableIntAttributeTest, IntTypes);

TYPED_TEST(OtlpLogRecordableIntAttributeTest, SetIntSingleAttribute)
{
  using IntType = typename TestFixture::IntParamType;
  IntType i     = 2;
  common::AttributeValue int_val(i);

  OtlpLogRecordable rec;

  rec.SetAttribute("int_attr", int_val);

  EXPECT_EQ(rec.log_record().attributes(0).value().int_value(), nostd::get<IntType>(int_val));
}

TYPED_TEST(OtlpLogRecordableIntAttributeTest, SetIntArrayAttribute)
{
  using IntType = typename TestFixture::IntParamType;

  const int kArraySize        = 3;
  IntType int_arr[kArraySize] = {4, 5, 6};
  nostd::span<const IntType> int_span(int_arr);

  OtlpLogRecordable rec;

  rec.SetAttribute("int_arr_attr", int_span);

  for (int i = 0; i < kArraySize; i++)
  {
    EXPECT_EQ(rec.log_record().attributes(0).value().array_value().values(i).int_value(),
              int_span[i]);
  }
}
// Test logs PopulateRequest groups records by resource and instrumentation scope
TEST(OtlpLogRecordable, PopulateRequest)
{
  auto rec1      = std::unique_ptr<sdk::logs::Recordable>(new OtlpLogRecordable);
  auto resource1 = resource::Resource::Create({{"service.name", "one"}});
  auto inst_lib1 =
      opentelemetry::sdk::instrumentationscope::InstrumentationScope::Create("one", "1");
  rec1->SetResource(resource1);
  rec1->SetInstrumentationScope(*inst_lib1);

  auto rec2      = std::unique_ptr<sdk::logs::Recordable>(new OtlpLogRecordable);
  auto resource2 = resource::Resource::Create({{"service.name", "two"}});
  auto inst_lib2 =
      opentelemetry::sdk::instrumentationscope::InstrumentationScope::Create("two", "2");
  rec2->SetResource(resource2);
  rec2->SetInstrumentationScope(*inst_lib2);

  // Same resource as rec2, different scope
  auto rec3 = std::unique_ptr<sdk::logs::Recordable>(new OtlpLogRecordable);
  auto inst_lib3 =
      opentelemetry::sdk::instrumentationscope::InstrumentationScope::Create("three", "3");
  rec3->SetResource(resource2);
  rec3->SetInstrumentationScope(*inst_lib3);

  proto::collector::logs::v1::ExportLogsServiceRequest req;
  std::vector<std::unique_ptr<sdk::logs::Recordable>> logs;
  logs.push_back(std::move(rec1));
  logs.push_back(std::move(rec2));
  logs.push_back(std::move(rec3));
  const nostd::span<std::unique_ptr<sdk::logs::Recordable>, 3> logs_span(logs.data(), 3);
  OtlpRecordableUtils::PopulateRequest(logs_span, &req);

  EXPECT_EQ(req.resource_logs_size(), 2);
  for (const auto &resource_logs : req.resource_logs())
  {
    ASSERT_GT(resource_logs.resource().attributes_size(), 0);
    const auto service_name    = resource_logs.resource().attributes(0).value().string_value();
    const auto scope_logs_size = resource_logs.scope_logs_size();
    if (service_name == "one")
    {
      EXPECT_EQ(scope_logs_size, 1);
      EXPECT_EQ(resource_logs.scope_logs(0).scope().name(), "one");
    }
    if (service_name == "two")
    {
      EXPECT_EQ(scope_logs_size, 2);
    }
  }
}

// Test logs PopulateRequest handles missing resource and scope gracefully
TEST(OtlpLogRecordable, PopulateRequestMissing)
{
  // Missing scope (no SetInstrumentationScope call)
  auto rec1      = std::unique_ptr<sdk::logs::Recordable>(new OtlpLogRecordable);
  auto resource1 = resource::Resource::Create({{"service.name", "one"}});
  rec1->SetResource(resource1);

  // Missing resource (no SetResource call)
  auto rec2 = std::unique_ptr<sdk::logs::Recordable>(new OtlpLogRecordable);
  auto inst_lib2 =
      opentelemetry::sdk::instrumentationscope::InstrumentationScope::Create("two", "2");
  rec2->SetInstrumentationScope(*inst_lib2);

  proto::collector::logs::v1::ExportLogsServiceRequest req;
  std::vector<std::unique_ptr<sdk::logs::Recordable>> logs;
  logs.push_back(std::move(rec1));
  logs.push_back(std::move(rec2));
  const nostd::span<std::unique_ptr<sdk::logs::Recordable>, 2> logs_span(logs.data(), 2);
  OtlpRecordableUtils::PopulateRequest(logs_span, &req);

  EXPECT_EQ(req.resource_logs_size(), 2);
  for (const auto &resource_logs : req.resource_logs())
  {
    // Both should produce exactly one scope_logs entry
    EXPECT_EQ(resource_logs.scope_logs_size(), 1);
  }
}

// Test logs PopulateRequest ignores a null request pointer
TEST(OtlpLogRecordable, PopulateRequestNullRequest)
{
  auto rec1      = std::unique_ptr<sdk::logs::Recordable>(new OtlpLogRecordable);
  auto resource1 = resource::Resource::Create({{"service.name", "one"}});
  rec1->SetResource(resource1);

  std::vector<std::unique_ptr<sdk::logs::Recordable>> logs;
  logs.push_back(std::move(rec1));
  const nostd::span<std::unique_ptr<sdk::logs::Recordable>, 1> logs_span(logs.data(), 1);

  // Should not crash when request is null
  OtlpRecordableUtils::PopulateRequest(logs_span, nullptr);
}

// Test logs PopulateRequest deduplicates scope by value when pointer identities differ
TEST(OtlpLogRecordable, PopulateRequestSameScope)
{
  auto resource = resource::Resource::Create({{"service.name", "same"}});

  // Two independent InstrumentationScope objects with identical values
  auto inst_lib_a =
      opentelemetry::sdk::instrumentationscope::InstrumentationScope::Create("lib", "1.0");
  auto inst_lib_b =
      opentelemetry::sdk::instrumentationscope::InstrumentationScope::Create("lib", "1.0");

  auto rec1 = std::unique_ptr<sdk::logs::Recordable>(new OtlpLogRecordable);
  rec1->SetResource(resource);
  rec1->SetInstrumentationScope(*inst_lib_a);

  auto rec2 = std::unique_ptr<sdk::logs::Recordable>(new OtlpLogRecordable);
  rec2->SetResource(resource);
  rec2->SetInstrumentationScope(*inst_lib_b);

  proto::collector::logs::v1::ExportLogsServiceRequest req;
  std::vector<std::unique_ptr<sdk::logs::Recordable>> logs;
  logs.push_back(std::move(rec1));
  logs.push_back(std::move(rec2));
  const nostd::span<std::unique_ptr<sdk::logs::Recordable>, 2> logs_span(logs.data(), 2);
  OtlpRecordableUtils::PopulateRequest(logs_span, &req);

  // One resource, one scope (deduplicated by value), two log records
  ASSERT_EQ(req.resource_logs_size(), 1);
  ASSERT_EQ(req.resource_logs(0).scope_logs_size(), 1);
  EXPECT_EQ(req.resource_logs(0).scope_logs(0).log_records_size(), 2);
  EXPECT_EQ(req.resource_logs(0).scope_logs(0).scope().name(), "lib");
}
}  // namespace otlp
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
