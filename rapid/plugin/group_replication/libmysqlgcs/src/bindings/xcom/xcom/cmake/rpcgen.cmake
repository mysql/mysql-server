# Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

# Setting initial variables
SET (SUNRPC_SRCS "")

IF (CMAKE_SYSTEM_NAME MATCHES "Windows")
    # Set the Windows specific and SUN RPC include dirs
    SET(WINDEPS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/windeps/include CACHE PATH "windows dependencies include dir")
    SET(SUNRPC_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/windeps/sunrpc CACHE PATH "sunrpc include dir")

    SET(SUNRPC_SRCS
        ${SUNRPC_INCLUDE_DIRS}/xdr_float.c
        ${SUNRPC_INCLUDE_DIRS}/xdr_ref.c
        ${SUNRPC_INCLUDE_DIRS}/xdr_array.c
        ${SUNRPC_INCLUDE_DIRS}/xdr_sizeof.c
        ${SUNRPC_INCLUDE_DIRS}/xdr_mem.c
        ${SUNRPC_INCLUDE_DIRS}/xdr.c
    )

  INCLUDE_DIRECTORIES(${WINDEPS_INCLUDE_DIRS})
  INCLUDE_DIRECTORIES(${SUNRPC_INCLUDE_DIRS})
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
  SET (x_vanilla_x      ${CMAKE_CURRENT_SOURCE_DIR}/${X}.x)
  SET (x_vanilla_plat_h ${CMAKE_CURRENT_SOURCE_DIR}/${X}_platform.h.gen)
  SET (x_vanilla_h      ${CMAKE_CURRENT_SOURCE_DIR}/${X}.h.gen)
  SET (x_vanilla_c      ${CMAKE_CURRENT_SOURCE_DIR}/${X}_xdr.c.gen)

  IF(CMAKE_SYSTEM_NAME MATCHES "Windows")
    # on windows system's there is no rpcgen, thence copy
    # the files in the source directory
    ADD_CUSTOM_COMMAND(OUTPUT ${x_gen_h} ${x_gen_c} ${x_tmp_plat_h}
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                  ${CMAKE_CURRENT_SOURCE_DIR}/xcom_proto_enum.h ${gen_xdr_dir}/xcom_proto_enum.h
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                  ${CMAKE_CURRENT_SOURCE_DIR}/xcom_limits.h ${gen_xdr_dir}/xcom_limits.h
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${x_vanilla_h} ${x_gen_h}
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${x_vanilla_c} ${x_gen_c}
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${x_vanilla_plat_h} ${x_tmp_plat_h}
                DEPENDS
                  ${x_vanilla_h}
                  ${x_vanilla_c}
                  ${x_vanilla_plat_h}
                  ${CMAKE_CURRENT_SOURCE_DIR}/xcom_proto_enum.h
                  ${CMAKE_CURRENT_SOURCE_DIR}/xcom_limits.h
                COMMENT "Copying ${x_vanilla_h} ${x_vanilla_c} ${x_vanilla_plat_h}
                                 ${CMAKE_CURRENT_SOURCE_DIR}/xcom_proto_enum.h
                                 ${CMAKE_CURRENT_SOURCE_DIR}/xcom_limits.h
                                 to ${gen_xdr_dir}.")
  ELSE()

    # on unix systems try to generate them if needed
    ADD_CUSTOM_COMMAND(OUTPUT ${x_gen_h} ${x_gen_c} ${x_tmp_plat_h}
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                  ${CMAKE_CURRENT_SOURCE_DIR}/xcom_proto_enum.h ${gen_xdr_dir}/xcom_proto_enum.h
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                  ${CMAKE_CURRENT_SOURCE_DIR}/xcom_limits.h ${gen_xdr_dir}/xcom_limits.h
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${x_vanilla_x} ${x_tmp_x}
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${x_vanilla_plat_h} ${x_tmp_plat_h}

                # generate the sources
                COMMAND ${CMAKE_COMMAND} -E remove -f ${x_gen_h}
                COMMAND ${CMAKE_COMMAND}
                    ARGS -E chdir ${gen_xdr_dir} rpcgen  -C -h -o ${x_gen_h} ${x_tmp_x_canonical_name}
                COMMAND ${CMAKE_COMMAND} -E remove -f ${x_gen_c}
                COMMAND ${CMAKE_COMMAND}
                    ARGS -E chdir ${gen_xdr_dir} rpcgen  -C -c -o ${x_gen_c} ${x_tmp_x_canonical_name})
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
