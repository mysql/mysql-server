## Process this file with automake to create Makefile.in

# This requires gnu cp at distribution time.
dist-hook:
	for lang in @AVAILABLE_LANGUAGES@ charsets; \
	do cp -a $(srcdir)/$$lang $(distdir); done

install-data-local:
	for lang in @AVAILABLE_LANGUAGES@; \
	do \
		$(mkinstalldirs) $(DESTDIR)$(pkgdatadir)/$$lang; \
		../../extra/comp_err $(srcdir)/$$lang/errmsg.txt $(srcdir)/$$lang/errmsg.sys; \
		$(INSTALL_DATA) $(srcdir)/$$lang/errmsg.sys \
			        $(DESTDIR)$(pkgdatadir)/$$lang/errmsg.sys; \
		$(INSTALL_DATA) $(srcdir)/$$lang/errmsg.txt \
			        $(DESTDIR)$(pkgdatadir)/$$lang/errmsg.txt; \
	done
	$(mkinstalldirs) $(DESTDIR)$(pkgdatadir)/charsets
	(for f in Index README "*.conf"; \
	do \
		$(INSTALL_DATA) $(srcdir)/charsets/$$f $(DESTDIR)$(pkgdatadir)/charsets/; \
	done)
