#
#   This file was modified by Oracle in 2011 and later.
#   Details of the modifications are described in the "changelog" section.
#
#   Modifications copyright (c) 2011, 2012, Oracle and/or its
#   affiliates. All rights reserved.
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
# along with this program; see the file COPYING. If not, write to the
# Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston
# MA  02110-1301  USA.

##############################################################################
# Some common macro definitions
##############################################################################

# NOTE: "vendor" is used in upgrade/downgrade check, so you can't
# change these, has to be exactly as is.
# %define mysql_old_vendor        MySQL AB               # Applies to traditional MySQL RPMs only.
# %define mysql_vendor_2          Sun Microsystems, Inc.
%define mysql_vendor            Oracle and/or its affiliates

%define mysql_version   @VERSION@

%define mysqldatadir    /var/lib/mysql

%define release         1

##############################################################################
# Command line handling
##############################################################################
#
# To set options:
#
#   $ rpmbuild --define="option <x>" ...
#

# ----------------------------------------------------------------------------
# Commercial builds
# ----------------------------------------------------------------------------
%if %{undefined commercial}
%define commercial 0
%endif

# ----------------------------------------------------------------------------
# Source name
# ----------------------------------------------------------------------------
%if %{undefined src_base}
%define src_base mysql
%endif
%define src_dir %{src_base}-%{mysql_version}

# ----------------------------------------------------------------------------
# Feature set (storage engines, options).  Default to community (everything)
# ----------------------------------------------------------------------------
%if %{undefined feature_set}
%define feature_set community
%endif

# ----------------------------------------------------------------------------
# Server comment strings
# ----------------------------------------------------------------------------
%if %{undefined compilation_comment_debug}
%define compilation_comment_debug       MySQL Community Server - Debug (GPL)
%endif
%if %{undefined compilation_comment_release}
%define compilation_comment_release     MySQL Community Server (GPL)
%endif

# ----------------------------------------------------------------------------
# Product and server suffixes
# ----------------------------------------------------------------------------
%if %{undefined product_suffix}
  %if %{defined short_product_tag}
    %define product_suffix      -%{short_product_tag}
  %else
    %define product_suffix      %{nil}
  %endif
%endif

%if %{undefined server_suffix}
%define server_suffix   %{nil}
%endif

# ----------------------------------------------------------------------------
# Distribution support
# ----------------------------------------------------------------------------
%if %{undefined distro_specific}
%define distro_specific 0
%endif
%if %{distro_specific}
    %if %(test -f /etc/oracle-release && echo 1 || echo 0)
      %define elver %(rpm -qf --qf '%%{version}\\n' /etc/oracle-release | sed -e 's/^\\([0-9]*\\).*/\\1/g')
      %if "%elver" == "6"
        %define distro_description      Oracle Linux 6
        %define distro_releasetag       el6
        %define distro_buildreq         gcc-c++ ncurses-devel perl readline-devel time zlib-devel
        %define distro_requires         chkconfig coreutils grep procps shadow-utils net-tools
      %else
        %{error:Oracle Linux %{elver} is unsupported}
      %endif
    %else
      %if %(test -f /etc/redhat-release && echo 1 || echo 0)
        %define rhelver %(rpm -qf --qf '%%{version}\\n' /etc/redhat-release | sed -e 's/^\\([0-9]*\\).*/\\1/g')
          %if "%rhelver" == "5"
            %define distro_description    Red Hat Enterprise Linux 5
            %define distro_releasetag     rhel5
            %define distro_buildreq       gcc-c++ gperf ncurses-devel perl readline-devel time zlib-devel
            %define distro_requires       chkconfig coreutils grep procps shadow-utils net-tools
          %else
            %if "%rhelver" == "6"
              %define distro_description    Red Hat Enterprise Linux 6
              %define distro_releasetag     rhel6
              %define distro_buildreq       gcc-c++ ncurses-devel perl readline-devel time zlib-devel
              %define distro_requires       chkconfig coreutils grep procps shadow-utils net-tools
            %else
              %{error:Red Hat Enterprise Linux %{rhelver} is unsupported}
            %endif
          %endif
      %else
        %if %(test -f /etc/SuSE-release && echo 1 || echo 0)
          %define susever %(rpm -qf --qf '%%{version}\\n' /etc/SuSE-release | cut -d. -f1)
          %if "%susever" == "10"
            %define distro_description    SUSE Linux Enterprise Server 10
            %define distro_releasetag     sles10
            %define distro_buildreq       gcc-c++ gdbm-devel gperf ncurses-devel openldap2-client readline-devel zlib-devel
            %define distro_requires       aaa_base coreutils grep procps pwdutils
          %else
            %if "%susever" == "11"
              %define distro_description  SUSE Linux Enterprise Server 11
              %define distro_releasetag   sles11
              %define distro_buildreq     gcc-c++ gdbm-devel gperf ncurses-devel openldap2-client procps pwdutils readline-devel zlib-devel
              %define distro_requires     aaa_base coreutils grep procps pwdutils
            %else
              %{error:SuSE %{susever} is unsupported}
            %endif
          %endif
        %else
          %{error:Unsupported distribution}
        %endif
      %endif
    %endif
%else
  %define generic_kernel %(uname -r | cut -d. -f1-2)
  %define distro_description            Generic Linux (kernel %{generic_kernel})
  %define distro_releasetag             linux%{generic_kernel}
  %define distro_buildreq               gcc-c++ gperf ncurses-devel perl readline-devel time zlib-devel
  %define distro_requires               coreutils grep procps /sbin/chkconfig /usr/sbin/useradd /usr/sbin/groupadd
%endif

# Avoid debuginfo RPMs, leaves binaries unstripped
%define debug_package   %{nil}

# Hack to work around bug in RHEL5 __os_install_post macro, wrong inverted
# test for __debug_package
%define __strip         /bin/true

# ----------------------------------------------------------------------------
# Support optional "tcmalloc" library (experimental)
# ----------------------------------------------------------------------------
%if %{defined malloc_lib_target}
%define WITH_TCMALLOC 1
%else
%define WITH_TCMALLOC 0
%endif

##############################################################################
# Configuration based upon above user input, not to be set directly
##############################################################################

%if %{commercial}
%define license_files_server    %{src_dir}/LICENSE.mysql
%define license_type            Commercial
%else
%define license_files_server    %{src_dir}/COPYING %{src_dir}/README
%define license_type            GPL
%endif

##############################################################################
# Main spec file section
##############################################################################

Name: mysql%{product_suffix}
Summary: MySQL client programs and shared libraries
Group: Applications/Databases
Version: @MYSQL_RPM_VERSION@
Release: %{release}%{?distro_releasetag:.%{distro_releasetag}}
# exceptions allow client libraries to be linked with most open source SW,
# not only GPL code.
License: Copyright (c) 2000, @MYSQL_COPYRIGHT_YEAR@, %{mysql_vendor}. All rights reserved. Under %{license_type} license as shown in the Description field.
URL: http://www.mysql.com/
Packager: MySQL Release Engineering <mysql-build@oss.oracle.com>
Vendor:         %{mysql_vendor}

# Regression tests may take a long time, override the default to skip them 
%{!?runselftest:%global runselftest 1}

# Upstream has a mirror redirector for downloads, so the URL is hard to
# represent statically.  You can get the tarball by following a link from
# http://dev.mysql.com/downloads/mysql/
Source0: %{src_dir}.tar.gz
# The upstream tarball includes non-free documentation that only the
# copyright holder (MySQL -> Sun -> Oracle) may ship.
# To remove the non-free documentation, run this script after downloading
# the tarball into the current directory:
# ./generate-tarball.sh $VERSION
# Then, source name changes:
#   Source0: mysql-%{version}-nodocs.tar.gz
%if %{commercial}
NoSource: 0
%endif
Source1: generate-tarball.sh
Source2: mysql.init
Source3: my.cnf
Source4: scriptstub.c
Source5: my_config.h
# The below is only needed for packages built outside MySQL -> Sun -> Oracle:
Source6: README.mysql-docs
Source9: mysql-embedded-check.c
# Working around perl dependency checking bug in rpm FTTB. Remove later.
Source999: filter-requires-mysql.sh

# Patch1: mysql-ssl-multilib.patch           Not needed by MySQL (yaSSL), will not work in 5.5 (cmake)
Patch2: mysql-5.5-errno.patch
Patch4: mysql-5.5-testing.patch
Patch5: mysql-install-test.patch
Patch6: mysql-5.6-stack-guard.patch
# Patch7: mysql-disable-test.patch           Already fixed in current 5.1
# Patch8: mysql-setschedparam.patch          Will not work in 5.5 (cmake)
# Patch9: mysql-no-docs.patch                Will not work in 5.5 (cmake)
Patch10: mysql-strmov.patch
       # Not used by MySQL
# Patch12: mysql-cve-2008-7247.patch         Already fixed in 5.5
Patch13: mysql-expired-certs.patch
       # Will not be used by MySQL
# Patch14: mysql-missing-string-code.patch   Undecided, will not work in 5.5 (cmake)
# Patch15: mysql-lowercase-bug.patch         Fixed in MySQL 5.1.54 and 5.5.9
Patch16: mysql-chain-certs.patch
Patch17: mysql-5.5-libdir.patch
Patch18: mysql-5.5-fix-tests.patch
Patch19: mysql-5.5-mtr1.patch

BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
BuildRequires:  %{distro_buildreq}
BuildRequires: gawk
# make test requires time and ps
BuildRequires: procps
# Socket and Time::HiRes are needed to run regression tests
BuildRequires: perl(Socket), perl(Time::HiRes)

Requires: %{distro_requires}
Requires: fileutils
Requires: mysql-libs%{product_suffix} = %{version}-%{release}
Requires: bash

# If %%{product_suffix} is non-empty, the auto-generated capability is insufficient:
# We want all dependency handling to use the generic name only.
# Similar in other sub-packages
Provides: mysql

# MySQL (with caps) is upstream's spelling of their own RPMs for mysql
Conflicts: MySQL
# mysql-cluster used to be built from this SRPM, but no more
Obsoletes: mysql-cluster < 5.1.44
# We need cross-product "Obsoletes:" to allow cross-product upgrades:
Obsoletes: mysql mysql-advanced

# Working around perl dependency checking bug in rpm FTTB. Remove later.
%global __perl_requires %{SOURCE999}

%description -n mysql%{product_suffix}
MySQL is a multi-user, multi-threaded SQL database server. MySQL is a
client/server implementation consisting of a server daemon (mysqld)
and many different client programs and libraries. The base package
contains the standard MySQL client programs and generic MySQL files.

The MySQL software has Dual Licensing, which means you can use the MySQL
software free of charge under the GNU General Public License
(http://www.gnu.org/licenses/). You can also purchase commercial MySQL
licenses from %{mysql_vendor} if you do not wish to be bound by the terms of
the GPL. See the chapter "Licensing and Support" in the manual for
further info.

%package -n mysql-libs%{product_suffix}

Summary: The shared libraries required for MySQL clients
Group: Applications/Databases
Requires: /sbin/ldconfig
Provides: mysql-libs
Obsoletes: mysql-libs mysql-libs-advanced

%description -n mysql-libs%{product_suffix}
The mysql-libs package provides the essential shared libraries for any 
MySQL client program or interface. You will need to install this package
to use any other MySQL package or any clients that need to connect to a
MySQL server.

%package -n mysql-server%{product_suffix}

Summary: The MySQL server and related files
Group: Applications/Databases
Requires: mysql%{product_suffix} = %{version}-%{release}
Requires: sh-utils
Requires(pre): /usr/sbin/useradd
Requires(post): chkconfig
Requires(preun): chkconfig
# This is for /sbin/service
Requires(preun): initscripts
Requires(postun): initscripts
# mysqlhotcopy needs DBI/DBD support
Requires: perl-DBI, perl-DBD-MySQL
Provides: mysql-server
Conflicts: MySQL-server
Obsoletes: mysql-server mysql-server-advanced

%description -n mysql-server%{product_suffix}
MySQL is a multi-user, multi-threaded SQL database server. MySQL is a
client/server implementation consisting of a server daemon (mysqld)
and many different client programs and libraries. This package contains
the MySQL server and some accompanying files and directories.

%package -n mysql-devel%{product_suffix}

Summary: Files for development of MySQL applications
Group: Applications/Databases
Requires: mysql%{product_suffix} = %{version}-%{release}
Requires: openssl-devel
Provides: mysql-devel
Conflicts: MySQL-devel
Obsoletes: mysql-devel mysql-devel-advanced

%description -n mysql-devel%{product_suffix}
MySQL is a multi-user, multi-threaded SQL database server. This
package contains the libraries and header files that are needed for
developing MySQL client applications.

%package -n mysql-embedded%{product_suffix}

Summary: MySQL as an embeddable library
Group: Applications/Databases
Provides: mysql-embedded
Obsoletes: mysql-embedded mysql-embedded-advanced

%description -n mysql-embedded%{product_suffix}
MySQL is a multi-user, multi-threaded SQL database server. This
package contains a version of the MySQL server that can be embedded
into a client application instead of running as a separate process,
as well as a command line client with such an embedded server.

%package -n mysql-embedded-devel%{product_suffix}

Summary: Development files for MySQL as an embeddable library
Group: Applications/Databases
Requires: mysql-embedded%{product_suffix} = %{version}-%{release}
Requires: mysql-devel%{product_suffix} = %{version}-%{release}
Provides: mysql-embedded-devel
Obsoletes: mysql-embedded-devel mysql-embedded-devel-advanced

%description -n mysql-embedded-devel%{product_suffix}
MySQL is a multi-user, multi-threaded SQL database server. This
package contains files needed for developing and testing with
the embedded version of the MySQL server.

%package -n mysql-test%{product_suffix}

Summary: The test suite distributed with MySQL
Group: Applications/Databases
Requires: mysql%{product_suffix} = %{version}-%{release}
Requires: mysql-server%{product_suffix} = %{version}-%{release}
Provides: mysql-test
Conflicts: MySQL-test
Obsoletes: mysql-test mysql-test-advanced

%description -n mysql-test%{product_suffix}
MySQL is a multi-user, multi-threaded SQL database server. This
package contains the regression test suite distributed with
the MySQL sources.

%prep
%setup -T -a 0 -c -n %{src_dir}

cd %{src_dir} # read about "%setup -n"
# %patch1 -p1
%patch2 -p1
# %patch4 -p1  TODO / FIXME: if wanted, needs to be adapted to new mysql-test-run setup
%patch5 -p1
%patch6 -p1
# %patch8 -p1
# %patch9 -p1
# %patch10 -p1
# %patch13 -p1
# %patch14 -p1
%patch16 -p1
%patch17 -p1
%patch18 -p1
%patch19 -p1

# workaround for upstream bug #56342
rm -f mysql-test/t/ssl_8k_key-master.opt

%build

# Fail quickly and obviously if user tries to build as root
%if %runselftest
	if [ x"`id -u`" = x0 ]; then
		echo "The MySQL regression tests may fail if run as root."
		echo "If you really need to build the RPM as root, use"
		echo "--define='runselftest 0' to skip the regression tests."
		exit 1
	fi
%endif

# Be strict about variables, bail at earliest opportunity, etc.
set -eu

# Optional package files
touch optional-files-devel

#
# Set environment in order of preference, MYSQL_BUILD_* first, then variable
# name, finally a default.  RPM_OPT_FLAGS is assumed to be a part of the
# default RPM build environment.
#
# We set CXX=gcc by default to support so-called 'generic' binaries, where we
# do not have a dependancy on libgcc/libstdc++.  This only works while we do
# not require C++ features such as exceptions, and may need to be removed at
# a later date.
#

# This is a hack, $RPM_OPT_FLAGS on ia64 hosts contains flags which break
# the compile in cmd-line-utils/readline - needs investigation, but for now
# we simply unset it and use those specified directly in cmake.
%if "%{_arch}" == "ia64"
RPM_OPT_FLAGS=
%endif

# This goes in sync with Patch19. "rm" is faster than "patch" for this.
rm -rf %{src_dir}/mysql-test/lib/v1

export PATH=${MYSQL_BUILD_PATH:-$PATH}
export CC=${MYSQL_BUILD_CC:-${CC:-gcc}}
export CXX=${MYSQL_BUILD_CXX:-${CXX:-gcc}}
export CFLAGS=${MYSQL_BUILD_CFLAGS:-${CFLAGS:-$RPM_OPT_FLAGS}}
# Following "%ifarch" developed by RedHat, MySQL/Oracle does not support/maintain Linux/Sparc:
# gcc seems to have some bugs on sparc as of 4.4.1, back off optimization
# submitted as bz #529298
%ifarch sparc sparcv9 sparc64
CFLAGS=`echo $CFLAGS| sed -e "s|-O2|-O1|g" `
%endif
export CXXFLAGS=${MYSQL_BUILD_CXXFLAGS:-${CXXFLAGS:-$RPM_OPT_FLAGS -felide-constructors -fno-exceptions -fno-rtti}}
export LDFLAGS=${MYSQL_BUILD_LDFLAGS:-${LDFLAGS:-}}
export CMAKE=${MYSQL_BUILD_CMAKE:-${CMAKE:-cmake}}
export MAKE_JFLAG=${MYSQL_BUILD_MAKE_JFLAG:-%{?_smp_mflags}}

# Build debug mysqld and libmysqld.a
mkdir debug
(
  cd debug
  # Attempt to remove any optimisation flags from the debug build
  CFLAGS=`echo " ${CFLAGS} " | \
            sed -e 's/ -O[0-9]* / /' \
                -e 's/ -unroll2 / /' \
                -e 's/ -ip / /' \
                -e 's/^ //' \
                -e 's/ $//'`
  CXXFLAGS=`echo " ${CXXFLAGS} " | \
              sed -e 's/ -O[0-9]* / /' \
                  -e 's/ -unroll2 / /' \
                  -e 's/ -ip / /' \
                  -e 's/^ //' \
                  -e 's/ $//'`
  # XXX: MYSQL_UNIX_ADDR should be in cmake/* but mysql_version is included before
  # XXX: install_layout so we can't just set it based on INSTALL_LAYOUT=RPM
  ${CMAKE} ../%{src_dir} -DBUILD_CONFIG=mysql_release -DINSTALL_LAYOUT=RPM \
           -DCMAKE_BUILD_TYPE=Debug \
           -DMYSQL_UNIX_ADDR="%{mysqldatadir}/mysql.sock" \
           -DFEATURE_SET="%{feature_set}" \
           -DCOMPILATION_COMMENT="%{compilation_comment_debug}" \
           -DMYSQL_SERVER_SUFFIX="%{server_suffix}"
  echo BEGIN_DEBUG_CONFIG ; egrep '^#define' include/config.h ; echo END_DEBUG_CONFIG
  make ${MAKE_JFLAG} VERBOSE=1
)
# Build full release
mkdir release
(
  cd release
  # XXX: MYSQL_UNIX_ADDR should be in cmake/* but mysql_version is included before
  # XXX: install_layout so we can't just set it based on INSTALL_LAYOUT=RPM
  ${CMAKE} ../%{src_dir} -DBUILD_CONFIG=mysql_release -DINSTALL_LAYOUT=RPM \
           -DCMAKE_BUILD_TYPE=RelWithDebInfo \
           -DMYSQL_UNIX_ADDR="%{mysqldatadir}/mysql.sock" \
           -DFEATURE_SET="%{feature_set}" \
           -DCOMPILATION_COMMENT="%{compilation_comment_release}" \
           -DMYSQL_SERVER_SUFFIX="%{server_suffix}"
  echo BEGIN_NORMAL_CONFIG ; egrep '^#define' include/config.h ; echo END_NORMAL_CONFIG
  make ${MAKE_JFLAG} VERBOSE=1
)

# TODO / FIXME: Do we need "scriptstub"?
gcc $CFLAGS $LDFLAGS -o scriptstub "-DLIBDIR=\"%{_libdir}/mysql\"" %{SOURCE4}

# TODO / FIXME: "libmysqld.so" should have been produced above  - WORK in PROGRESS
# regular build will make libmysqld.a but not libmysqld.so :-(
cd release
mkdir libmysqld/work
cd libmysqld/work
ar -x ../libmysqld.a
rm rpl_utility.cc.o sql_binlog.cc.o  # Try-and-Error: These modules cause unresolved references
gcc $CFLAGS $LDFLAGS -shared -Wl,-soname,libmysqld.so.0 -o libmysqld.so.0.0.1 \
	*.o \
	-lpthread -lcrypt -laio -lnsl -lssl -lcrypto -lz -lrt -lstdc++ -lm -lc
# this is to check that we built a complete library
cp %{SOURCE9} .
ln -s libmysqld.so.0.0.1 libmysqld.so.0
gcc -I../../include -I../../../%{src_dir}/include $CFLAGS mysql-embedded-check.c libmysqld.so.0
LD_LIBRARY_PATH=. ldd ./a.out
cd ../..
cd ..

# TODO / FIXME: autotools only?
# make check

# TODO / FIXME: Test suite is run elsewhere in release builds -
#               do we need this for users who want to build from source?
# Also, check whether MTR_BUILD_THREAD=auto would solve all issues
%if %runselftest
  # hack to let 32- and 64-bit tests run concurrently on same build machine
  case `uname -m` in
    ppc64 | s390x | x86_64 | sparc64 )
      MTR_BUILD_THREAD=7
      ;;
    *)
      MTR_BUILD_THREAD=11
      ;;
  esac
  export MTR_BUILD_THREAD

  # if you want to change which tests are run, look at mysql-5.5-testing.patch too.
  (cd release && make test-bt-fast )
%endif

%install
RBR=$RPM_BUILD_ROOT
MBD=$RPM_BUILD_DIR/%{src_dir}
[ -n "$RPM_BUILD_ROOT" -a  "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

# Ensure that needed directories exists
# TODO / FIXME: needed ?  install -d $RBR%{mysqldatadir}/mysql
# TODO / FIXME: needed ?  install -d $RBR%{_datadir}/mysql-test
# TODO / FIXME: needed ?  install -d $RBR%{_datadir}/mysql/SELinux/RHEL4
# TODO / FIXME: needed ?  install -d $RBR%{_includedir}
# TODO / FIXME: needed ?  install -d $RBR%{_libdir}
# TODO / FIXME: needed ?  install -d $RBR%{_mandir}
# TODO / FIXME: needed ?  install -d $RBR%{_sbindir}

# Install all binaries
(
  cd $MBD/release
  make DESTDIR=$RBR install
)

# For gcc builds, include libgcc.a in the devel subpackage (BUG 4921).  Do
# this in a sub-shell to ensure we don't pollute the install environment
# with compiler bits.
(
  PATH=${MYSQL_BUILD_PATH:-$PATH}
  CC=${MYSQL_BUILD_CC:-${CC:-gcc}}
  CFLAGS=${MYSQL_BUILD_CFLAGS:-${CFLAGS:-$RPM_OPT_FLAGS}}
  if "${CC}" -v 2>&1 | grep '^gcc.version' >/dev/null 2>&1; then
    libgcc=`${CC} ${CFLAGS} --print-libgcc-file`
    if [ -f ${libgcc} ]; then
      mkdir -p $RBR%{_libdir}/mysql
      install -m 644 ${libgcc} $RBR%{_libdir}/mysql/libmygcc.a
      echo "%{_libdir}/mysql/libmygcc.a" >>optional-files-devel
    fi
  fi
)

# multilib header hacks
# we only apply this to known Red Hat multilib arches, per bug #181335
case `uname -i` in
  i386 | x86_64 | ppc | ppc64 | s390 | s390x | sparc | sparc64 )
    mv $RPM_BUILD_ROOT/usr/include/mysql/my_config.h $RPM_BUILD_ROOT/usr/include/mysql/my_config_`uname -i`.h
    install -m 644 %{SOURCE5} $RPM_BUILD_ROOT/usr/include/mysql/
    ;;
  *)
    ;;
esac

mkdir -p $RPM_BUILD_ROOT/var/log
touch $RPM_BUILD_ROOT/var/log/mysqld.log

# List the installed tree for RPM package maintenance purposes.
find $RPM_BUILD_ROOT -print | sed "s|^$RPM_BUILD_ROOT||" | sort > ROOTFILES

mkdir -p $RPM_BUILD_ROOT/etc/rc.d/init.d
mkdir -p $RPM_BUILD_ROOT/var/run/mysqld
install -m 0755 -d $RPM_BUILD_ROOT/var/lib/mysql
install -m 0755 %{SOURCE2} $RPM_BUILD_ROOT/etc/rc.d/init.d/mysqld
install -m 0644 %{SOURCE3} $RPM_BUILD_ROOT/etc/my.cnf
# obsolete: mv $RPM_BUILD_ROOT/usr/sql-bench $RPM_BUILD_ROOT%{_datadir}/sql-bench   # 'sql-bench' is dropped
# obsolete: mv $RPM_BUILD_ROOT/usr/mysql-test $RPM_BUILD_ROOT%{_datadir}/mysql-test  # 'mysql-test' is there already
# 5.1.32 forgets to install the mysql-test README file
# obsolete: install -m 0644 mysql-test/README $RPM_BUILD_ROOT%{_datadir}/mysql-test/README  # 'README' is there already

mv ${RPM_BUILD_ROOT}%{_bindir}/mysqlbug ${RPM_BUILD_ROOT}%{_libdir}/mysql/mysqlbug
install -m 0755 scriptstub ${RPM_BUILD_ROOT}%{_bindir}/mysqlbug
mv ${RPM_BUILD_ROOT}%{_bindir}/mysql_config ${RPM_BUILD_ROOT}%{_libdir}/mysql/mysql_config
install -m 0755 scriptstub ${RPM_BUILD_ROOT}%{_bindir}/mysql_config

rm -f ${RPM_BUILD_ROOT}%{_libdir}/mysql/libmysqld.a
install -m 0755 release/libmysqld/work/libmysqld.so.0.0.1 ${RPM_BUILD_ROOT}%{_libdir}/mysql/libmysqld.so.0.0.1
ln -s libmysqld.so.0.0.1 ${RPM_BUILD_ROOT}%{_libdir}/mysql/libmysqld.so.0
ln -s libmysqld.so.0 ${RPM_BUILD_ROOT}%{_libdir}/mysql/libmysqld.so

rm -f ${RPM_BUILD_ROOT}%{_bindir}/comp_err
rm -f ${RPM_BUILD_ROOT}%{_mandir}/man1/comp_err.1*
rm -f ${RPM_BUILD_ROOT}%{_bindir}/make_win_binary_distribution
rm -f ${RPM_BUILD_ROOT}%{_bindir}/make_win_src_distribution
rm -f ${RPM_BUILD_ROOT}%{_mandir}/man1/make_win_bin_dist.1*
rm -f ${RPM_BUILD_ROOT}%{_mandir}/man1/make_win_src_distribution.1*
rm -f ${RPM_BUILD_ROOT}%{_libdir}/mysql/libmysqlclient*.la
rm -f ${RPM_BUILD_ROOT}%{_libdir}/mysql/*.a
rm -f ${RPM_BUILD_ROOT}%{_libdir}/mysql/plugin/*.la
rm -f ${RPM_BUILD_ROOT}%{_libdir}/mysql/plugin/*.a
rm -f ${RPM_BUILD_ROOT}%{_datadir}/mysql/binary-configure
rm -f ${RPM_BUILD_ROOT}%{_datadir}/mysql/make_binary_distribution
rm -f ${RPM_BUILD_ROOT}%{_datadir}/mysql/make_sharedlib_distribution
rm -f ${RPM_BUILD_ROOT}%{_datadir}/mysql/mi_test_all*
rm -f ${RPM_BUILD_ROOT}%{_datadir}/mysql/ndb-config-2-node.ini
rm -f ${RPM_BUILD_ROOT}%{_datadir}/mysql/mysql.server
rm -f ${RPM_BUILD_ROOT}%{_datadir}/mysql/mysqld_multi.server
rm -f ${RPM_BUILD_ROOT}%{_datadir}/mysql/MySQL-shared-compat.spec
rm -f ${RPM_BUILD_ROOT}%{_datadir}/mysql/*.plist
rm -f ${RPM_BUILD_ROOT}%{_datadir}/mysql/preinstall
rm -f ${RPM_BUILD_ROOT}%{_datadir}/mysql/postinstall
rm -f ${RPM_BUILD_ROOT}%{_datadir}/mysql/mysql-*.spec
rm -f ${RPM_BUILD_ROOT}%{_datadir}/mysql/mysql-log-rotate
rm -f ${RPM_BUILD_ROOT}%{_datadir}/mysql/ChangeLog
rm -f ${RPM_BUILD_ROOT}%{_mandir}/man1/mysql-stress-test.pl.1*
rm -f ${RPM_BUILD_ROOT}%{_mandir}/man1/mysql-test-run.pl.1*

mkdir -p $RPM_BUILD_ROOT/etc/ld.so.conf.d
echo "%{_libdir}/mysql" > $RPM_BUILD_ROOT/etc/ld.so.conf.d/%{name}-%{_arch}.conf

# The below *only* applies to builds not done by MySQL / Sun / Oracle:
# copy additional docs into build tree so %%doc will find them
# cp %{SOURCE6} README.mysql-docs

%clean
[ -n "$RPM_BUILD_ROOT" -a  "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%pre -n mysql-server%{product_suffix}

# Check if we can safely upgrade.  An upgrade is only safe if it's from one
# of our RPMs in the same version family.

# Handle both ways of spelling the capability.
installed=`rpm -q --whatprovides mysql-server 2> /dev/null`
if [ $? -ne 0 -o -z "$installed" ]; then
  installed=`rpm -q --whatprovides MySQL-server 2> /dev/null`
fi
if [ $? -eq 0 -a -n "$installed" ]; then
  installed=`echo $installed | sed 's/\([^ ]*\) .*/\1/'` # Tests have shown duplicated package names
  vendor=`rpm -q --queryformat='%{VENDOR}' "$installed" 2>&1`
  version=`rpm -q --queryformat='%{VERSION}' "$installed" 2>&1`
  myvendor='%{mysql_vendor}'
  myversion='%{mysql_version}'

  old_family=`echo $version \
    | sed -n -e 's,^\([1-9][0-9]*\.[0-9][0-9]*\)\..*$,\1,p'`
  new_family=`echo $myversion \
    | sed -n -e 's,^\([1-9][0-9]*\.[0-9][0-9]*\)\..*$,\1,p'`

  [ -z "$vendor" ] && vendor='<unknown>'
  [ -z "$old_family" ] && old_family="<unrecognized version $version>"
  [ -z "$new_family" ] && new_family="<bad package specification: version $myversion>"

  error_text=
  if [ "$vendor" != "$myvendor" ]; then
    error_text="$error_text
The current MySQL server package is provided by a different
vendor ($vendor) than $myvendor.
Some files may be installed to different locations, including log
files and the service startup script in %{_sysconfdir}/init.d/.
"
  fi

  if [ "$old_family" != "$new_family" ]; then
    error_text="$error_text
Upgrading directly from MySQL $old_family to MySQL $new_family may not
be safe in all cases.  A manual dump and restore using mysqldump is
recommended.  It is important to review the MySQL manual's Upgrading
section for version-specific incompatibilities.
"
  fi

  if [ -n "$error_text" ]; then
    cat <<HERE >&2

******************************************************************
A MySQL server package ($installed) is installed.
$error_text
A manual upgrade is required.

- Ensure that you have a complete, working backup of your data and my.cnf
  files
- Shut down the MySQL server cleanly
- Remove the existing MySQL packages.  Usually this command will
  list the packages you should remove:
  rpm -qa | grep -i '^mysql-'

  You may choose to use 'rpm --nodeps -ev <package-name>' to remove
  the package which contains the mysqlclient shared library.  The
  library will be reinstalled by the MySQL-shared-compat package.
- Install the new MySQL packages supplied by $myvendor
- Ensure that the MySQL server is started
- Run the 'mysql_upgrade' program

This is a brief description of the upgrade process.  Important details
can be found in the MySQL manual, in the Upgrading section.
******************************************************************
HERE
    exit 1
  fi
fi

/usr/sbin/groupadd -g 27 -o -r mysql >/dev/null 2>&1 || :
/usr/sbin/useradd -M -N -g mysql -o -r -d /var/lib/mysql -s /bin/bash \
	-c "MySQL Server" -u 27 mysql >/dev/null 2>&1 || :

%post -n mysql-libs%{product_suffix}
/sbin/ldconfig

%post -n mysql-server%{product_suffix}
if [ $1 = 1 ]; then
    /sbin/chkconfig --add mysqld
fi
/bin/chmod 0755 /var/lib/mysql
/bin/touch /var/log/mysqld.log

%preun -n mysql-server%{product_suffix}
if [ $1 = 0 ]; then
    /sbin/service mysqld stop >/dev/null 2>&1
    /sbin/chkconfig --del mysqld
fi

%postun -n mysql-libs%{product_suffix}
if [ $1 = 0 ] ; then
    /sbin/ldconfig
fi

%postun -n mysql-server%{product_suffix}
if [ $1 -ge 1 ]; then
    /sbin/service mysqld condrestart >/dev/null 2>&1 || :
fi


%files -n mysql%{product_suffix}
%defattr(-,root,root)
%doc %{license_files_server}

# The below file *only* applies to builds not done by MySQL / Sun / Oracle:
# %doc README.mysql-docs

%{_bindir}/msql2mysql
%{_bindir}/mysql
%{_bindir}/mysql_config
%{_bindir}/mysql_find_rows
%{_bindir}/mysql_waitpid
%{_bindir}/mysqlaccess
%{_bindir}/mysqlaccess.conf
%{_bindir}/mysqladmin
%{_bindir}/mysqlbinlog
%{_bindir}/mysqlcheck
%{_bindir}/mysqldump
%{_bindir}/mysqlimport
%{_bindir}/mysqlshow
%{_bindir}/mysqlslap
%{_bindir}/my_print_defaults

%{_mandir}/man1/mysql.1*
%{_mandir}/man1/mysql_config.1*
%{_mandir}/man1/mysql_find_rows.1*
%{_mandir}/man1/mysql_waitpid.1*
%{_mandir}/man1/mysqlaccess.1*
%{_mandir}/man1/mysqladmin.1*
%{_mandir}/man1/mysqldump.1*
%{_mandir}/man1/mysqlshow.1*
%{_mandir}/man1/mysqlslap.1*
%{_mandir}/man1/my_print_defaults.1*

%{_libdir}/mysql/mysqlbug
%{_libdir}/mysql/mysql_config

%files -n mysql-libs%{product_suffix}
%defattr(-,root,root)
%doc %{license_files_server}
# although the default my.cnf contains only server settings, we put it in the
# libs package because it can be used for client settings too.
%config(noreplace) /etc/my.cnf
%dir %{_libdir}/mysql
%{_libdir}/mysql/libmysqlclient*.so.*
/etc/ld.so.conf.d/*

%dir %{_datadir}/mysql
%{_datadir}/mysql/english
%lang(cs) %{_datadir}/mysql/czech
%lang(da) %{_datadir}/mysql/danish
%lang(nl) %{_datadir}/mysql/dutch
%lang(et) %{_datadir}/mysql/estonian
%lang(fr) %{_datadir}/mysql/french
%lang(de) %{_datadir}/mysql/german
%lang(el) %{_datadir}/mysql/greek
%lang(hu) %{_datadir}/mysql/hungarian
%lang(it) %{_datadir}/mysql/italian
%lang(ja) %{_datadir}/mysql/japanese
%lang(ko) %{_datadir}/mysql/korean
%lang(no) %{_datadir}/mysql/norwegian
%lang(no) %{_datadir}/mysql/norwegian-ny
%lang(pl) %{_datadir}/mysql/polish
%lang(pt) %{_datadir}/mysql/portuguese
%lang(ro) %{_datadir}/mysql/romanian
%lang(ru) %{_datadir}/mysql/russian
%lang(sr) %{_datadir}/mysql/serbian
%lang(sk) %{_datadir}/mysql/slovak
%lang(es) %{_datadir}/mysql/spanish
%lang(sv) %{_datadir}/mysql/swedish
%lang(uk) %{_datadir}/mysql/ukrainian
%{_datadir}/mysql/charsets

%files -n mysql-server%{product_suffix} -f release/support-files/plugins.files
%defattr(-,root,root)
%doc release/support-files/*.cnf
%doc %{_datadir}/info/mysql.info*
%doc %{src_dir}/Docs/ChangeLog
%doc %{src_dir}/Docs/INFO_SRC*
%doc release/Docs/INFO_BIN*

%{_bindir}/myisamchk
%{_bindir}/myisam_ftdump
%{_bindir}/myisamlog
%{_bindir}/myisampack
%{_bindir}/mysql_convert_table_format
%{_bindir}/mysql_fix_extensions
%{_bindir}/mysql_install_db
%{_bindir}/mysql_plugin
%{_bindir}/mysql_secure_installation
%if %{commercial}
%else
%{_bindir}/mysql_setpermission
%endif
%{_bindir}/mysql_tzinfo_to_sql
%{_bindir}/mysql_upgrade
%{_bindir}/mysql_zap
%{_bindir}/mysqlbug
%{_bindir}/mysqldumpslow
%{_bindir}/mysqld_multi
%{_bindir}/mysqld_safe
%{_bindir}/mysqlhotcopy
%{_bindir}/mysqltest
%{_bindir}/innochecksum
%{_bindir}/perror
%{_bindir}/replace
%{_bindir}/resolve_stack_dump
%{_bindir}/resolveip

/usr/libexec/mysqld
/usr/libexec/mysqld-debug
%{_libdir}/mysql/plugin/daemon_example.ini

%if %{WITH_TCMALLOC}
%{_libdir}/mysql/%{malloc_lib_target}
%endif

# obsolete by "-f release/support-files/plugins.files" above
# %{_libdir}/mysql/plugin

%{_mandir}/man1/msql2mysql.1*
%{_mandir}/man1/myisamchk.1*
%{_mandir}/man1/myisamlog.1*
%{_mandir}/man1/myisampack.1*
%{_mandir}/man1/mysql_convert_table_format.1*
%{_mandir}/man1/myisam_ftdump.1*
%{_mandir}/man1/mysql.server.1*
%{_mandir}/man1/mysql_fix_extensions.1*
%{_mandir}/man1/mysql_install_db.1*
%{_mandir}/man1/mysql_plugin.1*
%{_mandir}/man1/mysql_secure_installation.1*
%{_mandir}/man1/mysql_upgrade.1*
%{_mandir}/man1/mysql_zap.1*
%{_mandir}/man1/mysqlbug.1*
%{_mandir}/man1/mysqldumpslow.1*
%{_mandir}/man1/mysqlbinlog.1*
%{_mandir}/man1/mysqlcheck.1*
%{_mandir}/man1/mysqld_multi.1*
%{_mandir}/man1/mysqld_safe.1*
%{_mandir}/man1/mysqlhotcopy.1*
%{_mandir}/man1/mysqlimport.1*
%{_mandir}/man1/mysqlman.1*
%if %{commercial}
%else
%{_mandir}/man1/mysql_setpermission.1*
%endif
%{_mandir}/man1/mysqltest.1*
%{_mandir}/man1/innochecksum.1*
%{_mandir}/man1/perror.1*
%{_mandir}/man1/replace.1*
%{_mandir}/man1/resolve_stack_dump.1*
%{_mandir}/man1/resolveip.1*
%{_mandir}/man1/mysql_tzinfo_to_sql.1*
%{_mandir}/man8/mysqld.8*

%{_datadir}/mysql/errmsg-utf8.txt
%{_datadir}/mysql/fill_help_tables.sql
%{_datadir}/mysql/magic
%{_datadir}/mysql/mysql_system_tables.sql
%{_datadir}/mysql/mysql_system_tables_data.sql
%{_datadir}/mysql/mysql_test_data_timezone.sql
%{_datadir}/mysql/my-*.cnf
%{_datadir}/mysql/config.*.ini

/etc/rc.d/init.d/mysqld
%attr(0755,mysql,mysql) %dir /var/run/mysqld
%attr(0755,mysql,mysql) %dir /var/lib/mysql
%attr(0640,mysql,mysql) %config(noreplace) %verify(not md5 size mtime) /var/log/mysqld.log

# TODO / FIXME: Do we need "libmygcc.a"? If yes, append "-f optional-files-devel"
#               and fix the "rm -f" list in the "install" section.
%files -n mysql-devel%{product_suffix}
%defattr(-,root,root)
/usr/include/mysql
/usr/share/aclocal/mysql.m4
%{_libdir}/mysql/libmysqlclient*.so

%files -n mysql-embedded%{product_suffix}
%defattr(-,root,root)
%doc %{license_files_server}
%{_libdir}/mysql/libmysqld.so.*
%{_bindir}/mysql_embedded

%files -n mysql-embedded-devel%{product_suffix}
%defattr(-,root,root)
%{_libdir}/mysql/libmysqld.so
%{_bindir}/mysql_client_test_embedded
%{_bindir}/mysqltest_embedded
%{_mandir}/man1/mysql_client_test_embedded.1*
%{_mandir}/man1/mysqltest_embedded.1*

%files -n mysql-test%{product_suffix}
%defattr(-,root,root)
%{_bindir}/mysql_client_test
%attr(-,mysql,mysql) %{_datadir}/mysql-test

%{_mandir}/man1/mysql_client_test.1*

%changelog
* Fri Nov  9 2012 Joerg Bruehe <joerg.bruehe@oracle.com>
- The "stack-guard.patch" needs to be adapted for MySQL 5.6,
  reflect that in a name change "5.5" -> "5.6".

* Tue Sep 18 2012 Joerg Bruehe <joerg.bruehe@oracle.com>
- Restrict the vendor check to Oracle: There is no history here
  which we have to allow for.

* Thu Jul 26 2012 Joerg Bruehe <joerg.bruehe@oracle.com>
- Add the vendor and release series checks from the traditional MySQL RPM
  spec file, to protect against errors happening during upgrades.
- Do some code alignment with the traditional MySQL RPM spec file,
  to make synchronous maintenance (and possibly even integration?) easier.

* Mon Feb 13 2012 Joerg Bruehe <joerg.bruehe@oracle.com>
- Add "Provides:" lines for the generic names of the subpackages,
  independent of "product_suffix".

* Tue Feb  7 2012 Joerg Bruehe <joerg.bruehe@oracle.com>
- Make "mysql_setpermission" and its man page appear in GPL builds only.

* Thu Nov 24 2011 Joerg Bruehe <joerg.bruehe@oracle.com>
- Add two patches (#18 + #19) regarding the test suite;
  version 1 of "mysql-test-run.pl" had to go because the auto-detection
  of Perl dependencies does not handle differences between run directory
  and delivery location.

* Thu Nov  3 2011 Joerg Bruehe <joerg.bruehe@oracle.com>
- Adapt from MySQL 5.1 to 5.5, tested using 5.5.17:
  - Done by the MySQL Build Team at Oracle:
    set as packager, set copyright owner and related info;
  - handle command line options, allowing different configurations, platforms, ...
    - configurations will show up in the file name as "product_suffix",
    - use "-n" for all subpackage specifications,
    - license may be GPL or commercial, mention that in the description,
      the license output and the included license files will vary,
    - commercial is "nosource",
    - improve "requires" listings for different platforms,
    - explicitly use "product_suffix" in the "requires" entries;
  - adapt to 5.5 changes in features and function:
    - remove "mysql-bench" package (files are outdated, not maintained),
    - no InnoDB plugin,
    - the set of plugins will vary by configuration, to control the "server"
      package contents use "-f release/support-files/plugins.files" in the
      "files" section,
    - remove "mysqlmanager", "mysql_fix_privilege_tables",
    - add "mysql_embedded", "mysql-plugin", "mysqlaccess.conf", "magic",
    - "errmsg.txt" is now in UTF8: "errmsg-utf8.txt",
    - adapt patches to changed code where needed, rename these to include "5.5",
    - stop using patches which are not applicable to 5.5;
  - 5.5 uses a different way of building:
    - autotools are replaced by cmake,
    - both a "release" and a "debug" server are built in separate subtrees
      ("out of source"!), this also affects path names in further handling,
    - the debug server is added to the "server" subpackage,
    - add "mysql-5.5-libdir.patch" to handle file placement at user site.

* Mon Dec 20 2010 Tom Lane <tgl@redhat.com> 5.1.52-1.1
- Update to MySQL 5.1.52, for various fixes described at
  http://dev.mysql.com/doc/refman/5.1/en/news-5-1-52.html
  including numerous small security issues
Resolves: #652553
- Sync with current Fedora package; this includes:
- Duplicate COPYING and EXCEPTIONS-CLIENT in -libs and -embedded subpackages,
  to ensure they are available when any subset of mysql RPMs are installed,
  per revised packaging guidelines
- Allow init script's STARTTIMEOUT/STOPTIMEOUT to be overridden from sysconfig

* Thu Jul 15 2010 Tom Lane <tgl@redhat.com> 5.1.47-4
- Add backported patch for CVE-2010-2008 (upstream bug 53804)
Resolves: #614215
- Add BuildRequires perl(Time::HiRes) ... seems to no longer be installed
  by just pulling in perl.

* Mon Jun 28 2010 Tom Lane <tgl@redhat.com> 5.1.47-3
- Add -p "$mypidfile" to initscript's status call to improve corner cases.
  (Note: can't be fixed in Fedora until 595597 is fixed there.)
Resolves: #596008

* Mon Jun  7 2010 Tom Lane <tgl@redhat.com> 5.1.47-2
- Add back "partition" storage engine
Resolves: #598585
- Fix broken "federated" storage engine plugin
Resolves: #587170
- Read all certificates in SSL certificate files, to support chained certs
Resolves: #598656

* Mon May 24 2010 Tom Lane <tgl@redhat.com> 5.1.47-1
- Update to MySQL 5.1.47, for various fixes described at
  http://dev.mysql.com/doc/refman/5.1/en/news-5-1-47.html
  http://dev.mysql.com/doc/refman/5.1/en/news-5-1-46.html
  http://dev.mysql.com/doc/refman/5.1/en/news-5-1-45.html
  including fixes for CVE-2010-1621, CVE-2010-1626,
  CVE-2010-1848, CVE-2010-1849, CVE-2010-1850
Resolves: #590598
- Create mysql group explicitly in pre-server script, to ensure correct GID

* Mon Mar  8 2010 Tom Lane <tgl@redhat.com> 5.1.44-2
- Update to MySQL 5.1.44, for various fixes described at
  http://dev.mysql.com/doc/refman/5.1/en/news-5-1-44.html
Resolves: #565554
- Remove mysql.info, which is not freely redistributable
Related: #560181
- Revert broken upstream fix for their bug 45058
Related: #566547
- Bring init script into some modicum of compliance with Fedora/LSB standards
Resolves: #557711
Resolves: #562749

* Mon Feb 15 2010 Tom Lane <tgl@redhat.com> 5.1.43-2
- Update to MySQL 5.1.43, for various fixes described at
  http://dev.mysql.com/doc/refman/5.1/en/news-5-1-43.html
Resolves: #565554
- Remove mysql-cluster, which is no longer supported by upstream in this
  source distribution.  If we want it we'll need a separate SRPM for it.
Resolves: #565210

* Fri Jan 29 2010 Tom Lane <tgl@redhat.com> 5.1.42-7
- Add backported patch for CVE-2008-7247 (upstream bug 39277)
Resolves: #549329
- Use non-expired certificates for SSL testing (upstream bug 50702)

* Tue Jan 26 2010 Tom Lane <tgl@redhat.com> 5.1.42-6
- Emit explicit error message if user tries to build RPM as root
Resolves: #558915

* Wed Jan 20 2010 Tom Lane <tgl@redhat.com> 5.1.42-5
- Correct Source0: tag and comment to reflect how to get the tarball

* Fri Jan  8 2010 Tom Lane <tgl@redhat.com> 5.1.42-4
- Sync with current Fedora build, including:
- Update to MySQL 5.1.42, for various fixes described at
  http://dev.mysql.com/doc/refman/5.1/en/news-5-1-42.html
- Disable symbolic links by default in /etc/my.cnf
Resolves: #553653
- Remove static libraries (.a files) from package, per packaging guidelines
- Change %%define to %%global, per packaging guidelines
- Disable building the innodb plugin; it tickles assorted gcc bugs and
  doesn't seem entirely ready for prime time anyway.
Resolves: #553632
- Start mysqld_safe with --basedir=/usr, to avoid unwanted SELinux messages
  (see 547485)
- Stop waiting during "service mysqld start" if mysqld_safe exits
Resolves: #544095

* Mon Nov 23 2009 Tom Lane <tgl@redhat.com> 5.1.41-1
- Update to MySQL 5.1.41, for various fixes described at
  http://dev.mysql.com/doc/refman/5.1/en/news-5-1-41.html
  including fixes for CVE-2009-4019
Resolves: #549327
- Don't set old_passwords=1; we aren't being bug-compatible with 3.23 anymore
Resolves: #540735

* Tue Nov 10 2009 Tom Lane <tgl@redhat.com> 5.1.40-1
- Update to MySQL 5.1.40, for various fixes described at
  http://dev.mysql.com/doc/refman/5.1/en/news-5-1-40.html
- Do not force the --log-error setting in mysqld init script
Resolves: #533736

* Sat Oct 17 2009 Tom Lane <tgl@redhat.com> 5.1.39-4
- Replace kluge fix for ndbd sparc crash with a real fix (mysql bug 48132)

* Thu Oct 15 2009 Tom Lane <tgl@redhat.com> 5.1.39-3
- Work around two different compiler bugs on sparc, one by backing off
  optimization from -O2 to -O1, and the other with a klugy patch
Related: #529298, #529299
- Clean up bogosity in multilib stub header support: ia64 should not be
  listed (it's not multilib), sparc and sparc64 should be

* Wed Sep 23 2009 Tom Lane <tgl@redhat.com> 5.1.39-2
- Work around upstream bug 46895 by disabling outfile_loaddata test

* Tue Sep 22 2009 Tom Lane <tgl@redhat.com> 5.1.39-1
- Update to MySQL 5.1.39, for various fixes described at
  http://dev.mysql.com/doc/refman/5.1/en/news-5-1-39.html

* Mon Aug 31 2009 Tom Lane <tgl@redhat.com> 5.1.37-5
- Work around unportable assumptions about stpcpy(); re-enable main.mysql test
- Clean up some obsolete parameters to the configure script

* Sat Aug 29 2009 Tom Lane <tgl@redhat.com> 5.1.37-4
- Remove one misguided patch; turns out I was chasing a glibc bug
- Temporarily disable "main.mysql" test; there's something broken there too,
  but we need to get mysql built in rawhide for dependency reasons

* Fri Aug 21 2009 Tomas Mraz <tmraz@redhat.com> - 5.1.37-3
- rebuilt with new openssl

* Fri Aug 14 2009 Tom Lane <tgl@redhat.com> 5.1.37-2
- Add a couple of patches to improve the probability of the regression tests
  completing in koji builds

* Sun Aug  2 2009 Tom Lane <tgl@redhat.com> 5.1.37-1
- Update to MySQL 5.1.37, for various fixes described at
  http://dev.mysql.com/doc/refman/5.1/en/news-5-1-37.html

* Sat Jul 25 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 5.1.36-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_12_Mass_Rebuild

* Fri Jul 10 2009 Tom Lane <tgl@redhat.com> 5.1.36-1
- Update to MySQL 5.1.36, for various fixes described at
  http://dev.mysql.com/doc/refman/5.1/en/news-5-1-36.html

* Sat Jun  6 2009 Tom Lane <tgl@redhat.com> 5.1.35-1
- Update to MySQL 5.1.35, for various fixes described at
  http://dev.mysql.com/doc/refman/5.1/en/news-5-1-35.html
- Ensure that /var/lib/mysql is created with the right SELinux context
Resolves: #502966

* Fri May 15 2009 Tom Lane <tgl@redhat.com> 5.1.34-1
- Update to MySQL 5.1.34, for various fixes described at
  http://dev.mysql.com/doc/refman/5.1/en/news-5-1-34.html
- Increase startup timeout per bug #472222

* Wed Apr 15 2009 Tom Lane <tgl@redhat.com> 5.1.33-2
- Increase stack size of ndbd threads for safety's sake.
Related: #494631

* Tue Apr  7 2009 Tom Lane <tgl@redhat.com> 5.1.33-1
- Update to MySQL 5.1.33.
- Disable use of pthread_setschedparam; doesn't work the way code expects.
Related: #477624

* Wed Mar  4 2009 Tom Lane <tgl@redhat.com> 5.1.32-1
- Update to MySQL 5.1.32.

* Wed Feb 25 2009 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 5.1.31-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_11_Mass_Rebuild

* Fri Feb 13 2009 Tom Lane <tgl@redhat.com> 5.1.31-1
- Update to MySQL 5.1.31.

* Thu Jan 22 2009 Tom Lane <tgl@redhat.com> 5.1.30-2
- hm, apparently --with-innodb and --with-ndbcluster are still needed
  even though no longer documented ...

* Thu Jan 22 2009 Tom Lane <tgl@redhat.com> 5.1.30-1
- Update to MySQL 5.1.30.  Note that this includes an ABI break for
  libmysqlclient (it's now got .so major version 16).
- This also updates mysql for new openssl build

* Wed Oct  1 2008 Tom Lane <tgl@redhat.com> 5.0.67-2
- Build the "embedded server" library, and package it in a new sub-RPM
  mysql-embedded, along with mysql-embedded-devel for devel support files.
Resolves: #149829

* Sat Aug 23 2008 Tom Lane <tgl@redhat.com> 5.0.67-1
- Update to mysql version 5.0.67
- Move mysql_config's man page to base package, again (apparently I synced
  that change the wrong way while importing specfile changes for ndbcluster)

* Sun Jul 27 2008 Tom Lane <tgl@redhat.com> 5.0.51a-2
- Enable ndbcluster support
Resolves: #163758
- Suppress odd crash messages during package build, caused by trying to
  build dbug manual (which we don't install anyway) with dbug disabled
Resolves: #437053
- Improve mysql.init to pass configured datadir to mysql_install_db,
  and to force user=mysql for both mysql_install_db and mysqld_safe.
Related: #450178

* Mon Mar  3 2008 Tom Lane <tgl@redhat.com> 5.0.51a-1
- Update to mysql version 5.0.51a

* Mon Mar  3 2008 Tom Lane <tgl@redhat.com> 5.0.45-11
- Fix mysql-stack-guard patch to work correctly on IA64
- Fix mysql.init to wait correctly when socket is not in default place
Related: #435494

* Mon Mar 03 2008 Dennis Gilmore <dennis@ausil.us> 5.0.45-10
- add sparc64 to 64 bit arches for test suite checking
- add sparc, sparcv9 and sparc64 to multilib handling

* Thu Feb 28 2008 Tom Lane <tgl@redhat.com> 5.0.45-9
- Fix the stack overflow problem encountered in January.  It seems the real
issue is that the buildfarm machines were moved to RHEL5, which uses 64K not
4K pages on PPC, and because RHEL5 takes the guard area out of the requested
thread stack size we no longer had enough headroom.
Related: #435337

* Tue Feb 19 2008 Fedora Release Engineering <rel-eng@fedoraproject.org> - 5.0.45-8
- Autorebuild for GCC 4.3

* Tue Jan  8 2008 Tom Lane <tgl@redhat.com> 5.0.45-7
- Unbelievable ... upstream still thinks that it's a good idea to have a
  regression test that is guaranteed to begin failing come January 1.
- ... and it seems we need to raise STACK_MIN_SIZE again too.

* Thu Dec 13 2007 Tom Lane <tgl@redhat.com> 5.0.45-6
- Back-port upstream fixes for CVE-2007-5925, CVE-2007-5969, CVE-2007-6303.
Related: #422211

* Wed Dec  5 2007 Tom Lane <tgl@redhat.com> 5.0.45-5
- Rebuild for new openssl

* Sat Aug 25 2007 Tom Lane <tgl@redhat.com> 5.0.45-4
- Seems we need explicit BuildRequires on gawk and procps now
- Rebuild to fix Fedora toolchain issues

* Sun Aug 12 2007 Tom Lane <tgl@redhat.com> 5.0.45-3
- Recent perl changes in rawhide mean we need a more specific BuildRequires

* Thu Aug  2 2007 Tom Lane <tgl@redhat.com> 5.0.45-2
- Update License tag to match code.
- Work around recent Fedora change that makes "open" a macro name.

* Sun Jul 22 2007 Tom Lane <tgl@redhat.com> 5.0.45-1
- Update to MySQL 5.0.45
Resolves: #246535
- Move mysql_config's man page to base package
Resolves: #245770
- move my_print_defaults to base RPM, for consistency with Stacks packaging
- mysql user is no longer deleted at RPM uninstall
Resolves: #241912

* Thu Mar 29 2007 Tom Lane <tgl@redhat.com> 5.0.37-2
- Use a less hacky method of getting default values in initscript
Related: #233771, #194596
- Improve packaging of mysql-libs per suggestions from Remi Collet
Resolves: #233731
- Update default /etc/my.cnf ([mysql.server] has been bogus for a long time)

* Mon Mar 12 2007 Tom Lane <tgl@redhat.com> 5.0.37-1
- Update to MySQL 5.0.37
Resolves: #231838
- Put client library into a separate mysql-libs RPM to reduce dependencies
Resolves: #205630

* Fri Feb  9 2007 Tom Lane <tgl@redhat.com> 5.0.33-1
- Update to MySQL 5.0.33
- Install band-aid fix for "view" regression test designed to fail after 2006
- Don't chmod -R the entire database directory tree on every startup
Related: #221085
- Fix unsafe use of install-info
Resolves: #223713
- Cope with new automake in F7
Resolves: #224171

* Thu Nov  9 2006 Tom Lane <tgl@redhat.com> 5.0.27-1
- Update to MySQL 5.0.27 (see CVE-2006-4031, CVE-2006-4226, CVE-2006-4227)
Resolves: #202247, #202675, #203427, #203428, #203432, #203434, #208641
- Fix init script to return status 1 on server start timeout
Resolves: #203910
- Move mysqldumpslow from base package to mysql-server
Resolves: #193559
- Adjust link options for BDB module
Resolves: #199368

* Wed Jul 12 2006 Jesse Keating <jkeating@redhat.com> - 5.0.22-2.1
- rebuild

* Sat Jun 10 2006 Tom Lane <tgl@redhat.com> 5.0.22-2
- Work around brew's tendency not to clean up failed builds completely,
  by adding code in mysql-testing.patch to kill leftover mysql daemons.

* Thu Jun  8 2006 Tom Lane <tgl@redhat.com> 5.0.22-1
- Update to MySQL 5.0.22 (fixes CVE-2006-2753)
- Install temporary workaround for gcc bug on s390x (bz #193912)

* Tue May  2 2006 Tom Lane <tgl@redhat.com> 5.0.21-2
- Fix bogus perl Requires for mysql-test

* Mon May  1 2006 Tom Lane <tgl@redhat.com> 5.0.21-1
- Update to MySQL 5.0.21

* Mon Mar 27 2006 Tom Lane <tgl@redhat.com> 5.0.18-4
- Modify multilib header hack to not break non-RH arches, per bug #181335
- Remove logrotate script, per bug #180639.
- Add a new mysql-test RPM to carry the regression test files;
  hack up test scripts as needed to make them run in /usr/share/mysql-test.

* Fri Feb 10 2006 Jesse Keating <jkeating@redhat.com> - 5.0.18-2.1
- bump again for double-long bug on ppc(64)

* Thu Feb  9 2006 Tom Lane <tgl@redhat.com> 5.0.18-2
- err-log option has been renamed to log-error, fix my.cnf and initscript

* Tue Feb 07 2006 Jesse Keating <jkeating@redhat.com> - 5.0.18-1.1
- rebuilt for new gcc4.1 snapshot and glibc changes

* Thu Jan  5 2006 Tom Lane <tgl@redhat.com> 5.0.18-1
- Update to MySQL 5.0.18

* Thu Dec 15 2005 Tom Lane <tgl@redhat.com> 5.0.16-4
- fix my_config.h for ppc platforms

* Thu Dec 15 2005 Tom Lane <tgl@redhat.com> 5.0.16-3
- my_config.h needs to guard against 64-bit platforms that also define the
  32-bit symbol

* Wed Dec 14 2005 Tom Lane <tgl@redhat.com> 5.0.16-2
- oops, looks like we want uname -i not uname -m

* Mon Dec 12 2005 Tom Lane <tgl@redhat.com> 5.0.16-1
- Update to MySQL 5.0.16
- Add EXCEPTIONS-CLIENT license info to the shipped documentation
- Make my_config.h architecture-independent for multilib installs;
  put the original my_config.h into my_config_$ARCH.h
- Add -fwrapv to CFLAGS so that gcc 4.1 doesn't break it

* Fri Dec 09 2005 Jesse Keating <jkeating@redhat.com>
- rebuilt

* Mon Nov 14 2005 Tom Lane <tgl@redhat.com> 5.0.15-3
- Make stop script wait for daemon process to disappear (bz#172426)

* Wed Nov  9 2005 Tom Lane <tgl@redhat.com> 5.0.15-2
- Rebuild due to openssl library update.

* Thu Nov  3 2005 Tom Lane <tgl@redhat.com> 5.0.15-1
- Update to MySQL 5.0.15 (scratch build for now)

* Wed Oct  5 2005 Tom Lane <tgl@redhat.com> 4.1.14-1
- Update to MySQL 4.1.14

* Tue Aug 23 2005 Tom Lane <tgl@redhat.com> 4.1.12-3
- Use politically correct patch name.

* Tue Jul 12 2005 Tom Lane <tgl@redhat.com> 4.1.12-2
- Fix buffer overflow newly exposed in isam code; it's the same issue
  previously found in myisam, and not very exciting, but I'm tired of
  seeing build warnings.

* Mon Jul 11 2005 Tom Lane <tgl@redhat.com> 4.1.12-1
- Update to MySQL 4.1.12 (includes a fix for bz#158688, bz#158689)
- Extend mysql-test-ssl.patch to solve rpl_openssl test failure (bz#155850)
- Update mysql-lock-ssl.patch to match the upstream committed version
- Add --with-isam to re-enable the old ISAM table type, per bz#159262
- Add dependency on openssl-devel per bz#159569
- Remove manual.txt, as upstream decided not to ship it anymore;
  it was redundant with the mysql.info file anyway.

* Mon May  9 2005 Tom Lane <tgl@redhat.com> 4.1.11-4
- Include proper locking for OpenSSL in the server, per bz#155850

* Mon Apr 25 2005 Tom Lane <tgl@redhat.com> 4.1.11-3
- Enable openssl tests during build, per bz#155850
- Might as well turn on --disable-dependency-tracking

* Fri Apr  8 2005 Tom Lane <tgl@redhat.com> 4.1.11-2
- Avoid dependency on <asm/atomic.h>, cause it won't build anymore on ia64.
  This is probably a cleaner solution for bz#143537, too.

* Thu Apr  7 2005 Tom Lane <tgl@redhat.com> 4.1.11-1
- Update to MySQL 4.1.11 to fix bz#152911 as well as other issues
- Move perl-DBI, perl-DBD-MySQL dependencies to server package (bz#154123)
- Override configure thread library test to suppress HAVE_LINUXTHREADS check
- Fix BDB failure on s390x (bz#143537)
- At last we can enable "make test" on all arches

* Fri Mar 11 2005 Tom Lane <tgl@redhat.com> 4.1.10a-1
- Update to MySQL 4.1.10a to fix security vulnerabilities (bz#150868,
  for CAN-2005-0711, and bz#150871 for CAN-2005-0709, CAN-2005-0710).

* Sun Mar  6 2005 Tom Lane <tgl@redhat.com> 4.1.10-3
- Fix package Requires: interdependencies.

* Sat Mar  5 2005 Tom Lane <tgl@redhat.com> 4.1.10-2
- Need -fno-strict-aliasing in at least one place, probably more.
- Work around some C spec violations in mysql.

* Fri Feb 18 2005 Tom Lane <tgl@redhat.com> 4.1.10-1
- Update to MySQL 4.1.10.

* Sat Jan 15 2005 Tom Lane <tgl@redhat.com> 4.1.9-1
- Update to MySQL 4.1.9.

* Wed Jan 12 2005 Tom Lane <tgl@redhat.com> 4.1.7-10
- Don't assume /etc/my.cnf will specify pid-file (bz#143724)

* Wed Jan 12 2005 Tim Waugh <twaugh@redhat.com> 4.1.7-9
- Rebuilt for new readline.

* Tue Dec 21 2004 Tom Lane <tgl@redhat.com> 4.1.7-8
- Run make test on all archs except s390x (which seems to have a bdb issue)

* Mon Dec 13 2004 Tom Lane <tgl@redhat.com> 4.1.7-7
- Suppress someone's silly idea that libtool overhead can be skipped

* Sun Dec 12 2004 Tom Lane <tgl@redhat.com> 4.1.7-6
- Fix init script to not need a valid username for startup check (bz#142328)
- Fix init script to honor settings appearing in /etc/my.cnf (bz#76051)
- Enable SSL (bz#142032)

* Thu Dec  2 2004 Tom Lane <tgl@redhat.com> 4.1.7-5
- Add a restorecon to keep the mysql.log file in the right context (bz#143887)

* Tue Nov 23 2004 Tom Lane <tgl@redhat.com> 4.1.7-4
- Turn off old_passwords in default /etc/my.cnf file, for better compatibility
  with mysql 3.x clients (per suggestion from Joe Orton).

* Fri Oct 29 2004 Tom Lane <tgl@redhat.com> 4.1.7-3
- Handle ldconfig more cleanly (put a file in /etc/ld.so.conf.d/).

* Thu Oct 28 2004 Tom Lane <tgl@redhat.com> 4.1.7-2
- rebuild in devel branch

* Wed Oct 27 2004 Tom Lane <tgl@redhat.com> 4.1.7-1
- Update to MySQL 4.1.x.

* Tue Oct 12 2004 Tom Lane <tgl@redhat.com> 3.23.58-13
- fix security issues CAN-2004-0835, CAN-2004-0836, CAN-2004-0837
  (bugs #135372, 135375, 135387)
- fix privilege escalation on GRANT ALL ON `Foo\_Bar` (CAN-2004-0957)

* Wed Oct 06 2004 Tom Lane <tgl@redhat.com> 3.23.58-12
- fix multilib problem with mysqlbug and mysql_config
- adjust chkconfig priority per bug #128852
- remove bogus quoting per bug #129409 (MySQL 4.0 has done likewise)
- add sleep to mysql.init restart(); may or may not fix bug #133993

* Tue Oct 05 2004 Tom Lane <tgl@redhat.com> 3.23.58-11
- fix low-priority security issues CAN-2004-0388, CAN-2004-0381, CAN-2004-0457
  (bugs #119442, 125991, 130347, 130348)
- fix bug with dropping databases under recent kernels (bug #124352)

* Tue Jun 15 2004 Elliot Lee <sopwith@redhat.com> 3.23.58-10
- rebuilt

* Sat Apr 17 2004 Warren Togami <wtogami@redhat.com> 3.23.58-9
- remove redundant INSTALL-SOURCE, manual.*
- compress manual.txt.bz2
- BR time

* Tue Mar 16 2004 Tom Lane <tgl@redhat.com> 3.23.58-8
- repair logfile attributes in %%files, per bug #102190
- repair quoting problem in mysqlhotcopy, per bug #112693
- repair missing flush in mysql_setpermission, per bug #113960
- repair broken error message printf, per bug #115165
- delete mysql user during uninstall, per bug #117017
- rebuilt

* Tue Mar 02 2004 Elliot Lee <sopwith@redhat.com>
- rebuilt

* Tue Feb 24 2004 Tom Lane <tgl@redhat.com>
- fix chown syntax in mysql.init
- rebuild

* Fri Feb 13 2004 Elliot Lee <sopwith@redhat.com>
- rebuilt

* Tue Nov 18 2003 Kim Ho <kho@redhat.com> 3.23.58-5
- update mysql.init to use anonymous user (UNKNOWN_MYSQL_USER) for
  pinging mysql server (#108779)

* Mon Oct 27 2003 Kim Ho <kho@redhat.com> 3.23.58-4
- update mysql.init to wait (max 10 seconds) for mysql server to 
  start (#58732)

* Mon Oct 27 2003 Patrick Macdonald <patrickm@redhat.com> 3.23.58-3
- re-enable Berkeley DB support (#106832)
- re-enable ia64 testing

* Fri Sep 19 2003 Patrick Macdonald <patrickm@redhat.com> 3.23.58-2
- rebuilt

* Mon Sep 15 2003 Patrick Macdonald <patrickm@redhat.com> 3.23.58-1
- upgrade to 3.23.58 for security fix

* Tue Aug 26 2003 Patrick Macdonald <patrickm@redhat.com> 3.23.57-2
- rebuilt

* Wed Jul 02 2003 Patrick Macdonald <patrickm@redhat.com> 3.23.57-1
- revert to prior version of MySQL due to license incompatibilities 
  with packages that link against the client.  The MySQL folks are
  looking into the issue.

* Wed Jun 18 2003 Patrick Macdonald <patrickm@redhat.com> 4.0.13-4
- restrict test on ia64 (temporary)

* Wed Jun 04 2003 Elliot Lee <sopwith@redhat.com> 4.0.13-3
- rebuilt

* Thu May 29 2003 Patrick Macdonald <patrickm@redhat.com> 4.0.13-2
- fix filter-requires-mysql.sh with less restrictive for mysql-bench 

* Wed May 28 2003 Patrick Macdonald <patrickm@redhat.com> 4.0.13-1
- update for MySQL 4.0
- back-level shared libraries available in mysqlclient10 package

* Fri May 09 2003 Patrick Macdonald <patrickm@redhat.com> 3.23.56-2
- add sql-bench package (#90110) 

* Wed Mar 19 2003 Patrick Macdonald <patrickm@redhat.com> 3.23.56-1
- upgrade to 3.23.56 for security fixes
- remove patch for double-free (included in 3.23.56) 

* Tue Feb 18 2003 Patrick Macdonald <patrickm@redhat.com> 3.23.54a-11
- enable thread safe client
- add patch for double free fix

* Wed Jan 22 2003 Tim Powers <timp@redhat.com>
- rebuilt

* Mon Jan 13 2003 Karsten Hopp <karsten@redhat.de> 3.23.54a-9
- disable checks on s390x

* Sat Jan  4 2003 Jeff Johnson <jbj@redhat.com> 3.23.54a-8
- use internal dep generator.

* Wed Jan  1 2003 Bill Nottingham <notting@redhat.com> 3.23.54a-7
- fix mysql_config on hammer

* Sun Dec 22 2002 Tim Powers <timp@redhat.com> 3.23.54a-6
- don't use rpms internal dep generator

* Tue Dec 17 2002 Elliot Lee <sopwith@redhat.com> 3.23.54a-5
- Push it into the build system

* Mon Dec 16 2002 Joe Orton <jorton@redhat.com> 3.23.54a-4
- upgrade to 3.23.54a for safe_mysqld fix

* Thu Dec 12 2002 Joe Orton <jorton@redhat.com> 3.23.54-3
- upgrade to 3.23.54 for latest security fixes

* Tue Nov 19 2002 Jakub Jelinek <jakub@redhat.com> 3.23.52-5
- Always include <errno.h> for errno
- Remove unpackaged files

* Tue Nov 12 2002 Florian La Roche <Florian.LaRoche@redhat.de>
- do not prereq userdel, not used at all

* Mon Sep  9 2002 Trond Eivind Glomsrd <teg@redhat.com> 3.23.52-4
- Use %%{_libdir}
- Add patch for x86-64

* Wed Sep  4 2002 Jakub Jelinek <jakub@redhat.com> 3.23.52-3
- rebuilt with gcc-3.2-7

* Thu Aug 29 2002 Trond Eivind Glomsrd <teg@redhat.com> 3.23.52-2
- Add --enable-local-infile to configure - a new option
  which doesn't default to the old behaviour (#72885)

* Fri Aug 23 2002 Trond Eivind Glomsrd <teg@redhat.com> 3.23.52-1
- 3.23.52. Fixes a minor security problem, various bugfixes.

* Sat Aug 10 2002 Elliot Lee <sopwith@redhat.com> 3.23.51-5
- rebuilt with gcc-3.2 (we hope)

* Mon Jul 22 2002 Trond Eivind Glomsrd <teg@redhat.com> 3.23.51-4
- rebuild

* Thu Jul 18 2002 Trond Eivind Glomsrd <teg@redhat.com> 3.23.51-3
- Fix #63543 and #63542 

* Thu Jul 11 2002 Trond Eivind Glomsrd <teg@redhat.com> 3.23.51-2
- Turn off bdb on PPC(#68591)
- Turn off the assembly optimizations, for safety. 

* Wed Jun 26 2002 Trond Eivind Glomsrd <teg@redhat.com> 3.23.51-1
- Work around annoying auto* thinking this is a crosscompile
- 3.23.51

* Fri Jun 21 2002 Tim Powers <timp@redhat.com>
- automated rebuild

* Mon Jun 10 2002 Trond Eivind Glomsrd <teg@redhat.com> 3.23.50-2
- Add dependency on perl-DBI and perl-DBD-MySQL (#66349)

* Thu May 30 2002 Trond Eivind Glomsrd <teg@redhat.com> 3.23.50-1
- 3.23.50

* Thu May 23 2002 Tim Powers <timp@redhat.com>
- automated rebuild

* Mon May 13 2002 Trond Eivind Glomsrd <teg@redhat.com> 3.23.49-4
- Rebuild
- Don't set CXX to gcc, it doesn't work anymore
- Exclude Alpha

* Mon Apr  8 2002 Trond Eivind Glomsrd <teg@redhat.com> 3.23.49-3
- Add the various .cnf examples as doc files to mysql-server (#60349)
- Don't include manual.ps, it's just 200 bytes with a URL inside (#60349)
- Don't include random files in /usr/share/mysql (#60349)
- langify (#60349)

* Thu Feb 21 2002 Trond Eivind Glomsrd <teg@redhat.com> 3.23.49-2
- Rebuild

* Sun Feb 17 2002 Trond Eivind Glomsrd <teg@redhat.com> 3.23.49-1
- 3.23.49

* Thu Feb 14 2002 Trond Eivind Glomsrd <teg@redhat.com> 3.23.48-2
- work around perl dependency bug.

* Mon Feb 11 2002 Trond Eivind Glomsrd <teg@redhat.com> 3.23.48-1
- 3.23.48

* Thu Jan 17 2002 Trond Eivind Glomsrd <teg@redhat.com> 3.23.47-4
- Use kill, not mysqladmin, to flush logs and shut down. Thus, 
  an admin password can be set with no problems.
- Remove reload from init script

* Wed Jan 16 2002 Trond Eivind Glomsrd <teg@redhat.com> 3.23.47-3
- remove db3-devel from buildrequires, 
  MySQL has had its own bundled copy since the mid thirties

* Sun Jan  6 2002 Trond Eivind Glomsrd <teg@redhat.com> 3.23.47-1
- 3.23.47
- Don't build for alpha, toolchain immature.

* Mon Dec  3 2001 Trond Eivind Glomsrd <teg@redhat.com> 3.23.46-1
- 3.23.46
- use -fno-rtti and -fno-exceptions, and set CXX to increase stability. 
  Recommended by mysql developers.

* Sun Nov 25 2001 Trond Eivind Glomsrd <teg@redhat.com> 3.23.45-1
- 3.23.45

* Wed Nov 14 2001 Trond Eivind Glomsrd <teg@redhat.com> 3.23.44-2
- centralize definition of datadir in the initscript (#55873)

* Fri Nov  2 2001 Trond Eivind Glomsrd <teg@redhat.com> 3.23.44-1
- 3.23.44

* Thu Oct  4 2001 Trond Eivind Glomsrd <teg@redhat.com> 3.23.43-1
- 3.23.43

* Mon Sep 10 2001 Trond Eivind Glomsrd <teg@redhat.com> 3.23.42-1
- 3.23.42
- reenable innodb

* Tue Aug 14 2001 Trond Eivind Glomsrd <teg@redhat.com> 3.23.41-1
- 3.23.41 bugfix release
- disable innodb, to avoid the broken updates
- Use "mysqladmin flush_logs" instead of kill -HUP in logrotate 
  script (#51711)

* Sat Jul 21 2001 Trond Eivind Glomsrd <teg@redhat.com>
- 3.23.40, bugfix release
- Add zlib-devel to buildrequires:

* Fri Jul 20 2001 Trond Eivind Glomsrd <teg@redhat.com>
- BuildRequires-tweaking

* Thu Jun 28 2001 Trond Eivind Glomsrd <teg@redhat.com>
- Reenable test, but don't run them for s390, s390x or ia64
- Make /etc/my.cnf config(noplace). Same for /etc/logrotate.d/mysqld

* Thu Jun 14 2001 Trond Eivind Glomsrd <teg@redhat.com>
- 3.23.29
- enable innodb
- enable assembly again
- disable tests for now...

* Tue May 15 2001 Trond Eivind Glomsrd <teg@redhat.com>
- 3.23.38
- Don't use BDB on Alpha - no fast mutexes

* Tue Apr 24 2001 Trond Eivind Glomsrd <teg@redhat.com>
- 3.23.37
- Add _GNU_SOURCE to the compile flags

* Wed Mar 28 2001 Trond Eivind Glomsrd <teg@redhat.com>
- Make it obsolete our 6.2 PowerTools packages
- 3.23.36 bugfix release - fixes some security issues
  which didn't apply to our standard configuration
- Make "make test" part of the build process, except on IA64
  (it fails there)

* Tue Mar 20 2001 Trond Eivind Glomsrd <teg@redhat.com>
- 3.23.35 bugfix release
- Don't delete the mysql user on uninstall

* Tue Mar 13 2001 Trond Eivind Glomsrd <teg@redhat.com>
- 3.23.34a bugfix release

* Wed Feb  7 2001 Trond Eivind Glomsrd <teg@redhat.com>
- added readline-devel to BuildRequires:

* Tue Feb  6 2001 Trond Eivind Glomsrd <teg@redhat.com>
- small i18n-fixes to initscript (action needs $)

* Tue Jan 30 2001 Trond Eivind Glomsrd <teg@redhat.com>
- make it shut down and rotate logs without using mysqladmin 
  (from #24909)

* Mon Jan 29 2001 Trond Eivind Glomsrd <teg@redhat.com>
- conflict with "MySQL"

* Tue Jan 23 2001 Trond Eivind Glomsrd <teg@redhat.com>
- improve gettextizing

* Mon Jan 22 2001 Trond Eivind Glomsrd <teg@redhat.com>
- 3.23.32
- fix logrotate script (#24589)

* Wed Jan 17 2001 Trond Eivind Glomsrd <teg@redhat.com>
- gettextize
- move the items in Requires(post): to Requires: in preparation
  for an errata for 7.0 when 3.23.31 is released
- 3.23.31

* Tue Jan 16 2001 Trond Eivind Glomsrd <teg@redhat.com>
- add the log file to the rpm database, and make it 0640
  (#24116)
- as above in logrotate script
- changes to the init sequence - put most of the data
  in /etc/my.cnf instead of hardcoding in the init script
- use /var/run/mysqld/mysqld.pid instead of 
  /var/run/mysqld/pid
- use standard safe_mysqld
- shut down cleaner

* Mon Jan 08 2001 Trond Eivind Glomsrd <teg@redhat.com>
- 3.23.30
- do an explicit chmod on /var/lib/mysql in post, to avoid 
  any problems with broken permissons. There is a report
  of rm not changing this on its own (#22989)

* Mon Jan 01 2001 Trond Eivind Glomsrd <teg@redhat.com>
- bzipped source
- changed from 85 to 78 in startup, so it starts before
  apache (which can use modules requiring mysql)

* Wed Dec 27 2000 Trond Eivind Glomsrd <teg@redhat.com>
- 3.23.29a

* Tue Dec 19 2000 Trond Eivind Glomsrd <teg@redhat.com>
- add requirement for new libstdc++, build for errata

* Mon Dec 18 2000 Trond Eivind Glomsrd <teg@redhat.com>
- 3.23.29

* Mon Nov 27 2000 Trond Eivind Glomsrd <teg@redhat.com>
- 3.23.28 (gamma)
- remove old patches, as they are now upstreamed

* Thu Nov 14 2000 Trond Eivind Glomsrd <teg@redhat.com>
- Add a requirement for a new glibc (#20735)
- build on IA64

* Wed Nov  1 2000 Trond Eivind Glomsrd <teg@redhat.com>
- disable more assembly

* Wed Nov  1 2000 Jakub Jelinek <jakub@redhat.com>
- fix mysql on SPARC (#20124)

* Tue Oct 31 2000 Trond Eivind Glomsrd <teg@redhat.com>
- 3.23.27

* Wed Oct 25 2000 Trond Eivind Glomsrd <teg@redhat.com>
- add patch for fixing bogus aliasing in mysql from Jakub,
  which should fix #18905 and #18620

* Mon Oct 23 2000 Trond Eivind Glomsrd <teg@redhat.com>
- check for negative niceness values, and negate it
  if present (#17899)
- redefine optflags on IA32 FTTB

* Wed Oct 18 2000 Trond Eivind Glomsrd <teg@redhat.com>
- 3.23.26, which among other fixes now uses mkstemp()
  instead of tempnam().
- revert changes made yesterday, the problem is now
  isolated
 
* Tue Oct 17 2000 Trond Eivind Glomsrd <teg@redhat.com>
- use the compat C++ compiler FTTB. Argh.
- add requirement of ncurses4 (see above)

* Sun Oct 01 2000 Trond Eivind Glomsrd <teg@redhat.com>
- 3.23.25
- fix shutdown problem (#17956)

* Tue Sep 26 2000 Trond Eivind Glomsrd <teg@redhat.com>
- Don't try to include no-longer-existing PUBLIC file
  as doc (#17532)

* Thu Sep 12 2000 Trond Eivind Glomsrd <teg@redhat.com>
- rename config file to /etc/my.cnf, which is what
  mysqld wants... doh. (#17432)
- include a changed safe_mysqld, so the pid file option
  works. 
- make mysql dir world readable to they can access the 
  mysql socket. (#17432)
- 3.23.24

* Wed Sep 06 2000 Trond Eivind Glomsrd <teg@redhat.com>
- 3.23.23

* Sun Aug 27 2000 Trond Eivind Glomsrd <teg@redhat.com>
- Add "|| :" to condrestart to avoid non-zero exit code

* Thu Aug 24 2000 Trond Eivind Glomsrd <teg@redhat.com>
- it's mysql.com, not mysql.org and use correct path to 
  source (#16830)

* Wed Aug 16 2000 Trond Eivind Glomsrd <teg@redhat.com>
- source file from /etc/rc.d, not /etc/rd.d. Doh.

* Sun Aug 13 2000 Trond Eivind Glomsrd <teg@redhat.com>
- don't run ldconfig -n, it doesn't update ld.so.cache
  (#16034)
- include some missing binaries
- use safe_mysqld to start the server (request from
  mysql developers)

* Sat Aug 05 2000 Bill Nottingham <notting@redhat.com>
- condrestart fixes

* Mon Aug 01 2000 Trond Eivind Glomsrd <teg@redhat.com>
- 3.23.22. Disable the old patches, they're now in.

* Thu Jul 27 2000 Trond Eivind Glomsrd <teg@redhat.com>
- bugfixes in the initscript
- move the .so link to the devel package

* Wed Jul 19 2000 Trond Eivind Glomsrd <teg@redhat.com>
- rebuild due to glibc changes

* Tue Jul 18 2000 Trond Eivind Glomsrd <teg@redhat.com>
- disable compiler patch
- don't include info directory file

* Mon Jul 17 2000 Trond Eivind Glomsrd <teg@redhat.com>
- move back to /etc/rc.d/init.d

* Fri Jul 14 2000 Trond Eivind Glomsrd <teg@redhat.com>
- more cleanups in initscript

* Thu Jul 13 2000 Trond Eivind Glomsrd <teg@redhat.com>
- add a patch to work around compiler bug 
  (from monty@mysql.com) 

* Wed Jul 12 2000 Trond Eivind Glomsrd <teg@redhat.com>
- don't build the SQL daemon statically (glibc problems)
- fix the logrotate script - only flush log if mysql
  is running
- change the reloading procedure 
- remove icon - glint is obsolete a long time ago

* Wed Jul 12 2000 Prospector <bugzilla@redhat.com>
- automatic rebuild

* Mon Jul 10 2000 Trond Eivind Glomsrd <teg@redhat.com>
- try the new compiler again
- build the SQL daemon statically
- add compile time support for complex charsets
- enable assembler
- more cleanups in initscript

* Sun Jul 09 2000 Trond Eivind Glomsrd <teg@redhat.com>
- use old C++ compiler
- Exclusivearch x86

* Sat Jul 08 2000 Trond Eivind Glomsrd <teg@redhat.com>
- move .so files to devel package
- more cleanups
- exclude sparc for now

* Wed Jul 05 2000 Trond Eivind Glomsrd <teg@redhat.com>
- 3.23.21
- remove file from /etc/sysconfig
- Fix initscript a bit - initialization of databases doesn't
  work yet
- specify the correct licenses
- include a /etc/my.conf (empty, FTTB)
- add conditional restart to spec file

* Tue Jul  2 2000 Jakub Jelinek <jakub@redhat.com>
- Rebuild with new C++

* Fri Jun 30 2000 Trond Eivind Glomsrd <teg@redhat.com>
- update to 3.23.20
- use %%configure, %%makeinstall, %%{_tmppath}, %%{_mandir},
  %%{_infodir}, /etc/init.d
- remove the bench package
- change some of the descriptions a little bit
- fix the init script
- some compile fixes
- specify mysql user
- use mysql uid 27 (postgresql is 26)
- don't build on ia64

* Sat Feb 26 2000 Jos Vos <jos@xos.nl>
- Version 3.22.32 release XOS.1 for LinuX/OS 1.8.0
- Upgrade from version 3.22.27 to 3.22.32.
- Do "make install" instead of "make install-strip", because "install -s"
  now appears to fail on various scripts.  Afterwards, strip manually.
- Reorganize subpackages, according to common Red Hat packages: the client
  program and shared library become the base package and the server and
  some accompanying files are now in a separate server package.  The
  server package implicitly requires the base package (shared library),
  but we have added a manual require tag anyway (because of the shared
  config file, and more).
- Rename the mysql-benchmark subpackage to mysql-bench.

* Mon Jan 31 2000 Jos Vos <jos@xos.nl>
- Version 3.22.27 release XOS.2 for LinuX/OS 1.7.1
- Add post(un)install scripts for updating ld.so.conf (client subpackage).

* Sun Nov 21 1999 Jos Vos <jos@xos.nl>
- Version 3.22.27 release XOS.1 for LinuX/OS 1.7.0
- Initial version.
- Some ideas borrowed from Red Hat Powertools 6.1, although this spec
  file is a full rewrite from scratch.
