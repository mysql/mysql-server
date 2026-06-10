// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <stdio.h>
#include <string.h>
#include <string>

#include "opentelemetry/version.h"

TEST(VersionTest, Consistency)
{
  char expected[10];
  snprintf(expected, sizeof(expected), "%d.%d.%d", OPENTELEMETRY_VERSION_MAJOR,
           OPENTELEMETRY_VERSION_MINOR, OPENTELEMETRY_VERSION_PATCH);

  std::string actual = OPENTELEMETRY_VERSION;
  /* OPENTELEMETRY_VERSION may contain a -dev suffix */
  std::string prefix = actual.substr(0, strlen(expected));
  EXPECT_EQ(prefix, expected);
}
