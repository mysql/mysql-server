# Copyright (C) 2000-2004 MySQL AB
#  All rights reserved. Use is subject to license terms.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
#
#
#
#
# The lines above are intentionally left blank
#
# This file is public domain and comes with NO WARRANTY of any kind

target = libmysqlclient_r.la
target_defs = -DDONT_USE_RAID -DMYSQL_CLIENT @LIB_EXTRA_CCFLAGS@
LIBS = @LIBS@ @ZLIB_LIBS@ @openssl_libs@

INCLUDES =	-I$(top_builddir)/include -I$(top_srcdir)/include \
		$(openssl_includes) @ZLIB_INCLUDES@
## automake barfs if you don't use $(srcdir) or $(top_srcdir) in include
include $(top_srcdir)/libmysql/Makefile.shared

nodist_libmysqlclient_r_la_SOURCES = $(target_sources)
libmysqlclient_r_la_LIBADD = $(target_libadd) $(yassl_las)
libmysqlclient_r_la_LDFLAGS = $(target_ldflags)

link_sources:
	set -x; \
	for d in $(top_srcdir)/libmysql $(top_builddir)/libmysql; do \
	  for f in `cd $$d && echo *.[ch]`; do \
	    rm -f $$f; \
	    @LN_CP_F@ $$d/$$f $$f; \
	  done; \
	done
	echo timestamp > link_sources

# Don't update the files from bitkeeper
%::SCCS/s.%
