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

INCLUDES =		@MT_INCLUDES@ -I$(srcdir)/../include -I../include
noinst_LIBRARIES =	libregex.a
LDADD =			libregex.a ../strings/libmystrings.a
noinst_HEADERS =	cclass.h cname.h regex2.h utils.h engine.c regex.h
libregex_a_SOURCES =	regerror.c regcomp.c regexec.c regfree.c reginit.c
noinst_PROGRAMS =	re
re_SOURCES =		split.c debug.c main.c
re_DEPENDENCIES=	$(LIBRARIES)
re_LDFLAGS= @NOINST_LDFLAGS@
EXTRA_DIST =		tests CHANGES COPYRIGHT WHATSNEW regexp.c \
			debug.ih engine.ih main.ih regcomp.ih regerror.ih \
			regex.3 regex.7

OMIT_DEPENDENCIES =	pthread.h stdio.h __stdio.h stdlib.h __stdlib.h math.h\
			__math.h time.h __time.h unistd.h __unistd.h types.h \
			xtypes.h ac-types.h posix.h string.h __string.h \
			errno.h socket.h inet.h dirent.h netdb.h \
			cleanup.h cond.h debug_out.h fd.h kernel.h mutex.h \
			prio_queue.h pthread_attr.h pthread_once.h queue.h\
			sleep.h specific.h version.h pwd.h timers.h uio.h \
			cdefs.h machdep.h signal.h __signal.h util.h

test: re tests
	./re < tests
	./re -el < tests
	./re -er < tests

# Don't update the files from bitkeeper
%::SCCS/s.%
