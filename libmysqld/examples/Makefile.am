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
LIBS =		@LIBS@
LDADD =		../libmysqld.a @innodb_system_libs@ @LIBDL@

mysqltest_SOURCES =	mysqltest.c

mysql_SOURCES = mysql.cc readline.cc completion_hash.cc \
                my_readline.h sql_string.h completion_hash.h
mysql_LDADD = @readline_link@ @TERMCAP_LIB@ $(LDADD)

clean:
	rm -f $(client_sources)

# Don't update the files from bitkeeper
%::SCCS/s.%
