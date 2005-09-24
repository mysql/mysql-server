# Copyright (C) 2000-2004 MySQL AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of version 2 GNU General Public License as
# published by the Free Software Foundation.
#
# There are special exceptions to the terms and conditions of the GPL as it
# is applied to this software. View the full text of the exception in file
# EXCEPTIONS-CLIENT in the directory of this software distribution.
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
#
# This file is public domain and comes with NO WARRANTY of any kind

target = libmysqlclient_r.la
target_defs = -DDONT_USE_RAID -DMYSQL_CLIENT @LIB_EXTRA_CCFLAGS@
LIBS = @LIBS@ @ZLIB_LIBS@ @openssl_libs@

INCLUDES =	@MT_INCLUDES@ \
		-I$(top_builddir)/include -I$(top_srcdir)/include \
		$(openssl_includes) @ZLIB_INCLUDES@

## automake barfs if you don't use $(srcdir) or $(top_srcdir) in include
include $(top_srcdir)/libmysql/Makefile.shared

libmysql_dir = $(top_srcdir)/libmysql

libmysqlclient_r_la_SOURCES = $(target_sources)
libmysqlclient_r_la_LIBADD = $(target_libadd)
libmysqlclient_r_la_LDFLAGS = $(target_ldflags)

# This is called from the toplevel makefile
link_sources:
	set -x; \
	for f in `cd $(libmysql_dir) && echo *.[ch]`; do \
	  rm -f $$f; \
	  @LN_CP_F@ $(libmysql_dir)/$$f $$f; \
	done
