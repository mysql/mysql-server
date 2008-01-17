# Copyright (C) 2000-2006 MySQL AB
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

testdir =		$(prefix)/mysql-test

EXTRA_DIST =		README \
			mysql-test-run.pl \
			mysql-stress-test.pl \
			t r include suite extra \
			lib std_data \
			valgrind.supp

test_SCRIPTS =		mtr mysql-test-run
CLEANFILES =		$(test_SCRIPTS)

SUBDIRS =               lib/My/SafeProcess


# Install all files and files in directories listed in EXTRA_DIST
install-data-local:
	for f in `(cd $(srcdir); find $(EXTRA_DIST) -type f)`; \
	do \
	  d=$(DESTDIR)$(testdir)/`dirname $$f`; \
	  mkdir -p $$d ; \
	  $(INSTALL_DATA) $(srcdir)/$$f $$d ; \
	done

uninstall-local:
	@RM@ -f -r $(DESTDIR)$(testdir)

# mtr - a shortcut for executing mysql-test-run.pl
mtr:
	$(RM) -f mtr
	$(LN_S) mysql-test-run.pl mtr

# mysql-test-run - a shortcut for executing mysql-test-run.pl
mysql-test-run:
	$(RM) -f mysql-test-run
	$(LN_S) mysql-test-run.pl mysql-test-run

# Don't update the files from bitkeeper
%::SCCS/s.%
