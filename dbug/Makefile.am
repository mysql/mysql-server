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

INCLUDES =              @MT_INCLUDES@ -I$(top_srcdir)/include
LDADD =                 libdbug.a ../mysys/libmysys.a ../strings/libmystrings.a
pkglib_LIBRARIES =      libdbug.a
noinst_HEADERS =        dbug_long.h
libdbug_a_SOURCES =     dbug.c sanity.c
EXTRA_DIST =            example1.c example2.c example3.c \
                        user.r monty.doc readme.prof dbug_add_tags.pl \
                        my_main.c main.c factorial.c dbug_analyze.c
NROFF_INC =             example1.r example2.r example3.r main.r \
                        factorial.r output1.r output2.r output3.r \
                        output4.r output5.r


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

clean:
		@RM@ -f $(NROFF_INC) user.t user.ps

# Don't update the files from bitkeeper
%::SCCS/s.%
