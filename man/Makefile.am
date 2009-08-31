# Copyright (C) 2000-2001, 2003-2006 MySQL AB
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; version 2
# of the License.
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

## Process this file with automake to create Makefile.in

man1_MANS =   @man1_files@
man8_MANS =   @man8_files@
EXTRA_DIST =  $(man1_MANS) $(man8_MANS)

# 1) not needed in Unix binary packages,
# 2) programs not generated in 5.0:
install-data-hook:
	rm -f $(DESTDIR)$(mandir)/man1/make_win_*
	rm -f $(DESTDIR)$(mandir)/man1/ndb_print_*

# Don't update the files from bitkeeper
%::SCCS/s.%
