noinst_PROGRAMS =	mysqltest mysql client_test
client_sources =	$(mysqltest_SOURCES) $(mysql_SOURCES)
tests_sources= $(client_test_SOURCES)

link_sources:
	for f in $(client_sources); do \
	  rm -f $(srcdir)/$$f; \
	  @LN_CP_F@ $(srcdir)/../../client/$$f $(srcdir)/$$f; \
	done;
	for f in $(tests_sources); do \
	  rm -f $(srcdir)/$$f; \
	  @LN_CP_F@ $(srcdir)/../../tests/$$f $(srcdir)/$$f; \
	done;

DEFS = 		-DEMBEDDED_LIBRARY
INCLUDES =	@MT_INCLUDES@ -I$(top_srcdir)/include -I$(srcdir) \
		-I$(top_srcdir) -I$(top_srcdir)/client -I$(top_srcdir)/regex \
		$(openssl_includes)
LIBS =		@LIBS@ @WRAPLIBS@ @CLIENT_LIBS@
LDADD =		@CLIENT_EXTRA_LDFLAGS@ ../libmysqld.a @innodb_system_libs@ @LIBDL@ $(CXXLDFLAGS)

mysqltest_LINK = $(CXXLINK)
mysqltest_SOURCES =	mysqltest.c
mysqltest_LDADD =	$(LDADD) $(top_builddir)/regex/libregex.a

mysql_SOURCES = mysql.cc readline.cc completion_hash.cc \
                my_readline.h sql_string.h completion_hash.h
mysql_LDADD = @readline_link@ @TERMCAP_LIB@ $(LDADD)

client_test_LINK = $(CXXLINK)
client_test_SOURCES =	client_test.c

clean:
	rm -f $(client_sources)
	rm -f $(tests_sources)

# Don't update the files from bitkeeper
%::SCCS/s.%
