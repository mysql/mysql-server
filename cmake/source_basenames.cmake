# Copyright (c) 2017 Oracle and/or its affiliates. All rights reserved.
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


FUNCTION(define_source_basenames target)
  GET_TARGET_PROPERTY(files "${target}" SOURCES)
  FOREACH(file ${files})
    GET_PROPERTY(cdefs SOURCE "${file}" PROPERTY COMPILE_DEFINITIONS)
    GET_FILENAME_COMPONENT(basename "${file}" NAME_WE)
    LIST(APPEND cdefs "MY_BASENAME=\"${basename}\"")
    SET_PROPERTY(SOURCE "${file}" PROPERTY COMPILE_DEFINITIONS ${cdefs})
  ENDFOREACH()
ENDFUNCTION()
