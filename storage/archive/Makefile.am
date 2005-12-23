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

INCLUDES =		-I$(top_builddir)/include -I$(top_srcdir)/include \
			@ZLIB_INCLUDES@

LDADD =			libarchive.a $(top_srcdir)/mysys/libmysys.a \
			$(top_srcdir)/dbug/libdbug.a \
			$(top_srcdir)/strings/libmystrings.a \
			@ZLIB_LIBS@
pkglib_LIBRARIES =	libarchive.a
noinst_PROGRAMS	=	archive_test
archive_test_LDFLAGS = @NOINST_LDFLAGS@
noinst_HEADERS =	azlib.h
libarchive_a_SOURCES =	azio.c

# Don't update the files from bitkeeper
%::SCCS/s.%
