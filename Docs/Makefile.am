# Normally you do not need to remake the files here. But if you want
# to you will need the GNU TeX-info utilities. To make a Postscript
# files you also need TeX and dvips. To make the PDF file you will
# need pdftex. We use the teTeX distribution for all of these.

# We avoid default automake rules because of problems with .dvi file
# and BSD makes

# If you know how to fix any of this more elegantly please mail
# docs@mysql.com

noinst_SCRIPTS =	Support/generate-text-files.pl

EXTRA_DIST =		$(noinst_SCRIPTS) mysql.info INSTALL-BINARY

all:			txt_files

txt_files:		../INSTALL-SOURCE ../COPYING ../INSTALL-WIN-SOURCE ../EXCEPTIONS-CLIENT \
			INSTALL-BINARY ../support-files/MacOSX/ReadMe.txt

CLEAN_FILES:		$(txt_files)
	touch $(txt_files)

GT = $(srcdir)/Support/generate-text-files.pl

../INSTALL-SOURCE:	mysql.info $(GT)
	perl -w $(GT) mysql.info "Installing" "Tutorial" > $@

../INSTALL-WIN-SOURCE:	mysql.info $(GT)
	perl -w $(GT) mysql.info "Windows source build" "Post-installation" > $@

# We put the description for the binary installation here so that
# people who download source wont have to see it. It is moved up to
# the toplevel by the script that makes the binary tar files.
INSTALL-BINARY:	mysql.info $(GT)
	perl -w $(GT) mysql.info "Installing binary" "Installing source" > $@

../EXCEPTIONS-CLIENT: mysql.info $(GT)
	perl -w $(GT) mysql.info "MySQL FLOSS License Exception" "Function Index" > $@

../support-files/MacOSX/ReadMe.txt:	mysql.info $(GT)
	perl -w $(GT) mysql.info "Mac OS X installation" "NetWare installation" > $@

# Don't update the files from bitkeeper
%::SCCS/s.%
