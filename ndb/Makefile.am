SUBDIRS = src tools . include $(ndb_opt_test_subdirs)
DIST_SUBDIRS = src tools ndbapi include test

include $(top_srcdir)/ndb/config/common.mk.am

dist-hook:
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
