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

INCLUDES =		-I$(srcdir)/../include -I../include
pkglib_LIBRARIES =	libpstack.a
noinst_HEADERS =	bucomm.h debug.h ieee.h budbg.h demangle.h \
			linuxthreads.h pstack.h pstacktrace.h
libpstack_a_SOURCES =	bucomm.c filemode.c linuxthreads.c rddbg.c \
			debug.c ieee.c pstack.c stabs.c

# Don't update the files from bitkeeper
%::SCCS/s.%
