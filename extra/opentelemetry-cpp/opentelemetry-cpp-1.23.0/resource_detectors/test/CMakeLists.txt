# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

add_executable(resource_detector_test container_detector_test.cc
                                      process_detector_test.cc)

# Link the required dependencies
target_link_libraries(
  resource_detector_test PRIVATE opentelemetry_resource_detectors
                                 opentelemetry_api GTest::gtest_main)

gtest_add_tests(
  TARGET resource_detector_test
  TEST_PREFIX resource_detector.
  TEST_LIST resource_detector_test)
