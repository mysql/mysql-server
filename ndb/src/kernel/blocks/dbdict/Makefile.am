#SUBDIRS = printSchemafile

noinst_LIBRARIES = libdbdict.a

libdbdict_a_SOURCES = Dbdict.cpp

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_kernel.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%
