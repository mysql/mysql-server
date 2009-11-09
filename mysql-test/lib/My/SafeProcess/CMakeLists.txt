# Copyright (C) 2006 MySQL AB
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

IF (WIN32)
  ADD_EXECUTABLE(my_safe_process safe_process_win.cc)
  ADD_EXECUTABLE(my_safe_kill safe_kill_win.cc)
ELSE()
  ADD_EXECUTABLE(my_safe_process safe_process.cc)
ENDIF()

INSTALL(TARGETS my_safe_process DESTINATION "mysql-test/lib/My/SafeProcess")
IF(WIN32)
  INSTALL(TARGETS my_safe_kill DESTINATION "mysql-test/lib/My/SafeProcess")
ENDIF()
INSTALL(FILES safe_process.pl Base.pm DESTINATION "mysql-test/lib/My/SafeProcess")
