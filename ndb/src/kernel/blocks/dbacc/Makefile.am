
noinst_LIBRARIES = libdbacc.a

libdbacc_a_SOURCES = DbaccInit.cpp DbaccMain.cpp

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_kernel.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%
