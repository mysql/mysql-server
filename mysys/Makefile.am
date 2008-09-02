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
			-I$(top_srcdir)/include -I$(srcdir)
LDADD =			$(top_builddir)/strings/libmystringslt.la \
			$(top_builddir)/dbug/libdbuglt.la @ZLIB_LIBS@


pkglib_LTLIBRARIES =	libmysys.la
libmysys_la_LDFLAGS = 	-static
libmysys_la_SOURCES =   
# This can't be listed here as $(top_builddir)/mysys/libmysyslt.la
# or it breaks make's dependency track for -j builds
libmysys_la_LIBADD =	libmysyslt.la libmysyswrap.la
# Force C++ linking - dummy.cxx doesn't have to exist with EXTRA in the name
nodist_EXTRA_libmysys_la_SOURCES = dummy.cxx

# We split out my_wrap because it's only used by mysqld and otherwise
# creates an unneccesary linking dependency on -lwrap
noinst_LTLIBRARIES =	libmysyslt.la libmysyswrap.la
libmysyswrap_la_SOURCES = my_libwrap.c
noinst_HEADERS =	mysys_priv.h my_static.h my_handler_errors.h
libmysyslt_la_SOURCES =	my_init.c my_getwd.c mf_getdate.c my_mmap.c \
			mf_path.c mf_loadpath.c my_file.c \
			my_open.c my_create.c my_dup.c my_seek.c my_read.c \
			my_pread.c my_write.c my_getpagesize.c \
			mf_keycache.c mf_keycaches.c my_crc32.c \
			mf_iocache.c mf_iocache2.c mf_cache.c mf_tempfile.c \
			mf_tempdir.c my_lock.c mf_brkhant.c my_alarm.c \
			my_malloc.c my_realloc.c my_once.c mulalloc.c \
			my_alloc.c safemalloc.c my_new.cc \
			my_vle.c my_atomic.c \
			my_fopen.c my_fstream.c my_getsystime.c \
			my_error.c errors.c my_div.c my_messnc.c \
			mf_format.c mf_same.c mf_dirname.c mf_fn_ext.c \
			my_symlink.c my_symlink2.c \
			mf_pack.c mf_unixpath.c mf_strip.c mf_arr_appstr.c \
			mf_wcomp.c mf_wfile.c my_gethwaddr.c \
			mf_qsort.c mf_qsort2.c mf_sort.c \
			ptr_cmp.c mf_radix.c queues.c my_getncpus.c \
			tree.c trie.c list.c hash.c array.c string.c typelib.c \
			my_copy.c my_append.c my_lib.c \
			my_delete.c my_rename.c my_redel.c \
			my_chsize.c my_clock.c \
			my_quick.c my_lockmem.c my_static.c \
			my_sync.c my_getopt.c my_mkdir.c \
			default_modify.c default.c \
                        my_compress.c checksum.c \
			my_net.c my_port.c my_sleep.c \
			charset.c charset-def.c my_bitmap.c my_bit.c md5.c \
			my_gethostbyname.c rijndael.c my_aes.c sha1.c \
			my_handler.c my_netware.c my_largepage.c \
			my_memmem.c stacktrace.c \
			my_windac.c my_access.c base64.c my_libwrap.c \
			thr_alarm.c thr_lock.c my_pthread.c my_thr_init.c \
			thr_mutex.c thr_rwlock.c \
			mf_soundex.c \
			my_conio.c my_wincond.c my_winthread.c
EXTRA_DIST =		CMakeLists.txt

DEFS =			-DDEFAULT_BASEDIR=\"$(prefix)\" \
			-DDATADIR="\"$(MYSQLDATAdir)\"" \
			-DDEFAULT_CHARSET_HOME="\"$(MYSQLBASEdir)\"" \
			-DSHAREDIR="\"$(MYSQLSHAREdir)\"" \
			-DDEFAULT_HOME_ENV=MYSQL_HOME \
			-DDEFAULT_GROUP_SUFFIX_ENV=MYSQL_GROUP_SUFFIX \
			-DDEFAULT_SYSCONFDIR="\"$(sysconfdir)\"" \
                        @DEFS@

# Don't update the files from bitkeeper
%::SCCS/s.%
