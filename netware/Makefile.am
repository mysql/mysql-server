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

INCLUDES =			-I$(srcdir)/../include -I../include -I..
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
	  rm -f $(srcdir)/../$$f; \
	  org=`echo $$f | sed -e 's/.*\/\(.*\)/\1/g'`; \
	  @LN_CP_F@ $(srcdir)/$$org $(srcdir)/../$$f; \
	done;


# Don't update the files from bitkeeper
%::SCCS/s.%
