DOXYDIR = doxygen
noinst_HEADERS = $(DOXYDIR)/predoxy.pl $(DOXYDIR)/postdoxy.pl $(DOXYDIR)/Doxyfile.ndbapi $(DOXYDIR)/Doxyfile.mgmapi $(DOXYDIR)/header.ndbapi.tex $(DOXYDIR)/header.mgmapi.tex

all: 	do-check ndbapidoc mgmapidoc

DOXYTMP =	.doxytmp
DOXYOUT =	.doxyout

NDB_RELEASE = @NDB_VERSION_MAJOR@.@NDB_VERSION_MINOR@.@NDB_VERSION_BUILD@-@NDB_VERSION_STATUS@

clean:
	rm -rf ndbapi.pdf ndbapi.html mgmapi.pdf mgmapi.html
	rm -rf $(DOXYTMP) $(DOXYOUT)

do-check:
	@set -x; \
	if test @PERL@ = no ; then \
	  echo "Perl needed to make docs"; \
	  exit 1; \
	fi; \
	if test @DOXYGEN@ = no ; then \
	  echo "Doxygen needed to make docs"; \
	  exit 1; \
	fi; \
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
ndbapidoc: ndbapi.pdf

ndbapi.pdf: $(noinst_HEADERS)
	@set -x; \
	export NDB_RELEASE=$(NDB_RELEASE) \
	@RM@ -f ndbapi.pdf ndbapi.html; \
	@RM@ -rf $(DOXYTMP) $(DOXYOUT); \
	mkdir -p $(DOXYTMP) $(DOXYOUT); \
	@CP@ $(top_srcdir)/ndb/include/ndbapi/*  $(DOXYTMP); \
	@CP@ $(top_srcdir)/ndb/examples/*/*.[ch]pp $(DOXYTMP); \
	@PERL@ $(DOXYDIR)/predoxy.pl; \
	mv footer.html $(DOXYTMP); \
	(cd $(DOXYTMP) ; @DOXYGEN@ ../$(DOXYDIR)/Doxyfile.ndbapi); \
	@PERL@ $(DOXYDIR)/postdoxy.pl $(DOXYOUT)/ndbapi.latex "NDB API Programmer Guide"; \
	(cd $(DOXYOUT) && \
		find ndbapi.html -print | cpio -pdm ..); \
	(cd $(DOXYOUT)/ndbapi.latex && \
		@PDFLATEX@ refman.tex && @MAKEINDEX@ refman && @PDFLATEX@ refman.tex && \
		cp -p refman.pdf ../../ndbapi.pdf);

###
#
#  MGM API Guide
#
mgmapidoc: mgmapi.pdf

mgmapi.pdf: $(noinst_HEADERS)
	@set -x; \
	export NDB_RELEASE=$(NDB_RELEASE) \
	@RM@ -f mgmapi.pdf mgmapi.html; \
	@RM@ -rf $(DOXYTMP) $(DOXYOUT); \
	mkdir -p $(DOXYTMP) $(DOXYOUT); \
	@CP@ $(top_srcdir)/ndb/include/mgmapi/*  $(DOXYTMP); \
	@PERL@ $(DOXYDIR)/predoxy.pl; \
	mv footer.html $(DOXYTMP); \
	(cd $(DOXYTMP) ; @DOXYGEN@ ../$(DOXYDIR)/Doxyfile.mgmapi); \
	@PERL@ $(DOXYDIR)/postdoxy.pl $(DOXYOUT)/mgmapi.latex "NDB Cluster MGM API Guide"; \
	(cd $(DOXYOUT) && \
		find mgmapi.html -print | cpio -pdm ..); \
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
	cd $(top_srcdir)/ndb ; $(DOXYGEN) $(DOXYDIR)/Doxyfile.ndb

###
#
#  odbcdoc - Complete Source Browser for NDB ODBC (src/client/odbc)

odbcdoc: DUMMY
	mkdir -p $(OUTDIR)
	cd $(top_srcdir)/ndb ; $(DOXYGEN) $(DOXYDIR)/Doxyfile.odbc

testdoc: DUMMY
	mkdir -p $(OUTDIR)
	cd $(top_srcdir)/ndb ; $(DOXYGEN) $(DOXYDIR)/Doxyfile.test

windoze-dsp:
