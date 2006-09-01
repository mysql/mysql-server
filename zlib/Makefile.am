# Copyright (C) 2004 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
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

# Process this file with automake to create Makefile.in

INCLUDES=		-I$(top_builddir)/include -I$(top_srcdir)/include

pkglib_LTLIBRARIES=libz.la

libz_la_LDFLAGS= -version-info 3:3:2

noinst_HEADERS= crc32.h deflate.h inffast.h inffixed.h inflate.h \
		inftrees.h trees.h zconf.h zlib.h zutil.h

libz_la_SOURCES= adler32.c compress.c crc32.c deflate.c gzio.c \
		 infback.c inffast.c inflate.c inftrees.c trees.c \
		 uncompr.c zutil.c

EXTRA_DIST= README FAQ INDEX ChangeLog algorithm.txt zlib.3 CMakeLists.txt

