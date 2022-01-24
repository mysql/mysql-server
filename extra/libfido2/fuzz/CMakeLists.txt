# Copyright (c) 2019 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

list(APPEND COMPAT_SOURCES
	../openbsd-compat/strlcpy.c
	../openbsd-compat/strlcat.c
)

list(APPEND COMMON_SOURCES
	libfuzzer.c
	mutator_aux.c
)

# fuzz_cred
add_executable(fuzz_cred fuzz_cred.c ${COMMON_SOURCES} ${COMPAT_SOURCES})
target_compile_options(fuzz_cred PRIVATE ${FUZZ_LDFLAGS})
set_target_properties(fuzz_cred PROPERTIES LINK_FLAGS ${FUZZ_LDFLAGS})
target_link_libraries(fuzz_cred fido2_shared)

# fuzz_assert
add_executable(fuzz_assert fuzz_assert.c ${COMMON_SOURCES} ${COMPAT_SOURCES})
target_compile_options(fuzz_assert PRIVATE ${FUZZ_LDFLAGS})
set_target_properties(fuzz_assert PROPERTIES LINK_FLAGS ${FUZZ_LDFLAGS})
target_link_libraries(fuzz_assert fido2_shared)

# fuzz_mgmt
add_executable(fuzz_mgmt fuzz_mgmt.c ${COMMON_SOURCES} ${COMPAT_SOURCES})
target_compile_options(fuzz_mgmt PRIVATE ${FUZZ_LDFLAGS})
set_target_properties(fuzz_mgmt PROPERTIES LINK_FLAGS ${FUZZ_LDFLAGS})
target_link_libraries(fuzz_mgmt fido2_shared)

# fuzz_credman
add_executable(fuzz_credman fuzz_credman.c ${COMMON_SOURCES} ${COMPAT_SOURCES})
target_compile_options(fuzz_credman PRIVATE ${FUZZ_LDFLAGS})
set_target_properties(fuzz_credman PROPERTIES LINK_FLAGS ${FUZZ_LDFLAGS})
target_link_libraries(fuzz_credman fido2_shared)

# fuzz_bio
add_executable(fuzz_bio fuzz_bio.c ${COMMON_SOURCES} ${COMPAT_SOURCES})
target_compile_options(fuzz_bio PRIVATE ${FUZZ_LDFLAGS})
set_target_properties(fuzz_bio PROPERTIES LINK_FLAGS ${FUZZ_LDFLAGS})
target_link_libraries(fuzz_bio fido2_shared)
