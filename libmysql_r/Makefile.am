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



target = libmysqlclient_r.la
target_defs =
## LIBS = @LIBS@

INCLUDES =	@MT_INCLUDES@ -I$(srcdir)/../include -I../include \
		-I$(srcdir)/.. -I$(top_srcdir) -I..

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
	  rm -f $(srcdir)/$$f; \
	  @LN_CP_F@ $(libmysql_dir)/$$f $(srcdir)/$$f; \
	done
