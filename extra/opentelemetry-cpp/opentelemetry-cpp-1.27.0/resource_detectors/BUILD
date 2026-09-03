# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "headers",
    hdrs = glob(["include/**/*.h"]),
    strip_include_prefix = "include",
)

cc_library(
    name = "resource_detectors",
    srcs = [
        "src/container_detector.cc",
        "src/container_detector_utils.cc",
        "src/env_entity_detector.cc",
        "src/process_detector.cc",
        "src/process_detector_utils.cc",
    ],
    copts = ["-fexceptions"],
    deps = [
        "//api",
        "//resource_detectors:headers",
        "//sdk:headers",
        "//sdk/src/resource",
    ],
)
