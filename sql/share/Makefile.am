## Process this file with automake to create Makefile.in

ERRMSG=$(patsubst %, %/errmsg.sys, @AVAILABLE_LANGUAGES@)

# This requires gnu cp at distribution time.
dist-hook:
	for lang in @AVAILABLE_LANGUAGES@ charsets; \
	do cp -a $(srcdir)/$$lang $(distdir); done

all: $(ERRMSG)

$(ERRMSG): %.sys: %.txt
	../../extra/comp_err $< $@

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
	(for f in $(srcdir)/charsets/Index $(srcdir)/charsets/README $(srcdir)/charsets/*.conf; \
	do \
		n=`basename $$f`; \
		$(INSTALL_DATA) $$f $(DESTDIR)$(pkgdatadir)/charsets/$$n; \
	done)
