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

# This file is public domain and comes with NO WARRANTY of any kind

INCLUDES =		@MT_INCLUDES@ -I$(top_srcdir)/include
pkglib_LIBRARIES =	libmystrings.a

# Exact one of ASSEMBLER_X
if ASSEMBLER_x86
ASRCS		= strings-x86.s longlong2str-x86.s my_strtoll10-x86.s
CSRCS		= bfill.c bmove.c bmove512.c bchange.c strxnmov.c int2str.c str2int.c r_strinstr.c strtod.c bcmp.c strtol.c strtoul.c strtoll.c strtoull.c llstr.c strnlen.c ctype.c ctype-simple.c ctype-mb.c ctype-big5.c ctype-czech.c ctype-euc_kr.c ctype-gb2312.c ctype-gbk.c ctype-sjis.c ctype-tis620.c ctype-ujis.c ctype-utf8.c ctype-ucs2.c ctype-uca.c ctype-win1250ch.c ctype-bin.c ctype-latin1.c my_vsnprintf.c xml.c ctype-extra.c
else
if ASSEMBLER_sparc32
# These file MUST all be on the same line!! Otherwise automake
# generats a very broken makefile
ASRCS		= bmove_upp-sparc.s strappend-sparc.s strend-sparc.s strinstr-sparc.s strmake-sparc.s strmov-sparc.s strnmov-sparc.s strstr-sparc.s
CSRCS		= strcont.c strfill.c strcend.c is_prefix.c longlong2str.c bfill.c bmove.c bmove512.c bchange.c strxnmov.c int2str.c str2int.c r_strinstr.c strtod.c bcmp.c strtol.c strtoul.c strtoll.c strtoull.c llstr.c strnlen.c strxmov.c ctype.c ctype-simple.c ctype-mb.c ctype-big5.c ctype-czech.c ctype-euc_kr.c ctype-gb2312.c ctype-gbk.c ctype-sjis.c ctype-tis620.c ctype-ujis.c ctype-utf8.c ctype-ucs2.c ctype-uca.c ctype-win1250ch.c ctype-bin.c ctype-latin1.c my_vsnprintf.c xml.c ctype-extra.c my_strtoll10.c
else
#no assembler
ASRCS		=
# These file MUST all be on the same line!! Otherwise automake
# generats a very broken makefile
CSRCS		= strxmov.c bmove_upp.c strappend.c strcont.c strend.c strfill.c strcend.c is_prefix.c strstr.c strinstr.c strmake.c strnmov.c strmov.c longlong2str.c bfill.c bmove.c bmove512.c bchange.c strxnmov.c int2str.c str2int.c r_strinstr.c strtod.c bcmp.c strtol.c strtoul.c strtoll.c strtoull.c llstr.c strnlen.c ctype.c ctype-simple.c ctype-mb.c ctype-big5.c ctype-czech.c ctype-euc_kr.c ctype-gb2312.c ctype-gbk.c ctype-sjis.c ctype-tis620.c ctype-ujis.c ctype-utf8.c ctype-ucs2.c ctype-uca.c ctype-win1250ch.c ctype-bin.c ctype-latin1.c my_vsnprintf.c xml.c ctype-extra.c my_strtoll10.c
endif
endif

libmystrings_a_SOURCES = $(ASRCS) $(CSRCS)
noinst_PROGRAMS = conf_to_src
DISTCLEANFILES = ctype_autoconf.c
# Default charset definitions
EXTRA_DIST =		ctype-big5.c ctype-czech.c ctype-euc_kr.c ctype-win1250ch.c \
                        ctype-gb2312.c ctype-gbk.c ctype-sjis.c ctype-utf8.c \
                        ctype-ucs2.c ctype-uca.c ctype-tis620.c ctype-ujis.c \
                        xml.c strto.c strings-x86.s \
			longlong2str.c longlong2str-x86.s \
			my_strtoll10.c my_strtoll10-x86.s \
			strxmov.c bmove_upp.c strappend.c strcont.c strend.c \
			strfill.c strcend.c is_prefix.c strstr.c strinstr.c \
			strmake.c strnmov.c strmov.c strnlen.c \
			bmove_upp-sparc.s strappend-sparc.s strend-sparc.s \
			strinstr-sparc.s strmake-sparc.s strmov-sparc.s \
			strnmov-sparc.s strstr-sparc.s strxmov-sparc.s \
                        t_ctype.h

libmystrings_a_LIBADD=
conf_to_src_SOURCES = conf_to_src.c xml.c ctype.c bcmp.c
conf_to_src_LDADD=
#force static linking of conf_to_src - essential when linking against
#custom installation of libc
conf_to_src_LDFLAGS= @NOINST_LDFLAGS@

# This is because the dependency tracking misses @FOO@ vars in sources.
#strtoull.o:		@CHARSET_OBJS@


if ASSEMBLER
# On Linux gcc can compile the assembly files
%.o	: %.s
	$(AS) $(ASFLAGS) -o $@ $<
endif

str_test: str_test.c $(LIBRARIES)
	$(LINK) $(FLAGS) -DMAIN $INCLUDES $(srcdir)/str_test.c $(LDADD) $(LIBS) $(pkglib_LIBRARIES)

# Don't update the files from bitkeeper
%::SCCS/s.%
