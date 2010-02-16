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

IF(WIN32)
  SET(localstatedir "C:\\mysql\\data")
  SET(ini_file_extension "ini")
ELSE()
  SET(localstatedir "${MYSQL_DATADIR}")
  SET(prefix "${CMAKE_INSTALL_PREFIX}")
  SET(libexecdir "${CMAKE_INSTALL_PREFIX}/${INSTALL_SBINDIR}")
  SET(bindir "${CMAKE_INSTALL_PREFIX}/${INSTALL_BINDIR}" )
  SET(sbindir "${libexecdir}")
  SET(datadir "${MYSQL_DATADIR}")
  SET(CC ${CMAKE_C_COMPILER})
  SET(CXX ${CMAKE_CXX_COMPILER})
  SET(CFLAGS ${CMAKE_C_FLAGS})
  SET(CXXFLAGS ${CMAKE_CXX_FLAGS})
  SET(MYSQLD_USER "mysql")
  SET(ini_file_extension "cnf")
ENDIF()

FOREACH(inifile my-huge my-innodb-heavy-4G my-large my-medium my-small)
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/${inifile}.cnf.sh 
                 ${CMAKE_CURRENT_BINARY_DIR}/${inifile}.${ini_file_extension} @ONLY)
  INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/${inifile}.${ini_file_extension} DESTINATION 
  ${INSTALL_DOCREADMEDIR} COMPONENT IniFiles)

ENDFOREACH()

IF(UNIX)
  IF(INSTALL_LAYOUT MATCHES "STANDALONE")
    SET(prefix ".")
    SET(inst_location ${INSTALL_SUPPORTFILESDIR})
  ELSE()
    SET(prefix ${CMAKE_INSTALL_PREFIX})
    SET(inst_location ${INSTALL_MYSQLSHAREDIR})
  ENDIF()

  FILE(GLOB ndb_ini_files ${CMAKE_CURRENT_SOURCE_DIR}/*.ini)
  INSTALL(FILES ${ndb_ini_files}  DESTINATION  ${inst_location})

  FOREACH(script  mysqld_multi.server mysql-log-rotate)
    CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/${script}.sh 
                   ${CMAKE_CURRENT_BINARY_DIR}/${script} @ONLY )

    INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/${script} 
       DESTINATION ${inst_location}
       PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
  ENDFOREACH()
  IF(INSTALL_SUPPORTFILESDIR)
    INSTALL(FILES magic DESTINATION ${inst_location})
  ENDIF()

  INSTALL(FILES mysql.m4 DESTINATION ${INSTALL_SHAREDIR}/aclocal COMPONENT Development)
  CONFIGURE_FILE(MySQL-shared-compat.spec.sh ${CMAKE_CURRENT_BINARY_DIR}/MySQL-shared-compat.spec @ONLY)
  CONFIGURE_FILE(mysql.spec.sh ${CMAKE_CURRENT_BINARY_DIR}/mysql.spec @ONLY)
  CONFIGURE_FILE(mysql.spec.sh ${CMAKE_CURRENT_BINARY_DIR}/mysql.${VERSION}.spec @ONLY)
  CONFIGURE_FILE(MySQL-shared-compat.spec.sh ${CMAKE_CURRENT_BINARY_DIR}/MySQL-shared-compat.spec @ONLY)

  SET(bindir ${prefix}/${INSTALL_BINDIR})
  SET(sbindir ${prefix}/${INSTALL_SBINDIR})
  SET(scriptdir ${prefix}/${INSTALL_SCRIPTDIR})
  SET(libexecdir ${prefix}/${INSTALL_SBINDIR})
  SET(pkgdatadir ${prefix}/${INSTALL_MYSQLSHAREDIR})
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/mysql.server.sh 
                 ${CMAKE_CURRENT_BINARY_DIR}/mysql.server @ONLY)
  INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/mysql.server 
    DESTINATION ${inst_location}
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ 
    GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
ENDIF()
