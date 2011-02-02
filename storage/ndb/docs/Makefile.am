# Copyright (C) 2004-2005 MySQL AB
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

DOXYDIR = doxygen
noinst_HEADERS = $(DOXYDIR)/predoxy.pl $(DOXYDIR)/postdoxy.pl $(DOXYDIR)/Doxyfile.ndbapi $(DOXYDIR)/Doxyfile.mgmapi

all-local: 	do-check-html ndbapidoc-html mgmapidoc-html
all-pdf: 	do-check-pdf ndbapidoc-pdf mgmapidoc-pdf

DOXYTMP =	.doxytmp
DOXYOUT =	.doxyout

NDB_RELEASE = @NDB_VERSION_MAJOR@.@NDB_VERSION_MINOR@.@NDB_VERSION_BUILD@-@NDB_VERSION_STATUS@

clean-local:
	rm -rf ndbapi.pdf ndbapi.html mgmapi.pdf mgmapi.html
	rm -rf $(DOXYTMP) $(DOXYOUT)

do-check-html:
	@set -x; \
	if test @PERL@ = no ; then \
	  echo "Perl needed to make docs"; \
	  exit 1; \
	fi; \
	if test @DOXYGEN@ = no ; then \
	  echo "Doxygen needed to make docs"; \
	  exit 1; \
	fi;

do-check-pdf: do-check-html
	if test @PDFLATEX@ = no ; then \
	  echo "Pdflatex needed to make docs"; \
	  exit 1; \
	fi; \
	if test @MAKEINDEX@ = no ; then \
	  echo "Makeindex needed to make docs"; \
	  exit 1; \
	fi;

###
#
#  NDB API Programmer's Guide
#
ndbapidoc-html: ndbapi.html
ndbapidoc-pdf: ndbapi.pdf

ndbapi.html: $(noinst_HEADERS)
	@set -x; \
	export NDB_RELEASE=$(NDB_RELEASE); \
	@RM@ -f ndbapi.pdf ndbapi.html; \
	@RM@ -rf $(DOXYTMP) $(DOXYOUT); \
	mkdir -p $(DOXYTMP) $(DOXYOUT); \
	@CP@ $(top_srcdir)/storage/ndb/include/ndbapi/*  $(DOXYTMP); \
	@CP@ $(top_srcdir)/storage/ndb/ndbapi-examples/*/*.[ch]pp $(DOXYTMP); \
	@PERL@ $(DOXYDIR)/predoxy.pl; \
	mv footer.html $(DOXYTMP); \
	(cd $(DOXYTMP) ; @DOXYGEN@ ../$(DOXYDIR)/Doxyfile.ndbapi); \
	@PERL@ $(DOXYDIR)/postdoxy.pl $(DOXYOUT)/ndbapi.latex "MySQL Cluster NDB API Programmer Guide"; \
	(cd $(DOXYOUT) && \
		find ndbapi.html -print | cpio -pdm ..);

ndbapi.pdf: ndbapi.html
	(cd $(DOXYOUT)/ndbapi.latex && \
		@PDFLATEX@ refman.tex && @MAKEINDEX@ refman && @PDFLATEX@ refman.tex && \
		cp -p refman.pdf ../../ndbapi.pdf);

###
#
#  MGM API Guide
#
mgmapidoc-html: mgmapi.html
mgmapidoc-pdf: mgmapi.pdf

mgmapi.html: $(noinst_HEADERS)
	@set -x; \
	export NDB_RELEASE=$(NDB_RELEASE); \
	@RM@ -f mgmapi.pdf mgmapi.html; \
	@RM@ -rf $(DOXYTMP) $(DOXYOUT); \
	mkdir -p $(DOXYTMP) $(DOXYOUT); \
	@CP@ $(top_srcdir)/storage/ndb/include/mgmapi/*  $(DOXYTMP); \
	@PERL@ $(DOXYDIR)/predoxy.pl; \
	mv footer.html $(DOXYTMP); \
	(cd $(DOXYTMP) ; @DOXYGEN@ ../$(DOXYDIR)/Doxyfile.mgmapi); \
	@PERL@ $(DOXYDIR)/postdoxy.pl $(DOXYOUT)/mgmapi.latex "MySQL Cluster MGM API Guide"; \
	(cd $(DOXYOUT) && \
		find mgmapi.html -print | cpio -pdm ..);

mgmapi.pdf: mgmapi.html
	(cd $(DOXYOUT)/mgmapi.latex && \
		@PDFLATEX@ refman.tex && @MAKEINDEX@ refman && @PDFLATEX@ refman.tex && \
		cp -p refman.pdf ../../mgmapi.pdf);

###
#  
#  Complete Source Browser except for 
#        ndbapi odbc test tools win32 lib examples docs CVS config bin 
#        include/ndbapi 
#        include/newtonapi src/newtonapi
#        include/mgmapi src/mgmapi
#        src/client
ndbdoc: DUMMY
	mkdir -p $(OUTDIR)
	cd $(top_srcdir)/storage/ndb ; $(DOXYGEN) $(DOXYDIR)/Doxyfile.ndb

###
#
#  odbcdoc - Complete Source Browser for NDB ODBC (src/client/odbc)

odbcdoc: DUMMY
	mkdir -p $(OUTDIR)
	cd $(top_srcdir)/storage/ndb ; $(DOXYGEN) $(DOXYDIR)/Doxyfile.odbc

testdoc: DUMMY
	mkdir -p $(OUTDIR)
	cd $(top_srcdir)/storage/ndb ; $(DOXYGEN) $(DOXYDIR)/Doxyfile.test

windoze-dsp:
