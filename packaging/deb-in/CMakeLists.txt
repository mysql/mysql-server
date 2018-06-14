# Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

IF(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  RETURN()
ENDIF()
IF (NOT DEFINED DEB_CODENAME)
  execute_process(
    COMMAND lsb_release -cs
    OUTPUT_VARIABLE DEB_CODENAME
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  SET (DEB_CODENAME ${DEB_CODENAME} CACHE STRING "")
ENDIF()
IF (WITH_NDBCLUSTER)
  SET (DEB_BASE_PRODUCT "cluster-")
  SET (DEB_BASE_PRODUCTC "Cluster ")
  SET (DEB_BASE_VERSION ${MYSQL_CLUSTER_VERSION})
  SET (DEB_CONTROL_SERVER_CONFLICT_NDB "mysql-community-server, mysql-commercial-server")
  SET (DEB_CONTROL_CLIENT_CONFLICT_NDB "mysql-community-client, mysql-commercial-client")
ELSE()
  SET (DEB_BASE_PRODUCT "")
  SET (DEB_BASE_PRODUCTC "")
  SET (DEB_BASE_VERSION ${VERSION})
  SET (DEB_CONTROL_SERVER_CONFLICT_NDB "mysql-cluster-community-server, mysql-cluster-commercial-server")
  SET (DEB_CONTROL_CLIENT_CONFLICT_NDB "mysql-cluster-community-client, mysql-cluster-commercial-client")
ENDIF()
# Timestamp for use in debian/changelog
IF (NOT DEFINED DEB_CHANGELOG_TIMESTAMP)
  execute_process(
    COMMAND date --rfc-2822
    OUTPUT_VARIABLE DEB_CHANGELOG_TIMESTAMP
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  SET (DEB_CHANGELOG_TIMESTAMP ${DEB_CHANGELOG_TIMESTAMP} CACHE STRING "")
ENDIF()
# Commercial or community
IF (DEB_PRODUCT STREQUAL "commercial")
  INCLUDE (deb_commercial.cmake)
  SET (DEB_COPYRIGHT_UPSTREAMNAME "MySQL Commercial Server ${MYSQL_BASE_VERSION}")
  SET (DEB_PRODUCTNAME "${DEB_BASE_PRODUCT}commercial")
  SET (DEB_PRODUCTNAMEC "${DEB_BASE_PRODUCTC}Enterprise")
  SET (DEB_NOTPRODUCTNAME "${DEB_BASE_PRODUCT}community")
  SET (DEB_LICENSENAME "Commercial")
  SET (DEB_SERVERVERSION "${DEB_BASE_VERSION}+commercial-1")
  # List of plugins that are only in commercial packages
  # Plugins that are in both community and commercial should NOT be added here
  SET (DEB_INCLUDE_BINARIES_EXTRA
"
debian/extra/audit_log-plugin
debian/extra/authentication_pam-plugin
debian/extra/authentication_ldap_sasl-plugin
debian/extra/authentication_ldap_simple-plugin
debian/extra/firewall-plugin
debian/extra/keyring_okv-plugin
debian/extra/keyring_encrypted_file-plugin
debian/extra/mysql_clone-plugin
debian/extra/openssl_udf-plugin
debian/extra/thread_pool-plugin
")
  SET (DEB_INSTALL_SERVER_PLUGINS
"
usr/lib/mysql/plugin/audit_log.so
usr/lib/mysql/plugin/authentication_pam.so
usr/lib/mysql/plugin/authentication_ldap_sasl.so
usr/lib/mysql/plugin/authentication_ldap_simple.so
usr/lib/mysql/plugin/keyring_okv.so
usr/lib/mysql/plugin/keyring_encrypted_file.so
usr/lib/mysql/plugin/mysql_clone.so
usr/lib/mysql/plugin/openssl_udf.so
usr/lib/mysql/plugin/thread_pool.so
usr/lib/mysql/plugin/firewall.so
")
  # Add debug versions of commercial plugins, if enabled
  IF (DEFINED DEB_WITH_DEBUG)
    SET (DEB_INSTALL_SERVER_PLUGINS "${DEB_INSTALL_SERVER_PLUGINS}
usr/lib/mysql/plugin/debug/audit_log.so
usr/lib/mysql/plugin/debug/authentication_pam.so
usr/lib/mysql/plugin/debug/authentication_ldap_sasl.so
usr/lib/mysql/plugin/debug/authentication_ldap_simple.so
usr/lib/mysql/plugin/debug/keyring_okv.so
usr/lib/mysql/plugin/debug/keyring_encrypted_file.so
usr/lib/mysql/plugin/debug/mysql_clone.so
usr/lib/mysql/plugin/debug/openssl_udf.so
usr/lib/mysql/plugin/debug/thread_pool.so
usr/lib/mysql/plugin/debug/firewall.so
")
  ENDIF()

  IF (DEB_AWS_SDK)
    SET (DEB_CMAKE_EXTRAS "${DEB_CMAKE_EXTRAS} -DWITH_AWS_SDK=${DEB_AWS_SDK} -DDEB_AWS_SDK=${DEB_AWS_SDK}")
    SET (DEB_INCLUDE_BINARIES_EXTRA "${DEB_INCLUDE_BINARIES_EXTRA}
debian/extra/keyring_aws-plugin
")
    SET (DEB_INSTALL_SERVER_PLUGINS "${DEB_INSTALL_SERVER_PLUGINS}
usr/lib/mysql/plugin/keyring_aws.so
")
    IF (DEFINED DEB_WITH_DEBUG)
      SET (DEB_INSTALL_SERVER_PLUGINS "${DEB_INSTALL_SERVER_PLUGINS}
usr/lib/mysql/plugin/debug/keyring_aws.so
")
    ENDIF()
  ENDIF()

  IF (WITH_CURL)
    SET (DEB_CMAKE_EXTRAS "${DEB_CMAKE_EXTRAS} -DWITH_CURL=${WITH_CURL}")
  ENDIF()
  SET (DEB_REMOVEPATTERN "gpl.in")
ELSE()
  SET (DEB_PRODUCT "${DEB_BASE_PRODUCT}community")
  SET (DEB_COPYRIGHT_UPSTREAMNAME "MySQL Server ${MYSQL_BASE_VERSION}")
  SET (DEB_PRODUCTNAME "${DEB_BASE_PRODUCT}community")
  SET (DEB_PRODUCTNAMEC "${DEB_BASE_PRODUCTC}Community")
  SET (DEB_NOTPRODUCTNAME "${DEB_BASE_PRODUCT}commercial")
  SET (DEB_LICENSENAME "GPL")
  SET (DEB_SERVERVERSION "${DEB_BASE_VERSION}-1")
  SET (DEB_REMOVEPATTERN "com.in")
ENDIF()

# Generate debug symbol packages (this is done automatically in Debian9+)
SET (DEB_RULES_STRIP_DEFAULT
"
override_dh_strip:
	dh_strip -pmysql-${DEB_PRODUCTNAME}-server-core --dbg-package=mysql-${DEB_PRODUCTNAME}-server-core-dbgsym
	dh_strip -pmysql-${DEB_PRODUCTNAME}-client-core --dbg-package=mysql-${DEB_PRODUCTNAME}-client-core-dbgsym
	dh_strip -pmysql-${DEB_PRODUCTNAME}-server --dbg-package=mysql-${DEB_PRODUCTNAME}-server-dbgsym
	dh_strip -pmysql-${DEB_PRODUCTNAME}-client --dbg-package=mysql-${DEB_PRODUCTNAME}-client-dbgsym
	dh_strip -pmysql-${DEB_PRODUCTNAME}-test --dbg-package=mysql-${DEB_PRODUCTNAME}-test-dbgsym
	dh_strip -pmysql-server
	dh_strip -pmysql-client
	dh_strip -pmysql-testsuite
	dh_strip -plibmysqlclient21 --dbg-package=libmysqlclient21-dbgsym
	dh_strip -plibmysqlclient-dev
")
SET (DEB_CONTROL_DBGSYM_DEFAULT
"
Package: libmysqlclient21-dbgsym
Architecture: any
Section: debug
Depends: libmysqlclient21 (=\${binary:Version}), \${misc:Depends}
Description: Debugging symbols for client library

Package: mysql-${DEB_PRODUCTNAME}-test-dbgsym
Architecture: any
Section: debug
Depends: mysql-${DEB_PRODUCTNAME}-test (= \${binary:Version}), \${misc:Depends}
Description: Debugging symbols for test suite

Package: mysql-${DEB_PRODUCTNAME}-client-core-dbgsym
Architecture: any
Section: debug
Depends: mysql-${DEB_PRODUCTNAME}-client-core (= \${binary:Version}), \${misc:Depends}
Description: Debugging symbols for client core

Package: mysql-${DEB_PRODUCTNAME}-server-core-dbgsym
Architecture: any
Section: debug
Depends: mysql-${DEB_PRODUCTNAME}-server-core (= \${binary:Version}), \${misc:Depends}
Description: Debugging symbols for server core

Package: mysql-${DEB_PRODUCTNAME}-server-dbgsym
Architecture: any
Section: debug
Depends: mysql-${DEB_PRODUCTNAME}-server (= \${binary:Version}), \${misc:Depends}
Description: Debugging symbols for server

Package: mysql-${DEB_PRODUCTNAME}-client-dbgsym
Architecture: any
Section: debug
Depends: mysql-${DEB_PRODUCTNAME}-client (= \${binary:Version}), \${misc:Depends}
Description: Debugging symbols for client
")

# Platform specifics. The differences are generally only distro version
# and whether or not systemd and/or apparmor are available
IF(DEB_CODENAME STREQUAL "stretch")
  SET (DEB_PLATFORMRELEASE "debian9")
  SET (DEB_CONTROL_BDEPS ", dh-systemd")
  SET (DEB_INSTALL_SOURCE_XZ "../*.tar.xz usr/src/mysql/")
  SET (DEB_RULES_INSTALL_SYSTEMD
    "install -m 0755 debian/extra/mysql-systemd-start debian/tmp/usr/share/mysql-${MYSQL_BASE_VERSION}/")
  SET (DEB_RULES_INSTALL_APPARMOR "")
  SET (DEB_RULES_APPARMOR_LOAD "")
  SET (DEB_RULES_SYSTEMD_ENABLE "dh_systemd_enable --name=mysql")
  SET (DEB_RULES_SYSTEMD_START "dh_systemd_start --restart-after-upgrade")
  SET (DEB_INSTALL_SERVER_SYSTEMD "usr/share/mysql-${MYSQL_BASE_VERSION}/mysql-systemd-start")
  SET (DEB_INSTALL_SERVER_APPARMOR "")
  SET (DEB_SERVICE_SERVER_EXECPRE
    "ExecStartPre=/usr/share/mysql-${MYSQL_BASE_VERSION}/mysql-systemd-start pre")
  SET (DEB_INIT_APPARMOR "")
  SET (DEB_CONTROL_DBGSYM "")
  SET (DEB_RULES_STRIP "")
ELSEIF(DEB_CODENAME STREQUAL "sid")
  IF (DEFINED DEB_GCC_SNAPSHOT)
    SET (DEB_CMAKE_EXTRAS "${DEB_CMAKE_EXTRAS} -DCMAKE_C_COMPILER=/usr/lib/gcc-snapshot/bin/gcc -DCMAKE_CXX_COMPILER=/usr/lib/gcc-snapshot/bin/g++ -DMYSQL_MAINTAINER_MODE=0 -DCMAKE_CXX_COMPILER_LAUNCHER=ccache")
  ENDIF()
  SET (DEB_PLATFORMRELEASE "debianunstable")
  SET (DEB_CONTROL_BDEPS ", dh-systemd")
  SET (DEB_INSTALL_SOURCE_XZ "../*.tar.xz usr/src/mysql/")
  SET (DEB_RULES_INSTALL_SYSTEMD
    "install -m 0755 debian/extra/mysql-systemd-start debian/tmp/usr/share/mysql-${MYSQL_BASE_VERSION}/")
  SET (DEB_RULES_INSTALL_APPARMOR "")
  SET (DEB_RULES_APPARMOR_LOAD "")
  SET (DEB_RULES_SYSTEMD_ENABLE "dh_systemd_enable --name=mysql")
  SET (DEB_RULES_SYSTEMD_START "dh_systemd_start --restart-after-upgrade")
  SET (DEB_INSTALL_SERVER_SYSTEMD "usr/share/mysql-${MYSQL_BASE_VERSION}/mysql-systemd-start")
  SET (DEB_INSTALL_SERVER_APPARMOR "")
  SET (DEB_SERVICE_SERVER_EXECPRE
    "ExecStartPre=/usr/share/mysql-${MYSQL_BASE_VERSION}/mysql-systemd-start pre")
  SET (DEB_INIT_APPARMOR "")
  SET (DEB_CONTROL_DBGSYM "")
  SET (DEB_RULES_STRIP "")
ELSEIF(DEB_CODENAME STREQUAL "xenial")
  SET (DEB_PLATFORMRELEASE "ubuntu16.04")
  SET (DEB_CONTROL_BDEPS ", dh-apparmor, dh-systemd (>=1.5)")
  SET (DEB_INSTALL_SOURCE_XZ "../*.tar.xz usr/src/mysql/")
  SET (DEB_RULES_INSTALL_SYSTEMD
    "install -m 0755 debian/extra/mysql-systemd-start debian/tmp/usr/share/mysql-${MYSQL_BASE_VERSION}/")
  SET (DEB_RULES_INSTALL_APPARMOR
    "install -g root -o root -m 0644 -D debian/extra/apparmor-profile debian/tmp/etc/apparmor.d/usr.sbin.mysqld")
  SET (DEB_RULES_APPARMOR_LOAD
    "dh_apparmor -pmysql-${DEB_PRODUCTNAME}-server --profile-name=usr.sbin.mysqld")
  SET (DEB_RULES_SYSTEMD_ENABLE "dh_systemd_enable --name=mysql")
  SET (DEB_RULES_SYSTEMD_START "dh_systemd_start --restart-after-upgrade")
  SET (DEB_INSTALL_SERVER_SYSTEMD "usr/share/mysql-${MYSQL_BASE_VERSION}/mysql-systemd-start")
  SET (DEB_INSTALL_SERVER_APPARMOR "etc/apparmor.d/usr.sbin.mysqld")
  SET (DEB_SERVICE_SERVER_EXECPRE
    "ExecStartPre=/usr/share/mysql-${MYSQL_BASE_VERSION}/mysql-systemd-start pre")
  SET (DEB_INIT_APPARMOR "/lib/apparmor/profile-load usr.sbin.mysqld")
  SET (DEB_CONTROL_DBGSYM ${DEB_CONTROL_DBGSYM_DEFAULT})
  SET (DEB_RULES_STRIP ${DEB_RULES_STRIP_DEFAULT})
ELSEIF(DEB_CODENAME STREQUAL "bionic")
  SET (DEB_PLATFORMRELEASE "ubuntu18.04")
  SET (DEB_CONTROL_BDEPS ", dh-apparmor, dh-systemd (>=1.5)")
  SET (DEB_INSTALL_SOURCE_XZ "../*.tar.xz usr/src/mysql/")
  SET (DEB_RULES_INSTALL_SYSTEMD
    "install -m 0755 debian/extra/mysql-systemd-start debian/tmp/usr/share/mysql-${MYSQL_BASE_VERSION}/")
  SET (DEB_RULES_INSTALL_APPARMOR
    "install -g root -o root -m 0644 -D debian/extra/apparmor-profile debian/tmp/etc/apparmor.d/usr.sbin.mysqld")
  SET (DEB_RULES_APPARMOR_LOAD
    "dh_apparmor -pmysql-${DEB_PRODUCTNAME}-server --profile-name=usr.sbin.mysqld")
  SET (DEB_RULES_SYSTEMD_ENABLE "dh_systemd_enable --name=mysql")
  SET (DEB_RULES_SYSTEMD_START "dh_systemd_start --restart-after-upgrade")
  SET (DEB_INSTALL_SERVER_SYSTEMD "usr/share/mysql-${MYSQL_BASE_VERSION}/mysql-systemd-start")
  SET (DEB_INSTALL_SERVER_APPARMOR "etc/apparmor.d/usr.sbin.mysqld")
  SET (DEB_SERVICE_SERVER_EXECPRE
    "ExecStartPre=/usr/share/mysql-${MYSQL_BASE_VERSION}/mysql-systemd-start pre")
  SET (DEB_INIT_APPARMOR "/lib/apparmor/profile-load usr.sbin.mysqld")
  SET (DEB_CONTROL_DBGSYM "")
  SET (DEB_RULES_STRIP "")
ELSE()
  MESSAGE(STATUS
    "Skipping deb packaging on unsupported platform ${DEB_CODENAME}.")
  RETURN()
ENDIF()

IF (WITH_NDBCLUSTER)
  INCLUDE(deb_ndb.cmake)
ENDIF()
if (DEFINED DEB_WITH_DEBUG)
  INCLUDE(deb_debug.cmake)
ENDIF()
# All files are configured and copied to the debian/ directory, which is used 
# by debuild to make the packages
SET (DEB_ROOT ${CMAKE_SOURCE_DIR}/packaging/deb-in)
FILE(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/debian)
FILE(GLOB_RECURSE SOURCEFILES RELATIVE "${DEB_ROOT}" "${DEB_ROOT}/*")
# Skip the copyright file from the "other" product type
LIST(REMOVE_ITEM SOURCEFILES "copyright.${DEB_REMOVEPATTERN}")
LIST(REMOVE_ITEM SOURCEFILES "CMakeLists.txt")
# Don't install systemd file on systems without systemd
IF(DEB_INSTALL_SERVER_SYSTEMD STREQUAL "")
	LIST(REMOVE_ITEM SOURCEFILES "mysql-packagesource-server.mysql.service.in")
ENDIF()
FOREACH (SOURCEFILE ${SOURCEFILES})
  STRING(REGEX REPLACE "-packagesource-" "-${DEB_PRODUCTNAME}-"
    TMPFILE ${SOURCEFILE})
  # Strip away the input file endings from the destination filenames
  STRING(REGEX REPLACE ".in$" "" TMPFILE ${TMPFILE})
  STRING(REGEX REPLACE ".gpl$" "" TMPFILE ${TMPFILE})
  STRING(REGEX REPLACE ".com$" "" DESTFILE ${TMPFILE})
  CONFIGURE_FILE("${DEB_ROOT}/${SOURCEFILE}"
    "${CMAKE_BINARY_DIR}/debian/${DESTFILE}" @ONLY)
ENDFOREACH()
EXECUTE_PROCESS(
  COMMAND chmod +x ${CMAKE_BINARY_DIR}/debian/rules
)

