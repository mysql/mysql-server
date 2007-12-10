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
libmysql_LDADD =		../libmysql/.libs/libmysqlclient.a \
				@openssl_libs@ @yassl_libs@

netware_build_files =           client/mysql.def client/mysqladmin.def \
                                client/mysqlbinlog.def client/mysqlcheck.def \
                                client/mysqldump.def client/mysqlimport.def \
                                client/mysqlshow.def client/mysqltest.def \
                                client/mysqlslap.def client/mysql_upgrade.def \
                                sql/mysqld.def extra/mysql_waitpid.def \
				tests/mysql_client_test.def \
                                extra/my_print_defaults.def \
                                extra/perror.def extra/replace.def \
                                extra/resolveip.def extra/comp_err.def \
                                extra/resolve_stack_dump.def \
                                libmysqld/libmysqld.def \
                                storage/myisam/myisamchk.def \
                                storage/myisam/myisamlog.def \
                                storage/myisam/myisampack.def \
                                storage/myisam/myisam_ftdump.def

BUILT_SOURCES =                 link_sources init_db.sql test_db.sql
CLEANFILES = 			$(BUILT_SOURCES)

all: $(BUILT_SOURCES)

link_sources:
	for f in $(netware_build_files); do \
	  rm -f ../$$f; \
	  org=`basename $$f`; \
	  @LN_CP_F@ $(srcdir)/$$org ../$$f; \
	done
	echo timestamp > link_sources

else

BUILT_SOURCES =                 libmysql.imp init_db.sql test_db.sql
DISTCLEANFILES =                libmysql.imp
CLEANFILES =                    init_db.sql test_db.sql

# Create the libmysql.imp from libmysql/libmysql.def
libmysql.imp: $(top_srcdir)/libmysql/libmysql.def
	$(AWK) 'BEGIN{x=0;} \
             END{printf("\n");} \
             x==1 {printf(" %s",$$1); x++; next} \
             x>1 {printf(",\n %s", $$1); next} \
             /EXPORTS/{x=1}' $(top_srcdir)/libmysql/libmysql.def > libmysql.imp

EXTRA_DIST=	$(BUILT_SOURCES) comp_err.def install_test_db.ncf \
		libmysql.def \
		libmysqlmain.c my_manage.c my_manage.h \
		my_print_defaults.def myisam_ftdump.def myisamchk.def \
		myisamlog.def myisampack.def mysql.def mysql.xdc \
		mysql_fix_privilege_tables.pl \
		mysql_install_db.c mysql_install_db.def \
		mysql_secure_installation.pl mysql_test_run.c \
		mysql_test_run.def mysql_waitpid.def mysqladmin.def \
		mysqlbinlog.def mysqlcheck.def mysqld.def \
		mysqld_safe.c mysqld_safe.def mysqldump.def mysqlimport.def \
		mysqlshow.def mysqltest.def mysqlslap.def mysql_upgrade.def \
		perror.def \
		mysql_client_test.def \
		replace.def resolve_stack_dump.def resolveip.def \
		static_init_db.sql init_db.sql test_db.sql \
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


# Build init_db.sql from the files that contain
# the system tables for this version of MySQL plus any commands
init_db.sql: $(top_srcdir)/scripts/mysql_system_tables.sql \
	     $(top_srcdir)/scripts/mysql_system_tables_data.sql
	@echo "Building $@";
	@echo "CREATE DATABASE mysql;" > $@;
	@echo "CREATE DATABASE test;" >> $@;
	@echo "use mysql;" >> $@;
	@cat $(top_srcdir)/scripts/mysql_system_tables.sql \
	     $(top_srcdir)/scripts/mysql_system_tables_fix.sql >> $@;

# Build test_db.sql from init_db.sql plus
# some test data
test_db.sql: init_db.sql $(top_srcdir)/scripts/mysql_test_data_timezone.sql
	@echo "Building $@";
	@cat init_db.sql \
	     $(top_srcdir)/scripts/mysql_test_data_timezone.sql  > $@;

# Don't update the files from bitkeeper
%::SCCS/s.%
