# Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
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

INCLUDES =		@MT_INCLUDES@ -I$(srcdir)/../include -I../include
pkglib_LIBRARIES =	libmerge.a
noinst_HEADERS =	mrg_def.h
libmerge_a_SOURCES =	mrg_open.c mrg_extra.c mrg_info.c mrg_locking.c \
			mrg_rrnd.c mrg_update.c mrg_delete.c mrg_rsame.c \
			mrg_panic.c mrg_close.c mrg_create.c mrg_static.c

OMIT_DEPENDENCIES =	pthread.h stdio.h __stdio.h stdlib.h __stdlib.h math.h\
			__math.h time.h __time.h unistd.h __unistd.h types.h \
			xtypes.h ac-types.h posix.h string.h __string.h \
			errno.h socket.h inet.h dirent.h netdb.h \
			cleanup.h cond.h debug_out.h fd.h kernel.h mutex.h \
			prio_queue.h pthread_attr.h pthread_once.h queue.h\
			sleep.h specific.h version.h pwd.h timers.h uio.h \
			cdefs.h machdep.h signal.h __signal.h util.h

# Don't update the files from bitkeeper
%::SCCS/s.%
