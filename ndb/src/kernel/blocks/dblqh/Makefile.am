#SUBDIRS = redoLogReader

noinst_LIBRARIES = libdblqh.a

libdblqh_a_SOURCES = DblqhInit.cpp DblqhMain.cpp

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_kernel.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%
