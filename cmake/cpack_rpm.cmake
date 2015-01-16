IF(RPM)

SET(CPACK_GENERATOR "RPM")
SET(CPACK_RPM_PACKAGE_DEBUG 1)
SET(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
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
SET(CPACK_COMPONENT_COMMON_GROUP "common")
SET(CPACK_COMPONENT_COMPAT_GROUP "compat")
SET(CPACK_COMPONENTS_ALL Server ManPagesServer IniFiles Server_Scripts
                         SupportFiles Development ManPagesDevelopment
                         ManPagesTest Readme ManPagesClient Test 
                         Common Client SharedLibraries)

SET(CPACK_RPM_PACKAGE_NAME "MariaDB")
SET(CPACK_PACKAGE_FILE_NAME "${CPACK_RPM_PACKAGE_NAME}-${VERSION}-${RPM}-${CMAKE_SYSTEM_PROCESSOR}")

SET(CPACK_RPM_PACKAGE_RELEASE "1%{?dist}")
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
%define mysqlbasedir ${CMAKE_INSTALL_PREFIX}
%define mysqldatadir ${INSTALL_MYSQLDATADIR}
%define mysqld_user  mysql
%define mysqld_group mysql
%define _bindir     ${CMAKE_INSTALL_PREFIX}/${INSTALL_BINDIR}
%define _sbindir    ${CMAKE_INSTALL_PREFIX}/${INSTALL_SBINDIR}
%define _sysconfdir ${INSTALL_SYSCONFDIR}
")

# this creative hack is described here: http://www.cmake.org/pipermail/cmake/2012-January/048416.html
# both /etc and /etc/init.d should be ignored as of 2.8.7
# only /etc/init.d as of 2.8.8
# and eventually this hack should go away completely
SET(CPACK_RPM_SPEC_MORE_DEFINE "${CPACK_RPM_SPEC_MORE_DEFINE}
%define ignore \#
")

SET(CPACK_RPM_PACKAGE_REQUIRES "MariaDB-common")

SET(ignored
  "%ignore /etc"
  "%ignore /etc/init.d"
  "%ignore /etc/logrotate.d"
  "%ignore ${CMAKE_INSTALL_PREFIX}"
  "%ignore ${CMAKE_INSTALL_PREFIX}/bin"
  "%ignore ${CMAKE_INSTALL_PREFIX}/include"
  "%ignore ${CMAKE_INSTALL_PREFIX}/lib"
  "%ignore ${CMAKE_INSTALL_PREFIX}/lib64"
  "%ignore ${CMAKE_INSTALL_PREFIX}/sbin"
  "%ignore ${CMAKE_INSTALL_PREFIX}/share"
  "%ignore ${CMAKE_INSTALL_PREFIX}/share/aclocal"
  "%ignore ${CMAKE_INSTALL_PREFIX}/share/doc"
  "%ignore ${CMAKE_INSTALL_PREFIX}/share/man"
  "%ignore ${CMAKE_INSTALL_PREFIX}/share/man/man1*"
  "%ignore ${CMAKE_INSTALL_PREFIX}/share/man/man8*"
  )

SET(CPACK_RPM_server_USER_FILELIST ${ignored} "%config(noreplace) ${INSTALL_SYSCONF2DIR}/*")
SET(CPACK_RPM_common_USER_FILELIST ${ignored} "%config(noreplace) ${INSTALL_SYSCONFDIR}/my.cnf")
SET(CPACK_RPM_shared_USER_FILELIST ${ignored} "%config(noreplace) ${INSTALL_SYSCONF2DIR}/*")
SET(CPACK_RPM_client_USER_FILELIST ${ignored} "%config(noreplace) ${INSTALL_SYSCONF2DIR}/*")
SET(CPACK_RPM_compat_USER_FILELIST ${ignored})
SET(CPACK_RPM_devel_USER_FILELIST ${ignored})
SET(CPACK_RPM_test_USER_FILELIST ${ignored})

# "set/append array" - append a set of strings, separated by a space
MACRO(SETA var)
  FOREACH(v ${ARGN})
    SET(${var} "${${var}} ${v}")
  ENDFOREACH()
ENDMACRO(SETA)

SETA(CPACK_RPM_client_PACKAGE_OBSOLETES
  "mysql-client"
  "MySQL-client"
  "MySQL-OurDelta-client")
SETA(CPACK_RPM_client_PACKAGE_PROVIDES
  "MySQL-client"
  "mysql-client")

SETA(CPACK_RPM_devel_PACKAGE_OBSOLETES
  "MySQL-devel"
  "MySQL-OurDelta-devel")
SETA(CPACK_RPM_devel_PACKAGE_PROVIDES
  "MySQL-devel")

SETA(CPACK_RPM_server_PACKAGE_OBSOLETES
  "MariaDB"
  "MySQL"
  "mysql-server"
  "MySQL-server"
  "MySQL-OurDelta-server")
SETA(CPACK_RPM_server_PACKAGE_PROVIDES
  "MariaDB"
  "MySQL"
  "MySQL-server"
  "msqlormysql"
  "mysql-server")

SETA(CPACK_RPM_shared_PACKAGE_OBSOLETES
  "mysql-shared"
  "MySQL-shared-standard"
  "MySQL-shared-pro"
  "MySQL-shared-pro-cert"
  "MySQL-shared-pro-gpl"
  "MySQL-shared-pro-gpl-cert"
  "MySQL-shared"
  "MySQL-OurDelta-shared")
SETA(CPACK_RPM_shared_PACKAGE_PROVIDES
  "MySQL-shared"
  "mysql-shared")

SETA(CPACK_RPM_test_PACKAGE_OBSOLETES
  "MySQL-test"
  "MySQL-OurDelta-test")
SETA(CPACK_RPM_test_PACKAGE_PROVIDES
  "MySQL-test")

SETA(CPACK_RPM_server_PACKAGE_REQUIRES 
   ${CPACK_RPM_PACKAGE_REQUIRES}
   "MariaDB-client")

SET(CPACK_RPM_server_PRE_INSTALL_SCRIPT_FILE ${CMAKE_SOURCE_DIR}/support-files/rpm/server-prein.sh)
SET(CPACK_RPM_server_PRE_UNINSTALL_SCRIPT_FILE ${CMAKE_SOURCE_DIR}/support-files/rpm/server-preun.sh)
SET(CPACK_RPM_server_POST_INSTALL_SCRIPT_FILE ${CMAKE_SOURCE_DIR}/support-files/rpm/server-postin.sh)
SET(CPACK_RPM_server_POST_UNINSTALL_SCRIPT_FILE ${CMAKE_SOURCE_DIR}/support-files/rpm/server-postun.sh)
SET(CPACK_RPM_shared_POST_INSTALL_SCRIPT_FILE ${CMAKE_SOURCE_DIR}/support-files/rpm/shared-post.sh)
SET(CPACK_RPM_shared_POST_UNINSTALL_SCRIPT_FILE ${CMAKE_SOURCE_DIR}/support-files/rpm/shared-post.sh)

MACRO(ALTERNATIVE_NAME real alt)
  SET(ver "%{version}-%{release}")
  IF (${epoch})
    SET(ver "${epoch}:${ver}")
  ENDIF()

  SET(p "CPACK_RPM_${real}_PACKAGE_PROVIDES")
  SET(${p} "${${p}} ${alt} = ${ver} ${alt}%{?_isa} = ${ver} config(${alt}) = ${ver}")
  SET(o "CPACK_RPM_${real}_PACKAGE_OBSOLETES")
  SET(${o} "${${o}} ${alt} ${alt}%{_isa}")
ENDMACRO(ALTERNATIVE_NAME)

ALTERNATIVE_NAME("devel"  "mysql-devel")
ALTERNATIVE_NAME("server" "mysql-server")
ALTERNATIVE_NAME("test"   "mysql-test")

# Argh! Different distributions call packages differently, to be a drop-in
# replacement we have to fake distribution-speficic dependencies

IF(RPM MATCHES "(rhel|centos)5")
  ALTERNATIVE_NAME("shared" "mysql")
ELSEIF(RPM MATCHES "(rhel|centos)6")
  ALTERNATIVE_NAME("client" "mysql")
  ALTERNATIVE_NAME("shared" "mysql-libs")
ELSEIF(RPM MATCHES "fedora" OR RPM MATCHES "(rhel|centos)7")
  SET(epoch 1) # this is fedora
  ALTERNATIVE_NAME("client" "mariadb")
  ALTERNATIVE_NAME("client" "mysql")
  ALTERNATIVE_NAME("devel"  "mariadb-devel")
  ALTERNATIVE_NAME("server" "mariadb-server")
  ALTERNATIVE_NAME("server" "mysql-compat-server")
  ALTERNATIVE_NAME("shared" "mariadb-libs")
  ALTERNATIVE_NAME("shared" "mysql-libs")
  ALTERNATIVE_NAME("test"   "mariadb-test")
  SET(CPACK_RPM_common_PACKAGE_CONFLICTS "mariadb-libs < 1:%{version}-%{release}") 
ENDIF()

# workaround for lots of perl dependencies added by rpmbuild
SETA(CPACK_RPM_test_PACKAGE_PROVIDES
  "perl(lib::mtr_gcov.pl)"
  "perl(lib::mtr_gprof.pl)"
  "perl(lib::mtr_io.pl)"
  "perl(lib::mtr_misc.pl)"
  "perl(lib::mtr_process.pl)"
  "perl(lib::v1/mtr_cases.pl)"
  "perl(lib::v1/mtr_gcov.pl)"
  "perl(lib::v1/mtr_gprof.pl)"
  "perl(lib::v1/mtr_im.pl)"
  "perl(lib::v1/mtr_io.pl)"
  "perl(lib::v1/mtr_match.pl)"
  "perl(lib::v1/mtr_misc.pl)"
  "perl(lib::v1/mtr_process.pl)"
  "perl(lib::v1/mtr_report.pl)"
  "perl(lib::v1/mtr_stress.pl)"
  "perl(lib::v1/mtr_timer.pl)"
  "perl(lib::v1/mtr_unique.pl)"
  "perl(mtr_cases)"
  "perl(mtr_io.pl)"
  "perl(mtr_match)"
  "perl(mtr_misc.pl)"
  "perl(mtr_report)"
  "perl(mtr_results)"
  "perl(mtr_unique)")

# If we want to build build MariaDB-shared-compat,
# extract compat libraries from MariaDB-shared-5.3 rpm
FILE(GLOB compat_rpm RELATIVE ${CMAKE_SOURCE_DIR}
    "${CMAKE_SOURCE_DIR}/../MariaDB-shared-5.3.*.rpm")
IF (compat_rpm)
  MESSAGE("Using ${compat_rpm} to build MariaDB-compat")
  INSTALL(CODE "EXECUTE_PROCESS(
                 COMMAND rpm2cpio ${CMAKE_SOURCE_DIR}/${compat_rpm}
                 COMMAND cpio --extract --make-directories */libmysqlclient*.so.* -
                 WORKING_DIRECTORY \$ENV{DESTDIR})
                EXECUTE_PROCESS(
                 COMMAND chmod -R a+rX .
                 WORKING_DIRECTORY \$ENV{DESTDIR})"
                 COMPONENT Compat)
  SET(CPACK_COMPONENTS_ALL ${CPACK_COMPONENTS_ALL} Compat)

  # RHEL6/CentOS6 install Postfix by default, and it requires
  # libmysqlclient.so.16 that pulls in mysql-libs-5.1.x
  # And the latter conflicts with our rpms.
  # Make sure that for these distribuions all our rpms require
  # MariaDB-compat, that will replace mysql-libs-5.1
  IF(RPM MATCHES "(rhel|centos)6")
    SET(CPACK_RPM_common_PACKAGE_REQUIRES "MariaDB-compat")
  ENDIF()
ENDIF(compat_rpm)

SET(CPACK_RPM_compat_PACKAGE_PROVIDES "mysql-libs = 5.3.5") # exact version doesn't matter as long as it greater than 5.1
SET(CPACK_RPM_compat_PACKAGE_OBSOLETES "mysql-libs < 5.3.5")

ENDIF(RPM)

