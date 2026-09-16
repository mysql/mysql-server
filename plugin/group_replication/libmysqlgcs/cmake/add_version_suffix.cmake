# Copyright (c) 2020, 2026, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

# initialize
SET(ignore_list
  xdr_array
  xdr_bool
  xdr_bytes
  xdr_char
  xdr_double
  xdr_enum
  xdr_float
  xdr_int
  xdr_int8_t
  xdr_int16_t
  xdr_int32_t
  xdr_int64_t
  xdr_long
  xdr_opaque
  xdr_pointer
  xdr_reference
  xdr_short
  xdr_string
  xdr_u_char
  xdr_u_int
  xdr_uint8_t
  xdr_uint16_t
  xdr_uint32_t
  xdr_uint64_t
  xdr_u_long
  xdr_u_short
  xdr_union
  xdr_vector
  xdr_void
  xdr_wrapstring
  xdr_checked_data
  )

# Run rpcgen
#MESSAGE("${RPCGEN_EXECUTABLE} -DXCOM_PROTO_VERS=${XCOM_PROTO_VERS} -C -c ${x_tmp_x_canonical_name} > ${rpcgen_output}")
EXECUTE_PROCESS(
  COMMAND ${RPCGEN_EXECUTABLE} -DXCOM_PROTO_VERS=${XCOM_PROTO_VERS} -C -c ${x_tmp_x_canonical_name}
  OUTPUT_FILE ${rpcgen_output}
  )

#MESSAGE("Reading ${rpcgen_output}")
FILE(STRINGS ${rpcgen_output} xcom_vp_def)

#MESSAGE("writing ${x_gen_c}")

SET(emit_app_data_xdr_guard 0)
IF(NOT EXISTS ${x_gen_c})
  SET(emit_app_data_xdr_guard 1)
ENDIF()

# Process file line by line, and add version numbers and code stubs
SET(blurb 0)
FOREACH(line ${xcom_vp_def})
  STRING(REGEX MATCH "^BEGIN" xxx "${line}")
  IF(xxx)
	SET(blurb 1)
	SET(suppress 1)
  ENDIF()

  STRING(REGEX MATCH "^END" xxx "${line}")
  IF(xxx)
	SET(blurb 0)
	SET(suppress 1)
  ENDIF()

  # Save lines if within BEGIN..END
  IF(blurb)
	IF(suppress)
	  SET(suppress 0)
	ELSE()
	  SET(code "${code}\n${line}")
	ENDIF()
  ELSE()
	# Output saved lines when we see "return TRUE"
	STRING(REGEX MATCH "return \\(*TRUE" xxx "${line}")
	IF(xxx)
	  IF(code)
		FILE(APPEND ${x_gen_c} "/* BEGIN protocol conversion code */${code}\n/* END protocol conversion code */\n")
		UNSET(code)
	  ENDIF()
	ENDIF()

	IF(suppress)
	  SET(suppress 0)
	ELSE()
	  # Add version suffix to any xdr_* functions in this line
	  STRING(REGEX REPLACE "(xdr_[a-zA-Z0-9_]+)" \\1${version} vline "${line}")
          # If we added the suffix to a xdr function from the ignore list, revert to the name without the suffix.
	  FOREACH(IGNORE_PATTERN ${ignore_list})
              SET(IGNORE_PATTERN_WITH_VERSION "${IGNORE_PATTERN}${version}")
              STRING(REPLACE "${IGNORE_PATTERN_WITH_VERSION}" "${IGNORE_PATTERN}" vline "${vline}")
	  ENDFOREACH()
	  IF(vline MATCHES "xdr_pointer \\(xdrs, .*sizeof \\(app_data\\),")
	    STRING(REPLACE "xdr_pointer (xdrs,"
			   "xcom_app_data_pointer (xdrs," vline "${vline}")
	  ENDIF()
	  FILE(APPEND ${x_gen_c} "${vline}\n")
          IF(emit_app_data_xdr_guard AND
             vline MATCHES "^#include \"xcom/xcom_profile.h\"$")
            FILE(APPEND ${x_gen_c} [=[
typedef struct xcom_app_data_xdr_context {
  uint32_t app_data_depth;
} xcom_app_data_xdr_context;

static bool_t xcom_app_data_pointer(XDR *xdrs, char **objpp, u_int obj_size,
                                    xdrproc_t xdr_obj) {
  bool_t more_data;
  xcom_app_data_xdr_context *context;
  bool_t stat;

  if (xdrs->x_op != XDR_DECODE) {
    return xdr_pointer(xdrs, objpp, obj_size, xdr_obj);
  }

  more_data = (*objpp != NULL);
  if (!xdr_bool(xdrs, &more_data)) {
    return FALSE;
  }
  if (!more_data) {
    *objpp = NULL;
    return TRUE;
  }

  context = (xcom_app_data_xdr_context *)xdrs->x_public;
  if (context == NULL || context->app_data_depth >= MAX_BATCH_APP_DATA) {
    return FALSE;
  }

  context->app_data_depth++;
  stat = xdr_reference(xdrs, objpp, obj_size, xdr_obj);
  context->app_data_depth--;
  return stat;
}

]=])
            SET(emit_app_data_xdr_guard 0)
          ENDIF()
	ENDIF()
  ENDIF()
ENDFOREACH()

FILE(REMOVE ${rpcgen_output})

