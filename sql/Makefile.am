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
INCLUDES =		@ZLIB_INCLUDES@ \
			@bdb_includes@ @innodb_includes@ @ndbcluster_includes@ \
			-I$(top_builddir)/include -I$(top_srcdir)/include \
			-I$(top_srcdir)/regex -I$(srcdir) \
                        $(openssl_includes)
WRAPLIBS=		@WRAPLIBS@
SUBDIRS =		share
libexec_PROGRAMS =	mysqld
noinst_PROGRAMS =	gen_lex_hash
bin_PROGRAMS =		mysql_tzinfo_to_sql
gen_lex_hash_LDFLAGS =  @NOINST_LDFLAGS@
LDADD =			$(top_builddir)/myisam/libmyisam.a \
			$(top_builddir)/myisammrg/libmyisammrg.a \
			$(top_builddir)/heap/libheap.a \
			$(top_builddir)/vio/libvio.a \
			$(top_builddir)/mysys/libmysys.a \
			$(top_builddir)/dbug/libdbug.a \
			$(top_builddir)/regex/libregex.a \
			$(top_builddir)/strings/libmystrings.a @ZLIB_LIBS@ @NDB_SCI_LIBS@

mysqld_LDADD =		@MYSQLD_EXTRA_LDFLAGS@ \
			@bdb_libs@ @innodb_libs@ @pstack_libs@ \
                        @innodb_system_libs@ \
			@ndbcluster_libs@ @ndbcluster_system_libs@ \
			$(LDADD)  $(CXXLDFLAGS) $(WRAPLIBS) @LIBDL@ \
                        @yassl_libs@ @openssl_libs@
noinst_HEADERS =	item.h item_func.h item_sum.h item_cmpfunc.h \
			item_strfunc.h item_timefunc.h item_uniq.h \
			item_create.h item_subselect.h item_row.h \
			mysql_priv.h item_geofunc.h sql_bitmap.h \
			procedure.h sql_class.h sql_lex.h sql_list.h \
			sql_manager.h sql_map.h sql_string.h unireg.h \
			sql_error.h field.h handler.h mysqld_suffix.h \
			ha_myisammrg.h\
			ha_heap.h ha_myisam.h ha_berkeley.h ha_innodb.h \
			ha_ndbcluster.h opt_range.h protocol.h \
			sql_select.h structs.h table.h sql_udf.h hash_filo.h\
			lex.h lex_symbol.h sql_acl.h sql_crypt.h  \
			log_event.h sql_repl.h slave.h \
			stacktrace.h sql_sort.h sql_cache.h set_var.h \
			spatial.h gstream.h client_settings.h tzfile.h \
                        tztime.h my_decimal.h\
			sp_head.h sp_pcontext.h sp_rcontext.h sp.h sp_cache.h \
			parse_file.h sql_view.h	sql_trigger.h \
			sql_array.h sql_cursor.h \
			examples/ha_example.h ha_archive.h \
			examples/ha_tina.h ha_blackhole.h  \
			ha_federated.h
mysqld_SOURCES =	sql_lex.cc sql_handler.cc \
			item.cc item_sum.cc item_buff.cc item_func.cc \
			item_cmpfunc.cc item_strfunc.cc item_timefunc.cc \
			thr_malloc.cc item_create.cc item_subselect.cc \
			item_row.cc item_geofunc.cc \
			field.cc strfunc.cc key.cc sql_class.cc sql_list.cc \
			net_serv.cc protocol.cc sql_state.c \
			lock.cc my_lock.c \
			sql_string.cc sql_manager.cc sql_map.cc \
			mysqld.cc password.c hash_filo.cc hostname.cc \
			set_var.cc sql_parse.cc sql_yacc.yy \
			sql_base.cc table.cc sql_select.cc sql_insert.cc \
                        sql_prepare.cc sql_error.cc sql_locale.cc \
			sql_update.cc sql_delete.cc uniques.cc sql_do.cc \
			procedure.cc item_uniq.cc sql_test.cc \
			log.cc log_event.cc init.cc derror.cc sql_acl.cc \
			unireg.cc des_key_file.cc \
			discover.cc time.cc opt_range.cc opt_sum.cc \
		   	records.cc filesort.cc handler.cc \
		        ha_heap.cc ha_myisam.cc ha_myisammrg.cc \
	                ha_berkeley.cc ha_innodb.cc \
			ha_ndbcluster.cc \
			sql_db.cc sql_table.cc sql_rename.cc sql_crypt.cc \
			sql_load.cc mf_iocache.cc field_conv.cc sql_show.cc \
			sql_udf.cc sql_analyse.cc sql_analyse.h sql_cache.cc \
			slave.cc sql_repl.cc sql_union.cc sql_derived.cc \
			client.c sql_client.cc mini_client_errors.c pack.c\
			stacktrace.c repl_failsafe.h repl_failsafe.cc \
			sql_olap.cc sql_view.cc \
			gstream.cc spatial.cc sql_help.cc sql_cursor.cc \
			tztime.cc my_time.c my_user.c my_decimal.cc\
			sp_head.cc sp_pcontext.cc  sp_rcontext.cc sp.cc \
			sp_cache.cc parse_file.cc sql_trigger.cc \
			examples/ha_example.cc ha_archive.cc \
			examples/ha_tina.cc ha_blackhole.cc \
			ha_federated.cc

gen_lex_hash_SOURCES =	gen_lex_hash.cc
gen_lex_hash_LDADD =	$(LDADD) $(CXXLDFLAGS)
mysql_tzinfo_to_sql_SOURCES =   mysql_tzinfo_to_sql.cc
mysql_tzinfo_to_sql_LDADD =  @MYSQLD_EXTRA_LDFLAGS@ $(LDADD) $(CXXLDFLAGS)

DEFS =			-DMYSQL_SERVER \
			-DDEFAULT_MYSQL_HOME="\"$(MYSQLBASEdir)\"" \
			-DDATADIR="\"$(MYSQLDATAdir)\"" \
			-DSHAREDIR="\"$(MYSQLSHAREdir)\"" \
			@DEFS@

BUILT_SOURCES =		sql_yacc.cc sql_yacc.h lex_hash.h
EXTRA_DIST =		$(BUILT_SOURCES)
DISTCLEANFILES =        lex_hash.h
AM_YFLAGS =		-d

mysql_tzinfo_to_sql.cc:
	rm -f mysql_tzinfo_to_sql.cc
	@LN_CP_F@ $(srcdir)/tztime.cc mysql_tzinfo_to_sql.cc

link_sources: mysql_tzinfo_to_sql.cc
	rm -f mini_client_errors.c
	@LN_CP_F@ $(top_srcdir)/libmysql/errmsg.c mini_client_errors.c
	rm -f pack.c
	@LN_CP_F@ $(top_srcdir)/sql-common/pack.c pack.c
	rm -f client.c
	@LN_CP_F@ $(top_srcdir)/sql-common/client.c client.c
	rm -f my_time.c
	@LN_CP_F@ $(top_srcdir)/sql-common/my_time.c my_time.c
	rm -f my_user.c
	@LN_CP_F@ $(top_srcdir)/sql-common/my_user.c my_user.c

mysql_tzinfo_to_sql.o:	$(mysql_tzinfo_to_sql_SOURCES)
			$(CXXCOMPILE) -c $(INCLUDES) -DTZINFO2SQL $<

# Try to get better dependencies for the grammar. Othervise really bad
# things like different grammars for different pars of MySQL can
# happen if you are unlucky.
sql_yacc.cc:	sql_yacc.yy
sql_yacc.h:	sql_yacc.yy

sql_yacc.o:	sql_yacc.cc sql_yacc.h $(HEADERS)
		@echo "Note: The following compile may take a long time."
		@echo "If it fails, re-run configure with --with-low-memory"
		$(CXXCOMPILE) $(LM_CFLAGS) -c $<

lex_hash.h:	gen_lex_hash$(EXEEXT)
		./gen_lex_hash$(EXEEXT) > $@

# For testing of udf_example.so
noinst_LTLIBRARIES= udf_example.la
udf_example_la_SOURCES= udf_example.c
udf_example_la_LDFLAGS= -module -rpath $(pkglibdir)


# Don't update the files from bitkeeper
%::SCCS/s.%
