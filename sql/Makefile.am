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
			@bdb_includes@ @innodb_includes@ \
			-I$(srcdir)/../include \
			-I$(srcdir)/../regex \
			-I$(srcdir) -I../include -I. $(openssl_includes)
WRAPLIBS=		@WRAPLIBS@
SUBDIRS =		share
libexec_PROGRAMS =	mysqld
noinst_PROGRAMS =	gen_lex_hash
gen_lex_hash_LDFLAGS =  @NOINST_LDFLAGS@
LDADD =			@isam_libs@ \
			../myisam/libmyisam.a \
			../myisammrg/libmyisammrg.a \
			../heap/libheap.a \
			../vio/libvio.a \
			../mysys/libmysys.a \
			../dbug/libdbug.a \
			../regex/libregex.a \
			../strings/libmystrings.a

mysqld_LDADD =		@MYSQLD_EXTRA_LDFLAGS@ \
			@bdb_libs@ @innodb_libs@ @pstack_libs@ \
                        @innodb_system_libs@ \
			$(LDADD)  $(CXXLDFLAGS) $(WRAPLIBS) @LIBDL@ @openssl_libs@
noinst_HEADERS =	item.h item_func.h item_sum.h item_cmpfunc.h \
			item_strfunc.h item_timefunc.h item_uniq.h \
			item_create.h mysql_priv.h \
			procedure.h sql_class.h sql_lex.h sql_list.h \
			sql_manager.h sql_map.h sql_string.h unireg.h \
			field.h handler.h mysqld_suffix.h \
			ha_isammrg.h ha_isam.h ha_myisammrg.h\
			ha_heap.h ha_myisam.h ha_berkeley.h ha_innodb.h \
			opt_range.h opt_ft.h \
			sql_select.h structs.h table.h sql_udf.h hash_filo.h\
			lex.h lex_symbol.h sql_acl.h sql_crypt.h  \
			log_event.h mini_client.h sql_repl.h slave.h \
			stacktrace.h sql_sort.h sql_cache.h set_var.h
mysqld_SOURCES =	sql_lex.cc sql_handler.cc \
			item.cc item_sum.cc item_buff.cc item_func.cc \
			item_cmpfunc.cc item_strfunc.cc item_timefunc.cc \
			thr_malloc.cc item_create.cc \
			field.cc key.cc sql_class.cc sql_list.cc \
			net_serv.cc net_pkg.cc lock.cc my_lock.c \
			sql_string.cc sql_manager.cc sql_map.cc \
			mysqld.cc password.c hash_filo.cc hostname.cc \
			convert.cc set_var.cc sql_parse.cc sql_yacc.yy \
			sql_base.cc table.cc sql_select.cc sql_insert.cc \
			sql_update.cc sql_delete.cc uniques.cc sql_do.cc \
			procedure.cc item_uniq.cc sql_test.cc \
			log.cc log_event.cc init.cc derror.cc sql_acl.cc \
			unireg.cc des_key_file.cc \
			time.cc opt_range.cc opt_sum.cc opt_ft.cc \
		   	records.cc filesort.cc handler.cc \
		        ha_heap.cc ha_myisam.cc ha_myisammrg.cc \
	                ha_berkeley.cc ha_innodb.cc \
			ha_isam.cc ha_isammrg.cc \
			sql_db.cc sql_table.cc sql_rename.cc sql_crypt.cc \
			sql_load.cc mf_iocache.cc field_conv.cc sql_show.cc \
			sql_udf.cc sql_analyse.cc sql_analyse.h sql_cache.cc \
			slave.cc sql_repl.cc sql_union.cc \
			mini_client.cc mini_client_errors.c \
			stacktrace.c repl_failsafe.h repl_failsafe.cc
gen_lex_hash_SOURCES =	gen_lex_hash.cc
gen_lex_hash_LDADD =	$(LDADD) $(CXXLDFLAGS)

DEFS =			-DMYSQL_SERVER \
			-DDEFAULT_MYSQL_HOME="\"$(MYSQLBASEdir)\"" \
			-DDATADIR="\"$(MYSQLDATAdir)\"" \
			-DSHAREDIR="\"$(MYSQLSHAREdir)\"" \
			@DEFS@
# Don't put lex_hash.h in BUILT_SOURCES as this will give infinite recursion
BUILT_SOURCES =		sql_yacc.cc sql_yacc.h
EXTRA_DIST =		udf_example.cc $(BUILT_SOURCES)
AM_YFLAGS =		-d

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

sql_yacc.o:	sql_yacc.cc sql_yacc.h $(noinst_HEADERS)
		@echo "Note: The following compile may take a long time."
		@echo "If it fails, re-run configure with --with-low-memory"
		$(CXXCOMPILE) $(LM_CFLAGS) -c $<

lex_hash.h:	lex.h gen_lex_hash.cc sql_yacc.h
		$(MAKE) gen_lex_hash$(EXEEXT)
		./gen_lex_hash$(EXEEXT) > $@

# Hack to ensure that lex_hash.h is built early
sql_lex.o:	lex_hash.h

# For testing of udf_example.so;  Works on platforms with gcc
# (This is not part of our build process but only provided as an example)
udf_example.so:	udf_example.cc
		$(CXXCOMPILE) -shared -o $@ $<

distclean:
		rm -f lex_hash.h

# Don't update the files from bitkeeper
%::SCCS/s.%
