%define mysql_version		@VERSION@
%define shared_lib_version	@SHARED_LIB_VERSION@
%define release			2
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
Packager:	David Axmark <david@mysql.com>, Monty <monty@mysql.com>
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

The MySQL-max version differs from the normal MySQL server distribution
in that the BDB and Innobase table handlers are enabled by default.
You can use any normal MySQL client with the MySQL-max server.

See the documentation for more information.

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

%prep
%setup -n mysql-max-%{mysql_version}
# %setup -T -D -a 1 -n mysql-%{mysql_version}

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
            --with-berkeley-db \
            --with-innodb \
	    --with-comment=\"Official MySQL-Max RPM\";
	    # Add this for more debugging support
	    # --with-debug
	    # Add this for MyISAM RAID support:
	    # --with-raid
	    "
 make
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

#cd $MBD/db-%{db_version}/dist 
#./configure --prefix=$RBR/usr/BDB
#make install
#
#echo $RBR $MBD
#cd $MBD

BuildMySQL "--disable-shared" \
	   "--with-mysqld-ldflags='-all-static'" \
	   "--with-client-ldflags='-all-static'"

%install -n mysql-max-%{mysql_version}
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

%changelog 

* 2000-04-01 Monty
  First version of mysql-max.spec.sh based on mysql.spec.sh
