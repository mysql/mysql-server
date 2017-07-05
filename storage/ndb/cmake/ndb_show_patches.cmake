# Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
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

#
# Generate diff showing any changes to the MySQL Server
# used in this version of Cluster
# - when on unix, in git repo, having git and perl
#
MACRO(NDB_SHOW_PATCHES)

  # Only in Cluster branch
  IF(NOT MYSQL_CLUSTER_VERSION)
    RETURN()
  ENDIF()

  # Only on Unix
  IF(NOT UNIX)
    RETURN()
  ENDIF()

  # Check for git (again..)
  FIND_PACKAGE(Git)
  IF(NOT GIT_FOUND)
    RETURN()
  ENDIF()

  # Check for perl
  FIND_PACKAGE(Perl)
  IF(NOT PERL_FOUND)
    RETURN()
  ENDIF()

  # Check if source dir is git repo
  EXECUTE_PROCESS(
    COMMAND "${GIT_EXECUTABLE}" rev-parse --show-toplevel
    OUTPUT_VARIABLE GIT_ROOT
    ERROR_VARIABLE GIT_ROOT_ERROR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE RESULT
  )
  IF(NOT RESULT EQUAL 0 OR NOT GIT_ROOT STREQUAL ${CMAKE_SOURCE_DIR})
    RETURN()
  ENDIF()

  SET(diff_script ${CMAKE_SOURCE_DIR}/storage/ndb/diff-cluster)
  EXECUTE_PROCESS(
    COMMAND "${PERL_EXECUTABLE}" ${diff_script}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    TIMEOUT 60
  )
ENDMACRO()

