cc_library(
    name = "cbor",
    srcs = glob([
        "src/**/*.h",
        "src/**/*.c",
    ]),
    hdrs = [
        "cbor.h",
    ] + glob([
        "cbor/*.h",
    ]),
    includes = [
        "src",
        "src/cbor",
        "src/cbor/internal",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@libcbor_bazel_example//third_party/libcbor:config",
    ],
)
