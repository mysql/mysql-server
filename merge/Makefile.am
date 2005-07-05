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
pkglib_LIBRARIES =	libmerge.a
noinst_HEADERS =	mrg_def.h
libmerge_a_SOURCES =	mrg_open.c mrg_extra.c mrg_info.c mrg_locking.c \
			mrg_rrnd.c mrg_update.c mrg_delete.c mrg_rsame.c \
			mrg_panic.c mrg_close.c mrg_create.c mrg_static.c

# Don't update the files from bitkeeper
%::SCCS/s.%
