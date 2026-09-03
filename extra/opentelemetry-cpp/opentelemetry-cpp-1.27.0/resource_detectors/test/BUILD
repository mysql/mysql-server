# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

load("@rules_cc//cc:cc_test.bzl", "cc_test")

cc_test(
    name = "resource_detector_test",
    srcs = [
        "container_detector_test.cc",
        "env_entity_detector_test.cc",
        "process_detector_test.cc",
    ],
    tags = ["test"],
    deps = [
        "//api",
        "//resource_detectors",
        "//sdk/src/resource",
        "@com_google_googletest//:gtest_main",
    ],
)
