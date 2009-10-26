# Copyright (C) 2000-2008 MySQL AB
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

MYSQLDATAdir =          $(localstatedir)
MYSQLSHAREdir =         $(pkgdatadir)
MYSQLBASEdir=           $(prefix)
MYSQLLIBdir=            $(pkglibdir)
INCLUDES =              -I$(top_srcdir)/include -I$(top_builddir)/include \
			-I$(top_srcdir)/regex \
			-I$(top_srcdir)/sql \
                        -I$(srcdir)
WRAPLIBS=

LDADD =

DEFS =                  @DEFS@

# "." is needed first because tests in unittest need libmaria
SUBDIRS =		. unittest

EXTRA_DIST =		ma_test_all.sh ma_test_all.res ma_test_big.sh \
			ma_ft_stem.c CMakeLists.txt plug.in ma_test_recovery
pkgdata_DATA =		
pkglib_LIBRARIES =	libmaria.a
bin_PROGRAMS =		maria_chk maria_pack maria_ftdump maria_read_log \
			maria_dump_log
maria_chk_DEPENDENCIES=	$(LIBRARIES)
# Only reason to link with libmyisam.a here is that it's where some fulltext
# pieces are (but soon we'll remove fulltext dependencies from Maria).
# For now, it imposes that storage/myisam be built before storage/maria.
maria_chk_LDADD=		@CLIENT_EXTRA_LDFLAGS@ libmaria.a \
                        $(top_builddir)/storage/myisam/libmyisam.a \
			$(top_builddir)/mysys/libmysys.a \
			$(top_builddir)/dbug/libdbug.a \
			$(top_builddir)/strings/libmystrings.a @ZLIB_LIBS@
maria_pack_DEPENDENCIES=$(LIBRARIES)
maria_pack_LDADD=		@CLIENT_EXTRA_LDFLAGS@ libmaria.a \
                        $(top_builddir)/storage/myisam/libmyisam.a \
			$(top_builddir)/mysys/libmysys.a \
			$(top_builddir)/dbug/libdbug.a \
			$(top_builddir)/strings/libmystrings.a @ZLIB_LIBS@
maria_read_log_DEPENDENCIES=$(LIBRARIES)
maria_read_log_LDADD=		@CLIENT_EXTRA_LDFLAGS@ libmaria.a \
                        $(top_builddir)/storage/myisam/libmyisam.a \
			$(top_builddir)/mysys/libmysys.a \
			$(top_builddir)/dbug/libdbug.a \
			$(top_builddir)/strings/libmystrings.a @ZLIB_LIBS@
maria_dump_log_DEPENDENCIES=$(LIBRARIES) ma_loghandler.c
maria_dump_log_LDADD=		@CLIENT_EXTRA_LDFLAGS@ libmaria.a \
                        $(top_builddir)/storage/myisam/libmyisam.a \
                        $(top_builddir)/mysys/libmysys.a \
			$(top_builddir)/dbug/libdbug.a \
			$(top_builddir)/strings/libmystrings.a @ZLIB_LIBS@
maria_dump_log_SOURCES= ma_loghandler.c unittest/ma_loghandler_examples.c
maria_dump_log_CPPFLAGS= -DMARIA_DUMP_LOG
noinst_PROGRAMS =	ma_test1 ma_test2 ma_test3 ma_rt_test ma_sp_test
noinst_HEADERS =	maria_def.h ma_rt_index.h ma_rt_key.h ma_rt_mbr.h \
			ma_sp_defs.h ma_fulltext.h ma_ftdefs.h ma_ft_test1.h \
			ma_ft_eval.h trnman.h lockman.h tablockman.h \
			ma_control_file.h ha_maria.h ma_blockrec.h \
			ma_loghandler.h ma_loghandler_lsn.h ma_pagecache.h \
			ma_checkpoint.h ma_recovery.h ma_commit.h ma_state.h \
			trnman_public.h ma_check_standalone.h \
			ma_key_recover.h ma_recovery_util.h
ma_test1_DEPENDENCIES=	$(LIBRARIES)
ma_test1_LDADD=		@CLIENT_EXTRA_LDFLAGS@ libmaria.a \
                        $(top_builddir)/storage/myisam/libmyisam.a \
			$(top_builddir)/mysys/libmysys.a \
			$(top_builddir)/dbug/libdbug.a \
			$(top_builddir)/strings/libmystrings.a @ZLIB_LIBS@
ma_test2_DEPENDENCIES=	$(LIBRARIES)
ma_test2_LDADD=		@CLIENT_EXTRA_LDFLAGS@ libmaria.a \
                        $(top_builddir)/storage/myisam/libmyisam.a \
			$(top_builddir)/mysys/libmysys.a \
			$(top_builddir)/dbug/libdbug.a \
			$(top_builddir)/strings/libmystrings.a @ZLIB_LIBS@
ma_test3_DEPENDENCIES=	$(LIBRARIES)
ma_test3_LDADD=		@CLIENT_EXTRA_LDFLAGS@ libmaria.a \
                        $(top_builddir)/storage/myisam/libmyisam.a \
			$(top_builddir)/mysys/libmysys.a \
			$(top_builddir)/dbug/libdbug.a \
			$(top_builddir)/strings/libmystrings.a @ZLIB_LIBS@
#ma_ft_test1_DEPENDENCIES=	$(LIBRARIES)
#ma_ft_eval_DEPENDENCIES=	$(LIBRARIES)
maria_ftdump_DEPENDENCIES=	$(LIBRARIES)
maria_ftdump_LDADD=		@CLIENT_EXTRA_LDFLAGS@ libmaria.a \
                        $(top_builddir)/storage/myisam/libmyisam.a \
			$(top_builddir)/mysys/libmysys.a \
			$(top_builddir)/dbug/libdbug.a \
			$(top_builddir)/strings/libmystrings.a @ZLIB_LIBS@
ma_rt_test_DEPENDENCIES=	$(LIBRARIES)
ma_rt_test_LDADD=		@CLIENT_EXTRA_LDFLAGS@ libmaria.a \
                        $(top_builddir)/storage/myisam/libmyisam.a \
			$(top_builddir)/mysys/libmysys.a \
			$(top_builddir)/dbug/libdbug.a \
			$(top_builddir)/strings/libmystrings.a @ZLIB_LIBS@
ma_sp_test_DEPENDENCIES=	$(LIBRARIES)
ma_sp_test_LDADD=		@CLIENT_EXTRA_LDFLAGS@ libmaria.a \
                        $(top_builddir)/storage/myisam/libmyisam.a \
			$(top_builddir)/mysys/libmysys.a \
			$(top_builddir)/dbug/libdbug.a \
			$(top_builddir)/strings/libmystrings.a @ZLIB_LIBS@
libmaria_a_SOURCES =	ma_init.c ma_open.c ma_extra.c ma_info.c ma_rkey.c \
			ma_rnext.c ma_rnext_same.c \
			ma_search.c ma_page.c ma_key_recover.c ma_key.c \
			ma_locking.c ma_state.c \
			ma_rrnd.c ma_scan.c ma_cache.c \
			ma_statrec.c ma_packrec.c ma_dynrec.c \
			ma_blockrec.c ma_bitmap.c \
			ma_update.c ma_write.c ma_unique.c \
			ma_delete.c \
			ma_rprev.c ma_rfirst.c ma_rlast.c ma_rsame.c \
			ma_rsamepos.c ma_panic.c ma_close.c ma_create.c\
			ma_range.c ma_dbug.c ma_checksum.c \
			ma_changed.c ma_static.c ma_delete_all.c \
			ma_delete_table.c ma_rename.c  ma_check.c \
			ma_keycache.c ma_preload.c ma_ft_parser.c \
			ma_ft_update.c ma_ft_boolean_search.c \
			ma_ft_nlq_search.c ft_maria.c ma_sort.c \
			trnman.c lockman.c tablockman.c \
			ma_rt_index.c ma_rt_key.c ma_rt_mbr.c ma_rt_split.c \
			ma_sp_key.c ma_control_file.c ma_loghandler.c \
			ma_pagecache.c ma_pagecaches.c \
			ma_checkpoint.c ma_recovery.c ma_commit.c \
			ma_pagecrc.c ma_recovery_util.c \
			ha_maria.cc
CLEANFILES =		test?.MA? FT?.MA? isam.log ma_test_all ma_rt_test.MA? sp_test.MA? maria_log_control maria_log.0000*

SUFFIXES = .sh

.sh:
	@RM@ -f $@ $@-t
	@SED@ \
	  -e 's!@''bindir''@!$(bindir)!g' \
	  -e 's!@''scriptdir''@!$(bindir)!g' \
	  -e 's!@''prefix''@!$(prefix)!g' \
	  -e 's!@''datadir''@!$(datadir)!g' \
	  -e 's!@''localstatedir''@!$(localstatedir)!g' \
	  -e 's!@''libexecdir''@!$(libexecdir)!g' \
	  -e 's!@''CC''@!@CC@!'\
	  -e 's!@''CXX''@!@CXX@!'\
	  -e 's!@''GXX''@!@GXX@!'\
	  -e 's!@''PERL''@!@PERL@!' \
	  -e 's!@''CFLAGS''@!@SAVE_CFLAGS@!'\
	  -e 's!@''CXXFLAGS''@!@SAVE_CXXFLAGS@!'\
	  -e 's!@''LDFLAGS''@!@SAVE_LDFLAGS@!'\
	  -e 's!@''VERSION''@!@VERSION@!' \
	  -e 's!@''MYSQL_SERVER_SUFFIX''@!@MYSQL_SERVER_SUFFIX@!' \
	  -e 's!@''COMPILATION_COMMENT''@!@COMPILATION_COMMENT@!' \
	  -e 's!@''MACHINE_TYPE''@!@MACHINE_TYPE@!' \
	  -e 's!@''HOSTNAME''@!@HOSTNAME@!' \
	  -e 's!@''SYSTEM_TYPE''@!@SYSTEM_TYPE@!' \
	  -e 's!@''CHECK_PID''@!@CHECK_PID@!' \
	  -e 's!@''FIND_PROC''@!@FIND_PROC@!' \
	  -e 's!@''MYSQLD_DEFAULT_SWITCHES''@!@MYSQLD_DEFAULT_SWITCHES@!' \
	  -e 's!@''MYSQL_UNIX_ADDR''@!@MYSQL_UNIX_ADDR@!' \
	  -e 's!@''TARGET_LINUX''@!@TARGET_LINUX@!' \
	  -e "s!@""CONF_COMMAND""@!@CONF_COMMAND@!" \
	  -e 's!@''MYSQLD_USER''@!@MYSQLD_USER@!' \
	  -e 's!@''sysconfdir''@!@sysconfdir@!' \
	  -e 's!@''SHORT_MYSQL_INTRO''@!@SHORT_MYSQL_INTRO@!' \
	  -e 's!@''SHARED_LIB_VERSION''@!@SHARED_LIB_VERSION@!' \
	  -e 's!@''MYSQL_BASE_VERSION''@!@MYSQL_BASE_VERSION@!' \
	  -e 's!@''MYSQL_NO_DASH_VERSION''@!@MYSQL_NO_DASH_VERSION@!' \
	  -e 's!@''MYSQL_TCP_PORT''@!@MYSQL_TCP_PORT@!' \
	  -e 's!@''PERL_DBI_VERSION''@!@PERL_DBI_VERSION@!' \
	  -e 's!@''PERL_DBD_VERSION''@!@PERL_DBD_VERSION@!' \
	  -e 's!@''PERL_DATA_DUMPER''@!@PERL_DATA_DUMPER@!' \
	$< > $@-t
	@CHMOD@ +x $@-t
	@MV@ $@-t $@

tags:
	etags *.h *.c *.cc

unittests = unittest

test:
	perl $(top_srcdir)/unittest/unit.pl run $(unittests)

test-verbose:
	HARNESS_VERBOSE=1 perl $(top_srcdir)/unittest/unit.pl run $(unittests)

# Don't update the files from bitkeeper
%::SCCS/s.%
