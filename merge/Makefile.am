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
noinst_HEADERS =	mrgdef.h
libmerge_a_SOURCES =	open.c extra.c info.c _locking.c \
			rrnd.c update.c delete.c rsame.c panic.c \
			close.c create.c static.c

# Don't update the files from bitkeeper
%::SCCS/s.%
