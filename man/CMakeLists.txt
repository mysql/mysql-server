# Copyright (c) 2006, 2015, Oracle and/or its affiliates. All rights reserved.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

# Copy man pages
FILE(GLOB MAN1_FILES *.1)
FILE(GLOB MAN1_EXCLUDE make_win_bin_dist.1)
FILE(GLOB MAN1_NDB ndb*.1)
FILE(GLOB MAN8_FILES *.8)
FILE(GLOB MAN8_NDB ndb*.8)
IF(MAN1_FILES)
  IF(MAN1_EXCLUDE)
    LIST(REMOVE_ITEM MAN1_FILES ${MAN1_EXCLUDE})
  ENDIF()
  IF(MAN1_NDB AND NOT VERSION MATCHES "-ndb-")
    LIST(REMOVE_ITEM MAN1_FILES ${MAN1_NDB})
  ENDIF()
  FOREACH(file ${MAN1_FILES})
    IF(${file} MATCHES "(.*mysqltest.1$)")
      INSTALL(FILES ${file} DESTINATION ${INSTALL_MANDIR}/man1
      COMPONENT Test)
    ELSE()
      INSTALL(FILES ${file} DESTINATION ${INSTALL_MANDIR}/man1
      COMPONENT ManPages)
    ENDIF()
  ENDFOREACH()
ENDIF()
IF(MAN8_FILES)
  IF(MAN8_NDB AND NOT VERSION MATCHES "-ndb-")
    LIST(REMOVE_ITEM MAN8_FILES ${MAN8_NDB})
  ENDIF()
  INSTALL(FILES ${MAN8_FILES} DESTINATION ${INSTALL_MANDIR}/man8
  COMPONENT ManPages)
ENDIF()
