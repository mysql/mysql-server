# Copyright The OpenTelemetry Authors
# SPDX-License-Identifier: Apache-2.0

cc_test(
    name = "resource_detector_test",
    srcs = [
        "container_detector_test.cc",
        "process_detector_test.cc",
    ],
    tags = ["test"],
    deps = [
        "//api",
        "//resource_detectors",
        "@com_google_googletest//:gtest_main",
    ],
)
