# Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the Free
# Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
# MA 02111-1307, USA

## Process this file with automake to create Makefile.in

if HAVE_YASSL
  yassl_dummy_link_fix= $(top_srcdir)/extra/yassl/src/dummy.cpp
else
  yassl_dummy_link_fix=
endif
EXTRA_DIST =		auto_increment.res auto_increment.tst \
			function.res function.tst lock_test.pl lock_test.res \
			export.pl big_record.pl \
			fork2_test.pl fork_big.pl \
			insert_and_repair.pl \
			grant.pl grant.res test_delayed_insert.pl \
			pmail.pl mail_to_db.pl table_types.pl \
			udf_test udf_test.res myisam-big-rows.tst

bin_PROGRAMS =		mysql_client_test
noinst_PROGRAMS =	insert_test select_test thread_test

#
# C Test for 4.1 protocol
#
INCLUDES =		-I$(top_builddir)/include -I$(top_srcdir)/include \
			$(openssl_includes)
LIBS =			@CLIENT_LIBS@
LDADD =			@CLIENT_EXTRA_LDFLAGS@ \
                        $(top_builddir)/libmysql/libmysqlclient.la
if HAVE_NETWARE
mysql_client_test_LDADD= $(LDADD) $(CXXLDFLAGS)
mysql_client_test_SOURCES= mysql_client_test.c $(yassl_dummy_link_fix) \
			../mysys/my_memmem.c
else
mysql_client_test_LDADD= $(LDADD) $(CXXLDFLAGS) -L../mysys -lmysys
mysql_client_test_SOURCES= mysql_client_test.c $(yassl_dummy_link_fix)
endif
insert_test_SOURCES=       insert_test.c $(yassl_dummy_link_fix)
select_test_SOURCES=       select_test.c $(yassl_dummy_link_fix)
insert_test_DEPENDENCIES=	$(LIBRARIES) $(pkglib_LTLIBRARIES)
select_test_DEPENDENCIES=	$(LIBRARIES) $(pkglib_LTLIBRARIES)

# Fix for mit-threads
DEFS =			-DUNDEF_THREADS_HACK

thread_test.o:		thread_test.c
			$(COMPILE) -c $(INCLUDES) $<

# Don't update the files from bitkeeper
%::SCCS/s.%
