# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "wildcard_match",
    hdrs = [
        "wildcard_match.h",
    ],
    include_prefix = "src/common",
    deps = [
        "//api",
        "//sdk:headers",
    ],
)

cc_library(
    name = "random",
    srcs = [
        "random.cc",
    ],
    hdrs = [
        "fast_random_number_generator.h",
        "random.h",
    ],
    include_prefix = "src/common",
    deps = [
        "//api",
        "//sdk:headers",
        "//sdk/src/common/platform:fork",
    ],
)

cc_library(
    name = "base64",
    srcs = [
        "base64.cc",
    ],
    include_prefix = "src/common",
    deps = [
        "//api",
        "//sdk:headers",
        "//sdk/src/common/platform:fork",
    ],
)

cc_library(
    name = "disabled",
    srcs = [
        "disabled.cc",
    ],
    include_prefix = "src/common",
    deps = [
        "//api",
        "//sdk:headers",
    ],
)

cc_library(
    name = "env_variables",
    srcs = [
        "env_variables.cc",
    ],
    deps = [
        "//api",
        "//sdk:headers",
        "//sdk/src/common:global_log_handler",
    ],
)

cc_library(
    name = "empty_attributes",
    srcs = [
        "empty_attributes.cc",
    ],
    deps = [
        "//api",
        "//sdk:headers",
    ],
)

cc_library(
    name = "global_log_handler",
    srcs = [
        "global_log_handler.cc",
    ],
    deps = [
        "//api",
        "//sdk:headers",
    ],
)
