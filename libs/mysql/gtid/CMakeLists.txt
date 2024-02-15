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

SET(LINKED_LIBRARIES
)

SET(TARGET_HEADERS
  global.h
  gtid.h
  gtidset.h
  tag.h
  tag_plain.h
  tsid.h
  tsid_plain.h
  uuid.h
)

SET(TARGET_SRCS
  gtid.cpp
  gtidset.cpp
  tag.cpp
  tag_plain.cpp
  tsid.cpp
  tsid_plain.cpp
  uuid.cpp
)

LIBS_MYSQL_CREATE_LIBRARY(mysql_gtid
  TARGET_SRCS ${TARGET_SRCS}
  TARGET_HEADERS ${TARGET_HEADERS}
  LINK_LIBRARIES ${LINKED_LIBRARIES}
)

