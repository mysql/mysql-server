# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

load("@rules_cc//cc:cc_test.bzl", "cc_test")

cc_test(
    name = "provider_test",
    srcs = [
        "provider_test.cc",
    ],
    tags = [
        "api",
        "logs",
        "test",
    ],
    deps = [
        "//api",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "logger_test",
    srcs = [
        "logger_test.cc",
    ],
    tags = [
        "api",
        "logs",
        "test",
    ],
    deps = [
        "//api",
        "@com_google_googletest//:gtest_main",
    ],
)
