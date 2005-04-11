## Process this file with automake to create Makefile.in

EXTRA_DIST= errmsg.txt

dist-hook:
	for dir in charsets @AVAILABLE_LANGUAGES@; do \
	  test -d $(distdir)/$$dir || mkdir $(distdir)/$$dir; \
	  $(INSTALL_DATA) $(srcdir)/$$dir/*.* $(distdir)/$$dir; \
	done; \
	sleep 1 ; touch $(srcdir)/*/errmsg.sys
	$(INSTALL_DATA) $(srcdir)/charsets/README $(distdir)/charsets
	$(INSTALL_DATA) $(srcdir)/charsets/Index.xml $(distdir)/charsets

all-local:	english/errmsg.sys

# Use the english errmsg.sys as a flag that all errmsg.sys needs to be
# created. Normally these are created by extra/Makefile

english/errmsg.sys: errmsg.txt
	rm $(top_builddir)/include/mysqld_error.h
	(cd $(top_builddir)/extra && $(MAKE))

install-data-local:
	for lang in @AVAILABLE_LANGUAGES@; \
	do \
		$(mkinstalldirs) $(DESTDIR)$(pkgdatadir)/$$lang; \
		$(INSTALL_DATA) $(srcdir)/$$lang/errmsg.sys \
			        $(DESTDIR)$(pkgdatadir)/$$lang/errmsg.sys; \
	done
	$(mkinstalldirs) $(DESTDIR)$(pkgdatadir)/charsets
		$(INSTALL_DATA) $(srcdir)/errmsg.txt \
	        $(DESTDIR)$(pkgdatadir)/errmsg.txt; \
	$(INSTALL_DATA) $(srcdir)/charsets/README $(DESTDIR)$(pkgdatadir)/charsets/README
	$(INSTALL_DATA) $(srcdir)/charsets/*.xml $(DESTDIR)$(pkgdatadir)/charsets

# Don't update the files from bitkeeper
%::SCCS/s.%
