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

# This file is public domain and comes with NO WARRANTY of any kind

MYSQLDATAdir =			$(localstatedir)
MYSQLSHAREdir =			$(pkgdatadir)
MYSQLBASEdir=			$(prefix)
INCLUDES =			-I$(srcdir)/../include -I../include \
				-I$(srcdir)/.. -I$(top_srcdir) -I..
LIBS =				@CLIENT_LIBS@
LDADD =				@CLIENT_EXTRA_LDFLAGS@ libmysqlclient.la
pkglib_LTLIBRARIES =		libmysqlclient.la

noinst_PROGRAMS = conf_to_src

# We need .lo, not .o files for everything.
CHARSET_OBJS=@CHARSET_OBJS@
LTCHARSET_OBJS= ${CHARSET_OBJS:.o=.lo}

libmysqlclient_la_SOURCES = libmysql.c net.c violite.c password.c \
			    get_password.c errmsg.c

mystringsobjects =	strmov.lo strxmov.lo strnmov.lo strmake.lo strend.lo \
			strnlen.lo strfill.lo is_prefix.lo \
			int2str.lo str2int.lo strinstr.lo strcont.lo \
			strcend.lo \
			bchange.lo bmove.lo bmove_upp.lo longlong2str.lo \
			strtoull.lo strtoll.lo llstr.lo strto.lo \
			ctype.lo $(LTCHARSET_OBJS)
mystringsextra= 	strto.c ctype_autoconf.c
dbugobjects =		dbug.lo # IT IS IN SAFEMALLOC.C sanity.lo
mysysheaders =		mysys_priv.h my_static.h
mysysobjects1 =		my_init.lo my_static.lo my_malloc.lo my_realloc.lo \
			my_create.lo my_delete.lo my_tempnam.lo my_open.lo \
			mf_casecnv.lo my_read.lo my_write.lo errors.lo \
			my_error.lo my_getwd.lo my_div.lo \
			mf_pack.lo my_messnc.lo mf_dirname.lo mf_fn_ext.lo\
			mf_wcomp.lo typelib.lo safemalloc.lo my_alloc.lo \
			mf_format.lo mf_path.lo mf_unixpath.lo my_fopen.lo \
			mf_loadpath.lo my_pthread.lo my_thr_init.lo \
			thr_mutex.lo mulalloc.lo string.lo default.lo \
			my_compress.lo array.lo my_once.lo list.lo my_net.lo \
                        charset.lo
# Not needed in the minimum library
mysysobjects2 =		getopt.lo getopt1.lo getvar.lo my_lib.lo
mysysobjects =		$(mysysobjects1) $(mysysobjects2)
libmysqlclient_la_LIBADD = $(mysysobjects) $(mystringsobjects) $(dbugobjects)
libmysqlclient_la_LDFLAGS = -version-info @SHARED_LIB_VERSION@
CLEANFILES =		$(libmysqlclient_la_LIBADD) $(SHLIBOBJS) \
			libmysqclient.la
DEFS =			-DUNDEF_THREADS_HACK \
			-DDEFAULT_CHARSET_HOME="\"$(MYSQLBASEdir)\"" \
			-DDATADIR="\"$(MYSQLDATAdir)\"" \
			-DSHAREDIR="\"$(MYSQLSHAREdir)\""

# The automatic dependencies miss this
bmove_upp.lo:		$(LTCHARSET_OBJS)
ctype.c:		ctype_extra_sources.c

# This is called from the toplevel makefile
link_sources:
	  set -x; \
	  ss=`echo $(mystringsobjects) | sed "s;\.lo;.c;g"`; \
	  ds=`echo $(dbugobjects) | sed "s;\.lo;.c;g"`; \
	  ms=`echo $(mysysobjects) | sed "s;\.lo;.c;g"`; \
	  for f in $$ss; do \
	    rm -f $(srcdir)/$$f; \
	    @LN_CP_F@ $(srcdir)/../strings/$$f $(srcdir)/$$f; \
	  done; \
	  for f in $(mystringsextra); do \
	    rm -f $(srcdir)/$$f; \
	    @LN_CP_F@ $(srcdir)/../strings/$$f $(srcdir)/$$f; \
	  done; \
	  for f in $$ds; do \
	    rm -f $(srcdir)/$$f; \
	    @LN_CP_F@ $(srcdir)/../dbug/$$f $(srcdir)/$$f; \
	  done; \
	  for f in $$ms $(mysysheaders); do \
	    rm -f $(srcdir)/$$f; \
	    @LN_CP_F@ $(srcdir)/../mysys/$$f $(srcdir)/$$f; \
	  done;

clean-local:
	rm -f `echo $(mystringsobjects) | sed "s;\.lo;.c;g"` \
	      `echo $(dbugobjects) | sed "s;\.lo;.c;g"` \
	      `echo $(mysysobjects) | sed "s;\.lo;.c;g"` \
	       $(mystringsextra) ctype_extra_sources.c \
	       ../linked_client_sources

ctype_extra_sources.c: conf_to_src
	./conf_to_src $(top_srcdir) @CHARSETS_NEED_SOURCE@ > \
	              $(srcdir)/ctype_extra_sources.c
conf_to_src_SOURCES = conf_to_src.c
conf_to_src_LDADD=

# This part requires GNUmake
#
# This makes a distribution file with only the files needed to compile
# a minimal MySQL client library
#
# For a really minimal distribution (without debugging code) we could
# keep only the stubs for safemalloc.c and debug.c
#
# A list of needed headers collected from the deps information 000213
nh =		global.h config-win32.h dbug.h errmsg.h global.h \
		m_ctype.h m_string.h \
		my_alarm.h my_config.h my_dir.h my_list.h my_net.h my_sys.h \
		mysql.h mysql_com.h mysql_version.h mysqld_error.h mysys_err.h \
		my_pthread.h thr_alarm.h violite.h
# Get a list of the needed objects  
lobjs = $(mysysobjects1) $(dbugobjects) $(mystringsobjects)

do-lib-dist:
	dir=libmysql-$(MYSQL_NO_DASH_VERSION); \
	srcs1=`echo $(lobjs) | sed "s;\.lo;.c;g"`; \
	srcs2=$(libmysqlclient_la_SOURCES); \
	srcs="$$srcs1 $$srcs2"; \
	objs1=`echo $(lobjs) | sed "s;\.lo;.o;g"`; \
	objs2=`echo $(libmysqlclient_la_SOURCES) | sed "s;\.c;.o;g"`; \
	objs="$$objs1 $$objs2"; \
	rm -rf $$dir; \
	mkdir $$dir; \
	cp $$srcs $(mysysheaders) $$dir; \
	for i in $(nh); do cp ../include/$$i $$dir; done; \
	echo "# A very minimal Makefile to compile" > $$dir/Makefile; \
	echo "# the minimized libmysql library" >> $$dir/Makefile; \
	echo "# This file is autogenerated from Makefile.am" >> $$dir/Makefile; \
	echo 'CFLAGS=	-I. -DUNDEF_THREADS_HACK' >>$$dir/Makefile; \
	echo "obj=$$objs"  >>$$dir/Makefile; \
	echo 'all: libmysql.a'  >>$$dir/Makefile; \
	echo 'libmysql.a: $$(obj)'  >>$$dir/Makefile; \
	echo '	$$(AR) r $$@ $$?'  >>$$dir/Makefile; \
	gtar cvzf $$dir.tar.gz $$dir; \
	cd $$dir; gmake

# Don't update the files from bitkeeper
%::SCCS/s.%
