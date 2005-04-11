## Process this file with automake to create Makefile.in

dist-hook:
	for dir in charsets @AVAILABLE_LANGUAGES@; do \
	  test -d $(distdir)/$$dir || mkdir $(distdir)/$$dir; \
	  $(INSTALL_DATA) $(srcdir)/$$dir/*.* $(distdir)/$$dir; \
	done; \
	sleep 1 ; touch $(srcdir)/*/errmsg.sys
	$(INSTALL_DATA) $(srcdir)/charsets/README $(distdir)/charsets
	$(INSTALL_DATA) $(srcdir)/charsets/Index.xml $(distdir)/charsets

all-local: @AVAILABLE_LANGUAGES_ERRORS@

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
	$(INSTALL_DATA) $(srcdir)/charsets/*.xml $(DESTDIR)$(pkgdatadir)/charsets

fix_errors:
	for lang in @AVAILABLE_LANGUAGES@; \
	do \
	../../extra/comp_err -C$(srcdir)/charsets/ $(srcdir)/$$lang/errmsg.txt $(srcdir)/$$lang/errmsg.sys; \
        done

# Don't update the files from bitkeeper
%::SCCS/s.%
