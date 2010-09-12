# Copyright (C) 2000 MySQL AB
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
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

noinst_PROGRAMS = mysql
bin_PROGRAMS    = mysqltest_embedded mysql_client_test_embedded
client_sources  = $(nodist_mysqltest_embedded_SOURCES) $(nodist_mysql_SOURCES)
tests_sources   = $(nodist_mysql_client_test_embedded_SOURCES)
BUILT_SOURCES   = link_sources
CLEANFILES      = $(client_sources) $(tests_sources) $(BUILT_SOURCES)
EXTRA_DIST      = CMakeLists.txt

link_sources:
	for f in $(client_sources); do \
	  rm -f $$f; \
	  @LN_CP_F@ $(top_srcdir)/client/$$f $$f; \
	done; \
	for f in $(tests_sources); do \
	  rm -f $$f; \
	  @LN_CP_F@ $(top_srcdir)/tests/$$f $$f; \
	done
	echo timestamp > link_sources

DEFS = 		-DEMBEDDED_LIBRARY
INCLUDES =	-I$(top_builddir)/include -I$(top_srcdir)/include -I$(srcdir) \
		-I$(top_srcdir) -I$(top_srcdir)/client -I$(top_srcdir)/regex \
		$(openssl_includes)
LIBS =		@LIBS@ @WRAPLIBS@ @CLIENT_LIBS@ $(yassl_libs)
LDADD =		@CLIENT_EXTRA_LDFLAGS@ ../libmysqld.a @LIBDL@ $(CXXLDFLAGS) \
                @NDB_SCI_LIBS@

mysqltest_embedded_LINK = $(CXXLINK)
nodist_mysqltest_embedded_SOURCES =	mysqltest.cc
mysqltest_embedded_LDADD =	$(LDADD) $(top_builddir)/regex/libregex.a \
				@MYSQLD_EXTRA_LDFLAGS@

nodist_mysql_SOURCES = mysql.cc readline.cc completion_hash.cc \
                my_readline.h sql_string.h completion_hash.h
mysql_LDADD = @readline_link@ @TERMCAP_LIB@ $(LDADD)

mysql_client_test_embedded_LINK = $(CXXLINK)
nodist_mysql_client_test_embedded_SOURCES = mysql_client_test.c

# Don't update the files from bitkeeper
%::SCCS/s.%
