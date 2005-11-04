#  Copyright (c) 2002 Novell, Inc. All Rights Reserved.
#
#  This program is free software; you can redistribute it and/or modify 
#  it under the terms of the GNU General Public License as published by 
#  the Free Software Foundation; either version 2 of the License, or 
#  (at your option) any later version. 
#
#  This program is distributed in the hope that it will be useful, 
#  but WITHOUT ANY WARRANTY; without even the implied warranty of 
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
#  GNU General Public License for more details. 
#
#  You should have received a copy of the GNU General Public License 
#  along with this program; if not, write to the Free Software 
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

if HAVE_NETWARE
INCLUDES =			-I$(top_builddir)/include -I$(top_srcdir)/include -I..
LDADD =				@CLIENT_EXTRA_LDFLAGS@ ../mysys/libmysys.a \
				../dbug/libdbug.a ../strings/libmystrings.a
bin_PROGRAMS =			mysqld_safe mysql_install_db mysql_test_run libmysql
mysqld_safe_SOURCES=		mysqld_safe.c my_manage.c
mysql_install_db_SOURCES=	mysql_install_db.c my_manage.c
mysql_test_run_SOURCES=		mysql_test_run.c my_manage.c
libmysql_SOURCES=		libmysqlmain.c
libmysql_LDADD =		../libmysql/.libs/libmysqlclient.a @openssl_libs@

netware_build_files =           client/mysql.def client/mysqladmin.def \
                                client/mysqlbinlog.def client/mysqlcheck.def \
                                client/mysqldump.def client/mysqlimport.def \
                                client/mysqlshow.def client/mysqltest.def \
                                extra/mysql_install.def extra/my_print_defaults.def \
                                extra/perror.def extra/replace.def \
                                extra/resolveip.def extra/comp_err.def \
				isam/isamchk.def \
                                isam/isamlog.def isam/pack_isam.def \
                                libmysqld/libmysqld.def myisam/myisamchk.def \
                                myisam/myisamlog.def myisam/myisampack.def \
                                sql/mysqld.def extra/mysql_waitpid.def \
                                extra/resolve_stack_dump.def myisam/myisam_ftdump.def

link_sources:
	set -x; \
	for f in $(netware_build_files); do \
	  rm -f ../$$f; \
	  org=`echo $$f | sed -e 's/.*\/\(.*\)/\1/g'`; \
	  @LN_CP_F@ $(srcdir)/$$org ../$$f; \
	done
else
EXTRA_DIST=	comp_err.def init_db.sql install_test_db.ncf \
		isamchk.def isamlog.def libmysql.def libmysql.imp \
		libmysqlmain.c my_manage.c my_manage.h \
		my_print_defaults.def myisam_ftdump.def myisamchk.def \
		myisamlog.def myisampack.def mysql.def mysql.xdc \
		mysql_fix_privilege_tables.pl mysql_install.def \
		mysql_install_db.c mysql_install_db.def \
		mysql_secure_installation.pl mysql_test_run.c \
		mysql_test_run.def mysql_waitpid.def mysqladmin.def \
		mysqlbinlog.def mysqlcheck.def mysqld.def \
		mysqld_safe.c mysqld_safe.def mysqldump.def mysqlimport.def \
		mysqlshow.def mysqltest.def pack_isam.def perror.def \
		replace.def resolve_stack_dump.def resolveip.def test_db.sql \
		static_init_db.sql \
		BUILD/apply-patch BUILD/compile-AUTOTOOLS \
		BUILD/compile-linux-tools BUILD/compile-netware-END \
		BUILD/compile-netware-START BUILD/compile-netware-all\
		BUILD/compile-netware-debug BUILD/compile-netware-max \
		BUILD/compile-netware-max-debug BUILD/compile-netware-src \
		BUILD/compile-netware-standard BUILD/create-patch \
		BUILD/cron-build BUILD/crontab BUILD/knetware.imp \
		BUILD/mwasmnlm BUILD/mwccnlm BUILD/mwenv BUILD/mwldnlm \
		BUILD/nwbootstrap BUILD/openssl.imp BUILD/save-patch
endif

# Don't update the files from bitkeeper
%::SCCS/s.%
