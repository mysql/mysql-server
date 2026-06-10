# Copyright (c) 2022, 2026, Oracle and/or its affiliates.
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

# See extra/opentelemetry-proto/

SET(OPENTELEMETRY_PROTO_TAG "opentelemetry-proto-1.7.0")

MACRO(MYSQL_USE_BUNDLED_OPENTELEMETRY_PROTO)

  SET(OPENTELEMETRY_PROTO_BUNDLE_SRC_PATH
    "${CMAKE_SOURCE_DIR}/extra/opentelemetry-proto/")
  STRING_APPEND(OPENTELEMETRY_PROTO_BUNDLE_SRC_PATH
    "${OPENTELEMETRY_PROTO_TAG}")

ENDMACRO()

MACRO(MYSQL_CHECK_OPENTELEMETRY_PROTO)
  MYSQL_USE_BUNDLED_OPENTELEMETRY_PROTO()

  MESSAGE(STATUS
    "OPENTELEMETRY_PROTO_TAG is ${OPENTELEMETRY_PROTO_TAG}")
  MESSAGE(STATUS
    "OPENTELEMETRY_PROTO_BUNDLE_SRC_PATH is ${OPENTELEMETRY_PROTO_BUNDLE_SRC_PATH}")
ENDMACRO()
