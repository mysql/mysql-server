# Copyright (C) 2000-2003, 2005 MySQL AB
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

#
# As pstack doesn't work on all configurations, we have to use
# the USE_PSTACK hack to get all files into distribution
#

SUBDIRS =		aout

INCLUDES =		-I$(top_builddir)/include -I$(top_srcdir)/include

pkglib_LIBRARIES =	libpstack.a
libpstack_a_SOURCES =	bucomm.c filemode.c linuxthreads.c rddbg.c \
			debug.c ieee.c pstack.c stabs.c
noinst_HEADERS =	bucomm.h debug.h ieee.h budbg.h demangle.h \
			linuxthreads.h pstack.h pstacktrace.h

# Don't update the files from bitkeeper
%::SCCS/s.%
