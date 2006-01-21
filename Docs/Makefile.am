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

noinst_SCRIPTS =	Support/generate-text-files.pl

EXTRA_DIST =		$(noinst_SCRIPTS) mysql.info INSTALL-BINARY

TXT_FILES=		../INSTALL-SOURCE ../INSTALL-WIN-SOURCE ../EXCEPTIONS-CLIENT \
			INSTALL-BINARY ../support-files/MacOSX/ReadMe.txt

all-local:		$(TXT_FILES)

# make sure that "make install" installs the info page, too
# automake only seems to take care of this automatically,
# if we're building the info page from texi directly.
install-data-hook:	mysql.info
	$(mkinstalldirs) $(DESTDIR)$(infodir)
	$(INSTALL_DATA) $(srcdir)/mysql.info $(DESTDIR)$(infodir)

uninstall-local:
	@RM@ -f $(DESTDIR)$(infodir)/mysql.info

# This target is not used in builds, just for convinience
CLEAN_FILES:		$(TXT_FILES)
	touch $(TXT_FILES)

GT = $(srcdir)/Support/generate-text-files.pl

../INSTALL-SOURCE:	mysql.info $(GT)
	perl -w $(GT) $< "installing-source" "windows-source-build" > $@

../INSTALL-WIN-SOURCE:	mysql.info $(GT)
	perl -w $(GT) $< "windows-source-build" "post-installation" > $@

# We put the description for the binary installation here so that
# people who download source wont have to see it. It is moved up to
# the toplevel by the script that makes the binary tar files.
INSTALL-BINARY:	mysql.info $(GT)
	perl -w $(GT) $< "installing-binary" "installing-source" > $@

../EXCEPTIONS-CLIENT: mysql.info $(GT)
	perl -w $(GT) $< "mysql-floss-license-exception" "function-index" > $@

../support-files/MacOSX/ReadMe.txt:	mysql.info $(GT)
	perl -w $(GT) $< "mac-os-x-installation" "netware-installation" > $@

# Don't update the files from bitkeeper
%::SCCS/s.%
