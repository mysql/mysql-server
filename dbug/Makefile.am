# Copyright (C) 2000, 2002, 2004-2006 MySQL AB
#  All rights reserved. Use is subject to license terms.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
# 
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the Free
# Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
# MA 02110-1301, USA


INCLUDES =              -I$(top_builddir)/include -I$(top_srcdir)/include
LDADD =                 libdbuglt.la $(top_builddir)/mysys/libmysyslt.la \
			$(top_builddir)/strings/libmystringslt.la ${ZLIB_LIBS}
pkglib_LTLIBRARIES =	libdbug.la
libdbug_la_LDFLAGS =   -static
libdbug_la_SOURCES = 	
libdbug_la_LIBADD =	libdbuglt.la
# Force C++ linking - dummy.cxx doesn't have to exist with EXTRA in the name
nodist_EXTRA_libdbug_la_SOURCES = dummy.cxx

noinst_LTLIBRARIES =	libdbuglt.la
noinst_HEADERS =        dbug_long.h
libdbuglt_la_SOURCES =     dbug.c sanity.c
EXTRA_DIST =            CMakeLists.txt example1.c example2.c example3.c \
                        user.r monty.doc dbug_add_tags.pl \
                        my_main.c main.c factorial.c dbug_analyze.c \
						CMakeLists.txt
NROFF_INC =             example1.r example2.r example3.r main.r \
                        factorial.r output1.r output2.r output3.r \
                        output4.r output5.r
CLEANFILES =            $(NROFF_INC) user.t user.ps


# Must be linked with libs that are not compiled yet
noinst_PROGRAMS =       factorial dbug_analyze
factorial_SOURCES =     my_main.c factorial.c
dbug_analyze_SOURCES =  dbug_analyze.c

all:            user.t user.ps

user.t:         user.r $(NROFF_INC)
		-nroff -mm user.r > $@

user.ps:	user.r $(NROFF_INC)
		-groff -mm user.r > $@

output1.r:      factorial
		./factorial 1 2 3 4 5 | cat > $@

output2.r:      factorial
		./factorial -\#t:o 2 3 | cat >$@

output3.r:      factorial
		./factorial -\#d:t:o 3 | cat >$@

output4.r:      factorial
		./factorial -\#d,result:o 4 | cat >$@

output5.r:      factorial
		./factorial -\#d:f,factorial:F:L:o 3 | cat >$@
.c.r:
		@RM@ -f $@
		@SED@ -e 's!\\!\\\\!g' $< > $@

# Don't update the files from bitkeeper
%::SCCS/s.%
