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

#called from the top level Makefile


MYSQLDATAdir =		$(localstatedir)
MYSQLSHAREdir =		$(pkgdatadir)
MYSQLBASEdir=		$(prefix)
INCLUDES =		@MT_INCLUDES@ \
			@bdb_includes@ @innodb_includes@ @gemini_includes@ \
			-I$(srcdir)/../include \
			-I$(srcdir)/../regex \
			-I$(srcdir) -I../include -I.. -I.
WRAPLIBS=		@WRAPLIBS@
SUBDIRS =		share
bin_PROGRAMS	=	mysqlbinlog
libexec_PROGRAMS =	mysqld
noinst_PROGRAMS =	gen_lex_hash
gen_lex_hash_LDFLAGS =        @NOINST_LDFLAGS@
LDADD =			../isam/libnisam.a \
			../merge/libmerge.a \
			../myisam/libmyisam.a \
			../myisammrg/libmyisammrg.a \
			../heap/libheap.a \
			../mysys/libmysys.a \
			../dbug/libdbug.a \
			../regex/libregex.a \
			../strings/libmystrings.a
mysqld_LDADD =		@MYSQLD_EXTRA_LDFLAGS@ \
			@bdb_libs@ @innodb_libs@ @gemini_libs@ \
			$(LDADD)  $(CXXLDFLAGS) $(WRAPLIBS) @LIBDL@
noinst_HEADERS =	item.h item_func.h item_sum.h item_cmpfunc.h \
			item_strfunc.h item_timefunc.h item_uniq.h \
			item_create.h mysql_priv.h \
			procedure.h sql_class.h sql_lex.h sql_list.h \
			sql_manager.h sql_map.h sql_string.h unireg.h \
			field.h handler.h \
			ha_isammrg.h ha_isam.h ha_myisammrg.h\
			ha_heap.h ha_myisam.h ha_berkeley.h ha_innobase.h \
			ha_gemini.h opt_range.h opt_ft.h \
			sql_select.h structs.h table.h sql_udf.h hash_filo.h\
			lex.h lex_symbol.h sql_acl.h sql_crypt.h md5.h \
                        log_event.h mini_client.h sql_repl.h slave.h \
			stacktrace.h
mysqld_SOURCES =	sql_lex.cc \
			item.cc item_sum.cc item_buff.cc item_func.cc \
			item_cmpfunc.cc item_strfunc.cc item_timefunc.cc \
			thr_malloc.cc item_create.cc \
			field.cc key.cc sql_class.cc sql_list.cc \
			net_serv.cc violite.c net_pkg.cc lock.cc my_lock.c \
			sql_string.cc sql_manager.cc sql_map.cc \
			mysqld.cc password.c hash_filo.cc hostname.cc \
			convert.cc sql_parse.cc sql_yacc.yy \
			sql_base.cc table.cc sql_select.cc sql_insert.cc \
			sql_update.cc sql_delete.cc \
			procedure.cc item_uniq.cc sql_test.cc \
			log.cc log_event.cc init.cc derror.cc sql_acl.cc \
			unireg.cc \
			time.cc opt_range.cc opt_sum.cc opt_ft.cc \
		   	records.cc filesort.cc handler.cc \
		        ha_heap.cc ha_myisam.cc ha_myisammrg.cc \
	                ha_berkeley.cc ha_innobase.cc ha_gemini.cc \
			ha_isam.cc ha_isammrg.cc \
			sql_db.cc sql_table.cc sql_rename.cc sql_crypt.cc \
			sql_load.cc mf_iocache.cc field_conv.cc sql_show.cc \
			sql_udf.cc sql_analyse.cc sql_analyse.h sql_cache.cc \
			slave.cc sql_repl.cc \
			mini_client.cc mini_client_errors.c \
			md5.c stacktrace.c
gen_lex_hash_SOURCES =	gen_lex_hash.cc
gen_lex_hash_LDADD =	$(LDADD) $(CXXLDFLAGS)
mysqlbinlog_SOURCES =   mysqlbinlog.cc mini_client.cc net_serv.cc \
			mini_client_errors.c violite.c password.c
mysqlbinlog_LDADD = $(LDADD) $(CXXLDFLAGS) # $(mysqld_LDADD)

DEFS =			-DMYSQL_SERVER \
			-DDEFAULT_MYSQL_HOME="\"$(MYSQLBASEdir)\"" \
			-DDATADIR="\"$(MYSQLDATAdir)\"" \
			-DSHAREDIR="\"$(MYSQLSHAREdir)\"" \
			@DEFS@
# Don't put lex_hash.h in BUILT_SOURCES as this will give infinite recursion
BUILT_SOURCES =		sql_yacc.cc sql_yacc.h
EXTRA_DIST =		udf_example.cc $(BUILT_SOURCES)
YFLAGS =		-d

OMIT_DEPENDENCIES =	pthread.h stdio.h __stdio.h stdlib.h __stdlib.h math.h\
			__math.h time.h __time.h unistd.h __unistd.h types.h \
			xtypes.h ac-types.h posix.h string.h __string.h \
			errno.h socket.h inet.h dirent.h netdb.h \
			cleanup.h cond.h debug_out.h fd.h kernel.h mutex.h \
			prio_queue.h pthread_attr.h pthread_once.h queue.h\
			sleep.h specific.h version.h pwd.h timers.h uio.h \
			cdefs.h machdep.h signal.h __signal.h util.h lex.h \
			wait.h

link_sources:
	rm -f mini_client_errors.c
	@LN_CP_F@ ../libmysql/errmsg.c mini_client_errors.c

gen_lex_hash.o:		gen_lex_hash.cc lex.h
			$(CXXCOMPILE) -c $(INCLUDES) $<

# Try to get better dependencies for the grammar. Othervise really bad
# things like different grammars for different pars of MySQL can
# happen if you are unlucky.
sql_yacc.cc:	sql_yacc.yy
sql_yacc.h:	sql_yacc.yy

sql_yacc.o:	sql_yacc.cc sql_yacc.h
		@echo "Note: The following compile may take a long time."
		@echo "If it fails, re-run configure with --with-low-memory"
		$(CXXCOMPILE) $(LM_CFLAGS) -c $<

lex_hash.h:	lex.h gen_lex_hash.cc sql_yacc.h
		$(MAKE) gen_lex_hash
		./gen_lex_hash > $@

# Hack to ensure that lex_hash.h is built early
sql_lex.o:	lex_hash.h

#distclean:
#		rm -f lex_hash.h

# Don't update the files from bitkeeper
%::SCCS/s.%
