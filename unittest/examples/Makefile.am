# Copyright (C) 2006 MySQL AB
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

AM_CPPFLAGS        = -I$(srcdir) -I$(top_builddir)/include \
		     -I$(top_srcdir)/unittest/mytap -I$(top_srcdir)/include

AM_LDFLAGS         = -L$(top_builddir)/unittest/mytap

LDADD              = -lmytap

noinst_PROGRAMS    = simple-t skip-t todo-t skip_all-t no_plan-t core-t

# Don't update the files from bitkeeper
%::SCCS/s.%
