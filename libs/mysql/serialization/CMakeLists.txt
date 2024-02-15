# Copyright (c) 2023, 2024, Oracle and/or its affiliates.
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

DISABLE_MISSING_PROFILE_WARNING()

SET(TARGET_HEADERS
  archive_binary_field_max_size_calculator.h
  archive_binary.h
  archive_binary_impl.hpp
  archive.h
  archive_text.h
  archive_text_impl.hpp
  byte_order_helpers.h
  field_definition.h
  field_definition_helpers.h
  field_functor.h
  field_wrapper.h
  primitive_type_codec.h
  read_archive_binary.h
  read_archive_binary_impl.hpp
  serializable.h
  serializable_impl.hpp
  serializable_size_calculator.h
  serializable_type_tags.h
  serializable_type_traits.h
  serialization_error.h
  serialization_error_type.h
  serialization_format_version.h
  serialization_types.h
  serializer_default.h
  serializer_default_impl.hpp
  serializer.h
  serializer_impl.hpp
  unknown_field_policy.h
  variable_length_integers.h
  write_archive_binary.h
  write_archive_binary_impl.hpp
)

SET(TARGET_SRCS
  archive_binary.cpp
  archive_text.cpp
  primitive_type_codec.cpp
  serialization_error.cpp
  write_archive_binary.cpp
  read_archive_binary.cpp
)

LIBS_MYSQL_CREATE_LIBRARY(mysql_serialization
  TARGET_SRCS ${TARGET_SRCS}
  TARGET_HEADERS ${TARGET_HEADERS}
)


