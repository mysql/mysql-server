# Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

## After the checks, lets generate

IF (WIN32)
    # Set the Windows specific and SUN RPC include dirs
    SET(WINDEPS_INCLUDE_DIRS ${XCOM_BASEDIR}/windeps/include
        CACHE PATH "windows dependencies include dir")

    SET(SUNRPC_INCLUDE_DIRS
        ${XCOM_BASEDIR}/windeps/sunrpc
        ${XCOM_BASEDIR}/windeps/sunrpc/rpc
        CACHE PATH "sunrpc include dirs")

    SET(SUNRPC_SRCS
        ${XCOM_BASEDIR}/windeps/sunrpc/xdr_float.c
        ${XCOM_BASEDIR}/windeps/sunrpc/xdr_ref.c
        ${XCOM_BASEDIR}/windeps/sunrpc/xdr_array.c
        ${XCOM_BASEDIR}/windeps/sunrpc/xdr_sizeof.c
        ${XCOM_BASEDIR}/windeps/sunrpc/xdr_mem.c
        ${XCOM_BASEDIR}/windeps/sunrpc/xdr.c
    )
ENDIF()

# Generate the RPC files if needed
FOREACH(X xcom_vp)
  SET (gen_xdr_dir ${CMAKE_CURRENT_BINARY_DIR}/xdr_gen)

  # clean up the generated files
  FILE(REMOVE ${gen_xdr_dir})
  FILE(MAKE_DIRECTORY ${gen_xdr_dir})

  # we are generating and/or copying the original files to
  # gen_xdr_dir
  INCLUDE_DIRECTORIES(${gen_xdr_dir})

  # "copied" files
  SET (x_tmp_plat_h ${gen_xdr_dir}/${X}_platform.h)
  SET (x_tmp_x      ${gen_xdr_dir}/${X}.x)
  # we need a canonical name, so that rpcgen generates the
  # C source with relative includes paths
  SET (x_tmp_x_canonical_name ${X}.x)

  # generated or copied files
  SET (x_gen_h      ${gen_xdr_dir}/${X}.h)
  SET (x_gen_c      ${gen_xdr_dir}/${X}_xdr.c)

  # original files that we are copying or generating from
  SET (x_vanilla_x      ${XCOM_BASEDIR}/${X}.x)
  SET (x_vanilla_plat_h ${XCOM_BASEDIR}/${X}_platform.h.gen)
  SET (x_vanilla_h      ${XCOM_BASEDIR}/${X}.h.gen)
  SET (x_vanilla_c      ${XCOM_BASEDIR}/${X}_xdr.c.gen)

  IF(WIN32)
    # on windows system's there is no rpcgen, thence copy
    # the files in the source directory
    ADD_CUSTOM_COMMAND(OUTPUT ${x_gen_h} ${x_gen_c} ${x_tmp_plat_h}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
                           ${XCOM_BASEDIR}/xcom_proto_enum.h
                            ${gen_xdr_dir}/xcom_proto_enum.h
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
                               ${XCOM_BASEDIR}/xcom_limits.h
                                ${gen_xdr_dir}/xcom_limits.h
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                   ${x_vanilla_h} ${x_gen_h}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                   ${x_vanilla_c} ${x_gen_c}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
                         ${x_vanilla_plat_h} ${x_tmp_plat_h}
      DEPENDS
        ${x_vanilla_h}
        ${x_vanilla_c}
        ${x_vanilla_plat_h}
        ${XCOM_BASEDIR}/xcom_proto_enum.h
        ${XCOM_BASEDIR}/xcom_limits.h)
  ELSE()
    FIND_PROGRAM(RPCGEN_EXECUTABLE rpcgen DOC "path to the rpcgen executable")
    MARK_AS_ADVANCED(RPCGEN_EXECUTABLE)
    IF(NOT RPCGEN_EXECUTABLE)
      MESSAGE(FATAL_ERROR "Could not find rpcgen")
    ENDIF()

    IF(NOT RPC_INCLUDE_DIR)
      MESSAGE(FATAL_ERROR
        "Could not find rpc/rpc.h in /usr/include or /usr/include/tirpc")
    ENDIF()
    MESSAGE(STATUS "RPC_INCLUDE_DIR ${RPC_INCLUDE_DIR}")
    IF(RPC_INCLUDE_DIR STREQUAL "/usr/include/tirpc")
      INCLUDE_DIRECTORIES(SYSTEM /usr/include/tirpc)
      ADD_DEFINITIONS(-DHAVE_TIRPC)
      SET(TIRPC_LIBRARY tirpc)
    ENDIF()

    # on unix systems try to generate them if needed
    ADD_CUSTOM_COMMAND(OUTPUT ${x_gen_h} ${x_gen_c} ${x_tmp_plat_h}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                       ${XCOM_BASEDIR}/xcom_proto_enum.h
                                       ${gen_xdr_dir}/xcom_proto_enum.h
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                           ${XCOM_BASEDIR}/xcom_limits.h
                                           ${gen_xdr_dir}/xcom_limits.h
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                               ${x_vanilla_x} ${x_tmp_x}
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                               ${x_vanilla_plat_h}
                                               ${x_tmp_plat_h}

       # generate the sources
       COMMAND ${CMAKE_COMMAND} -E remove -f ${x_gen_h}
       COMMAND ${RPCGEN_EXECUTABLE}  -C -h -o
                    ${x_gen_h} ${x_tmp_x_canonical_name}
       COMMAND ${CMAKE_COMMAND} -E remove -f ${x_gen_c}
                COMMAND ${RPCGEN_EXECUTABLE}  -C -c -o
                ${x_gen_c} ${x_tmp_x_canonical_name}
       WORKING_DIRECTORY ${gen_xdr_dir}
       DEPENDS
         ${x_vanilla_x}
         ${x_vanilla_plat_h}
         ${XCOM_BASEDIR}/xcom_proto_enum.h
         ${XCOM_BASEDIR}/xcom_limits.h)

  ENDIF()

  SET(GEN_RPC_H_FILES ${GEN_RPC_H_FILES} ${x_gen_h})
  SET(GEN_RPC_C_FILES ${GEN_RPC_C_FILES} ${x_gen_c})

  ADD_CUSTOM_TARGET(gen_${X}_h DEPENDS ${x_gen_h})
  ADD_CUSTOM_TARGET(gen_${X}_c DEPENDS ${x_gen_h} ${x_gen_c})

  UNSET (x_tmp_plat_h)
  UNSET (x_tmp_x)
  UNSET (x_tmp_x_canonical_name)

  UNSET (x_gen_h)
  UNSET (x_gen_c)

  UNSET (x_vanilla_plat_h)
  UNSET (x_vanilla_x)
  UNSET (x_vanilla_h)
  UNSET (x_vanilla_c)
ENDFOREACH(X)

# export the list of files
SET(XCOM_RPCGEN_SOURCES ${GEN_RPC_C_FILES})
SET(XCOM_RPCGEN_HEADERS ${GEN_RPC_H_FILES})
SET(XCOM_SUNRPC_SOURCES ${SUNRPC_SRCS})
SET(XCOM_WINDEPS_INCLUDE_DIRS
      ${WINDEPS_INCLUDE_DIRS}
      ${SUNRPC_INCLUDE_DIRS})
