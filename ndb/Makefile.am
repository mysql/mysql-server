SUBDIRS = src tools . include @ndb_opt_subdirs@
DIST_SUBDIRS = src tools include test docs
EXTRA_DIST = config

include $(top_srcdir)/ndb/config/common.mk.am

dist-hook:
	-rm -rf `find $(distdir) -type d -name SCCS`
	-rm -rf `find $(distdir) -type d -name old_files`
	list='$(SUBDIRS)'; for subdir in $$list; do \
	  if test "$$subdir" != "." -a "$$subdir" != "include"; then \
	    files="`find $$subdir -name '*\.h'` `find $$subdir -name '*\.hpp'`"; \
	    for f in $$files; do \
	      if test -d "$(distdir)/`dirname $$f`" -a ! -e "$(distdir)/$$f"; then \
	        cp $$f $(distdir)/$$f; \
	      fi; \
	    done; \
	  fi; \
	done

windoze:
	for i in `find . -name 'Makefile.am'`; do make -C `dirname $$i` windoze-dsp; done

windoze-dsp:

all-windoze-dsp: windoze
	find . -name '*.dsp' | grep -v SCCS | xargs unix2dos
	$(top_srcdir)/ndb/config/make-win-dsw.sh | unix2dos > ndb.dsw
	tar cvfz ndb-win-dsp.tar.gz ndb.dsw `find . -name '*.dsp' | grep -v SCCS`
