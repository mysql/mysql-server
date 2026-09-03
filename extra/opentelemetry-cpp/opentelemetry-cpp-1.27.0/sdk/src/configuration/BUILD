# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "configuration",
    srcs = glob(["**/*.cc"]),
    include_prefix = "src/configuration",
    deps = [
        "//api",
        "//sdk:headers",
        "//sdk/src/common:wildcard_match",
        "@rapidyaml",
    ],
)
