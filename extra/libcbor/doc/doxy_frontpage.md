

This is the development reference of [libcbor](https://github.com/PJK/libcbor). Looking for the [user documentation](https://libcbor.readthedocs.io/)?


# Where to start

A couple of pointers for you to start with: `0x00000000`, `0xDEADBEEF`.

If you just want to peek under the hood, have a look at:
 - \ref src/cbor/common.h
 - \ref src/cbor/encoding.h
 - \ref src/cbor.h

If you want to implement your own decoder or see how the default one is made:
 - \ref src/cbor/internal/builder_callbacks.h
 - \ref src/cbor/internal/stack.h

For details on encoding and packing (could be useful when porting to exotic platforms):
 - \ref src/cbor/internal/encoders.h
 - \ref src/cbor/internal/loaders.h

Streaming driver:
 - \ref src/cbor/streaming.h

Manipulation routines for particular types:
 - \ref src/cbor/ints.h
 - \ref src/cbor/bytestrings.h
 - \ref src/cbor/strings.h
 - \ref src/cbor/arrays.h
 - \ref src/cbor/maps.h
 - \ref src/cbor/tags.h
 - \ref src/cbor/floats_ctrls.h

# How to contribute

Please refer to [the repository](https://github.com/PJK/libcbor)
