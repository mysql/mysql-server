noinst_LIBRARIES = libgrep.a

libgrep_a_SOURCES = Grep.cpp GrepInit.cpp 

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_kernel.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%
