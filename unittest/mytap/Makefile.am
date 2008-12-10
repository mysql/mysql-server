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

AM_CPPFLAGS  = -I$(top_builddir)/include -I$(top_srcdir)/include -I$(srcdir)

noinst_LIBRARIES   = libmytap.a
noinst_HEADERS     = tap.h

libmytap_a_SOURCES = tap.c

EXTRA_DIST         = CMakeLists.txt 

SUBDIRS            = . t

# Don't update the files from bitkeeper
%::SCCS/s.%
