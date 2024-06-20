# Use as a Bazel Dependency

To use libcbor in your
[Baze](https://bazel.build/)
project, first add the following section to your project's `WORKSPACE` file.
Note the location of the `third_party/libcbor.BUILD` file - you may use a
different location if you wish, but you the file must be make available to
`WORKSPACE`.

## WORKSPACE

Note, this imports version `0.8.0` - you may need to update the version and
the sha256 hash.

```python
# libcbor
http_archive(
    name = "libcbor",
    build_file = "//third_party:libcbor.BUILD",
    sha256 = "dd04ea1a7df484217058d389e027e7a0143a4f245aa18a9f89a5dd3e1a4fcc9a",
    strip_prefix = "libcbor-0.8.0",
    urls = ["https://github.com/PJK/libcbor/archive/refs/tags/v0.8.0.zip"],
)
```

## third_party/libcbor.BUILD

Bazel will unzip the libcbor zip file, then copy this file in as `BUILD`.
Bazel will then use this file to compile libcbor.
[Cmake](https://cmake.org/)
is used in two passes: to create the Makefiles, and then to invoke Make to build
the `libcbor.a` static library. `libcbor.a` and the `.h` files are then made
available for other packages to use.

```python
genrule(
  name = "cbor_cmake",
  srcs = glob(["**"]),
  outs = ["libcbor.a", "cbor.h", "cbor/arrays.h", "cbor/bytestrings.h",
          "cbor/callbacks.h", "cbor/cbor_export.h", "cbor/common.h", "cbor/configuration.h", "cbor/data.h",
          "cbor/encoding.h", "cbor/floats_ctrls.h", "cbor/ints.h", "cbor/maps.h",
          "cbor/serialization.h", "cbor/streaming.h", "cbor/strings.h", "cbor/tags.h"],
  cmd = " && ".join([
    # Remember where output should go.
    "INITIAL_WD=`pwd`",
    # Build libcbor library.
    "cd `dirname $(location CMakeLists.txt)`",
    "cmake -DCMAKE_BUILD_TYPE=Release .",
    "cmake --build .",
    # Export the .a and .h files for cbor rule, below.
    "cp src/libcbor.a src/cbor.h $$INITIAL_WD/$(RULEDIR)",
    "cp src/cbor/*h cbor/configuration.h $$INITIAL_WD/$(RULEDIR)/cbor"]),
  visibility = ["//visibility:private"],
)

cc_import(
  name = "cbor",
  hdrs = ["cbor.h", "cbor/arrays.h", "cbor/bytestrings.h",
          "cbor/callbacks.h", "cbor/cbor_export.h", "cbor/common.h", "cbor/configuration.h", "cbor/data.h",
          "cbor/encoding.h", "cbor/floats_ctrls.h", "cbor/ints.h", "cbor/maps.h",
          "cbor/serialization.h", "cbor/streaming.h", "cbor/strings.h", "cbor/tags.h"],
  static_library = "libcbor.a",
  visibility = ["//visibility:public"],
)
```

## third_party/BUILD

The `libcbor.BUILD` file must be make available to the top-level `WORKSPACE`
file:

```python
exports_files(["libcbor.BUILD"]))
```

## Your BUILD File

Add libcbor dependency to your package's `BUILD` file like so:

```python
cc_library(
    name = "...",
    srcs = [ ... ],
    hdrs = [ ... ],
    deps = [
        ...
        "@libcbor//:cbor",
    ],
)
```

## Your C File

Now you may simply include `cbor.h`:

```c
#include "cbor.h"

static const uint8_t version = cbor_major_version;
```
