# Copyright (c) 2010, 2016, Oracle and/or its affiliates. All rights reserved.
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

# The purpose of this file is to set the default installation layout.
#
# The current choices of installation layout are:
#
#  STANDALONE
#    Build with prefix=/usr/local/mysql, create tarball with install prefix="."
#    and relative links.  Windows zip uses the same tarball layout but without
#    the build prefix.
#
#  RPM, SLES
#    Build as per default RPM layout, with prefix=/usr
#    Note: The layout for ULN RPMs differs, see the "RPM" section.
#
#  DEB
#    Build as per STANDALONE, prefix=/opt/mysql/server-$major.$minor
#
#  SVR4
#    Solaris package layout suitable for pkg* tools, prefix=/opt/mysql/mysql
#
#  FREEBSD, GLIBC, OSX, TARGZ
#    Build with prefix=/usr/local/mysql, create tarball with install prefix="."
#    and relative links.
#
#  WIN
#     Windows zip : same as tarball layout but without the build prefix
#
# To force a directory layout, use -DINSTALL_LAYOUT=<layout>.
#
# The default is STANDALONE.
#
# Note : At present, RPM and SLES layouts are similar. This is also true
#        for layouts like FREEBSD, GLIBC, OSX, TARGZ. However, they provide
#        opportunity to fine-tune deployment for each platform without
#        affecting all other types of deployment.
#
# There is the possibility to further fine-tune installation directories.
# Several variables can be overwritten:
#
# - INSTALL_BINDIR          (directory with client executables and scripts)
# - INSTALL_SBINDIR         (directory with mysqld)
# - INSTALL_SCRIPTDIR       (several scripts, rarely used)
#
# - INSTALL_LIBDIR          (directory with client end embedded libraries)
# - INSTALL_PLUGINDIR       (directory for plugins)
#
# - INSTALL_INCLUDEDIR      (directory for MySQL headers)
#
# - INSTALL_DOCDIR          (documentation)
# - INSTALL_DOCREADMEDIR    (readme and similar)
# - INSTALL_MANDIR          (man pages)
# - INSTALL_INFODIR         (info pages)
#
# - INSTALL_SHAREDIR        (location of aclocal/mysql.m4)
# - INSTALL_MYSQLSHAREDIR   (MySQL character sets and localized error messages)
# - INSTALL_MYSQLTESTDIR    (mysql-test)
# - INSTALL_SUPPORTFILESDIR (various extra support files)
#
# - INSTALL_MYSQLDATADIR    (data directory)
# - INSTALL_MYSQLKEYRING    (keyring directory)
# - INSTALL_SECURE_FILE_PRIVDIR (--secure-file-priv directory)
#
# When changing this page,  _please_ do not forget to update public Wiki
# http://forge.mysql.com/wiki/CMake#Fine-tuning_installation_paths

IF(NOT INSTALL_LAYOUT)
  SET(DEFAULT_INSTALL_LAYOUT "STANDALONE")
ENDIF()

SET(INSTALL_LAYOUT "${DEFAULT_INSTALL_LAYOUT}"
CACHE STRING "Installation directory layout. Options are: TARGZ (as in tar.gz installer), WIN (as in zip installer), STANDALONE, RPM, DEB, SVR4, FREEBSD, GLIBC, OSX, SLES")

IF(UNIX)
  IF(INSTALL_LAYOUT MATCHES "RPM" OR
     INSTALL_LAYOUT MATCHES "SLES")
    SET(default_prefix "/usr")
  ELSEIF(INSTALL_LAYOUT MATCHES "DEB")
    SET(default_prefix "/opt/mysql/server-${MYSQL_BASE_VERSION}")
    # This is required to avoid "cpack -GDEB" default of prefix=/usr
    SET(CPACK_SET_DESTDIR ON)
  ELSEIF(INSTALL_LAYOUT MATCHES "SVR4")
    SET(default_prefix "/opt/mysql/mysql")
  ELSE()
    SET(default_prefix "/usr/local/mysql")
  ENDIF()
  IF(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    SET(CMAKE_INSTALL_PREFIX ${default_prefix}
      CACHE PATH "install prefix" FORCE)
  ENDIF()
  SET(VALID_INSTALL_LAYOUTS "RPM" "DEB" "SVR4" "FREEBSD" "GLIBC" "OSX" "TARGZ" "SLES" "STANDALONE")
  LIST(FIND VALID_INSTALL_LAYOUTS "${INSTALL_LAYOUT}" ind)
  IF(ind EQUAL -1)
    MESSAGE(FATAL_ERROR "Invalid INSTALL_LAYOUT parameter:${INSTALL_LAYOUT}."
    " Choose between ${VALID_INSTALL_LAYOUTS}" )
  ENDIF()

  SET(SYSCONFDIR "${CMAKE_INSTALL_PREFIX}/etc"
    CACHE PATH "config directory (for my.cnf)")
  MARK_AS_ADVANCED(SYSCONFDIR)
ENDIF()

IF(WIN32)
  SET(VALID_INSTALL_LAYOUTS "TARGZ" "STANDALONE" "WIN")
  LIST(FIND VALID_INSTALL_LAYOUTS "${INSTALL_LAYOUT}" ind)
  IF(ind EQUAL -1)
    MESSAGE(FATAL_ERROR "Invalid INSTALL_LAYOUT parameter:${INSTALL_LAYOUT}."
    " Choose between ${VALID_INSTALL_LAYOUTS}" )
  ENDIF()
ENDIF()

#
# plugin_tests's value should not be used by imported plugins,
# just use if(INSTALL_PLUGINTESTDIR).
# The plugin must set its own install path for tests
#
FILE(GLOB plugin_tests
  ${CMAKE_SOURCE_DIR}/plugin/*/tests
  ${CMAKE_SOURCE_DIR}/internal/plugin/*/tests
)

#
# DEFAULT_SECURE_FILE_PRIV_DIR/DEFAULT_SECURE_FILE_PRIV_EMBEDDED_DIR
#
IF(INSTALL_LAYOUT MATCHES "STANDALONE" OR
   INSTALL_LAYOUT MATCHES "WIN")
  SET(secure_file_priv_path "NULL")
ELSEIF(INSTALL_LAYOUT MATCHES "RPM" OR
       INSTALL_LAYOUT MATCHES "SLES" OR
       INSTALL_LAYOUT MATCHES "SVR4" OR
       INSTALL_LAYOUT MATCHES "DEB")
  SET(secure_file_priv_path "/var/lib/mysql-files")
ELSE()
  SET(secure_file_priv_path "${default_prefix}/mysql-files")
ENDIF()
SET(secure_file_priv_embedded_path "NULL")

#
# STANDALONE layout
#
SET(INSTALL_BINDIR_STANDALONE           "bin")
SET(INSTALL_SBINDIR_STANDALONE          "bin")
SET(INSTALL_SCRIPTDIR_STANDALONE        "scripts")
#
SET(INSTALL_LIBDIR_STANDALONE           "lib")
SET(INSTALL_PLUGINDIR_STANDALONE        "lib/plugin")
#
SET(INSTALL_INCLUDEDIR_STANDALONE       "include")
#
SET(INSTALL_DOCDIR_STANDALONE           "docs")
SET(INSTALL_DOCREADMEDIR_STANDALONE     ".")
SET(INSTALL_MANDIR_STANDALONE           "man")
SET(INSTALL_INFODIR_STANDALONE          "docs")
#
SET(INSTALL_SHAREDIR_STANDALONE         "share")
SET(INSTALL_MYSQLSHAREDIR_STANDALONE    "share")
SET(INSTALL_MYSQLTESTDIR_STANDALONE     "mysql-test")
SET(INSTALL_SUPPORTFILESDIR_STANDALONE  "support-files")
#
SET(INSTALL_MYSQLDATADIR_STANDALONE     "data")
SET(INSTALL_MYSQLKEYRINGDIR_STANDALONE  "keyring")
SET(INSTALL_PLUGINTESTDIR_STANDALONE    ${plugin_tests})
SET(INSTALL_SECURE_FILE_PRIVDIR_STANDALONE ${secure_file_priv_path})
SET(INSTALL_SECURE_FILE_PRIV_EMBEDDEDDIR_STANDALONE ${secure_file_priv_embedded_path})

#
# WIN layout
#
SET(INSTALL_BINDIR_WIN           "bin")
SET(INSTALL_SBINDIR_WIN          "bin")
SET(INSTALL_SCRIPTDIR_WIN        "scripts")
#
SET(INSTALL_LIBDIR_WIN           "lib")
SET(INSTALL_PLUGINDIR_WIN        "lib/plugin")
#
SET(INSTALL_INCLUDEDIR_WIN       "include")
#
SET(INSTALL_DOCDIR_WIN           "docs")
SET(INSTALL_DOCREADMEDIR_WIN     ".")
SET(INSTALL_MANDIR_WIN           "man")
SET(INSTALL_INFODIR_WIN          "docs")
#
SET(INSTALL_SHAREDIR_WIN         "share")
SET(INSTALL_MYSQLSHAREDIR_WIN    "share")
SET(INSTALL_MYSQLTESTDIR_WIN     "mysql-test")
SET(INSTALL_SUPPORTFILESDIR_WIN  "support-files")
#
SET(INSTALL_MYSQLDATADIR_WIN     "data")
SET(INSTALL_MYSQLKEYRINGDIR_WIN  "keyring")
SET(INSTALL_PLUGINTESTDIR_WIN    ${plugin_tests})
SET(INSTALL_SECURE_FILE_PRIVDIR_WIN ${secure_file_priv_path})
SET(INSTALL_SECURE_FILE_PRIV_EMBEDDEDDIR_WIN ${secure_file_priv_embedded_path})

#
# FREEBSD layout
#
SET(INSTALL_BINDIR_FREEBSD           "bin")
SET(INSTALL_SBINDIR_FREEBSD          "bin")
SET(INSTALL_SCRIPTDIR_FREEBSD        "scripts")
#
SET(INSTALL_LIBDIR_FREEBSD           "lib")
SET(INSTALL_PLUGINDIR_FREEBSD        "lib/plugin")
#
SET(INSTALL_INCLUDEDIR_FREEBSD       "include")
#
SET(INSTALL_DOCDIR_FREEBSD           "docs")
SET(INSTALL_DOCREADMEDIR_FREEBSD     ".")
SET(INSTALL_MANDIR_FREEBSD           "man")
SET(INSTALL_INFODIR_FREEBSD          "docs")
#
SET(INSTALL_SHAREDIR_FREEBSD         "share")
SET(INSTALL_MYSQLSHAREDIR_FREEBSD    "share")
SET(INSTALL_MYSQLTESTDIR_FREEBSD     "mysql-test")
SET(INSTALL_SUPPORTFILESDIR_FREEBSD  "support-files")
#
SET(INSTALL_MYSQLDATADIR_FREEBSD     "data")
SET(INSTALL_MYSQLKEYRINGDIR_FREEBSD  "keyring")
SET(INSTALL_PLUGINTESTDIR_FREEBSD    ${plugin_tests})
SET(INSTALL_SECURE_FILE_PRIVDIR_FREEBSD ${secure_file_priv_path})
SET(INSTALL_SECURE_FILE_PRIV_EMBEDDEDDIR_FREEBSD ${secure_file_priv_embedded_path})

#
# GLIBC layout
#
SET(INSTALL_BINDIR_GLIBC           "bin")
SET(INSTALL_SBINDIR_GLIBC          "bin")
SET(INSTALL_SCRIPTDIR_GLIBC        "scripts")
#
SET(INSTALL_LIBDIR_GLIBC           "lib")
SET(INSTALL_PLUGINDIR_GLIBC        "lib/plugin")
#
SET(INSTALL_INCLUDEDIR_GLIBC       "include")
#
SET(INSTALL_DOCDIR_GLIBC           "docs")
SET(INSTALL_DOCREADMEDIR_GLIBC     ".")
SET(INSTALL_MANDIR_GLIBC           "man")
SET(INSTALL_INFODIR_GLIBC          "docs")
#
SET(INSTALL_SHAREDIR_GLIBC         "share")
SET(INSTALL_MYSQLSHAREDIR_GLIBC    "share")
SET(INSTALL_MYSQLTESTDIR_GLIBC     "mysql-test")
SET(INSTALL_SUPPORTFILESDIR_GLIBC  "support-files")
#
SET(INSTALL_MYSQLDATADIR_GLIBC     "data")
SET(INSTALL_MYSQLKEYRINGDIR_GLIBC  "keyring")
SET(INSTALL_PLUGINTESTDIR_GLIBC    ${plugin_tests})
SET(INSTALL_SECURE_FILE_PRIVDIR_GLIBC ${secure_file_priv_path})
SET(INSTALL_SECURE_FILE_PRIV_EMBEDDEDDIR_GLIBC ${secure_file_priv_embedded_path})

#
# OSX layout
#
SET(INSTALL_BINDIR_OSX           "bin")
SET(INSTALL_SBINDIR_OSX          "bin")
SET(INSTALL_SCRIPTDIR_OSX        "scripts")
#
SET(INSTALL_LIBDIR_OSX           "lib")
SET(INSTALL_PLUGINDIR_OSX        "lib/plugin")
#
SET(INSTALL_INCLUDEDIR_OSX       "include")
#
SET(INSTALL_DOCDIR_OSX           "docs")
SET(INSTALL_DOCREADMEDIR_OSX     ".")
SET(INSTALL_MANDIR_OSX           "man")
SET(INSTALL_INFODIR_OSX          "docs")
#
SET(INSTALL_SHAREDIR_OSX         "share")
SET(INSTALL_MYSQLSHAREDIR_OSX    "share")
SET(INSTALL_MYSQLTESTDIR_OSX     "mysql-test")
SET(INSTALL_SUPPORTFILESDIR_OSX  "support-files")
#
SET(INSTALL_MYSQLDATADIR_OSX     "data")
SET(INSTALL_MYSQLKEYRINGDIR_OSX  "keyring")
SET(INSTALL_PLUGINTESTDIR_OSX    ${plugin_tests})
SET(INSTALL_SECURE_FILE_PRIVDIR_OSX ${secure_file_priv_path})
SET(INSTALL_SECURE_FILE_PRIV_EMBEDDEDDIR_OSX ${secure_file_priv_embedded_path})

#
# TARGZ layout
#
SET(INSTALL_BINDIR_TARGZ           "bin")
SET(INSTALL_SBINDIR_TARGZ          "bin")
SET(INSTALL_SCRIPTDIR_TARGZ        "scripts")
#
SET(INSTALL_LIBDIR_TARGZ           "lib")
SET(INSTALL_PLUGINDIR_TARGZ        "lib/plugin")
#
SET(INSTALL_INCLUDEDIR_TARGZ       "include")
#
SET(INSTALL_DOCDIR_TARGZ           "docs")
SET(INSTALL_DOCREADMEDIR_TARGZ     ".")
SET(INSTALL_MANDIR_TARGZ           "man")
SET(INSTALL_INFODIR_TARGZ          "docs")
#
SET(INSTALL_SHAREDIR_TARGZ         "share")
SET(INSTALL_MYSQLSHAREDIR_TARGZ    "share")
SET(INSTALL_MYSQLTESTDIR_TARGZ     "mysql-test")
SET(INSTALL_SUPPORTFILESDIR_TARGZ  "support-files")
#
SET(INSTALL_MYSQLDATADIR_TARGZ     "data")
SET(INSTALL_MYSQLKEYRINGDIR_TARGZ  "keyring")
SET(INSTALL_PLUGINTESTDIR_TARGZ    ${plugin_tests})
SET(INSTALL_SECURE_FILE_PRIVDIR_TARGZ ${secure_file_priv_path})
SET(INSTALL_SECURE_FILE_PRIV_EMBEDDEDDIR_TARGZ ${secure_file_priv_embedded_path})

#
# RPM layout
#
# See "packaging/rpm-uln/mysql-5.5-libdir.patch" for the differences
# which apply to RPMs in ULN (Oracle Linux), that patch file will
# be applied at build time via "rpmbuild".
#
SET(INSTALL_BINDIR_RPM                  "bin")
SET(INSTALL_SBINDIR_RPM                 "sbin")
SET(INSTALL_SCRIPTDIR_RPM               "bin")
#
IF(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
  SET(INSTALL_LIBDIR_RPM                "lib64")
  SET(INSTALL_PLUGINDIR_RPM             "lib64/mysql/plugin")
ELSE()
  SET(INSTALL_LIBDIR_RPM                "lib")
  SET(INSTALL_PLUGINDIR_RPM             "lib/mysql/plugin")
ENDIF()
#
SET(INSTALL_INCLUDEDIR_RPM              "include/mysql")
#
#SET(INSTALL_DOCDIR_RPM                 unset - installed directly by RPM)
#SET(INSTALL_DOCREADMEDIR_RPM           unset - installed directly by RPM)
SET(INSTALL_INFODIR_RPM                 "share/info")
SET(INSTALL_MANDIR_RPM                  "share/man")
#
SET(INSTALL_SHAREDIR_RPM                "share")
SET(INSTALL_MYSQLSHAREDIR_RPM           "share/mysql")
SET(INSTALL_MYSQLTESTDIR_RPM            "share/mysql-test")
SET(INSTALL_SUPPORTFILESDIR_RPM         "share/mysql")
#
SET(INSTALL_MYSQLDATADIR_RPM            "/var/lib/mysql")
SET(INSTALL_MYSQLKEYRINGDIR_RPM         "/var/lib/mysql-keyring")
SET(INSTALL_PLUGINTESTDIR_RPM           ${plugin_tests})
SET(INSTALL_SECURE_FILE_PRIVDIR_RPM     ${secure_file_priv_path})
SET(INSTALL_SECURE_FILE_PRIV_EMBEDDEDDIR_RPM     ${secure_file_priv_embedded_path})

#
# SLES layout
#
SET(INSTALL_BINDIR_SLES                  "bin")
SET(INSTALL_SBINDIR_SLES                 "sbin")
SET(INSTALL_SCRIPTDIR_SLES               "bin")
#
IF(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
  SET(INSTALL_LIBDIR_SLES                "lib64")
  SET(INSTALL_PLUGINDIR_SLES             "lib64/mysql/plugin")
ELSE()
  SET(INSTALL_LIBDIR_SLES                "lib")
  SET(INSTALL_PLUGINDIR_SLES             "lib/mysql/plugin")
ENDIF()
#
SET(INSTALL_INCLUDEDIR_SLES              "include/mysql")
#
#SET(INSTALL_DOCDIR_SLES                 unset - installed directly by SLES)
#SET(INSTALL_DOCREADMEDIR_SLES           unset - installed directly by SLES)
SET(INSTALL_INFODIR_SLES                 "share/info")
SET(INSTALL_MANDIR_SLES                  "share/man")
#
SET(INSTALL_SHAREDIR_SLES                "share")
SET(INSTALL_MYSQLSHAREDIR_SLES           "share/mysql")
SET(INSTALL_MYSQLTESTDIR_SLES            "share/mysql-test")
SET(INSTALL_SUPPORTFILESDIR_SLES         "share/mysql")
#
SET(INSTALL_MYSQLDATADIR_SLES            "/var/lib/mysql")
SET(INSTALL_MYSQLKEYRINGDIR_SLES         "/var/lib/mysql-keyring")
SET(INSTALL_PLUGINTESTDIR_SLES           ${plugin_tests})
SET(INSTALL_SECURE_FILE_PRIVDIR_SLES     ${secure_file_priv_path})
SET(INSTALL_SECURE_FILE_PRIV_EMBEDDEDDIR_SLES     ${secure_file_priv_embedded_path})

#
# DEB layout
#
SET(INSTALL_BINDIR_DEB                  "bin")
SET(INSTALL_SBINDIR_DEB                 "bin")
SET(INSTALL_SCRIPTDIR_DEB               "scripts")
#
SET(INSTALL_LIBDIR_DEB                  "lib")
SET(INSTALL_PLUGINDIR_DEB               "lib/plugin")
#
SET(INSTALL_INCLUDEDIR_DEB              "include")
#
SET(INSTALL_DOCDIR_DEB                  "docs")
SET(INSTALL_DOCREADMEDIR_DEB            ".")
SET(INSTALL_MANDIR_DEB                  "man")
SET(INSTALL_INFODIR_DEB                 "docs")
#
SET(INSTALL_SHAREDIR_DEB                "share")
SET(INSTALL_MYSQLSHAREDIR_DEB           "share")
SET(INSTALL_MYSQLTESTDIR_DEB            "mysql-test")
SET(INSTALL_SUPPORTFILESDIR_DEB         "support-files")
#
SET(INSTALL_MYSQLDATADIR_DEB            "/var/lib/mysql")
SET(INSTALL_MYSQLKEYRINGDIR_DEB         "/var/lib/mysql-keyring")
SET(INSTALL_PLUGINTESTDIR_DEB           ${plugin_tests})
SET(INSTALL_SECURE_FILE_PRIVDIR_DEB     ${secure_file_priv_path})
SET(INSTALL_SECURE_FILE_PRIV_EMBEDDEDDIR_DEB     ${secure_file_priv_embedded_path})

#
# SVR4 layout
#
SET(INSTALL_BINDIR_SVR4                 "bin")
SET(INSTALL_SBINDIR_SVR4                "bin")
SET(INSTALL_SCRIPTDIR_SVR4              "scripts")
#
SET(INSTALL_LIBDIR_SVR4                 "lib")
SET(INSTALL_PLUGINDIR_SVR4              "lib/plugin")
#
SET(INSTALL_INCLUDEDIR_SVR4             "include")
#
SET(INSTALL_DOCDIR_SVR4                 "docs")
SET(INSTALL_DOCREADMEDIR_SVR4           ".")
SET(INSTALL_MANDIR_SVR4                 "man")
SET(INSTALL_INFODIR_SVR4                "docs")
#
SET(INSTALL_SHAREDIR_SVR4               "share")
SET(INSTALL_MYSQLSHAREDIR_SVR4          "share")
SET(INSTALL_MYSQLTESTDIR_SVR4           "mysql-test")
SET(INSTALL_SUPPORTFILESDIR_SVR4        "support-files")
#
SET(INSTALL_MYSQLDATADIR_SVR4           "/var/lib/mysql")
SET(INSTALL_MYSQLKEYRINGDIR_SVR4        "/var/lib/mysql-keyring")
SET(INSTALL_PLUGINTESTDIR_SVR4          ${plugin_tests})
SET(INSTALL_SECURE_FILE_PRIVDIR_SVR4    ${secure_file_priv_path})
SET(INSTALL_SECURE_FILE_PRIV_EMBEDDEDDIR_SVR4    ${secure_file_priv_embedded_path})


# Clear cached variables if install layout was changed
IF(OLD_INSTALL_LAYOUT)
  IF(NOT OLD_INSTALL_LAYOUT STREQUAL INSTALL_LAYOUT)
    SET(FORCE FORCE)
  ENDIF()
ENDIF()
SET(OLD_INSTALL_LAYOUT ${INSTALL_LAYOUT} CACHE INTERNAL "")

# Set INSTALL_FOODIR variables for chosen layout (for example, INSTALL_BINDIR
# will be defined  as ${INSTALL_BINDIR_STANDALONE} by default if STANDALONE
# layout is chosen)
FOREACH(var BIN SBIN LIB MYSQLSHARE SHARE PLUGIN INCLUDE SCRIPT DOC MAN
  INFO MYSQLTEST DOCREADME SUPPORTFILES MYSQLDATA PLUGINTEST
  SECURE_FILE_PRIV SECURE_FILE_PRIV_EMBEDDED MYSQLKEYRING)
  SET(INSTALL_${var}DIR  ${INSTALL_${var}DIR_${INSTALL_LAYOUT}}
  CACHE STRING "${var} installation directory" ${FORCE})
  MARK_AS_ADVANCED(INSTALL_${var}DIR)
ENDFOREACH()

#
# Set DEFAULT_SECURE_FILE_PRIV_DIR
# This is used as default value for --secure-file-priv
#
IF(INSTALL_SECURE_FILE_PRIVDIR)
  SET(DEFAULT_SECURE_FILE_PRIV_DIR "\"${INSTALL_SECURE_FILE_PRIVDIR}\""
      CACHE INTERNAL "default --secure-file-priv directory" FORCE)
ELSE()
  SET(DEFAULT_SECURE_FILE_PRIV_DIR \"\"
      CACHE INTERNAL "default --secure-file-priv directory" FORCE)
ENDIF()

IF(INSTALL_SECURE_FILE_PRIV_EMBEDDEDDIR)
  SET(DEFAULT_SECURE_FILE_PRIV_EMBEDDED_DIR "\"${INSTALL_SECURE_FILE_PRIV_EMBEDDEDDIR}\""
    CACHE INTERNAL "default --secure-file-priv directory (for embedded library)" FORCE)
ELSE()
  SET(DEFAULT_SECURE_FILE_PRIV_EMBEDDED_DIR "NULL"
    CACHE INTERNAL "default --secure-file-priv directory (for embedded library)" FORCE)
ENDIF()
