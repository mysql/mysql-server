# Copyright (c)  2017, Oracle and/or its affiliates. All rights reserved.
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

# This CMake file builds ICU.
# We build:
# - libicustubdata.a, a stub library for data, which we don't use.
# - libicui18n.a, the internationalization library.
# - libicuuc.a, a common utility library.
#
# To save some space and time, we have removed directories which are not
# needed by MySQL:
#   source/allinone
#   source/config
#   source/data
#   source/extra
#   source/layoutex
#   source/samples
#   source/test
#   source/tools


PROJECT (ICU)

IF(LINUX)
  SET(ICU_LINUX_COMPILE_OPTIONS
    "-Wno-undef"
    "-Wno-deprecated-declarations"
    "-Wno-error"
    "-Wno-unused-parameter"
    "-Wno-missing-field-initializers"
    "-Wno-sign-compare"
    "-Wno-type-limits"
  )
ENDIF()

IF(WIN32)
  ADD_DEFINITIONS(
    -DU_STATIC_IMPLEMENTATION=1
    -DU_IMPORT=
    -DU_EXPORT=
  )
ENDIF()

ADD_SUBDIRECTORY(source/common)
ADD_SUBDIRECTORY(source/i18n)
ADD_SUBDIRECTORY(source/stubdata)
