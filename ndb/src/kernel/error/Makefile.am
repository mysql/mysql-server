noinst_LIBRARIES = liberror.a

liberror_a_SOURCES = TimeModule.cpp \
	ErrorReporter.cpp \
	ErrorMessages.cpp

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_kernel.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%
