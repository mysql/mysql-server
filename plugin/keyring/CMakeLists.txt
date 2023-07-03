# Copyright (c) 2016, 2023, Oracle and/or its affiliates.
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

ADD_DEFINITIONS(-DLOG_COMPONENT_TAG="keyring_file")

DISABLE_MISSING_PROFILE_WARNING()

MYSQL_ADD_PLUGIN(keyring_file
  buffer.cc
  buffered_file_io.cc
  checker/checker.cc
  checker/checker_factory.cc
  checker/checker_ver_1_0.cc
  checker/checker_ver_2_0.cc
  common/keyring_impl.cc
  common/keyring_key.cc
  common/keys_container.cc
  common/keys_iterator.cc
  converter.cc
  digest.cc
  file_io.cc
  hash_to_buffer_serializer.cc
  keyring.cc
  LINK_LIBRARIES ${SSL_LIBRARIES}
  MODULE_ONLY
  MODULE_OUTPUT_NAME "keyring_file"
  )
