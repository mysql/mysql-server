include .defs.mk
#
# hack before full autoconf
replace-targets := all clean
first-docs: all

include $(NDB_TOP)/Epilogue.mk

all: 	ndbapidoc mgmapidoc

DOXYGEN =	doxygen
DOXYTOP =	$(shell cd $(NDB_TOP); pwd)/docs
DOXYDIR =	$(DOXYTOP)/doxygen
DOXYTMP =	$(DOXYTOP)/.doxytmp
DOXYOUT =	$(DOXYTOP)/.doxyout

clean:
	rm -rf ndbapi.pdf ndbapi.html mgmapi.pdf mgmapi.html
	rm -rf $(DOXYTMP) $(DOXYOUT)

###
#
#  NDB API Programmer's Guide
#
ndbapidoc: ndbapi.pdf

ndbapi.pdf: $(NDB_TOP)/include/ndb_version.h
	@set -x; \
	rm -rf ndbapi.pdf ndbapi.html; \
	rm -rf $(DOXYTMP) $(DOXYOUT); \
	mkdir -p $(DOXYTMP) $(DOXYOUT); \
	(cd $(NDB_TOP)/include/ndbapi && \
		find . -type f -print | \
		grep -v /SCCS | \
		cpio -pdm $(DOXYTMP)); \
	(cd $(NDB_TOP)/examples && \
		cp -p */*.[ch]pp $(DOXYTMP)); \
	$(DOXYDIR)/predoxy.pl; \
	mv footer.html $(DOXYTMP); \
	(cd $(DOXYTMP) && \
		$(DOXYGEN) $(DOXYDIR)/Doxyfile.ndbapi); \
	$(DOXYDIR)/postdoxy.pl $(DOXYOUT)/ndbapi.latex "NDB API Programmer Guide"; \
	(cd $(DOXYOUT) && \
		find ndbapi.html -print | cpio -pdm $(DOXYTOP)); \
	(cd $(DOXYOUT)/ndbapi.latex && \
		pdflatex refman.tex && makeindex refman && pdflatex refman.tex && \
		cp -p refman.pdf $(DOXYTOP)/ndbapi.pdf);

###
#
#  MGM API Guide
#
mgmapidoc: mgmapi.pdf

mgmapi.pdf: $(NDB_TOP)/include/ndb_version.h
	@set -x; \
	rm -rf mgmapi.pdf mgmapi.html; \
	rm -rf $(DOXYTMP) $(DOXYOUT); \
	mkdir -p $(DOXYTMP) $(DOXYOUT); \
	(cd $(NDB_TOP)/include/mgmapi && \
		find . -type f -print | \
		grep -v /SCCS | \
		cpio -pdm $(DOXYTMP)); \
	$(DOXYDIR)/predoxy.pl; \
	mv footer.html $(DOXYTMP); \
	(cd $(DOXYTMP) && \
		$(DOXYGEN) $(DOXYDIR)/Doxyfile.mgmapi); \
	$(DOXYDIR)/postdoxy.pl $(OUTDIR)/mgmapi.latex "NDB Cluster MGM API Guide"; \
	(cd $(DOXYOUT) && \
		find mgmapi.html -print | cpio -pdm $(DOXYTOP)); \
	(cd $(DOXYOUT)/mgmapi.latex && \
		pdflatex refman.tex && makeindex refman && pdflatex refman.tex && \
		cp -p refman.pdf $(DOXYTOP)/mgmapi.pdf);

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
	cd $(NDB_TOP) ; $(DOXYGEN) $(DOXYDIR)/Doxyfile.ndb

###
#
#  odbcdoc - Complete Source Browser for NDB ODBC (src/client/odbc)

odbcdoc: DUMMY
	mkdir -p $(OUTDIR)
	cd $(NDB_TOP) ; $(DOXYGEN) $(DOXYDIR)/Doxyfile.odbc

testdoc: DUMMY
	mkdir -p $(OUTDIR)
	cd $(NDB_TOP) ; $(DOXYGEN) $(DOXYDIR)/Doxyfile.test
