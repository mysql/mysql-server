
noinst_LTLIBRARIES = libmgmapi.la libMGM_API.la

libmgmapi_la_SOURCES_loc = mgmapi.cpp

libmgmapi_la_SOURCES = $(libmgmapi_la_SOURCES_loc)
libMGM_API_la_SOURCES = $(libmgmapi_la_SOURCES_loc)

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_util.mk.am
INCLUDES += -I$(top_srcdir)/ndb/include/mgmapi -I$(top_srcdir)/ndb/src/common/mgmcommon

DEFS += -DNO_DEBUG_MESSAGES

libMGM_API_la_LIBADD = \
              $(top_srcdir)/ndb/src/common/portlib/unix/libportlib.la      \
              $(top_srcdir)/ndb/src/common/util/libgeneral.la 

# Don't update the files from bitkeeper
%::SCCS/s.%
