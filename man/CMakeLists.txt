# Copyright (C) 2009 Sun Microsystems, Inc
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

# Copy man pages
FILE(GLOB MAN1_FILES *.1)
FILE(GLOB MAN8_FILES *.8)
IF(MAN1_FILES)
  INSTALL(FILES ${MAN1_FILES} DESTINATION man/man1)
ENDIF()
IF(MAN8_FILES)
  INSTALL(FILES ${MAN8_FILES} DESTINATION man/man8)
ENDIF()
