load("@rules_cc//cc:defs.bzl", "cc_library")
load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_library(
    name = "src",
    srcs = [
        "hello.cc",
    ],
    hdrs = [
        "hello.h",
    ],
    visibility = [
        "//src:__pkg__",
    ],
    deps = [
        "@libcbor//:cbor",
    ],
)

cc_test(
    name = "tests",
    size = "small",
    srcs = [
        "hello_test.cc",
    ],
    visibility = [
        "//visibility:private",
    ],
    deps = [
        ":src",
        "@gtest//:gtest_main",
        "@libcbor//:cbor",
    ],
)


cc_binary(
    name = "hello",
    srcs = [
        "main.cc",
    ],
    deps = [
        ":src",
    ],
)

