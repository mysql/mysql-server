# Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
# 
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
# 
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
# 
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the Free
# Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
# MA 02111-1307, USA
#
# This file is public domain and comes with NO WARRANTY of any kind

MYSQLDATAdir =			$(localstatedir)
MYSQLSHAREdir =			$(pkgdatadir)
MYSQLBASEdir=			$(prefix)

DEFS =			-DEMBEDDED_LIBRARY -DMYSQL_SERVER \
			-DDEFAULT_MYSQL_HOME="\"$(MYSQLBASEdir)\"" \
			-DDATADIR="\"$(MYSQLDATAdir)\"" \
			-DSHAREDIR="\"$(MYSQLSHAREdir)\""
INCLUDES = @MT_INCLUDES@ @bdb_includes@ -I$(srcdir)/../include -I../include \
	-I$(srcdir)/.. -I$(top_srcdir) -I.. -I../sql -I../regex


## XXX: should we use client or server LDFLAGS for libmysqld?
LDADD =			@CLIENT_EXTRA_LDFLAGS@ libmysqld.la
pkglib_LTLIBRARIES =	libmysqld.la
SUBDIRS =		. examples
libmysqld_la_SOURCES=	libmysqld.c lib_sql.cc lib_load.cc

libmysqlsources =	errmsg.c get_password.c password.c
## XXX: we should not have to duplicate info from the sources list
libmysqlobjects =	errmsg.lo get_password.lo password.lo

noinst_HEADERS =	embedded_priv.h

sqlsources = convert.cc derror.cc field.cc field_conv.cc filesort.cc \
	ha_innobase.cc ha_berkeley.cc ha_heap.cc ha_isam.cc ha_isammrg.cc \
	ha_myisam.cc ha_myisammrg.cc handler.cc sql_handler.cc \
	hostname.cc init.cc \
	item.cc item_buff.cc item_cmpfunc.cc item_create.cc \
	item_func.cc item_strfunc.cc item_sum.cc item_timefunc.cc \
	item_uniq.cc key.cc lock.cc log.cc log_event.cc \
	mini_client.cc net_pkg.cc net_serv.cc opt_ft.cc opt_range.cc \
	opt_sum.cc procedure.cc records.cc slave.cc sql_acl.cc \
	sql_analyse.cc sql_base.cc sql_cache.cc sql_class.cc \
	sql_crypt.cc sql_db.cc sql_delete.cc sql_insert.cc sql_lex.cc \
	sql_list.cc sql_manager.cc sql_map.cc sql_parse.cc \
	sql_rename.cc sql_repl.cc sql_select.cc sql_show.cc \
	sql_string.cc sql_table.cc sql_test.cc sql_udf.cc \
	sql_update.cc sql_yacc.cc table.cc thr_malloc.cc time.cc \
	unireg.cc uniques.cc stacktrace.c sql_union.cc hash_filo.cc

## XXX: we should not have to duplicate info from the sources list
sqlobjects = convert.lo derror.lo field.lo field_conv.lo filesort.lo \
	ha_innobase.lo ha_berkeley.lo ha_heap.lo ha_isam.lo ha_isammrg.lo \
	ha_myisam.lo ha_myisammrg.lo handler.lo sql_handler.lo \
	hostname.lo init.lo \
	item.lo item_buff.lo item_cmpfunc.lo item_create.lo \
	item_func.lo item_strfunc.lo item_sum.lo item_timefunc.lo \
	item_uniq.lo key.lo lock.lo log.lo log_event.lo  \
	mini_client.lo net_pkg.lo net_serv.lo opt_ft.lo opt_range.lo \
	opt_sum.lo procedure.lo records.lo slave.lo sql_acl.lo \
	sql_analyse.lo sql_base.lo sql_cache.lo sql_class.lo \
	sql_crypt.lo sql_db.lo sql_delete.lo sql_insert.lo sql_lex.lo \
	sql_list.lo sql_manager.lo sql_map.lo sql_parse.lo \
	sql_rename.lo sql_repl.lo sql_select.lo sql_show.lo \
	sql_string.lo sql_table.lo sql_test.lo sql_udf.lo \
	sql_update.lo sql_yacc.lo table.lo thr_malloc.lo time.lo \
	unireg.lo uniques.lo stacktrace.lo sql_union.lo hash_filo.lo

EXTRA_DIST = lib_vio.c

# automake misses these
sql_yacc.cc sql_yacc.h: $(top_srcdir)/sql/sql_yacc.yy

libmysqld_la_LIBADD = $(sqlobjects) $(libmysqlobjects)
## XXX: any time the client interface changes, we'll need to bump
## the version info for libmysqld; however, it's possible for the
## libmysqld interface to change without affecting the standard
## libmysqlclient interface.  Should we make a separate version
## string for the two?
libmysqld_la_LDFLAGS = -version-info @SHARED_LIB_VERSION@
CLEANFILES =		$(libmysqld_la_LIBADD) libmysqld.la

# This is called from the toplevel makefile
link_sources:
	  set -x; \
	  for f in $(sqlsources); do \
	    rm -f $(srcdir)/$$f; \
	    @LN_CP_F@ $(srcdir)/../sql/$$f $(srcdir)/$$f; \
	  done; \
	  for f in $(libmysqlsources); do \
	    rm -f $(srcdir)/$$f; \
	    @LN_CP_F@ $(srcdir)/../libmysql/$$f $(srcdir)/$$f; \
	  done

clean-local:
	rm -f `echo $(sqlsources) $(libmysqlsources) | sed "s;\.lo;.c;g"` \
	       $(top_srcdir)/linked_libmysqld_sources

# Don't update the files from bitkeeper
%::SCCS/s.%
