Template:
- [Fix issue X in feature Y](https://github.com/PJK/libcbor/pull/XXX) (by [YYY](https://github.com/YYY))

Next
---------------------

0.11.0 (2024-02-04)
---------------------
- [Updated documentation to refer to RFC 8949](https://github.com/PJK/libcbor/issues/269)
- Improvements to `cbor_describe`
  - [Bytestring data will now be printed as well](https://github.com/PJK/libcbor/pull/281) by  [akallabeth](https://github.com/akallabeth)
  - [Formatting consistency and clarity improvements](https://github.com/PJK/libcbor/pull/285)
- [Fix `cbor_string_set_handle` not setting the codepoint count](https://github.com/PJK/libcbor/pull/286)
- BREAKING: [`cbor_load` will no longer fail on input strings that are well-formed but not valid UTF-8](https://github.com/PJK/libcbor/pull/286)
  - If you were relying on the validation, please check the result using `cbor_string_codepoint_count` instead 
- BREAKING: [All decoders like `cbor_load` and `cbor_stream_decode` will accept all well-formed tag values](https://github.com/PJK/libcbor/pull/308) (bug discovered by [dskern-github](https://github.com/dskern-github))
  - Previously, decoding of certain values would fail with `CBOR_ERR_MALFORMATED` or `CBOR_DECODER_ERROR`
  - This also makes decoding symmetrical with serialization, which already accepts all values

0.10.2 (2023-01-31)
---------------------
- [Fixed minor test bug causing failures for x86 Linux](https://github.com/PJK/libcbor/pull/266) (discovered by [trofi](https://github.com/PJK/libcbor/issues/263))
  - Actual libcbor functionality not affected, bug was in the test suite
- [Made tests platform-independent](https://github.com/PJK/libcbor/pull/272)

0.10.1 (2022-12-30)
---------------------
- [Fix a regression in `cbor_serialize_alloc` that caused serialization of zero-length strings and bytestrings or byte/strings with zero-length chunks to fail](https://github.com/PJK/libcbor/pull/260) (discovered by [martelletto](https://github.com/martelletto))

0.10.0 (2022-12-29)
---------------------
- Make the buffer_size optional in `cbor_serialize_alloc` [[#205]](https://github.com/PJK/libcbor/pull/205) (by [hughsie](https://github.com/hughsie))
- BREAKING: Improved half-float encoding for denormalized numbers. [[#208]](https://github.com/PJK/libcbor/pull/208) (by [ranvis](https://github.com/ranvis))
  - Denormalized half-floats will now preserve data in the mantissa
  - Note: Half-float NaNs still lose data (https://github.com/PJK/libcbor/issues/215)
- BUILD BREAKING: Minimum CMake version is 3.0 [[#201]](https://github.com/PJK/libcbor/pull/201) (by [thewtex@](https://github.com/thewtex))
  - See https://repology.org/project/cmake/versions for support; the vast majority of users should not be affected.
- Fix a potential memory leak when the allocator fails during array or map decoding [[#224]](https://github.com/PJK/libcbor/pull/224) (by [James-ZHANG](https://github.com/James-ZHANG))
- [Fix a memory leak when the allocator fails when adding chunks to indefinite bytestrings.](https://github.com/PJK/libcbor/pull/242) ([discovered](https://github.com/PJK/libcbor/pull/228) by [James-ZHANG](https://github.com/James-ZHANG))
- [Fix a memory leak when the allocator fails when adding chunks to indefinite strings](https://github.com/PJK/libcbor/pull/246)
- Potentially BUILD BREAKING: [Add nodiscard attributes to most functions](https://github.com/PJK/libcbor/pull/248)
  - **Warning**: This may cause new build warnings and (in rare cases, depending on your configuration) errors
- BREAKING: [Fix `cbor_copy` leaking memory and creating invalid items when the allocator fails](https://github.com/PJK/libcbor/pull/249).
  - Previously, the failures were not handled in the interface. Now, `cbor_copy` may return `NULL` upon failure; clients should check the return value
- [Fix `cbor_build_tag` illegal memory behavior when the allocator fails](https://github.com/PJK/libcbor/pull/249)
- [Add a new `cbor_serialized_size` API](https://github.com/PJK/libcbor/pull/250)
- [Reworked `cbor_serialize_alloc` to allocate the exact amount of memory necessary upfront](https://github.com/PJK/libcbor/pull/251)
  - This should significantly speed up `cbor_serialize_alloc` for large items by avoiding multiple reallocation iterations
  - Clients should not use the return value of `cbor_serialize_alloc`. It may be removed in the future.
- BUILD BREAKING: [Deprecate CBOR_CUSTOM_ALLOC](https://github.com/PJK/libcbor/pull/237)
  - `cbor_set_allocs` will always be enabled from now on
  - Note: The flag will be kept as a no-op triggering a warning when used for one version and then removed completely

0.9.0 (2021-11-14)
---------------------
- Improved pkg-config paths handling [[#164]](https://github.com/PJK/libcbor/pull/164) (by [jtojnar@](https://github.com/jtojnar))
- Use explicit math.h linkage [[#170]](https://github.com/PJK/libcbor/pull/170)
- BREAKING: Fixed handling of items that exceed the host size_t range [[#186]](https://github.com/PJK/libcbor/pull/186hg)  
    - Callbacks for bytestrings, strings, arrays, and maps use uint64_t instead of size_t to allow handling of large items that exceed size_t even if size_t < uint64_t
    - cbor_decode explicitly checks size to avoid overflows (previously broken, potentially resulting in erroneous decoding on affected systems)
    - The change should be a noop for 64b systems 
- Added a [Bazel](https://bazel.build/) build example [[#196]](https://github.com/PJK/libcbor/pull/196) (by [andyjgf@](https://github.com/andyjgf))

0.8.0 (2020-09-20)
---------------------
- BUILD BREAKING: Use BUILD_SHARED_LIBS to determine how to build libraries (fixed Windows linkage) [[#148]](https://github.com/PJK/libcbor/pull/148) (by [intelligide@](https://github.com/intelligide))
- BREAKING: Fix `cbor_tag_item` not increasing the reference count on the tagged item reference it returns [[Fixes #109](https://github.com/PJK/libcbor/issues/109)] (discovered bt [JohnGilmour](https://github.com/JohnGilmour))
  - If you have previously relied on the broken behavior, you can use `cbor_move` to emulate as long as the returned handle is an "rvalue"
- BREAKING: [`CBOR_DECODER_EBUFFER` removed from `cbor_decoder_status`](https://github.com/PJK/libcbor/pull/156)
    - `cbor_stream_decode` will set `CBOR_DECODER_NEDATA` instead if the input buffer is empty
- [Fix `cbor_stream_decode`](https://github.com/PJK/libcbor/pull/156) to set `cbor_decoder_result.required` to the minimum number of input bytes necessary to receive the next callback (as long as at least one byte was passed) (discovered by [woefulwabbit](https://github.com/woefulwabbit))
- Fixed several minor manpage issues [[#159]](https://github.com/PJK/libcbor/pull/159) (discovered by [kloczek@](https://github.com/kloczek))

0.7.0 (2020-04-25)
---------------------
- Fix bad encoding of NaN half-floats [[Fixes #53]](https://github.com/PJK/libcbor/issues/53) (discovered by [BSipos-RKF](https://github.com/BSipos-RKF))
    - **Warning**: Previous versions encoded NaNs as `0xf9e700` instead of `0xf97e00`; if you rely on the broken behavior, this will be a breaking change
- Fix potentially bad encoding of negative half-float with exponent < -14 [[Fixes #112]](https://github.com/PJK/libcbor/issues/112) (discovered by [yami36](https://github.com/yami36))
- BREAKING: Improved bool support [[Fixes #63]](https://github.com/PJK/libcbor/issues/63)
    - Rename `cbor_ctrl_is_bool` to `cbor_get_bool` and fix the behavior
    - Add `cbor_set_bool`
- Fix memory_allocation_test breaking the build without CBOR_CUSTOM_ALLOC [[Fixes #128]](https://github.com/PJK/libcbor/issues/128) (by [panlinux](https://github.com/panlinux))
- [Fix a potential build issue where cJSON includes may be misconfigured](https://github.com/PJK/libcbor/pull/132)
- Breaking: [Add a limit on the size of the decoding context stack](https://github.com/PJK/libcbor/pull/138) (by [James-ZHANG](https://github.com/James-ZHANG))
    - If your usecase requires parsing very deeply nested structures, you might need to increase the default 2k limit via `CBOR_MAX_STACK_SIZE` 
- Enable LTO/IPO based on [CheckIPOSupported](https://cmake.org/cmake/help/latest/module/CheckIPOSupported.html#module:CheckIPOSupported) [[#143]](https://github.com/PJK/libcbor/pull/143) (by [xanderlent](https://github.com/xanderlent))
    - If you rely on LTO being enabled and use CMake version older than 3.9, you will need to re-enable it manually or upgrade your CMake 

0.6.1 (2020-03-26)
---------------------
- [Fix bad shared library version number](https://github.com/PJK/libcbor/pull/131)
    - **Warning**: Shared library built from the 0.6.0 release is erroneously marked as version "0.6.0", which makes it incompatible with future releases *including the v0.6.X line* even though they may be compatible API/ABI-wise. Refer to the documentation for the new SO versioning scheme.

0.6.0 (2020-03-15)
---------------------
- Correctly set .so version [[Fixes #52]](https://github.com/PJK/libcbor/issues/52). 
    - **Warning**: All previous releases will be identified as 0.0 by the linker.
- Fix & prevent heap overflow error in example code [[#74]](https://github.com/PJK/libcbor/pull/74) [[#76]](https://github.com/PJK/libcbor/pull/76) (by @nevun)
- Correctly set OSX dynamic library version [[Fixes #75]](https://github.com/PJK/libcbor/issues/75)
- [Fix misplaced 0xFF bytes in maps possibly causing memory corruption](https://github.com/PJK/libcbor/pull/82)
- BREAKING: Fix handling & cleanup of failed memory allocation in constructor
  and builder helper functions [[Fixes #84]](https://github.com/PJK/libcbor/issues/84)
  - All cbor_new_* and cbor_build_* functions will now explicitly return NULL when memory allocation fails
  - It is up to the client to handle such cases
- Globally enforced code style [[Fixes #83]](https://github.com/PJK/libcbor/issues/83)
- Fix issue possible memory corruption bug on repeated 
  cbor_(byte)string_add_chunk calls with intermittently failing realloc calls
- Fix possibly misaligned reads and writes when endian.h is uses or when
  running on a big-endian machine [[Fixes #99](https://github.com/PJK/libcbor/issues/99), [#100](https://github.com/PJK/libcbor/issues/100)]
- [Improved CI setup with Travis-native arm64 support](https://github.com/PJK/libcbor/pull/116)
- [Docs migrated to Sphinx 2.4 and Python3](https://github.com/PJK/libcbor/pull/117)

0.5.0 (2017-02-06)
---------------------
- Remove cmocka from the subtree (always rely on system or user-provided version)
- Windows CI
- Only build tests if explicitly enabled (`-DWITH_TESTS=ON`)
- Fixed static header declarations (by cedric-d)
- Improved documentation (by Michael Richardson)
- Improved `examples/readfile.c`
- Reworked (re)allocation to handle huge inputs and overflows in size_t [[Fixes #16]](https://github.com/PJK/libcbor/issues/16)
- Improvements to C++ linkage (corrected `cbor_empty_callbacks`, fixed `restrict` pointers) (by Dennis Bijwaard)
- Fixed Linux installation directory depending on architecture [[Fixes #34]](https://github.com/PJK/libcbor/issues/34) (by jvymazal)
- Improved 32-bit support [[Fixes #35]](https://github.com/PJK/libcbor/issues/35)
- Fixed MSVC compatibility [[Fixes #31]](https://github.com/PJK/libcbor/issues/31)
- Fixed and improved half-float encoding [[Fixes #5](https://github.com/PJK/libcbor/issues/5), [#11](https://github.com/PJK/libcbor/issues/11)]

0.4.0 (2015-12-25)
---------------------
Breaks build & header compatibility due to:

- Improved build configuration and feature check macros
- Endianness configuration fixes (by Erwin Kroon and David Grigsby)
- pkg-config compatibility (by Vincent Bernat)
- enable use of versioned SONAME (by Vincent Bernat)
- better fuzzer (wasn't random until now, ooops)

0.3.1 (2015-05-21)
---------------------
- documentation and comments improvements, mostly for the API reference

0.3.0 (2015-05-21)
---------------------

- Fixes, polishing, niceties across the code base
- Updated examples
- `cbor_copy`
- `cbor_build_negint8`, 16, 32, 64, matching asserts
- `cbor_build_stringn`
- `cbor_build_tag`
- `cbor_build_float2`, ...

0.2.1 (2015-05-17)
---------------------
- C99 support

0.2.0 (2015-05-17)
---------------------

- `cbor_ctrl_bool` -> `cbor_ctrl_is_bool`
- Added `cbor_array_allocated` & map equivalent
- Overhauled endianess conversion - ARM now works as expected
- 'sort.c' example added
- Significantly improved and doxyfied documentation

0.1.0 (2015-05-06)
---------------------

The initial release, yay!
