# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "fork",
    srcs = select({
        "//bazel:windows": ["fork_windows.cc"],
        "//conditions:default": ["fork_unix.cc"],
    }),
    hdrs = [
        "fork.h",
    ],
    include_prefix = "src/common/platform",
    linkopts = select({
        "//bazel:windows": [],
        "//conditions:default": ["-pthread"],
    }),
    deps = [
        "//api",
    ],
)
