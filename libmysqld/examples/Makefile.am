noinst_PROGRAMS =	mysqltest mysql
client_sources =	$(mysqltest_SOURCES) $(mysql_SOURCES)

link_sources:
	for f in $(client_sources); do \
	  rm -f $(srcdir)/$$f; \
	  @LN_CP_F@ $(srcdir)/../../client/$$f $(srcdir)/$$f; \
	done;

DEFS = 		-DEMBEDDED_LIBRARY
INCLUDES =	-I$(top_srcdir)/include $(openssl_includes) \
		-I$(srcdir) -I$(top_srcdir) -I$(top_srcdir)/client
LIBS =	@LIBS@
LDADD =	$(top_builddir)/libmysqld/libmysqld.la \
	$(top_builddir)/isam/libnisam.a \
	$(top_builddir)/myisam/libmyisam.a \
	$(top_builddir)/heap/libheap.a \
	$(top_builddir)/merge/libmerge.a \
	$(top_builddir)/myisammrg/libmyisammrg.a \
	@innodb_libs@ @bdb_libs@ @pstack_libs@ \
	$(top_builddir)/mysys/libmysys.a \
	$(top_builddir)/strings/libmystrings.a \
	$(top_builddir)/dbug/libdbug.a \
	$(top_builddir)/regex/libregex.a @LIBDL@

mysqltest_DEPENDENCIES = ../libmysqld.la
mysqltest_SOURCES =	mysqltest.c

mysql_SOURCES = mysql.cc readline.cc completion_hash.cc \
                my_readline.h sql_string.h completion_hash.h
mysql_LDADD = @readline_link@ @TERMCAP_LIB@ $(LDADD)

clean:
	rm -f $(client_sources)

# Don't update the files from bitkeeper
%::SCCS/s.%
