# Please list files alphabetically in the lists to reduce the risk for
# a merge conflict.
#

MYSQLDATAdir =          $(localstatedir)
MYSQLSHAREdir =         $(pkgdatadir)
MYSQLBASEdir=           $(prefix)
MYSQLLIBdir=            $(pkglibdir)
pkgplugindir =          $(pkglibdir)/plugin

INCLUDES =              -I$(srcdir)/include/memcached -I../../../include -I../../../sql -I../../../bld/include -I../../../regex -I../../../include/mysql -I../libevent

# libmemcached_utilities.la needs to be listed first because some of the
# other modules depend on it
noinst_LTLIBRARIES = libmemcached_utilities.la 

# A collection of functions used by the various modules in memcached
libmemcached_utilities_la_CPPFLAGS = $(CPPFLAGS) -I$(srcdir)/utilities
libmemcached_utilities_la_LDFLAGS =-R '$(pkglibdir)' -R '$(libdir)'
libmemcached_utilities_la_SOURCES= \
                        engine_loader.h \
                        include/memcached/config_parser.h \
                        include/memcached/genhash.h \
                        include/memcached/util.h \
                        utilities/config_parser.c \
                        utilities/engine_loader.c \
                        utilities/extension_loggers.c \
                        utilities/genhash.c \
                        utilities/genhash_int.h \
                        utilities/util.c

pkglib_LTLIBRARIES =      libmemcached.la
pkgplugin_LTLIBRARIES = 

libmemcached_la_LDFLAGS =  -R '$(pkglibdir)' -R '$(libdir)' -L../libevent -levent
libmemcached_la_CPPFLAGS = $(CPPFLAGS) -I$(srcdir)/daemon
libmemcached_la_CFLAGS =   $(AM_CFLAGS) -DMYSQL_DYNAMIC_PLUGIN
libmemcached_la_DEPENDENCIES= libmemcached_utilities.la
libmemcached_la_LIBADD = libmemcached_utilities.la
libmemcached_la_CXXFLAGS=  $(AM_CXXFLAGS) -DMYSQL_DYNAMIC_PLUGIN

libmemcached_la_SOURCES = \
		    daemon/memcached_mysql.h \
		    daemon/memcached_mysql.cc \
                    daemon/cache.h \
		    daemon/cache.c \
                    config_static.h \
                    daemon/daemon.c \
                    daemon/hash.c \
                    daemon/hash.h \
                    daemon/memcached.c\
                    daemon/memcached.h \
                    daemon/sasl_defs.h \
                    daemon/stats.c \
                    daemon/stats.h \
                    daemon/thread.c \
                    daemon/topkeys.c \
                    daemon/topkeys.h \
                    daemon/trace.h

EXTRA_LIBRARIES =       libmemcached.a
noinst_LIBRARIES = 	libmemcached.a
libmemcached_a_CXXFLAGS =  $(AM_CXXFLAGS)
libmemcached_a_CFLAGS =    $(AM_CFLAGS)
libmemcached_a_DEPENDENCIES= libmemcached_utilities.la
libmemcached_a_LIBADD = libmemcached_utilities.la

libmemcached_a_SOURCES = \
		    daemon/memcached_mysql.h \
		    daemon/memcached_mysql.cc \
                    daemon/cache.h \
                    config_static.h \
                    daemon/daemon.c \
                    daemon/hash.c \
                    daemon/hash.h \
                    daemon/memcached.c\
                    daemon/memcached.h \
                    daemon/sasl_defs.h \
                    daemon/stats.c \
                    daemon/stats.h \
                    daemon/thread.c \
                    daemon/topkeys.c \
                    daemon/topkeys.h \
                    daemon/trace.h

