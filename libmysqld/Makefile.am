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
INCLUDES=		@MT_INCLUDES@ @bdb_includes@ \
			-I$(top_builddir)/include -I$(top_srcdir)/include \
			-I$(top_srcdir)/sql -I$(top_srcdir)/sql/examples -I$(top_srcdir)/regex \
			$(openssl_includes) @ZLIB_INCLUDES@

noinst_LIBRARIES =	libmysqld_int.a
pkglib_LIBRARIES =	libmysqld.a
SUBDIRS =		. examples
libmysqld_sources=	libmysqld.c lib_sql.cc emb_qcache.cc
libmysqlsources =	errmsg.c get_password.c libmysql.c client.c pack.c \
                        my_time.c
sqlexamplessources =	ha_example.cc ha_archive.cc ha_tina.cc

noinst_HEADERS =	embedded_priv.h emb_qcache.h

sqlsources = derror.cc field.cc field_conv.cc strfunc.cc filesort.cc \
	ha_innodb.cc ha_berkeley.cc ha_heap.cc ha_isam.cc ha_isammrg.cc \
	ha_myisam.cc ha_myisammrg.cc handler.cc sql_handler.cc \
	hostname.cc init.cc password.c \
	item.cc item_buff.cc item_cmpfunc.cc item_create.cc \
	item_func.cc item_strfunc.cc item_sum.cc item_timefunc.cc \
	item_geofunc.cc item_uniq.cc item_subselect.cc item_row.cc\
	key.cc lock.cc log.cc log_event.cc sql_state.c \
	protocol.cc net_serv.cc opt_range.cc \
	opt_sum.cc procedure.cc records.cc sql_acl.cc \
	sql_load.cc discover.cc sql_locale.cc \
	sql_analyse.cc sql_base.cc sql_cache.cc sql_class.cc \
	sql_crypt.cc sql_db.cc sql_delete.cc sql_error.cc sql_insert.cc \
	sql_lex.cc sql_list.cc sql_manager.cc sql_map.cc sql_parse.cc \
	sql_prepare.cc sql_derived.cc sql_rename.cc \
	sql_select.cc sql_do.cc sql_show.cc set_var.cc \
	sql_string.cc sql_table.cc sql_test.cc sql_udf.cc \
	sql_update.cc sql_yacc.cc table.cc thr_malloc.cc time.cc \
	unireg.cc uniques.cc stacktrace.c sql_union.cc hash_filo.cc \
	spatial.cc gstream.cc sql_help.cc tztime.cc \
	ha_blackhole.cc

libmysqld_int_a_SOURCES= $(libmysqld_sources) $(libmysqlsources) $(sqlsources) $(sqlexamplessources)
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
		$(top_builddir)/vio/libvio.a

#
# To make it easy for the end user to use the embedded library we
# generate a total libmysqld.a from all library files,

libmysqld.a:	libmysqld_int.a $(INC_LIB)
if DARWIN_MWCC
	mwld -lib -o $@ libmysqld_int.a `ls -1 $(INC_LIB) | sort -u`
else
		if test "$(host_os)" = "netware" ; \
		then \
		  $(libmysqld_a_AR) libmysqld.a libmysqld_int.a $(INC_LIB) ; \
		else \
		  if test ! -d tmp ; then mkdir tmp ; fi ; \
		  rm -f $@ libmysqld_int2.a tmp/*.o tmp/*.a ; \
		  cp $(INC_LIB) tmp ; \
		  cp libmysqld_int.a libmysqld_int2.a ; \
		  cd tmp ; \
		  for file in *.a ; do \
		    bfile=`basename $$file .a` ; \
		    $(AR) x $$file; \
		    for obj in *.o ; do mv $$obj $${bfile}_$$obj ; done ; \
		    $(AR) q ../libmysqld_int2.a *.o ; \
		    rm -f *.o ; \
		  done ; \
		  cd .. ; \
		  mv libmysqld_int2.a libmysqld.a ; \
		  rm -f tmp/* ; \
		  $(RANLIB) libmysqld.a	; \
		fi
endif

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
	    rm -f $$f; \
	    @LN_CP_F@ $(top_srcdir)/sql/$$f $$f; \
	  done; \
	  for f in $(libmysqlsources); do \
	    rm -f $$f; \
	    @LN_CP_F@ $(top_srcdir)/libmysql/$$f $$f; \
	  done; \
	  for f in $(sqlexamplessources); do \
	    rm -f $$f; \
	    @LN_CP_F@ $(top_srcdir)/sql/examples/$$f $$f; \
	  done; \
	  rm -f client_settings.h; \
	  @LN_CP_F@ $(top_srcdir)/libmysql/client_settings.h client_settings.h


clean-local:
	rm -f `echo $(sqlsources) $(libmysqlsources) $(sqlexamplessources) | sed "s;\.lo;.c;g"` \
	       $(top_srcdir)/linked_libmysqld_sources; \
	rm -f client_settings.h

# Don't update the files from bitkeeper
%::SCCS/s.%
