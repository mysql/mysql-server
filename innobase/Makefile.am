# Copyright (C) 2001, 2004, 2006 MySQL AB & Innobase Oy
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

# Process this file with automake to create Makefile.in

AUTOMAKE_OPTIONS =	foreign
TAR =			gtar

noinst_HEADERS = ib_config.h

SUBDIRS =		os ut btr buf data dict dyn eval fil fsp fut \
			ha ibuf include lock log mach mem mtr page \
			pars que read rem row srv sync thr trx usr
EXTRA_DIST = CMakeLists.txt

# Don't update the files from bitkeeper
%::SCCS/s.%
