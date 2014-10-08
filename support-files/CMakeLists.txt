# Copyright (c) 2006, 2014, Oracle and/or its affiliates.
# Copyright (c) 2012, 2014, SkySQL Ab.
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

IF(WIN32)
  SET(localstatedir "C:\\\\mysql\\\\data\\\\")
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
  SET(HOSTNAME "hostname")
ENDIF()

# XXX: shouldn't we just have variables for all this stuff and centralise
# XXX: their configuration in install_layout.cmake?
IF(WIN32)
  SET(inst_location ${INSTALL_DOCREADMEDIR})
ELSE()
  SET(inst_location ${INSTALL_SUPPORTFILESDIR})
ENDIF()

FOREACH(inifile my-huge my-innodb-heavy-4G my-large my-medium my-small)
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/${inifile}.cnf.sh 
                 ${CMAKE_CURRENT_BINARY_DIR}/${inifile}.${ini_file_extension} @ONLY)
  INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/${inifile}.${ini_file_extension}
          DESTINATION ${inst_location} COMPONENT IniFiles)
ENDFOREACH()

IF(UNIX)
  SET(prefix ${CMAKE_INSTALL_PREFIX})
  FOREACH(script  mysqld_multi.server mysql-log-rotate binary-configure)
    CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/${script}.sh 
                   ${CMAKE_CURRENT_BINARY_DIR}/${script} @ONLY )

    IF(script MATCHES ".ini")
      SET(comp IniFiles)
      SET(permissions OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
    ELSE()
      SET(comp Server_Scripts)
      SET(permissions OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    ENDIF()
    INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/${script} 
       DESTINATION ${inst_location} COMPONENT ${comp}
       PERMISSIONS ${permissions})
  ENDFOREACH()
  IF(INSTALL_SUPPORTFILESDIR)
    INSTALL(FILES magic DESTINATION ${inst_location} COMPONENT SupportFiles)
    INSTALL(DIRECTORY RHEL4-SElinux/ DESTINATION ${inst_location}/SELinux/RHEL4 COMPONENT SupportFiles)
  ENDIF()

  INSTALL(FILES mysql.m4 DESTINATION ${INSTALL_SHAREDIR}/aclocal COMPONENT Development)
  CONFIGURE_FILE(MySQL-shared-compat.spec.sh ${CMAKE_CURRENT_BINARY_DIR}/MySQL-shared-compat.spec @ONLY)
  CONFIGURE_FILE(mysql.spec.sh ${CMAKE_CURRENT_BINARY_DIR}/mysql.spec @ONLY)
  
  SET(SPECFILENAME "mysql.${VERSION}.spec")
  IF("${VERSION}" MATCHES "-ndb-")
    STRING(REGEX REPLACE "^.*-ndb-" "" NDBVERSION "${VERSION}")
    SET(SPECFILENAME "mysql-cluster-${NDBVERSION}.spec")
  ENDIF()
  CONFIGURE_FILE(mysql.spec.sh ${CMAKE_CURRENT_BINARY_DIR}/${SPECFILENAME} @ONLY)
  
  CONFIGURE_FILE(MySQL-shared-compat.spec.sh ${CMAKE_CURRENT_BINARY_DIR}/MySQL-shared-compat.spec @ONLY)

  SET(bindir ${prefix}/${INSTALL_BINDIR})
  SET(sbindir ${prefix}/${INSTALL_SBINDIR})
  SET(scriptdir ${prefix}/${INSTALL_SCRIPTDIR})
  SET(libexecdir ${prefix}/${INSTALL_SBINDIR})
  SET(pkgdatadir ${prefix}/${INSTALL_MYSQLSHAREDIR})
  CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/mysql.server.sh 
                 ${CMAKE_CURRENT_BINARY_DIR}/mysql.server @ONLY)
  INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/mysql.server 
    DESTINATION ${inst_location} COMPONENT SupportFiles
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ 
    GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

  IF (INSTALL_SYSCONFDIR)
    INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/mysql-log-rotate DESTINATION ${INSTALL_SYSCONFDIR}/logrotate.d
            RENAME mysql COMPONENT SupportFiles)
    INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/mysql.server
            DESTINATION  ${INSTALL_SYSCONFDIR}/init.d
            RENAME mysql COMPONENT SupportFiles
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ 
            GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

    INSTALL(FILES rpm/my.cnf DESTINATION ${INSTALL_SYSCONFDIR}
            COMPONENT Common)

    IF (INSTALL_SYSCONF2DIR)
      INSTALL(FILES rpm/client.cnf DESTINATION ${INSTALL_SYSCONF2DIR}
              COMPONENT SharedLibraries)
      INSTALL(FILES rpm/mysql-clients.cnf DESTINATION ${INSTALL_SYSCONF2DIR}
              COMPONENT Client)
      INSTALL(FILES rpm/server.cnf DESTINATION ${INSTALL_SYSCONF2DIR}
              COMPONENT IniFiles)
    ENDIF()

    # This is for SuSE:
    INSTALL(CODE "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E create_symlink
                   ${INSTALL_SYSCONFDIR}/init.d/mysql
                   ${INSTALL_SBINDIR}/rcmysql
                   WORKING_DIRECTORY \$ENV{DESTDIR}${prefix})"
                   COMPONENT SupportFiles)
  ENDIF(INSTALL_SYSCONFDIR)
ENDIF()
