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

MYSQLDATAdir =		$(localstatedir)
MYSQLSHAREdir =		$(pkgdatadir)
MYSQLBASEdir=		$(prefix)

DEFS =			-DEMBEDDED_LIBRARY -DMYSQL_SERVER \
			-DDEFAULT_MYSQL_HOME="\"$(MYSQLBASEdir)\"" \
			-DDATADIR="\"$(MYSQLDATAdir)\"" \
			-DSHAREDIR="\"$(MYSQLSHAREdir)\""
INCLUDES=		@MT_INCLUDES@ @bdb_includes@ -I$(srcdir)/../include \
			-I../include -I$(srcdir)/.. -I$(top_srcdir) -I.. \
			-I../sql -I../regex

noinst_LIBRARIES =	libmysqld_int.a
pkglib_LIBRARIES =	libmysqld.a
SUBDIRS =		. examples
libmysqld_sources=	libmysqld.c lib_sql.cc lib_load.cc
libmysqlsources =	errmsg.c get_password.c password.c

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

EXTRA_DIST = lib_vio.c

libmysqld_int_a_SOURCES= $(libmysqld_sources) $(libmysqlsources) $(sqlsources)
libmysqld_a_SOURCES=

# automake misses these
sql_yacc.cc sql_yacc.h: $(top_srcdir)/sql/sql_yacc.yy

# The following libraries should be included in libmysqld.a
INC_LIB=	$(top_builddir)/regex/libregex.a \
		$(top_builddir)/myisam/libmyisam.a \
		$(top_builddir)/myisammrg/libmyisammrg.a \
		$(top_builddir)/heap/libheap.a \
		@innodb_libs@ @bdb_libs_with_path@ \
		$(top_builddir)/mysys/libmysys.a \
		$(top_builddir)/strings/libmystrings.a \
		$(top_builddir)/dbug/libdbug.a \
		$(top_builddir)/regex/libregex.a

#
# To make it easy for the end user to use the embedded library we 
# generate a total libmysqld.a from all library files, 

libmysqld.a:	libmysqld_int.a $(INC_LIB)
		if test ! -d tmp ; then mkdir tmp ; fi
		rm -f $@ libmysqld_int2.a tmp/*.o tmp/*.a
		cp $(INC_LIB) tmp
		cp libmysqld_int.a libmysqld_int2.a ; \
		cd tmp ; \
		for file in *.a ; do \
		  bfile=`basename $$file .a` ; \
		  ar x $$file; \
		  for obj in *.o ; do mv $$obj $${bfile}_$$obj ; done ; \
		  ar q ../libmysqld_int2.a *.o ; \
		  rm *.o ; \
		done
		mv libmysqld_int2.a libmysqld.a
		rm tmp/*
		$(RANLIB) libmysqld.a

## XXX: any time the client interface changes, we'll need to bump
## the version info for libmysqld; however, it's possible for the
## libmysqld interface to change without affecting the standard
## libmysqlclient interface.  Should we make a separate version
## string for the two?
#libmysqld_la_LDFLAGS = -version-info @SHARED_LIB_VERSION@
#CLEANFILES =		$(libmysqld_la_LIBADD) libmysqld.la

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
