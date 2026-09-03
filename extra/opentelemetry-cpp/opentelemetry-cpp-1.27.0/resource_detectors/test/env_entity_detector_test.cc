// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <cstdlib>
#include <string>
#include <unordered_map>

#include "opentelemetry/nostd/variant.h"
#include "opentelemetry/resource_detectors/env_entity_detector.h"
#include "opentelemetry/sdk/resource/resource.h"

#if defined(_MSC_VER)
#  include "opentelemetry/sdk/common/env_variables.h"
using opentelemetry::sdk::common::setenv;
using opentelemetry::sdk::common::unsetenv;
#endif

using namespace opentelemetry::resource_detector;
namespace nostd = opentelemetry::nostd;

#ifndef NO_GETENV
TEST(EnvEntityDetectorTest, Basic)
{
  setenv("OTEL_ENTITIES", "service{service.name=my-app,service.instance.id=instance-1}", 1);

  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  EXPECT_TRUE(received_attributes.find("service.name") != received_attributes.end());
  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("service.name")), "my-app");
  EXPECT_TRUE(received_attributes.find("service.instance.id") != received_attributes.end());
  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("service.instance.id")), "instance-1");

  unsetenv("OTEL_ENTITIES");
}

TEST(EnvEntityDetectorTest, WithDescriptiveAttributes)
{
  setenv("OTEL_ENTITIES",
         "service{service.name=my-app,service.instance.id=instance-1}[service.version=1.0.0]", 1);

  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("service.name")), "my-app");
  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("service.instance.id")), "instance-1");
  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("service.version")), "1.0.0");

  unsetenv("OTEL_ENTITIES");
}

TEST(EnvEntityDetectorTest, MultipleEntities)
{
  setenv("OTEL_ENTITIES",
         "service{service.name=my-app,service.instance.id=instance-1}[service.version=1.0.0];"
         "host{host.id=host-123}[host.name=web-server-01]",
         1);

  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("service.name")), "my-app");
  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("service.instance.id")), "instance-1");
  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("service.version")), "1.0.0");
  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("host.id")), "host-123");
  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("host.name")), "web-server-01");

  unsetenv("OTEL_ENTITIES");
}

TEST(EnvEntityDetectorTest, PercentEncoding)
{
  setenv("OTEL_ENTITIES", "service{service.name=my%2Capp,service.instance.id=inst-1}", 1);

  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("service.name")), "my,app");
  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("service.instance.id")), "inst-1");

  unsetenv("OTEL_ENTITIES");
}

TEST(EnvEntityDetectorTest, DuplicateEntities)
{
  setenv("OTEL_ENTITIES",
         "service{service.name=app1}[version=1.0];service{service.name=app1}[version=2.0]", 1);

  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  // Last occurrence should win
  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("service.name")), "app1");
  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("version")), "2.0");

  unsetenv("OTEL_ENTITIES");
}

TEST(EnvEntityDetectorTest, EmptyEnv)
{
  unsetenv("OTEL_ENTITIES");
  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  EXPECT_TRUE(received_attributes.empty());
}

TEST(EnvEntityDetectorTest, EmptyString)
{
  setenv("OTEL_ENTITIES", "", 1);

  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  EXPECT_TRUE(received_attributes.empty());

  unsetenv("OTEL_ENTITIES");
}

TEST(EnvEntityDetectorTest, MalformedEntity)
{
  setenv("OTEL_ENTITIES", "service{service.name=app1};invalid{syntax;service{service.name=app2}",
         1);

  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  // Should process valid entities and skip malformed ones
  EXPECT_TRUE(received_attributes.find("service.name") != received_attributes.end());
  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("service.name")), "app2");

  unsetenv("OTEL_ENTITIES");
}

TEST(EnvEntityDetectorTest, WhitespaceHandling)
{
  setenv("OTEL_ENTITIES", " ; service { service.name = app1 } ; ", 1);

  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("service.name")), "app1");

  unsetenv("OTEL_ENTITIES");
}

TEST(EnvEntityDetectorTest, EmptySemicolons)
{
  // Test: Empty strings are allowed (leading, trailing, and consecutive semicolons are ignored)
  setenv("OTEL_ENTITIES", ";service{service.name=app1};;host{host.id=host-123};", 1);

  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("service.name")), "app1");
  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("host.id")), "host-123");

  unsetenv("OTEL_ENTITIES");
}

TEST(EnvEntityDetectorTest, MissingRequiredFields)
{
  // Test: Missing required fields (type or identifying attributes) - should skip entity
  setenv("OTEL_ENTITIES", "service{};host{host.id=123}", 1);

  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  EXPECT_TRUE(received_attributes.find("service.name") == received_attributes.end());
  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("host.id")), "123");

  unsetenv("OTEL_ENTITIES");
}

TEST(EnvEntityDetectorTest, ConflictingIdentifyingAttributes)
{
  // Test: Conflicting identifying attributes - last entity wins
  setenv("OTEL_ENTITIES", "service{service.name=app1};service{service.name=app2}", 1);

  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  // Last entity should win
  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("service.name")), "app2");

  unsetenv("OTEL_ENTITIES");
}

TEST(EnvEntityDetectorTest, ConflictingDescriptiveAttributes)
{
  // Test: Conflicting descriptive attributes - last entity's value is used
  setenv("OTEL_ENTITIES",
         "service{service.name=app1}[version=1.0];service{service.name=app2}[version=2.0]", 1);

  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("service.name")), "app2");
  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("version")), "2.0");

  unsetenv("OTEL_ENTITIES");
}

TEST(EnvEntityDetectorTest, KubernetesPod)
{
  // Test: Kubernetes pod entity example from spec
  setenv("OTEL_ENTITIES",
         "k8s.pod{k8s.pod.uid=pod-abc123}[k8s.pod.name=my-pod,k8s.pod.label.app=my-app]", 1);

  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("k8s.pod.uid")), "pod-abc123");
  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("k8s.pod.name")), "my-pod");
  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("k8s.pod.label.app")), "my-app");

  unsetenv("OTEL_ENTITIES");
}

TEST(EnvEntityDetectorTest, ContainerWithHost)
{
  // Test: Container with host (minimal descriptive attributes)
  setenv("OTEL_ENTITIES",
         "container{container.id=cont-456};host{host.id=host-789}[host.name=docker-host]", 1);

  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("container.id")), "cont-456");
  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("host.id")), "host-789");
  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("host.name")), "docker-host");

  unsetenv("OTEL_ENTITIES");
}

TEST(EnvEntityDetectorTest, MinimalEntity)
{
  // Test: Minimal entity (only required fields)
  setenv("OTEL_ENTITIES", "service{service.name=minimal-app}", 1);

  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("service.name")), "minimal-app");
  EXPECT_EQ(received_attributes.size(), 1);

  unsetenv("OTEL_ENTITIES");
}

TEST(EnvEntityDetectorTest, PercentEncodingMultiple)
{
  // Test: Multiple percent-encoded characters in one value
  setenv("OTEL_ENTITIES",
         "service{service.name=my%2Capp,service.instance.id=inst-1}[config=key%3Dvalue%5Bprod%5D]",
         1);

  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("service.name")), "my,app");
  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("config")), "key=value[prod]");

  unsetenv("OTEL_ENTITIES");
}

TEST(EnvEntityDetectorTest, SchemaUrlRelativePath)
{
  // Test: Relative path schema URL should be accepted
  setenv("OTEL_ENTITIES", "service{service.name=app1}@schemas/1.21.0", 1);

  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  // Entity should be processed and relative schema URL accepted
  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("service.name")), "app1");
  EXPECT_EQ(resource.GetSchemaURL(), "schemas/1.21.0");

  unsetenv("OTEL_ENTITIES");
}

TEST(EnvEntityDetectorTest, InvalidSchemaUrlEmptyScheme)
{
  setenv("OTEL_ENTITIES", "service{service.name=app1}@://example.com/schemas/1.0.0", 1);

  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  // Entity should be processed but schema URL ignored (invalid scheme)
  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("service.name")), "app1");
  EXPECT_TRUE(resource.GetSchemaURL().empty());

  unsetenv("OTEL_ENTITIES");
}

TEST(EnvEntityDetectorTest, InvalidSchemaUrlInvalidScheme)
{
  setenv("OTEL_ENTITIES", "service{service.name=app1}@123://example.com/schemas/1.0.0", 1);

  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  // Entity should be processed but schema URL ignored (scheme must start with letter)
  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("service.name")), "app1");
  EXPECT_TRUE(resource.GetSchemaURL().empty());

  unsetenv("OTEL_ENTITIES");
}

TEST(EnvEntityDetectorTest, AllMalformedEntities)
{
  // Test: All entities are malformed - ParseEntities returns empty, should return empty resource
  setenv("OTEL_ENTITIES", "invalid{syntax};{missing-type};123{invalid-type}", 1);

  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  // All entities are invalid, so parsed_entities.empty() is true, should return empty resource
  EXPECT_TRUE(received_attributes.empty());

  unsetenv("OTEL_ENTITIES");
}

TEST(EnvEntityDetectorTest, EmptySchemaUrl)
{
  // Test: Empty schema URL - should log warning and ignore URL
  setenv("OTEL_ENTITIES", "service{service.name=app1}@", 1);

  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  // Entity should be processed but empty schema URL ignored
  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("service.name")), "app1");

  unsetenv("OTEL_ENTITIES");
}

TEST(EnvEntityDetectorTest, ValidSchemaUrlAbsolute)
{
  // Test: Valid absolute schema URL with "://" - should be accepted
  setenv("OTEL_ENTITIES", "service{service.name=app1}@https://opentelemetry.io/schemas/1.0.0", 1);

  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  // Entity should be processed and schema URL should be valid (not cleared)
  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("service.name")), "app1");
  EXPECT_EQ(resource.GetSchemaURL(), "https://opentelemetry.io/schemas/1.0.0");
  unsetenv("OTEL_ENTITIES");
}

TEST(EnvEntityDetectorTest, ValidSchemaUrlRelative)
{
  setenv("OTEL_ENTITIES", "service{service.name=app1}@/schemas/1.21.0", 1);

  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("service.name")), "app1");
  EXPECT_EQ(resource.GetSchemaURL(), "/schemas/1.21.0");
  unsetenv("OTEL_ENTITIES");
}

TEST(EnvEntityDetectorTest, ValidSchemaUrlHttp)
{
  setenv("OTEL_ENTITIES", "service{service.name=app1}@http://example.com/schemas/1.0.0", 1);

  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("service.name")), "app1");
  EXPECT_EQ(resource.GetSchemaURL(), "http://example.com/schemas/1.0.0");
  unsetenv("OTEL_ENTITIES");
}

TEST(EnvEntityDetectorTest, SchemaUrlWithWhitespace)
{
  // Test: Schema URL with only whitespace - should be treated as empty after trim
  setenv("OTEL_ENTITIES", "service{service.name=app1}@   ", 1);

  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  // Entity should be processed but whitespace-only schema URL should be ignored
  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("service.name")), "app1");

  unsetenv("OTEL_ENTITIES");
}

TEST(EnvEntityDetectorTest, MissingClosingBracket)
{
  // Test: Missing closing bracket ']' for desc_attrs - should be rejected
  setenv("OTEL_ENTITIES", "service{service.name=app1}[version=1.0", 1);

  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  // Entity with missing closing bracket should be rejected
  EXPECT_TRUE(received_attributes.empty());

  unsetenv("OTEL_ENTITIES");
}

TEST(EnvEntityDetectorTest, MissingClosingBrace)
{
  // Test: Missing closing brace '}' for id_attrs - should be rejected
  setenv("OTEL_ENTITIES", "service{service.name=app1", 1);

  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  // Entity with missing closing brace should be rejected
  EXPECT_TRUE(received_attributes.empty());

  unsetenv("OTEL_ENTITIES");
}

TEST(EnvEntityDetectorTest, InvalidTypeCharacters)
{
  // Test: Type with invalid characters (not alphanumeric, '.', '_', or '-')
  setenv("OTEL_ENTITIES", "service@name{service.name=app1}", 1);

  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  // Entity with invalid type characters should be rejected
  EXPECT_TRUE(received_attributes.empty());

  unsetenv("OTEL_ENTITIES");
}

TEST(EnvEntityDetectorTest, EmptyKeyInAttributes)
{
  // Test: Key-value pair with empty key after trimming - should be skipped
  setenv("OTEL_ENTITIES", "service{=value,service.name=app1, =another}", 1);

  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  EXPECT_EQ(nostd::get<std::string>(received_attributes.at("service.name")), "app1");
  // Empty key entries should not appear in attributes
  EXPECT_EQ(received_attributes.size(), 1);

  unsetenv("OTEL_ENTITIES");
}

TEST(EnvEntityDetectorTest, EmptyEntityString)
{
  // Test: Only empty entity strings - should all be skipped and return empty resource
  setenv("OTEL_ENTITIES", ";;;", 1);

  EnvEntityDetector detector;
  auto resource                   = detector.Detect();
  const auto &received_attributes = resource.GetAttributes();

  // All empty strings should be skipped, resulting in empty resource
  EXPECT_TRUE(received_attributes.empty());

  unsetenv("OTEL_ENTITIES");
}
#endif
