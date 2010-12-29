# Copyright (C) 2004-2006 MySQL AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

SUBDIRS = src tools . include @ndb_opt_subdirs@
DIST_SUBDIRS = src tools include test docs
EXTRA_DIST = config ndbapi-examples plug.in
DIST_COMMON = $(srcdir)/Makefile.am $(srcdir)/Makefile.in

include $(top_srcdir)/storage/ndb/config/common.mk.am

dist-hook:
	-rm -rf `find $(distdir) -type d -name SCCS`
	-rm -rf `find $(distdir)/ndbapi-examples -name '*.o'`
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
	for i in `find . -name 'Makefile.am' -print`; \
	do make -C `dirname $$i` windoze-dsp; done

windoze-dsp:

all-windoze-dsp: windoze
	find . -name '*.dsp' | grep -v SCCS | xargs unix2dos
	$(top_srcdir)/storage/ndb/config/make-win-dsw.sh | unix2dos > ndb.dsw
	tar cvfz ndb-win-dsp.tar.gz ndb.dsw `find . -name '*.dsp' | grep -v SCCS`
