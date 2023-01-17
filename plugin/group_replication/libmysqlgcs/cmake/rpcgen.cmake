# Copyright (c) 2015, 2023, Oracle and/or its affiliates.
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

IF(WIN32)
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

  # /wd4311 'type cast': pointer truncation from 'const caddr_t' to 'u_long'
  # /wd4312 'type cast': conversion from 'long' to 'void *' of greater size
  ADD_COMPILE_FLAGS(
    ${XCOM_BASEDIR}/windeps/sunrpc/xdr_sizeof.c
    COMPILE_FLAGS "/wd4311" "/wd4312"
    )
  ADD_COMPILE_FLAGS(
    ${XCOM_BASEDIR}/windeps/sunrpc/xdr_mem.c
    COMPILE_FLAGS "/wd4311"
    )
ENDIF()

IF(APPLE)
  # macOS 10.14 and later do not provide xdr_sizeof()
  SET(SUNRPC_SRCS
    ${XCOM_BASEDIR}/windeps/sunrpc/xdr_sizeof.c
    )
ENDIF()

# Generate the RPC files if needed
FOREACH(X xcom_vp)
  SET(gen_xdr_base ${CMAKE_CURRENT_BINARY_DIR})
  SET(gen_xdr_dir ${gen_xdr_base}/xdr_gen)

  # clean up the generated files
  FILE(REMOVE ${gen_xdr_dir})
  FILE(MAKE_DIRECTORY ${gen_xdr_dir})

  # we are generating and/or copying the original files to
  # gen_xdr_dir
  INCLUDE_DIRECTORIES(${gen_xdr_base})


  # "copied" files
  SET(x_tmp_x ${gen_xdr_dir}/${X}.x)
  # we need a canonical name, so that rpcgen generates the
  # C source with relative includes paths
  SET(x_tmp_x_canonical_name ${X}.x)

  # generated or copied files
  SET(x_gen_h ${gen_xdr_dir}/${X}.h)
  SET(x_gen_c ${gen_xdr_dir}/${X}_xdr.c)

  # temp files
  SET(x_tmp_h ${gen_xdr_dir}/${X}_tmp.h)
  SET(x_tmp_c ${gen_xdr_dir}/${X}_xdr_tmp.c)


  # original files that we are copying or generating from
  SET(x_vanilla_x ${XCOM_BASEDIR}/${X}.x)
  SET(x_vanilla_h ${XCOM_BASEDIR}/${X}.h.gen)
  SET(x_vanilla_c ${XCOM_BASEDIR}/${X}_xdr.c.gen)

  IF(NOT LINUX)
    # on windows system's there is no rpcgen, thence copy
    # the files in the source directory
    ADD_CUSTOM_COMMAND(OUTPUT ${x_gen_h} ${x_gen_c}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${x_vanilla_h} ${x_gen_h}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${x_vanilla_c} ${x_gen_c}
      DEPENDS
      ${x_vanilla_h}
      ${x_vanilla_c})
  ELSE()
    FIND_PROGRAM(RPCGEN_EXECUTABLE rpcgen DOC "path to the rpcgen executable")
    MARK_AS_ADVANCED(RPCGEN_EXECUTABLE)
    IF(NOT RPCGEN_EXECUTABLE)
      WARN_MISSING_RPCGEN_EXECUTABLE()
      MESSAGE(FATAL_ERROR "Could not find rpcgen")
    ENDIF()

    MYSQL_CHECK_RPC()

    MESSAGE(STATUS "RPC_INCLUDE_DIRS ${RPC_INCLUDE_DIRS}")

    # on unix systems try to generate them if needed
    SET(enumfix ${CMAKE_CURRENT_SOURCE_DIR}/cmake/enumfix.cmake)
    SET(versionfix ${CMAKE_CURRENT_SOURCE_DIR}/cmake/add_version_suffix.cmake)
    ADD_CUSTOM_COMMAND(OUTPUT ${x_gen_h} ${x_gen_c}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${x_vanilla_x} ${x_tmp_x}

      #
      # Generate the header file
      # The struct definitions in the header file is for the latest version only,
      # since that is the canonical representation in the code.
      # Conversion between protocol versions is taken care of by
      # the generated xdr functions.
      #
      COMMAND ${CMAKE_COMMAND} -E remove -f ${x_gen_h}
      COMMAND ${CMAKE_COMMAND} -E remove -f ${x_tmp_h}
      COMMAND ${CMAKE_COMMAND} -DRPCGEN_EXECUTABLE=${RPCGEN_EXECUTABLE} -DXCOM_PROTO_VERS=109 -Dversion="" -Drpcgen_output=${x_tmp_h} -Dx_gen_h=${x_gen_h} -Dx_tmp_x_canonical_name=${x_tmp_x_canonical_name} -P ${enumfix}

      #
      # Generate xdr functions for all versions of the xdr protocol
      # There is one invocation of the add_version_suffix.cmake script for each version,
      # except for the last version.
      # Note that the version needs to be specified twice, once as
      # -DXCOM_PROTO_VERS=<number> and once as -Dversion="string"
      # For example, 101 corresponds to the version "_1_1" suffix
      #

      COMMAND ${CMAKE_COMMAND} -E remove -f ${x_gen_c}
      COMMAND ${CMAKE_COMMAND} -E remove -f ${x_tmp_c}
      COMMAND ${CMAKE_COMMAND} -DRPCGEN_EXECUTABLE=${RPCGEN_EXECUTABLE} -DXCOM_PROTO_VERS=100 -Dversion="_1_0" -Drpcgen_output=${x_tmp_c} -Dx_gen_c=${x_gen_c} -Dx_tmp_x_canonical_name=${x_tmp_x_canonical_name} -P ${versionfix}
      COMMAND ${CMAKE_COMMAND} -DRPCGEN_EXECUTABLE=${RPCGEN_EXECUTABLE} -DXCOM_PROTO_VERS=101 -Dversion="_1_1" -Drpcgen_output=${x_tmp_c} -Dx_gen_c=${x_gen_c} -Dx_tmp_x_canonical_name=${x_tmp_x_canonical_name} -P ${versionfix}
      COMMAND ${CMAKE_COMMAND} -DRPCGEN_EXECUTABLE=${RPCGEN_EXECUTABLE} -DXCOM_PROTO_VERS=102 -Dversion="_1_2" -Drpcgen_output=${x_tmp_c} -Dx_gen_c=${x_gen_c} -Dx_tmp_x_canonical_name=${x_tmp_x_canonical_name} -P ${versionfix}
      COMMAND ${CMAKE_COMMAND} -DRPCGEN_EXECUTABLE=${RPCGEN_EXECUTABLE} -DXCOM_PROTO_VERS=103 -Dversion="_1_3" -Drpcgen_output=${x_tmp_c} -Dx_gen_c=${x_gen_c} -Dx_tmp_x_canonical_name=${x_tmp_x_canonical_name} -P ${versionfix}
      COMMAND ${CMAKE_COMMAND} -DRPCGEN_EXECUTABLE=${RPCGEN_EXECUTABLE} -DXCOM_PROTO_VERS=104 -Dversion="_1_4" -Drpcgen_output=${x_tmp_c} -Dx_gen_c=${x_gen_c} -Dx_tmp_x_canonical_name=${x_tmp_x_canonical_name} -P ${versionfix}
      COMMAND ${CMAKE_COMMAND} -DRPCGEN_EXECUTABLE=${RPCGEN_EXECUTABLE} -DXCOM_PROTO_VERS=105 -Dversion="_1_5" -Drpcgen_output=${x_tmp_c} -Dx_gen_c=${x_gen_c} -Dx_tmp_x_canonical_name=${x_tmp_x_canonical_name} -P ${versionfix}
      COMMAND ${CMAKE_COMMAND} -DRPCGEN_EXECUTABLE=${RPCGEN_EXECUTABLE} -DXCOM_PROTO_VERS=106 -Dversion="_1_6" -Drpcgen_output=${x_tmp_c} -Dx_gen_c=${x_gen_c} -Dx_tmp_x_canonical_name=${x_tmp_x_canonical_name} -P ${versionfix}
      COMMAND ${CMAKE_COMMAND} -DRPCGEN_EXECUTABLE=${RPCGEN_EXECUTABLE} -DXCOM_PROTO_VERS=107 -Dversion="_1_7" -Drpcgen_output=${x_tmp_c} -Dx_gen_c=${x_gen_c} -Dx_tmp_x_canonical_name=${x_tmp_x_canonical_name} -P ${versionfix}
      COMMAND ${CMAKE_COMMAND} -DRPCGEN_EXECUTABLE=${RPCGEN_EXECUTABLE} -DXCOM_PROTO_VERS=108 -Dversion="_1_8" -Drpcgen_output=${x_tmp_c} -Dx_gen_c=${x_gen_c} -Dx_tmp_x_canonical_name=${x_tmp_x_canonical_name} -P ${versionfix}
      #
      # The latest version is generated twice, once with the version suffix, and once without the suffix
      # To add a new version, change the two next lines so they correspond
      # to the latest version, and add a line for the previous version above
      # this comment block.
      #
      COMMAND ${CMAKE_COMMAND} -DRPCGEN_EXECUTABLE=${RPCGEN_EXECUTABLE} -DXCOM_PROTO_VERS=109 -Dversion="_1_9" -Drpcgen_output=${x_tmp_c} -Dx_gen_c=${x_gen_c} -Dx_tmp_x_canonical_name=${x_tmp_x_canonical_name} -P ${versionfix}
      COMMAND ${CMAKE_COMMAND} -DRPCGEN_EXECUTABLE=${RPCGEN_EXECUTABLE} -DXCOM_PROTO_VERS=109 -Dversion="" -Drpcgen_output=${x_tmp_c} -Dx_gen_c=${x_gen_c} -Dx_tmp_x_canonical_name=${x_tmp_x_canonical_name} -P ${versionfix}
      WORKING_DIRECTORY ${gen_xdr_dir}

      DEPENDS
      ${x_vanilla_x})
  ENDIF()

  SET(GEN_RPC_H_FILES ${GEN_RPC_H_FILES} ${x_gen_h})
  SET(GEN_RPC_C_FILES ${GEN_RPC_C_FILES} ${x_gen_c})

  # Only copy back on Linux to avoid spurious changes because of
  # different versions of rpcgen
  IF(LINUX)
    # copy back the generated source if they are different
    # perhaps we have made changes to xcom_vp.x (?)
    ADD_CUSTOM_TARGET(checkedin_${X}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${x_gen_h} ${x_vanilla_h}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${x_gen_c} ${x_vanilla_c}
      DEPENDS ${x_gen_h} ${x_gen_c})

    SET_PROPERTY(TARGET checkedin_${X} PROPERTY EXCLUDE_FROM_ALL TRUE)
    SET_PROPERTY(TARGET checkedin_${X} PROPERTY EXCLUDE_FROM_DEFAULT_BUILD TRUE)
  ENDIF()

  UNSET(x_tmp_x)
  UNSET(x_tmp_x_canonical_name)

  UNSET(x_tmp_h)
  UNSET(x_tmp_c)
  UNSET(x_gen_h)
  UNSET(x_gen_c)

  UNSET(x_vanilla_x)
  UNSET(x_vanilla_h)
  UNSET(x_vanilla_c)
ENDFOREACH(X)

# export the list of files
SET(XCOM_RPCGEN_SOURCES ${GEN_RPC_C_FILES})
SET(XCOM_RPCGEN_HEADERS ${GEN_RPC_H_FILES})
SET(XCOM_SUNRPC_SOURCES ${SUNRPC_SRCS})
SET(XCOM_WINDEPS_INCLUDE_DIRS
  ${WINDEPS_INCLUDE_DIRS}
  ${SUNRPC_INCLUDE_DIRS})
