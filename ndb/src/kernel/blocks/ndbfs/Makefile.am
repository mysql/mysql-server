noinst_LIBRARIES = libndbfs.a

libndbfs_a_SOURCES = \
	AsyncFile.cpp			\
	Ndbfs.cpp VoidFs.cpp	\
	Filename.cpp			\
	CircularIndex.cpp

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_kernel.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%
