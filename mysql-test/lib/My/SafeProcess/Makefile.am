# Copyright (C) 2000-2006 MySQL AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

testroot = $(prefix)
safedir = $(testroot)/mysql-test/lib/My/SafeProcess
#nobase_bin_PROGRAMS = ...
safe_PROGRAMS =		my_safe_process

my_safe_process_SOURCES = safe_process.cc

EXTRA_DIST = 	safe_kill_win.cc \
		safe_process_win.cc \
		CMakeLists.txt


# Don't update the files from bitkeeper
%::SCCS/s.%
