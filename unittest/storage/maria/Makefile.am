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

AM_CPPFLAGS      = @ZLIB_INCLUDES@ -I$(top_builddir)/include 
AM_CPPFLAGS     += -I$(top_srcdir)/include -I$(top_srcdir)/unittest/mytap

# Only reason to link with libmyisam.a here is that it's where some fulltext
# pieces are (but soon we'll remove fulltext dependencies from Maria).
LDADD=			$(top_builddir)/unittest/mytap/libmytap.a \
			$(top_builddir)/storage/maria/libmaria.a \
                        $(top_builddir)/storage/myisam/libmyisam.a \
			$(top_builddir)/mysys/libmysys.a \
			$(top_builddir)/dbug/libdbug.a \
			$(top_builddir)/strings/libmystrings.a @ZLIB_LIBS@
noinst_PROGRAMS =	ma_control_file-t
CLEANFILES =		maria_control
