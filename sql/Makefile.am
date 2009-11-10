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

#called from the top level Makefile

MYSQLDATAdir =		$(localstatedir)
MYSQLSHAREdir =		$(pkgdatadir)
MYSQLBASEdir=		$(prefix)
MYSQLLIBdir=            $(pkglibdir)
pkgplugindir =		$(pkglibdir)/plugin
INCLUDES =		@ZLIB_INCLUDES@ \
			-I$(top_builddir)/include -I$(top_srcdir)/include \
			-I$(top_srcdir)/regex -I$(srcdir) $(openssl_includes)
WRAPLIBS=		@WRAPLIBS@
SUBDIRS =		share
libexec_PROGRAMS =	mysqld
EXTRA_PROGRAMS =	gen_lex_hash
bin_PROGRAMS =		mysql_tzinfo_to_sql
DTRACEFILES =           filesort.o \
			.libs/libndb_la-ha_ndbcluster.o \
                        handler.o \
                        mysqld.o \
                        net_serv.o \
                        scheduler.o \
                        sp_head.o \
                        sql_cache.o \
                        sql_connect.o \
                        sql_cursor.o \
                        sql_delete.o \
                        sql_insert.o \
                        sql_parse.o \
                        sql_prepare.o \
                        sql_select.o \
                        sql_update.o

DTRACEFILES_DEPEND =    filesort.o \
			libndb_la-ha_ndbcluster.lo \
                        handler.o \
                        mysqld.o \
                        net_serv.o \
                        scheduler.o \
                        sp_head.o \
                        sql_cache.o \
                        sql_connect.o \
                        sql_cursor.o \
                        sql_delete.o \
                        sql_insert.o \
                        sql_parse.o \
                        sql_prepare.o \
                        sql_select.o \
                        sql_update.o


noinst_LTLIBRARIES=	libndb.la \
			udf_example.la

SUPPORTING_LIBS =	$(top_builddir)/vio/libvio.a \
			$(top_builddir)/mysys/libmysys.a \
			$(top_builddir)/dbug/libdbug.a \
			$(top_builddir)/regex/libregex.a \
			$(top_builddir)/strings/libmystrings.a
mysqld_DEPENDENCIES=	@mysql_plugin_libs@ $(SUPPORTING_LIBS) libndb.la
LDADD = $(SUPPORTING_LIBS) @ZLIB_LIBS@ @NDB_SCI_LIBS@
mysqld_LDADD =		libndb.la \
			@MYSQLD_EXTRA_LDFLAGS@ \
			@pstack_libs@ \
			@mysql_plugin_libs@ \
			$(LDADD)  $(CXXLDFLAGS) $(WRAPLIBS) @LIBDL@ \
			$(yassl_libs) $(openssl_libs) @MYSQLD_EXTRA_LIBS@

noinst_HEADERS =	item.h item_func.h item_sum.h item_cmpfunc.h \
			item_strfunc.h item_timefunc.h \
			item_xmlfunc.h sql_plugin_services.h \
			item_create.h item_subselect.h item_row.h \
			mysql_priv.h item_geofunc.h sql_bitmap.h \
			procedure.h sql_class.h sql_lex.h sql_list.h \
			sql_map.h sql_string.h unireg.h \
			sql_error.h field.h handler.h mysqld_suffix.h \
			sql_profile.h \
			ha_ndbcluster.h ha_ndbcluster_cond.h \
			ha_ndbcluster_binlog.h ha_ndbcluster_tables.h \
			ha_partition.h rpl_constants.h \
			debug_sync.h \
			opt_range.h protocol.h rpl_tblmap.h rpl_utility.h \
			rpl_reporting.h \
			log.h sql_show.h rpl_rli.h rpl_mi.h \
			sql_select.h structs.h table.h sql_udf.h hash_filo.h \
			lex.h lex_symbol.h sql_acl.h sql_crypt.h  \
			sql_repl.h slave.h rpl_filter.h rpl_injector.h \
			log_event.h rpl_record.h \
			log_event_old.h rpl_record_old.h \
			sql_sort.h sql_cache.h set_var.h \
			spatial.h gstream.h client_settings.h tzfile.h \
			tztime.h my_decimal.h\
			sp_head.h sp_pcontext.h sp_rcontext.h sp.h sp_cache.h \
			parse_file.h sql_view.h	sql_trigger.h \
			sql_array.h sql_cursor.h events.h scheduler.h \
                        event_db_repository.h event_queue.h \
			sql_plugin.h authors.h event_parse_data.h \
			event_data_objects.h event_scheduler.h \
			sql_partition.h partition_info.h partition_element.h \
			contributors.h sql_servers.h sql_signal.h records.h \
			sql_prepare.h rpl_handler.h replication.h

mysqld_SOURCES =	sql_lex.cc sql_handler.cc sql_partition.cc \
			item.cc item_sum.cc item_buff.cc item_func.cc \
			item_cmpfunc.cc item_strfunc.cc item_timefunc.cc \
			thr_malloc.cc item_create.cc item_subselect.cc \
			item_row.cc item_geofunc.cc item_xmlfunc.cc \
			field.cc strfunc.cc key.cc sql_class.cc sql_list.cc \
			net_serv.cc protocol.cc sql_state.c \
			lock.cc my_lock.c \
			sql_string.cc sql_manager.cc sql_map.cc \
			mysqld.cc password.c hash_filo.cc hostname.cc \
			sql_connect.cc scheduler.cc sql_parse.cc \
			set_var.cc sql_yacc.yy \
			sql_base.cc table.cc sql_select.cc sql_insert.cc \
			sql_profile.cc \
			sql_prepare.cc sql_error.cc sql_locale.cc \
			sql_update.cc sql_delete.cc uniques.cc sql_do.cc \
			procedure.cc sql_test.cc \
			log.cc init.cc derror.cc sql_acl.cc \
			unireg.cc des_key_file.cc \
			log_event.cc rpl_record.cc \
			log_event_old.cc rpl_record_old.cc \
			discover.cc time.cc opt_range.cc opt_sum.cc \
		   	records.cc filesort.cc handler.cc \
		        ha_partition.cc \
			debug_sync.cc \
			sql_db.cc sql_table.cc sql_rename.cc sql_crypt.cc \
			sql_load.cc mf_iocache.cc field_conv.cc sql_show.cc \
			sql_udf.cc sql_analyse.cc sql_analyse.h sql_cache.cc \
			slave.cc sql_repl.cc rpl_filter.cc rpl_tblmap.cc \
			rpl_utility.cc rpl_injector.cc rpl_rli.cc rpl_mi.cc \
			rpl_reporting.cc \
                        sql_union.cc sql_derived.cc \
			sql_client.cc \
			repl_failsafe.h repl_failsafe.cc \
			sql_olap.cc sql_view.cc \
			gstream.cc spatial.cc sql_help.cc sql_cursor.cc \
			tztime.cc my_decimal.cc\
			sp_head.cc sp_pcontext.cc  sp_rcontext.cc sp.cc \
			sp_cache.cc parse_file.cc sql_trigger.cc \
                        event_scheduler.cc event_data_objects.cc \
                        event_queue.cc event_db_repository.cc events.cc \
			sql_plugin.cc sql_binlog.cc \
			sql_builtin.cc sql_tablespace.cc partition_info.cc \
			sql_servers.cc event_parse_data.cc sql_signal.cc \
			rpl_handler.cc

nodist_mysqld_SOURCES =	mini_client_errors.c pack.c client.c my_time.c my_user.c 

libndb_la_CPPFLAGS=	@ndbcluster_includes@
libndb_la_SOURCES=	ha_ndbcluster.cc \
			ha_ndbcluster_binlog.cc \
			ha_ndbcluster_cond.cc

gen_lex_hash_SOURCES =	gen_lex_hash.cc
gen_lex_hash_LDFLAGS =  @NOINST_LDFLAGS@

mysql_tzinfo_to_sql_SOURCES = tztime.cc
mysql_tzinfo_to_sql_CXXFLAGS= -DTZINFO2SQL

DEFS =			-DMYSQL_SERVER \
			-DDEFAULT_MYSQL_HOME="\"$(MYSQLBASEdir)\"" \
			-DMYSQL_DATADIR="\"$(MYSQLDATAdir)\"" \
			-DSHAREDIR="\"$(MYSQLSHAREdir)\"" \
			-DPLUGINDIR="\"$(pkgplugindir)\"" \
			-DHAVE_EVENT_SCHEDULER \
			@DEFS@

BUILT_MAINT_SRC =	sql_yacc.cc sql_yacc.h
BUILT_SOURCES =		$(BUILT_MAINT_SRC) lex_hash.h link_sources
EXTRA_DIST =		udf_example.c udf_example.def $(BUILT_MAINT_SRC) \
			nt_servc.cc nt_servc.h \
			message.mc  message.h message.rc MSG00001.bin \
			CMakeLists.txt

CLEANFILES =        	lex_hash.h sql_yacc.output link_sources
DISTCLEANFILES =        $(EXTRA_PROGRAMS)
MAINTAINERCLEANFILES =  $(BUILT_MAINT_SRC)
AM_YFLAGS =		-d --verbose

# These are listed in 'nodist_mysqld_SOURCES'
link_sources:
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
	echo timestamp > link_sources

# This generates lex_hash.h
# NOTE Built sources should depend on their sources not the tool
# this avoid the rebuild of the built files in a source dist
lex_hash.h:	gen_lex_hash.cc lex.h
		$(MAKE) $(AM_MAKEFLAGS) gen_lex_hash$(EXEEXT)
		./gen_lex_hash$(EXEEXT) > $@-t
		$(MV) $@-t $@

# For testing of udf_example.so
udf_example_la_SOURCES= udf_example.c
udf_example_la_LDFLAGS= -module -rpath $(pkglibdir)

# We might have some stuff not built in this build, but that we want to install
install-exec-hook:
	$(mkinstalldirs) $(DESTDIR)$(libexecdir) $(DESTDIR)$(pkglibdir)
	test ! -x mysqld-debug$(EXEEXT) || $(INSTALL_PROGRAM) mysqld-debug$(EXEEXT) $(DESTDIR)$(libexecdir)
	test ! -f mysqld-debug.sym.gz   || $(INSTALL_DATA)    mysqld-debug.sym.gz   $(DESTDIR)$(pkglibdir)
	test ! -f mysqld.sym.gz         || $(INSTALL_DATA)    mysqld.sym.gz         $(DESTDIR)$(pkglibdir)

if HAVE_DTRACE_DASH_G
libndb_la_LIBADD = probes_libndb.o
libndb_la_DEPENDENCIES = dtrace_files dtrace_providers probes_libndb.o
mysqld_LDADD += probes_all.o
mysqld_DEPENDENCIES += dtrace_files dtrace_providers probes_all.o
CLEANFILES += dtrace_files dtrace_providers probes_all.o
DTRACEPROVIDER = probes_mysql.d
CLEANFILES += $(DTRACEPROVIDER)

dtrace_files:
	echo $(DTRACEFILES) > $@
dtrace_providers: probes_mysql.d
	echo $(DTRACEPROVIDER) > $@
probes_mysql.d:
	-$(RM) -f probes_mysql.d
	$(CP) $(top_srcdir)/include/probes_mysql.d.base probes_mysql.d

DTRACEDIRS = . ../mysys $(patsubst %,$(top_builddir)/storage/%,@mysql_se_dirs@)

probes_all.o: probes_mysql.d $(DTRACEFILES_DEPEND)
	providers=`(for i in $(DTRACEDIRS); do cat $$i/dtrace_providers 2>/dev/null; done) | tr " " "\n" | sort | uniq | sed -e '/^$$/d' -e 's/^/-s /'`; \
	objects=`for i in $(DTRACEDIRS); do f=\`cat $$i/dtrace_files 2>/dev/null\`; for j in $$f; do test -f $$i/$$j && echo "$$i/$$j "; done; done`; \
	$(DTRACE) $(DTRACEFLAGS) -G $$providers $$objects -o $@

# Can't depend directly on .libs/*.o, because there is no generated rule for
# that in the Makefile; it is a byproduct of *.lo
probes_libndb.o: probes_mysql.d libndb_la-ha_ndbcluster.lo
	if test -f .libs/libndb_la-ha_ndbcluster.o ; then \
		$(DTRACE) $(DTRACEFLAGS) -G -s probes_mysql.d .libs/libndb_la-ha_ndbcluster.o -o $@; \
	fi; \
	if test -f libndb_la-ha_ndbcluster.o ; then \
		$(DTRACE) $(DTRACEFLAGS) -G -s probes_mysql.d libndb_la-ha_ndbcluster.o -o $@; \
	fi

endif

# Don't update the files from bitkeeper
%::SCCS/s.%
