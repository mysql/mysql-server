# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

load("@rules_cc//cc:cc_test.bzl", "cc_test")

cc_test(
    name = "hex_test",
    srcs = [
        "hex_test.cc",
    ],
    tags = [
        "api",
        "test",
        "trace",
    ],
    deps = [
        "//api",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "string_test",
    srcs = [
        "string_test.cc",
    ],
    tags = [
        "api",
        "test",
        "trace",
    ],
    deps = [
        "//api",
        "@com_google_googletest//:gtest_main",
    ],
)
