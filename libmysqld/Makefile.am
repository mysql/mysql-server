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
MYSQLLIBdir=            $(libdir)

EXTRA_DIST =		libmysqld.def
DEFS =			-DEMBEDDED_LIBRARY -DMYSQL_SERVER \
			-DDEFAULT_MYSQL_HOME="\"$(MYSQLBASEdir)\"" \
			-DDATADIR="\"$(MYSQLDATAdir)\"" \
			-DSHAREDIR="\"$(MYSQLSHAREdir)\"" \
			-DLIBDIR="\"$(MYSQLLIBdir)\""
INCLUDES=		-I$(top_builddir)/include -I$(top_srcdir)/include \
			-I$(top_builddir)/include -I$(top_srcdir)/include \
			-I$(top_srcdir)/sql -I$(top_srcdir)/sql/examples \
			-I$(top_srcdir)/regex \
			$(openssl_includes) @ZLIB_INCLUDES@

noinst_LIBRARIES =	libmysqld_int.a
pkglib_LIBRARIES =	libmysqld.a
SUBDIRS =		. examples
libmysqld_sources=	libmysqld.c lib_sql.cc emb_qcache.cc
libmysqlsources =	errmsg.c get_password.c libmysql.c client.c pack.c \
                        my_time.c

noinst_HEADERS =	embedded_priv.h emb_qcache.h

sqlsources = derror.cc field.cc field_conv.cc strfunc.cc filesort.cc \
	ha_heap.cc ha_myisam.cc ha_myisammrg.cc \
	ha_innodb.cc ha_berkeley.cc ha_federated.cc ha_ndbcluster.cc \
	ha_ndbcluster_binlog.cc ha_partition.cc \
	handler.cc sql_handler.cc \
	hostname.cc init.cc password.c \
	item.cc item_buff.cc item_cmpfunc.cc item_create.cc \
	item_func.cc item_strfunc.cc item_sum.cc item_timefunc.cc \
	item_geofunc.cc item_uniq.cc item_subselect.cc item_row.cc\
	item_xmlfunc.cc \
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
	spatial.cc gstream.cc sql_help.cc tztime.cc sql_cursor.cc \
	sp_head.cc sp_pcontext.cc sp.cc sp_cache.cc sp_rcontext.cc \
	parse_file.cc sql_view.cc sql_trigger.cc my_decimal.cc \
	event_scheduler.cc events.cc event_timed.cc \
	rpl_filter.cc sql_partition.cc sql_builtin.cc sql_plugin.cc \
	sql_tablespace.cc \
	rpl_injector.cc my_user.c partition_info.cc

libmysqld_int_a_SOURCES= $(libmysqld_sources) $(libmysqlsources) $(sqlsources)
libmysqld_a_SOURCES=

sqlstoragesources =	$(EXTRA_libmysqld_a_SOURCES)

# automake misses these
sql_yacc.cc sql_yacc.h: $(top_srcdir)/sql/sql_yacc.yy

# The following libraries should be included in libmysqld.a
INC_LIB=	$(top_builddir)/regex/libregex.a \
		$(top_builddir)/mysys/libmysys.a \
		$(top_builddir)/strings/libmystrings.a \
		$(top_builddir)/dbug/libdbug.a \
		$(top_builddir)/vio/libvio.a \
		@mysql_plugin_libs@ \
		$(yassl_inc_libs)

if HAVE_YASSL
yassl_inc_libs=	$(top_srcdir)/extra/yassl/src/.libs/libyassl.a \
		$(top_srcdir)/extra/yassl/taocrypt/src/.libs/libtaocrypt.a
endif

# Storage engine specific compilation options

ha_berkeley.o:  ha_berkeley.cc
		$(CXXCOMPILE) @bdb_includes@ $(LM_CFLAGS) -c $<

ha_ndbcluster.o:ha_ndbcluster.cc
		$(CXXCOMPILE) @ndbcluster_includes@ $(LM_CFLAGS) -c $<

ha_ndbcluster_binlog.o: ha_ndbcluster_binlog.cc
		$(CXXCOMPILE) @ndbcluster_includes@ $(LM_CFLAGS) -c $<

# Until we can remove dependency on ha_ndbcluster.h
handler.o:	handler.cc
		$(CXXCOMPILE) @ndbcluster_includes@ $(LM_CFLAGS) -c $<


#
# To make it easy for the end user to use the embedded library we
# generate a total libmysqld.a from all library files,

# note - InnoDB libraries have circular dependencies, so in INC_LIB
# few libraries are present two times. Metrowerks linker doesn't like
# it at all. Traditional ar has no problems with it, but still there's no
# need to add the same file twice to the library, so 'sort -u' save us
# some time and spares unnecessary work.

libmysqld.a:	libmysqld_int.a $(INC_LIB) $(libmysqld_a_DEPENDENCIES)
if DARWIN_MWCC
	mwld -lib -o $@ libmysqld_int.a `echo $(INC_LIB) | sort -u`
else
		-rm -f libmysqld.a
		if test "$(host_os)" = "netware" ; \
		then \
		  $(libmysqld_a_AR) libmysqld.a libmysqld_int.a $(INC_LIB) ; \
		else \
		  current_dir=`pwd`; \
		  rm -rf tmp; mkdir tmp; \
		  (for arc in ./libmysqld_int.a $(INC_LIB); do \
		    arpath=`echo $$arc|sed 's|[^/]*$$||'|sed 's|\.libs/$$||'`; \
		    artmp=`echo $$arc|sed 's|^.*/|tmp/lib-|'`; \
		    for F in `$(AR) t $$arc | grep -v SYMDEF`; do \
		      if test -e "$$arpath/$$F" ; then echo "$$arpath/$$F"; else \
		      mkdir $$artmp; cd $$artmp > /dev/null; \
		      $(AR) x ../../$$arc; \
		      cd $$current_dir > /dev/null; \
		      ls $$artmp/* | grep -v SYMDEF; \
		      continue 2; fi; done; \
		  done; echo $(libmysqld_a_DEPENDENCIES) ) | sort -u | xargs $(AR) cq libmysqld.a ; \
		  $(RANLIB) libmysqld.a	; \
		  rm -rf tmp; \
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
	  if test -n "$(sqlstoragesources)" ; \
	  then \
	    for f in "$(sqlstoragesources)"; do \
	      rm -f "$$f"; \
	      @LN_CP_F@ `find $(srcdir)/../sql -name "$$f"` "$$f"; \
	    done; \
	  fi; \
	  rm -f client_settings.h; \
	  @LN_CP_F@ $(top_srcdir)/libmysql/client_settings.h client_settings.h


clean-local:
	rm -f `echo $(sqlsources) $(libmysqlsources) $(sqlstoragesources) | sed "s;\.lo;.c;g"` \
	       $(top_srcdir)/linked_libmysqld_sources; \
	rm -f client_settings.h

# Don't update the files from bitkeeper
%::SCCS/s.%
