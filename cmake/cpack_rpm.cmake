IF(RPM)

SET(CPACK_GENERATOR "RPM")
SET(CPACK_RPM_PACKAGE_DEBUG 1)
SET(INSTALL_LAYOUT "RPM")
CMAKE_MINIMUM_REQUIRED(VERSION 2.8.7)

SET(CPACK_RPM_COMPONENT_INSTALL ON)

SET(CPACK_COMPONENT_SERVER_GROUP "server")
SET(CPACK_COMPONENT_MANPAGESSERVER_GROUP "server")
SET(CPACK_COMPONENT_INIFILES_GROUP "server")
SET(CPACK_COMPONENT_SERVER_SCRIPTS_GROUP "server")
SET(CPACK_COMPONENT_SUPPORTFILES_GROUP "server")
SET(CPACK_COMPONENT_DEVELOPMENT_GROUP "devel")
SET(CPACK_COMPONENT_MANPAGESDEVELOPMENT_GROUP "devel")
SET(CPACK_COMPONENT_TEST_GROUP "test")
SET(CPACK_COMPONENT_MANPAGESTEST_GROUP "test")
SET(CPACK_COMPONENT_CLIENT_GROUP "client")
SET(CPACK_COMPONENT_MANPAGESCLIENT_GROUP "client")
SET(CPACK_COMPONENT_README_GROUP "server")
SET(CPACK_COMPONENT_SHAREDLIBRARIES_GROUP "shared")
SET(CPACK_COMPONENTS_ALL Server ManPagesServer IniFiles Server_Scripts
                                SupportFiles Development ManPagesDevelopment
                                Test ManPagesTest Readme ManPagesClient
                                Client SharedLibraries)

SET(CPACK_RPM_PACKAGE_NAME "MariaDB")
SET(CPACK_PACKAGE_FILE_NAME "${CPACK_RPM_PACKAGE_NAME}-${VERSION}-${RPM}-${CMAKE_SYSTEM_PROCESSOR}")

SET(CPACK_RPM_PACKAGE_RELEASE 1) # FIX: add distribution name here
SET(CPACK_RPM_PACKAGE_LICENSE "GPL")
SET(CPACK_RPM_PACKAGE_RELOCATABLE FALSE)
SET(CPACK_RPM_PACKAGE_GROUP "Applications/Databases")
SET(CPACK_RPM_PACKAGE_URL "http://mariadb.org")
SET(CPACK_RPM_PACKAGE_SUMMARY "MariaDB: a very fast and robust SQL database server")
SET(CPACK_RPM_PACKAGE_DESCRIPTION "${CPACK_RPM_PACKAGE_SUMMARY}

It is GPL v2 licensed, which means you can use the it free of charge under the
conditions of the GNU General Public License Version 2 (http://www.gnu.org/licenses/).

MariaDB documentation can be found at http://kb.askmonty.org/
MariaDB bug reports should be submitted through https://mariadb.atlassian.net/

")

SET(CPACK_RPM_SPEC_MORE_DEFINE "
%define mysql_vendor ${CPACK_PACKAGE_VENDOR}
%define mysqlversion ${MYSQL_NO_DASH_VERSION}
%define mysqldatadir /var/lib/mysql
%define mysqld_user  mysql
%define mysqld_group mysql
")

# this creative hack is described here: http://www.cmake.org/pipermail/cmake/2012-January/048416.html
# both /etc and /etc/init.d should be ignored as of 2.8.7
# only /etc/init.d as of 2.8.8
# and eventually this hack should go away completely
SET(CPACK_RPM_SPEC_MORE_DEFINE "${CPACK_RPM_SPEC_MORE_DEFINE}
%define ignore \#
")
set(CPACK_RPM_server_USER_FILELIST "%ignore /etc" "%ignore /etc/init.d")

SET(CPACK_RPM_client_PACKAGE_OBSOLETES "mysql-client MariaDB-client MySQL-client MySQL-OurDelta-client")
SET(CPACK_RPM_client_PACKAGE_PROVIDES "MariaDB-client MySQL-client mysql-client")

SET(CPACK_RPM_devel_PACKAGE_OBSOLETES "mysql-devel MariaDB-devel MySQL-devel MySQL-OurDelta-devel")
SET(CPACK_RPM_devel_PACKAGE_PROVIDES "MariaDB-devel MySQL-devel mysql-devel")

SET(CPACK_RPM_server_PACKAGE_OBSOLETES "MariaDB mysql mysql-server MariaDB-server MySQL-server MySQL-OurDelta-server")
SET(CPACK_RPM_server_PACKAGE_PROVIDES "MariaDB MariaDB-server MySQL-server config(MariaDB-server) msqlormysql mysql mysql-server")
SET(CPACK_RPM_server_PRE_INSTALL_SCRIPT_FILE ${CMAKE_SOURCE_DIR}/support-files/rpm-prein.sh)
SET(CPACK_RPM_server_PRE_UNINSTALL_SCRIPT_FILE ${CMAKE_SOURCE_DIR}/support-files/rpm-preun.sh)
SET(CPACK_RPM_server_POST_INSTALL_SCRIPT_FILE ${CMAKE_SOURCE_DIR}/support-files/rpm-postin.sh)

SET(CPACK_RPM_shared_PACKAGE_OBSOLETES "mysql-shared MySQL-shared-standard MySQL-shared-pro MySQL-shared-pro-cert MySQL-shared-pro-gpl MySQL-shared-pro-gpl-cert MariaDB-shared MySQL-shared MySQL-OurDelta-shared")
SET(CPACK_RPM_shared_PACKAGE_PROVIDES "MariaDB-shared MySQL-shared mysql-shared mysql-libs libmysqlclient.so.${SHARED_LIB_MAJOR_VERSION} libmysqlclient.so.${SHARED_LIB_MAJOR_VERSION}(libmysqlclient_${SHARED_LIB_MAJOR_VERSION}) libmysqlclient_r.so.${SHARED_LIB_MAJOR_VERSION} libmysqlclient_r.so.${SHARED_LIB_MAJOR_VERSION}(libmysqlclient_${SHARED_LIB_MAJOR_VERSION})")
SET(CPACK_RPM_shared_POST_INSTALL_SCRIPT_FILE ${CMAKE_SOURCE_DIR}/support-files/rpm-ldconfig.sh)
SET(CPACK_RPM_shared_POST_UNINSTALL_SCRIPT_FILE ${CMAKE_SOURCE_DIR}/support-files/rpm-ldconfig.sh)

SET(CPACK_RPM_test_PACKAGE_OBSOLETES "mysql-test MariaDB-test MySQL-test MySQL-OurDelta-test")
SET(CPACK_RPM_test_PACKAGE_PROVIDES "MariaDB-test MySQL-test mysql-test")

# workaround for lots of perl dependencies added by rpmbuild
SET(CPACK_RPM_test_PACKAGE_PROVIDES "${CPACK_RPM_test_PACKAGE_PROVIDES} perl(lib::mtr_gcov.pl) perl(lib::mtr_gprof.pl) perl(lib::mtr_io.pl) perl(lib::mtr_misc.pl) perl(lib::mtr_process.pl) perl(lib::v1/mtr_cases.pl) perl(lib::v1/mtr_gcov.pl) perl(lib::v1/mtr_gprof.pl) perl(lib::v1/mtr_im.pl) perl(lib::v1/mtr_io.pl) perl(lib::v1/mtr_match.pl) perl(lib::v1/mtr_misc.pl) perl(lib::v1/mtr_process.pl) perl(lib::v1/mtr_report.pl) perl(lib::v1/mtr_stress.pl) perl(lib::v1/mtr_timer.pl) perl(lib::v1/mtr_unique.pl) perl(mtr_misc.pl)")

ENDIF(RPM)

