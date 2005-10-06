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

INCLUDES =		-I$(top_builddir)/include -I$(top_srcdir)/include
noinst_LIBRARIES =	libregex.a
LDADD=			libregex.a $(top_builddir)/strings/libmystrings.a
noinst_HEADERS =	cclass.h cname.h regex2.h utils.h engine.c my_regex.h
libregex_a_SOURCES =	regerror.c regcomp.c regexec.c regfree.c reginit.c
noinst_PROGRAMS =	re
re_SOURCES =		split.c debug.c main.c
re_LDFLAGS= @NOINST_LDFLAGS@
EXTRA_DIST =		tests CHANGES COPYRIGHT WHATSNEW regexp.c \
			debug.ih engine.ih main.ih regcomp.ih regerror.ih \
			regex.3 regex.7

test: re tests
	./re < tests
	./re -el < tests
	./re -er < tests

# Don't update the files from bitkeeper
%::SCCS/s.%
