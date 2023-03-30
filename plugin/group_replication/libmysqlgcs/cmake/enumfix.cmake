# Copyright (c) 2020, 2023, Oracle and/or its affiliates.
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

#
# Run rpcgen and fix the generated code so the compiler will not complain
# about enum definitions being terminated by ,}
#
#MESSAGE("${RPCGEN_EXECUTABLE} -DXCOM_PROTO_VERS=${XCOM_PROTO_VERS} -C -h ${x_tmp_x_canonical_name} > ${rpcgen_output}")
EXECUTE_PROCESS(
  COMMAND ${RPCGEN_EXECUTABLE} -DXCOM_PROTO_VERS=${XCOM_PROTO_VERS} -C -h ${x_tmp_x_canonical_name}
  OUTPUT_FILE ${rpcgen_output}
  )

#MESSAGE("Reading ${rpcgen_output}")
FILE(READ ${rpcgen_output} xcom_vp_def)

#MESSAGE("writing ${x_gen_h}")
STRING(REPLACE ",\n}" "\n}" xcom_vp_def "${xcom_vp_def}")
FILE(APPEND ${x_gen_h} "${xcom_vp_def}")
FILE(REMOVE ${rpcgen_output})
