
noinst_LTLIBRARIES = libtransporter.la

#libtransporter_la_SOURCES = @NDB_TRANSPORTER_SOURCES@

libtransporter_la_SOURCES = \
	Transporter.cpp \
	SendBuffer.cpp   \
	TCP_Transporter.cpp \
	TransporterRegistry.cpp \
        Packer.cpp

#libtransporter_la_SOURCES = \
#	Transporter.cpp \
#	SendBuffer.cpp   \
#	TCP_Transporter.cpp \
#	TransporterRegistry.cpp \
#        Packer.cpp
#
#if HAVE_NDB_SHM
#  libtransporter_la_SOURCES += SHM_Transporter.cpp SHM_Transporter.unix.cpp
#endif
#
#if HAVE_NDB_SCI
#  libtransporter_la_SOURCES += SCI_Transporter.cpp
#endif

INCLUDES_LOC = -I$(top_srcdir)/ndb/include/kernel -I$(top_srcdir)/ndb/include/transporter

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_util.mk.am

# Don't update the files from bitkeeper
%::SCCS/s.%
