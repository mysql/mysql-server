## Process this file with automake to create Makefile.in

# This requires gnu cp at distribution time.
dist-hook:
	for lang in @AVAILABLE_LANGUAGES@ charsets; \
	do cp -a $(srcdir)/$$lang $(distdir); done

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
