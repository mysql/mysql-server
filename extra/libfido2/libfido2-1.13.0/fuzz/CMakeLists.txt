# Copyright (c) 2019-2023 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.
# SPDX-License-Identifier: BSD-2-Clause

list(APPEND COMPAT_SOURCES
	../openbsd-compat/strlcpy.c
	../openbsd-compat/strlcat.c
)

list(APPEND COMMON_SOURCES
	libfuzzer.c
	mutator_aux.c
)

# XXX: OSS-Fuzz require linking using CXX
set(FUZZ_LINKER_LANGUAGE "C" CACHE STRING "Linker language for fuzz harnesses")
mark_as_advanced(FUZZ_LINKER_LANGUAGE)
enable_language(${FUZZ_LINKER_LANGUAGE})

# fuzz_cred
add_executable(fuzz_cred fuzz_cred.c ${COMMON_SOURCES} ${COMPAT_SOURCES})
set_target_properties(fuzz_cred PROPERTIES
	LINK_FLAGS ${FUZZ_LDFLAGS}
	LINKER_LANGUAGE ${FUZZ_LINKER_LANGUAGE})
target_link_libraries(fuzz_cred fido2_shared)

# fuzz_assert
add_executable(fuzz_assert fuzz_assert.c ${COMMON_SOURCES} ${COMPAT_SOURCES})
set_target_properties(fuzz_assert PROPERTIES
	LINK_FLAGS ${FUZZ_LDFLAGS}
	LINKER_LANGUAGE ${FUZZ_LINKER_LANGUAGE})
target_link_libraries(fuzz_assert fido2_shared)

# fuzz_mgmt
add_executable(fuzz_mgmt fuzz_mgmt.c ${COMMON_SOURCES} ${COMPAT_SOURCES})
set_target_properties(fuzz_mgmt PROPERTIES
	LINK_FLAGS ${FUZZ_LDFLAGS}
	LINKER_LANGUAGE ${FUZZ_LINKER_LANGUAGE})
target_link_libraries(fuzz_mgmt fido2_shared)

# fuzz_credman
add_executable(fuzz_credman fuzz_credman.c ${COMMON_SOURCES} ${COMPAT_SOURCES})
set_target_properties(fuzz_credman PROPERTIES
	LINK_FLAGS ${FUZZ_LDFLAGS}
	LINKER_LANGUAGE ${FUZZ_LINKER_LANGUAGE})
target_link_libraries(fuzz_credman fido2_shared)

# fuzz_bio
add_executable(fuzz_bio fuzz_bio.c ${COMMON_SOURCES} ${COMPAT_SOURCES})
set_target_properties(fuzz_bio PROPERTIES
	LINK_FLAGS ${FUZZ_LDFLAGS}
	LINKER_LANGUAGE ${FUZZ_LINKER_LANGUAGE})
target_link_libraries(fuzz_bio fido2_shared)

# fuzz_hid
add_executable(fuzz_hid fuzz_hid.c ${COMMON_SOURCES} ${COMPAT_SOURCES})
set_target_properties(fuzz_hid PROPERTIES
	LINK_FLAGS ${FUZZ_LDFLAGS}
	LINKER_LANGUAGE ${FUZZ_LINKER_LANGUAGE})
target_link_libraries(fuzz_hid fido2_shared)

# fuzz_netlink
add_executable(fuzz_netlink fuzz_netlink.c ${COMMON_SOURCES} ${COMPAT_SOURCES})
set_target_properties(fuzz_netlink PROPERTIES
	LINK_FLAGS ${FUZZ_LDFLAGS}
	LINKER_LANGUAGE ${FUZZ_LINKER_LANGUAGE})
target_link_libraries(fuzz_netlink fido2_shared)

# fuzz_largeblob
add_executable(fuzz_largeblob fuzz_largeblob.c ${COMMON_SOURCES} ${COMPAT_SOURCES})
set_target_properties(fuzz_largeblob PROPERTIES
	LINK_FLAGS ${FUZZ_LDFLAGS}
	LINKER_LANGUAGE ${FUZZ_LINKER_LANGUAGE})
target_link_libraries(fuzz_largeblob fido2_shared)

# fuzz_pcsc
add_executable(fuzz_pcsc fuzz_pcsc.c ${COMMON_SOURCES} ${COMPAT_SOURCES})
set_target_properties(fuzz_pcsc PROPERTIES
	LINK_FLAGS ${FUZZ_LDFLAGS}
	LINKER_LANGUAGE ${FUZZ_LINKER_LANGUAGE})
target_link_libraries(fuzz_pcsc fido2_shared)
