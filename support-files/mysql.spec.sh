%define mysql_version		@VERSION@
%define shared_lib_version	@SHARED_LIB_VERSION@
%define release			0
%define mysqld_user		mysql

%define see_base For a description of MySQL see the base MySQL RPM or http://www.mysql.com

Name: MySQL
Summary:	MySQL: a very fast and reliable SQL database server
Group:		Applications/Databases
Summary(pt_BR): MySQL: Um servidor SQL rápido e confiável.
Group(pt_BR):	Aplicações/Banco_de_Dados
Version:	@MYSQL_NO_DASH_VERSION@
Release:	%{release}
Copyright:	GPL
Source:		http://www.mysql.com/Downloads/MySQL-@MYSQL_BASE_VERSION@/mysql-%{mysql_version}.tar.gz
Icon:		mysql.gif
URL:		http://www.mysql.com/
Packager:	Lenz Grimmer <lenz@mysql.com>
Vendor:		MySQL AB
Requires: fileutils sh-utils
Provides:	msqlormysql MySQL-server mysql
Obsoletes:	mysql

# Think about what you use here since the first step is to
# run a rm -rf
BuildRoot:    %{_tmppath}/%{name}-%{version}-build

# From the manual
%description
The MySQL(TM) software delivers a very fast, multi-threaded, multi-user,
and robust SQL (Structured Query Language) database server. MySQL Server
is intended for mission-critical, heavy-load production systems as well
as for embedding into mass-deployed software. MySQL is a trademark of
MySQL AB.

The MySQL software has Dual Licensing, which means you can use the MySQL
software free of charge under the GNU General Public License
(http://www.gnu.org/licenses/). You can also purchase commercial MySQL
licenses from MySQL AB if you do not wish to be bound by the terms of
the GPL. See the chapter "Licensing and Support" in the manual for
further info.

The MySQL web site (http://www.mysql.com/) provides the latest
news and information about the MySQL software. Also please see the
documentation and the manual for more information.

%package server
Release: %{release}
Summary:	MySQL: a very fast and reliable SQL database server
Group:		Applications/Databases
Summary(pt_BR): MySQL: Um servidor SQL rápido e confiável.
Group(pt_BR):	Aplicações/Banco_de_Dados
Requires: fileutils sh-utils
Provides:	msqlormysql mysql-server mysql MySQL
Obsoletes:	MySQL mysql mysql-server

%description server
The MySQL(TM) software delivers a very fast, multi-threaded, multi-user,
and robust SQL (Structured Query Language) database server. MySQL Server
is intended for mission-critical, heavy-load production systems as well
as for embedding into mass-deployed software. MySQL is a trademark of
MySQL AB.

The MySQL software has Dual Licensing, which means you can use the MySQL
software free of charge under the GNU General Public License
(http://www.gnu.org/licenses/). You can also purchase commercial MySQL
licenses from MySQL AB if you do not wish to be bound by the terms of
the GPL. See the chapter "Licensing and Support" in the manual for
further info.

The MySQL web site (http://www.mysql.com/) provides the latest
news and information about the MySQL software. Also please see the
documentation and the manual for more information.

This package includes the MySQL server binary (statically linked,
compiled with InnoDB support) as well as related utilities to run
and administrate a MySQL server.

If you want to access and work with the database, you have to install
package "MySQL-client" as well!

%package client
Release: %{release}
Summary: MySQL - Client
Group: Applications/Databases
Summary(pt_BR): MySQL - Cliente
Group(pt_BR): Aplicações/Banco_de_Dados
Obsoletes: mysql-client
Provides: mysql-client

%description client
This package contains the standard MySQL clients and administration tools. 

%{see_base}

%description client -l pt_BR
Este pacote contém os clientes padrão para o MySQL.

%package bench
Release: %{release}
Requires: %{name}-client perl-DBI perl
Summary: MySQL - Benchmarks and test system
Group: Applications/Databases
Summary(pt_BR): MySQL - Medições de desempenho
Group(pt_BR): Aplicações/Banco_de_Dados
Provides: mysql-bench
Obsoletes: mysql-bench

%description bench
This package contains MySQL benchmark scripts and data.

%{see_base}

%description bench -l pt_BR
Este pacote contém medições de desempenho de scripts e dados do MySQL.

%package devel
Release: %{release}
Requires: %{name}-client
Summary: MySQL - Development header files and libraries
Group: Applications/Databases
Summary(pt_BR): MySQL - Medições de desempenho
Group(pt_BR): Aplicações/Banco_de_Dados
Provides: mysql-devel
Obsoletes: mysql-devel

%description devel
This package contains the development header files and libraries
necessary to develop MySQL client applications.

%{see_base}

%description devel -l pt_BR
Este pacote contém os arquivos de cabeçalho (header files) e bibliotecas 
necessárias para desenvolver aplicações clientes do MySQL. 

%package shared
Release: %{release}
Summary: MySQL - Shared libraries
Group: Applications/Databases

%description shared
This package contains the shared libraries (*.so*) which certain
languages and applications need to dynamically load and use MySQL.

%package Max
Release: %{release}
Summary: MySQL - server with Berkeley DB, RAID and UDF support
Group: Applications/Databases
Provides: mysql-Max
Obsoletes: mysql-Max
Requires: MySQL >= 4.0

%description Max 
Optional MySQL server binary that supports additional features like
Berkeley DB, RAID and User Defined Functions (UDF).
To activate this binary, just install this package in addition to
the standard MySQL package.

Please note that this is a dynamically linked binary!

%package embedded
Release: %{release}
Requires: %{name}-devel
Summary: MySQL - embedded library
Group: Applications/Databases
Summary(pt_BR): MySQL - Medições de desempenho
Group(pt_BR): Aplicações/Banco_de_Dados
Obsoletes: mysql-embedded

%description embedded
This package contains the MySQL server as an embedded library.

The embedded MySQL server library makes it possible to run a
full-featured MySQL server inside the client application.
The main benefits are increased speed and more simple management
for embedded applications.

The API is identical for the embedded MySQL version and the
client/server version.

%{see_base}

%prep
%setup -n mysql-%{mysql_version}

%build
# The all-static flag is to make the RPM work on different
# distributions. This version tries to put shared mysqlclient libraries
# in a separate package.

BuildMySQL() {
# The --enable-assembler simply does nothing on systems that does not
# support assembler speedups.
sh -c  "PATH=\"${MYSQL_BUILD_PATH:-$PATH}\" \
	CFLAGS=\"${MYSQL_BUILD_CFLAGS:-$RPM_OPT_FLAGS}\" \
	CXXFLAGS=\"${MYSQL_BUILD_CXXFLAGS:-$RPM_OPT_FLAGS \
	          -felide-constructors -fno-exceptions -fno-rtti \
		  }\" \
	./configure \
 	    $* \
	    --enable-assembler \
	    --enable-local-infile \
            --with-mysqld-user=%{mysqld_user} \
            --with-unix-socket-path=/var/lib/mysql/mysql.sock \
            --prefix=/ \
	    --with-extra-charsets=complex \
            --exec-prefix=/usr \
            --libexecdir=/usr/sbin \
            --sysconfdir=/etc \
            --datadir=/usr/share \
            --localstatedir=/var/lib/mysql \
            --infodir=%{_infodir} \
            --includedir=/usr/include \
            --mandir=%{_mandir} \
	    --with-embedded-server \
	    --enable-thread-safe-client \
	    --with-comment=\"Official MySQL RPM\";
	    # Add this for more debugging support
	    # --with-debug
	    # Add this for MyISAM RAID support:
	    # --with-raid
	    "

 # benchdir does not fit in above model. Maybe a separate bench distribution
 make benchdir_root=$RPM_BUILD_ROOT/usr/share/
}

# Use our own copy of glibc

OTHER_LIBC_DIR=/usr/local/mysql-glibc
USE_OTHER_LIBC_DIR=""
if test -d "$OTHER_LIBC_DIR"
then
  USE_OTHER_LIBC_DIR="--with-other-libc=$OTHER_LIBC_DIR"
fi

# Use the build root for temporary storage of the shared libraries.

RBR=$RPM_BUILD_ROOT
MBD=$RPM_BUILD_DIR/mysql-%{mysql_version}

# Clean up the BuildRoot first
[ "$RBR" != "/" ] && [ -d $RBR ] && rm -rf $RBR;
mkdir -p $RBR

#
# Use MYSQL_BUILD_PATH so that we can use a dedicated version of gcc
#
PATH=${MYSQL_BUILD_PATH:-/bin:/usr/bin}
export PATH

# Build the 4.0 Max binary (includes BDB and UDFs and therefore
# cannot be linked statically against the patched glibc)

BuildMySQL "--enable-shared \
		--with-berkeley-db \
		--with-innodb \
		--with-raid \
		--with-server-suffix='-Max'"

# Save everything for debug
# tar cf $RBR/all.tar .

# Save mysqld-max
mv sql/mysqld sql/mysqld-max
nm --numeric-sort sql/mysqld-max > sql/mysqld-max.sym

# Save libraries
(cd libmysql/.libs; tar cf $RBR/shared-libs.tar *.so*)
(cd libmysql_r/.libs; tar rf $RBR/shared-libs.tar *.so*)

# Save manual to avoid rebuilding
mv Docs/manual.ps Docs/manual.ps.save
make distclean
mv Docs/manual.ps.save Docs/manual.ps

# RPM:s destroys Makefile.in files, so we generate them here
automake

# Now build the statically linked 4.0 binary (which includes InnoDB)
BuildMySQL "--disable-shared \
		--with-mysqld-ldflags='-all-static' \
		--with-client-ldflags='-all-static' \
		$USE_OTHER_LIBC_DIR \
		--without-berkeley-db \
		--with-innodb \
		--without-vio \
		--without-openssl"
nm --numeric-sort sql/mysqld > sql/mysqld.sym

%install -n mysql-%{mysql_version}
RBR=$RPM_BUILD_ROOT
MBD=$RPM_BUILD_DIR/mysql-%{mysql_version}

# Ensure that needed directories exists
install -d $RBR/etc/{logrotate.d,init.d}
install -d $RBR/var/lib/mysql/mysql
install -d $RBR/usr/share/{sql-bench,mysql-test}
install -d $RBR%{_mandir}
install -d $RBR/usr/{sbin,lib,include}

# Install all binaries stripped 
make install-strip DESTDIR=$RBR benchdir_root=/usr/share/

# Install shared libraries (Disable for architectures that don't support it)
(cd $RBR/usr/lib; tar xf $RBR/shared-libs.tar)

# install saved mysqld-max
install -s -m755 $MBD/sql/mysqld-max $RBR/usr/sbin/mysqld-max

# install symbol files ( for stack trace resolution)
install -m644 $MBD/sql/mysqld-max.sym $RBR/usr/lib/mysql/mysqld-max.sym
install -m644 $MBD/sql/mysqld.sym $RBR/usr/lib/mysql/mysqld.sym

# Install logrotate and autostart
install -m644 $MBD/support-files/mysql-log-rotate $RBR/etc/logrotate.d/mysql
install -m755 $MBD/support-files/mysql.server $RBR/etc/init.d/mysql

# Create symbolic compatibility link safe_mysqld -> mysqld_safe
# (safe_mysqld will be gone in MySQL 4.1)
ln -sf ./mysqld_safe $RBR/usr/bin/safe_mysqld

# Touch the place where the my.cnf config file might be located
# Just to make sure it's in the file list and marked as a config file
touch $RBR/etc/my.cnf

%pre server
# Shut down a previously installed server first
if test -x /etc/init.d/mysql
then
  /etc/init.d/mysql stop > /dev/null 2>&1
  echo "Giving mysqld a couple of seconds to exit nicely"
  sleep 5
elif test -x /etc/rc.d/init.d/mysql
then
  /etc/rc.d/init.d/mysql stop > /dev/null 2>&1
  echo "Giving mysqld a couple of seconds to exit nicely"
  sleep 5
fi

%post server
mysql_datadir=/var/lib/mysql

# Create data directory if needed
if test ! -d $mysql_datadir; then mkdir -m755 $mysql_datadir; fi
if test ! -d $mysql_datadir/mysql; then mkdir $mysql_datadir/mysql; fi
if test ! -d $mysql_datadir/test; then mkdir $mysql_datadir/test; fi

# Make MySQL start/shutdown automatically when the machine does it.
# use insserv for older SuSE Linux versions
if test -x /sbin/insserv
then
	/sbin/insserv /etc/init.d/mysql
# use chkconfig on Red Hat and newer SuSE releases
elif test -x /sbin/chkconfig
then
	/sbin/chkconfig --add mysql
fi

# Create a MySQL user. Do not report any problems if it already
# exists. This is redhat specific and should be handled more portable
useradd -M -r -d $mysql_datadir -s /bin/bash -c "MySQL server" mysql 2> /dev/null || true 

# Change permissions so that the user that will run the MySQL daemon
# owns all database files.
chown -R mysql $mysql_datadir

# Initiate databases
mysql_install_db -IN-RPM

# Change permissions again to fix any new files.
chown -R mysql $mysql_datadir

# Fix permissions for the permission database so that only the user
# can read them.
chmod -R og-rw $mysql_datadir/mysql

# Restart in the same way that mysqld will be started normally.
/etc/init.d/mysql start

# Allow safe_mysqld to start mysqld and print a message before we exit
sleep 2

%post Max
# Restart mysqld, to use the new binary.
echo "Restarting mysqld."
/etc/init.d/mysql restart > /dev/null 2>&1

%preun server
if test $1 = 0
then
	# Stop MySQL before uninstalling it
  if test -x /etc/init.d/mysql
  then
    /etc/init.d/mysql stop > /dev/null
  fi

  # Remove autostart of mysql
	# for older SuSE Linux versions
	if test -x /sbin/insserv
	then
		/sbin/insserv -r /etc/init.d/mysql
	# use chkconfig on Red Hat and newer SuSE releases
	elif test -x /sbin/chkconfig
	then
		/sbin/chkconfig --del mysql
	fi
fi

# We do not remove the mysql user since it may still own a lot of
# database files.

# Clean up the BuildRoot
%clean
[ "$RBR" != "/" ] && [ -d $RBR ] && rm -rf $RBR;

%files server
%defattr(755 root, root)

%doc %attr(644, root, root) COPYING COPYING.LIB README
%doc %attr(644, root, root) Docs/manual.{html,ps,texi,txt} Docs/manual_toc.html
%doc %attr(644, root, root) support-files/my-*.cnf

%doc %attr(644, root, root) %{_infodir}/mysql.info*

%doc %attr(644, root, man) %{_mandir}/man1/isamchk.1*
%doc %attr(644, root, man) %{_mandir}/man1/isamlog.1*
%doc %attr(644, root, man) %{_mandir}/man1/mysql_zap.1*
%doc %attr(644, root, man) %{_mandir}/man1/mysqld.1*
%doc %attr(644, root, man) %{_mandir}/man1/mysqld_multi.1*
%doc %attr(644, root, man) %{_mandir}/man1/mysqld_safe.1*
%doc %attr(644, root, man) %{_mandir}/man1/perror.1*
%doc %attr(644, root, man) %{_mandir}/man1/replace.1*

%ghost %config(noreplace,missingok) /etc/my.cnf

%attr(755, root, root) /usr/bin/isamchk
%attr(755, root, root) /usr/bin/isamlog
%attr(755, root, root) /usr/bin/my_print_defaults
%attr(755, root, root) /usr/bin/myisamchk
%attr(755, root, root) /usr/bin/myisamlog
%attr(755, root, root) /usr/bin/myisampack
%attr(755, root, root) /usr/bin/mysql_convert_table_format
%attr(755, root, root) /usr/bin/mysql_explain_log
%attr(755, root, root) /usr/bin/mysql_fix_privilege_tables
%attr(755, root, root) /usr/bin/mysql_install_db
%attr(755, root, root) /usr/bin/mysql_secure_installation
%attr(755, root, root) /usr/bin/mysql_setpermission
%attr(755, root, root) /usr/bin/mysql_zap
%attr(755, root, root) /usr/bin/mysqlbug
%attr(755, root, root) /usr/bin/mysqld_multi
%attr(755, root, root) /usr/bin/mysqld_safe
%attr(755, root, root) /usr/bin/mysqlhotcopy
%attr(755, root, root) /usr/bin/mysqltest
%attr(755, root, root) /usr/bin/pack_isam
%attr(755, root, root) /usr/bin/perror
%attr(755, root, root) /usr/bin/replace
%attr(755, root, root) /usr/bin/resolve_stack_dump
%attr(755, root, root) /usr/bin/resolveip
%attr(755, root, root) /usr/bin/safe_mysqld

%attr(755, root, root) /usr/sbin/mysqld
%attr(644, root, root) /usr/lib/mysql/mysqld.sym

%attr(644, root, root) /etc/logrotate.d/mysql
%attr(755, root, root) /etc/init.d/mysql

%attr(755, root, root) /usr/share/mysql/

%files client
%attr(755, root, root) /usr/bin/msql2mysql
%attr(755, root, root) /usr/bin/mysql
%attr(755, root, root) /usr/bin/mysql_find_rows
%attr(755, root, root) /usr/bin/mysql_waitpid
%attr(755, root, root) /usr/bin/mysqlaccess
%attr(755, root, root) /usr/bin/mysqladmin
%attr(755, root, root) /usr/bin/mysqlbinlog
%attr(755, root, root) /usr/bin/mysqlcheck
%attr(755, root, root) /usr/bin/mysqldump
%attr(755, root, root) /usr/bin/mysqlimport
%attr(755, root, root) /usr/bin/mysqlshow

%doc %attr(644, root, man) %{_mandir}/man1/mysql.1*
%doc %attr(644, root, man) %{_mandir}/man1/mysqlaccess.1*
%doc %attr(644, root, man) %{_mandir}/man1/mysqladmin.1*
%doc %attr(644, root, man) %{_mandir}/man1/mysqldump.1*
%doc %attr(644, root, man) %{_mandir}/man1/mysqlshow.1*

%post shared
/sbin/ldconfig

%postun shared
/sbin/ldconfig

%files devel
%defattr(644 root, root)
%attr(755, root, root) /usr/bin/comp_err
%attr(755, root, root) /usr/bin/mysql_config
%dir %attr(755, root, root) /usr/include/mysql
%dir %attr(755, root, root) /usr/lib/mysql
/usr/include/mysql/*
/usr/lib/mysql/libdbug.a
/usr/lib/mysql/libheap.a
/usr/lib/mysql/libmerge.a
/usr/lib/mysql/libmyisam.a
/usr/lib/mysql/libmyisammrg.a
/usr/lib/mysql/libmysqlclient.a
/usr/lib/mysql/libmysqlclient.la
/usr/lib/mysql/libmysqlclient_r.a
/usr/lib/mysql/libmysqlclient_r.la
/usr/lib/mysql/libmystrings.a
/usr/lib/mysql/libmysys.a
/usr/lib/mysql/libnisam.a
/usr/lib/mysql/libvio.a

%files shared
%defattr(755 root, root)
# Shared libraries (omit for architectures that don't support them)
/usr/lib/*.so*

%files bench
%attr(-, root, root) /usr/share/sql-bench
%attr(-, root, root) /usr/share/mysql-test
%attr(755, root, root) /usr/bin/mysqlmanager
%attr(755, root, root) /usr/bin/mysqlmanager-pwgen
%attr(755, root, root) /usr/bin/mysqlmanagerc

%files Max
%attr(755, root, root) /usr/sbin/mysqld-max
%attr(644, root, root) /usr/lib/mysql/mysqld-max.sym

%files embedded
%attr(644, root, root) /usr/lib/mysql/libmysqld.a

%changelog 

* Mon Mar 10 2003 Lenz Grimmer <lenz@mysql.com>

- added missing file mysql_secure_installation to server subpackage
  (bug #141)

* Tue Feb 11 2003 Lenz Grimmer <lenz@mysql.com>

- re-added missing pre- and post(un)install scripts to server subpackage
- added config file /etc/my.cnf to the file list (just for completeness)
- make sure to create the datadir with 755 permissions

* Mon Jan 27 2003 Lenz Grimmer <lenz@mysql.com>

- removed unused CC and CXX variables
- CFLAGS and CXXFLAGS should honor RPM_OPT_FLAGS

* Fri Jan 24 2003 Lenz Grimmer <lenz@mysql.com>

- renamed package "MySQL" to "MySQL-server"
- fixed Copyright tag
- added mysql_waitpid to client subpackage (required for mysql-test-run)

* Wed Nov 27 2002 Lenz Grimmer <lenz@mysql.com>

- moved init script from /etc/rc.d/init.d to /etc/init.d (the majority of 
  Linux distributions now support this scheme as proposed by the LSB either
  directly or via a compatibility symlink)
- Use new "restart" init script action instead of starting and stopping
  separately
- Be more flexible in activating the automatic bootup - use insserv (on
  older SuSE versions) or chkconfig (Red Hat, newer SuSE versions and
  others) to create the respective symlinks

* Wed Sep 25 2002 Lenz Grimmer <lenz@mysql.com>

- MySQL-Max now requires MySQL >= 4.0 to avoid version mismatches
  (mixing 3.23 and 4.0 packages)

* Fri Aug 09 2002 Lenz Grimmer <lenz@mysql.com>
 
- Turn off OpenSSL in MySQL-Max for now until it works properly again
- enable RAID for the Max binary instead
- added compatibility link: safe_mysqld -> mysqld_safe to ease the
  transition from 3.23

* Thu Jul 18 2002 Lenz Grimmer <lenz@mysql.com>

- Reworked the build steps a little bit: the Max binary is supposed
  to include OpenSSL, which cannot be linked statically, thus trying
	to statically link against a special glibc is futile anyway
- because of this, it is not required to make yet another build run
  just to compile the shared libs (saves a lot of time)
- updated package description of the Max subpackage
- clean up the BuildRoot directory afterwards

* Mon Jul 15 2002 Lenz Grimmer <lenz@mysql.com>

- Updated Packager information
- Fixed the build options: the regular package is supposed to
  include InnoDB and linked statically, while the Max package
	should include BDB and SSL support

* Fri May 03 2002 Lenz Grimmer <lenz@mysql.com>

- Use more RPM macros (e.g. infodir, mandir) to make the spec
  file more portable
- reorganized the installation of documentation files: let RPM
  take care of this
- reorganized the file list: actually install man pages along
  with the binaries of the respective subpackage
- do not include libmysqld.a in the devel subpackage as well, if we
  have a special "embedded" subpackage
- reworked the package descriptions

* Mon Oct  8 2001 Monty

- Added embedded server as a separate RPM

* Fri Apr 13 2001 Monty

- Added mysqld-max to the distribution

* Tue Jan 2  2001  Monty

- Added mysql-test to the bench package

* Fri Aug 18 2000 Tim Smith <tim@mysql.com>

- Added separate libmysql_r directory; now both a threaded
  and non-threaded library is shipped.

* Wed Sep 28 1999 David Axmark <davida@mysql.com>

- Added the support-files/my-example.cnf to the docs directory.

- Removed devel dependency on base since it is about client
  development.

* Wed Sep 8 1999 David Axmark <davida@mysql.com>

- Cleaned up some for 3.23.

* Thu Jul 1 1999 David Axmark <davida@mysql.com>

- Added support for shared libraries in a separate sub
  package. Original fix by David Fox (dsfox@cogsci.ucsd.edu)

- The --enable-assembler switch is now automatically disables on
  platforms there assembler code is unavailable. This should allow
  building this RPM on non i386 systems.

* Mon Feb 22 1999 David Axmark <david@detron.se>

- Removed unportable cc switches from the spec file. The defaults can
  now be overridden with environment variables. This feature is used
  to compile the official RPM with optimal (but compiler version
  specific) switches.

- Removed the repetitive description parts for the sub rpms. Maybe add
  again if RPM gets a multiline macro capability.

- Added support for a pt_BR translation. Translation contributed by
  Jorge Godoy <jorge@bestway.com.br>.

* Wed Nov 4 1998 David Axmark <david@detron.se>

- A lot of changes in all the rpm and install scripts. This may even
  be a working RPM :-)

* Sun Aug 16 1998 David Axmark <david@detron.se>

- A developers changelog for MySQL is available in the source RPM. And
  there is a history of major user visible changed in the Reference
  Manual.  Only RPM specific changes will be documented here.
