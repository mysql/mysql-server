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
target_defs = -DDONT_USE_RAID
## LIBS = @LIBS@

INCLUDES =	@MT_INCLUDES@ -I$(srcdir)/../include -I../include \
		-I$(srcdir)/.. -I$(top_srcdir) -I..

## automake barfs if you don't use $(srcdir) or $(top_srcdir) in include
include $(top_srcdir)/libmysql/Makefile.shared

# Don't depend on files in pthread library
OMIT_DEPENDENCIES =	pthread.h stdio.h __stdio.h stdlib.h __stdlib.h math.h\
			__math.h time.h __time.h unistd.h __unistd.h types.h \
			xtypes.h ac-types.h posix.h string.h __string.h \
			errno.h socket.h inet.h dirent.h netdb.h \
			cleanup.h cond.h debug_out.h fd.h kernel.h mutex.h \
			prio_queue.h pthread_attr.h pthread_once.h queue.h\
			sleep.h specific.h version.h pwd.h timers.h uio.h \
			cdefs.h machdep.h signal.h __signal.h util.h lex.h \
			wait.h

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
