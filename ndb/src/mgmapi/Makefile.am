
noinst_LTLIBRARIES = libmgmapi.la libMGM_API.la

libmgmapi_la_SOURCES_loc = mgmapi.cpp

libmgmapi_la_SOURCES = $(libmgmapi_la_SOURCES_loc)
libMGM_API_la_SOURCES = $(libmgmapi_la_SOURCES_loc)

INCLUDES_LOC = -I$(top_srcdir)/ndb/include/mgmapi -I$(top_srcdir)/ndb/src/common/mgmcommon
DEFS_LOC = -DNO_DEBUG_MESSAGES

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_util.mk.am

libMGM_API_la_LIBADD = \
              $(top_srcdir)/ndb/src/common/portlib/unix/libportlib.la      \
              $(top_srcdir)/ndb/src/common/util/libgeneral.la 

# Don't update the files from bitkeeper
%::SCCS/s.%
