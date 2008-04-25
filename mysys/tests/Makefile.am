# Copyright (C) 2000-2006 MySQL AB
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
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

MYSQLDATAdir =		$(localstatedir)
MYSQLSHAREdir =		$(pkgdatadir)
MYSQLBASEdir=		$(prefix)
INCLUDES =		@ZLIB_INCLUDES@ -I$(top_builddir)/include \
			-I$(top_srcdir)/include -I$(srcdir)  \
                        -I$(top_srcdir)/mysys
LDADD =			$(top_builddir)/mysys/libmysyslt.la $(top_builddir)/strings/libmystrings.la $(top_builddir)/dbug/libdbug.la

DEFS =			-DMAIN \
			-DDEFAULT_BASEDIR=\"$(prefix)\" \
			-DDATADIR="\"$(MYSQLDATAdir)\"" \
			-DDEFAULT_CHARSET_HOME="\"$(MYSQLBASEdir)\"" \
			-DSHAREDIR="\"$(MYSQLSHAREdir)\"" \
			-DDEFAULT_HOME_ENV=MYSQL_HOME \
			-DDEFAULT_GROUP_SUFFIX_ENV=MYSQL_GROUP_SUFFIX \
			-DDEFAULT_SYSCONFDIR="\"$(sysconfdir)\"" \
                        @DEFS@

# I hope this always does the right thing. Otherwise this is only test programs
FLAGS=$(DEFS) $(INCLUDES) $(CPPFLAGS) $(CFLAGS) @NOINST_LDFLAGS@

noinst_PROGRAMS = test_bitmap test_priority_queue test_thr_alarm test_thr_lock test_vsnprintf test_io_cache testhash test_gethwaddr test_base64 #test_charset
test_bitmap_SOURCES = $(top_srcdir)/mysys/my_bitmap.c
test_thr_alarm_SOURCES = $(top_srcdir)/mysys/thr_alarm.c
test_priority_queue_SOURCES = $(top_srcdir)/mysys/queues.c
test_thr_lock_SOURCES = $(top_srcdir)/mysys/thr_lock.c
test_vsnprintf_SOURCES = $(top_srcdir)/strings/my_vsnprintf.c
test_io_cache_SOURCES = $(top_srcdir)/mysys/mf_iocache.c 
#test_dir_SOURCES = test_dir.c
#test_charset_SOURCES = test_charset.c
testhash_SOURCES = testhash.c
test_gethwaddr_SOURCES = $(top_srcdir)/mysys/my_gethwaddr.c
test_base64_SOURCES = $(top_srcdir)/mysys/base64.c
test_base64_LDADD = $(top_builddir)/mysys/libmysyslt.la $(top_builddir)/strings/libmystrings.la

# Don't update the files from bitkeeper
%::SCCS/s.%
