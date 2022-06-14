# Copyright (c) 2018 Yubico AB. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

add_custom_target(regress ALL)

macro(add_regress_test NAME SOURCES)
	add_executable(${NAME} ${SOURCES})
	target_link_libraries(${NAME} fido2_shared)
	add_custom_command(TARGET regress POST_BUILD COMMAND ${NAME}
		DEPENDS ${NAME})
endmacro()

add_regress_test(regress_cred cred.c)
add_regress_test(regress_assert assert.c)
add_regress_test(regress_dev dev.c)
