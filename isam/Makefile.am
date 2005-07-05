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

INCLUDES =		@MT_INCLUDES@ \
			-I$(top_builddir)/include -I$(top_srcdir)/include 
LDADD =			@CLIENT_EXTRA_LDFLAGS@ libnisam.a ../mysys/libmysys.a \
			../dbug/libdbug.a ../strings/libmystrings.a
pkglib_LIBRARIES =	libnisam.a
bin_PROGRAMS =		isamchk isamlog pack_isam
isamchk_DEPENDENCIES=	$(LIBRARIES)
isamlog_DEPENDENCIES=	$(LIBRARIES)
pack_isam_DEPENDENCIES=	$(LIBRARIES)
noinst_PROGRAMS =	test1 test2 test3
noinst_HEADERS =	isamdef.h
test1_DEPENDENCIES=	$(LIBRARIES)
test2_DEPENDENCIES=	$(LIBRARIES)
test3_DEPENDENCIES=	$(LIBRARIES)
libnisam_a_SOURCES =	open.c extra.c info.c rkey.c rnext.c \
			_search.c _page.c _key.c _locking.c \
			rrnd.c _cache.c _statrec.c _packrec.c \
			_dynrec.c update.c write.c delete.c \
			rprev.c rfirst.c rlast.c rsame.c rsamepos.c \
			panic.c close.c create.c range.c _dbug.c \
			log.c changed.c static.c
isamchk_SOURCES =	isamchk.c sort.c
CLEANFILES =		test?.IS? isam.log

# Move to automake rules ?
prolint:;	plparse -b -u -hF1 "-width(0,0)" "-format=%f:%l:\s%t:%n\s%m" \
		"-elib(????)" "+elib(?3??)" my.lnt $(nisam_SOURCES)

# Don't update the files from bitkeeper
%::SCCS/s.%
