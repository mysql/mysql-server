noinst_LIBRARIES = libndbcntr.a

libndbcntr_a_SOURCES = \
	NdbcntrInit.cpp	\
	NdbcntrSysTable.cpp \
	NdbcntrMain.cpp

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_kernel.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%
