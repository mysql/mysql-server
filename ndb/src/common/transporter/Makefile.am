
noinst_LTLIBRARIES = libtransporter.la

libtransporter_la_SOURCES = \
	Transporter.cpp \
	SendBuffer.cpp   \
	TCP_Transporter.cpp \
	TransporterRegistry.cpp \
        Packer.cpp

#	SHM_Transporter.cpp

include $(top_srcdir)/ndb/config/common.mk.am
include $(top_srcdir)/ndb/config/type_util.mk.am
INCLUDES += -I$(top_srcdir)/ndb/include/kernel -I$(top_srcdir)/ndb/include/transporter

# Don't update the files from bitkeeper
%::SCCS/s.%
