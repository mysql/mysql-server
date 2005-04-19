# Copyright (C) 2000 MySQL AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

noinst_PROGRAMS =	mysqltest mysql
client_sources =	$(mysqltest_SOURCES) $(mysql_SOURCES)

link_sources:
	for f in $(client_sources); do \
	  rm -f $(srcdir)/$$f; \
	  @LN_CP_F@ $(srcdir)/../../client/$$f $(srcdir)/$$f; \
	done;

DEFS = 		-DEMBEDDED_LIBRARY
INCLUDES =	@MT_INCLUDES@ -I$(top_srcdir)/include -I$(srcdir) \
		-I$(top_srcdir) -I$(top_srcdir)/client $(openssl_includes)
LIBS =		@LIBS@ @WRAPLIBS@
LDADD =		@CLIENT_EXTRA_LDFLAGS@ ../libmysqld.a @innodb_system_libs@ @LIBDL@ $(CXXLDFLAGS)

mysqltest_LINK = $(CXXLINK)

mysqltest_SOURCES =	mysqltest.c

mysql_SOURCES = mysql.cc readline.cc completion_hash.cc \
                my_readline.h sql_string.h completion_hash.h
mysql_LDADD = @readline_link@ @TERMCAP_LIB@ $(LDADD)

clean:
	rm -f $(client_sources)

# Don't update the files from bitkeeper
%::SCCS/s.%
