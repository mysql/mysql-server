## Process this file with automake to create Makefile.in

dist-hook:
	for dir in charsets @AVAILABLE_LANGUAGES@; do \
	  test -d $(distdir)/$$dir || mkdir $(distdir)/$$dir; \
	  $(INSTALL_DATA) $(srcdir)/$$dir/*.* $(distdir)/$$dir; \
	done; \
	$(INSTALL_DATA) $(srcdir)/charsets/Index $(distdir)/charsets

all: @AVAILABLE_LANGUAGES_ERRORS@

# this is ugly, but portable
@AVAILABLE_LANGUAGES_ERRORS_RULES@

install-data-local:
	for lang in @AVAILABLE_LANGUAGES@; \
	do \
		$(mkinstalldirs) $(DESTDIR)$(pkgdatadir)/$$lang; \
		$(INSTALL_DATA) $(srcdir)/$$lang/errmsg.sys \
			        $(DESTDIR)$(pkgdatadir)/$$lang/errmsg.sys; \
		$(INSTALL_DATA) $(srcdir)/$$lang/errmsg.txt \
			        $(DESTDIR)$(pkgdatadir)/$$lang/errmsg.txt; \
	done
	$(mkinstalldirs) $(DESTDIR)$(pkgdatadir)/charsets
	$(INSTALL_DATA) $(srcdir)/charsets/README $(DESTDIR)$(pkgdatadir)/charsets/README
	$(INSTALL_DATA) $(srcdir)/charsets/Index $(DESTDIR)$(pkgdatadir)/charsets/Index
	$(INSTALL_DATA) $(srcdir)/charsets/*.conf $(DESTDIR)$(pkgdatadir)/charsets

# Don't update the files from bitkeeper
%::SCCS/s.%
