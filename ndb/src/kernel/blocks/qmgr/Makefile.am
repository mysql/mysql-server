noinst_LIBRARIES = libqmgr.a

libqmgr_a_SOURCES = \
	QmgrInit.cpp	\
	QmgrMain.cpp

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_kernel.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%
