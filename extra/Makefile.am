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

INCLUDES =		@MT_INCLUDES@ -I$(top_srcdir)/include \
			@ndbcluster_includes@ -I$(top_srcdir)/sql
LDADD =			@CLIENT_EXTRA_LDFLAGS@ ../mysys/libmysys.a \
			../dbug/libdbug.a ../strings/libmystrings.a
bin_PROGRAMS =		replace comp_err perror resolveip my_print_defaults \
			resolve_stack_dump mysql_waitpid
noinst_PROGRAMS =	charset2html

# Don't update the files from bitkeeper
%::SCCS/s.%
