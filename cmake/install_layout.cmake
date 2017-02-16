# Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.
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
#  RPM
#    Build as per default RPM layout, with prefix=/usr
#
#  DEB
#    Build as per STANDALONE, prefix=/opt/mysql/server-$major.$minor
#
#  SVR4
#    Solaris package layout suitable for pkg* tools, prefix=/opt/mysql/mysql
#
# To force a directory layout, use -DINSTALL_LAYOUT=<layout>.
#
# The default is STANDALONE.
#
# There is the possibility to further fine-tune installation directories.
# Several variables can be overwritten:
#
# - INSTALL_BINDIR          (directory with client executables and scripts)
# - INSTALL_SBINDIR         (directory with mysqld)
# - INSTALL_SCRIPTDIR       (several scripts, rarely used)
# - INSTALL_SYSCONFDIR	    (my.cnf config file. Usually /etc or nothing)
# - INSTALL_SYSCONF2DIR     (additional config files, e.g. /etc/mysql/conf.d)
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
# - INSTALL_SQLBENCHDIR     (sql-bench)
# - INSTALL_SUPPORTFILESDIR (various extra support files)
#
# - INSTALL_MYSQLDATADIR    (data directory)
#
# - INSTALL_UNIX_ADDRDIR    (path to mysql.sock)
#
# When changing this page,  _please_ do not forget to update public Wiki
# http://forge.mysql.com/wiki/CMake#Fine-tuning_installation_paths

IF(NOT INSTALL_LAYOUT)
  IF(DEB)
    SET(INSTALL_LAYOUT "DEB")
  ELSEIF(RPM)
    SET(INSTALL_LAYOUT "RPM")
  ELSE()
    SET(INSTALL_LAYOUT "STANDALONE")
  ENDIF()
ENDIF()

SET(INSTALL_LAYOUT "${INSTALL_LAYOUT}"
CACHE STRING "Installation directory layout. Options are: STANDALONE (as in zip or tar.gz installer) RPM DEB SVR4")

IF(UNIX)
  IF(INSTALL_LAYOUT MATCHES "RPM")
    SET(default_prefix "/usr")
  ELSEIF(INSTALL_LAYOUT MATCHES "DEB")
    SET(default_prefix "/usr")
  ELSEIF(INSTALL_LAYOUT MATCHES "SVR4")
    SET(default_prefix "/opt/mysql/mysql")
  ELSE()
    SET(default_prefix "/usr/local/mysql")
  ENDIF()
  IF(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    SET(CMAKE_INSTALL_PREFIX ${default_prefix}
      CACHE PATH "install prefix" FORCE)
  ENDIF()
  SET(VALID_INSTALL_LAYOUTS "RPM" "STANDALONE" "DEB" "SVR4")
  LIST(FIND VALID_INSTALL_LAYOUTS "${INSTALL_LAYOUT}" ind)
  IF(ind EQUAL -1)
    MESSAGE(FATAL_ERROR "Invalid INSTALL_LAYOUT parameter:${INSTALL_LAYOUT}."
    " Choose between ${VALID_INSTALL_LAYOUTS}" )
  ENDIF()
ENDIF()

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
SET(INSTALL_INCLUDEDIR_STANDALONE       "include/mysql")
#
SET(INSTALL_DOCDIR_STANDALONE           "docs")
SET(INSTALL_DOCREADMEDIR_STANDALONE     ".")
SET(INSTALL_MANDIR_STANDALONE           "man")
SET(INSTALL_INFODIR_STANDALONE          "docs")
#
SET(INSTALL_SHAREDIR_STANDALONE         "share")
SET(INSTALL_MYSQLSHAREDIR_STANDALONE    "share")
SET(INSTALL_MYSQLTESTDIR_STANDALONE     "mysql-test")
SET(INSTALL_SQLBENCHDIR_STANDALONE      ".")
SET(INSTALL_SUPPORTFILESDIR_STANDALONE  "support-files")
#
SET(INSTALL_MYSQLDATADIR_STANDALONE     "data")

SET(INSTALL_UNIX_ADDRDIR_STANDALONE     "/tmp/mysql.sock")
#
# RPM layout
#
SET(INSTALL_BINDIR_RPM                  "bin")
SET(INSTALL_SBINDIR_RPM                 "sbin")
SET(INSTALL_SCRIPTDIR_RPM               "bin")
SET(INSTALL_SYSCONFDIR_RPM		"/etc")
SET(INSTALL_SYSCONF2DIR_RPM             "/etc/my.cnf.d")
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
SET(INSTALL_DOCDIR_RPM                  "share/doc")
SET(INSTALL_DOCREADMEDIR_RPM            "share/doc")
SET(INSTALL_INFODIR_RPM                 "share/info")
SET(INSTALL_MANDIR_RPM                  "share/man")
#
SET(INSTALL_SHAREDIR_RPM                "share")
SET(INSTALL_MYSQLSHAREDIR_RPM           "share/mysql")
SET(INSTALL_MYSQLTESTDIR_RPM            "share/mysql-test")
SET(INSTALL_SQLBENCHDIR_RPM             "")
SET(INSTALL_SUPPORTFILESDIR_RPM         "share/mysql")
#
SET(INSTALL_MYSQLDATADIR_RPM            "/var/lib/mysql")

SET(INSTALL_UNIX_ADDRDIR_RPM            "${INSTALL_MYSQLDATADIR_RPM}/mysql.sock")

#
# DEB layout
#
SET(INSTALL_BINDIR_DEB                  "bin")
SET(INSTALL_SBINDIR_DEB                 "sbin")
SET(INSTALL_SCRIPTDIR_DEB               "bin")
SET(INSTALL_SYSCONF2DIR_DEB             "/etc/mysql/conf.d")
#
SET(INSTALL_LIBDIR_DEB                  "lib")
SET(INSTALL_PLUGINDIR_DEB               "lib/mysql/plugin")
#
SET(INSTALL_INCLUDEDIR_DEB              "include/mysql")
#
SET(INSTALL_DOCDIR_DEB                  "share/doc")
SET(INSTALL_DOCREADMEDIR_DEB            "share/doc")
SET(INSTALL_MANDIR_DEB                  "share/man")
SET(INSTALL_INFODIR_DEB                 "share/info")
#
SET(INSTALL_SHAREDIR_DEB                "share")
SET(INSTALL_MYSQLSHAREDIR_DEB           "share/mysql")
SET(INSTALL_MYSQLTESTDIR_DEB            "mysql-test")
SET(INSTALL_SQLBENCHDIR_DEB             ".")
SET(INSTALL_SUPPORTFILESDIR_DEB         "share/mysql")
#
SET(INSTALL_MYSQLDATADIR_DEB            "/var/lib/mysql")

SET(INSTALL_UNIX_ADDRDIR_DEB            "/var/run/mysqld/mysqld.sock")
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
SET(INSTALL_INCLUDEDIR_SVR4             "include/mysql")
#
SET(INSTALL_DOCDIR_SVR4                 "docs")
SET(INSTALL_DOCREADMEDIR_SVR4           ".")
SET(INSTALL_MANDIR_SVR4                 "man")
SET(INSTALL_INFODIR_SVR4                "docs")
#
SET(INSTALL_SHAREDIR_SVR4               "share")
SET(INSTALL_MYSQLSHAREDIR_SVR4          "share")
SET(INSTALL_MYSQLTESTDIR_SVR4           "mysql-test")
SET(INSTALL_SQLBENCHDIR_SVR4            ".")
SET(INSTALL_SUPPORTFILESDIR_SVR4        "support-files")
#
SET(INSTALL_MYSQLDATADIR_SVR4           "/var/lib/mysql")

SET(INSTALL_UNIX_ADDRDIR_SVR            "/tmp/mysql.sock")

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
FOREACH(var BIN SBIN LIB MYSQLSHARE SHARE PLUGIN INCLUDE SCRIPT DOC MAN SYSCONF SYSCONF2
    INFO MYSQLTEST SQLBENCH DOCREADME SUPPORTFILES MYSQLDATA UNIX_ADDR)
  SET(INSTALL_${var}DIR  ${INSTALL_${var}DIR_${INSTALL_LAYOUT}}
  CACHE STRING "${var} installation directory" ${FORCE})
  MARK_AS_ADVANCED(INSTALL_${var}DIR)
ENDFOREACH()

IF(NOT MYSQL_UNIX_ADDR)
  SET(MYSQL_UNIX_ADDR ${INSTALL_UNIX_ADDRDIR})
ENDIF()

