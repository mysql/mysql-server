# Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
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
INCLUDES =		@MT_INCLUDES@ -I$(srcdir)/../include -I../include -I.. -I$(srcdir)
pkglib_LIBRARIES =	libmysys.a
LDADD =			libmysys.a ../dbug/libdbug.a \
			../strings/libmystrings.a
noinst_HEADERS =	mysys_priv.h my_static.h
libmysys_a_SOURCES =	my_init.c my_getwd.c mf_getdate.c\
			mf_path.c mf_loadpath.c\
			my_open.c my_create.c my_seek.c my_read.c \
			my_pread.c my_write.c \
			mf_keycache.c \
			mf_iocache.c mf_iocache2.c mf_cache.c mf_tempfile.c \
			my_lock.c mf_brkhant.c my_alarm.c \
			my_malloc.c my_realloc.c my_once.c mulalloc.c \
			my_alloc.c safemalloc.c my_fopen.c my_fstream.c \
			my_error.c errors.c my_div.c my_messnc.c \
			mf_format.c mf_same.c mf_dirname.c mf_fn_ext.c \
			my_symlink.c \
			mf_pack.c mf_pack2.c mf_unixpath.c mf_stripp.c \
			mf_casecnv.c mf_soundex.c mf_wcomp.c mf_wfile.c \
			mf_qsort.c mf_qsort2.c mf_sort.c \
			ptr_cmp.c mf_radix.c queues.c \
			tree.c list.c hash.c array.c string.c typelib.c \
			my_copy.c my_append.c my_lib.c \
			my_delete.c my_rename.c my_redel.c my_tempnam.c \
			my_chsize.c my_lread.c my_lwrite.c my_clock.c \
			my_quick.c my_lockmem.c my_static.c \
			getopt.c getopt1.c getvar.c my_mkdir.c \
			default.c my_compress.c checksum.c raid.cc my_net.c \
			my_vsnprintf.c charset.c my_bitmap.c
EXTRA_DIST =		thr_alarm.c thr_lock.c my_pthread.c my_thr_init.c \
			thr_mutex.c thr_rwlock.c
libmysys_a_LIBADD =	@THREAD_LOBJECTS@
# test_fn removed 980815 since it not upp to date test_dir
noinst_PROGRAMS =	test_charset @THREAD_LPROGRAMS@
# test_dir_DEPENDENCIES=	$(LIBRARIES)
# testhash_DEPENDENCIES=	$(LIBRARIES)
test_charset_DEPENDENCIES=	$(LIBRARIES)
EXTRA_PROGRAMS =	
DEFS =			-DDEFAULT_BASEDIR=\"$(prefix)\" \
			-DDATADIR="\"$(MYSQLDATAdir)\"" \
			-DDEFAULT_CHARSET_HOME="\"$(MYSQLBASEdir)\"" \
			-DDATADIR="\"$(MYSQLDATAdir)\"" \
			-DSHAREDIR="\"$(MYSQLSHAREdir)\"" \
                        @DEFS@

libmysys_a_DEPENDENCIES= @THREAD_LOBJECTS@

OMIT_DEPENDENCIES =	pthread.h stdio.h __stdio.h stdlib.h __stdlib.h math.h\
			__math.h time.h __time.h unistd.h __unistd.h types.h \
			xtypes.h ac-types.h posix.h string.h __string.h \
			errno.h socket.h inet.h dirent.h netdb.h \
			cleanup.h cond.h debug_out.h fd.h kernel.h mutex.h \
			prio_queue.h pthread_attr.h pthread_once.h queue.h\
			sleep.h specific.h version.h pwd.h timers.h uio.h \
			cdefs.h machdep.h signal.h __signal.h util.h

# I hope this always does the right thing. Otherwise this is only test programs
FLAGS=$(DEFS) $(INCLUDES) $(CPPFLAGS) $(CFLAGS) @NOINST_LDFLAGS@

#
# The CP .. RM stuff is to avoid problems with some compilers (like alpha ccc)
# which automaticly removes the object files you use to compile a final program
#

test_thr_alarm: thr_alarm.c $(LIBRARIES)
	$(CP) $(srcdir)/thr_alarm.c ./test_thr_alarm.c
	$(LINK) $(FLAGS) -DMAIN  ./test_thr_alarm.c $(LDADD) $(LIBS)
	$(RM) -f ./test_thr_alarm.*

test_thr_lock: thr_lock.c $(LIBRARIES)
	$(CP) $(srcdir)/thr_lock.c test_thr_lock.c
	$(LINK) $(FLAGS) -DMAIN  ./test_thr_lock.c $(LDADD) $(LIBS)
	$(RM) -f ./test_thr_lock.*

test_vsnprintf: my_vsnprintf.c $(LIBRARIES)
	$(CP) $(srcdir)/my_vsnprintf.c test_vsnprintf.c
	$(LINK) $(FLAGS) -DMAIN ./test_vsnprintf.c $(LDADD) $(LIBS)
	$(RM) -f test_vsnprintf.*

test_dir: test_dir.c $(LIBRARIES)
	$(LINK) $(FLAGS) -DMAIN $(srcdir)/test_dir.c $(LDADD) $(LIBS)

test_charset: test_charset.c $(LIBRARIES)
	$(LINK) $(FLAGS) -DMAIN $(srcdir)/test_charset.c $(LDADD) $(LIBS)

testhash: testhash.c $(LIBRARIES)
	$(LINK) $(FLAGS) -DMAIN $(srcdir)/testhash.c $(LDADD) $(LIBS)

# Don't update the files from bitkeeper
%::SCCS/s.%
