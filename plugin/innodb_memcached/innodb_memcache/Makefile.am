CLEANFILES=
BUILT_SOURCES=

INCLUDES =  	-I../../../include -I../../../sql -I../../../regex -I../../../bld/include -I../../../storage/innobase/include

lib_LTLIBRARIES = innodb_engine.la
noinst_LTLIBRARIES = libmcd_util.la

innodb_engine_la_SOURCES= \
                    src/innodb_config.c \
                    src/atomics.c \
                    src/debug.c \
                    src/hash_item_util.c \
                    src/innodb_engine.c \
		    src/innodb_api.c \
                    src/embedded_default_engine.c \
                    cache-src/assoc.c \
                    cache-src/items.c \
                    cache-src/slabs.c

libmcd_util_la_SOURCES = \
                    util-src/config_parser.c \
                    util-src/extension_loggers.c \
                    util-src/util.c

innodb_engine_la_CPPFLAGS = -I${top_srcdir}/include -I${top_srcdir}/cache-src
innodb_engine_la_CXXFLAGS= $(NO_ERROR)
innodb_engine_la_DEPENDENCIES= libmcd_util.la
innodb_engine_la_LIBADD= libmcd_util.la 
innodb_engine_la_LDFLAGS= -avoid-version -shared -module
