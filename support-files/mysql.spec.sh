%define mysql_version		@VERSION@
%define shared_lib_version	@SHARED_LIB_VERSION@
%define release			1
%define mysqld_user		mysql

%define see_base For a description of MySQL see the base MySQL RPM or http://www.mysql.com

Name: MySQL
Summary:	MySQL: a very fast and reliable SQL database engine
Group:		Applications/Databases
Summary(pt_BR): MySQL: Um servidor SQL rápido e confiável.
Group(pt_BR):	Aplicações/Banco_de_Dados
Version:	@MYSQL_NO_DASH_VERSION@
Release:	%{release}
Copyright:	GPL / LGPL
Source:		http://www.mysql.com/Downloads/MySQL-@MYSQL_BASE_VERSION@/mysql-%{mysql_version}.tar.gz
Icon:		mysql.gif
URL:		http://www.mysql.com/
Packager:	David Axmark <david@mysql.com>
Provides:	msqlormysql MySQL-server
Obsoletes:	mysql

# Think about what you use here since the first step is to
# run a rm -rf
BuildRoot:	/var/tmp/mysql

# From the manual
%description
MySQL is a true multi-user, multi-threaded SQL (Structured Query
Language) database server. MySQL is a client/server implementation
that consists of a server daemon (mysqld) and many different client
programs/libraries.

The main goals of MySQL are speed, robustness and ease of use.  MySQL
was originally developed because we needed a SQL server that could
handle very big databases with magnitude higher speed than what any
database vendor could offer to us. And since we did not need all the
features that made their server slow we made our own. We have now been
using MySQL since 1996 in a environment with more than 40 databases,
10,000 tables, of which more than 500 have more than 7 million
rows. This is about 200G of data.

The base upon which MySQL is built is a set of routines that have been
used in a highly demanding production environment for many
years. While MySQL is still in development, it already offers a rich
and highly useful function set.

See the documentation for more information

%description -l pt_BR
O MySQL é um servidor de banco de dados SQL realmente multiusuário e\
multi-tarefa. A linguagem SQL é a mais popular linguagem para banco de\
dados no mundo. O MySQL é uma implementação cliente/servidor que\
consiste de um servidor chamado mysqld e diversos\
programas/bibliotecas clientes. Os principais objetivos do MySQL são:\
velocidade, robustez e facilidade de uso.  O MySQL foi originalmente\
desenvolvido porque nós na Tcx precisávamos de um servidor SQL que\
pudesse lidar com grandes bases de dados e com uma velocidade muito\
maior do que a que qualquer vendedor podia nos oferecer. Estamos\
usando\
o MySQL desde 1996 em um ambiente com mais de 40 bases de dados com 10.000\
tabelas, das quais mais de 500 têm mais de 7 milhões de linhas. Isto é o\
equivalente a aproximadamente 50G de dados críticos. A base da construção do\
MySQL é uma série de rotinas que foram usadas em um ambiente de produção com\
alta demanda por muitos anos. Mesmo o MySQL estando ainda em desenvolvimento,\
ele já oferece um conjunto de funções muito ricas e úteis. Veja a documentação\
para maiores informações.

%package client
Release: %{release}
Summary: MySQL - Client
Group: Applications/Databases
Summary(pt_BR): MySQL - Cliente
Group(pt_BR): Aplicações/Banco_de_Dados
Obsoletes: mysql-client

%description client
This package contains the standard MySQL clients. 

%{see_base}

%description client -l pt_BR
Este pacote contém os clientes padrão para o MySQL.

%package bench
Release: %{release}
Requires: MySQL-client MySQL-DBI-perl-bin perl
Summary: MySQL - Benchmarks and test system
Group: Applications/Databases
Summary(pt_BR): MySQL - Medições de desempenho
Group(pt_BR): Aplicações/Banco_de_Dados
Obsoletes: mysql-bench

%description bench
This package contains MySQL benchmark scripts and data.

%{see_base}

%description bench -l pt_BR
Este pacote contém medições de desempenho de scripts e dados do MySQL.

%package devel
Release: %{release}
Requires: MySQL-client
Summary: MySQL - Development header files and libraries
Group: Applications/Databases
Summary(pt_BR): MySQL - Medições de desempenho
Group(pt_BR): Aplicações/Banco_de_Dados
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

%prep
%setup -n mysql-%{mysql_version}

%build
# The all-static flag is to make the RPM work on different
# distributions. This version tries to put shared mysqlclient libraries
# in a separate package.

BuildMySQL() {
# The --enable-assembler simply does nothing on systems that does not
# support assembler speedups.
sh -c  "PATH=\"${MYSQL_BUILD_PATH:-/bin:/usr/bin}\" \
	CC=\"${MYSQL_BUILD_CC:-egcs}\" \
	CFLAGS=\"${MYSQL_BUILD_CFLAGS:- -O6 -fno-omit-frame-pointer}\" \
	CXX=\"${MYSQL_BUILD_CXX:-egcs}\" \
	CXXFLAGS=\"${MYSQL_BUILD_CXXFLAGS:- -O6 \
	          -felide-constructors -fno-exceptions -fno-rtti \
		  -fno-omit-frame-pointer}\" \
	./configure \
 	    $* \
	    --enable-assembler \
            --with-mysqld-user=%{mysqld_user} \
            --with-unix-socket-path=/var/lib/mysql/mysql.sock \
            --prefix=/ \
	    --with-extra-charsets=complex \
            --exec-prefix=/usr \
            --libexecdir=/usr/sbin \
            --sysconfdir=/etc \
            --datadir=/usr/share \
            --localstatedir=/var/lib/mysql \
            --infodir=/usr/info \
            --includedir=/usr/include \
            --mandir=/usr/man \
	    --without-berkeley-db \
	    --without-innobase \
	    --with-comment=\"Official MySQL RPM\";
	    # Add this for more debugging support
	    # --with-debug
	    # Add this for MyISAM RAID support:
	    # --with-raid
	    "

 # benchdir does not fit in above model. Maybe a separate bench distribution
 make benchdir_root=$RPM_BUILD_ROOT/usr/share/
}

# Use the build root for temporary storage of the shared libraries.

RBR=$RPM_BUILD_ROOT
MBD=$RPM_BUILD_DIR/mysql-%{mysql_version}
if test -z "$RBR" -o "$RBR" = "/"
then
	echo "RPM_BUILD_ROOT has stupid value"
	exit 1
fi
rm -rf $RBR
mkdir -p $RBR

BuildMySQL "--enable-shared --enable-thread-safe-client --without-server"

# Save everything for debus
tar cf $RBR/all.tar .

# Save shared libraries
(cd libmysql/.libs; tar cf $RBR/shared-libs.tar *.so*)
(cd libmysql_r/.libs; tar rf $RBR/shared-libs.tar *.so*)

# Save manual to avoid rebuilding
mv Docs/manual.ps Docs/manual.ps.save
make distclean
mv Docs/manual.ps.save Docs/manual.ps

BuildMySQL "--disable-shared" \
	   "--with-mysqld-ldflags='-all-static'" \
	   "--with-client-ldflags='-all-static'"

%install -n mysql-%{mysql_version}
RBR=$RPM_BUILD_ROOT
MBD=$RPM_BUILD_DIR/mysql-%{mysql_version}
# Ensure that needed directories exists
install -d $RBR/etc/{logrotate.d,rc.d/init.d}
install -d $RBR/var/lib/mysql/mysql
install -d $RBR/usr/share/sql-bench
install -d $RBR/usr/share/mysql-test
install -d $RBR/usr/{sbin,share,man,include}
install -d $RBR/usr/doc/MySQL-%{mysql_version}
install -d $RBR/usr/lib
# Make install
make install DESTDIR=$RBR benchdir_root=/usr/share/

# Install shared libraries (Disable for architectures that don't support it)
(cd $RBR/usr/lib; tar xf $RBR/shared-libs.tar)

# Install logrotate and autostart
install -m644 $MBD/support-files/mysql-log-rotate $RBR/etc/logrotate.d/mysql
install -m755 $MBD/support-files/mysql.server $RBR/etc/rc.d/init.d/mysql

# Install docs
install -m644 $RPM_BUILD_DIR/mysql-%{mysql_version}/Docs/mysql.info \
 $RBR/usr/info/mysql.info
for file in README COPYING COPYING.LIB Docs/manual_toc.html Docs/manual.html \
    Docs/manual.txt Docs/manual.texi Docs/manual.ps \
    support-files/my-huge.cnf support-files/my-large.cnf \
    support-files/my-medium.cnf support-files/my-small.cnf
do
    b=`basename $file`
    install -m644 $MBD/$file $RBR/usr/doc/MySQL-%{mysql_version}/$b
done

%pre
if test -x /etc/rc.d/init.d/mysql
then
  /etc/rc.d/init.d/mysql stop > /dev/null 2>&1
  echo "Giving mysqld a couple of seconds to exit nicely"
  sleep 5
fi

%post
mysql_datadir=/var/lib/mysql

# Create data directory if needed
if test ! -d $mysql_datadir;		then mkdir $mysql_datadir; fi
if test ! -d $mysql_datadir/mysql;	then mkdir $mysql_datadir/mysql; fi
if test ! -d $mysql_datadir/test;	then mkdir $mysql_datadir/test; fi

# Make MySQL start/shutdown automatically when the machine does it.
/sbin/chkconfig --add mysql

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
/etc/rc.d/init.d/mysql start

# Allow safe_mysqld to start mysqld and print a message before we exit
sleep 2

%preun
if test -x /etc/rc.d/init.d/mysql
then
  /etc/rc.d/init.d/mysql stop > /dev/null
fi
# Remove autostart of mysql
if test $1 = 0
then
   /sbin/chkconfig --del mysql
fi
# We do not remove the mysql user since it may still own a lot of
# database files.

%files
%attr(-, root, root) %doc /usr/doc/MySQL-%{mysql_version}/

%attr(755, root, root) /usr/bin/isamchk
%attr(755, root, root) /usr/bin/isamlog
%attr(755, root, root) /usr/bin/pack_isam
%attr(755, root, root) /usr/bin/myisamchk
%attr(755, root, root) /usr/bin/myisamlog
%attr(755, root, root) /usr/bin/myisampack
%attr(755, root, root) /usr/bin/mysql_fix_privilege_tables
%attr(755, root, root) /usr/bin/mysql_convert_table_format
%attr(755, root, root) /usr/bin/mysql_install_db
%attr(755, root, root) /usr/bin/mysql_setpermission
%attr(755, root, root) /usr/bin/mysql_zap
%attr(755, root, root) /usr/bin/mysqlbug
%attr(755, root, root) /usr/bin/mysqltest
%attr(755, root, root) /usr/bin/mysqlhotcopy
%attr(755, root, root) /usr/bin/perror
%attr(755, root, root) /usr/bin/replace
%attr(755, root, root) /usr/bin/resolveip
%attr(755, root, root) /usr/bin/safe_mysqld
%attr(755, root, root) /usr/bin/mysqld_multi
%attr(755, root, root) /usr/bin/my_print_defaults

%attr(644, root, root) /usr/info/mysql.info*

%attr(755, root, root) /usr/sbin/mysqld

%attr(644, root, root) /etc/logrotate.d/mysql
%attr(755, root, root) /etc/rc.d/init.d/mysql

%attr(755, root, root) /usr/share/mysql/

%files client
%attr(755, root, root) /usr/bin/msql2mysql
%attr(755, root, root) /usr/bin/mysql
%attr(755, root, root) /usr/bin/mysqlaccess
%attr(755, root, root) /usr/bin/mysqladmin
%attr(755, root, root) /usr/bin/mysql_find_rows
%attr(755, root, root) /usr/bin/mysqldump
%attr(755, root, root) /usr/bin/mysqlimport
%attr(755, root, root) /usr/bin/mysqlshow
%attr(755, root, root) /usr/bin/mysqlbinlog

%attr(644, root, man) %doc /usr/man/man1/mysql.1*
%attr(644, root, man) %doc /usr/man/man1/isamchk.1*
%attr(644, root, man) %doc /usr/man/man1/isamlog.1*
%attr(644, root, man) %doc /usr/man/man1/mysql_zap.1*
%attr(644, root, man) %doc /usr/man/man1/mysqlaccess.1*
%attr(644, root, man) %doc /usr/man/man1/mysqladmin.1*
%attr(644, root, man) %doc /usr/man/man1/mysqld.1*
%attr(644, root, man) %doc /usr/man/man1/mysqld_multi.1*
%attr(644, root, man) %doc /usr/man/man1/mysqldump.1*
%attr(644, root, man) %doc /usr/man/man1/mysqlshow.1*
%attr(644, root, man) %doc /usr/man/man1/perror.1*
%attr(644, root, man) %doc /usr/man/man1/replace.1*
%attr(644, root, man) %doc /usr/man/man1/safe_mysqld.1*

%post shared
/sbin/ldconfig

%postun shared
/sbin/ldconfig

%files devel
%attr(755, root, root) /usr/bin/comp_err
%attr(755, root, root) /usr/include/mysql/
%attr(755, root, root) /usr/lib/mysql/
%attr(755, root, root) /usr/bin/mysql_config

%files shared
# Shared libraries (omit for architectures that don't support them)
%attr(755, root, root) /usr/lib/*.so*

%files bench
%attr(-, root, root) /usr/share/sql-bench
%attr(-, root, root) /usr/share/mysql-test

%changelog 

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
