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

EXTRA_DIST =		mi_test_all.sh mi_test_all.res
pkgdata_DATA =		mi_test_all mi_test_all.res

INCLUDES =		@MT_INCLUDES@ -I$(srcdir)/../include -I../include
LDADD =			@CLIENT_EXTRA_LDFLAGS@ libmyisam.a ../mysys/libmysys.a \
			../dbug/libdbug.a ../strings/libmystrings.a
pkglib_LIBRARIES =	libmyisam.a
bin_PROGRAMS =		myisamchk myisamlog myisampack
myisamchk_DEPENDENCIES=	$(LIBRARIES)
myisamlog_DEPENDENCIES=	$(LIBRARIES)
myisampack_DEPENDENCIES=$(LIBRARIES)
noinst_PROGRAMS =	mi_test1 mi_test2 mi_test3 #ft_test1 ft_eval ft_dump
noinst_HEADERS =	myisamdef.h fulltext.h ftdefs.h ft_test1.h ft_eval.h
mi_test1_DEPENDENCIES=	$(LIBRARIES)
mi_test2_DEPENDENCIES=	$(LIBRARIES)
mi_test3_DEPENDENCIES=	$(LIBRARIES)
ft_test1_DEPENDENCIES=	$(LIBRARIES)
ft_eval_DEPENDENCIES=	$(LIBRARIES)
ft_dump_DEPENDENCIES=	$(LIBRARIES)
libmyisam_a_SOURCES =	mi_open.c mi_extra.c mi_info.c mi_rkey.c \
			mi_rnext.c mi_rnext_same.c \
			mi_search.c mi_page.c mi_key.c mi_locking.c \
			mi_rrnd.c mi_scan.c mi_cache.c \
			mi_statrec.c mi_packrec.c mi_dynrec.c \
			mi_update.c mi_write.c mi_unique.c \
			mi_delete.c \
			mi_rprev.c mi_rfirst.c mi_rlast.c mi_rsame.c \
			mi_rsamepos.c mi_panic.c mi_close.c mi_create.c\
			mi_range.c mi_dbug.c mi_checksum.c mi_log.c \
			mi_changed.c mi_static.c mi_delete_all.c \
			mi_delete_table.c mi_rename.c  mi_check.c \
			ft_parser.c ft_stopwords.c ft_static.c \
			ft_update.c ft_boolean_search.c ft_nlq_search.c sort.c
CLEANFILES =		test?.MY? FT?.MY? isam.log mi_test_all
DEFS =			-DMAP_TO_USE_RAID
# Omit dependency for ../mit-pthreads/include/sys that only exits if
# mit-pthreads are used
OMIT_DEPENDENCIES =	pthread.h stdio.h __stdio.h stdlib.h __stdlib.h math.h\
			__math.h time.h __time.h unistd.h __unistd.h types.h \
			xtypes.h ac-types.h posix.h string.h __string.h \
			errno.h socket.h inet.h dirent.h netdb.h \
			cleanup.h cond.h debug_out.h fd.h kernel.h mutex.h \
			prio_queue.h pthread_attr.h pthread_once.h queue.h\
			sleep.h specific.h version.h pwd.h timers.h uio.h \
			cdefs.h machdep.h signal.h __signal.h util.h \
			wait.h

# Move to automake rules ?
prolint:;	plparse -b -u -hF1 "-width(0,0)" "-format=%f:%l:\s%t:%n\s%m" \
		"-elib(????)" "+elib(?3??)" my.lnt $(nisam_SOURCES)

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
	  -e 's!@''IS_LINUX''@!@IS_LINUX@!' \
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

# Don't update the files from bitkeeper
%::SCCS/s.%
